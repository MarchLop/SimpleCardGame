/**
 * game.js — 房间 + 游戏逻辑
 */

const urlParams = new URLSearchParams(window.location.search);
const roomId = parseInt(urlParams.get("room")) || 0;

document.getElementById("room-id").textContent = roomId;

// 玩家座位顺序：自己(bottom)→右→上→左
const POS = ["bottom", "right", "top", "left"];

const dom = {
    handCount: document.getElementById("hand-count"),
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
let myIdentity = 0;
let turnTimer = null;
let turnSeconds = 30;

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
    for (let seat = 0; seat < 4; seat++) {
        const pos = POS[seat], el = document.getElementById("player-" + pos);
        if (!el) continue;
        const globalIdx = myPlayerId ? ((myPlayerId - 1 + seat) % 4) : seat;
        const p = roomPlayers[globalIdx];
        const nameEl = el.querySelector(".player-name"), statusEl = el.querySelector(".player-status"), leftEl = el.querySelector(".player-cards-left"), identityEl = el.querySelector(".player-identity");
        if (p) {
            const avatarEl = el.querySelector(".avatar");
            if (avatarEl) avatarEl.textContent = p.isMe ? "我" : ((p.name || `玩家${p.id}`).trim().charAt(0) || "?");
            nameEl.textContent = p.name || `玩家${p.id}`;
            statusEl.textContent = p.actionState === "出牌中" ? "● 出牌中" :
                (p.actionState === "出牌" ? "● 已出牌" :
                (p.actionState === "PASS" ? "PASS" :
                (p.actionState === "超时" ? "⌛ 超时" :
                (p.ready ? "✓ 已准备" : (p.connected === false ? "断线" : "等待")))));
            leftEl.textContent = p.cardsLeft != null ? `${p.cardsLeft} 张` : "";
            if (identityEl) identityEl.textContent = p.publicIdentity ? identityText(p.publicIdentity) : "";
            el.classList.toggle("player-active", !!p.isActive);
            el.classList.toggle("player-disconnected", p.connected === false);
            el.classList.toggle("player-me", !!p.isMe);
        } else {
            const avatarEl = el.querySelector(".avatar");
            if (avatarEl) avatarEl.textContent = "?";
            nameEl.textContent = "-"; statusEl.textContent = ""; leftEl.textContent = "";
            if (identityEl) identityEl.textContent = "";
            el.classList.remove("player-active", "player-disconnected", "player-me");
        }
    }
}
function identityText(v) { return v === 3 ? "♠K + ♥2" : v === 1 ? "♠K队" : v === 2 ? "♥2队" : ""; }
function playerById(id) { return roomPlayers.find(p => p && p.id === id) || null; }

// ===== 房间事件 =====
window.onRoomJoined = function(msg) {
    if (msg.started) {
        dom.statusText.textContent = "游戏进行中";   // 已开局重连：不显示准备区，等快照
    } else {
        dom.statusText.textContent = `等待中 (${msg.player_count}/${msg.max_players})`;
        dom.readyArea.classList.remove("hidden");
    }
    // 初始化座位：player_id 从 1 开始
    // 自己=bottom, 2=right, 3=top, 4=left
    roomPlayers = [null, null, null, null];
    send({ type: "player_list" });
    // 如果我是创建者（player_count=1），把自己放在 bottom
    if (msg.player_count === 1 && msg.player_id) {
        const idx = msg.player_id - 1;  // player_id 1 → idx 0 = bottom
        roomPlayers[idx] = roomPlayers[idx] || {};
        roomPlayers[idx].id = msg.player_id;
        roomPlayers[idx].name = myName || "我";
        roomPlayers[idx].ready = false;
        roomPlayers[idx].isMe = true;
        updatePlayers();
    }
};

// 创建者也走同一套房间初始化逻辑，否则创建房间后不会出现准备按钮。
window.onRoomCreated = function(msg) { window.onRoomJoined(msg); };

