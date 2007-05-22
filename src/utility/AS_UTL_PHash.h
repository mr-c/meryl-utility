
/**************************************************************************
 * This file is part of Celera Assembler, a software program that 
 * assembles whole-genome shotgun reads into contigs and scaffolds.
 * Copyright (C) 1999-2004, Applera Corporation. All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received (LICENSE.txt) a copy of the GNU General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *************************************************************************/

// $Id: AS_UTL_PHash.h,v 1.7 2007-05-22 20:42:25 brianwalenz Exp $

#ifndef AS_UTL_PHASH_H
#define AS_UTL_PHASH_H

//  Saul A. Kravitz
//  January 21, 1999
//  Version 2.0
//
//  This document describes the architecture and API for the assembler's
//  mechanism of associating UIDs from DMS with internally generated IIDs.
//  This mechanism uses a persistent hash table in a memory mapped file.
//
//  Persistent hash tables can either be created using a memory mapped
//  file, or, transient tables can be created in memory.
//
//  This module is not intended to be general purpose (for that see
//  AS_UTL_Hash), but rather specifically focussed on the needs of the
//  assembler.
//
//  This module employs the hash function in AS_UTL_HashCommon.h and
//  AS_UTL_HashCommon.c.  In contrast to AS_UTL_Hash, this is
//  hardwired for the UID->IID mapping problem.
//
//  The hashing function used is from DDJ.  See AS_HashCommon.[hc].
//  
//  ** Requirements
//
//  Maintain a persistent association of 64-bit keys with 64-bit values.
//
//  Provide compatible implementations that are memory-based and
//  memory-mapped file based.
//
//  Assign dense IIDs for each type of key, starting from 1.
//
//  Support multiple, orthogonal, namespaces within a single hash table.
//
//  ** Design 
//
//  This is a fairly standard open hash table.  The hashing algorithm is
//  taken from an article in DDJ that compared the performance of several
//  hashing algorithms.  It employs a power-of-2 sized hash tables.  Keys
//  are mapped to buckets by computing their hash value, and then ANDing
//  the hash value with an appropriate mask.
//
//  ** Memory Usage
//
//  All memory for a persistent hash table is either allocated as a single
//  block of dynamically allocated memory, or mapped, in its entirety, to
//  a memory-mapped file.  Inserting a (key,value) pair into the hash
//  table copies the value.  When the hash table has filled up, it is
//  reallocated.
//
//  ** Limitations
//
//  Not written to support multiple threads, although I see no significant
//  obstacles in the way of such an extension.
//
//  ** Component Architecture and Unit Dependencies
//

#include <limits.h>

/* If the following is defined, the hash table assigns the IIDs */
#define COUNTS 1
#include "AS_UTL_HashCommon.h"

/* Lookup Failure Classes */

#define HASH_FAILURE_FOUND_BUT_DELETED (HASH_FAILURE - 1)
#define HASH_FAILURE_FOUND_BUT_WRONG_TYPE (HASH_FAILURE - 2)
#define HASH_FAILURE_OUTSTANDING_REFS (HASH_FAILURE - 3)
#define HASH_FAILURE_ALREADY_EXISTS (HASH_FAILURE - 4)


//  The following defines are for the type of data stored in the
//  PHashValue_AS type field
//
//  If you change these, update String_AS_IID[] in PHash.c too.
//
#define AS_IID_MIN 0

#define AS_IID_FRG 1
#define AS_IID_DST 2
#define AS_IID_LOC 3   // locale == bac ID
#define AS_IID_SEQ 4   // sequence ID
#define AS_IID_BTG 5   // bactig
#define AS_IID_PLA 6
#define AS_IID_LIB 7
#define AS_IID_BAT 8   // Batch
#define AS_IID_DON 9
#define AS_IID_MAX 9


/* PHashValue_AS is a 64-bit value used to store a 
 * 32-bit value and 
 * up to 32 bits of flag/etc info in the hash table */
