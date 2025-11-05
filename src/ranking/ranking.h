#ifndef RANKING_H
#define RANKING_H

#include <stdbool.h>

typedef struct {
    char name[64];
    float timeSec; // tempo para concluir
} RankEntry;

void Ranking_Init(void);
void Ranking_Add(int faseId, const char* name, float timeSec);
int  Ranking_GetCount(int faseId);
const RankEntry* Ranking_GetEntry(int faseId, int index);
// Verifica se já existe alguma entrada com este nome (qualquer fase)
bool Ranking_NameExists(const char* name);

// Tela para visualizar ranking por fase
void MostrarRanking(void);

#endif
