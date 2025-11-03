#include "raylib.h"
#include "player/player.h"
#include "menu/menu.h"
#include "mapa/mapa_fases.h"   // <- novo sistema de mapa binário

int main(void)
{
    const int screenWidth = 1920;
    const int screenHeight = 1080;

    // Inicializa a janela principal
    InitWindow(screenWidth, screenHeight, "Elements");
    SetTargetFPS(60);

    // --- MENU INICIAL ---
    if (!MostrarMenu()) { // Exibe a imagem do menu e espera ENTER
        CloseWindow();
        return 0;
    }

    // --- ABRE O MAPA DE FASES ---
    MostrarMapaFases(); // <- nova função do sistema de árvore binária

    // --- Finaliza o jogo ---
    CloseWindow();
    return 0;
}
