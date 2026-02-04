// OSSM Funscript Player
// Connects to OSSM via BLE and syncs video playback with funscript position commands

// BLE UUIDs
const OSSM_SERVICE_UUID = '522b443a-4f53-534d-0001-420badbabe69';
const OSSM_COMMAND_CHARACTERISTIC_UUID = '522b443a-4f53-534d-1000-420badbabe69';
const OSSM_STATE_CHARACTERISTIC_UUID = '522b443a-4f53-534d-2000-420badbabe69';

// State
let device = null;
let server = null;
let commandCharacteristic = null;
let stateCharacteristic = null;
let funscriptActions = [];
let currentActionIndex = 0;
let lastSentTime = 0;
let commandsSent = 0;
let isPlaying = false;
let syncInterval = null;

// Settings
let timeOffset = 0; // ms offset for funscript timing
let lookAhead = 100; // ms to send commands ahead of video time

// DOM Elements
const connectBtn = document.getElementById('connect-btn');
const disconnectBtn = document.getElementById('disconnect-btn');
const statusDot = document.getElementById('status-dot');
const statusText = document.getElementById('status-text');
const videoInput = document.getElementById('video-input');
const funscriptInput = document.getElementById('funscript-input');
const videoPlayer = document.getElementById('video-player');
const videoPlaceholder = document.getElementById('video-placeholder');
const videoFileDisplay = document.getElementById('video-file-display');
const funscriptFileDisplay = document.getElementById('funscript-file-display');
const positionBar = document.getElementById('position-bar');
const currentTimeDisplay = document.getElementById('current-time');
const currentPositionDisplay = document.getElementById('current-position');
const actionsCountDisplay = document.getElementById('actions-count');
const commandsSentDisplay = document.getElementById('commands-sent');
const offsetSlider = document.getElementById('offset-slider');
const offsetValue = document.getElementById('offset-value');
const lookaheadSlider = document.getElementById('lookahead-slider');
const lookaheadValue = document.getElementById('lookahead-value');
const logsContainer = document.getElementById('logs-container');
const clearLogsBtn = document.getElementById('clear-logs');
const browserWarning = document.getElementById('browser-warning');

// Check Web Bluetooth support
function checkWebBluetoothSupport() {
    if (!navigator.bluetooth) {
        browserWarning.classList.add('visible');
        connectBtn.disabled = true;
        return false;
    }
    return true;
}

// Logging
function log(direction, message) {
    const now = new Date();
    const timeStr = now.toTimeString().split(' ')[0] + '.' + String(now.getMilliseconds()).padStart(3, '0');

    const entry = document.createElement('div');
    entry.className = 'log-entry';
    entry.innerHTML = `
        <span class="log-time">${timeStr}</span>
        <span class="log-direction ${direction.toLowerCase()}">${direction}</span>
        <span class="log-message">${message}</span>
    `;

    logsContainer.appendChild(entry);
    logsContainer.scrollTop = logsContainer.scrollHeight;

    // Keep only last 100 entries
    while (logsContainer.children.length > 100) {
        logsContainer.removeChild(logsContainer.firstChild);
    }
}

// Connection status update
function setConnectionStatus(status) {
    statusDot.classList.remove('connected', 'connecting');
    switch (status) {
        case 'connected':
            statusDot.classList.add('connected');
            statusText.textContent = device?.name || 'Connected';
            connectBtn.classList.add('hidden');
            disconnectBtn.classList.remove('hidden');
            break;
        case 'connecting':
            statusDot.classList.add('connecting');
            statusText.textContent = 'Connecting...';
            connectBtn.disabled = true;
            break;
        case 'disconnected':
        default:
            statusText.textContent = 'Disconnected';
            connectBtn.classList.remove('hidden');
            connectBtn.disabled = false;
            disconnectBtn.classList.add('hidden');
            break;
    }
}

// Send BLE command
async function sendCommand(command) {
    if (!commandCharacteristic) {
        log('ERR', 'Not connected');
        return false;
    }

    try {
        const encoder = new TextEncoder();
        await commandCharacteristic.writeValue(encoder.encode(command));
        log('TX', command);
        return true;
    } catch (error) {
        log('ERR', `Send failed: ${error.message}`);
        return false;
    }
}

