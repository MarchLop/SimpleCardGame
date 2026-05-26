# SimpleCardGame 前后端通信协议

## 概述

- 通信方式：WebSocket
- 地址：`ws://localhost:9002`
- 数据格式：JSON（UTF-8 编码）

---

## 一、连接与认证

### 流程

```
客户端                            服务端
  │                                │
  ├── WebSocket 连接 ──────────────→│
  │                                │
  ├── { type: "auth" } ───────────→│
  │                                ├── 生成新 token
  │←─ { type: "auth_ok",           │
  │      token: "xxx",             │
  │      player_id: 1 }            │
  │                                │
  │          （后续操作）           │
  │                                │
  ├── { type: "auth",              │  ← 断线重连时
  │      token: "xxx" } ──────────→│
  │←─ { type: "auth_ok",           │
  │      player_id: 1 }            │
```

### 客户端 → 服务端

**连接认证（首次）**
```json
{ "type": "auth" }
```

**连接认证（重连）**
```json
{ "type": "auth", "token": "a3f9k2m1x7p4q8w0" }
```

### 服务端 → 客户端

**认证成功**
```json
{
    "type": "auth_ok",
    "token": "a3f9k2m1x7p4q8w0",
    "player_id": 1
}
```

`token` 和 `player_id` 由客户端保存（localStorage），重连时使用。

---

## 二、玩家与房间管理

### 修改昵称

**客户端 → 服务端**
```json
{ "type": "change_name", "name": "亮神" }
```

**服务端 → 客户端**（广播给同房间玩家）
```json
{
    "type": "player_name",
    "player_id": 1,
    "name": "亮神"
}
```

### 创建房间

**客户端 → 服务端**
```json
{ "type": "create_room" }
```

**服务端 → 客户端**
```json
{
    "type": "room_created",
    "room_id": 1,
    "player_id": 1
}
```

### 加入房间

**客户端 → 服务端**
```json
{ "type": "join_room", "room_id": 1 }
```

**服务端 → 客户端**（自己）
```json
{
    "type": "room_joined",
    "room_id": 1,
    "player_id": 2
}
```

**服务端 → 客户端**（广播给房间其他人）
```json
{
    "type": "player_joined",
    "player_id": 2,
    "player_name": "智宝",
    "room_id": 1,
    "player_count": 2
}
```

### 退出房间

**客户端 → 服务端**
```json
{ "type": "leave_room" }
```

**服务端 → 客户端**（广播）
```json
{
    "type": "player_left",
    "player_id": 2,
    "room_id": 1,
    "player_count": 1
}
```

### 获取房间列表

**客户端 → 服务端**
```json
{ "type": "get_rooms" }
```

**服务端 → 客户端**
```json
{
    "type": "room_list",
    "rooms": [
        { "room_id": 1, "player_count": 2, "max_players": 4, "started": false },
        { "room_id": 2, "player_count": 4, "max_players": 4, "started": true }
    ]
}
```

---

## 三、游戏流程

### 准备

**客户端 → 服务端**
```json
{ "type": "ready" }
```

**服务端 → 客户端**（广播）
```json
{
    "type": "player_ready",
    "player_id": 1,
    "room_id": 1,
    "ready_count": 1
}
```

### 游戏开始（4 人全部 ready 后自动触发）

**服务端 → 客户端**（单独发给每个玩家，只包含自己的手牌）
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

### 轮到某玩家

**服务端 → 客户端**
```json
{
    "type": "your_turn",
    "player_id": 0,
    "last_play": [
        { "num": 8, "suit": 1 },
        { "num": 8, "suit": 2 }
    ],
    "last_player": 3,
    "is_free": false
}
```

参数说明：
- `last_play`：上一次出的牌（空数组表示牌桌已清）
- `last_player`：上一次出牌的玩家 id
- `is_free`：true 表示可以自由出牌（其他三人均已 pass）

