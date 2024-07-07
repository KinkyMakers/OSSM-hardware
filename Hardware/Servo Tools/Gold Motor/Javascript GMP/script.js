let port;
let reader;
let writer;
let writtenRegisters = [];

document.getElementById('connect').addEventListener('click', async () => {
    try {
        displayFeedback('Requesting port...', 'is-info');
        port = await navigator.serial.requestPort();
        displayFeedback(`Port selected. Opening...`, 'is-info');
        await port.open({ baudRate: 19200, dataBits: 8, stopBits: 1, parity: 'none' });
        writer = port.writable.getWriter();
        reader = port.readable.getReader();
        
        document.getElementById('connect').style.display = 'none';
        document.getElementById('disconnect').style.display = 'block';
        document.getElementById('read').style.display = 'block';
        document.getElementById('write').style.display = 'block';
        document.getElementById('controls').style.display = 'block';
        displayFeedback('Connected to motor', 'is-success');
    } catch (error) {
        displayFeedback(`Failed to connect: ${error}`, 'is-danger');
    }
});

document.getElementById('disconnect').addEventListener('click', async () => {
    try {
        if (reader) {
            await reader.cancel();
            reader.releaseLock();
        }
        if (writer) {
            writer.releaseLock();
        }
        if (port) {
            await port.close();
        }
        document.getElementById('connect').style.display = 'block';
        document.getElementById('disconnect').style.display = 'none';
        document.getElementById('read').style.display = 'none';
        document.getElementById('write').style.display = 'none';
        document.getElementById('controls').style.display = 'none';
        displayFeedback('Disconnected from motor', 'is-info');
    } catch (error) {
        displayFeedback(`Failed to disconnect: ${error}`, 'is-danger');
    }
});

document.getElementById('read').addEventListener('click', async () => {
    try {
        displayFeedback('Reading registers...', 'is-info');
        await readRegisters();
        displayFeedback('Registers read successfully', 'is-success');
    } catch (error) {
        displayFeedback(`Failed to read registers: ${error}`, 'is-danger');
    }
});

document.getElementById('write').addEventListener('click', async () => {
    try {
        const settings = [
            { register: 0x0B, value: parseInt(document.getElementById('steps_per_revolution').value) },
            { register: 0x18, value: parseInt(document.getElementById('max_output').value) },
            { register: 0x05, value: parseInt(document.getElementById('speed_kp').value) },
            { register: 0x07, value: parseInt(document.getElementById('position_kp').value) },
            { register: 0x09, value: document.getElementById('direction_polarity').checked ? 1 : 0 },
            { register: 0x14, value: 1 }  // Save parameters to EEPROM
        ];
        displayFeedback('Writing settings...', 'is-info');
        writtenRegisters = settings.map(setting => setting.register);
        for (const setting of settings) {
            await writeRegister(setting.register, setting.value);
        }
        displayFeedback('Settings written successfully. Refreshing register values...', 'is-success');
        await readRegisters();
    } catch (error) {
        displayFeedback(`Failed to write settings: ${error}`, 'is-danger');
    }
});

async function readRegisters() {
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
        0x0A: "ELECTRONIC_GEAR_MOLECULES",
        0x0B: "ELECTRONIC_GEAR_DENOMINATOR",
        0x0C: "TARGET_LOCATION_IS_16_BITS_LOWER",
        0x0D: "TARGET_POSITION_IS_16_BITS_HIGH",
        0x0E: "ALARM_CODE",
        0x0F: "SYSTEM_CURRENT",
        0x10: "MOTOR_CURRENT_SPEED",
        0x11: "SYSTEM_VOLTAGE",
        0x12: "SYSTEM_TEMPERATURE",
        0x13: "THE_PWM_OF_THE_SYSTEM_OUTPUT",
        0x14: "PARAMETER_SAVING_FLAG",
        0x15: "DEVICE_ADDRESS",
        0x16: "ABSOLUTE_POSITION_IS_16_BITS_LOWER",
        0x17: "ABSOLUTE_POSITION_IS_16_BITS_HIGHER",
        0x18: "STILL_MAXIMUM_ALLOWED_OUTPUT",
        0x19: "SPECIFIC_FUNCTION"
    };
    let output = '';
    for (const [address, name] of Object.entries(registerMap)) {
        const value = await readRegister(parseInt(address));
        const highlightClass = writtenRegisters.includes(parseInt(address)) ? 'highlight' : '';
        if (value !== null && value !== undefined) {
            output += `<span class="${highlightClass}">${name} (0x${address.toString(16)}): ${value}</span>\n`;
        } else {
            output += `<span class="${highlightClass}">${name} (0x${address.toString(16)}): null</span>\n`;
        }
    }
    document.getElementById('register_values').innerHTML = output;
}

