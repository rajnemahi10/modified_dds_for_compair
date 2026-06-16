#include "game_engine.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <optional>
#include <string>

#include <api/solve_board.hpp>
#include <solver_context/solver_context.hpp>
#include <utility/constants.h>

namespace
{

struct TrickResolution
{
    bool completed = false;
    Player winner = NORTH;
};

auto makeMask(const int rank) -> unsigned
{
    return static_cast<unsigned>(bit_map_rank[rank] << 2);
}

auto countCurrentTrickCards(const Deal& deal) -> int
{
    int count = 0;
    while (count < 3 && deal.currentTrickRank[count] != 0)
    {
        ++count;
    }
    return count;
}

auto countCardsInHands(const Deal& deal) -> int
{
    int total = 0;

    for (int hand = 0; hand < DDS_HANDS; ++hand)
    {
        for (int suit = 0; suit < DDS_SUITS; ++suit)
        {
            unsigned cards = deal.remainCards[hand][suit] >> 2;
            while (cards != 0)
            {
                cards &= (cards - 1);
                ++total;
            }
        }
    }

    return total;
}

auto cardsToPlayCount(const Deal& deal) -> int
{
    return countCardsInHands(deal) + countCurrentTrickCards(deal);
}

auto remainingTricks(const Deal& deal) -> int
{
    return cardsToPlayCount(deal) / 4;
}

auto sideForPlayer(const Player player) -> Partnership
{
    return (player == NORTH || player == SOUTH) ? NS_SIDE : EW_SIDE;
}

auto sameCard(const EngineCard& lhs, const EngineCard& rhs) -> bool
{
    return lhs.suit == rhs.suit && lhs.rank == rhs.rank;
}

auto compareCards(const EngineCard& lhs, const EngineCard& rhs) -> bool
{
    if (lhs.suit != rhs.suit)
    {
        return lhs.suit < rhs.suit;
    }

    return lhs.rank > rhs.rank;
}

auto containsCard(
    const std::vector<EngineCard>& cards,
    const EngineCard& target) -> bool
{
    return std::any_of(
        cards.begin(),
        cards.end(),
        [&](const EngineCard& card)
        {
            return sameCard(card, target);
        });
}

auto dealCurrentPlayer(const Deal& deal) -> Player
{
    const int offset = countCurrentTrickCards(deal);
    return static_cast<Player>((deal.first + offset) % DDS_HANDS);
}

auto playerHasCard(
    const Deal& deal,
    const Player player,
    const EngineCard& card) -> bool
{
    if (card.suit < 0 || card.suit >= DDS_SUITS || card.rank < 2 || card.rank > 14)
    {
        return false;
    }

    return (deal.remainCards[player][card.suit] & makeMask(card.rank)) != 0;
}

auto playerHasSuit(
    const Deal& deal,
    const Player player,
    const int suit) -> bool
{
    if (suit < 0 || suit >= DDS_SUITS)
    {
        return false;
    }

    return (deal.remainCards[player][suit] >> 2) != 0;
}

auto isCardLegalForDeal(const Deal& deal, const EngineCard& card) -> bool
{
    const Player player = dealCurrentPlayer(deal);
    if (!playerHasCard(deal, player, card))
    {
        return false;
    }

    const int trickCount = countCurrentTrickCards(deal);
    if (trickCount == 0)
    {
        return true;
    }

    const int leadSuit = deal.currentTrickSuit[0];
    if (playerHasSuit(deal, player, leadSuit))
    {
        return card.suit == leadSuit;
    }

    return true;
}

auto listLegalCards(const Deal& deal) -> std::vector<EngineCard>
{
    std::vector<EngineCard> cards;
    const Player player = dealCurrentPlayer(deal);
    const int trickCount = countCurrentTrickCards(deal);
    int forcedSuit = -1;

    if (trickCount > 0)
    {
        const int leadSuit = deal.currentTrickSuit[0];
        if (playerHasSuit(deal, player, leadSuit))
        {
            forcedSuit = leadSuit;
        }
    }

    for (int suit = 0; suit < DDS_SUITS; ++suit)
    {
        if (forcedSuit != -1 && suit != forcedSuit)
        {
            continue;
        }

        const unsigned holding = deal.remainCards[player][suit] >> 2;
        for (int rank = 14; rank >= 2; --rank)
        {
            if ((holding & bit_map_rank[rank]) != 0)
            {
                cards.push_back({suit, rank});
            }
        }
    }

    std::sort(cards.begin(), cards.end(), compareCards);
    return cards;
}

auto determineTrickWinner(
    const int trump,
    const Player leader,
    const std::array<EngineCard, DDS_HANDS>& trick) -> Player
{
    int bestIndex = 0;
    int bestSuit = trick[0].suit;
    int bestRank = trick[0].rank;
    bool trumpPlayed = (bestSuit == trump);

    for (int i = 1; i < DDS_HANDS; ++i)
    {
        const EngineCard& card = trick[i];
        if (card.suit == trump)
        {
            if (!trumpPlayed || card.rank > bestRank)
            {
                bestIndex = i;
                bestSuit = card.suit;
                bestRank = card.rank;
                trumpPlayed = true;
            }
        }
        else if (!trumpPlayed && card.suit == bestSuit && card.rank > bestRank)
        {
            bestIndex = i;
            bestRank = card.rank;
        }
    }

    return static_cast<Player>((leader + bestIndex) % DDS_HANDS);
}

auto applyCardToDeal(Deal& deal, const Player player, const EngineCard& card) -> TrickResolution
{
    deal.remainCards[player][card.suit] &= ~makeMask(card.rank);

    TrickResolution resolution;
    const int trickCount = countCurrentTrickCards(deal);

    if (trickCount == 0)
    {
        deal.first = player;
        deal.currentTrickSuit[0] = card.suit;
        deal.currentTrickRank[0] = card.rank;
        return resolution;
    }

    if (trickCount < 3)
    {
        deal.currentTrickSuit[trickCount] = card.suit;
        deal.currentTrickRank[trickCount] = card.rank;
        return resolution;
    }

    std::array<EngineCard, DDS_HANDS> trickCards{};
    for (int i = 0; i < 3; ++i)
    {
        trickCards[i] = {deal.currentTrickSuit[i], deal.currentTrickRank[i]};
    }
    trickCards[3] = card;

    resolution.completed = true;
    resolution.winner = determineTrickWinner(
        deal.trump,
        static_cast<Player>(deal.first),
        trickCards);

    deal.first = resolution.winner;
    std::memset(deal.currentTrickSuit, 0, sizeof(deal.currentTrickSuit));
    std::memset(deal.currentTrickRank, 0, sizeof(deal.currentTrickRank));
    return resolution;
}

struct SolvedTotals
{
    int nsTotal = 0;
    int ewTotal = 0;
};

auto solveFutureTotals(
    SolverContext& ctx,
    const Deal& deal,
    const int nsTricks,
    const int ewTricks,
    SolvedTotals* totals) -> int
{
    if (cardsToPlayCount(deal) == 0)
    {
        totals->nsTotal = nsTricks;
        totals->ewTotal = ewTricks;
        return RETURN_NO_FAULT;
    }

    FutureTricks future{};
    const int rc = SolveBoard(ctx, deal, -1, 1, 1, &future);
    if (rc != RETURN_NO_FAULT)
    {
        return rc;
    }

    const Partnership sideToPlay = sideForPlayer(dealCurrentPlayer(deal));
    const int tricksLeft = remainingTricks(deal);

    int nsFuture = 0;
    if (sideToPlay == NS_SIDE)
    {
        nsFuture = future.score[0];
    }
    else
    {
        nsFuture = tricksLeft - future.score[0];
    }

    totals->nsTotal = nsTricks + nsFuture;
    totals->ewTotal = ewTricks + (tricksLeft - nsFuture);
    return RETURN_NO_FAULT;
}

auto normalizeText(const std::string& text) -> std::string
{
    std::string out;
    out.reserve(text.size());

    for (const unsigned char ch : text)
    {
        if (!std::isspace(ch))
        {
            out.push_back(static_cast<char>(std::toupper(ch)));
        }
    }

    return out;
}

auto parseSuitChar(const char ch) -> int
{
    switch (ch)
    {
        case 'S':
            return 0;
        case 'H':
            return 1;
        case 'D':
            return 2;
        case 'C':
            return 3;
        case 'N':
            return DDS_NOTRUMP;
        default:
            return -1;
    }
}

auto parseRankChar(const char ch) -> int
{
    switch (ch)
    {
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return ch - '0';
        case 'T':
            return 10;
        case 'J':
            return 11;
        case 'Q':
            return 12;
        case 'K':
            return 13;
        case 'A':
            return 14;
        default:
            return -1;
    }
}

} // namespace

