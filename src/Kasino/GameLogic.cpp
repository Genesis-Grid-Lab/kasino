#include "Kasino/GameLogic.h"
#include <algorithm>
#include <numeric>
#include <set>
#include <cassert>

// ---------- helpers
static bool isBigCasino(const Card& c){ return c.rank==Rank::Two && c.suit==Suit::Spades; }
static bool isLittleCasino(const Card& c){ return c.rank==Rank::Ten && c.suit==Suit::Diamonds; }
static bool isAce(const Card& c){ return c.rank==Rank::Ace; }

int CardSumValue(const std::vector<Card>& v){
  int s=0; for (auto& c: v) s += RankValue(c.rank); return s;
}

// generate all index subsets of loose cards that sum to target (no duplicates)
static void genSumCombos(const std::vector<Card>& loose, int target, size_t start, std::vector<int>& cur, std::vector<std::vector<int>>& out){
  int sum=0; for (int i:cur) sum += RankValue(loose[i].rank);
  if (sum==target) { out.push_back(cur); /*continue;*/ } // allow search for alternate disjoint sets
  if (sum>=target) return;
  for (size_t i=start; i<loose.size(); ++i) {
    cur.push_back((int)i);
    genSumCombos(loose, target, i+1, cur, out);
    cur.pop_back();
  }
}

static void removeIndices(std::vector<Card>& v, std::vector<int> idx){
  std::sort(idx.begin(), idx.end());
  for (int k=(int)idx.size()-1; k>=0; --k) v.erase(v.begin()+idx[k]);
}

static void removeBuilds(std::vector<Build>& v, std::vector<int> idx){
  std::sort(idx.begin(), idx.end());
  for (int k=(int)idx.size()-1; k>=0; --k) v.erase(v.begin()+idx[k]);
}

// ---------- flow

void StartRound(GameState& gs, int numPlayers, uint32_t shuffleSeed){
  gs = {};
  gs.numPlayers = numPlayers;
  gs.players.resize(numPlayers);
  Deck d; d.Reset(); d.Shuffle(shuffleSeed);
  gs.stock = std::move(d.cards);

  // initial deal: 4 to each, 4 to table
  for (int p=0; p<numPlayers; ++p) {
    for (int i=0; i<4; ++i) { gs.players[p].hand.push_back(gs.stock.back()); gs.stock.pop_back(); }
  }
  for (int i=0;i<4;++i){ gs.table.loose.push_back(gs.stock.back()); gs.stock.pop_back(); }

  gs.current = 0;
}

bool DealNextHands(GameState& gs){
  if (gs.stock.size() < (size_t)(4*gs.numPlayers)) return false;
  for (int p=0; p<gs.numPlayers; ++p)
    for (int i=0;i<4;++i){ gs.players[p].hand.push_back(gs.stock.back()); gs.stock.pop_back(); }
  return true;
}

void AdvanceTurn(GameState& gs){
  gs.current = (gs.current + 1) % gs.numPlayers;
}

// ---------- move gen

