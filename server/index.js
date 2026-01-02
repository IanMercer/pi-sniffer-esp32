require('dotenv').config();

const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const cors = require('cors');
const path = require('path');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

const PORT = process.env.PORT || 3000;

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// Socket.io connection handling
io.on('connection', (socket) => {
    console.log('A client connected');

    socket.on('disconnect', () => {
        console.log('Client disconnected');
    });
});

// Endpoint to receive scanned data
// Matches firmware path: /api/devices
app.post('/api/devices', (req, res) => {
    const data = req.body;

    if (!data) {
        return res.status(400).json({ status: 'error', message: 'No data provided' });
    }

    console.log('Received data batch');

    // Broadcast the data to all connected clients
    io.emit('scanned-data', data);

    res.status(200).json({ status: 'success', message: 'Data received' });
});

// Start server
server.listen(PORT, () => {
    console.log(`Server is running on http://localhost:${PORT}`);
});
