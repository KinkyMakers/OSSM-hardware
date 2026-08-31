#!/usr/bin/env python3
"""Build and validate the merged OSSM browser-flashing image."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

FLASH_CAPACITY = 16 * 1024 * 1024
COMPONENT_OFFSETS = {
    "bootloader.bin": 0x1000,
    "partitions.bin": 0x8000,
    "boot_app0.bin": 0xE000,
    "firmware.bin": 0x10000,
}
ESP32_CHIP_ID = 0
FLASH_MODE_ID = 2
FLASH_SIZE_ID = 4
FLASH_FREQUENCY_ID = 0
FLASH_PROFILES = {4: (4 * 1024 * 1024, 2), 16: (FLASH_CAPACITY, FLASH_SIZE_ID)}


def require_image(path: Path, label: str) -> Path:
    if not path.is_file() or path.stat().st_size == 0:
        raise RuntimeError(f"{label} image is missing or empty: {path}")
    return path


def find_platformio_package_file(
    package: str, relative_path: str, label: str
) -> Path:
    platformio_home = Path(
        os.environ.get("PLATFORMIO_HOME_DIR", Path.home() / ".platformio")
    )
    active_package = platformio_home / "packages" / package / relative_path
    if active_package.is_file():
        return require_image(active_package, label)

    candidates = sorted(platformio_home.glob(f"packages/{package}@*/{relative_path}"))
    if len(candidates) != 1:
        raise RuntimeError(
            f"expected one PlatformIO {label} file, found {len(candidates)}"
        )
    return require_image(candidates[0], label)


def find_boot_app0() -> Path:
    return find_platformio_package_file(
        "framework-arduinoespressif32",
        "tools/partitions/boot_app0.bin",
        "boot_app0",
    )


def find_esptool() -> Path:
    return find_platformio_package_file("tool-esptoolpy", "esptool.py", "esptool")


def validate_esp_image(path: Path, label: str) -> None:
    content = require_image(path, label).read_bytes()
    if len(content) < 14 or content[0] != 0xE9:
        raise RuntimeError(f"{label} does not contain an ESP image header: {path}")
    chip_id = int.from_bytes(content[12:14], "little")
    if chip_id != ESP32_CHIP_ID:
        raise RuntimeError(
            f"{label} targets chip ID {chip_id}, expected {ESP32_CHIP_ID}"
        )


def build_components(
    build_dir: Path, boot_app0: Path, flash_size_mib: int = 16
) -> list[tuple[int, Path]]:
    flash_capacity, expected_size_id = FLASH_PROFILES[flash_size_mib]
    bootloader = require_image(build_dir / "bootloader.bin", "bootloader")
    application = require_image(build_dir / "firmware.bin", "application")
    validate_esp_image(bootloader, "bootloader")
    validate_esp_image(application, "application")

    bootloader_header = bootloader.read_bytes()[:4]
    flash_mode_id = bootloader_header[2]
    flash_size_id = bootloader_header[3] >> 4
    flash_frequency_id = bootloader_header[3] & 0xF
    if (
        flash_mode_id != FLASH_MODE_ID
        or flash_size_id != expected_size_id
        or flash_frequency_id != FLASH_FREQUENCY_ID
    ):
        raise RuntimeError(
            f"bootloader flash header does not match OSSM's {flash_size_mib} MiB profile: "
            f"found mode/size/frequency IDs {flash_mode_id}/{flash_size_id}/"
            f"{flash_frequency_id}, expected {FLASH_MODE_ID}/{expected_size_id}/"
            f"{FLASH_FREQUENCY_ID}"
        )
    if application.read_bytes()[2:4] != bootloader_header[2:4]:
        raise RuntimeError("application flash header does not match the bootloader profile")

    components = [
        (
            COMPONENT_OFFSETS["bootloader.bin"],
            bootloader,
        ),
        (
            COMPONENT_OFFSETS["partitions.bin"],
            require_image(build_dir / "partitions.bin", "partition table"),
        ),
        (
            COMPONENT_OFFSETS["boot_app0.bin"],
            require_image(boot_app0, "boot_app0"),
        ),
        (
            COMPONENT_OFFSETS["firmware.bin"],
            application,
        ),
    ]
    for index, (offset, path) in enumerate(components):
        end = offset + path.stat().st_size
        if end > flash_capacity:
            raise RuntimeError(f"{path} exceeds the physical flash capacity")
        if index + 1 < len(components) and end > components[index + 1][0]:
            raise RuntimeError(f"{path} overlaps the next flash component")
    return components


def merge_command(
    esptool: Path, output: Path, components: list[tuple[int, Path]], flash_size_mib: int = 16
) -> list[str]:
    command = [
        sys.executable,
        str(esptool),
        "--chip",
        "esp32",
        "merge_bin",
        "--output",
        str(output),
        "--flash_mode",
        "keep",
        "--flash_freq",
        "keep",
        "--flash_size",
        f"{flash_size_mib}MB",
    ]
    for offset, path in components:
        command.extend((hex(offset), str(path)))
    return command


def validate_merged_image(
    output: Path, components: list[tuple[int, Path]], flash_size_mib: int = 16
) -> None:
    flash_capacity, flash_size_id = FLASH_PROFILES[flash_size_mib]
    content = require_image(output, "merged web installer").read_bytes()
    if len(content) > flash_capacity:
        raise RuntimeError(
            f"merged image is {len(content)} bytes, beyond {flash_size_mib} MiB flash"
        )
    for offset in (COMPONENT_OFFSETS["bootloader.bin"], 0x10000):
        if offset >= len(content) or content[offset] != 0xE9:
            raise RuntimeError(f"ESP image magic is missing at {hex(offset)}")
        chip_id = int.from_bytes(content[offset + 12 : offset + 14], "little")
        if chip_id != ESP32_CHIP_ID:
            raise RuntimeError(
                f"ESP chip ID {chip_id} at {hex(offset)} does not match "
                f"ESP32 ({ESP32_CHIP_ID})"
            )

    header = content[
        COMPONENT_OFFSETS["bootloader.bin"] : COMPONENT_OFFSETS["bootloader.bin"] + 4
    ]
    if (
        header[2] != FLASH_MODE_ID
        or header[3] >> 4 != flash_size_id
        or header[3] & 0xF != FLASH_FREQUENCY_ID
    ):
        raise RuntimeError("merged bootloader flash header does not match OSSM's profile")
    if content[0x10002:0x10004] != header[2:4]:
        raise RuntimeError("merged application flash header does not match OSSM's profile")

    for offset, source_path in components:
        source = source_path.read_bytes()
        merged = content[offset : offset + len(source)]
        if offset == COMPONENT_OFFSETS["bootloader.bin"]:
            # esptool may rewrite flash mode/frequency/size bytes 2-3.
            matches = merged[:2] == source[:2] and merged[4:] == source[4:]
        else:
            matches = merged == source
        if not matches:
            raise RuntimeError(f"merged component mismatch at {hex(offset)}")


def build(
    build_dir: Path, output: Path, boot_app0: Path | None = None, flash_size_mib: int = 16
) -> None:
    components = build_components(build_dir, boot_app0 or find_boot_app0(), flash_size_mib)
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(merge_command(find_esptool(), output, components, flash_size_mib), check=True)
    validate_merged_image(output, components, flash_size_mib)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--boot-app0", type=Path)
    parser.add_argument("--flash-size", choices=("4MB", "16MB"), default="16MB")
    args = parser.parse_args()
    try:
        build(args.build_dir, args.output, args.boot_app0, int(args.flash_size[:-2]))
        print(f"Built validated web installer: {args.output}")
        return 0
    except Exception as error:
        print(f"Web installer build failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
