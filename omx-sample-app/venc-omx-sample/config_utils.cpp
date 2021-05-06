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

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>

#include "video_debug.h"
#include "video_types.h"
#include "config_utils.h"

extern uint32_t m_DebugLevelSets;

static const unsigned char *GetValue(JsonItem * const item, const unsigned char * const input, const unsigned char ** const ep);

static const unsigned char *skip(const unsigned char *in) {
  FUNCTION_ENTER();

  while (in && *in && (*in <= 32))
  {
    in++;
  }

  FUNCTION_EXIT();
  return in;
}

static int ConfigStrCaseCmp(const unsigned char *s1, const unsigned char *s2) {
  FUNCTION_ENTER();
  if (!s1)
  {
    FUNCTION_EXIT();
    return (s1 == s2) ? 0 : 1;
  }
  if (!s2)
  {
    FUNCTION_EXIT();
    return 1;
  }
  for(; tolower(*s1) == tolower(*s2); ++s1, ++s2)
  {
    if (*s1 == '\0')
    {
      FUNCTION_EXIT();
      return 0;
    }
  }

  FUNCTION_EXIT();
  return tolower(*s1) - tolower(*s2);
}

static unsigned ParseHex4(const unsigned char * const input_file) {
  FUNCTION_ENTER();
  unsigned int h = 0;
  size_t i = 0;

  for (i = 0; i < 4; i++) {
    if ((input_file[i] >= '0') && (input_file[i] <= '9')) {
      h += (unsigned int) input_file[i] - '0';
    } else if ((input_file[i] >= 'A') && (input_file[i] <= 'F')) {
      h += (unsigned int) 10 + input_file[i] - 'A';
    } else if ((input_file[i] >= 'a') && (input_file[i] <= 'f')) {
      h += (unsigned int) 10 + input_file[i] - 'a';
    } else {
      VLOGE("invalide");
      FUNCTION_EXIT();
      return 0;
    }

    if (i < 3) {
      // shift left to make place for the next nibble
      h = h << 4;
    }
  }

  FUNCTION_EXIT();
  return h;
}

/* create a item. */
static JsonItem *CreateNewItem(void) {
  FUNCTION_ENTER();
  JsonItem* node = (JsonItem*)malloc(sizeof(JsonItem));
  if (node)
  {
    memset(node, '\0', sizeof(JsonItem));
  }

  FUNCTION_EXIT();
  return node;
}

/* Remove a item. */
void RemoveItem(JsonItem *c) {
  FUNCTION_ENTER();
  JsonItem *next = NULL;
  while (c)
  {
    next = c->next;
    if (!(c->type & Parse_IsReference) && c->child)
    {
      RemoveItem(c->child);
    }
    if (!(c->type & Parse_IsReference) && c->valuestring)
    {
      free(c->valuestring);
    }
    if (!(c->type & Parse_StringIsConst) && c->string)
    {
      free(c->string);
    }
    free(c);
    c = next;
  }

  FUNCTION_EXIT();
}

static const unsigned char *ParseString(JsonItem * const item, const unsigned char * const input, const unsigned char ** const p_error) {
  FUNCTION_ENTER();
  const unsigned char *p_input = input + 1;
  const unsigned char *e_input = input + 1;
  unsigned char *p_output = NULL;
  unsigned char *output = NULL;

  if (*input != '\"') {
    *p_error = input;

    VLOGE("input is invalid at first");
    FUNCTION_EXIT();
    return NULL;
  }

  {
    size_t allocation_length = 0;
    size_t skipped_bytes = 0;
    while ((*e_input != '\"') && (*e_input != '\0')) {
      if (e_input[0] == '\\') {
        if (e_input[1] == '\0') {
          VLOGE("prevent buffer overflow when last input character is a backslash");
          FUNCTION_EXIT();
          return NULL;
        }
        skipped_bytes++;
        e_input++;
      }
      e_input++;
    }
    if (*e_input == '\0') {
      VLOGE("string ended unexpectedly");
      FUNCTION_EXIT();
      return NULL;
    }

    allocation_length = (size_t) (e_input - input) - skipped_bytes;
    output = (unsigned char*)malloc(allocation_length + sizeof('\0'));
    if (output == NULL) {
      VLOGE("allocation failure");
      FUNCTION_EXIT();
      return NULL;
    }
  }

  p_output = output;
  // loop through the string literal
  while (p_input < e_input) {
    if (*p_input != '\\') {
      *p_output++ = *p_input++;
    } else {
      VLOGD("p_input:%d",p_input);
      unsigned char sequence_length = 2;
      switch (p_input[1]) {
        case 'b':
          *p_output++ = '\b';
          break;
        case 'f':
          *p_output++ = '\f';
          break;
        case 'n':
          *p_output++ = '\n';
          break;
        case 'r':
          *p_output++ = '\r';
          break;
        case 't':
          *p_output++ = '\t';
          break;
        case '\"':
        case '\\':
        case '/':
          *p_output++ = p_input[1];
          break;

        default:
          *p_error = p_input;
          if (output != NULL) {
            free(output);
          }
          VLOGE("invalide code");
          FUNCTION_EXIT();
          return NULL;
      }
      p_input += sequence_length;
    }
  }

  // zero terminate the output
  *p_output = '\0';

  item->type = Parse_String;
  item->valuestring = (char*)output;

  VLOGD("value:%s", item->valuestring);
  return e_input + 1;
}

