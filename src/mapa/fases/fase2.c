#include "raylib.h"
#include "../../player/player.h"
#include "../../objects/lake.h"
#include "../../objects/goal.h"
#include "../../objects/button.h"
#include "../../objects/fan.h"
#include "../../game/game.h"
#include "../../ranking/ranking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_COLISOES 1024
#define MAX_LAKES    512
#define STEP_HEIGHT  14.0f

typedef struct { Rectangle rect; } Colisao;

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
    int t = 0; for (int i=0;i<nNames && t<cap;i++) t += ParseRectsFromGroup(tmxPath, names[i], out+t, cap-t); return t;
}

// Plataforma simples (barra)
typedef struct Platform { Rectangle rect, area; float startY, speed; } Platform;
static void PlatformInit(Platform* p, Rectangle rect, Rectangle area, float speed) { p->rect=rect; p->area=area; p->startY=rect.y; p->speed=speed; }
static float moveTowards(float a, float b, float s) { if (a<b){a+=s; if(a>b)a=b;} else if(a>b){a-=s; if(a<b)a=b;} return a; }
static void HandlePlatformTop(Player* pl, Rectangle plat, float dY) {
    if (!CheckCollisionRecs(pl->rect, plat)) return; float pBottom = pl->rect.y + pl->rect.height;
    if (pBottom <= plat.y + 16.0f && pl->velocity.y >= -1.0f) { pl->rect.y = plat.y - pl->rect.height; pl->velocity.y = 0; pl->isJumping = false; pl->rect.y += dY; }
}

