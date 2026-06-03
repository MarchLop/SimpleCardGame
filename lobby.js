/**
 * lobby.js — 大厅逻辑
 */

const nickInput = document.getElementById("nick-input");
const saveNickBtn = document.getElementById("save-nick-btn");

function loadNick() {
    myName = getStored("nanaki_name", "");
    if (myName) {
        nickInput.value = myName;
        send({ type: "change_name", name: myName });
    }
}

// 后端新增：change_name_ok 确认昵称修改成功
window.onChangeNameOk = function(msg) {
    myName = msg.name;
    setStored("nanaki_name", myName);
};

saveNickBtn.addEventListener("click", () => {
    const name = nickInput.value.trim();
    if (!name) return;
    myName = name;
    setStored("nanaki_name", name);
    send({ type: "change_name", name: name });
});

function showLobby() {
    document.getElementById("login-section").classList.add("hidden");
    document.getElementById("lobby-section").classList.remove("hidden");
}

window.onAuthOk = function() {
    loadNick();
    showLobby();
    refreshRooms();
};

function refreshRooms() {
    send({ type: "get_rooms" });
}

window.onRoomList = function(rooms) {
    const container = document.getElementById("room-list");
    if (!rooms || rooms.length === 0) {
        container.innerHTML = '<div class="empty-msg">暂无房间，点击上方创建</div>';
        return;
    }
    container.innerHTML = rooms.map(r => {
        const max = r.max_players || 4;
        const tag = r.started
            ? '<span class="tag tag-playing">游戏中</span>'
            : '<span class="tag tag-ready">等待中</span>';
        const disabled = r.started || r.player_count >= max ? "disabled" : "";
        return `<div class="room-item">
            <div class="room-info">房间 #${r.room_id} (${r.player_count}/${max}) ${tag}</div>
            <button class="btn-join" data-room="${r.room_id}" ${disabled}>
                ${r.started ? "已开始" : r.player_count >= max ? "已满" : "加入"}
            </button>
        </div>`;
    }).join("");

    container.querySelectorAll(".btn-join").forEach(btn => {
        btn.addEventListener("click", () => {
            const roomId = parseInt(btn.dataset.room);
            send({ type: "join_room", room_id: roomId });
        });
    });
};

document.getElementById("create-room-btn").addEventListener("click", () => {
    send({ type: "create_room" });
});

window.onRoomCreated = function(msg) {
    window.location.href = `game.html?room=${msg.room_id}`;
};

window.onRoomJoined = function(msg) {
    window.location.href = `game.html?room=${msg.room_id}`;
};

setInterval(refreshRooms, 5000);
