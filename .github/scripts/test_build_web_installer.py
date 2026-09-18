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
    def test_merge_preserves_built_mode_and_uses_16_mib_capacity(self):
        with tempfile.TemporaryDirectory() as directory:
            command = MODULE.merge_command(
                Path(directory) / "esptool.py",
                Path(directory) / "web-installer.bin",
                [(0x1000, Path(directory) / "bootloader.bin")],
            )
        self.assertIn("keep", command)
        self.assertIn("16MB", command)
        self.assertEqual(MODULE.FLASH_CAPACITY, 16 * 1024 * 1024)

    def test_4_mib_headers_capacity_and_component_preservation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            header = b"\xe9\x01\x02\x20" + b"\0" * 12
            for name, content in (
                ("bootloader.bin", header),
                ("partitions.bin", b"partition-table"),
                ("boot_app0.bin", b"boot-app"),
                ("firmware.bin", header + b"application"),
            ):
                (root / name).write_bytes(content)
            components = MODULE.build_components(root, root / "boot_app0.bin", 4)
            with self.assertRaisesRegex(RuntimeError, "16 MiB profile"):
                MODULE.build_components(root, root / "boot_app0.bin")
            output = root / "web-installer.bin"
            command = MODULE.merge_command(root / "esptool.py", output, components, 4)
            self.assertEqual(command[command.index("--flash_size") + 1], "4MB")
            for option in ("--flash_mode", "--flash_freq"):
                self.assertEqual(command[command.index(option) + 1], "keep")

            merged = bytearray(b"\xff" * 0x10020)
            for offset, path in components:
                content = path.read_bytes()
                merged[offset : offset + len(content)] = content
            output.write_bytes(merged)
            MODULE.validate_merged_image(output, components, 4)
            with self.assertRaisesRegex(RuntimeError, "flash header"):
                MODULE.validate_merged_image(output, components)
            for offset, path in components:
                with self.subTest(component=path.name):
                    index = offset + path.stat().st_size - 1
                    merged[index] ^= 1
                    output.write_bytes(merged)
                    with self.assertRaisesRegex(RuntimeError, "component mismatch"):
                        MODULE.validate_merged_image(output, components, 4)
                    merged[index] ^= 1

            output.write_bytes(merged + b"\xff" * (4 * 1024 * 1024 + 1 - len(merged)))
            with self.assertRaisesRegex(RuntimeError, "beyond 4 MiB flash"):
                MODULE.validate_merged_image(output, components, 4)
            application = root / "firmware.bin"
            with application.open("r+b") as image:
                image.truncate(4 * 1024 * 1024 - 0x10000)
            MODULE.build_components(root, root / "boot_app0.bin", 4)
            with application.open("ab") as image:
                image.write(b"\0")
            with self.assertRaisesRegex(RuntimeError, "physical flash capacity"):
                MODULE.build_components(root, root / "boot_app0.bin", 4)
            (root / "bootloader.bin").write_bytes(header[:3] + b"\x40" + header[4:])
            with self.assertRaisesRegex(RuntimeError, "4 MiB profile"):
                MODULE.build_components(root, root / "boot_app0.bin", 4)
            with application.open("r+b") as image:
                image.seek(3)
                image.write(b"\x40")
            MODULE.build_components(root, root / "boot_app0.bin")

    def test_rejects_the_wrong_chip_or_flash_header(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            bootloader = root / "bootloader.bin"
            application = root / "firmware.bin"
            partitions = root / "partitions.bin"
            boot_app0 = root / "boot_app0.bin"
            bootloader.write_bytes(b"\xe9\x01\x02\x40" + b"\0" * 10)
            application.write_bytes(b"\xe9\x01\x02\x40" + b"\0" * 10)
            partitions.write_bytes(b"partitions")
            boot_app0.write_bytes(b"boot-app")
            MODULE.build_components(root, boot_app0)

            bootloader.write_bytes(b"\xe9\x01\x00\x20" + b"\0" * 10)
            with self.assertRaisesRegex(RuntimeError, "flash header"):
                MODULE.build_components(root, boot_app0)

            bootloader.write_bytes(b"\xe9\x01\x02\x40" + b"\0" * 10)
            for profile in (b"\x00\x40", b"\x02\x20", b"\x02\x41"):
                with self.subTest(application_profile=profile):
                    application.write_bytes(b"\xe9\x01" + profile + b"\0" * 10)
                    with self.assertRaisesRegex(RuntimeError, "application flash header"):
                        MODULE.build_components(root, boot_app0)

            application.write_bytes(
                b"\xe9\x01\x00\x00" + b"\0" * 8 + b"\x09\x00"
            )
            with self.assertRaisesRegex(RuntimeError, "chip ID"):
                MODULE.build_components(root, boot_app0)

    def test_validation_checks_magic_and_exact_components(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            components = []
            bootloader = bytearray(16)
            bootloader[0] = 0xE9
            bootloader[1] = 1
            bootloader[2] = MODULE.FLASH_MODE_ID
            bootloader[3] = (
                MODULE.FLASH_SIZE_ID << 4
            ) | MODULE.FLASH_FREQUENCY_ID
            bootloader[12:14] = MODULE.ESP32_CHIP_ID.to_bytes(2, "little")
            application = bytearray(16)
            application[0] = 0xE9
            application[1] = 1
            application[2:4] = bootloader[2:4]
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
            MODULE.validate_merged_image(output, components)

            merged[0x10003] = 0x20
            output.write_bytes(merged)
            with self.assertRaisesRegex(RuntimeError, "application flash header"):
                MODULE.validate_merged_image(output, components)
            merged[0x10003] = 0x40

            merged[0x10000] = 0
            output.write_bytes(merged)
            with self.assertRaisesRegex(RuntimeError, "magic"):
                MODULE.validate_merged_image(output, components)

            merged[0x10000] = 0xE9
            merged[0x8000] = 0
            output.write_bytes(merged)
            with self.assertRaisesRegex(RuntimeError, "component mismatch"):
                MODULE.validate_merged_image(output, components)


if __name__ == "__main__":
    unittest.main()
