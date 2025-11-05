#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include <stdbool.h>

typedef struct Player {
    Rectangle rect;
    Vector2 velocity;
    bool isJumping;
    bool facingRight;
    bool idle;

    Texture2D frames[10];
    int totalFrames;
    int frameAtual;
    float tempoFrame;
    float timer;
} Player;

void InitEarthboy(Player *p);
void InitFireboy(Player *p);
void InitWatergirl(Player *p);

void UpdatePlayer(Player *p, Rectangle ground, int keyLeft, int keyRight, int keyJump);
void DrawPlayer(Player p);
void UnloadPlayer(Player *p);

#endif
