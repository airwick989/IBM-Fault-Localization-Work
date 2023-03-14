/*
 * IBM Performance Inspector
 * Copyright (c) International Business Machines Corp., 1996 - 2009
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef _GV_H_
#define _GV_H_


#define G(f)      a2n_gv.f
#define S(s,f)    s.f


typedef struct _gv_t gv_t;
struct _gv_t {
   FILE          * msgfh;                         // Message file handle

   PID_NODE      * PidRoot;                       // Root of pid node list (first pid seen)
   PID_NODE      * PidEnd;                        // End of pid node list (last pid seen)
   PID_NODE      * PidTable[PID_HASH_TABLE_SIZE]; // Pid node hash table
   PID_NODE      * SystemPidNode;                 // Address of system/kernel PID node
   PID_NODE      * ReusedPidNode;                 // List of PID nodes that have been reused

   MOD_NODE      * ModRoot;                       // Root of cached module list (first module added)
   MOD_NODE      * ModEnd;                        // End of cached module list (last module added)

   JM_NODE       * JmPoolRoot;                    // Root of jitted method pool

   char          * KernelImageName;               // Fully qualified name non-stripped image
   char          * KernelMapName;                 // Fully qualified name of map file
   char          * KernelFilename;                // Filename of kernel image (as present in MTE hook)
   char          * KallsymsFilename;              // /proc/kallsyms. Could be included in some other file
   char          * ModulesFilename;               // /proc/modules. Could be included in some other file

   int           PidCnt;                          // Number of pid nodes
   int           ModCnt;                          // Number of cached modules
   int           JmPoolCnt;                       // Number of jitted methods in pool
   unsigned int  SystemPid;                       // System/Kernel PID
   int           ReusedPidCnt;                    // Numnber of re-used PID nodes

   char          * PathSeparator_str;             // Character that separates path components (as string)
   char          * SearchPathSeparator_str;       // Character that separates search path components (as string)
   char          * UnknownName;                   // Name given to unknown things
   char          * SymbolSearchPath;              // Paths on where to look for files to gather symbols
   char          * SymbolSearchPathWow64;         // Paths on where to look for files to gather symbols for Wow64 (Windows-only)

   char          * SpecialWindowsDlls;            // Names of Windows DLLs that, for some reason,
                                                  // don't include path information when the MTE
                                                  // is collected.

   RDESC_NODE    * range_desc_root;               // Root of range descriptors from range file.

   char          * collapsed_mmi_range_name;      // Set if A2N_MMI_RANGE_NAME environment
                                                  // variable is set to any value. The value will
                                                  // be the name given to the MMI interpreter
                                                  // symbol range.

   void          * jvmmap_data;                   // Where the data for the jvmmaps are
   char          * jvmmap_fn;                     // Set if A2N_JVMMAP_NAME environment variable
                                                  // is set to a value. This is an override to
                                                  // the default jvmmap.X filename if desired.
                                                  // No API available to set.

   char          * output_path;                   // Path where output files are written
                                                  // Set via A2N_OUTPUT_PATH environment variable

   char          PathSeparator;                   // Character that separates path components
   char          SearchPathSeparator;             // Character that separates search path components
   char          blank_sub;                       // What to substitute for blanks in symbol names
   char          res[5];

   int           SystemPidSet;                    // Whether or not SystemPid set by caller

   int           kernel_not_seen;                 // 1= haven't seen kernel module

   int           big_endian;                      // Whether machine we're running on is
                                                  // big endian or not.

   int           word_size;                       // Whether machine we're running on is
                                                  // 32 or 64 bits wide

   int           changes_made;                    // Incremented on every successful
                                                  // A2nAddModule(), A2nAddSymbol() and
                                                  // A2nAddSection()

   int           using_static_module_info;        // Set by A2nRestoreModules().
                                                  // When set, there will be *NO* symbol
                                                  // gathering - only the restored symbols,
                                                  // and any subsequentally added, will be
                                                  // used.

   int           debug;                           // Set if A2N_DEBUG environment variable is
                                                  // set (to any value).  Causes messages to
                                                  // stderr. Default is no debug messages.
                                                  // ***** A2nSetDebugMode()

   int           dump_sd;                         // Set if A2N_DUMP_SD environment variable
                                                  // is set (to yes).  Causes the contents of
                                                  // the SYMDATA structure to be dumped on
                                                  // every invocation of A2nGetSymbol().
                                                  // Default is not to dump the structure.

   int           display_error_messages;          // Set if A2N_ERROR_MESSAGES environment
                                                  // variable is set to any value.  Causes
                                                  // error messages to be displayed or
                                                  // suppressed on errors.
                                                  // ***** A2nSetErrorMessagesMode()

   int           immediate_gather;                // Set if A2N_IMMEDIATE_GATHER environment
                                                  // variable is set (to any value).  Causes
                                                  // symbols to be gathered at the time modules
                                                  // are added.  Default is to gather the first
                                                  // time a symbol is requested from a module.
                                                  // ***** A2nSetSymbolGatherMode()

   int           gather_code;                     // Set if A2N_GATHER_CODE environment
                                                  // variable is set (to any value).  Causes
                                                  // harvester to gather (save) the actual
                                                  // instruction stream along with the symbols.
                                                  // Ignored if we don't gather from an executable.
                                                  // Default is to not gather code.
                                                  // ***** A2nSetCodeGatherMode()

   int           validate_symbols;                // Cleared if A2N_VALIDATE environment
                                                  // variable is set NO.
                                                  // ***** A2nSetValidateKernelMode() and
                                                  // ***** A2nSetSymbolValidationMode()

   int           demangle_cpp_names;              // Set if A2N_DEMANGLE_NAMES environment
                                                  // variable is set (to any value)
                                                  // If set it causes the symbol harvester to
                                                  // attempt to demangle symbol names.
                                                  // ***** A2nSetDemangleCppNamesMode()

   int           demangle_complete;               // Set if A2N_DEMANGLE_COMPLETE environment
                                                  // variable is set (to any value)
                                                  // If set it causes the symbol harvester to
                                                  // attempt to demangle symbol names.
                                                  // 0= demangle only the name (no declaration
                                                  //    return types, attributes, etc.)
                                                  // 1= demangle completely.
                                                  // ***** A2nSetDemangleCppNamesMode()

   int           rename_duplicate_symbols;        // Set if A2N_RENAME_DUPLICATE_SYMBOLS environment
                                                  // variable is set (to any value)
                                                  // If set it causes an additional check to be
                                                  // made to detect duplicate symbol names and
                                                  // to rename them to something unique.
                                                  // 0= don't check and don't rename
                                                  // 1= check and rename duplicate symbols
                                                  // Default is not to check and not to rename.
                                                  // ***** A2nSetRenameDuplicateSymbolsMode()

   int           nsf_action;                      // Set if A2N_NSF_ACTION environment
                                                  // variable is set.  Performs different
                                                  // actions when NoSymbolFound..
                                                  // ***** A2nSetNoSymbolFoundAction()

   int           module_dump_on_exit;             // Set if A2N_MODULE_DUMP environment
                                                  // variable is set (to any value).  Causes
                                                  // module dump on shared object unload.
                                                  // Default is not to dump on exit.

   int           process_dump_on_exit;            // Set if A2N_PROCESS_DUMP environment
                                                  // variable is set (to any value).  Causes
                                                  // process dump on shared object unload.
                                                  // Default is not to dump on exit.

   int           msi_dump_on_exit;                // Set if A2N_MSI_DUMP environment variable
                                                  // is set (to any value).  Causes an msi
                                                  // file to be generated on exit.
                                                  // Default is not to dump on exit.

   int           nmf_threshold;                   // Set if A2N_NOMOD_THRESHOLD environment
                                                  // variable is set to a number.  Causes
                                                  // Process/Module dumps to be generated if
                                                  // the number of A2nGetSymbol() requests
                                                  // returning A2N_NO_MODULE equals or exceeds
                                                  // the threshold value.
                                                  // ***** A2nSetNoModuleFoundDumpThreshold()

   int           nsf_threshold;                   // Set if A2N_NOSYM_THRESHOLD environment
                                                  // variable is set to a number.  Causes
                                                  // Process/Module dumps to be generated if
                                                  // the number of A2nGetSymbol() requests
                                                  // returning A2N_NO_SYMBOL_FOUND equals or
                                                  // exceeds the threshold value.
                                                  // ***** A2nSetNoSymbolFoundDumpThreshold()

   int           symbol_quality;                  // Set if A2N_SYMBOL_QUALITY environment
                                                  // variable is set to a number.  Sets the
                                                  // type of symbols the caller is willing
                                                  // to accept on A2nGetSymbol() requests.
                                                  // ***** A2nSetSymbolQualityMode()

   int           harvest_exports_only;            // Set if A2N_EXPORTS_ONLY environment
                                                  // variable is set to something.  Forces
                                                  // A2N to only harvest exported symbols.
                                                  // ***** No API available to modify.

   int           always_harvest;                  // Set if A2N_ALWAYS_HARVEST environment
                                                  // variable is set to something.  Forces
                                                  // A2N to always harvest symbols, even if
                                                  // the quality is not what the user wants.
                                                  // ***** No API available to modify.

   int           do_fast_lookup;                  // Set if A2N_FAST_LOOKUP environment
                                                  // variable is set to yes. This is also
                                                  // the A2N symbol lookup default.
                                                  // Forces A2N to always check if requested
                                                  // symbol is the same as the last one returne,
                                                  // ***** No API available to modify.

   int           collapse_ibm_mmi;                // Set if A2N_COLLAPSE_MMI environment
                                                  // variable is set to yes. It causes all
                                                  // symbols in the range of the MMI interpreter
                                                  // to be collapsed into "IBM_MMI".
                                                  // ***** A2nCollapseMMIRange()

   int           return_range_symbols;            // Set if A2N_RETURN_RANGE_SYMBOLS environment
                                                  // variable is set to yes. It causes the actual
                                                  // symbol within a range to be appended to the
                                                  // range name when returning the symbol name.

   int           jvmmap_cnt;                      // Number of jvmmaps processed

   int           lineno;                          // Return source line number if available
                                                  // Set if A2N_LINENO environment variable
                                                  // is set to yes.

   int           ignore_ts_cs;                    // Ignore timestamp and/of checksum when
                                                  // checking if OK to use on-disk image.
                                                  // Set if A2N_IGNORE_TSCS environment
                                                  // variable is set to yes.

   int           use_kernel_image;                // Get kernel symbols from the on-disk image.
                                                  // Set by A2N_KERNEL_IMAGE_NAME environment
                                                  // variable and/or by the A2nSetKernelImageName()
                                                  // API.

   int           use_kernel_map;                  // Get kernel symbols from the System.map file.
                                                  // Set by A2N_KERNEL_MAP_NAME environment variable
                                                  // and/or by the A2nSetKernelMapName() API
                                                  // API.

   int           use_kallsyms;                    // Get kernel symbols from the kallsyms file.
                                                  // Set by the A2nSetKallsymsFileLocation() API.

   int           use_modules;                     // Get kernel module symbols using the modules
                                                  // and kallsyms files.
                                                  // Set by the A2nSetModulesFileLocation() API.

   int           quit_on_validation_error;        // Quit if kernel symbols can't be validated.
                                                  // Set by A2N_QUIT_ON_VALIDATION_ERROR environment
                                                  // variable and/or by A2nStopOnKernelValidationError()
                                                  // API.

   int           snp_elements;
   int           snp_size;                        // Set if A2N_SNA_ELEMENTS environment
                                                  // variable is set to a valid number. Sets
                                                  // the size of the temporary Symbol Node
                                                  // pointer array.
                                                  // ***** No API available to modify.

   int           msi_file_wordsize;               // Word size of machine where MSI file written
   int           msi_file_bigendian;              // 1=Big endian machine, 0=Little endian
   int           msi_file_os;                     // OS where MSI file written

   int           kernel_modules_added;            // Linux: read /proc/modules file and added
                                                  // loaded module block for each kernel module.
   int           kernel_modules_harvested;        // Linux: read /proc/kallsyms file and
                                                  // harvested symbols for kernel modules.

   int64_t       kallsyms_file_offset;            // Offset to start reading kallsyms file
   int64_t       kallsyms_file_size;              // How much of kallsyms to read
   int64_t       modules_file_offset;             // Offset to start reading modules file
   int64_t       modules_file_size;               // How much of modules to read

   int           ok_to_use_kernel_image;          // Never use the kernel image to get kernel
                                                  // symbols. Set by A2N_DONT_USE_KERNEL_IMAGE
   int           ok_to_use_kernel_map;            // Never use the kernel map to get kernel
                                                  // symbols. Set by A2N_DONT_USE_KERNEL_MAP
   int           ok_to_use_kallsyms;              // Never use /proc/kallsyms to get kernel
                                                  // symbols. Set by A2N_DONT_USE_KALLSYMS
   int           ok_to_use_modules;               // Never harvest kernel modules symbols.
                                                  // Set by A2N_DONT_USE_MODULES

   int           symopt_debug;                    // Set SYMOPT_DEBUG dbghelp option.
                                                  // Set by A2N_SYMOPT_DEBUG
   int           symopt_load_anything;            // Set SYMOPT_LOAD_ANYTHING dbghelp option.
                                                  // Set by A2N_SYMOPT_LOAD_ANYTHING

   int           add_module_cache_head;           // Add modules at the head (fron) of the cached
                                                  // module list. If not set, add to the tail (back)
                                                  // of the list.
                                                  // Set by A2N_ADD_MODULE_CACHE_HEAD and reset by
                                                  // A2N_ADD_MODULE_CACHE_TAIL environment variables.
                                                  // ***** No API available to modify.

   int           append_pid_to_fn;                // Append PID to output filenames

   int           mt_mode;                         // Multi-threaded mode. Don't do things that
                                                  // aren't thread safe. That's the idea.
                                                  // Set by A2nSetMultiThreadedMode().
                                                  // - Sets do_fast_lookup = 0
   int           st_mode;                         // Single-threaded mode. Inverse of mt_mode.
};
#endif // _GV_H_
