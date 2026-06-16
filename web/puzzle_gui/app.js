const state = {
  setup: null,
  moves: [],
  response: null,
};

const sample = {
  target: 8,
  trump: "S",
  startMode: "leader",
  leader: "N",
  openingPlayer: "W",
  openingCard: "HQ",
  north: "AQ85.AK976.5.J87",
  east: "JT.QJ5432.Q9.KQ9",
  south: "972..JT863.A6432",
  west: "K643.T8.AK742.T5",
};

const playerNames = ["N", "E", "S", "W"];
const handKeys = {
  N: "north",
  E: "east",
  S: "south",
  W: "west",
};

const suitOrder = ["S", "H", "D", "C"];

function byId(id) {
  return document.getElementById(id);
}

function suitClass(card) {
  return card.startsWith("H") || card.startsWith("D") ? "red-suit" : "";
}

function parseHandText(handText) {
  const suits = handText.split(".");
  const cards = [];
  suitOrder.forEach((suit, index) => {
    const holding = suits[index] || "";
    for (const rank of holding) {
      cards.push(`${suit}${rank}`);
    }
  });
  return cards;
}

function computeRemainingHands(setup, history) {
  const hands = {
    N: parseHandText(setup.north),
    E: parseHandText(setup.east),
    S: parseHandText(setup.south),
    W: parseHandText(setup.west),
  };

  if (setup.startMode === "opening" && setup.openingPlayer && setup.openingCard) {
    removeCardFromHand(hands[setup.openingPlayer], setup.openingCard);
  }

  for (const move of history || []) {
    removeCardFromHand(hands[move.player], move.card);
  }

  return hands;
}

function removeCardFromHand(cards, card) {
  const index = cards.indexOf(card);
  if (index >= 0) {
    cards.splice(index, 1);
  }
}

function computeCurrentTrick(setup, history) {
  const trick = [];

  if (setup.startMode === "opening" && setup.openingPlayer && setup.openingCard) {
    trick.push({ player: setup.openingPlayer, card: setup.openingCard });
  }

  for (const move of history || []) {
    trick.push({ player: move.player, card: move.card });
    if (trick.length === 4) {
      trick.length = 0;
    }
  }

  return trick;
}

function determineTrickWinner(trick, trump) {
  if (!trick.length) {
    return null;
  }

  let best = trick[0];
  let trumpPlayed = best.card.startsWith(trump);

  for (let i = 1; i < trick.length; i += 1) {
    const current = trick[i];
    const bestSuit = best.card[0];
    const bestRank = currentRankValue(best.card.slice(1));
    const suit = current.card[0];
    const rank = currentRankValue(current.card.slice(1));

    if (suit === trump) {
      if (!trumpPlayed || rank > bestRank) {
        best = current;
        trumpPlayed = true;
      }
    } else if (!trumpPlayed && suit === bestSuit && rank > bestRank) {
      best = current;
    }
  }

  return best.player;
}

function currentRankValue(rankText) {
  const rank = rankText.toUpperCase();
  if (/^[2-9]$/.test(rank)) {
    return Number(rank);
  }
  if (rank === "T") return 10;
  if (rank === "J") return 11;
  if (rank === "Q") return 12;
  if (rank === "K") return 13;
  if (rank === "A") return 14;
  return 0;
}

function computeLastTrickWinner(setup, history) {
  let trick = [];
  let lastWinner = null;

  if (setup.startMode === "opening" && setup.openingPlayer && setup.openingCard) {
    trick.push({ player: setup.openingPlayer, card: setup.openingCard });
  }

  for (const move of history || []) {
    trick.push({ player: move.player, card: move.card });
    if (trick.length === 4) {
      lastWinner = determineTrickWinner(trick, setup.trump);
      trick = [];
    }
  }

  return lastWinner;
}

function setStartModeVisibility() {
  const opening = byId("startMode").value === "opening";
  byId("leaderWrap").classList.toggle("hidden", opening);
  byId("openingPlayerWrap").classList.toggle("hidden", !opening);
  byId("openingCardWrap").classList.toggle("hidden", !opening);
}

function readSetup() {
  return {
    target: Number(byId("target").value),
    trump: byId("trump").value,
    startMode: byId("startMode").value,
    leader: byId("leader").value,
    openingPlayer: byId("openingPlayer").value,
    openingCard: byId("openingCard").value.trim(),
    north: byId("north").value.trim(),
    east: byId("east").value.trim(),
    south: byId("south").value.trim(),
    west: byId("west").value.trim(),
  };
}

