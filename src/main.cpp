#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "audio.hpp"
#include "raylib.h"
#include "town.hpp"
#include "ui.hpp"
#include "warp.hpp"

#ifndef ASSETS_DIR
#define ASSETS_DIR "assets"
#endif

namespace {

enum class Screen { Menu, Confirm, Running, Result };

enum class ItemKind { Connect, Disconnect, Vnet };

struct MenuItem {
    std::string label;  // full name (tooltip)
    std::string sign;   // plaque text on the tavern roof
    ItemKind kind;
    std::string uuid;   // for Vnet
};

// Message-box colours (dark blue window, white frame).
constexpr Color kBoxFill = {24, 32, 104, 235};
constexpr Color kBoxLine = {240, 240, 255, 255};

std::string upper(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

// Try a couple of common music filenames; first that exists wins.
std::string findMusic() {
    const char* names[] = {ASSETS_DIR "/music/theme.ogg",
                           ASSETS_DIR "/music/theme.xm",
                           ASSETS_DIR "/music/theme.mod",
                           ASSETS_DIR "/music/theme.mp3"};
    for (const char* n : names)
        if (FileExists(n)) return n;
    return "";
}

void messageBox(ui::Ui& gui, int w, int h, int y) {
    int x = (ui::kVirtW - w) / 2;
    DrawRectangle(x + 2, y + 2, w, h, {0, 0, 0, 120});
    gui.panel(x, y, w, h, kBoxFill, kBoxLine);
}

}  // namespace

int main() {
    // raylib logs every texture/file load at INFO; keep the terminal quiet
    // unless WARP_DEBUG is set. Warnings and errors still print.
    if (!std::getenv("WARP_DEBUG")) SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(ui::kVirtW * 3, ui::kVirtH * 3, "TOTVS WARP");
    SetWindowMinSize(ui::kVirtW, ui::kVirtH);
    InitAudioDevice();
    SetExitKey(KEY_NULL);  // we handle quit ourselves
    // No SetTargetFPS: vsync paces frames. A 60 fps timer cap beats against a
    // 59.9 Hz display and periodically repeats/drops a frame (visible judder
    // while the camera scrolls). The timer cap is only a fallback, applied
    // below if vsync turns out not to throttle.

    ui::Ui gui;
    gui.init(ASSETS_DIR "/font/PressStart2P.ttf");

    AudioBank audio;
    audio.load(findMusic());

    WarpController warp;

    // One tavern per action, left to right (tavern index == item index).
    std::vector<MenuItem> items = {
        {"Cloud Connect", "CONNECT", ItemKind::Connect, ""},
        {"Cloud Disconnect", "DISCONNECT", ItemKind::Disconnect, ""},
    };
    for (const auto& v : warp.vnets())
        items.push_back({"Warp " + v.name, upper(v.name), ItemKind::Vnet, v.uuid});

    Town town;
    {
        std::vector<std::string> signs;
        for (const auto& it : items) signs.push_back(it.sign);
        town.init(ASSETS_DIR, signs);
    }

    Screen screen = Screen::Menu;
    int confirmChoice = 0;  // 0 = No, 1 = Yes (disconnect confirm)
    bool quit = false;
    bool closeAfterResult = false;  // connect / vnet-switch close the app when done
    double resultShownAt = 0.0;

    auto launch = [&](int idx) {
        const MenuItem& it = items[idx];
        switch (it.kind) {
            case ItemKind::Connect:
                if (warp.connect()) {
                    audio.playSelect();
                    closeAfterResult = true;
                    screen = Screen::Running;
                }
                break;
            case ItemKind::Disconnect:
                confirmChoice = 0;
                audio.playSelect();
                screen = Screen::Confirm;
                break;
            case ItemKind::Vnet:
                if (warp.setVnet(it.uuid)) {
                    audio.playSelect();
                    closeAfterResult = true;  // pick network -> connect -> close
                    screen = Screen::Running;
                }
                break;
        }
        if (screen == Screen::Menu) {  // action refused (one already running)
            audio.playBack();
            town.leaveTavern();
        }
    };

    auto backToMenu = [&] {
        screen = Screen::Menu;
        audio.playDoor();
        town.leaveTavern();
    };

    // Dev hook: WARP_SNAPSHOT=out.png renders ~1s, saves the 320x240 canvas
    // and exits. WARP_SNAPSHOT_WIZ="x,y" places the wizard (world px, feet).
    const char* snapPath = std::getenv("WARP_SNAPSHOT");
    if (const char* p = std::getenv("WARP_SNAPSHOT_WIZ")) {
        float x = 0, y = 0;
        if (std::sscanf(p, "%f,%f", &x, &y) == 2) town.placeWizard(x, y);
    }
    int frame = 0;
    double tickAcc = 0.0;      // fixed-step accumulator (60 Hz game ticks)
    double fpsProbe = 0.0;     // sums the first frames to detect missing vsync

    while (!WindowShouldClose() && !quit) {
        float t = static_cast<float>(GetTime());
        // Game logic runs in fixed 1/60 s ticks so the wizard/camera move the
        // same whole number of pixels every frame. A frame close to 1/60 s
        // counts as exactly one tick (absorbs vsync jitter); anything else
        // goes through the accumulator.
        float ft = GetFrameTime();
        int ticks;
        if (ft > 0.0145f && ft < 0.0190f) {
            ticks = 1;
            tickAcc = 0.0;
        } else {
            tickAcc += std::min(ft, 0.1f);
            ticks = static_cast<int>(tickAcc * 60.0);
            tickAcc -= ticks / 60.0;
        }
        if (frame < 60) {
            fpsProbe += ft;
            if (frame == 59 && fpsProbe / 60.0 < 0.012) SetTargetFPS(60);  // no vsync
        }
        audio.update();
        warp.poll(GetTime());

        // ---- village (wizard only steerable on the Menu screen) -----------
        Town::Input in;
        if (screen == Screen::Menu) {
            in.left = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A);
            in.right = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
            in.up = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
            in.down = IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S);
        }
        Town::Events ev;
        for (int i = 0; i < ticks; ++i) {
            Town::Events e = town.update(1.0f / 60.0f, in);
            if (e.entered >= 0) ev.entered = e.entered;
            ev.exited |= e.exited;
            ev.doorOpened |= e.doorOpened;
        }
        if (ev.doorOpened) audio.playDoor();
        if (ev.entered >= 0 && screen == Screen::Menu) launch(ev.entered);
        if (ev.exited) quit = true;

