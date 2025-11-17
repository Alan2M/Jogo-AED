#include "raylib.h"
#include "../../player/player.h"
#include "../../objects/lake.h"
#include "../../objects/goal.h"
#include "../../objects/button.h"
#include "../../objects/fan.h"
#include "../../game/game.h"
#include "../../ranking/ranking.h"
#include "../../interface/pause.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <float.h>

#define MAX_COLISOES 1024
#define MAX_LAKE_SEGS 512
#define MAX_FANS 64
#define MAX_COOP_BOXES 4
#define STEP_HEIGHT  14.0f
#define MAX_BUTTONS 8
#define BUTTON_NAME_LEN 64

typedef struct { Rectangle rect; } Colisao;

typedef enum { PART_LEFT = 0, PART_MIDDLE, PART_RIGHT } LakePart;

typedef struct LakeSegment { Rectangle rect; LakeType type; LakePart part; } LakeSegment;

typedef struct LakeAnimFrames {
    Texture2D left[32];   int leftCount;
    Texture2D middle[32]; int middleCount;
    Texture2D right[32];  int rightCount;
    float timer; int frame;
} LakeAnimFrames;

typedef struct {
    Rectangle rect;
    float velX;
} CoOpBox;

typedef struct ButtonSpriteSet {
    Texture2D blue;
    Texture2D red;
    Texture2D white;
    Texture2D brown;
} ButtonSpriteSet;

typedef struct PhaseButton {
    Button button;
    char name[BUTTON_NAME_LEN];
    char nameLower[BUTTON_NAME_LEN];
    bool pressed;
} PhaseButton;

// Parser simples de grupos de objetos do Tiled (TMX)
static int ParseRectsFromGroup(const char* tmxPath, const char* groupName, Rectangle* out, int cap) {
    int count = 0; char* xml = LoadFileText(tmxPath); if (!xml) return 0;
    const char* search = xml;
    while ((search = strstr(search, "<objectgroup")) != NULL) {
        const char* tagClose = strchr(search, '>'); if (!tagClose) break;
        bool match = false; {
            size_t len = (size_t)(tagClose - search); char* header = (char*)malloc(len + 1);
            if (!header) { UnloadFileText(xml); return count; }
            memcpy(header, search, len); header[len] = '\0';
            char findName[128]; snprintf(findName, sizeof(findName), "name=\"%s\"", groupName);
            if (strstr(header, findName)) match = true; free(header);
        }
        const char* groupEnd = strstr(tagClose + 1, "</objectgroup>"); if (!groupEnd) break;
        if (match) {
            const char* p = tagClose + 1;
            while (p < groupEnd) {
                const char* obj = strstr(p, "<object "); if (!obj || obj >= groupEnd) break;
                float x=0,y=0,w=0,h=0; sscanf(obj, "<object id=%*[^x]x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\"", &x,&y,&w,&h);
                if (w>0 && h>0 && count < cap) out[count++] = (Rectangle){x,y,w,h};
                p = obj + 8;
            }
        }
        search = groupEnd ? (groupEnd + 14) : (search + 11);
    }
    UnloadFileText(xml); return count;
}

static int ParseRectsFromAny(const char* tmxPath, const char** names, int nNames, Rectangle* out, int cap) {
    int t = 0;
    for (int i = 0; i < nNames && t < cap; i++) t += ParseRectsFromGroup(tmxPath, names[i], out + t, cap - t);
    return t;
}

