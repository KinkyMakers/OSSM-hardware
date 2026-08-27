#!/usr/bin/env python3

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).with_name("build_web_installer.py")
SPEC = importlib.util.spec_from_file_location("build_web_installer", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class BuildWebInstallerTests(unittest.TestCase):
    def test_merge_preserves_built_mode_and_uses_selected_capacity(self):
        with tempfile.TemporaryDirectory() as directory:
            profile = MODULE.PROFILES["esp32-4mb"]
            command = MODULE.merge_command(
                Path(directory) / "esptool.py",
                profile,
                Path(directory) / "web-installer.bin",
                [(0x1000, Path(directory) / "bootloader.bin")],
            )
        self.assertIn("keep", command)
        self.assertIn("4MB", command)
        self.assertEqual(profile.capacity, 4 * 1024 * 1024)

    def test_rejects_the_wrong_chip_or_flash_header(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            bootloader = root / "bootloader.bin"
            application = root / "firmware.bin"
            partitions = root / "partitions.bin"
            boot_app0 = root / "boot_app0.bin"
            bootloader.write_bytes(b"\xe9\x01\x02\x40" + b"\0" * 10)
            application.write_bytes(b"\xe9\x01\x00\x00" + b"\0" * 10)
            partitions.write_bytes(b"partitions")
            boot_app0.write_bytes(b"boot-app")
            profile = MODULE.PROFILES["esp32-16mb"]
            MODULE.build_components(profile, root, boot_app0)

            bootloader.write_bytes(b"\xe9\x01\x00\x20" + b"\0" * 10)
            with self.assertRaisesRegex(RuntimeError, "flash header"):
                MODULE.build_components(profile, root, boot_app0)

            application.write_bytes(
                b"\xe9\x01\x00\x00" + b"\0" * 8 + b"\x09\x00"
            )
            bootloader.write_bytes(b"\xe9\x01\x02\x40" + b"\0" * 10)
            with self.assertRaisesRegex(RuntimeError, "chip ID"):
                MODULE.build_components(profile, root, boot_app0)

    def test_validation_checks_magic_and_exact_components(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            components = []
            bootloader = bytearray(16)
            bootloader[0] = 0xE9
            bootloader[1] = 1
            bootloader[2] = MODULE.FLASH_MODE_ID
            bootloader[3] = (
                MODULE.PROFILES["esp32-16mb"].flash_size_id << 4
            ) | MODULE.FLASH_FREQUENCY_ID
            bootloader[12:14] = MODULE.ESP32_CHIP_ID.to_bytes(2, "little")
            application = bytearray(16)
            application[0] = 0xE9
            application[1] = 1
            application[12:14] = MODULE.ESP32_CHIP_ID.to_bytes(2, "little")
            for offset, name, body in (
                (0x1000, "bootloader.bin", bytes(bootloader)),
                (0x8000, "partitions.bin", b"partition-table"),
                (0xE000, "boot_app0.bin", b"boot-app"),
                (0x10000, "firmware.bin", bytes(application)),
            ):
                path = root / name
                path.write_bytes(body)
                components.append((offset, path))

            merged = bytearray(b"\xff" * 0x10020)
            for offset, path in components:
                body = path.read_bytes()
                merged[offset : offset + len(body)] = body
            output = root / "web-installer.bin"
            output.write_bytes(merged)
            profile = MODULE.PROFILES["esp32-16mb"]
            MODULE.validate_merged_image(output, profile, components)

            merged[0x10000] = 0
            output.write_bytes(merged)
            with self.assertRaisesRegex(RuntimeError, "magic"):
                MODULE.validate_merged_image(output, profile, components)

            merged[0x10000] = 0xE9
            merged[0x8000] = 0
            output.write_bytes(merged)
            with self.assertRaisesRegex(RuntimeError, "component mismatch"):
                MODULE.validate_merged_image(output, profile, components)


if __name__ == "__main__":
    unittest.main()
