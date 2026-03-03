// WebSocket RPC smoke test helper for QuantClaw
// Usage:
//   node ws-rpc.js <url> <token> hello           — test connect.hello handshake
//   node ws-rpc.js <url> <token> auth-reject      — expect auth rejection (exit 0 on reject)
//   node ws-rpc.js <url> <token> rpc <method> [params-json] — call RPC method after hello
//
// Exit codes: 0 = success, 1 = failure
// Timeout: 5 seconds

const WebSocket = require('ws');

const url = process.argv[2];
const token = process.argv[3];
const testName = process.argv[4];
const rpcMethod = process.argv[5] || null;
const rpcParams = process.argv[6] ? JSON.parse(process.argv[6]) : {};
const TIMEOUT_MS = 5000;

if (!url || !testName) {
  console.error('Usage: node ws-rpc.js <url> <token> <hello|auth-reject|rpc> [method] [params-json]');
  process.exit(1);
}

const ws = new WebSocket(url);
let reqId = 1;
let done = false;

const timer = setTimeout(() => {
  if (!done) {
    console.error('TIMEOUT');
    ws.terminate();
    process.exit(1);
  }
}, TIMEOUT_MS);

function finish(code, msg) {
  if (done) return;
  done = true;
  clearTimeout(timer);
  if (msg) console.log(msg);
  ws.close();
  process.exit(code);
}

ws.on('open', () => {
  ws.send(JSON.stringify({
    type: 'req',
    id: String(reqId++),
    method: 'connect.hello',
    params: {
      clientName: 'smoke-test',
      clientVersion: '1.0.0',
      authToken: token,
      minProtocol: 1,
      maxProtocol: 3,
      role: 'operator',
      scopes: ['operator.read', 'operator.write', 'operator.admin']
    }
  }));
});

ws.on('message', (data) => {
  const msg = JSON.parse(data.toString());

  // Ignore challenge and other events
  if (msg.type === 'event') return;

  // Hello response (id=1)
  if (msg.type === 'res' && msg.id === '1') {
    if (msg.error) {
      if (testName === 'auth-reject') {
        finish(0, JSON.stringify(msg.error));
      } else {
        console.error('HELLO_FAILED: ' + JSON.stringify(msg.error));
        finish(1);
      }
      return;
    }

    if (testName === 'hello') {
      finish(0, JSON.stringify(msg.payload || msg.result));
      return;
    }

    if (testName === 'auth-reject') {
      console.error('AUTH_SHOULD_HAVE_REJECTED');
      finish(1);
      return;
    }

    // testName === 'rpc': send the RPC method
    if (testName === 'rpc' && rpcMethod) {
      ws.send(JSON.stringify({
        type: 'req',
        id: String(reqId++),
        method: rpcMethod,
        params: rpcParams
      }));
    } else {
      finish(0, JSON.stringify(msg.payload || msg.result));
    }
    return;
  }

  // RPC method response (id=2)
  if (msg.type === 'res' && msg.id === '2') {
    if (msg.error) {
      console.error('RPC_ERROR: ' + JSON.stringify(msg.error));
      finish(1);
    } else {
      finish(0, JSON.stringify(msg.payload || msg.result || msg));
    }
    return;
  }
});

ws.on('error', (err) => {
  if (!done) {
    if (testName === 'auth-reject') {
      finish(0, 'WS_ERROR (expected): ' + err.message);
    } else {
      console.error('WS_ERROR: ' + err.message);
      finish(1);
    }
  }
});

ws.on('close', (code) => {
  if (!done) {
    if (testName === 'auth-reject') {
      finish(0, 'WS_CLOSED (expected): code=' + code);
    } else {
      console.error('WS_CLOSED: code=' + code);
      finish(1);
    }
  }
});