function writeSetup(setup) {
  byId("target").value = setup.target;
  byId("trump").value = setup.trump;
  byId("startMode").value = setup.startMode;
  byId("leader").value = setup.leader;
  byId("openingPlayer").value = setup.openingPlayer;
  byId("openingCard").value = setup.openingCard;
  byId("north").value = setup.north;
  byId("east").value = setup.east;
  byId("south").value = setup.south;
  byId("west").value = setup.west;
  setStartModeVisibility();
}

async function analyse() {
  if (!state.setup) {
    return;
  }

  const errorBox = byId("errorBox");
  errorBox.classList.add("hidden");

  const response = await fetch("/api/analyse", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      setup: state.setup,
      moves: state.moves,
    }),
  });

  const data = await response.json();
  if (data.status !== "ok") {
    errorBox.textContent = data.message || "Analysis failed";
    errorBox.classList.remove("hidden");
    return;
  }

  state.response = data;
  renderState();
}

function badge(label, cls) {
  const span = document.createElement("span");
  span.className = `badge ${cls}`;
  span.textContent = label;
  return span;
}

function renderState() {
  const data = state.response;
  if (!data) {
    return;
  }

  const { result, analysis, history } = data;
  byId("statusTitle").textContent = result.finished
    ? "Play complete"
    : `Ready for ${analysis.player} to play`;

  const badgeRow = byId("badgeRow");
  badgeRow.innerHTML = "";
  badgeRow.appendChild(
    badge(result.smartPlay ? "Smart play still valid" : "Smart play failed", result.smartPlay ? "ok" : "fail"),
  );
  badgeRow.appendChild(
    badge(result.targetReached ? "Target reached" : "Target not reached yet", result.targetReached ? "ok" : "warn"),
  );
  badgeRow.appendChild(
    badge(result.finished ? "Finished" : "In progress", result.finished ? "ok" : "warn"),
  );

  byId("currentPlayer").textContent = result.finished ? "-" : analysis.player;
  byId("currentSide").textContent = result.finished ? "-" : analysis.side;
  byId("scoreLine").textContent = `NS ${result.nsTricks} / EW ${result.ewTricks}`;
  byId("bestLine").textContent = result.finished
    ? `Final NS ${result.nsTricks} / EW ${result.ewTricks}`
    : `NS ${analysis.bestNsTotal} / EW ${analysis.bestEwTotal}`;

  renderBoard(data);
  renderCardList(byId("optimalCards"), analysis.optimalCards, "pill ok");
  renderMoveButtons(byId("legalCards"), analysis.legalCards, analysis.optimalCards);
  renderEvaluations(analysis.evaluations);
  renderHistory(history);
}

function renderBoard(data) {
  const { analysis, history, result } = data;
  const setup = state.setup;
  const remainingHands = computeRemainingHands(setup, history);
  const optimalSet = new Set(analysis.optimalCards || []);
  const currentPlayer = result.finished ? null : analysis.player;

  renderSeat(byId("boardNorth"), remainingHands.N, currentPlayer === "N", optimalSet);
  renderSeat(byId("boardEast"), remainingHands.E, currentPlayer === "E", optimalSet);
  renderSeat(byId("boardSouth"), remainingHands.S, currentPlayer === "S", optimalSet);
  renderSeat(byId("boardWest"), remainingHands.W, currentPlayer === "W", optimalSet);

  byId("boardTrump").textContent = `Trump: ${setup.trump}`;
  byId("boardTarget").textContent = `Target: ${setup.target}`;
  byId("boardTricks").textContent = `Tricks: NS ${result.nsTricks} / EW ${result.ewTricks}`;
  const lastWinner = computeLastTrickWinner(setup, history);
  byId("lastWinner").textContent = `Last winner: ${lastWinner || "-"}`;

  renderCurrentTrick(computeCurrentTrick(setup, history));
}

function renderSeat(container, cards, isCurrentPlayer, optimalSet) {
  container.innerHTML = "";
  if (!cards.length) {
    const empty = document.createElement("span");
    empty.className = "table-card empty-card";
    empty.textContent = "Empty";
    container.appendChild(empty);
    return;
  }

  for (const card of cards) {
    const chip = document.createElement("span");
    const classes = ["table-card", suitClass(card)];
    if (isCurrentPlayer) {
      classes.push("current-player");
    }
    if (isCurrentPlayer && optimalSet.has(card)) {
      classes.push("optimal");
    }
    chip.className = classes.filter(Boolean).join(" ");
    chip.textContent = card;
    container.appendChild(chip);
  }
}

