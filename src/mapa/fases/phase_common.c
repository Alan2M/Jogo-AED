#include "phase_common.h"
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int ParseRectsFromGroup(const char* tmxPath, const char* groupName, Rectangle* out, int cap) {
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
                sscanf(obj, "<object id=%*[^x]x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\"",
                       &x,&y,&w,&h);
                if (w>0 && h>0 && count < cap) out[count++] = (Rectangle){x,y,w,h};
                p = obj + 8;
            }
        }

        search = groupEnd ? (groupEnd + 14) : (search + 11);
    }

    UnloadFileText(xml);
    return count;
}

int ParseRectsFromAny(const char* tmxPath, const char** names, int nNames,
                      Rectangle* out, int cap) {
    int total = 0;
    for (int i = 0; i < nNames && total < cap; ++i) {
        total += ParseRectsFromGroup(tmxPath, names[i], out + total, cap - total);
    }
    return total;
}

void AddCollisionGroup(const char* tmxPath, const char* name, Colisao* col,
                       int* count, int cap) {
    Rectangle rects[64];
    int n = ParseRectsFromGroup(tmxPath, name, rects, 64);
    for (int i = 0; i < n && *count < cap; ++i) col[(*count)++].rect = rects[i];
}

void AddLakeSegments(const char* tmxPath, const char* name, LakeType type,
                     LakePart part, LakeSegment* segs, int* count, int cap) {
    Rectangle rects[64];
    int n = ParseRectsFromGroup(tmxPath, name, rects, 64);
    for (int i=0;i<n && *count < cap;i++) {
        segs[*count].rect = rects[i];
        segs[*count].type = type;
        segs[*count].part = part;
        (*count)++;
    }
}

