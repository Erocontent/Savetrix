#!/usr/bin/env python3
"""Generate the tiny ESL-flagged Savetrix.esp used only to register the MCM.

The plugin contains one Start Game Enabled QUST (FormID 0x800) with the
MCM Helper script MCM_ConfigBase attached. It has no world records, items,
NPCs, gameplay records, aliases, fragments, or hard masters.
"""

from __future__ import annotations

import argparse
import pathlib
import struct

TES4_FLAG_ESL = 0x00000200
FORM_VERSION = 44
QUEST_FORM_ID = 0x00000800
NEXT_OBJECT_ID = 0x00000801
HEADER_VERSION = 1.71


def subrecord(sig: bytes, payload: bytes) -> bytes:
    if len(sig) != 4:
        raise ValueError("subrecord signature must be 4 bytes")
    if len(payload) > 0xFFFF:
        raise ValueError("payload too large for a normal subrecord")
    return sig + struct.pack("<H", len(payload)) + payload


def record(sig: bytes, payload: bytes, *, flags: int, form_id: int, form_version: int = FORM_VERSION) -> bytes:
    if len(sig) != 4:
        raise ValueError("record signature must be 4 bytes")
    return (
        sig
        + struct.pack("<I", len(payload))
        + struct.pack("<I", flags)
        + struct.pack("<I", form_id)
        + struct.pack("<I", 0)  # version control info 1
        + struct.pack("<H", form_version)
        + struct.pack("<H", 0)  # version control info 2
        + payload
    )


def grup(label: bytes, records: bytes) -> bytes:
    if len(label) != 4:
        raise ValueError("group label must be 4 bytes")
    total_size = 24 + len(records)
    return (
        b"GRUP"
        + struct.pack("<I", total_size)
        + label
        + struct.pack("<I", 0)  # top-level group
        + struct.pack("<H", 0)  # stamp
        + struct.pack("<H", 0)  # unknown
        + struct.pack("<H", 0)  # version
        + struct.pack("<H", 0)  # unknown
        + records
    )


def make_vmad_script(script_name: str) -> bytes:
    encoded = script_name.encode("ascii")
    # VMAD v5, object format 2, one script; script has no properties.
    return (
        struct.pack("<h", 5)
        + struct.pack("<h", 2)
        + struct.pack("<H", 1)
        + struct.pack("<H", len(encoded))
        + encoded
        + struct.pack("<B", 0)  # script status/flags
        + struct.pack("<H", 0)  # property count
    )


def build_plugin() -> bytes:
    hedr = struct.pack("<fII", HEADER_VERSION, 1, NEXT_OBJECT_ID)
    tes4_payload = b"".join(
        [
            subrecord(b"HEDR", hedr),
            subrecord(b"CNAM", b"Savetrix\x00"),
            subrecord(b"SNAM", b"Savetrix MCM registration plugin.\x00"),
        ]
    )
    tes4 = record(b"TES4", tes4_payload, flags=TES4_FLAG_ESL, form_id=0)

    # QUST DNAM is 12 bytes in SSE:
    # uint16 flags, uint8 priority, uint8 form version, 4 unknown bytes, uint32 quest type.
    # Bit 0 = Start Game Enabled. No aliases/fragments are needed for MCM registration.
    dnam = struct.pack("<HBB4sI", 0x0001, 0, FORM_VERSION, b"\x00\x00\x00\x00", 0)
    quest_payload = b"".join(
        [
            subrecord(b"EDID", b"SavetrixMCMQuest\x00"),
            subrecord(b"VMAD", make_vmad_script("MCM_ConfigBase")),
            subrecord(b"DNAM", dnam),
            subrecord(b"NEXT", b""),
        ]
    )
    quest = record(b"QUST", quest_payload, flags=0, form_id=QUEST_FORM_ID)
    return tes4 + grup(b"QUST", quest)


