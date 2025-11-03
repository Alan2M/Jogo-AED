#include "menu.h"

typedef enum { OPC_JOGAR = 0, OPC_INSTRUCOES, TOTAL_OPCOES } MenuOpcao;

bool MostrarMenu(void) {
    Texture2D background = LoadTexture("assets/menu/menuaed.png");
    int opcaoSelecionada = OPC_JOGAR;

    // Posições baseadas na arte 1920x1080
    Vector2 posJogar = { 700, 430 };       // leve ajuste horizontal
    Vector2 posInstrucoes = { 590, 665 };  // leve ajuste horizontal

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        // Fundo preenchendo toda a tela
        DrawTexturePro(
            background,
            (Rectangle){0, 0, background.width, background.height},
            (Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()},
            (Vector2){0, 0},
            0.0f,
            WHITE
        );

        // --- Indicador de seleção ---
        int fontSize = 60;
        Color corSeta = GOLD; // seta dourada

        // Piscar leve para dar vida
        if ((int)(GetTime() * 3) % 2 == 0) {
            if (opcaoSelecionada == OPC_JOGAR)
                DrawText(">", posJogar.x - 80, posJogar.y, fontSize, corSeta);
            else if (opcaoSelecionada == OPC_INSTRUCOES)
                DrawText(">", posInstrucoes.x - 80, posInstrucoes.y, fontSize, corSeta);
        }

        EndDrawing();

        // --- Navegação ---
        if (IsKeyPressed(KEY_DOWN))
            opcaoSelecionada = (opcaoSelecionada + 1) % TOTAL_OPCOES;
        if (IsKeyPressed(KEY_UP))
            opcaoSelecionada = (opcaoSelecionada - 1 + TOTAL_OPCOES) % TOTAL_OPCOES;

        // --- Seleção ---
        if (IsKeyPressed(KEY_ENTER)) {
            UnloadTexture(background);
            if (opcaoSelecionada == OPC_JOGAR)
                return true;
            else
                MostrarInstrucoes();
        }
    }

    UnloadTexture(background);
    return false;
}

void MostrarInstrucoes(void) {
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("INSTRUÇÕES:", 100, 100, 50, YELLOW);
        DrawText("Use as setas para mover", 120, 200, 30, WHITE);
        DrawText("Espaço para pular", 120, 250, 30, WHITE);
        DrawText("ESC para voltar", 120, 350, 30, GRAY);
        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE))
            break;
    }
}
