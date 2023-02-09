//----------------------------------------------------------------------------
//
//   IBM Performance Inspector
//   Copyright (c) International Business Machines Corp., 2003 - 2010
//
//   This library is free software; you can redistribute it and/or
//   modify it under the terms of the GNU Lesser General Public
//   License as published by the Free Software Foundation; either
//   version 2.1 of the License, or (at your option) any later version.
//
//   This library is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//   Lesser General Public License for more details.
//
//   You should have received a copy of the GNU Lesser General Public License
//   along with this library; if not, write to the Free Software Foundation,
//   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
//----------------------------------------------------------------------------
//
// General purpose routines that provide interface to the kernel driver.
//
// Change History
//

#include "perfutil_core.hpp"

//
// Architecture strings
// ********************
//

const char *p5_arch_string        = "x86 (P5)";
const char *p6_arch_string        = "x86 (P6)";
const char *pentium_m_arch_string = "x86 (Pentium M)";
const char *core_sd_arch_string   = "x86 (Core Solo/Duo)";
const char *core_arch_string      = "x86 (Core 2)";
const char *p4_arch_string        = "x86 (P4)";
const char *ppc64_arch_string     = "PPC64";
const char *amd64_arch_string     = "x86_64 (AMD)";
const char *em64t_arch_string     = "x86_64 (Intel)";
const char *s390_arch_string      = "S390";

const char *intel_mfg_string = "Intel";
const char *amd_mfg_string   = "AMD";
const char *ibm_mfg_string   = "IBM";


int     CpuAmdCoreIdBits;
UINT32  CpuThreadTopologyEax;            // I:   CPUID(0xB,0) eax / A: not supported
UINT32  CpuThreadTopologyEbx;            // I:   CPUID(0xB,0) ebx / A: not supported
UINT32  CpuCoreTopologyEax;              // I:   CPUID(0xB,1) eax / A: not supported
UINT32  CpuCoreTopologyEbx;              // I:   CPUID(0xB,1) ebx / A: not supported
int     have_topology_data;              // Non-zero Thread and Core topology data
UINT32  CpuidMaxIndex;                   // A/I: CPUID(0) eax
UINT32  CpuidMaxExtIndex;                // A/I: CPUID(0x80000000) eax
UINT32  CpuArchPerfMonFeaturesEax;       // I:   CPUID(0xA) eax / A: not supported
UINT32  CpuArchPerfMonFeaturesEbx;       // I:   CPUID(0xA) ebx / A: not supported
UINT32  CpuArchPerfMonFeaturesEdx;       // I:   CPUID(0xA) edx / A: not supported

//
//  get_CPUSpeed()
//  **************
// This is the only way?
// TODO: change all of this
//
void get_CPUSpeed(void)
{
#if !defined(CONFIG_S390X) && !defined(CONFIG_S390)
    int    fd;
    FILE * tmpfile;
    char   TmpfileName[15] = "perfutilXXXXXX";
    char   sysCommand[80];

    fd = mkstemp(TmpfileName);
    sprintf(sysCommand, "grep -m 1 MHz /proc/cpuinfo | awk -F : '{print $2}' > %s", TmpfileName);

    if (system(sysCommand) == -1) {
       printf_se("get_CPUSpeed(): ERROR system\n");
       close(fd);
       remove(TmpfileName);
       return;
    }

    tmpfile = fdopen(fd, "r+");
    if (tmpfile == NULL) {
       printf_se("get_CPUSpeed(): ERROR fdopen\n");
       close(fd);
       remove(TmpfileName);
       return;
    }

    rewind(tmpfile);
    if (fscanf(tmpfile, "%f", &CPUSpeed) != 1) {
       printf_se("get_CPUSpeed(): ERROR fscanf for CPUSpeed \n");
    }
    else {
       CPUSpeed = CPUSpeed * 1000 * 1000;
       llCPUSpeed = (uint64_t)CPUSpeed;
    }
    fclose(tmpfile);
    remove(TmpfileName);
#endif

    // If we could not get it from /proc/cpuinfo,
    // try the driver

    if (llCPUSpeed == 0) {
       llCPUSpeed = pmd->llCpuSpeed;
    }

    return;
}


//
// Processor information/affinity
// -------------------------------
//

