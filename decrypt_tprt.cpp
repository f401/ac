#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

static uint8_t key_byte_from_a3(uint32_t a3) {
  if ((a3 & 0x00FF0000u) == 0) {
    return 0x87u;
  }
  return static_cast<uint8_t>((a3 >> 16) & 0xFFu);
}

static std::vector<uint8_t> read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("failed to open input");
  }
  in.seekg(0, std::ios::end);
  std::streamoff size = in.tellg();
  if (size < 0) {
    throw std::runtime_error("failed to stat input");
  }
  in.seekg(0, std::ios::beg);
  std::vector<uint8_t> data(static_cast<size_t>(size));
  if (!data.empty()) {
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!in) {
      throw std::runtime_error("failed to read input");
    }
  }
  return data;
}

static void write_file(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    throw std::runtime_error("failed to open output");
  }
  if (!data.empty()) {
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!out) {
      throw std::runtime_error("failed to write output");
    }
  }
}

static void sub_12fa6c_inplace(std::vector<uint8_t>& buf, size_t offset, size_t length, uint32_t a3) {
  if (length == 0) {
    return;
  }
  if (offset > buf.size() || length > buf.size() - offset) {
    throw std::runtime_error("range out of bounds");
  }
  const uint8_t key = key_byte_from_a3(a3);
  const size_t end = offset + length;
  for (size_t i = offset; i < end; i++) {
    buf[i] ^= key;
  }
}

static std::vector<uint32_t> nibble_indices_from_a4(uint32_t a4) {
  return {
      (a4 >> 28) & 0xFu,
      (a4 >> 24) & 0xFu,
      (a4 >> 20) & 0xFu,
      (a4 >> 16) & 0xFu,
      (a4 >> 12) & 0xFu,
      (a4 >> 8) & 0xFu,
      (a4 >> 4) & 0xFu,
      (a4 >> 0) & 0xFu,
  };
}

static void sub_12fa98_inplace(std::vector<uint8_t>& buf, size_t offset, size_t length, uint32_t a3, uint32_t a4) {
  if (length == 0) {
    return;
  }
  if (offset > buf.size() || length > buf.size() - offset) {
    throw std::runtime_error("range out of bounds");
  }

  const uint8_t key = key_byte_from_a3(a3);
  const size_t full_chunks = length / 0x10000u;
  const size_t remainder = length - full_chunks * 0x10000u;

  std::vector<uint8_t> scratch;
  std::vector<uint32_t> indices;
  if (a4 != 0) {
    scratch.assign(0x20000u, 0);
    indices = nibble_indices_from_a4(a4);
  }

  for (size_t chunk_index = 0; chunk_index < full_chunks; chunk_index++) {
    const size_t chunk_base = offset + chunk_index * 0x10000u;
    for (size_t j = 0; j < 0x4000u; j++) {
      buf[chunk_base + j] ^= key;
    }

    if (!scratch.empty()) {
      const size_t block_index = chunk_index & 7u;
      const size_t scratch_off = block_index * 0x4000u;
      std::copy_n(buf.begin() + static_cast<std::ptrdiff_t>(chunk_base), 0x4000u, scratch.begin() + static_cast<std::ptrdiff_t>(scratch_off));

      if (block_index == 7u) {
        const size_t group_base = chunk_base - 0x70000u;
        for (size_t k = 0; k < 8; k++) {
          const uint32_t dest_idx = indices[k];
          const size_t dst = group_base + static_cast<size_t>(dest_idx) * 0x10000u;
          const size_t src = k * 0x4000u;
          std::copy_n(scratch.begin() + static_cast<std::ptrdiff_t>(src), 0x4000u, buf.begin() + static_cast<std::ptrdiff_t>(dst));
        }
        std::fill(scratch.begin(), scratch.end(), 0);
      }
    }
  }

  if (remainder != 0) {
    const size_t tail_base = offset + full_chunks * 0x10000u;
    const size_t end = tail_base + remainder;
    for (size_t i = tail_base; i < end; i++) {
      buf[i] ^= key;
    }
  }
}

static constexpr std::array<uint8_t, 256> SBOX = {
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
    0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0, 0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0, 0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
    0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C, 0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
    0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E, 0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16,
};

