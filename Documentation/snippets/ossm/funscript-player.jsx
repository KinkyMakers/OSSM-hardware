import { useState, useEffect, useCallback, useRef } from 'react';

export const OssmFunscriptPlayer = () => {
  // OSSM BLE UUIDs
  const OSSM_SERVICE_UUID = '522b443a-4f53-534d-0001-420badbabe69';
  const OSSM_COMMAND_CHARACTERISTIC_UUID = '522b443a-4f53-534d-1000-420badbabe69';
  const OSSM_SPEED_KNOB_CHARACTERISTIC_UUID = '522b443a-4f53-534d-1010-420badbabe69';
  // Check if Web Bluetooth is supported
  const isWebBluetoothSupported = () => {
    return typeof navigator !== 'undefined' && 'bluetooth' in navigator;
  };

  // Connection state
  const [connectionStatus, setConnectionStatus] = useState('disconnected');
  const [device, setDevice] = useState(null);
  const [error, setError] = useState(null);
  const [isSupported, setIsSupported] = useState(true);

  // File state
  const [videoFile, setVideoFile] = useState(null);
  const [videoUrl, setVideoUrl] = useState(null);
  const [funscriptFile, setFunscriptFile] = useState(null);
  const [funscriptActions, setFunscriptActions] = useState([]);

  // Playback state
  const [isPlaying, setIsPlaying] = useState(false);
  const [currentPosition, setCurrentPosition] = useState(0);
  const [currentTime, setCurrentTime] = useState(0);
  const [commandsSent, setCommandsSent] = useState(0);

  // Settings
  const [timeOffset, setTimeOffset] = useState(5);
  const [buffer, setBuffer] = useState(70);
  const [speed, setSpeed] = useState(0);
  const [stroke, setStroke] = useState(50);
  const [depth, setDepth] = useState(50);
  const [sensation, setSensation] = useState(100);

  // Logs
  const [logs, setLogs] = useState([]);
  const [logsExpanded, setLogsExpanded] = useState(false);

  // Refs
  const commandCharacteristicRef = useRef(null);
  const speedKnobCharacteristicRef = useRef(null);
  const serverRef = useRef(null);
  const videoRef = useRef(null);
  const logsContainerRef = useRef(null);
  const currentActionIndexRef = useRef(0);
  const lastSentTimeRef = useRef(0);
  const syncIntervalRef = useRef(null);
  const commandsSentRef = useRef(0);

  useEffect(() => {
    setIsSupported(isWebBluetoothSupported());
  }, []);

  // Auto-scroll logs
  useEffect(() => {
    if (logsExpanded && logsContainerRef.current) {
      logsContainerRef.current.scrollTop = logsContainerRef.current.scrollHeight;
    }
  }, [logs, logsExpanded]);

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      if (syncIntervalRef.current) {
        clearInterval(syncIntervalRef.current);
      }
    };
  }, []);

  // Create and cleanup video object URL
  useEffect(() => {
    if (videoFile) {
      const url = URL.createObjectURL(videoFile);
      setVideoUrl(url);
      return () => {
        URL.revokeObjectURL(url);
      };
    } else {
      setVideoUrl(null);
    }
  }, [videoFile]);

  // Helper to add log entries
  const addLog = useCallback((direction, data) => {
    const entry = {
      id: Date.now() + Math.random(),
      timestamp: new Date().toISOString().split('T')[1].slice(0, 12),
      direction,
      data,
    };
    setLogs((prev) => [...prev.slice(-999), entry]);
  }, []);

  // Send BLE command
  const sendCommand = useCallback(async (command) => {
    if (!commandCharacteristicRef.current) {
      return false;
    }

    try {
      const encoder = new TextEncoder();
      await commandCharacteristicRef.current.writeValueWithoutResponse(encoder.encode(command));
      addLog('TX', command);
      return true;
    } catch (err) {
      addLog('ERR', `Send failed: ${err.message}`);
      return false;
    }
  }, [addLog]);

  // Send stream position command
  const sendStreamPosition = useCallback(async (position, timeMs) => {
    const clampedPos = Math.max(0, Math.min(100, Math.round(position)));
    const clampedTime = Math.max(0, Math.min(10000, Math.round(timeMs)));

    const command = `stream:${clampedPos}:${clampedTime}`;
    const success = await sendCommand(command);

    if (success) {
      commandsSentRef.current += 1;
      setCommandsSent(commandsSentRef.current);
      setCurrentPosition(clampedPos);
    }

    return success;
  }, [sendCommand]);

  const handleBufferChange = useCallback((value) => {
    sendCommand(`set:buffer:${value}`);
  }, [sendCommand]);

  const handleSpeedChange = useCallback((value) => {
    sendCommand(`set:speed:${value}`);
  }, [sendCommand]);

  const handleStrokeChange = useCallback((value) => {
    sendCommand(`set:stroke:${value}`);
  }, [sendCommand]);

  const handleSensationChange = useCallback((value) => {
    sendCommand(`set:sensation:${value}`);
  }, [sendCommand]);

  const handleDepthChange = useCallback((value) => {
    sendCommand(`set:depth:${value}`);
  }, [sendCommand]);

  // Connect to OSSM
  const handleConnect = async () => {
    setError(null);
    setConnectionStatus('connecting');

    try {
      addLog('INFO', 'Requesting OSSM device...');

      const bleDevice = await navigator.bluetooth.requestDevice({
        filters: [{ services: [OSSM_SERVICE_UUID] }],
        optionalServices: [OSSM_SERVICE_UUID],
      });

      bleDevice.addEventListener('gattserverdisconnected', handleDisconnect);

      addLog('INFO', `Found device: ${bleDevice.name}`);

      const server = await bleDevice.gatt.connect();
      serverRef.current = server;
      addLog('INFO', 'Connected to GATT server');

      const service = await server.getPrimaryService(OSSM_SERVICE_UUID);
      addLog('INFO', 'Got OSSM service');

      commandCharacteristicRef.current = await service.getCharacteristic(OSSM_COMMAND_CHARACTERISTIC_UUID);
      addLog('INFO', 'Got command characteristic');

      speedKnobCharacteristicRef.current = await service.getCharacteristic(OSSM_SPEED_KNOB_CHARACTERISTIC_UUID);
      addLog('INFO','Got speed knob characteristic');

      const encoder = new TextEncoder();
      await speedKnobCharacteristicRef.current.writeValue(encoder.encode('false'));
      addLog('INFO','Disable speed knob as override');

      setDevice(bleDevice);
      setConnectionStatus('connected');
      addLog('INFO', 'Ready for funscript playback');

    } catch (err) {
      addLog('ERR', `Connection failed: ${err.message}`);
      setError(err.message);
      setConnectionStatus('disconnected');
    }
  };

  // Handle disconnection
  const handleDisconnect = useCallback(() => {
    addLog('INFO', 'Device disconnected');
    setConnectionStatus('disconnected');
    setDevice(null);
    serverRef.current = null;
    commandCharacteristicRef.current = null;
    stopSync();
  }, [addLog]);

  // Disconnect from OSSM
  const handleDisconnectClick = async () => {
    if (videoRef.current) {
      videoRef.current.pause();
    }
    stopSync();

    try {
      await sendCommand('set:speed:0');
    } catch (e) {
      // Ignore errors during disconnect
    }

    if (serverRef.current) {
      serverRef.current.disconnect();
    }
    handleDisconnect();
  };

  // Parse funscript file
  const parseFunscript = useCallback((content) => {
    try {
      const data = JSON.parse(content);

      if (!data.actions || !Array.isArray(data.actions)) {
        throw new Error('Invalid funscript: missing actions array');
      }

      const actions = data.actions
        .map(action => ({
          pos: action.pos,
          at: action.at
        }))
        .sort((a, b) => a.at - b.at);

      setFunscriptActions(actions);
      addLog('INFO', `Loaded ${actions.length} actions`);

      if (data.version) addLog('INFO', `Funscript version: ${data.version}`);
      if (data.inverted) addLog('INFO', 'Script is inverted');
      if (data.range) addLog('INFO', `Range: ${data.range}`);

      return true;
    } catch (err) {
      addLog('ERR', `Failed to parse funscript: ${err.message}`);
      setError(`Failed to parse funscript: ${err.message}`);
      return false;
    }
  }, [addLog]);

  // Format time for display
  const formatTime = (seconds) => {
    const mins = Math.floor(seconds / 60);
    const secs = Math.floor(seconds % 60);
    return `${mins}:${secs.toString().padStart(2, '0')}`;
  };

  // Sync funscript with video
  const syncFunscript = useCallback(() => {
    if (!videoRef.current || funscriptActions.length === 0) return;

    const currentTimeMs = (videoRef.current.currentTime * 1000) + timeOffset + buffer;
    setCurrentTime(videoRef.current.currentTime);

    while (currentActionIndexRef.current < funscriptActions.length) {
      const action = funscriptActions[currentActionIndexRef.current];

      if (action.at > currentTimeMs) {
        break;
      }

      let timeToNext = 100;
      if (currentActionIndexRef.current < funscriptActions.length - 1) {
        const nextAction = funscriptActions[currentActionIndexRef.current + 1];
        timeToNext = nextAction.at - action.at;      

        if (action.at > lastSentTimeRef.current) {
          sendStreamPosition(nextAction.pos, timeToNext);
          lastSentTimeRef.current = action.at;
        }
      }

      currentActionIndexRef.current++;
    }
  }, [funscriptActions, timeOffset, buffer, sendStreamPosition]);

  // Start sync loop
  const startSync = useCallback(() => {
    if (syncIntervalRef.current) return;
    setIsPlaying(true);
    syncIntervalRef.current = setInterval(syncFunscript, 2);
    addLog('INFO', 'Started sync');
  }, [syncFunscript, addLog]);

  // Stop sync loop
  const stopSync = useCallback(() => {
    if (syncIntervalRef.current) {
      clearInterval(syncIntervalRef.current);
      syncIntervalRef.current = null;
    }
    setIsPlaying(false);
  }, []);

  // Reset playback state
  const resetPlayback = useCallback(() => {
    currentActionIndexRef.current = 0;
    lastSentTimeRef.current = 0;
    commandsSentRef.current = 0;
    setCommandsSent(0);
    stopSync();
  }, [stopSync]);

  // Handle video file selection
  const handleVideoSelect = (e) => {
    const file = e.target.files[0];
    if (file) {
      setVideoFile(file);
      addLog('INFO', `Loaded video: ${file.name}`);
    }
  };

  // Handle funscript file selection
  const handleFunscriptSelect = (e) => {
    const file = e.target.files[0];
    if (file) {
      setFunscriptFile(file);
      const reader = new FileReader();
      reader.onload = (event) => {
        if (parseFunscript(event.target.result)) {
          resetPlayback();
        }
      };
      reader.readAsText(file);
    }
  };

  // Video event handlers
  const handleVideoPlay = () => {
    addLog('INFO', 'Video playing');
    startSync();
  };

  const handleVideoPause = () => {
    addLog('INFO', 'Video paused');
    stopSync();
  };

  const handleVideoSeeked = () => {
    if (!videoRef.current) return;
    const currentTimeMs = videoRef.current.currentTime * 1000;
    currentActionIndexRef.current = funscriptActions.findIndex(a => a.at > currentTimeMs);
    if (currentActionIndexRef.current === -1) currentActionIndexRef.current = funscriptActions.length;
    lastSentTimeRef.current = currentTimeMs - 1;
    addLog('INFO', `Seeked to ${formatTime(videoRef.current.currentTime)}`);
  };

  const handleVideoEnded = () => {
    addLog('INFO', 'Video ended');
    stopSync();
    sendStreamPosition(0, 500);
  };

  // Show unsupported message if Web Bluetooth is not available
  if (!isSupported) {
    return (
      <div className="not-prose mx-auto max-w-2xl rounded-xl border border-amber-300 bg-amber-50 p-6 dark:border-amber-700 dark:bg-amber-900/20">
        <p className="font-medium text-amber-800 dark:text-amber-200">
          Web Bluetooth is not supported in this browser.
        </p>
        <p className="mt-2 text-sm text-amber-700 dark:text-amber-300">
          Please use Chrome, Edge, or Opera on desktop or Android to use this player.
        </p>
      </div>
    );
  }

  const isConnected = connectionStatus === 'connected';
  const isConnecting = connectionStatus === 'connecting';

  return (
    <div className="not-prose mx-auto max-w-2xl space-y-4">
      {/* Error display */}
      {error && (
        <div className="rounded-xl border border-red-300 bg-red-50 p-4 dark:border-red-700 dark:bg-red-900/20">
          <div className="flex items-start gap-2">
            <svg className="h-5 w-5 text-red-500 flex-shrink-0 mt-0.5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8v4m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
            </svg>
            <div className="flex-1">
              <p className="font-medium text-red-800 dark:text-red-300">Error</p>
              <p className="text-sm text-red-700 dark:text-red-400">{error}</p>
            </div>
            <button
              onClick={() => setError(null)}
              className="text-red-500 hover:text-red-700 dark:hover:text-red-300"
            >
              <svg className="h-5 w-5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M6 18L18 6M6 6l12 12" />
              </svg>
            </button>
          </div>
        </div>
      )}

      {/* Connection Card */}
      <div className="rounded-xl border border-zinc-200 bg-white p-6 dark:border-zinc-700 dark:bg-zinc-900">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-3">
            <div
              className={`h-3 w-3 rounded-full ${isConnected
                ? 'bg-green-500'
                : isConnecting
                  ? 'bg-amber-500 animate-pulse'
                  : 'bg-zinc-300 dark:bg-zinc-600'
                }`}
            />
            <span className="text-sm font-medium text-zinc-700 dark:text-zinc-300">
              {isConnected
                ? `Connected to ${device?.name || 'OSSM'}`
                : isConnecting
                  ? 'Connecting...'
                  : 'Disconnected'}
            </span>
          </div>

          {!isConnected ? (
            <button
              onClick={handleConnect}
              disabled={isConnecting}
              className="rounded-full bg-violet-500 px-6 py-2 text-sm font-medium text-white transition-colors hover:bg-violet-600 disabled:opacity-50 disabled:cursor-not-allowed"
            >
              {isConnecting ? 'Connecting...' : 'Connect to OSSM'}
            </button>
          ) : (
            <button
              onClick={handleDisconnectClick}
              className="rounded-full bg-zinc-200 px-6 py-2 text-sm font-medium text-zinc-700 transition-colors hover:bg-zinc-300 dark:bg-zinc-700 dark:text-zinc-300 dark:hover:bg-zinc-600"
            >
              Disconnect
            </button>
          )}
        </div>
      </div>

      {/* Video & Funscript Card */}
      <div className="rounded-xl border border-zinc-200 bg-white p-6 dark:border-zinc-700 dark:bg-zinc-900">
        <h3 className="text-lg font-semibold text-zinc-900 dark:text-zinc-100 mb-4">Video & Funscript</h3>

        {/* File inputs */}
        <div className="grid grid-cols-1 sm:grid-cols-2 gap-4 mb-4">
          <div>
            <label className="block text-sm font-medium text-zinc-700 dark:text-zinc-300 mb-2">
              Video File
            </label>
            <label className="block cursor-pointer">
              <input
                type="file"
                accept="video/*"
                onChange={handleVideoSelect}
                className="hidden"
              />
              <div className={`px-4 py-3 rounded-lg border-2 border-dashed text-center text-sm transition-colors ${videoFile
                ? 'border-green-500 text-green-600 dark:text-green-400'
                : 'border-zinc-300 text-zinc-500 hover:border-violet-500 hover:text-violet-600 dark:border-zinc-600 dark:hover:border-violet-500'
                }`}>
                {videoFile ? videoFile.name : 'Click to select video'}
              </div>
            </label>
          </div>

          <div>
            <label className="block text-sm font-medium text-zinc-700 dark:text-zinc-300 mb-2">
              Funscript File
            </label>
            <label className="block cursor-pointer">
              <input
                type="file"
                accept=".funscript,.json"
                onChange={handleFunscriptSelect}
                className="hidden"
              />
              <div className={`px-4 py-3 rounded-lg border-2 border-dashed text-center text-sm transition-colors ${funscriptFile
                ? 'border-green-500 text-green-600 dark:text-green-400'
                : 'border-zinc-300 text-zinc-500 hover:border-violet-500 hover:text-violet-600 dark:border-zinc-600 dark:hover:border-violet-500'
                }`}>
                {funscriptFile ? funscriptFile.name : 'Click to select .funscript'}
              </div>
            </label>
          </div>
        </div>

        {/* Video player */}
        <div className="rounded-lg overflow-hidden bg-black mb-4">
          {videoUrl ? (
            <video
              ref={videoRef}
              src={videoUrl}
              controls
              className="w-full max-h-96"
              onPlay={handleVideoPlay}
              onPause={handleVideoPause}
              onSeeked={handleVideoSeeked}
              onEnded={handleVideoEnded}
            />
          ) : (
            <div className="flex flex-col items-center justify-center h-48 text-zinc-500">
              <svg className="w-12 h-12 mb-2" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M15.75 10.5l4.72-4.72a.75.75 0 011.28.53v11.38a.75.75 0 01-1.28.53l-4.72-4.72M4.5 18.75h9a2.25 2.25 0 002.25-2.25v-9a2.25 2.25 0 00-2.25-2.25h-9A2.25 2.25 0 002.25 7.5v9a2.25 2.25 0 002.25 2.25z" />
              </svg>
              <span className="text-sm">Load a video file to begin</span>
            </div>
          )}
        </div>

        {/* Position bar */}
        <div className="h-2 bg-zinc-200 dark:bg-zinc-700 rounded-full overflow-hidden mb-4">
          <div
            className="h-full bg-violet-500 transition-all duration-75"
            style={{ width: `${currentPosition}%` }}
          />
        </div>

        {/* Stats */}
        <div className="grid grid-cols-2 sm:grid-cols-4 gap-3">
          <div className="bg-zinc-50 dark:bg-zinc-800 rounded-lg p-3 text-center">
            <div className="text-lg font-semibold text-zinc-900 dark:text-zinc-100">{formatTime(currentTime)}</div>
            <div className="text-xs text-zinc-500 dark:text-zinc-400">Time</div>
          </div>
          <div className="bg-zinc-50 dark:bg-zinc-800 rounded-lg p-3 text-center">
            <div className="text-lg font-semibold text-zinc-900 dark:text-zinc-100">{currentPosition}</div>
            <div className="text-xs text-zinc-500 dark:text-zinc-400">Position</div>
          </div>
          <div className="bg-zinc-50 dark:bg-zinc-800 rounded-lg p-3 text-center">
            <div className="text-lg font-semibold text-zinc-900 dark:text-zinc-100">{funscriptActions.length}</div>
            <div className="text-xs text-zinc-500 dark:text-zinc-400">Actions</div>
          </div>
          <div className="bg-zinc-50 dark:bg-zinc-800 rounded-lg p-3 text-center">
            <div className="text-lg font-semibold text-zinc-900 dark:text-zinc-100">{commandsSent}</div>
            <div className="text-xs text-zinc-500 dark:text-zinc-400">Sent</div>
          </div>
        </div>
      </div>

      {/* Settings Card */}
      <div className="rounded-xl border border-zinc-200 bg-white p-6 dark:border-zinc-700 dark:bg-zinc-900">
        <h3 className="text-lg font-semibold text-zinc-900 dark:text-zinc-100 mb-4">Playback Settings</h3>

        <div className="space-y-4">
          <div>
            <div className="flex justify-between items-center mb-2">
              <label className="text-sm font-medium text-zinc-700 dark:text-zinc-300">Max Speed</label>
              <span className="text-sm font-mono text-zinc-500 dark:text-zinc-400">{speed}</span>
            </div>
            <input
              type="range"
              min="0"
              max="100"
              value={speed}
              onChange={(e) => setSpeed(parseInt(e.target.value))}
              onMouseUp={(e) => handleSpeedChange(parseInt(e.target.value))}
              onTouchEnd={(e) => handleSpeedChange(parseInt(e.target.value))}
              className="w-full h-2 rounded-lg appearance-none cursor-pointer bg-zinc-200 dark:bg-zinc-700 accent-violet-500"
            />
          </div>
          <div>
            <div className="flex justify-between items-center mb-2">
              <label className="text-sm font-medium text-zinc-700 dark:text-zinc-300">Max Stroke Length</label>
              <span className="text-sm font-mono text-zinc-500 dark:text-zinc-400">{stroke}</span>
            </div>
            <input
              type="range"
              min="0"
              max="100"
              value={stroke}
              onChange={(e) => setStroke(parseInt(e.target.value))}
              onMouseUp={(e) => handleStrokeChange(parseInt(e.target.value))}
              onTouchEnd={(e) => handleStrokeChange(parseInt(e.target.value))}
              className="w-full h-2 rounded-lg appearance-none cursor-pointer bg-zinc-200 dark:bg-zinc-700 accent-violet-500"
            />
          </div>
          <div>
            <div className="flex justify-between items-center mb-2">
              <label className="text-sm font-medium text-zinc-700 dark:text-zinc-300">Max Depth</label>
              <span className="text-sm font-mono text-zinc-500 dark:text-zinc-400">{depth}</span>
            </div>
            <input
              type="range"
              min="0"
              max="100"
              value={depth}
              onChange={(e) => setDepth(parseInt(e.target.value))}
              onMouseUp={(e) => handleDepthChange(parseInt(e.target.value))}
              onTouchEnd={(e) => handleDepthChange(parseInt(e.target.value))}
              className="w-full h-2 rounded-lg appearance-none cursor-pointer bg-zinc-200 dark:bg-zinc-700 accent-violet-500"
            />
          </div>
          <div>
            <div className="flex justify-between items-center mb-2">
              <label className="text-sm font-medium text-zinc-700 dark:text-zinc-300">Max Accelration</label>
              <span className="text-sm font-mono text-zinc-500 dark:text-zinc-400">{sensation}</span>
            </div>
            <input
              type="range"
              min="0"
              max="100"
              value={sensation}
              onChange={(e) => setSensation(parseInt(e.target.value))}
              onMouseUp={(e) => handleSensationChange(parseInt(e.target.value))}
              onTouchEnd={(e) => handleSensationChange(parseInt(e.target.value))}
              className="w-full h-2 rounded-lg appearance-none cursor-pointer bg-zinc-200 dark:bg-zinc-700 accent-violet-500"
            />
          </div>
          <div>
            <div className="flex justify-between items-center mb-2">
              <label className="text-sm font-medium text-zinc-700 dark:text-zinc-300">Device Buffer</label>
              <span className="text-sm font-mono text-zinc-500 dark:text-zinc-400">{buffer}ms</span>
            </div>
            <input
              type="range"
              min="0"
              max="100"
              value={buffer}
              onChange={(e) => setBuffer(parseInt(e.target.value))}
              onMouseUp={(e) => handleBufferChange(parseInt(e.target.value))}
              onTouchEnd={(e) => handleBufferChange(parseInt(e.target.value))}
              className="w-full h-2 rounded-lg appearance-none cursor-pointer bg-zinc-200 dark:bg-zinc-700 accent-violet-500"
            />
          </div>
          <div>
            <div className="flex justify-between items-center mb-2">
              <label className="text-sm font-medium text-zinc-700 dark:text-zinc-300">Time Offset</label>
              <span className="text-sm font-mono text-zinc-500 dark:text-zinc-400">{timeOffset}ms</span>
            </div>
            <input
              type="range"
              min="-50"
              max="50"
              value={timeOffset}
              onChange={(e) => setTimeOffset(parseInt(e.target.value))}
              className="w-full h-2 rounded-lg appearance-none cursor-pointer bg-zinc-200 dark:bg-zinc-700 accent-violet-500"
            />
          </div>
        </div>
      </div>

      {/* Debug Logs Card */}
      <div className="rounded-xl border border-zinc-200 bg-white dark:border-zinc-700 dark:bg-zinc-900 overflow-hidden">
        <button
          onClick={() => setLogsExpanded(!logsExpanded)}
          className="w-full flex items-center justify-between px-6 py-4 hover:bg-zinc-50 dark:hover:bg-zinc-800 transition-colors"
        >
          <span className="text-sm font-medium text-zinc-700 dark:text-zinc-300">
            Debug Logs ({logs.length})
          </span>
          <svg
            className={`h-4 w-4 text-zinc-500 transition-transform ${logsExpanded ? 'rotate-180' : ''}`}
            fill="none"
            viewBox="0 0 24 24"
            stroke="currentColor"
          >
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 9l-7 7-7-7" />
          </svg>
        </button>

        {logsExpanded && (
          <>
            <div ref={logsContainerRef} className="max-h-48 overflow-y-auto bg-zinc-900 p-3">
              {logs.length === 0 ? (
                <p className="text-xs text-zinc-500 font-mono">No logs yet...</p>
              ) : (
                <div className="space-y-0.5">
                  {logs.map((log) => (
                    <div key={log.id} className="text-xs font-mono flex gap-2">
                      <span className="text-zinc-500 shrink-0">{log.timestamp}</span>
                      <span
                        className={`shrink-0 font-semibold ${log.direction === 'TX'
                          ? 'text-green-400'
                          : log.direction === 'RX'
                            ? 'text-blue-400'
                            : log.direction === 'INFO'
                              ? 'text-amber-400'
                              : 'text-red-400'
                          }`}
                      >
                        {log.direction}
                      </span>
                      <span className="text-zinc-300 break-all">{log.data}</span>
                    </div>
                  ))}
                </div>
              )}
            </div>

            {logs.length > 0 && (
              <button
                onClick={() => setLogs([])}
                className="w-full px-3 py-2 text-xs text-zinc-500 hover:text-zinc-700 dark:hover:text-zinc-300 bg-zinc-50 dark:bg-zinc-800 border-t border-zinc-200 dark:border-zinc-700"
              >
                Clear Logs
              </button>
            )}
          </>
        )}
      </div>
    </div>
  );
};
