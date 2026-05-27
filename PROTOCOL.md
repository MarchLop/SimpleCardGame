# SimpleCardGame 前后端通信协议

## 概述

- 通信方式：WebSocket
- 地址：`ws://localhost:9002`
- 数据格式：JSON（UTF-8 编码）
- 所有消息必须包含 `type` 字段。
- 只通过 WebSocket 发送，无 HTTP 请求。

---

## 一、认证与会话

### 1. 认证流程

```
客户端                         服务端
  │                              │
  ├── WebSocket 连接 ───────────→│
  │                              │
  ├── { type: "auth" } ───────→│
  │                              │
  │←─ { type: "auth_ok",       │
  │      token: "xxx" }        │
  │                              │
  │   断线后重连时继续使用 token   │
  │                              │
  ├── { type: "auth",         │
  │      token: "xxx" } ───────→│
  │                              │
  │←─ { type: "auth_ok",       │
  │      token: "xxx" }        │
```

### 2. 客户端 → 服务端

**首次认证**
```json
{ "type": "auth" }
```

**断线重连认证**
```json
{ "type": "auth", "token": "a3f9k2m1x7p4q8w0" }
```

### 3. 服务端 → 客户端

**认证成功**
```json
{
  "type": "auth_ok",
  "token": "a3f9k2m1x7p4q8w0"
}
```

**认证失败**
```json
{
  "type": "error",
  "code": 1007,
  "message": "token 无效"
}
```

> 建议客户端将 `token` 存储到 `localStorage` 或本地持久化，后续重连时再次发送。

---

## 二、房间与玩家管理

### 1. 修改昵称

**客户端 → 服务端**
```json
{ "type": "change_name", "name": "NONE" }
```

**服务端 → 房间广播**
```json
{
  "type": "player_name_changed",
  "player_id": 1,
  "name": "NONE"
}
```

### 2. 创建房间

**客户端 → 服务端**
```json
{ "type": "create_room" }
```

**服务端 → 客户端**
```json
{
  "type": "room_created",
  "room_id": 1,
  "max_players": 4,
  "started": false
}
```

### 3. 加入房间

**客户端 → 服务端**
```json
{ "type": "join_room", "room_id": 1 }
```

**服务端 → 加入者**
```json
{
  "type": "room_joined",
  "room_id": 1,
  "player_count": 2,
  "max_players": 4,
  "started": false
}
```

**服务端 → 房间内广播**
```json
{
  "type": "player_joined",
  "player_id": 2,
  "player_name": "NONE",
  "room_id": 1,
  "player_count": 2
}
```

### 4. 退出房间

**客户端 → 服务端**
```json
{ "type": "leave_room" }
```

**服务端 → 房间广播**
```json
{
  "type": "player_left",
  "player_id": 2,
  "room_id": 1,
  "player_count": 1
}
```

### 5. 获取房间列表

**客户端 → 服务端**
```json
{ "type": "get_rooms" }
```

**服务端 → 客户端**
```json
{
  "type": "room_list",
  "rooms": [
    {
      "room_id": 1,
      "player_count": 2,
      "max_players": 4,
      "started": false
    },
    {
      "room_id": 2,
      "player_count": 4,
      "max_players": 4,
      "started": true
    }
  ]
}
```

---

## 三、游戏阶段

### 1. 准备

**客户端 → 服务端**
```json
{ "type": "ready" }
```

**服务端 → 房间广播**
```json
{
  "type": "player_ready",
  "player_id": 1,
  "room_id": 1,
  "ready_count": 2,
  "player_count": 4
}
```

### 2. 游戏开始

所有房间玩家 `ready` 后自动开始游戏。

**服务端 → 每个玩家**
```json
{
  "type": "game_start",
  "room_id": 1,
  "hand": [
    { "num": 3, "suit": 0 },
    { "num": 7, "suit": 2 },
    ...
  ],
  "first_turn": 0
}
```

> `hand` 仅包含接收该消息玩家自己的手牌。

### 3. 当前玩家回合

**服务端 → 当前玩家**
```json
{
  "type": "your_turn",
  "room_id": 1,
  "player_id": 0,
  "last_play": [
    { "num": 8, "suit": 1 },
    { "num": 8, "suit": 2 }
  ],
  "last_player": 3,
  "is_free": false
}
```

