@echo off
echo Compilando Jogo Elements...
gcc src/main.c src/menu/menu.c src/player/player.c src/mapa/mapa_fases.c ^
src/mapa/fases/fase1.c src/mapa/fases/fase2.c src/mapa/fases/fase3.c ^
src/mapa/fases/fase4.c src/mapa/fases/fase5.c ^
-o jogo.exe -I src -I C:\raylib\include -L C:\raylib\lib ^
-lraylib -lopengl32 -lgdi32 -lwinmm -static
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
