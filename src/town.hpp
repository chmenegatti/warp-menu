#pragma once
#include <string>
#include <vector>

#include "raylib.h"
#include "ui.hpp"

// Top-down village street (Pixel Crawler sprites by Anokolisa). A row of
// taverns faces a dirt street; each tavern is one action. The wizard walks
// around (camera scrolls horizontally) and enters a tavern by walking up into
// its door. Walking off the right end of the street exits.
//
// World coordinates are virtual pixels; the world is kWorldH tall (one screen)
// and wider than the screen.
class Town {
public:
    struct Input {
        bool left = false, right = false, up = false, down = false;
    };
    struct Events {
        int entered = -1;    // tavern whose door the wizard just went through
        bool exited = false; // walked off the end of the street
        bool doorOpened = false; // started walking into a tavern
    };

    // assetsDir: folder holding the pixelcrawler/ sheets. signs: plaque text,
    // one tavern per entry.
    void init(const std::string& assetsDir, const std::vector<std::string>& signs);
    void unload();

    Events update(float dt, const Input& in);
    void draw(ui::Ui& gui, float t) const;

    // Tavern whose door the wizard is standing at (for the tooltip), or -1.
    int doorNearby() const;
    // Wizard came back out (after an action / cancelled confirm).
    void leaveTavern();
    bool insideTavern() const { return state_ == State::Inside; }

    // Dev/snapshot helper: put the wizard at a world position (feet).
    void placeWizard(float x, float y) { wx_ = x; wy_ = y; }

    static constexpr int kWorldH = ui::kVirtH;

private:
    enum class State { Walking, Entering, Inside, Leaving };

    struct Tavern {
        float x;        // left edge of the 128px-wide house
        std::string sign;
        bool greenRoof;
    };
    struct Prop {       // y-sorted decoration (tree / bush)
        int tex;        // 0 = trees sheet, 1 = vegetation sheet
        Rectangle src;
        float x, y;     // feet (bottom-centre) position
    };

    Texture2D walls_{}, roofs_{}, bprops_{}, floors_{}, veg_{}, trees_{};
    Texture2D bonfire_{}, fire_{}, wizIdle_{}, wizRun_{};

    std::vector<Tavern> taverns_;
    std::vector<Prop> props_;
    float worldW_ = ui::kVirtW;
    float bonfireX_ = 0, bonfireY_ = 0;

    // wizard (feet position)
    float wx_ = 60, wy_ = 180;
    bool faceRight_ = true;
    bool moving_ = false;
    float animT_ = 0;
    State state_ = State::Walking;
    int doorIdx_ = -1;     // tavern being entered / inside
    float stateT_ = 0;

    bool blocked(float x, float y) const;
    Rectangle doorRect(int i) const;
    float camX() const;

    void drawGround(float cam) const;
    void drawTavern(ui::Ui& gui, const Tavern& tv, bool doorOpen) const;
    void drawWizard(float alpha, float t) const;
};