static constexpr std::array<uint8_t, 256> INV_SBOX = {
    0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38, 0xBF, 0x40, 0xA3, 0x9E, 0x81, 0xF3, 0xD7, 0xFB,
    0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87, 0x34, 0x8E, 0x43, 0x44, 0xC4, 0xDE, 0xE9, 0xCB,
    0x54, 0x7B, 0x94, 0x32, 0xA6, 0xC2, 0x23, 0x3D, 0xEE, 0x4C, 0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E,
    0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2, 0x76, 0x5B, 0xA2, 0x49, 0x6D, 0x8B, 0xD1, 0x25,
    0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92,
    0x6C, 0x70, 0x48, 0x50, 0xFD, 0xED, 0xB9, 0xDA, 0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84,
    0x90, 0xD8, 0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A, 0xF7, 0xE4, 0x58, 0x05, 0xB8, 0xB3, 0x45, 0x06,
    0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02, 0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13, 0x8A, 0x6B,
    0x3A, 0x91, 0x11, 0x41, 0x4F, 0x67, 0xDC, 0xEA, 0x97, 0xF2, 0xCF, 0xCE, 0xF0, 0xB4, 0xE6, 0x73,
    0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85, 0xE2, 0xF9, 0x37, 0xE8, 0x1C, 0x75, 0xDF, 0x6E,
    0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89, 0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B,
    0xFC, 0x56, 0x3E, 0x4B, 0xC6, 0xD2, 0x79, 0x20, 0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4,
    0x1F, 0xDD, 0xA8, 0x33, 0x88, 0x07, 0xC7, 0x31, 0xB1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xEC, 0x5F,
    0x60, 0x51, 0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D, 0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF,
    0xA0, 0xE0, 0x3B, 0x4D, 0xAE, 0x2A, 0xF5, 0xB0, 0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26, 0xE1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0C, 0x7D,
};

static constexpr std::array<uint8_t, 10> RCON = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36};

static uint8_t gf_mul(uint8_t a, uint8_t b) {
  uint8_t p = 0;
  for (int i = 0; i < 8; i++) {
    if (b & 1u) {
      p ^= a;
    }
    const uint8_t hi = a & 0x80u;
    a = static_cast<uint8_t>(a << 1u);
    if (hi) {
      a ^= 0x1Bu;
    }
    b = static_cast<uint8_t>(b >> 1u);
  }
  return p;
}

static std::array<std::array<uint8_t, 16>, 11> aes128_expand_key(const std::array<uint8_t, 16>& key) {
  std::array<uint32_t, 44> w{};
  for (size_t i = 0; i < 4; i++) {
    w[i] = (static_cast<uint32_t>(key[i * 4]) << 24) | (static_cast<uint32_t>(key[i * 4 + 1]) << 16) |
           (static_cast<uint32_t>(key[i * 4 + 2]) << 8) | static_cast<uint32_t>(key[i * 4 + 3]);
  }
  for (size_t i = 4; i < 44; i++) {
    uint32_t temp = w[i - 1];
    if (i % 4 == 0) {
      temp = (temp << 8) | ((temp >> 24) & 0xFFu);
      temp = (static_cast<uint32_t>(SBOX[(temp >> 24) & 0xFFu]) << 24) |
             (static_cast<uint32_t>(SBOX[(temp >> 16) & 0xFFu]) << 16) |
             (static_cast<uint32_t>(SBOX[(temp >> 8) & 0xFFu]) << 8) | static_cast<uint32_t>(SBOX[temp & 0xFFu]);
      temp ^= static_cast<uint32_t>(RCON[(i / 4) - 1]) << 24;
    }
    w[i] = w[i - 4] ^ temp;
  }
  std::array<std::array<uint8_t, 16>, 11> round_keys{};
  for (size_t r = 0; r < 11; r++) {
    std::array<uint8_t, 16> rk{};
    for (size_t j = 0; j < 4; j++) {
      const uint32_t word = w[r * 4 + j];
      rk[j * 4 + 0] = static_cast<uint8_t>((word >> 24) & 0xFFu);
      rk[j * 4 + 1] = static_cast<uint8_t>((word >> 16) & 0xFFu);
      rk[j * 4 + 2] = static_cast<uint8_t>((word >> 8) & 0xFFu);
      rk[j * 4 + 3] = static_cast<uint8_t>((word >> 0) & 0xFFu);
    }
    round_keys[r] = rk;
  }
  return round_keys;
}

