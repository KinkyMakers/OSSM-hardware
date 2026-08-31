#!/usr/bin/env python3
"""Build and validate OSSM browser installers for the existing 4/16 MiB layouts."""

from __future__ import annotations

import argparse
import hashlib
import os
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

OTA_SAFETY_MARGIN = 0x4000
COMPONENT_OFFSETS = {
    "bootloader.bin": 0x1000,
    "partitions.bin": 0x8000,
    "boot_app0.bin": 0xE000,
    "firmware.bin": 0x10000,
}
ESP32_CHIP_ID = 0
FLASH_MODE_ID = 2
FLASH_FREQUENCY_ID = 0
PARTITION_ENTRY = struct.Struct("<HBBII16sI")
PARTITION_MD5_MARKER = b"\xeb\xeb" + b"\xff" * 14
PARTITION_TABLE_CAPACITY = 0x1000  # 0x8000..0x9000; NVS starts immediately after it.
ESP_IMAGE_HEADER_SIZE = 24


@dataclass(frozen=True)
class FlashProfile:
    capacity: int
    size_id: int
    # Name, type, subtype, offset, size. Existing partitions are unencrypted.
    partitions: tuple[tuple[str, int, int, int, int], ...]

    @property
    def maximum_application_size(self) -> int:
        return min(row[4] for row in self.partitions if row[1] == 0) - OTA_SAFETY_MARGIN


COMMON_PARTITIONS = (
    ("nvs", 1, 2, 0x9000, 0x5000),
    ("otadata", 1, 0, 0xE000, 0x2000),
)
FLASH_PROFILES = {
    "4MB": FlashProfile(4 * 1024 * 1024, 2, COMMON_PARTITIONS + (
        ("app0", 0, 0x10, 0x10000, 0x1E0000),
        ("app1", 0, 0x11, 0x1F0000, 0x1E0000),
        ("spiffs", 1, 0x82, 0x3D0000, 0x20000),
        ("coredump", 1, 3, 0x3F0000, 0x10000),
    )),
    "16MB": FlashProfile(16 * 1024 * 1024, 4, COMMON_PARTITIONS + (
        ("app0", 0, 0x10, 0x10000, 0x780000),
        ("app1", 0, 0x11, 0x790000, 0x780000),
        ("spiffs", 1, 0x82, 0xF10000, 0xF0000),
    )),
}


def flash_profile(flash_size: str) -> FlashProfile:
    try:
        return FLASH_PROFILES[flash_size]
    except KeyError as error:
        raise ValueError("flash size must be 4MB or 16MB") from error


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


def validate_esp_header(content: bytes, label: str, profile: FlashProfile) -> None:
    if len(content) < ESP_IMAGE_HEADER_SIZE or content[0] != 0xE9:
        raise RuntimeError(f"{label} does not contain ESP image magic and header")
    chip_id = int.from_bytes(content[12:14], "little")
    if chip_id != ESP32_CHIP_ID:
        raise RuntimeError(
            f"{label} targets chip ID {chip_id}, expected {ESP32_CHIP_ID}"
        )
    if (
        content[2] != FLASH_MODE_ID
        or content[3] >> 4 != profile.size_id
        or content[3] & 0xF != FLASH_FREQUENCY_ID
    ):
        raise RuntimeError(f"{label} flash header does not match the selected OSSM profile")


def validate_partition_table(path: Path, profile: FlashProfile) -> None:
    content = require_image(path, "partition table").read_bytes()
    if len(content) > PARTITION_TABLE_CAPACITY:
        raise RuntimeError("partition table exceeds its reserved sector and would overwrite NVS")
    if len(content) % PARTITION_ENTRY.size:
        raise RuntimeError("partition table has a truncated entry")
    partitions = []
    for offset in range(0, len(content), PARTITION_ENTRY.size):
        entry = content[offset : offset + PARTITION_ENTRY.size]
        if entry[:16] == PARTITION_MD5_MARKER:
            if entry[16:] != hashlib.md5(content[:offset]).digest():
                raise RuntimeError("partition table MD5 mismatch")
            if any(byte != 0xFF for byte in content[offset + PARTITION_ENTRY.size :]):
                raise RuntimeError("partition table contains data after its MD5")
            break
        magic, kind, subtype, address, size, name, flags = PARTITION_ENTRY.unpack(entry)
        if magic != 0x50AA or flags != 0:
            raise RuntimeError("partition table contains an invalid or encrypted entry")
        partitions.append((name, kind, subtype, address, size))
    else:
        raise RuntimeError("partition table is missing its MD5")
    expected = [
        (name.encode().ljust(16, b"\0"), kind, subtype, address, size)
        for name, kind, subtype, address, size in profile.partitions
    ]
    if partitions != expected:
        raise RuntimeError("partition geometry does not match the selected OSSM profile")


def build_components(
    build_dir: Path, boot_app0: Path, flash_size: str = "16MB"
) -> list[tuple[int, Path]]:
    profile = flash_profile(flash_size)
    bootloader = require_image(build_dir / "bootloader.bin", "bootloader")
    application = require_image(build_dir / "firmware.bin", "application")
    partitions = require_image(build_dir / "partitions.bin", "partition table")
    validate_esp_header(bootloader.read_bytes(), "bootloader", profile)
    validate_esp_header(application.read_bytes(), "application", profile)
    validate_partition_table(partitions, profile)
    if application.stat().st_size > profile.maximum_application_size:
        raise RuntimeError(
            f"application is {application.stat().st_size} bytes; maximum is "
            f"{profile.maximum_application_size} bytes after the 16 KiB OTA safety margin"
        )

    components = [
        (
            COMPONENT_OFFSETS["bootloader.bin"],
            bootloader,
        ),
        (
            COMPONENT_OFFSETS["partitions.bin"],
            partitions,
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
        if end > profile.capacity:
            raise RuntimeError(f"{path} exceeds the physical flash capacity")
        if index + 1 < len(components) and end > components[index + 1][0]:
            raise RuntimeError(f"{path} overlaps the next flash component")
    return components


def merge_command(
    esptool: Path, output: Path, components: list[tuple[int, Path]],
    flash_size: str = "16MB",
) -> list[str]:
    flash_profile(flash_size)
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
        flash_size,
    ]
    for offset, path in components:
        command.extend((hex(offset), str(path)))
    return command


def validate_merged_image(
    output: Path, components: list[tuple[int, Path]], flash_size: str = "16MB"
) -> None:
    profile = flash_profile(flash_size)
    content = require_image(output, "merged web installer").read_bytes()
    if len(content) > profile.capacity:
        raise RuntimeError(
            f"merged image is {len(content)} bytes, beyond {flash_size} flash"
        )
    for offset in (COMPONENT_OFFSETS["bootloader.bin"], 0x10000):
        validate_esp_header(content[offset : offset + ESP_IMAGE_HEADER_SIZE], f"image at {hex(offset)}", profile)

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
    build_dir: Path, output: Path, boot_app0: Path | None = None,
    flash_size: str = "16MB",
) -> None:
    components = build_components(build_dir, boot_app0 or find_boot_app0(), flash_size)
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(merge_command(find_esptool(), output, components, flash_size), check=True)
    validate_merged_image(output, components, flash_size)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--boot-app0", type=Path)
    parser.add_argument("--flash-size", choices=FLASH_PROFILES, default="16MB")
    args = parser.parse_args()
    try:
        build(args.build_dir, args.output, args.boot_app0, args.flash_size)
        print(f"Built validated web installer: {args.output}")
        return 0
    except Exception as error:
        print(f"Web installer build failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
