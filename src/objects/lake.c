#include "lake.h"
#include "../player/player.h"

static Color lakeColor(LakeType t) {
    switch (t) {
        case LAKE_WATER: return (Color){ 70, 120, 220, 230 };
        case LAKE_FIRE:  return (Color){ 220, 80, 40, 230 };
        case LAKE_EARTH: return (Color){ 120, 80, 40, 230 };
        default: return (Color){ 150, 150, 150, 230 };
    }
}

void LakeInit(Lake* l, float x, float y, float w, float h, LakeType type) {
    l->rect = (Rectangle){ x, y, w, h };
    l->type = type;
    l->color = lakeColor(type);
}

void LakeDraw(const Lake* l) {
    DrawRectangleRec(l->rect, l->color);
    DrawRectangleLines((int)l->rect.x, (int)l->rect.y, (int)l->rect.width, (int)l->rect.height, BLACK);
}

bool LakeHandlePlayer(const Lake* l, Player* p, LakeType playerElement) {
    if (!CheckCollisionRecs(p->rect, l->rect)) return false;
    if (playerElement == l->type) {
        // Permitido entrar
        return false;
    }
    // Lago errado: MORRE
    return true;
}
