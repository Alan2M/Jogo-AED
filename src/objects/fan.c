#include "fan.h"
#include "../player/player.h"

void FanInit(Fan* f, float x, float y, float w, float h, float strength) {
    f->rect = (Rectangle){ x, y, w, h };
    f->strength = strength;
    f->color = (Color){ 140, 200, 255, 120 };
}

void FanDraw(const Fan* f) {
    // coluna de ar com listras
    DrawRectangleRec(f->rect, f->color);
    for (int i = 0; i < (int)f->rect.height; i += 20) {
        DrawLine((int)f->rect.x, (int)(f->rect.y + i), (int)(f->rect.x + f->rect.width), (int)(f->rect.y + i), (Color){180, 220, 255, 180});
    }
    DrawRectangleLines((int)f->rect.x, (int)f->rect.y, (int)f->rect.width, (int)f->rect.height, (Color){120, 170, 220, 200});
}

void FanApply(const Fan* f, Player* p) {
    if (!CheckCollisionRecs(p->rect, f->rect)) return;
    // Aplica força vertical suave para cima; limita a velocidade ascendente
    p->velocity.y -= f->strength;
    if (p->velocity.y < -12.0f) p->velocity.y = -12.0f;
}