/* This structure could probably be compressed somewhat */
/* Saved that task for later */

#define LOG_MAX_REFCOUNT (27)
#define PHASH_REFCOUNT_MAX ((1<<LOG_MAX_REFCOUNT) -1)
#define NUM_TYPES (16)
#define LOG_NUM_TYPES (4)

typedef struct {   
  CDS_IID_t IID;     /* Internal ID */
  unsigned int deleted:1; /* This value is dead */
  unsigned int type:LOG_NUM_TYPES;    /* AS_IID* above ... could share space with the IID? */
  unsigned int refCount:LOG_MAX_REFCOUNT;
} PHashValue_AS;


/* Values for the PHashNode_AS nameSpace field */
#define AS_INVALID_NAMESPACE 0
#define AS_UID_NAMESPACE 1

/* PHashNode_AS
 *   This is the basic elements stored in the hash table.
 */
typedef struct PHashNode_tag{
  CDS_UID_t key;         /* Hash is keyed by UID */
  PHashValue_AS value;   /* See above */
  int32 next;         /* Index of next element, relative to allocated, in chain, -1 if empty */
  char nameSpace;    /* Independent Name Spaces */
  char spare1;        /* unused, used to round things out to 3 64-bit values */
  int16 spare2;        /* unused, used to round things out to 3 64-bit values */
}PHashNode_AS;

/* PHashTable_AS
 *
 * This is the header for the hashtable.
 * NOTE: Pointer values are stored here, but each time a Hash File is opened/created
 * they are adjusted.  They could be eliminated from the persistent representation of
 * the hash table, but why?
 */

typedef struct{
  int32 numBuckets;
  int32 freeList;           /* Index of head of free list, relative to allocated */

  int32 numNodes;           /* Total number of nodes in use */
  int32 numNodesAllocated;  /* Total number of nodes allocated */

  CDS_UID_t lastKey;           /* The value of the key of the last element inserted */

  int32 lastNodeAllocated;  /* Highest index of an allocated node -- used to conserve on output */
  int32 collisions;         /* For statistics */

  uint32 hashmask;          /* Mask for hash value, depends directly on numBuckets */
  int32 dummy1For8byteWordPadding;

#if COUNTS
  CDS_IID_t counts[1<<LOG_NUM_TYPES]; /* Used to assign IIDs for each type */
#endif
  /**** The following are meaningful only when the hash file is open ****/

  PHashNode_AS *allocated;  /* All node indices are relative to allocated */
#if LONG_MAX == 2147483647
  void *dummyPadPtr1;
#endif

  int32 *buckets;           /* Array of indices of PHashNodes, relative to allocated */
#if LONG_MAX == 2147483647
  void *dummyPadPtr2;
#endif

  char *fileName;           /* Name of file in which this hashtable resides */
#if LONG_MAX == 2147483647
  void *dummyPadPtr3;
#endif

  FILE *fp;                 /* File * of hash file */
#if LONG_MAX == 2147483647
  void *dummyPadPtr4;
#endif

  int32 isDirty;
  int32 isReadWrite;  // if true, hashtable is in memory, otherwise, it is mmapped
} PHashTable_AS;


typedef struct{
  int32 currentNodeIndex;
  PHashTable_AS *table;
}PHashTable_Iterator_AS;


/***********************************************************************************
 *
 * A persistent hash table is created with the CreatePHashTable_AS
 * function.  If the 2nd argument is NULL, the hashtable is created in
 * memory and is not persistent.  This can be used to create interim
 * hashtables that can later be concatenated with a persistent hashtable
 * using ConcatPHashTableAS (bpw 20070522; doesn't seem to exist).
 *
 * Inputs:
 *     numItemsToHash   Used to compute the initial size of the hashtable
 *     pathToHashTable  Path to open for hashtable.  If NULL, hashtable is in memory.
 *
 * Return Value:
 *     Pointer to newly allocated AS_PHashTable_AS. (NULL if failure)
 ***********************************************************************************/
