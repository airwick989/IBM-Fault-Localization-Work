//**************************************************************************//
//                                                                          //
//                     Copyright (c) IBM Corporation 2001                   //
//                        Property of IBM Corporation                       //
//                            All Rights Reserved                           //
//                                                                          //
//                                                                          //
//           Windows Symbol Harvester Codeview Symbol Structures            //
//           ---------------------------------------------------            //
//                                                                          //
// EMP: Combination of cvexefmt.h and cvinfo.h                              //
//      both of which can be found in sdk\Samples\SdkTools\Image\Include    //
//      I renamed the original structures with names that made more sense   //
//      to me.  The original names are typedef'd                            //
//**************************************************************************//
//
#ifndef _CODEVIEW_H
#define _CODEVIEW_H

//
//  The following structures and constants describe the format of the
//  CodeView Debug OMF for that will be accepted by CodeView 4.0 and
//  later.  These are executables with signatures of NB05, NB06 and NB08.
//  There is some confusion about the signatures NB03 and NB04 so none
//  of the utilites will accept executables with these signatures.  NB07 is
//  the signature for QCWIN 1.0 packed executables.
//
//  All of the structures described below must start on a long word boundary
//  to maintain natural alignment.  Pad space can be inserted during the
//  write operation and the addresses adjusted without affecting the contents
//  of the structures.
//


//
//  Type of subsection entry.
//
#define sstModule           0x120
#define sstTypes            0x121
#define sstPublic           0x122
#define sstPublicSym        0x123   // publics as symbol (waiting for link)
#define sstSymbols          0x124
#define sstAlignSym         0x125
#define sstSrcLnSeg         0x126   // because link doesn't emit SrcModule
#define sstSrcModule        0x127
#define sstLibraries        0x128
#define sstGlobalSym        0x129
#define sstGlobalPub        0x12a
#define sstGlobalTypes      0x12b
#define sstMPC              0x12c
#define sstSegMap           0x12d
#define sstSegName          0x12e
#define sstPreComp          0x12f   // precompiled types
#define sstPreCompMap       0x130   // map precompiled types in global types
#define sstOffsetMap16      0x131
#define sstOffsetMap32      0x132
#define sstFileIndex        0x133   // Index of file names
#define sstStaticSym        0x134


typedef enum OMFHash {
    OMFHASH_NONE,           // no hashing
    OMFHASH_SUMUC16,        // upper case sum of chars in 16 bit table
    OMFHASH_SUMUC32,        // upper case sum of chars in 32 bit table
    OMFHASH_ADDR16,         // sorted by increasing address in 16 bit table
    OMFHASH_ADDR32          // sorted by increasing address in 32 bit table
} OMFHASH;


//
//  CodeView Debug OMF signature.  The signature at the end of the file is
//  a negative offset from the end of the file to another signature.  At
//  the negative offset (base address) is another signature whose filepos
//  field points to the first OMFDirHeader in a chain of directories.
//  The NB05 signature is used by the link utility to indicated a completely
//  unpacked file.  The NB06 signature is used by ilink to indicate that the
//  executable has had CodeView information from an incremental link appended
//  to the executable.  The NB08 signature is used by cvpack to indicate that
//  the CodeView Debug OMF has been packed.  CodeView will only process
//  executables with the NB08 signature.
//
//  *** OMFSignature ***
//
struct _image_cv_header {
    char        Signature[4];   // "NBxx"
    long        filepos;        // offset in file. Zero for NB10
};

typedef struct _image_cv_header        IMAGE_CV_HEADER;
typedef struct _image_cv_header        OMFSignature;


//
// This is what the NB10 Directory Header looks like.
// NB10 is the PDB signature.
//
struct _image_cv_nb10_header {
   char  Signature[4];         // "NB10"
   DWORD filepos;              // Always 0
   DWORD TimeDateStamp;        // Image timestamp
   DWORD PDBFileRevision;      // Used for incremental linking
   BYTE  Data[1];              // Fully qualified PDB file name
};

