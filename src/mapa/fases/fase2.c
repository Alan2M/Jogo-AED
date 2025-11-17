#include "raylib.h"
#include "fases.h"
#include "../../player/player.h"
#include "../../objects/lake.h"
#include "../../objects/button.h"
#include "../../objects/fan.h"
#include "../../interface/pause.h"
#include "../../game/game.h"
#include "../../ranking/ranking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <ctype.h>

#define MAX_COLISOES     1024
#define MAX_LAKE_SEGS    256
#define MAX_FANS         16
#define MAX_BUTTONS      8
#define BUTTON_NAME_LEN  64
#define MAX_PLATFORMS    4
#define STEP_HEIGHT      14.0f

typedef struct { Rectangle rect; } Colisao;

typedef enum { PART_LEFT = 0, PART_MIDDLE, PART_RIGHT } LakePart;

typedef struct {
    Rectangle rect;
    LakeType type;
    LakePart part;
} LakeSegment;

typedef struct LakeAnimFrames {
    Texture2D left[32];   int leftCount;
    Texture2D middle[32]; int middleCount;
    Texture2D right[32];  int rightCount;
    float timer; int frame;
} LakeAnimFrames;

typedef struct Platform {
    Rectangle rect;
    Rectangle area;
    float startY;
    float speed;
} Platform;

typedef struct ButtonSpriteSet {
    Texture2D blue;
    Texture2D red;
    Texture2D white;
    Texture2D brown;
} ButtonSpriteSet;

static int ParseRectsFromGroup(const char* tmxPath, const char* groupName, Rectangle* out, int cap) {
    int count = 0;
    char* xml = LoadFileText(tmxPath);
    if (!xml) return 0;
    const char* search = xml;
    while ((search = strstr(search, "<objectgroup")) != NULL) {
        const char* tagClose = strchr(search, '>');
        if (!tagClose) break;
        bool match = false;
        size_t len = (size_t)(tagClose - search);
        char* header = (char*)malloc(len + 1);
        if (!header) { UnloadFileText(xml); return count; }
        memcpy(header, search, len);
        header[len] = '\0';
        char findName[128];
        snprintf(findName, sizeof(findName), "name=\"%s\"", groupName);
        if (strstr(header, findName)) match = true;
        free(header);

        const char* groupEnd = strstr(tagClose + 1, "</objectgroup>");
        if (!groupEnd) break;

        if (match) {
            const char* p = tagClose + 1;
            while (p < groupEnd) {
                const char* obj = strstr(p, "<object ");
                if (!obj || obj >= groupEnd) break;
                float x=0,y=0,w=0,h=0;
                sscanf(obj, "<object id=%*[^x]x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\"", &x,&y,&w,&h);
                if (w>0 && h>0 && count < cap) out[count++] = (Rectangle){x,y,w,h};
                p = obj + 8;
            }
        }
        search = groupEnd ? (groupEnd + 14) : (search + 11);
    }
    UnloadFileText(xml);
    return count;
}

static void AddCollisionGroup(const char* tmxPath, const char* name, Colisao* col, int* count, int cap) {
    Rectangle rects[64];
    int n = ParseRectsFromGroup(tmxPath, name, rects, 64);
    for (int i = 0; i < n && *count < cap; ++i) col[(*count)++].rect = rects[i];
}

static void AddLakeSegments(const char* tmxPath, const char* name, LakeType type, LakePart part,
                            LakeSegment* segs, int* count, int cap) {
    Rectangle rects[64];
    int n = ParseRectsFromGroup(tmxPath, name, rects, 64);
    for (int i=0;i<n && *count < cap;i++) {
        segs[*count].rect = rects[i];
        segs[*count].type = type;
        segs[*count].part = part;
        (*count)++;
    }
}

