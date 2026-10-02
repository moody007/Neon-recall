#include "neon_recall/GameModel.hpp"

#include <algorithm>
#include <cmath>
#include <queue>

namespace neon {
// The only legal movement vectors. All path finding and movement use this same cardinal set.
// Right, down, left, up. Keeping the order fixed also makes seeded generation reproducible.
static constexpr std::array<Cell, 4> dirs{{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};
// Breadth-first search is optimal for this unweighted grid. The parent array also lets us rebuild
// a shortest path, rather than merely answering whether the vault is reachable.
static std::vector<Cell> bfs(const std::array<bool, Maze::Width * Maze::Height>& open, Cell s,
                             Cell goal) {
    // BFS receives the flat board array, so map each 2-D coordinate to that storage layout.
    auto idx = [](Cell c) { return c.y * Maze::Width + c.x; };
    std::array<int, Maze::Width * Maze::Height> parent;
    // -2 means unseen; -1 marks the root and every other entry points to its predecessor.
    parent.fill(-2);
    std::queue<Cell> q;
    // Seed the frontier with the requested start cell.
    q.push(s);
    parent[idx(s)] = -1;
    while (!q.empty()) {
        // FIFO order is what gives BFS its shortest-path guarantee in an unweighted graph.
        Cell c = q.front();
        q.pop();
        // Once the goal leaves the queue, no later path can be shorter.
        if (c == goal) {
            break;
        }
        // Explore only the four orthogonal neighbours: diagonal moves are not legal in the game.
        for (Cell d : dirs) {
            Cell n{c.x + d.x, c.y + d.y};
            // Skip off-board, wall, and already-visited cells. The bounds check happens before
            // idx(n), so flat-array indexing is always safe.
            if (n.x < 0 || n.y < 0 || n.x >= Maze::Width || n.y >= Maze::Height || !open[idx(n)] ||
                parent[idx(n)] != -2) {
                continue;
            }
            // Remember how we reached n so the answer can contain the actual route.
            parent[idx(n)] = idx(c);
            q.push(n);
        }
    }
    // A goal that was never visited is unreachable from this start.
    if (parent[idx(goal)] == -2) {
        return {};
    }
    // Follow predecessor links backwards, then reverse into start-to-goal order.
    std::vector<Cell> out;
    for (int at = idx(goal); at != -1; at = parent[at]) {
        out.push_back({at % Maze::Width, at / Maze::Width});
    }
    std::reverse(out.begin(), out.end());
    return out;
}
void Maze::generate(std::mt19937_64& rng, int round) {
    // Poison is frame-local; new topology must always begin with a clean hazard board.
    poison_.fill(false);
    // Starts are symmetric lower-corner positions; the vault is fixed in the centre.
    starts_ = {Cell{1, 13}, Cell{19, 13}};
    treasure_ = {10, 7};
    // Reject overly direct layouts. The required shortest route grows each round,
    // so a hard maze cannot place either runner on a near-straight vault corridor.
    // Difficulty increases only up to round 15, keeping the requirement bounded for any match.
    const int minimumRouteCells = 16 + std::min(round, 15);
    // Keep fallbacks: an unlucky random sample must still produce a playable board.
    std::array<bool, Width * Height> bestOpen{};
    std::array<bool, Width * Height> bestQualified{};
    int bestMinimum = 0;
    int bestQualifiedMinimum = 100000;
    // Generate several candidates so generation can enforce a difficulty floor without a
    // potentially unbounded retry loop.
    for (int attempt = 0; attempt < 80; attempt++) {
        // Start each candidate as an all-wall grid.
        open_.fill(false);
        // An explicit stack implements DFS without recursion-depth concerns.
        std::vector<Cell> stack{{1, 13}};
        open_[index(stack.back())] = true;
        // Iterative randomized DFS: visit odd grid cells and carve the wall between neighbours.
        while (!stack.empty()) {
            Cell c = stack.back();
            // Collect unvisited cells two tiles away; the intervening cell becomes a corridor.
            std::vector<Cell> next;
            for (Cell d : dirs) {
                Cell n{c.x + d.x * 2, c.y + d.y * 2};
                // The outer border stays closed, and DFS must not revisit a carved destination.
                if (n.x > 0 && n.y > 0 && n.x < Width - 1 && n.y < Height - 1 && !open_[index(n)]) {
                    next.push_back(n);
                }
            }
            // Dead end: backtrack until an unexplored branch exists.
            if (next.empty()) {
                stack.pop_back();
                continue;
            }
            // The seeded generator chooses a branch, creating replayable randomness.
            Cell n = next[rng() % next.size()];
            open_[index({(c.x + n.x) / 2, (c.y + n.y) / 2})] = true;
            open_[index(n)] = true;
            stack.push_back(n);
        }
        // Connect the even-x central vault and the opposite spawn to the maze.
        // These even-coordinate special cells are opened explicitly because DFS visits odd cells.
        open_[index(treasure_)] = true;
        open_[index({9, 7})] = true;
        open_[index({11, 7})] = true;
        open_[index(starts_[1])] = true;
        open_[index({18, 13})] = true;
        // Measure both competitors: a layout is only as easy as its easier start-to-vault route.
        auto p0 = bfs(open_, starts_[0], treasure_);
        auto p1 = bfs(open_, starts_[1], treasure_);
        int candidateMinimum = std::min((int)p0.size(), (int)p1.size());
        // If every candidate misses the desired floor, preserve the least-direct fallback.
        if (candidateMinimum > bestMinimum) {
            bestMinimum = candidateMinimum;
            bestOpen = open_;
        }
        // Among acceptable boards, prefer the closest fit rather than uncontrolled excess length.
        if (candidateMinimum >= minimumRouteCells && candidateMinimum < bestQualifiedMinimum) {
            bestQualifiedMinimum = candidateMinimum;
            bestQualified = open_;
        }
    }
    // Select an in-range candidate when possible; otherwise select the strongest fallback.
    open_ = bestQualifiedMinimum < 100000 ? bestQualified : bestOpen;
    // Add guaranteed loop connectors rather than merely attempting random walls.
    // Later rounds open more high-value connectors, producing more junctions and
    // plausible alternate directions without sacrificing maze connectivity.
    struct Connector {
        Cell c;
        int score;
        std::uint64_t tie;
    };
    std::vector<Connector> connectors;
    // Count open neighbours to identify potential junctions near a connector.
    auto exits = [&](Cell c) {
        int count = 0;
        for (Cell d : dirs) {
            if (open({c.x + d.x, c.y + d.y})) {
                ++count;
            }
        }
        return count;
    };
    for (int y = 1; y < Height - 1; y++) {
        for (int x = 1; x < Width - 1; x++) {
            Cell c{x, y};
            // Only closed cells can become new connectors.
            if (open(c)) {
                continue;
            }
            Cell a{};
            Cell b{};
            // A connector must bridge two existing corridors, horizontally or vertically.
            bool horizontal = open({x - 1, y}) && open({x + 1, y});
            bool vertical = open({x, y - 1}) && open({x, y + 1});
            if (!horizontal && !vertical) {
                continue;
            }
            if (horizontal) {
                a = {x - 1, y};
                b = {x + 1, y};
            } else {
                a = {x, y - 1};
                b = {x, y + 1};
            }
            // Prefer connectors that form decisions and that matter near the final approach.
            int score = (exits(a) >= 2 ? 4 : 0) + (exits(b) >= 2 ? 4 : 0);
            score += std::max(0, 5 - (std::abs(x - treasure_.x) + std::abs(y - treasure_.y)) / 2);
            // Random tie-breaking avoids a visually identical connector pattern for every seed.
            connectors.push_back({c, score, rng()});
        }
    }
    // Sort descending by strategic score, then ascending by the seeded random tiebreaker.
    std::sort(connectors.begin(), connectors.end(), [](const Connector& a, const Connector& b) {
        return a.score != b.score ? a.score > b.score : a.tie < b.tie;
    });
    // A connector is accepted only if it adds choices without making the vault
    // route shorter than this round's difficulty floor.
    // Later rounds receive more alternatives but the cap prevents an almost-open board.
    int extraConnections = std::min(18, 2 + round);
    int accepted = 0;
    for (const Connector& connector : connectors) {
        if (accepted >= extraConnections) {
            break;
        }
        // Try one connector, then validate it before making it part of the maze.
        open_[index(connector.c)] = true;
        auto p0 = bfs(open_, starts_[0], treasure_);
        auto p1 = bfs(open_, starts_[1], treasure_);
        // Loops are useful only when they do not create an overly direct vault shortcut.
        if ((int)p0.size() < minimumRouteCells || (int)p1.size() < minimumRouteCells) {
            open_[index(connector.c)] = false;
        } else {
            ++accepted;
        }
    }
    // Cache the final shortest routes for hazard weighting and inspection/testing.
    solutions_[0] = bfs(open_, starts_[0], treasure_);
    solutions_[1] = bfs(open_, starts_[1], treasure_);
    refreshPoison(rng, round, 1);
}
void Maze::refreshPoison(std::mt19937_64& rng, int round, int frame) {
    // At round creation, nobody has moved yet, so the starts are the protected live positions.
    refreshPoison(rng, round, frame, starts_);
}
void Maze::refreshPoison(std::mt19937_64& rng, int round, int frame,
                         const std::array<Cell, 2>& playerPositions) {
    // Refresh means relocate: clear the old frame's poison before proposing new tiles.
    poison_.fill(false);
    struct Candidate {
        Cell c;
        int score;
        std::uint64_t tie;
    };
    // Rank strategically relevant floor tiles before the safety checks below decide which can
    // actually be used. This separates intended pressure from final fair placement.
    std::vector<Candidate> candidates;
    for (int y = 1; y < Height - 1; y++) {
        for (int x = 1; x < Width - 1; x++) {
            Cell c{x, y};
            // Never poison walls, win/spawn cells, or a cell currently occupied by a player.
            if (!open(c) || c == treasure_ || c == starts_[0] || c == starts_[1] ||
                c == playerPositions[0] || c == playerPositions[1]) {
                continue;
            }
            int score = 0;
            // Ignore the first few spawn tiles, then favour routes each player is likely to use.
            bool p0 = std::find(solutions_[0].begin() + std::min<size_t>(3, solutions_[0].size()),
                                solutions_[0].end(), c) != solutions_[0].end();
            bool p1 = std::find(solutions_[1].begin() + std::min<size_t>(3, solutions_[1].size()),
                                solutions_[1].end(), c) != solutions_[1].end();
            score += (p0 ? 5 : 0) + (p1 ? 5 : 0);
            int exits = 0;
            for (Cell d : dirs) {
                if (open({x + d.x, y + d.y})) {
                    ++exits;
                }
            }
            // Junction poison creates a memory decision rather than decorating a dead end.
            if (exits >= 3) {
                score += 3;
            }
            if (std::abs(x - treasure_.x) + std::abs(y - treasure_.y) < 5) {
                score += 2;
            }
            candidates.push_back({c, score, rng()});
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.score != b.score ? a.score > b.score : a.tie < b.tie;
    });
    // The target rises with difficulty/frame but cannot exceed candidates or the hard cap of 12.
    int target = std::min((int)candidates.size(), std::min(12, 3 + round + frame));
    int placed = 0;
    // Greedily prefer strategically relevant tiles, but reject every candidate
    // that would remove the last completely safe route from a player's current
    // position or respawn point. This invariant is rechecked after every placement.
    for (const Candidate& candidate : candidates) {
        if (placed >= target) {
            break;
        }
        // Place provisionally; the four reachability checks below are the fairness gate.
        poison_[index(candidate.c)] = true;
        if (!hasPoisonFreeRoute(0) || !hasPoisonFreeRoute(1) ||
            !hasPoisonFreeRouteFrom(playerPositions[0]) ||
            !hasPoisonFreeRouteFrom(playerPositions[1])) {
            // Undo any candidate that would trap a spawn or the actual live player.
            poison_[index(candidate.c)] = false;
        } else {
            ++placed;
        }
    }
}
bool Maze::open(Cell c) const {
    // Public query: invalid cells are treated like walls instead of indexing out of range.
    return valid(c) && open_[index(c)];
}
bool Maze::poison(Cell c) const {
    return valid(c) && poison_[index(c)];
}
void Maze::setPoison(Cell c, bool v) {
    // Only an existing floor tile may carry poison; callers cannot poison a wall by mistake.
    if (valid(c) && open(c)) {
        poison_[index(c)] = v;
    }
}
int Maze::poisonCount() const {
    return (int)std::count(poison_.begin(), poison_.end(), true);
}
int Maze::choiceCount() const {
    // A choice is a floor cell with at least three exits; tests use this as a difficulty proxy.
    int choices = 0;
    for (int y = 0; y < Height; y++) {
        for (int x = 0; x < Width; x++) {
            Cell c{x, y};
            if (!open(c)) {
                continue;
            }
            int exits = 0;
            for (Cell d : dirs) {
                if (open({x + d.x, y + d.y})) {
                    ++exits;
                }
            }
            if (exits >= 3) {
                ++choices;
            }
        }
    }
    return choices;
}
bool Maze::hasPoisonFreeRoute(int player) const {
    // Player indices are validated here because this is a public query used by tests.
    return player >= 0 && player < 2 && hasPoisonFreeRouteFrom(starts_[player]);
}
bool Maze::hasPoisonFreeRouteFrom(Cell origin) const {
    // A poisoned, closed, or out-of-range origin cannot start a safe route.
    if (!valid(origin) || !open(origin) || poison(origin)) {
        return false;
    }
    // A reachability-only BFS: poison behaves like a temporary wall in this search.
    std::array<bool, Width * Height> seen{};
    std::queue<Cell> q;
    q.push(origin);
    seen[index(origin)] = true;
    while (!q.empty()) {
        Cell c = q.front();
        q.pop();
        // Reaching the vault proves at least one hazard-free route exists.
        if (c == treasure_) {
            return true;
        }
        for (Cell d : dirs) {
            Cell n{c.x + d.x, c.y + d.y};
            if (valid(n) && open(n) && !poison(n) && !seen[index(n)]) {
                seen[index(n)] = true;
                q.push(n);
            }
        }
    }
    return false;
}
std::uint64_t Maze::topologySignature() const {
    // FNV-style rolling hash over walls only; poison intentionally does not affect topology.
    std::uint64_t hash = 1469598103934665603ULL;
    for (bool cell : open_) {
        hash ^= cell ? 1ULL : 0ULL;
        hash *= 1099511628211ULL;
    }
    hash ^= static_cast<std::uint64_t>(treasure_.x + treasure_.y * Width);
    return hash;
}
const std::vector<Buff>& buffCatalogue() {
    // Function-local static means this display data is built once and safely shared read-only.
    static const std::vector<Buff> b = {
        // Each row is data, not behaviour. applyBuff below interprets the BuffType.
        {BuffType::Shield, "Prism Shield", "Block the next poison hit.", "COMMON"},
        {BuffType::ExtraHeart, "Heart Cache", "Gain one maximum heart now.", "COMMON"},
        {BuffType::FleetFeet, "Phase Boots", "Move 12% faster in blackout.", "RARE"},
        {BuffType::LongMemory, "Slow Eclipse", "Reveal lasts 0.7 seconds longer.", "RARE"},
        {BuffType::EchoPulse, "Echo Pulse", "Briefly flash nearby walls every few moves.", "RARE"},
        {BuffType::Antidote, "Antidote", "Poison hurts but no longer resets position.", "EPIC"},
        {BuffType::SecondWind, "Second Wind", "Survive one elimination with one heart.", "EPIC"},
        {BuffType::TrailSense, "Afterimage", "Your last three steps glow during blackout.", "RARE"},
        {BuffType::TreasureSense, "Vault Signal", "A faint compass points toward the treasure.",
         "EPIC"}};
    return b;
}
GameModel::GameModel(std::uint64_t seed) : rng_(seed), seed_(seed) {
    // Store the seed as well as initializing the generator so callers/tests can inspect it.
}
void GameModel::start(Mode m, int rounds, bool buffs) {
    // Save match options before creating the first round.
    mode_ = m;
    // Clamp API input to a supported range rather than trusting UI callers.
    totalRounds_ = std::clamp(rounds, 1, 15);
    soloBuffs_ = buffs;
    round_ = 1;
    winner_ = -1;
    // Value-initialize both players to their default health and empty buff state.
    players_ = {};
    players_[0].score = players_[1].score = 0;
    players_[0].roundWins = players_[1].roundWins = 0;
    beginRound();
}
float GameModel::revealDuration() const {
    // Player 0 owns LongMemory only in solo, where a longer shared reveal is fair.
    return std::max(1.8f, 3.6f - round_ * .12f) + players_[0].memoryBonus;
}
float GameModel::blackoutDuration() const {
    // This duration has a floor so late rounds remain possible rather than becoming instant loss.
    return std::max(4.5f, 7.5f - round_ * .15f);
}
void GameModel::beginRound() {
    // A new numbered round is the only time walls are regenerated.
    frame_ = 1;
    // Add a reveal/blackout cycle every three rounds, capped for match pacing.
    framesPerRound_ = std::min(7, 4 + round_ / 3);
    maze_.generate(rng_, round_);
    // Reset only round-local player state. Scores and acquired buffs intentionally persist.
    for (int i = 0; i < 2; i++) {
        // Spawn from the maze rather than hard-coding positions in the game state.
        players_[i].pos = players_[i].start = maze_.start(i);
        // Health resets each round, while maxHealth reflects retained Heart Cache buffs.
        players_[i].health = players_[i].maxHealth;
        players_[i].finished = players_[i].eliminated = false;
        players_[i].moveCooldown = 0;
        players_[i].recent.clear();
    }
    phase_ = Phase::Reveal;
    phaseTime_ = revealDuration();
    firstFinisher_ = -1;
    finishGrace_ = 0;
    roundMessage_ = "Study the maze and choose your own safe route";
}
void GameModel::forceBlackout() {
    // This guard prevents Space or a test call from skipping unrelated result/draft states.
    if (phase_ == Phase::Reveal) {
        phase_ = Phase::Blackout;
        phaseTime_ = blackoutDuration();
        roundMessage_ = "BLACKOUT - trust your memory";
    }
}
void GameModel::update(float dt) {
    // Clamping avoids a debugger pause or window stall jumping over several game transitions.
    dt = std::clamp(dt, 0.f, .1f);
    // Cooldowns tick in all phases so a player cannot carry a stale movement lock unexpectedly.
    for (auto& p : players_) {
        p.moveCooldown = std::max(0.f, p.moveCooldown - dt);
    }
    // Reveal has one transition: its timer expires into blackout.
    if (phase_ == Phase::Reveal) {
        phaseTime_ -= dt;
        if (phaseTime_ <= 0) {
            forceBlackout();
        }
    } else if (phase_ == Phase::Blackout) {
        // Blackout can end through completion, elimination, grace expiry, or time expiry.
        phaseTime_ -= dt;
        if (firstFinisher_ >= 0) {
            finishGrace_ -= dt;
        }
        // In solo, one player's terminal state ends the round. Versus waits for both unless a
        // first finisher's grace window expires.
        bool both = mode_ == Mode::Solo ? players_[0].finished || players_[0].eliminated
                                        : (players_[0].finished || players_[0].eliminated) &&
                                              (players_[1].finished || players_[1].eliminated);
        if (both || (firstFinisher_ >= 0 && finishGrace_ <= 0)) {
            finishRound();
        } else if (phaseTime_ <= 0) {
            // A timed-out frame returns to reveal with new poison but the exact same walls.
            if (frame_ < framesPerRound_) {
                ++frame_;
                maze_.refreshPoison(rng_, round_, frame_, {players_[0].pos, players_[1].pos});
                phase_ = Phase::Reveal;
                phaseTime_ = revealDuration();
                roundMessage_ = "Poison shifted - safe routes preserved from both players";
            } else {
                finishRound();
            }
        }
    }
}
bool GameModel::move(int who, Cell d) {
    // The model validates input independently of main.cpp, making this API safe for tests/future UIs.
    if (phase_ != Phase::Blackout || who < 0 || who > (mode_ == Mode::Solo ? 0 : 1)) {
        return false;
    }
    Player& p = players_[who];
    // Terminal players and players recovering from a prior movement attempt cannot act.
    if (p.finished || p.eliminated || p.moveCooldown > 0) {
        return false;
    }
    // Direction is a cardinal grid delta, so this is the next proposed cell.
    Cell n{p.pos.x + d.x, p.pos.y + d.y};
    // Speed buffs shorten the delay between attempts.
    p.moveCooldown = .135f / p.speedMultiplier;
    // A wall blocks movement but does not remove health.
    if (!maze_.open(n)) {
        return false;
    }
    // Keep a fixed, oldest-first history for Afterimage rendering.
    p.recent.push_back(p.pos);
    if (p.recent.size() > 3) {
        p.recent.erase(p.recent.begin());
    }
    // Commit the valid movement before resolving the tile effect.
    p.pos = n;
    // A poison tile disappears after it is triggered, preventing a second damage event from it.
    if (maze_.poison(n)) {
        maze_.setPoison(n, false);
        // Shield fully absorbs this event; otherwise poison costs one health.
        if (p.shields > 0) {
            --p.shields;
        } else {
            --p.health;
            // Antidote removes only the respawn consequence, not the health cost.
            if (!p.antidote) {
                p.pos = p.start;
            }
            // Second Wind turns one lethal hit into a one-health respawn.
            if (p.health <= 0) {
                if (p.secondWind) {
                    p.secondWind = false;
                    p.health = 1;
                    p.pos = p.start;
                } else {
                    p.eliminated = true;
                }
            }
        }
    }
    // Check final position because poison may have respawned the player away from n.
    if (p.pos == maze_.treasure()) {
        p.finished = true;
        // First arrival starts the versus grace window; solo resolves on the next update.
        if (firstFinisher_ < 0) {
            firstFinisher_ = who;
            finishGrace_ = mode_ == Mode::Solo ? 0.f : 5.f;
        }
    }
    return true;
}

void GameModel::finishRound() {
    // Centralize outcomes so timeout, elimination, and completion use one scoring transition.
    phase_ = Phase::RoundResult;
    int first = firstFinisher_;  // -1 means no player reached the vault.
    if (mode_ == Mode::Solo) {
        // Solo rewards speed (remaining phase time) and health retained.
        if (players_[0].finished) {
            players_[0].score += 1000 + (int)(phaseTime_ * 20) + players_[0].health * 100;
            players_[0].roundWins++;
            roundMessage_ = "Vault reached - memory intact!";
        } else {
            roundMessage_ = "The vault faded before you arrived";
        }
    } else {
        // Versus rewards first place most, while retaining a smaller comeback/participation score.
        if (first >= 0) {
            players_[first].score += 1000 + players_[first].health * 100;
            players_[first].roundWins++;
            int other = 1 - first;
            if (players_[other].finished) {
                players_[other].score += 600;
            } else {
                players_[other].score += 200;
            }
            roundMessage_ = "Player " + std::to_string(first + 1) + " reached the vault first!";
        } else {
            players_[0].score += 100;
            players_[1].score += 100;
            roundMessage_ = "The darkness defeated both runners";
        }
    }
}

void GameModel::continueFromResult() {
    // Ignore a stale/incorrect UI click outside the result state.
    if (phase_ != Phase::RoundResult) {
        return;
    }
    // The last round produces a leaderboard instead of more maze content.
    if (round_ >= totalRounds_) {
        phase_ = Phase::MatchResult;
        winner_ = players_[0].score == players_[1].score
                      ? -1
                      : (players_[0].score > players_[1].score ? 0 : 1);
        return;
    }
    // Pure solo runs deliberately skip the persistent-buff draft.
    if (mode_ == Mode::Solo && !soloBuffs_) {
        ++round_;
        beginRound();
        return;
    }
    prepareDraft();
}

void GameModel::prepareDraft() {
    phase_ = Phase::Draft;
    offers_.clear();
    // A winner picks first; a no-winner round alternates priority by round number.
    draftingPlayer_ = mode_ == Mode::Solo ? 0 : (firstFinisher_ >= 0 ? firstFinisher_ : round_ % 2);
    draftsRemaining_ = mode_ == Mode::Solo ? 1 : 2;
    // LongMemory is excluded from versus because its longer reveal would help both players.
    auto cat = buffCatalogue();
    std::vector<int> ids;
    for (int i = 0; i < (int)cat.size(); i++) {
        if (mode_ == Mode::Solo || cat[i].type != BuffType::LongMemory) {
            ids.push_back(i);
        }
    }
    // Shuffle indices to retain immutable catalogue ordering while producing a deterministic offer.
    std::shuffle(ids.begin(), ids.end(), rng_);
    int offerCount = totalRounds_ <= 3 ? 2 : totalRounds_ <= 7 ? 3 : 4;
    for (int i = 0; i < offerCount; i++) {
        offers_.push_back(cat[ids[i]]);
    }
}

void GameModel::applyBuff(Player& p, BuffType t) {
    // Rules mutate model state only; the frontend later decides how visible buffs are drawn.
    switch (t) {
        case BuffType::Shield: p.shields++; break;
        case BuffType::ExtraHeart: p.maxHealth++; break;
        case BuffType::FleetFeet: p.speedMultiplier *= 1.12f; break;
        case BuffType::LongMemory: p.memoryBonus += .7f; break;
        case BuffType::EchoPulse: p.echoTimer = 1; break;
        case BuffType::Antidote: p.antidote = true; break;
        case BuffType::SecondWind: p.secondWind = true; break;
        case BuffType::TrailSense: p.trailSense = true; break;
        case BuffType::TreasureSense: p.treasureSense = true; break;
    }
}

bool GameModel::chooseBuff(int i) {
    // Validate phase and offer index before a UI card selection changes game state.
    if (phase_ != Phase::Draft || i < 0 || i >= (int)offers_.size()) {
        return false;
    }
    applyBuff(players_[draftingPlayer_], offers_[i].type);
    // Solo has one pick; versus toggles to its opponent for a fresh counter-offer.
    if (--draftsRemaining_ > 0) {
        draftingPlayer_ = 1 - draftingPlayer_;
        auto cat = buffCatalogue();
        std::vector<int> ids;
        for (int n = 0; n < (int)cat.size(); n++) {
            if (cat[n].type != BuffType::LongMemory) {
                ids.push_back(n);
            }
        }
        std::shuffle(ids.begin(), ids.end(), rng_);
        int count = totalRounds_ <= 3 ? 2 : totalRounds_ <= 7 ? 3 : 4;
        offers_.clear();
        for (int n = 0; n < count; n++) {
            offers_.push_back(cat[ids[n]]);
        }
        return true;
    }
    // All picks are complete; increment before generation so difficulty uses the new round number.
    ++round_;
    beginRound();
    return true;
}

std::vector<Cell> GameModel::shortest(Cell, Cell) const {
    // Reserved for a future model-level path query. Maze currently owns cached spawn solutions.
    return {};
}
}  // namespace neon
