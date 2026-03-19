const socket = io();

const topPeers    = document.getElementById("topPeers");
const inboxCount  = document.getElementById("inboxCount");
const outboxCount = document.getElementById("outboxCount");
const inboxPanel  = document.getElementById("inboxPanel");
const outboxPanel = document.getElementById("outboxPanel");
const peerContainer = document.getElementById("peerContainer");

/* messages that have been decrypted — kept in memory so they survive re-renders */
const readHistory = []; /* { author, timestamp, encrypted, plaintext } */

/* track when outbox entries were first seen as sent — for 10s removal */
const outboxSentAt = {}; /* index -> Date.now() */

/* ── live stats from socket ── */
socket.on("stats", stats => {
    inboxCount.textContent = stats.incoming_used || 0;
});

/* ── peers ── */
async function loadPeers() {
    try {
        const res  = await fetch("/api/peers");
        const peers = await res.json();
        topPeers.textContent = peers.filter(p => p.status === "connected").length;
        peerContainer.innerHTML = "";
        peers.forEach(p => {
            const card = document.createElement("div");
            card.className = "peer-card";
            card.innerHTML = `<div class="peer-name">${p.ip}</div>
                              <div class="peer-status ${p.status}">${p.status}</div>`;
            peerContainer.appendChild(card);
        });
    } catch (e) { console.warn("peers unavailable"); }
}

/* ── inbox ── */
async function loadInbox() {
    try {
        const res  = await fetch("/api/messages");
        const msgs = await res.json();
        renderInbox(msgs);
    } catch (e) { console.warn("inbox unavailable"); }
}

function renderInbox(waiting) {
    inboxPanel.innerHTML = "";

    /* previously read messages — show encrypted + decrypted */
    readHistory.forEach(m => {
        const card = document.createElement("div");
        card.className = "msg-card read";
        card.innerHTML = `
            <div class="msg-meta">From: ${m.author} &bull; ${fmtTime(m.timestamp)}</div>
            <div class="msg-data encrypted">&#128274; ${m.encrypted}</div>
            <div class="msg-data decrypted">&#128275; ${m.plaintext}</div>`;
        inboxPanel.appendChild(card);
    });

    /* waiting messages — only oldest has Read button */
    waiting.forEach((m, idx) => {
        const card = document.createElement("div");
        card.className = "msg-card waiting-msg";
        card.innerHTML = `
            <div class="msg-meta">From: ${m.author} &bull; ${fmtTime(m.timestamp)}</div>
            <div class="msg-data encrypted">&#128274; ${m.data}</div>
            ${idx === 0 ? `<button class="read-btn" onclick="readNext(this, '${m.author}', ${m.timestamp}, '${m.data}')">Read</button>` : ""}`;
        inboxPanel.appendChild(card);
    });

    inboxCount.textContent = waiting.length;
}

async function readNext(btn, author, timestamp, encrypted) {
    btn.disabled = true;
    try {
        const res  = await fetch("/api/read/1", { method: "POST" });
        const data = await res.json();
        if (data.data) {
            readHistory.unshift({ author, timestamp, encrypted, plaintext: data.data });
            loadInbox();
        }
    } catch (e) { console.warn("read failed", e); btn.disabled = false; }
}

socket.on("message_read", () => loadInbox());

/* ── outbox ── */
async function loadOutbox() {
    try {
        const res  = await fetch("/api/outbox");
        const msgs = await res.json();
        renderOutbox(msgs);
    } catch (e) { console.warn("outbox unavailable"); }
}

function renderOutbox(msgs) {
    const now = Date.now();
    outboxPanel.innerHTML = "";
    let queued = 0;

    msgs.forEach((m, idx) => {
        if (!m.waiting) {
            if (!outboxSentAt[idx]) outboxSentAt[idx] = now;
            if (now - outboxSentAt[idx] > 10000) return; /* remove after 10s */
        } else {
            queued++;
        }

        const card = document.createElement("div");
        card.className = `msg-card ${m.waiting ? "outbox-waiting" : "outbox-sent"}`;
        card.innerHTML = `
            <div class="msg-meta">${fmtTime(m.timestamp)} &bull;
                <span class="status-tag">${m.waiting ? "&#9203; Waiting..." : "&#10003; Sent"}</span>
            </div>
            <div class="msg-data encrypted">&#128274; ${m.data}</div>`;
        outboxPanel.appendChild(card);
    });

    outboxCount.textContent = queued;
}

/* ── helpers ── */
function fmtTime(ts) {
    return new Date(ts * 1000).toLocaleTimeString();
}

/* ── polling ── */
loadPeers();
loadInbox();
loadOutbox();
setInterval(loadPeers,  2000);
setInterval(loadInbox,  1000);
setInterval(loadOutbox, 1000);
