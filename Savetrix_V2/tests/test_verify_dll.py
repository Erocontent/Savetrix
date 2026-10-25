import importlib.util
from pathlib import Path
import struct
from types import SimpleNamespace as NS
import unittest

spec = importlib.util.spec_from_file_location("verify_dll", Path(__file__).resolve().parents[1] / "scripts/verify_dll.py")
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)


class MetadataTests(unittest.TestCase):
    def fixture(self):
        data = bytearray(0x350)
        struct.pack_into("<II", data, 0, 1, 0x02030010)
        data[8:17] = b"Savetrix\0"
        struct.pack_into("<II", data, 0x304, 1, 1)
        self.data = data
        pe = NS(FILE_HEADER=NS(Machine=0x8664, Characteristics=0x2000),
                DIRECTORY_ENTRY_EXPORT=NS(symbols=[NS(name=n, address=0x1000, forwarder=None) for n in
                    (b"SKSEPlugin_Load", b"SKSEPlugin_Query", b"SKSEPlugin_Version")]),
                get_data=lambda address, size: bytes(self.data[:size]))
        return pe

    def test_valid_metadata(self):
        verify.validate_pe(self.fixture(), "2.3.1")

    def test_missing_each_export(self):
        for index in range(3):
            pe = self.fixture()
            del pe.DIRECTORY_ENTRY_EXPORT.symbols[index]
            with self.assertRaisesRegex(ValueError, "export"):
                verify.validate_pe(pe, "2.3.1")

    def test_rejects_wrong_version_and_compatibility(self):
        for offset in (0, 4, 8, 0x304, 0x308, 0x34C):
            pe = self.fixture()
            self.data[offset] ^= 2
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                verify.validate_pe(pe, "2.3.1")

    def test_address_library_ignores_default_runtime_list(self):
        pe = self.fixture()
        struct.pack_into("<16I", self.data, 0x30C, *([0x01000000] * 16))
        verify.validate_pe(pe, "2.3.1")

    def test_rejects_wrong_architecture(self):
        pe = self.fixture()
        pe.FILE_HEADER.Machine = 0x14C
        with self.assertRaisesRegex(ValueError, "x64"):
            verify.validate_pe(pe, "2.3.1")

    def test_rejects_executable_forwarder_and_truncation(self):
        pe = self.fixture()
        pe.FILE_HEADER.Characteristics = 0
        with self.assertRaisesRegex(ValueError, "not a DLL"):
            verify.validate_pe(pe, "2.3.1")
        pe = self.fixture()
        pe.DIRECTORY_ENTRY_EXPORT.symbols[0].forwarder = b"other.dll"
        with self.assertRaisesRegex(ValueError, "forwarded"):
            verify.validate_pe(pe, "2.3.1")
        pe = self.fixture()
        self.data = self.data[:-1]
        with self.assertRaisesRegex(ValueError, "truncated"):
            verify.validate_pe(pe, "2.3.1")


if __name__ == "__main__":
    unittest.main()
