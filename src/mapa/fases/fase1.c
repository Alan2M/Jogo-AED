#include "raylib.h"
#include "../../player/player.h"
#include "../../objects/lake.h"
#include "../../objects/button.h"
#include "../../game/game.h"
#include "../../ranking/ranking.h"
#include "../../objects/fan.h"
#include "../../interface/pause.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define MAX_COLISOES 1024
#define STEP_HEIGHT 14.0f   // altura máxima que o jogador pode "subir" sem pular
#define MAX_BUTTONS 16
#define BUTTON_NAME_LEN 64
#define MAX_COOP_BOXES 4
#define MAX_FAN_FRAMES 8
#define FAN_FRAME_TIME 0.12f

static const char* const FASE1_TMX_PATH = "assets/maps/Fase222.tmx";
static const char* const FASE1_MAP_TEXTURE = "assets/maps/Map.png";

typedef struct {
    Rectangle rect;
} Colisao;

typedef enum { PART_LEFT = 0, PART_MIDDLE, PART_RIGHT } LakePart;

#define MAX_LAKE_SEGS 256

typedef struct LakeSegment {
    Rectangle rect;
    LakeType type;
    LakePart part;
} LakeSegment;

typedef struct LakeAnimFrames {
    Texture2D left[16]; int leftCount;
    Texture2D middle[16]; int middleCount;
    Texture2D right[16]; int rightCount;
    float timer; int frame;
} LakeAnimFrames;

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

static Texture2D LoadTextureIfExists(const char* path) {
    if (!FileExists(path)) return (Texture2D){0};
    return LoadTexture(path);
}

typedef struct Platform {
    Rectangle rect;
    Rectangle area;
    float startY;
    float speed;
} Platform;

static float moveTowards(float a, float b, float maxStep) {
    if (a < b) { a += maxStep; if (a > b) a = b; }
    else if (a > b) { a -= maxStep; if (a < b) a = b; }
    return a;
}

static void PlatformInit(Platform* p, Rectangle rect, Rectangle area, float speed) {
    if (area.width <= 0 || area.height <= 0) area = rect;
    p->rect = rect;
    p->area = area;
    p->speed = speed;
    float minY = area.y;
    float maxY = area.y + area.height - rect.height;
    if (maxY < minY) maxY = minY;
    if (p->rect.y < minY) p->rect.y = minY;
    if (p->rect.y > maxY) p->rect.y = maxY;
    p->startY = p->rect.y;
}

static float PlatformBottomTarget(const Platform* p) {
    float minY = p->area.y;
    float maxY = p->area.y + p->area.height - p->rect.height;
    if (maxY < minY) maxY = minY;
    return maxY;
}

static float PlatformMoveTowards(Platform* p, float targetY) {
    if (p->rect.width <= 0 || p->rect.height <= 0) return 0.0f;
    float minY = p->area.y;
    float maxY = p->area.y + p->area.height - p->rect.height;
    if (maxY < minY) maxY = minY;
    if (targetY < minY) targetY = minY;
    if (targetY > maxY) targetY = maxY;
    float prevY = p->rect.y;
    p->rect.y = moveTowards(p->rect.y, targetY, p->speed);
    if (p->rect.y < minY) p->rect.y = minY;
    if (p->rect.y > maxY) p->rect.y = maxY;
    return p->rect.y - prevY;
}

static void HandlePlatformTop(Player* pl, Rectangle plat, float deltaY) {
    if (plat.width <= 0 || plat.height <= 0) return;
    if (!CheckCollisionRecs(pl->rect, plat)) return;
    float pBottom = pl->rect.y + pl->rect.height;
    if (pBottom <= plat.y + 16.0f && pl->velocity.y >= -1.0f) {
        pl->rect.y = plat.y - pl->rect.height;
        pl->velocity.y = 0;
        pl->isJumping = false;
        pl->rect.y += deltaY;
    }
}