PHashTable_AS *CreatePHashTable_AS(int numItemsToHash, char *pathToHashTable);


/***********************************************************************************
 *
 * An existing persistent hash table can be opened with the
 * OpenPHashTable_AS function. This opens the file, and maps it into the
 * memory of the executing process.
 *
 * Inputs:
 *     pathToHashTable  Path to open for hashtable.  
 *
 * Return Value:
 *     Pointer to newly opened AS_PHashTable_AS (NULL if failure)
 ***********************************************************************************/
PHashTable_AS *OpenPHashTable_AS(char *pathToHashTable);

/***********************************************************************************
 * Function: OpenReadOnlyPHashTable_AS
 * Description:
 *     Opens an existing PHashTable_AS
 *
 * Inputs:
 *     pathToHashTable  Path to open for hashtable.  
 *
 * Return Value:
 *     Pointer to newly opened AS_PHashTable_AS (NULL if failure)
 ***********************************************************************************/
PHashTable_AS *OpenReadOnlyPHashTable_AS(char *pathToHashTable);



/***********************************************************************************
 * Function: ClosePHashTable_AS
 * Description:
 *     Closes an open PHashTable_AS. If in-memory, dispose of memory.  If
 * file-based, close file.
 *
 * Inputs:
 *     table        Pointer to PHashTable_AS
 *
 * Return Value:
 *     None.
 ***********************************************************************************/
void ClosePHashTable_AS(PHashTable_AS *table);

/***********************************************************************************
 * Function: ResetPHashTable_AS
 * Description:
 *     Recycles an open PHashTable_AS, disposing of all data stored therein.
 *
 * Inputs:
 *     table        Pointer to PHashTable_AS
 *     counts       Pointer to an array of ints of size NUM_TYPES that initialize
 *                  the IIDs assigned by the hash table to inserted elements.
 * Return Value:
 *     None.
 ***********************************************************************************/

#if COUNTS
void ResetPHashTable_AS(PHashTable_AS *table, CDS_IID_t *counts);
#else
void ResetPHashTable_AS(PHashTable_AS *table);
#endif

#if COUNTS
/***********************************************************************************
 * Function: GetCountsPHashTable_AS
 * Description:
 *     Retrieves the counts of the number of keys of each type inserted.
 *
 * Inputs:
 *     table        Pointer to PHashTable_AS
 * Input/Output:
 *     counts       Pointer to an array of ints of size NUM_TYPES 
 * Return Value:
 *     None.
 ***********************************************************************************/
 void GetCountsPHashTable_AS(PHashTable_AS *table, CDS_IID_t *counts);

/***********************************************************************************
 * Function: AllocateCountPHashTable_AS
 * Description:
 *     Increments the count of the  number of keys of a particular type inserted.
 *     Used to reserve an IID for use by the gatekeeper.
 * Inputs:
 *     table        Pointer to PHashTable_AS
 *     type         A valid type
 * Input/Output:
 * Return Value:
 *     The allocated index;
 ***********************************************************************************/
CDS_IID_t AllocateCountPHashTable_AS(PHashTable_AS *table, unsigned int type);
#endif


/***********************************************************************************
 * Function: InsertInPHashTable_AS
 * Description:
 *     Inserts a value with a key into a hashtable, and assigns it an IID.
 *     The IID assigned is a function of the type of the value.
 *
 * Inputs:
 *     nameSpace     char                 Keys in different namespaces are orthogonal
 *     key           64-bit key
 *     value         Pointer to PHashValue_AS to be inserted.  The value is COPIED
 *                   into the hash table.
 *     useRefCount   If true, refCount is set to value.refCount, otherwise 1
 *     assignIID     If true, assign an IID, otherwise use value->IID
 * Input/Outputs:
 *     table         ** to a PHashTable_AS.  The table may be changed if the insertion
 *                   causes the table to be reallocated.
 *     value         Pointer to PHashValue_AS to be inserted.  The value is COPIED
 *                   into the hash table.   The IID field of the value is set to
 *                   reflect the IID assigned.
 *
 * Return Value:
 *     HASH_SUCCESS if successful
 *     HASH_FAILURE if failure
 ***********************************************************************************/
