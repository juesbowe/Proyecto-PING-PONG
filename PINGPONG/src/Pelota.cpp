#include "Pelota.h"
#include "Paleta.h"
#include "Juego.h" // Necesario para acceder a PIN_BUZZER

// --- Función auxiliar para emitir un sonido corto ---
// Utiliza PIN_BUZZER que está definido en Juego.h
void playSound(int freq, int duration_ms) {
    // Usamos tone() y noTone() para el buzzer
    tone(PIN_BUZZER, freq, duration_ms);
}


// --- Constructor ---
Pelota::Pelota() {
    randomSeed(analogRead(34)); 
    reiniciar();
}

// src/Pelota.cpp (Función reiniciar modificada)
void Pelota::reiniciar() {
    x = 128 / 2; 
    y = 64 / 2;

    // VELOCIDAD HORIZONTAL: AJUSTE A RANGO (0.2 a 0.5 píxeles por ciclo)
    float vel_x_base = 0.2; 
    float vel_x_rand = (float)random(0, 10) / 10.0 * 0.3; 

    // VELOCIDAD VERTICAL: AJUSTE A RANGO (0.1 a 0.4 píxeles por ciclo)
    float vel_y_base = 0.1; 
    float vel_y_rand = (float)random(0, 10) / 10.0 * 0.3; 

    // La velocidad final X estará entre 0.2 y 0.5
    velocidad_x = (random(0, 2) == 0) ? -(vel_x_base + vel_x_rand) : (vel_x_base + vel_x_rand); 
    
    // La velocidad final Y estará entre 0.1 y 0.4
    velocidad_y = (random(0, 2) == 0) ? -(vel_y_base + vel_y_rand) : (vel_y_base + vel_y_rand); 
}

// --- Lógica de Actualización Principal ---
void Pelota::actualizar(Paleta &p1, Paleta &p2, int &s1, int &s2) { 
    // 1. Mover la pelota
    x += velocidad_x;
    y += velocidad_y;

    // 2. Verificar colisiones de bordes
    verificarColisionBordes();
    
    // 3. Verificar colisiones de paleta
    verificarColisionPaleta(p1, p2); 

    // 4. Verificar si se salió del campo (Puntuación)
    if (x < 0) {
        s2++; 
        reiniciar();
        playSound(100, 200); // SONIDO: PUNTO GRAVE Y LARGO
    }
    
    if (x > 128 - TAMANO) {
        s1++;
        reiniciar();
        playSound(100, 200); // SONIDO: PUNTO GRAVE Y LARGO
    }
}

// --- Colisión con Bordes Superiores/Inferiores ---
void Pelota::verificarColisionBordes() {
    if (y <= 0) {
        velocidad_y = -velocidad_y;
        playSound(880, 50); // SONIDO: BORDE SUPERIOR (Agudo y corto)
    }
    if (y >= 64 - TAMANO) {
        velocidad_y = -velocidad_y;
        playSound(880, 50); // SONIDO: BORDE INFERIOR (Agudo y corto)
    }
}

// --- Colisión con Paletas (DEFINICIÓN DE LA FUNCIÓN CORREGIDA) ---
void Pelota::verificarColisionPaleta(Paleta &p1, Paleta &p2) {
    // Colisión con Paleta 1 (Izquierda)
    if (x <= p1.x + p1.ANCHO && 
        x >= p1.x &&
        // VERIFICACIÓN DE SUPERPOSICIÓN Y
        (y + TAMANO) >= p1.y && 
        y <= (p1.y + p1.ALTO) && 
        // FIN VERIFICACIÓN DE SUPERPOSICIÓN Y
        velocidad_x < 0) 
    {
        velocidad_x = -velocidad_x;
        playSound(440, 50); // SONIDO: PALETA (Medio y corto)
    }
    
    // Colisión con Paleta 2 (Derecha)
    if (x >= p2.x - TAMANO && 
        x <= p2.x + p2.ANCHO && 
        // VERIFICACIÓN DE SUPERPOSICIÓN Y
        (y + TAMANO) >= p2.y && 
        y <= p2.y + p2.ALTO &&
        // FIN VERIFICACIÓN DE SUPERPOSICIÓN Y
        velocidad_x > 0) 
    {
        velocidad_x = -velocidad_x;
        playSound(440, 50); // SONIDO: PALETA (Medio y corto)
    }
}

// --- Dibujo ---
void Pelota::dibujar(U8G2 &u8g2) {
    u8g2.drawBox((int)x, (int)y, TAMANO, TAMANO);
}