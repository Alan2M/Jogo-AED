#ifndef LAKE_H
#define LAKE_H

#include "raylib.h"

typedef struct Player Player; // forward decl

typedef enum {
    LAKE_WATER = 0,
    LAKE_FIRE,
    LAKE_EARTH,
    LAKE_POISON
} LakeType;

typedef struct Lake {
    Rectangle rect;
    LakeType type;
    Color color;
} Lake;

void LakeInit(Lake* l, float x, float y, float w, float h, LakeType type);
void LakeDraw(const Lake* l);
// Retorna true se o jogador MORRE ao tocar um lago de elemento diferente
bool LakeHandlePlayer(const Lake* l, Player* p, LakeType playerElement);

#endif
