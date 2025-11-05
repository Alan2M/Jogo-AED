#ifndef GOAL_H
#define GOAL_H

#include "raylib.h"

typedef struct Player Player; // forward declaration

typedef struct Goal {
    Rectangle rect;
    Color color;
} Goal;

void GoalInit(Goal* g, float x, float y, float w, float h, Color c);
void GoalDraw(const Goal* g);
// Retorna true se qualquer jogador alcançou a linha de chegada
bool GoalReached(const Goal* g, const Player* p1, const Player* p2, const Player* p3);

#endif