def parse_subrecords(payload: bytes) -> dict[bytes, list[bytes]]:
    out: dict[bytes, list[bytes]] = {}
    pos = 0
    while pos < len(payload):
        if pos + 6 > len(payload):
            raise ValueError("truncated subrecord header")
        sig = payload[pos : pos + 4]
        size = struct.unpack_from("<H", payload, pos + 4)[0]
        pos += 6
        if pos + size > len(payload):
            raise ValueError(f"truncated {sig!r} payload")
        out.setdefault(sig, []).append(payload[pos : pos + size])
        pos += size
    return out


def validate_plugin(data: bytes) -> None:
    if len(data) < 24 or data[:4] != b"TES4":
        raise ValueError("missing TES4 record")
    tes4_size, tes4_flags, tes4_form = struct.unpack_from("<III", data, 4)
    if tes4_flags & TES4_FLAG_ESL == 0:
        raise ValueError("TES4 ESL flag missing")
    if tes4_form != 0:
        raise ValueError("TES4 FormID must be zero")
    tes4_end = 24 + tes4_size
    tes4_subs = parse_subrecords(data[24:tes4_end])
    if b"HEDR" not in tes4_subs:
        raise ValueError("TES4/HEDR missing")
    version, record_count, next_object = struct.unpack("<fII", tes4_subs[b"HEDR"][0])
    if abs(version - HEADER_VERSION) > 0.001 or record_count != 1 or next_object != NEXT_OBJECT_ID:
        raise ValueError("unexpected TES4/HEDR values")

    if data[tes4_end : tes4_end + 4] != b"GRUP":
        raise ValueError("QUST top group missing")
    group_size = struct.unpack_from("<I", data, tes4_end + 4)[0]
    label = data[tes4_end + 8 : tes4_end + 12]
    group_type = struct.unpack_from("<I", data, tes4_end + 12)[0]
    if label != b"QUST" or group_type != 0:
        raise ValueError("invalid QUST top group")
    if tes4_end + group_size != len(data):
        raise ValueError("group size does not reach EOF")

    qpos = tes4_end + 24
    if data[qpos : qpos + 4] != b"QUST":
        raise ValueError("QUST record missing")
    qsize, _, qform = struct.unpack_from("<III", data, qpos + 4)
    if qform != QUEST_FORM_ID:
        raise ValueError("unexpected quest FormID")
    qsubs = parse_subrecords(data[qpos + 24 : qpos + 24 + qsize])
    for required in (b"EDID", b"VMAD", b"DNAM", b"NEXT"):
        if required not in qsubs:
            raise ValueError(f"required QUST subrecord {required!r} missing")
    if qsubs[b"EDID"][0] != b"SavetrixMCMQuest\x00":
        raise ValueError("unexpected quest EditorID")
    if len(qsubs[b"DNAM"][0]) != 12:
        raise ValueError("QUST/DNAM must be 12 bytes")
    flags = struct.unpack_from("<H", qsubs[b"DNAM"][0], 0)[0]
    if flags & 0x0001 == 0:
        raise ValueError("quest is not Start Game Enabled")

    vmad = qsubs[b"VMAD"][0]
    v, obj_fmt, script_count = struct.unpack_from("<hhH", vmad, 0)
    if (v, obj_fmt, script_count) != (5, 2, 1):
        raise ValueError("unexpected VMAD header")
    name_len = struct.unpack_from("<H", vmad, 6)[0]
    name = vmad[8 : 8 + name_len].decode("ascii")
    if name != "MCM_ConfigBase":
        raise ValueError("MCM_ConfigBase is not attached")
    flags_pos = 8 + name_len
    script_flags = vmad[flags_pos]
    prop_count = struct.unpack_from("<H", vmad, flags_pos + 1)[0]
    if script_flags != 0 or prop_count != 0:
        raise ValueError("unexpected MCM script metadata")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", nargs="?", default="Data/Savetrix.esp")
    args = parser.parse_args()

    data = build_plugin()
    validate_plugin(data)

    output = pathlib.Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)
    print(f"Generated {output} ({len(data)} bytes); structural validation passed.")


if __name__ == "__main__":
    main()
