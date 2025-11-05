// Caixa empurrável reutilizável pelas fases
#ifndef BOX_H
#define BOX_H

#include "raylib.h"

// Encaminhamento para evitar dependência direta aqui
typedef struct Player Player;

typedef struct Box {
    Rectangle rect;
    float velX;
} Box;

void BoxInit(Box* b, Rectangle ground, float x, float width, float height);
void BoxHandlePush(Box* b, Player* p, int keyLeft, int keyRight);
void BoxUpdate(Box* b, Rectangle ground);
void BoxDraw(const Box* b);

#endif

