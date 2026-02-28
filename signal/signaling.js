const WebSocket = require('ws');
const wss = new WebSocket.Server({ port: 8080 });

let gameServer = null;
const clientMap = new Map(); // clientId -> ws
let nextId = 1;

wss.on('connection', (ws) => {
    ws.on('message', (raw) => {
        const text = raw.toString();
        let msg;
        try { msg = JSON.parse(text); } catch { return; }

        if (msg.type === 'register-server') {
            gameServer = ws;
            console.log('game server registered');
            return;
        }

        if (ws === gameServer) {
            // server -> specific client
            const client = clientMap.get(msg.clientId);
            if (client && client.readyState === WebSocket.OPEN)
                client.send(text);
        } else {
            // client -> server
            if (!ws.clientId) {
                ws.clientId = nextId++;
                clientMap.set(ws.clientId, ws);
                console.log(`client ${ws.clientId} connected`);
            }
            if (gameServer && gameServer.readyState === WebSocket.OPEN) {
                msg.clientId = ws.clientId;
                gameServer.send(JSON.stringify(msg));
            }
        }
    });

    ws.on('close', () => {
        if (ws === gameServer) {
            gameServer = null;
            console.log('game server disconnected');
        } else if (ws.clientId) {
            clientMap.delete(ws.clientId);
            console.log(`client ${ws.clientId} disconnected`);
        }
    });
});

console.log('signaling server running on ws://localhost:8080');
