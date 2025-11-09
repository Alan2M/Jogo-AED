// src/mapa/fases/fase1.c
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "../../player/player.h"
#include "fases.h"

// --- Config do mapa (Tiled 27x27, 71x40) ---
#define TILE 27
#define MAP_W_TILES 71
#define MAP_H_TILES 40
#define MAP_W_PX (MAP_W_TILES * TILE)   // 1917
#define MAP_H_PX (MAP_H_TILES * TILE)   // 1080

// --- Colliders ---
typedef struct { Rectangle rect; } HitRect;
#define MAX_HITS 1024
static HitRect HITS[MAX_HITS];
static int HITS_COUNT = 0;

static bool DEBUG_ON = false;
static bool LOADED_TMX = false;
static char TMX_USED[256] = {0};

// ------------ Utils de parsing XML simples ------------
static float attr_float(const char *s, const char *key, bool *ok) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s=\"", key);
    const char *p = strstr(s, pattern);
    if (!p) { if (ok) *ok = false; return 0.0f; }
    p += (int)strlen(pattern);
    if (ok) *ok = true;
    return strtof(p, NULL);
}
static void push_hit(Rectangle r) {
    if (HITS_COUNT < MAX_HITS) HITS[HITS_COUNT++].rect = r;
}
static void add_polygon_as_aabb(float objX, float objY, const char *line) {
    const char *p = strstr(line, "points=\"");
    if (!p) return;
    p += 8;
    const char *endq = strchr(p, '"');
    if (!endq) return;

    char buf[2048];
    size_t n = (size_t)(endq - p);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, p, n); buf[n] = '\0';

    double minX =  1e12, minY =  1e12, maxX = -1e12, maxY = -1e12;
    char *cur = buf;
    while (*cur) {
        while (*cur == ' ') cur++;
        if (!*cur) break;
        double px = 0.0, py = 0.0;
        if (sscanf(cur, "%lf,%lf", &px, &py) == 2) {
            if (px < minX) minX = px;
            if (py < minY) minY = py;
            if (px > maxX) maxX = px;
            if (py > maxY) maxY = py;
        }
        char *space = strchr(cur, ' ');
        if (!space) break;
        cur = space + 1;
    }
    if (maxX > minX && maxY > minY) {
        push_hit((Rectangle){ (float)(objX + minX),
                              (float)(objY + minY),
                              (float)(maxX - minX),
                              (float)(maxY - minY) });
    }
}

static bool load_collisions_from_tmx_path(const char *tmxPath) {
    FILE *f = fopen(tmxPath, "r");
    if (!f) return false;

    HITS_COUNT = 0;
    bool in_Piso = false, in_Polig = false, waitingPolygon = false;
    float lastObjX = 0.0f, lastObjY = 0.0f;

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "<objectgroup") && strstr(line, "name=\"Colisao_Piso\"")) {
            in_Piso = true;  in_Polig = false; waitingPolygon = false; continue;
        }
        if (strstr(line, "<objectgroup") && strstr(line, "name=\"Colisao_Poligonos\"")) {
            in_Piso = false; in_Polig = true;  waitingPolygon = false; continue;
        }
        if (strstr(line, "</objectgroup>")) {
            in_Piso = false; in_Polig = false; waitingPolygon = false; continue;
        }

        if (in_Piso && strstr(line, "<object ") && strstr(line, "width=")) {
            bool okx=false, oky=false, okw=false, okh=false;
            float x = attr_float(line, "x", &okx);
            float y = attr_float(line, "y", &oky);
            float w = attr_float(line, "width",  &okw);
            float h = attr_float(line, "height", &okh);
            if (okx && oky && okw && okh) push_hit((Rectangle){ x, y, w, h });
            continue;
        }

        if (in_Polig) {
            if (strstr(line, "<object ")) {
                bool okx=false, oky=false;
                lastObjX = attr_float(line, "x", &okx);
                lastObjY = attr_float(line, "y", &oky);
                waitingPolygon = (okx && oky);
                if (waitingPolygon && strstr(line, "points=")) {
                    add_polygon_as_aabb(lastObjX, lastObjY, line);
                    waitingPolygon = false;
                }
                continue;
            }
            if (waitingPolygon && strstr(line, "points=")) {
                add_polygon_as_aabb(lastObjX, lastObjY, line);
                waitingPolygon = false;
                continue;
            }
        }
    }
    fclose(f);

    // bordas de segurança (fininhas)
    push_hit((Rectangle){ 0, 0, (float)MAP_W_PX, 2 });
    push_hit((Rectangle){ 0, (float)(MAP_H_PX-2), (float)MAP_W_PX, 2 });
    push_hit((Rectangle){ 0, 0, 2, (float)MAP_H_PX });
    push_hit((Rectangle){ (float)(MAP_W_PX-2), 0, 2, (float)MAP_H_PX });

    strncpy(TMX_USED, tmxPath, sizeof(TMX_USED)-1);
    return true;
}

