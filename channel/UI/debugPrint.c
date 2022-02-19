/*---------------------------------------------------------------------------*
 * Author  : Star
 * Date    : 15 Feb 2022
 * File    : debugPrint.c
 * Version : 1.0.0.2
 *---------------------------------------------------------------------------*/

#include "debugPrint.h"
#include <stdarg.h>
#include <stdio.h>

// os.h
//! Definitions
#define OS_CACHED_BASE   0x80000000
#define OS_UNCACHED_BASE 0xC0000000

// vi.h
//! Definitions
#define VI_REG_BASE        0xCC002000
#define VI_VTR_REG_OFFSET  0x00000000
#define VI_TFBL_REG_OFFSET 0x0000001C
#define VI_HSW_REG_OFFSET  0x00000048

#define VI_TFBL_REG_PAGE_OFFSET_BIT (1 << 28)

static void* sXFB;
static unsigned short sXFBWidth;
static unsigned short sXFBHeight;

//! Static Definitions
static unsigned int sForegroundColour = FOREGROUND_COLOUR;
static unsigned int sBackgroundColour = BACKGROUND_COLOUR;

//! Constant Definitions
static const int BITMAP_FONT_CHARACTER_HEIGHT =  16;
static const int BITMAP_FONT_CHARACTER_WIDTH  =  8;
static const int BITMAP_FONT_MAX_CHARS_COLUMN = 40;


extern unsigned char console_font_8x16[];

//! Enum Definitions
enum
{
	CHARACTER_BACKSPACE       = 0x08,
	CHARACTER_HORIZONTAL_TAB  = 0x09,
	CHARACTER_LINE_FEED       = 0x0A,
	CHARACTER_CARRIAGE_RETURN = 0x0D,
	CHARACTER_SPACE           = 0x20,
	CHARACTER_TILDE           = 0x7E
};

//! Function Prototype Declarations

//! Private Functions
static int DrawStringToXFB(int iCurrentRow, int iCurrentColumn, const char* pString);
static unsigned int ConvertRGBToYCbCr(unsigned char r, unsigned char g, unsigned char b);
static inline unsigned int* GetCurrentXFBPointer();
static inline unsigned short GetXFBHeight();
static inline unsigned short GetXFBWidth();
static inline int IsProgressiveScanMode();
static inline int IsCharacterValid(char c);

//! Function Definitions

//! Public Functions
void DebugPrint_Init(void* xfb, unsigned short xfbWidth, unsigned short xfbHeight)
{
	sXFB = xfb;
	sXFBWidth = xfbWidth;
	sXFBHeight = xfbHeight;
}

/*---------------------------------------------------------------------------*
 * Name        : DebugPrint_Printf
 * Description : Renders a character string on the External Frame Buffer.
 * Arguments   : iCurrentRow       The row to start rendering the character string on.
 *               iCurrentColumn    The column to start rendering the character string on.
 *               pFormatString     A pointer to a formatted character string.
 *               ...               Additional arguments to be formatted.
 * Returns     : The number of characters rendered on the External Frame Buffer.
 *---------------------------------------------------------------------------*/
int
DebugPrint_Printf(int iCurrentRow, int iCurrentColumn, const char* pFormatString, ...)
{
	const int STRING_BUFFER_LENGTH = 256;
	char chStringBuffer[STRING_BUFFER_LENGTH];

	va_list args;
	va_start(args, pFormatString);
	vsnprintf(chStringBuffer, STRING_BUFFER_LENGTH, pFormatString, args);
	va_end(args);

	return DrawStringToXFB(iCurrentRow, iCurrentColumn, chStringBuffer);
}

/*---------------------------------------------------------------------------*
 * Name        : DebugPrint_SetForegroundColour
 * Description : Sets the foreground colour of the characters.
 * Arguments   : GXColor    A GXColor structure containing the foreground colour
 *               of the characters in RGB format.
 *---------------------------------------------------------------------------*/
