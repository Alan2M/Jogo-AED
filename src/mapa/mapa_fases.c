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

void AtualizarDesbloqueios(NoFase** fases, int faseConcluida);

typedef struct {
    Texture2D inicial;
    Texture2D parcial;
    Texture2D completo;
} MapaTexturas;

static void CarregarMapaTexturas(MapaTexturas* mapas) {
    mapas->inicial = LoadTexture("assets/map/mapafases/1.png");
    mapas->parcial = LoadTexture("assets/map/mapafases/123.png");
    mapas->completo = LoadTexture("assets/map/mapafases/12345.png");
}

static void DescarregarMapaTexturas(MapaTexturas* mapas) {
    if (mapas->inicial.id > 0) UnloadTexture(mapas->inicial);
    if (mapas->parcial.id > 0) UnloadTexture(mapas->parcial);
    if (mapas->completo.id > 0) UnloadTexture(mapas->completo);
}

static Texture2D* SelecionarMapaAtual(MapaTexturas* mapas, NoFase* fase2, NoFase* fase3, NoFase* fase4) {
    Texture2D* selecionada = &mapas->inicial;

    if (fase4 && fase4->desbloqueada) {
        selecionada = &mapas->completo;
    } else {
        bool ladoEsquerdoDesbloqueado = fase2 && fase2->desbloqueada;
        bool ladoDireitoDesbloqueado = fase3 && fase3->desbloqueada;
        if (ladoEsquerdoDesbloqueado || ladoDireitoDesbloqueado) {
            selecionada = &mapas->parcial;
        }
    }

    return selecionada;
}

static NoFase* CriarArvoreFases(NoFase** fases) {
    for (int i = 0; i < 6; ++i) fases[i] = NULL;

    fases[1] = CriarFase(1, true);
    fases[2] = CriarFase(2, false);
    fases[3] = CriarFase(3, false);
    fases[4] = CriarFase(4, false);
    fases[5] = CriarFase(5, false);

    Conectar(fases[1], fases[2], fases[3]);
    Conectar(fases[2], fases[4], fases[5]);
    if (fases[3]) { fases[3]->esquerda = NULL; fases[3]->direita = NULL; }

    return fases[1];
}

static void LiberarFases(NoFase** fases, int total) {
    for (int i = 1; i < total; ++i) {
        if (fases[i]) free(fases[i]);
    }
}

static int NavegarPara(NoFase** fases, int faseAtual, bool direita) {
    NoFase* atual = fases[faseAtual];
    if (!atual) return 1;

    NoFase* destino = NULL;
    if (direita) {
        destino = atual->direita;
    } else {
        destino = atual->esquerda;
    }
    if (destino) return destino->id;
    return 1;
}

static void AplicarProgressoSessao(NoFase** fases) {
    for (int i = 1; i <= 5; ++i) {
        unsigned int flag = (1u << i);
        if ((gProgressMask & flag) != 0) {
            AtualizarDesbloqueios(fases, i);
        }
    }
}

void AtualizarDesbloqueios(NoFase** fases, int faseConcluida) {
    if (!fases) return;
    switch (faseConcluida) {
        case 1:
            if (fases[2]) fases[2]->desbloqueada = true;
            if (fases[3]) fases[3]->desbloqueada = true;
            break;
        case 2:
            if (fases[4]) fases[4]->desbloqueada = true;
            if (fases[5]) fases[5]->desbloqueada = true;
            break;
        default:
            break;
    }
}

