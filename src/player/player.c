#include "player.h"

void InitEarthboy(Player *p) {
    p->rect = (Rectangle){100, 300, 60, 60};
    p->velocity = (Vector2){0, 0};
    p->isJumping = false;
    p->facingRight = true;
    p->idle = true;

    p->walkFrames[0] = LoadTexture("assets/earthboy/walk/WALK1.png");
    p->walkFrames[1] = LoadTexture("assets/earthboy/walk/WALK2.png");
    p->walkFrames[2] = LoadTexture("assets/earthboy/walk/WALK3.png");
    p->walkFrames[3] = LoadTexture("assets/earthboy/walk/WALK4.png");
    p->walkFrames[4] = LoadTexture("assets/earthboy/walk/WALK5.png");
    p->walkFrames[5] = LoadTexture("assets/earthboy/walk/WALK6.png");
    p->walkFrames[6] = LoadTexture("assets/earthboy/walk/WALK7.png");
    p->walkFrames[7] = LoadTexture("assets/earthboy/walk/WALK8.png");
    p->totalWalkFrames = 8;

    p->idleFrames[0] = LoadTexture("assets/earthboy/IDLE.png");
    p->totalIdleFrames = 1;

    p->frameAtual = 0;
    p->tempoFrame = 0.1f;
    p->timer = 0.0f;
}

// --- FIREBOY ---
void InitFireboy(Player *p) {
    p->rect = (Rectangle){200, 700, 60, 60};
    p->velocity = (Vector2){0, 0};
    p->isJumping = false;
    p->facingRight = true;
    p->idle = true;

    p->walkFrames[0] = LoadTexture("assets/fireboy/walk/WALK1.png");
    p->walkFrames[1] = LoadTexture("assets/fireboy/walk/WALK2.png");
    p->walkFrames[2] = LoadTexture("assets/fireboy/walk/WALK3.png");
    p->walkFrames[3] = LoadTexture("assets/fireboy/walk/WALK4.png");
    p->walkFrames[4] = LoadTexture("assets/fireboy/walk/WALK5.png");
    p->walkFrames[5] = LoadTexture("assets/fireboy/walk/WALK6.png");
    p->walkFrames[6] = LoadTexture("assets/fireboy/walk/WALK7.png");
    p->walkFrames[7] = LoadTexture("assets/fireboy/walk/WALK8.png");
    p->totalWalkFrames = 8;

    p->idleFrames[0] = LoadTexture("assets/fireboy/idle/IDLE1.png");
    p->idleFrames[1] = LoadTexture("assets/fireboy/idle/IDLE2.png");
    p->idleFrames[2] = LoadTexture("assets/fireboy/idle/IDLE3.png");
    p->idleFrames[3] = LoadTexture("assets/fireboy/idle/IDLE4.png");
    p->totalIdleFrames = 4;

    p->frameAtual = 0;
    p->tempoFrame = 0.1f;
    p->timer = 0.0f;
}

// --- WATERGIRL ---
void InitWatergirl(Player *p) {
    p->rect = (Rectangle){400, 700, 60, 60};
    p->velocity = (Vector2){0, 0};
    p->isJumping = false;
    p->facingRight = true;
    p->idle = true;

    p->walkFrames[0] = LoadTexture("assets/watergirl/walk/WALK1.png");
    p->walkFrames[1] = LoadTexture("assets/watergirl/walk/WALK2.png");
    p->walkFrames[2] = LoadTexture("assets/watergirl/walk/WALK3.png");
    p->walkFrames[3] = LoadTexture("assets/watergirl/walk/WALK4.png");
    p->walkFrames[4] = LoadTexture("assets/watergirl/walk/WALK5.png");
    p->walkFrames[5] = LoadTexture("assets/watergirl/walk/WALK6.png");
    p->walkFrames[6] = LoadTexture("assets/watergirl/walk/WALK7.png");
    p->walkFrames[7] = LoadTexture("assets/watergirl/walk/WALK8.png");
    p->totalWalkFrames = 8;

    p->idleFrames[0] = LoadTexture("assets/watergirl/IDLE.png");

    p->totalIdleFrames = 1;

    p->frameAtual = 0;
    p->tempoFrame = 0.1f;
    p->timer = 0.0f;
}

// --- UPDATE genérico: teclas personalizadas ---
void UpdatePlayer(Player *p, Rectangle ground, int keyLeft, int keyRight, int keyJump) {
    bool moving = false;

    // Movimento horizontal
    if (IsKeyDown(keyRight)) {
        p->rect.x += 4;
        p->facingRight = true;
        moving = true;
    }
    if (IsKeyDown(keyLeft)) {
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
        if (p->frameAtual >= p->totalWalkFrames)
            p->frameAtual = 0;
        p->timer = 0.0f;
        }
    } 
    else { // Idle
        p->timer += GetFrameTime();
        if (p->timer >= p->tempoFrame) {
            p->frameAtual++;
            if (p->frameAtual >= p->totalIdleFrames)
                p->frameAtual = 0;
            p->timer = 0.0f;
        }
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
    if (IsKeyPressed(keyJump) && !p->isJumping) {
        p->velocity.y = -12;
        p->isJumping = true;
    }

    // Limites da tela
    if (p->rect.x < 0) p->rect.x = 0;
    if (p->rect.x + p->rect.width > ground.width)
        p->rect.x = ground.width - p->rect.width;
}

// --- Desenho ---
void DrawPlayer(Player p) {
    Texture2D frame;
    if (p.idle) {
        if (p.frameAtual >= p.totalIdleFrames) p.frameAtual = 0; // Reinicia se passou do último frame
        frame = p.idleFrames[p.frameAtual]; // Usa o frame atual da animação idle
    } 
    else {
        if (p.frameAtual >= p.totalWalkFrames) p.frameAtual = 0; // Reinicia se passou do último frame
        frame = p.walkFrames[p.frameAtual]; // Usa o frame atual da animação walk
    }


    // Mantém proporção do sprite dentro do retângulo do jogador,
    // alinhando pelos pés (base) e centralizando na largura.
    float aspect = (float)frame.width / (float)frame.height;
    float wFromH = p.rect.height * aspect;
    float hFromW = p.rect.width / aspect;
    float dw, dh;
    if (wFromH <= p.rect.width) { dw = wFromH; dh = p.rect.height; }
    else { dw = p.rect.width; dh = hFromW; }
    float dx = p.rect.x + (p.rect.width - dw) * 0.5f;
    float dy = p.rect.y + (p.rect.height - dh);
    Rectangle dest = { dx, dy, dw, dh };

    if (p.facingRight)
        DrawTexturePro(frame, (Rectangle){0, 0, frame.width, frame.height}, dest, (Vector2){0, 0}, 0.0f, WHITE);
    else
        DrawTexturePro(frame, (Rectangle){frame.width, 0, -frame.width, frame.height}, dest, (Vector2){0, 0}, 0.0f, WHITE);
}

// --- Liberar texturas ---
void UnloadPlayer(Player *p) {
    for (int i = 0; i < p->totalWalkFrames; i++) UnloadTexture(p->walkFrames[i]);
    for (int i = 0; i < p->totalIdleFrames; i++) UnloadTexture(p->idleFrames[i]);
}

