#include "ui.hpp"

#include <cmath>
#include <algorithm>

namespace ui {

void Ui::init(const std::string& fontPath) {
    canvas_ = LoadRenderTexture(kVirtW, kVirtH);
    SetTextureFilter(canvas_.texture, TEXTURE_FILTER_POINT);

    if (!fontPath.empty() && FileExists(fontPath.c_str())) {
        // PressStart2P is a native 8px pixel font. Bake the atlas at 8px so
        // size-8 text is 1:1 and bigger sizes (16/24/32) are clean integer
        // upscales under POINT filtering. Baking large then drawing small
        // downscales the atlas and turns glyphs to mush.
        font_ = LoadFontEx(fontPath.c_str(), 8, nullptr, 0);
        SetTextureFilter(font_.texture, TEXTURE_FILTER_POINT);
        customFont_ = true;
    } else {
        font_ = GetFontDefault();
        customFont_ = false;
    }
}

void Ui::unload() {
    if (customFont_) UnloadFont(font_);
    UnloadRenderTexture(canvas_);
}

int Ui::textWidth(const std::string& s, int size, float spacing) const {
    if (spacing < 0) spacing = size / 8.0f;
    Vector2 m = MeasureTextEx(font_, s.c_str(), static_cast<float>(size), spacing);
    return static_cast<int>(m.x);
}

void Ui::text(const std::string& s, int x, int y, int size, Color c, float spacing) {
    if (spacing < 0) spacing = size / 8.0f;
    Vector2 pos = {static_cast<float>(x), static_cast<float>(y)};
    Vector2 sh = {pos.x + 1, pos.y + 1};
    DrawTextEx(font_, s.c_str(), sh, static_cast<float>(size), spacing, kShadow);
    DrawTextEx(font_, s.c_str(), pos, static_cast<float>(size), spacing, c);
}

void Ui::textCentered(const std::string& s, int cx, int y, int size, Color c) {
    text(s, cx - textWidth(s, size) / 2, y, size, c);
}

void Ui::textOutlined(const std::string& s, int x, int y, int size, Color c) {
    float spacing = size / 8.0f;
    const char* cs = s.c_str();
    float fs = static_cast<float>(size);
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            if (dx || dy)
                DrawTextEx(font_, cs, {static_cast<float>(x + dx), static_cast<float>(y + dy)},
                           fs, spacing, kShadow);
    DrawTextEx(font_, cs, {static_cast<float>(x), static_cast<float>(y)}, fs, spacing, c);
}

void Ui::textOutlinedCentered(const std::string& s, int cx, int y, int size, Color c) {
    textOutlined(s, cx - textWidth(s, size) / 2, y, size, c);
}

void Ui::panel(int x, int y, int w, int h, Color fill, Color border) {
    DrawRectangle(x, y, w, h, fill);
    DrawRectangleLines(x, y, w, h, border);
    DrawRectangleLines(x + 2, y + 2, w - 4, h - 4, Fade(border, 0.5f));
}

void Ui::drawScanlines() {
    for (int y = 0; y < kVirtH; y += 2)
        DrawLine(0, y, kVirtW, y, {0, 0, 0, 22});
}

void Ui::beginCanvas() {
    BeginTextureMode(canvas_);
}

void Ui::endCanvas() {
    EndTextureMode();
}

void Ui::present() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    // Snap to an INTEGER scale so the virtual canvas upscales pixel-perfect
    // (fractional scaling smears the pixel font). Letterbox the remainder.
    int iscale = std::min(sw / kVirtW, sh / kVirtH);
    if (iscale < 1) iscale = 1;
    float scale = static_cast<float>(iscale);
    float dw = kVirtW * scale, dh = kVirtH * scale;
    float ox = (sw - dw) * 0.5f, oy = (sh - dh) * 0.5f;
    Rectangle src = {0, 0, static_cast<float>(kVirtW),
                     -static_cast<float>(kVirtH)};  // flip Y
    Rectangle dst = {ox, oy, dw, dh};
    ClearBackground(BLACK);
    DrawTexturePro(canvas_.texture, src, dst, {0, 0}, 0.0f, WHITE);
}

}  // namespace ui
