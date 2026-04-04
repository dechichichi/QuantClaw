# Configuration Guide

Configure QuantClaw for your specific use case.

## Configuration File

QuantClaw stores its configuration at `~/.quantclaw/quantclaw.json` (JSON5 format — comments and trailing commas are supported).

A full annotated example is available in `config.example.json` in the repository root.

## Configuration Structure

```json
{
  "system": {
    "logLevel": "info"
  },
  "llm": {
    "model": "openai/qwen-max",
    "maxIterations": 15,
    "temperature": 0.7,
    "maxTokens": 4096
  },
  "providers": {
    "openai": {
      "apiKey": "YOUR_OPENAI_API_KEY",
      "baseUrl": "https://api.openai.com/v1",
      "timeout": 30
    },
    "anthropic": {
      "apiKey": "YOUR_ANTHROPIC_API_KEY",
      "baseUrl": "https://api.anthropic.com",
      "timeout": 30
    }
  },
  "gateway": {
    "port": 18800,
    "bind": "loopback",
    "auth": { "mode": "token", "token": "YOUR_SECRET_TOKEN" },
    "controlUi": { "enabled": true, "port": 18801 }
  },
  "channels": {
    "discord": { "enabled": false, "token": "YOUR_DISCORD_BOT_TOKEN", "allowedIds": [] },
    "telegram": { "enabled": false, "token": "YOUR_TELEGRAM_BOT_TOKEN", "allowedIds": [] }
  },
  "tools": {
    "allow": ["group:fs", "group:runtime"],
    "deny": []
  },
  "security": {
    "sandbox": {
      "enabled": true,
      "allowedPaths": ["~/.quantclaw/agents/main/workspace"],
      "deniedPaths": ["/etc", "/sys", "/proc"]
    }
  },
  "mcp": {
    "servers": []
  }
}
```

## LLM Configuration (`llm`)

| Key | Default | Description |
|-----|---------|-------------|
| `model` | `openai/qwen-max` | Default model in `provider/model-name` format |
| `maxIterations` | `15` | Maximum agent loop iterations per request |
| `temperature` | `0.7` | Sampling temperature (0.0–1.0) |
| `maxTokens` | `4096` | Maximum tokens for each LLM response |

The `model` field uses `provider/model-name` prefix routing. If no prefix is given, it defaults to `openai`. Any OpenAI-compatible API can be used by setting the appropriate `baseUrl` in `providers`:

```json
{
  "llm": {
    "model": "openai/qwen-max"
  },
  "providers": {
    "openai": {
      "apiKey": "YOUR_KEY",
      "baseUrl": "https://dashscope.aliyuncs.com/compatible-mode/v1"
    }
  }
}
```

## Provider Configuration (`providers`)

Each key under `providers` defines a named provider:

```json
{
  "providers": {
    "openai": {
      "apiKey": "sk-...",
      "baseUrl": "https://api.openai.com/v1",
      "timeout": 30
    },
    "anthropic": {
      "apiKey": "sk-ant-...",
      "baseUrl": "https://api.anthropic.com",
      "timeout": 30
    }
  }
}
```

**Options:**
- `apiKey`: API authentication key
- `baseUrl`: API base URL (change to use compatible endpoints like DeepSeek, local Ollama, etc.)
- `timeout`: Request timeout in seconds (default: `30`)

### OpenAI Codex OAuth

If you want a browser login flow instead of `OPENAI_API_KEY`, use the dedicated `openai-codex` provider:

```bash
quantclaw models auth login --provider openai-codex
quantclaw models auth status --provider openai-codex
quantclaw models auth logout --provider openai-codex
```

Credentials are stored in `~/.quantclaw/auth/openai-codex.json` and refreshed automatically when possible. `status` reports whether cached credentials exist and whether the access token is still valid or refreshable. `logout` clears only the local cached credentials, it does not switch your configured model away from `openai-codex/...`. Auth-store updates use atomic replacement, so a failed write does not wipe an existing cached login. The OAuth-backed provider is configured separately from the standard `openai` provider:

