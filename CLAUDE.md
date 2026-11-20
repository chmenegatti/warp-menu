# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A **top-down 16-bit village** for controlling Cloudflare WARP at TOTVS. Native C++ app built on **raylib**. A wizard walks along a dirt street lined with taverns, and **each tavern is one action: walk up into its door to run it**. The actions are connect, disconnect and switching between the 3 vnets (`default`, `TESP2`, `TECE1`). Walking off the right end of the street (the `EXIT>` sign) quits. It has procedural chiptune SFX and optional background music.

The sprites come from **Anokolisa's "Pixel Crawler" free pack** (`assets/pixelcrawler/`, licence in `TERMS.pdf` there). Free use is allowed and credit is optional, but the assets must not be sold or marketed as a product of their own.

It ships to colleagues as packages for Linux (`.deb`/`.rpm`), Windows (`.zip`) and macOS (universal `.dmg`), built by GitHub Actions. User-facing docs are in pt-BR: `README.md`, `docs/install/{linux,windows,macos}.md` and `docs/building.md`, which covers building, packaging and the release process.

GUI-only by design (no terminal fallback). Needs a display and OpenGL 3.3.

## Build & run

```bash
./warp.sh                          # builds on first run (fetches raylib), then runs
cmake -S . -B build && cmake --build build -j   # manual build
./build/warp-menu                  # run binary directly
```

First build downloads & compiles raylib 5.5 via CMake **FetchContent** — no system raylib needed. Subsequent builds are fast. FetchContent applies `cmake/patch-raylib.cmake`: raylib 5.5's `InitWindow()` ignores an `InitPlatform()` failure and segfaults in `rlglInit()`. With the patch it returns, `main` checks `IsWindowReady()` and exits with an error. That happens with no display or no OpenGL 3.3, as on GitHub's macOS VMs. Patch changes only apply to a fresh `build/` (`rm -rf build`).

`warp.sh` configures and builds when `build/warp-menu` is missing, and rebuilds when anything in `src/` or `CMakeLists.txt` is newer than the binary. It resolves its own symlink: `~/.local/bin/warp` is a symlink to it, so `warp` runs the menu from anywhere. The user's previous `warp` binary was kept as `~/.local/bin/warp.old`.

No test suite or lint target; the compile (`-Wall -Wextra`) is the only automated check.

raylib logging is set to warnings/errors only; run with `WARP_DEBUG=1` to get the full INFO log (texture loads, audio device, …).

Visual check without clicking around (and without triggering `warp-cli`): `WARP_SNAPSHOT=/path/out.png ./build/warp-menu` renders ~1s, saves the 320×240 canvas, and exits. `WARP_SNAPSHOT_WIZ=x,y` places the wizard first, in world px at his feet. For example, `392,141` puts him at the door of the 3rd tavern, which shows the tooltip; the camera follows him. Synthetic X key events don't reach the window under Wayland, so use this hook instead.

The CMake file forces miniaudio's ALSA backend (`MA_NO_PULSEAUDIO MA_NO_JACK` on the raylib target) because the PulseAudio backend fails to open a device under PipeWire. Don't remove it.

`wp/` is its own git repo (remote `origin`: `git@github.com:chmenegatti/warp-menu.git`, public; an older copy also sits on remote `gitlab`). It is nested inside the unrelated parent repo `~/c++`, so run git commands from `wp/`.

### Packaging & CI

- **Linux** `.deb` + `.rpm`: built with CPack inside a `rockylinux:9` container by `packaging/linux/build-packages.sh`, which outputs to `build-pkg/`. Locally the container needs `--network host`, because DNS fails in the container while WARP is connected.
  - Linkage: `libstdc++`/`libgcc` are static, and raylib is forced to link the legacy `libGL.so.1` (`OpenGL_GL_PREFERENCE LEGACY`); the default `libOpenGL.so.0` is missing on stock Ubuntu.
  - Runtime deps: GLFW and miniaudio `dlopen` X11/GL/ALSA, so those deps are declared by hand (`CPACK_DEBIAN_PACKAGE_DEPENDS`, `CPACK_RPM_PACKAGE_REQUIRES`). The binary needs glibc ≥ 2.35 (`_dl_find_object` from the static libgcc); RHEL 9 has it backported.
  - Contents: only the `app` install component is packaged (`CPACK_INSTALL_CMAKE_PROJECTS`); otherwise raylib's own install rules leak headers and `libraylib.a` into the packages.
  - Verified: install + `xvfb-run` snapshot on Ubuntu 22.04/24.04, Debian 12, Fedora and Rocky 9.
