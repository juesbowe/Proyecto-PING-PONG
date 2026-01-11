#include <Arduino.h>
#include <U8g2lib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Juego.h" 
#include "esp_sleep.h" 
#include <WiFi.h> 
#include <esp_now.h> 

// Definición de variables globales y externas (necesarias para el ESP-NOW callback)
extern portMUX_TYPE scoreMux;

// --- HANDLES DE TAREAS (Para suspensión segura en Deep Sleep) ---
TaskHandle_t xTaskLogicaJuegoHandle = NULL;
TaskHandle_t xTaskDibujoHandle = NULL;


// Definición global del objeto U8g2 (Core 0)
U8G2_ST7920_128X64_1_SW_SPI u8g2(U8G2_R0, /* clock=*/ 18, /* data=*/ 23, /* CS=*/ 5, /* reset=*/ 22);

// Instancia global del juego
Juego pongGame; 

// --------------------------------------------------------------------------
// --- DEFINICIÓN DE LA VARIABLE GLOBAL RTC RAM (SECCIÓN RTC) ---
RTC_DATA_ATTR RtcData_t rtc_game_state = {0x0000, 0, 0, STATE_TITLE_SCREEN, STATE_TITLE_SCREEN}; 
// --------------------------------------------------------------------------


// ==========================================================
//     *** CALLBACK DE RECEPCIÓN ESP-NOW ***
// ==========================================================
// Añade esta variable estática dentro de OnDataRecv en Main.cpp
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
    // 1. Creamos una estructura temporal limpia
    AccelData_t receivedData;
    memset(&receivedData, 0, sizeof(receivedData));

    // 2. Forzamos la copia de bytes (ignorando lo que crea el compilador del tamaño)
    if (len >= 7) { 
        memcpy(&receivedData, incomingData, 7); // Forzamos 7 bytes que es lo que envía el mando

        portENTER_CRITICAL(&scoreMux);
        
        // DEBUG para ver qué llega exactamente

        if (receivedData.player_id == 1) {
            pongGame.remote_joy_y_val = receivedData.joy_y_val;
            pongGame.remote_control_active = true;
            pongGame.last_remote_packet = millis();
            pongGame.remote_btn_pressed = receivedData.btn_pressed;
        } 
        else if (receivedData.player_id == 2) {
            pongGame.remote_joy_y_val_j2 = receivedData.joy_y_val;
            pongGame.remote_control_active_j2 = true;
            pongGame.last_remote_packet_j2 = millis();
            pongGame.remote_btn_pressed_j2 = receivedData.btn_pressed;
        }
        portEXIT_CRITICAL(&scoreMux);
        
        pongGame.last_activity_time = millis();
    }
}
// ==========================================================
//     *** FUNCIONES DE TAREA DE FREERTOS ***
// ==========================================================

// --- Tarea de Lógica del Juego (Core 1 - Rápido) ---
void Task_LogicaJuego(void *pvParameters) {
    for (;;) {
        // Llama al método de la instancia global del juego
        pongGame.actualizarLogica();
        vTaskDelay(pdMS_TO_TICKS(5)); // Lógica rápida (200 Hz)
    }
}

// --- Tarea de Dibujo (Core 0) ---
void Task_Dibujo(void *pvParameters) {
    for (;;) {
        // Llama al método de la instancia global del juego para dibujar
        pongGame.dibujarPantalla();
        vTaskDelay(pdMS_TO_TICKS(1)); // Ceder el control por 1 ms
    }
}

// --- FUNCIÓN DE UTILIDAD: Reporta la causa del despertar ---
void print_wakeup_reason(){
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    Serial.print("Causa del Despertar: ");

    switch(wakeup_reason){
        case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Despertado por EXT0 (Pin)"); break;
        case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Despertado por EXT1 (Pines RTC: Boton/Joystick)"); break; 
        case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Despertado por Timer (Tiempo agotado)"); break;
        case ESP_SLEEP_WAKEUP_ULP : Serial.println("Despertado por ULP"); break;
        case ESP_SLEEP_WAKEUP_UNDEFINED : Serial.println("Reinicio normal (Power-On o Reset)"); break;
        default : Serial.printf("Despertado por: %d\n", wakeup_reason); break;
    }
}

void setup() {
    Serial.begin(115200); 
    // AGREGAR: Esperar un momento para que el monitor serial se conecte y se establezca el baud rate.
    delay(500); 
    Serial.println("--- Sistema Iniciado ---");
    // 1. Verificar la causa del despertar antes de inicializar todo
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    print_wakeup_reason();

    // 2. Inicializar la pantalla primero (Core 0)
    u8g2.begin();
    u8g2.setPowerSave(0); 
    delay(10); // Pausa mínima para que la pantalla inicie

    // --- LÓGICA DE RECUPERACIÓN DE ESTADO RTC ---
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1 || wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        if (rtc_game_state.magic_check == 0xDEAF) {
            Serial.println("Recuperando estado de RTC RAM...");
            
            pongGame.score_p1 = rtc_game_state.score_p1;
            pongGame.score_p2 = rtc_game_state.score_p2;
            pongGame.last_active_state = rtc_game_state.last_active_state;
            pongGame.gameState = rtc_game_state.current_game_state;

            if (pongGame.gameState == STATE_VS_PLAYER || pongGame.gameState == STATE_VS_AI || pongGame.gameState == STATE_PAUSED) {
                pongGame.gameState = STATE_PAUSED;
                pongGame.menuSelection = 0; // REANUDAR
            } else {
                pongGame.menuSelection = 0; 
            }
            
            rtc_game_state.magic_check = 0;
        }
    }
    // --- FIN LÓGICA DE RECUPERACIÓN ---
    
    // Inicializar el generador de números aleatorios para la pelota
    randomSeed(esp_random()); 
    
    // Configuración de pines de entrada
    pinMode(pongGame.PIN_JOYSTICK_1_Y, INPUT_PULLUP); 
    pinMode(pongGame.PIN_JOYSTICK_2_Y, INPUT_PULLUP);
    pinMode(pongGame.PIN_BUTTON_1, INPUT_PULLUP); 
    pinMode(pongGame.PIN_BUTTON_2, INPUT_PULLUP);
    pinMode(PIN_BUZZER, OUTPUT); 
    
    // 3. Crear las tareas de FreeRTOS del JUEGO (Prioridad: alta/normal)
    xTaskCreatePinnedToCore(
        Task_LogicaJuego, 
        "LogicaJuego", 
        4096, 
        NULL, 
        1, // Prioridad 1 (Normal)
        &xTaskLogicaJuegoHandle, 
        1 // Core 1 (Lógica)
    );

    xTaskCreatePinnedToCore(
        Task_Dibujo, 
        "Dibujo", 
        4096, 
        NULL, 
        1, // Prioridad 1 (Normal)
        &xTaskDibujoHandle, 
        0 // Core 0 (Dibujo)
    );
    // ----------------------------------------------------------------------
    // 💡 PASO 4. INICIALIZACIÓN DE ESP-NOW (MOVIDO DESDE EL TASK)
    // ----------------------------------------------------------------------
    Serial.println("Inicializando WiFi/ESP-NOW en setup().");
    
    // Inicialización del WiFi 
    WiFi.mode(WIFI_STA); 
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
    Serial.flush(); // Garantiza que la MAC se imprima

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error inicializando ESP-NOW");
    } else {
        // Una vez inicializado, establece el callback de recepción
        esp_now_register_recv_cb(OnDataRecv);
        Serial.println("ESP-NOW inicializado y receptor registrado.");
    }
}

void loop() {
    // El loop principal se deja vacío para la gestión de tareas de FreeRTOS
}