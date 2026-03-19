/**
 * server.js — Node.js bridge between frontend and C backend API
 * HTML <──WebSocket──> Node.js <──HTTP──> C API server
 *                                   <──/proc──> /proc/keycipher/stats
 */

const express    = require('express');
const http       = require('http');
const { Server } = require('socket.io');
const cors       = require('cors');
const axios      = require('axios');
const fs         = require('fs');
const path       = require('path');

const app        = express();
const httpServer = http.createServer(app);
const io         = new Server(httpServer, { cors: { origin: '*' } });

const C_BACKEND_URL = 'http://localhost:8080';
const PROC_STATS    = '/proc/keycipher/stats';
const PORT          = 3001;

app.use(cors());
app.use(express.json());

/* ─────────────────────────────────────────────────────────────── */
/* Serve static HTML/CSS/JS                                        */
/* ─────────────────────────────────────────────────────────────── */

app.use(express.static(path.join(__dirname, 'public')));

app.get('*', (req, res, next) => {
    if (req.path.startsWith('/api')) return next();
    res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

/* ─────────────────────────────────────────────────────────────── */
/* Utility: parse /proc/keycipher/stats                            */
/* ─────────────────────────────────────────────────────────────── */

function parseStats(rawText) {
    const stats = {};
    rawText.split('\n').forEach(line => {
        const [key, val] = line.split(':').map(s => s.trim());
        if (key && val !== undefined) stats[key] = Number(val);
    });
    return stats;
}

/* ─────────────────────────────────────────────────────────────── */
/* GET /api/stats — read /proc directly                            */
/* ─────────────────────────────────────────────────────────────── */

app.get('/api/stats', (req, res) => {
    fs.readFile(PROC_STATS, 'utf8', (err, data) => {
        if (err) return res.status(500).json({ error: 'Failed to read /proc' });

        const stats = parseStats(data);
        io.emit('stats', stats);
        res.json(stats);
    });
});

/* ─────────────────────────────────────────────────────────────── */
/* GET /api/peers — proxy to C backend stats, extract peers        */
/* ─────────────────────────────────────────────────────────────── */

app.get('/api/peers', async (req, res) => {
    try {
        const response = await axios.get(`${C_BACKEND_URL}/api/stats`);
        res.json(response.data.peers || []);
    } catch (err) {
        res.status(500).json({ error: 'C backend unreachable' });
    }
});

/* ─────────────────────────────────────────────────────────────── */
/* GET /api/outbox — proxy to C backend                            */
/* ─────────────────────────────────────────────────────────────── */

app.get('/api/outbox', async (req, res) => {
    try {
        const response = await axios.get(`${C_BACKEND_URL}/api/outbox`);
        res.json(response.data);
    } catch (err) {
        res.status(500).json({ error: 'C backend unreachable' });
    }
});

/* ─────────────────────────────────────────────────────────────── */
/* GET /api/messages — proxy to C backend                          */
/* ─────────────────────────────────────────────────────────────── */

app.get('/api/messages', async (req, res) => {
    try {
        const response = await axios.get(`${C_BACKEND_URL}/api/messages`);
        res.json(response.data);
    } catch (err) {
        res.status(500).json({ error: 'C backend unreachable' });
    }
});

/* ─────────────────────────────────────────────────────────────── */
/* GET /api/chatroom — proxy to C backend                          */
/* ─────────────────────────────────────────────────────────────── */

app.get('/api/chatroom', async (req, res) => {
    try {
        const response = await axios.get(`${C_BACKEND_URL}/api/chatroom`);
        res.json(response.data);
    } catch (err) {
        res.status(500).json({ error: 'C backend unreachable' });
    }
});

/* ─────────────────────────────────────────────────────────────── */
/* POST /api/read/:id — decrypt one message                        */
/* ─────────────────────────────────────────────────────────────── */

app.post('/api/read/:id', async (req, res) => {
    try {
        const response = await axios.post(`${C_BACKEND_URL}/api/read/${req.params.id}`);
        const { sender, plaintext, timestamp } = response.data;

        io.emit('message_read', { sender, plaintext, timestamp });

        // update stats after read
        fs.readFile(PROC_STATS, 'utf8', (err, data) => {
            if (!err) io.emit('stats', parseStats(data));
        });

        res.json(response.data);
    } catch (err) {
        res.status(500).json({ error: 'C backend read failed' });
    }
});

/* ─────────────────────────────────────────────────────────────── */
/* POST /api/read/all — flush FIFO                                 */
/* ─────────────────────────────────────────────────────────────── */

app.post('/api/read/all', async (req, res) => {
    try {
        const response = await axios.post(`${C_BACKEND_URL}/api/read/all`);
        const decryptedMessages = response.data.messages;

        io.emit('flush', { messages: decryptedMessages });

        // update stats after flush
        fs.readFile(PROC_STATS, 'utf8', (err, data) => {
            if (!err) io.emit('stats', parseStats(data));
        });

        res.json(response.data);
    } catch (err) {
        res.status(500).json({ error: 'C backend flush failed' });
    }
});

/* ─────────────────────────────────────────────────────────────── */
/* POST /api/send — send encrypted message                         */
/* ─────────────────────────────────────────────────────────────── */

app.post('/api/send', async (req, res) => {
    try {
        const response = await axios.post(`${C_BACKEND_URL}/api/send`, req.body);

        io.emit('message_sent', {
            target_ip: req.body.target_ip,
            encrypted_preview: response.data.encrypted_preview
        });

        res.json(response.data);
    } catch (err) {
        res.status(500).json({ error: 'C backend send failed' });
    }
});

/* ─────────────────────────────────────────────────────────────── */
/* POST /api/send/chatroom — broadcast encrypted chat message      */
/* ─────────────────────────────────────────────────────────────── */

app.post('/api/send/chatroom', async (req, res) => {
    try {
        const response = await axios.post(`${C_BACKEND_URL}/api/send/chatroom`, req.body);

        io.emit('chatroom_message', {
            sender: 'me',
            encrypted_preview: response.data.encrypted_preview
        });

        res.json(response.data);
    } catch (err) {
        res.status(500).json({ error: 'C backend chatroom send failed' });
    }
});

/* ─────────────────────────────────────────────────────────────── */
/* Background polling: push stats every 500ms                      */
/* ─────────────────────────────────────────────────────────────── */

function statsPollingLoop() {
    setInterval(() => {
        fs.readFile(PROC_STATS, 'utf8', (err, data) => {
            if (err) return;
            io.emit('stats', parseStats(data));
        });
    }, 500);
}

/* ─────────────────────────────────────────────────────────────── */
/* Socket.IO events                                                */
/* ─────────────────────────────────────────────────────────────── */

io.on('connection', (socket) => {
    console.log('Client connected:', socket.id);

    // send immediate stats snapshot
    fs.readFile(PROC_STATS, 'utf8', (err, data) => {
        if (!err) socket.emit('stats', parseStats(data));
    });

    socket.on('disconnect', () => {
        console.log('Client disconnected:', socket.id);
    });
});

/* ─────────────────────────────────────────────────────────────── */
/* Start server                                                    */
/* ─────────────────────────────────────────────────────────────── */

httpServer.listen(PORT, () => {
    console.log(`KeyCipher Node bridge running on port ${PORT}`);
    statsPollingLoop();
});