```json
{
  "llm": {
    "model": "openai-codex/gpt-5"
  },
  "providers": {
    "openai-codex": {
      "baseUrl": "https://chatgpt.com/backend-api",
      "timeout": 30
    }
  }
}
```

Use `openai` when you want direct API-key access, and `openai-codex` when you want ChatGPT/Codex OAuth.

### GitHub Copilot

If you want to use GitHub Copilot-backed models, authenticate the dedicated `github-copilot` provider through GitHub device login:

```bash
quantclaw models auth login --provider github-copilot
quantclaw models auth status --provider github-copilot
quantclaw models auth logout --provider github-copilot

# Convenience alias
quantclaw models auth login-github-copilot
```

Credentials are stored in `~/.quantclaw/auth/github-copilot.json`, and short-lived Copilot runtime tokens are cached in `~/.quantclaw/auth/github-copilot.token-cache.json`. `status` reports whether cached credentials exist and whether the access token is still valid or refreshable. `logout` clears only the local cached credentials, it does not switch your configured model away from `github-copilot/...`. Auth-store updates use atomic replacement, so a failed write does not wipe an existing cached login. Runtime token resolution prefers `COPILOT_GITHUB_TOKEN`, then `GH_TOKEN`, then `GITHUB_TOKEN`, and only falls back to the local auth store if no environment token is set.

```json
{
  "llm": {
    "model": "github-copilot/gpt-4o"
  }
}
```

Use the `github-copilot/...` namespace when you want account-backed GitHub Copilot access.

## Gateway Configuration (`gateway`)

```json
{
  "gateway": {
    "port": 18800,
    "bind": "loopback",
    "auth": {
      "mode": "token",
      "token": "YOUR_SECRET_TOKEN"
    },
    "controlUi": {
      "enabled": true,
      "port": 18801
    }
  }
}
```

| Key | Default | Description |
|-----|---------|-------------|
| `port` | `18800` | WebSocket RPC gateway port |
| `bind` | `loopback` | Bind address: `loopback` (127.0.0.1) or `any` (0.0.0.0) |
| `auth.mode` | `token` | Auth mode: `token` or `none` |
| `auth.token` | — | Secret token for client authentication |
| `controlUi.enabled` | `true` | Enable the web dashboard |
| `controlUi.port` | `18801` | HTTP port for dashboard and REST API |

