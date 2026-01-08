// src/Paleta.cpp

#include "Paleta.h" 

// --- Constructor ---
Paleta::Paleta(int start_x) {
    x = start_x;
    // Inicializamos tanto la Y entera como la flotante en el centro
    y = (64 / 2) - (ALTO / 2);
    y_float = (float)y; 
}

// --- Método de Actualización de Posición con Suavizado ---
void Paleta::actualizarPosicion(int joy_val) {
    
    // 1. MITIGACIÓN DE RUIDO
    if (joy_val < 50) { 
        joy_val = 0; 
    } else if (joy_val > 4045) {
        joy_val = 4095;
    }

    // 2. Cálculo del OBJETIVO (Target)
    // Calculamos a dónde debería ir la paleta según el joystick
    float target_y = map(joy_val, 0, 4095, 0, 64 - ALTO); 

    // 3. LÓGICA DE SUAVIZADO (Lerp)
    // Definimos una constante de suavizado (0.1 significa que se mueve el 10% de la distancia restante)
    // Puedes ajustar este valor: 0.05 es muy lento/suave, 0.20 es más rápido.
    const float SUAVIZADO = 0.12f; 
    
    // La paleta "persigue" al objetivo
    y_float = y_float + (target_y - y_float) * SUAVIZADO;

    // 4. Convertimos a entero para el dibujo en pantalla
    y = (int)y_float;

    // 5. Limitar posición
    y = constrain(y, 0, 64 - ALTO);
    y_float = constrain(y_float, 0.0f, (float)(64 - ALTO));
}

// --- Método de Dibujo ---
void Paleta::dibujar(U8G2 &u8g2) {
    u8g2.drawBox(x, y, ANCHO, ALTO);
}