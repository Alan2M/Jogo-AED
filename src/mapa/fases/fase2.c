#include "fases.h"
#include "raylib.h"

void Fase2(Player* player) {
    Rectangle ground = { 0, 800, 1920, 280 };
    InitPlayer(player);

    while (!WindowShouldClose()) {
        UpdatePlayer(player, ground);

        BeginDrawing();
        ClearBackground((Color){60, 120, 255, 255}); // fundo azul claro
        DrawText("FASE 2 - CAVERNAS DE GELO", 650, 100, 40, WHITE);
        DrawText("Pressione ESC para voltar", 700, 200, 20, WHITE);

        DrawRectangleRec(ground, BLUE);
        DrawPlayer(*player);

        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE)) break;
    }
}