typedef struct _image_cv_nb10_header   IMAGE_CV_NB10_HEADER;


//
// This is what the RSDS Directory Header looks like.
// RSDS is another PDB signature, used on Win64 (maybe VC++ 7)
//
struct _image_cv_rsds_header {
   char  Signature[4];         // "RSDS"
   char  id[16];               // A 16-byte id of some sort
   DWORD PDBFileRevision;      // Used for incremental linking (??)
   BYTE  Data[1];              // PDB file name
};

typedef struct _image_cv_rsds_header   IMAGE_CV_RSDS_HEADER;


//
//  directory information structure
//  This structure contains the information describing the directory.
//  It is pointed to by the signature at the base address or the directory
//  link field of a preceeding directory.  The directory entries immediately
//  follow this structure.
//
//  *** OMFDirHeader ***
//
struct _image_cv_dir_header {
    unsigned short  cbDirHeader;    // length of this structure
    unsigned short  cbDirEntry;     // number of bytes in each directory entry
    unsigned long   cDir;           // number of directorie entries
    long            lfoNextDir;     // offset from base of next directory
    unsigned long   flags;          // status flags
};

typedef struct _image_cv_dir_header    IMAGE_CV_DIR_HEADER;
typedef struct _image_cv_dir_header    OMFDirHeader;


//
//  directory structure
//  The data in this structure is used to reference the data for each
//  subsection of the CodeView Debug OMF information.  Tables that are
//  not associated with a specific module will have a module index of
//  oxffff.  These tables are the global types table, the global symbol
//  table, the global public table and the library table.
//
//  *** OMFDirEntry ***
//
struct _image_cv_dir_entry {
    unsigned short  SubSection;     // subsection type (sst...)
    unsigned short  iMod;           // module index
    long            lfo;            // large file offset of subsection
    unsigned long   cb;             // number of bytes in subsection
};

typedef struct _image_cv_dir_entry     IMAGE_CV_DIR_ENTRY;
typedef struct _image_cv_dir_entry     OMFDirEntry;


//
//  information decribing each segment in a module
//
//  *** OMFSegDesc ***
//
struct _image_cv_segment {
    unsigned short  Seg;            // segment index
    unsigned short  pad;            // pad to maintain alignment
    unsigned long   Off;            // offset of code in segment
    unsigned long   cbSeg;          // number of bytes in segment
};

typedef struct _image_cv_segment       IMAGE_CV_SEGMENT;
typedef struct _image_cv_segment       OMFSegDesc;


//
//  per module information
//  There is one of these subsection entries for each module
//  in the executable.  The entry is generated by link/ilink.
//  This table will probably require padding because of the
//  variable length module name.
//
//  *** OMFModule ***
//
#if defined(_WINDOWS64)
#pragma warning(disable : 4200)
#endif
struct _image_cv_module {
    unsigned short   ovlNumber;      // overlay number
    unsigned short   iLib;           // library that the module was linked from
    unsigned short   cSeg;           // count of number of segments in module
    char             Style[2];       // debugging style "CV"
    IMAGE_CV_SEGMENT SegInfo[1];     // describes segments in module
    char             Name[];         // length prefixed module name padded to
                                     // long word boundary
};
#if defined(_WINDOWS64)
#pragma warning(default : 4200)
#endif

typedef struct _image_cv_module        IMAGE_CV_MODULE;
typedef struct _image_cv_module        OMFModule;


//
//  Symbol hash table format
//  This structure immediately preceeds the global publics table
//  and global symbol tables.
//
//  *** OMFSymHash ***
//
struct _image_cv_symhash {
    unsigned short  symhash;        // symbol hash function index
    unsigned short  addrhash;       // address hash function index
    unsigned long   cbSymbol;       // length of symbol information
    unsigned long   cbHSym;         // length of symbol hash data
    unsigned long   cbHAddr;        // length of address hashdata
};