void PhaseToLowerCopy(const char* src, char* dst, size_t dstSize) {
    if (!dst || dstSize == 0) return;
    size_t i = 0;
    for (; src && src[i] && i + 1 < dstSize; ++i) {
        dst[i] = (char)tolower((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

int PhaseCollectButtonGroupNames(const char* tmxPath, char names[][PHASE_BUTTON_NAME_LEN], int maxNames) {
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

        char nameBuf[PHASE_BUTTON_NAME_LEN];
        size_t idx = 0;
        while (nameAttr[idx] && nameAttr[idx] != '"' && idx + 1 < sizeof(nameBuf)) {
            nameBuf[idx] = nameAttr[idx];
            idx++;
        }
        nameBuf[idx] = '\0';

        char lower[PHASE_BUTTON_NAME_LEN];
        PhaseToLowerCopy(nameBuf, lower, sizeof(lower));
        if (!strstr(lower, "botao")) { search = tagClose + 1; continue; }

        bool exists = false;
        for (int i = 0; i < count; ++i) {
            if (strcmp(names[i], nameBuf) == 0) { exists = true; break; }
        }
        if (!exists) {
            strncpy(names[count], nameBuf, PHASE_BUTTON_NAME_LEN - 1);
            names[count][PHASE_BUTTON_NAME_LEN - 1] = '\0';
            count++;
        }
        search = tagClose + 1;
    }

    UnloadFileText(xml);
    return count;
}

bool PhaseAnyButtonPressedWithToken(const bool* states, char names[][PHASE_BUTTON_NAME_LEN],
                                    int count, const char* tokenLower) {
    if (!states || !tokenLower || !tokenLower[0]) return false;
    for (int i = 0; i < count; ++i) {
        if (states[i] && strstr(names[i], tokenLower)) return true;
    }
    return false;
}

int LoadFramesRange(Texture2D* arr, int max, const char* pattern,
                    int startIdx, int endIdx) {
    int count = 0; bool started = false;
    for (int i = startIdx; i <= endIdx && count < max; ++i) {
        char path[256]; snprintf(path, sizeof(path), pattern, i);
        if (!FileExists(path)) { if (started) break; else continue; }
        Texture2D tex = LoadTexture(path);
        if (tex.id != 0) { arr[count++] = tex; started = true; }
        else if (started) break;
    }
    return count;
}

static void ResetLakeAnim(LakeAnimFrames* s) {
    s->timer = 0.0f;
    s->frame = 0;
}

void LoadLakeSet_Agua(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/agua/esquerdo/pixil-frame-%d.png",  0, 15);
    s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/agua/meio/pixil-frame-%d.png",      0, 15);
    s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/agua/direito/pixil-frame-%d.png",   0, 15);
    ResetLakeAnim(s);
}

void LoadLakeSet_Terra(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/terra/esquerdo/pixil-frame-%d.png", 0, 15);
    s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/terra/meio/pixil-frame-%d.png",     0, 15);
    s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/terra/direito/pixil-frame-%d.png",  0, 15);
    ResetLakeAnim(s);
}

void LoadLakeSet_Fogo(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/fogo/esquerdo/Esquerda%d.png", 1, 32);
    if (s->leftCount == 0)
        s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/fogo/esquerdo/pixil-frame-%d.png", 0, 31);

    s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/fogo/meio/Meio%d.png",     1, 32);
    if (s->middleCount == 0)
        s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/fogo/meio/pixil-frame-%d.png",      0, 31);

    s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/fogo/direito/Direita%d.png", 1, 32);
    if (s->rightCount == 0)
        s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/fogo/direito/pixil-frame-%d.png",   0, 31);

    ResetLakeAnim(s);
}

void LoadLakeSet_Acido(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   32, "assets/map/acido/esquerdo/pixil-frame-%d.png",  0, 15);
    s->middleCount = LoadFramesRange(s->middle, 32, "assets/map/acido/meio/pixil-frame-%d.png",      0, 15);
    s->rightCount  = LoadFramesRange(s->right,  32, "assets/map/acido/direito/pixil-frame-%d.png",   0, 15);
    ResetLakeAnim(s);
}

void PhaseUnloadLakeSet(LakeAnimFrames* s) {
    if (!s) return;
    for (int i = 0; i < s->leftCount;   ++i) UnloadTexture(s->left[i]);
    for (int i = 0; i < s->middleCount; ++i) UnloadTexture(s->middle[i]);
    for (int i = 0; i < s->rightCount;  ++i) UnloadTexture(s->right[i]);
    s->leftCount = s->middleCount = s->rightCount = 0;
    s->timer = 0.0f;
    s->frame = 0;
}

void PhaseUpdateLakeAnimations(LakeAnimFrames** sets, int setCount, float dt, float frameRate) {
    if (!sets || setCount <= 0 || frameRate <= 0.0f) return;
    for (int s = 0; s < setCount; ++s) {
        LakeAnimFrames* frames = sets[s];
        if (!frames) continue;
        frames->timer += dt;
        if (frames->timer >= frameRate) {
            frames->timer -= frameRate;
            int maxFrames = frames->middleCount;
            if (maxFrames == 0) maxFrames = frames->leftCount;
            if (maxFrames == 0) maxFrames = frames->rightCount;
            if (maxFrames > 0) {
                frames->frame = (frames->frame + 1) % maxFrames;
            }
        }
    }
}

const Texture2D* PhasePickLakeFrame(const LakeAnimFrames* frames, LakePart part) {
    if (!frames) return NULL;
    int idx = frames->frame;
    const Texture2D* array = NULL;
    int count = 0;
    switch (part) {
        case PART_LEFT:   array = frames->left;   count = frames->leftCount; break;
        case PART_MIDDLE: array = frames->middle; count = frames->middleCount; break;
        case PART_RIGHT:  array = frames->right;  count = frames->rightCount; break;
    }
    if (!array || count <= 0) return NULL;
    if (idx < 0) idx = 0;
    if (idx >= count) idx %= count;
    return &array[idx];
}

bool PhasePlayerInsideOwnLake(const Player* pl, LakeType type,
                              const LakeSegment* segs, int segCount) {
    if (!pl || !segs || segCount <= 0) return false;
    for (int i = 0; i < segCount; ++i) {
        if (segs[i].type != type) continue;
        if (CheckCollisionRecs(pl->rect, segs[i].rect)) return true;
    }
    return false;
}

Texture2D LoadTextureIfExists(const char* path) {
    if (!FileExists(path)) return (Texture2D){0};
    return LoadTexture(path);
}

void PhaseLoadButtonSprites(ButtonSpriteSet* set) {
    if (!set) return;
    set->blue  = LoadTextureIfExists("assets/map/buttons/pixil-layer-bluebutton.png");
    set->red   = LoadTextureIfExists("assets/map/buttons/pixil-layer-redbutton.png");
    set->white = LoadTextureIfExists("assets/map/buttons/pixil-layer-whitebutton.png");
    set->brown = LoadTextureIfExists("assets/map/buttons/pixil-layer-brownbutton.png");
}

void PhaseUnloadButtonSprites(ButtonSpriteSet* set) {
    if (!set) return;
    if (set->blue.id != 0)  UnloadTexture(set->blue);
    if (set->red.id != 0)   UnloadTexture(set->red);
    if (set->white.id != 0) UnloadTexture(set->white);
    if (set->brown.id != 0) UnloadTexture(set->brown);
    set->blue = set->red = set->white = set->brown = (Texture2D){0};
}

const Texture2D* PhasePickButtonSprite(const ButtonSpriteSet* set, const char* nameLower) {
    if (!set) return NULL;
    const Texture2D* match = NULL;
    if (nameLower) {
        if (strstr(nameLower, "branc") && set->white.id != 0) match = &set->white;
        else if (strstr(nameLower, "azul") && set->blue.id != 0) match = &set->blue;
        else if (strstr(nameLower, "verm") && set->red.id != 0) match = &set->red;
        else if ((strstr(nameLower, "marr") || strstr(nameLower, "terra")) && set->brown.id != 0) match = &set->brown;
    }
    if (match) return match;

    if (set->blue.id != 0)  return &set->blue;
    if (set->red.id != 0)   return &set->red;
    if (set->white.id != 0) return &set->white;
    if (set->brown.id != 0) return &set->brown;
    return NULL;
}

float PhaseMoveTowards(float a, float b, float maxStep) {
    if (a < b) {
        a += maxStep;
        if (a > b) a = b;
    } else if (a > b) {
        a -= maxStep;
        if (a < b) a = b;
    }
    return a;
}

static void PhaseClampPlatform(PhasePlatform* platform) {
    if (!platform) return;
    float minY = platform->area.y;
    float maxY = platform->area.y + platform->area.height - platform->rect.height;
    if (maxY < minY) maxY = minY;
    if (platform->rect.y < minY) platform->rect.y = minY;
    if (platform->rect.y > maxY) platform->rect.y = maxY;
}

void PhasePlatformInit(PhasePlatform* platform, Rectangle rect, Rectangle area, float speed) {
    if (!platform) return;
    if (area.width <= 0 || area.height <= 0) area = rect;
    platform->rect = rect;
    platform->area = area;
    platform->speed = speed;
    PhaseClampPlatform(platform);
    platform->startY = platform->rect.y;
}

float PhasePlatformBottomTarget(const PhasePlatform* platform) {
    if (!platform) return 0.0f;
    float minY = platform->area.y;
    float maxY = platform->area.y + platform->area.height - platform->rect.height;
    if (maxY < minY) maxY = minY;
    return maxY;
}

float PhasePlatformMoveTowards(PhasePlatform* platform, float targetY) {
    if (!platform) return 0.0f;
    float minY = platform->area.y;
    float maxY = platform->area.y + platform->area.height - platform->rect.height;
    if (maxY < minY) maxY = minY;
    if (targetY < minY) targetY = minY;
    if (targetY > maxY) targetY = maxY;
    float prevY = platform->rect.y;
    platform->rect.y = PhaseMoveTowards(platform->rect.y, targetY, platform->speed);
    PhaseClampPlatform(platform);
    return platform->rect.y - prevY;
}

void PhaseHandlePlatformTop(Player* pl, Rectangle plat, float deltaY) {
    if (!pl || plat.width <= 0 || plat.height <= 0) return;
    if (!CheckCollisionRecs(pl->rect, plat)) return;
    float pBottom = pl->rect.y + pl->rect.height;
    if (pBottom <= plat.y + 16.0f && pl->velocity.y >= -1.0f) {
        pl->rect.y = plat.y - pl->rect.height;
        pl->velocity.y = 0;
        pl->isJumping = false;
        pl->rect.y += deltaY;
    }
}

Rectangle PhaseAcquireSpriteForRect(Rectangle target, Rectangle* sprites, bool* used, int spriteCount) {
    if (!sprites || spriteCount <= 0) return target;
    Vector2 targetRef = { target.x + target.width * 0.5f, target.y + target.height };
    int best = -1;
    float bestDist = FLT_MAX;
    for (int i = 0; i < spriteCount; ++i) {
        if (used && used[i]) continue;
        Rectangle spr = sprites[i];
        Vector2 sprRef = { spr.x + spr.width * 0.5f, spr.y + spr.height };
        float dx = targetRef.x - sprRef.x;
        float dy = targetRef.y - sprRef.y;
        float dist = dx*dx + dy*dy;
        if (dist < bestDist) { bestDist = dist; best = i; }
    }
    if (best >= 0) {
        if (used) used[best] = true;
        return sprites[best];
    }
    return target;
}

bool PhaseCheckDoor(const Rectangle* door, const Player* p) {
    if (!door || !p) return false;
    if (door->width <= 0 || door->height <= 0) return false;
    return CheckCollisionRecs(*door, p->rect);
}

static void PhaseResolvePlayerVsRect(Player* pl, Rectangle bloco, float stepHeight) {
    if (!pl) return;
    if (!CheckCollisionRecs(pl->rect, bloco)) return;
    float dx = (pl->rect.x + pl->rect.width * 0.5f) - (bloco.x + bloco.width * 0.5f);
    float dy = (pl->rect.y + pl->rect.height * 0.5f) - (bloco.y + bloco.height * 0.5f);
    float overlapX = (pl->rect.width * 0.5f + bloco.width * 0.5f) - fabsf(dx);
    float overlapY = (pl->rect.height * 0.5f + bloco.height * 0.5f) - fabsf(dy);
    if (overlapX <= 0 || overlapY <= 0) return;

    if (overlapX < overlapY) {
        Rectangle teste = pl->rect;
        teste.y -= stepHeight;
        if (!(dy > 0 && pl->velocity.y > 0) && !CheckCollisionRecs(teste, bloco)) {
            pl->rect.y -= stepHeight;
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

void PhaseResolvePlayersVsWorld(Player** players, int playerCount,
                                const Colisao* colisoes, int totalColisoes, float stepHeight) {
    if (!players || !colisoes || playerCount <= 0 || totalColisoes <= 0) return;
    for (int p = 0; p < playerCount; ++p) {
        Player* pl = players[p];
        if (!pl) continue;
        for (int i = 0; i < totalColisoes; ++i) {
            PhaseResolvePlayerVsRect(pl, colisoes[i].rect, stepHeight);
        }
    }
}