static void ToLowerCopy(const char* src, char* dst, size_t dstSize) {
    if (dstSize == 0) return;
    size_t i = 0;
    for (; src[i] && i + 1 < dstSize; ++i) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
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
        if (!strstr(lower, "botao")) { search = tagClose + 1; continue; }
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

static bool AnyButtonPressedWithToken(const bool* states, char namesLower[][BUTTON_NAME_LEN], int count, const char* tokenLower) {
    if (!tokenLower || !tokenLower[0]) return false;
    for (int i = 0; i < count; ++i) {
        if (states[i] && strstr(namesLower[i], tokenLower)) return true;
    }
    return false;
}

static bool PlayerInsideOwnLake(const Player* pl, LakeType type, const LakeSegment* segs, int segCount) {
    for (int i = 0; i < segCount; ++i) {
        if (segs[i].type != type) continue;
        if (CheckCollisionRecs(pl->rect, segs[i].rect)) return true;
    }
    return false;
}

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

static void PlatformInit(Platform* p, Rectangle rect, Rectangle area, float speed) {
    p->rect = rect;
    p->area = area;
    p->startY = rect.y;
    p->speed = speed;
}

static float moveTowards(float a, float b, float maxStep) {
    if (a < b) { a += maxStep; if (a > b) a = b; }
    else if (a > b) { a -= maxStep; if (a < b) a = b; }
    return a;
}

static void DrawPlatformTexture(Texture2D tex, Rectangle rect) {
    if (tex.id != 0)
        DrawTexturePro(tex, (Rectangle){0,0,(float)tex.width,(float)tex.height}, rect, (Vector2){0,0}, 0.0f, WHITE);
    else
        DrawRectangleRec(rect, GRAY);
}

static Texture2D LoadTextureIfExists(const char* path) {
    if (!FileExists(path)) return (Texture2D){0};
    return LoadTexture(path);
}

static int FindCollisionIndex(Rectangle rect, const Colisao* col, int colCount) {
    for (int i = 0; i < colCount; ++i) {
        Rectangle r = col[i].rect;
        if (fabsf(r.x - rect.x) > 0.5f) continue;
        if (fabsf(r.y - rect.y) > 0.5f) continue;
        if (fabsf(r.width - rect.width) > 0.5f) continue;
        if (fabsf(r.height - rect.height) > 0.5f) continue;
        return i;
    }
    return -1;
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

static bool CheckDoor(const Rectangle* door, const Player* p) {
    if (door->width <= 0 || door->height <= 0) return false;
    return CheckCollisionRecs(*door, p->rect);
}

bool Fase2(void) {
    const char* tmxPath = "assets/maps/fase2/fase2.tmx";
    Texture2D mapTexture = LoadTexture("assets/maps/fase2/fase2.png");


    Colisao colisoes[MAX_COLISOES];
    int totalColisoes = 0;
    AddCollisionGroup(tmxPath, "colisao", colisoes, &totalColisoes, MAX_COLISOES);
    AddCollisionGroup(tmxPath, "barra1", colisoes, &totalColisoes, MAX_COLISOES);
    AddCollisionGroup(tmxPath, "barra2", colisoes, &totalColisoes, MAX_COLISOES);

    LakeSegment lakeSegs[MAX_LAKE_SEGS]; int lakeSegCount = 0;
    AddLakeSegments(tmxPath, "aguaesquerda", LAKE_WATER, PART_LEFT,   lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "aguameio",     LAKE_WATER, PART_MIDDLE, lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "aguadireita",  LAKE_WATER, PART_RIGHT,  lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "fogoesquerda", LAKE_FIRE,  PART_LEFT,   lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "fogomeio",     LAKE_FIRE,  PART_MIDDLE, lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "fogodireita",  LAKE_FIRE,  PART_RIGHT,  lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "terraesquerda",LAKE_EARTH, PART_LEFT,   lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "terrameio",    LAKE_EARTH, PART_MIDDLE, lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "terradireita", LAKE_EARTH, PART_RIGHT,  lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "venenoesquerda", LAKE_POISON, PART_LEFT,   lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "venenomeio",     LAKE_POISON, PART_MIDDLE, lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);
    AddLakeSegments(tmxPath, "venenodireita",  LAKE_POISON, PART_RIGHT,  lakeSegs, &lakeSegCount, MAX_LAKE_SEGS);

    LakeAnimFrames animAgua={0}, animFogo={0}, animTerra={0}, animAcido={0};
    LoadLakeSet_Agua(&animAgua);
    LoadLakeSet_Fogo(&animFogo);
    LoadLakeSet_Terra(&animTerra);
    LoadLakeSet_Acido(&animAcido);

    Rectangle spawn[4];
    Vector2 spawnAgua = { 200, 800 };
    Vector2 spawnFogo = { 250, 800 };
    Vector2 spawnTerra= { 300, 800 };
    if (ParseRectsFromGroup(tmxPath, "spawnAgua", spawn, 4) > 0) spawnAgua = (Vector2){ spawn[0].x, spawn[0].y };
    if (ParseRectsFromGroup(tmxPath, "spawnFogo", spawn, 4) > 0) spawnFogo = (Vector2){ spawn[0].x, spawn[0].y };
    if (ParseRectsFromGroup(tmxPath, "spawnTerra",spawn, 4) > 0) spawnTerra= (Vector2){ spawn[0].x, spawn[0].y };

    Rectangle doorAgua={0}, doorFogo={0}, doorTerra={0};
    ParseRectsFromGroup(tmxPath, "PortaAgua", &doorAgua, 1);
    ParseRectsFromGroup(tmxPath, "PortaFogo", &doorFogo, 1);
    ParseRectsFromGroup(tmxPath, "PortaTerra",&doorTerra,1);

    Button buttons[MAX_BUTTONS]; int buttonCount = 0;
    char buttonNamesLower[MAX_BUTTONS][BUTTON_NAME_LEN] = {{0}};
    ButtonSpriteSet buttonSprites = {0};
    buttonSprites.blue  = LoadTextureIfExists("assets/map/buttons/pixil-layer-bluebutton.png");
    buttonSprites.red   = LoadTextureIfExists("assets/map/buttons/pixil-layer-redbutton.png");
    buttonSprites.white = LoadTextureIfExists("assets/map/buttons/pixil-layer-whitebutton.png");
    buttonSprites.brown = LoadTextureIfExists("assets/map/buttons/pixil-layer-brownbutton.png");

    Rectangle btnRect[8];
    char buttonGroupNames[MAX_BUTTONS][BUTTON_NAME_LEN] = {{0}};
    int buttonGroupCount = CollectButtonGroupNames(tmxPath, buttonGroupNames, MAX_BUTTONS);
    for (int g = 0; g < buttonGroupCount && buttonCount < MAX_BUTTONS; ++g) {
        int rectsFound = ParseRectsFromGroup(tmxPath, buttonGroupNames[g], btnRect, (int)(sizeof(btnRect)/sizeof(btnRect[0])));
        if (rectsFound <= 0) continue;
        char lowerName[BUTTON_NAME_LEN];
        ToLowerCopy(buttonGroupNames[g], lowerName, sizeof(lowerName));
        Color colorUp, colorDown;
        DetermineButtonColors(lowerName, &colorUp, &colorDown);
        const Texture2D* sprite = PickButtonSprite(lowerName, &buttonSprites);
        for (int r = 0; r < rectsFound && buttonCount < MAX_BUTTONS; ++r) {
            Rectangle rect = btnRect[r];
            ButtonInit(&buttons[buttonCount], rect.x, rect.y, rect.width, rect.height, colorUp, colorDown);
            if (sprite) ButtonSetSprites(&buttons[buttonCount], sprite, NULL);
            strncpy(buttonNamesLower[buttonCount], lowerName, BUTTON_NAME_LEN - 1);
            buttonNamesLower[buttonCount][BUTTON_NAME_LEN - 1] = '\0';
            buttonCount++;
        }
    }

    if (buttonCount == 0) {
        const char* fallbackGroups[] = {
            "botao1ventilador1",
            "botao2ventilador1",
            "botao3ventilador2",
            "botao4barra1",
            "botao5barra2"
        };
        const int fallbackTotal = (int)(sizeof(fallbackGroups)/sizeof(fallbackGroups[0]));
        for (int i = 0; i < fallbackTotal && buttonCount < MAX_BUTTONS; ++i) {
            int rectsFound = ParseRectsFromGroup(tmxPath, fallbackGroups[i], btnRect, (int)(sizeof(btnRect)/sizeof(btnRect[0])));
            if (rectsFound <= 0) continue;
            char lowerName[BUTTON_NAME_LEN];
            ToLowerCopy(fallbackGroups[i], lowerName, sizeof(lowerName));
            Color colorUp, colorDown;
            DetermineButtonColors(lowerName, &colorUp, &colorDown);
            const Texture2D* sprite = PickButtonSprite(lowerName, &buttonSprites);
            Rectangle rect = btnRect[0];
            ButtonInit(&buttons[buttonCount], rect.x, rect.y, rect.width, rect.height, colorUp, colorDown);
            if (sprite) ButtonSetSprites(&buttons[buttonCount], sprite, NULL);
            strncpy(buttonNamesLower[buttonCount], lowerName, BUTTON_NAME_LEN - 1);
            buttonNamesLower[buttonCount][BUTTON_NAME_LEN - 1] = '\0';
            buttonCount++;
        }
    }

    Fan fans1[MAX_FANS]; int fans1Count = 0;
    Rectangle fanRects[8];
    int nFan1 = ParseRectsFromGroup(tmxPath, "ventilador1", fanRects, 8);
    for (int i=0;i<nFan1 && fans1Count<MAX_FANS;i++)
        FanInit(&fans1[fans1Count++], fanRects[i].x, fanRects[i].y, fanRects[i].width, fanRects[i].height, 0.8f);
    Fan fans2[MAX_FANS]; int fans2Count = 0;
    int nFan2 = ParseRectsFromGroup(tmxPath, "ventilador2", fanRects, 8);
    for (int i=0;i<nFan2 && fans2Count<MAX_FANS;i++)
        FanInit(&fans2[fans2Count++], fanRects[i].x, fanRects[i].y, fanRects[i].width, fanRects[i].height, 0.9f);

    Rectangle fanSpriteRects[16]; bool fanSpriteUsed[16]={0};
    int fanSpriteCount = ParseRectsFromGroup(tmxPath, "AnimarVentilador", fanSpriteRects, 16);
    Rectangle fan1Draw[MAX_FANS];
    Rectangle fan2Draw[MAX_FANS];
    for (int i=0;i<fans1Count;i++) fan1Draw[i] = AcquireSpriteForRect(fans1[i].rect, fanSpriteRects, fanSpriteUsed, fanSpriteCount);
    for (int i=0;i<fans2Count;i++) fan2Draw[i] = AcquireSpriteForRect(fans2[i].rect, fanSpriteRects, fanSpriteUsed, fanSpriteCount);

    Texture2D barraFallbackTex = LoadTextureIfExists("assets/map/barras/barragorda.png");
    if (barraFallbackTex.id == 0) barraFallbackTex = LoadTextureIfExists("assets/map/barras/branca.png");
    Texture2D barra1Tex = LoadTextureIfExists("assets/map/barras/Barra1_Fase2.png");
    Texture2D barra2Tex = LoadTextureIfExists("assets/map/barras/Barra2_Fase2.png");

    Platform platforms[MAX_PLATFORMS]; int platformCount = 0;
    int platformCollisionIndex[MAX_PLATFORMS];
    for (int i=0;i<MAX_PLATFORMS;i++) platformCollisionIndex[i] = -1;
    Texture2D* platformTexRefs[MAX_PLATFORMS] = {0};
    char platformControlTokens[MAX_PLATFORMS][32] = {{0}};
    bool platformMoveDownActive[MAX_PLATFORMS] = { false };
    Rectangle platR[4];
    Rectangle rangeRect[4];
    if (ParseRectsFromGroup(tmxPath, "barra1", platR, 4) > 0 && platformCount < MAX_PLATFORMS) {
        Rectangle area = platR[0];
        if (ParseRectsFromGroup(tmxPath, "barra1range", rangeRect, 4) > 0) area = rangeRect[0];
        PlatformInit(&platforms[platformCount], platR[0], area, 2.0f);
        platforms[platformCount].rect.y = area.y;
        platforms[platformCount].startY = area.y;
        strncpy(platformControlTokens[platformCount], "botao4barra1_branco", sizeof(platformControlTokens[platformCount]) - 1);
        platformTexRefs[platformCount] = &barra1Tex;
        platformMoveDownActive[platformCount] = true;
        platformCollisionIndex[platformCount] = FindCollisionIndex(platforms[platformCount].rect, colisoes, totalColisoes);
        if (platformCollisionIndex[platformCount] >= 0) colisoes[platformCollisionIndex[platformCount]].rect = platforms[platformCount].rect;
        platformCount++;
    }
    if (ParseRectsFromGroup(tmxPath, "barra2", platR, 4) > 0 && platformCount < MAX_PLATFORMS) {
        Rectangle area = platR[0];
        if (ParseRectsFromGroup(tmxPath, "barra2range", rangeRect, 4) > 0) area = rangeRect[0];
        PlatformInit(&platforms[platformCount], platR[0], area, 2.0f);
        platforms[platformCount].rect.y = area.y;
        platforms[platformCount].startY = area.y;
        strncpy(platformControlTokens[platformCount], "botao5barra2_vermelho", sizeof(platformControlTokens[platformCount]) - 1);
        platformTexRefs[platformCount] = &barra2Tex;
        platformMoveDownActive[platformCount] = true;
        platformCollisionIndex[platformCount] = FindCollisionIndex(platforms[platformCount].rect, colisoes, totalColisoes);
        if (platformCollisionIndex[platformCount] >= 0) colisoes[platformCollisionIndex[platformCount]].rect = platforms[platformCount].rect;
        platformCount++;
    }

    Texture2D fanOffTex = LoadTextureIfExists("assets/map/vento/desligado.png");
    Texture2D fanOnFrames[8]; int fanOnCount = 0;
    const char* fanPaths[] = {
        "assets/map/vento/ligado1.png",
        "assets/map/vento/ligado2.png",
        "assets/map/vento/ligado3.png",
        "assets/map/vento/ligado4.png"
    };
    for (int i=0;i<4;i++) {
        Texture2D tex = LoadTextureIfExists(fanPaths[i]);
        if (tex.id != 0) fanOnFrames[fanOnCount++] = tex;
    }
    float fanAnimTimer = 0.0f; int fanAnimFrame = 0;
    float fan2AnimTimer = 0.0f; int fan2AnimFrame = 0;
    const float FAN_FRAME_TIME = 0.12f;

    Rectangle doorStatus[3] = { doorTerra, doorFogo, doorAgua };
    Player watergirl, fireboy, earthboy;
    InitWatergirl(&watergirl);
    InitFireboy(&fireboy);
    InitEarthboy(&earthboy);
    watergirl.rect = (Rectangle){ spawnAgua.x, spawnAgua.y, 45, 50 };
    fireboy.rect   = (Rectangle){ spawnFogo.x, spawnFogo.y, 45, 50 };
    earthboy.rect  = (Rectangle){ spawnTerra.x, spawnTerra.y, 45, 50 };

    Camera2D camera = (Camera2D){
        .target = { mapTexture.width/2.0f, mapTexture.height/2.0f },
        .offset = { GetScreenWidth()/2.0f, GetScreenHeight()/2.0f },
        .rotation = 0.0f,
        .zoom = 1.0f
    };

    bool reachedAgua=false, reachedFogo=false, reachedTerra=false;
    bool debug=false, completed=false;
    float elapsed=0.0f;
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        elapsed += dt;
        if (IsKeyPressed(KEY_TAB)) debug = !debug;

        if (IsKeyPressed(KEY_ESCAPE)) {
            PauseResult pr = ShowPauseMenu();
            if (pr == PAUSE_TO_MAP) { completed = false; break; }
            if (pr == PAUSE_TO_MENU) { Game_SetReturnToMenu(true); completed = false; break; }
        }

        UpdatePlayer(&watergirl, (Rectangle){0,mapTexture.height,mapTexture.width,200}, KEY_J, KEY_L, KEY_I);
        UpdatePlayer(&fireboy,   (Rectangle){0,mapTexture.height,mapTexture.width,200}, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&earthboy,  (Rectangle){0,mapTexture.height,mapTexture.width,200}, KEY_A, KEY_D, KEY_W);

        Player* players[3] = { &earthboy, &fireboy, &watergirl };
        for (int p = 0; p < 3; ++p) {
            Player* pl = players[p];
            for (int i = 0; i < totalColisoes; ++i) {
                Rectangle bloco = colisoes[i].rect;
                if (!CheckCollisionRecs(pl->rect, bloco)) continue;
                float dx = (pl->rect.x + pl->rect.width/2) - (bloco.x + bloco.width/2);
                float dy = (pl->rect.y + pl->rect.height/2) - (bloco.y + bloco.height/2);
                float overlapX = (pl->rect.width/2 + bloco.width/2) - fabsf(dx);
                float overlapY = (pl->rect.height/2 + bloco.height/2) - fabsf(dy);
                if (overlapX < overlapY) {
                    Rectangle teste = pl->rect; teste.y -= STEP_HEIGHT;
                    if (!(dy > 0 && pl->velocity.y > 0) && !CheckCollisionRecs(teste, bloco)) {
                        pl->rect.y -= STEP_HEIGHT;
                        continue;
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
        }

        bool buttonStates[MAX_BUTTONS] = { false };
        for (int i=0;i<buttonCount;i++) {
            buttonStates[i] = ButtonUpdate(&buttons[i], &earthboy, &fireboy, &watergirl);
        }

        for (int i=0;i<platformCount;i++) {
            Platform* plat = &platforms[i];
            float targetDown = plat->area.y + plat->area.height - plat->rect.height;
            float targetUp = plat->area.y;
            bool active = false;
            if (platformControlTokens[i][0]) {
                active = AnyButtonPressedWithToken(buttonStates, buttonNamesLower, buttonCount, platformControlTokens[i]);
            }
            float inactiveTarget = platformMoveDownActive[i] ? targetUp : targetDown;
            float activeTarget = platformMoveDownActive[i] ? targetDown : targetUp;
            float target = active ? activeTarget : inactiveTarget;
            plat->rect.y = moveTowards(plat->rect.y, target, plat->speed);
            float minY = plat->area.y;
            float maxY = plat->area.y + plat->area.height - plat->rect.height;
            if (plat->rect.y < minY) plat->rect.y = minY;
            if (plat->rect.y > maxY) plat->rect.y = maxY;
            if (platformCollisionIndex[i] >= 0 && platformCollisionIndex[i] < totalColisoes) {
                colisoes[platformCollisionIndex[i]].rect = plat->rect;
            }
        }

        bool fan1Active = AnyButtonPressedWithToken(buttonStates, buttonNamesLower, buttonCount, "ventilador1");
        bool fan2Active = AnyButtonPressedWithToken(buttonStates, buttonNamesLower, buttonCount, "botao3ventilador2_marrom");

        if (fanOnCount > 0) {
            fanAnimTimer += dt;
            if (fanAnimTimer >= FAN_FRAME_TIME) {
                fanAnimTimer -= FAN_FRAME_TIME;
                fanAnimFrame = (fanAnimFrame + 1) % fanOnCount;
            }
        }
        if (fan2Active && fanOnCount > 0) {
            fan2AnimTimer += dt;
            if (fan2AnimTimer >= FAN_FRAME_TIME) {
                fan2AnimTimer -= FAN_FRAME_TIME;
                fan2AnimFrame = (fan2AnimFrame + 1) % fanOnCount;
            }
        } else {
            fan2AnimTimer = 0.0f;
            fan2AnimFrame = 0;
        }

        for (int i=0;i<fans1Count;i++) {
            if (fan1Active) {
                FanApply(&fans1[i], &earthboy);
                FanApply(&fans1[i], &fireboy);
                FanApply(&fans1[i], &watergirl);
            }
        }
        for (int i=0;i<fans2Count;i++) {
            if (fan2Active) {
                FanApply(&fans2[i], &earthboy);
                FanApply(&fans2[i], &fireboy);
                FanApply(&fans2[i], &watergirl);
            }
        }

        for (int p = 0; p < 3; ++p) {
            Player* pl = players[p];
            LakeType elem = (p == 0) ? LAKE_EARTH : (p == 1 ? LAKE_FIRE : LAKE_WATER);
            for (int i = 0; i < lakeSegCount; ++i) {
                Lake temp; temp.rect = lakeSegs[i].rect; temp.type = lakeSegs[i].type; temp.color = WHITE;
                if (LakeHandlePlayer(&temp, pl, elem)) {
                    if (p == 0) { pl->rect.x = spawnTerra.x; pl->rect.y = spawnTerra.y; }
                    else if (p == 1) { pl->rect.x = spawnFogo.x; pl->rect.y = spawnFogo.y; }
                    else { pl->rect.x = spawnAgua.x; pl->rect.y = spawnAgua.y; }
                    pl->velocity = (Vector2){0,0}; pl->isJumping = false;
                    break;
                }
            }
        }

        reachedAgua = reachedAgua || CheckDoor(&doorAgua, &watergirl);
        reachedFogo = reachedFogo || CheckDoor(&doorFogo, &fireboy);
        reachedTerra= reachedTerra|| CheckDoor(&doorTerra, &earthboy);
        if (reachedAgua && reachedFogo && reachedTerra) { completed = true; break; }

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode2D(camera);

        DrawTexture(mapTexture, 0, 0, WHITE);

        Player* drawPlayers[3] = { &earthboy, &fireboy, &watergirl };
        LakeType playerTypes[3] = { LAKE_EARTH, LAKE_FIRE, LAKE_WATER };
        bool playerBehindLake[3] = { false };
        for (int i = 0; i < 3; ++i) {
            playerBehindLake[i] = PlayerInsideOwnLake(drawPlayers[i], playerTypes[i], lakeSegs, lakeSegCount);
        }

        LakeAnimFrames* sets[4] = { &animAgua, &animFogo, &animTerra, &animAcido };
        for (int s=0;s<4;++s) {
            sets[s]->timer += dt;
            if (sets[s]->timer >= 0.12f) {
                sets[s]->timer = 0.0f;
                sets[s]->frame = (sets[s]->frame + 1) % 64;
            }
        }

        for (int pass = 0; pass < 2; ++pass) {
            if (pass == 1) {
                for (int i = 0; i < lakeSegCount; ++i) {
                    const LakeSegment* seg = &lakeSegs[i];
                    const LakeAnimFrames* anim = NULL;
                    switch (seg->type) {
                        case LAKE_WATER: anim = &animAgua; break;
                        case LAKE_FIRE: anim = &animFogo; break;
                        case LAKE_EARTH: anim = &animTerra; break;
                        case LAKE_POISON: anim = &animAcido; break;
                    }
                    Texture2D frame = (Texture2D){0};
                    bool has = false;
                    if (anim) {
                        if (seg->part == PART_LEFT   && anim->leftCount  > 0) { frame = anim->left[ anim->frame % anim->leftCount  ]; has = true; }
                        if (seg->part == PART_MIDDLE && anim->middleCount> 0) { frame = anim->middle[anim->frame % anim->middleCount]; has = true; }
                        if (seg->part == PART_RIGHT  && anim->rightCount > 0) { frame = anim->right[ anim->frame % anim->rightCount ]; has = true; }
                    }
                    if (has && seg->part == PART_MIDDLE) {
                        float tile = seg->rect.height;
                        int tiles = (int)floorf(seg->rect.width / tile);
                        float x = seg->rect.x;
                        for (int t=0;t<tiles;++t) {
                            Rectangle dst = { x, seg->rect.y, tile, seg->rect.height };
                            DrawTexturePro(frame, (Rectangle){0,0,(float)frame.width,(float)frame.height}, dst, (Vector2){0,0}, 0.0f, WHITE);
                            x += tile;
                        }
                        float rest = seg->rect.width - tiles*tile;
                        if (rest > 0.1f) {
                            Rectangle dst = { x, seg->rect.y, rest, seg->rect.height };
                            DrawTexturePro(frame, (Rectangle){0,0,(float)frame.width,(float)frame.height}, dst, (Vector2){0,0}, 0.0f, WHITE);
                        }
                    } else if (has) {
                        DrawTexturePro(frame, (Rectangle){0,0,(float)frame.width,(float)frame.height}, seg->rect, (Vector2){0,0}, 0.0f, WHITE);
                    } else {
                        Lake fallback; LakeInit(&fallback, seg->rect.x, seg->rect.y, seg->rect.width, seg->rect.height, seg->type);
                        LakeDraw(&fallback);
                    }
                }
            }
            for (int i = 0; i < 3; ++i) {
                bool drawBehind = playerBehindLake[i];
                if ((pass == 0 && drawBehind) || (pass == 1 && !drawBehind)) {
                    DrawPlayer(*drawPlayers[i]);
                }
            }
        }

        for (int i=0;i<platformCount;i++) {
            Texture2D useTex = barraFallbackTex;
            if (platformTexRefs[i] && platformTexRefs[i]->id != 0) useTex = *platformTexRefs[i];
            DrawPlatformTexture(useTex, platforms[i].rect);
        }

        for (int i=0;i<buttonCount;i++) ButtonDraw(&buttons[i]);

        for (int i=0;i<fans1Count;i++) DrawFanSprite(fan1Draw[i], fan1Active, fanOnFrames, fanOnCount, fanOffTex, fanAnimFrame);
        for (int i=0;i<fans2Count;i++) DrawFanSprite(fan2Draw[i], fan2Active, fanOnFrames, fanOnCount, fanOffTex, fan2AnimFrame);

        Color cWater = reachedAgua ? SKYBLUE : Fade(SKYBLUE, 0.6f);
        Color cFire  = reachedFogo ? ORANGE : Fade(ORANGE, 0.6f);
        Color cEarth = reachedTerra? BROWN  : Fade(BROWN, 0.6f);
        DrawRectangleLinesEx(doorAgua, 2, cWater);
        DrawRectangleLinesEx(doorFogo, 2, cFire);
        DrawRectangleLinesEx(doorTerra,2, cEarth);


        if (debug) {
            for (int i=0;i<totalColisoes;i++) DrawRectangleLinesEx(colisoes[i].rect,1,Fade(GREEN,0.5f));
            for (int i=0;i<lakeSegCount;i++) DrawRectangleLinesEx(lakeSegs[i].rect,1,Fade(BLUE,0.4f));
            DrawFPS(10,10);
        }

        EndMode2D();

        char timer[32];
        int min = (int)(elapsed / 60.0f);
        float sec = elapsed - min * 60.0f;
        sprintf(timer, "%02d:%05.2f", min, sec);
        DrawText(timer, 30, 30, 32, WHITE);
        DrawText("Use os botoes para controlar ventiladores e barras", 30, 70, 20, RAYWHITE);

        EndDrawing();
    }

    UnloadTexture(mapTexture);
    if (barra1Tex.id) UnloadTexture(barra1Tex);
    if (barra2Tex.id) UnloadTexture(barra2Tex);
    if (barraFallbackTex.id) UnloadTexture(barraFallbackTex);
    if (buttonSprites.blue.id) UnloadTexture(buttonSprites.blue);
    if (buttonSprites.red.id) UnloadTexture(buttonSprites.red);
    if (buttonSprites.white.id) UnloadTexture(buttonSprites.white);
    if (buttonSprites.brown.id) UnloadTexture(buttonSprites.brown);
    if (fanOffTex.id) UnloadTexture(fanOffTex);
    for (int i=0;i<fanOnCount;i++) if (fanOnFrames[i].id) UnloadTexture(fanOnFrames[i]);
    UnloadLakeSet(&animAgua);
    UnloadLakeSet(&animFogo);
    UnloadLakeSet(&animTerra);
    UnloadLakeSet(&animAcido);
    UnloadPlayer(&earthboy);
    UnloadPlayer(&fireboy);
    UnloadPlayer(&watergirl);
    if (completed) Ranking_Add(4, Game_GetPlayerName(), elapsed);
    return completed;
}