typedef struct _image_cv_symhash       IMAGE_CV_SYMHASH;
typedef struct _image_cv_symhash       OMFSymHash;


//
// Symbol definitions
//
// *** typedef enum SYM_ENUM_e ***
//
#define S_NULL          0x0000    // Filler record ***** EMP *****
#define S_COMPILE       0x0001    // Compile flags symbol
#define S_REGISTER_16t  0x0002    // Register variable
#define S_CONSTANT_16t  0x0003    // constant symbol
#define S_UDT_16t       0x0004    // User defined type
#define S_SSEARCH       0x0005    // Start Search
#define S_END           0x0006    // Block, procedure, "with" or thunk end
#define S_SKIP          0x0007    // Reserve symbol space in $$Symbols table
#define S_CVRESERVE     0x0008    // Reserved symbol for CV internal use
#define S_OBJNAME       0x0009    // path to object file name
#define S_ENDARG        0x000a    // end of argument/return list
#define S_COBOLUDT_16t  0x000b    // special UDT for cobol that does not symbol pack
#define S_MANYREG_16t   0x000c    // multiple register variable
#define S_RETURN        0x000d    // return description symbol
#define S_ENTRYTHIS     0x000e    // description of this pointer on entry

#define S_BPREL16       0x0100    // BP-relative
#define S_LDATA16       0x0101    // Module-local symbol
#define S_GDATA16       0x0102    // Global data symbol
#define S_PUB16         0x0103    // a public symbol
#define S_LPROC16       0x0104    // Local procedure start
#define S_GPROC16       0x0105    // Global procedure start
#define S_THUNK16       0x0106    // Thunk Start
#define S_BLOCK16       0x0107    // block start
#define S_WITH16        0x0108    // with start
#define S_LABEL16       0x0109    // code label
#define S_CEXMODEL16    0x010a    // change execution model
#define S_VFTABLE16     0x010b    // address of virtual function table
#define S_REGREL16      0x010c    // register relative address

#define S_BPREL32_16t   0x0200    // BP-relative
#define S_LDATA32_16t   0x0201    // Module-local symbol
#define S_GDATA32_16t   0x0202    // Global data symbol
#define S_PUB32_16t     0x0203    // a public symbol (CV internal reserved)
#define S_LPROC32_16t   0x0204    // Local procedure start
#define S_GPROC32_16t   0x0205    // Global procedure start
#define S_THUNK32       0x0206    // Thunk Start
#define S_BLOCK32       0x0207    // block start
#define S_WITH32        0x0208    // with start
#define S_LABEL32       0x0209    // code label
#define S_CEXMODEL32    0x020a    // change execution model
#define S_VFTABLE32_16t 0x020b    // address of virtual function table
#define S_REGREL32_16t  0x020c    // register relative address
#define S_LTHREAD32_16t 0x020d    // local thread storage
#define S_GTHREAD32_16t 0x020e    // global thread storage
#define S_SLINK32       0x020f    // static link for MIPS EH implementation

#define S_LPROCMIPS_16t 0x0300    // Local procedure start
#define S_GPROCMIPS_16t 0x0301    // Global procedure start

#define S_PROCREF       0x0400    // Reference to a procedure
#define S_DATAREF       0x0401    // Reference to data
#define S_ALIGN         0x0402    // Used for page alignment of symbols
#define S_LPROCREF      0x0403    // Local Reference to a procedure

