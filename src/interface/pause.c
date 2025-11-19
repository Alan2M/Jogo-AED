#include "pause.h"
#include "raylib.h"

PauseResult ShowPauseMenu(void) {
    int selected = 0; // 0=retomar,1=mapa,2=menu

    // Evita fechar imediatamente por causa do mesmo ESC que abriu o menu
 
    while (!WindowShouldClose()) {
        BeginDrawing();
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), (Color){0,0,0,180});
        DrawText("PAUSE", GetScreenWidth()/2 - 80, GetScreenHeight()/2 - 180, 50, RAYWHITE);

        const char* ops[3] = { "Retomar", "Voltar ao mapa de fases", "Voltar ao menu" };
        for (int i = 0; i < 3; i++) {
            Color c = (i == selected) ? GOLD : LIGHTGRAY;
            DrawText(ops[i], GetScreenWidth()/2 - 220, GetScreenHeight()/2 - 60 + i*60, 40, c);
            if (i == selected) DrawText(">", GetScreenWidth()/2 - 260, GetScreenHeight()/2 - 60 + i*60, 40, c);
        }
        EndDrawing();

        // Processa entradas após desenhar ao menos um frame do overlay
        if (IsKeyPressed(KEY_DOWN)) selected = (selected + 1) % 3;
        if (IsKeyPressed(KEY_UP)) selected = (selected + 2) % 3;
        if (IsKeyPressed(KEY_ENTER)) break;
        if (IsKeyPressed(KEY_ESCAPE)) { selected = 0; break; }
    }

    // Nada a restaurar (main já desativa ExitKey)

    switch (selected) {
        case 0: return PAUSE_RESUME;
        case 1: return PAUSE_TO_MAP;
        case 2: return PAUSE_TO_MENU;
        default: return PAUSE_RESUME;
    }
}
