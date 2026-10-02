// Domain model for Neon Recall. This header deliberately has no raylib dependency so that all
// rules can be reused by the UI and tested in a headless executable.
#pragma once
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace neon {
// One integer grid coordinate. Defaulted equality makes cells easy to compare in paths/tests.
struct Cell {
    int x{};
    int y{};
    friend bool operator==(Cell, Cell) = default;
};
// Game configuration and the explicit lifecycle states owned by GameModel.
enum class Mode { Solo, Versus };
enum class Phase { Reveal, Blackout, RoundResult, Draft, MatchResult };
enum class BuffType {
    Shield,
    ExtraHeart,
    FleetFeet,
    LongMemory,
    EchoPulse,
    Antidote,
    SecondWind,
    TrailSense,
    TreasureSense
};
// UI-facing metadata paired with the rule identifier used by applyBuff.
struct Buff {
    BuffType type{};
    std::string name;
    std::string description;
    std::string rarity;
};
// Mutable state for one runner. Persistent buff fields survive into the next round.
struct Player {
    Cell pos{};
    Cell start{};
    int health{3};
    int maxHealth{3};
    int shields{};
    int score{};
    int roundWins{};
    float moveCooldown{};
    bool finished{};
    bool eliminated{};
    bool antidote{};
    bool secondWind{};
    bool trailSense{};
    bool treasureSense{};
    float speedMultiplier{1.f};
    float echoTimer{};
    float memoryBonus{};
    std::vector<Cell> recent;
};

// Stores board topology and hazards. A maze remains topologically fixed for an entire round;
// only poison_ is refreshed between memory frames.
class Maze {
   public:
    static constexpr int Width = 21;
    static constexpr int Height = 15;
    // Generate connected topology whose route length/choice density scale with round.
    void generate(std::mt19937_64& rng, int round);
    // Re-place poison without changing walls. The second overload protects live positions too.
    void refreshPoison(std::mt19937_64& rng, int round, int frame);
    void refreshPoison(std::mt19937_64& rng, int round, int frame,
                       const std::array<Cell, 2>& playerPositions);
    bool open(Cell c) const;
    bool poison(Cell c) const;
    void setPoison(Cell c, bool value);
    // Reachability checks used as fairness invariants during poison placement.
    bool hasPoisonFreeRoute(int player) const;
    bool hasPoisonFreeRouteFrom(Cell origin) const;
    // Stable hash of walls/vault for tests that assert the maze does not mutate mid-round.
    std::uint64_t topologySignature() const;
    const std::vector<Cell>& solution(int player) const {
        return solutions_[player];
    }
    Cell treasure() const {
        return treasure_;
    }
    Cell start(int player) const {
        return starts_[player];
    }
    int poisonCount() const;
    int choiceCount() const;

   private:
    // Translate a valid 2-D cell into a flat fixed-size-array offset.
    int index(Cell c) const {
        return c.y * Width + c.x;
    }
    bool valid(Cell c) const {
        return c.x >= 0 && c.y >= 0 && c.x < Width && c.y < Height;
    }
    std::array<bool, Width * Height> open_{};    // Traversable floor; false represents a wall.
    std::array<bool, Width * Height> poison_{};
    Cell treasure_{10, 7};
    std::array<Cell, 2> starts_{{{1, 13}, {19, 13}}};
    std::array<std::vector<Cell>, 2> solutions_;
};

// The deterministic, renderer-independent game state machine and rules engine.
class GameModel {
   public:
    explicit GameModel(std::uint64_t seed = 1);
    // Reset a match. rng_ is seeded by construction, which keeps tests reproducible.
    void start(Mode mode, int totalRounds, bool soloBuffs = true);
    // Advance timers and perform phase transitions; called once per render frame by the UI.
    void update(float dt);
    // Attempt one cardinal grid movement; returns true only when a move was accepted.
    bool move(int player, Cell direction);
    void continueFromResult();
    bool chooseBuff(int offerIndex);
    Mode mode() const {
        return mode_;
    }
    Phase phase() const {
        return phase_;
    }
    int round() const {
        return round_;
    }
    int totalRounds() const {
        return totalRounds_;
    }
    float phaseTime() const {
        return phaseTime_;
    }
    float revealDuration() const;
    float blackoutDuration() const;
    const Maze& maze() const {
        return maze_;
    }
    const std::array<Player, 2>& players() const {
        return players_;
    }
    const std::vector<Buff>& offers() const {
        return offers_;
    }
    int draftingPlayer() const {
        return draftingPlayer_;
    }
    int winner() const {
        return winner_;
    }
    int firstFinisher() const {
        return firstFinisher_;
    }
    std::uint64_t seed() const {
        return seed_;
    }
    int frame() const {
        return frame_;
    }
    int framesPerRound() const {
        return framesPerRound_;
    }
    std::string roundMessage() const {
        return roundMessage_;
    }
    bool soloBuffs() const {
        return soloBuffs_;
    }
    // UI/test shortcut used by Space and deterministic test simulations.
    void forceBlackout();

   private:
    // Internal transition helpers keep public UI operations small and state-safe.
    void beginRound();
    void finishRound();
    void prepareDraft();
    void applyBuff(Player&, BuffType);
    std::vector<Cell> shortest(Cell from, Cell to) const;
    std::mt19937_64 rng_;  // Single random stream: same seed + actions gives same simulation.
    std::uint64_t seed_{};
    Mode mode_{Mode::Solo};
    Phase phase_{Phase::Reveal};
    int round_{1};
    int totalRounds_{5};
    int frame_{1};
    int framesPerRound_{4};
    int firstFinisher_{-1};
    int winner_{-1};
    int draftingPlayer_{0};
    int draftsRemaining_{};
    float phaseTime_{};
    float finishGrace_{};
    bool soloBuffs_{true};
    Maze maze_;
    std::array<Player, 2> players_{};
    std::vector<Buff> offers_;
    std::string roundMessage_;
};
const std::vector<Buff>& buffCatalogue();
}  // namespace neon
