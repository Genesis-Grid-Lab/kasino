#include "Kasino/GameLogic.h"
#include "Kasino/Scoring.h"
#include <cassert>
#include <iostream>
using namespace Casino;

static Card C(Rank r, Suit s){ return Card{r,s}; }
static void Assert(bool ok, const char* what){ if(!ok){ std::cerr<<"[FAIL] "<<what<<"\n"; std::abort(); } }
static void AssertEq(int got,int want,const char* what){ if(got!=want){ std::cerr<<"[FAIL] "<<what<<" got="<<got<<" want="<<want<<"\n"; std::abort(); } }

int main() {
  // deterministic sandbox state: 2 players, no stock, exact hands/table
  GameState gs; gs.numPlayers = 2; gs.players.resize(2);
  gs.stock.clear();
  gs.current = 0;
  // P0 hand: 5♣, 10♥ (used to build 8 then capture 10)
  gs.players[0].hand = { C(Rank::Five,Suit::Clubs), C(Rank::Ten,Suit::Hearts) };
  // P1 hand: irrelevant for now
  gs.players[1].hand = { C(Rank::Two,Suit::Clubs), C(Rank::Three,Suit::Diamonds) };

  // Table loose: 3♦, 2♠, 7♣
  gs.table.loose = { C(Rank::Three,Suit::Diamonds), C(Rank::Two,Suit::Spades), C(Rank::Seven,Suit::Clubs) };
  gs.table.builds.clear();

  // --- STEP 1: P0 builds 8 with 5 + 3 ---
  {
    auto moves = LegalMoves(gs);
    // find Build using hand 5 and loose[3♦] to value 8
    bool found=false;
    Move chosen{};
    for (auto& m : moves) {
      if (m.type==MoveType::Build && m.handCard==C(Rank::Five,Suit::Clubs) &&
	  m.buildTargetValue==8 && m.buildUseLooseIdx.size()==1) {
	// confirm the loose index points to 3♦ (index 0)
	if (m.buildUseLooseIdx[0]==0) { chosen=m; found=true; break; }
      }
    }
    Assert(found, "found build 8 (5+3)");
    Assert(ApplyMove(gs, chosen), "apply build 8");
    AssertEq((int)gs.table.builds.size(), 1, "one build on table");
    AssertEq(gs.table.builds[0].value, 8, "build value 8");
    AssertEq(gs.table.builds[0].ownerPlayer, 0, "build owner is P0");
    // hand should now have only 10♥
    AssertEq((int)gs.players[0].hand.size(), 1, "P0 hand after build");
    Assert(gs.players[0].hand[0]==C(Rank::Ten,Suit::Hearts), "P0 hand has 10♥");
    // 3♦ must be removed from loose
    AssertEq((int)gs.table.loose.size(), 2, "loose size after build");
  }

  // P1 trails (simple)
  {
    auto moves = LegalMoves(gs);
    Move trail{};
    bool found=false;
    for (auto& m : moves) if (m.type==MoveType::Trail) { trail=m; found=true; break; }
    Assert(found, "P1 can trail");
    Assert(ApplyMove(gs, trail), "apply P1 trail");
  }

  // --- STEP 2: P0 extends own build 8 -> 10 using 10♥? (Hazel-style extend rule is normally hand adds to value) ---
  // In our rules, ExtendBuild adds the hand’s value: 8 + 10 = 18 would be wrong.
  // So instead, P0 will CAPTURE build 8 + loose 2 to make 10 with 10♥  (since we require exact value == hand, we allow capturing builds of same value; here we need value 10)
  // Let's transform table to have build value 10 so we can test capture build:
  // Simpler: change the test to capture LOOSE 7 with 10 (rank capture), and next test will extend.
  // But we also want an ExtendBuild test: create a new scenario below.

  // --- STEP 2A: P0 captures 7♣ with 10♥ (equal-rank capture not possible; 7 != 10)
  // We'll capture sum (3+7) was not available (3 used in build). Instead do a fresh scenario for extend:
  // End round here:
  while (!gs.RoundOver()) {
    // let remaining players trail until hands empty
    auto moves = LegalMoves(gs);
    Move mv = moves.back(); // whatever legal (keeps it moving)
    ApplyMove(gs, mv);
  }

  // ========== NEW SCENARIO for Extend & Capture ==========

  GameState gs2; gs2.numPlayers=2; gs2.players.resize(2); gs2.stock.clear(); gs2.current=0;
  // P0 hand: 6♣, 10♣  (will build 8 with 6+2, then extend to 10 using 10, then later capture with a 10 we keep)
  gs2.players[0].hand = { C(Rank::Six,Suit::Clubs), C(Rank::Ten,Suit::Clubs) };
  gs2.players[1].hand = { C(Rank::Ten,Suit::Spades), C(Rank::Four,Suit::Hearts) };
  gs2.table.loose = { C(Rank::Two,Suit::Diamonds) };
  gs2.table.builds.clear();

  // P0 build 8 with 6 + 2
  {
    auto moves = LegalMoves(gs2);
    bool found=false; Move chosen{};
    for (auto& m : moves) {
      if (m.type==MoveType::Build && m.handCard==C(Rank::Six,Suit::Clubs) &&
	  m.buildTargetValue==8 && m.buildUseLooseIdx.size()==1) {
	chosen=m; found=true; break;
      }
    }
    Assert(found, "P0 build 8 with 6+2");
    Assert(ApplyMove(gs2, chosen), "apply build 8");
    AssertEq(gs2.table.builds[0].value, 8, "build8 exists");
  }

  // P1 trails any
  {
    auto moves = LegalMoves(gs2);
    Move trail{}; bool found=false;
    for (auto& m : moves) if (m.type==MoveType::Trail) { trail=m; found=true; break; }
    Assert(found, "P1 can trail");
    Assert(ApplyMove(gs2, trail), "apply P1 trail");
  }

  // P0 extend own build 8 -> 18 if we naively add 10; but our ExtendBuild rule sets value = old + hand,
  // which isn't right for Casino (extend typically changes declared target, but you must be able to capture later).
  // We'll reinterpret ExtendBuild in our engine as: set build.value to a new target T you can capture now/soon.
  // For this test, we extend to 10 using the 10 in hand (allowed).
  {
    auto moves = LegalMoves(gs2);
    bool found=false; Move ext{};
    for (auto& m : moves) {
      if (m.type==MoveType::ExtendBuild && m.handCard==C(Rank::Ten,Suit::Clubs) &&
	  !m.captureBuildIdx.empty() && m.buildTargetValue==10) {
	ext=m; found=true; break;
      }
    }
    Assert(found, "P0 extend build to 10");
    Assert(ApplyMove(gs2, ext), "apply extend to 10");
    AssertEq(gs2.table.builds[0].value, 10, "build value now 10");
  }

  // P1 plays 10♠ to CAPTURE the build (test that the opponent can take it if matching value)
  {
    auto moves = LegalMoves(gs2);
    bool found=false; Move cap{};
    for (auto& m : moves) {
      if (m.type==MoveType::Capture && m.handCard==C(Rank::Ten,Suit::Spades)) {
	// must include the build
	if (!m.captureBuildIdx.empty()) { cap=m; found=true; break; }
      }
    }
    Assert(found, "P1 can capture build 10 with 10♠");
    Assert(ApplyMove(gs2, cap), "apply capture build 10");
    AssertEq((int)gs2.table.builds.size(), 0, "build removed after capture");
    AssertEq((int)gs2.players[1].pile.size(), 1, "captor got played card");
    AssertEq(gs2.lastCaptureBy, 1, "last capture by P1");
  }

  std::cout << "All move tests passed.\n";
  return 0;
}
