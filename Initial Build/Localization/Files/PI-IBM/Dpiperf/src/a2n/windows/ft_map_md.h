/*
 * ===========================================================================
 * @(#)src/frame/pfm/ft_map_md.h, ras, hndev, 20030319 1.1
 * ===========================================================================
 */

/*
 *
 * Change activity:
 *
 * Reason Date     Origin Description
 * ------ ----     ------ ----------------------------------------------------
 * 53555  240702   hdrs   Map file structure for windows
 *
 * ===========================================================================
 * Module Information:
 *
 * DESCRIPTION: IBM.WRITEME
 * ===========================================================================
 */
#ifndef _IBM_FT_MAP_MD_H_
#define _IBM_FT_MAP_MD_H_


// The binary map file jvmmap contains multiple maps, each of them starting
// with a header of type ftMapFileHeader. Offsets specified in this header
// are relative to the top of the header, and are not absolute file offsets.
// Symbol table contains an array of structures of type ftMapFileSymbol
typedef struct {
        char           magic[8];         // Set to JVMMAPS
        unsigned int   version;          // Version of binary map file
        unsigned int   textSize;         // Size of text section
        unsigned int   timestamp;        // Timestamp of original map file
        unsigned int   totalSectSize;    // Total size of all sections in map
        unsigned int   loadAddress;      // preferred load address
        unsigned int   numSyms;          // number of symbols
        unsigned int   symOffset;        // Offset of symbol table
        unsigned int   symSize;          // Total size of symbol table
        unsigned int   strOffset;        // Offset of string table
        unsigned int   stringsLen;       // Size of string table
        unsigned int   nameOffset;       // Offset of filename
        unsigned int   nameLen;          // Length of filename
        unsigned int   next;             // Offset of next map file entry
} ftMapFileHeader;


// Symbol table entry used in the binary map file
typedef struct {
        unsigned long   nameOffset;      // Offset of name in strings table
        unsigned long   address;         // Address relative to start of module
} ftMapFileSymbol;;


#define JvmMapMajorVersion 1
#define JvmMapMinorVersion 0
#define JvmMapFileVersion ((JvmMapMajorVersion << 16)| JvmMapMinorVersion)

#endif  /*!_IBM_FT_MAP_MD_H_*/