// Send stream position command
async function sendStreamPosition(position, timeMs) {
    // Clamp position to 0-100
    const clampedPos = Math.max(0, Math.min(100, Math.round(position)));
    // Clamp time to reasonable range
    const clampedTime = Math.max(0, Math.min(10000, Math.round(timeMs)));

    const command = `stream:${clampedPos}:${clampedTime}`;
    const success = await sendCommand(command);

    if (success) {
        commandsSent++;
        commandsSentDisplay.textContent = commandsSent;
        currentPositionDisplay.textContent = clampedPos;
        positionBar.style.width = `${clampedPos}%`;
    }

    return success;
}

// Connect to OSSM
async function connect() {
    try {
        setConnectionStatus('connecting');
        log('INFO', 'Requesting OSSM device...');

        device = await navigator.bluetooth.requestDevice({
            filters: [{ services: [OSSM_SERVICE_UUID] }],
            optionalServices: [OSSM_SERVICE_UUID]
        });

        device.addEventListener('gattserverdisconnected', handleDisconnect);

        log('INFO', `Found device: ${device.name}`);

        server = await device.gatt.connect();
        log('INFO', 'Connected to GATT server');

        const service = await server.getPrimaryService(OSSM_SERVICE_UUID);
        log('INFO', 'Got OSSM service');

        commandCharacteristic = await service.getCharacteristic(OSSM_COMMAND_CHARACTERISTIC_UUID);
        log('INFO', 'Got command characteristic');

        // Try to get state characteristic for notifications
        try {
            stateCharacteristic = await service.getCharacteristic(OSSM_STATE_CHARACTERISTIC_UUID);
            await stateCharacteristic.startNotifications();
            stateCharacteristic.addEventListener('characteristicvaluechanged', handleStateChange);
            log('INFO', 'Subscribed to state notifications');
        } catch (e) {
            log('INFO', 'State notifications not available');
        }

        // Enter streaming mode
        log('INFO', 'Entering streaming mode...');
        await sendCommand('go:streaming');

        setConnectionStatus('connected');
        log('INFO', 'Ready for funscript playback');

    } catch (error) {
        log('ERR', `Connection failed: ${error.message}`);
        setConnectionStatus('disconnected');
    }
}

// Handle disconnection
function handleDisconnect() {
    log('INFO', 'Device disconnected');
    setConnectionStatus('disconnected');
    device = null;
    server = null;
    commandCharacteristic = null;
    stateCharacteristic = null;
    stopSync();
}

// Handle state changes from OSSM
function handleStateChange(event) {
    const decoder = new TextDecoder();
    const value = decoder.decode(event.target.value);
    log('RX', `State: ${value}`);
}

// Disconnect
async function disconnect() {
    if (server) {
        // Stop video playback
        videoPlayer.pause();
        stopSync();

        // Send speed 0 and return to menu
        try {
            await sendCommand('set:speed:0');
            await sendCommand('go:menu');
        } catch (e) {
            // Ignore errors during disconnect
        }

        server.disconnect();
    }
    handleDisconnect();
}

// Parse funscript file
function parseFunscript(content) {
    try {
        const data = JSON.parse(content);

        if (!data.actions || !Array.isArray(data.actions)) {
            throw new Error('Invalid funscript: missing actions array');
        }

        // Sort actions by timestamp
        funscriptActions = data.actions
            .map(action => ({
                pos: action.pos,
                at: action.at
            }))
            .sort((a, b) => a.at - b.at);

        log('INFO', `Loaded ${funscriptActions.length} actions`);
        actionsCountDisplay.textContent = funscriptActions.length;

        // Log metadata if present
        if (data.version) log('INFO', `Funscript version: ${data.version}`);
        if (data.inverted) log('INFO', 'Script is inverted');
        if (data.range) log('INFO', `Range: ${data.range}`);

        return true;
    } catch (error) {
        log('ERR', `Failed to parse funscript: ${error.message}`);
        return false;
    }
}

// Format time for display
function formatTime(seconds) {
    const mins = Math.floor(seconds / 60);
    const secs = Math.floor(seconds % 60);
    return `${mins}:${secs.toString().padStart(2, '0')}`;
}

