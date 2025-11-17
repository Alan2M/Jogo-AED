#include "raylib.h"
#include "fases.h"
#include "../../player/player.h"
#include "../../objects/lake.h"
#include "../../interface/pause.h"
#include "../../objects/button.h"
#include "../../game/game.h"
#include "../../ranking/ranking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define MAX_COLISOES 1024
#define MAX_LAKE_SEGS 256
#define MAX_BUTTONS 4
#define BUTTON_NAME_LEN 64
#define MAX_COOP_BOXES 4
#define STEP_HEIGHT  14.0f

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
                if (w > 0 && h > 0 && count < cap) out[count++] = (Rectangle){ x, y, w, h };
                p = obj + 8;
            }
        }

        search = groupEnd ? (groupEnd + 14) : (search + 11);
    }

    UnloadFileText(xml);
    return count;
}

static void AddCollisionGroup(const char* tmxPath, const char* name, Colisao* col, int* count, int cap) {
    Rectangle rects[32];
    int n = ParseRectsFromGroup(tmxPath, name, rects, 32);
    for (int i = 0; i < n && *count < cap; ++i) col[(*count)++].rect = rects[i];
}

static bool CheckDoor(const Rectangle* door, const Player* p) {
    if (door->width <= 0 || door->height <= 0) return false;
    return CheckCollisionRecs(*door, p->rect);
}

typedef struct Platform {
    Rectangle rect;
    Rectangle area;
    float startY;
    float speed;
} Platform;

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

