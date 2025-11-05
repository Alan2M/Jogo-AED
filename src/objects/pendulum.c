#include "pendulum.h"
#include <math.h>
#include "../player/player.h"

// Integração simples de pêndulo com AABB da plataforma
// A plataforma é desenhada/deslocada seguindo o bob do pêndulo

void PendulumInit(PendulumPlatform* p, Vector2 pivot, float length, float width, float height, float initialAngleRad) {
    p->pivot = pivot;
    p->length = length;
    p->angle = initialAngleRad;   // em radianos
    p->angVel = 0.0f;
    p->damping = 2.0f;            // amortecimento
    p->stiffness = 0.0f;          // sem mola adicional; restauração vem da gravidade
    p->gravity = 900.0f;          // px/s^2
    p->width = width;
    p->height = height;

    // Calcula posição inicial do bob
    Vector2 bob;
    bob.x = p->pivot.x + p->length * sinf(p->angle);
    bob.y = p->pivot.y + p->length * cosf(p->angle);
    p->center = bob;
    p->prevPos = bob;
    p->rect = (Rectangle){ bob.x - width/2.0f, bob.y - height/2.0f, width, height };
    p->torqueAccum = 0.0f;
}

void PendulumBeginFrame(PendulumPlatform* p) {
    p->torqueAccum = 0.0f;
}

void PendulumUpdate(PendulumPlatform* p) {
    float dt = GetFrameTime();
    // Dinâmica: theta'' = -(g/L) * sin(theta) + kT*torque - kD*theta' - kS*theta
    const float kTorque = 3.0f; // ganho de torque por jogador
    float angAcc = -(p->gravity / p->length) * sinf(p->angle) + kTorque * p->torqueAccum - p->damping * p->angVel - p->stiffness * p->angle;
    p->angVel += angAcc * dt;
    p->angle  += p->angVel * dt;

    // Atualiza posição do bob e do retângulo
    Vector2 bob;
    bob.x = p->pivot.x + p->length * sinf(p->angle);
    bob.y = p->pivot.y + p->length * cosf(p->angle);

    p->center = bob;
    p->rect.x = bob.x - p->width/2.0f;
    p->rect.y = bob.y - p->height/2.0f;
}

void PendulumHandlePlayer(PendulumPlatform* p, Player* pl) {
    // Aproximação: plataforma como segmento central com largura p->width e espessura p->height (orientada)
    // Vetores base
    float a = p->angle;
    Vector2 u = (Vector2){ cosf(a), -sinf(a) };   // eixo da plataforma (perpendicular ao cabo)
    Vector2 n = (Vector2){ sinf(a),  cosf(a) };   // normal (direção do cabo)

    // Ponto do pé do jogador
    Vector2 foot = (Vector2){ pl->rect.x + pl->rect.width*0.5f, pl->rect.y + pl->rect.height };
    // Vetor do centro até o pé
    Vector2 v = (Vector2){ foot.x - p->center.x, foot.y - p->center.y };
    float s = v.x*u.x + v.y*u.y; // coordenada ao longo do segmento
    float t = v.x*n.x + v.y*n.y; // distância ao longo da normal

    float halfLen = p->width * 0.5f;
    float halfThk = p->height * 0.5f;

    // Condição de aterrissagem: dentro dos limites laterais e aproximando pelo topo (t em [0, espessura])
    if (pl->velocity.y > 0 && s >= -halfLen && s <= halfLen && t >= -halfThk && t <= halfThk + 10.0f) {
        // Posiciona o jogador exatamente sobre a face superior (t = -halfThk)
        float desiredT = -halfThk;
        Vector2 newFoot = (Vector2){ p->center.x + u.x*s + n.x*desiredT,
                                     p->center.y + u.y*s + n.y*desiredT };
        // Ajusta a posição do retângulo do jogador
        pl->rect.y = newFoot.y - pl->rect.height;
        // Opcional: pequeno ajuste horizontal para grudar no movimento
        // pl->rect.x += (p->center.x - p->prevPos.x);

        pl->velocity.y = 0;
        pl->isJumping = false;

        // Torque proporcional à posição s (normalizado por halfLen)
        float norm = s / (halfLen + 1e-3f);
        if (norm < -1) norm = -1; else if (norm > 1) norm = 1;
        p->torqueAccum += norm;
    }
}

void PendulumDraw(const PendulumPlatform* p) {
    // Desenha o cabo
    DrawLineEx(p->pivot, p->center, 4.0f, DARKGRAY);

    // Desenha a plataforma como retângulo rotacionado
    Rectangle dest = { p->center.x, p->center.y, p->width, p->height };
    Vector2 origin = { p->width/2.0f, p->height/2.0f };
    float rotDeg = (-p->angle) * (180.0f/3.14159265f);
    DrawRectanglePro(dest, origin, rotDeg, (Color){ 90, 90, 120, 255 });
    DrawRectangleLinesEx((Rectangle){ p->center.x - p->width/2.0f, p->center.y - p->height/2.0f, p->width, p->height }, 1, BLACK);

    // Pivô
    DrawCircleV(p->pivot, 6.0f, GRAY);
}
