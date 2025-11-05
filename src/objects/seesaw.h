#ifndef SEESAW_H
#define SEESAW_H

#include "raylib.h"

typedef struct Player Player; // forward decl

typedef struct SeesawPlatform {
    Vector2 center;     // Ponto de pivô (meio da plataforma)
    float angle;        // Ângulo (rad), 0 = horizontal, +CCW
    float angVel;       // Velocidade angular
    float damping;      // Amortecimento

    float width, height; // Dimensões
    Rectangle rect;     // AABB auxiliar (não rotacionada)
    float torqueAccum;  // Torque acumulado dos jogadores por frame
    float angleLimit;   // Limite de rotação (rad)
} SeesawPlatform;

void SeesawInit(SeesawPlatform* s, Vector2 center, float width, float height, float initialAngleRad);
void SeesawBeginFrame(SeesawPlatform* s);
void SeesawHandlePlayer(SeesawPlatform* s, Player* pl);
void SeesawUpdate(SeesawPlatform* s);
void SeesawDraw(const SeesawPlatform* s);

#endif

