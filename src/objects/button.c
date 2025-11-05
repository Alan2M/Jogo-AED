#include "button.h"
#include "../player/player.h"

static bool overlaps(const Rectangle a, const Rectangle b) {
    return (a.x < b.x + b.width) && (a.x + a.width > b.x) &&
           (a.y < b.y + b.height) && (a.y + a.height > b.y);
}

void ButtonInit(Button* b, float x, float y, float w, float h, Color up, Color down) {
    b->rect = (Rectangle){ x, y, w, h };
    b->colorUp = up;
    b->colorDown = down;
    b->pressed = false;
}

bool ButtonUpdate(Button* b, const Player* p1, const Player* p2, const Player* p3) {
    // Considera pressionado se qualquer jogador está sobrepondo o retângulo do botão
    b->pressed = overlaps(b->rect, p1->rect) || overlaps(b->rect, p2->rect) || overlaps(b->rect, p3->rect);
    return b->pressed;
}

void ButtonDraw(const Button* b) {
    Color c = b->pressed ? b->colorDown : b->colorUp;
    // Corpo do botão
    DrawRectangleRec(b->rect, c);
    DrawRectangleLines((int)b->rect.x, (int)b->rect.y, (int)b->rect.width, (int)b->rect.height, BLACK);
    // Sombra/realce simples para parecer pressionado
    if (b->pressed) {
        DrawRectangle((int)b->rect.x, (int)(b->rect.y + b->rect.height - 3), (int)b->rect.width, 3, DARKGRAY);
    } else {
        DrawRectangle((int)b->rect.x, (int)b->rect.y, (int)b->rect.width, 3, RAYWHITE);
    }
}

