#pragma once
#include <string>
#include "raylib.h"

// 16-bit-style rendering helpers. Everything is drawn at a fixed low internal
// resolution into a RenderTexture, then upscaled with POINT filtering so pixels
// stay crisp (SNES look).
namespace ui {

constexpr int kVirtW = 320;
constexpr int kVirtH = 240;

// SNES-ish palette.
inline const Color kBg0 = {18, 16, 38, 255};      // deep indigo
inline const Color kBg1 = {38, 26, 74, 255};      // panel fill
inline const Color kInk = {232, 234, 255, 255};   // near-white text
inline const Color kInkDim = {150, 152, 196, 255};
inline const Color kAccent = {248, 208, 96, 255};  // gold (title/cursor)
inline const Color kGreen = {120, 224, 144, 255};
inline const Color kRed = {240, 104, 112, 255};
inline const Color kCyan = {120, 208, 240, 255};
inline const Color kShadow = {8, 6, 18, 255};

class Ui {
public:
    void init(const std::string& fontPath);
    void unload();

    // Begin/end drawing to the virtual canvas, then blit it scaled to window.
    void beginCanvas();
    void endCanvas();
    void present();  // call after EndDrawing-less flow: draws scaled texture

    // Pixel-font text with a 1px drop shadow. size in virtual pixels.
    // spacing < 0 -> default (size/8); signs pass 0 to pack tighter.
    void text(const std::string& s, int x, int y, int size, Color c, float spacing = -1.0f);
    void textCentered(const std::string& s, int cx, int y, int size, Color c);
    int textWidth(const std::string& s, int size, float spacing = -1.0f) const;

    // Bordered panel (double-line retro frame).
    void panel(int x, int y, int w, int h, Color fill, Color border);

    // Text with a 1px outline on all sides (SNES HUD style) — readable on
    // bright backgrounds.
    void textOutlined(const std::string& s, int x, int y, int size, Color c);
    void textOutlinedCentered(const std::string& s, int cx, int y, int size, Color c);
    // CRT scanline overlay on the virtual canvas.
    void drawScanlines();

    RenderTexture2D& canvas() { return canvas_; }

private:
    Font font_{};
    bool customFont_{false};
    RenderTexture2D canvas_{};
};

}  // namespace ui
