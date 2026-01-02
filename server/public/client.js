const socket = io();

const statusDot = document.getElementById('status-dot');
const statusText = document.getElementById('status-text');
const dataStream = document.getElementById('data-stream');
const counterDisplay = document.getElementById('counter');
const lastDeviceDisplay = document.getElementById('last-device');

let packetCount = 0;

socket.on('connect', () => {
    console.log('Connected to server');
    statusDot.classList.add('connected');
    statusText.innerText = 'Live System Online';
});

socket.on('disconnect', () => {
    console.log('Disconnected from server');
    statusDot.classList.remove('connected');
    statusText.innerText = 'Disconnected';
});

socket.on('scanned-data', (data) => {
    // Remove empty state if present
    const emptyState = dataStream.querySelector('.empty-state');
    if (emptyState) {
        emptyState.remove();
    }

    // Handle firmware data structure
    const devices = data.devices || [];
    const summary = data.summary || {};
    const batchCount = devices.length;

    // Update global counter (cumulative or current view?)
    // Let's make it cumulative packet/update count for now, or total devices seen
    packetCount++;

    // If summary is provided, use it for a "Devices Nearby" counter or similar
    // For now we just update the specific stats
    if (summary.total_devices !== undefined) {
        counterDisplay.innerText = summary.total_devices;
        // Update label to reflect this is "Devices Nearby" not "Total Packets"
        const cardTitle = counterDisplay.parentElement.querySelector('h2');
        if (cardTitle) cardTitle.innerText = "Devices Nearby";
    } else {
        counterDisplay.innerText = packetCount;
    }

    // Create log entry
    const entry = document.createElement('div');
    entry.className = 'log-entry';

    const timestamp = document.createElement('div');
    timestamp.className = 'log-timestamp';
    timestamp.innerText = new Date().toLocaleTimeString();

    const content = document.createElement('div');
    content.className = 'log-content';

    // Format the log message
    let message = "";
    if (devices.length > 0) {
        message = `Received ${devices.length} devices. Top: ${devices[0].name || devices[0].mac} (${devices[0].rssi}dBm)`;
    } else {
        message = "Received heartbeat (0 devices)";
    }
    content.innerText = message;

    entry.appendChild(timestamp);
    entry.appendChild(content);

    // Append to stream
    dataStream.appendChild(entry);

    // Auto scroll to bottom
    dataStream.scrollTop = dataStream.scrollHeight;

    // Update 'Last Device'
    if (devices.length > 0) {
        // Pick the strongest signal or just the first one? First one is fine.
        const last = devices[0];
        lastDeviceDisplay.innerText = `${last.name || 'Unknown'} [${last.mac}]`;
    }
});

function clearLog() {
    dataStream.innerHTML = '<div class="empty-state">Log cleared. Waiting for data...</div>';
    packetCount = 0;
    counterDisplay.innerText = packetCount;
}