std::vector<Move> LegalMoves(const GameState& gs){
  std::vector<Move> out;

  const auto& P = gs.CurPlayer();
  const auto& L = gs.table.loose;
  const auto& B = gs.table.builds;

  if (P.hand.empty()) return out;

  for (size_t h=0; h<P.hand.size(); ++h) {
    const Card hand = P.hand[h];
    const int hv = RankValue(hand.rank);

    // 1) CAPTURE: capture equal ranks + any sum-combos equaling hv + any builds of value hv.
    // Rule: you must take ALL matching builds of value hv; for loose card combos we generate all combos (engine can choose).
    std::vector<std::vector<int>> combos; std::vector<int> cur;
    genSumCombos(L, hv, 0, cur, combos);

    // Cassino rule: if capturing with a card, you must also take every
    // loose card of the same rank. Track their indices so we can append
    // them to every capture option that we emit.
    std::vector<int> equalRankIdx;
    for (size_t li = 0; li < L.size(); ++li) {
      if (L[li].rank == hand.rank) equalRankIdx.push_back((int)li);
    }

    // Equal-rank singles are combos too (handled by genSumCombos for singletons), but ensure build captures included.
    // For each combo, create a capture move that also includes all builds matching hv.
    std::vector<int> matchingBuildIdx;
    for (size_t bi=0; bi<B.size(); ++bi) if (B[bi].value == hv) matchingBuildIdx.push_back((int)bi);

    bool canCapture = !combos.empty() || !matchingBuildIdx.empty() || !equalRankIdx.empty();
    if (canCapture) {
      if (combos.empty()) combos.push_back({}); // allow capturing just builds/equal-rank cards

      std::set<std::vector<int>> seen;
      for (auto looseIdx : combos) {
        looseIdx.insert(looseIdx.end(), equalRankIdx.begin(), equalRankIdx.end());
        std::sort(looseIdx.begin(), looseIdx.end());
        looseIdx.erase(std::unique(looseIdx.begin(), looseIdx.end()), looseIdx.end());
        if (!seen.insert(looseIdx).second) continue; // avoid duplicate capture variants

        Move mv; mv.type=MoveType::Capture; mv.handCard=hand; mv.captureLooseIdx=looseIdx; mv.captureBuildIdx=matchingBuildIdx;
        out.push_back(std::move(mv));
      }
    }

    // 2) BUILD: create a new build value T using hand + one-or-more loose cards (you must hold/plan to hold a T to capture later).
    // Common simple builds: hand + one loose card.
    // We’ll permit multi-card builds too: any subset of loose such that hv + sum(subset) = T, where T is a value you can capture with a future card.
    // A conservative rule engine: only allow if player ALSO has a card of value T in hand (besides `hand`), or if T==hv (rare). Here we require a separate card.
    // Generate candidate T from your other hand cards.
    std::set<int> capturableValues;
    for (const Card& other : P.hand) if (!(other==hand)) capturableValues.insert(RankValue(other.rank));

    for (int T : capturableValues) {
      if (T < hv) continue; // build must increase or equal? Typically any T is fine; we’ll allow >= hv
      int need = T - hv;
      if (need<=0) continue;

      std::vector<std::vector<int>> parts; std::vector<int> cur2;
      genSumCombos(L, need, 0, cur2, parts);
      for (auto& subset : parts) {
	Move mv; mv.type=MoveType::Build; mv.handCard=hand; mv.buildTargetValue=T; mv.buildUseLooseIdx=subset;
	out.push_back(std::move(mv));
      }
    }

    // 3) EXTEND BUILD: if you already own build(s), you can raise their value (and must still be able to capture later).
    for (size_t bi=0; bi<B.size(); ++bi) {
      if (B[bi].ownerPlayer != gs.current) continue; // can only extend your own
      // target T = old.value + hv  (simple extend using just the hand card)
      int T = B[bi].value + hv;
      // validate you can later capture T (hold a T card besides this hand)
      bool ok=false;
      for (const Card& other: P.hand) if (!(other==hand) && RankValue(other.rank)==T) { ok=true; break; }
      if (!ok) continue;
      Move mv; mv.type=MoveType::ExtendBuild; mv.handCard=hand; mv.buildTargetValue=T;
      // we reference the build to extend via captureBuildIdx to reuse the vector
      mv.captureBuildIdx = { (int)bi };
      out.push_back(std::move(mv));
    }

    // 4) TRAIL: always legal unless there exists a mandatory capture rule; many variants allow trailing even if capture exists.
    {
      Move mv; mv.type=MoveType::Trail; mv.handCard=hand;
      out.push_back(std::move(mv));
    }
  }

  return out;
}

// ---------- apply move

static void giveCard(std::vector<Card>& from, std::vector<Card>& to, const Card& c){
  auto it = std::find(from.begin(), from.end(), c);
  assert(it != from.end());
  to.push_back(*it);
  from.erase(it);
}