- **Windows:** MSVC with the `WIN32` subsystem (`/ENTRY:mainCRTStartup`), so no console. The icon comes via `packaging/windows/warp-menu.rc`, and a POST_BUILD step copies `assets/` next to the exe. CI zips `warp-menu.exe` + `assets/`.
- **macOS:** `MACOSX_BUNDLE` with `warp-menu.icns`, and a POST_BUILD step copies `assets/` into `Contents/Resources/assets`. CI builds universal (`arm64;x86_64`, deployment target 11.0), ad-hoc signs the app (`codesign -s -`, required on arm64), renames it to `TOTVS WARP.app` and creates the DMG with `hdiutil`. It is not notarized.
- **CI:** `.github/workflows/build.yml` runs on push/PR. A `v*` tag also creates a GitHub Release with the 4 files. Package filenames come from `project(... VERSION x.y.z)` in `CMakeLists.txt`, so keep the tag and the version in sync.
- **Icons:** generated from the wizard idle frame: `assets/icon.png` (runtime window icon + Linux), `.ico`, `.icns`.

### System dependencies (one-time)
```bash
sudo apt-get install -y cmake build-essential pkg-config \
  libgl1-mesa-dev libx11-dev libxrandr-dev libxi-dev \
  libxinerama-dev libxcursor-dev libasound2-dev
```
Runtime: `warp-cli` must be on PATH (the app shells out to it).

## Architecture

`warp.sh` is just a launcher (build/rebuild-if-needed + `exec build/warp-menu`). All logic is in `src/`:

- **`warp.{hpp,cpp}` — `WarpController`**: the only code that touches `warp-cli`. Holds the `VNETS` name→UUID table (copied from the original script). `popen()` wrappers run `warp-cli`; `status()`/`vnetName()` parse output keyed on the literal strings `Status update` and `Currently selected` (brittle to `warp-cli` format changes — same fragility as the old script). **Actions run on a `std::thread`** with state exposed via `std::atomic<ActionState>` (Idle/Running/Ok/Fail) so the render loop never blocks. `poll()` starts a status refresh **on its own background thread** (`pollWorker_`) at most every 2s, and skips it while an action is running. Each refresh is two `warp-cli` calls, about 0.3s total, which froze the game when they ran on the render thread.
- **`ui.{hpp,cpp}` — `ui::Ui`**: canvas + text primitives. Renders at a fixed **320×240 virtual resolution** into a `RenderTexture2D`, then upscales at an integer scale with `TEXTURE_FILTER_POINT` and letterboxes in `present()`. Provides `text()` (drop shadow, optional letter `spacing`), `textOutlined()` (HUD text over the bright sky), `panel()`, the scanline overlay, and palette constants (`kAccent`, `kGreen`, `kRed`, …).
- **`town.{hpp,cpp}` — `Town`**: the village, which knows nothing about WARP.
  - **World:** one screen tall and wider than the screen. Width is `24 + n*152 + 64`, so it grows with the number of taverns, and a `Camera2D` scrolls it horizontally.
  - **Sprites:** loaded from the sheets as `Rectangle` constants at the top of `town.cpp`. The rects were measured by hand; see the pack README.
  - **Tavern:** wall strip, then roof (brown and green alternate), then door frame, then a closed door. The plaque is drawn procedurally with the pixel font.
  - **Street:** solid dirt tiles with the `Floors_Tiles` grass "hole ring" overlaid as the organic edge: ring rows 0/1 top, 3/4 bottom, with row 2 transparent.
  - **Draw order:** props, taverns, bonfire, exit sign and wizard are y-sorted by their base.
  - **Wizard:** feet-anchored. Run cells are 64×64 and Idle cells are 32×32, offset `(-17,-34)` because the Idle sheet is clipped about 2px at the feet. Flip by negative source width.
  - **Movement:** the wizard is confined to `y ≥ houseBottom+1`, so he can never walk behind the houses.
  - **States:** `Walking → Entering (fade into door) → Inside → Leaving → Walking`. `update()` returns `Events{entered, exited, doorOpened}`. `leaveTavern()` brings the wizard back out. On launch, `main` calls `emergeFrom(i)` for the tavern of the currently selected vnet (unless `WARP_SNAPSHOT_WIZ` is set), so the wizard starts by walking out of that door.
