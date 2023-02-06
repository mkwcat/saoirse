#pragma once

#include <optional>

#include <System/Types.h>

class Sel
{
public:
    struct Symbol {
        u32 section;
        u32 offset;
    };

    Sel(const u8* data, u32 size);

    std::optional<Symbol> GetSymbol(const char* name);

private:
    template <typename T>
    T Read(u32 offset) const
    {
        T val = 0;
#pragma GCC unroll(8)
        for (size_t i = 0; i < sizeof(T); i++) {
            val |= static_cast<T>(m_data[offset + i])
                   << (8 * (sizeof(T) - i - 1));
        }
        return val;
    }

    std::optional<const char*> GetString(u32 offset) const;

    static u32 GetHash(const char* name);

    const u8* m_data;
    u32 m_size;

    u32 m_sectCount;
    u32 m_sectOffset;

    u32 m_symTabStart;
    u32 m_symTabCount;
    u32 m_strTab;
};
