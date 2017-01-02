/*
 * refit/scan/common.c
 *
 * Copyright (c) 2006-2010 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//#include <Library/Platform/Platform.h>
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>

#include <Library/Common/CommonLib.h>

#ifndef DEBUG_ALL
#define DEBUG_COMMON_MENU 1
#else
#define DEBUG_COMMON_MENU DEBUG_ALL
#endif

#if DEBUG_COMMON_MENU == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_COMMON_MENU, __VA_ARGS__)
#endif

CONST CHAR16 *OsxPathLCaches[] = {
   L"\\System\\Library\\Caches\\com.apple.kext.caches\\Startup\\kernelcache",
   L"\\System\\Library\\Caches\\com.apple.kext.caches\\Startup\\Extensions.mkext",
   L"\\System\\Library\\Extensions.mkext",
   L"\\com.apple.recovery.boot\\kernelcache",
   L"\\com.apple.recovery.boot\\Extensions.mkext",
   L"\\.IABootFiles\\kernelcache"
};

CONST   UINTN OsxPathLCachesCount = ARRAY_SIZE(OsxPathLCaches);
CHAR8   *OsVerUndetected = "10.10.10";  //longer string

//extern BOOLEAN CopyKernelAndKextPatches(IN OUT KERNEL_AND_KEXT_PATCHES *Dst, IN KERNEL_AND_KEXT_PATCHES *Src);

//--> Base64

typedef enum {
  step_a, step_b, step_c, step_d
} base64_decodestep;

typedef struct {
  base64_decodestep   step;
  char                plainchar;
} base64_decodestate;

int base64_decode_value(char value_in) {
  static const signed char decoding[] = {
    62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,-1,0,1,2,3,4,5,6,
    7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,
    29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51
  };

  int value_in_i = (unsigned char) value_in;
  value_in_i -= 43;
  if (value_in_i < 0 || (unsigned int) value_in_i >= sizeof decoding) return -1;
  return decoding[value_in_i];
}

void
base64_init_decodestate (
  base64_decodestate    *state_in
) {
  state_in->step = step_a;
  state_in->plainchar = 0;
}

int
base64_decode_block (
  const char                *code_in,
  const int                 length_in,
        char                *plaintext_out,
        base64_decodestate  *state_in
) {
  const char  *codechar = code_in;
  char        *plainchar = plaintext_out;
  int         fragment;

  *plainchar = state_in->plainchar;

  switch (state_in->step) {
    while (1) {
      case step_a:
        do {
          if (codechar == code_in+length_in) {
            state_in->step = step_a;
            state_in->plainchar = *plainchar;
            return (int)(plainchar - plaintext_out);
          }
          fragment = base64_decode_value(*codechar++);
        } while (fragment < 0);

        *plainchar    = (char) ((fragment & 0x03f) << 2);

      case step_b:
        do {
          if (codechar == code_in+length_in) {
            state_in->step = step_b;
            state_in->plainchar = *plainchar;
            return (int)(plainchar - plaintext_out);
          }

          fragment = base64_decode_value(*codechar++);
        } while (fragment < 0);

        *plainchar++ |= (char) ((fragment & 0x030) >> 4);
        *plainchar    = (char) ((fragment & 0x00f) << 4);

      case step_c:
        do {
          if (codechar == code_in+length_in) {
            state_in->step = step_c;
            state_in->plainchar = *plainchar;
            return (int)(plainchar - plaintext_out);
          }
          fragment = base64_decode_value(*codechar++);
        } while (fragment < 0);

        *plainchar++ |= (char) ((fragment & 0x03c) >> 2);
        *plainchar    = (char) ((fragment & 0x003) << 6);

      case step_d:
        do {
          if (codechar == code_in+length_in)
          {
            state_in->step = step_d;
            state_in->plainchar = *plainchar;
            return (int)(plainchar - plaintext_out);
          }
          fragment = base64_decode_value(*codechar++);
        } while (fragment < 0);

        *plainchar++   |= (char) ((fragment & 0x03f));
    }
  }

  /* control should not reach here */
  return (int)(plainchar - plaintext_out);
}

