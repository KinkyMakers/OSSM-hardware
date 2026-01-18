export const GoldMotorControl = () => {
  // =============================================================================
  // STATE MANAGEMENT
  // =============================================================================
  
  // UI state
  const [isConnected, setIsConnected] = useState(false)
  const [showAdvanced, setShowAdvanced] = useState(false)
  const [feedback, setFeedback] = useState({ message: "", type: "" })
  const [writeStatus, setWriteStatus] = useState({ message: "", faded: false })
  const [registerOutput, setRegisterOutput] = useState([])

  // Refs for Web Serial API objects (non-reactive - don't need to trigger re-renders)
  const portRef = useRef(null)
  const readerRef = useRef(null)
  const writerRef = useRef(null)
  // Track which registers were written so we can highlight them after reading
  const writtenRegistersRef = useRef([])

  // Form values with defaults
  const [formValues, setFormValues] = useState({
    steps_per_revolution: 800,
    max_output: 600,
    speed_kp: 3000,
    position_kp: 3000,
    direction_polarity: true,
    hack_the_planet: false,
    // Advanced fields
    modbus_enable: 0,
    drive_output_enable: 7,
    motor_target_speed: 1500,
    motor_acceleration: 50000,
    weak_magnetic_angle: 495,
    speed_loop_integration_time: 10,
    speed_feed: 3900,
    electronic_gear_molecules: 32768,
    target_location_low: 0,
    target_location_high: 0,
    alarm_code: 0,
    system_current: 0,
    motor_current_speed: 0,
    system_voltage: 0,
    system_temperature: 0,
    system_pwm_output: 0,
    parameter_saving_flag: 0,
    device_address: 1,
    absolute_position_low: 0,
    absolute_position_high: 0,
    specific_function: 0,
  })

  const updateFormValue = (key, value) => {
    setFormValues((prev) => ({ ...prev, [key]: value }))
  }

  const resetDefaults = () => {
    setFormValues({
      steps_per_revolution: 800,
      max_output: 600,
      speed_kp: 3000,
      position_kp: 3000,
      direction_polarity: true,
      hack_the_planet: false,
      modbus_enable: 0,
      drive_output_enable: 7,
      motor_target_speed: 1500,
      motor_acceleration: 50000,
      weak_magnetic_angle: 495,
      speed_loop_integration_time: 10,
      speed_feed: 3900,
      electronic_gear_molecules: 32768,
      target_location_low: 0,
      target_location_high: 0,
      alarm_code: 0,
      system_current: 0,
      motor_current_speed: 0,
      system_voltage: 0,
      system_temperature: 0,
      system_pwm_output: 0,
      parameter_saving_flag: 0,
      device_address: 1,
      absolute_position_low: 0,
      absolute_position_high: 0,
      specific_function: 0,
    })
  }

  // =============================================================================
  // MODBUS RTU PROTOCOL IMPLEMENTATION
  // =============================================================================
  // The Gold Motor uses Modbus RTU over RS485 at 19200 baud.
  // Frame format: [SlaveAddr][FuncCode][Data...][CRC16]
  
  /**
   * Calculate CRC-16/MODBUS checksum
   * Polynomial: 0xA001 (reversed 0x8005)
   * Initial value: 0xFFFF
   */
  const calculateCRC = (buffer) => {
    let crc = 0xffff
    for (let pos = 0; pos < buffer.length; pos++) {
      crc ^= buffer[pos] // XOR byte into least sig. byte of CRC
      for (let i = 8; i !== 0; i--) {
        if ((crc & 0x0001) !== 0) {
          crc >>= 1
          crc ^= 0xa001
        } else {
          crc >>= 1
        }
      }
    }
    // Swap bytes for Modbus (LSB first)
    return ((crc & 0xff00) >> 8) | ((crc & 0x00ff) << 8)
  }

  /**
   * Build a Modbus RTU command frame
   * @param register - Register address (0x00-0x19)
   * @param value - Value to write, or number of registers to read
   * @param read - true for read (func 0x03), false for write (func 0x06)
   * @returns 8-byte Uint8Array command
   */
  const createModbusCommand = (register, value, read = false) => {
    const buffer = new ArrayBuffer(8)
    const view = new DataView(buffer)
    view.setUint8(0, 0x01)                    // Slave address (always 1)
    view.setUint8(1, read ? 0x03 : 0x06)      // Function: 0x03=read, 0x06=write single
    view.setUint16(2, register, false)        // Register address (big-endian)
    view.setUint16(4, value, false)           // Value or read count (big-endian)
    const crc = calculateCRC(new Uint8Array(buffer.slice(0, 6)))
    view.setUint16(6, crc, false)             // CRC-16 (big-endian after swap)
    return new Uint8Array(buffer)
  }

  /**
   * Parse a Modbus read response
   * Response format: [SlaveAddr][FuncCode][ByteCount][Data...][CRC]
   */
  const parseModbusResponse = (buffer) => {
    const view = new DataView(buffer.buffer)
    const functionCode = view.getUint8(1)
    // Check for error response (function code with high bit set)
    if (functionCode & 0x80) {
      const exceptionCode = view.getUint8(2)
      throw new Error(`Modbus exception code ${exceptionCode}`)
    }
    const byteCount = view.getUint8(2)
    const data = []
    for (let i = 0; i < byteCount / 2; i++) {
      data.push(view.getUint16(3 + i * 2, false))
    }
    return data.length === 1 ? data[0] : data
  }

  /**
   * Parse a Modbus write response (echo of the write command)
   * Response format: [SlaveAddr][FuncCode][RegAddr][Value][CRC]
   */
  const parseModbusWriteResponse = (buffer) => {
    const view = new DataView(buffer.buffer)
    const functionCode = view.getUint8(1)
    if (functionCode === 0x06) {
      return true // Successful write (func code echoed)
    } else if (functionCode & 0x80) {
      const exceptionCode = view.getUint8(2)
      throw new Error(`Modbus exception code ${exceptionCode}`)
    }
    return false
  }

  /** Helper to concatenate two Uint8Arrays (for accumulating serial data) */
  const concatenate = (buffer1, buffer2) => {
    const tmp = new Uint8Array(buffer1.length + buffer2.length)
    tmp.set(buffer1, 0)
    tmp.set(buffer2, buffer1.length)
    return tmp
  }

  const displayFeedback = (message, type) => {
    setFeedback({ message, type })
  }

  // =============================================================================
  // SERIAL COMMUNICATION (Web Serial API)
  // =============================================================================
  
  /**
   * Write a single register to the motor
   * Sends Modbus write command (func 0x06) and waits for 8-byte response
   */
  const writeRegister = async (register, value) => {
    const writer = writerRef.current
    const reader = readerRef.current
    if (writer && reader) {
      const command = createModbusCommand(register, value, false)
      await writer.write(command)
      displayFeedback(
        `Sent write command for register 0x${register.toString(16)} with value ${value}`,
        "info"
      )
      try {
        let responseBuffer = new Uint8Array()
        while (true) {
          const { value, done } = await reader.read()
          if (done) break
          responseBuffer = concatenate(responseBuffer, new Uint8Array(value))
          if (responseBuffer.length >= 8) break
        }
        if (responseBuffer.length >= 8) {
          const success = parseModbusWriteResponse(responseBuffer)
          if (success) {
            displayFeedback(
              `Write successful for register 0x${register.toString(16)}`,
              "success"
            )
          } else {
            displayFeedback(
              `Write failed for register 0x${register.toString(16)}`,
              "error"
            )
          }
        } else {
          displayFeedback(
            `Incomplete write response for register 0x${register.toString(16)}`,
            "error"
          )
        }
      } catch (error) {
        displayFeedback(
          `Error writing register 0x${register.toString(16)}: ${error}`,
          "error"
        )
      }
    } else {
      displayFeedback("No connection to motor", "error")
    }
  }

  /**
   * Read a single register from the motor
   * Sends Modbus read command (func 0x03) and waits for 7-byte response
   * Response: [Addr(1)][Func(1)][ByteCount(1)][Data(2)][CRC(2)] = 7 bytes
   */
  const readRegister = async (register) => {
    const writer = writerRef.current
    const reader = readerRef.current
    if (writer && reader) {
      const command = createModbusCommand(register, 1, true) // Read 1 register
      await writer.write(command)
      try {
        let responseBuffer = new Uint8Array()
        // Accumulate bytes until we have a complete response
        while (true) {
          const { value, done } = await reader.read()
          if (done) break
          responseBuffer = concatenate(responseBuffer, new Uint8Array(value))
          if (responseBuffer.length >= 7) break // 7 bytes for single register read
        }
        if (responseBuffer.length >= 7) {
          return parseModbusResponse(responseBuffer)
        } else {
          return null
        }
      } catch (error) {
        displayFeedback(
          `Error reading register 0x${register.toString(16)}: ${error}`,
          "error"
        )
        return null
      }
    }
    return null
  }

  // =============================================================================
  // REGISTER MAP
  // Gold Motor register addresses and their human-readable names
  // =============================================================================
  const registerMap = {
    0x00: "MODBUS_ENABLE",
    0x01: "DRIVE_OUTPUT_ENABLE",
    0x02: "MOTOR_TARGET_SPEED",
    0x03: "MOTOR_ACCELERATION",
    0x04: "WEAK_MAGNETIC_ANGLE",
    0x05: "SPEED_RING_PROPORTIONAL_RATIO_COEFFICIENT",
    0x06: "SPEED_LOOP_INTEGRATION_TIME",
    0x07: "POSITION_RING_PROPORTIONAL_COEFFICIENT",
    0x08: "SPEED_FEED",
    0x09: "DIR_POLARITY",
    0x0a: "ELECTRONIC_GEAR_MOLECULES",
    0x0b: "ELECTRONIC_GEAR_DENOMINATOR",
    0x0c: "TARGET_LOCATION_IS_16_BITS_LOWER",
    0x0d: "TARGET_POSITION_IS_16_BITS_HIGH",
    0x0e: "ALARM_CODE",
    0x0f: "SYSTEM_CURRENT",
    0x10: "MOTOR_CURRENT_SPEED",
    0x11: "SYSTEM_VOLTAGE",
    0x12: "SYSTEM_TEMPERATURE",
    0x13: "THE_PWM_OF_THE_SYSTEM_OUTPUT",
    0x14: "PARAMETER_SAVING_FLAG",
    0x15: "DEVICE_ADDRESS",
    0x16: "ABSOLUTE_POSITION_IS_16_BITS_LOWER",
    0x17: "ABSOLUTE_POSITION_IS_16_BITS_HIGHER",
    0x18: "STILL_MAXIMUM_ALLOWED_OUTPUT",
    0x19: "SPECIFIC_FUNCTION",
  }

  // =============================================================================
  // CONNECTION HANDLERS
  // =============================================================================
  
  const handleConnect = async () => {
    try {
      displayFeedback("Requesting port...", "info")
      // Web Serial API - prompts user to select a serial port
      const port = await navigator.serial.requestPort()
      displayFeedback("Port selected. Opening...", "info")
      await port.open({
        baudRate: 19200,
        dataBits: 8,
        stopBits: 1,
        parity: "none",
      })
      portRef.current = port
      writerRef.current = port.writable.getWriter()
      readerRef.current = port.readable.getReader()
      setIsConnected(true)
      displayFeedback("Connected to motor", "success")
    } catch (error) {
      displayFeedback(`Failed to connect: ${error}`, "error")
    }
  }

  const handleDisconnect = async () => {
    try {
      if (readerRef.current) {
        await readerRef.current.cancel()
        readerRef.current.releaseLock()
      }
      if (writerRef.current) {
        writerRef.current.releaseLock()
      }
      if (portRef.current) {
        await portRef.current.close()
      }
      portRef.current = null
      readerRef.current = null
      writerRef.current = null
      setIsConnected(false)
      setRegisterOutput([])
      displayFeedback("Disconnected from motor", "info")
    } catch (error) {
      displayFeedback(`Failed to disconnect: ${error}`, "error")
    }
  }

  const handleRead = async () => {
    try {
      displayFeedback("Reading registers...", "info")
      const output = []
      for (const [address, name] of Object.entries(registerMap)) {
        const value = await readRegister(parseInt(address))
        const isHighlighted = writtenRegistersRef.current.includes(
          parseInt(address)
        )
        output.push({
          name,
          address: parseInt(address),
          value: value !== null && value !== undefined ? value : "null",
          highlighted: isHighlighted,
        })
      }
      setRegisterOutput(output)
      const highlightCount = output.filter((r) => r.highlighted).length
      if (highlightCount > 0) {
        displayFeedback(
          `${highlightCount} value${highlightCount === 1 ? "" : "s"} updated`,
          "success"
        )
      } else {
        displayFeedback("Registers read successfully", "success")
      }
    } catch (error) {
      displayFeedback(`Failed to read registers: ${error}`, "error")
    }
  }

  /**
   * Write settings to motor
   * 
   * IMPORTANT WRITE SEQUENCE:
   * 1. Enable MODBUS mode (0x00 = 1) - allows register writes
   * 2. Disable motor output (0x01 = 0) - safety: motor won't move during config
   * 3. Write all settings 3 times (reliability workaround for serial comms)
   * 4. Re-enable motor output (0x01 = 1)
   * 5. Read back all registers to verify
   * 
   * The 0x14 register (PARAMETER_SAVING_FLAG) = 1 saves settings to EEPROM
   */
  const handleWrite = async () => {
    try {
      // Basic settings - always written
      const settings = [
        { register: 0x0b, value: formValues.steps_per_revolution }, // Steps/rev
        { register: 0x18, value: formValues.max_output },           // Max output
        { register: 0x05, value: formValues.speed_kp },             // Speed KP
        { register: 0x07, value: formValues.position_kp },          // Position KP
        { register: 0x09, value: formValues.direction_polarity ? 1 : 0 },
        { register: 0x14, value: 1 }, // Save to EEPROM
      ]

      // Add advanced settings only if the panel is open
      if (showAdvanced) {
        settings.unshift(
          { register: 0x14, value: formValues.parameter_saving_flag },
          { register: 0x19, value: formValues.specific_function },
          { register: 0x08, value: formValues.speed_feed },
          { register: 0x0a, value: formValues.electronic_gear_molecules },
          { register: 0x0e, value: formValues.alarm_code },
          { register: 0x15, value: formValues.device_address },
          { register: 0x00, value: formValues.modbus_enable },
          { register: 0x01, value: formValues.drive_output_enable },
          { register: 0x02, value: formValues.motor_target_speed },
          { register: 0x03, value: formValues.motor_acceleration },
          { register: 0x04, value: formValues.weak_magnetic_angle },
          { register: 0x06, value: formValues.speed_loop_integration_time }
        )
      }

      // Write 3 times for reliability (known workaround for serial communication issues)
      const repeatCount = 3
      setWriteStatus({
        message: `Writing settings ${repeatCount} times for reliability...`,
        faded: false,
      })
      writtenRegistersRef.current = settings.map((s) => s.register)

      // Step 1 & 2: Enable MODBUS mode and disable motor output for safety
      setWriteStatus({
        message: "Enabling Modbus and disabling motor power output",
        faded: false,
      })
      await writeRegister(0x00, 1) // MODBUS_ENABLE = 1
      await writeRegister(0x01, 0) // DRIVE_OUTPUT_ENABLE = 0 (motor disabled)

      // Step 3: Write all settings 3 times
      for (let i = 0; i < repeatCount; i++) {
        setWriteStatus({
          message: `Writing settings... Loop #${i + 1}/${repeatCount}`,
          faded: false,
        })
        for (const setting of settings) {
          await writeRegister(setting.register, setting.value)
        }
      }

      // Step 4: Re-enable motor output
      await writeRegister(0x01, 1) // DRIVE_OUTPUT_ENABLE = 1 (motor enabled)

      setWriteStatus({ message: "Write complete", faded: true })
      displayFeedback(
        "Settings written successfully. Refreshing register values...",
        "success"
      )

      // Step 5: Read back all registers to verify and display
      await handleRead()
    } catch (error) {
      setWriteStatus({ message: "", faded: true })
      displayFeedback(`Failed to write settings: ${error}`, "error")
    }
  }

  // =============================================================================
  // FORM COMPONENTS
  // =============================================================================
  
  const NumberInput = ({ label, regLabel, id, disabled = false, min, max }) => (
    <div className="flex flex-col">
      <label className="text-sm font-medium text-zinc-700 dark:text-zinc-300 mb-1">
        {label}
      </label>
      <span className="text-xs text-zinc-500 dark:text-zinc-500 mb-1">
        {regLabel}
        {disabled && (
          <span className="text-red-500 ml-1 text-xs">UNSUPPORTED</span>
        )}
      </span>
      <input
        type="number"
        value={formValues[id]}
        onChange={(e) => updateFormValue(id, parseInt(e.target.value) || 0)}
        disabled={disabled}
        min={min}
        max={max}
        className="px-3 py-2 rounded-lg border border-zinc-300 dark:border-zinc-600 bg-white dark:bg-zinc-800 text-zinc-900 dark:text-zinc-100 text-sm disabled:bg-zinc-100 dark:disabled:bg-zinc-900 disabled:text-zinc-500"
      />
    </div>
  )

  const ToggleSwitch = ({ label, regLabel, id }) => (
    <div className="flex flex-col">
      <label className="text-sm font-medium text-zinc-700 dark:text-zinc-300 mb-1">
        {label}
      </label>
      <span className="text-xs text-zinc-500 dark:text-zinc-500 mb-1">
        {regLabel}
      </span>
      <button
        type="button"
        onClick={() => updateFormValue(id, !formValues[id])}
        className={`relative w-14 h-7 rounded-full transition-colors ${
          formValues[id]
            ? "bg-violet-500"
            : "bg-zinc-300 dark:bg-zinc-600"
        }`}
      >
        <span
          className={`absolute top-1 w-5 h-5 rounded-full bg-white shadow transition-transform ${
            formValues[id] ? "translate-x-8" : "translate-x-1"
          }`}
        />
      </button>
    </div>
  )

  return (
    <div className="p-4 border dark:border-zinc-700 rounded-xl not-prose bg-zinc-50 dark:bg-zinc-900">
      <div className="max-w-2xl mx-auto">
        {/* Title */}
        <div className="text-center py-3 mb-4 bg-zinc-200 dark:bg-zinc-800 rounded-lg">
          <h2 className="text-lg font-semibold text-zinc-800 dark:text-zinc-200">
            Gold Motor Control
          </h2>
        </div>

        {/* Connection Instructions */}
        {!isConnected && (
          <div className="bg-zinc-100 dark:bg-zinc-800 border border-zinc-300 dark:border-zinc-600 rounded-lg p-4 mb-4">
            <p className="font-semibold text-zinc-800 dark:text-zinc-200 mb-2">
              Connection Instructions:
            </p>
            <p className="text-sm text-zinc-600 dark:text-zinc-400 mb-2">
              Connect the motor using an{" "}
              <strong>RS485 USB adapter</strong> that provides 5V+, GND, A, B.{" "}
              <a
                href="https://www.amazon.com/CERRXIAN-Terminal-Converter-Serial-Windows/dp/B09JMT9D59"
                target="_blank"
                rel="noopener noreferrer"
                className="text-violet-600 dark:text-violet-400 underline"
              >
                Example
              </a>
            </p>
            <p className="text-sm text-zinc-600 dark:text-zinc-400 mb-2">
              The motor must have <strong>20-36V DC</strong> supplied, which can
              come from the OSSM Reference Board or external power source.
            </p>
            <p className="text-sm text-red-600 dark:text-red-400 mt-3">
              <strong>Important:</strong> The OSSM Reference Board 4-Pin Signal
              Plug must be <strong>disconnected</strong> while programming.
            </p>
          </div>
        )}

        {/* Connect/Disconnect Buttons */}
        <div className="flex justify-center gap-4 mb-4">
          {!isConnected ? (
            <button
              onClick={handleConnect}
              className="px-6 py-2 bg-violet-500 hover:bg-violet-600 text-white font-medium rounded-full transition-colors"
            >
              Connect to Motor
            </button>
          ) : (
            <button
              onClick={handleDisconnect}
              className="px-6 py-2 bg-zinc-500 hover:bg-zinc-600 text-white font-medium rounded-full transition-colors"
            >
              Disconnect
            </button>
          )}
        </div>

        {/* Controls (visible when connected) */}
        {isConnected && (
          <div>
            <h3 className="text-md font-semibold text-zinc-800 dark:text-zinc-200 mb-3">
              Motor Settings
            </h3>

            <button
              onClick={resetDefaults}
              className="mb-4 px-4 py-1.5 text-sm bg-zinc-200 dark:bg-zinc-700 hover:bg-zinc-300 dark:hover:bg-zinc-600 text-zinc-700 dark:text-zinc-300 rounded-lg transition-colors"
            >
              Reset fields to OSSM Standard
            </button>

            {/* Basic Fields */}
            <div className="grid grid-cols-2 gap-4 mb-4">
              <NumberInput
                label="Electronic Gear Denominator (Steps per Revolution)"
                regLabel="0x0B"
                id="steps_per_revolution"
              />
              <NumberInput
                label="Still Maximum Allowed Output"
                regLabel="0x18"
                id="max_output"
              />
              <NumberInput
                label="Speed Ring Proportional Ratio Coefficient (KP)"
                regLabel="0x05"
                id="speed_kp"
              />
              <NumberInput
                label="Position Ring Proportional Coefficient (KP)"
                regLabel="0x07"
                id="position_kp"
              />
              <ToggleSwitch
                label="Direction Polarity"
                regLabel="0x09"
                id="direction_polarity"
              />
              <ToggleSwitch
                label="Hack the planet?"
                regLabel=""
                id="hack_the_planet"
              />
            </div>

            {/* Advanced Settings Toggle */}
            <button
              onClick={() => setShowAdvanced(!showAdvanced)}
              className="mb-4 px-4 py-1.5 text-sm bg-zinc-200 dark:bg-zinc-700 hover:bg-zinc-300 dark:hover:bg-zinc-600 text-zinc-700 dark:text-zinc-300 rounded-lg transition-colors"
            >
              {showAdvanced ? "Hide" : "Show"} Advanced Settings
            </button>

            {/* Advanced Disclaimer */}
            {showAdvanced && (
              <div className="bg-red-50 dark:bg-red-900/20 border border-red-300 dark:border-red-700 rounded-lg p-4 mb-4">
                <p className="text-red-700 dark:text-red-400 font-semibold">
                  Warning:
                </p>
                <p className="text-sm text-red-600 dark:text-red-400">
                  Advanced settings are experimental. Changing these values may
                  make your motor unusable until settings are restored.
                </p>
                <p className="text-sm text-zinc-700 dark:text-zinc-300 mt-1">
                  Please save your current settings somewhere safe before making
                  changes.
                </p>
              </div>
            )}

            {/* Advanced Fields */}
            {showAdvanced && (
              <div className="grid grid-cols-2 gap-4 mb-4">
                <NumberInput
                  label="MODBUS Enable"
                  regLabel="0x00"
                  id="modbus_enable"
                  min={0}
                  max={1}
                />
                <NumberInput
                  label="Drive Output Enable"
                  regLabel="0x01"
                  id="drive_output_enable"
                  min={0}
                  max={7}
                />
                <NumberInput
                  label="Motor Target Speed"
                  regLabel="0x02"
                  id="motor_target_speed"
                />
                <NumberInput
                  label="Motor Acceleration"
                  regLabel="0x03"
                  id="motor_acceleration"
                />
                <NumberInput
                  label="Weak Magnetic Angle"
                  regLabel="0x04"
                  id="weak_magnetic_angle"
                />
                <NumberInput
                  label="Speed Loop Integration Time"
                  regLabel="0x06"
                  id="speed_loop_integration_time"
                />
                <NumberInput
                  label="Speed Feed"
                  regLabel="0x08"
                  id="speed_feed"
                />
                <NumberInput
                  label="Electronic Gear Molecules"
                  regLabel="0x0A"
                  id="electronic_gear_molecules"
                />
                <NumberInput
                  label="Target Location is 16 bits Lower"
                  regLabel="0x0C"
                  id="target_location_low"
                  disabled
                />
                <NumberInput
                  label="Target Position is 16 bits High"
                  regLabel="0x0D"
                  id="target_location_high"
                  disabled
                />
                <NumberInput
                  label="Alarm Code"
                  regLabel="0x0E"
                  id="alarm_code"
                />
                <NumberInput
                  label="System Current"
                  regLabel="0x0F"
                  id="system_current"
                  disabled
                />
                <NumberInput
                  label="Motor Current Speed"
                  regLabel="0x10"
                  id="motor_current_speed"
                  disabled
                />
                <NumberInput
                  label="System Voltage"
                  regLabel="0x11"
                  id="system_voltage"
                  disabled
                />
                <NumberInput
                  label="System Temperature"
                  regLabel="0x12"
                  id="system_temperature"
                  disabled
                />
                <NumberInput
                  label="The PWM of the System Output"
                  regLabel="0x13"
                  id="system_pwm_output"
                  disabled
                />
                <NumberInput
                  label="Parameter Saving Flag"
                  regLabel="0x14"
                  id="parameter_saving_flag"
                />
                <NumberInput
                  label="Device Address"
                  regLabel="0x15"
                  id="device_address"
                />
                <NumberInput
                  label="Absolute Position is 16 bits Lower"
                  regLabel="0x16"
                  id="absolute_position_low"
                  disabled
                />
                <NumberInput
                  label="Absolute Position is 16 bits Higher"
                  regLabel="0x17"
                  id="absolute_position_high"
                  disabled
                />
                <NumberInput
                  label="Specific Function"
                  regLabel="0x19"
                  id="specific_function"
                />
              </div>
            )}

            {/* Write/Read Buttons */}
            <div className="flex justify-center gap-4 mb-4">
              <button
                onClick={handleWrite}
                className="px-6 py-2 bg-violet-500 hover:bg-violet-600 text-white font-medium rounded-full transition-colors"
              >
                Write Settings
              </button>
              <button
                onClick={handleRead}
                className="px-6 py-2 bg-zinc-500 hover:bg-zinc-600 text-white font-medium rounded-full transition-colors"
              >
                Read Registers
              </button>
            </div>

            {/* Register Values */}
            {registerOutput.length > 0 && (
              <div className="mb-4">
                <h3 className="text-md font-semibold text-zinc-800 dark:text-zinc-200 mb-2">
                  Register Values
                </h3>
                <div className="bg-zinc-900 dark:bg-black rounded-lg p-3 max-h-48 overflow-auto font-mono text-xs">
                  {registerOutput.map((reg, idx) => (
                    <div
                      key={idx}
                      className={
                        reg.highlighted
                          ? "text-green-400"
                          : "text-zinc-300"
                      }
                    >
                      {reg.name} (0x{reg.address.toString(16)}): {reg.value}
                    </div>
                  ))}
                </div>
              </div>
            )}
          </div>
        )}

        {/* Write Status */}
        {writeStatus.message && (
          <div
            className={`text-sm mb-2 ${
              writeStatus.faded
                ? "text-zinc-500"
                : "text-zinc-700 dark:text-zinc-300"
            }`}
          >
            {writeStatus.message}
          </div>
        )}

        {/* Feedback */}
        {feedback.message && (
          <div
            className={`p-3 rounded-lg text-sm ${
              feedback.type === "success"
                ? "bg-green-100 dark:bg-green-900/30 text-green-800 dark:text-green-300 border border-green-300 dark:border-green-700"
                : feedback.type === "error"
                ? "bg-red-100 dark:bg-red-900/30 text-red-800 dark:text-red-300 border border-red-300 dark:border-red-700"
                : "bg-zinc-100 dark:bg-zinc-800 text-zinc-700 dark:text-zinc-300 border border-zinc-300 dark:border-zinc-600"
            }`}
          >
            {feedback.message}
          </div>
        )}

        {/* Footer */}
        <div className="text-center text-zinc-500 dark:text-zinc-500 text-sm mt-6">
          Gold Motor Programming Utility
          <br />
          <strong>v2.0 alpha</strong>
        </div>
      </div>
    </div>
  )
}
