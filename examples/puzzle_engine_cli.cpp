#include <array>
#include <cctype>
#include <iostream>
#include <string>
#include <vector>

#include "game_engine.hpp"

namespace
{

auto parsePlayer(const std::string& text, Player* player) -> bool
{
  if (text.empty())
    return false;

  switch (std::toupper(static_cast<unsigned char>(text[0])))
  {
    case 'N':
      *player = NORTH;
      return true;
    case 'E':
      *player = EAST;
      return true;
    case 'S':
      *player = SOUTH;
      return true;
    case 'W':
      *player = WEST;
      return true;
    default:
      return false;
  }
}

auto parseTrump(const std::string& text, int* trump) -> bool
{
  if (text.empty())
    return false;

  switch (std::toupper(static_cast<unsigned char>(text[0])))
  {
    case 'S':
      *trump = 0;
      return true;
    case 'H':
      *trump = 1;
      return true;
    case 'D':
      *trump = 2;
      return true;
    case 'C':
      *trump = 3;
      return true;
    case 'N':
      *trump = DDS_NOTRUMP;
      return true;
    default:
      return false;
  }
}

auto playerName(const Player player) -> const char*
{
  switch (player)
  {
    case NORTH:
      return "North";
    case EAST:
      return "East";
    case SOUTH:
      return "South";
    case WEST:
      return "West";
    default:
      return "?";
  }
}

auto suitChar(const int suit) -> char
{
  static constexpr char kSuits[] = {'S', 'H', 'D', 'C', 'N'};
  if (suit < 0 || suit >= DDS_STRAINS)
    return '?';
  return kSuits[suit];
}

auto rankString(const int rank) -> std::string
{
  if (rank >= 2 && rank <= 9)
    return std::string(1, static_cast<char>('0' + rank));
  switch (rank)
  {
    case 10:
      return "T";
    case 11:
      return "J";
    case 12:
      return "Q";
    case 13:
      return "K";
    case 14:
      return "A";
    default:
      return "?";
  }
}

auto cardString(const EngineCard& card) -> std::string
{
  return std::string(1, suitChar(card.suit)) + rankString(card.rank);
}

auto printCards(const std::vector<EngineCard>& cards) -> void
{
  if (cards.empty())
  {
    std::cout << "(none)";
    return;
  }

  for (std::size_t i = 0; i < cards.size(); ++i)
  {
    if (i != 0)
      std::cout << ' ';
    std::cout << cardString(cards[i]);
  }
}

auto readLine(const char* prompt, std::string* line) -> bool
{
  std::cout << prompt;
  std::getline(std::cin, *line);
  return static_cast<bool>(std::cin);
}

}  // namespace

auto main() -> int
{
  PuzzleEngine engine;
  std::array<std::string, DDS_HANDS> hands;
  std::string line;
  std::string mode;
  int target = 0;
  int trump = 0;

  std::cout << "Bridge Puzzle CLI\n";
  std::cout << "Hand format: AKQJ.T98.76.5432\n";
  std::cout << "Card format: SA, HT, D7, C2\n\n";

  if (!readLine("Target tricks for N-S: ", &line))
    return 1;
  target = std::stoi(line);
  engine.setTarget(target);

  if (!readLine("Trump (S/H/D/C/N): ", &line) || !parseTrump(line, &trump))
  {
    std::cerr << "Invalid trump\n";
    return 1;
  }

  if (!readLine("Start mode ('leader' or 'opening'): ", &mode))
    return 1;

  if (!readLine("North hand: ", &hands[NORTH]) ||
      !readLine("East hand: ", &hands[EAST]) ||
      !readLine("South hand: ", &hands[SOUTH]) ||
      !readLine("West hand: ", &hands[WEST]))
  {
    return 1;
  }

  int rc = RETURN_NO_FAULT;

  if (!mode.empty() && std::toupper(static_cast<unsigned char>(mode[0])) == 'L')
  {
    Player leader;
    if (!readLine("Leader (N/E/S/W): ", &line) || !parsePlayer(line, &leader))
    {
      std::cerr << "Invalid leader\n";
      return 1;
    }

    rc = engine.loadPosition(hands, trump, leader);
  }
  else
  {
    Player opener;
    EngineCard openingCard;

    if (!readLine("Opening player (N/E/S/W): ", &line) || !parsePlayer(line, &opener))
    {
      std::cerr << "Invalid opening player\n";
      return 1;
    }

    if (!readLine("Opening card: ", &line) ||
        PuzzleEngine::parseCard(line, &openingCard) != RETURN_NO_FAULT)
    {
      std::cerr << "Invalid opening card\n";
      return 1;
    }

    rc = engine.loadPosition(hands, trump, OpeningLead{opener, openingCard});
  }

  if (rc != RETURN_NO_FAULT)
  {
    std::cerr << "Could not load position, DDS error code: " << rc << "\n";
    return 1;
  }

  while (!engine.finished())
  {
    rc = engine.analyseCurrentPosition();
    if (rc != RETURN_NO_FAULT)
    {
      std::cerr << "Analysis failed, DDS error code: " << rc << "\n";
      return 1;
    }

    const PositionAnalysis& analysis = engine.analysis();
    std::cout << "\nTo play: " << playerName(analysis.player) << "\n";
    std::cout << "Legal: ";
    printCards(analysis.legalCards);
    std::cout << "\nOptimal: ";
    printCards(analysis.optimalCards);
    std::cout << "\n";

    if (!readLine("Play card: ", &line))
      return 1;

    EngineCard card;
    if (PuzzleEngine::parseCard(line, &card) != RETURN_NO_FAULT)
    {
      std::cout << "Could not parse card. Try again.\n";
      continue;
    }

    rc = engine.playMove(card);
    if (rc != RETURN_NO_FAULT)
    {
      std::cout << "Illegal move or DDS error (" << rc << "). Try again.\n";
      continue;
    }

    const PlayedMove& played = engine.history().back();
    std::cout << "Played " << cardString(played.card)
              << (played.optimal ? " [optimal]" : " [not optimal]") << "\n";

    const PuzzleResult result = engine.result();
    std::cout << "Score: NS " << result.nsTricks << " / EW " << result.ewTricks
              << " | smart-play-valid=" << (result.smartPlay ? "yes" : "no") << "\n";
  }

  const PuzzleResult result = engine.result();
  std::cout << "\nFinal result\n";
  std::cout << "NS tricks: " << result.nsTricks << "\n";
  std::cout << "EW tricks: " << result.ewTricks << "\n";
  std::cout << "Target reached: " << (result.targetReached ? "yes" : "no") << "\n";
  std::cout << "Smart play valid: " << (result.smartPlay ? "yes" : "no") << "\n";
  std::cout << "Puzzle success: "
            << (result.targetReached && result.smartPlay ? "yes" : "no")
            << "\n";

  return 0;
}