PuzzleEngine::PuzzleEngine() = default;

void PuzzleEngine::setTarget(const int target)
{
    target_ = target;
}

void PuzzleEngine::setTrump(const int trump)
{
    currentDeal_.trump = trump;
    analysisDirty_ = true;
}

void PuzzleEngine::setDeal(const Deal& deal)
{
    currentDeal_ = deal;
    moveHistory_.clear();
    nsTricks_ = 0;
    ewTricks_ = 0;
    smartValid_ = true;
    analysisDirty_ = true;
    positionLoaded_ = true;
    currentAnalysis_ = {};
    lastAnalysisCode_ = RETURN_NO_FAULT;
}

int PuzzleEngine::loadPosition(
    const std::array<std::vector<EngineCard>, DDS_HANDS>& hands,
    const int trump,
    const Player leader)
{
    return loadPositionInternal(hands, trump, leader, std::nullopt);
}

int PuzzleEngine::loadPosition(
    const std::array<std::vector<EngineCard>, DDS_HANDS>& hands,
    const int trump,
    const OpeningLead& openingLead)
{
    return loadPositionInternal(hands, trump, std::nullopt, openingLead);
}

int PuzzleEngine::loadPosition(
    const std::array<std::string, DDS_HANDS>& hands,
    const int trump,
    const Player leader)
{
    std::array<std::vector<EngineCard>, DDS_HANDS> parsedHands;

    for (int player = 0; player < DDS_HANDS; ++player)
    {
        const int rc = parseHand(hands[player], &parsedHands[player]);
        if (rc != RETURN_NO_FAULT)
        {
            return rc;
        }
    }

    return loadPosition(parsedHands, trump, leader);
}

