#include "audio.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr int kSampleRate = 22050;

// Build a mono 16-bit square-wave blip with a short linear decay envelope and
// optional pitch slide (start -> end Hz). Returns a raylib Sound (caller owns).
Sound makeBlip(float startHz, float endHz, float durSec, float vol = 0.35f) {
    int n = static_cast<int>(kSampleRate * durSec);
    if (n < 1) n = 1;
    std::vector<int16_t> samples(n);

    double phase = 0.0;
    for (int i = 0; i < n; ++i) {
        float t = static_cast<float>(i) / n;          // 0..1
        float freq = startHz + (endHz - startHz) * t;  // linear slide
        phase += freq / kSampleRate;
        if (phase >= 1.0) phase -= 1.0;
        float sq = (phase < 0.5) ? 1.0f : -1.0f;      // square wave
        float env = (1.0f - t) * vol;                  // linear decay
        samples[i] = static_cast<int16_t>(sq * env * 32767.0f);
    }

    Wave w{};
    w.frameCount = static_cast<unsigned int>(n);
    w.sampleRate = kSampleRate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = samples.data();
    Sound s = LoadSoundFromWave(w);  // copies data into its own buffer
    return s;
}

}  // namespace

void AudioBank::load(const std::string& musicPath) {
    sfxMove_ = makeBlip(440.0f, 520.0f, 0.05f, 0.25f);    // tick up
    sfxSelect_ = makeBlip(660.0f, 990.0f, 0.10f, 0.35f);  // confirm chirp
    sfxOk_ = makeBlip(523.0f, 1046.0f, 0.22f, 0.40f);     // success rise
    sfxFail_ = makeBlip(330.0f, 110.0f, 0.30f, 0.40f);    // error fall
    sfxBack_ = makeBlip(520.0f, 400.0f, 0.06f, 0.25f);    // tick down
    sfxDoor_ = makeBlip(150.0f, 95.0f, 0.16f, 0.30f);     // wooden door creak

    if (!musicPath.empty() && FileExists(musicPath.c_str())) {
        music_ = LoadMusicStream(musicPath.c_str());
        if (music_.frameCount > 0) {
            hasMusic_ = true;
            music_.looping = true;
            SetMusicVolume(music_, 0.55f);
            PlayMusicStream(music_);
        }
    }
}

void AudioBank::unload() {
    UnloadSound(sfxMove_);
    UnloadSound(sfxSelect_);
    UnloadSound(sfxOk_);
    UnloadSound(sfxFail_);
    UnloadSound(sfxBack_);
    UnloadSound(sfxDoor_);
    if (hasMusic_) UnloadMusicStream(music_);
}

void AudioBank::update() {
    if (hasMusic_) UpdateMusicStream(music_);
}

void AudioBank::playMove() { PlaySound(sfxMove_); }
void AudioBank::playSelect() { PlaySound(sfxSelect_); }
void AudioBank::playOk() { PlaySound(sfxOk_); }
void AudioBank::playFail() { PlaySound(sfxFail_); }
void AudioBank::playBack() { PlaySound(sfxBack_); }
void AudioBank::playDoor() { PlaySound(sfxDoor_); }
