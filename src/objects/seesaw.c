#include "seesaw.h"
#include <math.h>
#include "../player/player.h"

void SeesawInit(SeesawPlatform* s, Vector2 center, float width, float height, float initialAngleRad) {
    s->center = center;
    s->width = width;
    s->height = height;
    s->angle = initialAngleRad;
    s->angVel = 0.0f;
    s->damping = 3.0f;        // amortecimento suficiente para parar quando sem carga
    s->torqueAccum = 0.0f;
    s->angleLimit = 1.2f;     // ~69 graus
    s->rect = (Rectangle){ center.x - width/2.0f, center.y - height/2.0f, width, height };
}

void SeesawBeginFrame(SeesawPlatform* s) {
    s->torqueAccum = 0.0f;
}

void SeesawHandlePlayer(SeesawPlatform* s, Player* pl) {
    // Base orientada da plataforma
    float a = s->angle;
    Vector2 u = (Vector2){ cosf(a), -sinf(a) };   // eixo da barra
    Vector2 n = (Vector2){ sinf(a),  cosf(a) };   // normal (cima da barra)

    // Pé do jogador
    Vector2 foot = (Vector2){ pl->rect.x + pl->rect.width*0.5f, pl->rect.y + pl->rect.height };
    // Centro -> pé
    Vector2 v = (Vector2){ foot.x - s->center.x, foot.y - s->center.y };
    float sCoord = v.x*u.x + v.y*u.y; // ao longo da barra
    float tCoord = v.x*n.x + v.y*n.y; // dist. na normal

    float halfLen = s->width * 0.5f;
    float halfThk = s->height * 0.5f;

    // Condição: jogador descendo e dentro da largura da barra, perto do topo
    if (pl->velocity.y > 0 && sCoord >= -halfLen && sCoord <= halfLen && tCoord >= -halfThk && tCoord <= halfThk + 10.0f) {
        // Aterrissa na face superior (t = -halfThk)
        float desiredT = -halfThk;
        Vector2 newFoot = (Vector2){ s->center.x + u.x*sCoord + n.x*desiredT,
                                     s->center.y + u.y*sCoord + n.y*desiredT };
        pl->rect.y = newFoot.y - pl->rect.height;
        pl->velocity.y = 0;
        pl->isJumping = false;

        // Torque proporcional à distância do centro
        // Inverte o sinal para que estar à esquerda faça a esquerda descer
        float norm = sCoord / (halfLen + 1e-3f);
        if (norm < -1) norm = -1; else if (norm > 1) norm = 1;
        s->torqueAccum -= norm;
    }
}

void SeesawUpdate(SeesawPlatform* s) {
    float dt = GetFrameTime();
    const float kTorque = 8.0f; // ganho de torque por jogador (mais responsivo)
    float angAcc = kTorque * s->torqueAccum - s->damping * s->angVel;
    s->angVel += angAcc * dt;
    s->angle  += s->angVel * dt;

    // Limita o ângulo para evitar virar demais
    if (s->angle > s->angleLimit) { s->angle = s->angleLimit; s->angVel = 0; }
    if (s->angle < -s->angleLimit) { s->angle = -s->angleLimit; s->angVel = 0; }

    // Atualiza AABB auxiliar para debug/desenho simples (não usada na colisão orientada)
    s->rect.x = s->center.x - s->width/2.0f;
    s->rect.y = s->center.y - s->height/2.0f;
}

void SeesawDraw(const SeesawPlatform* s) {
    // Desenha a barra rotacionada em torno do centro
    Rectangle dest = { s->center.x, s->center.y, s->width, s->height };
    Vector2 origin = { s->width/2.0f, s->height/2.0f };
    float rotDeg = (-s->angle) * (180.0f/3.14159265f);
    DrawRectanglePro(dest, origin, rotDeg, (Color){ 100, 120, 160, 255 });
    // Fulcro
    DrawCircleV(s->center, 6.0f, DARKGRAY);
}
