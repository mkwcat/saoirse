/*---------------------------------------------------------------------------*
 * Author  : Star
 * Date    : 26 Aug 2021
 * File    : nand.h
 * Version : 1.1.0.0
 *---------------------------------------------------------------------------*/

#pragma once

//! Definitions
#define NAND_DIRECTORY_SEPARATOR_CHAR   '/'

#define NAND_MAX_FILENAME_LENGTH         12 // Not including the NULL terminator
#define NAND_MAX_FILEPATH_LENGTH         64 // Including the NULL terminator
#define NAND_MAX_FILE_DESCRIPTOR_AMOUNT  15

#define NAND_SEEK_SET                     0
#define NAND_SEEK_CUR                     1
#define NAND_SEEK_END                     2