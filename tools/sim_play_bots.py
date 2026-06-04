#!/usr/bin/env python3
"""
三个机器人：创建房间并加入，发送 ready；在 your_turn 收到时尽量出牌（单张牌），
如果不可出则发送 pass。可以和本地浏览器一起作为第 4 个玩家加入。
用法:
    python3 tools/sim_play_bots.py
    python3 tools/sim_play_bots.py --room 2
需要：pip install websockets
"""
import argparse
import asyncio
import json
import random
import websockets

URL = 'ws://localhost:9002'

async def recv_json(ws):
    raw = await ws.recv()
    msg = json.loads(raw)
    print(f"[recv] {msg}")
    return msg

async def bot_task(idx, room_id_holder, room_id_arg):
    async with websockets.connect(URL) as ws:
        await ws.send(json.dumps({"type": "auth"}))
        auth_msg = await recv_json(ws)
        print(f"[bot{idx}] auth_ok token={auth_msg.get('token')}")

        my_room_id = room_id_arg
        if idx == 0 and my_room_id is None:
            await ws.send(json.dumps({"type": "create_room"}))
            msg = await recv_json(ws)
            if msg.get('type') != 'room_created':
                raise RuntimeError(f"bot{idx} create_room failed: {msg}")
            my_room_id = msg.get('room_id')
            room_id_holder[0] = my_room_id
            print(f"[bot{idx}] created room {my_room_id}")
        else:
            if my_room_id is None:
                while room_id_holder[0] is None:
                    await asyncio.sleep(0.1)
                my_room_id = room_id_holder[0]

            await ws.send(json.dumps({"type": "join_room", "room_id": my_room_id}))
            while True:
                msg = await recv_json(ws)
                if msg.get('type') == 'room_joined':
                    print(f"[bot{idx}] joined room {my_room_id}")
                    break

        await ws.send(json.dumps({"type": "ready"}))
        print(f"[bot{idx}] ready sent")

        my_player_id = None
        hand = []
        played_cards = []
        while True:
            try:
                msg = await recv_json(ws)
            except websockets.exceptions.ConnectionClosed:
                print(f"[bot{idx}] connection closed")
                return

            typ = msg.get('type')
            if typ == 'player_joined' and msg.get('player_name') is not None:
                if msg.get('player_id') and msg.get('room_id') == my_room_id:
                    if my_player_id is None and idx == 0 and msg.get('player_name') == auth_msg.get('token'):
                        my_player_id = msg.get('player_id')
                continue
            if typ == 'room_joined':
                my_player_id = msg.get('player_count')
                continue
            if typ == 'game_start':
                hand = msg.get('hand', [])[:]
                if my_player_id is None:
                    my_player_id = msg.get('first_turn')
                print(f"[bot{idx}] game_start hand {len(hand)} cards, my_player_id={my_player_id}")
                continue
            if typ == 'your_turn' and msg.get('room_id') == my_room_id:
                if msg.get('player_id') != my_player_id:
                    continue
                if hand:
                    card = hand[0]
                    await ws.send(json.dumps({"type": "play", "cards": [card]}))
                    print(f"[bot{idx}] try play card {card}")
                else:
                    await ws.send(json.dumps({"type": "pass"}))
                    print(f"[bot{idx}] pass (no cards)")
                continue
            if typ == 'play_result' and msg.get('player_id') == my_player_id:
                played = msg.get('cards', [])
                for card in played:
                    for i, h in enumerate(hand):
                        if h.get('num') == card.get('num') and h.get('suit') == card.get('suit'):
                            hand.pop(i)
                            break
                print(f"[bot{idx}] played result, remaining {len(hand)} cards")
                continue
            if typ == 'player_pass':
                continue
            if typ == 'game_over':
                print(f"[bot{idx}] game_over")
                return
            if typ == 'error':
                code = msg.get('code')
                if code == 1004 and hand:
                    print(f"[bot{idx}] invalid_play, fallback pass")
                    await ws.send(json.dumps({"type": "pass"}))
                continue

async def main():
    parser = argparse.ArgumentParser(description='Run 3 bots for SimpleCardGame')
    parser.add_argument('--room', type=int, help='Join existing room_id instead of creating a new one')
    args = parser.parse_args()
    room_id_holder = [args.room]
    tasks = [asyncio.create_task(bot_task(i, room_id_holder, args.room)) for i in range(3)]
    await asyncio.gather(*tasks)

if __name__ == '__main__':
    asyncio.run(main())
