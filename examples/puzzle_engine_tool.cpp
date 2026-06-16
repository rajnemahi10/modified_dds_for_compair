#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "game_engine.hpp"

namespace
{

struct Request
{
  int target = 0;
  int trump = 0;
  bool useOpening = false;
  Player leader = NORTH;
  Player openingPlayer = NORTH;
  EngineCard openingCard{};
  std::string north;
  std::string east;
  std::string south;
  std::string west;
  std::vector<std::string> moves;
};

auto trim(const std::string& input) -> std::string
{
  std::size_t start = 0;
  while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])))
  {
    ++start;
  }

  std::size_t end = input.size();
  while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])))
  {
    --end;
  }

  return input.substr(start, end - start);
}

auto split(const std::string& input, const char delim) -> std::vector<std::string>
{
  std::vector<std::string> parts;
  std::stringstream stream(input);
  std::string item;

  while (std::getline(stream, item, delim))
  {
    parts.push_back(trim(item));
  }

  return parts;
}

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
      return "N";
    case EAST:
      return "E";
    case SOUTH:
      return "S";
    case WEST:
      return "W";
    default:
      return "?";
  }
}

auto sideName(const Partnership side) -> const char*
{
  return side == NS_SIDE ? "NS" : "EW";
}

auto suitChar(const int suit) -> char
{
  static constexpr char suits[] = {'S', 'H', 'D', 'C', 'N'};
  if (suit < 0 || suit >= DDS_STRAINS)
    return '?';
  return suits[suit];
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

auto jsonEscape(const std::string& input) -> std::string
{
  std::string out;
  out.reserve(input.size() + 8);
  for (const char ch : input)
  {
    switch (ch)
    {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      default:
        out.push_back(ch);
        break;
    }
  }
  return out;
}

auto readRequest(Request* request, std::string* error) -> bool
{
  std::map<std::string, std::string> kv;
  std::string line;

  while (std::getline(std::cin, line))
  {
    line = trim(line);
    if (line.empty())
      continue;

    const std::size_t pos = line.find('=');
    if (pos == std::string::npos)
    {
      *error = "Malformed line: " + line;
      return false;
    }

    kv[trim(line.substr(0, pos))] = trim(line.substr(pos + 1));
  }

  if (kv.count("target") == 0 || kv.count("trump") == 0 || kv.count("start") == 0)
  {
    *error = "Missing target, trump, or start";
    return false;
  }

  request->target = std::atoi(kv["target"].c_str());
  if (!parseTrump(kv["trump"], &request->trump))
  {
    *error = "Invalid trump";
    return false;
  }

  request->north = kv["north"];
  request->east = kv["east"];
  request->south = kv["south"];
  request->west = kv["west"];

  if (kv["start"].rfind("leader:", 0) == 0)
  {
    if (!parsePlayer(kv["start"].substr(7), &request->leader))
    {
      *error = "Invalid leader";
      return false;
    }
  }
  else if (kv["start"].rfind("opening:", 0) == 0)
  {
    request->useOpening = true;
    const std::vector<std::string> parts = split(kv["start"], ':');
    if (parts.size() != 3 ||
        !parsePlayer(parts[1], &request->openingPlayer) ||
        PuzzleEngine::parseCard(parts[2], &request->openingCard) != RETURN_NO_FAULT)
    {
      *error = "Invalid opening specification";
      return false;
    }
  }
  else
  {
    *error = "Start must be leader:<player> or opening:<player>:<card>";
    return false;
  }

  if (kv.count("moves") != 0 && !kv["moves"].empty())
  {
    request->moves = split(kv["moves"], ',');
    request->moves.erase(
      std::remove_if(
        request->moves.begin(),
        request->moves.end(),
        [](const std::string& item) { return item.empty(); }),
      request->moves.end());
  }

  return true;
}

auto emitCards(const std::vector<EngineCard>& cards) -> void
{
  std::cout << "[";
  for (std::size_t i = 0; i < cards.size(); ++i)
  {
    if (i != 0)
      std::cout << ",";
    std::cout << "\"" << cardString(cards[i]) << "\"";
  }
  std::cout << "]";
}

auto emitError(const int code, const std::string& message) -> int
{
  std::cout << "{"
            << "\"status\":\"error\","
            << "\"code\":" << code << ","
            << "\"message\":\"" << jsonEscape(message) << "\""
            << "}\n";
  return 1;
}

}  // namespace

