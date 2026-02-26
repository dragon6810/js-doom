// This code is executed before the WASM module is loaded.
// It sets the canvas element that the SDL2 backend will use for rendering.

const RENDER_WIDTH = 320;
const RENDER_HEIGHT = 200;

Module['canvas'] = (function() { 
    const canvas = document.getElementById('canvas');
    // Set the physical size of the canvas to fill the window
    canvas.width = RENDER_WIDTH;
    canvas.height = RENDER_HEIGHT;
    return canvas; 
})();

Module.onRuntimeInitialized = function() {
    // Set the logical, internal rendering resolution.
    // SDL will automatically scale this up to the canvas size.
    Module._screen_init(RENDER_WIDTH, RENDER_HEIGHT);
};

let peerConnection;
let dataChannel;
let signalingSocket;

// Call this from the console: connectToGame('localhost', 8080)
async function connectToGame(ip, port) {
    signalingSocket = new WebSocket(`ws://${ip}:${port}`);

    signalingSocket.onopen = async () => {
        console.log("Connected to signaling server. Initiating WebRTC...");
        
        // 1. Create the WebRTC connection
        peerConnection = new RTCPeerConnection({
            iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
        });

        // 2. Handle ICE Candidates (finding network routes)
        peerConnection.onicecandidate = (event) => {
            if (event.candidate) {
                signalingSocket.send(JSON.stringify({ type: 'candidate', candidate: event.candidate }));
            }
        };

        // 3. Create the Unreliable Data Channel for Doom
        dataChannel = peerConnection.createDataChannel("doom-channel", {
            ordered: false,
            maxRetransmits: 0
        });

        dataChannel.onopen = () => {
            console.log("WebRTC channel open");
            Module._netDataChannel = dataChannel;
        };
        dataChannel.onmessage = (event) => console.log("sever says:", event.data);

        // 4. Create and send the Offer
        const offer = await peerConnection.createOffer();
        await peerConnection.setLocalDescription(offer);
        signalingSocket.send(JSON.stringify({ type: 'offer', sdp: peerConnection.localDescription }));
    };

    // 5. Handle incoming signaling messages from the C++ server
    signalingSocket.onmessage = async (event) => {
        const msg = JSON.parse(event.data);
        if (msg.type === 'answer') {
            await peerConnection.setRemoteDescription(new RTCSessionDescription(msg.sdp));
        } else if (msg.type === 'candidate') {
            await peerConnection.addIceCandidate(new RTCIceCandidate(msg.candidate));
        }
    };
}