void
DebugPrint_SetForegroundColour(GXColor gxColor)
{
	sForegroundColour = ConvertRGBToYCbCr(gxColor.r, gxColor.g, gxColor.b);
}

/*---------------------------------------------------------------------------*
 * Name        : DebugPrint_SetBackgroundColour
 * Description : Sets the background colour of the characters.
 * Arguments   : GXColor    A GXColor structure containing the background colour
 *               of the characters in RGB format.
 *---------------------------------------------------------------------------*/
void
DebugPrint_SetBackgroundColour(GXColor gxColor)
{
	sBackgroundColour = ConvertRGBToYCbCr(gxColor.r, gxColor.g, gxColor.b);
}

//! Private Functions
/*---------------------------------------------------------------------------*
 * Name        : DrawStringToXFB
 * Description : Renders a character string on the External Frame Buffer.
 * Arguments   : iCurrentRow       The row to start rendering the character string on.
 *               iCurrentColumn    The column to start rendering the character string on.
 *               pString           A pointer to a character string.
 * Returns     : The number of characters rendered on the External Frame Buffer.
 *---------------------------------------------------------------------------*/
static int
DrawStringToXFB(int iCurrentRow, int iCurrentColumn, const char* pString)
{
	// Keep track of the number of characters rendered on the External Frame Buffer
	int i = 0;

	// Do not attempt to render characters outside of the External Frame Buffer
	if (iCurrentRow < 0 || iCurrentColumn < 0)
		return i;

	// Loop over all of the characters in the character string
	for (char c; pString[i]; i++)
	{
		c = pString[i];

		// If we go past the final column, go to the first column and move to the next row
		if (iCurrentColumn >= BITMAP_FONT_MAX_CHARS_COLUMN)
		{
			iCurrentColumn = 0;
			iCurrentRow++;
		}

		// If we go past the final row, go to the first column and row
		if (iCurrentRow >= (GetXFBHeight() / BITMAP_FONT_CHARACTER_HEIGHT / (IsProgressiveScanMode() ? 2 : 1)))
		{
			iCurrentColumn = 0;
			iCurrentRow    = 0;
		}

		// Deduce where to render the character at on the External Frame Buffer
		unsigned int* pXFB = GetCurrentXFBPointer()                                                                          + // O
			             (iCurrentColumn * (BITMAP_FONT_CHARACTER_WIDTH / 2))                                                + // X
			             (GetXFBWidth() * iCurrentRow * BITMAP_FONT_CHARACTER_HEIGHT / 2); // Y

		// Handle control characters
		if (c == CHARACTER_BACKSPACE)
		{
			if (iCurrentColumn != 0)
				iCurrentColumn--;
			continue;
		}
		if (c == CHARACTER_HORIZONTAL_TAB)
		{
			do
			{
				DrawStringToXFB(iCurrentRow, iCurrentColumn, " ");
				iCurrentColumn++;
			} while (iCurrentColumn & 3);
			continue;
		}
		if (c == CHARACTER_LINE_FEED)
		{
			iCurrentRow++;
			continue;
		}
		if (c == CHARACTER_CARRIAGE_RETURN)
		{
			iCurrentColumn = 0;
			continue;
		}

		// Treat characters that are not included in the bitmap font as a "space" character
		if (!IsCharacterValid(c))
			c = CHARACTER_SPACE;

		// Loop over each row of the bitmap font character
		for (int iBitmapFontCharacterRow = 0; iBitmapFontCharacterRow < BITMAP_FONT_CHARACTER_HEIGHT; iBitmapFontCharacterRow++)
		{
			// Render each row of the bitmap font character
			unsigned char uchCharacterRow = console_font_8x16[c * BITMAP_FONT_CHARACTER_HEIGHT + iBitmapFontCharacterRow];
			int alternate = 0;
			for (unsigned char uchBits = 0x80; uchBits != 0; uchBits >>= 1)
			{
				if (alternate == 0) {
					if (uchCharacterRow & uchBits)
					{
						*pXFB = sForegroundColour & 0xFFFF00FF;
					}
					else
					{
						*pXFB = sBackgroundColour & 0xFFFF00FF;
					}
				} else {
					if (uchCharacterRow & uchBits)
					{
						*pXFB = *pXFB | (sForegroundColour & 0x0000FF00);
					}
					else
					{
						*pXFB = *pXFB | (sBackgroundColour & 0x0000FF00);
					}
				}

				// Move to the next column
				alternate ^= 1;
				if (alternate == 0)
					pXFB++;
			}
			// Move to the next row
			pXFB += (BITMAP_FONT_MAX_CHARS_COLUMN * BITMAP_FONT_CHARACTER_WIDTH) - (BITMAP_FONT_CHARACTER_WIDTH / 2);
		}
		// Move to the next character
		iCurrentColumn++;
	}

	return i;
}

