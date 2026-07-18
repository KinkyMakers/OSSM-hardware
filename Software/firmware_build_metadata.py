Import("env")

import os

env.Append(
    CPPDEFINES=[
        ("FIRMWARE_BUILD_SHA", env.StringifyMacro(os.getenv("GITHUB_SHA", "unknown")))
    ]
)
