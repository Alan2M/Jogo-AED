#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include <stdbool.h>

// Visual size of the sprites stays the same, but the hitbox is slightly narrower
// to avoid players looking like they're hovering when reaching block edges.
#define PLAYER_VISUAL_WIDTH 60.0f
#define PLAYER_VISUAL_HEIGHT 60.0f
#define PLAYER_HITBOX_WIDTH 44.0f
#define PLAYER_HITBOX_HEIGHT 60.0f

typedef struct Player {
    Rectangle rect;
    Vector2 velocity;
    bool isJumping;
    bool facingRight;
    bool idle;

    Texture2D walkFrames[8];
    Texture2D idleFrames[4];
    int totalWalkFrames;
    int totalIdleFrames;
    
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