static void ToLowerCopy(const char* src, char* dst, size_t dstSize) {
    if (!dstSize) return;
    size_t i = 0;
    for (; src[i] && i + 1 < dstSize; ++i) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

static bool StrContains(const char* hay, const char* needle) {
    return strstr(hay, needle) != NULL;
}

static LakeType DetectLakeType(const char* nameLower) {
    if (StrContains(nameLower, "agua")) return LAKE_WATER;
    if (StrContains(nameLower, "fogo")) return LAKE_FIRE;
    if (StrContains(nameLower, "marrom") || StrContains(nameLower, "terra")) return LAKE_EARTH;
    if (StrContains(nameLower, "verde") || StrContains(nameLower, "veneno") || StrContains(nameLower, "acido")) return LAKE_POISON;
    return LAKE_WATER;
}

static LakePart DetectLakePart(const char* nameLower) {
    if (StrContains(nameLower, "esq")) return PART_LEFT;
    if (StrContains(nameLower, "dir")) return PART_RIGHT;
    return PART_MIDDLE;
}

static int ParseLakeSegments(const char* tmxPath, LakeSegment* out, int cap) {
    int count = 0;
    char* xml = LoadFileText(tmxPath);
    if (!xml) return 0;

    const char* search = xml;
    while ((search = strstr(search, "<objectgroup")) != NULL) {
        const char* tagClose = strchr(search, '>');
        if (!tagClose) break;

        const char* groupEnd = strstr(tagClose + 1, "</objectgroup>");
        if (!groupEnd) break;

        size_t len = (size_t)(tagClose - search);
        char* header = (char*)malloc(len + 1);
        if (!header) { UnloadFileText(xml); return count; }
        memcpy(header, search, len);
        header[len] = '\0';

        const char* nameAttr = strstr(header, "name=\"");
        char nameBuf[128] = {0};
        if (nameAttr) {
            nameAttr += 6;
            const char* endQuote = strchr(nameAttr, '"');
            if (endQuote) {
                size_t nameLen = (size_t)(endQuote - nameAttr);
                if (nameLen >= sizeof(nameBuf)) nameLen = sizeof(nameBuf) - 1;
                memcpy(nameBuf, nameAttr, nameLen);
                nameBuf[nameLen] = '\0';
            }
        }
        free(header);

        if (nameBuf[0] == '\0') { search = groupEnd + 14; continue; }

        char nameLower[128];
        ToLowerCopy(nameBuf, nameLower, sizeof(nameLower));
        if (!StrContains(nameLower, "lago")) { search = groupEnd + 14; continue; }

        LakeType type = DetectLakeType(nameLower);
        LakePart part = DetectLakePart(nameLower);

        const char* p = tagClose + 1;
        while (p < groupEnd) {
            const char* obj = strstr(p, "<object ");
            if (!obj || obj >= groupEnd) break;
            float x=0,y=0,w=0,h=0;
            sscanf(obj, "<object id=%*[^x]x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\"", &x,&y,&w,&h);
            if (w>0 && h>0 && count < cap) out[count++] = (LakeSegment){ (Rectangle){x,y,w,h}, type, part };
            p = obj + 8;
        }

        search = groupEnd + 14;
    }

    UnloadFileText(xml);
    return count;
}

static int CollectButtonGroupNames(const char* tmxPath, char names[][BUTTON_NAME_LEN], int maxNames) {
    if (maxNames <= 0) return 0;
    char* xml = LoadFileText(tmxPath);
    if (!xml) return 0;
    int count = 0;
    const char* search = xml;
    while (count < maxNames && (search = strstr(search, "<objectgroup")) != NULL) {
        const char* tagClose = strchr(search, '>');
        if (!tagClose) break;
        const char* nameAttr = strstr(search, "name=\"");
        if (!nameAttr || nameAttr > tagClose) { search = tagClose + 1; continue; }
        nameAttr += 6;
        char nameBuf[BUTTON_NAME_LEN];
        size_t idx = 0;
        while (nameAttr[idx] && nameAttr[idx] != '"' && idx + 1 < sizeof(nameBuf)) {
            nameBuf[idx] = nameAttr[idx];
            idx++;
        }
        nameBuf[idx] = '\0';
        char lower[BUTTON_NAME_LEN];
        ToLowerCopy(nameBuf, lower, sizeof(lower));
        if (!strstr(lower, "bota")) { search = tagClose + 1; continue; }
        bool exists = false;
        for (int i = 0; i < count; ++i) {
            if (strcmp(names[i], nameBuf) == 0) { exists = true; break; }
        }
        if (!exists) {
            strncpy(names[count], nameBuf, BUTTON_NAME_LEN - 1);
            names[count][BUTTON_NAME_LEN - 1] = '\0';
            count++;
        }
        search = tagClose + 1;
    }
    UnloadFileText(xml);
    return count;
}

static void DetermineButtonColors(const char* nameLower, Color* up, Color* down) {
    if (strstr(nameLower, "azul")) {
        *up = (Color){70, 120, 240, 220};
        *down = (Color){40, 85, 200, 255};
        return;
    }
    if (strstr(nameLower, "verm")) {
        *up = (Color){210, 70, 70, 220};
        *down = (Color){150, 40, 40, 255};
        return;
    }
    if (strstr(nameLower, "branc")) {
        *up = (Color){230, 230, 230, 220};
        *down = (Color){190, 190, 210, 255};
        return;
    }
    if (strstr(nameLower, "marr") || strstr(nameLower, "terra")) {
        *up = (Color){180, 120, 70, 220};
        *down = (Color){140, 90, 50, 255};
        return;
    }
    *up = (Color){200, 200, 40, 200};
    *down = (Color){200, 140, 20, 255};
}

static const Texture2D* PickButtonSprite(const char* nameLower, const ButtonSpriteSet* sprites) {
    if (strstr(nameLower, "branc") && sprites->white.id != 0) return &sprites->white;
    if (strstr(nameLower, "azul") && sprites->blue.id != 0) return &sprites->blue;
    if (strstr(nameLower, "verm") && sprites->red.id != 0) return &sprites->red;
    if ((strstr(nameLower, "marr") || strstr(nameLower, "terra")) && sprites->brown.id != 0) return &sprites->brown;
    if (sprites->blue.id != 0) return &sprites->blue;
    if (sprites->red.id != 0) return &sprites->red;
    if (sprites->white.id != 0) return &sprites->white;
    if (sprites->brown.id != 0) return &sprites->brown;
    return NULL;
}

static bool AnyButtonPressedWithToken(const PhaseButton* buttons, int count, const char* tokenLower) {
    if (!tokenLower || !tokenLower[0]) return false;
    for (int i = 0; i < count; ++i) {
        if (buttons[i].pressed && strstr(buttons[i].nameLower, tokenLower)) return true;
    }
    return false;
}

static Texture2D LoadTextureIfExists(const char* path) {
    if (!FileExists(path)) return (Texture2D){0};
    return LoadTexture(path);
}

static void DrawFanSprite(Rectangle rect, bool active, const Texture2D* onFrames, int onCount, Texture2D offTex, int animFrame) {
    Texture2D tex = offTex;
    bool usingOffTex = !active && offTex.id != 0;
    if (active && onCount > 0) tex = onFrames[animFrame % onCount];
    else if (!active && offTex.id == 0 && onCount > 0) tex = onFrames[0];

    if (tex.id != 0) {
        Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
        Rectangle dst = rect;
        if (usingOffTex) {
            dst.width = (float)tex.width;
            dst.height = (float)tex.height;
            dst.x = rect.x + (rect.width - dst.width) * 0.5f;
            dst.y = rect.y + rect.height - dst.height;
        }
        DrawTexturePro(tex, src, dst, (Vector2){0,0}, 0.0f, WHITE);
    } else if (active) {
        DrawRectangleRec(rect, (Color){120, 180, 255, 80});
        DrawRectangleLinesEx(rect, 1, (Color){120, 160, 220, 160});
    }
}

static Rectangle AcquireSpriteForRect(Rectangle target, Rectangle* sprites, bool* used, int spriteCount) {
    if (spriteCount <= 0) return target;
    Vector2 targetRef = { target.x + target.width * 0.5f, target.y + target.height };
    int best = -1;
    float bestDist = FLT_MAX;
    for (int i = 0; i < spriteCount; ++i) {
        if (used[i]) continue;
        Rectangle spr = sprites[i];
        Vector2 sprRef = { spr.x + spr.width * 0.5f, spr.y + spr.height };
        float dx = targetRef.x - sprRef.x;
        float dy = targetRef.y - sprRef.y;
        float dist = dx*dx + dy*dy;
        if (dist < bestDist) { bestDist = dist; best = i; }
    }
    if (best >= 0) { used[best] = true; return sprites[best]; }
    return target;
}

static bool PlayerPushingBox(const Player* pl, Rectangle box, bool pushRight, int key, float tol) {
    if (!IsKeyDown(key)) return false;
    Rectangle expanded = box;
    expanded.x -= 3.0f; expanded.width += 6.0f;
    expanded.y -= 4.0f; expanded.height += 8.0f;
    if (!CheckCollisionRecs(pl->rect, expanded)) return false;
    float pLeft = pl->rect.x;
    float pRight = pl->rect.x + pl->rect.width;
    if (pushRight) {
        if (pLeft >= box.x) return false;
        return fabsf(pRight - box.x) <= tol;
    } else {
        if (pRight <= box.x + box.width) return false;
        return fabsf(pLeft - (box.x + box.width)) <= tol;
    }
}

static void ResolvePlayerVsCoOpBox(Player* pl, const CoOpBox* box, float deltaX) {
    if (!CheckCollisionRecs(pl->rect, box->rect)) return;
    float dx = (pl->rect.x + pl->rect.width*0.5f) - (box->rect.x + box->rect.width*0.5f);
    float dy = (pl->rect.y + pl->rect.height*0.5f) - (box->rect.y + box->rect.height*0.5f);
    float overlapX = (pl->rect.width*0.5f + box->rect.width*0.5f) - fabsf(dx);
    float overlapY = (pl->rect.height*0.5f + box->rect.height*0.5f) - fabsf(dy);
    if (overlapX < overlapY) {
        if (dx > 0) pl->rect.x += overlapX;
        else pl->rect.x -= overlapX;
        pl->velocity.x = 0;
    } else {
        if (dy > 0 && pl->velocity.y < 0) {
            pl->rect.y += overlapY;
            pl->velocity.y = 0;
        } else if (dy < 0 && pl->velocity.y >= 0) {
            pl->rect.y -= overlapY;
            pl->velocity.y = 0;
            pl->isJumping = false;
            pl->rect.x += deltaX;
        }
    }
}

static void ResolvePlayerVsRect(Player* pl, Rectangle bloco) {
    if (bloco.width <= 0 || bloco.height <= 0) return;
    if (!CheckCollisionRecs(pl->rect, bloco)) return;

    float dx = (pl->rect.x + pl->rect.width * 0.5f) - (bloco.x + bloco.width * 0.5f);
    float dy = (pl->rect.y + pl->rect.height * 0.5f) - (bloco.y + bloco.height * 0.5f);
    float overlapX = (pl->rect.width * 0.5f + bloco.width * 0.5f) - fabsf(dx);
    float overlapY = (pl->rect.height * 0.5f + bloco.height * 0.5f) - fabsf(dy);

    if (overlapX < overlapY) {
        Rectangle teste = pl->rect;
        teste.y -= STEP_HEIGHT;
        if (!(dy > 0 && pl->velocity.y > 0) && !CheckCollisionRecs(teste, bloco)) {
            pl->rect.y -= STEP_HEIGHT;
            return;
        }
        if (dx > 0) pl->rect.x += overlapX;
        else        pl->rect.x -= overlapX;
        pl->velocity.x = 0;
    } else {
        if (dy > 0 && pl->velocity.y < 0) {
            pl->rect.y += overlapY;
            pl->velocity.y = 0;
        } else if (dy < 0 && pl->velocity.y >= 0) {
            pl->rect.y -= overlapY;
            pl->velocity.y = 0;
            pl->isJumping = false;
        }
    }
}

static void ResolveCoOpBoxVsWorld(CoOpBox* box, const Colisao* col, int colCount) {
    for (int i = 0; i < colCount; ++i) {
        Rectangle bloco = col[i].rect;
        if (!CheckCollisionRecs(box->rect, bloco)) continue;
        float dx = (box->rect.x + box->rect.width*0.5f) - (bloco.x + bloco.width*0.5f);
        float dy = (box->rect.y + box->rect.height*0.5f) - (bloco.y + bloco.height*0.5f);
        float overlapX = (box->rect.width*0.5f + bloco.width*0.5f) - fabsf(dx);
        float overlapY = (box->rect.height*0.5f + bloco.height*0.5f) - fabsf(dy);
        if (overlapX <= 0 || overlapY <= 0) continue;
        if (overlapX < overlapY) {
            if (dx > 0) box->rect.x += overlapX;
            else        box->rect.x -= overlapX;
            box->velX = 0;
        } else {
            if (dy > 0) box->rect.y += overlapY;
            else        box->rect.y -= overlapY;
        }
    }
}

static void ResolveCoOpBoxVsRect(CoOpBox* box, Rectangle bloco) {
    if (bloco.width <= 0 || bloco.height <= 0) return;
    if (box->rect.width <= 0 || box->rect.height <= 0) return;
    if (!CheckCollisionRecs(box->rect, bloco)) return;
    float dx = (box->rect.x + box->rect.width*0.5f) - (bloco.x + bloco.width*0.5f);
    float dy = (box->rect.y + box->rect.height*0.5f) - (bloco.y + bloco.height*0.5f);
    float overlapX = (box->rect.width*0.5f + bloco.width*0.5f) - fabsf(dx);
    float overlapY = (box->rect.height*0.5f + bloco.height*0.5f) - fabsf(dy);
    if (overlapX <= 0 || overlapY <= 0) return;
    if (overlapX < overlapY) {
        if (dx > 0) box->rect.x += overlapX;
        else        box->rect.x -= overlapX;
        box->velX = 0;
    } else {
        if (dy > 0) box->rect.y += overlapY;
        else        box->rect.y -= overlapY;
    }
}

static void HandleCoOpBoxOnPlatform(CoOpBox* box, Rectangle plat, float deltaY) {
    if (plat.width <= 0 || plat.height <= 0) return;
    if (box->rect.width <= 0 || box->rect.height <= 0) return;
    if (!CheckCollisionRecs(box->rect, plat)) return;
    float bBottom = box->rect.y + box->rect.height;
    if (bBottom <= plat.y + 12.0f) {
        box->rect.y = plat.y - box->rect.height;
        box->rect.y += deltaY;
    } else if (box->rect.y <= plat.y + plat.height && box->rect.y >= plat.y + plat.height - 12.0f) {
        box->rect.y = plat.y + plat.height;
    }
}

static bool BoxTouchesAnyLake(const CoOpBox* box, const LakeSegment* segs, int segCount) {
    if (box->rect.width <= 0 || box->rect.height <= 0) return false;
    for (int i = 0; i < segCount; ++i) {
        if (CheckCollisionRecs(box->rect, segs[i].rect)) return true;
    }
    return false;
}

// Plataforma simples (barra)
typedef struct Platform { Rectangle rect, area; float startY, speed; } Platform;
static void PlatformInit(Platform* p, Rectangle rect, Rectangle area, float speed) { p->rect=rect; p->area=area; p->startY=rect.y; p->speed=speed; }
static float moveTowards(float a, float b, float s) { if (a<b){a+=s; if(a>b)a=b;} else if(a>b){a-=s; if(a<b)a=b;} return a; }
static void HandlePlatformTop(Player* pl, Rectangle plat, float dY) {
    if (!CheckCollisionRecs(pl->rect, plat)) return; float pBottom = pl->rect.y + pl->rect.height;
    if (pBottom <= plat.y + 16.0f && pl->velocity.y >= -1.0f) { pl->rect.y = plat.y - pl->rect.height; pl->velocity.y = 0; pl->isJumping = false; pl->rect.y += dY; }
}

static bool PlayerAtDoor(const Player* p, Rectangle door) {
    if (door.width <= 0 || door.height <= 0) return false;
    float pxCenter = p->rect.x + p->rect.width * 0.5f;
    float doorLeft = door.x;
    float doorRight = door.x + door.width;
    const float toleranceX = 10.0f;
    if (pxCenter < doorLeft - toleranceX || pxCenter > doorRight + toleranceX) return false;
    float feet = p->rect.y + p->rect.height;
    float doorBase = door.y + door.height;
    const float toleranceY = 18.0f;
    return feet >= doorBase - toleranceY && feet <= doorBase + toleranceY;
}

// Lakes animation frames loaders
static int LoadFramesRange(Texture2D* arr, int max, const char* pattern, int startIdx, int endIdx) {
    int count = 0; bool started = false;
    for (int i = startIdx; i <= endIdx && count < max; ++i) {
        char path[256]; snprintf(path, sizeof(path), pattern, i);
        if (!FileExists(path)) { if (started) break; else continue; }
        Texture2D tex = LoadTexture(path);
        if (tex.id != 0) { arr[count++] = tex; started = true; }
        else if (started) { break; }
    }
    return count;
}

static void LoadLakeSet_Agua(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/agua/esquerdo/pixil-frame-%d.png",  0, 15);
    s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/agua/meio/pixil-frame-%d.png",      0, 15);
    s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/agua/direito/pixil-frame-%d.png",   0, 15);
    s->timer = 0.0f; s->frame = 0;
}

static void LoadLakeSet_Terra(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/terra/esquerdo/pixil-frame-%d.png", 0, 15);
    s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/terra/meio/pixil-frame-%d.png",     0, 15);
    s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/terra/direito/pixil-frame-%d.png",  0, 15);
    s->timer = 0.0f; s->frame = 0;
}

static void LoadLakeSet_Fogo(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/fogo/esquerdo/Esquerda%d.png", 1, 32);
    if (s->leftCount == 0)   s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/fogo/esquerdo/pixil-frame-%d.png", 0, 31);
    s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/fogo/meio/Meio%d.png",     1, 32);
    if (s->middleCount == 0) s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/fogo/meio/pixil-frame-%d.png",      0, 31);
    s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/fogo/direito/Direita%d.png", 1, 32);
    if (s->rightCount == 0)  s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/fogo/direito/pixil-frame-%d.png",   0, 31);
    s->timer = 0.0f; s->frame = 0;
}

static void LoadLakeSet_Acido(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/acido/esquerdo/pixil-frame-%d.png",  0, 15);
    s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/acido/meio/pixil-frame-%d.png",      0, 15);
    s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/acido/direito/pixil-frame-%d.png",   0, 15);
    s->timer = 0.0f; s->frame = 0;
}

static void UnloadLakeSet(LakeAnimFrames* s) {
    for (int i = 0; i < s->leftCount;   ++i) UnloadTexture(s->left[i]);
    for (int i = 0; i < s->middleCount; ++i) UnloadTexture(s->middle[i]);
    for (int i = 0; i < s->rightCount;  ++i) UnloadTexture(s->right[i]);
    s->leftCount = s->middleCount = s->rightCount = 0;
}

bool Fase5(void) {
    const char* tmx = "assets/maps/fase5/fase5.tmx";
    Texture2D mapTex = LoadTexture("assets/maps/fase5/fase5.png"); if (mapTex.id==0) { printf("Erro: mapa Fase2Mapa.png\n"); return false; }

    // Colisões
    Colisao col[MAX_COLISOES]; int nCol = 0; {
        const char* names[] = { "Colisao", "Colisão", "Colisoes", "Colisões" };
        Rectangle rtmp[MAX_COLISOES]; int n = ParseRectsFromAny(tmx, names, 4, rtmp, MAX_COLISOES);
        for (int i=0;i<n && i<MAX_COLISOES;i++) col[nCol++].rect = rtmp[i];
    }

    // Lake segments (detecta qualquer camada \"Lago_\")
    LakeSegment segs[MAX_LAKE_SEGS];
    int segCount = ParseLakeSegments(tmx, segs, MAX_LAKE_SEGS);

    // Caixas cooperativas
    CoOpBox coopBoxes[MAX_COOP_BOXES]; int coopBoxCount = 0;
    {
        Rectangle boxesTmp[MAX_COOP_BOXES];
        const char* names[] = { "Caixa", "caixa", "Caixas" };
        int nBoxes = ParseRectsFromAny(tmx, names, 3, boxesTmp, MAX_COOP_BOXES);
        for (int i=0;i<nBoxes && coopBoxCount < MAX_COOP_BOXES;i++) {
            coopBoxes[coopBoxCount].rect = boxesTmp[i];
            coopBoxes[coopBoxCount].velX = 0.0f;
            coopBoxCount++;
        }
    }

    // Load animation frames
    LakeAnimFrames animAgua={0}, animFogo={0}, animTerra={0}, animAcido={0};
    LoadLakeSet_Agua(&animAgua); LoadLakeSet_Fogo(&animFogo); LoadLakeSet_Terra(&animTerra); LoadLakeSet_Acido(&animAcido);

    // Chegada (opcional)
    Rectangle goalR[2]; int gC = ParseRectsFromGroup(tmx, "Chegada", goalR, 2); if (gC==0) gC = ParseRectsFromGroup(tmx, "Goal", goalR, 2);
    Rectangle goalRect = (gC>0) ? goalR[0] : (Rectangle){ mapTex.width-80, mapTex.height-140, 40, 120 }; Goal goal; GoalInit(&goal, goalRect.x, goalRect.y, goalRect.width, goalRect.height, GOLD);
    Rectangle doorTerra={0}, doorFogo={0}, doorAgua={0};
    {
        Rectangle doorTmp[2];
        if (ParseRectsFromGroup(tmx, "Porta_Terra", doorTmp, 2) > 0) doorTerra = doorTmp[0];
        if (ParseRectsFromGroup(tmx, "Porta_Fogo", doorTmp, 2) > 0) doorFogo = doorTmp[0];
        if (ParseRectsFromGroup(tmx, "Porta_Agua", doorTmp, 2) > 0) doorAgua = doorTmp[0];
    }

    // Spawns
    Vector2 spawnE={300,700}, spawnF={400,700}, spawnW={500,700};
    { Rectangle s[2]; int c;
      c = ParseRectsFromGroup(tmx, "Spawn_Terra", s, 2); if (c>0) { float cx = s[0].x + (s[0].width-45)*0.5f; spawnE=(Vector2){cx, s[0].y + s[0].height - 50}; }
      c = ParseRectsFromGroup(tmx, "Spawn_Fogo",  s, 2); if (c>0) { float cx = s[0].x + (s[0].width-45)*0.5f; spawnF=(Vector2){cx, s[0].y + s[0].height - 50}; }
      c = ParseRectsFromGroup(tmx, "Spawn_Agua",  s, 2); if (c>0) { float cx = s[0].x + (s[0].width-45)*0.5f; spawnW=(Vector2){cx, s[0].y + s[0].height - 50}; } }

    // Jogadores
    Player earthboy, fireboy, watergirl; InitEarthboy(&earthboy); InitFireboy(&fireboy); InitWatergirl(&watergirl);
    earthboy.rect = (Rectangle){spawnE.x, spawnE.y, 45, 50}; fireboy.rect = (Rectangle){spawnF.x, spawnF.y, 45, 50}; watergirl.rect = (Rectangle){spawnW.x, spawnW.y, 45, 50};

    // Botões com sprites
    ButtonSpriteSet buttonSprites = {0};
    buttonSprites.blue  = LoadTexture("assets/map/buttons/pixil-layer-bluebutton.png");
    buttonSprites.red   = LoadTexture("assets/map/buttons/pixil-layer-redbutton.png");
    buttonSprites.white = LoadTexture("assets/map/buttons/pixil-layer-whitebutton.png");
    buttonSprites.brown = LoadTexture("assets/map/buttons/pixil-layer-brownbutton.png");
    PhaseButton buttons[MAX_BUTTONS] = {0};
    int buttonCount = 0;
    char buttonNames[MAX_BUTTONS][BUTTON_NAME_LEN] = {{0}};
    int buttonGroups = CollectButtonGroupNames(tmx, buttonNames, MAX_BUTTONS);
    Rectangle btnRect[4];
    for (int g = 0; g < buttonGroups && buttonCount < MAX_BUTTONS; ++g) {
        int rects = ParseRectsFromGroup(tmx, buttonNames[g], btnRect, (int)(sizeof(btnRect)/sizeof(btnRect[0])));
        if (rects <= 0) continue;
        char lowerName[BUTTON_NAME_LEN];
        ToLowerCopy(buttonNames[g], lowerName, sizeof(lowerName));
        Color colorUp, colorDown;
        DetermineButtonColors(lowerName, &colorUp, &colorDown);
        const Texture2D* sprite = PickButtonSprite(lowerName, &buttonSprites);
        for (int r = 0; r < rects && buttonCount < MAX_BUTTONS; ++r) {
            ButtonInit(&buttons[buttonCount].button, btnRect[r].x, btnRect[r].y, btnRect[r].width, btnRect[r].height, colorUp, colorDown);
            if (sprite) ButtonSetSprites(&buttons[buttonCount].button, sprite, NULL);
            strncpy(buttons[buttonCount].name, buttonNames[g], BUTTON_NAME_LEN - 1);
            buttons[buttonCount].name[BUTTON_NAME_LEN - 1] = '\0';
            ToLowerCopy(buttonNames[g], buttons[buttonCount].nameLower, BUTTON_NAME_LEN);
            buttons[buttonCount].pressed = false;
            buttonCount++;
        }
    }

    // Plataformas
    typedef struct Platform Platform; Platform elev={0}, barraM={0}, elev2={0}, barra3={0};
    Texture2D barraElevTex = LoadTexture("assets/map/barras/amarelo.png");
    if (barraElevTex.id==0) barraElevTex = LoadTexture("assets/map/barras/branca.png");
    Texture2D barraMovTex = LoadTexture("assets/map/barras/barragorda.png");
    bool barraMovShared = false;
    Texture2D barraMovDefault = LoadTexture("assets/map/barras/branca.png");
    if (barraMovTex.id==0) {
        barraMovTex = barraMovDefault;
        if (barraMovTex.id==0) { barraMovTex = barraElevTex; barraMovShared = true; }
    }
    Texture2D coopBoxTex = LoadTexture("assets/map/caixa/caixa2.png");
    bool coopBoxShared = false;
    if (coopBoxTex.id == 0) coopBoxTex = LoadTexture("assets/map/caixa/caixa.png");
    if (coopBoxTex.id == 0) { coopBoxTex = barraMovTex; coopBoxShared = true; }
    {
        Rectangle r1[2], r2[2];
        int c1 = ParseRectsFromGroup(tmx, "Barra_Elevador1_Colisao", r1, 2);
        int c2 = ParseRectsFromGroup(tmx, "Barra_Elevador1", r2, 2);
        if (c1>0 && c2>0) PlatformInit(&elev, r1[0], r2[0], 2.5f);
        c1 = ParseRectsFromGroup(tmx, "Barra_Elevador2_Colisao", r1, 2);
        c2 = ParseRectsFromGroup(tmx, "Barra_Elevador2", r2, 2);
        if (c2==0) c2 = ParseRectsFromGroup(tmx, "Barrra_Elevador2", r2, 2);
        if (c1>0 && c2>0) PlatformInit(&elev2, r1[0], r2[0], 2.5f);
        c1 = ParseRectsFromGroup(tmx, "Barra3", r1, 2);
        c2 = ParseRectsFromGroup(tmx, "Area_Barra3", r2, 2);
        if (c1>0 && c2>0) {
            PlatformInit(&barra3, r1[0], r2[0], 2.0f);
            barra3.rect.y = barra3.area.y;
            barra3.startY = barra3.rect.y;
        }
        c1 = ParseRectsFromGroup(tmx, "Barra1_Movel", r1, 2);
        c2 = ParseRectsFromGroup(tmx, "AreaMovimentoBarraMovel", r2, 2);
        if (c1>0 && c2>0) PlatformInit(&barraM, r1[0], r2[0], 2.0f);
    }

    // Ventiladores: 1 e 3 sempre ligados; 2 depende do b3 apenas
    Fan fans1[MAX_FANS]; int fans1Count=0; {
        Rectangle vr[16]; int vc = ParseRectsFromGroup(tmx, "Ventilador1", vr, 16); if (vc==0) vc = ParseRectsFromGroup(tmx, "Ventilador 1", vr, 16);
        for (int i=0;i<vc && fans1Count<MAX_FANS;i++) FanInit(&fans1[fans1Count++], vr[i].x, vr[i].y, vr[i].width, vr[i].height, 0.6f);
        if (fans1Count==0) { vc = ParseRectsFromGroup(tmx, "Ventilador", vr, 16); for (int i=0;i<vc && fans1Count<MAX_FANS;i++) FanInit(&fans1[fans1Count++], vr[i].x, vr[i].y, vr[i].width, vr[i].height, 0.6f); }
    }
    Fan fans3[MAX_FANS]; int fans3Count=0; {
        Rectangle vr[16]; int vc = ParseRectsFromGroup(tmx, "Ventilador3", vr, 16); if (vc==0) vc = ParseRectsFromGroup(tmx, "Ventilador 3", vr, 16);
        for (int i=0;i<vc && fans3Count<MAX_FANS;i++) FanInit(&fans3[fans3Count++], vr[i].x, vr[i].y, vr[i].width, vr[i].height, 0.75f);
    }
    Fan vent2={0}; bool haveVent2=false; { Rectangle vr[2]; int vc=ParseRectsFromGroup(tmx, "Ventilador2", vr, 2); if (vc>0) { FanInit(&vent2, vr[0].x, vr[0].y, vr[0].width, vr[0].height, 0.8f); haveVent2=true; } }

    Rectangle fanSpriteRects[32]; bool fanSpriteUsed[32] = { false };
    int fanSpriteCount = ParseRectsFromGroup(tmx, "AnimarVentilador", fanSpriteRects, 32);
    Rectangle fan1Draw[MAX_FANS]; Rectangle fan3Draw[MAX_FANS]; Rectangle vent2DrawRect = vent2.rect;
    for (int i=0;i<fans1Count;i++) fan1Draw[i] = AcquireSpriteForRect(fans1[i].rect, fanSpriteRects, fanSpriteUsed, fanSpriteCount);
    for (int i=0;i<fans3Count;i++) fan3Draw[i] = AcquireSpriteForRect(fans3[i].rect, fanSpriteRects, fanSpriteUsed, fanSpriteCount);
    if (haveVent2) vent2DrawRect = AcquireSpriteForRect(vent2.rect, fanSpriteRects, fanSpriteUsed, fanSpriteCount);

    Texture2D fanOffTex = LoadTextureIfExists("assets/map/vento/desligado.png");
    Texture2D fanOnFrames[8]; int fanOnCount = 0;
    const char* fanPaths[] = {
        "assets/map/vento/ligado1.png",
        "assets/map/vento/ligado2.png",
        "assets/map/vento/ligado3.png",
        "assets/map/vento/ligado4.png"
    };
    for (int i = 0; i < (int)(sizeof(fanPaths)/sizeof(fanPaths[0])); ++i) {
        Texture2D tex = LoadTextureIfExists(fanPaths[i]);
        if (tex.id != 0 && fanOnCount < (int)(sizeof(fanOnFrames)/sizeof(fanOnFrames[0]))) {
            fanOnFrames[fanOnCount++] = tex;
        }
    }
    float fanAnimTimer = 0.0f; int fanAnimFrame = 0;
    float fan2AnimTimer = 0.0f; int fan2AnimFrame = 0;
    const float FAN_FRAME_TIME = 0.12f;

    // Câmera
    Camera2D cam = {0}; cam.target=(Vector2){mapTex.width/2.0f,mapTex.height/2.0f}; cam.offset=(Vector2){GetScreenWidth()/2.0f,GetScreenHeight()/2.0f}; cam.zoom=1.0f;

    bool completed=false; float elapsed=0.0f; bool debug=false;
    bool tempoButtonLatched=false; float tempoButtonTimer=0.0f; float barra3Timer=0.0f;
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        float frameDt = GetFrameTime();
        elapsed += frameDt;
        if (IsKeyPressed(KEY_TAB)) debug=!debug;
        if (IsKeyPressed(KEY_ESCAPE)) {
            PauseResult pr = ShowPauseMenu();
            if (pr == PAUSE_TO_MAP) { completed = false; break; }
            if (pr == PAUSE_TO_MENU) { Game_SetReturnToMenu(true); completed = false; break; }
        }
        if (fanOnCount > 0) {
            fanAnimTimer += frameDt;
            if (fanAnimTimer >= FAN_FRAME_TIME) {
                fanAnimTimer -= FAN_FRAME_TIME;
                fanAnimFrame = (fanAnimFrame + 1) % fanOnCount;
            }
        }

        for (int i = 0; i < buttonCount; ++i) {
            buttons[i].pressed = ButtonUpdate(&buttons[i].button, &earthboy, &fireboy, &watergirl);
        }

        bool p1 = AnyButtonPressedWithToken(buttons, buttonCount, "botao1");
        bool p2 = AnyButtonPressedWithToken(buttons, buttonCount, "botao2");
        bool p3 = AnyButtonPressedWithToken(buttons, buttonCount, "botao3");
        bool tempoPressed = AnyButtonPressedWithToken(buttons, buttonCount, "botao_tempo");
        if (tempoPressed && !tempoButtonLatched) {
            tempoButtonLatched = true;
            tempoButtonTimer = 15.0f; // elevador inferior por 15s
            barra3Timer = 30.0f;      // barra superior por 30s
        }
        if (tempoButtonTimer > 0.0f) {
            tempoButtonTimer -= frameDt;
            if (tempoButtonTimer < 0.0f) tempoButtonTimer = 0.0f;
        }
        if (barra3Timer > 0.0f) {
            barra3Timer -= frameDt;
            if (barra3Timer < 0.0f) barra3Timer = 0.0f;
        }
        bool elev2Btn1 = AnyButtonPressedWithToken(buttons, buttonCount, "botao1_barra1_morvel_branco");
        bool elev2Btn4 = AnyButtonPressedWithToken(buttons, buttonCount, "botao4_branco");
        bool elev2Active = (tempoButtonTimer > 0.0f) || (elev2Btn1 && elev2Btn4);
        if (tempoButtonLatched) {
            for (int i = 0; i < buttonCount; ++i) {
                if (strstr(buttons[i].nameLower, "botao_tempo")) buttons[i].pressed = true;
            }
        }

        float elevPrevY = elev.rect.y, elev2PrevY = elev2.rect.y, barraPrevY = barraM.rect.y, barra3PrevY = barra3.rect.y;
        if (elev.area.height>0 && elev.rect.height>0) {
            if (p1 && !p2 && !p3) { float base = elev.area.y + elev.area.height - elev.rect.height; elev.rect.y = moveTowards(elev.rect.y, base, elev.speed); }
            else { elev.rect.y = moveTowards(elev.rect.y, elev.startY, elev.speed); }
            float minY=elev.area.y, maxY=elev.area.y+elev.area.height-elev.rect.height; if (elev.rect.y<minY) elev.rect.y=minY; if (elev.rect.y>maxY) elev.rect.y=maxY;
        }
        if (elev2.area.height>0 && elev2.rect.height>0) {
            float targetDown = elev2.area.y + elev2.area.height - elev2.rect.height;
            float target = elev2Active ? targetDown : elev2.startY;
            elev2.rect.y = moveTowards(elev2.rect.y, target, elev2.speed);
            float minY=elev2.area.y, maxY=elev2.area.y+elev2.area.height-elev2.rect.height;
            if (elev2.rect.y<minY) elev2.rect.y=minY;
            if (elev2.rect.y>maxY) elev2.rect.y=maxY;
        }
        if (barra3.area.height>0 && barra3.rect.height>0) {
            float targetDown = barra3.area.y + barra3.area.height - barra3.rect.height;
            float targetUp = barra3.startY;
            bool active = barra3Timer > 0.0f;
            float target = active ? targetDown : targetUp;
            barra3.rect.y = moveTowards(barra3.rect.y, target, barra3.speed);
            float minY=barra3.area.y, maxY=barra3.area.y+barra3.area.height-barra3.rect.height;
            if (barra3.rect.y<minY) barra3.rect.y=minY;
            if (barra3.rect.y>maxY) barra3.rect.y=maxY;
        }
        if (barraM.area.height>0 && barraM.rect.height>0) {
            int pressed=(p1?1:0)+(p2?1:0)+(p3?1:0);
            if (pressed>=2) { float topo = barraM.area.y; barraM.rect.y = moveTowards(barraM.rect.y, topo, barraM.speed); }
            else { barraM.rect.y = moveTowards(barraM.rect.y, barraM.startY, barraM.speed); }
            float minY=barraM.area.y, maxY=barraM.area.y+barraM.area.height-barraM.rect.height; if (barraM.rect.y<minY) barraM.rect.y=minY; if (barraM.rect.y>maxY) barraM.rect.y=maxY;
        }
        float elevDY = elev.rect.y - elevPrevY;
        float elev2DY = elev2.rect.y - elev2PrevY;
        float barra3DY = barra3.rect.y - barra3PrevY;
        float barraDY = barraM.rect.y - barraPrevY;

        Rectangle ground = (Rectangle){0, mapTex.height, mapTex.width, 200};
        UpdatePlayer(&earthboy, ground, KEY_A, KEY_D, KEY_W);
        UpdatePlayer(&fireboy,  ground, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&watergirl,ground, KEY_J, KEY_L, KEY_I);

        Player* P[3] = {&earthboy,&fireboy,&watergirl};
        struct { Player* pl; int keyLeft; int keyRight; } controls[3] = {
            { &earthboy, KEY_A, KEY_D },
            { &fireboy, KEY_LEFT, KEY_RIGHT },
            { &watergirl, KEY_J, KEY_L }
        };

        for (int b=0;b<coopBoxCount;b++) {
            CoOpBox* box = &coopBoxes[b];
            if (box->rect.width <= 0 || box->rect.height <= 0) continue;
            int pushLeft = 0, pushRight = 0;
            for (int i=0;i<3;i++) {
                Player* pl = controls[i].pl;
                if (PlayerPushingBox(pl, box->rect, true, controls[i].keyRight, 6.0f)) pushRight++;
                else if (PlayerPushingBox(pl, box->rect, false, controls[i].keyLeft, 6.0f)) pushLeft++;
            }
            if (pushRight >= 2 && pushRight > pushLeft) box->velX += 1.2f;
            else if (pushLeft >= 2 && pushLeft > pushRight) box->velX -= 1.2f;
            box->velX *= 0.88f;
            if (fabsf(box->velX) < 0.05f) box->velX = 0.0f;
            if (box->velX > 4.5f) box->velX = 4.5f;
            if (box->velX < -4.5f) box->velX = -4.5f;
            float prevX = box->rect.x;
            box->rect.x += box->velX;
            if (box->rect.x < 0) { box->rect.x = 0; box->velX = 0; }
            float maxX = mapTex.width - box->rect.width;
            if (box->rect.x > maxX) { box->rect.x = maxX; box->velX = 0; }
            ResolveCoOpBoxVsWorld(box, col, nCol);
            float deltaX = box->rect.x - prevX;
            ResolveCoOpBoxVsRect(box, elev.rect);
            ResolveCoOpBoxVsRect(box, elev2.rect);
            ResolveCoOpBoxVsRect(box, barra3.rect);
            ResolveCoOpBoxVsRect(box, barraM.rect);
            HandleCoOpBoxOnPlatform(box, elev.rect, elevDY);
            HandleCoOpBoxOnPlatform(box, elev2.rect, elev2DY);
            HandleCoOpBoxOnPlatform(box, barra3.rect, barra3DY);
            HandleCoOpBoxOnPlatform(box, barraM.rect, barraDY);
            for (int i=0;i<3;i++) ResolvePlayerVsCoOpBox(controls[i].pl, box, deltaX);
            if (BoxTouchesAnyLake(box, segs, segCount)) {
                box->rect.width = 0;
                box->rect.height = 0;
                box->velX = 0;
                continue;
            }
        }
        for (int p=0;p<3;p++) {
            Player* pl=P[p];
            for (int i=0;i<nCol;i++) ResolvePlayerVsRect(pl, col[i].rect);
            if (elev2.rect.width > 0 && elev2.rect.height > 0) ResolvePlayerVsRect(pl, elev2.rect);
            if (barra3.rect.width > 0 && barra3.rect.height > 0) ResolvePlayerVsRect(pl, barra3.rect);
            if (barraM.rect.width > 0 && barraM.rect.height > 0) ResolvePlayerVsRect(pl, barraM.rect);
        }

        for (int p=0;p<3;p++) {
            Player* pl=P[p];
            if (elev.rect.width>0) HandlePlatformTop(pl, elev.rect, elevDY);
            if (elev2.rect.width>0) HandlePlatformTop(pl, elev2.rect, elev2DY);
            if (barra3.rect.width>0) HandlePlatformTop(pl, barra3.rect, barra3DY);
            if (barraM.rect.width>0) HandlePlatformTop(pl, barraM.rect, barraDY);
        }

        // Ventiladores: 1 e 3 sempre aplicam, 2 só se somente b3
        for (int i=0;i<fans1Count;i++) { FanApply(&fans1[i], &earthboy); FanApply(&fans1[i], &fireboy); FanApply(&fans1[i], &watergirl); }
        for (int i=0;i<fans3Count;i++) { FanApply(&fans3[i], &earthboy); FanApply(&fans3[i], &fireboy); FanApply(&fans3[i], &watergirl); }
        bool fan2Active = haveVent2 && p3 && !p1 && !p2;
        if (fan2Active && fanOnCount > 0) {
            fan2AnimTimer += frameDt;
            if (fan2AnimTimer >= FAN_FRAME_TIME) {
                fan2AnimTimer -= FAN_FRAME_TIME;
                fan2AnimFrame = (fan2AnimFrame + 1) % fanOnCount;
            }
        } else {
            fan2AnimTimer = 0.0f;
            fan2AnimFrame = 0;
        }
        if (fan2Active) { FanApply(&vent2, &earthboy); FanApply(&vent2, &fireboy); FanApply(&vent2, &watergirl); }

        // Respawn por lagos errados: faz por segmentos
        for (int p=0;p<3;p++) {
            Player* pl=P[p]; LakeType elem=(p==0)?LAKE_EARTH:((p==1)?LAKE_FIRE:LAKE_WATER);
            for (int i=0;i<segCount;i++) {
                Lake l; l.rect=segs[i].rect; l.type=segs[i].type; l.color=(Color){0};
                if (LakeHandlePlayer(&l, pl, elem)) { if (p==0){pl->rect.x=spawnE.x;pl->rect.y=spawnE.y;} else if (p==1){pl->rect.x=spawnF.x;pl->rect.y=spawnF.y;} else {pl->rect.x=spawnW.x;pl->rect.y=spawnW.y;} pl->velocity=(Vector2){0,0}; pl->isJumping=false; break; }
            }
        }

        // Desenho
        BeginDrawing(); ClearBackground(BLACK); BeginMode2D(cam);
        DrawTexture(mapTex, 0, 0, WHITE);
        for (int i = 0; i < buttonCount; ++i) {
            ButtonDraw(&buttons[i].button);
        }

        // Quem está dentro do próprio lago (para desenhar por trás)
        bool insideOwn[3] = { false, false, false };
        for (int ip = 0; ip < 3; ++ip) {
            Player* pl = P[ip]; LakeType elem = (ip==0)?LAKE_EARTH:((ip==1)?LAKE_FIRE:LAKE_WATER);
            for (int i = 0; i < segCount; ++i) {
                const LakeSegment* seg = &segs[i]; if (seg->type != elem) continue; if (!CheckCollisionRecs(pl->rect, seg->rect)) continue;
                Rectangle ov = GetCollisionRec(pl->rect, seg->rect); float pBottom = pl->rect.y + pl->rect.height;
                if (pBottom > seg->rect.y + 2 && ov.height >= 4) { insideOwn[ip] = true; break; }
            }
        }

        // Avança animações (8 FPS aprox)
        float dt = frameDt;
        LakeAnimFrames* sets[4] = { &animAgua, &animFogo, &animTerra, &animAcido };
        for (int s=0;s<4;s++) { sets[s]->timer += dt; if (sets[s]->timer >= 0.12f) { sets[s]->timer = 0.0f; sets[s]->frame = (sets[s]->frame + 1) % 64; } }

        // Desenha ventiladores (antes dos jogadores para não ocultá-los)
        for (int i=0;i<fans1Count;i++) DrawFanSprite(fan1Draw[i], true, fanOnFrames, fanOnCount, fanOffTex, fanAnimFrame);
        for (int i=0;i<fans3Count;i++) DrawFanSprite(fan3Draw[i], true, fanOnFrames, fanOnCount, fanOffTex, fanAnimFrame);
        if (haveVent2) DrawFanSprite(vent2DrawRect, fan2Active, fanOnFrames, fanOnCount, fanOffTex, fan2AnimFrame);

        // 1) desenha jogadores que estão dentro do lago correto primeiro
        if (insideOwn[0]) DrawPlayer(earthboy); if (insideOwn[1]) DrawPlayer(fireboy); if (insideOwn[2]) DrawPlayer(watergirl);
        // 2) desenha lagos animados por cima
        for (int i=0;i<segCount;i++) {
            const LakeSegment* seg=&segs[i]; const LakeAnimFrames* anim=NULL; Texture2D frame=(Texture2D){0}; int idx=0; bool has=false;
            if (seg->type==LAKE_WATER) anim=&animAgua; else if (seg->type==LAKE_FIRE) anim=&animFogo; else if (seg->type==LAKE_EARTH) anim=&animTerra; else if (seg->type==LAKE_POISON) anim=&animAcido;
            if (anim) {
                if (seg->part==PART_LEFT   && anim->leftCount  >0) { idx = (anim->frame % anim->leftCount);   frame = anim->left[idx];   has=true; }
                if (seg->part==PART_MIDDLE && anim->middleCount>0) { idx = (anim->frame % anim->middleCount); frame = anim->middle[idx]; has=true; }
                if (seg->part==PART_RIGHT  && anim->rightCount >0) { idx = (anim->frame % anim->rightCount);  frame = anim->right[idx];  has=true; }
            }
            if (has && seg->part==PART_MIDDLE) {
                float tile = seg->rect.height; int tiles = (int)floorf(seg->rect.width / tile); float x = seg->rect.x;
                for (int t=0;t<tiles;++t) { Rectangle dst={x,seg->rect.y,tile,seg->rect.height}; DrawTexturePro(frame,(Rectangle){0,0,(float)frame.width,(float)frame.height},dst,(Vector2){0,0},0.0f,WHITE); x+=tile; }
                float rest = seg->rect.width - tiles*tile; if (rest>0.1f) { Rectangle dst={x,seg->rect.y,rest,seg->rect.height}; DrawTexturePro(frame,(Rectangle){0,0,(float)frame.width,(float)frame.height},dst,(Vector2){0,0},0.0f,WHITE); }
            } else if (has) {
                DrawTexturePro(frame,(Rectangle){0,0,(float)frame.width,(float)frame.height},seg->rect,(Vector2){0,0},0.0f,WHITE);
            } else {
                // fallback sólido se não encontrar frames
                Color c = (seg->type==LAKE_POISON)? (Color){60,180,60,230} : (Color){90,90,90,200}; DrawRectangleRec(seg->rect, c);
            }
        }
        // 3) plataformas
        if (elev.rect.width>0 && barraElevTex.id) DrawTexturePro(barraElevTex,(Rectangle){0,0,(float)barraElevTex.width,(float)barraElevTex.height},elev.rect,(Vector2){0,0},0.0f,WHITE);
        if (elev2.rect.width>0 && barraElevTex.id) DrawTexturePro(barraElevTex,(Rectangle){0,0,(float)barraElevTex.width,(float)barraElevTex.height},elev2.rect,(Vector2){0,0},0.0f,WHITE);
        if (barraM.rect.width>0 && barraMovTex.id) DrawTexturePro(barraMovTex,(Rectangle){0,0,(float)barraMovTex.width,(float)barraMovTex.height},barraM.rect,(Vector2){0,0},0.0f,WHITE);
        if (barra3.rect.width>0 && barraMovTex.id) DrawTexturePro(barraMovTex,(Rectangle){0,0,(float)barraMovTex.width,(float)barraMovTex.height},barra3.rect,(Vector2){0,0},0.0f,WHITE);
        for (int b=0; b<coopBoxCount; ++b) {
        if (coopBoxes[b].rect.width <= 0 || coopBoxes[b].rect.height <= 0) continue;
        if (coopBoxTex.id) DrawTexturePro(coopBoxTex,(Rectangle){0,0,(float)coopBoxTex.width,(float)coopBoxTex.height},coopBoxes[b].rect,(Vector2){0,0},0.0f,WHITE);
        else DrawRectangleRec(coopBoxes[b].rect, DARKBROWN);
        }
        // 4) jogadores restantes
        if (!insideOwn[0]) DrawPlayer(earthboy); if (!insideOwn[1]) DrawPlayer(fireboy); if (!insideOwn[2]) DrawPlayer(watergirl);
        // 5) chegada
        GoalDraw(&goal);
        if (earthAtDoor && fireAtDoor && waterAtDoor) { completed = true; EndMode2D(); EndDrawing(); break; }
        // 6) debug
        if (debug) {
            for (int i=0;i<nCol;i++) DrawRectangleLinesEx(col[i].rect,1,Fade(GREEN,0.5f));
            DrawRectangleLinesEx(elev.area,1,Fade(YELLOW,0.5f));
            if (elev2.area.width>0 && elev2.area.height>0) DrawRectangleLinesEx(elev2.area,1,Fade(YELLOW,0.5f));
            if (barra3.area.width>0 && barra3.area.height>0) DrawRectangleLinesEx(barra3.area,1,Fade(GOLD,0.5f));
            DrawRectangleLinesEx(barraM.area,1,Fade(ORANGE,0.5f));
            if (doorTerra.width>0 && doorTerra.height>0) DrawRectangleLinesEx(doorTerra,1,Fade(BROWN,0.6f));
            if (doorFogo.width>0 && doorFogo.height>0) DrawRectangleLinesEx(doorFogo,1,Fade(RED,0.6f));
            if (doorAgua.width>0 && doorAgua.height>0) DrawRectangleLinesEx(doorAgua,1,Fade(BLUE,0.6f));
            for (int i=0;i<fans1Count;i++) DrawRectangleLinesEx(fans1[i].rect,1,Fade(BLUE,0.5f));
            for (int i=0;i<fans3Count;i++) DrawRectangleLinesEx(fans3[i].rect,1,Fade(PURPLE,0.5f));
            if (haveVent2) DrawRectangleLinesEx(vent2.rect,1,Fade(SKYBLUE,0.5f));
            DrawText(TextFormat("b1=%d b2=%d b3=%d", p1,p2,p3), 10,10,20,RAYWHITE);
        }
        if (GoalReached(&goal, &earthboy, &fireboy, &watergirl)) { completed=true; EndMode2D(); EndDrawing(); break; }
        EndMode2D(); EndDrawing();
    }

    UnloadTexture(mapTex); if (barraElevTex.id) UnloadTexture(barraElevTex);
    if (!barraMovShared && barraMovTex.id) UnloadTexture(barraMovTex);
    if (!coopBoxShared && coopBoxTex.id) UnloadTexture(coopBoxTex);
    if (fanOffTex.id) UnloadTexture(fanOffTex);
    for (int i=0;i<fanOnCount;i++) if (fanOnFrames[i].id) UnloadTexture(fanOnFrames[i]);
    if (buttonSprites.blue.id != 0) UnloadTexture(buttonSprites.blue);
    if (buttonSprites.red.id != 0) UnloadTexture(buttonSprites.red);
    if (buttonSprites.white.id != 0) UnloadTexture(buttonSprites.white);
    if (buttonSprites.brown.id != 0) UnloadTexture(buttonSprites.brown);
    // unload lake frames
    UnloadLakeSet(&animAgua); UnloadLakeSet(&animFogo); UnloadLakeSet(&animTerra); UnloadLakeSet(&animAcido);
    UnloadPlayer(&earthboy); UnloadPlayer(&fireboy); UnloadPlayer(&watergirl);
    if (completed) Ranking_Add(2, Game_GetPlayerName(), elapsed);
    return completed;
}
