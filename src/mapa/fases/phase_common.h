#ifndef PHASE_COMMON_H
#define PHASE_COMMON_H

#include <stddef.h>
#include <stdbool.h>
#include "raylib.h"
#include "../../player/player.h"
#include "../../objects/lake.h"

#define PHASE_BUTTON_NAME_LEN 64
#ifndef BUTTON_NAME_LEN
#define BUTTON_NAME_LEN PHASE_BUTTON_NAME_LEN
#endif
#define PHASE_STEP_HEIGHT     14.0f

typedef struct PhaseCollision {
    Rectangle rect;
} Colisao;

typedef enum PhaseLakePart {
    PART_LEFT = 0,
    PART_MIDDLE,
    PART_RIGHT
} LakePart;

typedef struct PhaseLakeSegment {
    Rectangle rect;
    LakeType type;
    LakePart part;
} LakeSegment;

typedef struct PhaseLakeAnimFrames {
    Texture2D left[32];   int leftCount;
    Texture2D middle[32]; int middleCount;
    Texture2D right[32];  int rightCount;
    float timer; int frame;
} LakeAnimFrames;

typedef struct PhaseButtonSpriteSet {
    Texture2D blue;
    Texture2D red;
    Texture2D white;
    Texture2D brown;
} ButtonSpriteSet;

typedef struct PhasePlatform {
    Rectangle rect;
    Rectangle area;
    float startY;
    float speed;
} PhasePlatform;

typedef PhasePlatform Platform;

int ParseRectsFromGroup(const char* tmxPath, const char* groupName, Rectangle* out, int cap);
int ParseRectsFromAny(const char* tmxPath, const char** names, int nNames, Rectangle* out, int cap);
void AddCollisionGroup(const char* tmxPath, const char* name, Colisao* col, int* count, int cap);
void AddLakeSegments(const char* tmxPath, const char* name, LakeType type, LakePart part,
                     LakeSegment* segs, int* count, int cap);

void PhaseToLowerCopy(const char* src, char* dst, size_t dstSize);
int PhaseCollectButtonGroupNames(const char* tmxPath, char names[][PHASE_BUTTON_NAME_LEN], int maxNames);
bool PhaseAnyButtonPressedWithToken(const bool* states, char names[][PHASE_BUTTON_NAME_LEN],
                                    int count, const char* tokenLower);

int LoadFramesRange(Texture2D* arr, int max, const char* pattern, int startIdx, int endIdx);
void LoadLakeSet_Agua(LakeAnimFrames* frames);
void LoadLakeSet_Terra(LakeAnimFrames* frames);
void LoadLakeSet_Fogo(LakeAnimFrames* frames);
void LoadLakeSet_Acido(LakeAnimFrames* frames);
void PhaseUnloadLakeSet(LakeAnimFrames* frames);
void PhaseUpdateLakeAnimations(LakeAnimFrames** sets, int setCount, float dt, float frameRate);
const Texture2D* PhasePickLakeFrame(const LakeAnimFrames* frames, LakePart part);
bool PhasePlayerInsideOwnLake(const Player* pl, LakeType type, const LakeSegment* segs, int segCount);

Texture2D LoadTextureIfExists(const char* path);
void PhaseLoadButtonSprites(ButtonSpriteSet* set);
void PhaseUnloadButtonSprites(ButtonSpriteSet* set);
const Texture2D* PhasePickButtonSprite(const ButtonSpriteSet* set, const char* nameLower);

float PhaseMoveTowards(float a, float b, float maxStep);
void PhasePlatformInit(PhasePlatform* platform, Rectangle rect, Rectangle area, float speed);
float PhasePlatformBottomTarget(const PhasePlatform* platform);
float PhasePlatformMoveTowards(PhasePlatform* platform, float targetY);
void PhaseHandlePlatformTop(Player* pl, Rectangle platformRect, float deltaY);

Rectangle PhaseAcquireSpriteForRect(Rectangle target, Rectangle* sprites, bool* used, int spriteCount);
bool PhaseCheckDoor(const Rectangle* door, const Player* p);
void PhaseResolvePlayersVsWorld(Player** players, int playerCount,
                                const Colisao* colisoes, int totalColisoes, float stepHeight);

#endif
