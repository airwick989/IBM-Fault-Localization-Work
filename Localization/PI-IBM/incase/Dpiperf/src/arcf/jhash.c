/*
 *   Copyright (c) IBM Corporation 1997.
 *   Property of IBM Corporation.
 *   All Rights Reserved.
 *   Origin 30 (IBM)
 *
 * Description : Hashing Routines
 *
 */

#include "global.h"
#include "jhash.h"

#define maxBufSize 512

/******************************/
/*** METHOD SECTION ***/

/******************************/
method_t * lookup_method(void * mb)
{
   return( (method_t *)hashLookup(&gv.method_table, mb) );
}

/******************************/
method_t * insert_method( void * mb )
{
   method_t * mp;

   if ( 0 == gv.method_table.bucks )
   {
      hashInit(&gv.method_table, 8009, sizeof(method_t), "method_table", 0, 0);
   }

   mp = hashLookup(&gv.method_table, mb);

   if ( NULL == mp )
   {
      gc.allocMsg = "insert_method";

      mp = (method_t *)hashAdd(&gv.method_table, mb);

      if ( NULL == mp )
      {
         err_exit("table_put in insert_method");
      }
   }
   return( mp );
}
/*** end method hash routines ***/


/******************************/
/*** JSTACK SECTION ***/

jsp_t * lookup_jstack(uint jsp)
{
   return( (jsp_t *)hashLookup(&gv.jstack_table, Uint32ToPtr(jsp)) );
}

/******************************/
jsp_t * insert_jstack( uint jsp )
{
   jsp_t * jp;

   if ( 0 == gv.jstack_table.bucks )
   {
      hashInit(&gv.jstack_table, 2053, sizeof(jsp_t), "jstack_table", 0, 0);
   }

     jp = hashLookup(&gv.jstack_table, Uint32ToPtr(jsp));

   if ( NULL == jp )
   {
      gc.allocMsg = "insert_jstack";

      jp = (jsp_t *)hashAdd(&gv.jstack_table, Uint32ToPtr(jsp));

      if ( NULL == jp )
      {
         err_exit("table_put in insert_jstack");
      }
   }
   return( jp );
}
