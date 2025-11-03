#include "fases.h"
#include "raylib.h"

void Fase3(Player* player) {
    Rectangle ground = { 0, 800, 1920, 280 };
    InitPlayer(player);

    while (!WindowShouldClose()) {
        UpdatePlayer(player, ground);

        BeginDrawing();
        ClearBackground((Color){200, 50, 50, 255}); // fundo avermelhado
        DrawText("FASE 3 - MONTANHA DE FOGO", 620, 100, 40, YELLOW);
        DrawText("Pressione ESC para voltar", 700, 200, 20, WHITE);

        DrawRectangleRec(ground, RED);
        DrawPlayer(*player);

        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE)) break;
    }
}
