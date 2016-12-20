/*
 * Copyright (c) 2011-2012 Frank Peng. All rights reserved.
 *
 */

#include <Library/Platform/KernelPatcher.h>

//#define KEXT_DEBUG 0

//#if KEXT_DEBUG
//#define DBG(...)  Print(__VA_ARGS__);
//#else
//#define DBG(...)
//#endif

// runtime debug
#define DBG_ON(entry) \
  ((entry != NULL) && (entry->KernelAndKextPatches != NULL) \
  /*&& entry->KernelAndKextPatches->KPDebug*/ && (OSFLAG_ISSET(gSettings.FlagsBits, OSFLAG_DBGPATCHES) || gSettings.DebugKP))
#define DBG_RT(entry, ...) \
  if (DBG_ON(entry)) AsciiPrint(__VA_ARGS__)
#define DBG_PAUSE(entry, s) \
  if (DBG_ON(entry)) gBS->Stall(s * 1000000)

//
// Searches Source for Search pattern of size SearchSize
// and returns the number of occurences.
//
UINTN
SearchAndCount (
  UINT8     *Source,
  UINT32    SourceSize,
  UINT8     *Search,
  UINTN     SearchSize
) {
  UINTN     NumFounds = 0;
  UINT8     *End = Source + SourceSize;

  while (Source < End) {
    if (CompareMem(Source, Search, SearchSize) == 0) {
      NumFounds++;
      Source += SearchSize;
    } else {
      Source++;
    }
  }

  return NumFounds;
}

//
// Searches Source for Search pattern of size SearchSize
// and replaces it with Replace up to MaxReplaces times.
// If MaxReplaces <= 0, then there is no restriction on number of replaces.
// Replace should have the same size as Search.
// Returns number of replaces done.
//
UINTN
SearchAndReplace (
  UINT8     *Source,
  UINT32    SourceSize,
  UINT8     *Search,
  UINTN     SearchSize,
  UINT8     *Replace,
  INTN      MaxReplaces
) {
  UINTN     NumReplaces = 0;
  BOOLEAN   NoReplacesRestriction = MaxReplaces <= 0;
  UINT8     *End = Source + SourceSize;

  if (!Source || !Search || !Replace || !SearchSize) {
    return 0;
  }

  while ((Source < End) && (NoReplacesRestriction || (MaxReplaces > 0))) {
    if (CompareMem(Source, Search, SearchSize) == 0) {
      CopyMem(Source, Replace, SearchSize);
      NumReplaces++;
      MaxReplaces--;
      Source += SearchSize;
    } else {
      Source++;
    }
  }

  return NumReplaces;
}

UINTN
SearchAndReplaceTxt (
  UINT8     *Source,
  UINT32    SourceSize,
  UINT8     *Search,
  UINTN     SearchSize,
  UINT8     *Replace,
  INTN      MaxReplaces
) {
  BOOLEAN   NoReplacesRestriction = (MaxReplaces <= 0);
  UINTN     NumReplaces = 0, Skip;
  UINT8     *End = Source + SourceSize, *SearchEnd = Search + SearchSize, *Pos = NULL, *FirstMatch = Source;

  if (!Source || !Search || !Replace || !SearchSize) {
    return 0;
  }

  while (
    ((Source + SearchSize) <= End) &&
    (NoReplacesRestriction || (MaxReplaces > 0))
  ) { // num replaces
    while (*Source != '\0') {  //comparison
      Pos = Search;
      FirstMatch = Source;
      Skip = 0;

      while ((*Source != '\0') && (Pos != SearchEnd)) {
        if (*Source <= 0x20) { //skip invisibles in sources
          Source++;
          Skip++;
          continue;
        }

        if (*Source != *Pos) {
          break;
        }

        //AsciiPrint("%c", *Source);
        Source++;
        Pos++;
      }

      if (Pos == SearchEnd) { // pattern found
        Pos = FirstMatch;
        break;
      } else {
        Pos = NULL;
      }

      Source = FirstMatch + 1;
      /*
      if (Pos != Search) {
        AsciiPrint("\n");
      }
      */

    }

    if (!Pos) {
      break;
    }

    CopyMem (Pos, Replace, SearchSize);
    SetMem (Pos + SearchSize, Skip, 0x20); //fill skip places with spaces
    NumReplaces++;
    MaxReplaces--;
    Source = FirstMatch + SearchSize + Skip;
  }

  return NumReplaces;
}

/** Extracts kext BundleIdentifier from given Plist into gKextBundleIdentifier */

#define PropCFBundleIdentifierKey "<key>" kPropCFBundleIdentifier "</key>"

