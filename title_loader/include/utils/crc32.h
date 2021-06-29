/*!
 * File    : crc32.h
 */

#pragma once

//! Definitions
#define POLYNOMIAL 0xEDB88320
#define XOR_CRYPT_POLYNOMIAL(polynomial) ((polynomial) ^ 0x53746172)

//! Function Definitions

//! Public Functions
/*---------------------------------------------------------------------------*
 * Name        : CRC32_Calculate
 * Description : Calculates the CRC32 checksum of a buffer.
 * Arguments   : uiCRC32           The initial value of the CRC32 checksum.
 *               buffer            A pointer to a buffer.
 *               uiBufferLength    The length of the buffer.
 * Returns     : The CRC32 checksum of the buffer.
 *---------------------------------------------------------------------------*/
__attribute__((always_inline))
inline unsigned int
CRC32_Calculate(unsigned int uiCRC32, const void* buffer, unsigned int uiBufferLength)
{
	unsigned int polynomial = XOR_CRYPT_POLYNOMIAL(POLYNOMIAL);

	uiCRC32 = ~uiCRC32;
	for (unsigned char* pBuffer = (unsigned char*)buffer; uiBufferLength != 0; uiBufferLength--)
	{
		uiCRC32 ^= *pBuffer++;
		for (int i = 0; i < 8; i++)
			uiCRC32 = (uiCRC32 >> 1) ^ (XOR_CRYPT_POLYNOMIAL(polynomial) & -(uiCRC32 & 1));
	}
	return ~uiCRC32;
}

#undef POLYNOMIAL
#undef XOR_CRYPT_POLYNOMIAL