static void add_round_key(std::array<uint8_t, 16>& state, const std::array<uint8_t, 16>& rk) {
  for (size_t i = 0; i < 16; i++) {
    state[i] ^= rk[i];
  }
}

static void inv_shift_rows(std::array<uint8_t, 16>& s) {
  const std::array<uint8_t, 16> t = s;
  s[1] = t[13];
  s[5] = t[1];
  s[9] = t[5];
  s[13] = t[9];
  s[2] = t[10];
  s[6] = t[14];
  s[10] = t[2];
  s[14] = t[6];
  s[3] = t[7];
  s[7] = t[11];
  s[11] = t[15];
  s[15] = t[3];
}

static void inv_sub_bytes(std::array<uint8_t, 16>& s) {
  for (auto& b : s) {
    b = INV_SBOX[b];
  }
}

static void inv_mix_columns(std::array<uint8_t, 16>& s) {
  for (size_t c = 0; c < 4; c++) {
    const size_t i = c * 4;
    const uint8_t a0 = s[i + 0];
    const uint8_t a1 = s[i + 1];
    const uint8_t a2 = s[i + 2];
    const uint8_t a3 = s[i + 3];
    s[i + 0] = static_cast<uint8_t>(gf_mul(a0, 14) ^ gf_mul(a1, 11) ^ gf_mul(a2, 13) ^ gf_mul(a3, 9));
    s[i + 1] = static_cast<uint8_t>(gf_mul(a0, 9) ^ gf_mul(a1, 14) ^ gf_mul(a2, 11) ^ gf_mul(a3, 13));
    s[i + 2] = static_cast<uint8_t>(gf_mul(a0, 13) ^ gf_mul(a1, 9) ^ gf_mul(a2, 14) ^ gf_mul(a3, 11));
    s[i + 3] = static_cast<uint8_t>(gf_mul(a0, 11) ^ gf_mul(a1, 13) ^ gf_mul(a2, 9) ^ gf_mul(a3, 14));
  }
}

static std::array<uint8_t, 16> aes128_decrypt_block(const std::array<uint8_t, 16>& in, const std::array<std::array<uint8_t, 16>, 11>& rks) {
  std::array<uint8_t, 16> s = in;
  add_round_key(s, rks[10]);
  for (int r = 9; r >= 1; r--) {
    inv_shift_rows(s);
    inv_sub_bytes(s);
    add_round_key(s, rks[static_cast<size_t>(r)]);
    inv_mix_columns(s);
  }
  inv_shift_rows(s);
  inv_sub_bytes(s);
  add_round_key(s, rks[0]);
  return s;
}

static void sub_118160_cbc_decrypt_inplace(std::vector<uint8_t>& buf, size_t offset, size_t length, const std::array<uint8_t, 16>& key16) {
  if (length == 0) {
    return;
  }
  if (offset > buf.size() || length > buf.size() - offset) {
    throw std::runtime_error("range out of bounds");
  }
  if ((length % 16u) != 0u) {
    throw std::runtime_error("sub_118160 use-case expects 16-byte multiple length");
  }
  const auto rks = aes128_expand_key(key16);
  std::array<uint8_t, 16> iv{};
  for (size_t i = 0; i < 16; i++) {
    iv[i] = static_cast<uint8_t>(0x02u + i);
  }
  for (size_t pos = offset; pos < offset + length; pos += 16) {
    std::array<uint8_t, 16> c{};
    std::memcpy(c.data(), buf.data() + static_cast<std::ptrdiff_t>(pos), 16);
    const auto p = aes128_decrypt_block(c, rks);
    for (size_t i = 0; i < 16; i++) {
      buf[pos + i] = static_cast<uint8_t>(p[i] ^ iv[i]);
    }
    iv = c;
  }
}

static std::array<uint8_t, 16> key16_from_a3(uint32_t a3) {
  char s[17];
  std::snprintf(s, sizeof(s), "%08x%08x", a3, a3);
  std::array<uint8_t, 16> k{};
  for (size_t i = 0; i < 16; i++) {
    k[i] = static_cast<uint8_t>(s[i]);
  }
  return k;
}

