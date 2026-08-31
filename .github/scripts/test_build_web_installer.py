#!/usr/bin/env python3

import importlib.util
import hashlib
import struct
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
    @staticmethod
    def partition_bytes(flash_size, *, wrong_app_offset=False):
        rows = [
            ("nvs", 1, 2, 0x9000, 0x5000),
            ("otadata", 1, 0, 0xE000, 0x2000),
        ]
        if flash_size == "4MB":
            rows += [
                ("app0", 0, 16, 0x10000, 0x1E0000),
                ("app1", 0, 17, 0x1F0000, 0x1E0000),
                ("spiffs", 1, 0x82, 0x3D0000, 0x20000),
                ("coredump", 1, 3, 0x3F0000, 0x10000),
            ]
        else:
            rows += [
                ("app0", 0, 16, 0x10000, 0x780000),
                ("app1", 0, 17, 0x790000, 0x780000),
                ("spiffs", 1, 0x82, 0xF10000, 0xF0000),
            ]
        content = b"".join(
            struct.pack("<HBBII16sI", 0x50AA, kind, subtype,
                        offset + (0x10000 if wrong_app_offset and name == "app1" else 0),
                        size, name.encode(), 0)
            for name, kind, subtype, offset, size in rows
        )
        content += b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(content).digest()
        return content.ljust(0xC00, b"\xff")

    def write_images(self, root, flash_size="16MB", application_size=80):
        header = b"\xe9\x01\x02" + (b"\x20" if flash_size == "4MB" else b"\x40")
        (root / "bootloader.bin").write_bytes(header + b"\0" * 76)
        (root / "firmware.bin").write_bytes(header + b"\0" * (application_size - 4))
        (root / "partitions.bin").write_bytes(self.partition_bytes(flash_size))
        boot_app0 = root / "boot_app0.bin"
        boot_app0.write_bytes(b"boot-app")
        return boot_app0

    def test_merge_defaults_to_16_mib_and_supports_4_mib(self):
        for flash_size in ("4MB", "16MB"):
            with self.subTest(flash_size=flash_size):
                command = MODULE.merge_command(Path("esptool.py"), Path("out.bin"), [], flash_size)
                self.assertEqual(command[command.index("--flash_size") + 1], flash_size)
                self.assertEqual(command[command.index("--flash_mode") + 1], "keep")
                self.assertEqual(command[command.index("--flash_freq") + 1], "keep")
        self.assertIn("16MB", MODULE.merge_command(Path("esptool.py"), Path("out.bin"), []))
        with self.assertRaises(ValueError):
            MODULE.merge_command(Path("esptool.py"), Path("out.bin"), [], "8MB")

    def test_accepts_both_existing_partition_layouts(self):
        for flash_size in ("4MB", "16MB"):
            with self.subTest(flash_size=flash_size), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                boot_app0 = self.write_images(root, flash_size)
                self.assertEqual(
                    [offset for offset, _ in MODULE.build_components(root, boot_app0, flash_size)],
                    [0x1000, 0x8000, 0xE000, 0x10000],
                )

    def test_checks_both_application_and_bootloader_headers(self):
        for name in ("bootloader.bin", "firmware.bin"):
            for byte, replacement, error in ((0, 0, "magic"), (2, 0, "flash header"),
                                             (3, 0x20, "flash header"), (3, 0x41, "flash header"),
                                             (12, 9, "chip ID")):
                with self.subTest(name=name, byte=byte, replacement=replacement), tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    boot_app0 = self.write_images(root)
                    content = bytearray((root / name).read_bytes())
                    content[byte] = replacement
                    (root / name).write_bytes(content)
                    with self.assertRaisesRegex(RuntimeError, error):
                        MODULE.build_components(root, boot_app0)

    def test_rejects_truncated_headers(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            boot_app0 = self.write_images(root)
            path = root / "firmware.bin"
            content = path.read_bytes()
            for size in (1, 14, 23):
                with self.subTest(size=size):
                    path.write_bytes(content[:size])
                    with self.assertRaisesRegex(RuntimeError, "header"):
                        MODULE.build_components(root, boot_app0)

    def test_rejects_wrong_partition_geometry_even_with_valid_md5(self):
        for flash_size in ("4MB", "16MB"):
            with self.subTest(flash_size=flash_size), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                boot_app0 = self.write_images(root, flash_size)
                (root / "partitions.bin").write_bytes(self.partition_bytes(flash_size, wrong_app_offset=True))
                with self.assertRaisesRegex(RuntimeError, "geometry"):
                    MODULE.build_components(root, boot_app0, flash_size)

    def test_rejects_a_partition_table_for_the_other_flash_capacity(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            boot_app0 = self.write_images(root, "4MB")
            (root / "partitions.bin").write_bytes(self.partition_bytes("16MB"))
            with self.assertRaisesRegex(RuntimeError, "geometry"):
                MODULE.build_components(root, boot_app0, "4MB")

    def test_rejects_corrupt_or_truncated_partition_tables(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            boot_app0 = self.write_images(root)
            path = root / "partitions.bin"
            content = bytearray(path.read_bytes())
            content[12] ^= 1
            path.write_bytes(content)
            with self.assertRaisesRegex(RuntimeError, "MD5"):
                MODULE.build_components(root, boot_app0)
            path.write_bytes(content[:-1])
            with self.assertRaisesRegex(RuntimeError, "truncated"):
                MODULE.build_components(root, boot_app0)

    def test_partition_padding_cannot_overwrite_nvs(self):
        for flash_size in ("4MB", "16MB"):
            with self.subTest(flash_size=flash_size), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                boot_app0 = self.write_images(root, flash_size)
                path = root / "partitions.bin"
                content = path.read_bytes()
                path.write_bytes(content.ljust(0x1000, b"\xff"))
                MODULE.build_components(root, boot_app0, flash_size)
                path.write_bytes(content.ljust(0x1020, b"\xff"))
                with self.assertRaisesRegex(RuntimeError, "overwrite NVS"):
                    MODULE.build_components(root, boot_app0, flash_size)

    def test_enforces_16_kib_margin_at_both_ota_slot_boundaries(self):
        for flash_size, slot_size in (("4MB", 0x1E0000), ("16MB", 0x780000)):
            with self.subTest(flash_size=flash_size), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                boot_app0 = self.write_images(root, flash_size, slot_size - 0x4000)
                MODULE.build_components(root, boot_app0, flash_size)
                with (root / "firmware.bin").open("ab") as handle:
                    handle.write(b"\0")
                with self.assertRaisesRegex(RuntimeError, "safety margin"):
                    MODULE.build_components(root, boot_app0, flash_size)

    def test_rejects_components_overlapping_the_partition_table(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            boot_app0 = self.write_images(root)
            path = root / "bootloader.bin"
            path.write_bytes(path.read_bytes().ljust(0x8000, b"\0"))
            with self.assertRaisesRegex(RuntimeError, "overlaps"):
                MODULE.build_components(root, boot_app0)

    def test_merged_image_checks_headers_and_every_component(self):
        for flash_size in ("4MB", "16MB"):
            with self.subTest(flash_size=flash_size), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                boot_app0 = self.write_images(root, flash_size)
                components = MODULE.build_components(root, boot_app0, flash_size)
                merged = bytearray(b"\xff" * 0x10050)
                for offset, path in components:
                    merged[offset : offset + path.stat().st_size] = path.read_bytes()
                output = root / "web-installer.bin"
                output.write_bytes(merged)
                MODULE.validate_merged_image(output, components, flash_size)
                for offset, error in ((0x10000, "magic"), (0x10003, "flash header"),
                                      (0x8000, "component mismatch"), (0x10040, "component mismatch")):
                    damaged = merged.copy()
                    damaged[offset] ^= 0xFF
                    output.write_bytes(damaged)
                    with self.assertRaisesRegex(RuntimeError, error):
                        MODULE.validate_merged_image(output, components, flash_size)


if __name__ == "__main__":
    unittest.main()
