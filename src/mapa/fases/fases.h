#ifndef FASES_H
#define FASES_H

#include <stdbool.h>
#include "../../player/player.h"
#include "raylib.h"               // garante acesso às funções gráficas

// Cada fase retorna true se concluída (linha de chegada), false se saiu (ESC)
bool Fase1(void);
bool Fase2(void);
bool Fase3(void);
bool Fase4(void);
bool Fase5(void);

#endif
