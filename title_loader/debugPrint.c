/*!
 * Author  : Star
 * Date    : 24 May 2021
 * File    : debugPrint.c
 * Version : 1.1.0.0
 */

#include "debug/debugPrint.h"

#include <rvl/os.h>
#include <rvl/vi.h>

#include <stdarg.h>
#include <stddef.h>

#include "types.h"
#include "vsprintf.h"

//! Definitions
#define FOREGROUND_COLOUR 0xEB7FEB7F // White
#define BACKGROUND_COLOUR 0x10801080 // Black

//! Static Definitions
static unsigned int sForegroundColour = FOREGROUND_COLOUR;
static unsigned int sBackgroundColour = BACKGROUND_COLOUR;

//! Constant Definitions
static const int BITMAP_FONT_CHARACTER_HEIGHT = 8;
static const int BITMAP_FONT_CHARACTER_WIDTH  = 8;

static const int BITMAP_FONT_MAX_CHARS_COLUMN = 40;

static const unsigned char s8x8BitmapFont[] =
{
	// Credits: IBM
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /*   */
	0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00, /* ! */
	0x6C, 0x24, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, /* " */
	0x6C, 0x6C, 0xFE, 0x6C, 0xFE, 0x6C, 0x6C, 0x00, /* # */
	0x10, 0x7C, 0xD0, 0x7C, 0x16, 0xFC, 0x10, 0x00, /* $ */
	0x00, 0x66, 0xAC, 0xD8, 0x36, 0x6A, 0xCC, 0x00, /* % */
	0x38, 0x4C, 0x38, 0x78, 0xCE, 0xCC, 0x7A, 0x00, /* & */
	0x30, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, /* ' */
	0x18, 0x30, 0x60, 0x60, 0x60, 0x30, 0x18, 0x00, /* ( */
	0x60, 0x30, 0x18, 0x18, 0x18, 0x30, 0x60, 0x00, /* ) */
	0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00, /* * */
	0x00, 0x30, 0x30, 0xFC, 0x30, 0x30, 0x00, 0x00, /* + */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x10, 0x20, /* , */
	0x00, 0x00, 0x00, 0xFC, 0x00, 0x00, 0x00, 0x00, /* - */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, /* . */
	0x02, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x00, /* / */
	0x7C, 0xCE, 0xDE, 0xF6, 0xE6, 0xE6, 0x7C, 0x00, /* 0 */
	0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x7E, 0x00, /* 1 */
	0x7C, 0xC6, 0x06, 0x1C, 0x70, 0xC6, 0xFE, 0x00, /* 2 */
	0x7C, 0xC6, 0x06, 0x3C, 0x06, 0xC6, 0x7C, 0x00, /* 3 */
	0x1C, 0x3C, 0x6C, 0xCC, 0xFE, 0x0C, 0x1E, 0x00, /* 4 */
	0xFE, 0xC0, 0xFC, 0x06, 0x06, 0xC6, 0x7C, 0x00, /* 5 */
	0x7C, 0xC6, 0xC0, 0xFC, 0xC6, 0xC6, 0x7C, 0x00, /* 6 */
	0xFE, 0xC6, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00, /* 7 */
	0x7C, 0xC6, 0xC6, 0x7C, 0xC6, 0xC6, 0x7C, 0x00, /* 8 */
	0x7C, 0xC6, 0xC6, 0x7E, 0x06, 0xC6, 0x7C, 0x00, /* 9 */
	0x00, 0x30, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, /* : */
	0x00, 0x30, 0x00, 0x00, 0x00, 0x30, 0x10, 0x20, /* ; */
	0x0C, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0C, 0x00, /* < */
	0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00, /* = */
	0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00, /* > */
	0x78, 0xCC, 0x0C, 0x18, 0x30, 0x00, 0x30, 0x00, /* ? */
	0x7C, 0x82, 0x9E, 0xA6, 0x9E, 0x80, 0x7C, 0x00, /* @ */
	0x7C, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0x00, /* A */
	0xFC, 0x66, 0x66, 0x7C, 0x66, 0x66, 0xFC, 0x00, /* B */
	0x7C, 0xC6, 0xC0, 0xC0, 0xC0, 0xC6, 0x7C, 0x00, /* C */
	0xFC, 0x66, 0x66, 0x66, 0x66, 0x66, 0xFC, 0x00, /* D */
	0xFE, 0x62, 0x68, 0x78, 0x68, 0x62, 0xFE, 0x00, /* E */
	0xFE, 0x62, 0x68, 0x78, 0x68, 0x60, 0xF0, 0x00, /* F */
	0x7C, 0xC6, 0xC6, 0xC0, 0xCE, 0xC6, 0x7E, 0x00, /* G */
	0xC6, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0x00, /* H */
	0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, /* I */
	0x1E, 0x0C, 0x0C, 0x0C, 0xCC, 0xCC, 0x78, 0x00, /* J */
	0xE6, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0xE6, 0x00, /* K */
	0xF0, 0x60, 0x60, 0x60, 0x62, 0x66, 0xFE, 0x00, /* L */
	0x82, 0xC6, 0xEE, 0xFE, 0xD6, 0xC6, 0xC6, 0x00, /* M */
	0xC6, 0xE6, 0xF6, 0xDE, 0xCE, 0xC6, 0xC6, 0x00, /* N */
	0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, /* O */
	0xFC, 0x66, 0x66, 0x7C, 0x60, 0x60, 0xF0, 0x00, /* P */
	0x7C, 0xC6, 0xC6, 0xC6, 0xD6, 0xDE, 0x7C, 0x06, /* Q */
	0xFC, 0x66, 0x66, 0x7C, 0x66, 0x66, 0xE6, 0x00, /* R */
	0x7C, 0xC6, 0xC0, 0x7C, 0x06, 0xC6, 0x7C, 0x00, /* S */
	0x7E, 0x5A, 0x5A, 0x18, 0x18, 0x18, 0x3C, 0x00, /* T */
	0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, /* U */
	0xC6, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x10, 0x00, /* V */
	0xC6, 0xC6, 0xD6, 0xFE, 0xEE, 0xC6, 0x82, 0x00, /* W */
	0xC6, 0x6C, 0x38, 0x38, 0x38, 0x6C, 0xC6, 0x00, /* X */
	0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x3C, 0x00, /* Y */
	0xFE, 0xC6, 0x8C, 0x18, 0x32, 0x66, 0xFE, 0x00, /* Z */
	0x78, 0x60, 0x60, 0x60, 0x60, 0x60, 0x78, 0x00, /* [ */
	0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x02, 0x00, /* \ */
	0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x78, 0x00, /* ] */
	0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00, /* ^ */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, /* _ */
	0x30, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, /* ` */
	0x00, 0x00, 0x78, 0x0C, 0x7C, 0xCC, 0x76, 0x00, /* a */
	0xE0, 0x60, 0x60, 0x7C, 0x66, 0x66, 0x7C, 0x00, /* b */
	0x00, 0x00, 0x7C, 0xC6, 0xC0, 0xC6, 0x7C, 0x00, /* c */
	0x1C, 0x0C, 0x0C, 0x7C, 0xCC, 0xCC, 0x76, 0x00, /* d */
	0x00, 0x00, 0x7C, 0xC6, 0xFE, 0xC0, 0x7C, 0x00, /* e */
	0x1C, 0x36, 0x30, 0x78, 0x30, 0x30, 0x78, 0x00, /* f */
	0x00, 0x00, 0x76, 0xCC, 0xCC, 0x7C, 0x0C, 0x78, /* g */
	0xE0, 0x60, 0x6C, 0x76, 0x66, 0x66, 0xE6, 0x00, /* h */
	0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00, /* i */
	0x00, 0x0C, 0x00, 0x1C, 0x0C, 0x0C, 0xCC, 0x78, /* j */
	0xE0, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0xE6, 0x00, /* k */
	0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, /* l */
	0x00, 0x00, 0xCC, 0xFE, 0xD6, 0xD6, 0xD6, 0x00, /* m */
	0x00, 0x00, 0xDC, 0x66, 0x66, 0x66, 0x66, 0x00, /* n */
	0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, /* o */
	0x00, 0x00, 0xDC, 0x66, 0x66, 0x7C, 0x60, 0xF0, /* p */
	0x00, 0x00, 0x7C, 0xCC, 0xCC, 0x7C, 0x0C, 0x1E, /* q */
	0x00, 0x00, 0xDE, 0x76, 0x60, 0x60, 0xF0, 0x00, /* r */
	0x00, 0x00, 0x7C, 0xC0, 0x7C, 0x06, 0x7C, 0x00, /* s */
	0x10, 0x30, 0xFC, 0x30, 0x30, 0x34, 0x18, 0x00, /* t */
	0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0x76, 0x00, /* u */
	0x00, 0x00, 0xC6, 0xC6, 0x6C, 0x38, 0x10, 0x00, /* v */
	0x00, 0x00, 0xC6, 0xD6, 0xD6, 0xFE, 0x6C, 0x00, /* w */
	0x00, 0x00, 0xC6, 0x6C, 0x38, 0x6C, 0xC6, 0x00, /* x */
	0x00, 0x00, 0xCC, 0xCC, 0xCC, 0x7C, 0x0C, 0xF8, /* y */
	0x00, 0x00, 0xFC, 0x98, 0x30, 0x64, 0xFC, 0x00, /* z */
	0x0E, 0x18, 0x18, 0x30, 0x18, 0x18, 0x0E, 0x00, /* { */
	0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00, /* | */
	0xE0, 0x30, 0x30, 0x18, 0x30, 0x30, 0xE0, 0x00, /* } */
	0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* ~ */
};

