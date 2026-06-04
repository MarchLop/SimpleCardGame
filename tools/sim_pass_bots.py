#!/usr/bin/env python3
"""
三个机器人：创建房间并加入，发送 ready；在收到 your_turn 时立即发送 pass。
用法：在前端作为第 4 个玩家加入同一房间进行测试。
需要：pip install websockets
"""
import asyncio
import json
import websockets

URL = 'ws://localhost:9002'

async def bot_task(idx, room_id_holder):
    async with websockets.connect(URL) as ws:
        await ws.send(json.dumps({"type":"auth"}))
        resp = json.loads(await ws.recv())
        print(f"[bot{idx}] auth: {resp.get('token')}")

        if idx == 0:
            # 创建房间
            await ws.send(json.dumps({"type":"create_room"}))
            msg = json.loads(await ws.recv())
            print(f"[bot{idx}] room_created: {msg}")
            room_id_holder[0] = msg.get('room_id')
        else:
            # 等待 room_id
            while room_id_holder[0] is None:
                await asyncio.sleep(0.1)

        # join room
        await ws.send(json.dumps({"type":"join_room","room_id": room_id_holder[0]}))
        # read the immediate room_joined reply
        while True:
            raw = await ws.recv()
            msg = json.loads(raw)
            t = msg.get('type')
            print(f"[bot{idx}] recv: {msg}")
            if t == 'room_joined':
                break
            # ignore other broadcasts until join confirmed

        # send ready
        await ws.send(json.dumps({"type":"ready"}))
        print(f"[bot{idx}] ready sent")

        # 主循环：响应 game_start / your_turn / game_over
        while True:
            try:
                raw = await ws.recv()
            except websockets.exceptions.ConnectionClosed:
                print(f"[bot{idx}] connection closed")
                return
            msg = json.loads(raw)
            t = msg.get('type')
            print(f"[bot{idx}] event: {t}")

            if t == 'game_start':
                print(f"[bot{idx}] game_start hand size: {len(msg.get('hand', []))}")
                continue
            if t == 'your_turn':
                # 只发送 pass
                await ws.send(json.dumps({"type":"pass"}))
                print(f"[bot{idx}] sent pass")
                continue
            if t == 'game_over':
                print(f"[bot{idx}] game_over, exiting")
                return

async def main():
    room_id_holder = [None]
    tasks = [asyncio.create_task(bot_task(i, room_id_holder)) for i in range(3)]
    await asyncio.gather(*tasks)

if __name__ == '__main__':
    asyncio.run(main())
