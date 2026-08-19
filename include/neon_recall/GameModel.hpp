#pragma once
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace neon {
struct Cell { int x{},y{}; friend bool operator==(Cell,Cell)=default; };
enum class Mode { Solo, Versus };
enum class Phase { Reveal, Blackout, RoundResult, Draft, MatchResult };
enum class BuffType { Shield, ExtraHeart, FleetFeet, LongMemory, EchoPulse, Antidote, SecondWind, TrailSense, TreasureSense };
struct Buff { BuffType type{}; std::string name,description,rarity; };
struct Player {
  Cell pos{},start{}; int health{3},maxHealth{3},shields{},score{},roundWins{};
  float moveCooldown{}; bool finished{},eliminated{},antidote{},secondWind{},trailSense{},treasureSense{};
  float speedMultiplier{1.f},echoTimer{},memoryBonus{}; std::vector<Cell> recent;
};

class Maze {
public:
  static constexpr int Width=21,Height=15;
  void generate(std::mt19937_64& rng,int round);
  void refreshPoison(std::mt19937_64& rng,int round,int frame);
  bool open(Cell c)const; bool poison(Cell c)const; void setPoison(Cell c,bool value);
  bool hasPoisonFreeRoute(int player)const;
  std::uint64_t topologySignature()const;
  const std::vector<Cell>& solution(int player)const{return solutions_[player];}
  Cell treasure()const{return treasure_;} Cell start(int player)const{return starts_[player];}
  int poisonCount()const;
private:
  int index(Cell c)const{return c.y*Width+c.x;} bool valid(Cell c)const{return c.x>=0&&c.y>=0&&c.x<Width&&c.y<Height;}
  std::array<bool,Width*Height> open_{}; std::array<bool,Width*Height> poison_{};
  Cell treasure_{10,7}; std::array<Cell,2> starts_{{{1,13},{19,13}}}; std::array<std::vector<Cell>,2> solutions_;
};

class GameModel {
public:
  explicit GameModel(std::uint64_t seed=1);
  void start(Mode mode,int totalRounds,bool soloBuffs=true); void update(float dt);
  bool move(int player,Cell direction); void continueFromResult(); bool chooseBuff(int offerIndex);
  Mode mode()const{return mode_;} Phase phase()const{return phase_;} int round()const{return round_;} int totalRounds()const{return totalRounds_;}
  float phaseTime()const{return phaseTime_;} float revealDuration()const; float blackoutDuration()const;
  const Maze& maze()const{return maze_;} const std::array<Player,2>& players()const{return players_;}
  const std::vector<Buff>& offers()const{return offers_;} int draftingPlayer()const{return draftingPlayer_;}
  int winner()const{return winner_;} int firstFinisher()const{return firstFinisher_;} std::uint64_t seed()const{return seed_;}
  int frame()const{return frame_;} int framesPerRound()const{return framesPerRound_;}
  std::string roundMessage()const{return roundMessage_;} bool soloBuffs()const{return soloBuffs_;}
  void forceBlackout();
private:
  void beginRound(); void finishRound(); void prepareDraft(); void applyBuff(Player&,BuffType); std::vector<Cell> shortest(Cell from,Cell to)const;
  std::mt19937_64 rng_; std::uint64_t seed_{}; Mode mode_{Mode::Solo}; Phase phase_{Phase::Reveal};
  int round_{1},totalRounds_{5},frame_{1},framesPerRound_{4},firstFinisher_{-1},winner_{-1},draftingPlayer_{0},draftsRemaining_{};
  float phaseTime_{},finishGrace_{}; bool soloBuffs_{true}; Maze maze_; std::array<Player,2> players_{};
  std::vector<Buff> offers_; std::string roundMessage_;
};
const std::vector<Buff>& buffCatalogue();
}
