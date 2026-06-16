#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "dds.h"

enum Player
{
    NORTH = 0,
    EAST = 1,
    SOUTH = 2,
    WEST = 3
};

enum Partnership
{
    NS_SIDE = 0,
    EW_SIDE = 1
};

struct EngineCard
{
    int suit = 0;
    int rank = 0;
};

struct OpeningLead
{
    Player player = NORTH;
    EngineCard card{};
};

struct MoveEvaluation
{
    Player player = NORTH;
    EngineCard card{};
    bool legal = false;
    bool optimal = false;
    int nsTotalTricksIfPlayed = 0;
    int ewTotalTricksIfPlayed = 0;
    int currentSideTotalTricksIfPlayed = 0;
};

struct PositionAnalysis
{
    Player player = NORTH;
    Partnership side = NS_SIDE;
    int bestNsTotal = 0;
    int bestEwTotal = 0;
    int bestCurrentSideTotal = 0;
    std::vector<EngineCard> legalCards;
    std::vector<EngineCard> optimalCards;
    std::vector<MoveEvaluation> evaluations;
};

struct PlayedMove
{
    Player player = NORTH;
    EngineCard card{};
    bool legal = false;
    bool optimal = false;
    std::vector<EngineCard> optimalCardsAtTurn;
};

struct PuzzleResult
{
    bool smartPlay = true;
    bool targetReached = false;
    bool finished = false;
    int target = 0;
    int nsTricks = 0;
    int ewTricks = 0;
};

class PuzzleEngine
{
public:
    PuzzleEngine();

    void setTarget(int target);
    void setTrump(int trump);
    void setDeal(const Deal& deal);

    int loadPosition(
        const std::array<std::vector<EngineCard>, DDS_HANDS>& hands,
        int trump,
        Player leader);

    int loadPosition(
        const std::array<std::vector<EngineCard>, DDS_HANDS>& hands,
        int trump,
        const OpeningLead& openingLead);

    int loadPosition(
        const std::array<std::string, DDS_HANDS>& hands,
        int trump,
        Player leader);

    int loadPosition(
        const std::array<std::string, DDS_HANDS>& hands,
        int trump,
        const OpeningLead& openingLead);

    int analyseCurrentPosition();
    MoveEvaluation evaluateMove(const EngineCard& card);
    int playMove(const EngineCard& card);

    bool isCardLegal(const EngineCard& card) const;
    bool isCardOptimal(const EngineCard& card) const;
    bool smartPlayValid() const;
    bool finished() const;
    Player currentPlayer() const;
    Partnership currentPartnership() const;

    PuzzleResult result() const;

    const PositionAnalysis& analysis() const;
    const std::vector<PlayedMove>& history() const;
    const Deal& deal() const;

    static int parseHand(const std::string& text, std::vector<EngineCard>* cards);
    static int parseCard(const std::string& text, EngineCard* card);

private:
    Deal currentDeal_{};
    PositionAnalysis currentAnalysis_{};
    std::vector<PlayedMove> moveHistory_{};
    bool smartValid_ = true;
    bool analysisDirty_ = true;
    bool positionLoaded_ = false;
    int target_ = 0;
    int nsTricks_ = 0;
    int ewTricks_ = 0;
    int lastAnalysisCode_ = RETURN_NO_FAULT;

    int loadPositionInternal(
        const std::array<std::vector<EngineCard>, DDS_HANDS>& hands,
        int trump,
        const std::optional<Player>& leader,
        const std::optional<OpeningLead>& openingLead);

    int validateCurrentPosition();
};