static void sub_118408_inplace(std::vector<uint8_t>& buf, size_t offset, size_t length, uint32_t a3) {
  if (length < 0x4000u) {
    throw std::runtime_error("length must be >= 0x4000 for 118408");
  }
  const auto key = key16_from_a3(a3);
  for (size_t block = 0; block < 8; block++) {
    sub_118160_cbc_decrypt_inplace(buf, offset + block * 0x800u, 0x800u, key);
  }
  sub_12fa6c_inplace(buf, offset + 0x4000u, length - 0x4000u, a3);
}

static void sub_118360_inplace(std::vector<uint8_t>& buf, size_t offset, size_t length, uint32_t a3, uint32_t a4) {
  if (length < 0x10000u) {
    throw std::runtime_error("length must be >= 0x10000 for 118360");
  }
  const auto key = key16_from_a3(a3);
  for (size_t block = 0; block < 8; block++) {
    sub_118160_cbc_decrypt_inplace(buf, offset + block * 0x800u, 0x800u, key);
  }
  sub_12fa98_inplace(buf, offset + 0x10000u, length - 0x10000u, a3, a4);
}

struct ElfSection {
  size_t offset = 0;
  size_t size = 0;
};

static ElfSection elf64_find_section(const std::vector<uint8_t>& data, const std::string& name) {
  if (data.size() < 0x40 || std::memcmp(data.data(), "\x7f""ELF", 4) != 0 || data[4] != 2) {
    throw std::runtime_error("not an ELF64 file");
  }
  const bool le = (data[5] == 1);
  auto rd16 = [&](size_t off) -> uint16_t {
    if (off + 2 > data.size()) throw std::runtime_error("ELF parse out of range");
    uint16_t v;
    std::memcpy(&v, data.data() + static_cast<std::ptrdiff_t>(off), 2);
    if (!le) v = static_cast<uint16_t>((v >> 8) | (v << 8));
    return v;
  };
  auto rd32 = [&](size_t off) -> uint32_t {
    if (off + 4 > data.size()) throw std::runtime_error("ELF parse out of range");
    uint32_t v;
    std::memcpy(&v, data.data() + static_cast<std::ptrdiff_t>(off), 4);
    if (!le) v = ((v & 0xFFu) << 24) | ((v & 0xFF00u) << 8) | ((v & 0xFF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
    return v;
  };
  auto rd64 = [&](size_t off) -> uint64_t {
    if (off + 8 > data.size()) throw std::runtime_error("ELF parse out of range");
    uint64_t v;
    std::memcpy(&v, data.data() + static_cast<std::ptrdiff_t>(off), 8);
    if (!le) {
      v = ((v & 0x00000000000000FFull) << 56) | ((v & 0x000000000000FF00ull) << 40) | ((v & 0x0000000000FF0000ull) << 24) |
          ((v & 0x00000000FF000000ull) << 8) | ((v & 0x000000FF00000000ull) >> 8) | ((v & 0x0000FF0000000000ull) >> 24) |
          ((v & 0x00FF000000000000ull) >> 40) | ((v & 0xFF00000000000000ull) >> 56);
    }
    return v;
  };

  const uint64_t e_shoff = rd64(0x28);
  const uint16_t e_shentsize = rd16(0x3A);
  const uint16_t e_shnum = rd16(0x3C);
  const uint16_t e_shstrndx = rd16(0x3E);
  if (e_shoff == 0 || e_shentsize == 0 || e_shnum == 0) {
    throw std::runtime_error("ELF has no section headers");
  }
  auto sh_at = [&](uint16_t i) -> size_t { return static_cast<size_t>(e_shoff) + static_cast<size_t>(i) * static_cast<size_t>(e_shentsize); };
  const size_t shstr_off = sh_at(e_shstrndx);
  const uint64_t shstrtab_off = rd64(shstr_off + 0x18);
  const uint64_t shstrtab_sz = rd64(shstr_off + 0x20);
  if (shstrtab_off + shstrtab_sz > data.size()) {
    throw std::runtime_error("invalid shstrtab range");
  }
  const uint8_t* shstrtab = data.data() + static_cast<std::ptrdiff_t>(shstrtab_off);
  const size_t shstrtab_len = static_cast<size_t>(shstrtab_sz);

  auto get_name = [&](uint32_t name_off) -> std::string {
    if (name_off >= shstrtab_len) {
      return "";
    }
    const char* s = reinterpret_cast<const char*>(shstrtab + name_off);
    return std::string(s, std::strnlen(s, shstrtab_len - name_off));
  };

  for (uint16_t i = 0; i < e_shnum; i++) {
    const size_t off = sh_at(i);
    const uint32_t sh_name = rd32(off + 0x00);
    if (get_name(sh_name) != name) {
      continue;
    }
    const uint64_t sh_offset = rd64(off + 0x18);
    const uint64_t sh_size = rd64(off + 0x20);
    if (sh_offset + sh_size > data.size()) {
      throw std::runtime_error("invalid section range");
    }
    return ElfSection{static_cast<size_t>(sh_offset), static_cast<size_t>(sh_size)};
  }
  throw std::runtime_error("section not found: " + name);
}

static uint64_t parse_u64(const std::string& s) {
  size_t idx = 0;
  uint64_t v = std::stoull(s, &idx, 0);
  if (idx != s.size()) {
    throw std::runtime_error("invalid integer: " + s);
  }
  return v;
}

int main(int argc, char** argv) {
  try {
    std::filesystem::path inp;
    std::filesystem::path out;
    std::string algo;
    std::string section;
    uint64_t offset = 0;
    uint64_t length = 0;
    bool length_set = false;
    uint64_t a3 = 0;
    uint64_t a4 = 0;
    bool align_2000 = false;

    for (int i = 1; i < argc; i++) {
      const std::string arg = argv[i];
      auto require_value = [&](const char* name) -> std::string {
        if (i + 1 >= argc) {
          throw std::runtime_error(std::string("missing value for ") + name);
        }
        return std::string(argv[++i]);
      };

      if (arg == "--in") {
        inp = require_value("--in");
      } else if (arg == "--out") {
        out = require_value("--out");
      } else if (arg == "--algo") {
        algo = require_value("--algo");
      } else if (arg == "--section") {
        section = require_value("--section");
      } else if (arg == "--offset") {
        offset = parse_u64(require_value("--offset"));
      } else if (arg == "--length") {
        length = parse_u64(require_value("--length"));
        length_set = true;
      } else if (arg == "--a3") {
        a3 = parse_u64(require_value("--a3"));
      } else if (arg == "--a4") {
        a4 = parse_u64(require_value("--a4"));
      } else if (arg == "--align-2000") {
        align_2000 = true;
      } else {
        throw std::runtime_error("unknown arg: " + arg);
      }
    }

    if (inp.empty() || out.empty() || algo.empty()) {
      throw std::runtime_error(
          "required: --in <path> --out <path> --algo <12FA6C|12FA98|118408|118360> --a3 <int> [--a4 <int>] [--section <name>] [--offset <int>] [--length <int>] [--align-2000]");
    }

    std::vector<uint8_t> data = read_file(inp);
    size_t off = static_cast<size_t>(offset);
    size_t len = length_set ? static_cast<size_t>(length) : (data.size() - off);
    if (!section.empty()) {
      const auto sec = elf64_find_section(data, section);
      off = sec.offset;
      len = sec.size;
    }
    if (align_2000) {
      const size_t delta = 0x2000u - (off & 0xFFFu);
      if (delta != 0x2000u) {
        off += delta;
        if (len < delta) {
          throw std::runtime_error("aligned range underflow");
        }
        len -= delta;
      }
    }

    if (algo == "12FA6C") {
      sub_12fa6c_inplace(data, off, len, static_cast<uint32_t>(a3));
    } else if (algo == "12FA98") {
      sub_12fa98_inplace(data, off, len, static_cast<uint32_t>(a3), static_cast<uint32_t>(a4));
    } else if (algo == "118408") {
      sub_118408_inplace(data, off, len, static_cast<uint32_t>(a3));
    } else if (algo == "118360") {
      sub_118360_inplace(data, off, len, static_cast<uint32_t>(a3), static_cast<uint32_t>(a4));
    } else {
      throw std::runtime_error("unsupported algo");
    }

    write_file(out, data);
    return 0;
  } catch (const std::exception& e) {
    const std::string msg = e.what();
    std::fwrite(msg.c_str(), 1, msg.size(), stderr);
    std::fwrite("\n", 1, 1, stderr);
    return 2;
  }
}
