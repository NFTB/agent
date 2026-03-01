# agent

跨语言、基于本地进程间通信 (IPC) 的极简终端安全引擎原型。

## 架构

- **C++ 数据采集端 (Sensor/Agent)**：通过 macOS libproc 静默采集当前所有进程的 PID 与名称，序列化为 JSON，经 Unix Domain Socket 以 NDJSON 心跳形式发送。
- **Go 数据接收端 (Server/Engine)**：本地 UDS 服务，接收心跳、反序列化，并做简单策略引擎（日志 + 可疑进程名规则匹配告警）。

先启动 Engine 再启动 Agent；Agent 连接失败时会持续重连。

## 构建

### C++ Agent

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

产物：`build/main`。依赖：macOS，libproc，CMake 3.16+，C++17。首次构建会通过 FetchContent 下载 nlohmann/json。

### Go Engine

```bash
cd engine && go build -o engine
```

产物：`engine/engine`。

## 运行

1. 启动引擎（先启动）：
   ```bash
   ./engine/engine
   ```
2. 启动采集端：
   ```bash
   ./build/main
   ```

## 配置（环境变量）

| 变量 | 说明 | 默认 |
|------|------|------|
| `AGENT_SOCK` | Unix socket 路径（两端需一致） | `/tmp/agent-engine.sock` |
| `HEARTBEAT_INTERVAL_SEC` | Agent 心跳间隔（秒） | `3` |

示例：`AGENT_SOCK=/tmp/my.sock HEARTBEAT_INTERVAL_SEC=5 ./build/main`

## 权限说明

- 采集其他用户进程时，Agent 需 root 运行（如 `sudo ./build/main`）。
- Engine 仅监听 UDS，无需特殊权限；socket 权限由创建时的 umask 决定。