static void build_fallback_collisions(const char *reason) {
    HITS_COUNT = 0;
    // chão aproximado
    push_hit((Rectangle){ 0, 918, (float)MAP_W_PX, 27 });
    // paredes + teto
    push_hit((Rectangle){ 0, 0, (float)MAP_W_PX, 2 });
    push_hit((Rectangle){ 0, (float)(MAP_H_PX-2), (float)MAP_W_PX, 2 });
    push_hit((Rectangle){ 0, 0, 2, (float)MAP_H_PX });
    push_hit((Rectangle){ (float)(MAP_W_PX-2), 0, 2, (float)MAP_H_PX });
    snprintf(TMX_USED, sizeof(TMX_USED), "FALLBACK (%s)", reason ? reason : "no TMX");
}

static void load_collisions_resilient(void) {
    const char *cands[] = {
        "assets/maps/maps1.tmx",
        "assets/maps/maps1..tmx",
        "assets\\maps\\maps1.tmx",
        "assets\\maps\\maps1..tmx"
    };
    LOADED_TMX = false;
    for (int i = 0; i < (int)(sizeof(cands)/sizeof(cands[0])); i++) {
        if (load_collisions_from_tmx_path(cands[i])) { LOADED_TMX = true; break; }
    }
    if (!LOADED_TMX) build_fallback_collisions("tmx not found");
}

// ---------- Sensor de chão ----------
// ---------- Sensor de chão (com tolerância de quina) ----------
static bool is_on_floor(const Player *p) {
    Rectangle foot = { p->rect.x + 2, p->rect.y + p->rect.height + 2,
                       p->rect.width - 4, 4 };
    for (int i = 0; i < HITS_COUNT; i++) {
        Rectangle r = HITS[i].rect;
        if (!CheckCollisionRecs(foot, r)) continue;
        Rectangle ov = GetCollisionRec(foot, r);
        // Só considera piso se a "largura" da sobreposição for razoável
        if (ov.width >= 8.0f && ov.height > 0.0f) return true;
    }
    return false;
}


