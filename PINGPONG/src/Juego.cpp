#include "Juego.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Definición de variables globales y externas (debe ser definida una vez)
extern U8G2_ST7920_128X64_1_SW_SPI u8g2;
portMUX_TYPE scoreMux = portMUX_INITIALIZER_UNLOCKED;

// --- Declaración externa de los handles de las tareas (definidas en main.cpp) ---
extern TaskHandle_t xTaskLogicaJuegoHandle;
extern TaskHandle_t xTaskDibujoHandle;

// --- Declaración externa de la variable RTC (definida en main.cpp) ---
extern RtcData_t rtc_game_state;

// Constructor: Inicializa estados
Juego::Juego()
    : paleta1(2),
      paleta2(128 - 3 - 2),
      gameState(STATE_TITLE_SCREEN),
      last_active_state(STATE_VS_PLAYER),
      state_before_idle(STATE_TITLE_SCREEN),
      menuSelection(0),
      score_p1(0),
      score_p2(0),
      btn1_last_state(HIGH),
      last_debounce_time(0),
      btn1_debounced_state(HIGH),
      btn2_debounced_state(HIGH),
      btn1_just_pressed(false),
      btn2_just_pressed(false),
      last_activity_time(millis()) // Inicialización correcta
{
    // Si despertamos del Deep Sleep, la lógica de main.cpp ya restauró el estado.
}

// Método auxiliar para reiniciar la partida
void Juego::reiniciarJuego() {
    score_p1 = 0;
    score_p2 = 0;
    pelota.reiniciar();
    gameState = STATE_PLAYER_SELECT;
    menuSelection = 0;
}

// --- Lógica básica para que la Paleta 2 siga a la Pelota (IA)
void Juego::logica_IA() {
    // 1. Encontrar el centro de la pelota
    int target_y = (int)pelota.y + pelota.TAMANO / 2;
    int paleta_max_y = 64 - paleta2.ALTO;

    // --- LÓGICA DE DIFICULTAD ---
    int margen_error;
    float velocidad_suavizado;

    switch(dificultadIA) {
        case 0: // FÁCIL: La IA es lenta y se despista mucho
            margen_error = 15; 
            velocidad_suavizado = 0.05; // Reacciona muy lento
            break;
        case 1: // NORMAL: Equilibrio
            margen_error = 7;
            velocidad_suavizado = 0.15; 
            break;
        case 2: // DIFÍCIL: Casi perfecta
        default:
            margen_error = 2;
            velocidad_suavizado = 0.4; 
            break;
    }

    // 2. Solo moverse si la pelota está fuera del "margen de error" 
    // Esto evita que la paleta vibre y permite que falle si la pelota va muy rápido
    static float y_suave = 32; // Posición interna para suavizar movimiento
    
    if (abs(target_y - (y_suave + paleta2.ALTO/2)) > margen_error) {
        // La paleta intenta alcanzar el objetivo pero no instantáneamente
        y_suave += (target_y - (y_suave + paleta2.ALTO/2)) * velocidad_suavizado;
    }

    // 3. Calcular el punto Y superior y restringirlo
    int target_top_y = (int)y_suave - (paleta2.ALTO / 2);
    target_top_y = constrain(target_top_y, 0, paleta_max_y);

    // 4. Mapear a 0-4095 para usar tu sistema de actualización de posición
    float joy_float = (float)target_top_y * 4095.0f / (float)paleta_max_y;
    paleta2.actualizarPosicion((int)joy_float);
}
// --- Manejo de la entrada del Botón 1 y Botón 2 (Flanco descendente debounced, Confirmar)
void Juego::checkInput() {
    // 1. Leer pines locales
    int reading1 = digitalRead(PIN_BUTTON_1);
    int reading2 = digitalRead(PIN_BUTTON_2);
    
    // 2. Limpiar señales remotas si el mando se desconecta
    if (!remote_control_active) remote_btn_pressed = false;
    if (!remote_control_active_j2) remote_btn_pressed_j2 = false;

    // 3. LÓGICA OR (CORREGIDA): J1 local o remoto / J2 local o remoto
    bool btn1_active = (reading1 == LOW) || remote_btn_pressed;
    bool btn2_active = (reading2 == LOW) || remote_btn_pressed_j2; // Aquí estaba el error (tenías reading1)

    // Reiniciamos flancos
    btn1_just_pressed = false;
    btn2_just_pressed = false;

    // --- PROCESAR JUGADOR 1 ---
    if (btn1_active && btn1_debounced_state == HIGH) {
        btn1_just_pressed = true; 
        btn1_debounced_state = LOW;
        last_activity_time = millis(); 
        Serial.println("Click: Jugador 1 (Local o Remoto)");
    }
    else if (!btn1_active && btn1_debounced_state == LOW) {
        btn1_debounced_state = HIGH;
    }

    // --- PROCESAR JUGADOR 2 ---
    if (btn2_active && btn2_debounced_state == HIGH) {
        btn2_just_pressed = true; 
        btn2_debounced_state = LOW;
        last_activity_time = millis(); 
        Serial.println("Click: Jugador 2 (Local o Remoto)");
    }
    else if (!btn2_active && btn2_debounced_state == LOW) {
        btn2_debounced_state = HIGH;
    }
    
    // Guardar estados anteriores (opcional, por compatibilidad)
    btn1_last_state = reading1;
    // Si usas btn2_last_state en Juego.h, añádelo aquí:
    // btn2_last_state = reading2; 
}

