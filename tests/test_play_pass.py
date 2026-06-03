#!/usr/bin/env python3
"""
并发模拟 4 个客户端：创建房间、加入、ready、等待 game_start、在 your_turn 收到后发送 pass。
需要安装：pip install websockets
"""
import asyncio
import json
import websockets

URL = 'ws://localhost:9002'

async def client_task(idx, room_id_holder):
    async with websockets.connect(URL) as ws:
        # auth
        await ws.send(json.dumps({"type":"auth"}))
        token_msg = json.loads(await ws.recv())
        print(f"[{idx}] auth_ok: {token_msg}")

        if idx == 0:
            # create room
            await ws.send(json.dumps({"type":"create_room"}))
            msg = json.loads(await ws.recv())
            print(f"[{idx}] room_created: {msg}")
            room_id_holder[0] = msg.get('room_id')
        else:
            # wait until room id is available
            while room_id_holder[0] is None:
                await asyncio.sleep(0.1)
            await ws.send(json.dumps({"type":"join_room","room_id": room_id_holder[0]}))
            # read two msgs: room_joined and broadcast player_joined
            msg = json.loads(await ws.recv())
            print(f"[{idx}] room_joined: {msg}")

        # send ready
        await ws.send(json.dumps({"type":"ready"}))
        # consume broadcasts until game_start
        while True:
            msg = json.loads(await ws.recv())
            print(f"[{idx}] recv: {msg}")
            typ = msg.get('type')
            if typ == 'player_ready':
                continue
            if typ == 'game_start':
                # store hand if needed
                hand = msg.get('hand', [])
                print(f"[{idx}] game_start hand size: {len(hand)}")
                continue
            if typ == 'your_turn':
                # Immediately send pass to test pass handling
                await ws.send(json.dumps({"type":"pass"}))
                print(f"[{idx}] sent pass")
                # continue listening for game events then exit when game_over
                continue
            if typ == 'game_over':
                print(f"[{idx}] game_over: {msg}")
                break

async def main():
    room_id_holder = [None]
    tasks = [client_task(i, room_id_holder) for i in range(4)]
    await asyncio.gather(*tasks)

if __name__ == '__main__':
    asyncio.run(main())
