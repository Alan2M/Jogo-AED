#include "menu.h"
#include "../mapa/mapa_fases.h"

typedef enum { OPC_JOGAR = 0, OPC_INSTRUCOES, OPC_SAIR, TOTAL_OPCOES } MenuOpcao;

bool MostrarMenu(void) {
    Texture2D background = LoadTexture("assets/menu/menuaed.png");
    int opcaoSelecionada = OPC_JOGAR;

    // Posições baseadas na arte 1920x1080
    Vector2 posJogar = { 700, 430 };
    Vector2 posInstrucoes = { 590, 665 };
    Vector2 posSair = { 830, 850 };

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        // --- Fundo preenchendo toda a tela ---
        DrawTexturePro(
            background,
            (Rectangle){0, 0, background.width, background.height},
            (Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()},
            (Vector2){0, 0},
            0.0f,
            WHITE
        );

        // --- Indicador de seleção (seta piscante dourada) ---
        int fontSize = 60;
        Color corSeta = GOLD;

        if ((int)(GetTime() * 2) % 2 == 0) {
            if (opcaoSelecionada == OPC_JOGAR)
                DrawText(">", posJogar.x - 80, posJogar.y, fontSize, corSeta);
            else if (opcaoSelecionada == OPC_INSTRUCOES)
                DrawText(">", posInstrucoes.x - 80, posInstrucoes.y, fontSize, corSeta);
            else if (opcaoSelecionada == OPC_SAIR)
                DrawText(">", posSair.x - 80, posSair.y, fontSize, corSeta);
        }

        // --- Opção "SAIR" escrita ---
        DrawText("SAIR", posSair.x, posSair.y, 60, WHITE);

        EndDrawing();

        // --- Navegação ---
        if (IsKeyPressed(KEY_DOWN))
            opcaoSelecionada = (opcaoSelecionada + 1) % TOTAL_OPCOES;
        if (IsKeyPressed(KEY_UP))
            opcaoSelecionada = (opcaoSelecionada - 1 + TOTAL_OPCOES) % TOTAL_OPCOES;

        // --- Seleção ---
        if (IsKeyPressed(KEY_ENTER)) {
            if (opcaoSelecionada == OPC_JOGAR) {
                UnloadTexture(background);
                return true; // apenas retorna — o main chamará o mapa
            }
            else if (opcaoSelecionada == OPC_INSTRUCOES) {
                MostrarInstrucoes(); // abre tela de instruções
                background = LoadTexture("assets/menu/menuaed.png"); // recarrega ao voltar
            }
            else if (opcaoSelecionada == OPC_SAIR) {
                UnloadTexture(background);
                return false; // encerra o jogo
            }
        }
    }

    UnloadTexture(background);
    return false;
}

void MostrarInstrucoes(void) {
    bool voltar = false;

    while (!WindowShouldClose() && !voltar) {
        BeginDrawing();
        ClearBackground(BLACK);

        DrawText("INSTRUÇÕES:", 100, 100, 50, YELLOW);
        DrawText("Use as setas ou A/D para mover", 120, 200, 30, WHITE);
        DrawText("Espaço para pular", 120, 250, 30, WHITE);

        if ((int)(GetTime() * 2) % 2 == 0)
            DrawText("Pressione ESC para voltar", 120, 350, 30, GRAY);

        EndDrawing();

        if (IsKeyPressed(KEY_ESCAPE))
            voltar = true;
    }
}
