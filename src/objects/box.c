#include "box.h"
#include <math.h>
#include "../player/player.h"

// Parâmetros de física simples
static const float kFriction = 0.85f;
static const float kMaxSpeed = 6.0f;
static const float kPushAccel = 1.2f;

void BoxInit(Box* b, Rectangle ground, float x, float width, float height) {
    b->rect = (Rectangle){ x, ground.y - height, width, height };
    b->velX = 0.0f;
}

void BoxHandlePush(Box* b, Player* p, int keyLeft, int keyRight) {
    if (!CheckCollisionRecs(p->rect, b->rect)) return;

    float pLeft = p->rect.x;
    float pRight = p->rect.x + p->rect.width;
    float pTop = p->rect.y;
    float pBottom = p->rect.y + p->rect.height;
    float bLeft = b->rect.x;
    float bRight = b->rect.x + b->rect.width;
    float bTop = b->rect.y;
    float bBottom = b->rect.y + b->rect.height;

    float overlapX = fminf(pRight - bLeft, bRight - pLeft);
    float overlapY = fminf(pBottom - bTop, bBottom - pTop);

    if (overlapX < overlapY) {
        // Lateral
        if (IsKeyDown(keyRight) && pRight > bLeft && pLeft < bLeft) {
            p->rect.x = b->rect.x - p->rect.width;
            b->velX += kPushAccel;
        } else if (IsKeyDown(keyLeft) && pLeft < bRight && pRight > bRight) {
            p->rect.x = b->rect.x + b->rect.width;
            b->velX -= kPushAccel;
        } else {
            if (p->rect.x < b->rect.x) p->rect.x = b->rect.x - p->rect.width;
            else p->rect.x = b->rect.x + b->rect.width;
        }
    } else {
        // Vertical
        if (p->velocity.y > 0 && pBottom > bTop && pTop < bTop) {
            p->rect.y = b->rect.y - p->rect.height;
            p->velocity.y = 0;
            p->isJumping = false;
        } else if (p->velocity.y < 0 && pTop < bBottom && pBottom > bBottom) {
            p->rect.y = b->rect.y + b->rect.height;
            p->velocity.y = 0;
        }
    }
}

void BoxUpdate(Box* b, Rectangle ground) {
    if (fabsf(b->velX) < 0.05f) b->velX = 0.0f; else b->velX *= kFriction;
    if (b->velX > kMaxSpeed) b->velX = kMaxSpeed; else if (b->velX < -kMaxSpeed) b->velX = -kMaxSpeed;
    b->rect.x += b->velX;
    if (b->rect.x < 0) { b->rect.x = 0; b->velX = 0; }
    if (b->rect.x + b->rect.width > ground.width) { b->rect.x = ground.width - b->rect.width; b->velX = 0; }
}

void BoxDraw(const Box* b) {
    DrawRectangleRec(b->rect, DARKBROWN);
    DrawRectangleLines((int)b->rect.x, (int)b->rect.y, (int)b->rect.width, (int)b->rect.height, BLACK);
}