CHAR8
*ExtractKextBundleIdentifier (
  CHAR8     *Plist
) {
  CHAR8     *Tag, *BIStart, *BIEnd,
            gKextBundleIdentifier[256];
  INTN      DictLevel = 0;

  gKextBundleIdentifier[0] = '\0';

  // start with first <dict>
  Tag = AsciiStrStr(Plist, "<dict>");
  if (Tag == NULL) {
    goto Finish;
  }

  Tag += 6;
  DictLevel++;

  while (*Tag != '\0') {
    if (AsciiStrnCmp(Tag, "<dict>", 6) == 0) {
      // opening dict
      DictLevel++;
      Tag += 6;
    } else if (AsciiStrnCmp(Tag, "</dict>", 7) == 0) {
      // closing dict
      DictLevel--;
      Tag += 7;
    } else if ((DictLevel == 1) && (AsciiStrnCmp(Tag, PropCFBundleIdentifierKey, 29) == 0)) {
      // BundleIdentifier is next <string>...</string>
      BIStart = AsciiStrStr(Tag + 29, "<string>");
      if (BIStart != NULL) {
        BIStart += 8; // skip "<string>"
        BIEnd = AsciiStrStr(BIStart, "</string>");
        if ((BIEnd != NULL) && ((BIEnd - BIStart + 1) < sizeof(gKextBundleIdentifier))) {
          CopyMem(gKextBundleIdentifier, BIStart, BIEnd - BIStart);
          gKextBundleIdentifier[BIEnd - BIStart] = '\0';
          //return;
          goto Finish;
        }
      }
      Tag++;
    } else {
      Tag++;
    }

    // advance to next tag
    while (*Tag != '<' && *Tag != '\0') {
      Tag++;
    }
  }

  Finish:

  return AllocateCopyPool(sizeof(gKextBundleIdentifier), gKextBundleIdentifier);
}

BOOLEAN
isPatchNameMatch (
  CHAR8   *BundleIdentifier,
  CHAR8   *InfoPlist,
  CHAR8   *Name
) {
  return
    (
      (InfoPlist != NULL) &&
      (countOccurrences(Name, '.') < 2) // Full BundleIdentifier: com.apple.driver.AppleHDA
    )
      ? (AsciiStrStr(InfoPlist, Name) != NULL)
      : (AsciiStrCmp(BundleIdentifier, Name) == 0);
}

////////////////////////////////////
//
// ATIConnectors patch
//
// bcc9's patch: http://www.insanelymac.com/forum/index.php?showtopic=249642
//

// inited or not?
BOOLEAN ATIConnectorsPatchInited = FALSE;

// ATIConnectorsController's bundle IDs for
// 0: ATI version - Lion, SnowLeo 10.6.7 2011 MBP
// 1: AMD version - ML
CHAR8 ATIKextBundleId[4][64];

//
// Inits patcher: prepares ATIKextBundleIds.
//
VOID
ATIConnectorsPatchInit (
  LOADER_ENTRY    *Entry
) {
  //
  // prepare bundle ids
  //

  // Lion, SnowLeo 10.6.7 2011 MBP
  AsciiSPrint (
    ATIKextBundleId[0],
    sizeof(ATIKextBundleId[0]),
    "com.apple.kext.ATI%sController",
    Entry->KernelAndKextPatches->KPATIConnectorsController
  );

  // ML
  AsciiSPrint (
    ATIKextBundleId[1],
    sizeof(ATIKextBundleId[1]),
    "com.apple.kext.AMD%sController",
    Entry->KernelAndKextPatches->KPATIConnectorsController
  );

  AsciiSPrint (
    ATIKextBundleId[2],
    sizeof(ATIKextBundleId[2]),
    "com.apple.kext.ATIFramebuffer"
  );

  AsciiSPrint (
    ATIKextBundleId[3],
    sizeof(ATIKextBundleId[3]),
    "com.apple.kext.AMDFramebuffer"
  );

  ATIConnectorsPatchInited = TRUE;

  //DBG_RT(Entry, "Bundle1: %a\n", ATIKextBundleId[0]);
  //DBG_RT(Entry, "Bundle2: %a\n", ATIKextBundleId[1]);
  //DBG_RT(Entry, "Bundle3: %a\n", ATIKextBundleId[2]);
  //DBG_RT(Entry, "Bundle4: %a\n", ATIKextBundleId[3]);
  //DBG_PAUSE(Entry, 5);
}

