#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "fases/fases.h"
#include "../player/player.h"
#include "../game/game.h"

// Estrutura da árvore binária
typedef struct NoFase {
    int id;
    bool desbloqueada;
    struct NoFase* esquerda;
    struct NoFase* direita;
} NoFase;

// Progresso em memória (por sessão): bits das fases concluídas (1..5)
static unsigned int gProgressMask = 0;

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

static NoFase* EncontrarFasePorId(NoFase* raiz, int id) {
    if (!raiz) return NULL;
    if (raiz->id == id) return raiz;
    NoFase* encontrada = EncontrarFasePorId(raiz->esquerda, id);
    if (encontrada) return encontrada;
    return EncontrarFasePorId(raiz->direita, id);
}

void AtualizarDesbloqueios(NoFase* raiz, int faseConcluida) {
    if (faseConcluida == 1) {
        if (raiz->esquerda) raiz->esquerda->desbloqueada = true;
        if (raiz->direita) raiz->direita->desbloqueada = true;
    }
    if (faseConcluida == 2 && raiz->esquerda) {
        if (raiz->esquerda->esquerda) raiz->esquerda->esquerda->desbloqueada = true;
        if (raiz->esquerda->direita) raiz->esquerda->direita->desbloqueada = true;
    }
}

// --- Mapa de fases ---
bool MostrarMapaFases(void) {
    const int screenWidth = 1920;
    const int screenHeight = 1080;
    Texture2D texMapaInicial = LoadTexture("assets/map/mapafases/1.png");
    Texture2D texMapaParcial = LoadTexture("assets/map/mapafases/123.png");
    Texture2D texMapaCompleto = LoadTexture("assets/map/mapafases/12345.png");

    // Cria árvore binária
    NoFase* f1 = CriarFase(1, true);
    NoFase* f2 = CriarFase(2, false);
    NoFase* f3 = CriarFase(3, false);
    NoFase* f4 = CriarFase(4, false);
    NoFase* f5 = CriarFase(5, false);

    Conectar(f1, f2, f3);
    Conectar(f2, f5, f4);

    // Aplica progresso em memória para reabrir fases já concluídas nesta sessão
    for (int i = 1; i <= 5; i++) {
        if (gProgressMask & (1u << i)) {
            AtualizarDesbloqueios(f1, i);
        }
    }

    int faseSelecionada = 1;
    int faseConcluida = 0;
    bool confirmExit = false;
    bool sairDoMapa = false;
    // Botão de desbloqueio (debug)
    Rectangle btnUnlock = (Rectangle){ screenWidth - 300.0f, 20.0f, 280.0f, 40.0f };

    // 🟡 EVITA que o ENTER do menu entre direto na Fase 1
    while (IsKeyDown(KEY_ENTER)) {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Carregando mapa...", 800, 500, 30, WHITE);
        EndDrawing();
    }

    // Evita que o ESC usado para sair da fase acione o overlay imediatamente
    while (IsKeyDown(KEY_ESCAPE)) {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Carregando mapa...", 800, 500, 30, WHITE);
        EndDrawing();
    }

    while (!WindowShouldClose() && !sairDoMapa) {

        // --- Navegação entre fases ---
        if (!confirmExit && IsKeyPressed(KEY_RIGHT)) {
            NoFase* atual = EncontrarFasePorId(f1, faseSelecionada);
            if (atual && atual->direita) faseSelecionada = atual->direita->id;
            else faseSelecionada = 1;
        }
        if (!confirmExit && IsKeyPressed(KEY_LEFT)) {
            NoFase* atual = EncontrarFasePorId(f1, faseSelecionada);
            if (atual && atual->esquerda) faseSelecionada = atual->esquerda->id;
            else faseSelecionada = 1;
        }

        // --- Botão: Desbloquear todas as fases (clique ou tecla U) ---
        if (!confirmExit) {
            Vector2 mp = GetMousePosition();
            bool hover = CheckCollisionPointRec(mp, btnUnlock);
            if ((hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) || IsKeyPressed(KEY_U)) {
                // Marca todas como desbloqueadas e ajusta progresso em memória
                f1->desbloqueada = true;
                f2->desbloqueada = true;
                f3->desbloqueada = true;
                f4->desbloqueada = true;
                f5->desbloqueada = true;
                for (int i = 1; i <= 5; ++i) gProgressMask |= (1u << i);
            }
        }

        // --- Entrar na fase selecionada (somente ao apertar ENTER) ---
        if (!confirmExit && IsKeyPressed(KEY_ENTER)) {
            NoFase* atual = EncontrarFasePorId(f1, faseSelecionada);

            if (atual && atual->desbloqueada) {
                bool concluida = false;
                // Abre a fase correspondente
                switch (atual->id) {
                    case 1: concluida = Fase1(); break;
                    case 2: concluida = Fase2(); break;
                    case 3: concluida = Fase3(); break;
                    case 4: concluida = Fase4(); break;
                    case 5: concluida = Fase5(); break;
                }

                if (concluida) {
                    faseConcluida = atual->id;
                    gProgressMask |= (1u << faseConcluida);
                    AtualizarDesbloqueios(f1, faseConcluida);
                }

                // Se a fase solicitou retorno ao menu, sai do mapa
                if (Game_ShouldReturnToMenu()) {
                    Game_ClearReturnToMenu();
                    sairDoMapa = true;
                    break;
                }
            }
        }

        // --- Sair do mapa (voltar ao menu) ---
        if (IsKeyPressed(KEY_ESCAPE)) {
            confirmExit = !confirmExit; // alterna confirmação
        }

        // --- Desenho ---
        BeginDrawing();
        ClearBackground((Color){35, 25, 10, 255});

        Texture2D* mapaAtual = &texMapaInicial;
        if (f4->desbloqueada) {
            mapaAtual = &texMapaCompleto;
        } else if (f2->desbloqueada || f3->desbloqueada) {
            mapaAtual = &texMapaParcial;
        }

        Rectangle srcMapa = {0, 0, (float)mapaAtual->width, (float)mapaAtual->height};
        Rectangle dstMapa = {0, 0, (float)screenWidth, (float)screenHeight};
        DrawTexturePro(*mapaAtual, srcMapa, dstMapa, (Vector2){0, 0}, 0.0f, WHITE);

        DrawText("MAPA DE FASES", 780, 40, 40, YELLOW);
        DrawText("Setas <- -> para selecionar | ENTER para jogar", 600, 100, 20, RAYWHITE);
        DrawText("ESC para voltar ao menu", 780, 130, 20, GRAY);
        DrawText("T para Fase Teste (dev)", 780, 160, 20, (Color){180,180,220,255});

        NoFase* faseAtual = EncontrarFasePorId(f1, faseSelecionada);
        bool faseDisponivel = faseAtual && faseAtual->desbloqueada;

        Vector2 posicoes[6] = {
            {0, 0},
            {985.0f, 825.0f},   // Fase 1
            {725.0f, 570.0f},   // Fase 2
            {1223.0f, 524.0f},  // Fase 3
            {995.0f, 190.0f},   // Fase 4
            {455.0f, 225.0f}    // Fase 5
        };
        Vector2 posIndicador = posicoes[faseSelecionada];
        Color corIndicador = faseDisponivel ? YELLOW : (Color){120, 120, 120, 220};

        DrawCircleLines(posIndicador.x, posIndicador.y, 70.0f, corIndicador);
        DrawCircleLines(posIndicador.x, posIndicador.y, 76.0f, (Color){0, 0, 0, 120});

        const char* msgStatus = faseDisponivel ? "ENTER para iniciar" : "Conclua a fase anterior para liberar";
        Color corStatus = faseDisponivel ? GREEN : GRAY;
        DrawText(msgStatus, 60, screenHeight - 80, 22, corStatus);
        Vector2 cursor = GetMousePosition();
        char coordTexto[64];
        snprintf(coordTexto, sizeof(coordTexto), "Mouse: %.0f, %.0f", cursor.x, cursor.y);
        DrawText(coordTexto, 60, screenHeight - 120, 20, WHITE);


        // Botão desenhado
        bool hover = CheckCollisionPointRec(GetMousePosition(), btnUnlock);
        Color cBtn = hover ? (Color){50,170,60,255} : (Color){40,140,50,255};
        DrawRectangleRec(btnUnlock, cBtn);
        DrawRectangleLinesEx(btnUnlock, 2, BLACK);
        DrawText("DESBLOQUEAR TODAS (U)", btnUnlock.x + 12, btnUnlock.y + 10, 20, WHITE);

        if (confirmExit) {
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){0,0,0,180});
            DrawText("Voltar ao menu?", 820, 480, 30, RAYWHITE);
            DrawText("Y = Sim   N = Nao", 835, 520, 20, LIGHTGRAY);
            if (IsKeyPressed(KEY_Y)) { sairDoMapa = true; }
            if (IsKeyPressed(KEY_N)) { confirmExit = false; }
        }

        EndDrawing();

        if (sairDoMapa) break;

        // Atalho dev: abrir Fase Teste (não altera desbloqueios)
        if (!confirmExit && IsKeyPressed(KEY_T)) {
            FaseTeste();
        }
    }

    UnloadTexture(texMapaInicial);
    UnloadTexture(texMapaParcial);
    UnloadTexture(texMapaCompleto);
    return false;
}

void ResetarProgressoFases(void) {
    gProgressMask = 0;
}