#define S_REGISTER      0x1001    // Register variable
#define S_CONSTANT      0x1002    // Constant symbol
#define S_UDT           0x1003    // User defined type
#define S_COBOLUDT      0x1004    // Microfocus COBOL user-defined type
#define S_MANYREG       0x1005    // multiple register variable
#define S_BPREL32       0x1006    // BP-relative 16:32 (ie. allocated on the stack)
#define S_LDATA32       0x1007    // Local data 16:32 (statics in C++)
#define S_GDATA32       0x1008    // Global data 16:32
#define S_PUB32         0x1009    // Public symbol 16:32
#define S_LPROC32       0x100a    // Local (file static) procedure start 16:32
#define S_GPROC32       0x100b    // Global procedure start 16:32
#define S_VFTABLE32     0x100c    // Virtual function table path descriptor 16:32
#define S_REGREL32      0x100d    // 16:32 offset relative to arbitrary register
#define S_LTHREAD32     0x100e    // local thread storage data
#define S_GTHREAD32     0x100f    // global thread storage data
#define S_LPROCMIPS     0x1010    // Local procedure start MIPS
#define S_GPROCMIPS     0x1011    // Global procedure start MIPS
#define S_LINKER        0x1013    // ***** EMP ***** Seems to be the linker string


//
// Generic symbol record
//
typedef struct _CVSYM {           // ***** EMP *****
    unsigned short  reclen;       // Record length
    unsigned short  rectyp;       // Record type
    char            data[1];
} CVSYM;



//
// Non-modal symbols
// *****************
//

//
// Machine enumeration:
//
#define CF_MACHINE_I8080   0x00    // Intel 8080
#define CF_MACHINE_I8086   0x01    // Intel 8086
#define CF_MACHINE_I286    0x02    // Intel 80286
#define CF_MACHINE_I386    0x03    // Intel 80386
#define CF_MACHINE_I486    0x04    // Intel 80486
#define CF_MACHINE_P5      0x05    // Intel Pentium
#define CF_MACHINE_P6      0x06    // Intel Pentium Pro
#define CF_MACHINE_IA64    0xFF    // Intel IA64
//
// Language enumeration
//
#define CF_LANG_C          0       // C
#define CF_LANG_CPP        1       // C++
#define CF_LANG_FORTRAN    2       // Fortran
#define CF_LANG_MASN       3       // Masm
#define CF_LANG_PASCAL     4       // Pascal
#define CF_LANG_BASIC      5       // Basic
#define CF_LANG_COBOL      6       // Cobol
//
// Ambient code and data memory model enumeration
//
#define CF_AMBIENT_NEAR    0       // Near
#define CF_AMBIENT_FAR     1       // Far
#define CF_AMBIENT_HUGE    2       // Huge
//
// Floating precision enumeration
//
#define CF_FLOATPREC_ANSI  1       // ANSI C floating point precision rules
//
// Floating package enumeration
//
#define CF_FLOATPKG_HDW    0       // Hardware processor (80x87 for Intel 80x86 processors)
#define CF_FLOATPKG_EMUL   1       // Emulator
#define CF_FLOATPKG_ALT    2       // Altmath

typedef struct CFLAGSYM {
    unsigned short  reclen;        // Record length
    unsigned short  rectyp;        // S_COMPILE (0x0001)
    struct  {
        unsigned int    machine   :8; // target processor
        unsigned int    language  :8; // language index
        unsigned int    pcode     :1; // true if pcode present
        unsigned int    floatprec :2; // floating precision
        unsigned int    floatpkg  :2; // float package
        unsigned int    ambdata   :3; // ambiant data model
        unsigned int    ambcode   :3; // ambiant code model
        unsigned int    mode32    :1; // true if compiled 32 bit mode
        unsigned int    pad       :4; // reserved
    } machine_and_flags;
    unsigned char   ver[1];        // Length-prefixed compiler version string
} CFLAGSYM;

typedef struct REGISTERSYM {       // ***** EMP *****
    unsigned short  reclen;        // Record length
    unsigned short  rectyp;        // S_REGISTER (0x1001)
    unsigned long   typind;        // Type index
    unsigned short  reg;           // Register
    unsigned char   name[1];       // Register name followed by
                                   // Register tracking information
} REGISTERSYM;

