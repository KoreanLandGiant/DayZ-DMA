# KLG — DayZ External (DMA Edition)

> External overlay for DayZ built on FPGA-based Direct Memory Access. No injection, no hooks, no drivers — all memory I/O happens over PCIe via a hardware DMA card.

---

## Features

### ESP
- **Player ESP** — boxes, skeletons, names, health, distance, weapon
- **Zombie ESP** — infected detection with distance filter
- **Animal ESP** — wildlife tracking
- **Vehicle ESP** — cars and transports
- **Item ESP** — ground loot with quality filtering (Pristine / Worn / Damaged / Badly Damaged / Ruined)
- **Inventory ESP** — peek other players' inventory contents with adjustable FOV
- **Per-category distance sliders** — tune visibility range independently for each ESP type
- **Customizable colors** — full RGBA color pickers for every ESP category

### Aimbot
- **Silent Aim** — bullet trajectory manipulation, no visible crosshair movement
- **Zombie silent aim** toggle
- **Configurable hotkey** — bind any key to activate
- **FOV-limited targeting** with visual FOV circle

### Exploits
- **Speed Hack** — movement speed multiplier
- **Loot Teleport** — pull items directly to you
- **Corpse Teleport** — pull corpses to your location
- **Force Third Person** — bypass server 1PP restriction (client-side)

### World
- **Remove Grass** — disables grass rendering for visibility
- **Time of Day** — override server hour
- **Eye Accommodation** — disable brightness/contrast adaptation

### DMA & Performance
- **FPGA scatter reads** — multiple memory requests batched into a single PCIe round-trip
- **Single-thread architecture** — reads → logic → render on one thread, eliminating mutex contention and cross-thread cache thrashing
- **`-norefresh` mode** — skips MemProcFS periodic TLB/cache invalidation for buttery-smooth gameplay with zero random freezes
- **`NOCACHE` scatter flag** — every read goes fresh to the FPGA, bypassing MemProcFS content cache
- **Static pointer chain caching** — skeleton / animClass / matrixClass pointers cached across fast ticks
- **F3 manual cache refresh** — full TLB + memory cache drop on demand, for catching entities in newly-committed VA regions after long sessions

### UI / QoL
- **F1** toggles menu on/off (anti-keylogger friendly — no INSERT/HOME)
- **Multiple named config profiles** — save/load binary settings files
- **Rebindable hotkeys** for silent aim, teleport, and menu
- **Hidden from screen capture** via `WDA_EXCLUDEFROMCAPTURE`
- **Click-through overlay** when menu is hidden

---

## Renderer

The overlay is rendered with **Direct3D 9 Extended (`IDirect3D9Ex`)** + **Dear ImGui**.

- **Device:** `D3DDEVTYPE_HAL`, `D3DCREATE_HARDWARE_VERTEXPROCESSING`, `D3DFMT_A8R8G8B8` back buffer
- **Transparency:** `DwmExtendFrameIntoClientArea` with full margins for composited per-pixel alpha
- **ImGui backends:** `imgui_impl_win32` + `imgui_impl_dx9`
- **Window:** borderless `WS_POPUP` with `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST`, sized to exactly cover the game window
- **Draw surface:** all ESP geometry drawn via `ImGui::GetOverlayDrawList()` — boxes, lines, text, and skeletons rendered directly onto the transparent surface outside any ImGui window

---

## Architecture

```
┌─────────────────────────────────────────────┐
│  Single Thread (reads → logic → render)    │
│                                             │
│  update_entities()   → FPGA scatter reads   │
│        ↓                                    │
│  classify / filter   → populate g_entities  │
│        ↓                                    │
│  draw_esp()          → ImGui + DX9 present  │
└─────────────────────────────────────────────┘
```

All DMA I/O, entity classification, and rendering happen on a single thread with no locks. A background keyboard listener handles hotkeys (F1 menu toggle, F3 manual DMA refresh).

---

## Dependencies

- [MemProcFS / VMMDLL](https://github.com/ufrisk/MemProcFS) — FPGA DMA interface
- [Dear ImGui](https://github.com/ocornut/imgui) — UI and overlay draw lists
- DirectX 9 SDK — rendering backend
- Windows 10/11 — requires `DwmExtendFrameIntoClientArea` and `WDA_EXCLUDEFROMCAPTURE`

---

## Project Structure

```
.
├── main.cpp          # Entry point, Win32 message loop, crash handler
├── overlay.cpp       # Window creation, DX9 init, render loop, ImGui GUI
├── entity.cpp        # Entity iteration, ESP drawing, silent aim, keyboard listener
├── SDK.h             # Game SDK — coordinate math, bones, world-to-screen
├── memory.h          # DMA read/write wrappers, scatter helpers
├── offsets.h         # All memory offsets as #define constants
├── globals.h         # Shared global state, entity structs, config vars
├── settings.h        # Binary config save/load
└── DMALib/           # MemProcFS VMMDLL wrapper and scatter implementation
```

---

## Build

Built with **MSVC** on Windows (Visual Studio 2022). Requires the DirectX 9 SDK headers and the compiled MemProcFS static library linked in. No other external dependencies.
