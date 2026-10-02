# Neon Recall codebase guide

This is the practical reference for reading, explaining, and extending Neon Recall. Start with this file, then follow the suggested reading order below.

## What the project is

Neon Recall is a C++20 maze-racing game built with raylib. During a **reveal** phase, a player studies a procedurally generated maze, the vault, and poison. During **blackout**, the renderer hides those spatial elements, but the simulation still enforces walls, hazards, time, and the win condition.

The core design principle is **randomness with fairness constraints**. Mazes and poison positions vary, but the code must preserve connectivity and at least one poison-free route to the vault.

## Reading order

1. `include/neon_recall/GameModel.hpp` defines the vocabulary and the public API.
2. `src/GameModel.cpp` implements maze generation and all game rules. This is the most important file.
3. `tests/tests.cpp` shows the invariants expected of the model.
4. `src/main.cpp` adapts raylib input to model calls and draws model state.
5. `CMakeLists.txt` and `.github/workflows/ci.yml` show how the program is built and verified.
6. `site/` is an independent static portfolio site; it does not run the game.

## System map

```text
raylib window + input + drawing
             |
             v
        GameModel                 tests
       /         \                  |
      v           v                 v
  Maze rules   player/round state  headless simulation
```

`GameModel` contains no raylib types. The game can therefore be tested without a window or graphics hardware. `main.cpp` owns presentation concerns such as menus, buttons, key polling, colour, and virtual-resolution rendering.

## Core types

| Type | Meaning |
| --- | --- |
| `Cell` | An integer `(x, y)` grid coordinate. |
| `Mode` | `Solo` or local two-player `Versus`. |
| `Phase` | The high-level game state: reveal, blackout, result, draft, or match result. |
| `Player` | Position, health, score, temporary movement data, and persistent buffs. |
| `Buff` | Display metadata and the `BuffType` rule it activates. |
| `Maze` | Grid topology, poison state, starts, vault, and cached shortest solutions. |
| `GameModel` | Owns the RNG, maze, players, timing, scoring, drafts, and phase transitions. |

The maze is a 21 by 15 grid. Its `open_` and `poison_` arrays use a flattened index: `y * Width + x`. This is compact and avoids dynamic allocations for fixed-size board data.

## Round flow

```text
beginRound
  -> Reveal (study; movement denied)
  -> Blackout (movement allowed)
  -> another reveal/blackout frame, or finishRound
  -> RoundResult
  -> Draft or next round
  -> MatchResult after the last round
```

`GameModel::update(dt)` advances the timers and controls transitions. The frontend calls it every render frame. `dt` is clamped to 0.1 seconds so an application stall cannot skip a large amount of game time.

Each round has `4 + round / 3` frames, capped at seven. Maze walls are created once in `beginRound`; only poison is refreshed as a new reveal frame begins. `Maze::topologySignature()` gives tests a compact way to prove the wall layout has not changed.

## Maze generation

`Maze::generate` works in four stages:

1. Clear poison and establish the two lower-corner starts and central vault.
2. Run randomized depth-first carving 80 times. DFS creates a connected, tree-like maze on odd grid coordinates.
3. Use BFS to measure each candidate's shortest route to the vault. Prefer a candidate meeting the round's minimum route length, falling back to the longest available candidate if none qualifies.
4. Add selected wall connectors to form loops. Each possible connector is scored by nearby exits and vault proximity. The code accepts it only if BFS confirms neither player's route falls below the difficulty floor.

DFS is a good fit because it is simple, fast, produces connected mazes, and becomes deterministic under a fixed RNG seed. Loops are added afterward because a perfect DFS maze has a single route between any two cells; loops create believable alternative directions for a memory game.

## Pathfinding and poison fairness

The generic `bfs` helper returns a shortest route in an unweighted grid. It keeps a parent index for every visited cell, then walks backward from the goal to reconstruct the route.

Poison refresh uses a different BFS, `hasPoisonFreeRouteFrom`. It treats poison as blocked. Poison candidates are ranked toward solution corridors, junctions, and vault approaches. Each candidate is added provisionally, and is rejected if it prevents a route:

- from either spawn, or
- from either player's current position.

