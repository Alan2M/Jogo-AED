#include "menu.h"

typedef enum { OPC_JOGAR = 0, OPC_INSTRUCOES, TOTAL_OPCOES } MenuOpcao;

bool MostrarMenu(void) {
    Texture2D background = LoadTexture("assets/sprites/menuaed.png");
    int opcaoSelecionada = OPC_JOGAR;

    // Posições manuais baseadas na arte
    Vector2 posJogar = { 750, 470 };
    Vector2 posInstrucoes = { 720, 550 };

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        // Ajusta a imagem para ocupar toda a tela
        DrawTexturePro(
        background,
        (Rectangle){0, 0, background.width, background.height},
        (Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()},
        (Vector2){0, 0},
        0.0f,
        WHITE
);


        // Indicador de seleção (seta ou marcador)
        if (opcaoSelecionada == OPC_JOGAR) {
            DrawText(">", posJogar.x - 60, posJogar.y, 60, YELLOW);
        } else if (opcaoSelecionada == OPC_INSTRUCOES) {
            DrawText(">", posInstrucoes.x - 60, posInstrucoes.y, 60, YELLOW);
        }

        EndDrawing();

        // Navegação
        if (IsKeyPressed(KEY_DOWN))
            opcaoSelecionada = (opcaoSelecionada + 1) % TOTAL_OPCOES;
        if (IsKeyPressed(KEY_UP))
            opcaoSelecionada = (opcaoSelecionada - 1 + TOTAL_OPCOES) % TOTAL_OPCOES;

        // Seleção
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
