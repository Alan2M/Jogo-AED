#ifndef BUTTON_H
#define BUTTON_H

#include "raylib.h"

typedef struct Player Player; // forward declaration

typedef struct Button {
    Rectangle rect;     // área do botão no chão
    Color colorUp;      // cor quando solto
    Color colorDown;    // cor quando pressionado
    bool pressed;       // estado atual
    const Texture2D* spriteUp;    // sprite opcional para estado solto (compartilhado)
    const Texture2D* spriteDown;  // sprite opcional para estado pressionado
} Button;

void ButtonInit(Button* b, float x, float y, float w, float h, Color up, Color down);
// Atualiza o estado com base nos jogadores; retorna true se está pressionado
bool ButtonUpdate(Button* b, const Player* p1, const Player* p2, const Player* p3);
void ButtonDraw(const Button* b);
void ButtonSetSprites(Button* b, const Texture2D* spriteUp, const Texture2D* spriteDown);

#endif