async function writeRegister(register, value) {
    if (writer && reader) {
        const command = createModbusCommand(register, value, false);
        await writer.write(command);
        displayFeedback(`Sent write command for register 0x${register.toString(16)} with value ${value}`, 'is-info');
        try {
            let responseBuffer = new Uint8Array();
            while (true) {
                const { value, done } = await reader.read();
                if (done) {
                    break;
                }
                responseBuffer = concatenate(responseBuffer, new Uint8Array(value));
                // Check if we have received enough data for a complete response
                if (responseBuffer.length >= 8) {
                    break;
                }
            }
            if (responseBuffer.length >= 8) {
                const success = parseModbusWriteResponse(responseBuffer);
                if (success) {
                    displayFeedback(`Write successful for register 0x${register.toString(16)} with value ${value}`, 'is-success');
                } else {
                    displayFeedback(`Write failed for register 0x${register.toString(16)} with value ${value}`, 'is-danger');
                }
            } else {
                displayFeedback(`Incomplete write response for register 0x${register.toString(16)}`, 'is-danger');
            }
        } catch (error) {
            displayFeedback(`Error writing data for register 0x${register.toString(16)}: ${error}`, 'is-danger');
        }
    } else {
        displayFeedback('No connection to motor', 'is-danger');
    }
}

async function readRegister(register) {
    if (writer && reader) {
        const command = createModbusCommand(register, 1, true); // Request one register
        await writer.write(command);
        displayFeedback(`Sent read command for register 0x${register.toString(16)}`, 'is-info');
        try {
            let responseBuffer = new Uint8Array();
            while (true) {
                const { value, done } = await reader.read();
                if (done) {
                    break;
                }
                responseBuffer = concatenate(responseBuffer, new Uint8Array(value));
                // Check if we have received enough data for a complete response
                if (responseBuffer.length >= 7) {
                    break;
                }
            }
            if (responseBuffer.length >= 7) {
                const value = parseModbusResponse(responseBuffer);
                displayFeedback(`Received data for register 0x${register.toString(16)}: ${value}`, 'is-info');
                return value;
            } else {
                displayFeedback(`Incomplete data for register 0x${register.toString(16)}`, 'is-danger');
                return null;
            }
        } catch (error) {
            displayFeedback(`Error reading data for register 0x${register.toString(16)}: ${error}`, 'is-danger');
            return null;
        }
    } else {
        displayFeedback('No connection to motor', 'is-danger');
        return null;
    }
}

function createModbusCommand(register, value, read = false) {
    const buffer = new ArrayBuffer(8);
    const view = new DataView(buffer);
    view.setUint8(0, 0x01);  // Slave address
    view.setUint8(1, read ? 0x03 : 0x06);  // Function code (0x03 for read, 0x06 for write)
    view.setUint16(2, register, false); // Register address
    view.setUint16(4, value, false); // Read length or write value

    const crc = calculateCRC(new Uint8Array(buffer.slice(0, 6)));
    view.setUint16(6, crc, false); // Add CRC to the buffer

    return new Uint8Array(buffer);
}

function calculateCRC(buffer) {
    let crc = 0xFFFF;

    for (let pos = 0; pos < buffer.length; pos++) {
        crc ^= buffer[pos]; // XOR byte into least sig. byte of crc

        for (let i = 8; i !== 0; i--) { // Loop over each bit
            if ((crc & 0x0001) !== 0) { // If the LSB is set
                crc >>= 1; // Shift right and XOR 0xA001
                crc ^= 0xA001;
            } else // Else LSB is not set
                crc >>= 1; // Just shift right
        }
    }

    // Swap the bytes
    crc = ((crc & 0xFF00) >> 8) | ((crc & 0x00FF) << 8);

    return crc;
}

function parseModbusResponse(buffer) {
    const view = new DataView(buffer.buffer);
    const functionCode = view.getUint8(1);
    if (functionCode & 0x80) {
        // Error response
        const exceptionCode = view.getUint8(2);
        throw new Error(`Modbus exception code ${exceptionCode}`);
    }
    const byteCount = view.getUint8(2);
    const data = [];
    for (let i = 0; i < byteCount / 2; i++) {
        data.push(view.getUint16(3 + i * 2, false));
    }
    return data.length === 1 ? data[0] : data;
}

function parseModbusWriteResponse(buffer) {
    const view = new DataView(buffer.buffer);
    const functionCode = view.getUint8(1);
    if (functionCode === 0x06) {
        // Successful write response
        return true;
    } else if (functionCode & 0x80) {
        // Error response
        const exceptionCode = view.getUint8(2);
        throw new Error(`Modbus exception code ${exceptionCode}`);
    }
    return false;
}

function concatenate(buffer1, buffer2) {
    const tmp = new Uint8Array(buffer1.length + buffer2.length);
    tmp.set(buffer1, 0);
    tmp.set(buffer2, buffer1.length);
    return tmp;
}

function displayFeedback(message, colorClass) {
    const feedbackElement = document.getElementById('feedback');
    feedbackElement.textContent = message;
    feedbackElement.className = `notification ${colorClass}`;
    feedbackElement.style.display = 'block';
}
