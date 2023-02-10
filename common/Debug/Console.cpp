#include "Console.hpp"

#include "Font.hpp"
#include <Boot/AddressMap.hpp>
#include <Debug/Debug_VI.hpp>

namespace Console
{

static const u8 bg = 16;
static const u8 fg = 235;
static u8 cols;
static u8 rows;
static u8 col;
static bool newline = false;

void Init()
{
    cols = Debug_VI::GetXFBWidth() / Font::GetGlyphWidth() - 1;
    rows = Debug_VI::GetXFBHeight() / Font::GetGlyphHeight() - 2;
    col = 0;

    Print("\n");
}

#ifdef TARGET_IOS

#  include <IOS/Syscalls.h>

static void Lock()
{
    auto data = reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS);

    for (u32 i = 0; i < 16;) {
        IOS_InvalidateDCache(data, sizeof(Boot_ConsoleData));

        u32 lock = data->lock;

        // Check if PPC has locked it
        if (lock & Boot_ConsoleData::PPC_LOCK) {
            i = 0;
            continue;
        }

        data->lock = lock | Boot_ConsoleData::IOS_LOCK;
        IOS_FlushDCache(data, sizeof(Boot_ConsoleData));
        i++;
    }
}

static void Unlock()
{
    auto data = reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS);
    IOS_InvalidateDCache(data, sizeof(Boot_ConsoleData));

    data->lock = 0;

    IOS_FlushDCache(data, sizeof(Boot_ConsoleData));
}

static u8 GetRow()
{
    auto data = reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS);
    IOS_InvalidateDCache(data, sizeof(Boot_ConsoleData));

    return data->iosRow;
}

static s32 IncrementRow()
{
    auto data = reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS);
    IOS_InvalidateDCache(data, sizeof(Boot_ConsoleData));

    data->iosRow++;
    if (data->iosRow <= data->ppcRow) {
        data->iosRow = data->ppcRow + 1;
    }

    IOS_FlushDCache(data, sizeof(Boot_ConsoleData));

    return data->iosRow;
}

static s32 DecrementRow()
{
    auto data = reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS);
    IOS_InvalidateDCache(data, sizeof(Boot_ConsoleData));

    data->iosRow--;
    data->ppcRow--;

    IOS_FlushDCache(data, sizeof(Boot_ConsoleData));

    return data->iosRow;
}

#else

static void Lock()
{
    auto data =
      reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS | 0xC0000000);

    for (u32 i = 0; i < 8;) {
        u32 lock = data->lock;

        // Check if PPC has locked it
        if (lock & Boot_ConsoleData::IOS_LOCK) {
            i = 0;
            continue;
        }

        data->lock = lock | Boot_ConsoleData::PPC_LOCK;
        i++;
    }
}

static void Unlock()
{
    auto data =
      reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS | 0xC0000000);

    data->lock = 0;
}

static s32 GetRow()
{
    auto data =
      reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS | 0xC0000000);

    return data->ppcRow;
}

static s32 IncrementRow()
{
    auto data =
      reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS | 0xC0000000);

    data->ppcRow++;
    if (data->ppcRow <= data->iosRow) {
        data->ppcRow = data->iosRow + 1;
    }

    return data->iosRow;
}

static s32 DecrementRow()
{
    auto data =
      reinterpret_cast<Boot_ConsoleData*>(CONSOLE_DATA_ADDRESS | 0xC0000000);

    data->iosRow--;
    data->ppcRow--;

    return data->ppcRow;
}

#endif

static void Print(char c)
{
    if (col >= cols) {
        return;
    }

    if (c == '\n') {
        if (newline) {
            IncrementRow();
        }

        col = 0;
        newline = true;
        return;
    }

    if (c == '\r') {
        col = 0;
        return;
    }

    if (newline) {
        IncrementRow();
        newline = false;
    }

    s32 row = GetRow();

    if (row < 0) {
        return;
    }

    while (row >= rows) {
        u16 xfbWidth = Debug_VI::GetXFBWidth();
        u8 glyphHeight = Font::GetGlyphHeight();
        for (u8 row = 0; row < rows; row++) {
            u16 y0 = row * glyphHeight + glyphHeight / 2;
            for (u16 y = 0; y < glyphHeight; y++) {
                for (u16 x = 0; x < xfbWidth; x++) {
                    u8 intensity =
                      Debug_VI::ReadGrayscaleFromXFB(x, y0 + glyphHeight + y);
                    Debug_VI::WriteGrayscaleToXFB(x, y0 + y, intensity);
                }
            }
        }
        row = DecrementRow();
    }

    u8 glyphWidth = Font::GetGlyphWidth();
    u8 glyphHeight = Font::GetGlyphHeight();
    const u8* glyph = Font::GetGlyph(c);
    u16 y0 = row * glyphHeight + glyphHeight / 2;
    for (u16 y = 0; y < glyphHeight; y++) {
        u16 x0 = col * glyphWidth + glyphWidth / 2;
        for (u16 x = 0; x < glyphWidth; x++) {
            u8 intensity =
              glyph[(y * glyphWidth + x) / 8] & (1 << (8 - (x % 8))) ? fg : bg;
            Debug_VI::WriteGrayscaleToXFB(x0 + x, y0 + y, intensity);
        }
    }

    col++;
}

void Print(const char* s)
{
    Lock();
    for (; *s; s++) {
        Print(*s);
    }
    Debug_VI::FlushXFB();
    Unlock();
}

void Print(u32 val)
{
    Lock();
    for (u32 i = 0; i < 8; i++) {
        u32 digit = val >> ((7 - i) * 4) & 0xf;
        if (digit < 0xa) {
            Print(static_cast<char>(digit + '0'));
        } else {
            Print(static_cast<char>(digit - 0xa + 'A'));
        }
    }
    Debug_VI::FlushXFB();
    Unlock();
}

} // namespace Console
