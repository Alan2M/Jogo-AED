#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "fases/fases.h"
#include "../player/player.h"

// Estrutura da árvore binária
typedef struct NoFase {
    int id;
    bool desbloqueada;
    struct NoFase* esquerda;
    struct NoFase* direita;
} NoFase;

// --- Funções auxiliares ---
NoFase* CriarFase(int id, bool desbloqueada) {
    NoFase* nova = malloc(sizeof(NoFase));
    nova->id = id;
    nova->desbloqueada = desbloqueada;
    nova->esquerda = NULL;
    nova->direita = NULL;
    return nova;
}

void Conectar(NoFase* pai, NoFase* esq, NoFase* dir) {
    pai->esquerda = esq;
    pai->direita = dir;
}

void AtualizarDesbloqueios(NoFase* raiz, int faseConcluida) {
    if (faseConcluida == 1) {
        if (raiz->esquerda) raiz->esquerda->desbloqueada = true;
        if (raiz->direita) raiz->direita->desbloqueada = true;
    }
    if (faseConcluida == 2 && raiz->esquerda)
        if (raiz->esquerda->esquerda) raiz->esquerda->esquerda->desbloqueada = true;
    if (faseConcluida == 3 && raiz->direita)
        if (raiz->direita->direita) raiz->direita->direita->desbloqueada = true;
}

// --- Desenhar fase ---
void DesenharFase(NoFase* fase, Vector2 pos, int selecionada, float brilho) {
    Color corBase;

    if (!fase->desbloqueada) corBase = DARKGRAY;
    else if (fase->id == selecionada)
        corBase = (Color){ (int)(255 * brilho), (int)(255 * brilho), 0, 255 };
    else
        corBase = GREEN;

    DrawCircleV(pos, 50, corBase);
    DrawCircleLines(pos.x, pos.y, 50, BLACK);

    char texto[10];
    sprintf(texto, "%d", fase->id);
    DrawText(texto, pos.x - 10, pos.y - 15, 40, BLACK);
}

// --- Mapa de fases ---
bool MostrarMapaFases(void) {
    const int screenWidth = 1920;
    const int screenHeight = 1080;

    // Cria árvore binária
    NoFase* f1 = CriarFase(1, true);
    NoFase* f2 = CriarFase(2, false);
    NoFase* f3 = CriarFase(3, false);
    NoFase* f4 = CriarFase(4, false);
    NoFase* f5 = CriarFase(5, false);

    Conectar(f1, f2, f3);
    Conectar(f2, f4, NULL);
    Conectar(f3, NULL, f5);

    int faseSelecionada = 1;
    int faseConcluida = 0;

    // 🟡 EVITA que o ENTER do menu entre direto na Fase 1
    while (IsKeyDown(KEY_ENTER)) {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Carregando mapa...", 800, 500, 30, WHITE);
        EndDrawing();
    }

    while (!WindowShouldClose()) {

        // --- Navegação entre fases ---
        if (IsKeyPressed(KEY_RIGHT)) {
            faseSelecionada++;
            if (faseSelecionada > 5) faseSelecionada = 1;
        }
        if (IsKeyPressed(KEY_LEFT)) {
            faseSelecionada--;
            if (faseSelecionada < 1) faseSelecionada = 5;
        }

        // --- Entrar na fase selecionada (somente ao apertar ENTER) ---
        if (IsKeyPressed(KEY_ENTER)) {
            NoFase* atual = NULL;
            switch (faseSelecionada) {
                case 1: atual = f1; break;
                case 2: atual = f2; break;
                case 3: atual = f3; break;
                case 4: atual = f4; break;
                case 5: atual = f5; break;
            }

            if (atual && atual->desbloqueada) {
                // Abre a fase correspondente
                switch (atual->id) {
                    case 1: Fase1(); break;
                    case 2: Fase2(); break;
                    case 3: Fase3(); break;
                    case 4: Fase4(); break;
                    case 5: Fase5(); break;
                }

                faseConcluida = atual->id;
                AtualizarDesbloqueios(f1, faseConcluida);
            }
        }

        // --- Sair do mapa (voltar ao menu) ---
        if (IsKeyPressed(KEY_ESCAPE)) {
            return false; // volta pro menu
        }

        // --- Desenho ---
        BeginDrawing();
        ClearBackground((Color){35, 25, 10, 255});

        float brilho = (sinf(GetTime() * 2) + 1.0f) / 2.0f;

        Vector2 posF1 = {screenWidth / 2, 200};
        Vector2 posF2 = {screenWidth / 2 - 350, 450};
        Vector2 posF3 = {screenWidth / 2 + 350, 450};
        Vector2 posF4 = {screenWidth / 2 - 500, 700};
        Vector2 posF5 = {screenWidth / 2 + 500, 700};

        DrawLineBezier(posF1, posF2, 6, GOLD);
        DrawLineBezier(posF1, posF3, 6, GOLD);
        DrawLineBezier(posF2, posF4, 6, GOLD);
        DrawLineBezier(posF3, posF5, 6, GOLD);

        DesenharFase(f1, posF1, faseSelecionada, brilho);
        DesenharFase(f2, posF2, faseSelecionada, brilho);
        DesenharFase(f3, posF3, faseSelecionada, brilho);
        DesenharFase(f4, posF4, faseSelecionada, brilho);
        DesenharFase(f5, posF5, faseSelecionada, brilho);

        DrawText("MAPA DE FASES", 780, 40, 40, YELLOW);
        DrawText("← → para selecionar | ENTER para jogar", 600, 100, 20, RAYWHITE);
        DrawText("ESC para voltar ao menu", 780, 130, 20, GRAY);
        DrawText("A fase amarela está selecionada", 20, screenHeight - 60, 20, GRAY);

        EndDrawing();
    }

    return false;
}