//
// GetProcessorType()
// ******************
//
// Returns the processor type, or 0 on errors
//
// - Processor type is bits 8:11 of the eax register value
//   returned by CPUID with index value of 1.
// - CPUID_P5_FAMILY:  P5 (Pentium, Pentium MMX)
// - CPUID_P6_FAMILY:  P6 (Pentium Pro/II/III, Celeron)
// - CPUID_P4_FAMILY:  P15 (Pentium 4)
// - PVR_FAM(pvr):     PPC64
//
int __cdecl GetProcessorType(void)
{
   return(pmd->CpuFamily);
}


//
// GetProcessorArchitecture()
// **************************
//
// Returns the processor hardware architecture
//
int __cdecl GetProcessorArchitecture(void)
{
   return(pmd->CpuArchitecture);
}


//
// GetProcessorArchitectureString()
// ********************************
//
// Returns the processor hardware architecture string
//
const char * __cdecl GetProcessorArchitectureString(void)
{
   if (pmd->is_p4)
      return(p4_arch_string);
   if (pmd->is_p6) {
      if (pmd->is_pentium_m)
         return (pentium_m_arch_string);
      if (pmd->is_core_sd)
         return (core_sd_arch_string);
      if (pmd->is_core)
         return (core_arch_string);
      return(p6_arch_string);
   }
   if (pmd->is_amd64)
      return(amd64_arch_string);
   if (pmd->is_em64t)
      return(em64t_arch_string);
   if (pmd->is_ppc64)
      return(ppc64_arch_string);
   if ( (pmd->is_s390x) || (pmd->is_s390) )
      return(s390_arch_string);
   return(NULL);
}


//
// GetProcessorManufacturer()
// **************************
//
// Returns the processor hardware manufacturer
//
int __cdecl GetProcessorManufacturer(void)
{
   return(pmd->CpuManufacturer);
}

//
// GetProcessorManufacturerString()
// ********************************
//
// Returns the processor hardware manufacturer string
//
const char * __cdecl GetProcessorManufacturerString(void)
{
   if (pmd->is_intel)
      return(intel_mfg_string);
   if (pmd->is_amd)
      return(amd_mfg_string);
   if (pmd->is_ppc64)
      return(ibm_mfg_string);
   return(NULL);
}

//
// GetProcessorByteOrdering()
// **************************
//
int __cdecl GetProcessorByteOrdering(void)
{
#if __BYTE_ORDER == __BIG_ENDIAN
   return(CPU_BIG_ENDIAN);
#else
   return(CPU_LITTLE_ENDIAN);
#endif
}

//
// GetProcessorWordSize()
// **********************
//
int __cdecl GetProcessorWordSize(void)
{
#ifdef CONFIG_PI_TRACE32
   return(CPU_32BITS);
#else
   return(CPU_64BITS);
#endif
}


//
// GetLogicalProcessorsPerPackage()
// ********************************
//
int __cdecl GetLogicalProcessorsPerPackage(void)
{
   return(pmd->LogicalCpuCount);
}


//
// GetCoresPerPackage()
// ********************
//
int __cdecl GetCoresPerPackage(void)
{
   return(pmd->CoresPerPhysical);
}


//
// GetProcessorSignature()
// ***********************
//
// Returns the processor signature, or 0 on errors
//
// - Processor signature is the contents of the eax register returned
//   by CPUID with index value of 1 (x86)
//
UINT32 GetProcessorSignature(void)
{
   return(pmd->CpuSignature);
}


//
// GetProcessorFeatureFlagsEdx()
// *****************************
//
// Returns the processor features bit mask, or 0 on errors
//
// - Processor features is the contents of the edx register returned
//   by CPUID with index value of 1.
//
UINT32 __cdecl GetProcessorFeatureFlagsEdx(void)
{
   return(pmd->CpuFeatures);
}


//
// GetProcessorFeatureFlagsEcx()
// *****************************
//
// Returns the processor features bit mask, or 0 on errors
//
// - Processor features is the contents of the ecx register returned
//   by CPUID with index value of 1.
//
UINT32 __cdecl GetProcessorFeatureFlagsEcx(void)
{
   return(pmd->CpuFeaturesEcx);
}


//
// GetProcessorExtendedFeatures()
// ******************************
//
// Returns the processor features bit mask, or 0 on errors
//
// - Processor features is the contents of either the edx register AMD) or
//   the eax register (Intel) returned by CPUID with index value of 0x80000001.
//
UINT32 __cdecl GetProcessorExtendedFeatures(void)
{
   return(pmd->CpuExtendedFeatures);
}