//
// Registers kexts that need force-load during WithKexts boot.
//
VOID
ATIConnectorsPatchRegisterKexts (
  FSINJECTION_PROTOCOL  *FSInject,
  FSI_STRING_LIST       *ForceLoadKexts,
  LOADER_ENTRY          *Entry
) {
  CHAR16  *AtiForceLoadKexts[] = {
    L"\\IOGraphicsFamily.kext\\Info.plist",
    L"\\ATISupport.kext\\Contents\\Info.plist",
    L"\\AMDSupport.kext\\Contents\\Info.plist",
    L"\\AppleGraphicsControl.kext\\Info.plist",
    L"\\AppleGraphicsControl.kext\\Contents\\PlugIns\\AppleGraphicsDeviceControl.kext\\Info.plist"/*,
    // SnowLeo
    L"\\ATIFramebuffer.kext\\Contents\\Info.plist",
    L"\\AMDFramebuffer.kext\\Contents\\Info.plist"*/
  };
  UINTN   i = 0, AtiForceLoadKextsCount = ARRAY_SIZE(AtiForceLoadKexts);

  // for future?
  FSInject->AddStringToList (
              ForceLoadKexts,
              PoolPrint(L"\\AMD%sController.kext\\Contents\\Info.plist", Entry->KernelAndKextPatches->KPATIConnectorsController)
            );

  // Lion, ML, SnowLeo 10.6.7 2011 MBP
  FSInject->AddStringToList (
              ForceLoadKexts,
              PoolPrint(L"\\ATI%sController.kext\\Contents\\Info.plist", Entry->KernelAndKextPatches->KPATIConnectorsController)
            );

  // dependencies
  while (i < AtiForceLoadKextsCount) {
    FSInject->AddStringToList(ForceLoadKexts, AtiForceLoadKexts[i++]);
  }
}

//
// Patch function.
//
VOID
ATIConnectorsPatch (
  UINT8         *Driver,
  UINT32        DriverSize,
  CHAR8         *InfoPlist,
  UINT32        InfoPlistSize,
  LOADER_ENTRY  *Entry
) {
  UINTN   Num = 0;

  DBG_RT(Entry, "\nATIConnectorsPatch: driverAddr = %x, driverSize = %x\nController = %s\n",
         Driver, DriverSize, Entry->KernelAndKextPatches->KPATIConnectorsController);

  // number of occurences od Data should be 1
  Num = SearchAndCount (
          Driver,
          DriverSize,
          Entry->KernelAndKextPatches->KPATIConnectorsData,
          Entry->KernelAndKextPatches->KPATIConnectorsDataLen
        );

  if (Num > 1) {
    // error message - shoud always be printed
    DBG_RT(Entry, "==> KPATIConnectorsData found %d times - skip patching!\n", Num);
    //DBG_PAUSE(Entry, 5);
    return;
  }

  // patch
  Num = SearchAndReplace (
          Driver,
          DriverSize,
          Entry->KernelAndKextPatches->KPATIConnectorsData,
          Entry->KernelAndKextPatches->KPATIConnectorsDataLen,
          Entry->KernelAndKextPatches->KPATIConnectorsPatch,
          1
        );

  if (Num > 0) {
    DBG_RT(Entry, "==> patched %d times!\n", Num);
  } else {
    DBG_RT(Entry, "==> NOT patched!\n");
  }

  //DBG_PAUSE(Entry, 5);
}

VOID
GetText (
  UINT8           *binary,
  OUT UINT32      *Addr,
  OUT UINT32      *Size,
  OUT UINT32      *Off,
  LOADER_ENTRY    *Entry
)
{
          UINT32              ncmds, cmdsize, binaryIndex, sectionIndex;
          UINTN               cnt;
  struct  load_command        *loadCommand;
  struct  segment_command_64  *segCmd64;
  struct  section_64          *sect64;

  if (MACH_GET_MAGIC(binary) != MH_MAGIC_64) {
    return;
  }

  binaryIndex = sizeof (struct mach_header_64);

  ncmds = MACH_GET_NCMDS (binary);

  for (cnt = 0; cnt < ncmds; cnt++) {
    loadCommand = (struct load_command *) (binary + binaryIndex);

    if (loadCommand->cmd != LC_SEGMENT_64) {
      continue;
    }

    cmdsize = loadCommand->cmdsize;
    segCmd64 = (struct segment_command_64 *)loadCommand;
    sectionIndex = sizeof(struct segment_command_64);

    while (sectionIndex < segCmd64->cmdsize) {
      sect64 = (struct section_64 *)((UINT8*)segCmd64 + sectionIndex);
      sectionIndex += sizeof(struct section_64);

      if (
        (sect64->size > 0) &&
        (AsciiStrCmp(sect64->segname, kTextSegment) == 0) &&
        (AsciiStrCmp(sect64->sectname, kTextTextSection) == 0)
      ) {
        *Addr = (UINT32)sect64->addr;
        *Size = (UINT32)sect64->size;
        *Off = sect64->offset;
        //DBG_RT(Entry, "%a, %a address 0x%x\n", kTextSegment, kTextTextSection, Off);
        //DBG_PAUSE(Entry, 10);
        break;
      }
    }

    binaryIndex += cmdsize;
  }

  return;
}

