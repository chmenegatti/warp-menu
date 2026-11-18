#include "town.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

// ---- layout (virtual px) --------------------------------------------------
constexpr float kHouseW = 128;
constexpr float kHouseGap = 24;
constexpr float kHouseTop = 26;                 // top of the roof
constexpr float kWallTop = kHouseTop + 63;      // wall strip tucks under the roof
constexpr float kHouseBottom = kWallTop + 51;   // = 140, front edge / door sill
constexpr float kStreetTop = 128;               // 5 tiles: 128..208
constexpr float kExitRun = 64;                  // street past the last tavern
constexpr float kWizSpeed = 120;
constexpr float kMinFeetY = kHouseBottom + 1;   // can't walk behind houses
constexpr float kMaxFeetY = ui::kVirtH - 4;
constexpr float kEnterDur = 0.45f;
constexpr float kLeaveDur = 0.35f;

// ---- sprite rects inside the sheets (see assets/pixelcrawler/README.md) ----
constexpr Rectangle kRoofBrown = {0, 6, 128, 89};
constexpr Rectangle kRoofGreen = {128, 6, 128, 89};
constexpr Rectangle kWallStrip = {288, 189, 128, 51};  // timber-frame facade
constexpr Rectangle kDoorFrame = {32, 20, 32, 44};     // open wooden arch
constexpr Rectangle kDoorClosed = {134, 27, 20, 39};   // iron-hinged door
constexpr Rectangle kTree = {4, 4, 37, 76};
constexpr Rectangle kTree2 = {52, 84, 37, 76};
constexpr Rectangle kBushes[] = {{2, 5, 29, 27}, {50, 5, 29, 27}, {194, 86, 39, 33}};

// Wizard sheets: Run = 6 cells of 64x64 (feet on the bottom row), Idle = 4
// cells of 32x32 whose top lines up with Run's row 30.
constexpr int kRunFrames = 6, kIdleFrames = 4;

uint32_t hash2(int x, int y) {
    uint32_t h = static_cast<uint32_t>(x) * 73856093u ^ static_cast<uint32_t>(y) * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    return h ^ (h >> 15);
}

void blit(const Texture2D& tex, Rectangle src, float x, float y, Color tint = WHITE) {
    DrawTextureRec(tex, src, {std::floor(x), std::floor(y)}, tint);
}

constexpr Color kWoodDk = {72, 40, 28, 255};
constexpr Color kWood = {150, 92, 52, 255};
constexpr Color kWoodHi = {190, 126, 74, 255};
constexpr Color kSignInk = {255, 240, 210, 255};

void plaque(ui::Ui& gui, const std::string& s, int cx, int y) {
    int tw = gui.textWidth(s, 8, 0.0f);
    int w = tw + 10, x = cx - w / 2;
    DrawRectangle(x - 1, y - 1, w + 2, 15, {20, 12, 10, 255});
    DrawRectangle(x, y, w, 13, kWoodDk);
    DrawRectangle(x + 1, y + 1, w - 2, 11, kWood);
    DrawRectangle(x + 1, y + 1, w - 2, 1, kWoodHi);
    gui.text(s, cx - tw / 2, y + 3, 8, kSignInk, 0.0f);
}

}  // namespace

void Town::init(const std::string& assetsDir, const std::vector<std::string>& signs) {
    auto load = [&](const char* f) {
        return LoadTexture((assetsDir + "/pixelcrawler/" + f).c_str());
    };
    walls_ = load("walls.png");
    roofs_ = load("roofs.png");
    bprops_ = load("building_props.png");
    floors_ = load("floors.png");
    veg_ = load("vegetation.png");
    trees_ = load("trees.png");
    bonfire_ = load("bonfire.png");
    fire_ = load("fire.png");
    wizIdle_ = load("wizard_idle.png");
    wizRun_ = load("wizard_run.png");

    taverns_.clear();
    props_.clear();
    int n = static_cast<int>(signs.size());
    for (int i = 0; i < n; ++i)
        taverns_.push_back({kHouseGap + i * (kHouseW + kHouseGap), signs[i], (i % 2) == 1});
    worldW_ = kHouseGap + n * (kHouseW + kHouseGap) + kExitRun;
    worldW_ = std::max(worldW_, static_cast<float>(ui::kVirtW));

    // conifers peeking out between the houses (feet behind the house fronts)
    for (int i = 0; i <= n; ++i) {
        float gx = kHouseGap * 0.5f + i * (kHouseW + kHouseGap);
        props_.push_back({0, (i % 2) ? kTree2 : kTree, gx, kHouseBottom - 14});
    }
    bonfireX_ = std::floor(worldW_ * 0.5f);
    bonfireY_ = 206;

    // bushes along the south verge (leaving room around the bonfire)
    for (float x = 20; x < worldW_ - 20; x += 54) {
        if (std::fabs(x - bonfireX_) < 40) continue;
        uint32_t h = hash2(static_cast<int>(x), 7);
        Rectangle r = kBushes[h % 3];
        props_.push_back({1, r, x + static_cast<float>(h % 17), static_cast<float>(ui::kVirtH + 4)});
    }

    wx_ = 60;
    wy_ = 176;
    state_ = State::Walking;
}

