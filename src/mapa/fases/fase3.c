#include "raylib.h"
#include "fases.h"
#include "../../player/player.h"
#include "../../objects/lake.h"
#include "../../interface/pause.h"
#include "../../game/game.h"
#include "../../ranking/ranking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_COLISOES 1024
#define MAX_LAKE_SEGS 128
#define STEP_HEIGHT 14.0f

typedef struct { Rectangle rect; } Colisao;

typedef enum { PART_LEFT = 0, PART_MIDDLE, PART_RIGHT } LakePart;

typedef struct {
    Rectangle rect;
    LakeType type;
    LakePart part;
} LakeSegment;

typedef struct LakeAnimFrames {
    Texture2D left[32];   int leftCount;
    Texture2D middle[32]; int middleCount;
    Texture2D right[32];  int rightCount;
    float timer; int frame;
} LakeAnimFrames;

static int ParseRectsFromGroup(const char* tmxPath, const char* groupName, Rectangle* out, int cap) {
    int count = 0;
    char* xml = LoadFileText(tmxPath);
    if (!xml) return 0;

    const char* search = xml;
    while ((search = strstr(search, "<objectgroup")) != NULL) {
        const char* tagClose = strchr(search, '>');
        if (!tagClose) break;

        bool match = false;
        size_t len = (size_t)(tagClose - search);
        char* header = (char*)malloc(len + 1);
        if (!header) { UnloadFileText(xml); return count; }
        memcpy(header, search, len);
        header[len] = '\0';
        char findName[128];
        snprintf(findName, sizeof(findName), "name=\"%s\"", groupName);
        if (strstr(header, findName)) match = true;
        free(header);

        const char* groupEnd = strstr(tagClose + 1, "</objectgroup>");
        if (!groupEnd) break;

        if (match) {
            const char* p = tagClose + 1;
            while (p < groupEnd) {
                const char* obj = strstr(p, "<object ");
                if (!obj || obj >= groupEnd) break;
                float x=0,y=0,w=0,h=0;
                sscanf(obj, "<object id=%*[^x]x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\"", &x,&y,&w,&h);
                if (w>0 && h>0 && count < cap) out[count++] = (Rectangle){x,y,w,h};
                p = obj + 8;
            }
        }

        search = groupEnd ? (groupEnd + 14) : (search + 11);
    }

    UnloadFileText(xml);
    return count;
}

static void AddCollisionGroup(const char* tmxPath, const char* name, Colisao* col, int* count, int cap) {
    Rectangle rects[64];
    int n = ParseRectsFromGroup(tmxPath, name, rects, 64);
    for (int i = 0; i < n && *count < cap; ++i) col[(*count)++].rect = rects[i];
}

static void AddLakeSegments(const char* tmxPath, const char* name, LakeType type, LakePart part,
                            LakeSegment* segs, int* count, int cap) {
    Rectangle rects[64];
    int n = ParseRectsFromGroup(tmxPath, name, rects, 64);
    for (int i = 0; i < n && *count < cap; ++i) {
        segs[*count].rect = rects[i];
        segs[*count].type = type;
        segs[*count].part = part;
        (*count)++;
    }
}

static int LoadFramesRange(Texture2D* arr, int max, const char* pattern, int startIdx, int endIdx) {
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
    s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/agua/esquerdo/pixil-frame-%d.png",  0, 15);
    s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/agua/meio/pixil-frame-%d.png",      0, 15);
    s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/agua/direito/pixil-frame-%d.png",   0, 15);
    s->timer = 0.0f; s->frame = 0;
}

static void LoadLakeSet_Terra(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/terra/esquerdo/pixil-frame-%d.png", 0, 15);
    s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/terra/meio/pixil-frame-%d.png",     0, 15);
    s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/terra/direito/pixil-frame-%d.png",  0, 15);
    s->timer = 0.0f; s->frame = 0;
}

static void LoadLakeSet_Fogo(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/fogo/esquerdo/Esquerda%d.png", 1, 32);
    if (s->leftCount == 0)   s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/fogo/esquerdo/pixil-frame-%d.png", 0, 31);
    s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/fogo/meio/Meio%d.png",     1, 32);
    if (s->middleCount == 0) s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/fogo/meio/pixil-frame-%d.png",      0, 31);
    s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/fogo/direito/Direita%d.png", 1, 32);
    if (s->rightCount == 0)  s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/fogo/direito/pixil-frame-%d.png",   0, 31);
    s->timer = 0.0f; s->frame = 0;
}