////////////////////////////////////
//
// AsusAICPUPM patch
//
// fLaked's SpeedStepper patch for Asus (and some other) boards:
// http://www.insanelymac.com/forum/index.php?showtopic=258611
//
// Credits: Samantha/RevoGirl/DHP
// http://www.insanelymac.com/forum/topic/253642-dsdt-for-asus-p8p67-m-pro/page__st__200#entry1681099
// Rehabman corrections 2014
//

UINT8   MovlE2ToEcx[] = { 0xB9, 0xE2, 0x00, 0x00, 0x00 };
UINT8   MovE2ToCx[]   = { 0x66, 0xB9, 0xE2, 0x00 };
UINT8   Wrmsr[]       = { 0x0F, 0x30 };

VOID
AsusAICPUPMPatch (
  UINT8           *Driver,
  UINT32          DriverSize,
  CHAR8           *InfoPlist,
  UINT32          InfoPlistSize,
  LOADER_ENTRY    *Entry
) {
  UINTN   Index1 = 0, Index2 = 0, Count = 0;
  UINT32  addr, size, off;

  GetText (Driver, &addr, &size, &off, Entry);

  DBG_RT(Entry, "\nAsusAICPUPMPatch: driverAddr = %x, driverSize = %x\n", Driver, DriverSize);

  if (off && size) {
    Index1 = off;
    DriverSize = (off + size);
  }

  //TODO: we should scan only __text __TEXT
  for (; Index1 < DriverSize; Index1++) {
    // search for MovlE2ToEcx
    if (CompareMem(Driver + Index1, MovlE2ToEcx, sizeof(MovlE2ToEcx)) == 0) {
      // search for wrmsr in next few bytes
      for (Index2 = Index1 + sizeof(MovlE2ToEcx); Index2 < Index1 + sizeof(MovlE2ToEcx) + 32; Index2++) {
        if ((Driver[Index2] == Wrmsr[0]) && (Driver[Index2 + 1] == Wrmsr[1])) {
          // found it - patch it with nops
          Count++;
          Driver[Index2] = 0x90;
          Driver[Index2 + 1] = 0x90;
          DBG_RT(Entry, " %d. patched at 0x%x\n", Count, Index2);
          break;
        } else if (
          ((Driver[Index2] == 0xC9) && (Driver[Index2 + 1] == 0xC3)) ||
          ((Driver[Index2] == 0x5D) && (Driver[Index2 + 1] == 0xC3)) ||
          ((Driver[Index2] == 0xB9) && (Driver[Index2 + 3] == 0) && (Driver[Index2 + 4] == 0)) ||
          ((Driver[Index2] == 0x66) && (Driver[Index2 + 1] == 0xB9) && (Driver[Index2 + 3] == 0))
        ) {
          // a leave/ret will cancel the search
          // so will an intervening "mov[l] $xx, [e]cx"
          break;
        }
      }
    } else if (CompareMem(Driver + Index1, MovE2ToCx, sizeof(MovE2ToCx)) == 0) {
      // search for wrmsr in next few bytes
      for (Index2 = Index1 + sizeof(MovE2ToCx); Index2 < Index1 + sizeof(MovE2ToCx) + 32; Index2++) {
        if ((Driver[Index2] == Wrmsr[0]) && (Driver[Index2 + 1] == Wrmsr[1])) {
          // found it - patch it with nops
          Count++;
          Driver[Index2] = 0x90;
          Driver[Index2 + 1] = 0x90;
          DBG_RT(Entry, " %d. patched at 0x%x\n", Count, Index2);
          break;
        } else if (
          ((Driver[Index2] == 0xC9) && (Driver[Index2 + 1] == 0xC3)) ||
          ((Driver[Index2] == 0x5D) && (Driver[Index2 + 1] == 0xC3)) ||
          ((Driver[Index2] == 0xB9) && (Driver[Index2 + 3] == 0) && (Driver[Index2 + 4] == 0)) ||
          ((Driver[Index2] == 0x66) && (Driver[Index2 + 1] == 0xB9) && (Driver[Index2 + 3] == 0))
        ) {
          // a leave/ret will cancel the search
          // so will an intervening "mov[l] $xx, [e]cx"
          break;
        }
      }
    }
  }

  DBG_RT(Entry, "= %d patches\n", Count);
  //DBG_PAUSE(Entry, 5);
}

