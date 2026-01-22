Import("env")
import os
import re
import socket

def get_local_ip():
    try:
        # Create a socket connection to an external server (doesn't actually connect)
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        return local_ip
    except Exception as e:
        print(f"Error getting local IP: {e}")
        return None

# Function to check if SW_VERSION is already in CPPDEFINES
def get_build_flag_value(flag_name):
    # Check if BUILD_FLAGS is defined in the environment
    try:
        build_flags = env.ParseFlags(env['BUILD_FLAGS'])
        all_defines = build_flags.get('CPPDEFINES', [])

        for define in all_defines:
            # Handle both list and tuple types (PlatformIO can use either)
            if isinstance(define, (list, tuple)) and len(define) >= 2 and define[0] == flag_name:
                print(f"Found Build Flag {flag_name} = {define[1]}")
                return define[1]

        print(f"Found Build Flag {flag_name} = None")
        return None
    except KeyError:
        return None

# Function to check if a flag is defined (with or without value)
def is_build_flag_defined(flag_name):
    try:
        build_flags = env.ParseFlags(env['BUILD_FLAGS'])
        all_defines = build_flags.get('CPPDEFINES', [])
        for define in all_defines:
            if isinstance(define, list) and define[0] == flag_name:
                return True
            elif define == flag_name:
                return True
        return False
    except KeyError:
        return False

# Helper to remove a -D flag from BUILD_FLAGS
def remove_build_flag_define(flag_name):
    """Remove a -D FLAG=value or -D FLAG from BUILD_FLAGS"""
    build_flags = env.get('BUILD_FLAGS', [])
    new_flags = []
    for flag in build_flags:
        if isinstance(flag, str):
            # Match -D FLAG=... or -DFLAG=... patterns
            if not re.match(rf'^-D\s*{flag_name}(=|$)', flag):
                new_flags.append(flag)
            else:
                print(f"Removing build flag: {flag}")
        else:
            new_flags.append(flag)
    env.Replace(BUILD_FLAGS=new_flags)

# Replace "localhost" with the local IP address in all BUILD_FLAGS
def replace_localhost_in_flags():
    local_ip = get_local_ip()
    if not local_ip:
        print("No local IP found, skipping localhost replacement")
        return

    build_flags = env.get('BUILD_FLAGS', [])
    new_flags = []
    replacements = []

    for flag in build_flags:
        if isinstance(flag, str) and 'localhost' in flag:
            new_flag = flag.replace('localhost', local_ip)
            replacements.append((flag, new_flag))
            new_flags.append(new_flag)
        else:
            new_flags.append(flag)

    if replacements:
        print(f"\nLocalhost -> {local_ip}")
        print(f"{'Key':<20}{'Old':<25}{'New'}")
        print("-" * 65)
        for orig, repl in replacements:
            # Parse key and values from "-D KEY=VALUE" format
            if '=' in orig:
                key = orig.split('=')[0].replace('-D ', '').strip()
                old_val = orig.split('=', 1)[1].strip('"')
                new_val = repl.split('=', 1)[1].strip('"')
            else:
                key, old_val, new_val = orig, orig, repl
            print(f"{key:<20}{old_val:<25}{new_val}")
        print()

    env.Replace(BUILD_FLAGS=new_flags)


# Handle SW_VERSION
swVersion = get_build_flag_value('SW_VERSION')

if not swVersion:
    # SW_VERSION not already defined, check environment variable or GITHUB_ENV
    swVersion = os.getenv('SW_VERSION')
    if not swVersion:
        env_file = os.getenv('GITHUB_ENV')
        if env_file:
            with open(env_file, 'r') as f:
                for line in f:
                    if line.startswith('SW_VERSION='):
                        swVersion = line.split('=')[1].strip()
                        print(f"Found SW_VERSION in {env_file} = {swVersion}")
                        break

    env.Append(CPPDEFINES=('SW_VERSION', swVersion))
    print(f"SW_VERSION is now {swVersion}")
else:
    print(f"SW_VERSION already defined as {swVersion}")

print(f"SW_VERSION is {swVersion}")


# Replace localhost with local IP (only for VERSIONDEV builds)
if is_build_flag_defined('VERSIONDEV'):
    print("VERSIONDEV is defined")

    # Print detected local IP
    local_ip = get_local_ip()
    print(f"Detected local IP: {local_ip}")

    # Replace localhost with local IP in all flags
    replace_localhost_in_flags()

    # Print final BUILD_FLAGS to verify
    print(f"Final BUILD_FLAGS: {env['BUILD_FLAGS']}")
else:
    print("VERSIONDEV is not defined")