//
// GetProcessorSpeed()
// *******************
//
// Returns the processor speed in Hz
//
//
INT64 __cdecl GetProcessorSpeed(void)
{
   if (0 == llCPUSpeed)
   	get_CPUSpeed();

   return(llCPUSpeed);
}


//
// GetFrontSideBusSpeed()
// **********************
//
// Returns the front side bus (system bus) speed in Hz
//
int __cdecl GetFrontSideBusSpeed(void)
{
   return((int)pmd->FSBSpeed);
}


//
// PiGetActiveProcessorCount()
// *************************
//
int PiGetActiveProcessorCount()
{
   return(pmd->ActiveCPUs);

   //return sysconf (_SC_NPROCESSORS_CONF);
   //return sysconf(_SC_NPROCESSORS_ONLN);
}


//
// GetActiveProcessorAffinityMask()
// ********************************
//
// Returns affinity mask with all active CPU's bit set
//
UINT64  __cdecl GetActiveProcessorAffinityMask(void)
{
   return(pmd->ActiveCPUSet);
}


//
// GetPhysicalProcessorCount()
// ***************************
//
// Returns the number of physical CPUs in the system
// * In a HyperThread-enabled system the number of physical processors
//   can be less-than or equal-to the number of active processors.
//
int __cdecl GetPhysicalProcessorCount(void)
{
   return(pmd->InstalledCPUs);
}


//
// GetPhysicalProcessorAffinityMask()
// **********************************
//
// Returns affinity mask with all physical CPU's bit set
//
UINT64 __cdecl GetPhysicalProcessorAffinityMask(void)
{
   return(pmd->InstalledCPUSet);
}


//
// IsSystemSmp()
// *************
//
// Returns TRUE/non-zero if system is SMP (more than 1 active processors)
// or FALSE/zero if system is UNI (1 active processor)
//
int __cdecl IsSystemSmp(void)
{
   if (pmd->ActiveCPUs > 1)
      return(1);
   else
      return(0);
}


//
// IsSystemUni()
// *************
//
// Returns TRUE/non-zero if system is UNI (1 active processor)
// or FALSE/zero if system is SMP (more than 1 active processors)
//
int __cdecl IsSystemUni(void)
{
   if (pmd->ActiveCPUs == 1)
      return(1);
   else
      return(0);
}


//
// IsProcessor32Bits()
// *******************
//
// Returns TRUE/non-zero if 32-bit processor, FALSE/zero otherwise.
//
int __cdecl IsProcessor32Bits(void)
{
   return(pmd->is_32bit);
}


//
// IsProcessor64Bits()
// *******************
//
// Returns TRUE/non-zero if 64-bit processor, FALSE/zero otherwise.
//
int __cdecl IsProcessor64Bits(void)
{
   return(pmd->is_64bit);
}


//
// IsProcessorLittleEndian()
// *************************
//
// Returns TRUE/non-zero if little endian processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorLittleEndian(void)
{
#if __BYTE_ORDER == __BIG_ENDIAN
   return(0);
#else
   return(1);
#endif
}


//
// IsProcessorBigEndian()
// **********************
//
// Returns TRUE/non-zero if big endian processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorBigEndian(void)
{
#if __BYTE_ORDER == __BIG_ENDIAN
   return(1);
#else
   return(0);
#endif
}


//
// IsProcessorIntel()
// ******************
//
// Returns TRUE/non-zero if Intel processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorIntel(void)
{
   return(pmd->is_intel);
}


//
// IsProcessorAmd()
// ****************
//
// Returns TRUE/non-zero if AMD processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorAmd(void)
{
   return(pmd->is_amd);
}


//
// IsProcessorX86()
// ****************
//
// Returns TRUE/non-zero if x86 processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorX86(void)
{
   return(pmd->is_x86);
}


//
// IsProcessorX64()
// ****************
//
// Returns TRUE/non-zero if x64 processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorX64(void)
{
   return(pmd->is_x64);
}


//
// IsProcessorAMD64()
// ******************
//
// Returns TRUE/non-zero if AMD64 processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorAMD64(void)
{
   return(pmd->is_amd64);
}


//
// IsProcessorEM64T()
// ******************
//
// Returns TRUE/non-zero if EM64T processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorEM64T(void)
{
   return(pmd->is_em64t);
}


//
// IsProcessorP4()
// ***************
//
// Returns TRUE/non-zero if P4 processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorP4(void)
{
   return(pmd->is_p4);
}


