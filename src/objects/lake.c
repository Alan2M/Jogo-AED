#include "lake.h"
#include "../player/player.h"

static Color lakeColor(LakeType t) {
    switch (t) {
        case LAKE_WATER: return (Color){ 70, 120, 220, 230 };
        case LAKE_FIRE:  return (Color){ 220, 80, 40, 230 };
        case LAKE_EARTH: return (Color){ 120, 80, 40, 230 };
        case LAKE_POISON: return (Color){ 60, 180, 60, 230 };
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

    // Zona de "superfície" que realmente mata quando o elemento é errado.
    // Restrita ao topo do lago para evitar mortes ao passar por baixo de lagos suspensos
    // ou encostar na lateral.
    const float killBand = 8.0f; // px de faixa letal a partir da superfície
    Rectangle killRect = l->rect;
    if (killRect.height > killBand) killRect.height = killBand; // só a faixa de cima

    if (!CheckCollisionRecs(p->rect, killRect)) return false;

    // Exige penetração vertical mínima dentro da faixa de topo
    Rectangle overlap = GetCollisionRec(p->rect, killRect);
    float pBottom = p->rect.y + p->rect.height;
    float lTop = l->rect.y;
    const float minDepthY = 4.0f;       // precisa afundar pelo menos 4px
    const float minBottomInside = 2.0f; // base precisa cruzar 2px abaixo da borda
    if (pBottom <= lTop + minBottomInside) return false; // só encostou na borda superior
    if (overlap.height < minDepthY) return false;        // raspada/entrada lateral

    if (l->type == LAKE_POISON) {
        return true; // veneno mata todos (na faixa de topo)
    }
    if (playerElement == l->type) {
        // Permitido entrar
        return false;
    }
    // Lago errado: MORRE
    return true;
}
