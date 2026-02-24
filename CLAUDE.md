# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**OpenCiv4** is a clean-room, open-source, 64-bit engine replacement for Civilization IV: Beyond the Sword — similar in spirit to OpenMW (Morrowind) or OpenRCT2. It reads original game assets from a legal Civ4 installation and uses the officially released CvGameCoreDLL C++ source for all game logic. It is NOT a decompilation.

## Build Commands

**Prerequisites**: MSVC 14.29 (VS2019 Build Tools, v142 toolset), CMake, Windows SDK 10.0.19041.0

```bash
# Configure (first time, or after CMakeLists.txt changes):
do_configure.bat
# This runs vcvars64.bat then cmake with NMake Makefiles

# Build (from Developer Command Prompt or after vcvars64.bat):
cd build && nmake

# Or configure + build in one shot:
do_configure.bat && cd build && nmake
```

The executable is `build/openciv4.exe`. SDL2.dll, SDL2_ttf.dll, and fonts are auto-copied to the build dir by CMake post-build steps.

**Running**:
```bash
build/openciv4.exe              # Normal mode: 1 human + 3 AI, OpenGL window
build/openciv4.exe --headless   # AI-only, no window (Phase 0 test mode)
build/openciv4.exe --screenshot # Auto-capture screenshot and exit
build/openciv4.exe --test-nif   # Test NIF model parsing
build/openciv4.exe --scan-nif-types  # Scan all NIF block types in FPK
```

There are no automated tests or linter — validation is done by successful compilation and runtime behavior.

## Self-Management Rules

Claude is the primary developer, project manager, and architect. Gemini serves as a reviewer and second opinion. Follow these rules every session:

1. **Read CLAUDE.md and MEMORY.md at session start** to restore context
2. **Read `docs/ai-collab/messages.md`** for any updates from Gemini or the user
3. **Write an entry to `docs/ai-collab/messages.md` EVERY SINGLE TURN** — not at session end, EVERY TURN. Log what the user asked and what you did/said. Non-negotiable.
4. **Update documentation after every major milestone** — don't wait to be asked
5. **Keep MEMORY.md under 200 lines** — link to topic files for details
6. **Track progress in the task list** and update task status as you work
7. **Test compiles after every change** — never leave the build broken
8. **Explain technical decisions in plain language** — the user is not a coder
9. **Commit to git with clear messages** after each working milestone
10. **When starting a session**, check TaskList and pick up where you left off
11. **Always use `py` not `python`** on this machine (Windows Store stub issue)

## AI Collaboration Workflow

Claude (primary dev) and Gemini (reviewer) collaborate via `docs/ai-collab/messages.md`. The user bridges both terminals.

- **`docs/ai-collab/messages.md`** — running conversation log. Every entry tagged `[Claude]`, `[Gemini]`, or `[User]` with date. Include the user's prompt that triggered your work.
- Always read `messages.md` before starting work. Write an entry EVERY SINGLE TURN — what the user asked, what you did/said.
- Be specific about file changes (names, line ranges, what changed).
- Flag disagreements directly with technical reasoning.

## Project Phases

| Phase | Goal | Status |
|-------|------|--------|
| Phase 0 | Headless mode: 64-bit gamecore, AI-only game | **COMPLETE** |
| Phase 1 | Map viewer: SDL2/OpenGL renderer with terrain, features, HUD | **COMPLETE** |
| Phase 2 | Playable game: units, cities, UI, turn flow | **COMPLETE** |
| Phase 3 | Polish: animations, diplomacy, multiplayer, modding | Not started |

## Architecture

### The Interface Layer (core design pattern)

The CvGameCoreDLL never talks to the engine directly. It talks through **13 abstract interface classes** (all pure virtual). Our engine provides concrete implementations in `src/engine/stubs/`. The DLL accesses them through a single global pointer `gDLL` (type `CvDLLUtilityIFaceBase*`).

`StubUtilityIFace` is the master hub — it owns instances of every sub-interface and returns pointers from `get*IFace()` accessors. Key sub-interfaces with real implementations (not just stubs):

| Interface | Our Implementation |
|-----------|-------------------|
| `CvDLLXmlIFaceBase` | `XmlParser.cpp` — pugixml-based XML loader |
| `CvDLLFAStarIFaceBase` | `RealFAStarIFace.cpp` + `FAStar.cpp` — A* pathfinding |
| `CvDLLInterfaceIFaceBase` | `StubInterfaceIFace` in `StubInterfaces.h` — real selection tracking for Phase 2 |

### Threading Model

Two threads: **game thread** (runs `CvGame::update()` loop) and **render thread** (SDL2/OpenGL event loop + drawing).

- **Game → Render**: `MapSnapshot` (mutex-protected snapshot of all plot/player data, copied each tick)
- **Render → Game**: `GameCommand` queue (mutex-protected, polled by game thread)
- Game thread tick rate: 16ms during human turn, 1ms during AI turns

### Rendering Pipeline

