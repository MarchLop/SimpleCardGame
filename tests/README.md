测试 play/pass 自动化说明

依赖：
- Python 3.8+
- websockets 库

安装：
```bash
pip install websockets
```

运行：在项目根目录执行：

```bash
python3 tests/test_play_pass.py
```

说明：脚本会启动 4 个并发客户端，流程：
1. 第一个客户端创建房间，其余 3 个加入
2. 全员发送 `ready`，等待 `game_start`
3. 收到 `your_turn` 时立即发送 `pass`，验证服务器对 `pass` 的广播与超时行为

注意：请先启动 `server`（`./server`），再运行此测试脚本。