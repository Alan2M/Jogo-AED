#include "raylib.h"
#include "player/player.h"
#include "menu/menu.h"
#include "mapa/mapa_fases.h"
#include "ranking/ranking.h"
#include "audio/theme.h"

int main(void)
{
    const int screenWidth = 1920;
    const int screenHeight = 1080;

    InitWindow(screenWidth, screenHeight, "Elements");
    SetExitKey(0);
    SetTargetFPS(60);
    Ranking_Init();
    Theme_Init();

    bool rodando = true;

    while (rodando && !WindowShouldClose())
    {
        // --- MENU PRINCIPAL ---
        Theme_Update();
        bool jogar = MostrarMenu();

        // Se o jogador escolheu SAIR
        if (!jogar) {
            rodando = false;
            break;
        }

        // --- MOSTRA O MAPA DE FASES ---
        bool continuarJogo = MostrarMapaFases();

        // Se apertar ESC no mapa, retorna ao menu
        if (!continuarJogo)
            continue;
    }

    Theme_Shutdown();

    CloseWindow();
    return 0;
}
