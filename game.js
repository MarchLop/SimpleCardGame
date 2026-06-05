/**
 * game.js — 房间 + 游戏逻辑
 */

const urlParams = new URLSearchParams(window.location.search);
const roomId = parseInt(urlParams.get("room")) || 0;

document.getElementById("room-id").textContent = roomId;

const POS = ["top", "right", "bottom", "left"];  // 上下左右顺序

const dom = {
    statusText: document.getElementById("game-status-text"),
    lastPlayArea: document.getElementById("last-play-area"),
    turnHint: document.getElementById("turn-hint"),
    handCards: document.getElementById("hand-cards"),
    handActions: document.getElementById("hand-actions"),
    playBtn: document.getElementById("play-btn"),
    passBtn: document.getElementById("pass-btn"),
    readyArea: document.getElementById("ready-area"),
    readyBtn: document.getElementById("ready-btn"),
    chatMsgs: document.getElementById("chat-msgs"),
    chatInput: document.getElementById("chat-input"),
    chatSend: document.getElementById("chat-send-btn"),
    leaveBtn: document.getElementById("leave-btn"),
};

let selectedCards = [];

// 登录后加入房间
window.onAuthOk = function() {
    send({ type: "join_room", room_id: roomId });
    loadNick();
};

function loadNick() {
    myName = getStored("nanaki_name", "");
    if (myName) send({ type: "change_name", name: myName });
}

function updatePlayers() {
    for (let i = 0; i < 4; i++) {
        const pos = POS[i];
        const el = document.getElementById("player-" + pos);
        if (!el) continue;
        const p = roomPlayers[i];
        const nameEl = el.querySelector(".player-name");
        const statusEl = el.querySelector(".player-status");
        const leftEl = el.querySelector(".player-cards-left");
        if (p) {
            nameEl.textContent = p.name || `玩家${p.id}`;
            statusEl.textContent = p.ready ? "✓" : "";
            leftEl.textContent = p.cardsLeft != null ? `${p.cardsLeft}张` : "";
            el.classList.toggle("player-active", p.isActive);
            nameEl.style.color = p.isMe ? "#ffd700" : "";
        } else {
            nameEl.textContent = "-";
            statusEl.textContent = "";
            leftEl.textContent = "";
            el.classList.remove("player-active");
        }
    }
}

// ===== 房间事件 =====
window.onRoomJoined = function(msg) {
    dom.statusText.textContent = `等待中 (${msg.player_count}/${msg.max_players})`;
    // room_joined 不返回 player_id，但 player_joined 广播中会带
    // 显示准备区域并初始化玩家槽位
    dom.readyArea.classList.remove("hidden");
    roomPlayers = [];
    for (let i = 0; i < 4; i++) roomPlayers.push(null);
    // 请求当前房间玩家列表以填充已有玩家
    send({ type: "player_list" });
};

window.onPlayerJoined = function(msg) {
    // 后端 player_joined 的 player_id 从 1 开始，放到对应槽位
    const idx = msg.player_id - 1;
    while (roomPlayers.length < 4) roomPlayers.push(null);
    roomPlayers[idx] = roomPlayers[idx] || {};
    roomPlayers[idx].id = msg.player_id;
    roomPlayers[idx].name = msg.player_name || `玩家${msg.player_id}`;
    roomPlayers[idx].ready = roomPlayers[idx].ready || false;
    roomPlayers[idx].isMe = (roomPlayers[idx].id === myPlayerId);
    updatePlayers();
    dom.statusText.textContent = `等待中 (${msg.player_count}/4)`;
};

window.onPlayerLeft = function(msg) {
    // 清除对应槽位
    const idx = msg.player_id - 1;
    if (idx >= 0 && idx < roomPlayers.length) {
        roomPlayers[idx] = null;
    }
    updatePlayers();
};

// 离开房间确认
window.onLeaveRoomOk = function(msg) {
    window.location.href = "index.html";
};

window.onPlayerNameChanged = function(msg) {
    const p = roomPlayers.find(x => x && x.id === msg.player_id);
    if (p) p.name = msg.name;
    updatePlayers();
};

