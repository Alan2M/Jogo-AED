#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include <stdbool.h>

#define MAX_FRAMES 6

typedef struct {
    Rectangle rect;
    Vector2 velocity;
    bool isJumping;
    bool facingRight;
    bool idle;

    Texture2D frames[MAX_FRAMES];
    int frameAtual;
    int totalFrames;
    float tempoFrame;
    float timer;
} Player;

void InitPlayer(Player *p);
void UpdatePlayer(Player *p, Rectangle ground);
void DrawPlayer(Player p);
void UnloadPlayer(Player *p);

#endif
