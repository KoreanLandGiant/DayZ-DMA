# KLG — DayZ External (DMA Edition)

> External overlay for DayZ built on FPGA-based Direct Memory Access. No injection, no hooks, no drivers — all memory I/O happens over PCIe via a hardware DMA card.

---

## How It Works

This project is split into two completely independent systems that run in parallel and share state through a synchronized cache:

- **DMA Layer** — reads and writes game memory via FPGA hardware
- **Render Layer** — draws an overlay using DirectX 9 + ImGui

These two systems never directly interact with the target process's execution. There is no code injection, no DLL loading, and no kernel driver touching the target.

---

## Memory Access — DMA via MemProcFS

All memory operations go through an **FPGA card sitting on the PCIe bus**, using the [MemProcFS](https://github.com/ufrisk/MemProcFS) / VMMDLL library. The card reads and writes physical memory directly — the target process has no visibility into this.

Reads are performed using **scatter batching** — a MemProcFS feature that groups multiple non-contiguous memory requests into a single PCIe transaction, minimizing round-trip overhead:

```cpp
auto s = ScatterBegin();
ScatterAdd(s, address1, &value1);
ScatterAdd(s, address2, &value2);
ScatterExecute(s); // one PCIe round-trip for all reads
```

---

## Threading — Fetch/Draw Separation

Data collection and rendering are deliberately decoupled to keep the overlay smooth regardless of DMA read latency.

| Thread | Responsibility | Cadence |
|---|---|---|
| **Update thread** | All DMA I/O — entity tables, pointer chains, names, positions, health | 250ms cycle |
| **Main/render thread** | Reads cached data only, drives ImGui + DX9 render loop | Every frame |

A mutex protects the shared entity list during the swap between threads. The render thread never blocks on DMA — it always has a fresh-enough cache to draw from.

Item data has an additional dedicated cache (`cachedItemsForDraw`) on a 500ms throttle, since item tables are large and expensive to fully resolve every cycle.

---

## Entity System

The game world exposes several entity tables accessible from a base world pointer:

- **Near table** — close-range entities
- **Far table** — distant entities
- **Slow table** — low-priority entities
- **Item table** — ground items

The update thread walks all of these every cycle, classifying each entity by type name string (e.g. `"dayzplayer"`, `"zombiefemale"`), resolving pointer chains for skeletons, visual states, health, network IDs, and item quality. Results are written into a `std::vector<player_t>` that the render thread consumes read-only.

---

## Overlay Rendering

The overlay is a **borderless `WS_POPUP` Win32 window** sized and positioned to exactly cover the game window. Key properties:

- **DirectX 9 Extended** (`IDirect3D9Ex`) — `D3DFMT_A8R8G8B8` back buffer, cleared to full transparent (`ARGB 0,0,0,0`) each frame
- **DWM glass** — `DwmExtendFrameIntoClientArea` with full margins enables composited per-pixel transparency
- **ImGui** — `imgui_impl_win32` + `imgui_impl_dx9` backends for all UI and draw list rendering
- **Click-through when hidden** — `WS_EX_LAYERED | WS_EX_TRANSPARENT` passes all mouse input to the game; these flags are stripped when the menu is open so ImGui can receive input

All ESP geometry (boxes, lines, text, skeletons) is rendered via `ImGui::GetOverlayDrawList()` — drawn directly onto the surface outside any ImGui window.

---

## Configuration

Settings are serialized to binary `.dat` files via flat struct field dumps in declaration order. Multiple named profiles are supported, listed via a directory scan at runtime. Hotkeys are stored as part of the same binary blob.

---

## Project Structure

```
.
├── main.cpp          # Entry point, Win32 message loop, crash handler
├── overlay.cpp       # Window creation, DX9 init, render loop, ImGui GUI
├── entity.cpp        # Entity iteration, ESP drawing, caches, silent aim
├── SDK.h             # Game SDK — coordinate math, bone resolution, world-to-screen
├── memory.h          # DMA read/write wrappers, scatter helpers
├── offsets.h         # All memory offsets as #define constants
├── globals.h         # Shared global state, entity structs, config vars
├── settings.h        # Binary config save/load
└── DMALib/           # MemProcFS VMMDLL wrapper and scatter implementation
```

---

## Dependencies

- [MemProcFS / VMMDLL](https://github.com/ufrisk/MemProcFS) — DMA interface
- [Dear ImGui](https://github.com/ocornut/imgui) — UI and overlay draw lists
- DirectX 9 SDK — rendering backend
- Windows 10/11 — `DwmExtendFrameIntoClientArea`, `WDA_EXCLUDEFROMCAPTURE`

---

## Build

Built with **MSVC** on Windows. Requires the DirectX 9 SDK headers and the compiled MemProcFS static library linked in. No other external dependencies.