int PuzzleEngine::loadPosition(
    const std::array<std::string, DDS_HANDS>& hands,
    const int trump,
    const OpeningLead& openingLead)
{
    std::array<std::vector<EngineCard>, DDS_HANDS> parsedHands;

    for (int player = 0; player < DDS_HANDS; ++player)
    {
        const int rc = parseHand(hands[player], &parsedHands[player]);
        if (rc != RETURN_NO_FAULT)
        {
            return rc;
        }
    }

    return loadPosition(parsedHands, trump, openingLead);
}

int PuzzleEngine::loadPositionInternal(
    const std::array<std::vector<EngineCard>, DDS_HANDS>& hands,
    const int trump,
    const std::optional<Player>& leader,
    const std::optional<OpeningLead>& openingLead)
{
    if (trump < 0 || trump >= DDS_STRAINS)
    {
        return RETURN_TRUMP_WRONG;
    }

    if (leader.has_value() == openingLead.has_value())
    {
        return RETURN_FIRST_WRONG;
    }

    Deal deal{};
    deal.trump = trump;
    deal.first = leader.has_value() ? *leader : openingLead->player;

    for (int player = 0; player < DDS_HANDS; ++player)
    {
        for (const EngineCard& card : hands[player])
        {
            if (card.suit < 0 || card.suit >= DDS_SUITS || card.rank < 2 || card.rank > 14)
            {
                return RETURN_SUIT_OR_RANK;
            }

            const unsigned mask = makeMask(card.rank);
            if ((deal.remainCards[player][card.suit] & mask) != 0)
            {
                return RETURN_DUPLICATE_CARDS;
            }

            for (int otherPlayer = 0; otherPlayer < DDS_HANDS; ++otherPlayer)
            {
                if (otherPlayer != player &&
                    (deal.remainCards[otherPlayer][card.suit] & mask) != 0)
                {
                    return RETURN_DUPLICATE_CARDS;
                }
            }

            deal.remainCards[player][card.suit] |= mask;
        }
    }

    if (openingLead.has_value())
    {
        const Player player = openingLead->player;
        const EngineCard& card = openingLead->card;

        if (!playerHasCard(deal, player, card))
        {
            return RETURN_PLAY_FAULT;
        }

        deal.remainCards[player][card.suit] &= ~makeMask(card.rank);
        deal.currentTrickSuit[0] = card.suit;
        deal.currentTrickRank[0] = card.rank;
    }

    setDeal(deal);
    return validateCurrentPosition();
}

int PuzzleEngine::validateCurrentPosition()
{
    analysisDirty_ = true;
    return analyseCurrentPosition();
}