window.onPlayerJoined = function(msg) {
    const idx = msg.player_id - 1;  // player_id 1→bottom, 2→right, 3→top, 4→left
    if (idx < 0 || idx >= 4) return;
    roomPlayers[idx] = roomPlayers[idx] || {};
    roomPlayers[idx].id = msg.player_id;
    roomPlayers[idx].name = msg.player_name || `玩家${msg.player_id}`;
    roomPlayers[idx].ready = roomPlayers[idx].ready || false;
    roomPlayers[idx].isMe = (msg.player_id === myPlayerId);
    updatePlayers();
    dom.statusText.textContent = `等待中 (${msg.player_count}/4)`;
};

window.onPlayerLeft = function(msg) { const idx = msg.player_id - 1; if (idx >= 0 && idx < roomPlayers.length) roomPlayers[idx] = null; updatePlayers(); };
window.onPlayerConnection = function(msg) { const p = playerById(msg.player_id); if (p) p.connected = !!msg.connected; updatePlayers(); };

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
                    connected: player.connected !== false,
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
    dom.handActions.classList.add("hidden");
    myHand = msg.hand || [];
    myIdentity = msg.identity || 0;
    dom.statusText.textContent = "游戏进行中";
    renderHand();
    updatePlayers();
    // 显示身份
    if (myIdentity) {
        const team = myIdentity === 1 ? "你: ♠K队" : myIdentity === 2 ? "你: ♥2队" : "你: ♥2+♠K";
        dom.turnHint.textContent = team;
        setTimeout(() => { dom.turnHint.textContent = ""; }, 3000);
    }
};

// ===== 轮到谁 =====
function startTurnTimer(seconds = 30) {
    stopTurnTimer(); turnSeconds = Math.max(1, Number(seconds) || 30); renderTimer();
    turnTimer = setInterval(() => {
        turnSeconds--;
        renderTimer();
        if (turnSeconds <= 0) {
            stopTurnTimer();
            dom.handActions.classList.add("hidden");
            selectedCards = [];
            renderHand();
            dom.turnHint.textContent = "操作超时，等待服务器自动过牌…";
        }
    }, 1000);
}
function renderTimer() { const el = document.getElementById("turn-timer"); if (el) el.textContent = `${Math.max(0, turnSeconds)}s`; }
function stopTurnTimer() { if (turnTimer) clearInterval(turnTimer); turnTimer = null; const el=document.getElementById("turn-timer"); if(el) el.textContent="--"; }

window.onYourTurn = function(msg) {
    stopTurnTimer();
    const current = Number(msg.player_id);
    roomPlayers.forEach(p => {
        if (!p) return;
        p.isActive = p.id === current;
        if (p.id === current) p.actionState = "出牌中";
    });

    renderCenterPlay(msg.last_play || [], msg.last_player || 0);
    if (msg.is_free) {
        clearCenterPlay();
    }

    updatePlayers();
    if (msg.player_id === myPlayerId) {
        dom.turnHint.textContent = msg.is_free ? "自由出牌" : "轮到你了";
        dom.handActions.classList.remove("hidden");
        dom.playBtn.disabled = false;
        dom.passBtn.disabled = !!msg.is_free;
        startTurnTimer(msg.timeout || 30);
    } else {
        const p = playerById(msg.player_id);
        dom.turnHint.textContent = `等待 ${p ? p.name : "对方"} 出牌...`;
        dom.handActions.classList.add("hidden");
    }
};

let myPlayerId = null;

// ===== 手牌渲染 =====
function renderHand() {
    if (dom.handCount) dom.handCount.textContent = `${myHand.length} 张`;
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
    dom.handActions.classList.add("hidden");
    selectedCards = [];
});

dom.passBtn.addEventListener("click", () => {
    send({ type: "pass" });
    selectedCards = [];
    dom.handActions.classList.add("hidden");
});