static void LoadLakeSet_Acido(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/acido/esquerdo/pixil-frame-%d.png",  0, 15);
    s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/acido/meio/pixil-frame-%d.png",      0, 15);
    s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/acido/direito/pixil-frame-%d.png",   0, 15);
    s->timer = 0.0f; s->frame = 0;
}

static void UnloadLakeSet(LakeAnimFrames* s) {
    for (int i = 0; i < s->leftCount;   ++i) UnloadTexture(s->left[i]);
    for (int i = 0; i < s->middleCount; ++i) UnloadTexture(s->middle[i]);
    for (int i = 0; i < s->rightCount;  ++i) UnloadTexture(s->right[i]);
    s->leftCount = s->middleCount = s->rightCount = 0;
}

static bool CheckDoor(const Rectangle* door, const Player* p) {
    if (door->width <= 0 || door->height <= 0) return false;
    return CheckCollisionRecs(*door, p->rect);
}

bool Fase3(void) {
    const char* tmxPath = "assets/maps/fase3/fase3.tmx";
    Texture2D mapTexture = LoadTexture("assets/maps/fase3/fase3.png");
 

    Colisao colisoes[MAX_COLISOES];
    int totalColisoes = 0;
    AddCollisionGroup(tmxPath, "colisao", colisoes, &totalColisoes, MAX_COLISOES);

    LakeSegment lakeSegs[MAX_LAKE_SEGS];
    int lakeSegCount = 0;
    AddLakeSegments(tmxPath, "aguameio",    LAKE_WATER, PART_MIDDLE, lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "aguaesquerda",LAKE_WATER, PART_LEFT,   lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "aguadireita", LAKE_WATER, PART_RIGHT,  lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "fogomeio",    LAKE_FIRE,  PART_MIDDLE, lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "fogoesquerda",LAKE_FIRE,  PART_LEFT,   lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "fogodireita", LAKE_FIRE,  PART_RIGHT,  lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "terrameio",   LAKE_EARTH, PART_MIDDLE, lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "terraesquerda",LAKE_EARTH,PART_LEFT,   lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "terradireita",LAKE_EARTH, PART_RIGHT,  lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "veneno",      LAKE_POISON,PART_MIDDLE, lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);

    LakeAnimFrames animAgua = {0}, animFogo = {0}, animTerra = {0}, animAcido = {0};
    LoadLakeSet_Agua(&animAgua);
    LoadLakeSet_Fogo(&animFogo);
    LoadLakeSet_Terra(&animTerra);
    LoadLakeSet_Acido(&animAcido);

    Rectangle spawns[4];
    Vector2 spawnWater = { 300, 700 };
    Vector2 spawnFire  = { 350, 700 };
    Vector2 spawnEarth = { 400, 700 };
    if (ParseRectsFromGroup(tmxPath, "spawnAgua", spawns, 4) > 0)
        spawnWater = (Vector2){ spawns[0].x, spawns[0].y };
    if (ParseRectsFromGroup(tmxPath, "spawnFogo", spawns, 4) > 0)
        spawnFire = (Vector2){ spawns[0].x, spawns[0].y };
    if (ParseRectsFromGroup(tmxPath, "spawnTerra", spawns, 4) > 0)
        spawnEarth = (Vector2){ spawns[0].x, spawns[0].y };

    Rectangle doorWater = {0}, doorFire = {0}, doorEarth = {0};
    if (ParseRectsFromGroup(tmxPath, "portaAgua", &doorWater, 1) == 0)
        doorWater = (Rectangle){ mapTexture.width - 90.0f, mapTexture.height - 180.0f, 30.0f, 120.0f };
    if (ParseRectsFromGroup(tmxPath, "portaFogo", &doorFire, 1) == 0)
        doorFire = (Rectangle){ mapTexture.width - 150.0f, mapTexture.height - 180.0f, 30.0f, 120.0f };
    if (ParseRectsFromGroup(tmxPath, "portaTerra", &doorEarth, 1) == 0)
        doorEarth = (Rectangle){ mapTexture.width - 210.0f, mapTexture.height - 180.0f, 30.0f, 120.0f };

    Player watergirl, fireboy, earthboy;
    InitWatergirl(&watergirl);
    InitFireboy(&fireboy);
    InitEarthboy(&earthboy);

    watergirl.rect = (Rectangle){ spawnWater.x, spawnWater.y, 45, 50 };
    fireboy.rect    = (Rectangle){ spawnFire.x,  spawnFire.y,  45, 50 };
    earthboy.rect   = (Rectangle){ spawnEarth.x, spawnEarth.y, 45, 50 };

    Camera2D camera = {0};
    camera.target = (Vector2){ mapTexture.width/2.0f, mapTexture.height/2.0f };
    camera.offset = (Vector2){ GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };
    camera.zoom = 1.0f;

    bool reachedWater=false, reachedFire=false, reachedEarth=false;
    bool debug=false, completed=false;
    float elapsed=0.0f;
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        elapsed += dt;
        if (IsKeyPressed(KEY_TAB)) debug = !debug;

        if (IsKeyPressed(KEY_ESCAPE)) {
            PauseResult pr = ShowPauseMenu();
            if (pr == PAUSE_TO_MAP) { completed = false; break; }
            if (pr == PAUSE_TO_MENU) { Game_SetReturnToMenu(true); completed = false; break; }
        }

        UpdatePlayer(&watergirl, (Rectangle){0,mapTexture.height,mapTexture.width,200}, KEY_J, KEY_L, KEY_I);
        UpdatePlayer(&fireboy,   (Rectangle){0,mapTexture.height,mapTexture.width,200}, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&earthboy,  (Rectangle){0,mapTexture.height,mapTexture.width,200}, KEY_A, KEY_D, KEY_W);

        Player* players[3] = { &earthboy, &fireboy, &watergirl };
        for (int p = 0; p < 3; ++p) {
            Player* pl = players[p];
            for (int i = 0; i < totalColisoes; ++i) {
                Rectangle bloco = colisoes[i].rect;
                if (!CheckCollisionRecs(pl->rect, bloco)) continue;
                float dx = (pl->rect.x + pl->rect.width/2) - (bloco.x + bloco.width/2);
                float dy = (pl->rect.y + pl->rect.height/2) - (bloco.y + bloco.height/2);
                float overlapX = (pl->rect.width/2 + bloco.width/2) - fabsf(dx);
                float overlapY = (pl->rect.height/2 + bloco.height/2) - fabsf(dy);
                if (overlapX < overlapY) {
                    Rectangle teste = pl->rect; teste.y -= STEP_HEIGHT;
                    if (!(dy > 0 && pl->velocity.y > 0) && !CheckCollisionRecs(teste, bloco)) {
                        pl->rect.y -= STEP_HEIGHT;
                        continue;
                    }
                    if (dx > 0) pl->rect.x += overlapX;
                    else        pl->rect.x -= overlapX;
                    pl->velocity.x = 0;
                } else {
                    if (dy > 0 && pl->velocity.y < 0) {
                        pl->rect.y += overlapY;
                        pl->velocity.y = 0;
                    } else if (dy < 0 && pl->velocity.y >= 0) {
                        pl->rect.y -= overlapY;
                        pl->velocity.y = 0;
                        pl->isJumping = false;
                    }
                }
            }
        }

        for (int p = 0; p < 3; ++p) {
            Player* pl = players[p];
            LakeType elem = (p == 0) ? LAKE_EARTH : (p == 1 ? LAKE_FIRE : LAKE_WATER);
            for (int i = 0; i < lakeSegCount; ++i) {
                Lake temp; temp.rect = lakeSegs[i].rect; temp.type = lakeSegs[i].type;
                if (LakeHandlePlayer(&temp, pl, elem)) {
                    if (p == 0) { pl->rect.x = spawnEarth.x; pl->rect.y = spawnEarth.y; }
                    else if (p == 1) { pl->rect.x = spawnFire.x; pl->rect.y = spawnFire.y; }
                    else { pl->rect.x = spawnWater.x; pl->rect.y = spawnWater.y; }
                    pl->velocity = (Vector2){0,0}; pl->isJumping = false;
                    break;
                }
            }
        }

        reachedWater = reachedWater || CheckDoor(&doorWater, &watergirl);
        reachedFire  = reachedFire  || CheckDoor(&doorFire,  &fireboy);
        reachedEarth = reachedEarth || CheckDoor(&doorEarth, &earthboy);
        if (reachedWater && reachedFire && reachedEarth) { completed = true; break; }

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode2D(camera);

        DrawTexture(mapTexture, 0, 0, WHITE);

        float lakeDt = dt;
        LakeAnimFrames* sets[4] = { &animAgua, &animFogo, &animTerra, &animAcido };
        for (int s=0;s<4;++s) {
            sets[s]->timer += lakeDt;
            if (sets[s]->timer >= 0.12f) {
                sets[s]->timer = 0.0f;
                sets[s]->frame = (sets[s]->frame + 1) % 64;
            }
        }

        for (int i = 0; i < lakeSegCount; ++i) {
            const LakeSegment* seg = &lakeSegs[i];
            const LakeAnimFrames* anim = NULL;
            switch (seg->type) {
                case LAKE_WATER: anim = &animAgua; break;
                case LAKE_FIRE: anim = &animFogo; break;
                case LAKE_EARTH: anim = &animTerra; break;
                case LAKE_POISON: anim = &animAcido; break;
            }
            Texture2D frame = (Texture2D){0};
            bool has = false;
            if (anim) {
                if (seg->part == PART_LEFT   && anim->leftCount  > 0) { frame = anim->left[ anim->frame % anim->leftCount  ]; has = true; }
                if (seg->part == PART_MIDDLE && anim->middleCount> 0) { frame = anim->middle[anim->frame % anim->middleCount]; has = true; }
                if (seg->part == PART_RIGHT  && anim->rightCount > 0) { frame = anim->right[ anim->frame % anim->rightCount ]; has = true; }
            }
            if (has && seg->part == PART_MIDDLE) {
                float tile = seg->rect.height;
                int tiles = (int)floorf(seg->rect.width / tile);
                float x = seg->rect.x;
                for (int t=0;t<tiles;++t) {
                    Rectangle dst = { x, seg->rect.y, tile, seg->rect.height };
                    DrawTexturePro(frame, (Rectangle){0,0,(float)frame.width,(float)frame.height}, dst, (Vector2){0,0}, 0.0f, WHITE);
                    x += tile;
                }
                float rest = seg->rect.width - tiles*tile;
                if (rest > 0.1f) {
                    Rectangle dst = { x, seg->rect.y, rest, seg->rect.height };
                    DrawTexturePro(frame, (Rectangle){0,0,(float)frame.width,(float)frame.height}, dst, (Vector2){0,0}, 0.0f, WHITE);
                }
            } else if (has) {
                DrawTexturePro(frame, (Rectangle){0,0,(float)frame.width,(float)frame.height}, seg->rect, (Vector2){0,0}, 0.0f, WHITE);
            } else {
                Lake fallback; LakeInit(&fallback, seg->rect.x, seg->rect.y, seg->rect.width, seg->rect.height, seg->type);
                LakeDraw(&fallback);
            }
        }

        Color cWater = reachedWater ? SKYBLUE : Fade(SKYBLUE, 0.6f);
        Color cFire  = reachedFire  ? ORANGE : Fade(ORANGE, 0.6f);
        Color cEarth = reachedEarth ? BROWN  : Fade(BROWN, 0.6f);
        DrawRectangleLinesEx(doorWater, 2, cWater);
        DrawRectangleLinesEx(doorFire,  2, cFire);
        DrawRectangleLinesEx(doorEarth, 2, cEarth);

        DrawPlayer(earthboy);
        DrawPlayer(fireboy);
        DrawPlayer(watergirl);

        if (debug) {
            for (int i=0;i<totalColisoes;i++) DrawRectangleLinesEx(colisoes[i].rect,1,Fade(GREEN,0.5f));
            for (int i=0;i<lakeSegCount;i++) DrawRectangleLinesEx(lakeSegs[i].rect,1,Fade(BLUE,0.4f));
            DrawFPS(10,10);
        }

        EndMode2D();

        char timer[32];
        int min = (int)(elapsed / 60.0f);
        float sec = elapsed - min * 60.0f;
        sprintf(timer, "%02d:%05.2f", min, sec);
        DrawText(timer, 30, 30, 32, WHITE);
        DrawText("Leve cada personagem para sua porta correspondente", 30, 70, 20, RAYWHITE);

        EndDrawing();
    }

    UnloadTexture(mapTexture);
    UnloadLakeSet(&animAgua);
    UnloadLakeSet(&animFogo);
    UnloadLakeSet(&animTerra);
    UnloadLakeSet(&animAcido);
    UnloadPlayer(&earthboy);
    UnloadPlayer(&fireboy);
    UnloadPlayer(&watergirl);
    if (completed) Ranking_Add(5, Game_GetPlayerName(), elapsed);
    return completed;
}