static const unsigned char *ParseNumber(JsonItem * const item, const unsigned char * const input) {
  FUNCTION_ENTER();
  double n = 0;
  unsigned char *after_end = NULL;

  if (input == NULL)
  {
    VLOGE("input is null");
    FUNCTION_EXIT();
    return NULL;
  }

  n = strtod((const char*)input, (char**)&after_end);
  if (input == after_end)
  {
    VLOGE("parse_error, input = after_end");
    FUNCTION_EXIT();
    return NULL;
  }

  item->valuedouble = n;

  if (n >= INT_MAX)
  {
    item->valueint = INT_MAX;
  }
  else if (n <= INT_MIN)
  {
    item->valueint = INT_MIN;
  }
  else
  {
    item->valueint = (int)n;
  }

  item->type = Parse_Number;

  FUNCTION_EXIT();
  return after_end;
}

static const unsigned char *ParseArray(JsonItem * const item, const unsigned char *input, const unsigned char ** const p_error) {
  FUNCTION_ENTER();
  JsonItem *list_head = NULL; // head of the linked list
  JsonItem *item_current = NULL;

  if (*input != '[') {
    *p_error = input;
    VLOGE("parse array failed at first!");
    FUNCTION_EXIT();
    return NULL;
  }

  input = skip(input + 1); // skip whitespace
  if (*input == ']') {
    item->type = Parse_Array;
    item->child = list_head;

    VLOGD("parse empty array finish");
    FUNCTION_EXIT();
    return input + 1;
  }

  // step back to character in front of the first element
  input--;
  // loop through the comma separated array elements
  do {
    JsonItem *new_item = CreateNewItem();
    if (new_item == NULL) {
      if (list_head != NULL) {
        RemoveItem(list_head);
      }

      VLOGE("create new item failed.");
      FUNCTION_EXIT();
      return NULL;
    }

    if (list_head == NULL) {
      VLOGD("set list head");
      item_current = list_head = new_item;
    } else {
      item_current->next = new_item;
      new_item->prev = item_current;
      item_current = new_item;
    }

    // skip whitespace
    input = skip(input + 1);
    input = GetValue(item_current, input, p_error);
    input = skip(input);
    if (input == NULL) {
      if (list_head != NULL) {
        RemoveItem(list_head);
      }

      FUNCTION_EXIT();
      return NULL;
      VLOGE("failed to parse value");
    }
  }
  while (*input == ',');

  if (*input != ']') {
    *p_error = input;
    if (list_head != NULL){
      RemoveItem(list_head);
    }
    VLOGD("parse arrsy failed, without ending of array");
    FUNCTION_EXIT();
    return NULL;
  }

  item->type = Parse_Array;
  item->child = list_head;

  FUNCTION_EXIT();
  return input + 1;
}

