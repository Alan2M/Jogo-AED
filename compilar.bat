@echo off
echo Compilando Jogo Elements...
gcc src/main.c src/menu/menu.c src/player/player.c src/mapa/mapa_fases.c src/game/game.c src/interface/pause.c src/ranking/ranking.c src/objects/box.c src/objects/pendulum.c src/objects/seesaw.c src/objects/goal.c src/objects/button.c src/objects/lake.c src/objects/fan.c ^
src/mapa/fases/fase1.c src/mapa/fases/fase2.c src/mapa/fases/fase3.c ^
src/mapa/fases/fase4.c src/mapa/fases/fase5.c ^
-o jogo.exe -I src -I C:\raylib\include -L C:\raylib\lib ^
-lraylib -lopengl32 -lgdi32 -lwinmm -lm -static
if %errorlevel%==0 (
    echo ===========================
    echo Compilado com sucesso!
    echo ===========================
    jogo.exe
) else (
    echo ===========================
    echo ERRO NA COMPILACAO!
    echo ===========================
)
pause