- **`audio.{hpp,cpp}` — `AudioBank`**: SFX are **synthesised procedurally** (square waves with pitch slide + decay, built into `Wave`→`Sound` buffers) — no sound asset files. Background music is an optional file loaded from `assets/music/theme.{ogg,xm,mod,mp3}`, skipped silently if absent.
- **`main.cpp`**: window/audio init, the `MenuItem` list (label = tooltip, sign = roof plaque; tavern index == item index), and the **state machine** `Menu → Confirm (disconnect only) → Running → Result → Menu`. The wizard is steerable only on `Menu`. `entered` calls `launch(idx)`. Going back to `Menu` (a cancelled confirm or a shown result) calls `town.leaveTavern()`. `exited` or Esc quits. Connect / vnet switch close the app on success. It also renders the HUD, the `UP: <label>` door tooltip and the message boxes.

Assets are found at runtime by `findAssets()` in `main.cpp`, relative to the executable (`GetApplicationDirectory()`). The candidates are tried in this order:
1. `<exe>/assets` (Windows / loose copy)
2. `<exe>/../share/warp-menu/assets` (Linux packages)
3. `<exe>/../Resources/assets` (macOS bundle)
4. the compile-time `ASSETS_DIR`, which is the source tree and serves as the dev fallback.

`warp-cli` is resolved once in `warp.cpp`. `WARP_CLI` overrides it; otherwise it is searched in `PATH`, then in Cloudflare's install locations, because a GUI launched from Finder or a menu gets a minimal `PATH`. On Windows it is run via `CreateProcess` with `CREATE_NO_WINDOW`, so no console flashes; on POSIX via `popen`/`system` with single-quoted args. Keep `windows.h` out of any file that includes `raylib.h`, since they clash (`CloseWindow`, `Rectangle`, …).

## Conventions

- Adding a vnet: add to the `VNETS` table in `WarpController` ctor (`warp.cpp`). Items and taverns are built **dynamically** from `warp.vnets()`. The plaque is the upper-cased vnet name, and the street simply gets longer.
- Frame pacing: there is no `SetTargetFPS`; vsync paces the frames, and a 60fps timer cap is applied only if vsync doesn't throttle. A timer cap against a 59.9Hz display caused periodic judder. Game logic runs in fixed 1/60s ticks (main loop), so at `kWizSpeed = 120` the camera scrolls exactly 2px per frame.
- All gameplay coordinates are in virtual pixels (screen 320×240; the town world is wider and scrolled by the camera), not window pixels.
- Long/blocking `warp-cli` calls must stay off the render thread — route them through `WarpController::runAsync`.

## Assets

- `assets/pixelcrawler/` — the subset of the Pixel Crawler sheets in use (renamed; mapping in its `README.md`). The full pack's other sheets (furniture, interiors, mobs, …) are not copied.

- `assets/font/PressStart2P.ttf` — pixel font (OFL). Download:
  `curl -L -o assets/font/PressStart2P.ttf https://github.com/google/fonts/raw/main/ofl/pressstart2p/PressStart2P-Regular.ttf`
  Falls back to raylib's default font if missing.
- `assets/music/theme.{ogg,xm,mod,mp3}` — optional chiptune loop (`.xm`/`.mod` tracker modules play natively in raylib).