// --- Implementación del Deep Sleep ---
void Juego::entrarEnDeepSleep() {
    // 1. GUARDAR ESTADO EN RTC RAM antes de dormir
    rtc_game_state.magic_check = 0xDEAF; // Usamos el valor hexadecimal válido 0xDEAF
    rtc_game_state.score_p1 = score_p1;
    rtc_game_state.score_p2 = score_p2;
    rtc_game_state.last_active_state = last_active_state;
    // Guardar el estado que NO es IDLE
    if (gameState != STATE_IDLE) {
        rtc_game_state.current_game_state = gameState;
    } else {
        rtc_game_state.current_game_state = STATE_TITLE_SCREEN; // Valor seguro en caso de IDLE previo
    }

    // 2. Apagar la pantalla U8g2
    u8g2.setPowerSave(1);

    // 3. Apagar el buzzer (si está encendido)
    noTone(PIN_BUZZER);

    Serial.println("Entrando en modo Deep Sleep por inactividad...");
    Serial.printf("Despertara en %d segundos o al presionar un botón\n", DEEP_SLEEP_TIMEOUT_SEC);

    // 4. Configurar la fuente de despertar (EXT1):
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIMEOUT_SEC * 1000000ULL);

    // Máscara de pines RTC que activarán el despertar
    const uint64_t wakeup_mask = (1ULL << PIN_BUTTON_1) | (1ULL << PIN_BUTTON_2);

    // Despertar si CUALQUIERA de los pines en la máscara pasa a LOW (presionado)
    esp_sleep_enable_ext1_wakeup(wakeup_mask, ESP_EXT1_WAKEUP_ALL_LOW);

    // 5. CRÍTICO: SUSPENDER LA TAREA DE DIBUJO ANTES DE DORMIR
    Serial.println("SUSPENDIENDO TAREA DE DIBUJO (CORE 0)...");
    if (xTaskDibujoHandle != NULL) {
        vTaskSuspend(xTaskDibujoHandle);
    }
    // Damos un breve tiempo para que Core 0 reciba la suspensión.
    vTaskDelay(pdMS_TO_TICKS(5));

    // 6. Entrar en Deep Sleep
    esp_deep_sleep_start();

    // NOTA IMPORTANTE: Si llegamos aquí, el Deep Sleep falló.
    Serial.println("ERROR: Fallo al entrar en Deep Sleep. Entrando en IDLE pasivo...");
    // 7. Si falla, entrar en IDLE pasivo forzado:
    if (xTaskLogicaJuegoHandle != NULL) {
        vTaskSuspend(xTaskLogicaJuegoHandle); // Suspende Core 1 (Lógica)
    }
    if (xTaskDibujoHandle != NULL) {
        vTaskSuspend(xTaskDibujoHandle);
    }
    vTaskSuspend(NULL); // Suspende la tarea actual si todo lo demás falla
}