/** UEFI interface to base54 decode.
 * Decodes EncodedData into a new allocated buffer and returns it. Caller is responsible to FreePool() it.
 * If DecodedSize != NULL, then size od decoded data is put there.
 */
UINT8
*Base64Decode (
  IN  CHAR8     *EncodedData,
  OUT UINTN     *DecodedSize
) {
  UINTN                 EncodedSize, DecodedSizeInternal;
  UINT8                 *DecodedData;
  base64_decodestate    state_in;

  if (EncodedData == NULL) {
    return NULL;
  }

  EncodedSize = AsciiStrLen(EncodedData);

  if (EncodedSize == 0) {
    return NULL;
  }

  // to simplify, we'll allocate the same size, although smaller size is needed
  DecodedData = AllocateZeroPool(EncodedSize);

  base64_init_decodestate(&state_in);
  DecodedSizeInternal = base64_decode_block(EncodedData, (const int)EncodedSize, (char*) DecodedData, &state_in);

  if (DecodedSize != NULL) {
    *DecodedSize = DecodedSizeInternal;
  }

  return DecodedData;
}

//<-- Base64

/**
  Duplicate a string.

  @param Src             The source.

  @return A new string which is duplicated copy of the source.
  @retval NULL If there is not enough memory.

**/
CHAR16
*EfiStrDuplicate (
  IN CHAR16   *Src
) {
  CHAR16  *Dest;
  UINTN   Size;

  Size  = StrSize (Src); //at least 2bytes
  Dest  = AllocatePool (Size);
  //ASSERT (Dest != NULL);
  if (Dest != NULL) {
    CopyMem (Dest, Src, Size);
  }

  return Dest;
}

INTN
StrniCmp (
  IN CHAR16   *Str1,
  IN CHAR16   *Str2,
  IN UINTN    Count
) {
  CHAR16  Ch1, Ch2;

  if (Count == 0) {
    return 0;
  }

  if (Str1 == NULL) {
    if (Str2 == NULL) {
      return 0;
    } else {
      return -1;
    }
  } else  if (Str2 == NULL) {
    return 1;
  }

  do {
    Ch1 = TO_LOWER(*Str1);
    Ch2 = TO_LOWER(*Str2);

    Str1++;
    Str2++;

    if (Ch1 != Ch2) {
      return (Ch1 - Ch2);
    }

    if (Ch1 == 0) {
      return 0;
    }
  } while (--Count > 0);

  return 0;
}

CHAR16
*StriStr (
  IN CHAR16   *Str,
  IN CHAR16   *SearchFor
) {
  CHAR16  *End;
  UINTN   Length, SearchLength;

  if ((Str == NULL) || (SearchFor == NULL)) {
    return NULL;
  }

  Length = StrLen(Str);
  if (Length == 0) {
    return NULL;
  }

  SearchLength = StrLen(SearchFor);

  if (SearchLength > Length) {
    return NULL;
  }

  End = Str + (Length - SearchLength) + 1;

  while (Str < End) {
    if (StrniCmp(Str, SearchFor, SearchLength) == 0) {
      return Str;
    }
    ++Str;
  }

  return NULL;
}

CHAR16
*StrToLower (
  IN CHAR16   *Str
) {
  CHAR16    *Tmp = EfiStrDuplicate(Str);
  INTN      i;

  for(i = 0; Tmp[i]; i++) {
    Tmp[i] = TO_LOWER(Tmp[i]);
  }

  return Tmp;
}

CHAR16
*StrToUpper (
  IN CHAR16   *Str
) {
  CHAR16    *Tmp = EfiStrDuplicate(Str);
  INTN      i;

  for(i = 0; Tmp[i]; i++) {
    Tmp[i] = TO_UPPER(Tmp[i]);
  }

  return Tmp;
}