////////////////////////////////////
//
// Place other kext patches here
//

// ...

////////////////////////////////////
//
// Generic kext patch functions
//
//
VOID
AnyKextPatch (
  UINT8         *Driver,
  UINT32        DriverSize,
  CHAR8         *InfoPlist,
  UINT32        InfoPlistSize,
  INT32         N,
  LOADER_ENTRY  *Entry
) {
  UINTN   Num = 0;
  //INTN    Ind;

  DBG_RT(Entry, "\nAnyKextPatch %d: driverAddr = %x, driverSize = %x\nAnyKext = %a\n",
         N, Driver, DriverSize, Entry->KernelAndKextPatches->KextPatches[N].Label);

  if (Entry->KernelAndKextPatches->KextPatches[N].Disabled) {
    DBG_RT(Entry, "==> DISABLED!\n");
    return;
  }

  if (Entry->KernelAndKextPatches->KextPatches[N].IsPlistPatch) { // Info plist patch
    DBG_RT(Entry, "Info.plist patch\n");

    /*
      DBG_RT(Entry, "Info.plist data : '");

      for (Ind = 0; Ind < Entry->KernelAndKextPatches->KextPatches[N].DataLen; Ind++) {
        DBG_RT(Entry, "%c", Entry->KernelAndKextPatches->KextPatches[N].Data[Ind]);
      }

      DBG_RT(Entry, "' ->\n");
      DBG_RT(Entry, "Info.plist patch: '");

      for (Ind = 0; Ind < Entry->KernelAndKextPatches->KextPatches[N].DataLen; Ind++) {
        DBG_RT(Entry, "%c", Entry->KernelAndKextPatches->KextPatches[N].Patch[Ind]);
      }

      DBG_RT(Entry, "' \n");
    */

    Num = SearchAndReplaceTxt (
            (UINT8*)InfoPlist,
            InfoPlistSize,
            Entry->KernelAndKextPatches->KextPatches[N].Data,
            Entry->KernelAndKextPatches->KextPatches[N].DataLen,
            Entry->KernelAndKextPatches->KextPatches[N].Patch,
            -1
          );
  } else { // kext binary patch
    UINT32    addr, size, off;

    GetText (Driver, &addr, &size, &off, Entry);

    DBG_RT(Entry, "Binary patch\n");

    if (off && size) {
      Driver += off;
      DriverSize = size;
    }

    Num = SearchAndReplace (
            Driver,
            DriverSize,
            Entry->KernelAndKextPatches->KextPatches[N].Data,
            Entry->KernelAndKextPatches->KextPatches[N].DataLen,
            Entry->KernelAndKextPatches->KextPatches[N].Patch,
            -1
          );
  }

  if (Num > 0) {
    DBG_RT(Entry, "==> patched %d times!\n", Num);
  } else {
    DBG_RT(Entry, "==> NOT patched!\n");
  }

  //DBG_PAUSE(Entry, 2);
}

//
// Called from SetFSInjection(), before boot.efi is started,
// to allow patchers to prepare FSInject to force load needed kexts.
//
VOID
KextPatcherRegisterKexts (
  FSINJECTION_PROTOCOL    *FSInject,
  FSI_STRING_LIST         *ForceLoadKexts,
  LOADER_ENTRY            *Entry
) {
  INTN i;

  if (Entry->KernelAndKextPatches->KPATIConnectorsController != NULL) {
    ATIConnectorsPatchRegisterKexts(FSInject, ForceLoadKexts, Entry);
  }

  // Not sure about this. NrKexts / NrForceKexts, what purposes?
  for (i = 0; i < Entry->KernelAndKextPatches->/*NrKexts*/NrForceKexts; i++) {
    FSInject->AddStringToList (
                ForceLoadKexts,
                //PoolPrint(
                //  L"\\%a.kext\\Contents\\Info.plist",
                //    Entry->KernelAndKextPatches->KextPatches[i].Filename
                //      ? Entry->KernelAndKextPatches->KextPatches[i].Filename
                //      : Entry->KernelAndKextPatches->KextPatches[i].Name
                //)
                PoolPrint(L"\\%a\\Contents\\Info.plist", Entry->KernelAndKextPatches->ForceKexts[i])
              );
  }
}

