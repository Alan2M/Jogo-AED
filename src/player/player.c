#include "player.h"

void InitPlayer(Player *p) {
    p->rect = (Rectangle){100, 300, 40, 40};
    p->velocity = (Vector2){0, 0};
    p->isJumping = false;
}

void UpdatePlayer(Player *p, Rectangle ground) {
    // Movimento horizontal
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) p->rect.x += 4;
    if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) p->rect.x -= 4;

    // Gravidade
    p->velocity.y += 0.5f;
    p->rect.y += p->velocity.y;

    
    // Colisão com o chão
    if (CheckCollisionRecs(p->rect, ground)) {
        p->rect.y = ground.y - p->rect.height;
        p->velocity.y = 0;
        p->isJumping = false;
    }
    
    // Pulo
    if ((IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) && !p->isJumping) {
        p->velocity.y = -10;
        p->isJumping = true;
    }
    
    if (p->rect.x < 0){
        p->rect.x = 0;
    }

    if (p->rect.x + p->rect.width > ground.width){
        p->rect.x = ground.width - p->rect.width;
    }

    if (IsKeyDown(KEY_R)){
            InitPlayer(p);
    }
}

void DrawPlayer(Player p) {
    DrawRectangleRec(p.rect, RED);
}