auto main() -> int
{
  Request request;
  std::string parseError;

  if (!readRequest(&request, &parseError))
  {
    return emitError(-1000, parseError);
  }

  PuzzleEngine engine;
  engine.setTarget(request.target);

  std::array<std::string, DDS_HANDS> hands = {
    request.north,
    request.east,
    request.south,
    request.west,
  };

  int rc = RETURN_NO_FAULT;
  if (request.useOpening)
  {
    rc = engine.loadPosition(
      hands,
      request.trump,
      OpeningLead{request.openingPlayer, request.openingCard});
  }
  else
  {
    rc = engine.loadPosition(hands, request.trump, request.leader);
  }

  if (rc != RETURN_NO_FAULT)
  {
    return emitError(rc, "Failed to load position");
  }

  for (const std::string& moveText : request.moves)
  {
    EngineCard card;
    if (PuzzleEngine::parseCard(moveText, &card) != RETURN_NO_FAULT)
    {
      return emitError(RETURN_PBN_FAULT, "Invalid move card: " + moveText);
    }

    rc = engine.playMove(card);
    if (rc != RETURN_NO_FAULT)
    {
      return emitError(rc, "Failed to replay move: " + moveText);
    }
  }

  rc = engine.analyseCurrentPosition();
  if (rc != RETURN_NO_FAULT && !engine.finished())
  {
    return emitError(rc, "Analysis failed");
  }

  const PuzzleResult result = engine.result();
  const PositionAnalysis& analysis = engine.analysis();
  const std::vector<PlayedMove>& history = engine.history();

  std::cout << "{";
  std::cout << "\"status\":\"ok\",";
  std::cout << "\"result\":{"
            << "\"target\":" << result.target << ","
            << "\"nsTricks\":" << result.nsTricks << ","
            << "\"ewTricks\":" << result.ewTricks << ","
            << "\"smartPlay\":" << (result.smartPlay ? "true" : "false") << ","
            << "\"targetReached\":" << (result.targetReached ? "true" : "false") << ","
            << "\"finished\":" << (result.finished ? "true" : "false")
            << "},";

  std::cout << "\"analysis\":{"
            << "\"player\":\"" << playerName(analysis.player) << "\","
            << "\"side\":\"" << sideName(analysis.side) << "\","
            << "\"bestNsTotal\":" << analysis.bestNsTotal << ","
            << "\"bestEwTotal\":" << analysis.bestEwTotal << ","
            << "\"bestCurrentSideTotal\":" << analysis.bestCurrentSideTotal << ",";

  std::cout << "\"legalCards\":";
  emitCards(analysis.legalCards);
  std::cout << ",";

  std::cout << "\"optimalCards\":";
  emitCards(analysis.optimalCards);
  std::cout << ",";

  std::cout << "\"evaluations\":[";
  for (std::size_t i = 0; i < analysis.evaluations.size(); ++i)
  {
    const MoveEvaluation& evaluation = analysis.evaluations[i];
    if (i != 0)
      std::cout << ",";
    std::cout << "{"
              << "\"card\":\"" << cardString(evaluation.card) << "\","
              << "\"legal\":" << (evaluation.legal ? "true" : "false") << ","
              << "\"optimal\":" << (evaluation.optimal ? "true" : "false") << ","
              << "\"nsTotal\":" << evaluation.nsTotalTricksIfPlayed << ","
              << "\"ewTotal\":" << evaluation.ewTotalTricksIfPlayed << ","
              << "\"currentSideTotal\":" << evaluation.currentSideTotalTricksIfPlayed
              << "}";
  }
  std::cout << "]},";

  std::cout << "\"history\":[";
  for (std::size_t i = 0; i < history.size(); ++i)
  {
    const PlayedMove& move = history[i];
    if (i != 0)
      std::cout << ",";
    std::cout << "{"
              << "\"player\":\"" << playerName(move.player) << "\","
              << "\"card\":\"" << cardString(move.card) << "\","
              << "\"optimal\":" << (move.optimal ? "true" : "false") << ","
              << "\"optimalCardsAtTurn\":";
    emitCards(move.optimalCardsAtTurn);
    std::cout << "}";
  }
  std::cout << "]";
  std::cout << "}\n";

  return 0;
}