function renderCurrentTrick(trick) {
  const container = byId("currentTrick");
  container.innerHTML = "";

  for (const player of playerNames) {
    const slot = document.createElement("div");
    slot.className = "trick-slot";
    const item = trick.find((entry) => entry.player === player);
    slot.innerHTML = `<div class="seat-label">${player}</div>`;

    const card = document.createElement("span");
    card.className = `table-card ${item ? suitClass(item.card) : "empty-card"}`.trim();
    card.textContent = item ? item.card : "—";
    slot.appendChild(card);
    container.appendChild(slot);
  }
}

function renderCardList(container, cards, cls) {
  container.innerHTML = "";
  if (!cards || cards.length === 0) {
    const empty = document.createElement("span");
    empty.className = "pill warn";
    empty.textContent = "(none)";
    container.appendChild(empty);
    return;
  }

  for (const card of cards) {
    const pill = document.createElement("span");
    pill.className = cls;
    pill.textContent = card;
    container.appendChild(pill);
  }
}

function renderMoveButtons(container, cards, optimalCards) {
  container.innerHTML = "";
  if (!cards || cards.length === 0) {
    const empty = document.createElement("span");
    empty.className = "pill warn";
    empty.textContent = "No legal cards";
    container.appendChild(empty);
    return;
  }

  const optimalSet = new Set(optimalCards || []);
  for (const card of cards) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = `card-button ${optimalSet.has(card) ? "optimal" : ""}`.trim();
    button.textContent = card;
    button.addEventListener("click", () => {
      state.moves.push(card);
      analyse();
    });
    container.appendChild(button);
  }
}

function renderEvaluations(evaluations) {
  const body = byId("evaluationBody");
  body.innerHTML = "";

  for (const item of evaluations || []) {
    const row = document.createElement("tr");
    row.innerHTML = `
      <td>${item.card}</td>
      <td>${item.optimal ? "Yes" : "No"}</td>
      <td>${item.nsTotal}</td>
      <td>${item.ewTotal}</td>
      <td>${item.currentSideTotal}</td>
    `;
    body.appendChild(row);
  }
}

function renderHistory(history) {
  const body = byId("historyBody");
  body.innerHTML = "";

  for (let i = 0; i < history.length; i += 1) {
    const item = history[i];
    const row = document.createElement("tr");
    row.innerHTML = `
      <td>${i + 1}</td>
      <td>${item.player}</td>
      <td>${item.card}</td>
      <td>${item.optimal ? "Yes" : "No"}</td>
      <td>${(item.optimalCardsAtTurn || []).join(" ")}</td>
    `;
    body.appendChild(row);
  }
}

function loadPosition() {
  state.setup = readSetup();
  state.moves = [];
  state.response = null;
  analyse();
}

function clearAll() {
  writeSetup({
    target: 8,
    trump: "S",
    startMode: "leader",
    leader: "N",
    openingPlayer: "N",
    openingCard: "",
    north: "",
    east: "",
    south: "",
    west: "",
  });
  state.setup = null;
  state.moves = [];
  state.response = null;
  byId("badgeRow").innerHTML = "";
  byId("statusTitle").textContent = "Waiting for a position";
  byId("currentPlayer").textContent = "-";
  byId("currentSide").textContent = "-";
  byId("scoreLine").textContent = "NS 0 / EW 0";
  byId("bestLine").textContent = "-";
  byId("boardNorth").innerHTML = "";
  byId("boardEast").innerHTML = "";
  byId("boardSouth").innerHTML = "";
  byId("boardWest").innerHTML = "";
  byId("currentTrick").innerHTML = "";
  byId("boardTrump").textContent = "Trump: -";
  byId("boardTarget").textContent = "Target: -";
  byId("boardTricks").textContent = "Tricks: NS 0 / EW 0";
  byId("lastWinner").textContent = "Last winner: -";
  byId("optimalCards").innerHTML = "";
  byId("legalCards").innerHTML = "";
  byId("evaluationBody").innerHTML = "";
  byId("historyBody").innerHTML = "";
  byId("errorBox").classList.add("hidden");
}

function playManual() {
  const move = byId("manualMove").value.trim();
  if (!move) {
    return;
  }
  state.moves.push(move);
  byId("manualMove").value = "";
  analyse();
}

function undoMove() {
  if (state.moves.length === 0) {
    return;
  }
  state.moves.pop();
  analyse();
}

document.addEventListener("DOMContentLoaded", () => {
  writeSetup(sample);
  byId("startMode").addEventListener("change", setStartModeVisibility);
  byId("loadPosition").addEventListener("click", loadPosition);
  byId("clearAll").addEventListener("click", clearAll);
  byId("sampleDeal").addEventListener("click", () => writeSetup(sample));
  byId("playManual").addEventListener("click", playManual);
  byId("undoMove").addEventListener("click", undoMove);
  setStartModeVisibility();
});
