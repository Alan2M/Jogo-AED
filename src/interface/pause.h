#ifndef PAUSE_H
#define PAUSE_H

typedef enum {
    PAUSE_RESUME = 0,
    PAUSE_TO_MAP,
    PAUSE_TO_MENU
} PauseResult;

// Exibe menu de pausa modal e retorna a escolha.
PauseResult ShowPauseMenu(void);

#endif