//! Enum Definitions
enum
{
	CHARACTER_SPACE = 0x20,
	CHARACTER_TILDE = 0x7E
};

//! Function Prototype Declarations

//! Private Functions
static int DrawStringToXFB(const char* pString, int iCurrentRow, int iCurrentColumn);
__attribute__((noinline)) static unsigned int ConvertRGBToYCbCr(unsigned char R, unsigned char G, unsigned char B);
static inline unsigned short GetXFBWidth();
static inline unsigned short GetXFBHeight();
static inline bool IsProgressiveScanMode();
static inline unsigned int* GetCurrentXFBPointer();
static inline bool IsCharacterValid(char c);

//! Function Definitions

//! Public Functions
/*---------------------------------------------------------------------------*
 * Name        : DebugPrint_Printf
 * Description : Renders a string on the External Frame Buffer.
 * Arguments   : iCurrentRow       The row to start rendering characters on.
 *               iCurrentColumn    The column to start rendering the characters on.
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

	return DrawStringToXFB(chStringBuffer, iCurrentRow, iCurrentColumn);
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
 * Description : Renders a string on the External Frame Buffer.
 * Arguments   : pString           A pointer to a character string.
 *               iCurrentRow       The row to start rendering characters on.
 *               iCurrentColumn    The column to start rendering the characters on.
 * Returns     : The number of characters rendered on the External Frame Buffer.
 *---------------------------------------------------------------------------*/