int InsertInPHashTable_AS(PHashTable_AS **table, 
                         char nameSpace, 
                         CDS_UID_t key, 
                         PHashValue_AS *value,
                         int useRefCount,
                         int assignIID);


/***********************************************************************************
 * Function: AddRefPHashTable_AS
 * Description:
 *     Increments the reference cound for a key
 *
 * Inputs:
 *     nameSpace     char                 Keys in different namespaces are orthogonal
 *     key           64-bit key
 *
 * Input/Outputs:
 *     table         * to a PHashTable_AS.  
 *
 * Return Value:
 *     HASH_SUCCESS if successful
 *     HASH_FAILURE if the key is not found
 *     HASH_FAILURE_FOUND_BUT_DELETED If the key is found, but marked deleted
 ***********************************************************************************/
int AddRefPHashTable_AS(PHashTable_AS *table, 
                         char nameSpace, 
                         CDS_UID_t key);


/***********************************************************************************
 * Function: UnRefPHashTable_AS
 * Description:
 *     Decrements the reference cound for a key
 *
 * Inputs:
 *     nameSpace     char                 Keys in different namespaces are orthogonal
 *     key           64-bit key
 *
 * Input/Outputs:
 *     table         * to a PHashTable_AS.  
 *
 * Return Value:
 *     HASH_SUCCESS if successful
 *     HASH_FAILURE if the key is not found
 *     HASH_FAILURE_FOUND_BUT_DELETED If the key is found, but marked deleted
 ***********************************************************************************/
int UnRefPHashTable_AS(PHashTable_AS *table, 
                         char nameSpace, 
                         CDS_UID_t key);


/***********************************************************************************
 * Function: DeleteFromPHashTable_AS
 * Description:
 *     Deletes a value with a key from the hashtable.
 *
 * Inputs:
 *     nameSpace     char                 Keys in different namespaces are orthogonal
 *     key           64-bit key
 *     table         * to a PHashTable_AS.  
 *
 * Return Value:
 *     HASH_SUCCESS if successful
 *     HASH_FAILURE if failure
 ***********************************************************************************/
int DeleteFromPHashTable_AS(PHashTable_AS *table, 
                            char nameSpace, 
                            CDS_UID_t key);


/***********************************************************************************
 * Function: MarksAsDeletedPHashTable_AS
 * Description:
 *     Marks a value with a key as deleted.  Value remains in database.
 *
 * Inputs:
 *     nameSpace     char                 Keys in different namespaces are orthogonal
 *     key           64-bit key
 *     table         * to a PHashTable_AS.  
 *
 * Return Value:
 *     HASH_SUCCESS if successful
 *     HASH_FAILURE if failure
 ***********************************************************************************/
int MarkAsDeletedPHashTable_AS(PHashTable_AS *table, 
                            char nameSpace, 
                               CDS_UID_t key);

/***********************************************************************************
 * Function: LookupInPHashTable_AS
 * Description:
 *     Looks up a  key in table
 *
 * Inputs:
 *     table         * to a PHashTable_AS.  
 *     nameSpace     char                 Keys in different namespaces are orthogonal
 *     key           64-bit key
 * Input/Outputs
 *     value         * to a PHashValue_AS.  If found, value is COPIED into this buffer.
 *
 * Return Value:
 *     HASH_SUCCESS if successful (value is valid)
 *     HASH_FAILURE if failure
 ***********************************************************************************/
int LookupInPHashTable_AS(PHashTable_AS *table, char nameSpace, CDS_UID_t key, PHashValue_AS *value);