// --- Lógica Principal del Juego (CORE 1)
void Juego::actualizarLogica() {
    // ------------------------------------------------------------------
    // NUEVO: Temporizador estático para limitar la velocidad de navegación del menú
    static unsigned long last_menu_move_time = 0;
    // REDUCIDO DE 200 a 100ms para mejorar la respuesta del joystick en el menú.
    const unsigned long MENU_MOVE_DELAY = 100;

    // NUEVOS UMBRALES DE JOYSTICK: Más sensibles para la navegación del menú.
    // Centro es 2048. 1500 y 2500 requieren menos movimiento que 1000 y 3000.
    const int THRESHOLD_UP_MENU = 1500;
    const int THRESHOLD_DOWN_MENU = 2500;
    // ------------------------------------------------------------------

    // 1. Lectura y procesamiento de entradas
    checkInput();

 // Leer joysticks ANTES de la lógica para detectar actividad
    int joy1_val_local = analogRead(PIN_JOYSTICK_1_Y);
    int joy2_val = analogRead(PIN_JOYSTICK_2_Y);
    int joy1_val_final; // Valor que se usará para Paleta 1
    int joy2_val_final;
    // --- MANEJO DEL CONTROL REMOTO (Remote Control Handler) ---
    portENTER_CRITICAL(&scoreMux);
    // Desactivar el control remoto si no se ha recibido nada en 100ms
    if (remote_control_active && (millis() - last_remote_packet) > 100) {
        remote_control_active = false;
    }
    if (remote_control_active_j2 && (millis() - last_remote_packet_j2) > 100) {
        remote_control_active_j2 = false;
        // Serial.println("Control remoto inactivo."); // Descomentar para debug
    }

    if (remote_control_active) {
        joy1_val_final = remote_joy_y_val;
    } else {
        joy1_val_final = joy1_val_local;
    }
    if (remote_control_active_j2) {
        joy2_val_final = remote_joy_y_val_j2;
    } else {
        joy2_val_final = joy2_val;
    }
   
    portEXIT_CRITICAL(&scoreMux);

    // --- REINICIO DE CONTADOR POR MOVIMIENTO DE JOYSTICK ---
    // Si el valor está fuera de la zona muerta central (ej. 2048 +/- 500), es actividad.
    const int THRESHOLD = 500;

    // Detección de actividad local O remota
    if (abs(joy1_val_local - 2048) > THRESHOLD || remote_control_active) {
        last_activity_time = millis();
    }

    // --- REGLA DE INACTIVIDAD AUTOMÁTICA ---
    // El contador corre en TODOS los estados excepto IDLE
    if (gameState != STATE_IDLE && (millis() - last_activity_time) >= INACTIVITY_TIMEOUT_MS) {

        // --- TRANSICIÓN A IDLE (Modo Ahorro) ---
        // 1. Guardar estado actual antes de cambiar.
        if (gameState != STATE_GAME_OVER) {
             state_before_idle = gameState;
        } else {
             state_before_idle = STATE_TITLE_SCREEN;
        }

        gameState = STATE_IDLE;
        return;
    }

    // Si estamos en IDLE, la única actividad que hacemos es verificar si salir.
    if (gameState == STATE_IDLE) {
        // Verificar actividad de botones o joysticks (local o remota)
        if (btn1_just_pressed || btn2_just_pressed || abs(joy1_val_local - 2048) > THRESHOLD ||  remote_control_active||  remote_control_active_j2) {

             GameState_t previous_state = state_before_idle;

             // Si el estado guardado era una partida en curso, al despertar debe ir a PAUSED
             if (previous_state == STATE_VS_PLAYER || previous_state == STATE_VS_AI) {
                 gameState = STATE_PAUSED;
                 menuSelection = 0; // REANUDAR
             } else {
                 // Si estaba en un menú (TITLE o PLAYER_SELECT), vuelve al menú.
                 gameState = previous_state;
             }

             last_activity_time = millis(); // Reiniciar temporizador
             return;
        }
        // Si estamos inactivos, esperamos un poco más antes de la siguiente revisión de lógica
        vTaskDelay(pdMS_TO_TICKS(100)); // Ralentiza la lógica a 10 FPS
        return; // No procesar más lógica de juego en estado IDLE
    }

    // Definir confirmación unificada: Botón 1 O Botón 2
    bool confirm_pressed = btn1_just_pressed || btn2_just_pressed;

    // Lógica unificada para el movimiento del menú (Joystick 1 O Joystick 2)
    // Se usa el joy local para navegación en menú
    // <-- INICIO CAMBIO -->
    // Lógica independiente: Si J1 O J2 se mueven, se activa la bandera
    // 1. Lectura de valores finales (considerando remoto o local)
    // Esto debe estar antes de la máquina de estados
    // --- DETECCIÓN INDEPENDIENTE ---
    // Forzar valores a 2048 (centro) si están cerca para evitar que un joystick bloquee al otro
    if (abs(joy1_val_final - 2048) < 300) joy1_val_final = 2048;
    if (abs(joy2_val_final - 2048) < 300) joy2_val_final = 2048;
    // Joystick 1
    bool j1_sube = (joy1_val_final < THRESHOLD_UP_MENU);
    bool j1_baja = (joy1_val_final > THRESHOLD_DOWN_MENU);

    // Joystick 2 (CORREGIDO: aquí estaba el error de lógica)
    bool j2_sube = (remote_joy_y_val_j2 < THRESHOLD_UP_MENU);
    bool j2_baja = (remote_joy_y_val_j2 > THRESHOLD_DOWN_MENU);

    // --- COMBINACIÓN FINAL ---
    // El menú se mueve si CUALQUIERA de los dos actúa
    bool move_up = j1_sube || j2_sube;
    bool move_down  = j1_baja || j2_baja;
    // <-- FIN CAMBIO -->

    // 2. Máquina de Estados
    switch (gameState) {
        case STATE_TITLE_SCREEN:
            if (confirm_pressed) {
                gameState = STATE_PLAYER_SELECT;
            }
            break;

    case STATE_PLAYER_SELECT:
        // --- NAVEGACIÓN DEL MENÚ ---
        if ((millis() - last_menu_move_time) > MENU_MOVE_DELAY) {
            if (move_up) {
                if (menuSelection > 0) {
                    menuSelection--;
                    last_menu_move_time = millis();
                }
            } else if (move_down) {
                // Si estamos eligiendo dificultad hay 3 opciones (0,1,2), si no solo 2 (0,1)
                int limiteMax = eligiendoDificultad ? 2 : 1;
                if (menuSelection < limiteMax) {
                    menuSelection++;
                    last_menu_move_time = millis();
                }
            }
        }

        // --- LÓGICA DE CONFIRMACIÓN ---
            if (confirm_pressed) {
                if (!eligiendoDificultad) { 
                    if (menuSelection == 0) { // --- CASO 2 JUGADORES ---
                        eligiendoDificultad = false; // <--- ASEGURAR QUE ESTÉ EN FALSE
                        gameState = STATE_VS_PLAYER;
                        last_active_state = STATE_VS_PLAYER;
                        score_p1 = 0;
                        score_p2 = 0;
                        pelota.reiniciar();
                        last_activity_time = millis(); // Reset para que no entre en sleep
                        Serial.println("Iniciando Modo 2 Jugadores"); // Debug
                    } else { // --- CASO VS MÁQUINA ---
                        eligiendoDificultad = true; 
                        menuSelection = 0; 
                        last_menu_move_time = millis();
                    }
            } else {
                // --- ESTAMOS ELIGIENDO DIFICULTAD ---
                dificultadIA = menuSelection;
                gameState = STATE_VS_AI;
                last_active_state = STATE_VS_AI;
                eligiendoDificultad = false; // Reset para la próxima vez
                score_p1 = 0;
                score_p2 = 0;
                pelota.reiniciar();
            }
        }
        break;

        case STATE_VS_PLAYER:
        case STATE_VS_AI:
            // --- Lógica del juego en curso ---

            // **PAUSA UNIFICADA**: Si cualquier botón de confirmación es presionado, pausa.
            if (confirm_pressed) {
                gameState = STATE_PAUSED;
                menuSelection = 0;
                break;
            }

            if (gameState == STATE_PAUSED || gameState == STATE_GAME_OVER) {
                break;
            }

            // --- ACTUALIZACIÓN DE PALETAS CON CONTROL REMOTO/LOCAL ---
            if (gameState == STATE_VS_PLAYER) {
                paleta1.actualizarPosicion(joy1_val_final); // Usa valor final (local o remoto)
                paleta2.actualizarPosicion(joy2_val_final);
            } else { // STATE_VS_AI
                paleta1.actualizarPosicion(joy1_val_final); // Usa valor final (local o remoto)
                logica_IA();
            }

            portENTER_CRITICAL(&scoreMux);
            pelota.actualizar(paleta1, paleta2, score_p1, score_p2);
            portEXIT_CRITICAL(&scoreMux);

            // --- Verificación de Victoria ---
            if (score_p1 >= MAX_SCORE || score_p2 >= MAX_SCORE) {
                gameState = STATE_GAME_OVER;
                menuSelection = 0; // Rematch por defecto
            }
            break;

        case STATE_PAUSED:
            // --- NAVEGACIÓN DEL MENÚ (Corregida con temporizador) ---
            if ((millis() - last_menu_move_time) > MENU_MOVE_DELAY) {
                if (move_up) {
                    if (menuSelection > 0) menuSelection = 0;
                    last_menu_move_time = millis();
                } else if (move_down) {
                    if (menuSelection < 1) menuSelection = 1;
                    last_menu_move_time = millis();
                }
            }
            // --- FIN NAVEGACIÓN ---

            // Confirmar Pausa (Botón 1 O Botón 2)
            if (confirm_pressed) {
                if (menuSelection == 0) { // REANUDAR
                    gameState = last_active_state;
                    last_activity_time = millis(); // Actividad: Reiniciar temporizador
                } else if (menuSelection == 1) { // SALIR
                    gameState = STATE_TITLE_SCREEN;
                    menuSelection = 0;
                }
            }
            break;


        case STATE_GAME_OVER:
            // --- NAVEGACIÓN DEL MENÚ (Corregida con temporizador) ---
            if ((millis() - last_menu_move_time) > MENU_MOVE_DELAY) {
                if (move_up) {
                    if (menuSelection > 0) menuSelection = 0;
                    last_menu_move_time = millis();
                } else if (move_down) {
                    if (menuSelection < 1) menuSelection = 1;
                    last_menu_move_time = millis();
                }
            }
            // --- FIN NAVEGACIÓN ---

            // Confirmar Game Over (Botón 1 O Botón 2)
            if (confirm_pressed) {
                if (menuSelection == 0) { // Rematch
                    GameState_t previous_mode = last_active_state;
                    reiniciarJuego();
                    gameState = previous_mode;
                } else if (menuSelection == 1) { // Salir
                    gameState = STATE_TITLE_SCREEN;
                    menuSelection = 0;
                }
            }
            break;
        case STATE_IDLE:
              break;
    }
}