**Note:** QuantClaw uses ports `18800-18801` (different from OpenClaw's `18789-18790`), so both can run simultaneously.

### Authentication Modes

#### Token Mode (Recommended)

When `auth.mode` is set to `"token"`, all clients (including the web dashboard) must provide the correct token to access the gateway:

```json
{
  "gateway": {
    "auth": {
      "mode": "token",
      "token": "my-secure-password-here"
    }
  }
}
```

**Dashboard access:**
1. Open `http://127.0.0.1:18801` in your browser
2. Enter the token when prompted
3. The token is stored in your browser's localStorage for future visits

**API access:**
```bash
curl -H "Authorization: Bearer YOUR_TOKEN" http://localhost:18801/api/status
```

**WebSocket access:**
Clients must send a `connect.hello` RPC message with `authToken` after connecting.

#### No Authentication

To disable authentication (not recommended for production):

```json
{
  "gateway": {
    "auth": {
      "mode": "none"
    }
  }
}
```

### Changing Your Token

1. Edit `~/.quantclaw/quantclaw.json` and update `gateway.auth.token`
2. Apply the change: `quantclaw config reload` (or restart the gateway)
3. For dashboard users: clear localStorage for `127.0.0.1:18801` and enter the new token

## Channel Configuration (`channels`)

```json
{
  "channels": {
    "discord": {
      "enabled": true,
      "token": "YOUR_DISCORD_BOT_TOKEN",
      "allowedIds": ["123456789"]
    },
    "telegram": {
      "enabled": false,
      "token": "YOUR_TELEGRAM_BOT_TOKEN",
      "allowedIds": []
    }
  }
}
```

| Key | Description |
|-----|-------------|
| `enabled` | Enable/disable this channel adapter |
| `token` | Bot token from Discord/Telegram |
| `allowedIds` | Allowlist of user/group IDs (empty = allow all) |

## Tool Configuration (`tools`)

```json
{
  "tools": {
    "allow": ["group:fs", "group:runtime"],
    "deny": ["bash"]
  }
}
```

**Built-in tool groups:**
- `group:fs` — file read/write/edit/apply_patch
- `group:runtime` — bash, process, web_search, web_fetch, browser
- `group:memory` — memory_search, memory_get

## Security Configuration (`security`)

```json
{
  "security": {
    "sandbox": {
      "enabled": true,
      "allowedPaths": ["~/.quantclaw/agents/main/workspace"],
      "deniedPaths": ["/etc", "/sys", "/proc"]
    }
  }
}
```

| Key | Default | Description |
|-----|---------|-------------|
| `sandbox.enabled` | `true` | Enable filesystem sandbox |
| `sandbox.allowedPaths` | `["~/.quantclaw/agents/main/workspace"]` | Paths the agent may read/write |
| `sandbox.deniedPaths` | `["/etc", "/sys", "/proc"]` | Paths always blocked |

## MCP Configuration (`mcp`)

```json
{
  "mcp": {
    "servers": [
      {
        "name": "my-server",
        "command": "npx",
        "args": ["-y", "@modelcontextprotocol/server-filesystem", "/tmp"]
      }
    ]
  }
}
```

## System / Logging (`system`)

```json
{
  "system": {
    "logLevel": "info"
  }
}
```

**Log levels:** `trace`, `debug`, `info`, `warn`, `error`

Log files are stored at `~/.quantclaw/logs/`. The main log (`quantclaw.log`) is size-rotated automatically; the gateway service log (`gateway.log`) is time-pruned at startup.

## Environment Variable Substitution

Configuration supports `${VAR}` substitution from the shell environment:

```json
{
  "providers": {
    "openai": {
      "apiKey": "${OPENAI_API_KEY}"
    },
    "anthropic": {
      "apiKey": "${ANTHROPIC_API_KEY}"
    }
  }
}
```

## Configuration Commands

```bash
# View full config
quantclaw config get

# Get a specific value (dot-path)
quantclaw config get llm.model

# Change a value
quantclaw config set llm.model "anthropic/claude-sonnet-4-6"

# Remove a key
quantclaw config unset llm.temperature

# Validate syntax and structure
quantclaw config validate

# Show configuration schema
quantclaw config schema

# Hot-reload config (no gateway restart needed)
quantclaw config reload
```

## Common Setups

### Minimal (OpenAI-compatible)

```json
{
  "llm": { "model": "openai/gpt-4o" },
  "providers": {
    "openai": { "apiKey": "sk-..." }
  }
}
```

### Anthropic Claude

```json
{
  "llm": { "model": "anthropic/claude-sonnet-4-6" },
  "providers": {
    "anthropic": { "apiKey": "sk-ant-..." }
  }
}
```

### Local Ollama

```json
{
  "llm": { "model": "openai/llama3" },
  "providers": {
    "openai": {
      "apiKey": "ollama",
      "baseUrl": "http://localhost:11434/v1"
    }
  }
}
```

### DeepSeek / Qwen / Custom Endpoint

```json
{
  "llm": { "model": "openai/deepseek-chat" },
  "providers": {
    "openai": {
      "apiKey": "YOUR_DEEPSEEK_KEY",
      "baseUrl": "https://api.deepseek.com/v1"
    }
  }
}
```

---

**Next**: [View CLI reference](/guide/cli-reference) or [get started](/guide/getting-started).
