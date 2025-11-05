#include "fases.h"
#include "../../player/player.h"

void Fase4(void) {
    Rectangle ground = { 0, 800, 1920, 280 };

    Player fireboy, watergirl, earthboy;
    InitFireboy(&fireboy);
    InitWatergirl(&watergirl);
    InitEarthboy(&earthboy);

    while (!WindowShouldClose()) {
        UpdatePlayer(&fireboy, ground, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&watergirl, ground, KEY_A, KEY_D, KEY_W);
        UpdatePlayer(&earthboy, ground, KEY_H, KEY_K, KEY_U); // UHJK esquema

        BeginDrawing();
        ClearBackground((Color){180, 80, 50, 255});
        DrawText("FASE 1 - TEMPLO INICIAL", 650, 100, 40, GOLD);
        DrawText("Pressione ESC para voltar", 700, 200, 20, WHITE);

        DrawRectangleRec(ground, BROWN);
        DrawPlayer(fireboy);
        DrawPlayer(watergirl);
        DrawPlayer(earthboy);

        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE)) break;
    }

    UnloadPlayer(&fireboy);
    UnloadPlayer(&watergirl);
    UnloadPlayer(&earthboy);
}
