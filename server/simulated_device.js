const http = require('http');

console.log("Starting simulated device...");

function generateRandomMac() {
    return "XX:XX:XX:XX:XX:XX".replace(/X/g, function () {
        return "0123456789ABCDEF".charAt(Math.floor(Math.random() * 16));
    });
}

function sendData() {
    const numDevices = Math.floor(Math.random() * 5) + 1;
    const devices = [];

    for (let i = 0; i < numDevices; i++) {
        devices.push({
            mac: generateRandomMac(),
            rssi: -Math.floor(Math.random() * 60) - 30, // -30 to -90
            distance: Math.random() * 10,
            name: Math.random() > 0.5 ? "Test Device " + i : "",
            category: "Unknown",
            address_type: "random",
            seen_count: Math.floor(Math.random() * 100),
            first_seen: Date.now() / 1000 - 100,
            last_seen: Date.now() / 1000
        });
    }

    const data = JSON.stringify({
        device_id: "ESP32-Simulated",
        timestamp: Date.now() / 1000,
        devices: devices,
        summary: {
            total_devices: numDevices,
            phones: 0,
            computers: 0,
            others: numDevices
        }
    });

    const options = {
        hostname: 'localhost',
        port: 3000,
        path: '/api/devices', // Updated to match firmware
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Content-Length': data.length
        }
    };

    const req = http.request(options, (res) => {
        // console.log(`StatusCode: ${res.statusCode}`);
    });

    req.on('error', (error) => {
        console.error(error);
    });

    req.write(data);
    req.end();
}

// Send data every 2 seconds
setInterval(sendData, 2000);
console.log("Sending data every 2 seconds to /api/devices. Press Ctrl+C to stop.");
