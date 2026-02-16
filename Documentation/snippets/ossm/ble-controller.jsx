export const OssmBleController = () => {
  // OSSM BLE UUIDs
  const OSSM_SERVICE_UUID = '522b443a-4f53-534d-0001-420badbabe69';
  const OSSM_COMMAND_CHARACTERISTIC_UUID = '522b443a-4f53-534d-1000-420badbabe69';
  const OSSM_SPEED_KNOB_LIMIT_CHARACTERISTIC_UUID = '522b443a-4f53-534d-1010-420badbabe69';
  const OSSM_WIFI_CONFIG_CHARACTERISTIC_UUID = '522b443a-4f53-534d-1020-420badbabe69';
  const OSSM_STATE_CHARACTERISTIC_UUID = '522b443a-4f53-534d-2000-420badbabe69';
  const OSSM_PATTERNS_CHARACTERISTIC_UUID = '522b443a-4f53-534d-3000-420badbabe69';
  const OSSM_PATTERN_DESCRIPTION_CHARACTERISTIC_UUID = '522b443a-4f53-534d-3010-420badbabe69';

  // Default patterns (fallback if device doesn't report them)
  const DEFAULT_PATTERNS = [
    { idx: 0, name: 'Simple Stroke' },
    { idx: 1, name: 'Teasing Pounding' },
    { idx: 2, name: 'Robo Stroke' },
    { idx: 3, name: "Half'n'Half" },
    { idx: 4, name: 'Deeper' },
    { idx: 5, name: "Stop'n'Go" },
    { idx: 6, name: 'Insist' },
  ];

  // Check if Web Bluetooth is supported
  const isWebBluetoothSupported = () => {
    return typeof navigator !== 'undefined' && 'bluetooth' in navigator;
  };

  // Slider component with smooth dragging and send-on-release
  const Slider = ({ label, value, onRelease, color = 'violet', disabled = false }) => {
    const [localValue, setLocalValue] = useState(value);
    const isDraggingRef = useRef(false);

    // Sync local value with prop when not dragging
    useEffect(() => {
      if (!isDraggingRef.current) {
        setLocalValue(value);
      }
    }, [value]);

    const colorClasses = {
      violet: 'accent-violet-500',
      red: 'accent-red-500',
      green: 'accent-green-500',
      blue: 'accent-blue-500',
      orange: 'accent-orange-500',
    };

    const handleChange = (e) => {
      const newValue = parseInt(e.target.value, 10);
      isDraggingRef.current = true;
      setLocalValue(newValue);
      // No longer calling onChange - just update local state for smooth dragging
    };

    const handleRelease = () => {
      isDraggingRef.current = false;
      if (onRelease) {
        onRelease(localValue);
      }
    };

    return (
      <div className="flex flex-col gap-2">
        <div className="flex justify-between items-center">
          <label className="text-sm font-medium text-zinc-700 dark:text-zinc-300">{label}</label>
          <span className="text-sm font-mono text-zinc-500 dark:text-zinc-400 tabular-nums">
            {localValue}%
          </span>
        </div>
        <input
          type="range"
          min="0"
          max="100"
          value={localValue}
          onChange={handleChange}
          onMouseUp={handleRelease}
          onTouchEnd={handleRelease}
          disabled={disabled}
          className={`w-full h-2 rounded-lg appearance-none cursor-pointer bg-zinc-200 dark:bg-zinc-700 ${colorClasses[color]} disabled:opacity-50 disabled:cursor-not-allowed`}
        />
      </div>
    );
  };

  const [connectionStatus, setConnectionStatus] = useState('disconnected');
  const [device, setDevice] = useState(null);
  const [error, setError] = useState(null);

  const [speed, setSpeed] = useState(0);
  const [depth, setDepth] = useState(50);
  const [stroke, setStroke] = useState(50);
  const [sensation, setSensation] = useState(50);
  const [pattern, setPattern] = useState(0);
  const [patterns, setPatterns] = useState(DEFAULT_PATTERNS);
  const [isPaused, setIsPaused] = useState(true);

  // WiFi state
  const [wifiSSID, setWifiSSID] = useState('');
  const [wifiPassword, setWifiPassword] = useState('');
  const [wifiStatus, setWifiStatus] = useState(null);
  const [isWifiSaving, startWifiTransition] = useTransition();

  // Dev tools state
  const [logs, setLogs] = useState([]);
  const [rawInput, setRawInput] = useState('');

  // Tab state
  const [activeTab, setActiveTab] = useState('controller');

  const commandCharacteristicRef = useRef(null);
  const wifiConfigCharacteristicRef = useRef(null);
  const serverRef = useRef(null);

  const [isSupported, setIsSupported] = useState(true);

  useEffect(() => {
    setIsSupported(isWebBluetoothSupported());
  }, []);

  const logsContainerRef = useRef(null);

  // Auto-scroll logs when new entries are added (only within the container)
  useEffect(() => {
    if (activeTab === 'logs' && logsContainerRef.current) {
      logsContainerRef.current.scrollTop = logsContainerRef.current.scrollHeight;
    }
  }, [logs, activeTab]);

  // Helper to add log entries
  const addLog = useCallback((direction, data) => {
    const entry = {
      id: Date.now() + Math.random(),
      timestamp: new Date().toISOString().split('T')[1].slice(0, 12),
      direction,
      data,
    };
    setLogs((prev) => [...prev.slice(-99), entry]); // Keep last 100 entries
  }, []);

  const sendCommand = useCallback(async (command) => {
    if (!commandCharacteristicRef.current) {
      console.warn('Command characteristic not available');
      return false;
    }

    console.log('[OSSM BLE] Sending:', command);
    addLog('TX', command);

    try {
      const encoder = new TextEncoder();
      await commandCharacteristicRef.current.writeValue(encoder.encode(command));
      return true;
    } catch (err) {
      console.error('Failed to send command:', err);
      addLog('ERR', `Send failed: ${err.message}`);
      setError(`Failed to send command: ${err.message}`);
      return false;
    }
  }, [addLog]);

  // Send commands on slider release
  const handleSpeedRelease = useCallback((value) => {
    setSpeed(value);
    if (value > 0 && isPaused) {
      setIsPaused(false);
    }
    sendCommand(`set:speed:${value}`);
  }, [sendCommand, isPaused]);

  const handleDepthRelease = useCallback((value) => {
    setDepth(value);
    sendCommand(`set:depth:${value}`);
  }, [sendCommand]);

  const handleStrokeRelease = useCallback((value) => {
    setStroke(value);
    sendCommand(`set:stroke:${value}`);
  }, [sendCommand]);

  const handleSensationRelease = useCallback((value) => {
    setSensation(value);
    sendCommand(`set:sensation:${value}`);
  }, [sendCommand]);

  const handlePatternChange = useCallback(async (patternIdx) => {
    setPattern(patternIdx);
    await sendCommand(`set:pattern:${patternIdx}`);
  }, [sendCommand]);

  const handlePauseToggle = useCallback(async () => {
    if (isPaused) {
      const resumeSpeed = speed > 0 ? speed : 50;
      setSpeed(resumeSpeed);
      setIsPaused(false);
      await sendCommand(`set:speed:${resumeSpeed}`);
    } else {
      setSpeed(0);
      setIsPaused(true);
      await sendCommand('set:speed:0');
    }
  }, [isPaused, speed, sendCommand]);

  const handleWifiSave = useCallback(() => {
    if (!wifiConfigCharacteristicRef.current || !wifiSSID || !wifiPassword) {
      console.warn('WiFi characteristic not available or credentials missing');
      return;
    }

    startWifiTransition(async () => {
      const command = `set:wifi:${wifiSSID}|${wifiPassword}`;
      console.log('[OSSM BLE] Configuring WiFi:', wifiSSID);
      addLog('TX', command.replace(/\|.*$/, '|***')); // Hide password in logs

      try {
        const encoder = new TextEncoder();
        await wifiConfigCharacteristicRef.current.writeValue(encoder.encode(command));

        // Read the response
        const responseValue = await wifiConfigCharacteristicRef.current.readValue();
        const response = new TextDecoder().decode(responseValue);
        addLog('RX', response);
        console.log('[OSSM BLE] WiFi config response:', response);

        // Wait a moment then read status
        await new Promise((resolve) => setTimeout(resolve, 2000));
        const statusValue = await wifiConfigCharacteristicRef.current.readValue();
        const status = JSON.parse(new TextDecoder().decode(statusValue));
        setWifiStatus(status);
        console.log('[OSSM BLE] WiFi status:', status);
      } catch (err) {
        console.error('Failed to configure WiFi:', err);
        addLog('ERR', `WiFi config failed: ${err.message}`);
        setError(`Failed to configure WiFi: ${err.message}`);
      }
    });
  }, [wifiSSID, wifiPassword, addLog, startWifiTransition]);

  const handleWifiRefresh = useCallback(async () => {
    if (!wifiConfigCharacteristicRef.current) {
      console.warn('WiFi characteristic not available');
      return;
    }

    try {
      const statusValue = await wifiConfigCharacteristicRef.current.readValue();
      const status = JSON.parse(new TextDecoder().decode(statusValue));
      setWifiStatus(status);
      console.log('[OSSM BLE] WiFi status refreshed:', status);
    } catch (err) {
      console.error('Failed to read WiFi status:', err);
    }
  }, []);

  const handleConnect = async () => {
    setError(null);
    setConnectionStatus('connecting');

    try {
      const bleDevice = await navigator.bluetooth.requestDevice({
        filters: [{ services: [OSSM_SERVICE_UUID] }],
        optionalServices: [OSSM_SERVICE_UUID],
      });

      bleDevice.addEventListener('gattserverdisconnected', () => {
        setConnectionStatus('disconnected');
        setDevice(null);
        commandCharacteristicRef.current = null;
        serverRef.current = null;
        setSpeed(0);
        setIsPaused(true);
      });

      const server = await bleDevice.gatt.connect();
      serverRef.current = server;

      const service = await server.getPrimaryService(OSSM_SERVICE_UUID);

      const commandChar = await service.getCharacteristic(OSSM_COMMAND_CHARACTERISTIC_UUID);
      commandCharacteristicRef.current = commandChar;

      // Get WiFi config characteristic
      try {
        const wifiConfigChar = await service.getCharacteristic(OSSM_WIFI_CONFIG_CHARACTERISTIC_UUID);
        wifiConfigCharacteristicRef.current = wifiConfigChar;

        // Read initial WiFi status
        const wifiStatusValue = await wifiConfigChar.readValue();
        const wifiStatusRaw = new TextDecoder().decode(wifiStatusValue);
        addLog('RX', `wifi status: ${wifiStatusRaw}`);
        const wifiStatusData = JSON.parse(wifiStatusRaw);
        console.log('[OSSM BLE] WiFi status:', wifiStatusData);
        setWifiStatus(wifiStatusData);
      } catch (wifiErr) {
        console.warn('[OSSM BLE] Could not get WiFi characteristic:', wifiErr);
      }

      // Read current state from device
      try {
        const stateChar = await service.getCharacteristic(OSSM_STATE_CHARACTERISTIC_UUID);
        const stateValue = await stateChar.readValue();
        const stateRaw = new TextDecoder().decode(stateValue);
        addLog('RX', `state: ${stateRaw}`);
        const state = JSON.parse(stateRaw);
        console.log('[OSSM BLE] Parsed state:', {
          speed: state.speed,
          depth: state.depth,
          stroke: state.stroke,
          sensation: state.sensation,
          pattern: state.pattern,
        });

        // Update UI with device state
        if (state.speed !== undefined) {
          console.log('[OSSM BLE] Setting speed:', state.speed);
          setSpeed(state.speed);
          setIsPaused(state.speed === 0);
        }
        if (state.depth !== undefined) {
          console.log('[OSSM BLE] Setting depth:', state.depth);
          setDepth(state.depth);
        }
        if (state.stroke !== undefined) {
          console.log('[OSSM BLE] Setting stroke:', state.stroke);
          setStroke(state.stroke);
        }
        if (state.sensation !== undefined) {
          console.log('[OSSM BLE] Setting sensation:', state.sensation);
          setSensation(state.sensation);
        }
        if (state.pattern !== undefined) {
          console.log('[OSSM BLE] Setting pattern:', state.pattern);
          setPattern(state.pattern);
        }
      } catch (stateErr) {
        console.warn('[OSSM BLE] Could not read state:', stateErr);
      }

      // Read patterns from device
      try {
        const patternsChar = await service.getCharacteristic(OSSM_PATTERNS_CHARACTERISTIC_UUID);
        const patternsValue = await patternsChar.readValue();
        const patternsRaw = new TextDecoder().decode(patternsValue);
        addLog('RX', `patterns: ${patternsRaw}`);
        const devicePatterns = JSON.parse(patternsRaw);
        console.log('[OSSM BLE] Parsed patterns:', devicePatterns);

        if (Array.isArray(devicePatterns) && devicePatterns.length > 0) {
          // Try to read pattern descriptions sequentially (GATT only allows one operation at a time)
          try {
            const patternDescChar = await service.getCharacteristic(OSSM_PATTERN_DESCRIPTION_CHARACTERISTIC_UUID);
            const encoder = new TextEncoder();
            const decoder = new TextDecoder();

            const patternsWithDescriptions = [];
            for (const p of devicePatterns) {
              try {
                // Write the pattern index to request its description
                await patternDescChar.writeValue(encoder.encode(String(p.idx)));
                // Read back the description
                const descValue = await patternDescChar.readValue();
                const description = decoder.decode(descValue);
                console.log(`[OSSM BLE] Pattern ${p.idx} (${p.name}): ${description}`);
                patternsWithDescriptions.push({ ...p, description });
              } catch (descErr) {
                console.warn(`[OSSM BLE] Could not read description for pattern ${p.idx}:`, descErr);
                patternsWithDescriptions.push(p);
              }
            }

            setPatterns(patternsWithDescriptions);
          } catch (descCharErr) {
            console.warn('[OSSM BLE] Could not get pattern description characteristic:', descCharErr);
            setPatterns(devicePatterns);
          }
        }
      } catch (patternsErr) {
        console.warn('[OSSM BLE] Could not read patterns, using defaults:', patternsErr);
      }

      const encoder = new TextEncoder();

      // Disable physical speed knob override - allows BLE to control speed
      try {
        const speedKnobLimitChar = await service.getCharacteristic(OSSM_SPEED_KNOB_LIMIT_CHARACTERISTIC_UUID);
        console.log('[OSSM BLE] Sending: speedKnobLimit = false');
        addLog('TX', 'speedKnobLimit: false');
        await speedKnobLimitChar.writeValue(encoder.encode('false'));
      } catch (knobErr) {
        console.warn('Could not set speed knob limit:', knobErr);
      }

      // Enter stroke engine mode
      console.log('[OSSM BLE] Sending: go:strokeEngine');
      addLog('TX', 'go:strokeEngine');
      await commandChar.writeValue(encoder.encode('go:strokeEngine'));

      setDevice(bleDevice);
      setConnectionStatus('connected');
    } catch (err) {
      console.error('Connection failed:', err);
      setError(err.message);
      setConnectionStatus('disconnected');
    }
  };

  const handleDisconnect = async () => {
    if (commandCharacteristicRef.current) {
      try {
        const encoder = new TextEncoder();
        addLog('TX', 'set:speed:0');
        await commandCharacteristicRef.current.writeValue(encoder.encode('set:speed:0'));
        addLog('TX', 'go:menu');
        await commandCharacteristicRef.current.writeValue(encoder.encode('go:menu'));
      } catch (err) {
        console.warn('Could not send disconnect commands:', err);
      }
    }

    if (serverRef.current) {
      serverRef.current.disconnect();
    }

    setDevice(null);
    setConnectionStatus('disconnected');
    commandCharacteristicRef.current = null;
    wifiConfigCharacteristicRef.current = null;
    serverRef.current = null;
    setSpeed(0);
    setIsPaused(true);
    setWifiStatus(null);
  };

  // Show unsupported message if Web Bluetooth is not available
  if (!isSupported) {
    return (
      <div className="not-prose mx-auto max-w-2xl rounded-xl border border-amber-300 bg-amber-50 p-6 dark:border-amber-700 dark:bg-amber-900/20">
        <p className="font-medium text-amber-800 dark:text-amber-200">
          Web Bluetooth is not supported in this browser.
        </p>
        <p className="mt-2 text-sm text-amber-700 dark:text-amber-300">
          Please use Chrome, Edge, or Opera on desktop or Android to use this controller.
        </p>
      </div>
    );
  }

  const isConnected = connectionStatus === 'connected';
  const isConnecting = connectionStatus === 'connecting';

  return (
    <div className="not-prose mx-auto max-w-2xl rounded-xl border border-zinc-200 bg-white p-6 dark:border-zinc-700 dark:bg-zinc-900">
      {/* Error display */}
      {error && (
        <div className="mb-4 rounded-lg border border-red-300 bg-red-50 p-3 dark:border-red-700 dark:bg-red-900/20">
          <div className="flex items-start gap-2">
            <svg className="h-5 w-5 text-red-500 flex-shrink-0 mt-0.5" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8v4m0 4h.01M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
            </svg>
            <div>
              <p className="font-medium text-red-800 dark:text-red-300">Connection Error</p>
              <p className="text-sm text-red-700 dark:text-red-400">{error}</p>
            </div>
          </div>
          <button
            onClick={() => setError(null)}
            className="mt-2 text-sm text-red-600 hover:text-red-800 dark:text-red-400 dark:hover:text-red-300"
          >
            Dismiss
          </button>
        </div>
      )}

      {/* Connection status & button */}
      <div className="mb-6 flex items-center justify-between">
        <div className="flex items-center gap-2">
          <div
            className={`h-3 w-3 rounded-full ${isConnected
              ? 'bg-green-500'
              : isConnecting
                ? 'bg-amber-500 animate-pulse'
                : 'bg-zinc-300 dark:bg-zinc-600'
              }`}
          />
          <span className="text-sm text-zinc-600 dark:text-zinc-400">
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
            {isConnecting ? 'Connecting...' : 'Connect'}
          </button>
        ) : (
          <button
            onClick={handleDisconnect}
            className="rounded-full bg-zinc-200 px-6 py-2 text-sm font-medium text-zinc-700 transition-colors hover:bg-zinc-300 dark:bg-zinc-700 dark:text-zinc-300 dark:hover:bg-zinc-600"
          >
            Disconnect
          </button>
        )}
      </div>

      {/* Controls (only shown when connected) */}
      {isConnected && (
        <>
          {/* Tabs */}
          <div className="mb-4 flex border-b border-zinc-200 dark:border-zinc-700">
            {[
              { id: 'controller', label: 'Controller' },
              { id: 'wifi', label: 'WiFi Settings' },
              { id: 'logs', label: 'Raw Logs' },
            ].map((tab) => (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id)}
                className={`px-4 py-2 text-sm font-medium transition-colors border-b-2 -mb-px ${
                  activeTab === tab.id
                    ? 'border-violet-500 text-violet-600 dark:text-violet-400'
                    : 'border-transparent text-zinc-500 hover:text-zinc-700 dark:text-zinc-400 dark:hover:text-zinc-300'
                }`}
              >
                {tab.label}
                {tab.id === 'logs' && logs.length > 0 && (
                  <span className="ml-1.5 text-xs text-zinc-400">({logs.length})</span>
                )}
              </button>
            ))}
          </div>

          {/* Controller Tab */}
          {activeTab === 'controller' && (
            <>
              {/* Play/Pause toggle */}
              <div className="mb-6">
                <button
                  onClick={handlePauseToggle}
                  className={`w-full rounded-lg px-6 py-3 text-lg font-bold transition-colors ${
                    isPaused
                      ? 'bg-green-500 text-white hover:bg-green-600 active:bg-green-700'
                      : 'bg-red-500 text-white hover:bg-red-600 active:bg-red-700'
                  }`}
                >
                  {isPaused ? 'START' : 'STOP'}
                </button>
              </div>

              {/* Speed control */}
              <div className="mb-6 p-4 rounded-lg bg-zinc-50 dark:bg-zinc-800/50">
                <Slider
                  label="Speed"
                  value={speed}
                  onRelease={handleSpeedRelease}
                  color="violet"
                />
              </div>

              {/* Other controls */}
              <div className="mb-6 space-y-4">
                <Slider
                  label="Depth"
                  value={depth}
                  onRelease={handleDepthRelease}
                  color="red"
                />
                <Slider
                  label="Stroke"
                  value={stroke}
                  onRelease={handleStrokeRelease}
                  color="green"
                />
                <Slider
                  label="Sensation"
                  value={sensation}
                  onRelease={handleSensationRelease}
                  color="blue"
                />
              </div>

              {/* Pattern selection */}
              <div>
                <label className="block text-sm font-medium text-zinc-700 dark:text-zinc-300 mb-2">
                  Pattern
                </label>
                <select
                  value={pattern}
                  onChange={(e) => handlePatternChange(parseInt(e.target.value, 10))}
                  className="w-full rounded-lg border border-zinc-300 bg-white px-3 py-2 text-sm text-zinc-900 focus:border-violet-500 focus:outline-none focus:ring-1 focus:ring-violet-500 dark:border-zinc-600 dark:bg-zinc-800 dark:text-zinc-100"
                >
                  {patterns.map((p) => (
                    <option key={p.idx} value={p.idx}>
                      {p.name}
                    </option>
                  ))}
                </select>
                {patterns.find((p) => p.idx === pattern)?.description && (
                  <p className="mt-1 text-xs text-zinc-500 dark:text-zinc-400">
                    {patterns.find((p) => p.idx === pattern)?.description}
                  </p>
                )}
              </div>
            </>
          )}

          {/* WiFi Settings Tab */}
          {activeTab === 'wifi' && (
            <div className="space-y-4">
              {/* WiFi Status Display */}
              {wifiStatus && (
                <div className={`rounded-lg p-3 ${wifiStatus.connected ? 'bg-green-50 dark:bg-green-900/20 border border-green-200 dark:border-green-700' : 'bg-amber-50 dark:bg-amber-900/20 border border-amber-200 dark:border-amber-700'}`}>
                  <div className="flex items-center justify-between mb-2">
                    <span className={`text-sm font-medium ${wifiStatus.connected ? 'text-green-800 dark:text-green-300' : 'text-amber-800 dark:text-amber-300'}`}>
                      {wifiStatus.connected ? '✓ Connected' : '○ Not Connected'}
                    </span>
                    <button
                      onClick={handleWifiRefresh}
                      className="text-xs text-zinc-600 hover:text-zinc-800 dark:text-zinc-400 dark:hover:text-zinc-200"
                    >
                      Refresh
                    </button>
                  </div>
                  {wifiStatus.ssid && (
                    <div className="text-xs space-y-1">
                      <div className="text-zinc-700 dark:text-zinc-300">
                        <span className="font-medium">SSID:</span> {wifiStatus.ssid}
                      </div>
                      {wifiStatus.ip && (
                        <div className="text-zinc-700 dark:text-zinc-300">
                          <span className="font-medium">IP:</span> {wifiStatus.ip}
                        </div>
                      )}
                      {wifiStatus.connected && wifiStatus.rssi && (
                        <div className="text-zinc-700 dark:text-zinc-300">
                          <span className="font-medium">Signal:</span> {wifiStatus.rssi} dBm
                        </div>
                      )}
                    </div>
                  )}
                </div>
              )}

              {/* WiFi Configuration Form */}
              <div className="space-y-3">
                <div>
                  <label className="block text-sm font-medium text-zinc-700 dark:text-zinc-300 mb-1">
                    Network Name (SSID)
                  </label>
                  <input
                    type="text"
                    value={wifiSSID}
                    onChange={(e) => setWifiSSID(e.target.value)}
                    placeholder="Enter WiFi SSID"
                    maxLength={32}
                    className="w-full rounded-lg border border-zinc-300 bg-white px-3 py-2 text-sm text-zinc-900 placeholder-zinc-400 focus:border-violet-500 focus:outline-none focus:ring-1 focus:ring-violet-500 dark:border-zinc-600 dark:bg-zinc-800 dark:text-zinc-100 dark:placeholder-zinc-500"
                  />
                </div>

                <div>
                  <label className="block text-sm font-medium text-zinc-700 dark:text-zinc-300 mb-1">
                    Password
                  </label>
                  <input
                    type="password"
                    value={wifiPassword}
                    onChange={(e) => setWifiPassword(e.target.value)}
                    placeholder="Enter WiFi password"
                    maxLength={63}
                    className="w-full rounded-lg border border-zinc-300 bg-white px-3 py-2 text-sm text-zinc-900 placeholder-zinc-400 focus:border-violet-500 focus:outline-none focus:ring-1 focus:ring-violet-500 dark:border-zinc-600 dark:bg-zinc-800 dark:text-zinc-100 dark:placeholder-zinc-500"
                  />
                  <p className="mt-1 text-xs text-zinc-500 dark:text-zinc-400">
                    Password must be 8-63 characters
                  </p>
                </div>

                <button
                  onClick={handleWifiSave}
                  disabled={!wifiSSID || wifiPassword.length < 8 || isWifiSaving}
                  className="w-full rounded-lg bg-violet-500 px-4 py-2 text-sm font-medium text-white transition-colors hover:bg-violet-600 disabled:opacity-50 disabled:cursor-not-allowed"
                >
                  {isWifiSaving ? 'Connecting...' : 'Save & Connect'}
                </button>
              </div>
            </div>
          )}

          {/* Raw Logs Tab */}
          {activeTab === 'logs' && (
            <div className="space-y-4">
              {/* Raw Text Input */}
              <div>
                <label className="block text-sm font-medium text-zinc-700 dark:text-zinc-300 mb-2">
                  Raw Command
                </label>
                <div className="flex gap-2">
                  <input
                    type="text"
                    value={rawInput}
                    onChange={(e) => setRawInput(e.target.value)}
                    onKeyDown={(e) => {
                      if (e.key === 'Enter' && rawInput.trim()) {
                        sendCommand(rawInput.trim());
                        setRawInput('');
                      }
                    }}
                    placeholder="e.g., set:speed:50"
                    className="flex-1 rounded-lg border border-zinc-300 bg-white px-3 py-2 text-sm font-mono text-zinc-900 placeholder-zinc-400 focus:border-violet-500 focus:outline-none focus:ring-1 focus:ring-violet-500 dark:border-zinc-600 dark:bg-zinc-800 dark:text-zinc-100 dark:placeholder-zinc-500"
                  />
                  <button
                    onClick={() => {
                      if (rawInput.trim()) {
                        sendCommand(rawInput.trim());
                        setRawInput('');
                      }
                    }}
                    disabled={!rawInput.trim()}
                    className="rounded-lg bg-violet-500 px-4 py-2 text-sm font-medium text-white transition-colors hover:bg-violet-600 disabled:opacity-50 disabled:cursor-not-allowed"
                  >
                    Send
                  </button>
                </div>
              </div>

              {/* Logs Display */}
              <div className="rounded-lg border border-zinc-200 dark:border-zinc-700 overflow-hidden">
                <div className="flex items-center justify-between px-3 py-2 bg-zinc-50 dark:bg-zinc-800">
                  <span className="text-sm font-medium text-zinc-700 dark:text-zinc-300">
                    Log Output
                  </span>
                  {logs.length > 0 && (
                    <button
                      onClick={() => setLogs([])}
                      className="text-xs text-zinc-500 hover:text-zinc-700 dark:hover:text-zinc-300"
                    >
                      Clear
                    </button>
                  )}
                </div>
                <div ref={logsContainerRef} className="h-64 overflow-y-auto bg-zinc-900 p-2">
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
              </div>
            </div>
          )}
        </>
      )}
    </div>
  );
};