static void collectBuild(std::vector<Build>& builds, int idx, std::vector<Card>& into){
  // for now, collecting a build just grants its recorded cards (purely visual). In many implementations builds are virtual; capturing consumes nothing extra.
  builds.erase(builds.begin()+idx);
}

bool ApplyMove(GameState& gs, const Move& mv){
  // find and remove the played hand card
  auto& hand = gs.CurPlayer().hand;
  auto it = std::find(hand.begin(), hand.end(), mv.handCard);
  if (it == hand.end()) return false; // invalid
  Card played = *it;
  hand.erase(it);

  auto& L = gs.table.loose;
  auto& B = gs.table.builds;
  auto& P = gs.players[gs.current];

  switch (mv.type) {
  case MoveType::Capture: {
    int cardPointsEarned = 0;
    int buildsCaptured = 0;
    // Take loose indices
    std::vector<int> sorted = mv.captureLooseIdx; std::sort(sorted.begin(), sorted.end());
    for (int k=(int)sorted.size()-1; k>=0; --k) {
      P.pile.push_back(L[sorted[k]]);
      cardPointsEarned++;
      L.erase(L.begin()+sorted[k]);
    }
    // Take matching builds
    if (!mv.captureBuildIdx.empty()) {
      std::vector<int> bsorted = mv.captureBuildIdx; std::sort(bsorted.begin(), bsorted.end());
      for (int k=(int)bsorted.size()-1; k>=0; --k) {
        int bi = bsorted[k];
        if (bi < 0 || bi >= (int)B.size()) continue;
        const auto capturedCards = B[bi].cards;
        cardPointsEarned += static_cast<int>(capturedCards.size());
        P.pile.insert(P.pile.end(), capturedCards.begin(), capturedCards.end());
	buildsCaptured++;        
        B.erase(B.begin()+bi);
      }
    }
    // the played card itself goes to pile
    P.pile.push_back(played);
    P.buildBonus += buildsCaptured;    
    cardPointsEarned++;

    P.capturedCardPoints += cardPointsEarned;
    gs.lastCaptureBy = gs.current;
  } break;

  case MoveType::Build: {
    // Create new build owned by current player
    Build nb; nb.ownerPlayer = gs.current;
    nb.value = mv.buildTargetValue;
    nb.cards.push_back(played);
    // move used loose cards into the build record (optional; for display)
    // but on table we typically remove those loose cards
    std::vector<int> sorted = mv.buildUseLooseIdx; std::sort(sorted.begin(), sorted.end());
    for (int k=(int)sorted.size()-1; k>=0; --k) {
      nb.cards.push_back(L[sorted[k]]);
      L.erase(L.begin()+sorted[k]);
    }
    B.push_back(std::move(nb));
    // P.buildBonus += 1;
    // played card goes to table *as part of build* (not to pile)
  } break;

  case MoveType::ExtendBuild: {
    if (mv.captureBuildIdx.size()!=1) return false;
    int bi = mv.captureBuildIdx[0];
    if (bi < 0 || bi >= (int)B.size()) return false;
    if (B[bi].ownerPlayer != gs.current) return false;
    B[bi].value = mv.buildTargetValue;
    B[bi].cards.push_back(played); // record contribution
  } break;

  case MoveType::Trail: {
    // Place the card as a loose card
    L.push_back(played);
  } break;
  }

  // End-of-turn handling when everyone’s hand exhausted
  if (gs.HandsEmpty()) {
    // last-capture takes remaining table at end of round
    if (gs.stock.empty()) {
      if (gs.lastCaptureBy >= 0) {
        auto& last = gs.players[gs.lastCaptureBy];
        // collect all remaining
        last.pile.insert(last.pile.end(), L.begin(), L.end());
        last.capturedCardPoints += static_cast<int>(L.size());
        L.clear();
        B.clear(); // builds vanish
      }
    }
  }

  // advance turn
  AdvanceTurn(gs);
  return true;
}


void Selection::Clear() {
  handIndex.reset();
  loose.clear();
  builds.clear();
}