// ===== 出牌结果 =====
window.onPlayResult = function(msg) {
    stopTurnTimer();
    const p = playerById(msg.player_id);
    if (p) {
        p.actionState = "出牌";
        p.isActive = false;
        p.cardsLeft = msg.cards_left;
    }
    if (msg.player_id === myPlayerId) {
        msg.cards.forEach(c => {
            const idx = myHand.findIndex(h => h.num === c.num && h.suit === c.suit);
            if (idx !== -1) myHand.splice(idx, 1);
        });
        renderHand();
        dom.handActions.classList.add("hidden");
    }
    renderCenterPlay(msg.cards || [], msg.player_id, p ? p.name : `玩家${msg.player_id}`);
    dom.turnHint.textContent = `${p ? p.name : "玩家"} 出牌`;
    updatePlayers();
};

window.onPlayerPass = function(msg) {
    stopTurnTimer();
    const p = playerById(msg.player_id);
    if (p) {
        p.actionState = msg.timeout ? "超时" : "PASS";
        p.isActive = false;
    }
    if (msg.player_id === myPlayerId) {
        selectedCards = [];
        dom.handActions.classList.add("hidden");
    }
    if (msg.table_cleared) clearCenterPlay();
    const text = msg.timeout ? "超时，自动过牌" : "PASS";
    dom.turnHint.textContent = `${p ? p.name : "玩家"}：${text}`;
    updatePlayers();
};

window.onPlayerFinish = function(msg) {
    const p = playerById(msg.player_id);
    if (p) {
        p.actionState = `第 ${msg.rank} 名`;
        p.finished = true;
    }
    dom.turnHint.textContent = `${p ? p.name : '玩家'} 出完了！第 ${msg.rank} 名`;
    updatePlayers();
};

window.onTableStatus = function(msg) {
    console.log('[game] onTableStatus', msg);
    // table_status 是全员同步状态，不能让所有客户端都启动自己的倒计时。
    // 只有收到 your_turn 的客户端才启动本地计时器。
    if (Array.isArray(msg.player_status)) {
        for (const ps of msg.player_status) {
            const p = playerById(ps.player_id);
            if (!p) continue;
            p.cardsLeft = ps.playerhand_size;
            p.publicIdentity = ps.player_public_identity || 0;
            p.connected = ps.is_connected !== false;
            p.isActive = ps.player_id === msg.current_player;
            p.actionState = ps.action_state || (p.isActive ? "出牌中" : "等待");
            p.finished = !!ps.is_over;
        }
        updatePlayers();
    }
    if (msg.is_free) clearCenterPlay();
    else renderCenterPlay(msg.last_play || [], msg.last_player || 0);
};

window.onGameOver = function(msg) {
    stopTurnTimer();
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

function clearCenterPlay() {
    dom.lastPlayArea.innerHTML = "";
    const who = document.getElementById("last-play-player");
    if (who) who.textContent = "等待出牌";
    const hint = document.querySelector(".center-empty-hint");
    if (hint) hint.classList.remove("hidden");
}

function renderCenterPlay(cards, playerId, playerName) {
    if (!cards || !cards.length) {
        clearCenterPlay();
        return;
    }
    dom.lastPlayArea.innerHTML = cards.map(c =>
        `<img src="${cardImg(c.num, c.suit)}" alt="card">`
    ).join("");
    const p = playerById(Number(playerId));
    const who = document.getElementById("last-play-player");
    if (who) who.textContent = `${playerName || (p ? p.name : `玩家${playerId}`)} 出牌`;
    const hint = document.querySelector(".center-empty-hint");
    if (hint) hint.classList.add("hidden");
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
        showToast("不能这样出牌", "error");
        return;
    }
    if (msg.code === 1005) {
        showToast("自由出牌时不能 PASS", "error");
        return;
    }
    if (msg.code === 302 || msg.code === 306) {
        showToast("房间已不存在或已开局，返回大厅…", "error");
        setTimeout(() => { window.location.href = "index.html"; }, 1200);
        return;
    }
    if (msg.code === 304 && msg.room_id) {
        window.location.href = `game.html?room=${msg.room_id}`;
        return;
    }
    if (msg.code === 303) { showToast("房间已满", "error"); return; }
    console.warn("server error:", msg.code, msg.message);
};
