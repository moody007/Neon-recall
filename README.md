# Neon Recall: Blackout Heist

A C++20/raylib memory-racing maze game inspired by the readable movement of classic maze chases, with a new central mechanic: **the entire maze disappears when movement begins**.

## The game in one sentence

Memorize a safe neon route to the center vault, then navigate it in total darkness before time, poison, or your rival defeats you.

## Why the design works

The game separates perception from action:

1. **Memory Frame** — movement is locked. The maze, treasure, poison, and players are visible; no solution route is drawn.
2. **Total Blackout** — everything spatial disappears. Players move from memory while the maze still controls collision.
3. **Result** — reaching the center first earns the most points.
4. **Buff Draft** — surviving players adapt their build before a newly generated round.

Each round contains several short reveal/blackout frames. Wall and corridor topology is generated exactly once and locked for that entire round. Only poison relocates on a new frame. Later rounds enforce progressively longer minimum routes to the vault and deliberately add more multi-direction junctions without accepting direct shortcuts. This creates plausible alternate directions and makes key-probing less informative. Hazards are weighted toward useful corridors, junctions, and vault approaches, so they affect meaningful route decisions instead of decorating irrelevant dead ends. A fresh connected maze is generated only when the next numbered round begins.

## Modes

### Solo Memory Run

- Ten progressively harder rounds
- WASD controls
- Score for reaching the vault quickly and retaining health
- Optional buffs; they can be disabled in setup for a pure memory challenge

### Two-player Rival Heist

- Local same-keyboard competition
- Player 1: WASD; Player 2: arrow keys
- Select 3, 5, 7, or 10 rounds
- First to the center earns 1,000 points plus a health bonus
- The rival receives a five-second last-chance window
- Round winner drafts first; the opponent receives a fresh counter-pick
- Longer matches provide more buff choices
- Final leaderboard uses accumulated score

## Hazards and buffs

Poison tiles change every frame. Placement is constrained: every proposed hazard is rejected unless each player retains a completely poison-free route from their current position to the vault. Respawn-to-vault routes are protected too. A poison hit removes a heart, immediately clears that poison tile, and normally returns only that player to spawn; the maze, frame, timer, score, and round continue unchanged.

| Buff | Effect |
|---|---|
| Prism Shield | Blocks the next poison hit |
| Heart Cache | Adds one maximum heart immediately |
| Phase Boots | Increases movement speed by 12% |
| Slow Eclipse | Solo-only: extends future reveal phases by 0.7 seconds |
| Echo Pulse | Unlocks spatial-assistance capability |
| Antidote | Poison still hurts but no longer resets position |
| Second Wind | Prevents one elimination and restores one heart |
| Afterimage | Shows the last three steps faintly in blackout |
| Vault Signal | Adds a faint direction indicator toward the treasure |

Buff offer count scales with selected match length: two choices in short matches, three in medium matches, and four in long matches.

Slow Eclipse is deliberately excluded from local multiplayer. Both players share the same display, so a longer reveal would help both competitors and could not honestly function as an individual reward.

## Build and run on this machine

The verified Release executable is:

```powershell
& ".\build\release\neon_recall.exe"
```

To rebuild from a Visual Studio developer environment:

```powershell
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
ctest --test-dir build/release --output-on-failure
& ".\build\release\neon_recall.exe"
```

The first configure downloads pinned raylib 5.5. Requirements are CMake 3.20+, Ninja, a C++20 compiler, Git/network access for the initial configure, and OpenGL desktop support.

## Architecture

For a file-by-file walkthrough, algorithm explanations, and interview-ready answers, see
[the codebase guide](docs/CODEBASE_GUIDE.md).

- `Maze`: deterministic depth-first maze carving, controlled loop insertion, BFS safe-route calculation, and poison placement
- `GameModel`: reveal/blackout/result/draft/match state machine, movement, health, buffs, scores, rounds, and seeded RNG
- `main.cpp`: raylib input, virtual-resolution rendering, menus, HUD, and local multiplayer presentation
- `tests`: headless checks for connectivity, safe poison placement, phase locking, deterministic generation, mode restrictions, progression, and buff catalogue

The model contains no rendering code and can be tested without opening a window.

## Current scope

Multiplayer is local, not online. The game intentionally hides spatial information during blackout; Afterimage and Vault Signal are earned exceptions. It ships with generated geometry and no copyrighted Pac-Man art, characters, sounds, branding, or maze layouts.
