from __future__ import annotations

import argparse
import pathlib
from dataclasses import dataclass


def _key_byte_from_a3(a3: int) -> int:
    a3 &= 0xFFFFFFFF
    if (a3 & 0x00FF0000) == 0:
        return 0x87
    return (a3 >> 16) & 0xFF


def sub_12fa6c_inplace(buf: bytearray, *, offset: int, length: int, a3: int) -> None:
    if length <= 0:
        return
    if offset < 0 or length < 0 or offset + length > len(buf):
        raise ValueError("range out of bounds")
    key = _key_byte_from_a3(a3)
    end = offset + length
    for i in range(offset, end):
        buf[i] ^= key


def _nibble_indices_from_a4(a4: int) -> list[int]:
    a4 &= 0xFFFFFFFF
    return [
        (a4 >> 28) & 0xF,
        (a4 >> 24) & 0xF,
        (a4 >> 20) & 0xF,
        (a4 >> 16) & 0xF,
        (a4 >> 12) & 0xF,
        (a4 >> 8) & 0xF,
        (a4 >> 4) & 0xF,
        (a4 >> 0) & 0xF,
    ]


def sub_12fa98_inplace(buf: bytearray, *, offset: int, length: int, a3: int, a4: int) -> None:
    if length <= 0:
        return
    if offset < 0 or length < 0 or offset + length > len(buf):
        raise ValueError("range out of bounds")

    key = _key_byte_from_a3(a3)
    full_chunks = length // 0x10000
    remainder = length - full_chunks * 0x10000

    scratch = bytearray(0x20000) if a4 else None
    indices = _nibble_indices_from_a4(a4) if a4 else None

    for chunk_index in range(full_chunks):
        chunk_base = offset + chunk_index * 0x10000

        for j in range(0, 0x4000):
            buf[chunk_base + j] ^= key

        if scratch is not None:
            block_index = chunk_index & 7
            scratch[block_index * 0x4000 : (block_index + 1) * 0x4000] = buf[
                chunk_base : chunk_base + 0x4000
            ]

            if block_index == 7:
                group_base = chunk_base - 0x70000
                for k, dest_idx in enumerate(indices):
                    src_block = scratch[k * 0x4000 : (k + 1) * 0x4000]
                    dst = group_base + dest_idx * 0x10000
                    buf[dst : dst + 0x4000] = src_block
                scratch[:] = b"\x00" * 0x20000

    if remainder:
        tail_base = offset + full_chunks * 0x10000
        end = tail_base + remainder
        for i in range(tail_base, end):
            buf[i] ^= key