int PuzzleEngine::analyseCurrentPosition()
{
    currentAnalysis_ = {};
    currentAnalysis_.player = currentPlayer();
    currentAnalysis_.side = currentPartnership();

    if (!positionLoaded_)
    {
        lastAnalysisCode_ = RETURN_ZERO_CARDS;
        return lastAnalysisCode_;
    }

    if (cardsToPlayCount(currentDeal_) == 0)
    {
        currentAnalysis_.bestNsTotal = nsTricks_;
        currentAnalysis_.bestEwTotal = ewTricks_;
        currentAnalysis_.bestCurrentSideTotal =
            (currentAnalysis_.side == NS_SIDE) ? nsTricks_ : ewTricks_;
        analysisDirty_ = false;
        lastAnalysisCode_ = RETURN_NO_FAULT;
        return lastAnalysisCode_;
    }

    currentAnalysis_.legalCards = listLegalCards(currentDeal_);

    int bestCurrentSideTotal = -1;

    for (const EngineCard& card : currentAnalysis_.legalCards)
    {
        MoveEvaluation evaluation = evaluateMove(card);
        if (!evaluation.legal)
        {
            continue;
        }

        currentAnalysis_.evaluations.push_back(evaluation);
        bestCurrentSideTotal =
            std::max(bestCurrentSideTotal, evaluation.currentSideTotalTricksIfPlayed);
    }

    if (lastAnalysisCode_ != RETURN_NO_FAULT)
    {
        return lastAnalysisCode_;
    }

    currentAnalysis_.bestCurrentSideTotal = bestCurrentSideTotal;

    for (MoveEvaluation& evaluation : currentAnalysis_.evaluations)
    {
        evaluation.optimal =
            (evaluation.currentSideTotalTricksIfPlayed == bestCurrentSideTotal);
        if (evaluation.optimal)
        {
            currentAnalysis_.optimalCards.push_back(evaluation.card);
        }
    }

    if (!currentAnalysis_.evaluations.empty())
    {
        currentAnalysis_.bestNsTotal =
            currentAnalysis_.evaluations.front().nsTotalTricksIfPlayed;
        currentAnalysis_.bestEwTotal =
            currentAnalysis_.evaluations.front().ewTotalTricksIfPlayed;

        for (const MoveEvaluation& evaluation : currentAnalysis_.evaluations)
        {
            if (evaluation.currentSideTotalTricksIfPlayed == bestCurrentSideTotal)
            {
                currentAnalysis_.bestNsTotal = evaluation.nsTotalTricksIfPlayed;
                currentAnalysis_.bestEwTotal = evaluation.ewTotalTricksIfPlayed;
                break;
            }
        }
    }

    analysisDirty_ = false;
    lastAnalysisCode_ = RETURN_NO_FAULT;
    return lastAnalysisCode_;
}

MoveEvaluation PuzzleEngine::evaluateMove(const EngineCard& card)
{
    MoveEvaluation evaluation;
    evaluation.player = currentPlayer();
    evaluation.card = card;

    if (!positionLoaded_)
    {
        lastAnalysisCode_ = RETURN_ZERO_CARDS;
        return evaluation;
    }

    evaluation.legal = isCardLegal(card);
    if (!evaluation.legal)
    {
        return evaluation;
    }

    Deal nextDeal = currentDeal_;
    int nsTricks = nsTricks_;
    int ewTricks = ewTricks_;

    const TrickResolution trick = applyCardToDeal(nextDeal, evaluation.player, card);
    if (trick.completed)
    {
        if (sideForPlayer(trick.winner) == NS_SIDE)
        {
            ++nsTricks;
        }
        else
        {
            ++ewTricks;
        }
    }

    SolverContext ctx;
    SolvedTotals totals{};
    lastAnalysisCode_ = solveFutureTotals(ctx, nextDeal, nsTricks, ewTricks, &totals);
    if (lastAnalysisCode_ != RETURN_NO_FAULT)
    {
        return evaluation;
    }

    evaluation.nsTotalTricksIfPlayed = totals.nsTotal;
    evaluation.ewTotalTricksIfPlayed = totals.ewTotal;
    evaluation.currentSideTotalTricksIfPlayed =
        (sideForPlayer(evaluation.player) == NS_SIDE) ? totals.nsTotal : totals.ewTotal;

    return evaluation;
}

