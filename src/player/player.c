#include "player.h"

void InitPlayer(Player *p) {
    p->rect = (Rectangle){100, 300, 60, 60};
    p->velocity = (Vector2){0, 0};
    p->isJumping = false;
    p->facingRight = true;
    p->idle = true;

    p->frames[0] = LoadTexture("assets/fireboy/walk/WALK1.png");
    p->frames[1] = LoadTexture("assets/fireboy/walk/WALK2.png");
    p->frames[2] = LoadTexture("assets/fireboy/walk/WALK3.png");
    p->frames[3] = LoadTexture("assets/fireboy/walk/WALK4.png");
    p->frames[4] = LoadTexture("assets/fireboy/walk/WALK5.png");
    p->frames[5] = LoadTexture("assets/fireboy/walk/WALK6.png");
    p->frames[6] = LoadTexture("assets/fireboy/walk/WALK7.png");
    p->frames[7] = LoadTexture("assets/fireboy/walk/WALK8.png");    

    p->totalFrames = 6;
    p->frameAtual = 0;
    p->tempoFrame = 0.1f;
    p->timer = 0.0f;
}


void UpdatePlayer(Player *p, Rectangle ground) {
    bool moving = false;

    // Movimento horizontal
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
        p->rect.x += 4;
        p->facingRight = true;
        moving = true;
    }
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
        p->rect.x -= 4;
        p->facingRight = false;
        moving = true;
    }

    p->idle = !moving;

    // Animação — troca de frames se estiver se movendo
    if (moving) {
        p->timer += GetFrameTime();
        if (p->timer >= p->tempoFrame) {
            p->frameAtual++;
            if (p->frameAtual >= p->totalFrames)
                p->frameAtual = 0;
            p->timer = 0.0f;
        }
    } else {
        p->frameAtual = 0;
    }

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

    // Limites da tela
    if (p->rect.x < 0) p->rect.x = 0;
    if (p->rect.x + p->rect.width > ground.width)
        p->rect.x = ground.width - p->rect.width;
}

void DrawPlayer(Player p) {
    Texture2D frame = p.frames[p.frameAtual];

    Rectangle dest = {p.rect.x, p.rect.y, p.rect.width, p.rect.height};

    // desenha espelhado se estiver virado pra esquerda
    if (p.facingRight)
        DrawTexturePro(frame, (Rectangle){0, 0, frame.width, frame.height}, dest, (Vector2){0, 0}, 0.0f, WHITE);
    else
        DrawTexturePro(frame, (Rectangle){frame.width, 0, -frame.width, frame.height}, dest, (Vector2){0, 0}, 0.0f, WHITE);
}
void UnloadPlayer(Player *p) {
    for (int i = 0; i < p->totalFrames; i++) {
        UnloadTexture(p->frames[i]);
    }
}
