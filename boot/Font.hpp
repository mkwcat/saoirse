#pragma once

#include <System/Types.h>

extern const u8 font[128][16];

namespace Font
{

u8 GetGlyphWidth();

u8 GetGlyphHeight();

const u8* GetGlyph(char c);

} // namespace Font
