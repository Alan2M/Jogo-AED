#include "goal.h"
#include "../player/player.h"

void GoalInit(Goal* g, float x, float y, float w, float h, Color c) {
    g->rect = (Rectangle){ x, y, w, h };
    g->color = c;
}

void GoalDraw(const Goal* g) {
    DrawRectangleRec(g->rect, g->color);
    DrawRectangleLines((int)g->rect.x, (int)g->rect.y, (int)g->rect.width, (int)g->rect.height, BLACK);
    DrawText("CHEGADA", (int)(g->rect.x - 10), (int)(g->rect.y - 30), 20, g->color);
}

bool GoalReached(const Goal* g, const Player* p1, const Player* p2, const Player* p3) {
    if (CheckCollisionRecs(g->rect, p1->rect)) return true;
    if (CheckCollisionRecs(g->rect, p2->rect)) return true;
    if (CheckCollisionRecs(g->rect, p3->rect)) return true;
    return false;
}

