const sendBtn = document.getElementById("sendBtn");
const textInput = document.getElementById("textInput");
const messages = document.getElementById("messages");
const roomItems = document.querySelectorAll(".room.item");
const peerContainer = document.getElementById("peerContainer");
const chatTitle = document.getElementById("chatTitle");

const roomData = {
  "global-chat": {
    title: "Global Chat",
    messages: ["Welcome to global chat.", "Feel free to send a message."],
    peers: [
      { name: "peer-1", ip: "192.168.1.45", last: "2m ago" },
      { name: "peer-2", ip: "192.168.1.81", last: "10m ago" },
    ],
  },
  "peer-1": {
    title: "Chat with peer-1",
    messages: ["peer-1: Hello", "You: Hey there"],
    peers: [
      { name: "peer-1", ip: "192.168.1.45", last: "just now" },
    ],
  },
  "peer-2": {
    title: "Chat with peer-2",
    messages: ["peer-2: Are you available?", "You: Yes."],
    peers: [
      { name: "peer-2", ip: "192.168.1.81", last: "1m ago" },
    ],
  },
  "peer-3": {
    title: "Chat with peer-3",
    messages: ["peer-3: Hi", "You: Hi!"],
    peers: [
      { name: "peer-3", ip: "192.168.1.92", last: "3m ago" },
    ],
  },
  "peer-4": {
    title: "Chat with peer-4",
    messages: ["peer-4: How's it going?", "You: All good."],
    peers: [
      { name: "peer-4", ip: "192.168.1.99", last: "5m ago" },
    ],
  },
};

let currentRoom = "global-chat";

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
}

roomItems.forEach((room) => {
  room.addEventListener("click", () => {
    const roomId = room.dataset.room;
    if (roomId) setSelectedRoom(roomId);
  });
});

function appendBubble(text, fromMe = true) {
  if (!text.trim()) return;
  const msg = document.createElement("div");
  msg.className = `message ${fromMe ? "from-me" : "from-them"}`;
  msg.textContent = text;
  messages.appendChild(msg);
  messages.scrollTop = messages.scrollHeight;
}

sendBtn.addEventListener("click", () => {
  const text = textInput.value;
  if (!text.trim()) return;
  appendBubble(text, true);
  textInput.value = "";
});

textInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter") {
    e.preventDefault();
    sendBtn.click();
  }
});

setSelectedRoom(currentRoom);