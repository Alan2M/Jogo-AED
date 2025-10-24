#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"

typedef struct {
    Rectangle rect;
    Vector2 velocity;
    bool isJumping;
} Player;

void InitPlayer(Player *p);
void UpdatePlayer(Player *p, Rectangle ground);
void DrawPlayer(Player p);

#endif
