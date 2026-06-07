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
  "token": "a3f9k2m1x7p4q8w0",
  "name": "NONE"
}
```

> 当前服务器实现会对所有 `auth` 请求返回 `auth_ok`。
> 若客户端传入的 `token` 不存在，服务器会自动为该连接创建一个新的会话并回传新的 `token`。

> 建议客户端将 `token` 存储到 `localStorage` 或本地持久化，后续重连时再次发送。

---

## 二、房间与玩家管理

### 1. 修改昵称

**客户端 → 服务端**
```json
{ "type": "change_name", "name": "NONE" }
```

**服务端 → 客户端**
```json
{
  "type": "change_name_ok",
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
  "player_count": 1,
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

**服务端 → 退出者**
```json
{
  "type": "leave_room_ok",
  "room_id": 1,
  "player_count": 1
}
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

### 5. 获取房间列表（未来扩展）

当前服务器实现中尚未提供 `get_rooms` 请求，该消息类型属于后续功能扩展。

未来设计示例：

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

> 服务器会根据当前连接玩家所在房间判断准备状态，无需显式传递 `room_id`。

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

字段说明：
- `player_id`：当前准备玩家在房间内的编号，服务器按加入顺序分配（从 1 开始）。
- `room_id`：房间 ID。
- `ready_count`：已准备人数。
- `player_count`：房间总人数。

当房间中有 4 名玩家且全部准备后，服务器会自动开始游戏。

### 2. 游戏开始

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
  "identity": 0, 
  "first_turn": 0
}
```

字段说明：
- `hand`：当前玩家自己的手牌。
- `first_turn`：当前回合玩家在房间内的索引。
- `identity`：玩家所持有的特殊牌，1黑桃K，2红桃2，3都有，0都没有。

> 只有当房间人数达到 4 且所有玩家都发送 `ready` 后，服务器才会调用游戏启动逻辑并向房间内每个玩家发送 `game_start`。


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
}
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
| 101    | 未认证或未登录       |
| 201    | 未认证，无法创建房间 |
| 202    | 房间数量达到上限     |
| 300    | 请求参数缺失         |
| 301    | 未认证，无法加入房间 |
| 302    | 房间不存在           |
| 303    | 房间已满             |
| 304    | 已在当前房间中       |
| 305    | 不在房间中           |
| 306    | 游戏已开始           |

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

### 花色（suit）

| 值 | 花色   |
| -- | ------ |
| 0  | 梅花   |
| 1  | 方片   |
| 2  | 黑桃   |
| 3  | 红心   |

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