static bool AnyButtonPressedWithToken(const PhaseButton* buttons, int count, const char* tokenLower) {
    if (!tokenLower || !tokenLower[0]) return false;
    for (int i = 0; i < count; ++i) {
        if (buttons[i].pressed && strstr(buttons[i].nameLower, tokenLower)) return true;
    }
    return false;
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

static void DrawPlatformWithTexture(const Platform* plat, Texture2D tex, Color fallback) {
    if (plat->rect.width <= 0 || plat->rect.height <= 0) return;
    if (tex.id == 0) {
        DrawRectangleRec(plat->rect, fallback);
        return;
    }

    float scale = plat->rect.width / (float)tex.width;
    if (scale <= 0.0f) scale = 1.0f;
    float tileHeight = tex.height * scale;
    if (tileHeight <= 0.0f) tileHeight = plat->rect.height;

    float y = plat->rect.y;
    float bottom = plat->rect.y + plat->rect.height;
    while (y < bottom - 0.1f) {
        float remaining = bottom - y;
        float destHeight = tileHeight;
        Rectangle src = {0, 0, (float)tex.width, (float)tex.height};
        if (remaining < destHeight) {
            float ratio = remaining / destHeight;
            destHeight = remaining;
            src.height = tex.height * ratio;
        }
        Rectangle dest = { plat->rect.x, y, plat->rect.width, destHeight };
        DrawTexturePro(tex, src, dest, (Vector2){0,0}, 0.0f, WHITE);
        y += destHeight;
    }
}

static void DrawFanColumn(Rectangle column, Texture2D frame) {
    if (column.width <= 0 || column.height <= 0) return;
    if (frame.id == 0) {
        DrawRectangleRec(column, (Color){180, 220, 255, 120});
        return;
    }

    float scale = column.width / (float)frame.width;
    if (scale <= 0.0f) scale = 1.0f;
    float tileHeight = frame.height * scale;
    if (tileHeight <= 0.0f) tileHeight = column.height;

    float y = column.y;
    float bottom = column.y + column.height;
    while (y < bottom - 0.1f) {
        float remaining = bottom - y;
        float destHeight = tileHeight;
        Rectangle src = {0, 0, (float)frame.width, (float)frame.height};
        if (remaining < destHeight) {
            float ratio = remaining / destHeight;
            destHeight = remaining;
            src.height = frame.height * ratio;
        }
        Rectangle dest = { column.x, y, column.width, destHeight };
        DrawTexturePro(frame, src, dest, (Vector2){0,0}, 0.0f, WHITE);
        y += destHeight;
    }
}

typedef struct {
    Rectangle rect;
    float velX;
    float velY;
} CoOpBox;

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
            box->velY = 0;
        }
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

static int LoadFramesRange(Texture2D* arr, int max, const char* pattern, int startIdx, int endIdx) {
    // Carrega frames sequenciais verificando existência (evita WARNINGs); para no primeiro buraco
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
    s->leftCount = LoadFramesRange(s->left, 16, "assets/map/agua/esquerdo/pixil-frame-%d.png", 0, 15);
    s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/agua/meio/pixil-frame-%d.png", 0, 15);
    s->rightCount = LoadFramesRange(s->right, 16, "assets/map/agua/direito/pixil-frame-%d.png", 0, 15);
    s->timer = 0.0f; s->frame = 0;
}

static void LoadLakeSet_Terra(LakeAnimFrames* s) {
    s->leftCount = LoadFramesRange(s->left, 16, "assets/map/terra/esquerdo/pixil-frame-%d.png", 0, 15);
    s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/terra/meio/pixil-frame-%d.png", 0, 15);
    s->rightCount = LoadFramesRange(s->right, 16, "assets/map/terra/direito/pixil-frame-%d.png", 0, 15);
    s->timer = 0.0f; s->frame = 0;
}

static void LoadLakeSet_Fogo(LakeAnimFrames* s) {
    // Tenta nomes Esquerda1.., Meio1.., Direita1..; se falhar, tenta pixil-frame-%d
    s->leftCount = LoadFramesRange(s->left, 16, "assets/map/fogo/esquerdo/Esquerda%d.png", 1, 32);
    if (s->leftCount == 0) s->leftCount = LoadFramesRange(s->left, 16, "assets/map/fogo/esquerdo/pixil-frame-%d.png", 0, 31);
    s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/fogo/meio/Meio%d.png", 1, 32);
    if (s->middleCount == 0) s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/fogo/meio/pixil-frame-%d.png", 0, 31);
    s->rightCount = LoadFramesRange(s->right, 16, "assets/map/fogo/direito/Direita%d.png", 1, 32);
    if (s->rightCount == 0) s->rightCount = LoadFramesRange(s->right, 16, "assets/map/fogo/direito/pixil-frame-%d.png", 0, 31);
    s->timer = 0.0f; s->frame = 0;
}

static void UnloadLakeSet(LakeAnimFrames* s) {
    for (int i = 0; i < s->leftCount; ++i) UnloadTexture(s->left[i]);
    for (int i = 0; i < s->middleCount; ++i) UnloadTexture(s->middle[i]);
    for (int i = 0; i < s->rightCount; ++i) UnloadTexture(s->right[i]);
    s->leftCount = s->middleCount = s->rightCount = 0;
}

static void LoadLakeSet_Acido(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   16, "assets/map/acido/esquerdo/pixil-frame-%d.png", 0, 15);
    s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/acido/meio/pixil-frame-%d.png",     0, 15);
    s->rightCount  = LoadFramesRange(s->right,  16, "assets/map/acido/direito/pixil-frame-%d.png",  0, 15);
    s->timer = 0.0f; s->frame = 0;
}

static int ParseRectsFromGroup(const char* tmxPath, const char* groupName, Rectangle* out, int cap) {
    int count = 0;
    char* xml = LoadFileText(tmxPath);
    if (!xml) return 0;

    const char* search = xml;
    while ((search = strstr(search, "<objectgroup")) != NULL) {
        const char* tagClose = strchr(search, '>');
        if (!tagClose) break;

        bool match = false;
        {
            size_t len = (size_t)(tagClose - search);
            char* header = (char*)malloc(len + 1);
            if (!header) { UnloadFileText(xml); return count; }
            memcpy(header, search, len);
            header[len] = '\0';
            char findName[128]; snprintf(findName, sizeof(findName), "name=\"%s\"", groupName);
            if (strstr(header, findName)) match = true;
            free(header);
        }

        const char* groupEnd = strstr(tagClose + 1, "</objectgroup>");
        if (!groupEnd) break;

        if (match) {
            const char* p = tagClose + 1;
            while (p < groupEnd) {
                const char* obj = strstr(p, "<object ");
                if (!obj || obj >= groupEnd) break;
                float x=0,y=0,w=0,h=0;
                if (sscanf(obj, "<object id=\"%*d\" x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\"", &x,&y,&w,&h) == 4) {
                    if (w>0 && h>0 && count < cap) out[count++] = (Rectangle){x,y,w,h};
                }
                p = obj + 8;
            }
        }
        search = groupEnd + 14;
    }

    UnloadFileText(xml);
    return count;
}