CHAR16
*StrToTitle (
  IN CHAR16   *Str
) {
  CHAR16    *Tmp = EfiStrDuplicate(Str);
  INTN      i;
  BOOLEAN   First = TRUE;

  for(i = 0; Tmp[i]; i++) {
    if (First) {
      Tmp[i] = TO_UPPER(Tmp[i]);
      First = FALSE;
    } else {
      if (Tmp[i] != 0x20) {
        Tmp[i] = TO_LOWER(Tmp[i]);
      } else {
        First = TRUE;
      }
    }
  }

  return Tmp;
}

//Compare strings case insensitive
INTN
StriCmp (
  IN  CONST CHAR16   *FirstS,
  IN  CONST CHAR16   *SecondS
) {
  if (
    (FirstS == NULL) || (SecondS == NULL) ||
    (StrLen(FirstS) != StrLen(SecondS))
  ) {
    return 1;
  }

  while (*FirstS != L'\0') {
    if ( (((*FirstS >= 'a') && (*FirstS <= 'z')) ? (*FirstS - ('a' - 'A')) : *FirstS ) !=
      (((*SecondS >= 'a') && (*SecondS <= 'z')) ? (*SecondS - ('a' - 'A')) : *SecondS ) ) break;
    FirstS++;
    SecondS++;
  }

  return *FirstS - *SecondS;
}

/**
  Adjusts the size of a previously allocated buffer.


  @param OldPool         - A pointer to the buffer whose size is being adjusted.
  @param OldSize         - The size of the current buffer.
  @param NewSize         - The size of the new buffer.

  @return   The newly allocated buffer.
  @retval   NULL  Allocation failed.

**/
VOID
*EfiReallocatePool (
  IN VOID     *OldPool,
  IN UINTN    OldSize,
  IN UINTN    NewSize
) {
  VOID  *NewPool = NULL;

  if (NewSize != 0) {
    NewPool = AllocateZeroPool (NewSize);
  }

  if (OldPool != NULL) {
    if (NewPool != NULL) {
      CopyMem (NewPool, OldPool, OldSize < NewSize ? OldSize : NewSize);
    }

    FreePool (OldPool);
  }

  return NewPool;
}

UINT64
AsciiStrVersionToUint64 (
  const CHAR8   *Version,
        UINT8   MaxDigitByPart,
        UINT8   MaxParts
) {
  UINT64    result = 0;
  UINT16    part_value = 0, part_mult  = 1, max_part_value;

  if (!Version) {
    Version = OsVerUndetected; //pointer to non-NULL string
  }

  while (MaxDigitByPart--) {
    part_mult = part_mult * 10;
  }

  max_part_value = part_mult - 1;

  while (*Version && (MaxParts > 0)) {  //Slice - NULL pointer dereferencing
    if (*Version >= '0' && *Version <= '9') {
      part_value = part_value * 10 + (*Version - '0');

      if (part_value > max_part_value) {
        part_value = max_part_value;
      }
    }
    else if (*Version == '.') {
      result = MultU64x64(result, part_mult) + part_value;
      part_value = 0;
      MaxParts--;
    }

    Version++;
  }

  while (MaxParts--) {
    result = MultU64x64(result, part_mult) + part_value;
    part_value = 0; // part_value is only used at first pass
  }

  return result;
}

CHAR8
*AsciiStrToLower (
  IN CHAR8   *Str
) {
  CHAR8   *Tmp = AllocateCopyPool(AsciiStrSize(Str), Str);
  INTN      i;

  for(i = 0; Tmp[i]; i++) {
    Tmp[i] = TO_ALOWER(Tmp[i]);
  }

  return Tmp;
}

CHAR8
*AsciiStriStr (
  IN CHAR8    *String,
  IN CHAR8    *SearchString
) {
  return AsciiStrStr(AsciiStrToLower(String), AsciiStrToLower(SearchString));
}