// --- Mapa de fases ---
bool MostrarMapaFases(void) {
    const int screenWidth = 1920;
    const int screenHeight = 1080;
    MapaTexturas mapas;
    CarregarMapaTexturas(&mapas);

    NoFase* fases[6] = {0};
    NoFase* raiz = CriarArvoreFases(fases);

    // Aplica progresso em memória para reabrir fases já concluídas nesta sessão
    AplicarProgressoSessao(fases);

    int faseSelecionada = 1;
    int faseConcluida = 0;
    bool confirmExit = false;
    bool sairDoMapa = false;
    // Botão de desbloqueio (debug)
    Rectangle btnUnlock = (Rectangle){ screenWidth - 300.0f, 20.0f, 280.0f, 40.0f };
    Rectangle btnSkip   = (Rectangle){ 20.0f, screenHeight - 80.0f, 360.0f, 40.0f };

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
        if (!confirmExit && IsKeyPressed(KEY_LEFT)) {
            faseSelecionada = NavegarPara(fases, faseSelecionada, true);
        }
        if (!confirmExit && IsKeyPressed(KEY_RIGHT)) {
            faseSelecionada = NavegarPara(fases, faseSelecionada, false);
        }

        // --- Botão: Desbloquear todas as fases (clique ou tecla U) ---
        if (!confirmExit) {
            Vector2 mp = GetMousePosition();
            bool hover = CheckCollisionPointRec(mp, btnUnlock);
            if ((hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) || IsKeyPressed(KEY_U)) {
                for (int i = 1; i <= 5; ++i) {
                    if (fases[i]) fases[i]->desbloqueada = true;
                    gProgressMask |= (1u << i);
                }
            }

            bool skipHover = CheckCollisionPointRec(mp, btnSkip);
            if ((skipHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) || IsKeyPressed(KEY_P)) {
                NoFase* atual = fases[faseSelecionada];
                if (atual && atual->desbloqueada) {
                    gProgressMask |= (1u << atual->id);
                    AtualizarDesbloqueios(fases, atual->id);
                }
            }
        }

        // --- Entrar na fase selecionada (somente ao apertar ENTER) ---
        if (!confirmExit && IsKeyPressed(KEY_ENTER)) {
            NoFase* atual = fases[faseSelecionada];

            if (atual && atual->desbloqueada) {
                bool concluida = false;
                // Abre a fase correspondente
                switch (atual->id) {
                    case 1: concluida = Fase1(); break;       // Raiz
                    case 2: concluida = Fase2(); break;       // Raiz->Esquerda (2ª fase)
                    case 3: concluida = Fase3(); break;       // Raiz->Direita (3ª fase)
                    case 4: concluida = Fase5(); break;       // Raiz->Esq->Esq (4ª fase)
                    case 5: concluida = Fase4(); break;       // Raiz->Esq->Dir (5ª fase)
                }

                if (concluida) {
                    faseConcluida = atual->id;
                    gProgressMask |= (1u << faseConcluida);
                    AtualizarDesbloqueios(fases, faseConcluida);
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

        Texture2D* mapaAtual = SelecionarMapaAtual(&mapas, fases[2], fases[3], fases[4]);

        Rectangle srcMapa = {0, 0, (float)mapaAtual->width, (float)mapaAtual->height};
        Rectangle dstMapa = {0, 0, (float)screenWidth, (float)screenHeight};
        DrawTexturePro(*mapaAtual, srcMapa, dstMapa, (Vector2){0, 0}, 0.0f, WHITE);

        DrawText("MAPA DE FASES", 780, 40, 40, YELLOW);
        DrawText("Setas <- -> para selecionar | ENTER para jogar", 600, 100, 20, RAYWHITE);
        DrawText("ESC para voltar ao menu", 780, 130, 20, GRAY);
        DrawText("T para Fase Teste (dev)", 780, 160, 20, (Color){180,180,220,255});

        NoFase* faseAtual = fases[faseSelecionada];
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
        Color corIndicador;
        if (faseDisponivel) {
            corIndicador = YELLOW;
        } else {
            corIndicador = (Color){120, 120, 120, 220};
        }

        DrawCircleLines(posIndicador.x, posIndicador.y, 70.0f, corIndicador);
        DrawCircleLines(posIndicador.x, posIndicador.y, 76.0f, (Color){0, 0, 0, 120});

        const char* msgStatus;
        Color corStatus;
        if (faseDisponivel) {
            msgStatus = "ENTER para iniciar";
            corStatus = GREEN;
        } else {
            msgStatus = "Conclua a fase anterior para liberar";
            corStatus = GRAY;
        }
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

        bool skipHover = CheckCollisionPointRec(GetMousePosition(), btnSkip);
        Color cSkip = skipHover ? (Color){180,120,40,255} : (Color){140,90,30,255};
        DrawRectangleRec(btnSkip, cSkip);
        DrawRectangleLinesEx(btnSkip, 2, BLACK);
        DrawText("MARCAR FASE COMO CONCLUIDA (P)", btnSkip.x + 12, btnSkip.y + 10, 20, WHITE);

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

    DescarregarMapaTexturas(&mapas);
    LiberarFases(fases, 6);
    return false;
}

void ResetarProgressoFases(void) {
    gProgressMask = 0;
}