//
// PatchKext is called for every kext from prelinked kernel (kernelcache) or from DevTree (booting with drivers).
// Add kext detection code here and call kext specific patch function.
//
VOID
PatchKext (
  UINT8         *Driver,
  UINT32        DriverSize,
  CHAR8         *InfoPlist,
  UINT32        InfoPlistSize,
  LOADER_ENTRY  *Entry
) {
  INT32   i;
  CHAR8   *gKextBundleIdentifier = ExtractKextBundleIdentifier(InfoPlist);

  if (
    (Entry->KernelAndKextPatches->KPATIConnectorsController != NULL) &&
    (
      isPatchNameMatch(gKextBundleIdentifier, NULL, ATIKextBundleId[0]) ||
      isPatchNameMatch(gKextBundleIdentifier, NULL, ATIKextBundleId[1]) ||
      isPatchNameMatch(gKextBundleIdentifier, NULL, ATIKextBundleId[2]) ||
      isPatchNameMatch(gKextBundleIdentifier, NULL, ATIKextBundleId[3])
    )
  ) {
    //
    // ATIConnectors
    //
    if (!ATIConnectorsPatchInited) {
      ATIConnectorsPatchInit(Entry);
    }

    DBG_RT(Entry, "Kext: %a\n", gKextBundleIdentifier);
    ATIConnectorsPatch(Driver, DriverSize, InfoPlist, InfoPlistSize, Entry);
  } else if (
    Entry->KernelAndKextPatches->KPAsusAICPUPM &&
    isPatchNameMatch(gKextBundleIdentifier, NULL, "com.apple.driver.AppleIntelCPUPowerManagement")
  ) {
    //
    // AsusAICPUPM
    //
    DBG_RT(Entry, "Kext: %a\n", gKextBundleIdentifier);
    AsusAICPUPMPatch(Driver, DriverSize, InfoPlist, InfoPlistSize, Entry);
  } else {
    //
    // others
    //
    for (i = 0; i < Entry->KernelAndKextPatches->NrKexts; i++) {
      if (
        isPatchNameMatch(gKextBundleIdentifier, InfoPlist, Entry->KernelAndKextPatches->KextPatches[i].Name)
      ) {
        DBG_RT(Entry, "Kext: %a\n", gKextBundleIdentifier);
        AnyKextPatch(Driver, DriverSize, InfoPlist, InfoPlistSize, i, Entry);
      }
    }
  }

  FreePool(gKextBundleIdentifier);
}

//
// Returns parsed hex integer key.
// Plist - kext pist
// Key - key to find
// WholePlist - _PrelinkInfoDictionary, used to find referenced values
//
// Searches for Key in Plist and it's value:
// a) <integer ID="26" size="64">0x2b000</integer>
//    returns 0x2b000
// b) <integer IDREF="26"/>
//    searches for <integer ID="26"... from WholePlist
//    and returns value from that referenced field
//
// Whole function is here since we should avoid ParseXML() and it's
// memory allocations during ExitBootServices(). And it seems that
// ParseXML() does not support IDREF.
// This func is hard to read and debug and probably not reliable,
// but it seems it works.
//
UINT64
GetPlistHexValue (
  CHAR8     *Plist,
  CHAR8     *Key,
  CHAR8     *WholePlist
) {
  CHAR8     *Value, *IntTag, *IDStart, *IDEnd, Buffer[48];
  UINT64    NumValue = 0;
  UINTN     IDLen;
  //static INTN   DbgCount = 0;

  // search for Key
  Value = AsciiStrStr(Plist, Key);
  if (Value == NULL) {
    //DBG(L"\nNo key: %a\n", Key);
    return 0;
  }

  // search for <integer
  IntTag = AsciiStrStr(Value, "<integer");
  if (IntTag == NULL) {
    //DBG(L"\nNo integer\n");
    return 0;
  }

  // find <integer end
  Value = AsciiStrStr(IntTag, ">");
  if (Value == NULL) {
    //DBG(L"\nNo <integer end\n");
    return 0;
  }

  if (Value[-1] != '/') {
    // normal case: value is here
    NumValue = AsciiStrHexToUint64(Value + 1);
    return NumValue;
  }

  // it might be a reference: IDREF="173"/>
  Value = AsciiStrStr(IntTag, "<integer IDREF=\"");
  if (Value != IntTag) {
    //DBG(L"\nNo <integer IDREF=\"\n");
    return 0;
  }

  // compose <integer ID="xxx" in the Buffer
  IDStart = AsciiStrStr(IntTag, "\"") + 1;
  IDEnd = AsciiStrStr(IDStart, "\"");
  IDLen = IDEnd - IDStart;

  /*
    if (DbgCount < 3) {
      AsciiStrnCpy(Buffer, Value, sizeof(Buffer) - 1);
      DBG(L"\nRef: '%a'\n", Buffer);
    }
  */

  if (IDLen > 8) {
    //DBG(L"\nIDLen too big\n");
    return 0;
  }

  AsciiStrCpy(Buffer, "<integer ID=\"");
  AsciiStrnCat(Buffer, IDStart, IDLen);
  AsciiStrCat(Buffer, "\"");

  /*
    if (DbgCount < 3) {
      DBG(L"Searching: '%a'\n", Buffer);
    }
  */

  // and search whole plist for ID
  IntTag = AsciiStrStr(WholePlist, Buffer);
  if (IntTag == NULL) {
    //DBG(L"\nNo %a\n", Buffer);
    return 0;
  }

  // got it. find closing >
  /*
    if (DbgCount < 3) {
      AsciiStrnCpy(Buffer, IntTag, sizeof(Buffer) - 1);
      DBG(L"Found: '%a'\n", Buffer);
    }
  */

  Value = AsciiStrStr(IntTag, ">");
  if (Value == NULL) {
   // DBG(L"\nNo <integer end\n");
    return 0;
  }

  if (Value[-1] == '/') {
    //DBG(L"\nInvalid <integer IDREF end\n");
    return 0;
  }

  // we should have value now
  NumValue = AsciiStrHexToUint64(Value + 1);

  /*
    if (DbgCount < 3) {
      AsciiStrnCpy(Buffer, IntTag, sizeof(Buffer) - 1);
      DBG(L"Found num: %x\n", NumValue);
      gBS->Stall(10000000);
    }
    DbgCount++;
  */

  return NumValue;
}