// --------- Física com colliders (prioriza Y) ----------
// --------- Colisões (prioriza Y, mas com "perdão de quina") ----------
static void solve_collisions(Player *p) {
    const float EDGE_TOL = 8.0f;   // mínimo de largura de contato para ser chão/teto

    for (int i = 0; i < HITS_COUNT; i++) {
        Rectangle r = HITS[i].rect;
        if (!CheckCollisionRecs(p->rect, r)) continue;

        Rectangle ov = GetCollisionRec(p->rect, r);
        if (ov.width <= 0 || ov.height <= 0) continue;

        bool preferY = (fabsf(p->velocity.y) > 0.01f);
        if (!preferY) preferY = (ov.height <= ov.width);

        if (preferY) {
            // SUBINDO: possível "teto"
            if (p->velocity.y < 0.0f) {
                // Só trata como teto se tiver largura suficiente; senão, vira parede
                if (ov.width >= EDGE_TOL) {
                    p->rect.y += ov.height;      // afasta para baixo
                    p->velocity.y = 0.0f;
                } else {
                    // Encostou de quina no teto: desliza lateralmente
                    if (p->rect.x + p->rect.width * 0.5f < r.x + r.width * 0.5f)
                        p->rect.x -= ov.width;
                    else
                        p->rect.x += ov.width;
                }
            }
            // DESCENDO: possível "assoalho"
            else if (p->velocity.y > 0.0f) {
                if (ov.width >= EDGE_TOL) {
                    p->rect.y -= ov.height;      // assenta em cima
                    p->velocity.y = 0.0f;
                    p->isJumping = false;
                } else {
                    // Quina fina: trata como parede para não "colar" nem cair à força
                    if (p->rect.x + p->rect.width * 0.5f < r.x + r.width * 0.5f)
                        p->rect.x -= ov.width;
                    else
                        p->rect.x += ov.width;
                }
            } else {
                // Sem velocidade vertical relevante: resolve pelo menor eixo
                if (ov.height <= ov.width) {
                    if (p->rect.y < r.y) {        // em cima
                        p->rect.y -= ov.height;
                        p->velocity.y = 0.0f;
                        p->isJumping = false;
                    } else {                      // embaixo (teto)
                        if (ov.width >= EDGE_TOL) {
                            p->rect.y += ov.height;
                            if (p->velocity.y < 0) p->velocity.y = 0.0f;
                        } else {
                            // desliza na lateral
                            if (p->rect.x + p->rect.width * 0.5f < r.x + r.width * 0.5f)
                                p->rect.x -= ov.width;
                            else
                                p->rect.x += ov.width;
                        }
                    }
                } else {
                    if (p->rect.x < r.x) p->rect.x -= ov.width;
                    else                 p->rect.x += ov.width;
                }
            }
        } else {
            // Preferência X (parede)
            if (p->rect.x < r.x) p->rect.x -= ov.width;
            else                 p->rect.x += ov.width;
        }
    }

    // Estado final + limites
    if (is_on_floor(p)) {
        if (p->velocity.y > 0) p->velocity.y = 0;
        p->isJumping = false;
    }
    if (p->rect.x < 0) p->rect.x = 0;
    if (p->rect.x + p->rect.width > MAP_W_PX) p->rect.x = MAP_W_PX - p->rect.width;
    if (p->rect.y < 0) { p->rect.y = 0; if (p->velocity.y < 0) p->velocity.y = 0; }
    if (p->rect.y + p->rect.height > MAP_H_PX) {
        p->rect.y = MAP_H_PX - p->rect.height;
        if (p->velocity.y > 0) p->velocity.y = 0;
        p->isJumping = false;
    }
}

// ---- Spawn: piso mais baixo e mais à esquerda, seguro ----
static Vector2 find_spawn(void) {
    const float MARGIN_X   = 150.0f;
    const float TOP_LIMIT  = 80.0f;
    const float BOT_LIMIT  = MAP_H_PX - 60;
    const float Y_TOL      = 2.0f;

    float bestY = -1.0f;
    for (int i = 0; i < HITS_COUNT; i++) {
        Rectangle r = HITS[i].rect;
        if (r.width  < 60.0f)  continue;
        if (r.height > 40.0f)  continue;
        if (r.width  <= 4.0f || r.height <= 4.0f) continue;
        if (r.x < MARGIN_X || (r.x + r.width) > (MAP_W_PX - MARGIN_X)) continue;
        if (r.y < TOP_LIMIT || r.y > BOT_LIMIT) continue;
        if (r.y > bestY) bestY = r.y;
    }

    if (bestY >= 0.0f) {
        float leftX = 1e9f;
        for (int i = 0; i < HITS_COUNT; i++) {
            Rectangle r = HITS[i].rect;
            if (r.width  < 60.0f)  continue;
            if (r.height > 40.0f)  continue;
            if (r.width  <= 4.0f || r.height <= 4.0f) continue;
            if (fabsf(r.y - bestY) > Y_TOL) continue;
            if (r.x < leftX) leftX = r.x;
        }
        if (leftX < 9e8f) {
            float sx = leftX + 12.0f;
            float sy = bestY - 64.0f;
            if (sx < MARGIN_X) sx = MARGIN_X;
            if (sx > MAP_W_PX - MARGIN_X - 60.0f) sx = MAP_W_PX - MARGIN_X - 60.0f;
            if (sy < 0) sy = 0;
            if (sy > MAP_H_PX - 80.0f) sy = MAP_H_PX - 80.0f;
            return (Vector2){ sx, sy };
        }
    }
    return (Vector2){ 180.0f, 918.0f - 64.0f };
}

