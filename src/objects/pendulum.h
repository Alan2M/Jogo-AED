// Plataforma pendular reutilizável
#ifndef PENDULUM_H
#define PENDULUM_H

#include "raylib.h"

// Encaminhamento para usar com Player
typedef struct Player Player;

typedef struct PendulumPlatform {
    Vector2 pivot;      // Ponto de ancoragem
    float length;       // Comprimento do fio
    float angle;        // Ângulo (rad), 0 = vertical
    float angVel;       // Velocidade angular
    float damping;      // Amortecimento
    float stiffness;    // Força que puxa de volta ao centro (0)
    float gravity;      // Gravidade efetiva (px/s^2)

    float width, height; // Tamanho da plataforma
    Rectangle rect;     // Retângulo AABB auxiliar
    Vector2 center;     // Centro atual da plataforma (bob)
    Vector2 prevPos;    // Centro no frame anterior
    float torqueAccum;  // Acúmulo de torque dos jogadores neste frame
} PendulumPlatform;

void PendulumInit(PendulumPlatform* p, Vector2 pivot, float length, float width, float height, float initialAngleRad);
void PendulumBeginFrame(PendulumPlatform* p);
void PendulumUpdate(PendulumPlatform* p);
void PendulumHandlePlayer(PendulumPlatform* p, Player* pl);
void PendulumDraw(const PendulumPlatform* p);

#endif
