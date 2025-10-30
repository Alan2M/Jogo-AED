#include "raylib.h"
#include "player/player.h"
#include "mapa/mapa.h"
#include "menu/menu.h"

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

    // --- INICIALIZA O JOGO ---
    Player player;
    InitPlayer(&player);

    Mapa mapa;
    InitMapa(&mapa);

    Camera2D camera = { 0 };
    camera.target = (Vector2){ player.rect.x + player.rect.width / 2, player.rect.y + player.rect.height / 2 };
    camera.offset = (Vector2){ screenWidth / 2.0f, screenHeight / 2.0f };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;

    // --- LOOP PRINCIPAL DO JOGO ---
    while (!WindowShouldClose())
    {
        // Atualiza o jogador e a câmera
        UpdatePlayer(&player, mapa.ground);
        camera.target = (Vector2){ player.rect.x + player.rect.width / 2, player.rect.y + player.rect.height / 2 };

        // Desenho da tela
        BeginDrawing();
        ClearBackground(SKYBLUE);

        BeginMode2D(camera);
        DrawMapa(mapa);
        DrawPlayer(player);
        EndMode2D();

        // HUD simples
        DrawText("Elements", 10, 10, 20, DARKBLUE);

        EndDrawing();
    }

    // Finaliza o jogo
    CloseWindow();
    return 0;
}