/***********************************************************************************
 * Function: LookupTypeInPHashTable_AS
 * Description:
 *     Looks up a  key in table, constrained to a given type of data, and reporting
 *     errors.
 *
 * Inputs:
 *     table         * to a PHashTable_AS.  
 *     nameSpace     char                 Keys in different namespaces are orthogonal
 *     key           64-bit key
 *     uint32        type (AS_IID_FRG, AS_IID_DST, etc)
 *     int           reportFailure
 *     FILE*         msgFile
 * Input/Outputs
 *     value         * to a PHashValue_AS.  If found, value is COPIED into this buffer.
 *
 * Return Value:
 *     HASH_SUCCESS if successful (value is valid)
 *     HASH_FAILURE if lookup failure
 *     HASH_FAILURE_FOUND_BUT_DELETED Found, but deleted flag set
 *     HASH_FAILURE_FOUND_BUT_WRONG_TYPE Found, but type is wrong
 *     
 ***********************************************************************************/
int LookupTypeInPHashTable_AS(PHashTable_AS *table, 
                              char nameSpace,
                              CDS_UID_t key, 
                              int type, 
                              int reportFailure, 
                              FILE *msgFile,
                              PHashValue_AS *value);





/***********************************************************************************
 * Function: MakeSpacePHashTable_AS
 * Description:
 *     Reallocates target table, if adding numNodes entries will cause a realloc.
 *     This can be done preemptively, before adding or concating two tables.
 *
 * The persistent hash table reallocates itself, as needed.  However, if
 * the user knows that a bunch of entries are about to be added to the
 * hashtable, it is best to preemptively call MakeSpaceForPHashTable_AS.
 * This can be considered strictly an optional performance enhancer.
 *
 * Input/Outputs
 *     target         ** to a PHashTable_AS. (May be reallocated as a result of operation )
 *
 * Return Value:
 *     HASH_SUCCESS if successful (value is valid)
 *     HASH_FAILURE if failure
 ***********************************************************************************/
int MakeSpacePHashTable_AS(PHashTable_AS **target, int numNodes);



/***********************************************************************************
 * Function: InitializePHashTable_Iterator_AS
 * Description:
 *     Initializes a PHashTable_Iterator_AS
 *
 * Often it is useful to know all of the key, value pairs in a hashtable.
 * This iterator provides such easy access.  First the iterator is
 * initialized, and subsequent calls to NextPHashTable_Iterator_AS return
 * 1 until all key, value pairs have been enumerated.
 *
 * In the absence of frees, values are returned in the order inserted
 * (bpw 20070522: not verified, stated in very old documentation!)
 *
 * Inputs:
 *     table         * to a PHashTable_AS.  
 * Input/Outputs
 *     iterator      * to a PHashTable_Iterator_AS allocated by client
 *
 * Return Value:
 *     HASH_SUCCESS if successful (value is valid)
 *     HASH_FAILURE if failure
 ***********************************************************************************/
int InitializePHashTable_Iterator_AS(PHashTable_AS *table, PHashTable_Iterator_AS *iterator);

/***********************************************************************************
 * Function: NextPHashTable_Iterator_AS
 * Description:
 *     Retrieves next element of PHashTable using iterator
 *
 * Inputs:
 *     iterator      * to a PHashTable_Iterator_AS
 * Input/Outputs
 *     value         * to a PHashValue_AS  (buffer is updated)
 *     key           * to a uint64         (buffer is updated)
 *     nameSpace     * to a char          (buffer is updated)
 *
 * Return Value:
 *     HASH_SUCCESS if successful (value is valid)
 *     HASH_FAILURE if failure
 ***********************************************************************************/
int NextPHashTable_Iterator_AS(PHashTable_Iterator_AS *iterator, 
                               char *nameSpace,
                               CDS_UID_t *key, 
                               PHashValue_AS *value);
#endif



