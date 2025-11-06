#include "fases.h"
#include "../../player/player.h"
#include "../../objects/box.h"
#include "../../objects/seesaw.h"
#include "../../objects/goal.h"
#include "../../objects/button.h"
#include "../../objects/lake.h"
#include "../../objects/fan.h"
#include "../../interface/pause.h"
#include "../../game/game.h"
#include <stdio.h>

// Fase de teste: mistura mecânicas disponíveis para validar integração
bool FaseTeste(void) {
    Rectangle ground = { 0, 800, 1920, 280 };

    // Objetos
    Box box; BoxInit(&box, ground, 600, 80, 80);
    SeesawPlatform seesaw; SeesawInit(&seesaw, (Vector2){ 1000, 650 }, 320.0f, 22.0f, 0.0f);
    Button btn; ButtonInit(&btn, 350, ground.y - 20, 80, 20, (Color){180,180,180,255}, (Color){120,200,120,255});
    Fan fan; FanInit(&fan, 1400, ground.y - 260, 120, 260, 0.7f);
    Lake lakeWater; LakeInit(&lakeWater, 200, ground.y - 20, 200, 20, LAKE_WATER);
    Lake lakeFire;  LakeInit(&lakeFire,  1550, ground.y - 20, 220, 20, LAKE_FIRE);
    Goal goal; GoalInit(&goal, 1820, ground.y - 160, 40, 160, GOLD);

    // Jogadores
    Player fireboy, watergirl, earthboy;
    InitFireboy(&fireboy);    fireboy.rect.x = 120; fireboy.rect.y = 700;
    InitWatergirl(&watergirl); watergirl.rect.x = 220; watergirl.rect.y = 700;
    InitEarthboy(&earthboy);   earthboy.rect.x = 320; earthboy.rect.y = 700;

    // Spawns
    Vector2 spawnFire = (Vector2){ fireboy.rect.x, fireboy.rect.y };
    Vector2 spawnWater = (Vector2){ watergirl.rect.x, watergirl.rect.y };
    Vector2 spawnEarth = (Vector2){ earthboy.rect.x, earthboy.rect.y };

    bool completed = false;
    float elapsed = 0.0f;

    while (!WindowShouldClose()) {
        elapsed += GetFrameTime();

        // Players
        UpdatePlayer(&fireboy, ground, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&watergirl, ground, KEY_A, KEY_D, KEY_W);
        UpdatePlayer(&earthboy, ground, KEY_H, KEY_K, KEY_U);

        // Interações
        BoxHandlePush(&box, &fireboy, KEY_LEFT, KEY_RIGHT);
        BoxHandlePush(&box, &watergirl, KEY_A, KEY_D);
        BoxHandlePush(&box, &earthboy, KEY_H, KEY_K);
        BoxUpdate(&box, ground);

        SeesawBeginFrame(&seesaw);
        SeesawHandlePlayer(&seesaw, &fireboy);
        SeesawHandlePlayer(&seesaw, &watergirl);
        SeesawHandlePlayer(&seesaw, &earthboy);
        SeesawUpdate(&seesaw);

        FanApply(&fan, &fireboy);
        FanApply(&fan, &watergirl);
        FanApply(&fan, &earthboy);

        ButtonUpdate(&btn, &fireboy, &watergirl, &earthboy);

        // Lagos com morte/respawn por elemento incorreto
        if (LakeHandlePlayer(&lakeWater, &fireboy, LAKE_FIRE) ||
            LakeHandlePlayer(&lakeFire, &fireboy, LAKE_FIRE)) {
            fireboy.rect = (Rectangle){ spawnFire.x, spawnFire.y, fireboy.rect.width, fireboy.rect.height };
            fireboy.velocity = (Vector2){0,0}; fireboy.isJumping = false;
        }
        if (LakeHandlePlayer(&lakeWater, &watergirl, LAKE_WATER) ||
            LakeHandlePlayer(&lakeFire, &watergirl, LAKE_WATER)) {
            watergirl.rect = (Rectangle){ spawnWater.x, spawnWater.y, watergirl.rect.width, watergirl.rect.height };
            watergirl.velocity = (Vector2){0,0}; watergirl.isJumping = false;
        }
        if (LakeHandlePlayer(&lakeWater, &earthboy, LAKE_EARTH) ||
            LakeHandlePlayer(&lakeFire, &earthboy, LAKE_EARTH)) {
            earthboy.rect = (Rectangle){ spawnEarth.x, spawnEarth.y, earthboy.rect.width, earthboy.rect.height };
            earthboy.velocity = (Vector2){0,0}; earthboy.isJumping = false;
        }

        if (GoalReached(&goal, &fireboy, &watergirl, &earthboy)) { completed = true; break; }

        // Pause
        if (IsKeyPressed(KEY_ESCAPE)) {
            PauseResult pr = ShowPauseMenu();
            if (pr == PAUSE_TO_MAP) { completed = false; break; }
            if (pr == PAUSE_TO_MENU) { Game_SetReturnToMenu(true); completed = false; break; }
        }

        // Desenho
        BeginDrawing();
        ClearBackground((Color){30, 40, 60, 255});
        DrawText("FASE TESTE", 860, 40, 40, YELLOW);
        char tbuf[32]; int min = (int)(elapsed/60.0f); float sec = elapsed - min*60.0f;
        sprintf(tbuf, "%02d:%05.2f", min, sec);
        DrawText(tbuf, 920, 20, 30, WHITE);

        DrawRectangleRec(ground, BROWN);
        BoxDraw(&box);
        SeesawDraw(&seesaw);
        FanDraw(&fan);
        ButtonDraw(&btn);
        LakeDraw(&lakeWater);
        LakeDraw(&lakeFire);
        GoalDraw(&goal);
        DrawPlayer(fireboy);
        DrawPlayer(watergirl);
        DrawPlayer(earthboy);
        EndDrawing();
    }

    UnloadPlayer(&fireboy);
    UnloadPlayer(&watergirl);
    UnloadPlayer(&earthboy);
    return completed;
}

