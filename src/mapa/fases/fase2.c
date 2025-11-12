#include "fases.h"
#include "../../player/player.h"
#include "../../objects/box.h"
#include "../../objects/goal.h"
#include "../../objects/seesaw.h"
#include "../../objects/lake.h"
#include "../../objects/button.h"
#include "../../interface/pause.h"
#include "../../game/game.h"
#include "../../ranking/ranking.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

// --- Helpers para ler grupos do Tiled e animar lagos ---
typedef enum { PART_LEFT = 0, PART_MIDDLE, PART_RIGHT } LakePart;

#define MAX_LAKE_SEGS 256
typedef struct LakeSegment { Rectangle rect; LakeType type; LakePart part; } LakeSegment;

// Colisões de cenário
#define MAX_COLISOES 1024
#define STEP_HEIGHT 14.0f
typedef struct { Rectangle rect; } Colisao;

typedef struct LakeAnimFrames {
    Texture2D left[16]; int leftCount;
    Texture2D middle[16]; int middleCount;
    Texture2D right[16]; int rightCount;
    float timer; int frame;
} LakeAnimFrames;

// Ventiladores (vento invisível)
#define MAX_FANS 64
typedef struct FanZone { Rectangle rect; float strength; } FanZone;

// Barra móvel controlada por dois botões
typedef struct MovableBar {
    Rectangle rect;
    float startY;
    float targetY;
    float speed; // px/s
} MovableBar;


static int LoadFramesRange(Texture2D* arr, int max, const char* pattern, int startIdx, int endIdx) {
    // Carrega frames sequenciais, verificando existência para evitar WARNINGs; para no primeiro buraco
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
    s->leftCount   = LoadFramesRange(s->left,   16, "assets/map/agua/esquerdo/pixil-frame-%d.png", 0, 15);
    s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/agua/meio/pixil-frame-%d.png",     0, 15);
    s->rightCount  = LoadFramesRange(s->right,  16, "assets/map/agua/direito/pixil-frame-%d.png",  0, 15);
    s->timer = 0.0f; s->frame = 0;
}
static void LoadLakeSet_Terra(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   16, "assets/map/terra/esquerdo/pixil-frame-%d.png", 0, 15);
    s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/terra/meio/pixil-frame-%d.png",     0, 15);
    s->rightCount  = LoadFramesRange(s->right,  16, "assets/map/terra/direito/pixil-frame-%d.png",  0, 15);
    s->timer = 0.0f; s->frame = 0;
}
static void LoadLakeSet_Fogo(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   16, "assets/map/fogo/esquerdo/Esquerda%d.png", 1, 32);
    if (s->leftCount == 0)
        s->leftCount = LoadFramesRange(s->left, 16, "assets/map/fogo/esquerdo/pixil-frame-%d.png", 0, 15);
    s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/fogo/meio/Meio%d.png", 1, 32);
    if (s->middleCount == 0)
        s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/fogo/meio/pixil-frame-%d.png", 0, 15);
    s->rightCount  = LoadFramesRange(s->right,  16, "assets/map/fogo/direito/Direita%d.png", 1, 32);
    if (s->rightCount == 0)
        s->rightCount = LoadFramesRange(s->right, 16, "assets/map/fogo/direito/pixil-frame-%d.png", 0, 15);
    s->timer = 0.0f; s->frame = 0;
}
static void UnloadLakeSet(LakeAnimFrames* s) {
    for (int i = 0; i < s->leftCount;   ++i) UnloadTexture(s->left[i]);
    for (int i = 0; i < s->middleCount; ++i) UnloadTexture(s->middle[i]);
    for (int i = 0; i < s->rightCount;  ++i) UnloadTexture(s->right[i]);
    s->leftCount = s->middleCount = s->rightCount = 0;
}