bool Fase2(void) {
    const char* tmx = "assets/maps/Fase2.tmx";
    Texture2D mapTex = LoadTexture("assets/maps/Fase2Mapa.png"); if (mapTex.id==0) { printf("Erro: mapa Fase2Mapa.png\n"); return false; }

    // Colisões
    Colisao col[MAX_COLISOES]; int nCol = 0; {
        const char* names[] = { "Colisao", "Colisão", "Colisoes", "Colisões" };
        Rectangle r[MAX_COLISOES]; int n = ParseRectsFromAny(tmx, names, 4, r, MAX_COLISOES);
        for (int i=0;i<n && i<MAX_COLISOES;i++) col[nCol++].rect = r[i];
    }

    // Lagos
    Lake lakes[MAX_LAKES]; int nLakes = 0; Rectangle r[128]; int n;
    n = ParseRectsFromGroup(tmx, "Lago_Agua_Esquerdo", r, 128); for (int i=0;i<n && nLakes<MAX_LAKES;i++) LakeInit(&lakes[nLakes++], r[i].x, r[i].y, r[i].width, r[i].height, LAKE_WATER);
    n = ParseRectsFromGroup(tmx, "Lago_Agua_Meio", r, 128);    for (int i=0;i<n && nLakes<MAX_LAKES;i++) LakeInit(&lakes[nLakes++], r[i].x, r[i].y, r[i].width, r[i].height, LAKE_WATER);
    n = ParseRectsFromGroup(tmx, "Lago_Agua_Direito", r, 128); for (int i=0;i<n && nLakes<MAX_LAKES;i++) LakeInit(&lakes[nLakes++], r[i].x, r[i].y, r[i].width, r[i].height, LAKE_WATER);
    n = ParseRectsFromGroup(tmx, "Lago_Fogo_Esquerdo", r, 128); for (int i=0;i<n && nLakes<MAX_LAKES;i++) LakeInit(&lakes[nLakes++], r[i].x, r[i].y, r[i].width, r[i].height, LAKE_FIRE);
    n = ParseRectsFromGroup(tmx, "Lago_Fogo_Meio", r, 128);    for (int i=0;i<n && nLakes<MAX_LAKES;i++) LakeInit(&lakes[nLakes++], r[i].x, r[i].y, r[i].width, r[i].height, LAKE_FIRE);
    n = ParseRectsFromGroup(tmx, "Lago_Fogo_Direito", r, 128); for (int i=0;i<n && nLakes<MAX_LAKES;i++) LakeInit(&lakes[nLakes++], r[i].x, r[i].y, r[i].width, r[i].height, LAKE_FIRE);
    n = ParseRectsFromGroup(tmx, "Lago_Marrom_Esquerdo", r, 128); for (int i=0;i<n && nLakes<MAX_LAKES;i++) LakeInit(&lakes[nLakes++], r[i].x, r[i].y, r[i].width, r[i].height, LAKE_EARTH);
    n = ParseRectsFromGroup(tmx, "Lago_Marrom_Meio", r, 128);    for (int i=0;i<n && nLakes<MAX_LAKES;i++) LakeInit(&lakes[nLakes++], r[i].x, r[i].y, r[i].width, r[i].height, LAKE_EARTH);
    n = ParseRectsFromGroup(tmx, "Lago_Marrom_Direito", r, 128); for (int i=0;i<n && nLakes<MAX_LAKES;i++) LakeInit(&lakes[nLakes++], r[i].x, r[i].y, r[i].width, r[i].height, LAKE_EARTH);
    n = ParseRectsFromGroup(tmx, "Lago_Verde_Esquerda", r, 128); for (int i=0;i<n && nLakes<MAX_LAKES;i++) LakeInit(&lakes[nLakes++], r[i].x, r[i].y, r[i].width, r[i].height, LAKE_POISON);
    n = ParseRectsFromGroup(tmx, "Lago_Verde_Meio", r, 128);    for (int i=0;i<n && nLakes<MAX_LAKES;i++) LakeInit(&lakes[nLakes++], r[i].x, r[i].y, r[i].width, r[i].height, LAKE_POISON);
    n = ParseRectsFromGroup(tmx, "Lago_Verde_Direita", r, 128); for (int i=0;i<n && nLakes<MAX_LAKES;i++) LakeInit(&lakes[nLakes++], r[i].x, r[i].y, r[i].width, r[i].height, LAKE_POISON);

    // Chegada (opcional)
    Rectangle goalR[2]; int gC = ParseRectsFromGroup(tmx, "Chegada", goalR, 2); if (gC==0) gC = ParseRectsFromGroup(tmx, "Goal", goalR, 2);
    Rectangle goalRect = (gC>0) ? goalR[0] : (Rectangle){ mapTex.width-80, mapTex.height-140, 40, 120 }; Goal goal; GoalInit(&goal, goalRect.x, goalRect.y, goalRect.width, goalRect.height, GOLD);

    // Spawns
    Vector2 spawnE={300,700}, spawnF={400,700}, spawnW={500,700};
    { Rectangle s[2]; int c;
      c = ParseRectsFromGroup(tmx, "Spawn_Terra", s, 2); if (c>0) { float cx = s[0].x + (s[0].width-45)*0.5f; spawnE=(Vector2){cx, s[0].y + s[0].height - 50}; }
      c = ParseRectsFromGroup(tmx, "Spawn_Fogo",  s, 2); if (c>0) { float cx = s[0].x + (s[0].width-45)*0.5f; spawnF=(Vector2){cx, s[0].y + s[0].height - 50}; }
      c = ParseRectsFromGroup(tmx, "Spawn_Agua",  s, 2); if (c>0) { float cx = s[0].x + (s[0].width-45)*0.5f; spawnW=(Vector2){cx, s[0].y + s[0].height - 50}; } }

    // Jogadores (tamanhos reduzidos como na Fase1)
    Player earthboy, fireboy, watergirl; InitEarthboy(&earthboy); InitFireboy(&fireboy); InitWatergirl(&watergirl);
    earthboy.rect = (Rectangle){spawnE.x, spawnE.y, 45, 50}; fireboy.rect = (Rectangle){spawnF.x, spawnF.y, 45, 50}; watergirl.rect = (Rectangle){spawnW.x, spawnW.y, 45, 50};

    // Botões
    Button b1={0}, b2={0}, b3={0}; {
        Rectangle br[2]; int c;
        c = ParseRectsFromGroup(tmx, "Botao1_Barra1_Morvel", br, 2); if (c==0) c = ParseRectsFromGroup(tmx, "Botao1_Barra1_Movel", br, 2);
        if (c>0) ButtonInit(&b1, br[0].x, br[0].y, br[0].width, br[0].height, (Color){200,200,40,200}, (Color){200,140,20,255});
        c = ParseRectsFromGroup(tmx, "Botao2_Barra1_Movel", br, 2); if (c==0) c = ParseRectsFromGroup(tmx, "Botao2_Barra1_Morvel", br, 2);
        if (c>0) ButtonInit(&b2, br[0].x, br[0].y, br[0].width, br[0].height, (Color){40,200,200,200}, (Color){20,140,200,255});
        c = ParseRectsFromGroup(tmx, "Botao3_Barra1_Movel_e_ventilador2", br, 2); if (c==0) c = ParseRectsFromGroup(tmx, "Botao3_Barra1_Morvel_e_ventilador2", br, 2);
        if (c>0) ButtonInit(&b3, br[0].x, br[0].y, br[0].width, br[0].height, (Color){200,40,200,200}, (Color){140,20,200,255});
    }

    // Plataformas
    typedef struct Platform Platform; Platform elev={0}, barraM={0};
    Texture2D barraTex = LoadTexture("assets/map/barras/amarelo.png"); if (barraTex.id==0) barraTex = LoadTexture("assets/map/barras/branca.png");
    {
        Rectangle r1[2], r2[2]; int c1 = ParseRectsFromGroup(tmx, "Barra_Elevador1_Colisao", r1, 2); int c2 = ParseRectsFromGroup(tmx, "Barra_Elevador1", r2, 2);
        if (c1>0 && c2>0) PlatformInit(&elev, r1[0], r2[0], 2.5f);
        c1 = ParseRectsFromGroup(tmx, "Barra1_Movel", r1, 2); c2 = ParseRectsFromGroup(tmx, "AreaMovimentoBarraMovel", r2, 2);
        if (c1>0 && c2>0) PlatformInit(&barraM, r1[0], r2[0], 2.0f);
    }

    // Ventilador2
    Fan vent2={0}; bool haveVent2=false; { Rectangle vr[2]; int vc=ParseRectsFromGroup(tmx, "Ventilador2", vr, 2); if (vc>0) { FanInit(&vent2, vr[0].x, vr[0].y, vr[0].width, vr[0].height, 0.9f); haveVent2=true; } }

    // Câmera
    Camera2D cam = {0}; cam.target=(Vector2){mapTex.width/2.0f,mapTex.height/2.0f}; cam.offset=(Vector2){GetScreenWidth()/2.0f,GetScreenHeight()/2.0f}; cam.zoom=1.0f;

    bool completed=false; float elapsed=0.0f; bool debug=false; SetTargetFPS(60);
    while (!WindowShouldClose()) {
        elapsed += GetFrameTime(); if (IsKeyPressed(KEY_TAB)) debug=!debug;

        bool p1 = ButtonUpdate(&b1, &earthboy, &fireboy, &watergirl);
        bool p2 = ButtonUpdate(&b2, &earthboy, &fireboy, &watergirl);
        bool p3 = ButtonUpdate(&b3, &earthboy, &fireboy, &watergirl);

        float elevPrevY = elev.rect.y, barraPrevY = barraM.rect.y;
        if (elev.area.height>0 && elev.rect.height>0) {
            if (p1 && !p2 && !p3) { float base = elev.area.y + elev.area.height - elev.rect.height; elev.rect.y = moveTowards(elev.rect.y, base, elev.speed); }
            else { elev.rect.y = moveTowards(elev.rect.y, elev.startY, elev.speed); }
            float minY=elev.area.y, maxY=elev.area.y+elev.area.height-elev.rect.height; if (elev.rect.y<minY) elev.rect.y=minY; if (elev.rect.y>maxY) elev.rect.y=maxY;
        }
        if (barraM.area.height>0 && barraM.rect.height>0) {
            int pressed=(p1?1:0)+(p2?1:0)+(p3?1:0);
            if (pressed>=2) { float topo = barraM.area.y; barraM.rect.y = moveTowards(barraM.rect.y, topo, barraM.speed); }
            else { barraM.rect.y = moveTowards(barraM.rect.y, barraM.startY, barraM.speed); }
            float minY=barraM.area.y, maxY=barraM.area.y+barraM.area.height-barraM.rect.height; if (barraM.rect.y<minY) barraM.rect.y=minY; if (barraM.rect.y>maxY) barraM.rect.y=maxY;
        }
        float elevDY = elev.rect.y - elevPrevY, barraDY = barraM.rect.y - barraPrevY;

        Rectangle ground = (Rectangle){0, mapTex.height, mapTex.width, 200};
        UpdatePlayer(&earthboy, ground, KEY_A, KEY_D, KEY_W);
        UpdatePlayer(&fireboy,  ground, KEY_LEFT, KEY_RIGHT, KEY_UP);
        UpdatePlayer(&watergirl,ground, KEY_J, KEY_L, KEY_I);

        Player* P[3] = {&earthboy,&fireboy,&watergirl};
        for (int p=0;p<3;p++) {
            Player* pl=P[p];
            for (int i=0;i<nCol;i++) {
                Rectangle b = col[i].rect; if (!CheckCollisionRecs(pl->rect,b)) continue;
                float dx=(pl->rect.x+pl->rect.width*0.5f)-(b.x+b.width*0.5f);
                float dy=(pl->rect.y+pl->rect.height*0.5f)-(b.y+b.height*0.5f);
                float ox=(pl->rect.width*0.5f+b.width*0.5f)-fabsf(dx);
                float oy=(pl->rect.height*0.5f+b.height*0.5f)-fabsf(dy);
                if (ox<oy) { if (!(dy>0 && pl->velocity.y>0)) { Rectangle t=pl->rect; t.y-=STEP_HEIGHT; if (!CheckCollisionRecs(t,b)) { pl->rect.y-=STEP_HEIGHT; continue; } } if (dx>0) pl->rect.x+=ox; else pl->rect.x-=ox; pl->velocity.x=0; }
                else { if (dy>0 && pl->velocity.y<0) { pl->rect.y+=oy; pl->velocity.y=0; } else if (dy<0 && pl->velocity.y>=0) { pl->rect.y-=oy; pl->velocity.y=0; pl->isJumping=false; } }
            }
        }

        for (int p=0;p<3;p++) { Player* pl=P[p]; if (elev.rect.width>0) HandlePlatformTop(pl, elev.rect, elevDY); if (barraM.rect.width>0) HandlePlatformTop(pl, barraM.rect, barraDY); }

        if (haveVent2 && p3 && !p1 && !p2) { FanApply(&vent2, &earthboy); FanApply(&vent2, &fireboy); FanApply(&vent2, &watergirl); }

        for (int p=0;p<3;p++) {
            Player* pl=P[p]; LakeType elem=(p==0)?LAKE_EARTH:((p==1)?LAKE_FIRE:LAKE_WATER);
            for (int i=0;i<nLakes;i++) if (LakeHandlePlayer(&lakes[i], pl, elem)) { if (p==0){pl->rect.x=spawnE.x;pl->rect.y=spawnE.y;} else if (p==1){pl->rect.x=spawnF.x;pl->rect.y=spawnF.y;} else {pl->rect.x=spawnW.x;pl->rect.y=spawnW.y;} pl->velocity=(Vector2){0,0}; pl->isJumping=false; break; }
        }

        BeginDrawing(); ClearBackground(BLACK); BeginMode2D(cam);
        DrawTexture(mapTex, 0, 0, WHITE);
        for (int i=0;i<nLakes;i++) LakeDraw(&lakes[i]);
        if (elev.rect.width>0 && barraTex.id) DrawTexturePro(barraTex, (Rectangle){0,0,(float)barraTex.width,(float)barraTex.height}, elev.rect, (Vector2){0,0}, 0.0f, WHITE);
        if (barraM.rect.width>0 && barraTex.id) DrawTexturePro(barraTex, (Rectangle){0,0,(float)barraTex.width,(float)barraTex.height}, barraM.rect, (Vector2){0,0}, 0.0f, WHITE);
        DrawPlayer(earthboy); DrawPlayer(fireboy); DrawPlayer(watergirl);
        GoalDraw(&goal);
        ButtonDraw(&b1); ButtonDraw(&b2); ButtonDraw(&b3);
        if (haveVent2 && p3 && !p1 && !p2) FanDraw(&vent2);
        if (debug) { for (int i=0;i<nCol;i++) DrawRectangleLinesEx(col[i].rect,1,Fade(GREEN,0.5f)); DrawRectangleLinesEx(elev.area,1,Fade(YELLOW,0.5f)); DrawRectangleLinesEx(barraM.area,1,Fade(ORANGE,0.5f)); DrawText(TextFormat("b1=%d b2=%d b3=%d", p1,p2,p3), 10,10,20,RAYWHITE); }
        if (GoalReached(&goal, &earthboy, &fireboy, &watergirl)) { completed=true; EndMode2D(); EndDrawing(); break; }
        EndMode2D(); EndDrawing();
    }

    UnloadTexture(mapTex); if (barraTex.id) UnloadTexture(barraTex);
    UnloadPlayer(&earthboy); UnloadPlayer(&fireboy); UnloadPlayer(&watergirl);
    if (completed) Ranking_Add(2, Game_GetPlayerName(), elapsed);
    return completed;
}
