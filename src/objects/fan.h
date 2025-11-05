#ifndef FAN_H
#define FAN_H

#include "raylib.h"

typedef struct Player Player; // forward declaration

typedef struct Fan {
    Rectangle rect;   // área de efeito do ventilador (coluna de ar)
    float strength;   // intensidade do empurro para cima (px/frame^2)
    Color color;      // cor para visualização
} Fan;

void FanInit(Fan* f, float x, float y, float w, float h, float strength);
void FanDraw(const Fan* f);
void FanApply(const Fan* f, Player* p);

#endif

