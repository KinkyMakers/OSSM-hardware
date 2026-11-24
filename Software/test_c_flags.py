Import("env")

if not any(
    isinstance(flag, str) and flag.startswith("-std=")
    for flag in env.get("CFLAGS", [])
):
    env.Append(CFLAGS=["-std=gnu17"])