static int ParseColisoesDaCamada(const char* tmxPath, Colisao* out, int outCap) {
    Rectangle rects[MAX_COLISOES];
    int n = ParseRectsFromGroup(tmxPath, "Colisao", rects, MAX_COLISOES);
    if (n > outCap) n = outCap;
    for (int i = 0; i < n; ++i) out[i].rect = rects[i];
    return n;
}

bool Fase1(void) {
    // --- Carrega colisões somente da camada de objetos "Colisao" ---
    Colisao colisoes[MAX_COLISOES];
    int totalColisoes = ParseColisoesDaCamada(FASE1_TMX_PATH, colisoes, MAX_COLISOES);
    if (totalColisoes <= 0) {
        printf("❌ Nao foi possivel carregar colisoes da camada 'Colisao' em %s\n", FASE1_TMX_PATH);
    }

    // --- Carrega segmentos de lagos conforme camadas do Tiled ---
    LakeSegment lakeSegs[MAX_LAKE_SEGS]; int lakeCount = 0;
    Rectangle tmpRects[128]; int n;
    // Agua
    n = ParseRectsFromGroup(FASE1_TMX_PATH, "Lago_Agua_Esquerdo", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_WATER, PART_LEFT };
    n = ParseRectsFromGroup(FASE1_TMX_PATH, "Lago_Agua_Meio", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_WATER, PART_MIDDLE };
    n = ParseRectsFromGroup(FASE1_TMX_PATH, "Lago_Agua_Direito", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_WATER, PART_RIGHT };
    // Fogo
    n = ParseRectsFromGroup(FASE1_TMX_PATH, "Lago_Fogo_Esquerdo", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_FIRE, PART_LEFT };
    n = ParseRectsFromGroup(FASE1_TMX_PATH, "Lago_Fogo_Meio", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_FIRE, PART_MIDDLE };
    n = ParseRectsFromGroup(FASE1_TMX_PATH, "Lago_Fogo_Direito", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_FIRE, PART_RIGHT };
    // Terra (marrom)
    n = ParseRectsFromGroup(FASE1_TMX_PATH, "Lago_Marrom_Esquerdo", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_EARTH, PART_LEFT };
    n = ParseRectsFromGroup(FASE1_TMX_PATH, "Lago_Marrom_Meio", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_EARTH, PART_MIDDLE };
    n = ParseRectsFromGroup(FASE1_TMX_PATH, "Lago_Marrom_Direito", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_EARTH, PART_RIGHT };
    // Veneno (verde)
    n = ParseRectsFromGroup(FASE1_TMX_PATH, "Lago_Verde_Esquerda", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_POISON, PART_LEFT };
    n = ParseRectsFromGroup(FASE1_TMX_PATH, "Lago_Verde_Meio", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_POISON, PART_MIDDLE };
    n = ParseRectsFromGroup(FASE1_TMX_PATH, "Lago_Verde_Direita", tmpRects, 128);
    for (int i = 0; i < n && lakeCount < MAX_LAKE_SEGS; ++i) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_POISON, PART_RIGHT };

    // Carrega animações para cada tipo com assets existentes
    LakeAnimFrames animAgua = {0}, animFogo = {0}, animTerra = {0}, animAcido = {0};
    LoadLakeSet_Agua(&animAgua);
    LoadLakeSet_Fogo(&animFogo);
    LoadLakeSet_Terra(&animTerra);
    LoadLakeSet_Acido(&animAcido);

    // --- Carrega textura do mapa ---
    Texture2D mapTexture = LoadTexture(FASE1_MAP_TEXTURE);
    if (mapTexture.id == 0) {
        printf("❌ Erro ao carregar %s\n", FASE1_MAP_TEXTURE);
        return false;
    }

    Rectangle doorTerra = { mapTexture.width - 210.0f, mapTexture.height - 180.0f, 40.0f, 120.0f };
    Rectangle doorFogo  = { mapTexture.width - 150.0f, mapTexture.height - 180.0f, 40.0f, 120.0f };
    Rectangle doorAgua  = { mapTexture.width -  90.0f, mapTexture.height - 180.0f, 40.0f, 120.0f };
    Rectangle doorBuf[2];
    if (ParseRectsFromGroup(FASE1_TMX_PATH, "Porta_Terra", doorBuf, 1) > 0) doorTerra = doorBuf[0];
    if (ParseRectsFromGroup(FASE1_TMX_PATH, "Porta_Fogo",  doorBuf, 1) > 0) doorFogo  = doorBuf[0];
    if (ParseRectsFromGroup(FASE1_TMX_PATH, "Porta_Agua",  doorBuf, 1) > 0) doorAgua  = doorBuf[0];

    // --- Botões definidos nas camadas de objetos ---
    PhaseButton buttons[MAX_BUTTONS] = {0};
    int buttonCount = 0;
    ButtonSpriteSet buttonSprites = {0};
    buttonSprites.blue  = LoadTexture("assets/map/buttons/pixil-layer-bluebutton.png");
    buttonSprites.red   = LoadTexture("assets/map/buttons/pixil-layer-redbutton.png");
    buttonSprites.white = LoadTexture("assets/map/buttons/pixil-layer-whitebutton.png");
    buttonSprites.brown = LoadTexture("assets/map/buttons/pixil-layer-brownbutton.png");
    char groupNames[MAX_BUTTONS][BUTTON_NAME_LEN] = {{0}};
    int groupCount = CollectButtonGroupNames(FASE1_TMX_PATH, groupNames, MAX_BUTTONS);
    Rectangle btnBuffer[8];
    for (int g = 0; g < groupCount && buttonCount < MAX_BUTTONS; ++g) {
        int rectsFound = ParseRectsFromGroup(FASE1_TMX_PATH, groupNames[g], btnBuffer, (int)(sizeof(btnBuffer)/sizeof(btnBuffer[0])));
        if (rectsFound <= 0) continue;
        char lowerName[BUTTON_NAME_LEN];
        ToLowerCopy(groupNames[g], lowerName, sizeof(lowerName));
        Color colorUp, colorDown;
        DetermineButtonColors(lowerName, &colorUp, &colorDown);
        const Texture2D* sprite = PickButtonSprite(lowerName, &buttonSprites);
        for (int r = 0; r < rectsFound && buttonCount < MAX_BUTTONS; ++r) {
            Rectangle rect = btnBuffer[r];
            ButtonInit(&buttons[buttonCount].button, rect.x, rect.y, rect.width, rect.height, colorUp, colorDown);
            if (sprite) ButtonSetSprites(&buttons[buttonCount].button, sprite, NULL);
            strncpy(buttons[buttonCount].name, groupNames[g], BUTTON_NAME_LEN - 1);
            buttons[buttonCount].name[BUTTON_NAME_LEN - 1] = '\0';
            ToLowerCopy(groupNames[g], buttons[buttonCount].nameLower, BUTTON_NAME_LEN);
            buttons[buttonCount].pressed = false;
            buttonCount++;
        }
    }

    Platform barra1 = {0};
    Platform elevador1 = {0};
    Platform elevador2 = {0};
    int barra1ColIndex = -1;
    Rectangle rectBuf[2];
    Rectangle areaBuf[2];
    int rectCount = ParseRectsFromGroup(FASE1_TMX_PATH, "Barra1", rectBuf, 1);
    if (rectCount > 0) {
        Rectangle area = rectBuf[0];
        if (ParseRectsFromGroup(FASE1_TMX_PATH, "AreaMovimentoBarra1", areaBuf, 1) > 0) area = areaBuf[0];
        PlatformInit(&barra1, rectBuf[0], area, 2.0f);
        if (totalColisoes < MAX_COLISOES) {
            barra1ColIndex = totalColisoes;
            colisoes[totalColisoes++].rect = barra1.rect;
        }
    }
    rectCount = ParseRectsFromGroup(FASE1_TMX_PATH, "Elevaodor1_Colisao", rectBuf, 1);
    if (rectCount > 0) {
        Rectangle area = rectBuf[0];
        if (ParseRectsFromGroup(FASE1_TMX_PATH, "Elavador1_area", areaBuf, 1) > 0) area = areaBuf[0];
        PlatformInit(&elevador1, rectBuf[0], area, 1.6f);
    }
    rectCount = ParseRectsFromGroup(FASE1_TMX_PATH, "Elevaodor2_Colisao", rectBuf, 1);
    if (rectCount > 0) {
        Rectangle area = rectBuf[0];
        if (ParseRectsFromGroup(FASE1_TMX_PATH, "Elavador2_area", areaBuf, 1) > 0) area = areaBuf[0];
        PlatformInit(&elevador2, rectBuf[0], area, 1.8f);
    }

    Texture2D barraAzulTex = LoadTexture("assets/map/barras/azul.png");
    Texture2D barraBrancaTex = LoadTexture("assets/map/barras/branca.png");
    Fan vent1 = {0}; bool haveFan = false;
    Rectangle fanArea = (Rectangle){0};
    Texture2D fanFrames[MAX_FAN_FRAMES] = {0};
    int fanFrameCount = 0;
    float fanAnimTimer = 0.0f;
    int fanAnimFrame = 0;
    {
        Rectangle ventRects[4];
        int vCount = ParseRectsFromGroup(FASE1_TMX_PATH, "Ventilador1", ventRects, 4);
        if (vCount > 0) {
            FanInit(&vent1, ventRects[0].x, ventRects[0].y, ventRects[0].width, ventRects[0].height, 0.4f);
            haveFan = true;
        }
        Rectangle areaRect[2];
        if (ParseRectsFromGroup(FASE1_TMX_PATH, "Area_Ventilador1", areaRect, 2) > 0) fanArea = areaRect[0];
        else if (haveFan) fanArea = vent1.rect;
        const char* fanPaths[] = {
            "assets/map/vento/ligado1.png",
            "assets/map/vento/ligado2.png",
            "assets/map/vento/ligado3.png",
            "assets/map/vento/ligado4.png"
        };
        for (int i = 0; i < (int)(sizeof(fanPaths)/sizeof(fanPaths[0])) && fanFrameCount < MAX_FAN_FRAMES; ++i) {
            Texture2D tex = LoadTextureIfExists(fanPaths[i]);
            if (tex.id != 0) fanFrames[fanFrameCount++] = tex;
        }
    }
    CoOpBox coopBoxes[MAX_COOP_BOXES]; int coopBoxCount = 0;
    {
        Rectangle boxRects[MAX_COOP_BOXES];
        int boxCount = ParseRectsFromGroup(FASE1_TMX_PATH, "Caixa", boxRects, MAX_COOP_BOXES);
        for (int i = 0; i < boxCount && coopBoxCount < MAX_COOP_BOXES; ++i) {
            coopBoxes[coopBoxCount].rect = boxRects[i];
            coopBoxes[coopBoxCount].velX = 0.0f;
            coopBoxes[coopBoxCount].velY = 0.0f;
            coopBoxCount++;
        }
    }
    Texture2D coopBoxTex = LoadTexture("assets/map/caixa/caixa2.png");
    if (coopBoxTex.id == 0) coopBoxTex = LoadTexture("assets/map/caixa/caixa.png");

    // --- Inicializa jogadores menores ---
    Player earthboy, fireboy, watergirl;
    InitEarthboy(&earthboy);
    InitFireboy(&fireboy);
    InitWatergirl(&watergirl);

    earthboy.rect = (Rectangle){300, 700, 45, 50};
    fireboy.rect  = (Rectangle){400, 700, 45, 50};
    watergirl.rect= (Rectangle){500, 700, 45, 50};
    Vector2 spawnEarth = (Vector2){ earthboy.rect.x, earthboy.rect.y };
    Vector2 spawnFire  = (Vector2){ fireboy.rect.x,  fireboy.rect.y };
    Vector2 spawnWater = (Vector2){ watergirl.rect.x, watergirl.rect.y };

    // --- Câmera fixa ---
    Camera2D camera = {0};
    camera.target = (Vector2){mapTexture.width / 2.0f, mapTexture.height / 2.0f};
    camera.offset = (Vector2){GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
    camera.zoom = 1.0f;

    bool completed = false;
    float elapsed = 0.0f;
    bool debug = false;
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

        // --- Atualiza jogadores ---
        UpdatePlayer(&earthboy, (Rectangle){0, mapTexture.height, mapTexture.width, 100}, KEY_A, KEY_D, KEY_W);
        UpdatePlayer(&fireboy,  (Rectangle){0, mapTexture.height, mapTexture.width, 100}, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&watergirl,(Rectangle){0, mapTexture.height, mapTexture.width, 100}, KEY_J, KEY_L, KEY_I);

        // --- Colisões completas (com degraus e teto bloqueado) ---
        Player* players[3] = {&earthboy, &fireboy, &watergirl};
        for (int p = 0; p < 3; p++) {
            Player* pl = players[p];
            for (int i = 0; i < totalColisoes; i++) {
                Rectangle bloco = colisoes[i].rect;

                if (CheckCollisionRecs(pl->rect, bloco)) {
                    float dx = (pl->rect.x + pl->rect.width / 2) - (bloco.x + bloco.width / 2);
                    float dy = (pl->rect.y + pl->rect.height / 2) - (bloco.y + bloco.height / 2);
                    float overlapX = (pl->rect.width / 2 + bloco.width / 2) - fabsf(dx);
                    float overlapY = (pl->rect.height / 2 + bloco.height / 2) - fabsf(dy);

                    if (overlapX < overlapY) {
                        // --- Tentativa de subir degrau ---
                        if (dy > 0 && pl->velocity.y > 0) {
                            // caindo ao lado do bloco → trata no eixo Y primeiro (evita atravessar lateral)
                        } else {
                            Rectangle teste = pl->rect;
                            teste.y -= STEP_HEIGHT;
                            if (!CheckCollisionRecs(teste, bloco)) {
                                pl->rect.y -= STEP_HEIGHT;
                                goto fim_bloco; // subiu o degrau, evita correção lateral extra
                            }
                        }
                        // --- Bloqueio lateral normal ---
                        if (dx > 0) pl->rect.x += overlapX;
                        else        pl->rect.x -= overlapX;
                        pl->velocity.x = 0;
                    } else {
                        // --- Corrige colisão vertical (teto e chão) ---
                        if (dy > 0 && pl->velocity.y < 0) {
                            // Jogador bateu de baixo (subindo) → teto
                            pl->rect.y += overlapY;
                            pl->velocity.y = 0;
                        }
                        else if (dy < 0 && pl->velocity.y >= 0) {
                            // Jogador caiu sobre o bloco (chão)
                            pl->rect.y -= overlapY;
                            pl->velocity.y = 0;
                            pl->isJumping = false;
                        }
                    }
fim_bloco:
                }
            }
        }

        struct { Player* pl; int keyLeft; int keyRight; } controls[3] = {
            { &earthboy, KEY_A, KEY_D },
            { &fireboy, KEY_LEFT, KEY_RIGHT },
            { &watergirl, KEY_J, KEY_L }
        };

        const float BOX_GRAVITY = 0.45f;
        const float BOX_MAX_FALL = 10.0f;
        for (int b = 0; b < coopBoxCount; ++b) {
            CoOpBox* box = &coopBoxes[b];
            int pushRight = 0, pushLeft = 0;
            for (int i = 0; i < 3; ++i) {
                Player* pl = controls[i].pl;
                if (PlayerPushingBox(pl, box->rect, true, controls[i].keyRight, 6.0f)) pushRight++;
                else if (PlayerPushingBox(pl, box->rect, false, controls[i].keyLeft, 6.0f)) pushLeft++;
            }
            if (pushRight >= 2 && pushRight >= pushLeft) box->velX += 1.1f;
            else if (pushLeft >= 2 && pushLeft > pushRight) box->velX -= 1.1f;
            box->velY += BOX_GRAVITY;
            if (box->velY > BOX_MAX_FALL) box->velY = BOX_MAX_FALL;

            float prevX = box->rect.x;
            box->rect.x += box->velX;
            ResolveCoOpBoxVsWorld(box, colisoes, totalColisoes);
            float deltaX = box->rect.x - prevX;
            for (int i = 0; i < 3; ++i) {
                ResolvePlayerVsCoOpBox(players[i], box, deltaX);
            }
            float prevY = box->rect.y;
            box->rect.y += box->velY;
            ResolveCoOpBoxVsWorld(box, colisoes, totalColisoes);
            float deltaY = box->rect.y - prevY;
            (void)deltaY;
            box->velX *= 0.88f;
            if (fabsf(box->velX) < 0.02f) box->velX = 0.0f;
        }

        // --- Interação com lagos: matar/reiniciar se tocar lago errado ---
        for (int p = 0; p < 3; ++p) {
            Player* pl = players[p];
            LakeType elem = (p == 0) ? LAKE_EARTH : (p == 1 ? LAKE_FIRE : LAKE_WATER);
            for (int i = 0; i < lakeCount; ++i) {
                Lake l; l.rect = lakeSegs[i].rect; l.type = lakeSegs[i].type; l.color = (Color){0};
                bool dead = LakeHandlePlayer(&l, pl, elem);
                if (dead) {
                    // Respawn
                    if (p == 0) { pl->rect.x = spawnEarth.x; pl->rect.y = spawnEarth.y; }
                    else if (p == 1) { pl->rect.x = spawnFire.x; pl->rect.y = spawnFire.y; }
                    else { pl->rect.x = spawnWater.x; pl->rect.y = spawnWater.y; }
                    pl->velocity = (Vector2){0,0}; pl->isJumping = false;
                    break;
                }
            }
        }

        for (int i = 0; i < buttonCount; ++i) {
            buttons[i].pressed = ButtonUpdate(&buttons[i].button, &earthboy, &fireboy, &watergirl);
        }

        bool barraButtonsActive = AnyButtonPressedWithToken(buttons, buttonCount, "barra1");
        bool elevador1Active =
            AnyButtonPressedWithToken(buttons, buttonCount, "elevaodor1") ||
            AnyButtonPressedWithToken(buttons, buttonCount, "elavador1") ||
            AnyButtonPressedWithToken(buttons, buttonCount, "elevador1");
        bool elevador2Active =
            AnyButtonPressedWithToken(buttons, buttonCount, "elevaodor2") ||
            AnyButtonPressedWithToken(buttons, buttonCount, "elavador2") ||
            AnyButtonPressedWithToken(buttons, buttonCount, "elevador2");

        bool earthAtDoor = PlayerAtDoor(&earthboy, doorTerra);
        bool fireAtDoor  = PlayerAtDoor(&fireboy,  doorFogo);
        bool waterAtDoor = PlayerAtDoor(&watergirl, doorAgua);

        if (fanArea.width > 0 && fanArea.height > 0) {
            for (int p = 0; p < 3; ++p) {
                Player* pl = players[p];
                if (CheckCollisionRecs(pl->rect, fanArea)) {
                    pl->velocity.y += 0.35f;
                    if (pl->velocity.y > 10.0f) pl->velocity.y = 10.0f;
                }
            }
            if (fanFrameCount > 0) {
                fanAnimTimer += dt;
                if (fanAnimTimer >= FAN_FRAME_TIME) {
                    fanAnimTimer -= FAN_FRAME_TIME;
                    fanAnimFrame = (fanAnimFrame + 1) % fanFrameCount;
                }
            }
        }

        float barraDelta = PlatformMoveTowards(&barra1, barraButtonsActive ? PlatformBottomTarget(&barra1) : barra1.startY);
        float elev1Delta = PlatformMoveTowards(&elevador1, elevador1Active ? PlatformBottomTarget(&elevador1) : elevador1.startY);
        float elev2Delta = PlatformMoveTowards(&elevador2, elevador2Active ? PlatformBottomTarget(&elevador2) : elevador2.startY);

        if (barra1ColIndex >= 0 && barra1ColIndex < totalColisoes) {
            colisoes[barra1ColIndex].rect = barra1.rect;
        }

        for (int p = 0; p < 3; ++p) {
            Player* pl = players[p];
            if (barra1.rect.width > 0 && barra1.rect.height > 0) HandlePlatformTop(pl, barra1.rect, barraDelta);
            if (elevador1.rect.width > 0 && elevador1.rect.height > 0) HandlePlatformTop(pl, elevador1.rect, elev1Delta);
            if (elevador2.rect.width > 0 && elevador2.rect.height > 0) HandlePlatformTop(pl, elevador2.rect, elev2Delta);
        }

        // --- Desenho ---
        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode2D(camera);


        DrawTexture(mapTexture, 0, 0, WHITE);

        if (haveFan) {
            Rectangle fanDrawArea = (fanArea.width > 0 && fanArea.height > 0) ? fanArea : vent1.rect;
            if (fanFrameCount > 0) {
                Texture2D frame = fanFrames[fanAnimFrame % fanFrameCount];
                DrawFanColumn(fanDrawArea, frame);
            } else {
                DrawRectangleRec(fanDrawArea, (Color){160, 210, 255, 120});
                DrawRectangleLinesEx(fanDrawArea, 1, (Color){110, 160, 210, 180});
            }
        }

        DrawPlatformWithTexture(&barra1, barraAzulTex, BLUE);
        DrawPlatformWithTexture(&elevador1, barraBrancaTex, LIGHTGRAY);
        DrawPlatformWithTexture(&elevador2, barraBrancaTex, LIGHTGRAY);
        for (int b = 0; b < coopBoxCount; ++b) {
            if (coopBoxTex.id != 0) {
                DrawTexturePro(coopBoxTex, (Rectangle){0,0,(float)coopBoxTex.width,(float)coopBoxTex.height}, coopBoxes[b].rect, (Vector2){0,0}, 0.0f, WHITE);
            } else {
                DrawRectangleRec(coopBoxes[b].rect, DARKBROWN);
            }
        }

        for (int i = 0; i < buttonCount; ++i) {
            ButtonDraw(&buttons[i].button);
        }

        // Decide quem está dentro do lago correto (para desenhar por trás)
        bool insideOwn[3] = { false, false, false };
        Player* playersArr[3] = { &earthboy, &fireboy, &watergirl };
        LakeType elemsArr[3]  = { LAKE_EARTH, LAKE_FIRE, LAKE_WATER };
        for (int ip = 0; ip < 3; ++ip) {
            Player* pl = playersArr[ip];
            LakeType elem = elemsArr[ip];
            for (int i = 0; i < lakeCount; ++i) {
                const LakeSegment* seg = &lakeSegs[i];
                if (seg->type != elem) continue;
                if (!CheckCollisionRecs(pl->rect, seg->rect)) continue;
                // Requer pequena profundidade vertical para considerar "dentro"
                Rectangle ov = GetCollisionRec(pl->rect, seg->rect);
                float pBottom = pl->rect.y + pl->rect.height;
                if (pBottom > seg->rect.y + 2 && ov.height >= 4) { insideOwn[ip] = true; break; }
            }
        }

        // Avança frames (8 FPS)
        float lakeDt = GetFrameTime();
        LakeAnimFrames* sets[4] = { &animAgua, &animFogo, &animTerra, &animAcido };
        for (int s = 0; s < 4; ++s) {
            sets[s]->timer += lakeDt;
            if (sets[s]->timer >= 0.12f) { sets[s]->timer = 0.0f; sets[s]->frame = (sets[s]->frame + 1) % 64; }
        }

        // 1) Desenha jogadores que estao dentro do lago correto (por trás)
        if (insideOwn[0]) DrawPlayer(earthboy);
        if (insideOwn[1]) DrawPlayer(fireboy);
        if (insideOwn[2]) DrawPlayer(watergirl);

        // 2) Desenha lagos animados por cima
        for (int i = 0; i < lakeCount; ++i) {
            const LakeSegment* seg = &lakeSegs[i];
            const LakeAnimFrames* anim = NULL;
            if (seg->type == LAKE_WATER) anim = &animAgua;
            else if (seg->type == LAKE_FIRE) anim = &animFogo;
            else if (seg->type == LAKE_EARTH) anim = &animTerra;
            else if (seg->type == LAKE_POISON) anim = &animAcido;

            if (anim && ((seg->part==PART_LEFT && anim->leftCount>0) || (seg->part==PART_MIDDLE && anim->middleCount>0) || (seg->part==PART_RIGHT && anim->rightCount>0))) {
                Texture2D frame;
                int idx;
                if (seg->part == PART_LEFT) { idx = (anim->frame % anim->leftCount); frame = anim->left[idx]; }
                else if (seg->part == PART_MIDDLE) { idx = (anim->frame % anim->middleCount); frame = anim->middle[idx]; }
                else { idx = (anim->frame % anim->rightCount); frame = anim->right[idx]; }
                if (seg->part == PART_MIDDLE) {
                    // Tile horizontal com passo de 27px (tamanho do tile do mapa)
                    float tile = seg->rect.height; // 27 no mapa
                    int tiles = (int)floorf(seg->rect.width / tile);
                    float x = seg->rect.x;
                    for (int t = 0; t < tiles; ++t) {
                        Rectangle dst = (Rectangle){ x, seg->rect.y, tile, seg->rect.height };
                        DrawTexturePro(frame, (Rectangle){0,0,(float)frame.width,(float)frame.height}, dst, (Vector2){0,0}, 0.0f, WHITE);
                        x += tile;
                    }
                    // resto (se houver)
                    float rest = seg->rect.width - tiles*tile;
                    if (rest > 0.1f) {
                        Rectangle dst = (Rectangle){ x, seg->rect.y, rest, seg->rect.height };
                        DrawTexturePro(frame, (Rectangle){0,0,(float)frame.width,(float)frame.height}, dst, (Vector2){0,0}, 0.0f, WHITE);
                    }
                } else {
                    DrawTexturePro(frame, (Rectangle){0,0,(float)frame.width,(float)frame.height}, seg->rect, (Vector2){0,0}, 0.0f, WHITE);
                }
            } else {
                // Fallback: desenha sólido com cor se não houver animação
                Color c = (seg->type==LAKE_POISON)? (Color){60,180,60,230} : (Color){90,90,90,200};
                DrawRectangleRec(seg->rect, c);
            }
        }

        // 3) Desenha os demais jogadores por cima dos lagos
        if (!insideOwn[0]) DrawPlayer(earthboy);
        if (!insideOwn[1]) DrawPlayer(fireboy);
        if (!insideOwn[2]) DrawPlayer(watergirl);

        // --- Debug ---
        if (debug) {
            for (int i = 0; i < totalColisoes; i++)
                DrawRectangleLinesEx(colisoes[i].rect, 1, Fade(GREEN, 0.5f));
            if (barra1.area.width > 0 && barra1.area.height > 0) DrawRectangleLinesEx(barra1.area, 1, Fade(BLUE, 0.4f));
            if (elevador1.area.width > 0 && elevador1.area.height > 0) DrawRectangleLinesEx(elevador1.area, 1, Fade(LIGHTGRAY, 0.4f));
            if (elevador2.area.width > 0 && elevador2.area.height > 0) DrawRectangleLinesEx(elevador2.area, 1, Fade(LIGHTGRAY, 0.4f));
            if (fanArea.width > 0 && fanArea.height > 0) DrawRectangleLinesEx(fanArea, 1, Fade(SKYBLUE, 0.4f));
            DrawRectangleLinesEx(doorTerra, 1, Fade(BROWN, 0.6f));
            DrawRectangleLinesEx(doorFogo, 1, Fade(RED, 0.6f));
            DrawRectangleLinesEx(doorAgua, 1, Fade(BLUE, 0.6f));

            DrawText(TextFormat("Earthboy: (%.0f, %.0f)", earthboy.rect.x, earthboy.rect.y), 10, 10, 20, YELLOW);
            DrawText(TextFormat("Fireboy:  (%.0f, %.0f)", fireboy.rect.x, fireboy.rect.y), 10, 35, 20, ORANGE);
            DrawText(TextFormat("Watergirl:(%.0f, %.0f)", watergirl.rect.x, watergirl.rect.y), 10, 60, 20, SKYBLUE);
            DrawText("TAB - Modo Debug", 10, 90, 20, GRAY);
            DrawFPS(10, 120);
        }

        // Checagem de chegada individual
        if (earthAtDoor && fireAtDoor && waterAtDoor) { completed = true; EndMode2D(); EndDrawing(); break; }

        EndMode2D();
        EndDrawing();
    }

    // --- Libera recursos ---
    UnloadTexture(mapTexture);
    UnloadLakeSet(&animAgua);
    UnloadLakeSet(&animFogo);
    UnloadLakeSet(&animTerra);
    UnloadLakeSet(&animAcido);
    UnloadPlayer(&earthboy);
    UnloadPlayer(&fireboy);
    UnloadPlayer(&watergirl);
    if (barraAzulTex.id != 0) UnloadTexture(barraAzulTex);
    if (barraBrancaTex.id != 0) UnloadTexture(barraBrancaTex);
    if (coopBoxTex.id != 0) UnloadTexture(coopBoxTex);
    for (int i = 0; i < fanFrameCount; ++i) if (fanFrames[i].id != 0) UnloadTexture(fanFrames[i]);
    if (buttonSprites.blue.id != 0) UnloadTexture(buttonSprites.blue);
    if (buttonSprites.red.id != 0) UnloadTexture(buttonSprites.red);
    if (buttonSprites.white.id != 0) UnloadTexture(buttonSprites.white);
    if (buttonSprites.brown.id != 0) UnloadTexture(buttonSprites.brown);
    if (completed) Ranking_Add(1, Game_GetPlayerName(), elapsed);
    return completed;
}
