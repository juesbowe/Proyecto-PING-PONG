#ifndef PALETA_H
#define PALETA_H

#include <U8g2lib.h>
#include <Arduino.h>

class Paleta {
public:
    // Constantes de tamaño
    const int ANCHO = 3;
    const int ALTO = 15;

    // Variables de posición
    int x;
    int y;
    float y_float; // Para cálculos de movimiento fluido

    // Constructor
    Paleta(int start_x); 
    
    // Método para actualizar la posición basado en el joystick o remoto
    void actualizarPosicion(int joy_val); 
    
    // Método para dibujar (recibe la referencia al objeto u8g2)
    void dibujar(U8G2 &u8g2);
};

#endif // PALETA_H