void Town::unload() {
    for (Texture2D* t : {&walls_, &roofs_, &bprops_, &floors_, &veg_, &trees_, &bonfire_,
                         &fire_, &wizIdle_, &wizRun_})
        UnloadTexture(*t);
}

Rectangle Town::doorRect(int i) const {
    float cx = taverns_[i].x + kHouseW * 0.5f;
    return {cx - 10, kHouseBottom - 39, 20, 39};
}

int Town::doorNearby() const {
    if (state_ != State::Walking || wy_ > kHouseBottom + 22) return -1;
    for (int i = 0; i < static_cast<int>(taverns_.size()); ++i) {
        float cx = taverns_[i].x + kHouseW * 0.5f;
        if (std::fabs(wx_ - cx) <= 14) return i;
    }
    return -1;
}

bool Town::blocked(float x, float y) const {
    // bonfire stones
    return std::fabs(x - bonfireX_) < 14 && y > bonfireY_ - 10 && y < bonfireY_ + 2;
}

float Town::camX() const {
    float c = std::clamp(wx_ - ui::kVirtW * 0.5f, 0.0f, worldW_ - ui::kVirtW);
    return std::floor(c);
}

void Town::leaveTavern() {
    if (state_ != State::Inside && state_ != State::Entering) return;
    state_ = State::Leaving;
    stateT_ = 0;
}

Town::Events Town::update(float dt, const Input& in) {
    Events ev;
    animT_ += dt;

    switch (state_) {
        case State::Walking: {
            float dx = (in.right ? 1.0f : 0.0f) - (in.left ? 1.0f : 0.0f);
            float dy = (in.down ? 1.0f : 0.0f) - (in.up ? 1.0f : 0.0f);
            moving_ = dx != 0 || dy != 0;
            if (dx != 0) faceRight_ = dx > 0;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len > 0) { dx /= len; dy /= len; }

            float nx = wx_ + dx * kWizSpeed * dt;
            if (!blocked(nx, wy_)) wx_ = nx;
            float ny = std::clamp(wy_ + dy * kWizSpeed * dt, kMinFeetY, kMaxFeetY);
            if (!blocked(wx_, ny)) wy_ = ny;
            wx_ = std::max(wx_, 8.0f);

            if (wx_ > worldW_ - 2) {  // off the end of the street
                wx_ = worldW_ - 2;
                ev.exited = true;
                break;
            }
            int d = doorNearby();
            if (d >= 0 && in.up && wy_ <= kMinFeetY + 0.5f) {
                state_ = State::Entering;
                ev.doorOpened = true;
                doorIdx_ = d;
                stateT_ = 0;
                moving_ = true;
            }
            break;
        }
        case State::Entering: {
            stateT_ += dt;
            float cx = taverns_[doorIdx_].x + kHouseW * 0.5f;
            wx_ += (cx - wx_) * std::min(1.0f, dt * 12.0f);
            wy_ = kMinFeetY - 10.0f * (stateT_ / kEnterDur);
            if (stateT_ >= kEnterDur) {
                state_ = State::Inside;
                moving_ = false;
                ev.entered = doorIdx_;
            }
            break;
        }
        case State::Inside:
            break;
        case State::Leaving: {
            stateT_ += dt;
            float k = std::min(1.0f, stateT_ / kLeaveDur);
            wx_ = taverns_[doorIdx_].x + kHouseW * 0.5f;
            wy_ = kMinFeetY - 10.0f + 18.0f * k;
            moving_ = true;
            if (k >= 1.0f) {
                state_ = State::Walking;
                moving_ = false;
                doorIdx_ = -1;
            }
            break;
        }
    }
    return ev;
}

// ---------------------------------------------------------------------------
// drawing

void Town::drawGround(float cam) const {
    int tx0 = static_cast<int>(cam) / 16 - 1, tx1 = tx0 + ui::kVirtW / 16 + 3;
    // grass everywhere (6 solid variants)
    for (int ty = 0; ty < ui::kVirtH / 16 + 1; ++ty)
        for (int tx = tx0; tx < tx1; ++tx) {
            uint32_t h = hash2(tx, ty);
            Rectangle g = {16.0f + 16 * (h % 3), 160.0f + 16 * ((h >> 3) % 2), 16, 16};
            blit(floors_, g, tx * 16.0f, ty * 16.0f);
        }
    // dirt street: solid dirt, then the organic grass edge on top
    // (Floors_Tiles grass "hole" ring: rows 0/1 = top edge, 3/4 = bottom edge)
    for (int row = 0; row < 5; ++row)
        for (int tx = tx0; tx < tx1; ++tx) {
            float x = tx * 16.0f, y = kStreetTop + row * 16.0f;
            uint32_t h = hash2(tx, row + 50);
            Rectangle d = {176.0f + 16 * (h % 3), 160.0f + 16 * ((h >> 3) % 2), 16, 16};
            blit(floors_, d, x, y);
            if (row == 2) continue;
            float col = (row == 0 || row == 4) ? 16.0f * (1 + (h >> 5) % 3) : 32.0f;
            blit(floors_, {col, row * 16.0f, 16, 16}, x, y);
        }
    // a few flowers on the south verge
    for (int tx = tx0; tx < tx1; ++tx) {
        uint32_t h = hash2(tx, 99);
        if (h % 4) continue;
        Rectangle f = {96.0f + 16 * (h % 5), 368.0f + 16 * ((h >> 4) % 3), 16, 16};
        blit(veg_, f, tx * 16.0f, 210.0f + (h >> 8) % 12);
    }
}

