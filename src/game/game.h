// Estado global simples para sinalizar retorno ao menu a partir de uma fase
#ifndef GAME_H
#define GAME_H

#include <stdbool.h>

void Game_SetReturnToMenu(bool v);
bool Game_ShouldReturnToMenu(void);
void Game_ClearReturnToMenu(void);

// Player name management (session-wide)
void Game_SetPlayerName(const char* name);
const char* Game_GetPlayerName(void);
bool Game_HasPlayerName(void);

#endif
