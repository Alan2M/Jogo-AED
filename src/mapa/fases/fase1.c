#include "fases.h"
#include "raylib.h"

void Fase1(Player* player) {
    Rectangle ground = { 0, 800, 1920, 280 };
    InitPlayer(player);

    Camera2D camera = { 0 };
    camera.target = (Vector2){ player->rect.x + player->rect.width / 2, player->rect.y + player->rect.height / 2 };
    camera.offset = (Vector2){ 1920 / 2.0f, 1080 / 2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    while (!WindowShouldClose()) {
        // Atualiza o jogador
        UpdatePlayer(player, ground);
        camera.target = (Vector2){ player->rect.x + player->rect.width / 2, player->rect.y + player->rect.height / 2 };

        // --- Desenho ---
        BeginDrawing();
        ClearBackground(SKYBLUE);

        BeginMode2D(camera);
        DrawRectangleRec(ground, DARKGREEN);
        DrawPlayer(*player);
        EndMode2D();

        DrawText("FASE 1 - Use as setas ou A/D para andar", 600, 50, 30, BLACK);
        DrawText("Espaço para pular - ESC para sair", 700, 100, 20, BLACK);

        EndDrawing();

        // Sai da fase com ESC
        if (IsKeyPressed(KEY_ESCAPE)) break;
    }
}
