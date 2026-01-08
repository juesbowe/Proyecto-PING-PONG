// src/Pelota.h

#ifndef PELOTA_H
#define PELOTA_H

#include <U8g2lib.h>
#include <Arduino.h>
#include "Paleta.h" // Incluir Paleta para la lógica de colisión

class Pelota {
public:
    const int TAMANO = 3;
    float x;
    float y;
    float velocidad_x;
    float velocidad_y;

    // Constructor
    Pelota(); 

    // Métodos
    // En Pelota.h
    void actualizar(Paleta &p1, Paleta &p2, int &s1, int &s2);
    void dibujar(U8G2 &u8g2);
    void reiniciar();

private:
    void verificarColisionBordes();
    void verificarColisionPaleta(Paleta &p1, Paleta &p2);
};

#endif // PELOTA_H