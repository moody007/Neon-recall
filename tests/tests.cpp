// Headless regression suite. It exercises the same model used by the game without raylib.
#include <iostream>
#include <stdexcept>

#include "neon_recall/GameModel.hpp"
using namespace neon;
static int checks = 0;
// Keep the project dependency-free while still reporting the expression that first fails.
#define CHECK(x)                          \
    do {                                  \
        ++checks;                         \
        if (!(x))                         \
            throw std::runtime_error(#x); \
    } while (0)
int main() {
    try {
        // Basic topology and first poison refresh invariants.
        std::mt19937_64 r(42);
        Maze m;
        m.generate(r, 1);
        CHECK(m.open(m.start(0)));
        CHECK(m.open(m.start(1)));
        CHECK(m.open(m.treasure()));
        CHECK(!m.solution(0).empty());
        CHECK(!m.solution(1).empty());
        CHECK(m.hasPoisonFreeRoute(0));
        CHECK(m.hasPoisonFreeRoute(1));
        int oldCount = m.poisonCount();
        m.refreshPoison(r, 2, 2);
        CHECK(m.poisonCount() >= oldCount);
        CHECK(m.hasPoisonFreeRoute(0));
        CHECK(m.hasPoisonFreeRoute(1));
        // Exercise many round/frame combinations and protect simulated live positions.
        for (int round = 1; round <= 50; round++) {
            m.generate(r, round);
            for (int frame = 1; frame <= 7; frame++) {
                const auto& s0 = m.solution(0);
                const auto& s1 = m.solution(1);
                std::array<Cell, 2> positions{{s0[s0.size() / 2], s1[s1.size() / 2]}};
                m.refreshPoison(r, round, frame, positions);
                CHECK(m.hasPoisonFreeRoute(0));
                CHECK(m.hasPoisonFreeRoute(1));
                CHECK(!m.poison(positions[0]));
                CHECK(!m.poison(positions[1]));
                CHECK(m.hasPoisonFreeRouteFrom(positions[0]));
                CHECK(m.hasPoisonFreeRouteFrom(positions[1]));
            }
        }
        // Statistical progression check over fixed seeds: hard mazes should be longer/branchier.
        int easyChoices = 0;
        int hardChoices = 0;
        int easyRoutes = 0;
        int hardRoutes = 0;
        for (std::uint64_t seed = 1; seed <= 64; seed++) {
            std::mt19937_64 easyRng(seed);
            std::mt19937_64 hardRng(seed);
            Maze easy;
            Maze hard;
            easy.generate(easyRng, 1);
            hard.generate(hardRng, 12);
            easyChoices += easy.choiceCount();
            hardChoices += hard.choiceCount();
            easyRoutes += (int)easy.solution(0).size() + (int)easy.solution(1).size();
            hardRoutes += (int)hard.solution(0).size() + (int)hard.solution(1).size();
            CHECK(hard.solution(0).size() >= 28);
            CHECK(hard.solution(1).size() >= 28);
        }
        CHECK(hardChoices > easyChoices);
        CHECK(hardRoutes > easyRoutes);
        // State machine validation, including denied moves during reveal and solo-only control.
        GameModel g(12);
        g.start(Mode::Solo, 10, true);
        CHECK(g.phase() == Phase::Reveal);
        CHECK(g.revealDuration() > 2);
        CHECK(g.totalRounds() == 10);
        Cell start = g.players()[0].pos;
        CHECK(!g.move(0, {1, 0}));
        g.forceBlackout();
        CHECK(g.phase() == Phase::Blackout);
        CHECK(!g.move(1, {1, 0}));
        GameModel same(99);
        GameModel same2(99);
        same.start(Mode::Versus, 5);
        same2.start(Mode::Versus, 5);
        CHECK(same.maze().solution(0) == same2.maze().solution(0));
        CHECK(same.maze().poisonCount() == same2.maze().poisonCount());
        GameModel difficulty(7);
        difficulty.start(Mode::Solo, 10);
        CHECK(difficulty.framesPerRound() >= 4);
        for (int tick = 0; tick < 1200 && difficulty.phase() != Phase::RoundResult; tick++) {
            if (difficulty.phase() == Phase::Reveal) {
                difficulty.forceBlackout();
            }
            difficulty.update(.1f);
        }
        CHECK(difficulty.phase() == Phase::RoundResult);
        difficulty.continueFromResult();
        if (difficulty.phase() == Phase::Draft) {
            difficulty.chooseBuff(0);
        }
        CHECK(difficulty.round() > 1);
        CHECK(difficulty.blackoutDuration() < 8.f);
        // Walls must stay locked while poison/reveal frames cycle within one round.
        GameModel versus(5);
        versus.start(Mode::Versus, 3);
        CHECK(versus.totalRounds() == 3);
        auto lockedMaze = versus.maze().topologySignature();
        for (int frame = 1; frame <= versus.framesPerRound(); frame++) {
            CHECK(versus.maze().topologySignature() == lockedMaze);
            versus.forceBlackout();
            for (int tick = 0; tick < 200 && versus.phase() == Phase::Blackout; tick++) {
                versus.update(.1f);
            }
            CHECK(versus.maze().topologySignature() == lockedMaze);
        }
        CHECK(buffCatalogue().size() >= 9);
        // LongMemory would extend the shared reveal, so it must never be offered in versus.
        GameModel draftCheck(81);
        draftCheck.start(Mode::Versus, 5);
        draftCheck.forceBlackout();
        for (int tick = 0; tick < 2000 && draftCheck.phase() != Phase::RoundResult; tick++) {
            if (draftCheck.phase() == Phase::Reveal) {
                draftCheck.forceBlackout();
            }
            draftCheck.update(.1f);
        }
        draftCheck.continueFromResult();
        CHECK(draftCheck.phase() == Phase::Draft);
        for (const auto& b : draftCheck.offers()) {
            CHECK(b.type != BuffType::LongMemory);
        }
        draftCheck.chooseBuff(0);
        for (const auto& b : draftCheck.offers()) {
            CHECK(b.type != BuffType::LongMemory);
        }
        std::cout << "Neon Recall: " << checks << " checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
