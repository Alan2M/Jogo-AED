#include "ranking.h"
#include "raylib.h"
#include <string.h>
#include <stdio.h>

#define MAX_PHASES 6
#define MAX_ENTRIES 64

static RankEntry gRank[MAX_PHASES][MAX_ENTRIES];
static int gCount[MAX_PHASES];

void Ranking_Init(void) {
    for (int i = 0; i < MAX_PHASES; ++i) gCount[i] = 0;
}

static int cmpEntry(const void* a, const void* b) {
    const RankEntry* A = (const RankEntry*)a;
    const RankEntry* B = (const RankEntry*)b;
    if (A->timeSec < B->timeSec) return -1;
    if (A->timeSec > B->timeSec) return 1;
    return strcmp(A->name, B->name);
}

void Ranking_Add(int faseId, const char* name, float timeSec) {
    if (faseId < 1 || faseId >= MAX_PHASES) return;
    int c = gCount[faseId];
    if (c >= MAX_ENTRIES) c = MAX_ENTRIES - 1; else gCount[faseId] = c + 1;
    RankEntry e;
    memset(&e, 0, sizeof(e));
    if (name) {
        int i = 0; while (name[i] && i < (int)sizeof(e.name)-1) { e.name[i] = name[i]; i++; }
        e.name[i] = '\0';
    }
    e.timeSec = timeSec;
    gRank[faseId][c] = e;
    // Ordena simples (insertion-like): usa qsort local
    // Nota: w64devkit tem qsort em stdlib
    // Mas para evitar include, fazemos uma passada simples de bubble
    for (int i = gCount[faseId]-1; i > 0; --i) {
        if (gRank[faseId][i].timeSec < gRank[faseId][i-1].timeSec) {
            RankEntry tmp = gRank[faseId][i-1];
            gRank[faseId][i-1] = gRank[faseId][i];
            gRank[faseId][i] = tmp;
        }
    }
}

int Ranking_GetCount(int faseId) {
    if (faseId < 1 || faseId >= MAX_PHASES) return 0;
    return gCount[faseId];
}

const RankEntry* Ranking_GetEntry(int faseId, int index) {
    if (faseId < 1 || faseId >= MAX_PHASES) return NULL;
    if (index < 0 || index >= gCount[faseId]) return NULL;
    return &gRank[faseId][index];
}

bool Ranking_NameExists(const char* name) {
    if (!name || !name[0]) return false;
    for (int f = 1; f < MAX_PHASES; ++f) {
        for (int i = 0; i < gCount[f]; ++i) {
            if (strcmp(gRank[f][i].name, name) == 0) return true;
        }
    }
    return false;
}

static void drawTime(float t, int x, int y, int fontSize, Color c) {
    int minutes = (int)(t / 60.0f);
    float secs = t - minutes*60;
    char buf[64];
    sprintf(buf, "%02d:%05.2f", minutes, secs);
    DrawText(buf, x, y, fontSize, c);
}

void MostrarRanking(void) {
    int fase = 1;
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){10, 10, 14, 255});
        DrawText("RANKING", 820, 60, 50, YELLOW);
        DrawText("LEFT/RIGHT: Fase | ESC: Voltar", 700, 120, 20, GRAY);

        // Fase selector
        char titulo[64];
        sprintf(titulo, "Fase %d", fase);
        DrawText(titulo, 900, 170, 30, GOLD);

        int count = Ranking_GetCount(fase);
        int startY = 220;
        int lineH = 34;
        for (int i = 0; i < count && i < 15; ++i) {
            const RankEntry* e = Ranking_GetEntry(fase, i);
            if (!e) continue;
            char nameBuf[80];
            sprintf(nameBuf, "%2d. %s", i+1, e->name[0] ? e->name : "<anon>");
            DrawText(nameBuf, 700, startY + i*lineH, 28, RAYWHITE);
            drawTime(e->timeSec, 1200, startY + i*lineH, 28, LIGHTGRAY);
        }

        EndDrawing();

        if (IsKeyPressed(KEY_RIGHT)) { fase++; if (fase > 5) fase = 1; }
        if (IsKeyPressed(KEY_LEFT))  { fase--; if (fase < 1) fase = 5; }
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) break;
    }
}
