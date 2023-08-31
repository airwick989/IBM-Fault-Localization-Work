#ifndef _NRM_H_
#define _NRM_H_

/* ************************************************************ */
#include "global.h"

/* ************************************************************ */
/* File Related */
extern STRING sumfile;
extern FILE * nrm;    /* nrm file */
extern FILE * dsum;  /* dekko summary output */
extern FILE * hkout; /* dekko summary output */
STRING find_radtbl(char ** anchor, aint cb, int * rcp, int arrsize);

/* ************************************************************ */
/* Single Hook Common */
struct Dekhdr {
   /* bytes(0-9) via fread */
   unsigned char idmaj;
   unsigned char idmin;
   char hklen;
   char cres;      /* reserved char (always 4d) */
   char ts16[6];   /* 6 byte big endian time stamp */

   char buf[256];  /* hook after 1st 10 bytes */
   STRING bptr;    /* buf ptr after pos */

   UNIT ntime;     /* raw hk time */
   UNIT ts;        /* time of 1st hk */
   aint ex;        /* entry/exit flag */
   aint hkx;       /* hook table index, mj/mn as 2byte int */
   aint dbm;       /* double byte minor */
   aint hkkey;     /* 32 bit key for hkdb lookup */
   struct HKDBx * hp; /* ptr to hkdb entry for this hook */

   /* parms parsed from hook */
   aint p[16];     /* int parms */
   char s[2][256]; /* string parms */
   int  picnt;     /* int parms parsed */
   int  pscnt;     /* str parms parsed */

   /* nrm file variables */
   aint next;      /* nrm file offset @ start of hook */
   aint fsize;     /* nrm file fsize */
   int  eof;       /* eof indicator */
};
struct Dekhdr dh1;

/* ************************************************************ */
/* Hook definition & usage stats array */
struct HKDBx {
   /* static */
   aint   mj;           /* major id */
   aint   mn;           /* minor id */
   aint   dbm;          /* double byte minor */
   aint   hkkey;        /* search key( mj(1) mn(1) dbmn(2) */
   int    pid;          /* pid */
   int    mat;          /* unmatched(0) matched(1) */
   STRING hstr;         /* hk ascii name */
   int    pcnt;         /* length of % parms string */
   char   parms[24];    /* format chars */
   char * pstr[24];     /* format desc strs */

   int    first;        /* set on first instance of hook in trace */
   int    cnt;          /* freq in trace */

   STRING hstr2;        /* hknm with both hkkey & hstr */
};

/* ************************************************************ */
/* Function Prototypes */
/* ************************************************************ */
/* nrmhdf.c */
void   proc_hdf(STRING fname);
STRING hkparse1(FILE * nrm, int oopt);
void   hkclean(void);
void   hkshow(void);

/* nrmrd.c */
pPIDTID ptptr(STRING apt);  /* retn ptr to apt PIDTID struct */
void    pt_init(pPIDTID p); /* parx unique pidtid init */
void    showpt(void);       /* show pidtid table */

#endif /* #ifndef _NRM_H_ */