typedef struct CONSTANTSYM {       // ***** EMP *****
    unsigned short  reclen;        // Record length
    unsigned short  rectyp;        // S_CONSTANT (0x1002)
    unsigned long   typind;        // Type index (containing enum if enumerate)
    unsigned short  value;         // numeric leaf containing value
    unsigned char   name[1];       // Length-prefixed name
} CONSTANTSYM;

typedef struct UDTSYM {            // ***** EMP *****
    unsigned short  reclen;        // Record length
    unsigned short  rectyp;        // S_UDT (0x1003)
    unsigned long   typind;        // Type index
    unsigned char   name[1];       // User defined type name
} UDTSYM;

typedef struct SEARCHSYM {
    unsigned short  reclen;        // Record length
    unsigned short  rectyp;        // S_SSEARCH (0x0005)
    unsigned long   startsym;      // offset of the procedure
    unsigned short  seg;           // segment of symbol
} SEARCHSYM;

typedef struct ENDSYM {            // ***** EMP *****
    unsigned short  reclen;        // Record length
    unsigned short  rectyp;        // S_END (0x0006)
} ENDSYM;

typedef struct SKIPSYM {           // ***** EMP *****
    unsigned short  reclen;        // Record length
    unsigned short  rectyp;        // S_SKIP (0x0007)
    unsigned char   skipdata[1];   // Skip data.  Use Record Length to skip record
} SKIPSYM;

typedef struct OBJNAMESYM {
    unsigned short  reclen;        // Record length
    unsigned short  rectyp;        // S_OBJNAME (0x0009)
    unsigned long   signature;     // signature
    unsigned char   name[1];       // Length-prefixed name
} OBJNAMESYM;

typedef struct ENDARGSYM {
    unsigned short  reclen;        // Record length
    unsigned short  rectyp;        // S_ENDARG (0x000A)
} ENDARGSYM;

typedef struct COBOLUDTSYM {       // ***** EMP *****
    unsigned short  reclen;        // Record length
    unsigned short  rectyp;        // S_COBOLUDT (0x1004)
    unsigned long   typind;        // Type index
    unsigned char   name[1];       // User defined type name
} COBOLUDTSYM;

typedef struct MANYREGSYM {        // ***** EMP *****
    unsigned short  reclen;        // Record length
    unsigned short  rectyp;        // S_MANYREG (0x1005)
    unsigned long   typind;        // Type index
    unsigned char   count;         // count of number of registers
    unsigned char   reg[1];        // count register enumerates followed by
                                   // length-prefixed name.  Registers are
                                   // most significant first.
} MANYREGSYM;

//
// Return style values:
//
#define RET_STY_VOID         0x00  // void return
#define RET_STY_REG          0x01  // return value is in registers specified in data
#define RET_STY_CALLOC_NEAR  0x02  // indirect caller allocated near
#define RET_STY_CALLOC_FAR   0x03  // indirect caller allocated far
#define RET_STY_RALLOC_NAER  0x04  // indirect returnee allocated near
#define RET_STY_RALLOC_FAR   0x05  // indirect returnee allocated far

typedef struct RETURNSYM {
    unsigned short reclen;         // Record length
    unsigned short rectyp;         // S_RETURN (0x000D)
    unsigned short flags;          // flags
    unsigned char  style;          // return style
    unsigned char  data[1];        // return data (only with RET_STYLE_REG)
} RETURNSYM;

typedef struct ENTRYTHISSYM {
    unsigned short  reclen;        // Record length
    unsigned short  rectyp;        // S_ENTRYTHIS (0x000E)
    unsigned char   thissym[1];    // symbol describing this pointer on entry
} ENTRYTHISSYM;

typedef struct LINKERSYM {         // ***** EMP *****
    unsigned short  reclen;        // Record length
    unsigned short  rectyp;        // S_LINKER (0x1013)
    unsigned long   ul1;           // ???
    unsigned long   ul2;           // ???
    unsigned long   ul3;           // ???
    unsigned long   ul4;           // ???
    unsigned short  us1;           // ???
    unsigned char   ver[1];        // Length-prefixed linker version string
} LINKERSYM;




