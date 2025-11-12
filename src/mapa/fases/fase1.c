#include "raylib.h"
#include "../../player/player.h"
#include "../../objects/lake.h"
#include "../../objects/goal.h"
#include "../../game/game.h"
#include "../../ranking/ranking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_COLISOES 1024
#define STEP_HEIGHT 14.0f   // altura máxima que o jogador pode "subir" sem pular

typedef struct {
    Rectangle rect;
} Colisao;

typedef enum { PART_LEFT = 0, PART_MIDDLE, PART_RIGHT } LakePart;

#define MAX_LAKE_SEGS 256

typedef struct LakeSegment {
    Rectangle rect;
    LakeType type;
    LakePart part;
} LakeSegment;

typedef struct LakeAnimFrames {
    Texture2D left[16]; int leftCount;
    Texture2D middle[16]; int middleCount;
    Texture2D right[16]; int rightCount;
    float timer; int frame;
} LakeAnimFrames;

static int LoadFramesRange(Texture2D* arr, int max, const char* pattern, int startIdx, int endIdx) {
    // Carrega frames sequenciais verificando existência (evita WARNINGs); para no primeiro buraco
    int count = 0; bool started = false;
    for (int i = startIdx; i <= endIdx && count < max; ++i) {
        char path[256]; snprintf(path, sizeof(path), pattern, i);
        if (!FileExists(path)) { if (started) break; else continue; }
        Texture2D tex = LoadTexture(path);
        if (tex.id != 0) { arr[count++] = tex; started = true; }
        else if (started) { break; }
    }
    return count;
}

static void LoadLakeSet_Agua(LakeAnimFrames* s) {
    s->leftCount = LoadFramesRange(s->left, 16, "assets/map/agua/esquerdo/pixil-frame-%d.png", 0, 15);
    s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/agua/meio/pixil-frame-%d.png", 0, 15);
    s->rightCount = LoadFramesRange(s->right, 16, "assets/map/agua/direito/pixil-frame-%d.png", 0, 15);
    s->timer = 0.0f; s->frame = 0;
}

static void LoadLakeSet_Terra(LakeAnimFrames* s) {
    s->leftCount = LoadFramesRange(s->left, 16, "assets/map/terra/esquerdo/pixil-frame-%d.png", 0, 15);
    s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/terra/meio/pixil-frame-%d.png", 0, 15);
    s->rightCount = LoadFramesRange(s->right, 16, "assets/map/terra/direito/pixil-frame-%d.png", 0, 15);
    s->timer = 0.0f; s->frame = 0;
}

static void LoadLakeSet_Fogo(LakeAnimFrames* s) {
    // Tenta nomes Esquerda1.., Meio1.., Direita1..; se falhar, tenta pixil-frame-%d
    s->leftCount = LoadFramesRange(s->left, 16, "assets/map/fogo/esquerdo/Esquerda%d.png", 1, 32);
    if (s->leftCount == 0) s->leftCount = LoadFramesRange(s->left, 16, "assets/map/fogo/esquerdo/pixil-frame-%d.png", 0, 31);
    s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/fogo/meio/Meio%d.png", 1, 32);
    if (s->middleCount == 0) s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/fogo/meio/pixil-frame-%d.png", 0, 31);
    s->rightCount = LoadFramesRange(s->right, 16, "assets/map/fogo/direito/Direita%d.png", 1, 32);
    if (s->rightCount == 0) s->rightCount = LoadFramesRange(s->right, 16, "assets/map/fogo/direito/pixil-frame-%d.png", 0, 31);
    s->timer = 0.0f; s->frame = 0;
}

static void UnloadLakeSet(LakeAnimFrames* s) {
    for (int i = 0; i < s->leftCount; ++i) UnloadTexture(s->left[i]);
    for (int i = 0; i < s->middleCount; ++i) UnloadTexture(s->middle[i]);
    for (int i = 0; i < s->rightCount; ++i) UnloadTexture(s->right[i]);
    s->leftCount = s->middleCount = s->rightCount = 0;
}

static void LoadLakeSet_Acido(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   16, "assets/map/acido/esquerdo/pixil-frame-%d.png", 0, 15);
    s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/acido/meio/pixil-frame-%d.png",     0, 15);
    s->rightCount  = LoadFramesRange(s->right,  16, "assets/map/acido/direito/pixil-frame-%d.png",  0, 15);
    s->timer = 0.0f; s->frame = 0;
}

