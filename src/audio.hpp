#pragma once
#include <string>
#include "raylib.h"

// Retro audio: procedurally-synthesised square-wave SFX (no asset files) plus
// an optional chiptune background track loaded from disk.
class AudioBank {
public:
    void load(const std::string& musicPath);  // music optional; skipped if missing
    void unload();
    void update();  // pump the music stream; call each frame

    void playMove();
    void playSelect();
    void playOk();
    void playFail();
    void playBack();
    void playDoor();

private:
    Sound sfxMove_{};
    Sound sfxSelect_{};
    Sound sfxOk_{};
    Sound sfxFail_{};
    Sound sfxBack_{};
    Sound sfxDoor_{};

    Music music_{};
    bool hasMusic_{false};
};
