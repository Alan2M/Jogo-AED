#include "menu.h"

bool MostrarMenu(void) {
    Texture2D background = LoadTexture("assets/sprites/menuaed.png");

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexture(background, 0, 0, WHITE);
        EndDrawing();

        if (IsKeyPressed(KEY_ENTER)) {
            UnloadTexture(background);
            return true; // Começar o jogo
        }
    }

    UnloadTexture(background);
    return false;
}
