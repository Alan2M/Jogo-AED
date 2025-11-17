#include "theme.h"
#include <stdbool.h>

static Music gTheme = {0};
static bool gAudioOpened = false;
static bool gThemeReady = false;

void Theme_Init(void) {
    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
        gAudioOpened = true;
    }
    gTheme = LoadMusicStream("assets/sounds/theme.mp3");
    if (gTheme.stream.sampleRate > 0) {
        gThemeReady = true;
        PlayMusicStream(gTheme);
    }
}

void Theme_Update(void) {
    if (!gThemeReady) return;
    UpdateMusicStream(gTheme);
}

void Theme_Shutdown(void) {
    if (gThemeReady) {
        StopMusicStream(gTheme);
        UnloadMusicStream(gTheme);
        gThemeReady = false;
    }
    if (gAudioOpened) {
        CloseAudioDevice();
        gAudioOpened = false;
    }
}