//
// Symbols for 16:32 Segmented and 32-bit Flat Architectures
// *********************************************************
//

//
// Flag field bit definitions
//
#define  CV_PFLAG_FPO      0x01     // frame pointer omitted
#define  CV_PFLAG_INT      0x02     // interrupt return
#define  CV_PFLAG_FAR      0x04     // far return
#define  CV_PFLAG_NEVER    0x08     // function does not return

typedef struct BPRELSYM32 {         // ***** EMP *****
    unsigned short  reclen;         // Record length
    unsigned short  rectyp;         // S_BPREL32 (0x1006)
    unsigned long   off;            // BP-relative offset
    unsigned long   typind;         // Type index
    unsigned char   name[1];        // Length-prefixed name
} BPRELSYM32;

typedef struct PUBSYM32 {           // ***** EMP *****
    unsigned short  reclen;         // Record length
    unsigned short  rectyp;         // S_LDATA32 (0x1007), S_GDATA32 (0x1008) or S_PUB32 (0x1009)
    unsigned long   typind;         // Type index
    unsigned long   off;
    unsigned short  seg;
    unsigned char   name[1];        // Length-prefixed name
} PUBSYM32;
typedef PUBSYM32  DATASYM32;

typedef struct PROCSYM32 {       // ***** EMP *****
    unsigned short  reclen;         // Record length
    unsigned short  rectyp;         // S_LPROC32 (0x100A) or S_GPROC32 (0x100B)
    unsigned long   pParent;        // pointer to the parent
    unsigned long   pEnd;           // pointer to this blocks end
    unsigned long   pNext;          // pointer to next symbol
    unsigned long   len;            // Proc length
    unsigned long   DbgStart;       // Debug start offset
    unsigned long   DbgEnd;         // Debug end offset
    unsigned long   typind;         // Type index
    unsigned long   off;
    unsigned short  seg;
    unsigned char   flags;          // Proc flags
    unsigned char   name[1];        // Length-prefixed name
} PROCSYM32;

//
// THUNK32 Ordinal values:  ***** EMP *****
//
#define THUNK32_ORD_NOTYPE    0
#define THUNK32_ORD_ADJUSTOR  1
#define THUNK32_ORD_VCALL     2
#define THUNK32_ORD_PCODE     3

typedef struct THUNKSYM32 {
    unsigned short  reclen;         // Record length
    unsigned short  rectyp;         // S_THUNK32 (0x0206)
    unsigned long   pParent;        // pointer to the parent
    unsigned long   pEnd;           // pointer to this blocks end
    unsigned long   pNext;          // pointer to next symbol
    unsigned long   off;
    unsigned short  seg;
    unsigned short  len;            // length of thunk
    unsigned char   ord;            // ordinal specifying type of thunk
    unsigned char   name[1];        // Length-prefixed name
    unsigned char   variant[1];     // variant portion of thunk (not present for THUNK32_ORD_NOTYPE)
} THUNKSYM32;

typedef struct BLOCKSYM32 {
    unsigned short  reclen;         // Record length
    unsigned short  rectyp;         // S_BLOCK32 (0x0207)
    unsigned long   pParent;        // pointer to the parent
    unsigned long   pEnd;           // pointer to this blocks end
    unsigned long   len;            // Block length
    unsigned long   off;            // Offset in code segment
    unsigned short  seg;            // segment of label
    unsigned char   name[1];        // Length-prefixed name
} BLOCKSYM32;

typedef struct WITHSYM32 {
    unsigned short  reclen;         // Record length
    unsigned short  rectyp;         // S_WITH32 (0x0208)
    unsigned long   pParent;        // pointer to the parent
    unsigned long   pEnd;           // pointer to this blocks end
    unsigned long   len;            // Block length
    unsigned long   off;            // Offset in code segment
    unsigned short  seg;            // segment of label
    unsigned char   expr[1];        // Length-prefixed expression string
} WITHSYM32;

