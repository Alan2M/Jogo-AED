#include "menu.h"
#include "../mapa/mapa_fases.h"
#include "../game/game.h"
#include "../ranking/ranking.h"
#include <string.h>

// Ordem visual: JOGAR, RANKING, TROCAR USUARIO, INSTRUCOES
typedef enum { OPC_JOGAR = 0, OPC_RANKING, OPC_TROCAR_USUARIO, OPC_INSTRUCOES, OPC_SAIR, TOTAL_OPCOES } MenuOpcao;

bool MostrarMenu(void) {
    // Usa a arte principal do menu
    Texture2D background = LoadTexture("assets/menu/menu.png");
    if (background.id == 0) {
        // Fallback para o antigo nome se necessário
        background = LoadTexture("assets/menu/menuaed.png");
    }
    int opcaoSelecionada = OPC_JOGAR;

    // Posições da seta (ajustáveis)
    Vector2 posJogar = { 770, 300 };
    Vector2 posRanking = { 700, 435 };
    Vector2 posTrocar = { 430, 570 };
    Vector2 posInstrucoes = { 575, 710 };
    //Vector2 posSair = { 830, 850 };

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
            else if (opcaoSelecionada == OPC_RANKING)
                DrawText(">", posRanking.x - 80, posRanking.y, fontSize, corSeta);
            else if (opcaoSelecionada == OPC_TROCAR_USUARIO)
                DrawText(">", posTrocar.x - 80, posTrocar.y, fontSize, corSeta);
            else if (opcaoSelecionada == OPC_INSTRUCOES)
                DrawText(">", posInstrucoes.x - 80, posInstrucoes.y, fontSize, corSeta);
            // Oculta indicador para SAIR
        }

        // --- Opções escritas --- (ocultadas na arte)
        // Não desenhamos textos sobre a arte do menu.

        // Mostra usuário atual
        const char* uname = Game_GetPlayerName();
        DrawText(uname && uname[0] ? uname : "SEM USUARIO", 20, GetScreenHeight() - 40, 20, GRAY);

        EndDrawing();

        // --- Navegação ---
        if (IsKeyPressed(KEY_DOWN)) {
            do {
                opcaoSelecionada = (opcaoSelecionada + 1) % TOTAL_OPCOES;
            } while (opcaoSelecionada == OPC_SAIR);
        }
        if (IsKeyPressed(KEY_UP)) {
            do {
                opcaoSelecionada = (opcaoSelecionada - 1 + TOTAL_OPCOES) % TOTAL_OPCOES;
            } while (opcaoSelecionada == OPC_SAIR);
        }

        // --- Seleção ---
        if (IsKeyPressed(KEY_ENTER)) {
            if (opcaoSelecionada == OPC_JOGAR) {
                // Pede nome apenas se ainda não definido
                if (!Game_HasPlayerName()) {
                    if (!PromptPlayerName()) {
                        // cancelado
                        continue;
                    }
                }
                UnloadTexture(background);
                return true; // apenas retorna — o main chamará o mapa
            }
            else if (opcaoSelecionada == OPC_TROCAR_USUARIO) {
                // Troca de usuário: se confirmou, reseta progresso em memória
                if (PromptPlayerName()) {
                    ResetarProgressoFases();
                }
                // permanece no menu
            }
            else if (opcaoSelecionada == OPC_RANKING) {
                MostrarRanking();
                background = LoadTexture("assets/menu/menu.png");
                if (background.id == 0) background = LoadTexture("assets/menu/menuaed.png");
            }
            else if (opcaoSelecionada == OPC_INSTRUCOES) {
                MostrarInstrucoes(); // abre tela de instruções
                background = LoadTexture("assets/menu/menu.png"); // recarrega ao voltar
                if (background.id == 0) background = LoadTexture("assets/menu/menuaed.png");
            }
            // Removido: ação de SAIR
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

// --- Prompt simples para coletar nome do jogador ---
bool PromptPlayerName(void) {
    char buffer[64] = {0};
    int len = 0;
    // flush enter
    while (IsKeyDown(KEY_ENTER)) { BeginDrawing(); EndDrawing(); }

    while (!WindowShouldClose()) {
        bool showError = false;
        int ch = GetCharPressed();
        while (ch > 0) {
            if (ch >= 32 && ch <= 126 && len < 63) {
                buffer[len++] = (char)ch; buffer[len] = '\0';
            }
            ch = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && len > 0) { buffer[--len] = '\0'; }
        if (IsKeyPressed(KEY_ENTER)) {
            if (len == 0) {
                showError = true; // vazio
            } else {
                // Não permitir nome repetido (já existente no ranking) ou igual ao atual
                const char* current = Game_GetPlayerName();
                if ((current && current[0] && strcmp(current, buffer) == 0) || Ranking_NameExists(buffer)) {
                    showError = true;
                } else {
                    break; // aceito
                }
            }
        }
        if (IsKeyPressed(KEY_ESCAPE)) { return false; }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("DIGITE SEU NOME:", 700, 400, 40, RAYWHITE);
        DrawRectangle(680, 460, 560, 60, (Color){30,30,30,255});
        DrawText(buffer[0] ? buffer : "_", 700, 470, 40, GOLD);
        DrawText("ENTER para confirmar | ESC para cancelar", 650, 540, 20, GRAY);
        if (showError) {
            DrawText("Nome invalido ou ja existe", 700, 520, 20, RED);
        }
        EndDrawing();
    }
    if (len == 0) return false;
    Game_SetPlayerName(buffer);
    return true;
}
