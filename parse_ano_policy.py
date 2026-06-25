from __future__ import annotations

import argparse
import json
import pathlib
import struct
import zlib
from dataclasses import asdict, dataclass


MAGIC = 0x20250812
ENTRY_SIZE = 24

KNOWN_KEYS = {
    41539: "ano_det_cnt",
    41553: "ano_touch_thres",
    41624: "ano_touch_reg",
    41640: "ano_record_cnt",
    41657: "ano_record_scale",
    41676: "ano_touch_period",
    41701: "opt_anti_clicker",
    42168: "click_capt",
    42181: "click_alert",
}


@dataclass
class PolicyEntry:
    index: int
    key0: int
    key1: int
    value0: int
    value1: int

    def to_json(self) -> dict[str, object]:
        data = asdict(self)
        data["key0_hex"] = f"0x{self.key0:016x}"
        data["key1_hex"] = f"0x{self.key1:016x}"
        data["value0_hex"] = f"0x{self.value0:08x}"
        data["value1_hex"] = f"0x{self.value1:08x}"
        return data


@dataclass
class PolicyFile:
    path: pathlib.Path
    size: int
    magic: int
    stored_crc32: int
    computed_crc32: int
    crc_ok: bool
    entry_count: int
    parsed_count: int
    truncated: bool
    entries: list[PolicyEntry]

    def to_json(self) -> dict[str, object]:
        return {
            "path": str(self.path),
            "size": self.size,
            "magic": f"0x{self.magic:08x}",
            "stored_crc32": f"0x{self.stored_crc32:08x}",
            "computed_crc32": f"0x{self.computed_crc32:08x}",
            "crc_ok": self.crc_ok,
            "entry_count": self.entry_count,
            "parsed_count": self.parsed_count,
            "truncated": self.truncated,
            "entry_struct": "<QQII",
            "known_keys": KNOWN_KEYS,
            "entries": [entry.to_json() for entry in self.entries],
        }


def parse_policy_file(path: pathlib.Path) -> PolicyFile:
    data = bytearray(path.read_bytes())
    if len(data) < 12:
        raise ValueError("file too small: expected at least magic + crc32 + count")

    magic, stored_crc32, entry_count = struct.unpack_from("<III", data, 0)
    crc_view = bytearray(data)
    struct.pack_into("<I", crc_view, 4, 0)
    computed_crc32 = zlib.crc32(crc_view) & 0xFFFFFFFF

    entries: list[PolicyEntry] = []
    offset = 12
    remaining = len(data) - offset
    parsed_count = remaining // ENTRY_SIZE
    truncated = parsed_count < entry_count or (remaining % ENTRY_SIZE) != 0
    limit = min(entry_count, parsed_count)

    for index in range(limit):
        key0, key1, value0, value1 = struct.unpack_from("<QQII", data, offset)
        entries.append(
            PolicyEntry(
                index=index,
                key0=key0,
                key1=key1,
                value0=value0,
                value1=value1,
            )
        )
        offset += ENTRY_SIZE

    return PolicyFile(
        path=path,
        size=len(data),
        magic=magic,
        stored_crc32=stored_crc32,
        computed_crc32=computed_crc32,
        crc_ok=(computed_crc32 == stored_crc32),
        entry_count=entry_count,
        parsed_count=parsed_count,
        truncated=truncated,
        entries=entries,
    )


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Parse libtersafe anti-clicker policy file ano_tmp/mpmc.dat. "
            "Expected Android path: /data/user/0/<pkg>/files/ano_tmp/mpmc.dat"
        )
    )
    parser.add_argument("input", type=pathlib.Path, help="Path to mpmc.dat")
    parser.add_argument(
        "--pretty",
        action="store_true",
        help="Pretty-print JSON output",
    )
    return parser


def main() -> int:
    parser = build_arg_parser()
    args = parser.parse_args()
    result = parse_policy_file(args.input)
    if result.magic != MAGIC:
        raise SystemExit(
            f"unexpected magic: got 0x{result.magic:08x}, expected 0x{MAGIC:08x}"
        )
    indent = 2 if args.pretty else None
    print(json.dumps(result.to_json(), indent=indent, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