_SBOX = bytes(
    [
        0x63,
        0x7C,
        0x77,
        0x7B,
        0xF2,
        0x6B,
        0x6F,
        0xC5,
        0x30,
        0x01,
        0x67,
        0x2B,
        0xFE,
        0xD7,
        0xAB,
        0x76,
        0xCA,
        0x82,
        0xC9,
        0x7D,
        0xFA,
        0x59,
        0x47,
        0xF0,
        0xAD,
        0xD4,
        0xA2,
        0xAF,
        0x9C,
        0xA4,
        0x72,
        0xC0,
        0xB7,
        0xFD,
        0x93,
        0x26,
        0x36,
        0x3F,
        0xF7,
        0xCC,
        0x34,
        0xA5,
        0xE5,
        0xF1,
        0x71,
        0xD8,
        0x31,
        0x15,
        0x04,
        0xC7,
        0x23,
        0xC3,
        0x18,
        0x96,
        0x05,
        0x9A,
        0x07,
        0x12,
        0x80,
        0xE2,
        0xEB,
        0x27,
        0xB2,
        0x75,
        0x09,
        0x83,
        0x2C,
        0x1A,
        0x1B,
        0x6E,
        0x5A,
        0xA0,
        0x52,
        0x3B,
        0xD6,
        0xB3,
        0x29,
        0xE3,
        0x2F,
        0x84,
        0x53,
        0xD1,
        0x00,
        0xED,
        0x20,
        0xFC,
        0xB1,
        0x5B,
        0x6A,
        0xCB,
        0xBE,
        0x39,
        0x4A,
        0x4C,
        0x58,
        0xCF,
        0xD0,
        0xEF,
        0xAA,
        0xFB,
        0x43,
        0x4D,
        0x33,
        0x85,
        0x45,
        0xF9,
        0x02,
        0x7F,
        0x50,
        0x3C,
        0x9F,
        0xA8,
        0x51,
        0xA3,
        0x40,
        0x8F,
        0x92,
        0x9D,
        0x38,
        0xF5,
        0xBC,
        0xB6,
        0xDA,
        0x21,
        0x10,
        0xFF,
        0xF3,
        0xD2,
        0xCD,
        0x0C,
        0x13,
        0xEC,
        0x5F,
        0x97,
        0x44,
        0x17,
        0xC4,
        0xA7,
        0x7E,
        0x3D,
        0x64,
        0x5D,
        0x19,
        0x73,
        0x60,
        0x81,
        0x4F,
        0xDC,
        0x22,
        0x2A,
        0x90,
        0x88,
        0x46,
        0xEE,
        0xB8,
        0x14,
        0xDE,
        0x5E,
        0x0B,
        0xDB,
        0xE0,
        0x32,
        0x3A,
        0x0A,
        0x49,
        0x06,
        0x24,
        0x5C,
        0xC2,
        0xD3,
        0xAC,
        0x62,
        0x91,
        0x95,
        0xE4,
        0x79,
        0xE7,
        0xC8,
        0x37,
        0x6D,
        0x8D,
        0xD5,
        0x4E,
        0xA9,
        0x6C,
        0x56,
        0xF4,
        0xEA,
        0x65,
        0x7A,
        0xAE,
        0x08,
        0xBA,
        0x78,
        0x25,
        0x2E,
        0x1C,
        0xA6,
        0xB4,
        0xC6,
        0xE8,
        0xDD,
        0x74,
        0x1F,
        0x4B,
        0xBD,
        0x8B,
        0x8A,
        0x70,
        0x3E,
        0xB5,
        0x66,
        0x48,
        0x03,
        0xF6,
        0x0E,
        0x61,
        0x35,
        0x57,
        0xB9,
        0x86,
        0xC1,
        0x1D,
        0x9E,
        0xE1,
        0xF8,
        0x98,
        0x11,
        0x69,
        0xD9,
        0x8E,
        0x94,
        0x9B,
        0x1E,
        0x87,
        0xE9,
        0xCE,
        0x55,
        0x28,
        0xDF,
        0x8C,
        0xA1,
        0x89,
        0x0D,
        0xBF,
        0xE6,
        0x42,
        0x68,
        0x41,
        0x99,
        0x2D,
        0x0F,
        0xB0,
        0x54,
        0xBB,
        0x16,
    ]
)

