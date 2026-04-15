# GGPO + Raylib demo

A minimal two-player networked game demonstrating how to integrate
[GGPO](https://github.com/pond3r/ggpo) rollback netcode into a
C + CMake + [Raylib](https://www.raylib.com/) project.

---

## What this demo shows

| Concern | This demo |
|---|---|
| Game loop | Raylib `WindowShouldClose` loop with GGPO hooks at each step |
| Game state | Flat `GameState` struct — fully serialisable with `memcpy` |
| Input | Raylib `IsKeyDown` → bitmask → `ggpo_add_local_input` |
| Rollback | Transparent: GGPO calls `save/load/advance_frame` callbacks |
| Determinism | `-ffp-contract=off`; own xorshift32 PRNG inside `GameState` |
| Networking | UDP managed entirely by GGPO (no manual socket code needed) |

---

## Dependencies

Both dependencies are fetched automatically by CMake `FetchContent` at
configure time — no manual download step required.

| Library | Source |
|---|---|
| [Raylib 5.0](https://github.com/raysan5/raylib) | `FetchContent` |
| [GGPO](https://github.com/pond3r/ggpo) | `FetchContent` (lib only) |

---

## Build

```sh
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

---

## Run

Open two terminals.  The peer with the **lower** port number is assigned to
player slot 0 (left side, red).

```sh
# Terminal 1 – Player 1
./build/game 7001 127.0.0.1 7002 2

# Terminal 2 – Player 2
./build/game 7002 127.0.0.1 7001 2
```

The third argument is input delay in frames (2 is a good default).

### Controls (local player only)

| Key | Action |
|---|---|
| ← / → | Move left / right |
| ↑ | Jump |
| Space | Attack |

---

## File layout

```
game/
├── CMakeLists.txt        CMake project – FetchContent for Raylib + GGPO
└── src/
    ├── main.c            Raylib window init + GGPO-aware main loop
    ├── game.h / game.c   GameState struct + deterministic game_advance()
    ├── input.h / input.c Raylib key state → uint32_t bitmask
    └── net.h / net.cpp   GGPO session setup + save/load/advance callbacks
```

---

## How GGPO integrates — the short version

### 1. Serialisable `GameState`

Everything the simulation touches lives inside one flat, heap-pointer-free
`GameState` struct (`game.h`).  GGPO's `save_game_state` callback
`malloc`s a buffer and `memcpy`s the struct into it; `load_game_state`
restores it.  This is the entire rollback mechanism.

### 2. Determinism

`game_advance()` (`game.c`) must produce **identical output** on both
machines given the same inputs.  To guarantee this:

- Compile with `-ffp-contract=off` (no fused multiply-add across TUs).
- Never call `rand()` inside `game_advance`; use the `rng_state` xorshift32
  stored inside `GameState` instead.
- Avoid any OS calls, timing queries, or I/O in the simulation path.

### 3. Main loop

```
while frame:
  local_input  = input_collect()          ← Raylib, once per frame
  inputs[]     = net_begin_frame(local)   ← GGPO: may rollback internally
  game_advance(&gs, inputs)               ← deterministic tick
  net_end_frame()                         ← ggpo_advance_frame notification
  render(&gs)                             ← draw confirmed state
  net_idle(0)                             ← flush/receive UDP packets
```

### 4. Rollback path (inside `net_begin_frame`)

When GGPO detects that earlier inputs were mispredicted it:

1. Calls `save_game_state` → snapshot current state.
2. Calls `load_game_state` → restore state to the divergence point.
3. Calls `advance_frame` N times → re-simulate forward.
4. Returns the corrected merged inputs to the caller.

The `advance_frame` callback (`net.cpp`) calls `ggpo_synchronize_input`
for the replayed frame's inputs, then calls `game_advance`, then calls
`ggpo_advance_frame` — mirroring the normal main loop exactly.

### 5. Input delay

`ggpo_set_frame_delay(session, local_handle, 2)` adds 2 frames of
deliberate input lag to the local player.  This widens the prediction
window and reduces visible rollback stutter on moderate-latency connections.
Trade off against feel: 1 frame for LAN, 2-3 for the internet.
