#include "raylib.h"
#include "../../player/player.h"
#include "../../objects/lake.h"
#include "../../objects/goal.h"
#include "../../objects/button.h"
#include "../../objects/fan.h"
#include "../../game/game.h"
#include "../../ranking/ranking.h"
#include "../../interface/pause.h"
#include "phase_common.h"
#include "../../audio/theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <float.h>
#include <stddef.h>

#define MAX_COLISOES 1024
#define MAX_LAKE_SEGS 512
#define MAX_FANS 64
#define MAX_COOP_BOXES 4
#define MAX_BUTTONS 8

typedef struct {
    Rectangle rect;
    float velX;
} CoOpBox;

typedef struct PhaseButton {
    Button button;
    char name[BUTTON_NAME_LEN];
    char nameLower[BUTTON_NAME_LEN];
    bool pressed;
} PhaseButton;

static bool AnyButtonPressedWithToken(const PhaseButton* buttons, int count, const char* tokenLower) {
    if (!tokenLower || !tokenLower[0]) return false;
    for (int i = 0; i < count; ++i) {
        if (buttons[i].pressed && strstr(buttons[i].nameLower, tokenLower)) return true;
    }
    return false;
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
        PhaseToLowerCopy(nameBuf, nameLower, sizeof(nameLower));
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
bool Fase5(void) {
    const char* tmx = "assets/maps/fase5/fase5.tmx";
    Texture2D mapTex = LoadTexture("assets/maps/fase5/fase5.png"); if (mapTex.id==0) { printf("Erro: mapa Fase2Mapa.png\n"); return false; }

    // Colis�es
    Colisao col[MAX_COLISOES]; int nCol = 0; {
        const char* names[] = { "Colisao", "Colis�o", "Colisoes", "Colis�es" };
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

    // Bot�es com sprites
    ButtonSpriteSet buttonSprites = (ButtonSpriteSet){0};
    PhaseLoadButtonSprites(&buttonSprites);
    PhaseButton buttons[MAX_BUTTONS] = {0};
    int buttonCount = 0;
    char buttonNames[MAX_BUTTONS][BUTTON_NAME_LEN] = {{0}};
    int buttonGroups = PhaseCollectButtonGroupNames(tmx, buttonNames, MAX_BUTTONS);
    Rectangle btnRect[4];
    for (int g = 0; g < buttonGroups && buttonCount < MAX_BUTTONS; ++g) {
        int rects = ParseRectsFromGroup(tmx, buttonNames[g], btnRect, (int)(sizeof(btnRect)/sizeof(btnRect[0])));
        if (rects <= 0) continue;
        char lowerName[BUTTON_NAME_LEN];
        PhaseToLowerCopy(buttonNames[g], lowerName, sizeof(lowerName));
        Color colorUp, colorDown;
        DetermineButtonColors(lowerName, &colorUp, &colorDown);
        const Texture2D* sprite = PhasePickButtonSprite(&buttonSprites, lowerName);
        for (int r = 0; r < rects && buttonCount < MAX_BUTTONS; ++r) {
            ButtonInit(&buttons[buttonCount].button, btnRect[r].x, btnRect[r].y, btnRect[r].width, btnRect[r].height, colorUp, colorDown);
            if (sprite) ButtonSetSprites(&buttons[buttonCount].button, sprite, NULL);
            strncpy(buttons[buttonCount].name, buttonNames[g], BUTTON_NAME_LEN - 1);
            buttons[buttonCount].name[BUTTON_NAME_LEN - 1] = '\0';
            PhaseToLowerCopy(buttonNames[g], buttons[buttonCount].nameLower, BUTTON_NAME_LEN);
            buttons[buttonCount].pressed = false;
            buttonCount++;
        }
    }

    // Plataformas
    PhasePlatform elev={0}, barraM={0}, elev2={0}, barra3={0};
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
        if (c1>0 && c2>0) PhasePlatformInit(&elev, r1[0], r2[0], 2.5f);
        c1 = ParseRectsFromGroup(tmx, "Barra_Elevador2_Colisao", r1, 2);
        c2 = ParseRectsFromGroup(tmx, "Barra_Elevador2", r2, 2);
        if (c2==0) c2 = ParseRectsFromGroup(tmx, "Barrra_Elevador2", r2, 2);
        if (c1>0 && c2>0) PhasePlatformInit(&elev2, r1[0], r2[0], 2.5f);
        c1 = ParseRectsFromGroup(tmx, "Barra3", r1, 2);
        c2 = ParseRectsFromGroup(tmx, "Area_Barra3", r2, 2);
        if (c1>0 && c2>0) {
            PhasePlatformInit(&barra3, r1[0], r2[0], 2.0f);
            barra3.rect.y = barra3.area.y;
            barra3.startY = barra3.rect.y;
        }
        c1 = ParseRectsFromGroup(tmx, "Barra1_Movel", r1, 2);
        c2 = ParseRectsFromGroup(tmx, "AreaMovimentoBarraMovel", r2, 2);
        if (c1>0 && c2>0) PhasePlatformInit(&barraM, r1[0], r2[0], 2.0f);
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
    for (int i=0;i<fans1Count;i++) fan1Draw[i] = PhaseAcquireSpriteForRect(fans1[i].rect, fanSpriteRects, fanSpriteUsed, fanSpriteCount);
    for (int i=0;i<fans3Count;i++) fan3Draw[i] = PhaseAcquireSpriteForRect(fans3[i].rect, fanSpriteRects, fanSpriteUsed, fanSpriteCount);
    if (haveVent2) vent2DrawRect = PhaseAcquireSpriteForRect(vent2.rect, fanSpriteRects, fanSpriteUsed, fanSpriteCount);

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

    // C�mera
    Camera2D cam = {0}; cam.target=(Vector2){mapTex.width/2.0f,mapTex.height/2.0f}; cam.offset=(Vector2){GetScreenWidth()/2.0f,GetScreenHeight()/2.0f}; cam.zoom=1.0f;

    bool completed=false; float elapsed=0.0f; bool debug=false;
    bool earthAtDoor=false, fireAtDoor=false, waterAtDoor=false;
    bool tempoButtonLatched=false; float tempoButtonTimer=0.0f; float barra3Timer=0.0f;
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        Theme_Update();
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

        float elevDY = 0.0f, elev2DY = 0.0f, barra3DY = 0.0f, barraDY = 0.0f;
        if (elev.area.height>0 && elev.rect.height>0) {
            float elevTarget = (p1 && !p2 && !p3) ? PhasePlatformBottomTarget(&elev) : elev.startY;
            elevDY = PhasePlatformMoveTowards(&elev, elevTarget);
        }
        if (elev2.area.height>0 && elev2.rect.height>0) {
            float elev2Target = elev2Active ? PhasePlatformBottomTarget(&elev2) : elev2.startY;
            elev2DY = PhasePlatformMoveTowards(&elev2, elev2Target);
        }
        if (barra3.area.height>0 && barra3.rect.height>0) {
            float barra3Target = (barra3Timer > 0.0f) ? PhasePlatformBottomTarget(&barra3) : barra3.startY;
            barra3DY = PhasePlatformMoveTowards(&barra3, barra3Target);
        }
        if (barraM.area.height>0 && barraM.rect.height>0) {
            int pressed=(p1?1:0)+(p2?1:0)+(p3?1:0);
            float barraTarget = (pressed>=2) ? barraM.area.y : barraM.startY;
            barraDY = PhasePlatformMoveTowards(&barraM, barraTarget);
        }

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
        PhaseResolvePlayersVsWorld(P, 3, col, nCol, PHASE_STEP_HEIGHT);
        Colisao movingCols[3];
        int movingCount = 0;
        if (elev2.rect.width > 0 && elev2.rect.height > 0) movingCols[movingCount++].rect = elev2.rect;
        if (barra3.rect.width > 0 && barra3.rect.height > 0) movingCols[movingCount++].rect = barra3.rect;
        if (barraM.rect.width > 0 && barraM.rect.height > 0) movingCols[movingCount++].rect = barraM.rect;
        if (movingCount > 0) PhaseResolvePlayersVsWorld(P, 3, movingCols, movingCount, PHASE_STEP_HEIGHT);

        for (int p=0;p<3;p++) {
            Player* pl=P[p];
            if (elev.rect.width>0) PhaseHandlePlatformTop(pl, elev.rect, elevDY);
            if (elev2.rect.width>0) PhaseHandlePlatformTop(pl, elev2.rect, elev2DY);
            if (barra3.rect.width>0) PhaseHandlePlatformTop(pl, barra3.rect, barra3DY);
            if (barraM.rect.width>0) PhaseHandlePlatformTop(pl, barraM.rect, barraDY);
        }

        // Ventiladores: 1 e 3 sempre aplicam, 2 s� se somente b3
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
            Player* pl=P[p];
            LakeType elem=(p==0)?LAKE_EARTH:((p==1)?LAKE_FIRE:LAKE_WATER);
            for (int i=0;i<segCount;i++) {
                Lake l; l.rect=segs[i].rect; l.type=segs[i].type; l.color=(Color){0};
                if (LakeHandlePlayer(&l, pl, elem)) {
                    if (p==0){pl->rect.x=spawnE.x;pl->rect.y=spawnE.y;}
                    else if (p==1){pl->rect.x=spawnF.x;pl->rect.y=spawnF.y;}
                    else {pl->rect.x=spawnW.x;pl->rect.y=spawnW.y;}
                    pl->velocity=(Vector2){0,0};
                    pl->isJumping=false;
                    break;
                }
            }
        }

        earthAtDoor = PlayerAtDoor(&earthboy, doorTerra);
        fireAtDoor  = PlayerAtDoor(&fireboy,  doorFogo);
        waterAtDoor = PlayerAtDoor(&watergirl, doorAgua);
        bool finishedByDoors = earthAtDoor && fireAtDoor && waterAtDoor;

        // Desenho
        BeginDrawing(); ClearBackground(BLACK); BeginMode2D(cam);
        DrawTexture(mapTex, 0, 0, WHITE);
        for (int i = 0; i < buttonCount; ++i) {
            ButtonDraw(&buttons[i].button);
        }

        bool insideOwn[3] = { false, false, false };
        for (int ip = 0; ip < 3; ++ip) {
            LakeType elem = (ip==0)?LAKE_EARTH:((ip==1)?LAKE_FIRE:LAKE_WATER);
            insideOwn[ip] = PhasePlayerInsideOwnLake(P[ip], elem, segs, segCount);
        }

        LakeAnimFrames* sets[4] = { &animAgua, &animFogo, &animTerra, &animAcido };
        PhaseUpdateLakeAnimations(sets, 4, frameDt, 0.12f);

        for (int i=0;i<fans1Count;i++) DrawFanSprite(fan1Draw[i], true, fanOnFrames, fanOnCount, fanOffTex, fanAnimFrame);
        for (int i=0;i<fans3Count;i++) DrawFanSprite(fan3Draw[i], true, fanOnFrames, fanOnCount, fanOffTex, fanAnimFrame);
        if (haveVent2) DrawFanSprite(vent2DrawRect, fan2Active, fanOnFrames, fanOnCount, fanOffTex, fan2AnimFrame);

        if (insideOwn[0]) DrawPlayer(earthboy);
        if (insideOwn[1]) DrawPlayer(fireboy);
        if (insideOwn[2]) DrawPlayer(watergirl);

        for (int i=0;i<segCount;i++) {
            const LakeSegment* seg=&segs[i];
            const LakeAnimFrames* anim=NULL;
            if (seg->type==LAKE_WATER) anim=&animAgua;
            else if (seg->type==LAKE_FIRE) anim=&animFogo;
            else if (seg->type==LAKE_EARTH) anim=&animTerra;
            else if (seg->type==LAKE_POISON) anim=&animAcido;

            const Texture2D* framePtr = anim ? PhasePickLakeFrame(anim, seg->part) : NULL;
            if (framePtr && framePtr->id != 0) {
                const Texture2D frame = *framePtr;
                if (seg->part==PART_MIDDLE) {
                    float tile = seg->rect.height;
                    int tiles = (int)floorf(seg->rect.width / tile);
                    float x = seg->rect.x;
                    for (int t=0;t<tiles;++t) {
                        Rectangle dst={x,seg->rect.y,tile,seg->rect.height};
                        DrawTexturePro(frame,(Rectangle){0,0,(float)frame.width,(float)frame.height},dst,(Vector2){0,0},0.0f,WHITE);
                        x+=tile;
                    }
                    float rest = seg->rect.width - tiles*tile;
                    if (rest>0.1f) {
                        Rectangle dst={x,seg->rect.y,rest,seg->rect.height};
                        DrawTexturePro(frame,(Rectangle){0,0,(float)frame.width,(float)frame.height},dst,(Vector2){0,0},0.0f,WHITE);
                    }
                } else {
                    DrawTexturePro(frame,(Rectangle){0,0,(float)frame.width,(float)frame.height},seg->rect,(Vector2){0,0},0.0f,WHITE);
                }
            } else {
                Color c = (seg->type==LAKE_POISON)? (Color){60,180,60,230} : (Color){90,90,90,200};
                DrawRectangleRec(seg->rect, c);
            }
        }

        if (elev.rect.width>0 && barraElevTex.id) DrawTexturePro(barraElevTex,(Rectangle){0,0,(float)barraElevTex.width,(float)barraElevTex.height},elev.rect,(Vector2){0,0},0.0f,WHITE);
        if (elev2.rect.width>0 && barraElevTex.id) DrawTexturePro(barraElevTex,(Rectangle){0,0,(float)barraElevTex.width,(float)barraElevTex.height},elev2.rect,(Vector2){0,0},0.0f,WHITE);
        if (barraM.rect.width>0 && barraMovTex.id) DrawTexturePro(barraMovTex,(Rectangle){0,0,(float)barraMovTex.width,(float)barraMovTex.height},barraM.rect,(Vector2){0,0},0.0f,WHITE);
        if (barra3.rect.width>0 && barraMovTex.id) DrawTexturePro(barraMovTex,(Rectangle){0,0,(float)barraMovTex.width,(float)barraMovTex.height},barra3.rect,(Vector2){0,0},0.0f,WHITE);
        for (int b=0; b<coopBoxCount; ++b) {
            if (coopBoxes[b].rect.width <= 0 || coopBoxes[b].rect.height <= 0) continue;
            if (coopBoxTex.id) DrawTexturePro(coopBoxTex,(Rectangle){0,0,(float)coopBoxTex.width,(float)coopBoxTex.height},coopBoxes[b].rect,(Vector2){0,0},0.0f,WHITE);
            else DrawRectangleRec(coopBoxes[b].rect, DARKBROWN);
        }

        if (!insideOwn[0]) DrawPlayer(earthboy);
        if (!insideOwn[1]) DrawPlayer(fireboy);
        if (!insideOwn[2]) DrawPlayer(watergirl);

        GoalDraw(&goal);
        bool goalCompleted = GoalReached(&goal, &earthboy, &fireboy, &watergirl);

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
        EndMode2D();

        char timerTxt[32];
        int min = (int)(elapsed / 60.0f);
        float sec = elapsed - min * 60.0f;
        snprintf(timerTxt, sizeof(timerTxt), "%02d:%05.2f", min, sec);
        DrawText(timerTxt, 30, 30, 32, WHITE);

        EndDrawing();

        if (goalCompleted || finishedByDoors) { completed = true; break; }
    }

    UnloadTexture(mapTex); if (barraElevTex.id) UnloadTexture(barraElevTex);
    if (!barraMovShared && barraMovTex.id) UnloadTexture(barraMovTex);
    if (!coopBoxShared && coopBoxTex.id) UnloadTexture(coopBoxTex);
    if (fanOffTex.id) UnloadTexture(fanOffTex);
    for (int i=0;i<fanOnCount;i++) if (fanOnFrames[i].id) UnloadTexture(fanOnFrames[i]);
    PhaseUnloadButtonSprites(&buttonSprites);
    // unload lake frames
    PhaseUnloadLakeSet(&animAgua); PhaseUnloadLakeSet(&animFogo); PhaseUnloadLakeSet(&animTerra); PhaseUnloadLakeSet(&animAcido);
    UnloadPlayer(&earthboy); UnloadPlayer(&fireboy); UnloadPlayer(&watergirl);
    if (completed) Ranking_Add(5, Game_GetPlayerName(), elapsed);
    return completed;
}