_INV_SBOX = bytes(
    [
        0x52,
        0x09,
        0x6A,
        0xD5,
        0x30,
        0x36,
        0xA5,
        0x38,
        0xBF,
        0x40,
        0xA3,
        0x9E,
        0x81,
        0xF3,
        0xD7,
        0xFB,
        0x7C,
        0xE3,
        0x39,
        0x82,
        0x9B,
        0x2F,
        0xFF,
        0x87,
        0x34,
        0x8E,
        0x43,
        0x44,
        0xC4,
        0xDE,
        0xE9,
        0xCB,
        0x54,
        0x7B,
        0x94,
        0x32,
        0xA6,
        0xC2,
        0x23,
        0x3D,
        0xEE,
        0x4C,
        0x95,
        0x0B,
        0x42,
        0xFA,
        0xC3,
        0x4E,
        0x08,
        0x2E,
        0xA1,
        0x66,
        0x28,
        0xD9,
        0x24,
        0xB2,
        0x76,
        0x5B,
        0xA2,
        0x49,
        0x6D,
        0x8B,
        0xD1,
        0x25,
        0x72,
        0xF8,
        0xF6,
        0x64,
        0x86,
        0x68,
        0x98,
        0x16,
        0xD4,
        0xA4,
        0x5C,
        0xCC,
        0x5D,
        0x65,
        0xB6,
        0x92,
        0x6C,
        0x70,
        0x48,
        0x50,
        0xFD,
        0xED,
        0xB9,
        0xDA,
        0x5E,
        0x15,
        0x46,
        0x57,
        0xA7,
        0x8D,
        0x9D,
        0x84,
        0x90,
        0xD8,
        0xAB,
        0x00,
        0x8C,
        0xBC,
        0xD3,
        0x0A,
        0xF7,
        0xE4,
        0x58,
        0x05,
        0xB8,
        0xB3,
        0x45,
        0x06,
        0xD0,
        0x2C,
        0x1E,
        0x8F,
        0xCA,
        0x3F,
        0x0F,
        0x02,
        0xC1,
        0xAF,
        0xBD,
        0x03,
        0x01,
        0x13,
        0x8A,
        0x6B,
        0x3A,
        0x91,
        0x11,
        0x41,
        0x4F,
        0x67,
        0xDC,
        0xEA,
        0x97,
        0xF2,
        0xCF,
        0xCE,
        0xF0,
        0xB4,
        0xE6,
        0x73,
        0x96,
        0xAC,
        0x74,
        0x22,
        0xE7,
        0xAD,
        0x35,
        0x85,
        0xE2,
        0xF9,
        0x37,
        0xE8,
        0x1C,
        0x75,
        0xDF,
        0x6E,
        0x47,
        0xF1,
        0x1A,
        0x71,
        0x1D,
        0x29,
        0xC5,
        0x89,
        0x6F,
        0xB7,
        0x62,
        0x0E,
        0xAA,
        0x18,
        0xBE,
        0x1B,
        0xFC,
        0x56,
        0x3E,
        0x4B,
        0xC6,
        0xD2,
        0x79,
        0x20,
        0x9A,
        0xDB,
        0xC0,
        0xFE,
        0x78,
        0xCD,
        0x5A,
        0xF4,
        0x1F,
        0xDD,
        0xA8,
        0x33,
        0x88,
        0x07,
        0xC7,
        0x31,
        0xB1,
        0x12,
        0x10,
        0x59,
        0x27,
        0x80,
        0xEC,
        0x5F,
        0x60,
        0x51,
        0x7F,
        0xA9,
        0x19,
        0xB5,
        0x4A,
        0x0D,
        0x2D,
        0xE5,
        0x7A,
        0x9F,
        0x93,
        0xC9,
        0x9C,
        0xEF,
        0xA0,
        0xE0,
        0x3B,
        0x4D,
        0xAE,
        0x2A,
        0xF5,
        0xB0,
        0xC8,
        0xEB,
        0xBB,
        0x3C,
        0x83,
        0x53,
        0x99,
        0x61,
        0x17,
        0x2B,
        0x04,
        0x7E,
        0xBA,
        0x77,
        0xD6,
        0x26,
        0xE1,
        0x69,
        0x14,
        0x63,
        0x55,
        0x21,
        0x0C,
        0x7D,
    ]
)

_RCON = [0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36]


def _gf_mul(a: int, b: int) -> int:
    p = 0
    a &= 0xFF
    b &= 0xFF
    for _ in range(8):
        if b & 1:
            p ^= a
        hi = a & 0x80
        a = (a << 1) & 0xFF
        if hi:
            a ^= 0x1B
        b >>= 1
    return p