static int ParseRectsFromGroup(const char* tmxPath, const char* groupName, Rectangle* out, int cap) {
    int count = 0;
    char* xml = LoadFileText(tmxPath);
    if (!xml) return 0;

    const char* search = xml;
    while ((search = strstr(search, "<objectgroup")) != NULL) {
        const char* tagClose = strchr(search, '>');
        if (!tagClose) break;

        bool match = false;
        {
            size_t len = (size_t)(tagClose - search);
            char* header = (char*)malloc(len + 1);
            if (!header) { UnloadFileText(xml); return count; }
            memcpy(header, search, len);
            header[len] = '\0';
            char findName[128]; snprintf(findName, sizeof(findName), "name=\"%s\"", groupName);
            if (strstr(header, findName)) match = true;
            free(header);
        }

        const char* groupEnd = strstr(tagClose + 1, "</objectgroup>");
        if (!groupEnd) break;

        if (match) {
            const char* p = tagClose + 1;
            while (p < groupEnd) {
                const char* obj = strstr(p, "<object ");
                if (!obj || obj >= groupEnd) break;
                float x=0,y=0,w=0,h=0;
                if (sscanf(obj, "<object id=\"%*d\" x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\"", &x,&y,&w,&h) == 4) {
                    if (w>0 && h>0 && count < cap) out[count++] = (Rectangle){x,y,w,h};
                }
                p = obj + 8;
            }
        }
        search = groupEnd + 14;
    }

    UnloadFileText(xml);
    return count;
}

static int ParseColisoesDaCamada(const char* tmxPath, Colisao* out, int outCap) {
    Rectangle rects[MAX_COLISOES];
    int n = ParseRectsFromGroup(tmxPath, "Colisao", rects, MAX_COLISOES);
    if (n > outCap) n = outCap;
    for (int i = 0; i < n; ++i) out[i].rect = rects[i];
    return n;
}