//
// Iterates over kexts in kernelcache
// and calls PatchKext() for each.
//
// PrelinkInfo section contains following plist, without spaces:
// <dict>
//   <key>_PrelinkInfoDictionary</key>
//   <array>
//     <!-- start of kext Info.plist -->
//     <dict>
//       <key>CFBundleName</key>
//       <string>MAC Framework Pseudoextension</string>
//       <key>_PrelinkExecutableLoadAddr</key>
//       <integer size="64">0xffffff7f8072f000</integer>
//       <!-- Kext size -->
//       <key>_PrelinkExecutableSize</key>
//       <integer size="64">0x3d0</integer>
//       <!-- Kext address -->
//       <key>_PrelinkExecutableSourceAddr</key>
//       <integer size="64">0xffffff80009a3000</integer>
//       ...
//     </dict>
//     <!-- start of next kext Info.plist -->
//     <dict>
//       ...
//     </dict>
//       ...
VOID
PatchPrelinkedKexts (
  LOADER_ENTRY    *Entry
) {
  CHAR8     *WholePlist, *DictPtr, *InfoPlistStart = NULL,
            *InfoPlistEnd = NULL, SavedValue;
  INTN      DictLevel = 0;
  UINT32    KextAddr, KextSize;
  //INTN      DbgCount = 0;


  WholePlist = (CHAR8*)(UINTN)KernelInfo->PrelinkInfoAddr;

  //
  // Detect FakeSMC and if present then
  // disable kext injection InjectKexts().
  // There is some bug in the folowing code that
  // searches for individual kexts in prelink info
  // and FakeSMC is not found on my SnowLeo although
  // it is present in kernelcache.
  // But searching through the whole prelink info
  // works and that's the reason why it is here.
  //
  //CheckForFakeSMC(WholePlist, Entry);

  DictPtr = WholePlist;
  while ((DictPtr = AsciiStrStr(DictPtr, "dict>")) != NULL) {

    if (DictPtr[-1] == '<') {
      // opening dict
      DictLevel++;
      if (DictLevel == 2) {
        // kext start
        InfoPlistStart = DictPtr - 1;
      }

    } else if ((DictPtr[-2] == '<') && (DictPtr[-1] == '/')) {

      // closing dict
      if ((DictLevel == 2) && (InfoPlistStart != NULL)) {
        // kext end
        InfoPlistEnd = DictPtr + 5 /* "dict>" */;

        // terminate Info.plist with 0
        SavedValue = *InfoPlistEnd;
        *InfoPlistEnd = '\0';

        // get kext address from _PrelinkExecutableSourceAddr
        // truncate to 32 bit to get physical addr
        KextAddr = (UINT32)GetPlistHexValue(InfoPlistStart, kPrelinkExecutableSourceKey, WholePlist);
        // KextAddr is always relative to 0x200000
        // and if KernelSlide is != 0 then KextAddr must be adjusted
        KextAddr += KernelInfo->Slide;
        // and adjust for AptioFixDrv's KernelRelocBase
        KextAddr += (UINT32)KernelInfo->RelocBase;

        KextSize = (UINT32)GetPlistHexValue(InfoPlistStart, kPrelinkExecutableSizeKey, WholePlist);

        /*
          if (DbgCount < 3 || DbgCount == 100 || DbgCount == 101 || DbgCount == 102 ) {
            DBG(L"\n\nKext: St = %x, Size = %x\n", KextAddr, KextSize);
            DBG(L"Info: St = %p, End = %p\n%a\n", InfoPlistStart, InfoPlistEnd, InfoPlistStart);
            gBS->Stall(20000000);
          }
        */

        // patch it
        PatchKext (
          (UINT8*)(UINTN)KextAddr,
          KextSize,
          InfoPlistStart,
          (UINT32)(InfoPlistEnd - InfoPlistStart),
          Entry
        );

        // return saved char
        *InfoPlistEnd = SavedValue;
        //DbgCount++;
      }

      DictLevel--;

    }
    DictPtr += 5;
  }
}