字段含义：
- `last_play`：上一次出牌，若桌面已清则为 `[]`。
- `last_player`：上一次出牌玩家的 `player_id`。
- `is_free`：若为 `true`，表示本回合可以随意出牌，不需要压制之前的牌。

### 4. 出牌

**客户端 → 服务端**
```json
{
  "type": "play",
  "cards": [
    { "num": 10, "suit": 0 },
    { "num": 10, "suit": 1 },
    { "num": 10, "suit": 2 }
  ]
```

**服务端 → 房间广播（成功）**
```json
{
  "type": "play_result",
  "room_id": 1,
  "player_id": 0,
  "cards": [
    { "num": 10, "suit": 0 },
    { "num": 10, "suit": 1 },
    { "num": 10, "suit": 2 }
  ],
  "cards_left": 10,
  "next_turn": 1
}
```

**服务端 → 出牌者（失败）**
```json
{
  "type": "play_invalid",
  "code": 1004,
  "message": "牌型不合法"
}
```

### 5. 不出

**客户端 → 服务端**
```json
{ "type": "pass" }
```

**服务端 → 房间广播**
```json
{
  "type": "player_pass",
  "room_id": 1,
  "player_id": 1,
  "next_turn": 2,
  "pass_count": 2
}
```

当 `pass_count == 3` 时，桌面清空，下一位玩家的 `your_turn` 中 `is_free` 为 `true`。

### 6. 玩家出完

**服务端 → 房间广播**
```json
{
  "type": "player_finish",
  "room_id": 1,
  "player_id": 0,
  "rank": 1
}
```

### 7. 游戏结束

**服务端 → 房间广播**
```json
{
  "type": "game_over",
  "room_id": 1,
  "ranking": [0, 2, 1, 3]
}
```

> `ranking` 按最终名次排列，第一项为获胜玩家的 `player_id`。

---

## 四、聊天

**客户端 → 服务端**
```json
{ "type": "chat", "text": "好牌" }
```

**服务端 → 房间广播**
```json
{
  "type": "chat",
  "room_id": 1,
  "player_id": 1,
  "player_name": "NONE",
  "text": "好牌"
}
```

---

## 五、错误处理

### 通用格式

```json
{
  "type": "error",
  "code": 1001,
  "message": "房间已满"
}
```

### 错误码

| 错误码 | 说明                 |
| ------ | -------------------- |
| 1001   | 房间已满             |
| 1002   | 房间不存在           |
| 1003   | 游戏已开始，无法加入 |
| 1004   | 出牌不合法           |
| 1005   | 未轮到该玩家         |
| 1006   | 未在房间中           |
| 1007   | 认证失败，token 无效 |
| 1008   | 消息格式错误         |

---

## 六、数据结构

### 牌（Card）

```json
{ "num": 3, "suit": 0 }
```

`num` 表示点数：

| 值  | 牌面 |
| --- | ---- |
| 0   | 3    |
| 1   | 4    |
| 2   | 5    |
| 3   | 6    |
| 4   | 7    |
| 5   | 8    |
| 6   | 9    |
| 7   | 10   |
| 8   | J    |
| 9   | Q    |
| 10  | K    |
| 11  | A    |
| 12  | 2    |

`suit` 表示花色：

| 值  | 花色   |
| --- | ------ |
| 0   | ♣ 梅花 |
| 1   | ♦ 方块 |
| 2   | ♠ 黑桃 |
| 3   | ♥ 红桃 |

---

## 七、交互示例

1. 客户端连接 WebSocket → `ws://localhost:9002`
2. 客户端 → `{ "type": "auth" }`
   服务端 → `{ "type": "auth_ok", "token": "..." }`
3. 客户端 → `{ "type": "change_name", "name": "亮神" }`
4. 客户端 → `{ "type": "create_room" }`
   服务端 → `{ "type": "room_created", "room_id": 1, "max_players": 4, "started": false }`
5. 其他玩家加入房间，服务端广播 `{ "type": "player_joined", ... }`
6. 所有玩家发送 `{ "type": "ready" }`
7. 服务端发送 `{ "type": "game_start", "hand": [...], "first_turn": 0 }`
8. 服务端依次发送 `{ "type": "your_turn", ... }`
9. 玩家发送 `{ "type": "play", "cards": [...] }` 或 `{ "type": "pass" }`
10. 服务端发送 `{ "type": "game_over", "ranking": [...] }`