bool Fase1(void) {
    // --- Carrega colisões somente da camada de objetos "Colisao" ---
    Colisao colisoes[MAX_COLISOES];
    int totalColisoes = ParseColisoesDaCamada("assets/maps/Mapa.tmx", colisoes, MAX_COLISOES);
    if (totalColisoes <= 0) {
        printf("❌ Nao foi possivel carregar colisoes da camada 'Colisao' em assets/maps/Mapa.tmx\n");
    }

    // --- Carrega segmentos de lagos conforme camadas do Tiled ---
    LakeSegment lakeSegs[MAX_LAKE_SEGS]; int lakeCount = 0;
    Rectangle tmpRects[128]; int n;
    // Agua
    n = ParseRectsFromGroup("assets/maps/Mapa.tmx", "Lago_Agua_Esquerdo", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_WATER, PART_LEFT };
    n = ParseRectsFromGroup("assets/maps/Mapa.tmx", "Lago_Agua_Meio", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_WATER, PART_MIDDLE };
    n = ParseRectsFromGroup("assets/maps/Mapa.tmx", "Lago_Agua_Direito", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_WATER, PART_RIGHT };
    // Fogo
    n = ParseRectsFromGroup("assets/maps/Mapa.tmx", "Lago_Fogo_Esquerdo", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_FIRE, PART_LEFT };
    n = ParseRectsFromGroup("assets/maps/Mapa.tmx", "Lago_Fogo_Meio", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_FIRE, PART_MIDDLE };
    n = ParseRectsFromGroup("assets/maps/Mapa.tmx", "Lago_Fogo_Direito", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_FIRE, PART_RIGHT };
    // Terra (marrom)
    n = ParseRectsFromGroup("assets/maps/Mapa.tmx", "Lago_Marrom_Esquerdo", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_EARTH, PART_LEFT };
    n = ParseRectsFromGroup("assets/maps/Mapa.tmx", "Lago_Marrom_Meio", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_EARTH, PART_MIDDLE };
    n = ParseRectsFromGroup("assets/maps/Mapa.tmx", "Lago_Marrom_Direito", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_EARTH, PART_RIGHT };
    // Veneno (verde)
    n = ParseRectsFromGroup("assets/maps/Mapa.tmx", "Lago_Verde_Esquerda", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_POISON, PART_LEFT };
    n = ParseRectsFromGroup("assets/maps/Mapa.tmx", "Lago_Verde_Meio", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_POISON, PART_MIDDLE };
    n = ParseRectsFromGroup("assets/maps/Mapa.tmx", "Lago_Verde_Direita", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_POISON, PART_RIGHT };

    // Carrega animações para cada tipo com assets existentes
    LakeAnimFrames animAgua = {0}, animFogo = {0}, animTerra = {0}, animAcido = {0};
    LoadLakeSet_Agua(&animAgua);
    LoadLakeSet_Fogo(&animFogo);
    LoadLakeSet_Terra(&animTerra);
    LoadLakeSet_Acido(&animAcido);

    // --- Carrega textura do mapa ---
    Texture2D mapTexture = LoadTexture("assets/maps/Map.png");
    if (mapTexture.id == 0) {
        printf("❌ Erro ao carregar assets/maps/Map.png\n");
        return false;
    }

    // --- Chegada (Goal) ---
    Rectangle gRects[4];
    int gCount = ParseRectsFromGroup("assets/maps/Mapa.tmx", "Chegada", gRects, 4);
    if (gCount == 0) gCount = ParseRectsFromGroup("assets/maps/Mapa.tmx", "Goal", gRects, 4);
    Rectangle goalRect;
    if (gCount > 0) goalRect = gRects[0];
    else goalRect = (Rectangle){ mapTexture.width - 80.0f, mapTexture.height - 140.0f, 40.0f, 120.0f };
    Goal goal; GoalInit(&goal, goalRect.x, goalRect.y, goalRect.width, goalRect.height, GOLD);

    // --- Inicializa jogadores menores ---
    Player earthboy, fireboy, watergirl;
    InitEarthboy(&earthboy);
    InitFireboy(&fireboy);
    InitWatergirl(&watergirl);

    earthboy.rect = (Rectangle){300, 700, 45, 50};
    fireboy.rect  = (Rectangle){400, 700, 45, 50};
    watergirl.rect= (Rectangle){500, 700, 45, 50};
    Vector2 spawnEarth = (Vector2){ earthboy.rect.x, earthboy.rect.y };
    Vector2 spawnFire  = (Vector2){ fireboy.rect.x,  fireboy.rect.y };
    Vector2 spawnWater = (Vector2){ watergirl.rect.x, watergirl.rect.y };

    // --- Câmera fixa ---
    Camera2D camera = {0};
    camera.target = (Vector2){mapTexture.width / 2.0f, mapTexture.height / 2.0f};
    camera.offset = (Vector2){GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
    camera.zoom = 1.0f;

    bool completed = false;
    float elapsed = 0.0f;
    bool debug = false;
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        elapsed += GetFrameTime();
        if (IsKeyPressed(KEY_TAB)) debug = !debug;

        // --- Atualiza jogadores ---
        UpdatePlayer(&earthboy, (Rectangle){0, mapTexture.height, mapTexture.width, 100}, KEY_A, KEY_D, KEY_W);
        UpdatePlayer(&fireboy,  (Rectangle){0, mapTexture.height, mapTexture.width, 100}, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&watergirl,(Rectangle){0, mapTexture.height, mapTexture.width, 100}, KEY_J, KEY_L, KEY_I);

        // --- Colisões completas (com degraus e teto bloqueado) ---
        Player* players[3] = {&earthboy, &fireboy, &watergirl};
        for (int p = 0; p < 3; p++) {
            Player* pl = players[p];
            for (int i = 0; i < totalColisoes; i++) {
                Rectangle bloco = colisoes[i].rect;

                if (CheckCollisionRecs(pl->rect, bloco)) {
                    float dx = (pl->rect.x + pl->rect.width / 2) - (bloco.x + bloco.width / 2);
                    float dy = (pl->rect.y + pl->rect.height / 2) - (bloco.y + bloco.height / 2);
                    float overlapX = (pl->rect.width / 2 + bloco.width / 2) - fabsf(dx);
                    float overlapY = (pl->rect.height / 2 + bloco.height / 2) - fabsf(dy);

                    if (overlapX < overlapY) {
                        // --- Tentativa de subir degrau ---
                        if (dy > 0 && pl->velocity.y > 0) {
                            // caindo ao lado do bloco → trata no eixo Y primeiro (evita atravessar lateral)
                        } else {
                            Rectangle teste = pl->rect;
                            teste.y -= STEP_HEIGHT;
                            if (!CheckCollisionRecs(teste, bloco)) {
                                pl->rect.y -= STEP_HEIGHT;
                                goto fim_bloco; // subiu o degrau, evita correção lateral extra
                            }
                        }
                        // --- Bloqueio lateral normal ---
                        if (dx > 0) pl->rect.x += overlapX;
                        else        pl->rect.x -= overlapX;
                        pl->velocity.x = 0;
                    } else {
                        // --- Corrige colisão vertical (teto e chão) ---
                        if (dy > 0 && pl->velocity.y < 0) {
                            // Jogador bateu de baixo (subindo) → teto
                            pl->rect.y += overlapY;
                            pl->velocity.y = 0;
                        }
                        else if (dy < 0 && pl->velocity.y >= 0) {
                            // Jogador caiu sobre o bloco (chão)
                            pl->rect.y -= overlapY;
                            pl->velocity.y = 0;
                            pl->isJumping = false;
                        }
                    }
fim_bloco:
                }
            }
        }

        // --- Interação com lagos: matar/reiniciar se tocar lago errado ---
        for (int p = 0; p < 3; ++p) {
            Player* pl = players[p];
            LakeType elem = (p == 0) ? LAKE_EARTH : (p == 1 ? LAKE_FIRE : LAKE_WATER);
            for (int i = 0; i < lakeCount; ++i) {
                Lake l; l.rect = lakeSegs[i].rect; l.type = lakeSegs[i].type; l.color = (Color){0};
                bool dead = LakeHandlePlayer(&l, pl, elem);
                if (dead) {
                    // Respawn
                    if (p == 0) { pl->rect.x = spawnEarth.x; pl->rect.y = spawnEarth.y; }
                    else if (p == 1) { pl->rect.x = spawnFire.x; pl->rect.y = spawnFire.y; }
                    else { pl->rect.x = spawnWater.x; pl->rect.y = spawnWater.y; }
                    pl->velocity = (Vector2){0,0}; pl->isJumping = false;
                    break;
                }
            }
        }

        // --- Desenho ---
        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode2D(camera);

        DrawTexture(mapTexture, 0, 0, WHITE);

        // Decide quem está dentro do lago correto (para desenhar por trás)
        bool insideOwn[3] = { false, false, false };
        Player* playersArr[3] = { &earthboy, &fireboy, &watergirl };
        LakeType elemsArr[3]  = { LAKE_EARTH, LAKE_FIRE, LAKE_WATER };
        for (int ip = 0; ip < 3; ++ip) {
            Player* pl = playersArr[ip];
            LakeType elem = elemsArr[ip];
            for (int i = 0; i < lakeCount; ++i) {
                const LakeSegment* seg = &lakeSegs[i];
                if (seg->type != elem) continue;
                if (!CheckCollisionRecs(pl->rect, seg->rect)) continue;
                // Requer pequena profundidade vertical para considerar "dentro"
                Rectangle ov = GetCollisionRec(pl->rect, seg->rect);
                float pBottom = pl->rect.y + pl->rect.height;
                if (pBottom > seg->rect.y + 2 && ov.height >= 4) { insideOwn[ip] = true; break; }
            }
        }

        // Avança frames (8 FPS)
        float dt = GetFrameTime();
        LakeAnimFrames* sets[4] = { &animAgua, &animFogo, &animTerra, &animAcido };
        for (int s = 0; s < 4; ++s) {
            sets[s]->timer += dt;
            if (sets[s]->timer >= 0.12f) { sets[s]->timer = 0.0f; sets[s]->frame = (sets[s]->frame + 1) % 64; }
        }

        // 1) Desenha jogadores que estao dentro do lago correto (por trás)
        if (insideOwn[0]) DrawPlayer(earthboy);
        if (insideOwn[1]) DrawPlayer(fireboy);
        if (insideOwn[2]) DrawPlayer(watergirl);

        // 2) Desenha lagos animados por cima
        for (int i = 0; i < lakeCount; ++i) {
            const LakeSegment* seg = &lakeSegs[i];
            const LakeAnimFrames* anim = NULL;
            if (seg->type == LAKE_WATER) anim = &animAgua;
            else if (seg->type == LAKE_FIRE) anim = &animFogo;
            else if (seg->type == LAKE_EARTH) anim = &animTerra;
            else if (seg->type == LAKE_POISON) anim = &animAcido;

            if (anim && ((seg->part==PART_LEFT && anim->leftCount>0) || (seg->part==PART_MIDDLE && anim->middleCount>0) || (seg->part==PART_RIGHT && anim->rightCount>0))) {
                Texture2D frame;
                int idx;
                if (seg->part == PART_LEFT) { idx = (anim->frame % anim->leftCount); frame = anim->left[idx]; }
                else if (seg->part == PART_MIDDLE) { idx = (anim->frame % anim->middleCount); frame = anim->middle[idx]; }
                else { idx = (anim->frame % anim->rightCount); frame = anim->right[idx]; }
                if (seg->part == PART_MIDDLE) {
                    // Tile horizontal com passo de 27px (tamanho do tile do mapa)
                    float tile = seg->rect.height; // 27 no mapa
                    int tiles = (int)floorf(seg->rect.width / tile);
                    float x = seg->rect.x;
                    for (int t = 0; t < tiles; ++t) {
                        Rectangle dst = (Rectangle){ x, seg->rect.y, tile, seg->rect.height };
                        DrawTexturePro(frame, (Rectangle){0,0,(float)frame.width,(float)frame.height}, dst, (Vector2){0,0}, 0.0f, WHITE);
                        x += tile;
                    }
                    // resto (se houver)
                    float rest = seg->rect.width - tiles*tile;
                    if (rest > 0.1f) {
                        Rectangle dst = (Rectangle){ x, seg->rect.y, rest, seg->rect.height };
                        DrawTexturePro(frame, (Rectangle){0,0,(float)frame.width,(float)frame.height}, dst, (Vector2){0,0}, 0.0f, WHITE);
                    }
                } else {
                    DrawTexturePro(frame, (Rectangle){0,0,(float)frame.width,(float)frame.height}, seg->rect, (Vector2){0,0}, 0.0f, WHITE);
                }
            } else {
                // Fallback: desenha sólido com cor se não houver animação
                Color c = (seg->type==LAKE_POISON)? (Color){60,180,60,230} : (Color){90,90,90,200};
                DrawRectangleRec(seg->rect, c);
            }
        }

        // 3) Desenha os demais jogadores por cima dos lagos
        if (!insideOwn[0]) DrawPlayer(earthboy);
        if (!insideOwn[1]) DrawPlayer(fireboy);
        if (!insideOwn[2]) DrawPlayer(watergirl);

        // 4) Desenha a chegada
        GoalDraw(&goal);

        // --- Debug ---
        if (debug) {
            for (int i = 0; i < totalColisoes; i++)
                DrawRectangleLinesEx(colisoes[i].rect, 1, Fade(GREEN, 0.5f));

            DrawText(TextFormat("Earthboy: (%.0f, %.0f)", earthboy.rect.x, earthboy.rect.y), 10, 10, 20, YELLOW);
            DrawText(TextFormat("Fireboy:  (%.0f, %.0f)", fireboy.rect.x, fireboy.rect.y), 10, 35, 20, ORANGE);
            DrawText(TextFormat("Watergirl:(%.0f, %.0f)", watergirl.rect.x, watergirl.rect.y), 10, 60, 20, SKYBLUE);
            DrawText("TAB - Modo Debug", 10, 90, 20, GRAY);
            DrawFPS(10, 120);
        }

        // Checagem de chegada
        if (GoalReached(&goal, &earthboy, &fireboy, &watergirl)) { completed = true; EndMode2D(); EndDrawing(); break; }

        EndMode2D();
        EndDrawing();
    }

    // --- Libera recursos ---
    UnloadTexture(mapTexture);
    UnloadLakeSet(&animAgua);
    UnloadLakeSet(&animFogo);
    UnloadLakeSet(&animTerra);
    UnloadLakeSet(&animAcido);
    UnloadPlayer(&earthboy);
    UnloadPlayer(&fireboy);
    UnloadPlayer(&watergirl);
    if (completed) Ranking_Add(1, Game_GetPlayerName(), elapsed);
    return completed;
}
    
