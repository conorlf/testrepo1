// ★ NEW — connect to Node.js backend
const socket = io("http://localhost:3001");

// Your existing DOM references
const sendBtn = document.getElementById("sendBtn");
const textInput = document.getElementById("textInput");
const messages = document.getElementById("messages");
const roomItems = document.querySelectorAll(".room.item");
const peerContainer = document.getElementById("peerContainer");
const chatTitle = document.getElementById("chatTitle");

// Your existing roomData stays for UI fallback
const roomData = {
  "global-chat": {
    title: "Global Chat",
    messages: ["Welcome to global chat.", "Feel free to send a message."],
    peers: [
      { name: "peer-1", ip: "192.168.1.45", last: "2m ago" },
      { name: "peer-2", ip: "192.168.1.81", last: "10m ago" },
    ],
  },
  // ... your other rooms unchanged ...
};

let currentRoom = "global-chat";

/* ─────────────────────────────────────────────── */
/* ★ NEW — Load real chatroom messages from backend */
/* ─────────────────────────────────────────────── */

async function loadChatroomFromBackend() {
  try {
    const res = await fetch("/api/chatroom");
    const data = await res.json();

    messages.innerHTML = "";

    data.messages.forEach(msg => {
      const div = document.createElement("div");
      div.className = msg.from_me ? "message from-me" : "message from-them";
      div.textContent = msg.text;
      messages.appendChild(div);
    });

    messages.scrollTop = messages.scrollHeight;
  } catch (err) {
    console.warn("Backend not available, using local mock data.");
  }
}

/* ─────────────────────────────────────────────── */
/* Your existing room switching logic stays         */
/* ─────────────────────────────────────────────── */

function setSelectedRoom(roomId) {
  currentRoom = roomId;
  roomItems.forEach((item) => {
    item.classList.toggle("selected", item.dataset.room === roomId);
  });

  const room = roomData[roomId] || { title: roomId, messages: [], peers: [] };
  chatTitle.textContent = room.title;

  messages.innerHTML = "";
  room.messages.forEach((text, i) => {
    const div = document.createElement("div");
    div.className = i % 2 === 0 ? "message from-them" : "message from-me";
    div.textContent = text;
    messages.appendChild(div);
  });

  messages.scrollTop = messages.scrollHeight;

  peerContainer.innerHTML = "";
  room.peers.forEach((peer) => {
    const card = document.createElement("div");
    card.className = "peer-card";
    card.innerHTML = `
      <div class="peer-name">${peer.name}</div>
      <div class="peer-ip">IP: ${peer.ip}</div>
      <div class="peer-last">Last online: ${peer.last}</div>`;
    peerContainer.appendChild(card);
  });

  // ★ NEW — load real messages when entering global chat
  if (roomId === "global-chat") {
    loadChatroomFromBackend();
  }
}

roomItems.forEach((room) => {
  room.addEventListener("click", () => {
    const roomId = room.dataset.room;
    if (roomId) setSelectedRoom(roomId);
  });
});

/* ─────────────────────────────────────────────── */
/* Your existing bubble creation stays              */
/* ─────────────────────────────────────────────── */

function appendBubble(text, fromMe = true) {
  if (!text.trim()) return;
  const msg = document.createElement("div");
  msg.className = `message ${fromMe ? "from-me" : "from-them"}`;
  msg.textContent = text;
  messages.appendChild(msg);
  messages.scrollTop = messages.scrollHeight;
}

/* ─────────────────────────────────────────────── */
/* ★ NEW — Send message to backend                 */
/* ─────────────────────────────────────────────── */

async function sendToBackend(text) {
  await fetch("/api/send/chatroom", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ message: text })
  });
}

/* ─────────────────────────────────────────────── */
/* Modified send button logic                      */
/* ─────────────────────────────────────────────── */

sendBtn.addEventListener("click", () => {
  const text = textInput.value;
  if (!text.trim()) return;

  appendBubble(text, true); // local UI
  sendToBackend(text);      // ★ NEW — send to Node.js

  textInput.value = "";
});

textInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter") {
    e.preventDefault();
    sendBtn.click();
  }
});

/* ─────────────────────────────────────────────── */
/* ★ NEW — Receive messages from backend           */
/* ─────────────────────────────────────────────── */

socket.on("chatroom_message", data => {
  appendBubble(data.encrypted_preview, false);
});

/* ─────────────────────────────────────────────── */

setSelectedRoom(currentRoom);