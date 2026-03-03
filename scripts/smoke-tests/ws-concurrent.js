// Concurrent WebSocket connection smoke test for QuantClaw
// Launches N parallel connections, each performs connect.hello handshake.
// Usage: node ws-concurrent.js <url> <token> [count=10]
// Exit codes: 0 = all succeeded, 1 = any failed

const WebSocket = require('ws');

const url = process.argv[2];
const token = process.argv[3];
const N = parseInt(process.argv[4] || '10', 10);
const TIMEOUT_MS = 8000;

if (!url || !token) {
  console.error('Usage: node ws-concurrent.js <url> <token> [count]');
  process.exit(1);
}

let ok = 0;
let fail = 0;
let pending = N;
const errors = [];

for (let i = 0; i < N; i++) {
  setTimeout(() => {
    const ws = new WebSocket(url);
    const timer = setTimeout(() => {
      fail++;
      finish(`timeout-${i}`);
      try { ws.terminate(); } catch (e) { /* ignore */ }
    }, TIMEOUT_MS);

    ws.on('open', () => {
      ws.send(JSON.stringify({
        type: 'req',
        id: '1',
        method: 'connect.hello',
        params: {
          clientName: `concurrent-${i}`,
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
      if (msg.type === 'event') return;
      if (msg.type === 'res' && msg.id === '1') {
        clearTimeout(timer);
        if (msg.error) {
          fail++;
          finish(`auth-err-${i}`);
        } else {
          ok++;
          finish();
        }
        ws.close();
      }
    });

    ws.on('error', (e) => {
      clearTimeout(timer);
      fail++;
      finish(`err-${i}: ${e.message}`);
    });
  }, i * 50); // 50ms stagger to avoid thundering herd
}

function finish(err) {
  if (err) errors.push(err);
  pending--;
  if (pending === 0) {
    const result = { ok, fail, total: N };
    if (errors.length > 0) result.errors = errors.slice(0, 5);
    console.log(JSON.stringify(result));
    process.exit(fail > 0 ? 1 : 0);
  }
}
