import serial.tools.list_ports
from pymodbus.client import ModbusSerialClient
from pymodbus.exceptions import ModbusException

# Dictionary mapping register addresses to their human-readable names
REGISTER_MAP = {
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
    0x19: "SPECIFIC_FUNCTION",
    # Add more register mappings as necessary
}

def list_serial_ports():
    """Lists available serial ports and returns them."""
    ports = list(serial.tools.list_ports.comports())
    return ports

def choose_serial_port(ports):
    """Prompts the user to choose a serial port from the list."""
    if not ports:
        print("No serial ports found.")
        return None

    print("\nAvailable serial ports:")
    for index, port in enumerate(ports):
        print(f"{index + 1}: {port.device} - {port.description}")

    if len(ports) == 1:
        print("\nOnly one serial port found, selecting automatically.")
        return ports[0].device

    try:
        choice = int(input("Enter the number of the port to use: "))
        selected_port = ports[choice - 1].device
        print(f"Selected Port: {selected_port}")
        return selected_port
    except (IndexError, ValueError):
        print("Invalid selection. Please enter a number from the list.")
        return None

def read_registers(client):
    """Reads a range of registers and prints their values with human-readable names."""
    # Find the longest name for formatting
    longest_name_length = max(len(name) for name in REGISTER_MAP.values())
    
    for address, name in REGISTER_MAP.items():
        try:
            result = client.read_holding_registers(address, count=1, unit=1)
            if not result.isError():
                # ANSI escape sequences for colors
                color_green = '\033[92m'
                color_reset = '\033[0m'
                # String formatting for aligned output
                print(f"{color_green}{name.ljust(longest_name_length)} (Register {hex(address)}): {result.registers[0]}{color_reset}")
            else:
                color_red = '\033[91m'
                print(f"{color_red}Error reading {name.ljust(longest_name_length)} (Register {hex(address)}){color_reset}")
        except ModbusException as e:
            color_red = '\033[91m'
            print(f"{color_red}Error reading {name.ljust(longest_name_length)} (Register {hex(address)}): {e}{color_reset}")



def write_single_register(client, address, value):
    try:
        response = client.write_register(address, value, unit=1)
        if not response.isError():
            print(f"Successfully wrote {value} to register {hex(address)}")
        else:
            pass

    except (ModbusException) as e:
        pass



def main():
    ports = list_serial_ports()
    if ports:
        selected_port = choose_serial_port(ports)
        if selected_port:
            client = ModbusSerialClient(method='rtu', port=selected_port, baudrate=19200, timeout=1, parity='N', stopbits=1, bytesize=8)
            if client.connect():
                # Write steps per revolution value
                write_single_register(client, 0x0B, 800)    # Steps per revolution
                write_single_register(client, 0x18, 600)    # Maximum allowed output
                write_single_register(client, 0x05, 3000)   # Speed ring proportional ratio coefficient (KP)
                write_single_register(client, 0x07, 3000)   # Position ring proportional coefficient (KP)
                write_single_register(client, 0x09, 1)      # Direction polarity, 1 is default
                #Save the parameters to the EEPROM
                write_single_register(client, 0x14, 1)
                # You can read the register afterward to confirm the write operation
                read_registers(client)
                client.close()
            else:
                print("Failed to connect to the selected port.")
        else:
            print("No port selected. Exiting.")
    else:
        print("No serial devices detected. Exiting.")

if __name__ == "__main__":
    main()