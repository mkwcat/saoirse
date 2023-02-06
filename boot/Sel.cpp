#include "Sel.hpp"

#include <algorithm>

Sel::Sel(const u8* data, u32 size)
  : m_data(data)
  , m_size(size)
{
    m_sectCount = Read<u32>(0x8);
    m_sectOffset = Read<u32>(0xC);

    m_symTabStart = Read<u32>(0x40);
    m_symTabCount = Read<u32>(0x44) / 0x10;

    m_strTab = Read<u32>(0x48);
}

std::optional<Sel::Symbol> Sel::GetSymbol(const char* name)
{
    u32 hash = GetHash(name);

    // TODO: Fix this
    u32 index = reinterpret_cast<u32>(std::lower_bound(
      (u8*) 0, (u8*) m_symTabCount, hash, [&](const u8& index, u32 hash) {
          return Read<u32>(m_symTabStart +
                           reinterpret_cast<u32>(&index) * 0x10 + 0xC) < hash;
      }));

    if (index == m_symTabCount) {
        return {};
    }

    u32 offset = m_symTabStart + index * 0x10;
    if (Read<u32>(offset + 0xC) != hash) {
        return {};
    }

    return Symbol({
      .section = Read<u32>(offset + 0x8),
      .offset = Read<u32>(offset + 0x4),
    });
}

std::optional<const char*> Sel::GetString(u32 offset) const
{
    const char* string =
      reinterpret_cast<const char*>(m_data + m_strTab + offset);
    for (; offset < m_size; offset++) {
        if (Read<char>(offset) == '\0') {
            return string;
        }
    }

    return {};
}

u32 Sel::GetHash(const char* name)
{
    u32 hash = 0;

    while (*name != 0) {
        u32 c = *name++;

        hash = hash * 0x10 + c;
        if (hash & 0xF0000000) {
            hash ^= (hash >> 24) & 0xF0;
        }
        hash &= ~(hash & 0xF0000000);
    }

    return hash;
}