### 出牌

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

**服务端 → 客户端**（广播，出牌成功）
```json
{
    "type": "play_result",
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

**服务端 → 客户端**（出牌无效）
```json
{
    "type": "play_invalid",
    "reason": "牌型不合法"
}
```

### 不出牌

**客户端 → 服务端**
```json
{ "type": "pass" }
```

**服务端 → 客户端**（广播）
```json
{
    "type": "player_pass",
    "player_id": 1,
    "next_turn": 2,
    "pass_count": 2
}
```

当 `pass_count == 3` 时，下一家的 `your_turn` 中 `is_free = true`，且桌面清空。

### 玩家手牌为空

**服务端 → 客户端**（广播）
```json
{
    "type": "player_finish",
    "player_id": 0,
    "rank": 1
}
```

### 游戏结束

**服务端 → 客户端**（广播）
```json
{
    "type": "game_over",
    "winner_id": 0,
    "ranking": [0, 2, 1, 3]
}
```

---

## 四、聊天

### 发送

**客户端 → 服务端**
```json
{ "type": "chat", "text": "好牌" }
```

**服务端 → 客户端**（广播给同房间）
```json
{
    "type": "chat",
    "player_id": 1,
    "player_name": "亮神",
    "text": "好牌"
}
```

---

## 五、错误处理

### 通用错误格式

```json
{
    "type": "error",
    "code": 1001,
    "message": "房间已满"
}
```

### 错误码

| 错误码 | 说明 |
|--------|------|
| 1001   | 房间已满 |
| 1002   | 房间不存在 |
| 1003   | 游戏已开始，无法加入 |
| 1004   | 出牌不合法 |
| 1005   | 未轮到该玩家 |
| 1006   | 未在房间中 |
| 1007   | 认证失败，token 无效 |
| 1008   | 消息格式错误 |

---

## 六、数据结构

### 牌（Card）

```json
{ "num": 3, "suit": 0 }
```

num（牌面值）：
| 值  | 牌面 |
|-----|------|
| 0   | 3    |
| 1   | 4    |
| ... | ...  |
| 11  | A    |
| 12  | 2    |

suit（花色）：
| 值 | 花色 |
|----|------|
| 0  | ♣ 梅花 |
| 1  | ♦ 方块 |
| 2  | ♠ 黑桃 |
| 3  | ♥ 红桃 |

---

## 七、完整交互示例

```
1. 客户端连接 WebSocket → ws://localhost:9002

2. 客户端 → { type: "auth" }
   服务端 → { type: "auth_ok", token: "...", player_id: 1 }

3. 客户端 → { type: "change_name", name: "亮神" }
   客户端 → { type: "create_room" }
   服务端 → { type: "room_created", room_id: 1, player_id: 1 }

   玩家 2 连接、认证、改名、加入房间：
   服务端 → 广播 { type: "player_joined", player_id: 2, player_name: "智宝", ... }

   玩家 3、4 加入...

4. 所有玩家发 { type: "ready" }
   最后一人 ready 后：
   服务端 → 单独发给每人 { type: "game_start", hand: [...], first_turn: 0 }

5. 轮到玩家 0：
   服务端 → { type: "your_turn", player_id: 0, is_free: true, last_play: [] }

6. 玩家 0 出牌：
   客户端 → { type: "play", cards: [...] }
   服务端 → 广播 { type: "play_result", player_id: 0, cards: [...], next_turn: 1 }

7. 玩家 1 pass：
   客户端 → { type: "pass" }
   服务端 → 广播 { type: "player_pass", player_id: 1, pass_count: 1, next_turn: 2 }

8. 玩家 2、3 pass → pass_count == 3
   服务端 → { type: "your_turn", player_id: 0, is_free: true }

9. ...继续到有人手牌为空...

10. 服务端 → 广播 { type: "game_over", winner_id: 0, ranking: [0, 2, 1, 3] }
```
