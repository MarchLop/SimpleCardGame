/**
 * ws.js — WebSocket 连接与认证
 */
const WS_URL = "ws://localhost:9002";
const CARD_BASE = "https://raw.githubusercontent.com/djp952/external-vectorcards/master/svg/";

// 牌面映射（game.h 定义：THREE=0 ... ACE=11 TWO=12）
const SUIT_NAMES = ["clubs", "diamonds", "spades", "hearts"];
const NUM_NAMES = [
    "three","four","five","six","seven","eight","nine","ten",
    "jack","queen","king","ace","two"
];

// 全局状态
let ws = null;
let myToken = null;
let myName = "";
let inRoom = null;
let roomPlayers = []; // [{id, name, ready, cardsLeft}]
let myHand = [];
let lastPlay = [];

// ===== 工具函数 =====

function cardImg(num, suit) {
    return `${CARD_BASE}${SUIT_NAMES[suit]}-${NUM_NAMES[num]}.svg`;
}

function getStored(key, def) {
    try { return localStorage.getItem(key) || def; } catch { return def; }
}
function setStored(key, val) {
    try { localStorage.setItem(key, val); } catch {}
}

// ===== 连接管理 =====

function connect() {
    if (ws && ws.readyState === WebSocket.OPEN) return;
    setStatus("connecting");
    ws = new WebSocket(WS_URL);
    ws.onopen = () => {
        setStatus("online");
        myToken = getStored("nanaki_token", "");
        send({ type: "auth", token: myToken });
    };
    ws.onmessage = (e) => {
        try { handleMsg(JSON.parse(e.data)); }
        catch { console.warn("parse error:", e.data); }
    };
    ws.onclose = () => {
        setStatus("offline");
        ws = null;
        setTimeout(connect, 3000);
    };
    ws.onerror = () => { ws && ws.close(); };
}

function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(obj));
    }
}

function setStatus(state) {
    const dot = document.getElementById("status-dot");
    const text = document.getElementById("status-text");
    if (!dot || !text) return;
    dot.className = "dot-" + state;
    const map = { offline: "未连接", online: "已连接", connecting: "连接中...", error: "连接失败" };
    text.textContent = map[state] || state;
}

// ===== 消息分发 =====

function handleMsg(msg) {
    const type = msg.type;

    if (type === "auth_ok") {
        myToken = msg.token;
        setStored("nanaki_token", myToken);
        // auth_ok 现在可能带 name 字段（后端已添加）
        if (msg.name && msg.name !== "NONE") {
            myName = msg.name;
            setStored("nanaki_name", myName);
        }
        if (typeof onAuthOk === "function") onAuthOk();
        return;
    }

    // 后端新增：change_name_ok
    if (type === "change_name_ok" && typeof onChangeNameOk === "function") {
        onChangeNameOk(msg);
        return;
    }
    // 后端新增：leave_room_ok
    if (type === "leave_room_ok" && typeof onLeaveRoomOk === "function") {
        onLeaveRoomOk(msg);
        return;
    }

    if (type === "room_list" && typeof onRoomList === "function") {
        onRoomList(msg.rooms);
        return;
    }
    if (type === "room_created" && typeof onRoomCreated === "function") {
        onRoomCreated(msg);
        return;
    }
    if (type === "room_joined" && typeof onRoomJoined === "function") {
        onRoomJoined(msg);
        return;
    }
    if (type === "player_joined" && typeof onPlayerJoined === "function") {
        onPlayerJoined(msg);
        return;
    }
    if (type === "player_left" && typeof onPlayerLeft === "function") {
        onPlayerLeft(msg);
        return;
    }
    if (type === "player_name_changed" && typeof onPlayerNameChanged === "function") {
        onPlayerNameChanged(msg);
        return;
    }
    if (type === "player_ready" && typeof onPlayerReady === "function") {
        onPlayerReady(msg);
        return;
    }

    if (type === "game_start" && typeof onGameStart === "function") {
        onGameStart(msg);
        return;
    }
    if (type === "your_turn" && typeof onYourTurn === "function") {
        onYourTurn(msg);
        return;
    }
    if (type === "play_result" && typeof onPlayResult === "function") {
        onPlayResult(msg);
        return;
    }
    if (type === "player_pass" && typeof onPlayerPass === "function") {
        onPlayerPass(msg);
        return;
    }
    if (type === "player_finish" && typeof onPlayerFinish === "function") {
        onPlayerFinish(msg);
        return;
    }
    if (type === "game_over" && typeof onGameOver === "function") {
        onGameOver(msg);
        return;
    }

    if (type === "chat" && typeof onChatMsg === "function") {
        onChatMsg(msg);
        return;
    }
    if (type === "error" && typeof onError === "function") {
        onError(msg);
        return;
    }
}

// 自动连接
document.addEventListener("DOMContentLoaded", () => {
    setTimeout(connect, 200);
});
