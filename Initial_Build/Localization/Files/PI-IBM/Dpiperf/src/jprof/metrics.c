#include "jprof.h"
#include "hash.h"
#include "utils/pi_time.h"

void AddPhysMetric( int metric )
{
   int i;

   for ( i = 0; i < gv->physmets; i++ )
   {
      if ( gv->physmet[i] == metric )
      {
         break;
      }
   }
   if ( i == gv->physmets )
   {
      if ( gv->physmets < gv_MaxMetrics )
      {
         gv->physmet[ gv->physmets ] = metric;   // PTT metric
         gv->physmets++;
      }
      else
      {
         err_exit( "Too many physical metrics" );
      }
   }
   if ( MODE_PTT_INSTS == metric )
   {
      gv->indexPTTinsts = i;            // Save index of PTT_INSTS for possible CPI
   }
}

int SetMetric( int metric )
{
   if ( 0 == gc.posNeg )                // Not adding a metric, reset to start
   {
      gv->usermets    = 0;

      gv->physmets = 0;
   }
   else if ( 0 > gc.posNeg )            // Subtracting a metric is INVALID
   {
      ErrVMsgLog( "Metrics may only be replaced or added, not subtracted\n" );
      return(JVMTI_ERROR_INTERNAL);
   }

   if ( gv->usermets < gv_MaxMetrics )
   {
      gv->usermet[ gv->usermets ] = metric;   // Logical metrics
      gv->sf[      gv->usermets ] = gv->scaleFactor;
      gv->usermets++;

      if ( metric < MODE_RAW_CYCLES )
      {
         AddPhysMetric( metric );
      }
      else if ( MODE_CPI == metric )
      {
         AddPhysMetric( MODE_PTT_CYCLES );
         AddPhysMetric( MODE_PTT_INSTS );
      }

      if ( MODE_NOMETRICS == metric )
      {
         gv->NoMetrics = 1;
      }
      else
      {
         gv->NoMetrics = 0;
      }
   }
   else
   {
      ErrVMsgLog( "Too many logical metrics\n" );
      return(JVMTI_ERROR_INTERNAL);
   }
   return(JVMTI_ERROR_NONE);
}

int SetMetricMapping()
{
   int physmet;
   int usermet;
   int metric;

   OptVMsgLog( "\n" );

   gv->physmet[gv->physmets] = MODE_RAW_CYCLES;

   for ( usermet = 0; usermet < gv->usermets; usermet++ )
   {
      OptVMsgLog( " Logical Metric #%d = %s\n", usermet, ge.mmnames[gv->usermet[usermet]] );
   }
   for ( physmet = 0; physmet <= gv->physmets; physmet++ )
   {
      OptVMsgLog( "Physical Metric #%d = %s\n", physmet, ge.mmnames[gv->physmet[physmet]] );
   }

   if ( gv->NoMetrics )
   {
      OptVMsgLog( "No Logical Mappings when using NOMETRICS mode\n" );
   }
   else
   {
      for ( usermet = 0; usermet < gv->usermets; usermet++ )
      {
         metric = gv->usermet[usermet];

         if ( MODE_CPI == metric )
         {
            metric = MODE_PTT_CYCLES;
         }

         for ( physmet = 0; physmet <= gv->physmets; physmet++ )
         {
            if ( gv->physmet[physmet] == metric )
            {
               break;
            }
         }
         if ( physmet <= gv->physmets )
         {
            gv->mapmet[usermet] = physmet;
         }
         else
         {
            ErrVMsgLog( "Requested metric not available for mapping\n" );
            return(JVMTI_ERROR_INTERNAL);
         }
      }
      for ( usermet = 0; usermet < gv->usermets; usermet++ )
      {
         OptVMsgLog( "Logical Mapping #%d = %d\n", usermet, gv->mapmet[usermet] );
      }
   }

   return(JVMTI_ERROR_NONE);
}

char * getScaledName( char * namebuf, int metric )
{
   int n;

   if ( ge.newHeader )
   {
      strcpy( namebuf, ge.mmdescr[gv->usermet[metric]] );
   }
   else
   {
      strcpy( namebuf, ge.mmnames[gv->usermet[metric]] );
   }

   n = (int)strlen( namebuf );

   if ( gv->sf[metric] )
   {
      if ( gv->sf[metric] > 0 )
      {
         sprintf( namebuf + n,
                  " ( scaled by 10E+%02d )",
                  gv->sf[metric] );
      }
      else
      {
         sprintf( namebuf + n,
                  " ( reported as 10E%03d seconds )",
                  1 + gv->sf[metric] );
      }
   }
   return( namebuf );
}

//======================================================================
// Time Processing routines
//----------------------------------------------------------------------
// The idea is to get the start of instrumentation time with constant
// pre-time overhead and the end of instrumentation time with constant
// post-time overhead.  This allows us to disregard all time spent
// between those points and provide instrumentation with seemingly
// constant overhead.
//======================================================================

//======================================================================
// Get current time with constant pre-time overhead
//======================================================================

