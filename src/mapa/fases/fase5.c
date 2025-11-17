#include "raylib.h"
#include "../../player/player.h"
#include "../../objects/lake.h"
#include "../../ranking/ranking.h"
#include "../../game/game.h"
#include "../../audio/theme.h"
#include "../../interface/pause.h"
#include "phase_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_COLISOES 1024
#define MAX_LAKE_SEGS 512

static bool StrContains(const char* hay, const char* needle) {
    return strstr(hay, needle) != NULL;
}

static LakeType DetectLakeType(const char* nameLower) {
    if (StrContains(nameLower, "agua")) return LAKE_WATER;
    if (StrContains(nameLower, "fogo")) return LAKE_FIRE;
    if (StrContains(nameLower, "marrom") || StrContains(nameLower, "terra")) return LAKE_EARTH;
    if (StrContains(nameLower, "verde") || StrContains(nameLower, "veneno") || StrContains(nameLower, "acido")) return LAKE_POISON;
    return LAKE_WATER;
}

static LakePart DetectLakePart(const char* nameLower) {
    if (StrContains(nameLower, "esq")) return PART_LEFT;
    if (StrContains(nameLower, "dir")) return PART_RIGHT;
    return PART_MIDDLE;
}

static int ParseLakeSegments(const char* tmxPath, LakeSegment* out, int cap) {
    int count = 0;
    char* xml = LoadFileText(tmxPath);
    if (!xml) return 0;

    const char* search = xml;
    while ((search = strstr(search, "<objectgroup")) != NULL) {
        const char* tagClose = strchr(search, '>');
        if (!tagClose) break;

        const char* groupEnd = strstr(tagClose + 1, "</objectgroup>");
        if (!groupEnd) break;

        size_t len = (size_t)(tagClose - search);
        char* header = (char*)malloc(len + 1);
        if (!header) { UnloadFileText(xml); return count; }
        memcpy(header, search, len);
        header[len] = '\0';

        const char* nameAttr = strstr(header, "name=\"");
        char nameBuf[128] = {0};
        if (nameAttr) {
            nameAttr += 6;
            const char* endQuote = strchr(nameAttr, '"');
            if (endQuote) {
                size_t nameLen = (size_t)(endQuote - nameAttr);
                if (nameLen >= sizeof(nameBuf)) nameLen = sizeof(nameBuf) - 1;
                memcpy(nameBuf, nameAttr, nameLen);
                nameBuf[nameLen] = '\0';
            }
        }
        free(header);

        if (nameBuf[0] == '\0') { search = groupEnd + 1; continue; }

        char nameLower[128];
        PhaseToLowerCopy(nameBuf, nameLower, sizeof(nameLower));
        if (StrContains(nameLower, "spawn") || StrContains(nameLower, "porta")) {
            search = groupEnd + 1;
            continue;
        }
        if (!StrContains(nameLower, "lago") && !StrContains(nameLower, "agua") &&
            !StrContains(nameLower, "fogo") && !StrContains(nameLower, "terra") &&
            !StrContains(nameLower, "veneno")) {
            search = groupEnd + 1;
            continue;
        }

        LakeType type = DetectLakeType(nameLower);
        LakePart part = DetectLakePart(nameLower);

        const char* p = tagClose + 1;
        while (p < groupEnd) {
            const char* obj = strstr(p, "<object ");
            if (!obj || obj >= groupEnd) break;
            float x=0,y=0,w=0,h=0;
            sscanf(obj, "<object id=%*[^x]x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\"", &x,&y,&w,&h);
            if (w>0 && h>0 && count < cap) {
                out[count++] = (LakeSegment){ (Rectangle){x,y,w,h}, type, part };
            }
            p = obj + 1;
        }

        search = groupEnd + 1;
    }

    UnloadFileText(xml);
    return count;
}

static void DrawLakes(const LakeSegment* segs, int count,
                      const LakeAnimFrames* agua,
                      const LakeAnimFrames* fogo,
                      const LakeAnimFrames* terra,
                      const LakeAnimFrames* acido) {
    for (int i = 0; i < count; ++i) {
        const LakeSegment* seg = &segs[i];
        const LakeAnimFrames* anim = NULL;
        if (seg->type == LAKE_WATER) anim = agua;
        else if (seg->type == LAKE_FIRE) anim = fogo;
        else if (seg->type == LAKE_EARTH) anim = terra;
        else if (seg->type == LAKE_POISON) anim = acido;

        const Texture2D* frame = anim ? PhasePickLakeFrame(anim, seg->part) : NULL;
        if (frame && frame->id != 0) {
            if (seg->part == PART_MIDDLE) {
                float tile = seg->rect.height;
                int tiles = (int)floorf(seg->rect.width / tile);
                float x = seg->rect.x;
                for (int t = 0; t < tiles; ++t) {
                    Rectangle dst = { x, seg->rect.y, tile, seg->rect.height };
                    DrawTexturePro(*frame, (Rectangle){0,0,(float)frame->width,(float)frame->height},
                                   dst, (Vector2){0,0}, 0.0f, WHITE);
                    x += tile;
                }
                float rest = seg->rect.width - tiles * tile;
                if (rest > 0.1f) {
                    Rectangle dst = { x, seg->rect.y, rest, seg->rect.height };
                    DrawTexturePro(*frame, (Rectangle){0,0,(float)frame->width,(float)frame->height},
                                   dst, (Vector2){0,0}, 0.0f, WHITE);
                }
            } else {
                DrawTexturePro(*frame, (Rectangle){0,0,(float)frame->width,(float)frame->height},
                               seg->rect, (Vector2){0,0}, 0.0f, WHITE);
            }
        } else {
            Color fallback = (seg->type == LAKE_POISON) ? (Color){60,180,60,230} : (Color){90,90,90,200};
            DrawRectangleRec(seg->rect, fallback);
        }
    }
}

