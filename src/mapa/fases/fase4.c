#include "fases.h"
#include "raylib.h"

void Fase4(Player* player) {
    Rectangle ground = { 0, 800, 1920, 280 };
    InitPlayer(player);

    while (!WindowShouldClose()) {
        UpdatePlayer(player, ground);

        BeginDrawing();
        ClearBackground((Color){100, 50, 150, 255}); // fundo roxo
        DrawText("FASE 4 - FLORESTA SOMBRIA", 640, 100, 40, GREEN);
        DrawText("Pressione ESC para voltar", 700, 200, 20, WHITE);

        DrawRectangleRec(ground, DARKGREEN);
        DrawPlayer(*player);

        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE)) break;
    }
}