/*
  Taken from Shell
  Trim leading trailing spaces
*/
EFI_STATUS
AsciiTrimSpaces (
  IN CHAR8 **String
) {
  if (!String || !(*String)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Remove any spaces and tabs at the beginning of the (*String).
  //
  while (((*String)[0] == ' ') || ((*String)[0] == '\t')) {
    CopyMem((*String), (*String)+1, AsciiStrSize((*String)) - sizeof((*String)[0]));
  }

  //
  // Remove any spaces and tabs at the end of the (*String).
  //
  while (
    (AsciiStrLen (*String) > 0) &&
    (
      ((*String)[AsciiStrLen((*String))-1] == ' ') ||
      ((*String)[AsciiStrLen((*String))-1] == '\t')
    )
  ) {
    (*String)[AsciiStrLen((*String))-1] = '\0';
  }

  return (EFI_SUCCESS);
}

// If Null-terminated strings are case insensitive equal or its sSize symbols are equal then TRUE
BOOLEAN
AsciiStriNCmp (
  IN  CONST CHAR8   *FirstS,
  IN  CONST CHAR8   *SecondS,
  IN  CONST UINTN    sSize
) {
  INTN    i = sSize;

  while ( i && (*FirstS != '\0') ) {
    if ( (((*FirstS >= 'a') && (*FirstS <= 'z')) ? (*FirstS - ('a' - 'A')) : *FirstS ) !=
      (((*SecondS >= 'a') && (*SecondS <= 'z')) ? (*SecondS - ('a' - 'A')) : *SecondS ) ) return FALSE;
    FirstS++;
    SecondS++;
    i--;
  }

  return TRUE;
}

// Case insensitive search of WhatString in WhereString
BOOLEAN
AsciiStrStriN (
  IN  CONST CHAR8   *WhatString,
  IN  CONST UINTN   sWhatSize,
  IN  CONST CHAR8   *WhereString,
  IN  CONST UINTN   sWhereSize
) {
  INTN      i = sWhereSize;
  BOOLEAN   Found = FALSE;

  if (sWhatSize > sWhereSize) return FALSE;
  for (; i && !Found; i--) {
    Found = AsciiStriNCmp(WhatString, WhereString, sWhatSize);
    WhereString++;
  }

  return Found;
}

UINT8
hexstrtouint8 (
  CHAR8   *buf
) {
  INT8  i = 0;

  if (IS_DIGIT(buf[0])) {
    i = buf[0]-'0';
  } else if (IS_HEX(buf[0])) {
    i = (buf[0] | 0x20) - 'a' + 10;
  }

  if (AsciiStrLen(buf) == 1) {
    return i;
  }

  i <<= 4;
  if (IS_DIGIT(buf[1])) {
    i += buf[1]-'0';
  } else if (IS_HEX(buf[1])) {
    i += (buf[1] | 0x20) - 'a' + 10;
  }

  return i;
}

BOOLEAN
IsHexDigit (
  CHAR8   c
) {
  return (IS_DIGIT(c) || (IS_HEX(c))) ? TRUE : FALSE;
}

//out value is a number of byte. If len is even then out = len/2

UINT32
hex2bin (
  IN  CHAR8   *hex,
  OUT UINT8   *bin,
      UINT32  len
) {  //assume len = number of UINT8 values
  CHAR8   *p, buf[3];
  UINT32  i, outlen = 0;

  if (hex == NULL || bin == NULL || len <= 0 || AsciiStrLen(hex) < len * 2) {
    //DBG("[ERROR] bin2hex input error\n"); //this is not error, this is empty value
    return FALSE;
  }

  buf[2] = '\0';
  p = (CHAR8 *) hex;

  for (i = 0; i < len; i++) {
    while ((*p == 0x20) || (*p == ',')) {
      p++; //skip spaces and commas
    }

    if (*p == 0) {
      break;
    }

    if (!IsHexDigit(p[0]) || !IsHexDigit(p[1])) {
      //MsgLog("[ERROR] bin2hex '%a' syntax error\n", hex);
      return 0;
    }

    buf[0] = *p++;
    buf[1] = *p++;
    bin[i] = hexstrtouint8(buf);
    outlen++;
  }

  //bin[outlen] = 0;

  return outlen;
}

CHAR8
*Bytes2HexStr (
  UINT8   *data,
  UINTN   len
) {
  UINTN   i, j, b = 0;
  CHAR8   *result = AllocateZeroPool((len*2)+1);

  for (i = j = 0; i < len; i++) {
    b = data[i] >> 4;
    result[j++] = (CHAR8) (87 + b + (((b - 10) >> 31) & -39));
    b = data[i] & 0xf;
    result[j++] = (CHAR8) (87 + b + (((b - 10) >> 31) & -39));
  }

  result[j] = '\0';

  return result;
}

/** Returns pointer to last Char in String or NULL. */
CHAR16*
EFIAPI
GetStrLastChar (
  IN CHAR16   *String
) {
  CHAR16    *Pos;

  if ((String == NULL) || (*String == L'\0')) {
    return NULL;
  }

  // go to end
  Pos = String;

  while (*Pos != L'\0') {
    Pos++;
  }

  Pos--;

  return Pos;
}

/** Returns pointer to last occurence of Char in String or NULL. */
CHAR16*
EFIAPI
GetStrLastCharOccurence (
  IN CHAR16   *String,
  IN CHAR16   Char
) {
  CHAR16    *Pos;

  if ((String == NULL) || (*String == L'\0')) {
    return NULL;
  }

  // go to end
  Pos = String;
  while (*Pos != L'\0') {
    Pos++;
  }

  // search for Char
  while ((*Pos != Char) && (Pos != String)) {
    Pos--;
  }

  return (*Pos == Char) ? Pos : NULL;
}

#if 0

/** Returns upper case version of char - valid only for ASCII chars in unicode. */
CHAR16
EFIAPI
ToUpperChar (
  IN CHAR16   Chr
) {
  CHAR8   C;

  if (Chr > 0x100) return Chr;

  C = (CHAR8)Chr;

  return ((C >= 'a' && C <= 'z') ? C - ('a' - 'A') : C);
}

/** Returns 0 if two strings are equal, !=0 otherwise. Compares just first 8 bits of chars (valid for ASCII), case insensitive.. */
UINTN
EFIAPI
StrCmpiBasic (
  IN CHAR16   *String1,
  IN CHAR16   *String2
) {
  CHAR16    Chr1, Chr2;

  //DBG("Cmpi('%s', '%s') ", String1, String2);

  if ((String1 == NULL) || (String2 == NULL)) {
    return 1;
  }

  if ((*String1 == L'\0') && (*String2 == L'\0')) {
    return 0;
  }

  if ((*String1 == L'\0') || (*String2 == L'\0')) {
    return 1;
  }

  Chr1 = ToUpperChar(*String1);
  Chr2 = ToUpperChar(*String2);

  while ((*String1 != L'\0') && (Chr1 == Chr2)) {
    String1++;
    String2++;
    Chr1 = ToUpperChar(*String1);
    Chr2 = ToUpperChar(*String2);
  }

  //DBG("=%s ", (Chr1 - Chr2) ? L"NEQ" : L"EQ");

  return Chr1 - Chr2;
}
#endif

/** Returns TRUE if String1 starts with String2, FALSE otherwise. Compares just first 8 bits of chars (valid for ASCII), case insensitive.. */
BOOLEAN
EFIAPI
StriStartsWith (
  IN CHAR16   *String1,
  IN CHAR16   *String2
) {
  CHAR16    Chr1, Chr2;
  BOOLEAN   Result;

  //DBG("StriStarts('%s', '%s') ", String1, String2);

  if ((String1 == NULL) || (String2 == NULL)) {
    return FALSE;
  }

  if ((*String1 == L'\0') && (*String2 == L'\0')) {
    return TRUE;
  }

  if ((*String1 == L'\0') || (*String2 == L'\0')) {
    return FALSE;
  }

  Chr1 = TO_UPPER(*String1);
  Chr2 = TO_UPPER(*String2);

  while ((Chr1 != L'\0') && (Chr2 != L'\0') && (Chr1 == Chr2)) {
    String1++;
    String2++;
    Chr1 = TO_UPPER(*String1);
    Chr2 = TO_UPPER(*String2);
  }

  Result = ((Chr1 == L'\0') && (Chr2 == L'\0')) ||
           ((Chr1 != L'\0') && (Chr2 == L'\0'));

  //DBG("=%s \n", Result ? L"TRUE" : L"FALSE");

  return Result;
}