static int
DrawStringToXFB(const char* pString, int iCurrentRow, int iCurrentColumn)
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

		// Calculate where to start rendering the character at
		unsigned int* pXFB = GetCurrentXFBPointer()                                                                          + // O
			             (iCurrentColumn * BITMAP_FONT_CHARACTER_WIDTH)                                                  + // X
			             (GetXFBWidth() * iCurrentRow * BITMAP_FONT_CHARACTER_HEIGHT / (IsProgressiveScanMode() ? 1 : 2)); // Y

		// Handle control characters
		if (c == '\b') // Backspace
		{
			if (iCurrentColumn != 0)
				iCurrentColumn--;
			continue;
		}
		if (c == '\t') // Horizontal Tab
		{
			do
			{
				DrawStringToXFB(" ", iCurrentRow, iCurrentColumn);
				iCurrentColumn++;
			} while (iCurrentColumn & 3);
			continue;
		}
		if (c == '\n') // Line Feed
		{
			iCurrentRow++;
			continue;
		}
		if (c == '\r') // Carriage Return
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
			// Render each row of the character
			unsigned char uchCharacterRow = *(s8x8BitmapFont + (unsigned int)((c - CHARACTER_SPACE) * BITMAP_FONT_CHARACTER_HEIGHT) + iBitmapFontCharacterRow);
			for (unsigned char uchBits = 0x80; uchBits != 0; uchBits >>= 1)
			{
				if (uchCharacterRow & uchBits)
				{
					*pXFB = sForegroundColour;
				}
				else
				{
					*pXFB = sBackgroundColour;
				}
				// Move to the next column
				pXFB++;
			}
			// Move to the next row
			pXFB += (BITMAP_FONT_MAX_CHARS_COLUMN * BITMAP_FONT_CHARACTER_HEIGHT) - BITMAP_FONT_CHARACTER_WIDTH;
		}
		// Move to the next character
		iCurrentColumn++;
	}

	return i;
}