window.onPlayerReady = function(msg) {
    const idx = msg.player_id - 1;
    if (idx >=0 && idx < roomPlayers.length && roomPlayers[idx]) {
        roomPlayers[idx].ready = true;
    }
    updatePlayers();
};

window.onPlayerList = function(msg) {
    roomPlayers = [];
    for (let i = 0; i < 4; i++) roomPlayers.push(null);
    if (Array.isArray(msg.players)) {
        msg.players.forEach(player => {
            const idx = (player.player_id || 0) - 1;
            if (idx >= 0 && idx < roomPlayers.length) {
                roomPlayers[idx] = {
                    id: player.player_id,
                    name: player.player_name || `玩家${player.player_id}`,
                    ready: !!player.ready,
                    isMe: player.player_id === myPlayerId
                };
            }
        });
    }
    updatePlayers();
};

// ===== 准备按钮 =====
dom.readyBtn.addEventListener("click", () => {
    if (ws && ws.readyState === WebSocket.OPEN) {
        console.log('[game] clicking ready -> sending ready');
        send({ type: "ready" });
        dom.readyBtn.disabled = true;
        dom.readyBtn.textContent = "✓ 已准备";
    } else {
        console.warn('[game] ready clicked but WebSocket not open');
        alert('WebSocket 未连接，无法发送准备，请稍候再试');
    }
});

// ===== 游戏开始 =====
window.onGameStart = function(msg) {
    console.log('[game] onGameStart', msg);
    dom.readyArea.classList.add("hidden");
    dom.handActions.classList.remove("hidden");
    myHand = msg.hand || [];
    dom.statusText.textContent = "游戏进行中";
    renderHand();
    updatePlayers();
};

// ===== 轮到谁 =====
window.onYourTurn = function(msg) {
    console.log('[game] onYourTurn', msg);
    roomPlayers.forEach(p => { if (p) p.isActive = (p.id === msg.player_id); });
    updatePlayers();

    // 如果是自由出牌（清牌桌），清除所有玩家的出牌区
    if (msg.is_free) {
        for (let i = 0; i < 4; i++) {
            const pos = POS[i];
            const el = document.querySelector("#player-" + pos + " .player-played");
            if (el) el.innerHTML = "";
        }
    }

    if (msg.last_play && msg.last_play.length > 0) {
        dom.lastPlayArea.innerHTML = msg.last_play.map(c =>
            `<img src="${cardImg(c.num, c.suit)}" alt="card">`
        ).join("");
    } else {
        dom.lastPlayArea.innerHTML = "";
    }

    if (msg.player_id === myPlayerId) {
        dom.turnHint.textContent = msg.is_free ? "自由出牌" : "轮到你了";
        dom.handActions.classList.remove("hidden");
    } else {
        const p = roomPlayers.find(x => x && x.id === msg.player_id);
        dom.turnHint.textContent = `等待 ${p ? p.name : '对方'} 出牌...`;
        dom.handActions.classList.add("hidden");
    }
};

let myPlayerId = null;

// ===== 手牌渲染 =====
function renderHand() {
    dom.handCards.innerHTML = myHand.map((c, i) => {
        const sel = selectedCards.includes(i) ? "selected" : "";
        return `<img src="${cardImg(c.num, c.suit)}" class="${sel}" data-idx="${i}" alt="card">`;
    }).join("");

    dom.handCards.querySelectorAll("img").forEach(img => {
        img.addEventListener("click", () => {
            const idx = parseInt(img.dataset.idx);
            const pos = selectedCards.indexOf(idx);
            if (pos === -1) selectedCards.push(idx);
            else selectedCards.splice(pos, 1);
            renderHand();
        });
    });
}

// ===== 出牌/过牌 =====
dom.playBtn.addEventListener("click", () => {
    if (selectedCards.length === 0) return;
    const cards = selectedCards.map(i => myHand[i]);
    send({ type: "play", cards: cards });
    selectedCards = [];
});

dom.passBtn.addEventListener("click", () => {
    send({ type: "pass" });
    selectedCards = [];
    dom.handActions.classList.add("hidden");
});