        // ---- screen state machine -----------------------------------------
        switch (screen) {
            case Screen::Menu: {
                if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) {
                    audio.playBack();
                    quit = true;
                }
                break;
            }
            case Screen::Confirm: {
                if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) ||
                    IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)) {
                    confirmChoice ^= 1;
                    audio.playMove();
                }
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                    if (confirmChoice == 1) {
                        if (warp.disconnect()) {
                            audio.playSelect();
                            screen = Screen::Running;
                        }
                    } else {
                        audio.playBack();
                        backToMenu();
                    }
                }
                if (IsKeyPressed(KEY_ESCAPE)) {
                    audio.playBack();
                    backToMenu();
                }
                break;
            }
            case Screen::Running: {
                if (warp.actionState() != WarpController::ActionState::Running) {
                    if (warp.actionState() == WarpController::ActionState::Ok)
                        audio.playOk();
                    else
                        audio.playFail();
                    resultShownAt = GetTime();
                    screen = Screen::Result;
                }
                break;
            }
            case Screen::Result: {
                bool ok = warp.actionState() == WarpController::ActionState::Ok;
                double dwell = (closeAfterResult && ok) ? 1.1 : 2.5;
                bool advance = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                               IsKeyPressed(KEY_ESCAPE) ||
                               (GetTime() - resultShownAt > dwell);
                if (advance) {
                    warp.clearAction();
                    if (closeAfterResult && ok) {
                        quit = true;  // connected / switched vnet -> close app
                    } else {
                        closeAfterResult = false;
                        backToMenu();
                    }
                }
                break;
            }
        }

        // ---- render --------------------------------------------------------
        gui.beginCanvas();
        town.draw(gui, t);

        // HUD (outlined text straight on the sky)
        {
            bool conn = warp.isConnected();
            std::string st = warp.status();
            Color stc = conn ? ui::kGreen : (st == "Disconnected" ? ui::kRed : ui::kInk);
            gui.textOutlined("TOTVS WARP", 8, 7, 8, ui::kAccent);
            gui.textOutlined("> " + st, 8, 18, 8, stc);
            std::string vn = warp.vnetName();
            gui.textOutlined("VNET", ui::kVirtW - 8 - gui.textWidth("VNET", 8), 7, 8,
                             ui::kAccent);
            gui.textOutlined(vn, ui::kVirtW - 8 - gui.textWidth(vn, 8), 18, 8, ui::kCyan);
        }

        // Tooltip: full action name of the tavern door the wizard stands at.
        if (screen == Screen::Menu) {
            int di = town.doorNearby();
            if (di >= 0) {
                std::string lbl = "UP: " + items[di].label;
                int tw = gui.textWidth(lbl, 8);
                int x = (ui::kVirtW - tw) / 2, y = ui::kVirtH - 30;
                DrawRectangle(x - 4, y - 3, tw + 8, 14, kBoxFill);
                DrawRectangleLines(x - 4, y - 3, tw + 8, 14, kBoxLine);
                gui.text(lbl, x, y, 8, ui::kInk);
            }
        }

        if (screen == Screen::Confirm) {
            messageBox(gui, 220, 64, 70);
            gui.textCentered("Disconnect WARP?", ui::kVirtW / 2, 84, 8, ui::kInk);
            int yy = 110;
            const char* opts[2] = {"NO", "YES"};
            int cx[2] = {ui::kVirtW / 2 - 40, ui::kVirtW / 2 + 24};
            for (int i = 0; i < 2; ++i) {
                bool on = confirmChoice == i;
                Color c = on ? ui::kAccent : ui::kInkDim;
                if (on && std::sin(t * 8.0f) > -0.3f)
                    gui.text(">", cx[i] - 12, yy, 8, ui::kAccent);
                gui.text(opts[i], cx[i], yy, 8, c);
            }
        }

        if (screen == Screen::Running) {
            messageBox(gui, 240, 56, 70);
            gui.textCentered(warp.actionLabel(), ui::kVirtW / 2, 82, 8, ui::kAccent);
            int dots = static_cast<int>(t * 4.0f) % 4;
            gui.textCentered("WORKING" + std::string(dots, '.'), ui::kVirtW / 2, 104, 8,
                             ui::kInk);
        }

        if (screen == Screen::Result) {
            bool ok = warp.actionState() == WarpController::ActionState::Ok;
            messageBox(gui, 240, 72, 64);
            gui.textCentered(warp.actionLabel(), ui::kVirtW / 2, 76, 8, ui::kInkDim);
            gui.textCentered(ok ? "SUCCESS" : "FAILED", ui::kVirtW / 2, 92, 16,
                             ok ? ui::kGreen : ui::kRed);
            gui.textCentered("- press enter -", ui::kVirtW / 2, 118, 8, ui::kInkDim);
        }

        // footer hint (PressStart2P is wide; must fit 320px)
        gui.textOutlinedCentered("ARROWS WALK  UP ENTER  ESC QUIT", ui::kVirtW / 2,
                                 ui::kVirtH - 12, 8, ui::kInk);

        gui.drawScanlines();
        gui.endCanvas();

        BeginDrawing();
        gui.present();
        EndDrawing();

        ++frame;
        if (snapPath && frame == 60) {
            Image img = LoadImageFromTexture(gui.canvas().texture);
            ImageFlipVertical(&img);
            ExportImage(img, snapPath);
            UnloadImage(img);
            quit = true;
        }
    }

    town.unload();
    audio.unload();
    gui.unload();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