This is a greedy constrained-placement algorithm. It may place fewer than the target number of poison tiles if further tiles would break the fairness invariant. That trade-off is intentional: a valid, fair round matters more than hitting a poison count.

## Movement, health, and scoring

`GameModel::move` is called with a player index and a cardinal direction. It rejects movement outside blackout, for an invalid player, after completion/elimination, or during a move cooldown. It then checks walls, records recent cells for Afterimage, moves, resolves poison, and finally checks whether the player reached the vault.

Poison is removed immediately when stepped on. A shield consumes the hit; otherwise health falls. Without Antidote, the player returns to their start. Second Wind prevents one elimination and restores one health.

Solo gives 1,000 points plus a time and health bonus for reaching the vault. In versus mode, the first finisher receives the main score and a health bonus; the other player gets a reduced score depending on whether they also reach the vault during the grace period.

## Buff drafting

The catalogue has nine buffs. After a non-final round, solo gets one draft if buffs are enabled; versus gives both players one pick and lets the round winner pick first. The second player receives a newly shuffled offer rather than merely choosing from the winner's leftovers.

`LongMemory` increases reveal duration, so it is excluded from versus mode because reveal time is shared by both players. Offer size depends on match length: two, three, or four choices.

## Frontend responsibilities

`main.cpp` contains a small presentation state machine: Menu, Setup, How, and Game. It uses raylib's immediate-mode drawing API.

- `at(Cell)` converts a model cell to a screen position.
- `button` draws and detects a clickable button.
- The main loop polls keys, calls `GameModel::move` and `update`, then renders the appropriate screen.
- A 1280x720 `RenderTexture2D` is scaled to the actual window size, preserving the intended layout on resize.

During reveal, it draws maze cells, poison, vault, and players. During blackout, it does not draw the maze; only earned assistance such as Afterimage and Vault Signal is visible.

## Tests and verification

`tests/tests.cpp` is intentionally framework-free. Its `CHECK` macro counts assertions and throws on the first failure. It tests:

- start/vault connectivity and non-empty routes;
- poison safety over many rounds and frames;
- increasing route length and junction count at higher difficulty;
- same-seed determinism;
- phase locking and progression;
- topology stability within a round; and
- buff catalogue and multiplayer exclusions.

Run the configured test target with:

```powershell
ctest --test-dir build/debug-mingw --output-on-failure
```

The release build directory may need configuring before it has a registered CTest target.

## Build and delivery

`CMakeLists.txt` builds `neon_core` first. Both the executable and tests link this core. raylib 5.5 is fetched with CMake `FetchContent` only when the graphical game is enabled. The GitHub Actions workflow builds and tests on Windows and Linux. Netlify publishes the independent `site/` directory.

## Honest current limitations

- `Echo Pulse` stores `echoTimer`, but no gameplay or rendering code uses it yet.
- `GameModel::shortest` is a currently unused placeholder that returns an empty route.
- `shake` and `notice` in `main.cpp` are unused presentation scaffolding.
- A wall attempt starts the move cooldown before the wall check, which can make controls feel slightly less responsive.
- The documentation's older timing claims should be updated to match the current formulas.

These are good discussion points in an interview: identify them clearly, explain their impact, and describe the next change you would make rather than claiming a feature is complete.

## Interview-ready answers

**Why BFS instead of A-star?** Every step has equal cost and the board is only 315 cells. BFS is simpler and guarantees a shortest path in `O(V + E)`. A-star would add complexity without a meaningful performance gain.

**How do you make procedural content fair?** I treat fairness as a runtime invariant. Random generation proposes layouts, but BFS rejects hazard placements that remove every safe route from a player’s spawn or current position.

**How is it deterministic?** `GameModel` owns `std::mt19937_64`. The same construction seed and input sequence reproduce generation, poison layouts, and draft offers. The UI intentionally starts a new game with a time-based seed; tests use fixed seeds.

**How would you support multiplayer over a network?** Keep `GameModel` authoritative on the server, send input commands rather than client positions, simulate on fixed ticks, issue a server seed, and replicate snapshots or use deterministic lockstep with reconciliation.

**What would you improve first?** Finish or remove dead feature scaffolding, centralize balance constants, add named test cases/property testing, and add replay support using a seed plus input history.
