#include "raylib.h"
#include "../../player/player.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_COLISOES 512
#define STEP_HEIGHT 14.0f   // altura máxima que o jogador pode "subir" sem pular

typedef struct {
    Rectangle rect;
} Colisao;

bool Fase1(void) {
    // --- Carrega o mapa do Tiled ---
    char *xml = LoadFileText("assets/maps/Mapa.tmx");
    if (!xml) {
        printf("❌ Erro ao carregar assets/maps/Mapa.tmx\n");
        return false;
    }

    Colisao colisoes[MAX_COLISOES];
    int totalColisoes = 0;

    const char *ptr = xml;
    while ((ptr = strstr(ptr, "<object id=")) != NULL) {
        float x=0, y=0, w=0, h=0;
        if (sscanf(ptr, "<object id=\"%*d\" x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\"",
                   &x, &y, &w, &h) == 4) {
            if (totalColisoes < MAX_COLISOES)
                colisoes[totalColisoes++].rect = (Rectangle){x, y, w, h};
        }
        ptr++;
    }
    UnloadFileText(xml);

    // --- Carrega textura do mapa ---
    Texture2D mapTexture = LoadTexture("assets/maps/Map.png");
    if (mapTexture.id == 0) {
        printf("❌ Erro ao carregar assets/maps/Map.png\n");
        return false;
    }

    // --- Inicializa jogadores menores ---
    Player earthboy, fireboy, watergirl;
    InitEarthboy(&earthboy);
    InitFireboy(&fireboy);
    InitWatergirl(&watergirl);

    earthboy.rect = (Rectangle){300, 700, 45, 50};
    fireboy.rect  = (Rectangle){400, 700, 45, 50};
    watergirl.rect= (Rectangle){500, 700, 45, 50};

    // --- Câmera fixa ---
    Camera2D camera = {0};
    camera.target = (Vector2){mapTexture.width / 2.0f, mapTexture.height / 2.0f};
    camera.offset = (Vector2){GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
    camera.zoom = 1.0f;

    bool debug = false;
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_TAB)) debug = !debug;

        // --- Atualiza jogadores ---
        UpdatePlayer(&earthboy, (Rectangle){0, mapTexture.height, mapTexture.width, 100}, KEY_A, KEY_D, KEY_W);
        UpdatePlayer(&fireboy,  (Rectangle){0, mapTexture.height, mapTexture.width, 100}, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&watergirl,(Rectangle){0, mapTexture.height, mapTexture.width, 100}, KEY_J, KEY_L, KEY_I);

        // --- Colisões completas (com degraus e teto bloqueado) ---
        Player* players[3] = {&earthboy, &fireboy, &watergirl};
        for (int p = 0; p < 3; p++) {
            Player* pl = players[p];
            for (int i = 0; i < totalColisoes; i++) {
                Rectangle bloco = colisoes[i].rect;

                if (CheckCollisionRecs(pl->rect, bloco)) {
                    float dx = (pl->rect.x + pl->rect.width / 2) - (bloco.x + bloco.width / 2);
                    float dy = (pl->rect.y + pl->rect.height / 2) - (bloco.y + bloco.height / 2);
                    float overlapX = (pl->rect.width / 2 + bloco.width / 2) - fabsf(dx);
                    float overlapY = (pl->rect.height / 2 + bloco.height / 2) - fabsf(dy);

                    if (overlapX < overlapY) {
                        // --- Tentativa de subir degrau ---
                        if (dy > 0 && pl->velocity.y > 0) continue;

                        Rectangle teste = pl->rect;
                        teste.y -= STEP_HEIGHT;
                        if (!CheckCollisionRecs(teste, bloco)) {
                            pl->rect.y -= STEP_HEIGHT;
                            continue;
                        }

                        // --- Bloqueio lateral normal ---
                        if (dx > 0) pl->rect.x += overlapX;
                        else        pl->rect.x -= overlapX;
                        pl->velocity.x = 0;
                    } else {
                        // --- Corrige colisão vertical (teto e chão) ---
                        if (dy > 0 && pl->velocity.y < 0) {
                            // Jogador bateu de baixo (subindo) → teto
                            pl->rect.y += overlapY;
                            pl->velocity.y = 0;
                        }
                        else if (dy < 0 && pl->velocity.y >= 0) {
                            // Jogador caiu sobre o bloco (chão)
                            pl->rect.y -= overlapY;
                            pl->velocity.y = 0;
                            pl->isJumping = false;
                        }
                    }

                }
            }
        }

        // --- Desenho ---
        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode2D(camera);

        DrawTexture(mapTexture, 0, 0, WHITE);
        DrawPlayer(earthboy);
        DrawPlayer(fireboy);
        DrawPlayer(watergirl);

        // --- Debug ---
        if (debug) {
            for (int i = 0; i < totalColisoes; i++)
                DrawRectangleLinesEx(colisoes[i].rect, 1, Fade(GREEN, 0.5f));

            DrawText(TextFormat("Earthboy: (%.0f, %.0f)", earthboy.rect.x, earthboy.rect.y), 10, 10, 20, YELLOW);
            DrawText(TextFormat("Fireboy:  (%.0f, %.0f)", fireboy.rect.x, fireboy.rect.y), 10, 35, 20, ORANGE);
            DrawText(TextFormat("Watergirl:(%.0f, %.0f)", watergirl.rect.x, watergirl.rect.y), 10, 60, 20, SKYBLUE);
            DrawText("TAB - Modo Debug", 10, 90, 20, GRAY);
            DrawFPS(10, 120);
        }

        EndMode2D();
        EndDrawing();
    }

    // --- Libera recursos ---
    UnloadTexture(mapTexture);
    UnloadPlayer(&earthboy);
    UnloadPlayer(&fireboy);
    UnloadPlayer(&watergirl);
    return true;
}
    