`GLRenderer` (OpenGL 3.3 Core via glad) handles all drawing:
- **Terrain**: Multi-pass (base texture → corner-weighted edge blend → features → detail textures → rivers)
- **3D models**: NIF parser (`NifLoader.cpp`) loads Gamebryo models from FPK archives, rendered via `Mesh.cpp` with GLSL shaders
- **2D HUD**: SDL_ttf → GL texture for text, batch quad renderer (dynamic VBO, 8 floats/vert)
- **Fog of war**: visibility levels 0/1/2 with brightness dimming

### Asset Pipeline

`AssetManager` → `FPKArchive` → `DDSLoader` / `NifLoader`

Assets come from Civ4's layered filesystem. **Lookup order: BTS → Warlords → Base game**.

| Layer | Path (relative to install root) |
|-------|------|
| **Base game** | `Assets\` (Art0.FPK = 317MB, core XML/Python/sounds) |
| **Warlords** | `Warlords\Assets\` |
| **Beyond the Sword** | `Beyond the Sword\Assets\` |

Art formats: DDS textures (DXT1/3/5), NIF models (Gamebryo v20.0.0.4), FPK archives (Firaxis packages).

## Key Directories

| Path | Purpose |
|------|---------|
| `src/engine/` | New engine code (main, renderer, asset loading, stubs) |
| `src/engine/stubs/` | Interface implementations (`StubInterfaces.h`, `StubUtilityIFace.cpp`, `StubLinkFixes.cpp`) |
| `src/engine/xml/` | XML parser (`XmlParser.cpp`, pugixml) |
| `src/engine/pathfinding/` | A* pathfinding (`FAStar.cpp`, `RealFAStarIFace.cpp`) |
| `src/gamecore/` | Forked CvGameCoreDLL source (modified for 64-bit) |
| `third_party/` | SDL2, SDL2_ttf, glad (OpenGL loader) |
| `assets/fonts/` | DejaVu Sans TTF |
| `tools/` | Python analysis scripts (DDS, NIF, FPK inspection) |

## Key Source Files

| File | Role |
|------|------|
| `src/engine/main.cpp` | Entry point: wires interfaces, initializes game, runs game thread + render loop |
| `src/engine/GLRenderer.cpp` | OpenGL renderer: terrain, 3D models, HUD, all input handling |
| `src/engine/MapSnapshot.h` | Thread-safe game state snapshot (`PlotData`, `PlayerInfo`, `GameCommand`) |
| `src/engine/stubs/StubInterfaces.h` | All 13 interface stubs (some with real behavior) |
| `src/engine/stubs/StubUtilityIFace.cpp` | Master interface: text translation, file enumeration, memory management |
| `src/engine/NifLoader.cpp` | NIF model parser (37 block types, ~1400 lines) |
| `src/engine/AssetManager.cpp` | FPK + DDS texture loading, 3-layer asset search |

## Critical Gotchas

- **Civ4 uses SQUARE tiles, NOT hexagons** — hexes were introduced in Civ5
- **All 18 player/team slots must be initialized** (not just active players) — uninitialized slots cause crashes in `isVassal()` etc.
- **Never use button art (`art/interface/buttons/...`) for map rendering** — those are tiny UI thumbnails. Use real textures from Art0.FPK.
- **`OPENCIV4` preprocessor define** gates all our modifications in gamecore source via `#ifdef OPENCIV4` / `#ifndef OPENCIV4`
- **`/Zc:forScope-`** is required — the original SDK code leaks for-loop variables (MSVC 2003 behavior)
- **Excluded gamecore files** are stubbed in `StubLinkFixes.cpp`: CvEventReporter, CvDLLPython, CvGameInterface, CvGameCoreDLL, Cy*.cpp, FAssert, FDialogTemplate, CvDllTranslator
- **`Py_BUILD_CORE`** prevents Python.h from auto-linking python24.lib (we don't use Python runtime)
- **FAStar pathfinding**: BTS callback convention is `(parent, node, data, ...)` where `node` (2nd param) is the current tile

## External References

| Resource | Path |
|----------|------|
| Original BTS SDK | `C:\Civ4SDK\beyond-the-sword-sdk-develop\CvGameCoreDLL\` |
| K-Mod (reference) | `C:\Civ4SDK\Civ4-K-Mod-master\CvGameCoreDLL\` |
| Game install | `C:\Program Files (x86)\Steam\steamapps\common\Sid Meier's Civilization IV Beyond the Sword\` |
| Boost 1.32 headers | `C:\Civ4SDK\beyond-the-sword-sdk-develop\CvGameCoreDLL\Boost-1.32.0\` |
| Python 2.4 headers | `C:\Civ4SDK\beyond-the-sword-sdk-develop\CvGameCoreDLL\Python24\` |

## Important Constraints

1. **Clean-room only** — never decompile or reverse-engineer the Civ4 .exe
2. **Asset loading** — the engine points to a legal Civ4 BTS installation for assets
3. **The game logic source is legally released** by Firaxis as part of the SDK
4. **User is not a coder** — explain technical concepts in plain language
