#include "raylib.h"
#include "fases.h"
#include "../../player/player.h"
#include "../../objects/lake.h"
#include "../../interface/pause.h"
#include "../../game/game.h"
#include "../../ranking/ranking.h"
#include "../../audio/theme.h"
#include "phase_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_COLISOES 1024
#define MAX_LAKE_SEGS 128

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

    watergirl.rect = (Rectangle){ spawnWater.x, spawnWater.y, PLAYER_HITBOX_WIDTH, PLAYER_HITBOX_HEIGHT };
    fireboy.rect    = (Rectangle){ spawnFire.x,  spawnFire.y,  PLAYER_HITBOX_WIDTH, PLAYER_HITBOX_HEIGHT };
    earthboy.rect   = (Rectangle){ spawnEarth.x, spawnEarth.y, PLAYER_HITBOX_WIDTH, PLAYER_HITBOX_HEIGHT };

    Camera2D camera = {0};
    camera.target = (Vector2){ mapTexture.width/2.0f, mapTexture.height/2.0f };
    camera.offset = (Vector2){ GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };
    camera.zoom = 1.0f;

    bool reachedWater=false, reachedFire=false, reachedEarth=false;
    bool debug=false, completed=false;
    float elapsed=0.0f;
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

        UpdatePlayer(&watergirl, (Rectangle){0,mapTexture.height,mapTexture.width,200}, KEY_A, KEY_D, KEY_W);
        UpdatePlayer(&fireboy,   (Rectangle){0,mapTexture.height,mapTexture.width,200}, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&earthboy,  (Rectangle){0,mapTexture.height,mapTexture.width,200}, KEY_J, KEY_L, KEY_I);

        Player* players[3] = { &earthboy, &fireboy, &watergirl };
        PhaseResolvePlayersVsWorld(players, 3, colisoes, totalColisoes, PHASE_STEP_HEIGHT);

        bool respawnAll = false;
        for (int p = 0; p < 3 && !respawnAll; ++p) {
            Player* pl = players[p];
            LakeType elem = (p == 0) ? LAKE_EARTH : (p == 1 ? LAKE_FIRE : LAKE_WATER);
            for (int i = 0; i < lakeSegCount; ++i) {
                Lake temp; temp.rect = lakeSegs[i].rect; temp.type = lakeSegs[i].type;
                if (LakeHandlePlayer(&temp, pl, elem)) {
                    respawnAll = true;
                    break;
                }
            }
        }
        if (respawnAll) {
            earthboy.rect.x = spawnEarth.x; earthboy.rect.y = spawnEarth.y;
            fireboy.rect.x  = spawnFire.x;  fireboy.rect.y  = spawnFire.y;
            watergirl.rect.x= spawnWater.x; watergirl.rect.y= spawnWater.y;
            earthboy.velocity = fireboy.velocity = watergirl.velocity = (Vector2){0,0};
            earthboy.isJumping = fireboy.isJumping = watergirl.isJumping = false;
            continue;
        }

        reachedWater = reachedWater || PhaseCheckDoor(&doorWater, &watergirl);
        reachedFire  = reachedFire  || PhaseCheckDoor(&doorFire,  &fireboy);
        reachedEarth = reachedEarth || PhaseCheckDoor(&doorEarth, &earthboy);
        if (reachedWater && reachedFire && reachedEarth) { completed = true; break; }

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode2D(camera);

        DrawTexture(mapTexture, 0, 0, WHITE);

        LakeAnimFrames* sets[4] = { &animAgua, &animFogo, &animTerra, &animAcido };
        PhaseUpdateLakeAnimations(sets, 4, dt, 0.12f);

        for (int i = 0; i < lakeSegCount; ++i) {
            const LakeSegment* seg = &lakeSegs[i];
            const LakeAnimFrames* anim = NULL;
            switch (seg->type) {
                case LAKE_WATER: anim = &animAgua; break;
                case LAKE_FIRE: anim = &animFogo; break;
                case LAKE_EARTH: anim = &animTerra; break;
                case LAKE_POISON: anim = &animAcido; break;
            }
            const Texture2D* frameTex = PhasePickLakeFrame(anim, seg->part);
            if (frameTex && frameTex->id != 0 && seg->part == PART_MIDDLE) {
                Texture2D frame = *frameTex;
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
            } else if (frameTex && frameTex->id != 0) {
                Texture2D frame = *frameTex;
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
    PhaseUnloadLakeSet(&animAgua);
    PhaseUnloadLakeSet(&animFogo);
    PhaseUnloadLakeSet(&animTerra);
    PhaseUnloadLakeSet(&animAcido);
    UnloadPlayer(&earthboy);
    UnloadPlayer(&fireboy);
    UnloadPlayer(&watergirl);
    if (completed) Ranking_Add(3, Game_GetPlayerName(), elapsed);
    return completed;
}