/*---------------------------------------------------------------------------*
 * Name        : ConvertRGBToYCbCr
 * Description : Converts a colour from the RGB format to the YCbCr format.
 * Arguments   : r    The red   component of the colour.
 *               g    The green component of the colour.
 *               b    The blue  component of the colour.
 * Returns     : The colour in YCbCr format.
 *---------------------------------------------------------------------------*/
// Credits: NVIDIA Performance Primitives
static unsigned int
ConvertRGBToYCbCr(unsigned char r, unsigned char g, unsigned char b)
{
	unsigned char y = (unsigned char)(( 0.257f * r) + (0.504f * g) + (0.098f * b) +  16.0f);
	unsigned char u = (unsigned char)((-0.148f * r) - (0.291f * g) + (0.439f * b) + 128.0f);
	unsigned char v = (unsigned char)(( 0.439f * r) - (0.368f * g) - (0.071f * b) + 128.0f);

	return (y << 24 | u << 16 | y << 8 | v << 0);
}

//! Inline Functions
/*---------------------------------------------------------------------------*
 * Name        : GetCurrentXFBPointer
 * Description : Gets a pointer to the current External Frame Buffer.
 * Returns     : A pointer to the current External Frame Buffer.
 *---------------------------------------------------------------------------*/
// Credits: Yet Another Gamecube Documentation
static inline unsigned int*
GetCurrentXFBPointer()
{
	return (unsigned int*)sXFB;
}

/*---------------------------------------------------------------------------*
 * Name        : GetXFBHeight
 * Description : Gets the height of the External Frame Buffer.
 * Returns     : The height of the External Frame Buffer.
 *---------------------------------------------------------------------------*/
// Credits: Gecko dotNet
static inline unsigned short
GetXFBHeight()
{
	return sXFBHeight;
}

/*---------------------------------------------------------------------------*
 * Name        : GetXFBWidth
 * Description : Gets the width of the External Frame Buffer.
 * Returns     : The width of the External Frame Buffer.
 *---------------------------------------------------------------------------*/
// Credits: Gecko dotNet
static inline unsigned short
GetXFBWidth()
{
	return sXFBWidth;
}

/*---------------------------------------------------------------------------*
 * Name        : IsCharacterValid
 * Description : Checks if the bitmap font contains a character.
 * Arguments   : c    A character.
 * Returns     : 0    The bitmap font does not contain the character.
 *               1    The bitmap font does contain the character.
 *---------------------------------------------------------------------------*/
static inline int
IsCharacterValid(char c)
{
	return (c >= CHARACTER_SPACE && c <= CHARACTER_TILDE);
}

/*---------------------------------------------------------------------------*
 * Name        : IsProgressiveScanMode
 * Description : Checks if the game is being displayed in progressive scan mode.
 * Returns     : 0    The game is not being displayed in progressive scan mode.
 *               1    The game is being displayed in progressive scan mode.
 *---------------------------------------------------------------------------*/
// Credits: Savezelda
static inline int
IsProgressiveScanMode()
{
	return (*(volatile unsigned char*)(VI_REG_BASE + VI_VTR_REG_OFFSET + 1) & 0x0F) > 10;
}