// ===== 出牌结果 =====
window.onPlayResult = function(msg) {
    if (msg.player_id === myPlayerId) {
        msg.cards.forEach(c => {
            const idx = myHand.findIndex(h => h.num === c.num && h.suit === c.suit);
            if (idx !== -1) myHand.splice(idx, 1);
        });
        renderHand();
        dom.handActions.classList.add("hidden");
    }
    // 在对应玩家位置显示出牌
    showPlayedCards(msg.player_id, msg.cards);
    // 中央也显示出牌
    dom.lastPlayArea.innerHTML = msg.cards.map(c =>
        `<img src="${cardImg(c.num, c.suit)}" alt="card">`
    ).join("");
    const p = roomPlayers.find(x => x && x.id === msg.player_id);
    if (p) p.cardsLeft = msg.cards_left;
    updatePlayers();
    dom.turnHint.textContent = "";
};

window.onPlayInvalid = function(msg) {
    // 后端没有 play_invalid type，通过 error 处理
    // 此函数保留但不再由 ws.js 分发
};

window.onPlayerPass = function(msg) {
    dom.turnHint.textContent = "";
    // pass 玩家清空出牌区
    showPlayedCards(msg.player_id, []);
};

window.onPlayerFinish = function(msg) {
    const p = roomPlayers.find(x => x && x.id === msg.player_id);
    dom.turnHint.textContent = `${p ? p.name : '玩家'} 出完了！第 ${msg.rank} 名`;
};

window.onGameOver = function(msg) {
    const ranking = msg.ranking || [];
    const winnerId = ranking[0];
    const winner = roomPlayers.find(x => x && x.id === winnerId);
    dom.turnHint.textContent = `游戏结束！${winner ? winner.name : '玩家'} 获胜！`;
    showToast(`游戏结束！${winner ? winner.name : '玩家'} 获胜！`, "success");
    dom.handActions.classList.add("hidden");
    selectedCards = [];
};

// ===== 聊天 =====
dom.chatSend.addEventListener("click", sendChat);
dom.chatInput.addEventListener("keydown", (e) => { if (e.key === "Enter") sendChat(); });

function sendChat() {
    const text = dom.chatInput.value.trim();
    if (!text) return;
    send({ type: "chat", text: text });
    dom.chatInput.value = "";
}

window.onChatMsg = function(msg) {
    const isMe = msg.player_id === myPlayerId;
    const name = isMe ? myName : (msg.player_name || `玩家${msg.player_id}`);
    const el = document.createElement("div");
    el.className = "chat-msg";
    el.innerHTML = `<span class="sender ${isMe ? 'me' : ''}">${escapeHtml(name)}:</span>${escapeHtml(msg.text)}`;
    dom.chatMsgs.appendChild(el);
    dom.chatMsgs.scrollTop = dom.chatMsgs.scrollHeight;
};

function showPlayedCards(playerId, cards) {
    const idx = roomPlayers.findIndex(p => p && p.id === playerId);
    if (idx < 0 || idx >= 4) return;
    const pos = POS[idx];
    const playedEl = document.querySelector("#player-" + pos + " .player-played");
    if (!playedEl) return;
    playedEl.innerHTML = cards.map(c =>
        '<img src="' + cardImg(c.num, c.suit) + '" alt="card">'
    ).join("");
    // 更新手牌数
    const p = roomPlayers[idx];
    if (p && cards.length > 0) {
        // 不在这里减手牌，只在 play_result 里处理
    }
}

function showToast(message, type) {
    const el = document.createElement('div');
    el.className = 'toast' + (type === 'error' ? ' toast-error' : type === 'success' ? ' toast-success' : '');
    el.textContent = message;
    document.body.appendChild(el);
    setTimeout(() => el.remove(), 2000);
}

function escapeHtml(s) {
    return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

// ===== 退出房间 =====
dom.leaveBtn.addEventListener("click", () => {
    send({ type: "leave_room" });
    // 不直接跳转，等 leave_room_ok 再跳
});

window.onError = function(msg) {
    if (msg.code === 1004 || msg.message === "invalid_play") {
        showToast(msg.message || "不合法的牌型", "error");
    } else {
        console.warn("server error:", msg.code, msg.message);
    }
};
