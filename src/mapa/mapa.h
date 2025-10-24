#ifndef MAPA_H
#define MAPA_H

#include "raylib.h"

typedef struct {
    Rectangle ground; // chão principal (por enquanto)
    int width;
    int height;
} Mapa;

void InitMapa(Mapa *m);
void DrawMapa(Mapa m);

#endif