// Sync funscript with video
function syncFunscript() {
    if (!isPlaying || funscriptActions.length === 0) return;

    const currentTimeMs = (videoPlayer.currentTime * 1000) + timeOffset + lookAhead;
    currentTimeDisplay.textContent = formatTime(videoPlayer.currentTime);

    // Find the next action to send
    while (currentActionIndex < funscriptActions.length) {
        const action = funscriptActions[currentActionIndex];

        // If this action is in the future, wait
        if (action.at > currentTimeMs) {
            break;
        }

        // Calculate time to next action (for speed calculation)
        let timeToNext = 100; // Default 100ms
        if (currentActionIndex < funscriptActions.length - 1) {
            const nextAction = funscriptActions[currentActionIndex + 1];
            timeToNext = nextAction.at - action.at;
        }

        // Don't send if we've already sent this timestamp
        if (action.at > lastSentTime) {
            sendStreamPosition(action.pos, timeToNext);
            lastSentTime = action.at;
        }

        currentActionIndex++;
    }
}

// Start sync loop
function startSync() {
    if (syncInterval) return;
    isPlaying = true;
    syncInterval = setInterval(syncFunscript, 10); // Check every 10ms
    log('INFO', 'Started sync');
}

// Stop sync loop
function stopSync() {
    if (syncInterval) {
        clearInterval(syncInterval);
        syncInterval = null;
    }
    isPlaying = false;
}

// Reset playback state
function resetPlayback() {
    currentActionIndex = 0;
    lastSentTime = 0;
    stopSync();
}

// Event Listeners

connectBtn.addEventListener('click', connect);
disconnectBtn.addEventListener('click', disconnect);

videoInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (file) {
        videoFileDisplay.textContent = file.name;
        videoFileDisplay.classList.add('has-file');

        const url = URL.createObjectURL(file);
        videoPlayer.src = url;
        videoPlayer.classList.remove('hidden');
        videoPlaceholder.classList.add('hidden');

        log('INFO', `Loaded video: ${file.name}`);

        // Try to auto-load matching funscript
        const baseName = file.name.replace(/\.[^/.]+$/, '');
        log('INFO', `Looking for funscript: ${baseName}.funscript`);
    }
});

funscriptInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (file) {
        funscriptFileDisplay.textContent = file.name;
        funscriptFileDisplay.classList.add('has-file');

        const reader = new FileReader();
        reader.onload = (event) => {
            if (parseFunscript(event.target.result)) {
                resetPlayback();
            }
        };
        reader.readAsText(file);
    }
});

videoPlayer.addEventListener('play', () => {
    log('INFO', 'Video playing');
    startSync();
});

videoPlayer.addEventListener('pause', () => {
    log('INFO', 'Video paused');
    stopSync();
});

videoPlayer.addEventListener('seeked', () => {
    // Reset to find the correct action index after seeking
    const currentTimeMs = videoPlayer.currentTime * 1000;
    currentActionIndex = funscriptActions.findIndex(a => a.at > currentTimeMs);
    if (currentActionIndex === -1) currentActionIndex = funscriptActions.length;
    lastSentTime = currentTimeMs - 1;
    log('INFO', `Seeked to ${formatTime(videoPlayer.currentTime)}, action index: ${currentActionIndex}`);
});

videoPlayer.addEventListener('ended', () => {
    log('INFO', 'Video ended');
    stopSync();
    // Send position to 0
    sendStreamPosition(0, 500);
});

offsetSlider.addEventListener('input', (e) => {
    timeOffset = parseInt(e.target.value);
    offsetValue.textContent = `${timeOffset}ms`;
});

lookaheadSlider.addEventListener('input', (e) => {
    lookAhead = parseInt(e.target.value);
    lookaheadValue.textContent = `${lookAhead}ms`;
});

clearLogsBtn.addEventListener('click', () => {
    logsContainer.innerHTML = '';
});

// Initialize
checkWebBluetoothSupport();
log('INFO', 'OSSM Funscript Player ready');
log('INFO', 'Load a video and funscript file, then connect to your OSSM');