int PuzzleEngine::playMove(const EngineCard& card)
{
    const int analysisCode = analyseCurrentPosition();
    if (analysisCode != RETURN_NO_FAULT)
    {
        return analysisCode;
    }

    MoveEvaluation evaluation = evaluateMove(card);
    if (!evaluation.legal)
    {
        return RETURN_PLAY_FAULT;
    }

    if (!evaluation.optimal)
    {
        const int bestCurrentSideTotal = currentAnalysis_.bestCurrentSideTotal;
        evaluation.optimal =
            (evaluation.currentSideTotalTricksIfPlayed == bestCurrentSideTotal);
    }

    PlayedMove played;
    played.player = evaluation.player;
    played.card = card;
    played.legal = true;
    played.optimal = evaluation.optimal;
    played.optimalCardsAtTurn = currentAnalysis_.optimalCards;
    moveHistory_.push_back(played);

    if (!evaluation.optimal)
    {
        smartValid_ = false;
    }

    const TrickResolution trick = applyCardToDeal(currentDeal_, played.player, card);
    if (trick.completed)
    {
        if (sideForPlayer(trick.winner) == NS_SIDE)
        {
            ++nsTricks_;
        }
        else
        {
            ++ewTricks_;
        }
    }

    analysisDirty_ = true;
    return RETURN_NO_FAULT;
}

bool PuzzleEngine::isCardLegal(const EngineCard& card) const
{
    return positionLoaded_ && isCardLegalForDeal(currentDeal_, card);
}

bool PuzzleEngine::isCardOptimal(const EngineCard& card) const
{
    return containsCard(currentAnalysis_.optimalCards, card);
}

bool PuzzleEngine::smartPlayValid() const
{
    return smartValid_;
}

bool PuzzleEngine::finished() const
{
    return positionLoaded_ && cardsToPlayCount(currentDeal_) == 0;
}

Player PuzzleEngine::currentPlayer() const
{
    return dealCurrentPlayer(currentDeal_);
}

Partnership PuzzleEngine::currentPartnership() const
{
    return sideForPlayer(currentPlayer());
}

PuzzleResult PuzzleEngine::result() const
{
    PuzzleResult result;
    result.smartPlay = smartValid_;
    result.finished = finished();
    result.target = target_;
    result.nsTricks = nsTricks_;
    result.ewTricks = ewTricks_;
    result.targetReached = (nsTricks_ == target_);
    return result;
}

const PositionAnalysis& PuzzleEngine::analysis() const
{
    return currentAnalysis_;
}

const std::vector<PlayedMove>& PuzzleEngine::history() const
{
    return moveHistory_;
}

const Deal& PuzzleEngine::deal() const
{
    return currentDeal_;
}

int PuzzleEngine::parseHand(const std::string& text, std::vector<EngineCard>* cards)
{
    cards->clear();

    const std::string normalized = normalizeText(text);
    int suit = 0;

    for (const char ch : normalized)
    {
        if (ch == '.')
        {
            ++suit;
            if (suit >= DDS_SUITS)
            {
                return RETURN_PBN_FAULT;
            }
            continue;
        }

        const int rank = parseRankChar(ch);
        if (rank < 0)
        {
            return RETURN_PBN_FAULT;
        }

        cards->push_back({suit, rank});
    }

    return RETURN_NO_FAULT;
}

int PuzzleEngine::parseCard(const std::string& text, EngineCard* card)
{
    const std::string normalized = normalizeText(text);
    if (normalized.size() < 2 || normalized.size() > 3)
    {
        return RETURN_PBN_FAULT;
    }

    int suit = -1;
    int rank = -1;

    if (normalized.size() == 2)
    {
        rank = parseRankChar(normalized[0]);
        suit = parseSuitChar(normalized[1]);
        if (rank < 0 || suit < 0 || suit >= DDS_SUITS)
        {
            suit = parseSuitChar(normalized[0]);
            rank = parseRankChar(normalized[1]);
        }
    }
    else if (normalized == "10S" || normalized == "10H" ||
             normalized == "10D" || normalized == "10C")
    {
        rank = 10;
        suit = parseSuitChar(normalized[2]);
    }

    if (rank < 0 || suit < 0 || suit >= DDS_SUITS)
    {
        return RETURN_PBN_FAULT;
    }

    card->suit = suit;
    card->rank = rank;
    return RETURN_NO_FAULT;
}
