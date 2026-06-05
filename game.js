/**
 * game.js — 房间 + 游戏逻辑
 */

const urlParams = new URLSearchParams(window.location.search);
const roomId = parseInt(urlParams.get("room")) || 0;

document.getElementById("room-id").textContent = roomId;

const dom = {
    statusText: document.getElementById("game-status-text"),
    playersInfo: document.getElementById("players-info"),
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
    const slots = dom.playersInfo.querySelectorAll(".player-slot");
    for (let i = 0; i < 4; i++) {
        const slot = slots[i];
        const p = roomPlayers[i];
        if (p) {
            slot.querySelector(".player-name").textContent = p.name || `玩家${p.id}`;
            slot.querySelector(".player-status").textContent = p.ready ? "✓ 已准备" : "";
            const leftEl = slot.querySelector(".player-cards-left");
            leftEl.textContent = p.cardsLeft != null ? `${p.cardsLeft}张` : "";
            // 身份标记：红桃2队/黑桃K队
            if (p.team) {
                leftEl.textContent += (p.team === "red" ? " ♥" : " ♠");
            }
            slot.classList.toggle("player-active", p.isActive);
            slot.querySelector(".player-name").style.color = p.isMe ? "#ffd700" : "";
        } else {
            slot.querySelector(".player-name").textContent = "-";
            slot.querySelector(".player-status").textContent = "";
            slot.querySelector(".player-cards-left").textContent = "";
            slot.classList.remove("player-active");
        }
    }
}

// ===== 房间事件 =====
window.onRoomJoined = function(msg) {
    dom.statusText.textContent = `等待中 (${msg.player_count}/${msg.max_players})`;
    // room_joined 不返回 player_id，但 player_joined 广播中会带
};

window.onPlayerJoined = function(msg) {
    // 后端 player_joined 的 player_id 从 1 开始
    const exists = roomPlayers.find(p => p && p.id === msg.player_id);
    if (!exists) {
        roomPlayers.push({
            id: msg.player_id,
            name: msg.player_name || `玩家${msg.player_id}`,
            ready: false
        });
    }
    updatePlayers();
    dom.statusText.textContent = `等待中 (${msg.player_count}/4)`;
};

window.onPlayerLeft = function(msg) {
    roomPlayers = roomPlayers.filter(p => p && p.id !== msg.player_id);
    while (roomPlayers.length < 4) roomPlayers.push(null);
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
    const p = roomPlayers.find(x => x && x.id === msg.player_id);
    if (p) p.ready = true;
    updatePlayers();
};

// ===== 准备按钮 =====
dom.readyBtn.addEventListener("click", () => {
    send({ type: "ready" });
    dom.readyBtn.disabled = true;
    dom.readyBtn.textContent = "已准备";
});

// ===== 游戏开始 =====
window.onGameStart = function(msg) {
    dom.readyArea.classList.add("hidden");
    dom.handActions.classList.remove("hidden");
    myHand = msg.hand || [];
    dom.statusText.textContent = "游戏进行中";
    renderHand();
    updatePlayers();
};

// ===== 轮到谁 =====
window.onYourTurn = function(msg) {
    roomPlayers.forEach(p => { if (p) p.isActive = (p.id === msg.player_id); });
    updatePlayers();

    if (msg.last_play && msg.last_play.length > 0) {
        dom.lastPlayArea.innerHTML = msg.last_play.map(c =>
            `<img src="${cardImg(c.num, c.suit)}" alt="card">`
        ).join("");
    } else {
        dom.lastPlayArea.innerHTML = "<div class='table-clear-hint'>牌桌已清</div>";
    }
    dom.turnHint.className = "turn-highlight";
    if (msg.player_id === myPlayerId) {
        dom.turnHint.textContent = msg.is_free ? "🎯 自由出牌" : "🎯 轮到你了！";
        dom.handActions.classList.remove("hidden");
        dom.playBtn.disabled = false;
        dom.passBtn.disabled = false;
    } else {
        const p = roomPlayers.find(x => x && x.id === msg.player_id);
        dom.turnHint.textContent = `⏳ 等待 ${p ? p.name : '对方'} 出牌...`;
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
    dom.lastPlayArea.innerHTML = msg.cards.map(c =>
        `<img src="${cardImg(c.num, c.suit)}" alt="card">`
    ).join("");
    const p = roomPlayers.find(x => x && x.id === msg.player_id);
    if (p) {
        p.cardsLeft = msg.cards_left;
        // 标记身份（红桃2或黑桃K）
        if (msg.cards) {
            for (const c of msg.cards) {
                if (c.num === 12 && c.suit === 3) p.team = "red";   // 红桃2
                if (c.num === 10 && c.suit === 2) p.team = "spade"; // 黑桃K
            }
        }
    }
    updatePlayers();
    dom.turnHint.textContent = "";
    dom.turnHint.className = "";
};

window.onPlayInvalid = function(msg) {
    // 后端没有 play_invalid type，通过 error 处理
    // 此函数保留但不再由 ws.js 分发
};

window.onPlayerPass = function(msg) {
    dom.turnHint.textContent = "";
    dom.lastPlayArea.innerHTML = "";
};

window.onPlayerFinish = function(msg) {
    const p = roomPlayers.find(x => x && x.id === msg.player_id);
    dom.turnHint.textContent = `${p ? p.name : '玩家'} 出完了！第 ${msg.rank} 名`;
};

window.onGameOver = function(msg) {
    const ranking = msg.ranking || [];
    const winnerId = ranking[0];
    const winner = roomPlayers.find(x => x && x.id === winnerId);
    dom.turnHint.textContent = `🏆 游戏结束！${winner ? winner.name : '玩家'} 获胜！`;
    dom.turnHint.className = "gameover-highlight";
    dom.handActions.classList.add("hidden");
    document.getElementById("hand-cards").innerHTML = "";
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
        alert("出牌无效：" + (msg.message || "不合法的牌型"));
    } else {
        console.warn("server error:", msg.code, msg.message);
    }
};
