Import("env")
import os


# Function to check if SW_VERSION is already in CPPDEFINES
def get_build_flag_value(flag_name):
    # Check if BUILD_FLAGS is defined in the environment
    try:
        build_flags = env.ParseFlags(env['BUILD_FLAGS'])
        flags_with_value_list = [build_flag for build_flag in build_flags.get('CPPDEFINES') if type(build_flag) == list]
        defines = {k: v for (k, v) in flags_with_value_list}
        value = defines.get(flag_name)

        print(f"Found Bluid Flag {flag_name} = {value}")
        return value
    except KeyError:
        return None

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

    env.Append(CPPDEFINES=('SW_VERSION', swVersion));
    print(f"SW_VERSION is now {swVersion}")
else:
    print(f"SW_VERSION already defined as ${swVersion}");

print(f"SW_VERSION is {swVersion}")