/*--------------------------------------------------------------------------
Copyright (c) 2020, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--------------------------------------------------------------------------*/

/*============================================================================
                            O p e n M A X   w r a p p e r s
                             O p e n  M A X   C o r e

  This module contains the implementation of the OpenMAX core & component.

*//*========================================================================*/

//////////////////////////////////////////////////////////////////////////////
//                             Include Files
//////////////////////////////////////////////////////////////////////////////


#include <stddef.h>

/*Parse Types: */
#define Parse_Invalid (0)
#define Parse_False  (1 << 0)
#define Parse_True   (1 << 1)
#define Parse_NULL   (1 << 2)
#define Parse_Number (1 << 3)
#define Parse_String (1 << 4)
#define Parse_Array  (1 << 5)
#define Parse_Object (1 << 6)
#define Parse_Raw    (1 << 7) /* raw json */

#define Parse_IsReference 256
#define Parse_StringIsConst 512

/* The JsonItem structure: */
typedef struct JsonItem
{
  struct JsonItem *next;
  struct JsonItem *prev;
  struct JsonItem *child;

  int type;

  char *valuestring;
  int valueint;
  double valuedouble;

  char *string;
} JsonItem;

typedef struct JsonHooks
{
  void *(*malloc_fn)(size_t sz);
  void (*free_fn)(void *ptr);
} JsonHooks;

extern JsonItem *GetRoot(const char *value);
extern JsonItem *GetItem(const JsonItem *object, const char *string);
extern int GetArraySize(const JsonItem *array);
extern JsonItem *GetArrayItem(const JsonItem *array, int item);