//
// Iterates over kexts loaded by booter
// and calls PatchKext() for each.
//
VOID
PatchLoadedKexts (
  LOADER_ENTRY    *Entry
) {
  CHAR8                             *PropName, SavedValue, *InfoPlist;
  DTEntry                           MMEntry;
  _BooterKextFileInfo               *KextFileInfo;
  _DeviceTreeBuffer                 *PropEntry;
  struct OpaqueDTPropertyIterator   OPropIter;
  DTPropertyIterator                PropIter = &OPropIter;
  //UINTN                           DbgCount = 0;

  DBG_RT(Entry, "\nPatchLoadedKexts ... dtRoot = %p\n", dtRoot);

  if (!dtRoot) {
    return;
  }

  DTInit(dtRoot);

  if (DTLookupEntry(NULL,"/chosen/memory-map", &MMEntry) == kSuccess) {
    if (DTCreatePropertyIteratorNoAlloc(MMEntry, PropIter) == kSuccess) {
      while (DTIterateProperties(PropIter, &PropName) == kSuccess) {
        //DBG_RT(Entry, "Prop: %a\n", PropName);
        if (AsciiStrStr(PropName,"Driver-")) {
          // PropEntry _DeviceTreeBuffer is the value of Driver-XXXXXX property
          PropEntry = (_DeviceTreeBuffer*)(((UINT8*)PropIter->currentProperty) + sizeof(DeviceTreeNodeProperty));
          //if (DbgCount < 3) {
          //  DBG_RT(Entry, "%a: paddr = %x, length = %x\n", PropName, PropEntry->paddr, PropEntry->length);
          //}

          // PropEntry->paddr points to _BooterKextFileInfo
          KextFileInfo = (_BooterKextFileInfo *)(UINTN)PropEntry->paddr;

          // Info.plist should be terminated with 0, but will also do it just in case
          InfoPlist = (CHAR8*)(UINTN)KextFileInfo->infoDictPhysAddr;
          SavedValue = InfoPlist[KextFileInfo->infoDictLength];
          InfoPlist[KextFileInfo->infoDictLength] = '\0';

          PatchKext (
            (UINT8*)(UINTN)KextFileInfo->executablePhysAddr,
            KextFileInfo->executableLength,
            InfoPlist,
            KextFileInfo->infoDictLength,
            Entry
          );

          // Check for FakeSMC here
          //CheckForFakeSMC(InfoPlist, Entry);

          InfoPlist[KextFileInfo->infoDictLength] = SavedValue;
          //DbgCount++;
        }

        //if(AsciiStrStr(PropName,"DriversPackage-")!=0)
        //{
        //    DBG_RT(Entry, "Found %a\n", PropName);
        //    break;
        //}
      }
    }
  }
}

//
// Entry for all kext patches.
// Will iterate through kext in prelinked kernel (kernelcache)
// or DevTree (drivers boot) and do patches.
//
VOID
KextPatcherStart (
  LOADER_ENTRY    *Entry
) {
  DBG_RT(Entry, "\n\n%a: Start\n", __FUNCTION__);

  if (KernelInfo->isCache) {
    DBG_RT(Entry, "Patching kernelcache ...\n");
    DBG_PAUSE(Entry, 2);

    PatchPrelinkedKexts(Entry);
  } else {
    DBG_RT(Entry, "Patching loaded kexts ...\n");
    DBG_PAUSE(Entry, 2);

    PatchLoadedKexts(Entry);
  }

  DBG_RT(Entry, "%a: End\n", __FUNCTION__);
}