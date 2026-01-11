#ifndef JUEGO_H
#define JUEGO_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "Paleta.h" 
#include "Pelota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h" 
#include <WiFi.h> // Necesario para ESP-NOW

// --- ESTRUCTURA DE COMUNICACIÓN ESP-NOW ---
// Enviamos la posición Y del joystick/acelerómetro (0-4095)
typedef struct struct_message {
    int player_id;      // 4 bytes
    int16_t joy_y_val;  // 2 bytes
    bool btn_pressed;   // 1 byte
} __attribute__((packed)) AccelData_t;

// Declaración de los tipos de estados (necesario antes de la estructura RTC)
typedef enum {
    STATE_TITLE_SCREEN,
    STATE_PLAYER_SELECT,
    STATE_VS_PLAYER,
    STATE_VS_AI,
    STATE_PAUSED, 
    STATE_GAME_OVER,
    STATE_IDLE // NUEVO: Estado de bajo consumo/modo pasivo
} GameState_t;

// --- ESTRUCTURA RTC RAM (Guarda el estado del juego) ---
// Debe estar en la sección RTC_DATA_ATTR para sobrevivir al Deep Sleep.
typedef struct {
    uint32_t magic_check;      // Usado para verificar si los datos son válidos (usaremos 0xDEAF)
    int score_p1;
    int score_p2;
    GameState_t last_active_state; // Último modo de juego activo (VS_PLAYER o VS_AI)
    GameState_t current_game_state; // Estado exacto en el que estaba (TÍTULO, PAUSA, etc.)
} RtcData_t;

// Variable global para almacenar el estado en la RTC RAM (definida con RTC_DATA_ATTR en main.cpp)
extern RTC_DATA_ATTR RtcData_t rtc_game_state; 

// Declaración externa de la U8g2 (definida en main.cpp)
extern U8G2_ST7920_128X64_1_SW_SPI u8g2;

// Macros de bloqueo (definida en main.cpp)
extern portMUX_TYPE scoreMux;

// --- HANDLES DE TAREAS (Para suspensión segura de FreeRTOS) ---
extern TaskHandle_t xTaskLogicaJuegoHandle;
extern TaskHandle_t xTaskDibujoHandle;

// --- CONSTANTES PARA DEEP SLEEP / INACTIVIDAD ---
const int INACTIVITY_TIMEOUT_MS = 30000; // Tiempo de inactividad para Ahorro de Energía (30 segundos)
// Tiempo de despertar del timer (ej: 600 segundos = 10 minutos).
const int DEEP_SLEEP_TIMEOUT_SEC = 600; 

// Constante de la puntuación máxima
const int MAX_SCORE = 10; 
// Pin del Buzzer
const int PIN_BUZZER = 27; 


// Clase principal para manejar el estado y la lógica del juego
class Juego {
public:
    // Objetos del juego
    Paleta paleta1;
    Paleta paleta2;
    Pelota pelota;

    // Variables de Estado
    GameState_t gameState;
    GameState_t last_active_state; // Guarda el último modo de juego (VS_PLAYER o VS_AI)
    GameState_t state_before_idle; // NUEVA: Guarda el estado exacto antes de entrar en IDLE
    int menuSelection; 
    int score_p1; 
    int score_p2;

    // En Juego.h, dentro de la clase Juego
    int dificultadIA = 1; // 0: Fácil, 1: Normal, 2: Difícil
    bool eligiendoDificultad = false; // Controla si mostramos el submenú

    // --- VARIABLES DE COMUNICACIÓN ---
    // Valor recibido del acelerómetro/joystick remoto (0-4095)
    volatile int remote_joy_y_val = 2048; 
    // Bandera que indica si se recibió un paquete recientemente (para fallback)
    volatile bool remote_control_active = false;
    // Marca de tiempo del último paquete recibido
    volatile unsigned long last_remote_packet = 0;
    volatile bool remote_btn_pressed = false; // Estado actual del botón remoto

    // --- VARIABLES DE COMUNICACIÓN J2 ---
    volatile int remote_joy_y_val_j2 = 2048; 
    volatile bool remote_control_active_j2 = false;
    volatile unsigned long last_remote_packet_j2 = 0;
    volatile bool remote_btn_pressed_j2 = false;

    // Variable de Detección de Actividad
    unsigned long last_activity_time; // Guarda el último momento de interacción

    // Variables de Botón y Debounce (Botón 1)
    int btn1_last_state;
    unsigned long last_debounce_time;
    const unsigned long debounce_delay = 50;
    int btn1_debounced_state;
    int btn2_debounced_state;
    bool btn1_just_pressed; // Bandera de flanco

    // Variables de Botón 2 (Confirmar/Regresar)
    bool btn2_just_pressed;

    // Pines de Joysticks y Botones
    const int PIN_BUTTON_1 = 25; 
    const int PIN_BUTTON_2 = 26;
    const int PIN_JOYSTICK_1_Y = 34;
    const int PIN_JOYSTICK_2_Y = 35;

    // Constructor
    Juego();

    // Métodos de lógica principal (llamados por las tareas de FreeRTOS)
    void logica_IA();
    void actualizarLogica();
    void dibujarPantalla();
    
    // Método para entrar en Deep Sleep
    void entrarEnDeepSleep();

private:
    void reiniciarJuego();
    void checkInput();
};

#endif // JUEGO_H