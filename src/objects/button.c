#include "button.h"
#include "../player/player.h"
#include <stddef.h>

static bool overlaps(const Rectangle a, const Rectangle b) {
    return (a.x < b.x + b.width) && (a.x + a.width > b.x) &&
           (a.y < b.y + b.height) && (a.y + a.height > b.y);
}

void ButtonInit(Button* b, float x, float y, float w, float h, Color up, Color down) {
    b->rect = (Rectangle){ x, y, w, h };
    b->colorUp = up;
    b->colorDown = down;
    b->pressed = false;
    b->spriteUp = NULL;
    b->spriteDown = NULL;
}

bool ButtonUpdate(Button* b, const Player* p1, const Player* p2, const Player* p3) {
    // Considera pressionado se qualquer jogador está sobrepondo o retângulo do botão
    b->pressed = overlaps(b->rect, p1->rect) || overlaps(b->rect, p2->rect) || overlaps(b->rect, p3->rect);
    return b->pressed;
}

void ButtonDraw(const Button* b) {
    if (b->spriteUp && b->spriteUp->id != 0 && b->rect.width > 0 && b->rect.height > 0) {
        const Texture2D* tex = (b->pressed && b->spriteDown && b->spriteDown->id != 0) ? b->spriteDown : b->spriteUp;
        float texW = (float)tex->width;
        float texH = (float)tex->height;
        if (texW > 0 && texH > 0) {
            float destW = b->rect.width;
            float destH = destW * (texH / texW);
            if (destH > b->rect.height) {
                float scale = b->rect.height / destH;
                destW *= scale;
                destH = b->rect.height;
            }
            Rectangle dest = {
                b->rect.x + (b->rect.width - destW) * 0.5f,
                b->rect.y + b->rect.height - destH,
                destW,
                destH
            };
            Color tint = b->pressed ? (Color){ 220, 220, 220, 255 } : WHITE;
            DrawTexturePro(*tex, (Rectangle){0, 0, texW, texH}, dest, (Vector2){0, 0}, 0.0f, tint);
            if (b->pressed) {
                DrawRectangle((int)b->rect.x, (int)(b->rect.y + b->rect.height - 2), (int)b->rect.width, 2, Fade(BLACK, 0.5f));
            }
            return;
        }
    }

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

void ButtonSetSprites(Button* b, const Texture2D* spriteUp, const Texture2D* spriteDown) {
    b->spriteUp = spriteUp;
    b->spriteDown = spriteDown ? spriteDown : spriteUp;
}

