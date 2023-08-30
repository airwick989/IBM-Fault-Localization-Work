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

#include "pu_scs.hpp"

//
// Java Callstack Sampling
// -----------------------
//


int __cdecl InitScs(UINT32 requested_rate, scs_callback callback)
{
   int rc =_init_scs(callback);
   RETURN_RC_FAIL(rc);
   rc = SetScsRate(requested_rate);
   return rc;
}

int __cdecl ScsInitThread(UINT32 scs_active, int tid)
{
   return _scs_init_thread(scs_active, tid);
}

int __cdecl ScsUninitThread(int tid)
{
   return _scs_uninit_thread(tid);
}

int __cdecl ScsOn()
{
   int rc = _scs_on();
   RETURN_RC_FAIL(rc);
   pmd->scs_enabled = 1;
   return rc;
}

int __cdecl ScsOff()
{
   int rc = _scs_off();
   RETURN_RC_FAIL(rc);
   pmd->scs_enabled = 0;
   return rc;
}

int __cdecl ScsPause()
{
   int rc = _scs_pause();
   RETURN_RC_FAIL(rc);
   pmd->scs_enabled = 0;
   return rc;
}

int __cdecl ScsResume()
{
   int rc = _scs_resume();
   RETURN_RC_FAIL(rc);
   pmd->scs_enabled = 1;
   return rc;
}


//
// pi_get_default_scs_rate()
// *************************
//
uint32_t  __cdecl pi_get_default_scs_rate(void)
{
   if (IsCallstackSamplingSupported())
      return (pmd->CpuTimerTick / pmd->DefaultTimeScsDivisor);
   else
      return (32);
}


//
// SetScsRate()
// ************
//
// Called to set SCS rate when doing Time-based SCS (running off the
// system's Timer Tick interrupt).
int __cdecl SetScsRate(UINT32 requested_rate) {

   int rc;

   // Time-based sampling:
   if (requested_rate == 0) {
      rc = PU_ERROR_INVALID_PARAMETER;
   }
   // Set SCS rate to requested rate
   else {
      pmd->CurrentScsRate = requested_rate;
      rc = PU_SUCCESS;
   }

   if (rc != PU_SUCCESS) {
      printf_se("## SetScsRate: Invalid rate (%d)\n", requested_rate);
      return rc;
   }

   printf_se("## SetScsRate: Done. rc = %d (%s)\n", rc, RcToString(rc));
   return rc;
}