// ---------------------- Fase ----------------------
bool Fase1(void) {
    Texture2D mapa = LoadTexture("assets/maps/maps1.png");
    load_collisions_resilient(); // sempre cria algo (TMX ou fallback)

    Player earth, fire, water;
    InitEarthboy(&earth);
    InitFireboy(&fire);
    InitWatergirl(&water);

    Vector2 sp = find_spawn();
    earth.rect.x = sp.x;          earth.rect.y = sp.y;
    fire.rect.x  = sp.x + 70.0f;  fire.rect.y  = sp.y;
    water.rect.x = sp.x + 140.0f; water.rect.y = sp.y;

    SetTargetFPS(60);

    // ground “fake” só para limite horizontal do UpdatePlayer
    Rectangle fakeGround = (Rectangle){ 0, MAP_H_PX + 1000.0f, MAP_W_PX, 10.0f };

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE)) {
            UnloadTexture(mapa);
            UnloadPlayer(&earth);
            UnloadPlayer(&fire);
            UnloadPlayer(&water);
            return false;
        }
        if (IsKeyPressed(KEY_F1)) DEBUG_ON = !DEBUG_ON;

        // 1) Leitura do estado de chão
        bool earthOn  = is_on_floor(&earth);
        bool fireOn   = is_on_floor(&fire);
        bool waterOn  = is_on_floor(&water);

        // 2) Garantir que UpdatePlayer permita pular
        earth.isJumping  = !earthOn;
        fire.isJumping   = !fireOn;
        water.isJumping  = !waterOn;

        // 3) Injeção explícita do pulo no mesmo frame do input
        if (earthOn && IsKeyPressed(KEY_W))    { earth.velocity.y = -15.0f; earth.isJumping = true; }
        if (fireOn  && IsKeyPressed(KEY_UP))   { fire .velocity.y = -15.0f; fire .isJumping = true; }
        if (waterOn && IsKeyPressed(KEY_I))    { water.velocity.y = -15.0f; water.isJumping = true; }

        // 4) Update genérico (movimento, gravidade, animação)
        UpdatePlayer(&earth, fakeGround, KEY_A,    KEY_D,    KEY_W);
        UpdatePlayer(&fire,  fakeGround, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&water, fakeGround, KEY_J,    KEY_L,    KEY_I);

        // 5) Resolver colisões do mundo
        solve_collisions(&earth);
        solve_collisions(&fire);
        solve_collisions(&water);

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexture(mapa, 0, 0, WHITE);

        if (DEBUG_ON) {
            DrawText("DEBUG ON (F1 alterna)", 24, 24, 26, YELLOW);
            char info[256];
            snprintf(info, sizeof(info), "Colliders: %d | Fonte: %s%s",
                     HITS_COUNT, LOADED_TMX ? "TMX " : "",
                     TMX_USED[0] ? TMX_USED : "(fallback)");
            DrawText(info, 24, 56, 20, WHITE);

            for (int i = 0; i < HITS_COUNT; i++) {
                DrawRectangleLinesEx(HITS[i].rect, 2, RED);
            }

            // sensores de chão (verde) — visual
            Rectangle f1 = (Rectangle){ earth.rect.x + 2, earth.rect.y + earth.rect.height + 2, earth.rect.width - 4, 4 };
            Rectangle f2 = (Rectangle){ fire .rect.x + 2, fire .rect.y + fire .rect.height + 2, fire .rect.width - 4, 4 };
            Rectangle f3 = (Rectangle){ water.rect.x + 2, water.rect.y + water.rect.height + 2, water.rect.width - 4, 4 };
            DrawRectangleLinesEx(f1, 1, GREEN);
            DrawRectangleLinesEx(f2, 1, GREEN);
            DrawRectangleLinesEx(f3, 1, GREEN);
        } else {
            DrawText("F1 - Debug | ESC - Sair", 24, 24, 26, WHITE);
        }

        DrawPlayer(earth);
        DrawPlayer(fire);
        DrawPlayer(water);

        EndDrawing();
    }

    UnloadTexture(mapa);
    UnloadPlayer(&earth);
    UnloadPlayer(&fire);
    UnloadPlayer(&water);
    return false;
}
