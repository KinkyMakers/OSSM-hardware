Import("env")

import os

env.Append(
    CPPDEFINES=[
        (
            "FIRMWARE_BUILD_SHA",
            env.StringifyMacro(
                os.getenv("RELEASE_BUILD_SHA", os.getenv("GITHUB_SHA", "unknown"))
            ),
        )
    ]
)
