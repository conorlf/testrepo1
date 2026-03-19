const socket = io("http://localhost:3001");

const sendBtn = document.getElementById("sendBtn");
const textInput = document.getElementById("textInput");
const messages = document.getElementById("messages");
const peerContainer = document.getElementById("peerContainer");
const chatTitle = document.getElementById("chatTitle");
const roomList = document.querySelector(".peers");
const topPeers = document.getElementById("topPeers");
const topIP = document.getElementById("topIP");
const topID = document.getElementById("topID");

let currentRoom = "global-chat";

/* -------------------------------
   Load chatroom messages
--------------------------------*/
async function loadChatroomFromBackend() {
  try {
    const res = await fetch("/api/chatroom");
    const data = await res.json();

    messages.innerHTML = "";

    data.messages.forEach(msg => {
      const div = document.createElement("div");
      div.className = msg.from_me ? "message from-me" : "message from-them";
      div.textContent = msg.encrypted_preview;
      messages.appendChild(div);
    });

    messages.scrollTop = messages.scrollHeight;
  } catch (err) {
    console.warn("Backend not available.");
  }
}

/* -------------------------------
   Load peers into left + right sidebar
--------------------------------*/
async function loadPeers() {
  try {
    const res = await fetch("/api/peers");
    const peers = await res.json();

    // Left sidebar (rooms)
    const staticRoom = document.querySelector("[data-room='global-chat']");
    roomList.innerHTML = "";
    roomList.appendChild(staticRoom);

    peers.forEach(p => {
      const div = document.createElement("div");
      div.className = "room item";
      div.dataset.room = p.ip;
      div.textContent = p.ip;
      div.addEventListener("click", () => setSelectedRoom(p.ip));
      roomList.appendChild(div);
    });

    // Right sidebar (peer info)
    peerContainer.innerHTML = "";
    peers.forEach(p => {
      const card = document.createElement("div");
      card.className = "peer-card";
      card.innerHTML = `
        <div class="peer-name">${p.ip}</div>
        <div class="peer-ip">IP: ${p.ip}</div>
        <div class="peer-last">Online</div>`;
      peerContainer.appendChild(card);
    });

    // Update top bar
    topPeers.textContent = peers.length;
  } catch (err) {
    console.warn("Could not load peers");
  }
}

/* -------------------------------
   Load top bar stats
--------------------------------*/
async function loadStats() {
  try {
    const res = await fetch("/api/stats");
    const stats = await res.json();

    topIP.textContent = stats.my_ip;
    topID.textContent = stats.my_id;
  } catch (err) {
    console.warn("Stats unavailable");
  }
}

/* -------------------------------
   Room switching logic
--------------------------------*/
function setSelectedRoom(roomId) {
  currentRoom = roomId;

  // Highlight selected room
  document.querySelectorAll(".room.item").forEach(item => {
    item.classList.toggle("selected", item.dataset.room === roomId);
  });

  chatTitle.textContent = roomId === "global-chat" ? "Global Chat" : roomId;

  messages.innerHTML = "";

  if (roomId === "global-chat") {
    loadChatroomFromBackend();
  } else {
    loadDirectMessages(roomId);
  }
}

/* -------------------------------
   Load direct messages for a peer
--------------------------------*/
async function loadDirectMessages(peerIp) {
  try {
    const res = await fetch(`/api/messages/${peerIp}`);
    const data = await res.json();

    messages.innerHTML = "";

    data.messages.forEach(msg => {
      const div = document.createElement("div");
      div.className = msg.from_me ? "message from-me" : "message from-them";
      div.textContent = msg.encrypted_preview;
      messages.appendChild(div);
    });

    messages.scrollTop = messages.scrollHeight;
  } catch (err) {
    console.warn("Could not load direct messages");
  }
}

/* -------------------------------
   Append bubble to UI
--------------------------------*/
function appendBubble(text, fromMe = true) {
  if (!text.trim()) return;
  const msg = document.createElement("div");
  msg.className = `message ${fromMe ? "from-me" : "from-them"}`;
  msg.textContent = text;
  messages.appendChild(msg);
  messages.scrollTop = messages.scrollHeight;
}

/* -------------------------------
   Send chatroom message
--------------------------------*/
async function sendToBackend(text) {
  await fetch("/api/send/chatroom", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ message: text })
  });
}

/* -------------------------------
   Send button
--------------------------------*/
sendBtn.addEventListener("click", () => {
  const text = textInput.value;
  if (!text.trim()) return;

  appendBubble(text, true);
  sendToBackend(text);

  textInput.value = "";
});

/* -------------------------------
   Enter key
--------------------------------*/
textInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter") {
    e.preventDefault();
    sendBtn.click();
  }
});

/* -------------------------------
   Receive chatroom message
--------------------------------*/
socket.on("chatroom_message", data => {
  if (currentRoom === "global-chat") {
    appendBubble(data.encrypted_preview, false);
  }
});

/* -------------------------------
   Initial load
--------------------------------*/
loadPeers();
loadStats();
setInterval(loadPeers, 1500);
setInterval(loadStats, 1500);

setSelectedRoom(currentRoom);