typedef struct LABELSYM32 {
    unsigned short  reclen;         // Record length
    unsigned short  rectyp;         // S_LABEL32 (0x0209)
    unsigned long   off;
    unsigned short  seg;
    unsigned char   flags;          // flags
    unsigned char   name[1];        // Length-prefixed name
} LABELSYM32;

//
// Execution model values:
//
#define CEX_EXMODEL_NONEXEC   0x00  // Not executable code (e.g., a table)
#define CEX_EXMODEL_CGEN      0x01  // Compiler generated jump table
#define CEX_EXMODEL_DATAPAD   0x02  // Padding for data
#define CEX_EXMODEL_NATIVE    0x20  // Native model (no processor specified)
#define CEX_EXMODEL_MFCOBOL   0x21  // Microfocus Cobol (unused in 16:32)
#define CEX_EXMODEL_CODEPAD   0x22  // Code padding for alignment
#define CEX_EXMODEL_CODE      0x23  // Code
#define CEX_EXMODEL_PCODE     0x40  // Pcode (Reserved

typedef struct CEXMSYM32 {
    unsigned short  reclen;         // Record length
    unsigned short  rectyp;         // S_CEXMODEL32 (0x020A)
    unsigned long   off;            // offset of symbol
    unsigned short  seg;            // segment of symbol
    unsigned short  model;          // execution model
    union var32 {
        struct  {
            unsigned long pcdtable;      // offset to pcode function table
            unsigned long pcdspi;        // offset to segment pcode information
        } pcode;
        struct {
            unsigned short  subtype;     // see CV_COBOL_e above
            unsigned short  flag;
        } cobol;
        struct {
            unsigned long  calltableOff; // offset to function table
            unsigned short calltableSeg; // segment of function table
        } pcode32Mac;
    };
} CEXMSYM32;

typedef struct VFTABLESYM32 {
    unsigned short  reclen;         // Record length
    unsigned short  rectyp;         // S_VFTABLE32 (0x100C)
    unsigned long   root;
    unsigned long   path;
    unsigned long   off;
    unsigned short  seg;
} VFTABLESYM32;

typedef struct REGRELSYM32 {
    unsigned short  reclen;         // Record length
    unsigned short  rectyp;         // S_REGREL32 (0x100D)
    unsigned long   off;            // offset of symbol
    unsigned long   typind;         // Type index
    unsigned short  reg;            // register index for symbol
    unsigned char   name[1];        // Length-prefixed name
} REGRELSYM32;

typedef struct THREADSYM32 {
    unsigned short  reclen;         // record length
    unsigned short  rectyp;         // S_LTHREAD32 (0x100E) or S_GTHREAD32 (0x100F)
    unsigned long   typind;         // Type index
    unsigned long   off;            // offset of symbol
    unsigned short  seg;            // segment of thread storage
    unsigned char   name[1];        // length prefixed name
} THREADSYM32;




//
// Symbols for CVPACK optimizations
// ********************************
//

typedef struct REFSYM {
    unsigned short  reclen;         // Record length
    unsigned short  rectyp;         // S_PROCREF (0x0400) or S_DATAREF (0x0401)
    unsigned long   checksum;       // Checksum of referenced symbol
    unsigned long   offsym;         // Offset of actual symbol in $$Symbols
    unsigned short  imod;           // Index of module containing the actual symbol
    unsigned short  fill;           // align this record
} REFSYM;

typedef struct SALIGN {             // ***** EMP *****
    unsigned short  reclen;         // Record length
    unsigned short  rectyp;         // S_ALIGN (0x0402)
    unsigned char   pad[1];         // if 0xFFFFFFFF it's the end of symbols
} SALIGN;


#endif // _CODEVIEW_H