/*---------------------------------------------------------------------------*
 * Name        : ConvertRGBToYCbCr
 * Description : Converts a colour from the RGB format to the YCbCr format.
 * Arguments   : R    The red component of the colour.
 *               G    The green component of the colour.
 *               B    The blue component of the colour.
 * Returns     : The colour in YCbCr format.
 *---------------------------------------------------------------------------*/
// Credits: NVIDIA Performance Primitives
__attribute__((noinline))
static unsigned int
ConvertRGBToYCbCr(unsigned char R, unsigned char G, unsigned char B)
{
	unsigned char Y = (unsigned char)(( 0.257f * R) + (0.504f * G) + (0.098f * B) +  16.0f);
	unsigned char U = (unsigned char)((-0.148f * R) - (0.291f * G) + (0.439f * B) + 128.0f);
	unsigned char V = (unsigned char)(( 0.439f * R) - (0.368f * G) - (0.071f * B) + 128.0f);

	return (Y << 24 | U << 16 | Y << 8 | V << 0);
}

//! Inline Functions
/*---------------------------------------------------------------------------*
 * Name        : GetXFBWidth
 * Description : Gets the width of the External Frame Buffer.
 * Returns     : The width of the External Frame Buffer.
 *---------------------------------------------------------------------------*/
// Credits: Gecko dotNet
static inline unsigned short
GetXFBWidth()
{
	return *(volatile unsigned char*)(VI_REG_BASE + VI_HSW_REG_OFFSET + 1) << 3;
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
	return ((*(volatile unsigned char*)(VI_REG_BASE + VI_VTR_REG_OFFSET + 0) << 5)  |
		(*(volatile unsigned char*)(VI_REG_BASE + VI_VTR_REG_OFFSET + 1) >> 3)) & 0x07FE;
}

/*---------------------------------------------------------------------------*
 * Name        : IsProgressiveScanMode
 * Description : Checks if the game is being displayed in progressive scan mode.
 * Returns     : 0    The game is not being displayed in progressive scan mode.
 *               1    The game is being displayed in progressive scan mode.
 *---------------------------------------------------------------------------*/
// Credits: Savezelda
static inline bool
IsProgressiveScanMode()
{
	return (*(volatile unsigned char*)(VI_REG_BASE + VI_VTR_REG_OFFSET + 1) & 0x0F) > 10;
}

/*---------------------------------------------------------------------------*
 * Name        : GetCurrentXFBPointer
 * Description : Gets a pointer to the current External Frame Buffer.
 * Returns     : A pointer to the current External Frame Buffer.
 *---------------------------------------------------------------------------*/
// Credits: Yet Another Gamecube Documentation
static inline unsigned int*
GetCurrentXFBPointer()
{
	unsigned int uiTFBLReg = *(volatile unsigned int*)(VI_REG_BASE + VI_TFBL_REG_OFFSET);
	if (uiTFBLReg & VI_TFBL_REG_PAGE_OFFSET_BIT)
		uiTFBLReg <<= 5;
	return (unsigned int*)(uiTFBLReg | OS_UNCACHED_BASE);
}

/*---------------------------------------------------------------------------*
 * Name        : IsCharacterValid
 * Description : Checks if the bitmap font contains a character.
 * Arguments   : c    A character.
 * Returns     : 0    The bitmap font does not contain the character.
 *               1    The bitmap font does contain the character.
 *---------------------------------------------------------------------------*/
static inline bool
IsCharacterValid(char c)
{
	return (c >= CHARACTER_SPACE && c <= CHARACTER_TILDE);
}