def _aes128_expand_key(key16: bytes) -> list[bytes]:
    if len(key16) != 16:
        raise ValueError("AES-128 key must be 16 bytes")
    w = [0] * 44
    for i in range(4):
        w[i] = int.from_bytes(key16[4 * i : 4 * i + 4], "big")
    for i in range(4, 44):
        temp = w[i - 1]
        if i % 4 == 0:
            temp = ((temp << 8) & 0xFFFFFFFF) | ((temp >> 24) & 0xFF)
            temp = (
                (_SBOX[(temp >> 24) & 0xFF] << 24)
                | (_SBOX[(temp >> 16) & 0xFF] << 16)
                | (_SBOX[(temp >> 8) & 0xFF] << 8)
                | (_SBOX[temp & 0xFF])
            )
            temp ^= _RCON[(i // 4) - 1] << 24
        w[i] = w[i - 4] ^ temp
    round_keys = []
    for r in range(11):
        rk = b"".join(w[4 * r + j].to_bytes(4, "big") for j in range(4))
        round_keys.append(rk)
    return round_keys


def _inv_shift_rows(state: list[int]) -> None:
    state[1], state[5], state[9], state[13] = state[13], state[1], state[5], state[9]
    state[2], state[6], state[10], state[14] = state[10], state[14], state[2], state[6]
    state[3], state[7], state[11], state[15] = state[7], state[11], state[15], state[3]


def _inv_sub_bytes(state: list[int]) -> None:
    for i in range(16):
        state[i] = _INV_SBOX[state[i]]


def _add_round_key(state: list[int], rk: bytes) -> None:
    for i in range(16):
        state[i] ^= rk[i]


def _inv_mix_columns(state: list[int]) -> None:
    for c in range(4):
        i = 4 * c
        a0, a1, a2, a3 = state[i], state[i + 1], state[i + 2], state[i + 3]
        state[i + 0] = _gf_mul(a0, 14) ^ _gf_mul(a1, 11) ^ _gf_mul(a2, 13) ^ _gf_mul(a3, 9)
        state[i + 1] = _gf_mul(a0, 9) ^ _gf_mul(a1, 14) ^ _gf_mul(a2, 11) ^ _gf_mul(a3, 13)
        state[i + 2] = _gf_mul(a0, 13) ^ _gf_mul(a1, 9) ^ _gf_mul(a2, 14) ^ _gf_mul(a3, 11)
        state[i + 3] = _gf_mul(a0, 11) ^ _gf_mul(a1, 13) ^ _gf_mul(a2, 9) ^ _gf_mul(a3, 14)


def _aes128_decrypt_block(block16: bytes, round_keys: list[bytes]) -> bytes:
    state = list(block16)
    _add_round_key(state, round_keys[10])
    for r in range(9, 0, -1):
        _inv_shift_rows(state)
        _inv_sub_bytes(state)
        _add_round_key(state, round_keys[r])
        _inv_mix_columns(state)
    _inv_shift_rows(state)
    _inv_sub_bytes(state)
    _add_round_key(state, round_keys[0])
    return bytes(state)


def sub_118160_cbc_decrypt_inplace(buf: bytearray, *, offset: int, length: int, key16: bytes) -> None:
    if length <= 0:
        return
    if offset < 0 or length < 0 or offset + length > len(buf):
        raise ValueError("range out of bounds")
    if length % 16 != 0:
        raise ValueError("sub_118160 use-case expects 16-byte multiple length")

    round_keys = _aes128_expand_key(key16)
    iv = bytes(range(0x02, 0x12))
    end = offset + length
    for pos in range(offset, end, 16):
        c = bytes(buf[pos : pos + 16])
        p = _aes128_decrypt_block(c, round_keys)
        out = bytes((p[i] ^ iv[i]) for i in range(16))
        buf[pos : pos + 16] = out
        iv = c


def sub_118408_inplace(buf: bytearray, *, offset: int, length: int, a3: int) -> None:
    if length < 0x4000:
        raise ValueError("length must be >= 0x4000 for 118408")
    key16 = f"{a3 & 0xFFFFFFFF:08x}{a3 & 0xFFFFFFFF:08x}".encode("ascii")
    for block in range(8):
        sub_118160_cbc_decrypt_inplace(
            buf, offset=offset + block * 0x800, length=0x800, key16=key16
        )
    sub_12fa6c_inplace(buf, offset=offset + 0x4000, length=length - 0x4000, a3=a3)


def sub_118360_inplace(buf: bytearray, *, offset: int, length: int, a3: int, a4: int) -> None:
    if length < 0x10000:
        raise ValueError("length must be >= 0x10000 for 118360")
    key16 = f"{a3 & 0xFFFFFFFF:08x}{a3 & 0xFFFFFFFF:08x}".encode("ascii")
    for block in range(8):
        sub_118160_cbc_decrypt_inplace(
            buf, offset=offset + block * 0x800, length=0x800, key16=key16
        )
    sub_12fa98_inplace(buf, offset=offset + 0x10000, length=length - 0x10000, a3=a3, a4=a4)


def _elf64_section_range(data: bytes, name: str) -> tuple[int, int]:
    import struct

    if data[:4] != b"\x7fELF" or data[4] != 2:
        raise ValueError("not an ELF64 file")
    endian = "<" if data[5] == 1 else ">"
    (
        _e_type,
        _e_machine,
        _e_version,
        _e_entry,
        _e_phoff,
        e_shoff,
        _e_flags,
        _e_ehsize,
        _e_phentsize,
        _e_phnum,
        e_shentsize,
        e_shnum,
        e_shstrndx,
    ) = struct.unpack_from(endian + "HHIQQQIHHHHHH", data, 0x10)
    if e_shoff == 0 or e_shnum == 0:
        raise ValueError("ELF has no section headers")

    def read_sh(i: int) -> tuple[int, int, int, int, int, int, int, int, int, int]:
        off = e_shoff + i * e_shentsize
        return struct.unpack_from(endian + "IIQQQQIIQQ", data, off)

    shstr = read_sh(e_shstrndx)
    shstr_off, shstr_sz = shstr[4], shstr[5]
    shstrtab = data[shstr_off : shstr_off + shstr_sz]

    def cstr(off: int) -> str:
        if off < 0 or off >= len(shstrtab):
            return ""
        return shstrtab[off:].split(b"\x00", 1)[0].decode("ascii", "ignore")

    for i in range(e_shnum):
        sh = read_sh(i)
        if cstr(sh[0]) == name:
            return sh[4], sh[5]
    raise ValueError(f"section not found: {name}")


@dataclass(frozen=True)
class Args:
    inp: pathlib.Path
    out: pathlib.Path
    algo: str
    section: str | None
    offset: int
    length: int | None
    a3: int
    a4: int
    align_2000: bool


def _parse_args() -> Args:
    p = argparse.ArgumentParser(add_help=True)
    p.add_argument("--in", dest="inp", required=True)
    p.add_argument("--out", dest="out", required=True)
    p.add_argument("--algo", choices=["12FA6C", "12FA98", "118408", "118360"], required=True)
    p.add_argument("--section", default=None)
    p.add_argument("--offset", type=lambda s: int(s, 0), default=0)
    p.add_argument("--length", type=lambda s: int(s, 0), default=None)
    p.add_argument("--a3", type=lambda s: int(s, 0), required=True)
    p.add_argument("--a4", type=lambda s: int(s, 0), default=0)
    p.add_argument("--align-2000", action="store_true")
    ns = p.parse_args()
    return Args(
        inp=pathlib.Path(ns.inp),
        out=pathlib.Path(ns.out),
        algo=ns.algo,
        section=ns.section,
        offset=ns.offset,
        length=ns.length,
        a3=ns.a3,
        a4=ns.a4,
        align_2000=bool(ns.align_2000),
    )


def main() -> int:
    args = _parse_args()
    data = bytearray(args.inp.read_bytes())
    if args.section is not None:
        off, sz = _elf64_section_range(data, args.section)
        offset = off
        length = sz
    else:
        offset = args.offset
        length = args.length if args.length is not None else len(data) - offset

    if args.align_2000:
        delta = 0x2000 - (offset & 0xFFF)
        if delta != 0x2000:
            offset += delta
            length -= delta

    if args.algo == "12FA6C":
        sub_12fa6c_inplace(data, offset=offset, length=length, a3=args.a3)
    elif args.algo == "12FA98":
        sub_12fa98_inplace(data, offset=offset, length=length, a3=args.a3, a4=args.a4)
    elif args.algo == "118408":
        sub_118408_inplace(data, offset=offset, length=length, a3=args.a3)
    else:
        sub_118360_inplace(data, offset=offset, length=length, a3=args.a3, a4=args.a4)

    args.out.write_bytes(data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