static const unsigned char *ParseObject(JsonItem * const item, const unsigned char *input, const unsigned char ** const p_error) {
  FUNCTION_ENTER();
  JsonItem *list_head = NULL;
  JsonItem *item_current = NULL;

  if (*input != '{') {
    *p_error = input;
    VLOGE("no start for object");
    FUNCTION_EXIT();
    return NULL;
  }

  input = skip(input + 1);
  if (*input == '}') {
    item->type = Parse_Object;
    item->child = list_head;

    VLOGD("empty object");
    FUNCTION_EXIT();
    return input + 1;
  }

  input--;
  // loop through the comma separated array elements
  do {
    JsonItem *new_item = CreateNewItem();
    if (new_item == NULL) {
      if (list_head != NULL) {
        RemoveItem(list_head);
      }
      VLOGE("create new item  failure");

      FUNCTION_EXIT();
      return NULL;
    }

    if (list_head == NULL) {
      item_current = list_head = new_item;
    } else {
      item_current->next = new_item;
      new_item->prev = item_current;
      item_current = new_item;
    }

    input = skip(input + 1);
    input = ParseString(item_current, input, p_error);
    input = skip(input);
    if (input == NULL) {
      if (list_head != NULL) {
        RemoveItem(list_head);
      }
      VLOGE("fail to parse name");

      FUNCTION_EXIT();
      return NULL;
    }

    item_current->string = item_current->valuestring;
    item_current->valuestring = NULL;

    VLOGD("get key name: %s", item_current->string);
    if (*input != ':')
    {
      *p_error = input;
      if (list_head != NULL) {
        RemoveItem(list_head);
      }
      VLOGE("invalid object");

      FUNCTION_EXIT();
      return NULL;
    }

    input = skip(input + 1);
    input = GetValue(item_current, input, p_error);
    input = skip(input);
    if (input == NULL)
    {
      if (list_head != NULL) {
        RemoveItem(list_head);
      }
      VLOGE("failed to parse value");

      FUNCTION_EXIT();
      return NULL;
    }
  }
  while (*input == ',');

  if (*input != '}')
  {
    *p_error = input;
    if (list_head != NULL) {
      RemoveItem(list_head);
    }
    VLOGE("expected end of object");

    FUNCTION_EXIT();
    return NULL;
  }

  item->type = Parse_Object;
  item->child = list_head;

  return input + 1;
}

static const unsigned  char *GetValue(JsonItem * const item, const unsigned char * const input, const unsigned char ** const p_error) {
  FUNCTION_ENTER();
  if (input == NULL) {
    VLOGE("no input");
    FUNCTION_EXIT();
    return NULL;
  }

  if (!strncmp((const char*)input, "null", 4)) {
    item->type = Parse_NULL;
    VLOGD("null");
    return input + 4;
  }
  if (!strncmp((const char*)input, "false", 5)) {
    item->type = Parse_False;
    VLOGD("false");
    return input + 5;
  }
  if (!strncmp((const char*)input, "true", 4)) {
    item->type = Parse_True;
    item->valueint = 1;
    VLOGD("true");
    return input + 4;
  }
  if (*input == '\"') {
    VLOGD("get "" ---- start ParseString");
    return ParseString(item, input, p_error);
  }
  if ((*input == '-') || ((*input >= '0') && (*input <= '9'))) {
    VLOGD("get -/0-9 start ParseNumber");
    return ParseNumber(item, input);
  }
  if (*input == '[') {
    VLOGD("get [ ---- start ParseArray");
    return ParseArray(item, input, p_error);
  }
  if (*input == '{') {
    VLOGD("get { ---- start ParseObject");
    return ParseObject(item, input, p_error);
  }

  *p_error = input;

  FUNCTION_EXIT();
  return NULL;
}

JsonItem *GetRoot(const char *value) {
  FUNCTION_ENTER();
  const unsigned char *end = NULL;
  const unsigned char *ep = NULL;
  JsonItem *root = CreateNewItem();
  if (!root) {
    VLOGE("memory fail");
    return NULL;
  }

  end = GetValue(root, skip((const unsigned char*)value), &ep);
  if (!end) {
    RemoveItem(root);
    return NULL;
  }

  FUNCTION_EXIT();
  return root;
}

JsonItem *GetItem(const JsonItem *object, const char *string) {
  FUNCTION_ENTER();
  JsonItem *c = object ? object->child : NULL;
  while (c && ConfigStrCaseCmp((unsigned char*)c->string, (const unsigned char*)string)) {
    c = c->next;
  }
  FUNCTION_EXIT();
  return c;
}

int GetArraySize(const JsonItem *array) {
  FUNCTION_ENTER();
  JsonItem *c = array->child;
  size_t i = 0;
  while(c)
  {
    i++;
    c = c->next;
  }

  FUNCTION_EXIT();
  return (int)i;
}

JsonItem *GetArrayItem(const JsonItem *array, int item) {
  FUNCTION_ENTER();
  JsonItem *c = array ? array->child : NULL;
  while (c && item > 0)
  {
    item--;
    c = c->next;
  }

  FUNCTION_EXIT();
  return c;
}
