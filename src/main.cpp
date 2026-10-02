// raylib adapter: this file owns windows, input, menus, and drawing. Rules remain in GameModel.
#include <algorithm>
#include <cmath>
#include <ctime>
#include <string>

#include "neon_recall/GameModel.hpp"
#include "raylib.h"
using namespace neon;
namespace {
// Fixed virtual canvas. Rendering it to a texture first preserves layout on a resized window.
// Logical canvas dimensions; every UI coordinate below is expressed in this coordinate system.
constexpr int W = 1280, H = 720;
constexpr float TS = 36, MX = 78, MY = 114;
// Presentation-only states. GameModel separately owns the gameplay phase state.
enum class Screen { Menu, Setup, How, Game };
// Named palette values prevent repeated raw RGBA literals for the core neon identity.
Color cyan{67, 232, 224, 255};
Color pink{255, 78, 151, 255};
Color gold{255, 207, 83, 255};
Color ink{7, 10, 24, 255};
Color panelC{16, 24, 49, 245};
// Convert a grid cell from the model into the centre of its screen tile.
Vector2 at(Cell c) {
    return {MX + c.x * TS + TS / 2, MY + c.y * TS + TS / 2};
}
void panel(Rectangle r, Color c = panelC) {
    // Every card uses the same rounded dark fill and subtle outline.
    DrawRectangleRounded(r, .08f, 10, c);
    DrawRectangleRoundedLinesEx(r, .08f, 10, 1, {72, 94, 139, 190});
}
// Draw an immediate-mode button and return whether it was clicked this frame.
bool button(Rectangle r, const char* text, Color accent = cyan) {
    bool h = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRounded(r, .2f, 8, h ? Color{38, 61, 99, 255} : Color{25, 40, 73, 255});
    DrawRectangleRoundedLinesEx(r, .2f, 8, 2, h ? accent : Color{70, 92, 130, 255});
    int fs = 20;
    DrawText(text, (int)(r.x + (r.width - MeasureText(text, fs)) / 2),
             (int)(r.y + (r.height - fs) / 2), fs, RAYWHITE);
    return h && IsMouseButtonReleased(0);
}
void heading(const char* right) {
    // Shared title bar used by setup, help, and gameplay screens.
    DrawText("NEON RECALL", 46, 28, 34, cyan);
    DrawText("BLACKOUT HEIST", 48, 65, 16, pink);
    DrawText(right, W - MeasureText(right, 18) - 42, 47, 18, {167, 185, 221, 255});
}
const char* phaseName(Phase p) {
    // HUD labels for active phases. Result/draft overlays provide their own larger headings.
    return p == Phase::Reveal     ? "MEMORY FRAME"
           : p == Phase::Blackout ? "TOTAL BLACKOUT"
                                  : "ROUND COMPLETE";
}
}  // namespace
int main() {
    // Ask raylib for resize support and a vsync-friendly presentation loop before window creation.
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(W, H, "Neon Recall: Blackout Heist");
    SetWindowMinSize(960, 540);
    // Escape is used for the in-game pause menu, not raylib's default close-window shortcut.
    SetExitKey(KEY_NULL);
    // Render to the fixed virtual canvas so arbitrary window sizes preserve the designed layout.
    RenderTexture2D target = LoadRenderTexture(W, H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    // These values hold UI choices until the Start button creates a new GameModel match.
    Screen screen = Screen::Menu;
    Mode selectedMode = Mode::Solo;
    int matchRounds = 5;
    bool soloBuffs = true;
    bool paused = false;
    // The interactive version receives a time-based seed; tests use explicit fixed seeds instead.
    GameModel game((uint64_t)std::time(nullptr));
    float flash = 0;
    float shake = 0;
    std::string notice;
    // Reconstructing the model creates a fresh random stream and clears all prior match state.
    auto begin = [&] {
        game = GameModel((uint64_t)std::time(nullptr));
        game.start(selectedMode, selectedMode == Mode::Solo ? 10 : matchRounds, soloBuffs);
        paused = false;
        screen = Screen::Game;
    };
    // Input updates the model first; drawing below is a pure presentation of current state.
    while (!WindowShouldClose()) {
        // Clamp frame delta just as the model does, avoiding large visual timer jumps after stalls.
        float dt = std::min(GetFrameTime(), .1f);
        // flash/shake are presentation timers and never influence game rules.
        if (flash > 0) {
            flash -= dt;
        }
        if (shake > 0) {
            shake -= dt;
        }
        // Do not send gameplay input while another screen or the pause overlay is active.
        if (screen == Screen::Game && !paused) {
            // Space is an explicit player choice to sacrifice remaining study time for speed.
            if (game.phase() == Phase::Reveal && IsKeyPressed(KEY_SPACE)) {
                game.forceBlackout();
            }
            // Polling is intentionally limited to blackout: GameModel also enforces this rule.
            if (game.phase() == Phase::Blackout) {
                // Each key maps to a cardinal Cell delta. GameModel decides whether it is legal.
                bool moved = false;
                moved |= IsKeyDown(KEY_W) && game.move(0, {0, -1});
                moved |= IsKeyDown(KEY_S) && game.move(0, {0, 1});
                moved |= IsKeyDown(KEY_A) && game.move(0, {-1, 0});
                moved |= IsKeyDown(KEY_D) && game.move(0, {1, 0});
                // Player 2 input exists only in local-versus mode.
                if (game.mode() == Mode::Versus) {
                    moved |= IsKeyDown(KEY_UP) && game.move(1, {0, -1});
                    moved |= IsKeyDown(KEY_DOWN) && game.move(1, {0, 1});
                    moved |= IsKeyDown(KEY_LEFT) && game.move(1, {-1, 0});
                    moved |= IsKeyDown(KEY_RIGHT) && game.move(1, {1, 0});
                }
                // A short cyan flash confirms accepted movement despite blackout hiding the avatar.
                if (moved) {
                    flash = .07f;
                }
            }
            // Timers transition reveal/blackout frames after input is processed for this frame.
            game.update(dt);
        }
        // Escape toggles the overlay even after rules input has been skipped due to pause state.
        if (screen == Screen::Game && IsKeyPressed(KEY_ESCAPE)) {
            paused = !paused;
        }
        // Draw on the fixed virtual canvas, then letterbox-scale it to the real window.
        BeginTextureMode(target);
        ClearBackground(ink);
        // Decorative background grid establishes a consistent cyber-neon visual language.
        for (int x = 0; x < W; x += 64) {
            DrawLine(x, 0, x, H, {11, 17, 37, 255});
        }
        for (int y = 0; y < H; y += 64) {
            DrawLine(0, y, W, y, {11, 17, 37, 255});
        }
        // MENU: static product pitch and navigation into setup/help.
        if (screen == Screen::Menu) {
            heading("A maze remembered in the dark");
            DrawCircleGradient(900, 365, 300, {39, 25, 93, 210}, Fade(ink, 0));
            DrawText("THE MAZE VANISHES.", 72, 150, 45, RAYWHITE);
            DrawText("YOUR MEMORY DOESN'T.", 72, 205, 45, gold);
            DrawText(
                "Study a neon route. Then race blind through shifting poison\nto steal the vault "
                "before time - or your rival - gets there.",
                75, 280, 21, {188, 203, 229, 255});
            if (button({75, 395, 330, 56}, "Play the heist", cyan)) {
                screen = Screen::Setup;
            }
            if (button({75, 466, 158, 48}, "How it works", pink)) {
                screen = Screen::How;
            }
            if (button({247, 466, 158, 48}, "Quit")) {
                break;
            }
            panel({750, 135, 440, 430});
            DrawText("THE TWIST", 790, 175, 24, pink);
            DrawText("1", 800, 235, 40, cyan);
            DrawText("LOOK", 860, 242, 22, RAYWHITE);
            DrawText("Walls, treasure and poison are visible.", 860, 270, 15, {171, 189, 218, 255});
            DrawText("2", 800, 325, 40, pink);
            DrawText("BLACKOUT", 860, 332, 22, RAYWHITE);
            DrawText("Everything important disappears.", 860, 360, 15, {171, 189, 218, 255});
            DrawText("3", 800, 415, 40, gold);
            DrawText("REMEMBER + RACE", 860, 422, 22, RAYWHITE);
            DrawText("Every round redraws the danger.", 860, 450, 15, {171, 189, 218, 255});
        } else if (screen == Screen::Setup) {
            // SETUP: choose mode and mode-specific configuration before calling begin().
            heading("Choose your heist");
            DrawText("GAME MODE", 80, 130, 22, gold);
            Rectangle solo{80, 180, 520, 130};
            Rectangle versus{680, 180, 520, 130};
            panel(solo, selectedMode == Mode::Solo ? Color{24, 58, 75, 255} : panelC);
            panel(versus, selectedMode == Mode::Versus ? Color{68, 34, 73, 255} : panelC);
            DrawText("SOLO MEMORY RUN", 115, 215, 27, cyan);
            DrawText("Beat escalating mazes and chase a high score.", 115, 260, 17, RAYWHITE);
            DrawText("2-PLAYER RIVAL HEIST", 715, 215, 27, pink);
            DrawText("WASD versus arrows on the same keyboard.", 715, 260, 17, RAYWHITE);
            if (IsMouseButtonReleased(0) && CheckCollisionPointRec(GetMousePosition(), solo)) {
                selectedMode = Mode::Solo;
            }
            if (IsMouseButtonReleased(0) && CheckCollisionPointRec(GetMousePosition(), versus)) {
                selectedMode = Mode::Versus;
            }
            panel({210, 350, 860, 200});
            // Versus offers match length; solo offers an optional no-buff challenge mode.
            if (selectedMode == Mode::Versus) {
                DrawText("MATCH LENGTH", 260, 385, 20, RAYWHITE);
                const int opts[] = {3, 5, 7, 10};
                for (int i = 0; i < 4; i++) {
                    Rectangle r{260 + i * 150.f, 430, 120, 48};
                    if (button(r, TextFormat("%d rounds", opts[i]),
                               opts[i] == matchRounds ? gold : cyan)) {
                        matchRounds = opts[i];
                    }
                }
                DrawText("Longer matches unlock 4 buff choices; short matches stay punchy.", 260,
                         505, 16, {174, 190, 219, 255});
            } else {
                DrawText("POWER-UPS", 260, 385, 20, RAYWHITE);
                if (button({260, 430, 250, 48}, soloBuffs ? "Enabled" : "Disabled",
                           soloBuffs ? cyan : pink)) {
                    soloBuffs = !soloBuffs;
                }
                DrawText("Solo is a 10-round survival run. Buffs are optional.", 260, 505, 16,
                         {174, 190, 219, 255});
            }
            if (button({420, 590, 210, 52}, "Back", pink)) {
                screen = Screen::Menu;
            }
            if (button({650, 590, 210, 52}, "Start", gold)) {
                begin();
            }
        } else if (screen == Screen::How) {
            // HOW: in-game explanation, kept separate from rules so the model remains UI-free.
            heading("Thirty-second briefing");
            panel({85, 115, 1110, 525});
            DrawText("THE LOOP", 125, 150, 25, gold);
            DrawText(
                "1. MEMORY FRAME - movement is locked. Study the maze, cyan route, vault and "
                "poison.",
                125, 200, 19, RAYWHITE);
            DrawText("2. BLACKOUT - maze, player and hazards vanish. Move entirely from memory.",
                     125, 240, 19, RAYWHITE);
            DrawText(
                "3. REACH THE CENTER - poison costs a heart and usually throws you back to spawn.",
                125, 280, 19, RAYWHITE);
            DrawText(
                "4. DRAFT A BUFF - shields, hearts, speed, echoes, antidotes and treasure sense.",
                125, 320, 19, RAYWHITE);
            DrawText("SOLO: WASD", 125, 385, 22, cyan);
            DrawText("VERSUS: Player 1 uses WASD   |   Player 2 uses arrow keys", 125, 430, 22,
                     pink);
            DrawText(
                "The first player to the vault earns 1,000 points. The rival gets a "
                "five-second\nlast-chance window. Longer matches offer more buff choices.",
                125, 490, 18, {184, 200, 225, 255});
            if (button({520, 570, 240, 48}, "Got it", cyan)) {
                screen = Screen::Menu;
            }
        } else if (screen == Screen::Game) {
            // GAME: draw the model's current phase and provide phase-appropriate controls.
            heading(TextFormat("Round %d / %d", game.round(), game.totalRounds()));
            if (button({1115, 62, 125, 30}, "MENU", pink)) {
                paused = true;
            }
            Phase ph = game.phase();
            // Cache phase predicates for readable render branches below.
            bool reveal = ph == Phase::Reveal;
            bool black = ph == Phase::Blackout;
            panel({38, 98, 850, 574}, black ? Color{4, 6, 13, 255} : Color{13, 22, 45, 255});
            if (reveal) {
                // Reveal intentionally exposes all navigation information for memorization.
                for (int y = 0; y < Maze::Height; y++) {
                    for (int x = 0; x < Maze::Width; x++) {
                        Cell c{x, y};
                        Vector2 p = at(c);
                        // Open cells are drawn as corridor tiles; closed cells are solid walls.
                        if (game.maze().open(c)) {
                            DrawRectangle((int)(p.x - TS / 2 + 2), (int)(p.y - TS / 2 + 2), TS - 4,
                                          TS - 4, {22, 39, 70, 255});
                            DrawRectangleLines((int)(p.x - TS / 2 + 2), (int)(p.y - TS / 2 + 2),
                                               TS - 4, TS - 4, {37, 69, 105, 255});
                        } else {
                            DrawRectangle((int)(p.x - TS / 2), (int)(p.y - TS / 2), TS, TS,
                                          {8, 12, 27, 255});
                        }
                        // Poison is visible only while memorization is permitted.
                        if (game.maze().poison(c)) {
                            DrawCircleV(p, 9, pink);
                            DrawText("X", (int)p.x - 5, (int)p.y - 8, 16, ink);
                        }
                    }
                }
                Vector2 vault = at(game.maze().treasure());
                DrawCircleV(vault, 18, gold);
                DrawCircleLines((int)vault.x, (int)vault.y, 25, gold);
                DrawText("$", (int)vault.x - 7, (int)vault.y - 13, 25, ink);
                for (int i = 0; i < (game.mode() == Mode::Solo ? 1 : 2); i++) {
                    Vector2 p = at(game.players()[i].pos);
                    DrawCircleV(p, 13, i ? pink : cyan);
                    DrawText(TextFormat("P%d", i + 1), (int)p.x - 8, (int)p.y - 7, 12, ink);
                }
            } else if (black) {
                // Blackout hides maze, players, poison, and vault; only acquired assistance leaks
                // spatial information back to the player.
                DrawText("TOTAL BLACKOUT", 265, 315, 42, {50, 55, 78, 255});
                DrawText("THE MAZE IS STILL THERE", 288, 365, 18, {34, 40, 62, 255});
                for (int who = 0; who < (game.mode() == Mode::Solo ? 1 : 2); who++) {
                    const Player& p = game.players()[who];
                    // Afterimage renders the model's bounded recent-position history.
                    if (p.trailSense) {
                        for (size_t i = 0; i < p.recent.size(); i++) {
                            DrawCircleV(at(p.recent[i]), 3 + i * 2,
                                        Fade(who ? pink : cyan, .18f + i * .12f));
                        }
                    }
                    // Vault Signal computes a visual compass direction from player to vault.
                    if (p.treasureSense) {
                        Vector2 center{470, 390};
                        Vector2 v = at(game.maze().treasure());
                        Vector2 pos = at(p.pos);
                        float ang = std::atan2(v.y - pos.y, v.x - pos.x);
                        DrawCircleLines((int)center.x + (who ? 80 : -80), (int)center.y + 80, 18,
                                        Fade(who ? pink : cyan, .35f));
                        DrawLineEx({center.x + (who ? 80 : -80), center.y + 80},
                                   {center.x + (who ? 80 : -80) + std::cos(ang) * 15,
                                    center.y + 80 + std::sin(ang) * 15},
                                   2, Fade(who ? pink : cyan, .4f));
                    }
                }
            }
            panel({910, 98, 330, 574});
            DrawText(phaseName(ph), 940, 125, 22, reveal ? gold : black ? pink : cyan);
            // Active-play HUD shows timing, frame, scores, health, shields, and keyboard mapping.
            if (ph == Phase::Reveal || ph == Phase::Blackout) {
                DrawText(TextFormat("FRAME %d / %d", game.frame(), game.framesPerRound()), 940, 158,
                         16, cyan);
                DrawText(TextFormat("%.1f", game.phaseTime()), 940, 184, 38, RAYWHITE);
                DrawText(reveal ? "Same maze. Only poison has shifted."
                                : "Move now. Same maze returns soon.",
                         940, 228, 14, {183, 198, 224, 255});
                DrawText("MAZE LOCKED FOR THIS ROUND", 940, 248, 12, gold);
                DrawLine(940, 270, 1210, 270, {61, 77, 112, 255});
                for (int i = 0; i < (game.mode() == Mode::Solo ? 1 : 2); i++) {
                    const Player& p = game.players()[i];
                    int yy = 290 + i * 140;
                    DrawText(TextFormat("PLAYER %d", i + 1), 940, yy, 20, i ? pink : cyan);
                    DrawText(TextFormat("Score %d", p.score), 940, yy + 30, 17, RAYWHITE);
                    DrawText("HEALTH", 940, yy + 58, 13, {160, 178, 207, 255});
                    for (int h = 0; h < p.maxHealth; h++) {
                        DrawCircle(1010 + h * 22, yy + 64, 7,
                                   h < p.health ? (i ? pink : cyan) : Color{49, 54, 74, 255});
                    }
                    DrawText(TextFormat("Shields %d", p.shields), 940, yy + 87, 14, gold);
                }
                DrawText(game.mode() == Mode::Solo ? "WASD to move" : "P1 WASD  |  P2 ARROWS", 940,
                         605, 15, RAYWHITE);
            }
            if (ph == Phase::RoundResult) {
                // Result overlay pauses gameplay progression until Continue is clicked.
                DrawRectangle(0, 0, W, H, {4, 7, 18, 220});
                panel({250, 155, 780, 410});
                DrawText("ROUND COMPLETE", 460, 195, 29, gold);
                DrawText(game.roundMessage().c_str(),
                         640 - MeasureText(game.roundMessage().c_str(), 22) / 2, 255, 22, RAYWHITE);
                DrawText(TextFormat("P1  %d points", game.players()[0].score), 370, 325, 23, cyan);
                if (game.mode() == Mode::Versus) {
                    DrawText(TextFormat("P2  %d points", game.players()[1].score), 700, 325, 23,
                             pink);
                }
                if (button({515, 455, 250, 54},
                           game.round() == game.totalRounds() ? "Final leaderboard" : "Continue",
                           gold)) {
                    game.continueFromResult();
                }
            }
            if (ph == Phase::Draft) {
                // Draft overlay presents the offer data that GameModel generated for this player.
                DrawRectangle(0, 0, W, H, {4, 7, 18, 240});
                DrawText(TextFormat("PLAYER %d: CHOOSE A BUFF", game.draftingPlayer() + 1), 390,
                         100, 30, game.draftingPlayer() ? pink : cyan);
                DrawText("The round winner drafts first; the rival still gets a counter-pick.", 360,
                         142, 17, {183, 198, 224, 255});
                auto& o = game.offers();
                float cardW = o.size() == 4 ? 275 : 340;
                float start = (W - cardW * o.size() - (o.size() - 1) * 18) / 2;
                for (size_t i = 0; i < o.size(); i++) {
                    Rectangle r{start + i * (cardW + 18), 210, cardW, 330};
                    panel(r, i == 0 ? Color{29, 43, 73, 255} : panelC);
                    DrawText(o[i].rarity.c_str(), (int)r.x + 22, (int)r.y + 24, 14,
                             o[i].rarity == "EPIC"   ? pink
                             : o[i].rarity == "RARE" ? gold
                                                     : cyan);
                    DrawText(o[i].name.c_str(), (int)r.x + 22, (int)r.y + 70, 22, RAYWHITE);
                    DrawText(o[i].description.c_str(), (int)r.x + 22, (int)r.y + 115, 15,
                             {183, 198, 224, 255});
                    if (button({r.x + 22, r.y + 255, r.width - 44, 48}, "Take buff",
                               game.draftingPlayer() ? pink : cyan)) {
                        game.chooseBuff((int)i);
                    }
                }
            }
            if (ph == Phase::MatchResult) {
                // Match result is terminal for the current model; buttons return to presentation flow.
                DrawRectangle(0, 0, W, H, {4, 7, 18, 245});
                panel({280, 120, 720, 500});
                DrawText(game.mode() == Mode::Solo ? "RUN COMPLETE" : "FINAL LEADERBOARD", 425, 165,
                         31, gold);
                if (game.mode() == Mode::Solo) {
                    DrawText(TextFormat("Score  %d", game.players()[0].score), 500, 245, 32, cyan);
                    DrawText(TextFormat("Vaults reached  %d", game.players()[0].roundWins), 500,
                             300, 21, RAYWHITE);
                } else {
                    int lead = game.winner();
                    DrawText(lead < 0 ? "DRAW" : TextFormat("PLAYER %d WINS", lead + 1),
                             lead < 0 ? 580 : 500, 240, 34, lead == 1 ? pink : cyan);
                    DrawText(TextFormat("1  Player 1       %d", game.players()[0].score), 430, 315,
                             22, cyan);
                    DrawText(TextFormat("2  Player 2       %d", game.players()[1].score), 430, 360,
                             22, pink);
                }
                if (button({405, 500, 220, 50}, "Play again", cyan)) {
                    screen = Screen::Setup;
                }
                if (button({655, 500, 220, 50}, "Main menu", pink)) {
                    screen = Screen::Menu;
                }
            }
            if (paused) {
                // Pause is a frontend overlay: update() is simply not called while it is visible.
                DrawRectangle(0, 0, W, H, {4, 7, 18, 230});
                panel({420, 205, 440, 300});
                DrawText("MATCH PAUSED", 520, 245, 30, gold);
                DrawText("Your current round will be lost if you leave.", 472, 292, 15,
                         {183, 198, 224, 255});
                if (button({500, 340, 280, 48}, "Resume game", cyan)) {
                    paused = false;
                }
                if (button({500, 410, 280, 48}, "Quit to homepage", pink)) {
                    paused = false;
                    screen = Screen::Menu;
                }
            }
        }
        // The flash is composed last so it sits above every screen/overlay element.
        if (flash > 0) {
            DrawRectangle(0, 0, W, H, Fade(cyan, flash * 2));
        }
        EndTextureMode();
        BeginDrawing();
        ClearBackground(BLACK);
        // Uniform scale maintains aspect ratio; the unused dimension becomes black letterboxing.
        float s = std::min(GetScreenWidth() / (float)W, GetScreenHeight() / (float)H);
        DrawTexturePro(
            target.texture, {0, 0, (float)W, (float)-H},
            {(GetScreenWidth() - W * s) / 2, (GetScreenHeight() - H * s) / 2, W * s, H * s}, {0, 0},
            0, WHITE);
        EndDrawing();
    }
    // Release GPU resource before closing the raylib window.
    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}
