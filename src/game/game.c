#include <stdbool.h>
#include <string.h>

static int gReturnToMenu = 0;
static char gPlayerName[64] = {0};

void Game_SetReturnToMenu(bool v) { gReturnToMenu = v ? 1 : 0; }
bool Game_ShouldReturnToMenu(void) { return gReturnToMenu != 0; }
void Game_ClearReturnToMenu(void) { gReturnToMenu = 0; }

void Game_SetPlayerName(const char* name) {
    if (!name) { gPlayerName[0] = '\0'; return; }
    int i = 0; while (name[i] && i < (int)sizeof(gPlayerName)-1) { gPlayerName[i] = name[i]; i++; }
    gPlayerName[i] = '\0';
}

const char* Game_GetPlayerName(void) {
    return gPlayerName[0] ? gPlayerName : "PLAYER";
}

bool Game_HasPlayerName(void) {
    return gPlayerName[0] != '\0';
}