void Town::drawTavern(ui::Ui& gui, const Tavern& tv, bool doorOpen) const {
    float x = tv.x;
    blit(walls_, kWallStrip, x, kWallTop);
    blit(roofs_, tv.greenRoof ? kRoofGreen : kRoofBrown, x, kHouseTop);
    float cx = x + kHouseW * 0.5f;
    blit(bprops_, kDoorFrame, cx - 16, kHouseBottom - kDoorFrame.height);
    if (!doorOpen) blit(bprops_, kDoorClosed, cx - 10, kHouseBottom - kDoorClosed.height);
    plaque(gui, tv.sign, static_cast<int>(cx), static_cast<int>(kHouseTop + 40));
}

void Town::drawWizard(float alpha, float t) const {
    Color tint = Fade(WHITE, alpha);
    DrawEllipse(static_cast<int>(wx_), static_cast<int>(wy_) - 1, 8, 3,
                Fade(BLACK, 0.25f * alpha));
    if (moving_) {
        int f = static_cast<int>(t * 14.0f) % kRunFrames;
        Rectangle src = {f * 64.0f, 0, faceRight_ ? 64.0f : -64.0f, 64};
        blit(wizRun_, src, wx_ - 32, wy_ - 64, tint);
    } else {
        int f = static_cast<int>(t * 6.0f) % kIdleFrames;
        Rectangle src = {f * 32.0f, 0, faceRight_ ? 32.0f : -32.0f, 32};
        blit(wizIdle_, src, wx_ - 17 + (faceRight_ ? 0 : 2), wy_ - 34, tint);
    }
}

void Town::draw(ui::Ui& gui, float t) const {
    float cam = camX();
    Camera2D c{};
    c.target = {cam, 0};
    c.zoom = 1.0f;
    BeginMode2D(c);

    drawGround(cam);

    // y-sorted drawables: props, taverns, bonfire, exit sign, wizard
    struct D { float y; int kind; int idx; };
    std::vector<D> ds;
    for (int i = 0; i < static_cast<int>(props_.size()); ++i) ds.push_back({props_[i].y, 0, i});
    for (int i = 0; i < static_cast<int>(taverns_.size()); ++i) ds.push_back({kHouseBottom, 1, i});
    ds.push_back({bonfireY_, 2, 0});
    ds.push_back({kHouseBottom, 3, 0});
    bool wizVisible = state_ != State::Inside;
    bool wizInDoor = state_ == State::Entering || state_ == State::Leaving;
    if (wizVisible && !wizInDoor) ds.push_back({wy_, 4, 0});
    std::stable_sort(ds.begin(), ds.end(), [](const D& a, const D& b) { return a.y < b.y; });

    float view0 = cam - 64, view1 = cam + ui::kVirtW + 64;
    for (const D& d : ds) {
        switch (d.kind) {
            case 0: {
                const Prop& p = props_[d.idx];
                if (p.x < view0 || p.x > view1) break;
                const Texture2D& tex = p.tex == 0 ? trees_ : veg_;
                blit(tex, p.src, p.x - p.src.width * 0.5f, p.y - p.src.height);
                break;
            }
            case 1: {
                const Tavern& tv = taverns_[d.idx];
                if (tv.x + kHouseW < view0 || tv.x > view1) break;
                bool open = wizInDoor && d.idx == doorIdx_;
                drawTavern(gui, tv, open);
                if (open) {  // wizard in the doorway, fading in/out
                    float k = state_ == State::Entering ? 1.0f - stateT_ / kEnterDur
                                                        : stateT_ / kLeaveDur;
                    drawWizard(std::clamp(k, 0.0f, 1.0f), t);
                }
                break;
            }
            case 2: {
                int f = static_cast<int>(t * 8.0f) % 4;
                blit(bonfire_, {f * 32.0f, 0, 32, 32}, bonfireX_ - 16, bonfireY_ - 28);
                blit(fire_, {f * 32.0f, 0, 32, 48}, bonfireX_ - 16, bonfireY_ - 41);
                break;
            }
            case 3: {  // exit signpost at the end of the street
                float sx = worldW_ - 28;
                DrawRectangle(static_cast<int>(sx) - 1, 104, 3, 38, kWoodDk);
                DrawRectangle(static_cast<int>(sx), 104, 1, 38, kWood);
                plaque(gui, "EXIT>", static_cast<int>(sx), 98);
                break;
            }
            case 4:
                drawWizard(1.0f, t);
                break;
        }
    }
    EndMode2D();
}
