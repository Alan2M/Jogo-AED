#include "fases.h"
#include "../../player/player.h"
#include "../../objects/box.h"
#include "../../objects/button.h"
#include "../../objects/lake.h"
#include "../../objects/fan.h"
#include "../../interface/pause.h"
#include "../../game/game.h"
#include "../../objects/goal.h"
#include "../../ranking/ranking.h"
#include <stdio.h>

bool Fase1(void) {
    Rectangle ground = { 0, 800, 1920, 280 };
    Box box; BoxInit(&box, ground, 900, 80, 80);
    // Botão sobre o chão (próximo do meio)
    Button btn; ButtonInit(&btn, 600, ground.y - 20, 80, 20, (Color){180,180,180,255}, (Color){120,200,120,255});
    // Lagos (no topo do chão)
    Lake lakeWater; LakeInit(&lakeWater, 400, ground.y - 20, 200, 20, LAKE_WATER);
    Lake lakeFire;  LakeInit(&lakeFire,  750, ground.y - 20, 200, 20, LAKE_FIRE);
    Lake lakeEarth; LakeInit(&lakeEarth, 1100, ground.y - 20, 200, 20, LAKE_EARTH);
    // Ventilador: coluna de ar que empurra para cima
    Fan fan; FanInit(&fan, 300, ground.y - 220, 120, 220, 0.6f);
    Goal goal; GoalInit(&goal, 1820, ground.y - 120, 40, 120, GREEN);

    Player fireboy, watergirl, earthboy;
    InitFireboy(&fireboy);
    InitWatergirl(&watergirl);
    InitEarthboy(&earthboy);

    bool completed = false;
    float elapsed = 0.0f;
    // Spawns (para respawn ao morrer no lago errado)
    Vector2 spawnFire = (Vector2){ fireboy.rect.x, fireboy.rect.y };
    Vector2 spawnWater = (Vector2){ watergirl.rect.x, watergirl.rect.y };
    Vector2 spawnEarth = (Vector2){ earthboy.rect.x, earthboy.rect.y };

    while (!WindowShouldClose()) {
        elapsed += GetFrameTime();
        UpdatePlayer(&fireboy, ground, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&watergirl, ground, KEY_A, KEY_D, KEY_W);
        UpdatePlayer(&earthboy, ground, KEY_H, KEY_K, KEY_U); // UHJK esquema

        BoxHandlePush(&box, &fireboy, KEY_LEFT, KEY_RIGHT);
        BoxHandlePush(&box, &watergirl, KEY_A, KEY_D);
        BoxHandlePush(&box, &earthboy, KEY_H, KEY_K);
        BoxUpdate(&box, ground);

        // Atualiza botão (pressionado se qualquer jogador estiver sobre ele)
        bool pressed = ButtonUpdate(&btn, &fireboy, &watergirl, &earthboy);

        // Ventilador: empurra players que estiverem na área de efeito
        FanApply(&fan, &fireboy);
        FanApply(&fan, &watergirl);
        FanApply(&fan, &earthboy);

        // Lagos: só o elemento corresponde pode entrar; demais são bloqueados
        // Checa morte por lago errado e respawna
        if (LakeHandlePlayer(&lakeWater, &fireboy, LAKE_FIRE) ||
            LakeHandlePlayer(&lakeFire, &fireboy, LAKE_FIRE) ||
            LakeHandlePlayer(&lakeEarth, &fireboy, LAKE_FIRE)) {
            fireboy.rect.x = spawnFire.x; fireboy.rect.y = spawnFire.y;
            fireboy.velocity = (Vector2){0,0}; fireboy.isJumping = false;
        }
        if (LakeHandlePlayer(&lakeWater, &watergirl, LAKE_WATER) ||
            LakeHandlePlayer(&lakeFire, &watergirl, LAKE_WATER) ||
            LakeHandlePlayer(&lakeEarth, &watergirl, LAKE_WATER)) {
            watergirl.rect.x = spawnWater.x; watergirl.rect.y = spawnWater.y;
            watergirl.velocity = (Vector2){0,0}; watergirl.isJumping = false;
        }
        if (LakeHandlePlayer(&lakeWater, &earthboy, LAKE_EARTH) ||
            LakeHandlePlayer(&lakeFire, &earthboy, LAKE_EARTH) ||
            LakeHandlePlayer(&lakeEarth, &earthboy, LAKE_EARTH)) {
            earthboy.rect.x = spawnEarth.x; earthboy.rect.y = spawnEarth.y;
            earthboy.velocity = (Vector2){0,0}; earthboy.isJumping = false;
        }

        if (GoalReached(&goal, &fireboy, &watergirl, &earthboy)) { completed = true; break; }

        // Pause menu
        if (IsKeyPressed(KEY_ESCAPE)) {
            PauseResult pr = ShowPauseMenu();
            if (pr == PAUSE_TO_MAP) { completed = false; break; }
            if (pr == PAUSE_TO_MENU) { Game_SetReturnToMenu(true); completed = false; break; }
            // PAUSE_RESUME => continua
        }

        BeginDrawing();
        ClearBackground((Color){180, 80, 50, 255});
        DrawText("FASE 1 - TEMPLO INICIAL", 650, 100, 40, GOLD);
        DrawText("Pressione ESC para voltar", 700, 200, 20, WHITE);

        DrawRectangleRec(ground, BROWN);
        // Timer topo
        char tbuf[32]; int min = (int)(elapsed/60.0f); float sec = elapsed - min*60.0f;
        sprintf(tbuf, "%02d:%05.2f", min, sec);
        DrawText(tbuf, 920, 20, 30, WHITE);
        BoxDraw(&box);
        ButtonDraw(&btn);
        FanDraw(&fan);
        LakeDraw(&lakeWater);
        LakeDraw(&lakeFire);
        LakeDraw(&lakeEarth);
        GoalDraw(&goal);
        DrawPlayer(fireboy);
        DrawPlayer(watergirl);
        DrawPlayer(earthboy);

        EndDrawing();

        // ESC controlado pelo menu de pausa acima
    }

    UnloadPlayer(&fireboy);
    UnloadPlayer(&watergirl);
    UnloadPlayer(&earthboy);
    if (completed) {
        Ranking_Add(1, Game_GetPlayerName(), elapsed);
    }
    return completed;
}
