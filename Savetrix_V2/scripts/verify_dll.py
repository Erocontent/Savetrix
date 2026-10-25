"""Reject loader-invisible or incorrectly versioned Savetrix Release binaries."""
import argparse
import struct


def pack_version(version):
    parts = [int(x) for x in version.split(".")]
    parts += [0] * (4 - len(parts))
    major, minor, patch, build = parts
    if not (0 <= major <= 255 and 0 <= minor <= 255 and 0 <= patch <= 4095 and 0 <= build <= 15):
        raise ValueError("version is outside SKSE's packed range")
    return major << 24 | minor << 16 | patch << 4 | build


def validate_pe(pe, expected_version):
    if pe.FILE_HEADER.Machine != 0x8664:
        raise ValueError("Savetrix must be an x64 DLL")
    if not pe.FILE_HEADER.Characteristics & 0x2000:
        raise ValueError("PE is not a DLL")
    symbols = {s.name: s for s in getattr(getattr(pe, "DIRECTORY_ENTRY_EXPORT", None), "symbols", [])}
    required = (b"SKSEPlugin_Load", b"SKSEPlugin_Query", b"SKSEPlugin_Version")
    for name in required:
        if name not in symbols or symbols[name].forwarder:
            raise ValueError(f"missing or forwarded export: {name.decode()}")
    data = pe.get_data(symbols[b"SKSEPlugin_Version"].address, 0x350)
    if len(data) != 0x350:
        raise ValueError("truncated SKSE metadata")
    if struct.unpack_from("<II", data) != (1, pack_version(expected_version)):
        raise ValueError("incorrect SKSE metadata/plugin version")
    if data[8:264].split(b"\0", 1)[0] != b"Savetrix":
        raise ValueError("incorrect plugin name")
    if struct.unpack_from("<I", data, 0x304)[0] != 1:
        raise ValueError("struct compatibility must match CommonLib NG's independent layouts")
    if struct.unpack_from("<I", data, 0x308)[0] != 1:
        raise ValueError("Address Library runtime compatibility must be enabled")
    if any(data[0x30C:]):
        raise ValueError("unexpected runtime whitelist or minimum SKSE version")
    return symbols


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dll")
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    import pefile
    with pefile.PE(args.dll) as pe:
        symbols = validate_pe(pe, args.version)
    print(f"Savetrix {args.version}: x64 DLL and SKSE metadata verified ({len(symbols)} exports)")


if __name__ == "__main__":
    main()