static void LoadLakeSet_Acido(LakeAnimFrames* s) {
    s->leftCount   = LoadFramesRange(s->left,   16, "assets/map/acido/esquerdo/pixil-frame-%d.png", 0, 15);
    s->middleCount = LoadFramesRange(s->middle, 16, "assets/map/acido/meio/pixil-frame-%d.png",     0, 15);
    s->rightCount  = LoadFramesRange(s->right,  16, "assets/map/acido/direito/pixil-frame-%d.png",  0, 15);
    s->timer = 0.0f; s->frame = 0;
}

/* Ventilador animado removido temporariamente */

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
            memcpy(header, search, len); header[len] = '\0';
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

bool Fase2(void) {
    // Carrega mapa de fundo exportado do Tiled
    Texture2D mapTexture = LoadTexture("assets/maps/Fase2Mapa.png");
    if (mapTexture.id == 0) {
        TraceLog(LOG_WARNING, "Falha ao carregar assets/maps/Fase2Mapa.png");
    }

    // Inicializa jogadores
    Player fireboy, watergirl, earthboy;
    InitFireboy(&fireboy);
    InitWatergirl(&watergirl);
    InitEarthboy(&earthboy);
    // Ajusta tamanhos para combinar com a Fase 1 (45x50)
    fireboy.rect.width = 45;  fireboy.rect.height = 50;
    watergirl.rect.width = 45; watergirl.rect.height = 50;
    earthboy.rect.width = 45; earthboy.rect.height = 50;
    Vector2 spawnFire  = (Vector2){ fireboy.rect.x,  fireboy.rect.y };
    Vector2 spawnWater = (Vector2){ watergirl.rect.x, watergirl.rect.y };
    Vector2 spawnEarth = (Vector2){ earthboy.rect.x, earthboy.rect.y };

    // Chao de fallback (sem camada Colisao no TMX por enquanto)
    Rectangle ground = { 0, mapTexture.id ? (float)mapTexture.height : 800.0f,
                         mapTexture.id ? (float)mapTexture.width  : 1920.0f, 200.0f };

    // Objetos existentes na fase
    Box box; BoxInit(&box, ground, 900, 80, 80);
    Goal goal; GoalInit(&goal, ground.width - 100, ground.y - 120, 40, 120, GREEN);
    SeesawPlatform seesaw; SeesawInit(&seesaw, (Vector2){ 700, 650 }, 300.0f, 20.0f, 0.0f);

    // Lagos do TMX
    LakeSegment lakeSegs[MAX_LAKE_SEGS]; int lakeCount = 0; Rectangle tmpRects[128]; int n;
    const char* tmxPath = "assets/maps/Fase2.tmx";
    // Fogo
    n = ParseRectsFromGroup(tmxPath, "Lago_Fogo_Esquerda", tmpRects, 128);  for (int i=0;i<n && lakeCount<MAX_LAKE_SEGS;i++) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_FIRE, PART_LEFT };
    n = ParseRectsFromGroup(tmxPath, "Lago_Fogo_Meio", tmpRects, 128);      for (int i=0;i<n && lakeCount<MAX_LAKE_SEGS;i++) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_FIRE, PART_MIDDLE };
    n = ParseRectsFromGroup(tmxPath, "Lago_Fogo_Direita", tmpRects, 128);   for (int i=0;i<n && lakeCount<MAX_LAKE_SEGS;i++) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_FIRE, PART_RIGHT };
    // Agua (suporta grafia Direita/Diretira)
    n = ParseRectsFromGroup(tmxPath, "Lago_Agua_Esquerda", tmpRects, 128);  for (int i=0;i<n && lakeCount<MAX_LAKE_SEGS;i++) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_WATER, PART_LEFT };
    n = ParseRectsFromGroup(tmxPath, "Lago_Agua_Meio", tmpRects, 128);      for (int i=0;i<n && lakeCount<MAX_LAKE_SEGS;i++) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_WATER, PART_MIDDLE };
    n = ParseRectsFromGroup(tmxPath, "Lago_Agua_Direita", tmpRects, 128);   for (int i=0;i<n && lakeCount<MAX_LAKE_SEGS;i++) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_WATER, PART_RIGHT };
    if (n == 0) { // erro de digitação observado no TMX
        n = ParseRectsFromGroup(tmxPath, "Lago_Agua_Diretira", tmpRects, 128);
        for (int i=0;i<n && lakeCount<MAX_LAKE_SEGS;i++) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_WATER, PART_RIGHT };
    }
    // Terra (marrom)
    n = ParseRectsFromGroup(tmxPath, "Lago_Marrom_Esquerda", tmpRects, 128); for (int i=0;i<n && lakeCount<MAX_LAKE_SEGS;i++) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_EARTH, PART_LEFT };
    n = ParseRectsFromGroup(tmxPath, "Lago_Marrom_Meio", tmpRects, 128);     for (int i=0;i<n && lakeCount<MAX_LAKE_SEGS;i++) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_EARTH, PART_MIDDLE };
    n = ParseRectsFromGroup(tmxPath, "Lago_Marrom_Direita", tmpRects, 128);  for (int i=0;i<n && lakeCount<MAX_LAKE_SEGS;i++) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_EARTH, PART_RIGHT };
    // Veneno (verde)
    n = ParseRectsFromGroup(tmxPath, "Lago_Verde_Esquerda", tmpRects, 128);  for (int i=0;i<n && lakeCount<MAX_LAKE_SEGS;i++) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_POISON, PART_LEFT };
    n = ParseRectsFromGroup(tmxPath, "Lago_Verde_Meio", tmpRects, 128);      for (int i=0;i<n && lakeCount<MAX_LAKE_SEGS;i++) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_POISON, PART_MIDDLE };
    n = ParseRectsFromGroup(tmxPath, "Lago_Verde_Direita", tmpRects, 128);   for (int i=0;i<n && lakeCount<MAX_LAKE_SEGS;i++) lakeSegs[lakeCount++] = (LakeSegment){ tmpRects[i], LAKE_POISON, PART_RIGHT };

    // Animações
    LakeAnimFrames animAgua={0}, animFogo={0}, animTerra={0}, animAcido={0};
    LoadLakeSet_Agua(&animAgua); LoadLakeSet_Fogo(&animFogo); LoadLakeSet_Terra(&animTerra); LoadLakeSet_Acido(&animAcido);

    // Ventiladores a partir do TMX (grupos: Ventilador, Vento, Fan, Wind)
    FanZone fans[MAX_FANS]; int fanCount = 0;
    const float defaultStrength = 0.6f; // aceleração moderada para cima
    const char* fanGroups[] = { "Ventilador", "Vento", "Fan", "Wind" };
    for (int gi = 0; gi < 4 && fanCount < MAX_FANS; ++gi) {
        Rectangle fRects[128];
        int fn = ParseRectsFromGroup(tmxPath, fanGroups[gi], fRects, 128);
        for (int i = 0; i < fn && fanCount < MAX_FANS; ++i) {
            fans[fanCount].rect = fRects[i];
            fans[fanCount].strength = defaultStrength;
            fanCount++;
        }
    }

    // Botões e barra móvel (Barra1_Movel / Barra_Elevador1)
    Button btn1 = {0}, btn2 = {0}; bool hasBtn1=false, hasBtn2=false;
    Rectangle rBtn[4]; int nb;
    nb = ParseRectsFromGroup(tmxPath, "Botao1_Barra1_Movel", rBtn, 4);
    if (nb==0) nb = ParseRectsFromGroup(tmxPath, "Botao1_Barra1_Morvel", rBtn, 4);
    if (nb>0) { ButtonInit(&btn1, rBtn[0].x, rBtn[0].y, rBtn[0].width, rBtn[0].height, (Color){100,100,100,255}, (Color){60,200,80,255}); hasBtn1=true; }
    nb = ParseRectsFromGroup(tmxPath, "Botao2_Barra1_Movel", rBtn, 4);
    if (nb==0) nb = ParseRectsFromGroup(tmxPath, "Botao2_Barra1_Morvel", rBtn, 4);
    if (nb>0) { ButtonInit(&btn2, rBtn[0].x, rBtn[0].y, rBtn[0].width, rBtn[0].height, (Color){100,100,100,255}, (Color){60,200,80,255}); hasBtn2=true; }

    MovableBar bar1 = {0}; bool hasBar1=false; bool bar1Collides=true; Rectangle rBar[4]; Texture2D barTex = (Texture2D){0};
    int nbElev = ParseRectsFromGroup(tmxPath, "Barra_Elevador1", rBar, 4);
    int nbMov  = ParseRectsFromGroup(tmxPath, "Barra1_Movel",   rBar, 4);
    if (nbElev > 0) {
        bar1.rect = rBar[0]; bar1.startY = rBar[0].y; bar1.targetY = rBar[0].y - rBar[0].height; bar1.speed = 120.0f;
        hasBar1=true; bar1Collides=false; // elevador sem colisão
    } else if (nbMov > 0) {
        bar1.rect = rBar[0]; bar1.startY = rBar[0].y; bar1.targetY = rBar[0].y - rBar[0].height; bar1.speed = 120.0f;
        hasBar1=true; bar1Collides=true;  // barra móvel com colisão
    }
    if (hasBar1) {
        barTex = LoadTexture("assets/map/barras/branca.png");
    }

    // Colisões do cenário (camada de objetos "Colisao" no Fase2.tmx)
    Colisao colisoes[MAX_COLISOES]; int totalColisoes = 0;
    {
        Rectangle rects[MAX_COLISOES];
        int nC = ParseRectsFromGroup(tmxPath, "Colisao", rects, MAX_COLISOES);
        for (int i = 0; i < nC && totalColisoes < MAX_COLISOES; ++i) colisoes[totalColisoes++].rect = rects[i];
    }

    bool completed = false; float elapsed = 0.0f; bool debug = false;
    while (!WindowShouldClose()) {
        float dtFrame = GetFrameTime();
        elapsed += dtFrame;
        if (IsKeyPressed(KEY_TAB)) debug = !debug;
        // Atualiza jogadores com o chão base
        UpdatePlayer(&fireboy,  ground, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&watergirl,ground, KEY_A,    KEY_D,     KEY_W);
        UpdatePlayer(&earthboy, ground, KEY_H,    KEY_K,     KEY_U);

        // Vento invisível: aplica ANTES da colisão com blocos para permitir levantar mesmo parado
        for (int ip = 0; ip < 3; ++ip) {
            Player* pl = (ip==0? &earthboy : (ip==1? &fireboy : &watergirl));
            for (int i = 0; i < fanCount; ++i) {
                if (CheckCollisionRecs(pl->rect, fans[i].rect)) {
                    float feet = pl->rect.y + pl->rect.height;
                    float t = (feet - fans[i].rect.y) / fans[i].rect.height; // 0=topo,1=base
                    if (t < 0) t = 0; else if (t > 1) t = 1;
                    float s = 0.5f + 0.5f * (1.0f - fabsf(2.0f * t - 1.0f)); // 0.5 bordas, 1.0 centro
                    // Se estiver caindo (velocidade positiva), garante impulso suficiente ao entrar
                    if (pl->velocity.y > 0.0f) {
                        if (s < 0.9f) s = 0.9f;     // força mínima ao entrar no topo
                        pl->velocity.y -= 0.2f;     // impulso extra constante para vencer a gravidade
                    }
                    pl->velocity.y -= fans[i].strength * s;
                    if (pl->velocity.y < -8.0f) pl->velocity.y = -8.0f;
                }
            }
        }

        // Atualiza botões e movimento da barra (antes das colisões, para refletir posição)
        bool b1 = hasBtn1 ? ButtonUpdate(&btn1, &fireboy, &watergirl, &earthboy) : false;
        bool b2 = hasBtn2 ? ButtonUpdate(&btn2, &fireboy, &watergirl, &earthboy) : false;
        bool bothPressed = (hasBtn1 && hasBtn2) ? (b1 && b2) : false;
        if (hasBar1) {
            if (bothPressed) {
                // Sobe até o alvo
                bar1.rect.y -= bar1.speed * dtFrame;
                if (bar1.rect.y < bar1.targetY) bar1.rect.y = bar1.targetY;
            }
        }

        // Colisão com blocos da camada "Colisao" (com degrau)
        Player* pArr[3] = { &earthboy, &fireboy, &watergirl };
        for (int pi = 0; pi < 3; ++pi) {
            Player* pl = pArr[pi];
            for (int i = 0; i < totalColisoes; ++i) {
                Rectangle bloco = colisoes[i].rect;
                if (!CheckCollisionRecs(pl->rect, bloco)) continue;

                float dx = (pl->rect.x + pl->rect.width * 0.5f) - (bloco.x + bloco.width * 0.5f);
                float dy = (pl->rect.y + pl->rect.height* 0.5f) - (bloco.y + bloco.height*0.5f);
                float overlapX = (pl->rect.width *0.5f + bloco.width *0.5f) - fabsf(dx);
                float overlapY = (pl->rect.height*0.5f + bloco.height*0.5f) - fabsf(dy);

                if (overlapX < overlapY) {
                    // tentativa de subir degrau
                    if (!(dy > 0 && pl->velocity.y > 0)) {
                        Rectangle teste = pl->rect; teste.y -= STEP_HEIGHT;
                        if (!CheckCollisionRecs(teste, bloco)) { pl->rect.y -= STEP_HEIGHT; continue; }
                    }
                    if (dx > 0) pl->rect.x += overlapX; else pl->rect.x -= overlapX;
                    pl->velocity.x = 0;
                } else {
                    if (dy > 0 && pl->velocity.y < 0) { pl->rect.y += overlapY; pl->velocity.y = 0; }
                    else if (dy < 0 && pl->velocity.y >= 0) { pl->rect.y -= overlapY; pl->velocity.y = 0; pl->isJumping = false; }
                }
            }
            // Colisão com a barra (apenas se configurada como colidível)
            if (hasBar1 && bar1Collides) {
                Rectangle bloco = bar1.rect;
                if (CheckCollisionRecs(pl->rect, bloco)) {
                    float dx = (pl->rect.x + pl->rect.width * 0.5f) - (bloco.x + bloco.width * 0.5f);
                    float dy = (pl->rect.y + pl->rect.height* 0.5f) - (bloco.y + bloco.height*0.5f);
                    float overlapX = (pl->rect.width *0.5f + bloco.width *0.5f) - fabsf(dx);
                    float overlapY = (pl->rect.height*0.5f + bloco.height*0.5f) - fabsf(dy);
                    if (overlapX < overlapY) {
                        if (!(dy > 0 && pl->velocity.y > 0)) {
                            Rectangle teste = pl->rect; teste.y -= STEP_HEIGHT;
                            if (!CheckCollisionRecs(teste, bloco)) { pl->rect.y -= STEP_HEIGHT; }
                            else {
                                if (dx > 0) pl->rect.x += overlapX; else pl->rect.x -= overlapX; pl->velocity.x = 0;
                            }
                        }
                    } else {
                        if (dy > 0 && pl->velocity.y < 0) { pl->rect.y += overlapY; pl->velocity.y = 0; }
                        else if (dy < 0 && pl->velocity.y >= 0) { pl->rect.y -= overlapY; pl->velocity.y = 0; pl->isJumping = false; }
                    }
                }
            }
        }

        // Interações com objetos
        BoxHandlePush(&box, &fireboy, KEY_LEFT, KEY_RIGHT);
        BoxHandlePush(&box, &watergirl, KEY_A, KEY_D);
        BoxHandlePush(&box, &earthboy, KEY_H, KEY_K);
        BoxUpdate(&box, ground);

        // Seesaw
        SeesawBeginFrame(&seesaw);
        SeesawHandlePlayer(&seesaw, &fireboy);
        SeesawHandlePlayer(&seesaw, &watergirl);
        SeesawHandlePlayer(&seesaw, &earthboy);
        SeesawUpdate(&seesaw);

        // Lagos: morte/respawn com tolerância de profundidade (implementado em LakeHandlePlayer)
        Player* playersArr[3] = { &earthboy, &fireboy, &watergirl };
        LakeType elemsArr[3]  = { LAKE_EARTH, LAKE_FIRE, LAKE_WATER };
        for (int ip = 0; ip < 3; ++ip) {
            Player* pl = playersArr[ip]; LakeType elem = elemsArr[ip];
            for (int i = 0; i < lakeCount; ++i) {
                Lake l; l.rect = lakeSegs[i].rect; l.type = lakeSegs[i].type; l.color=(Color){0};
                if (LakeHandlePlayer(&l, pl, elem)) {
                    if (ip==0) { pl->rect.x=spawnEarth.x; pl->rect.y=spawnEarth.y; }
                    else if (ip==1) { pl->rect.x=spawnFire.x; pl->rect.y=spawnFire.y; }
                    else { pl->rect.x=spawnWater.x; pl->rect.y=spawnWater.y; }
                    pl->velocity=(Vector2){0,0}; pl->isJumping=false; break;
                }
            }
        }

        // (vento já aplicado antes das colisões)

        // Desenho
        BeginDrawing();
        ClearBackground(BLACK);

        if (mapTexture.id != 0) DrawTexture(mapTexture, 0, 0, WHITE);

        // Quem fica por tras do lago correto?
        bool insideOwn[3] = { false, false, false };
        for (int ip = 0; ip < 3; ++ip) {
            Player* pl = playersArr[ip]; LakeType elem = elemsArr[ip];
            for (int i = 0; i < lakeCount; ++i) {
                const LakeSegment* seg = &lakeSegs[i]; if (seg->type != elem) continue;
                if (!CheckCollisionRecs(pl->rect, seg->rect)) continue;
                Rectangle ov = GetCollisionRec(pl->rect, seg->rect);
                float pBottom = pl->rect.y + pl->rect.height;
                if (pBottom > seg->rect.y + 2 && ov.height >= 4) { insideOwn[ip] = true; break; }
            }
        }

        // 1) jogadores por trás
        if (insideOwn[0]) DrawPlayer(earthboy);
        if (insideOwn[1]) DrawPlayer(fireboy);
        if (insideOwn[2]) DrawPlayer(watergirl);

        // avança frames dos lagos (8 FPS)
        float dt = GetFrameTime(); LakeAnimFrames* sets[4] = { &animAgua, &animFogo, &animTerra, &animAcido };
        for (int s = 0; s < 4; ++s) { sets[s]->timer += dt; if (sets[s]->timer >= 0.12f) { sets[s]->timer = 0.0f; sets[s]->frame = (sets[s]->frame + 1) % 64; } }

        // 2) desenha lagos por cima
        for (int i = 0; i < lakeCount; ++i) {
            const LakeSegment* seg = &lakeSegs[i];
            const LakeAnimFrames* anim = NULL; if (seg->type==LAKE_WATER) anim=&animAgua; else if (seg->type==LAKE_FIRE) anim=&animFogo; else if (seg->type==LAKE_EARTH) anim=&animTerra; else if (seg->type==LAKE_POISON) anim=&animAcido;
            if (anim && ((seg->part==PART_LEFT && anim->leftCount>0) || (seg->part==PART_MIDDLE && anim->middleCount>0) || (seg->part==PART_RIGHT && anim->rightCount>0))) {
                Texture2D frame; int idx;
                if (seg->part==PART_LEFT)   { idx = (anim->frame % anim->leftCount);   frame = anim->left[idx]; }
                else if (seg->part==PART_MIDDLE){ idx = (anim->frame % anim->middleCount); frame = anim->middle[idx]; }
                else                        { idx = (anim->frame % anim->rightCount);  frame = anim->right[idx]; }
                if (seg->part == PART_MIDDLE) {
                    float tile = seg->rect.height; int tiles = (int)floorf(seg->rect.width / tile); float x = seg->rect.x;
                    for (int t=0;t<tiles;++t) { Rectangle dst={x,seg->rect.y,tile,seg->rect.height}; DrawTexturePro(frame,(Rectangle){0,0,(float)frame.width,(float)frame.height},dst,(Vector2){0,0},0.0f,WHITE); x += tile; }
                    float rest = seg->rect.width - tiles*tile; if (rest > 0.1f) { Rectangle dst={x,seg->rect.y,rest,seg->rect.height}; DrawTexturePro(frame,(Rectangle){0,0,(float)frame.width,(float)frame.height},dst,(Vector2){0,0},0.0f,WHITE); }
                } else {
                    DrawTexturePro(frame,(Rectangle){0,0,(float)frame.width,(float)frame.height},seg->rect,(Vector2){0,0},0.0f,WHITE);
                }
            } else {
                Color c = (seg->type==LAKE_POISON)? (Color){60,180,60,230} : (Color){90,90,90,200};
                DrawRectangleRec(seg->rect, c);
            }
        }

        // 4) demais jogadores por cima
        if (!insideOwn[0]) DrawPlayer(earthboy);
        if (!insideOwn[1]) DrawPlayer(fireboy);
        if (!insideOwn[2]) DrawPlayer(watergirl);

        // Objetos visuais base
        DrawRectangleRec(ground, (Color){110,90,70,255});
        if (hasBar1) {
            if (barTex.id != 0) {
                // Desenha a barra com a textura branca esticada para o retângulo da barra
                DrawTexturePro(barTex, (Rectangle){0,0,(float)barTex.width,(float)barTex.height}, bar1.rect, (Vector2){0,0}, 0.0f, WHITE);
            } else {
                DrawRectangleRec(bar1.rect, (Color){160,160,180,255});
            }
        }
        if (hasBtn1) ButtonDraw(&btn1);
        if (hasBtn2) ButtonDraw(&btn2);
        BoxDraw(&box); SeesawDraw(&seesaw); GoalDraw(&goal);

        // Chegada
        if (GoalReached(&goal, &fireboy, &watergirl, &earthboy)) { completed = true; EndDrawing(); break; }

        // HUD
        char tbuf[32]; int min = (int)(elapsed/60.0f); float sec = elapsed - min*60.0f; sprintf(tbuf, "%02d:%05.2f", min, sec);
        DrawText(tbuf, 20, 20, 30, WHITE);
        if (debug) {
            // desenha colisões
            for (int i = 0; i < totalColisoes; ++i) DrawRectangleLinesEx(colisoes[i].rect, 1, Fade(GREEN, 0.5f));
            // desenha regiões de vento
            for (int i = 0; i < fanCount; ++i) DrawRectangleLinesEx(fans[i].rect, 1, Fade(SKYBLUE, 0.7f));
            if (hasBar1) DrawRectangleLinesEx(bar1.rect, 1, Fade(WHITE, 0.8f));
            if (hasBtn1) DrawRectangleLinesEx(btn1.rect, 1, Fade(YELLOW, 0.8f));
            if (hasBtn2) DrawRectangleLinesEx(btn2.rect, 1, Fade(ORANGE, 0.8f));
            DrawText("TAB - Debug colisao", 20, 60, 20, (Color){200,200,200,255});
            DrawFPS(20, 84);
        }

        // Pause
        if (IsKeyPressed(KEY_ESCAPE)) {
            PauseResult pr = ShowPauseMenu();
            if (pr == PAUSE_TO_MAP) { completed = false; EndDrawing(); break; }
            if (pr == PAUSE_TO_MENU) { Game_SetReturnToMenu(true); completed = false; EndDrawing(); break; }
        }

        EndDrawing();
    }

    UnloadPlayer(&fireboy);
    UnloadPlayer(&watergirl);
    UnloadPlayer(&earthboy);
    if (mapTexture.id != 0) UnloadTexture(mapTexture);
    if (barTex.id != 0) UnloadTexture(barTex);
    UnloadLakeSet(&animAgua); UnloadLakeSet(&animFogo); UnloadLakeSet(&animTerra); UnloadLakeSet(&animAcido);
    if (completed) Ranking_Add(2, Game_GetPlayerName(), elapsed);
    return completed;
}