thread_t * timeJprofEntry()
{

   UINT64      metrics[PTT_MAX_METRICS];
   UINT64      timeStamp;
   int         cpuNum;
   int         rc;
   thread_t  * tp = NULL;

   if ( gv->gatherMetrics && gv->rton)
   {
      if ( gv->physmets )
      {
         // Get the metric data
         rc = PttGetMetricData(metrics);
         if ( rc )
         {
            err_exit("PttGetCurrentThreadData failure\n");
         }

         cpuNum = 0;
         timeStamp = get_timestamp();
         metrics[gv->physmets] = timeStamp;

         tp = insert_thread( CurrentThreadID() );

         // Read Data
         if (tp->ptt_enabled)
         {
            if ( gv->calltree || gv->calltrace )
            {
               timeAccum(tp, metrics);
            }
         }
      }
      else if ( gv->NoMetrics )         // Special NOMETRICS metric
      {
         tp = insert_thread( CurrentThreadID() );
         timeStamp = gv->timeStamp++;   // Not thread safe, but does not matter
      }
      else
      {
         cpuNum = 0;
         timeStamp = get_timestamp();
         tp = insert_thread( CurrentThreadID() );
      }

      if (tp)
      {
         tp->timeStamp = timeStamp;
         tp->cpuNum = cpuNum;
      }
   }
   else
   {
      tp = insert_thread( CurrentThreadID() );
   }

   return tp;
}
//======================================================================
// Accumulate metrics and discard time spent inside JPROF
//======================================================================

void timeAccum( thread_t * tp, UINT64 *metrics )
{
   int    met;

   if ( metrics )
   {
      if ( tp->flags & thread_resetdelta )
      {
         for ( met = 0; met <= gv->physmets; met++ )
         {
            tp->metDelt[met] = 0;
         }

         tp->flags &= ~thread_resetdelta;
      }

      if ( tp->metEntr[gv->physmets] )
      {
         for ( met = 0; met <= gv->physmets; met++ )
         {
            // metJprf accumulates time spent inside JPROF (the instrumentation).
            // It accumulates the time between previous entry and exit to JPROF.
            // It is computed now to avoid disturbing the exit logic.

            tp->metJprf[met] += tp->metExit[met] - tp->metEntr[met];

            // metDelt accumulates time spent outside JPROF (the application).
            // It accumulates the time between previous exit and this entry to
            // JPROF.  It is reset each time the metrics are applied.

            tp->metEntr[met]  = metrics[met];
            tp->metDelt[met] += tp->metEntr[met] - tp->metExit[met];
         }
      }
      else                           // Make sure the next event works
      {
         for ( met = 0; met <= gv->physmets; met++ )
         {
            tp->metEntr[met]  = metrics[met];
         }
      }
   }
}

//======================================================================
// Apply the times acquired by timeJprofEntry/timeAccum
//======================================================================

void timeApply( thread_t * tp )
{
   unsigned int x;
   int          met;

   pNode        mpNode;                 // Current Node

   if ( gv_rton_Trees != gv->rton ) return;   // trees only

   if ( gv->addbase                     // default is 1 (rtdriver: pause=0, restart=1)// Calibration question
        && tp->currm != tp->currT )     // Only apply to method nodes
   {
      mpNode = (pNode)(tp->currm);      // Set mpNode to current Node on thread

      for ( met = 0; met < gv->physmets; met++ )
      {
         mpNode->bmm[met] += tp->metDelt[met];   // Update base time
         tp->metAppl[met] += tp->metDelt[met];   // Add to thread total
      }
   }

   tp->flags |= thread_resetdelta;
}

//======================================================================
// Get the current time with constant time after reading metrics.
//======================================================================

void timeJprofExit( thread_t * tp )
{
   int    met;
   int    rc;

   // Begin cleanup at End of Event

   if ( gv->shutdown_count && gv->rton )
   {
      if ( gv->shutdown_event < 0
           || tp->etype == gv->shutdown_event )
      {
         if ( 0 == --gv->shutdown_count )
         {
            ErrVMsgLog( "\nSHUTDOWN_EVENT encountered, terminating JVM\n" );

            StopJProf( sjp_SHUTDOWN_EVENT );
         }
      }
   }

   if ( tp->x23 <= gv_ee_ExitPop )
   {
      tp->mt1 = tp->mt2;                // Remember history of types
      tp->x12 = tp->x23;                // Remember history of transitions
      tp->mt2 = tp->mt3;                // Remember history of types
   }
   tp->mp  = 0;

//----------------------------------------------------------------------
// Time processing at end of JPROF with constant exit overhead
//----------------------------------------------------------------------

   if ( gv->gatherMetrics )
   {
      if ( gv->rton )      // ( x || y ) compare not constant path
      {
         if ( gv->physmets )            // Per thread metrics
         {
            // Gather metrics
            tp->metExit[gv->physmets] = get_timestamp();

            rc = PttGetMetricData(tp->metExit);
            if ( rc ) {
               err_exit("PttGetCurrentThreadData failure\n");
            }
         }
         else if ( gv->NoMetrics )      // Special NOMETRICS mode
         {
            tp->metExit[0]     = tp->timeStamp;
         }
         else                           // Raw cycles as only metric
         {
            tp->cpuNum         = 0;
            tp->metExit[0]     = get_timestamp();
         }
      }
   }
   tp->fWorking = 0;
   memory_fence();
}
