# SimpleCardGame

一个基于 WebSocket 的多人扑克牌游戏服务端，C++ 实现。

## 玩法

- 4人局，每人13张牌
- 红桃2和黑桃K持有者为一队，另外两人为一队
- 先出完牌的队伍获胜
- 支持牌型：单张、对子、三条、顺子、炸弹、三带二、四带二

## 快速开始

```bash
# 编译
g++ -std=c++17 server.cpp -o server -lwebsocketpp -lpthread

# 运行
./server
```

服务端默认监听 `ws://localhost:9002`。

## 消息协议

### 客户端 → 服务端

| type | 说明 |
|------|------|
| `create_room` | 创建房间 |
| `join_room` | 加入房间 |
| `leave_room` | 离开房间 |
| `ready` | 准备 |
| `play` | 出牌 |
| `pass` | 不出 |
| `get_rooms` | 获取房间列表 |
| `chat` | 聊天 |

### 服务端 → 客户端

| type | 说明 |
|------|------|
| `room_created` | 房间创建成功 |
| `player_joined` | 有玩家加入 |
| `player_left` | 有玩家离开 |
| `game_start` | 游戏开始 |
| `your_turn` | 轮到你了 |
| `play_result` | 出牌结果 |
| `game_over` | 游戏结束 |
| `error` | 错误提示 |
| `chat` | 聊天消息 |

## 项目结构

```
├── game.h       # 核心游戏逻辑（牌、牌型判断、出牌校验）
├── server.cpp   # WebSocket 服务端
└── output/      # 编译输出
```

## 依赖

- C++17
- [websocketpp](https://github.com/zaphoyd/websocketpp)
- [nlohmann/json](https://github.com/nlohmann/json)
