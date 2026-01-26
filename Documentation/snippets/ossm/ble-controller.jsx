import { useState, useEffect, useCallback, useRef } from 'react';

export const OssmBleController = () => {
  // OSSM BLE UUIDs
  const OSSM_SERVICE_UUID = '522b443a-4f53-534d-0001-420badbabe69';
  const OSSM_COMMAND_CHARACTERISTIC_UUID = '522b443a-4f53-534d-1000-420badbabe69';
  const OSSM_SPEED_KNOB_LIMIT_CHARACTERISTIC_UUID = '522b443a-4f53-534d-1010-420badbabe69';
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

  // Slider component
  const Slider = ({ label, value, onChange, color = 'violet', disabled = false }) => {
    const colorClasses = {
      violet: 'accent-violet-500',
      red: 'accent-red-500',
      green: 'accent-green-500',
      blue: 'accent-blue-500',
      orange: 'accent-orange-500',
    };

    return (
      <div className="flex flex-col gap-2">
        <div className="flex justify-between items-center">
          <label className="text-sm font-medium text-zinc-700 dark:text-zinc-300">{label}</label>
          <span className="text-sm font-mono text-zinc-500 dark:text-zinc-400 tabular-nums">
            {value}%
          </span>
        </div>
        <input
          type="range"
          min="0"
          max="100"
          value={value}
          onChange={(e) => onChange(parseInt(e.target.value, 10))}
          disabled={disabled}
          className={`w-full h-2 rounded-lg appearance-none cursor-pointer bg-zinc-200 dark:bg-zinc-700 ${colorClasses[color]} disabled:opacity-50 disabled:cursor-not-allowed`}
        />
      </div>
    );
  };

  // Pattern button component
  const PatternButton = ({ pattern, isActive, onClick, disabled }) => (
    <button
      onClick={() => onClick(pattern.idx)}
      disabled={disabled}
      title={pattern.description || pattern.name}
      className={`px-3 py-2 rounded-lg text-sm font-medium transition-all ${isActive
        ? 'bg-violet-500 text-white shadow-md'
        : 'bg-zinc-100 text-zinc-700 hover:bg-zinc-200 dark:bg-zinc-800 dark:text-zinc-300 dark:hover:bg-zinc-700'
        } disabled:opacity-50 disabled:cursor-not-allowed`}
    >
      {pattern.name}
    </button>
  );

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

  const commandCharacteristicRef = useRef(null);
  const serverRef = useRef(null);

  // Debounce refs for slider controls
  const DEBOUNCE_MS = 100;
  const speedDebounceRef = useRef(null);
  const depthDebounceRef = useRef(null);
  const strokeDebounceRef = useRef(null);
  const sensationDebounceRef = useRef(null);

  const [isSupported, setIsSupported] = useState(true);

  useEffect(() => {
    setIsSupported(isWebBluetoothSupported());
  }, []);

  // Cleanup debounce timers on unmount
  useEffect(() => {
    return () => {
      if (speedDebounceRef.current) clearTimeout(speedDebounceRef.current);
      if (depthDebounceRef.current) clearTimeout(depthDebounceRef.current);
      if (strokeDebounceRef.current) clearTimeout(strokeDebounceRef.current);
      if (sensationDebounceRef.current) clearTimeout(sensationDebounceRef.current);
    };
  }, []);

  const sendCommand = useCallback(async (command) => {
    if (!commandCharacteristicRef.current) {
      console.warn('Command characteristic not available');
      return false;
    }

    console.log('[OSSM BLE] Sending:', command);

    try {
      const encoder = new TextEncoder();
      await commandCharacteristicRef.current.writeValue(encoder.encode(command));
      return true;
    } catch (err) {
      console.error('Failed to send command:', err);
      setError(`Failed to send command: ${err.message}`);
      return false;
    }
  }, []);

  const handleSpeedChange = useCallback((value) => {
    setSpeed(value);
    if (value > 0 && isPaused) {
      setIsPaused(false);
    }

    if (speedDebounceRef.current) clearTimeout(speedDebounceRef.current);
    speedDebounceRef.current = setTimeout(() => {
      sendCommand(`set:speed:${value}`);
    }, DEBOUNCE_MS);
  }, [sendCommand, isPaused]);

  const handleDepthChange = useCallback((value) => {
    setDepth(value);

    if (depthDebounceRef.current) clearTimeout(depthDebounceRef.current);
    depthDebounceRef.current = setTimeout(() => {
      sendCommand(`set:depth:${value}`);
    }, DEBOUNCE_MS);
  }, [sendCommand]);

  const handleStrokeChange = useCallback((value) => {
    setStroke(value);

    if (strokeDebounceRef.current) clearTimeout(strokeDebounceRef.current);
    strokeDebounceRef.current = setTimeout(() => {
      sendCommand(`set:stroke:${value}`);
    }, DEBOUNCE_MS);
  }, [sendCommand]);

  const handleSensationChange = useCallback((value) => {
    setSensation(value);

    if (sensationDebounceRef.current) clearTimeout(sensationDebounceRef.current);
    sensationDebounceRef.current = setTimeout(() => {
      sendCommand(`set:sensation:${value}`);
    }, DEBOUNCE_MS);
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

  const handlePause = useCallback(async () => {
    setSpeed(0);
    setIsPaused(true);
    await sendCommand('set:speed:0');
  }, [sendCommand]);

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

      // Read current state from device
      try {
        const stateChar = await service.getCharacteristic(OSSM_STATE_CHARACTERISTIC_UUID);
        const stateValue = await stateChar.readValue();
        const state = JSON.parse(new TextDecoder().decode(stateValue));
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
        const devicePatterns = JSON.parse(new TextDecoder().decode(patternsValue));
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
        await speedKnobLimitChar.writeValue(encoder.encode('false'));
      } catch (knobErr) {
        console.warn('Could not set speed knob limit:', knobErr);
      }

      // Enter stroke engine mode
      console.log('[OSSM BLE] Sending: go:strokeEngine');
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
        await commandCharacteristicRef.current.writeValue(encoder.encode('set:speed:0'));
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
    serverRef.current = null;
    setSpeed(0);
    setIsPaused(true);
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
          {/* Pause button */}
          <div className="mb-6">
            <button
              onClick={handlePause}
              className="w-full rounded-lg bg-red-500 px-6 py-3 text-lg font-bold text-white transition-colors hover:bg-red-600 active:bg-red-700"
            >
              PAUSE
            </button>
          </div>

          {/* Resume button */}
          <div className="mb-6">
            <button
              onClick={handlePauseToggle}
              className={`w-full rounded-lg px-6 py-3 text-base font-medium transition-colors ${isPaused
                ? 'bg-green-500 text-white hover:bg-green-600'
                : 'bg-amber-500 text-white hover:bg-amber-600'
                }`}
            >
              {isPaused ? 'Resume' : 'Pause'}
            </button>
          </div>

          {/* Speed control */}
          <div className="mb-6 p-4 rounded-lg bg-zinc-50 dark:bg-zinc-800/50">
            <Slider
              label="Speed"
              value={speed}
              onChange={handleSpeedChange}
              color="violet"
            />
          </div>

          {/* Other controls */}
          <div className="mb-6 space-y-4">
            <Slider
              label="Depth"
              value={depth}
              onChange={handleDepthChange}
              color="red"
            />
            <Slider
              label="Stroke"
              value={stroke}
              onChange={handleStrokeChange}
              color="green"
            />
            <Slider
              label="Sensation"
              value={sensation}
              onChange={handleSensationChange}
              color="blue"
            />
          </div>

          {/* Pattern selection */}
          <div className="mb-4">
            <label className="block text-sm font-medium text-zinc-700 dark:text-zinc-300 mb-3">
              Pattern
            </label>
            <div className="flex flex-wrap gap-2">
              {patterns.map((p) => (
                <PatternButton
                  key={p.idx}
                  pattern={p}
                  isActive={pattern === p.idx}
                  onClick={handlePatternChange}
                  disabled={false}
                />
              ))}
            </div>
          </div>
        </>
      )}
    </div>
  );
};
