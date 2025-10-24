#include "mapa.h"

void InitMapa(Mapa *m) {
    m->ground = (Rectangle){0, 600, 2000, 2000}; // chão grande
    m->width = 2000;
    m->height = 450;
}

void DrawMapa(Mapa m) {
    DrawRectangleRec(m.ground, BROWN);
}