static Vector2 CollectSpawnCenter(const char* tmx, const char* name, Vector2 fallback) {
    Rectangle rect[2];
    if (ParseRectsFromGroup(tmx, name, rect, 2) > 0) {
        return (Vector2){
            rect[0].x + (rect[0].width - PLAYER_HITBOX_WIDTH) * 0.5f,
            rect[0].y + rect[0].height - PLAYER_HITBOX_HEIGHT
        };
    }
    return fallback;
}

static Rectangle CollectDoor(const char* tmx, const char* name) {
    Rectangle rect[2];
    if (ParseRectsFromGroup(tmx, name, rect, 2) > 0) return rect[0];
    return (Rectangle){0};
}

static bool PlayersAtDoors(const Rectangle* doorTerra, const Rectangle* doorFogo,
                           const Rectangle* doorAgua, const Player* earthboy,
                           const Player* fireboy, const Player* watergirl) {
    return PhaseCheckDoor(doorTerra, earthboy) &&
           PhaseCheckDoor(doorFogo, fireboy) &&
           PhaseCheckDoor(doorAgua, watergirl);
}

bool Fase5(void) {
    const char* tmx = "assets/maps/fase5/fase5.tmx";
    Texture2D mapTex = LoadTexture("assets/maps/fase5/fase5.png");
    if (mapTex.id == 0) {
        printf("Erro: nao consegui carregar assets/maps/fase5/fase5.png\n");
        return false;
    }

    Colisao colisas[MAX_COLISOES];
    int colCount = 0;
    const char* colNames[] = { "Colisao", "Colis\u00e3o", "Colisoes", "Colis\u00f5es", "colisao", "colisao_fase5" };
    Rectangle buffer[MAX_COLISOES];
    int found = ParseRectsFromAny(tmx, colNames, (int)(sizeof(colNames)/sizeof(colNames[0])), buffer, MAX_COLISOES);
    for (int i = 0; i < found && colCount < MAX_COLISOES; ++i) {
        colisas[colCount++].rect = buffer[i];
    }

    LakeSegment lakes[MAX_LAKE_SEGS];
    int lakeCount = ParseLakeSegments(tmx, lakes, MAX_LAKE_SEGS);

    LakeAnimFrames animAgua = {0}, animFogo = {0}, animTerra = {0}, animAcido = {0};
    LoadLakeSet_Agua(&animAgua);
    LoadLakeSet_Fogo(&animFogo);
    LoadLakeSet_Terra(&animTerra);
    LoadLakeSet_Acido(&animAcido);

    Vector2 spawnEarthPos = CollectSpawnCenter(tmx, "spawnTerra", (Vector2){300, mapTex.height - 120});
    Vector2 spawnFirePos  = CollectSpawnCenter(tmx, "spawnFogo",  (Vector2){400, mapTex.height - 120});
    Vector2 spawnWaterPos = CollectSpawnCenter(tmx, "spawnAgua",  (Vector2){500, mapTex.height - 120});

    Rectangle doorEarth = CollectDoor(tmx, "portaTerra");
    Rectangle doorFire  = CollectDoor(tmx, "portaFogo");
    Rectangle doorWater = CollectDoor(tmx, "portaAgua");

    Player earthboy, fireboy, watergirl;
    InitEarthboy(&earthboy);
    InitFireboy(&fireboy);
    InitWatergirl(&watergirl);
    earthboy.rect.x = spawnEarthPos.x; earthboy.rect.y = spawnEarthPos.y;
    fireboy.rect.x  = spawnFirePos.x;  fireboy.rect.y  = spawnFirePos.y;
    watergirl.rect.x= spawnWaterPos.x; watergirl.rect.y= spawnWaterPos.y;

    Camera2D cam = {0};
    cam.target = (Vector2){mapTex.width / 2.0f, mapTex.height / 2.0f};
    cam.offset = (Vector2){GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
    cam.zoom = 1.0f;

    bool completed = false;
    bool debug = false;
    float elapsed = 0.0f;
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        Theme_Update();
        float dt = GetFrameTime();
        elapsed += dt;
        if (IsKeyPressed(KEY_TAB)) debug = !debug;

        if (IsKeyPressed(KEY_ESCAPE)) {
            PauseResult pr = ShowPauseMenu();
            if (pr == PAUSE_TO_MAP) { completed = false; break; }
            if (pr == PAUSE_TO_MENU) { Game_SetReturnToMenu(true); completed = false; break; }
        }

        UpdatePlayer(&earthboy, (Rectangle){0, mapTex.height, mapTex.width, 200}, KEY_J, KEY_L, KEY_I);
        UpdatePlayer(&fireboy,  (Rectangle){0, mapTex.height, mapTex.width, 200}, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&watergirl,(Rectangle){0, mapTex.height, mapTex.width, 200}, KEY_A, KEY_D, KEY_W);

        Player* players[3] = { &earthboy, &fireboy, &watergirl };
        PhaseResolvePlayersVsWorld(players, 3, colisas, colCount, PHASE_STEP_HEIGHT);

        bool respawnAll = false;
        for (int p = 0; p < 3 && !respawnAll; ++p) {
            Player* pl = players[p];
            LakeType target = (p == 0) ? LAKE_EARTH : (p == 1 ? LAKE_FIRE : LAKE_WATER);
            for (int i = 0; i < lakeCount; ++i) {
                Lake temp = { lakes[i].rect, lakes[i].type, (Color){0} };
                if (LakeHandlePlayer(&temp, pl, target)) {
                    respawnAll = true;
                    break;
                }
            }
        }
        if (respawnAll) {
            earthboy.rect.x = spawnEarthPos.x; earthboy.rect.y = spawnEarthPos.y;
            fireboy.rect.x  = spawnFirePos.x;  fireboy.rect.y  = spawnFirePos.y;
            watergirl.rect.x= spawnWaterPos.x; watergirl.rect.y= spawnWaterPos.y;
            earthboy.velocity = fireboy.velocity = watergirl.velocity = (Vector2){0,0};
            earthboy.isJumping = fireboy.isJumping = watergirl.isJumping = false;
            continue;
        }

        bool finishedByDoors = PlayersAtDoors(&doorEarth, &doorFire, &doorWater, &earthboy, &fireboy, &watergirl);

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode2D(cam);
        DrawTexture(mapTex, 0, 0, WHITE);

        bool insideOwn[3] = { false, false, false };
        for (int i = 0; i < 3; ++i) {
            LakeType target = (i == 0) ? LAKE_EARTH : (i == 1 ? LAKE_FIRE : LAKE_WATER);
            insideOwn[i] = PhasePlayerInsideOwnLake(players[i], target, lakes, lakeCount);
        }

        LakeAnimFrames* sets[4] = { &animAgua, &animFogo, &animTerra, &animAcido };
        PhaseUpdateLakeAnimations(sets, 4, dt, 0.12f);

        if (insideOwn[0]) DrawPlayer(earthboy);
        if (insideOwn[1]) DrawPlayer(fireboy);
        if (insideOwn[2]) DrawPlayer(watergirl);

        DrawLakes(lakes, lakeCount, &animAgua, &animFogo, &animTerra, &animAcido);

        if (!insideOwn[0]) DrawPlayer(earthboy);
        if (!insideOwn[1]) DrawPlayer(fireboy);
        if (!insideOwn[2]) DrawPlayer(watergirl);

        if (debug) {
            for (int i = 0; i < colCount; ++i) DrawRectangleLinesEx(colisas[i].rect, 1, Fade(GREEN, 0.5f));
            if (doorEarth.width > 0 && doorEarth.height > 0) DrawRectangleLinesEx(doorEarth, 1, Fade(BROWN, 0.6f));
            if (doorFire.width > 0 && doorFire.height > 0) DrawRectangleLinesEx(doorFire, 1, Fade(RED, 0.6f));
            if (doorWater.width > 0 && doorWater.height > 0) DrawRectangleLinesEx(doorWater, 1, Fade(BLUE, 0.6f));
        }

        EndMode2D();
        DrawText(TextFormat("Tempo: %05.2f", elapsed), 30, 30, 26, WHITE);
        EndDrawing();

        if (finishedByDoors) { completed = true; break; }
    }

    UnloadTexture(mapTex);
    PhaseUnloadLakeSet(&animAgua);
    PhaseUnloadLakeSet(&animFogo);
    PhaseUnloadLakeSet(&animTerra);
    PhaseUnloadLakeSet(&animAcido);
    UnloadPlayer(&earthboy);
    UnloadPlayer(&fireboy);
    UnloadPlayer(&watergirl);

    if (completed) Ranking_Add(5, Game_GetPlayerName(), elapsed);
    return completed;
}