//
// IsProcessorP15()
// ****************
//
// Returns TRUE/non-zero if AMD P15 processor, FALSE/zero otherwise.
// ##### THIS NEEDS TO BE FIXED #####
//
int __cdecl IsProcessorP15(void)
{
   return (pmd->is_amd64);
}


//
// IsProcessorP6()
// ***************
//
// Returns TRUE/non-zero if P6 processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorP6(void)
{
   return(pmd->is_p6);
}


//
// IsProcessorPentiumM()
// *********************
//
// Returns TRUE/non-zero if Pentium-M processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorPentiumM(void)
{
   return(pmd->is_pentium_m);
}


//
// IsProcessorCoreSD()
// *******************
//
// Returns TRUE/non-zero if Core Solo/Core Duo processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorCoreSD(void)
{
   return(pmd->is_core_sd);
}


//
// IsProcessorCore()
// *****************
//
// Returns TRUE/non-zero if Core 2 (Core architecture) processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorCore(void)
{
   return(pmd->is_core);
}

//
// IsProcessorNehalem()
// ********************
//
// Returns TRUE/non-zero if Nehalem architecture (Core i7/Core i5) processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorNehalem(void)
{
   return (pmd->is_nehalem);
}

//
// IsProcessorWestmere()
// ********************
//
// Returns TRUE/non-zero if westmere architecture processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorWestmere(void)
{
   return (pmd->is_westmere);
}

//
// IsProcessorSandybridge()
// ********************
//
// Returns TRUE/non-zero if Nehalem architecture (Core i7/Core i5) processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorSandybridge(void)
{
   return (pmd->is_sandybridge);
}

int __cdecl IsProcessorIvybridge(void)
{
   return (pmd->is_ivybridge);
}

//
// IsProcessorAtom()
// *****************
//
// Returns TRUE/non-zero if Intel Atom processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorAtom(void)
{
   return (pmd->is_atom);
}


//
// IsProcessorMultiCore()
// **********************
//
// Returns TRUE/non-zero if multi-core processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorMultiCore(void)
{
   return(pmd->is_multicore);
}


//
// IsProcessorOpteron()
// ********************
//
// Returns TRUE/non-zero if AMD Opteron processor, FALSE/zero otherwise.
// ##### THIS NEEDS TO BE FIXED #####
//
int __cdecl IsProcessorOpteron(void)
{
   return (pmd->is_amd64);
}


//
// IsProcessorAthlon()
// *******************
//
// Returns TRUE/non-zero if AMD Athlon processor, FALSE/zero otherwise.
// ##### THIS NEEDS TO BE FIXED #####
//
int __cdecl IsProcessorAthlon(void)
{
   return (pmd->is_amd64);
}


//
// IsProcessorPPC64()
// ******************
//
// Returns TRUE/non-zero if PPC64 processor, FALSE/zero otherwise.
//
int __cdecl IsProcessorPPC64(void)
{
   return(pmd->is_ppc64);
}


//
// IsHyperThreadingSupported()
// ***************************
//
// Returns whether (TRUE/non-zero) or not (FALSE/zero) HyperThreading
// is supported by the processor. HyperThreading may or may not be
// enabled in the BIOS.
//
int __cdecl IsHyperThreadingSupported(void)
{
   return(pmd->HyperThreadingSupported);
}

//
// IsHyperThreadingEnabled()
// *************************
//
// Returns whether (TRUE/non-zero) or not (FALSE/zero) HyperThreading
// is currently enabled.  Needless to say it can only ever be enabled
// on processors that support HyperThreading and the feature has been
// enabled in the BIOS.
//
int __cdecl IsHyperThreadingEnabled(void)
{
   return(pmd->HyperThreadingEnabled);
}

//
// IsS390()
// ********
//
int __cdecl IssS390(void)
{
   return(pmd->is_s390 || pmd->is_s390x);
}

//
// GetActiveProcessorList()
// ************************
//
int __cdecl GetActiveProcessorList(int cpu_list[], int elements)
{
   if (cpu_list == NULL || elements <= 0)
      return(0);

   memset(cpu_list, 0, (elements * sizeof(int)));

   if (elements > pmd->ActiveCPUs)
	   elements = pmd->ActiveCPUs;

   memcpy(cpu_list, pmd->cpu_list, sizeof(UINT32) * elements);

   return(elements);
}