// --- Tarea de Dibujo (CORE 0)
void Juego::dibujarPantalla() {
    char score_str[5];

    // Si estamos en modo IDLE, solo apagamos la pantalla y salimos de la función.
    if (gameState == STATE_IDLE) {
        u8g2.setPowerSave(1); // Apagar pantalla
        return;
    }

    // Si no estamos en IDLE, nos aseguramos de que la pantalla esté encendida.
    u8g2.setPowerSave(0);

    u8g2.firstPage();
    do {
        u8g2.setDrawColor(1);

        // --- LÓGICA DE DETECCIÓN DE PRE-APAGADO ---
        bool show_warning = (millis() - last_activity_time) >= (INACTIVITY_TIMEOUT_MS - 3000);

        // Si el estado es la advertencia de 3 segundos, y NO es GAME_OVER.
        if (show_warning && gameState != STATE_GAME_OVER) {

            u8g2.setFont(u8g2_font_7x14B_tf);
            u8g2.drawStr(10, 20, "AHORRO ENERGIA");

            // Calculamos el tiempo restante
            long remaining_ms = (long)INACTIVITY_TIMEOUT_MS - (long)(millis() - last_activity_time);
            int remaining_sec = (int)(remaining_ms / 1000);

            if (remaining_sec > 0) {
                char msg[30];
                sprintf(msg, "Dormira en: %d s", remaining_sec);
                u8g2.setFont(u8g2_font_7x14_tf);
                u8g2.drawStr(5, 40, msg);
                u8g2.drawStr(5, 55, "Mover Joystick o boton");
            } else {
                u8g2.drawStr(10, 40, "Entrando a Sleep...");
            }

            // Si estamos en la advertencia, no dibujamos el juego subyacente.
            continue;
        }

        // -----------------------------------------------------------------
        // --- Dibujo de la Lógica Principal del Juego/Menú ---
        // -----------------------------------------------------------------

        switch (gameState) {
            case STATE_TITLE_SCREEN:
                // Fuente ligeramente más grande y centrada
                u8g2.setFont(u8g2_font_7x14B_tf);
                u8g2.drawStr(31, 20, "PING PONG");
                u8g2.setFont(u8g2_font_7x14B_tf);
                u8g2.drawStr(10, 50, "Presiona BOTON 1");
                break;
            case STATE_PLAYER_SELECT:
                u8g2.setFont(u8g2_font_7x14B_tf);
                
                if (!eligiendoDificultad) {
                    u8g2.drawStr(20, 15, "MODO DE JUEGO");
                    u8g2.setFont(menuSelection == 0 ? u8g2_font_7x14B_tf : u8g2_font_7x14_tf);
                    u8g2.drawStr(30, 35, "2 JUGADORES");
                    u8g2.setFont(menuSelection == 1 ? u8g2_font_7x14B_tf : u8g2_font_7x14_tf);
                    u8g2.drawStr(30, 50, "VS MAQUINA");
                } else {
                    u8g2.drawStr(20, 15, "DIFICULTAD IA");
                    u8g2.setFont(menuSelection == 0 ? u8g2_font_7x14B_tf : u8g2_font_7x14_tf);
                    u8g2.drawStr(35, 30, "FACIL");
                    u8g2.setFont(menuSelection == 1 ? u8g2_font_7x14B_tf : u8g2_font_7x14_tf);
                    u8g2.drawStr(35, 45, "NORMAL");
                    u8g2.setFont(menuSelection == 2 ? u8g2_font_7x14B_tf : u8g2_font_7x14_tf);
                    u8g2.drawStr(35, 60, "DIFICIL");
                }
                break;
            case STATE_VS_PLAYER:
            case STATE_VS_AI:
                // --- PANTALLA DE JUEGO ---
                u8g2.drawVLine(64, 0, 64); // Línea central

                portENTER_CRITICAL(&scoreMux);
                u8g2.setFont(u8g2_font_6x10_tf);

                // Dibujar Puntuación P1
                u8g2.setCursor(45, 10);
                sprintf(score_str, "%d", score_p1);
                u8g2.print(score_str);

                // Dibujar Puntuación P2
                u8g2.setCursor(75, 10);
                sprintf(score_str, "%d", score_p2);
                u8g2.print(score_str);
                portEXIT_CRITICAL(&scoreMux);

                // Dibujar objetos
                paleta1.dibujar(u8g2);
                paleta2.dibujar(u8g2);
                pelota.dibujar(u8g2);
                break;

            case STATE_PAUSED:
                // --- PANTALLA DE PAUSA ---
                u8g2.setFont(u8g2_font_7x14B_tf);
                u8g2.drawStr(40, 15, "PAUSA");

                // Opciones del menú...
                u8g2.setFont(menuSelection == 0 ? u8g2_font_7x14B_tf : u8g2_font_7x14_tf);
                u8g2.drawStr(40, 35, "REANUDAR");

                u8g2.setFont(menuSelection == 1 ? u8g2_font_7x14B_tf : u8g2_font_7x14_tf);
                u8g2.drawStr(40, 50, "SALIR");

                u8g2.setFont(u8g2_font_4x6_tf);
                u8g2.drawStr(0, 63, "J1/J2: Confirmar | J2: Pausa/Salir");
                break;

            case STATE_GAME_OVER:
                // --- PANTALLA DE FIN DE JUEGO ---
                u8g2.setFont(u8g2_font_7x14B_tf);

                // Ganador
                if (score_p1 >= MAX_SCORE) {
                    u8g2.drawStr(30, 15, "GANADOR J1!");
                } else {
                    u8g2.drawStr(30, 15, "GANADOR J2!");
                }

                // Opciones del menú...
                u8g2.setFont(menuSelection == 0 ? u8g2_font_7x14B_tf : u8g2_font_7x14_tf);
                u8g2.drawStr(40, 35, "REMATCH");

                u8g2.setFont(menuSelection == 1 ? u8g2_font_7x14B_tf : u8g2_font_7x14_tf);
                u8g2.drawStr(40, 50, "SALIR");

                u8g2.setFont(u8g2_font_4x6_tf);
                u8g2.drawStr(0, 63, "J1/J2: Confirmar | J2: Salir");
                break;
            case STATE_IDLE:
                // Ignorar
                break;
        }

    } while ( u8g2.nextPage() );
}