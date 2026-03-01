# QuantClaw 使用指南

## 快速开始

### 1. 编译和安装

#### 从源码编译

**Linux:**
```bash
git clone https://github.com/QuantClaw/QuantClaw.git
cd QuantClaw
./scripts/build-linux.sh gcc Release
```

**Windows:**
```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
.\scripts\build-windows.ps1 -BuildType Release
```

#### Docker 安装
```bash
docker-compose up -d
```

### 2. 运行 Onboarding 向导

最简单的方式是使用交互式 Onboarding 向导：

```bash
# 完整的交互式向导（推荐）
quantclaw onboard

# 自动安装守护进程
quantclaw onboard --install-daemon

# 快速设置（无交互提示）
quantclaw onboard --quick
```

Onboarding 向导会：
- 引导你配置网关端口、AI 模型等
- 创建工作空间目录和 SOUL.md 文件
- 可选地安装为系统服务
- 验证所有组件是否正确配置

### 3. 启动网关

```bash
# 如果已安装为服务
quantclaw gateway start

# 或前台运行（调试用）
quantclaw gateway
```

### 4. 打开仪表板

```bash
quantclaw dashboard
```

这会在 `http://127.0.0.1:18790` 打开 Web UI

## CLI 命令

### Onboarding 和初始设置

```bash
# 运行交互式 Onboarding 向导
quantclaw onboard

# 仅安装守护进程
quantclaw onboard --install-daemon

# 快速设置（无提示）
quantclaw onboard --quick
```

### Gateway 管理

```bash
# 查看状态
quantclaw status

# 启动服务
quantclaw gateway start

# 停止服务
quantclaw gateway stop

# 重启服务
quantclaw gateway restart

# 前台运行（调试用）
quantclaw gateway
```

### Agent 功能

```bash
# 发送消息
quantclaw agent -m "Hello, introduce yourself"

# 指定会话
quantclaw agent --session my:session -m "What's the weather?"

# 停止当前请求
quantclaw agent stop
```

### 会话管理

```bash
# 列出所有会话
quantclaw sessions list

# 查看会话历史
quantclaw sessions history <session-key>

# 删除会话
quantclaw sessions delete <session-key>

# 重置会话
quantclaw sessions reset <session-key>
```

### 技能管理

```bash
# 列出已加载的技能
quantclaw skills list
```

### 配置管理

```bash
# 获取配置值
quantclaw config get agent.model

# 设置配置值
quantclaw config set agent.model "anthropic/claude-sonnet-4-6"

# 重新加载配置
quantclaw config reload
```

### 其他命令

```bash
# 健康检查
quantclaw health

# 诊断检查
quantclaw doctor

# 查看日志
quantclaw logs

# 查看仪表板
quantclaw dashboard
```

## Web API

### REST API

#### 发送消息
```bash
curl -X POST http://localhost:18790/api/agent/request \
  -H "Content-Type: application/json" \
  -d '{
    "message": "Hello!",
    "sessionKey": "my:session"
  }'
```

#### 查看 Gateway 状态
```bash
curl http://localhost:18790/api/status
```

#### 健康检查
```bash
curl http://localhost:18790/api/health
```

#### 列出会话
```bash
curl http://localhost:18790/api/sessions?limit=10
```

#### 查看会话历史
```bash
curl "http://localhost:18790/api/sessions/history?sessionKey=my:session"
```

#### 列出插件
```bash
curl http://localhost:18790/api/plugins
```

### WebSocket RPC API

```javascript
const ws = new WebSocket('ws://localhost:18789');

ws.onopen = () => {
  // 发送认证请求
  ws.send(JSON.stringify({
    type: 'req',
    id: '1',
    method: 'connect.hello',
    params: {
      clientName: 'my-client',
      clientVersion: '1.0.0',
      authToken: 'your-token'
    }
  }));
};

ws.onmessage = (event) => {
  const response = JSON.parse(event.data);
  console.log(response);
};
```

## Discord 集成

### 1. 创建 Discord Bot

1. 访问 https://discord.com/developers/applications
2. 创建新应用
3. 创建 Bot 并复制 Token
4. 邀请 Bot 到你的服务器

### 2. 配置 QuantClaw

在 `config.json` 中添加:

```json
{
  "channels": {
    "discord": {
      "enabled": true,
      "token": "your-bot-token-here",
      "allowed_ids": ["your-user-id"]
    }
  }
}
```

### 3. 重启服务

```bash
systemctl restart quantclaw
```

## 故障排除

### 查看日志
```bash
# systemd 日志
journalctl -u quantclaw -f

# 查看 QuantClaw 日志
quantclaw logs
```

### 常见问题

**Q: Onboarding 向导无法启动**
```bash
# 检查主目录权限
ls -la ~/.quantclaw/

# 重新运行向导
quantclaw onboard
```

**Q: Gateway 无法启动**
```bash
# 检查配置文件
quantclaw config get gateway.port

# 检查端口占用
lsof -i :18789

# 运行诊断
quantclaw doctor
```

**Q: 无法连接到 Gateway**
```bash
# 检查 Gateway 状态
quantclaw status

# 检查健康状态
quantclaw health

# 查看日志
quantclaw logs
```

**Q: API 调用失败**
- 检查 LLM API Key 是否有效
- 检查网络连接
- 运行 `quantclaw doctor` 进行诊断
- 查看日志获取详细错误信息

## 性能调优

### 内存限制
在 systemd 服务文件中调整:
```ini
MemoryLimit=512M
CPUQuota=200%
```

### 并发连接
调整 Web 服务器配置:
```json
{
  "web": {
    "max_connections": 100,
    "max_requests_per_second": 50
  }
}
```

## 安全最佳实践

1. **不要以 root 用户运行**
2. **使用防火墙限制访问**
3. **定期更新依赖**
4. **监控资源使用**
5. **启用日志审计**

## 更多信息

- GitHub: https://github.com/openclaw/quantclaw
- 文档: https://docs.quantclaw.ai
- 社区: https://discord.gg/quantclaw