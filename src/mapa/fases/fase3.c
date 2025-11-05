#include "fases.h"
#include "../../player/player.h"
#include "../../objects/box.h"
#include "../../objects/seesaw.h"
#include "../../objects/goal.h"
#include "../../interface/pause.h"
#include "../../game/game.h"
#include "../../ranking/ranking.h"
#include <stdio.h>

bool Fase3(void) {
    Rectangle ground = { 0, 800, 1920, 280 };

    // Caixa empurrável (reutilizável via módulo Box)
    Box box; BoxInit(&box, ground, 900, 80, 80);
    // Seesaw (fulcro no meio) para "cair" para o lado do jogador
    SeesawPlatform seesaw;
    SeesawInit(&seesaw, (Vector2){ 700, 620 }, 300.0f, 20.0f, 0.0f);

    Goal goal; GoalInit(&goal, 1820, ground.y - 120, 40, 120, GREEN);

    Player fireboy, watergirl, earthboy;
    InitFireboy(&fireboy);
    InitWatergirl(&watergirl);
    InitEarthboy(&earthboy);

    bool completed = false;
    float elapsed = 0.0f;
    while (!WindowShouldClose()) {
        elapsed += GetFrameTime();
        UpdatePlayer(&fireboy, ground, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&watergirl, ground, KEY_A, KEY_D, KEY_W);
        UpdatePlayer(&earthboy, ground, KEY_H, KEY_K, KEY_U); // UHJK esquema

        BoxHandlePush(&box, &fireboy, KEY_LEFT, KEY_RIGHT);
        BoxHandlePush(&box, &watergirl, KEY_A, KEY_D);
        BoxHandlePush(&box, &earthboy, KEY_H, KEY_K);
        BoxUpdate(&box, ground);

        // Seesaw: acumula torque por jogadores e atualiza
        SeesawBeginFrame(&seesaw);
        SeesawHandlePlayer(&seesaw, &fireboy);
        SeesawHandlePlayer(&seesaw, &watergirl);
        SeesawHandlePlayer(&seesaw, &earthboy);
        SeesawUpdate(&seesaw);

        if (GoalReached(&goal, &fireboy, &watergirl, &earthboy)) { completed = true; break; }

        // Pause menu
        if (IsKeyPressed(KEY_ESCAPE)) {
            PauseResult pr = ShowPauseMenu();
            if (pr == PAUSE_TO_MAP) { completed = false; break; }
            if (pr == PAUSE_TO_MENU) { Game_SetReturnToMenu(true); completed = false; break; }
        }

        BeginDrawing();
        ClearBackground((Color){180, 80, 50, 255});
        DrawText("FASE 3", 900, 100, 40, GOLD);
        DrawText("Empurre a caixa (colida e mova)", 700, 160, 20, WHITE);
        DrawText("Pêndulo (fulcro central): suba e ele inclina", 700, 230, 20, WHITE);
        DrawText("Pressione ESC para voltar", 700, 200, 20, WHITE);

        DrawRectangleRec(ground, BROWN);
        char tbuf[32]; int min = (int)(elapsed/60.0f); float sec = elapsed - min*60.0f;
        sprintf(tbuf, "%02d:%05.2f", min, sec);
        DrawText(tbuf, 920, 20, 30, WHITE);
        BoxDraw(&box);
        SeesawDraw(&seesaw);
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
    if (completed) Ranking_Add(3, Game_GetPlayerName(), elapsed);
    return completed;
}