static void HandlePlatformTop(Player* pl, Rectangle plat, float deltaY) {
    if (!CheckCollisionRecs(pl->rect, plat)) return;
    float pBottom = pl->rect.y + pl->rect.height;
    if (pBottom <= plat.y + 16.0f && pl->velocity.y >= -1.0f) {
        pl->rect.y = plat.y - pl->rect.height;
        pl->velocity.y = 0;
        pl->isJumping = false;
        pl->rect.y += deltaY;
    }
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
    if (overlapX <= 0 || overlapY <= 0) return;
    if (overlapX < overlapY) {
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
            pl->rect.x += deltaX;
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

static bool PlayerInsideOwnLake(const Player* pl, LakeType type, const LakeSegment* segs, int segCount) {
    for (int i = 0; i < segCount; ++i) {
        if (segs[i].type != type) continue;
        if (CheckCollisionRecs(pl->rect, segs[i].rect)) return true;
    }
    return false;
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

bool Fase1(void) {
    const char* tmxPath = "assets/maps/fase1/fase1.tmx";
    Texture2D mapTexture = LoadTexture("assets/maps/fase1/fase1.png");

    Colisao colisoes[MAX_COLISOES];
    int totalColisoes = 0;
    Rectangle tmp[1024];
    int n = ParseRectsFromGroup(tmxPath, "colisao", tmp, 1024);
    for (int i = 0; i < n && totalColisoes < MAX_COLISOES; ++i) colisoes[totalColisoes++].rect = tmp[i];

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

    LakeAnimFrames animAgua = {0}, animFogo = {0}, animTerra = {0}, animAcido = {0};
    LoadLakeSet_Agua(&animAgua);
    LoadLakeSet_Fogo(&animFogo);
    LoadLakeSet_Terra(&animTerra);
    LoadLakeSet_Acido(&animAcido);

    Vector2 spawnEarth = { 300, 700 };
    Vector2 spawnFire  = { 400, 700 };
    Vector2 spawnWater = { 500, 700 };
    Rectangle spawn[4];
    if (ParseRectsFromGroup(tmxPath, "spawnTerra", spawn, 4) > 0)
        spawnEarth = (Vector2){ spawn[0].x, spawn[0].y };
    if (ParseRectsFromGroup(tmxPath, "spawnFogo", spawn, 4) > 0)
        spawnFire = (Vector2){ spawn[0].x, spawn[0].y };
    if (ParseRectsFromGroup(tmxPath, "spawnAgua", spawn, 4) > 0)
        spawnWater = (Vector2){ spawn[0].x, spawn[0].y };

    Rectangle doorWater = {0}, doorFire = {0}, doorEarth = {0};
    if (ParseRectsFromGroup(tmxPath, "PortaAgua", &doorWater, 1) == 0)
        doorWater = (Rectangle){ mapTexture.width - 90.0f, mapTexture.height - 180.0f, 30.0f, 120.0f };
    if (ParseRectsFromGroup(tmxPath, "PortaFogo", &doorFire, 1) == 0)
        doorFire = (Rectangle){ mapTexture.width - 150.0f, mapTexture.height - 180.0f, 30.0f, 120.0f };
    if (ParseRectsFromGroup(tmxPath, "PortaTerra", &doorEarth, 1) == 0)
        doorEarth = (Rectangle){ mapTexture.width - 210.0f, mapTexture.height - 180.0f, 30.0f, 120.0f };

    Button buttons[MAX_BUTTONS]; float buttonAnim[MAX_BUTTONS] = {0}; int buttonCount = 0;
    ButtonSpriteSet buttonSprites = {0};
    buttonSprites.blue  = LoadTexture("assets/map/buttons/pixil-layer-bluebutton.png");
    buttonSprites.red   = LoadTexture("assets/map/buttons/pixil-layer-redbutton.png");
    buttonSprites.white = LoadTexture("assets/map/buttons/pixil-layer-whitebutton.png");
    buttonSprites.brown = LoadTexture("assets/map/buttons/pixil-layer-brownbutton.png");
    char buttonGroupNames[MAX_BUTTONS][BUTTON_NAME_LEN] = {{0}};
    Rectangle btnRect[8];
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
            buttonCount++;
        }
    }
    if (buttonCount == 0) {
        Rectangle legacyRects[4];
        if (ParseRectsFromGroup(tmxPath, "botao1barra1", legacyRects, 4) > 0 && buttonCount < MAX_BUTTONS) {
            ButtonInit(&buttons[buttonCount++], legacyRects[0].x, legacyRects[0].y, legacyRects[0].width, legacyRects[0].height,
                       (Color){200,200,40,200}, (Color){200,140,20,255});
        }
        if (ParseRectsFromGroup(tmxPath, "botao2barra1", legacyRects, 4) > 0 && buttonCount < MAX_BUTTONS) {
            ButtonInit(&buttons[buttonCount++], legacyRects[0].x, legacyRects[0].y, legacyRects[0].width, legacyRects[0].height,
                       (Color){40,200,200,200}, (Color){20,140,200,255});
        }
        if (ParseRectsFromGroup(tmxPath, "botao3barra1", legacyRects, 4) > 0 && buttonCount < MAX_BUTTONS) {
            ButtonInit(&buttons[buttonCount++], legacyRects[0].x, legacyRects[0].y, legacyRects[0].width, legacyRects[0].height,
                       (Color){200,40,200,200}, (Color){140,20,200,255});
        }
    }

    Platform barra = {0};
    Rectangle barraRect[2];
    if (ParseRectsFromGroup(tmxPath, "barra1", barraRect, 2) > 0) {
        barraRect[0].height = 27;
        Rectangle area = barraRect[0];
        area.y -= 120;
        area.height += 120;
        PlatformInit(&barra, barraRect[0], area, 1.5f);
    }

    CoOpBox coopBoxes[MAX_COOP_BOXES]; int coopBoxCount = 0;
    Rectangle boxRects[MAX_COOP_BOXES];
    int nBoxes = ParseRectsFromGroup(tmxPath, "caixa1", boxRects, MAX_COOP_BOXES);
    for (int i=0;i<nBoxes && coopBoxCount < MAX_COOP_BOXES;i++) {
        coopBoxes[coopBoxCount].rect = boxRects[i];
        coopBoxes[coopBoxCount].velX = 0.0f;
        coopBoxCount++;
    }

    Texture2D coopBoxTex = LoadTexture("assets/map/caixa/caixa3.png");
    if (coopBoxTex.id == 0) coopBoxTex = LoadTexture("assets/map/caixa/caixa2.png");
    if (coopBoxTex.id == 0) coopBoxTex = LoadTexture("assets/map/caixa/caixa.png");

    Texture2D barraTex = LoadTexture("assets/map/barras/azul.png");
    if (barraTex.id == 0) barraTex = LoadTexture("assets/map/barras/barragorda.png");
    if (barraTex.id == 0) barraTex = LoadTexture("assets/map/barras/branca.png");
    Player earthboy, fireboy, watergirl;
    InitEarthboy(&earthboy);
    InitFireboy(&fireboy);
    InitWatergirl(&watergirl);

    earthboy.rect = (Rectangle){ spawnEarth.x, spawnEarth.y, 45, 50 };
    fireboy.rect  = (Rectangle){ spawnFire.x,  spawnFire.y,  45, 50 };
    watergirl.rect= (Rectangle){ spawnWater.x, spawnWater.y, 45, 50 };

    Camera2D camera = {0};
    camera.target = (Vector2){ mapTexture.width/2.0f, mapTexture.height/2.0f };
    camera.offset = (Vector2){ GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };
    camera.zoom = 1.0f;

    bool reachedWater = false, reachedFire = false, reachedEarth = false;
    bool completed = false;
    bool debug = false;
    float elapsed = 0.0f;
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

        UpdatePlayer(&earthboy, (Rectangle){0, mapTexture.height, mapTexture.width, 200}, KEY_A, KEY_D, KEY_W);
        UpdatePlayer(&fireboy,  (Rectangle){0, mapTexture.height, mapTexture.width, 200}, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&watergirl,(Rectangle){0, mapTexture.height, mapTexture.width, 200}, KEY_J, KEY_L, KEY_I);

        Player* players[3] = { &earthboy, &fireboy, &watergirl };
        for (int p = 0; p < 3; ++p) {
            Player* pl = players[p];
            for (int i = 0; i < totalColisoes; ++i) {
                Rectangle bloco = colisoes[i].rect;
                if (!CheckCollisionRecs(pl->rect, bloco)) continue;
                float dx = (pl->rect.x + pl->rect.width / 2) - (bloco.x + bloco.width / 2);
                float dy = (pl->rect.y + pl->rect.height / 2) - (bloco.y + bloco.height / 2);
                float overlapX = (pl->rect.width / 2 + bloco.width / 2) - fabsf(dx);
                float overlapY = (pl->rect.height / 2 + bloco.height / 2) - fabsf(dy);

                if (overlapX < overlapY) {
                    Rectangle teste = pl->rect;
                    teste.y -= STEP_HEIGHT;
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

        struct ControlInfo { Player* pl; int keyLeft; int keyRight; } controls[3] = {
            { &earthboy, KEY_A, KEY_D },
            { &fireboy, KEY_LEFT, KEY_RIGHT },
            { &watergirl, KEY_J, KEY_L }
        };

        for (int b=0;b<coopBoxCount;b++) {
            CoOpBox* box = &coopBoxes[b];
            int pushLeft = 0, pushRight = 0;
            for (int i=0;i<3;i++) {
                Player* pl = controls[i].pl;
                if (PlayerPushingBox(pl, box->rect, true, controls[i].keyRight, 6.0f)) pushRight++;
                else if (PlayerPushingBox(pl, box->rect, false, controls[i].keyLeft, 6.0f)) pushLeft++;
            }
            if (pushRight >= 3 && pushRight >= pushLeft) box->velX += 1.2f;
            else if (pushLeft >= 3 && pushLeft > pushRight) box->velX -= 1.2f;
            box->velX *= 0.88f;
            if (fabsf(box->velX) < 0.05f) box->velX = 0.0f;
            if (box->velX > 4.5f) box->velX = 4.5f;
            if (box->velX < -4.5f) box->velX = -4.5f;
            float prevX = box->rect.x;
            box->rect.x += box->velX;
            if (box->rect.x < 0) { box->rect.x = 0; box->velX = 0; }
            float maxX = mapTexture.width - box->rect.width;
            if (box->rect.x > maxX) { box->rect.x = maxX; box->velX = 0; }
            ResolveCoOpBoxVsWorld(box, colisoes, totalColisoes);
            float deltaX = box->rect.x - prevX;
            for (int i=0;i<3;i++) ResolvePlayerVsCoOpBox(controls[i].pl, box, deltaX);
        }

        bool buttonStates[MAX_BUTTONS] = { false };
        for (int i = 0; i < buttonCount; ++i) {
            bool pressed = ButtonUpdate(&buttons[i], &earthboy, &fireboy, &watergirl);
            buttonStates[i] = pressed;
            if (pressed) buttonAnim[i] += dt;
            else buttonAnim[i] = 0.0f;
        }

        float barraDeltaY = 0.0f;
        float barraPrevY = barra.rect.y;
        if (barra.area.height > 0 && barra.rect.height > 0) {
            bool anyPressed = false;
            for (int i=0;i<buttonCount;i++) if (buttonStates[i]) { anyPressed = true; break; }
            float targetUp = barra.area.y;
            float targetDown = barra.area.y + barra.area.height - barra.rect.height;
            if (anyPressed) barra.rect.y = moveTowards(barra.rect.y, targetUp, barra.speed);
            else           barra.rect.y = moveTowards(barra.rect.y, targetDown, barra.speed);
            if (barra.rect.y < barra.area.y) barra.rect.y = barra.area.y;
            float maxY = barra.area.y + barra.area.height - barra.rect.height;
            if (barra.rect.y > maxY) barra.rect.y = maxY;
            barraDeltaY = barra.rect.y - barraPrevY;
        }

        for (int p = 0; p < 3; ++p) {
            Player* pl = players[p];
            LakeType elem = (p == 0) ? LAKE_EARTH : (p == 1 ? LAKE_FIRE : LAKE_WATER);
            for (int i = 0; i < lakeSegCount; ++i) {
                Lake l; l.rect = lakeSegs[i].rect; l.type = lakeSegs[i].type; l.color = (Color){0};
                if (LakeHandlePlayer(&l, pl, elem)) {
                    if (p == 0) { pl->rect.x = spawnEarth.x; pl->rect.y = spawnEarth.y; }
                    else if (p == 1) { pl->rect.x = spawnFire.x; pl->rect.y = spawnFire.y; }
                    else { pl->rect.x = spawnWater.x; pl->rect.y = spawnWater.y; }
                    pl->velocity = (Vector2){0,0};
                    pl->isJumping = false;
                    break;
                }
            }
        }

        reachedWater = reachedWater || CheckDoor(&doorWater, &watergirl);
        reachedFire  = reachedFire  || CheckDoor(&doorFire,  &fireboy);
        reachedEarth = reachedEarth || CheckDoor(&doorEarth, &earthboy);
        if (reachedWater && reachedFire && reachedEarth) { completed = true; break; }

        for (int p=0;p<3;p++) if (barra.rect.width>0) HandlePlatformTop(players[p], barra.rect, barraDeltaY);

    BeginDrawing();
    ClearBackground(BLACK);
    BeginMode2D(camera);

        DrawTexture(mapTexture, 0, 0, WHITE);

        Player* drawPlayers[3] = { &earthboy, &fireboy, &watergirl };
        LakeType playerLakeTypes[3] = { LAKE_EARTH, LAKE_FIRE, LAKE_WATER };
        bool playerBehindLake[3] = { false };
        for (int i = 0; i < 3; ++i) {
            playerBehindLake[i] = PlayerInsideOwnLake(drawPlayers[i], playerLakeTypes[i], lakeSegs, lakeSegCount);
        }

        float lakeDt = dt;
        LakeAnimFrames* sets[4] = { &animAgua, &animFogo, &animTerra, &animAcido };
        for (int s=0; s<4; ++s) {
            sets[s]->timer += lakeDt;
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
                        case LAKE_FIRE:  anim = &animFogo; break;
                        case LAKE_EARTH: anim = &animTerra; break;
                        case LAKE_POISON: anim = &animAcido; break;
                    }
                    Texture2D frame = {0};
                    bool has = false;
                    if (anim) {
                        if (seg->part == PART_LEFT  && anim->leftCount  > 0) { frame = anim->left [anim->frame % anim->leftCount];   has = true; }
                        if (seg->part == PART_MIDDLE&& anim->middleCount> 0) { frame = anim->middle[anim->frame % anim->middleCount]; has = true; }
                        if (seg->part == PART_RIGHT && anim->rightCount > 0) { frame = anim->right[anim->frame % anim->rightCount];  has = true; }
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

        for (int i = 0; i < buttonCount; ++i) ButtonDraw(&buttons[i]);

        Color cWater = reachedWater ? SKYBLUE : Fade(SKYBLUE, 0.6f);
        Color cFire  = reachedFire  ? ORANGE : Fade(ORANGE, 0.6f);
        Color cEarth = reachedEarth ? BROWN  : Fade(BROWN, 0.6f);
        DrawRectangleLinesEx(doorWater, 2, cWater);
        DrawRectangleLinesEx(doorFire,  2, cFire);
        DrawRectangleLinesEx(doorEarth, 2, cEarth);

        if (barra.rect.width > 0) {
            if (barraTex.id != 0) {
                DrawTexturePro(barraTex, (Rectangle){0,0,(float)barraTex.width,(float)barraTex.height},
                               barra.rect, (Vector2){0,0}, 0.0f, WHITE);
            } else {
                DrawRectangleRec(barra.rect, (Color){200, 200, 200, 255});
            }
        }
        for (int b=0;b<coopBoxCount;b++) {
            Rectangle rect = coopBoxes[b].rect;
            if (coopBoxTex.id != 0)
                DrawTexturePro(coopBoxTex,(Rectangle){0,0,(float)coopBoxTex.width,(float)coopBoxTex.height},rect,(Vector2){0,0},0.0f,WHITE);
            else
                DrawRectangleRec(rect, (Color){150,120,80,255});
        }

        if (debug) {
            for (int i=0;i<totalColisoes;i++)
                DrawRectangleLinesEx(colisoes[i].rect, 1, Fade(GREEN, 0.5f));
            for (int i=0;i<lakeSegCount;i++)
                DrawRectangleLinesEx(lakeSegs[i].rect, 1, Fade(BLUE, 0.4f));
            if (barra.rect.width > 0) DrawRectangleLinesEx(barra.area, 1, Fade(YELLOW, 0.4f));
            for (int b=0;b<coopBoxCount;b++)
                DrawRectangleLinesEx(coopBoxes[b].rect, 1, Fade(BROWN, 0.6f));
            DrawFPS(10,10);
        }

        EndMode2D();

        char timer[32];
        int min = (int)(elapsed / 60.0f);
        float sec = elapsed - min * 60.0f;
        sprintf(timer, "%02d:%05.2f", min, sec);
        DrawText(timer, 30, 30, 32, WHITE);
        DrawText("Leve cada personagem para sua porta correspondente", 30, 70, 20, RAYWHITE);

        EndDrawing();
    }

    UnloadTexture(mapTexture);
    UnloadLakeSet(&animAgua);
    UnloadLakeSet(&animFogo);
    UnloadLakeSet(&animTerra);
    UnloadLakeSet(&animAcido);
    if (coopBoxTex.id) UnloadTexture(coopBoxTex);
    if (barraTex.id) UnloadTexture(barraTex);
    if (buttonSprites.blue.id) UnloadTexture(buttonSprites.blue);
    if (buttonSprites.red.id) UnloadTexture(buttonSprites.red);
    if (buttonSprites.white.id) UnloadTexture(buttonSprites.white);
    if (buttonSprites.brown.id) UnloadTexture(buttonSprites.brown);
    UnloadPlayer(&earthboy);
    UnloadPlayer(&fireboy);
    UnloadPlayer(&watergirl);
    if (completed) Ranking_Add(3, Game_GetPlayerName(), elapsed);
    return completed;
}
