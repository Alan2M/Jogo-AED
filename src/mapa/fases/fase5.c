#include "fases.h"
#include "raylib.h"

void Fase5(Player* player) {
    Rectangle ground = { 0, 800, 1920, 280 };
    InitPlayer(player);

    while (!WindowShouldClose()) {
        UpdatePlayer(player, ground);

        BeginDrawing();
        ClearBackground((Color){255, 180, 50, 255}); // fundo dourado
        DrawText("FASE 5 - TEMPLO FINAL", 680, 100, 40, DARKBROWN);
        DrawText("Pressione ESC para voltar", 700, 200, 20, DARKBROWN);

        DrawRectangleRec(ground, BROWN);
        DrawPlayer(*player);

        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE)) break;
    }
}
