/*
 *   Copyright (c) IBM Corporation 1997.
 *   Property of IBM Corporation.
 *   All Rights Reserved.
 *   Origin 30 (IBM)
 */

#ifndef _JHASH_H
   #define _JHASH_H

/**************************/
/* Method Structure */
typedef struct _method_t method_t;

struct _method_t
{
   method_t * nextInBucket;
   void     * mb;
   char     * mnm;                      /* method name */
};

/* js method table */
method_t * insert_method(void *);
method_t * lookup_method(void *);

/**************************/
/* java stack pointer */
typedef struct _jsp_t    jsp_t;

// hash : javaStkPtr => arcfTreeNode
struct _jsp_t
{
   jsp_t * nextInBucket;
   uint    jsp;                          // java tree Node addr
   struct  TREE * tp;                    // arcf tree Node addr
};

jsp_t * insert_jstack(uint jsp);
jsp_t * lookup_jstack(uint jsp);

#endif
