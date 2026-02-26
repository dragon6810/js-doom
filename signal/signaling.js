const WebSocket = require('ws');
const wss = new WebSocket.Server({ port: 8080 });

wss.on('connection', (ws) => 
{
    console.log("new connection");

    ws.on('message', (message) =>
        {
        // Broadcast the message to the OTHER connected peer
        wss.clients.forEach((client) => {
            if (client !== ws && client.readyState === WebSocket.OPEN) {
                client.send(message.toString());
            }
        });
    });
});

console.log("signaling server running on ws://localhost:8080");