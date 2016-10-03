/*
 * refit/menu.c
 * Menu functions
 *
 * Copyright (c) 2006 Christoph Pfisterer
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

//#include "Platform.h"
#include "libegint.h"   //this includes platform.h

#ifndef DEBUG_ALL
#define DEBUG_MENU 1
#else
#define DEBUG_MENU DEBUG_ALL
#endif

#if DEBUG_MENU == 0
#define DBG(...)
#else
#define DBG(...) DebugLog(DEBUG_MENU, __VA_ARGS__)
#endif

// scrolling definitions
STATIC INTN               MaxItemOnScreen = -1;
extern REFIT_MENU_ENTRY   MenuEntryReturn;

BOOLEAN SavePreBootLog = FALSE;

#define SCROLL_LINE_UP                  (0)
#define SCROLL_LINE_DOWN                (1)
#define SCROLL_PAGE_UP                  (2)
#define SCROLL_PAGE_DOWN                (3)
#define SCROLL_FIRST                    (4)
#define SCROLL_LAST                     (5)
#define SCROLL_NONE                     (6)
#define SCROLL_SCROLLBAR_MOVE           (7)

#define SCROLL_MODE_NONE                (0)
#define SCROLL_MODE_LOOP                (1)

#define TEXT_CORNER_REVISION            (1)
#define TEXT_CORNER_HELP                (2)

// other menu definitions

#define MENU_FUNCTION_INIT              (0)
#define MENU_FUNCTION_CLEANUP           (1)
#define MENU_FUNCTION_PAINT_ALL         (2)
#define MENU_FUNCTION_PAINT_SELECTION   (3)
#define MENU_FUNCTION_PAINT_TIMEOUT     (4)

#define LAYOUT_MAINMENU_HEIGHT          (376)
#define TITLEICON_SPACING               (16)

#define TILE_XSPACING                   (8)
#define TILE_YSPACING                   (24)
#define ROW0_SCROLLSIZE                 (100)

typedef VOID (*MENU_STYLE_FUNC)(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, IN UINTN Function, IN CHAR16 *ParamText);

static CHAR16 ArrowUp[2]   = { ARROW_UP, 0 };
static CHAR16 ArrowDown[2] = { ARROW_DOWN, 0 };

static EG_IMAGE *TextBuffer = NULL;

STATIC INTN         row0Count, row0PosX, row0PosXRunning;
STATIC INTN         row1Count, row1PosX, row1PosXRunning;
STATIC INTN         *itemPosX = NULL;
//static INTN       *itemPosY = NULL;
STATIC INTN         row0PosY, row1PosY, textPosY, FunctextPosY;
STATIC EG_IMAGE     *MainImage;
STATIC INTN         OldX = 0, OldY = 0;
STATIC INTN         OldTextWidth = 0;
STATIC UINTN        OldRow = 0;
STATIC INTN         OldTimeoutTextWidth = 0;
STATIC INTN         MenuWidth, TimeoutPosY;
STATIC INTN         EntriesPosX, EntriesPosY;
STATIC INTN         EntriesWidth, EntriesHeight, EntriesGap;

BOOLEAN             MainAnime = FALSE;
BOOLEAN             mGuiReady = FALSE;

INTN                OldChosenTheme;
INTN                OldChosenConfig;

REFIT_MENU_ENTRY MenuEntryOptions  = { L"Options", TAG_OPTIONS, 1, 0, 'O', NULL, NULL, NULL, NULL,
  {0, 0, 0, 0}, NULL };
REFIT_MENU_ENTRY MenuEntryAbout    = { L"About Clover", TAG_ABOUT, 1, 0, 'A', NULL, NULL, NULL, NULL,
  {0, 0, 0, 0},  NULL };
REFIT_MENU_ENTRY MenuEntryReset    = { L"Restart Computer", TAG_RESET, 1, 0, 'R', NULL, NULL, NULL, NULL,
  {0, 0, 0, 0},  NULL };
REFIT_MENU_ENTRY MenuEntryExit = { L"Exit Clover", TAG_EXIT, 1, 0, 'U', NULL, NULL, NULL, NULL,
  {0, 0, 0, 0},  NULL };
REFIT_MENU_ENTRY MenuEntryReturn   = { L"Return", TAG_RETURN, 0, 0, 0, NULL, NULL, NULL, NULL,
  {0, 0, 0, 0},  NULL };
REFIT_MENU_ENTRY MenuEntryHelp    = { L"Help", TAG_HELP, 1, 0, 'H', NULL, NULL, NULL, NULL,
  {0, 0, 0, 0},  NULL };

REFIT_MENU_SCREEN MainMenu    = {SCREEN_MAIN,     L"Main Menu", NULL, 0, NULL, 0, NULL, 0, L"Automatic boot",
  NULL, FALSE, FALSE, 0, 0, 0, 0, {0, 0, 0, 0}, NULL};
REFIT_MENU_SCREEN AboutMenu   = {SCREEN_ABOUT,    L"About",     NULL, 0, NULL, 0, NULL, 0, NULL,
  NULL, FALSE, FALSE, 0, 0, 0, 0, {0, 0, 0, 0}, NULL};
REFIT_MENU_SCREEN HelpMenu    = {SCREEN_HELP,     L"Help",      NULL, 0, NULL, 0, NULL, 0, NULL,
  NULL, FALSE, FALSE, 0, 0, 0, 0, {0, 0, 0, 0}, NULL};
REFIT_MENU_SCREEN OptionMenu  = {SCREEN_OPTIONS,  L"Options",   NULL, 0, NULL, 0, NULL, 0, NULL,
  NULL, FALSE, FALSE, 0, 0, 0, 0, {0, 0, 0, 0}, NULL};

#define SUBMENU_COUNT  5 // MainMenu with SUB total

typedef enum {
  mBootArgs,
  mConfigs,
  mThemes,

  // DSDT
  mDSDTName,
  mDebugDSDT,
  mDSDTFix,

  // Tables
  mDropOEM,

  // Devices
  mInjectNVidia,
  mInjectATI,
  mInjectIntel,
  mInjectEDID,
  mInjectEFIStrings,
  mNoDefaultProperties,
  mLoadVBios,

  // Patch
  mKextPatchesAllowed,
  mKernelPatchesAllowed,
  mKPKernelPm,
  mKPAsusAICPUPM,

  // OTHER
  mFlagsBits,
  mOptionsBits,
} OPT_MENU_K;

typedef struct {
  OPT_MENU_K  ID;
  CHAR16      *Title;
} OPT_MENU_GEN;

OPT_MENU_GEN OPT_MENU_DEVICES[] = {
  { mInjectNVidia,          L"Inject NVidia"       },
  { mInjectATI,             L"Inject ATI"          },
  { mInjectIntel,           L"Inject Intel"        },
  { mInjectEDID,            L"Inject EDID"         },
  { mInjectEFIStrings,      L"Inject EFIStrings"   },
  { mNoDefaultProperties,   L"NoDefaultProperties" },
  { mLoadVBios,             L"LoadVBios"           },
};

INTN    OptMenuDevicesNum = ARRAY_SIZE(OPT_MENU_DEVICES);

OPT_MENU_GEN OPT_MENU_PATCHES[] = {
  { mKextPatchesAllowed,    L"Allow Kext Patch"   },
  { mKernelPatchesAllowed,  L"Allow Kernel Patch" },
  { mKPKernelPm,            L"KernelPM Patch"     },
  { mKPAsusAICPUPM,         L"AsusAICPUPM Patch"  },
};

INTN    OptMenuPatchesNum = ARRAY_SIZE(OPT_MENU_PATCHES);

OPT_MENU_GEN OPT_MENU_DSDT[] = {
  { mDSDTName,              L"DSDT name"  },
  { mDebugDSDT,             L"Debug DSDT" },
  { mDSDTFix,               NULL          },
};

INTN    OptMenuDSDTFixesNum = ARRAY_SIZE(OPT_MENU_DSDT);

OPT_MENU_GEN OPT_MENU_TABLE[] = {
  { mDropOEM,               L"Drop all OEM SSDT" },
};

INTN    OptMenuTableNum = ARRAY_SIZE(OPT_MENU_TABLE);

typedef struct {
  CHAR16  *Title;
  CHAR16  *Args;
  UINTN   Bit;
  INTN    OsType;
} OPT_MENU_OPTBIT_K;

OPT_MENU_OPTBIT_K OPT_MENU_OPTBIT[] = {
  { L"Verbose",       L"-v",          OPT_VERBOSE,        OSTYPE_OSX },
  { L"Single User",   L"-s",          OPT_SINGLE_USER,    OSTYPE_OSX },
  { L"Safe Mode",     L"-x",          OPT_SAFE,           OSTYPE_OSX },
  { L"Quiet",         L"quiet",       OPT_QUIET,          OSTYPE_LIN },
  { L"Splash",        L"splash",      OPT_SPLASH,         OSTYPE_LIN },
  { L"Nomodeset",     L"nomodeset",   OPT_NOMODESET,      OSTYPE_LIN },
  { L"Verbose",       L"-v",          OPT_VERBOSE,        OSTYPE_WIN },
  { L"Single User",   L"-s",          OPT_SINGLE_USER,    OSTYPE_WIN },
  { L"Hard Disk",     L"-h",          OPT_HDD,            OSTYPE_WIN },
  { L"CD-ROM",        L"-c",          OPT_CDROM,          OSTYPE_WIN },
};

INTN    OptMenuOptBitNum = ARRAY_SIZE(OPT_MENU_OPTBIT);

OPT_MENU_OPTBIT_K OPT_MENU_FLAGBIT[] = {
  { L"Hibernate wake",        NULL,   OSFLAG_HIBERNATED,  OSTYPE_OSX },
  { L"Without caches",        NULL,   OSFLAG_NOCACHES,    OSTYPE_OSX },
  { L"With injected kexts",   NULL,   OSFLAG_WITHKEXTS,   OSTYPE_OSX },
  { L"No SIP",                NULL,   OSFLAG_NOSIP,       OSTYPE_OSX },
};

INTN    OptMenuFlagBitNum = ARRAY_SIZE(OPT_MENU_FLAGBIT);

BOOLEAN     SubInjectNVidia = FALSE,
            SubInjectATI = FALSE,
            SubInjectIntel = FALSE,
            SubInjectX3 = FALSE;

INTN          OptMenuItemsNum = 0;
INPUT_ITEM    *InputItems = NULL;

INTN GetSubMenuCount(VOID) {
  return SUBMENU_COUNT;
}

REFIT_MENU_ENTRY
*GenMenu (
  REFIT_MENU_SCREEN     *Screen,
  CHAR16                *Title,
  INPUT_ITEM            *Item,
  INTN                  ItemNum,
  UINTN                 Tag,
  UINTN                 Row
) {
  REFIT_INPUT_DIALOG *InputBootArgs = AllocateZeroPool(sizeof(REFIT_INPUT_DIALOG));

  InputBootArgs->Entry.Title = PoolPrint(L"%s", Title);
  InputBootArgs->Entry.Tag = Tag;
  InputBootArgs->Entry.Row = Row;
  InputBootArgs->Item = Item ? Item : &InputItems[ItemNum];
  InputBootArgs->Item->ID = ItemNum;

  AddMenuEntry(Screen, (REFIT_MENU_ENTRY*)InputBootArgs);

  return (REFIT_MENU_ENTRY*)InputBootArgs;
}

REFIT_MENU_ENTRY
*AddMenuBOOL (
  REFIT_MENU_SCREEN   *Screen,
  CHAR16              *Title,
  INPUT_ITEM          *Item,
  INTN                ItemNum
) {
  return GenMenu(Screen, Title, Item, ItemNum, TAG_INPUT, 0xFFFF);
}

REFIT_MENU_ENTRY
*AddMenuString (
  REFIT_MENU_SCREEN   *Screen,
  CHAR16              *Title,
  INTN                ItemNum
) {
  return GenMenu(Screen, Title, NULL, ItemNum, TAG_INPUT, StrLen(InputItems[ItemNum].SValue));
}

REFIT_MENU_ENTRY
*AddMenuCheck (
  REFIT_MENU_SCREEN   *Screen,
  CHAR16              *Title,
  UINTN               Bit,
  INTN                ItemNum
) {
  return GenMenu(Screen, Title, NULL, ItemNum, TAG_CHECKBIT, Bit);
}

REFIT_MENU_ENTRY
*AddMenuRadio (
  REFIT_MENU_SCREEN   *Screen,
  CHAR16              *Title,
  INPUT_ITEM          *Item,
  INTN                ItemNum,
  UINTN               Row
) {
  return GenMenu(Screen, Title, Item, ItemNum, TAG_SWITCH, Row);
}

VOID
FillInputRadio (
  UINTN   Index
) {
  InputItems[Index].ItemType = RadioSwitch;
}

VOID
FillInputInt (
  UINTN   Index,
  UINT32  IValue
) {
  InputItems[Index].ItemType = CheckBit;
  InputItems[Index].IValue = IValue;
}

UINT32
ApplyInputInt (
  UINTN   Index
) {
  return InputItems[Index].IValue;
}

VOID
FillInputBool (
  UINTN     Index,
  BOOLEAN   BValue
) {
  InputItems[Index].ItemType = BoolValue;
  InputItems[Index].BValue = BValue;
}

BOOLEAN
ApplyInputBool (
  UINTN   Index
) {
  return InputItems[Index].BValue;
}

VOID
FillInputString (
  UINTN     Index,
  CHAR16    *Item,
  UINTN     Len,
  UINTN     Type,
  BOOLEAN   New
) {
  InputItems[Index].ItemType = Type;
  if (New) {
    InputItems[Index].SValue = AllocateZeroPool(Len * sizeof(CHAR16));
  }
  UnicodeSPrint(InputItems[Index].SValue, Len, L"%s", Item);
}

VOID
ApplyInputString (
  UINTN     Index,
  CHAR16    *Item,
  UINTN     Len
) {
  UnicodeSPrint(Item, sizeof(Item), L"%s", InputItems[Index].SValue);
}

VOID
AddMenuInfo (
  REFIT_MENU_SCREEN   *SubScreen,
  CHAR16              *Label
) {
  REFIT_INPUT_DIALOG    *InputBootArgs = AllocateZeroPool(sizeof(REFIT_INPUT_DIALOG));

  InputBootArgs->Entry.Title = PoolPrint(L"%s", Label);
  InputBootArgs->Entry.Tag = TAG_INFO;
  InputBootArgs->Item = NULL;

  AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY*)InputBootArgs);
}

VOID
AddMenuLabel (
  REFIT_MENU_SCREEN   *SubScreen,
  CHAR16              *Label
) {
  REFIT_INPUT_DIALOG *InputBootArgs = AllocateZeroPool(sizeof(REFIT_INPUT_DIALOG));

  InputBootArgs->Entry.Title = PoolPrint(L"%s", Label);
  InputBootArgs->Entry.Tag = TAG_LABEL;
  InputBootArgs->Item = NULL;

  AddMenuEntry(SubScreen, (REFIT_MENU_ENTRY*)InputBootArgs);
}

VOID
FillInputs (
  BOOLEAN   New
) {
  if (New) {
    OptMenuItemsNum = (
      1 + // mBootArgs
      1 + // mConfigs
      OptMenuDevicesNum +
      OptMenuPatchesNum +
      OptMenuDSDTFixesNum +
      OptMenuTableNum +
      ACPIDropTablesNum +
      ACPIPatchedAMLNum +
      1 + // mThemes
      1 + // mOptionsBits
      1 + // mFlagsBits
      0
    );

    InputItems = AllocateZeroPool(OptMenuItemsNum * sizeof(INPUT_ITEM));
  }

  FillInputString(mBootArgs, PoolPrint(L"%a", gSettings.BootArgs), SVALUE_MAX_SIZE, ASString, New);

  FillInputRadio(mConfigs);
  FillInputRadio(mThemes);

  // Tables
  FillInputBool(mDropOEM, gSettings.DropSSDT);

  // DSDT
  FillInputString(mDSDTName, gSettings.DsdtName, 32, UNIString, New);
  FillInputBool(mDebugDSDT, gSettings.DebugDSDT);
  FillInputInt(mDSDTFix, gSettings.FixDsdt);

  // Devices
  FillInputBool(mInjectNVidia, gSettings.InjectNVidia);
  FillInputBool(mInjectATI, gSettings.InjectATI);
  FillInputBool(mInjectIntel, gSettings.InjectIntel);
  FillInputBool(mInjectEDID, gSettings.InjectEDID);
  FillInputBool(mInjectEFIStrings, gSettings.StringInjector);
  FillInputBool(mNoDefaultProperties, gSettings.NoDefaultProperties);
  FillInputBool(mLoadVBios, gSettings.LoadVBios);

  // Patch
  FillInputBool(mKextPatchesAllowed, gSettings.KextPatchesAllowed);
  FillInputBool(mKernelPatchesAllowed, gSettings.KernelPatchesAllowed);
  FillInputBool(mKPKernelPm, gSettings.KernelAndKextPatches.KPKernelPm);
  FillInputBool(mKPAsusAICPUPM, gSettings.KernelAndKextPatches.KPAsusAICPUPM);

  // OTHER
  FillInputInt(mOptionsBits, gSettings.OptionsBits);
  FillInputInt(mFlagsBits, gSettings.FlagsBits);
}

VOID
ApplyInputs () {
  EFI_STATUS    Status = EFI_NOT_FOUND;
  BOOLEAN       NeedSave = TRUE;
  CHAR16        *ch;
  TagPtr        dict;
  INTN          i = 0;

  for (i = 0; i < OptMenuItemsNum; i++) {
    if (!InputItems[i].Valid) {
      continue;
    }

    switch(i) {
      case mBootArgs:
        ZeroMem(&gSettings.BootArgs, AVALUE_MAX_SIZE);
        gBootChanged = TRUE;
        ch = InputItems[i].SValue;

        do {
          if (*ch == L'\\') {
            *ch = L'_';
          }
        } while (*(++ch));

        AsciiSPrint(gSettings.BootArgs, AVALUE_MAX_SIZE-1, "%s ", InputItems[i].SValue);
        break;

      case mConfigs:
        if (aConfigs) {
          S_FILES    *aTmp = aConfigs;

          while (aTmp) {
            if (
              (StriCmp(gSettings.ConfigName, aTmp->FileName) != 0) &&
              (aTmp->Index == OldChosenConfig)
            ) {
              gBootChanged = gThemeChanged = TRUE;

              Status = LoadUserSettings(SelfRootDir, aTmp->FileName, &dict);
              if (!EFI_ERROR(Status)) {
                GetUserSettings(SelfRootDir, dict);

                if (gSettings.ConfigName) {
                  FreePool(gSettings.ConfigName);
                }

                gSettings.ConfigName = PoolPrint(aTmp->FileName);
              }

              DBG("Main settings custom (%s.plist) from menu: %r\n", aTmp->FileName, Status);

              FillInputs(FALSE);
              NeedSave = FALSE;

              break;
            }

            aTmp = aTmp->Next;
          }
        }
        break;

      case mThemes:
        if (GlobalConfig.Theme) {
          FreePool(GlobalConfig.Theme);
        }

        if (aThemes) {
          S_FILES    *aTmp = aThemes;

          while (aTmp) {
            if (aTmp->Index == OldChosenTheme) {
              GlobalConfig.Theme = PoolPrint(aTmp->FileName);
              gThemeChanged = TRUE;
              break;
            }

            aTmp = aTmp->Next;
          }
        }

        //will change theme after ESC
        //gThemeChanged = TRUE;
        break;

      // Tables
      case mDropOEM:
        //gSettings.DropSSDT = InputItems[i].BValue;
        gSettings.DropSSDT = ApplyInputBool(i);
        break;

      // DSDT
      case mDSDTName:
        ApplyInputString(mDSDTName, gSettings.DsdtName, sizeof(gSettings.DsdtName));
        break;

      case mDebugDSDT:
        gSettings.DebugDSDT = ApplyInputBool(i);
        break;

      case mDSDTFix:
        gSettings.FixDsdt = ApplyInputInt(i);
        break;

      // Devices
      case mInjectNVidia:
        gSettings.InjectNVidia = ApplyInputBool(i);
        break;

      case mInjectATI:
        gSettings.InjectATI = ApplyInputBool(i);
        break;

      case mInjectIntel:
        gSettings.InjectIntel = ApplyInputBool(i);
        break;

      case mInjectEDID:
        gSettings.InjectEDID = ApplyInputBool(i);
        break;

      case mInjectEFIStrings:
        gSettings.StringInjector = ApplyInputBool(i);
        break;

      case mNoDefaultProperties:
        gSettings.NoDefaultProperties = ApplyInputBool(i);
        break;

      case mLoadVBios:
        gSettings.LoadVBios = ApplyInputBool(i);
        break;

      // Patch
      case mKextPatchesAllowed:
        gSettings.KextPatchesAllowed = ApplyInputBool(i);
        break;

      case mKernelPatchesAllowed:
        gSettings.KernelPatchesAllowed = ApplyInputBool(i);
        break;

      case mKPKernelPm:
        gSettings.KernelAndKextPatches.KPKernelPm = ApplyInputBool(i);
        break;

      case mKPAsusAICPUPM:
        gSettings.KernelAndKextPatches.KPAsusAICPUPM = ApplyInputBool(i);
        break;

      // OTHER
      case mOptionsBits:
        gSettings.OptionsBits = ApplyInputInt(i);
        break;

      case mFlagsBits:
        gSettings.FlagsBits = ApplyInputInt(i);
        break;

      default:
        //if (InputItems[i].ItemType == BoolValue) {
        //  gSettings.DropSSDT = InputItems[i].BValue;
        //}
        break;
    }
  }

  if (NeedSave) {
    SaveSettings();
  }
}

VOID
AddSeparator (
  REFIT_MENU_SCREEN   *SubScreen,
  CHAR8               *Label
) {
  /*
    INTN    len = 40 - AsciiStrSize(Label);
    CHAR8   *fill = AllocateZeroPool(len);
    CHAR16  *Str;

    if (!Label) {
      Str = L"";
      goto FINISH;
    }

    SetMem(fill, len, '=');
    fill[len] = '\0';
    Str = PoolPrint(L"=== [ %a ] %a", Label, fill);
    FreePool(fill);

    FINISH:

    AddMenuLabel(SubScreen, Str);
    FreePool(Str);
  */

  //AddMenuLabel(SubScreen, Label ? PoolPrint(L"=== %a ===", Label) : L"");
  AddMenuLabel(SubScreen, Label ? PoolPrint(L"** %a:", Label) : L"");
}

VOID
AddOptionEntries (
  REFIT_MENU_SCREEN   *SubScreen,
  LOADER_ENTRY        *SubEntry
) {
  BOOLEAN     FlagsExists = FALSE;
  INTN        i, OsType;

  if (OSTYPE_IS_OSX_GLOB(SubEntry->LoaderType)) {
    OsType = OSTYPE_OSX;
  } else if (OSTYPE_IS_WINDOWS_GLOB(SubEntry->LoaderType)) {
    OsType = OSTYPE_WIN;
  } else if  (OSTYPE_IS_LINUX_GLOB(SubEntry->LoaderType)) {
    OsType = OSTYPE_LIN;
  } else {
    return;
  }

  for (i = 0; i < OptMenuFlagBitNum; i++) {
    if (OPT_MENU_FLAGBIT[i].OsType != OsType) {
      continue;
    }

    AddMenuCheck(SubScreen, OPT_MENU_FLAGBIT[i].Title, OPT_MENU_FLAGBIT[i].Bit, mFlagsBits);

    gSettings.FlagsBits = OSFLAG_ISSET(SubEntry->Flags, OPT_MENU_FLAGBIT[i].Bit)
      ? OSFLAG_SET(gSettings.FlagsBits, OPT_MENU_FLAGBIT[i].Bit)
      : OSFLAG_UNSET(gSettings.FlagsBits, OPT_MENU_FLAGBIT[i].Bit);

    FlagsExists = TRUE;
  }

  if (FlagsExists) {
    FillInputInt(mFlagsBits, gSettings.FlagsBits);
    AddSeparator(SubScreen, "boot-args");
  }

  for (i = 0; i < OptMenuOptBitNum; i++) {
    if (OPT_MENU_OPTBIT[i].OsType != OsType) {
      continue;
    }

    AddMenuCheck(
      SubScreen,
      PoolPrint(L"%s (%s)", OPT_MENU_OPTBIT[i].Title, OPT_MENU_OPTBIT[i].Args),
      OPT_MENU_OPTBIT[i].Bit,
      mOptionsBits
    );
  }
}

VOID
DrawFuncIcons () {
  //we should never exclude them
  //if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_FUNCS)) {
    MenuEntryOptions.Image = BuiltinIcon(BUILTIN_ICON_FUNC_OPTIONS);
    MenuEntryOptions.ImageHover = GetSmallHover(BUILTIN_ICON_FUNC_OPTIONS);
    AddMenuEntry(&MainMenu, &MenuEntryOptions);
    MenuEntryAbout.Image = BuiltinIcon(BUILTIN_ICON_FUNC_ABOUT);
    MenuEntryAbout.ImageHover = GetSmallHover(BUILTIN_ICON_FUNC_ABOUT);
    AddMenuEntry(&MainMenu, &MenuEntryAbout);
  //}

  if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_FUNCS) || (MainMenu.EntryCount == 0)) {
    // We are able to include "real" help button in 2nd row, but confuse how to activate it via theme.plist
    // Check BOOLEAN "Help" entry (HIDEUI_FLAG_HELP) currently reserved for help text on bottom corner.
    // Previously HELP is for ABOUT. Same as SHUTDOWN as EXIT.
    if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_HELP)) {
      MenuEntryHelp.Image = BuiltinIcon(BUILTIN_ICON_FUNC_HELP);
      MenuEntryHelp.ImageHover = GetSmallHover(BUILTIN_ICON_FUNC_HELP);
      AddMenuEntry(&MainMenu, &MenuEntryHelp);
    }

    MenuEntryReset.Image = BuiltinIcon(BUILTIN_ICON_FUNC_RESET);
    MenuEntryReset.ImageHover = GetSmallHover(BUILTIN_ICON_FUNC_RESET);
    AddMenuEntry(&MainMenu, &MenuEntryReset);
    MenuEntryExit.Image = BuiltinIcon(BUILTIN_ICON_FUNC_EXIT);
    MenuEntryExit.ImageHover = GetSmallHover(BUILTIN_ICON_FUNC_EXIT);
    AddMenuEntry(&MainMenu, &MenuEntryExit);
  }
}

VOID
AboutRefit () {
  if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_MENU_TITLE_IMAGE)) {
    AboutMenu.TitleImage = BuiltinIcon(BUILTIN_ICON_FUNC_ABOUT);
  } else {
    AboutMenu.TitleImage = NULL;
  }

  if (AboutMenu.EntryCount == 0) {
    AddMenuInfo(&AboutMenu, PoolPrint(L"Clover Version 2.3k rev %s", FIRMWARE_REVISION));
#ifdef FIRMWARE_BUILDDATE
    AddMenuInfo(&AboutMenu, PoolPrint(L" Build: %a", FIRMWARE_BUILDDATE));
#else
    AddMenuInfo(&AboutMenu, L" Build: unknown");
#endif
    //AddMenuInfo(&AboutMenu, L"");
    AddSeparator(&AboutMenu, NULL);
    AddMenuInfo(&AboutMenu, L"Based on rEFIt (c) 2006-2010 Christoph Pfisterer");
    AddMenuInfo(&AboutMenu, L"Portions Copyright (c) Intel Corporation");
    AddMenuInfo(&AboutMenu, L"Developers:");
    AddMenuInfo(&AboutMenu, L"  Slice, dmazar, apianti, JrCs, pene, usrsse2");
    AddMenuInfo(&AboutMenu, L"Credits also:");
    AddMenuInfo(&AboutMenu, L"  Kabyl, pcj, jadran, Blackosx, STLVNUB, ycr.ru");
    AddMenuInfo(&AboutMenu, L"  FrodoKenny, skoczi, crazybirdy, Oscar09, xsmile");
    AddMenuInfo(&AboutMenu, L"  cparm, rehabman, nms42, sherlocks, Zenith432");
    AddMenuInfo(&AboutMenu, L"  stinga11, TheRacerMaster, solstice, SoThOr, DF");
    AddMenuInfo(&AboutMenu, L"  cecekpawon, Micky1979, Needy, joevt");
    AddMenuInfo(&AboutMenu, L"  projectosx.com, applelife.ru, insanelymac.com");
    //AddMenuInfo(&AboutMenu, L"");
    AddSeparator(&AboutMenu, NULL);
    AddMenuInfo(&AboutMenu, L"Running on:");
    AddMenuInfo(&AboutMenu, PoolPrint(L" EFI Revision %d.%02d",
                              gST->Hdr.Revision >> 16, gST->Hdr.Revision & ((1 << 16) - 1))
                            );
    AddMenuInfo(&AboutMenu, L" Platform: x86_64 (64 bit)");
    AddMenuInfo(&AboutMenu, PoolPrint(L" Firmware: %s rev %d.%d",
                              gST->FirmwareVendor,
                              gST->FirmwareRevision >> 16,
                              gST->FirmwareRevision & ((1 << 16) - 1)
                            ));
    AddMenuInfo(&AboutMenu, PoolPrint(L" Screen Output: %s", egScreenDescription()));
    AboutMenu.AnimeRun = GetAnime(&AboutMenu);
    //AddMenuEntry(&AboutMenu, &MenuEntryReturn);
  } else if (AboutMenu.EntryCount >= 2) {
    FreePool(AboutMenu.Entries[AboutMenu.EntryCount-2]->Title);
    AboutMenu.Entries[AboutMenu.EntryCount-2]->Title = PoolPrint(L" Screen Output: %s", egScreenDescription());
  }

  RunMenu(&AboutMenu, NULL);
}

VOID HelpRefit() {
  if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_MENU_TITLE_IMAGE)) {
    HelpMenu.TitleImage = BuiltinIcon(BUILTIN_ICON_FUNC_HELP);
  } else {
    HelpMenu.TitleImage = NULL;
  }

  if (HelpMenu.EntryCount == 0) {
    CHAR16    *PathOrigin = PoolPrint(DIR_ACPI_ORIGIN, OEMPath);

    switch (gLanguage) {
      case english:
      default:
        AddMenuInfo(&HelpMenu, L"ESC - Escape from submenu, Refresh main menu");
        AddMenuInfo(&HelpMenu, L"F1  - This help");
        AddMenuInfo(&HelpMenu, PoolPrint(L"F2  - Save '%s' into '%s'", Basename(PREBOOT_LOG), DIR_MISC));
        AddMenuInfo(&HelpMenu, L"F3  - Show hidden entries");
        AddMenuInfo(&HelpMenu, PoolPrint(L"F4  - Save oem DSDT into '%s'", PathOrigin));
        AddMenuInfo(&HelpMenu, PoolPrint(L"F5  - Save patched DSDT into '%s'", PathOrigin));
        AddMenuInfo(&HelpMenu, PoolPrint(L"F6  - Save VideoBios into '%s'", DIR_MISC));
        AddMenuInfo(&HelpMenu, PoolPrint(L"F10 - Save screenshot into '%s'", DIR_MISC));
        AddMenuInfo(&HelpMenu, L"F12 - Eject selected volume (DVD)");
        AddMenuInfo(&HelpMenu, L"Space - Details about selected menu entry");
        AddMenuInfo(&HelpMenu, L"Digits [1-9] - Shortcut to menu entry");
        AddMenuInfo(&HelpMenu, L"A - Menu About");
        AddMenuInfo(&HelpMenu, L"O - Menu Options");
        AddMenuInfo(&HelpMenu, L"R - Soft Reset");
        AddMenuInfo(&HelpMenu, L"U - Exit");
        break;
    }

    //HelpMenu.AnimeRun = GetAnime(&HelpMenu);
    //AddMenuEntry(&HelpMenu, &MenuEntryReturn);

    FreePool(PathOrigin);
  }

  RunMenu(&HelpMenu, NULL);
}

//
// Scrolling functions
//

STATIC
VOID
InitScroll (
  OUT SCROLL_STATE    *State,
  IN INTN             ItemCount,
  IN UINTN            MaxCount,
  IN UINTN            VisibleSpace,
  IN INTN             Selected // 0xFFFF -> 0: Label, 1: Current
) {
  //ItemCount - a number to scroll (Row0)
  //MaxCount - total number (Row0 + Row1)
  //VisibleSpace - a number to fit
  // DBG("InitScroll <= %d %d %d\n", ItemCount, MaxCount, VisibleSpace);
  // main menu  <= 2 8 5
  // about menu <= 21 21 14
  State->LastSelection = State->CurrentSelection = Selected;
  //MaxIndex, MaxScroll, MaxVisible are indexes, 0..N-1
  State->MaxIndex = (INTN)MaxCount - 1;
  State->MaxScroll = ItemCount - 1;

  if (VisibleSpace == 0) {
    State->MaxVisible = State->MaxScroll;
  } else {
    State->MaxVisible = (INTN)VisibleSpace - 1;
  }

  if (State->MaxVisible >= ItemCount) {
    State->MaxVisible = ItemCount - 1;
  }

  State->MaxFirstVisible = State->MaxScroll - State->MaxVisible;
  CONSTRAIN_MIN(State->MaxFirstVisible, 0);

  if (Selected == 0xFFFF) {
    State->FirstVisible = 0;
    State->LastSelection = State->CurrentSelection = 1;
    //Selected = 1;
  } else {
    State->FirstVisible = MIN(Selected, State->MaxFirstVisible);
  }
  //State->FirstVisible = (Selected > State->MaxFirstVisible)?State->MaxFirstVisible:Selected;

  State->IsScrolling = (State->MaxFirstVisible > 0);
  State->PaintAll = TRUE;
  State->PaintSelection = FALSE;

  State->LastVisible = State->FirstVisible + State->MaxVisible;
  //  DBG("InitScroll => MaxIndex=%d, FirstVisible=%d, MaxVisible=%d, MaxFirstVisible=%d\n",
  //      State->MaxIndex, State->FirstVisible, State->MaxVisible, State->MaxFirstVisible);
  // main menu
  // => MaxIndex=7, FirstVisible=0, MaxVisible=1, MaxFirstVisible=0
  //  about
  // => MaxIndex=20, FirstVisible=0, MaxVisible=13, MaxFirstVisible=7
}

STATIC
VOID
UpdateScroll (
  IN OUT  SCROLL_STATE  *State,
  IN      UINTN         Movement
) {
  State->LastSelection = State->CurrentSelection;
  //DBG("UpdateScroll on %d\n", Movement);

  switch (Movement) {
    case SCROLL_LINE_UP: //of left = decrement
      if (State->CurrentSelection > 0) {
        State->CurrentSelection--;

        if (State->CurrentSelection < State->FirstVisible) {
          State->PaintAll = TRUE;
          State->FirstVisible = State->CurrentSelection;
        }

        if (State->CurrentSelection == State->MaxScroll) {
          State->PaintAll = TRUE;
        }

        if (
          (State->CurrentSelection < State->MaxScroll) &&
          (State->CurrentSelection > State->LastVisible)
        ) {
          State->PaintAll = TRUE;
          State->LastVisible = State->CurrentSelection;
          State->FirstVisible = State->LastVisible - State->MaxVisible;
        }
      } else if (State->ScrollMode == SCROLL_MODE_LOOP) {
        State->CurrentSelection = State->FirstVisible = 0;
        UpdateScroll(State, SCROLL_LAST);
      }
      break;

    case SCROLL_LINE_DOWN: //or right -- increment
      if (State->CurrentSelection < State->MaxIndex) {
        State->CurrentSelection++;

        if (
          (State->CurrentSelection > State->LastVisible) &&
          (State->CurrentSelection <= State->MaxScroll)
        ) {
          State->PaintAll = TRUE;
          State->FirstVisible++;
          CONSTRAIN_MAX(State->FirstVisible, State->MaxFirstVisible);
        }
        //DBG("SCROLL_LINE_DOWN\n");
      } else if (State->ScrollMode == SCROLL_MODE_LOOP) {
        State->CurrentSelection = State->FirstVisible = State->MaxIndex;
        UpdateScroll(State, SCROLL_FIRST);
      }
      break;

    case SCROLL_PAGE_UP:
      if (State->CurrentSelection > 0) {
        if (State->CurrentSelection == State->MaxIndex) { // currently at last entry, special treatment
          if (State->IsScrolling) {
            State->CurrentSelection -= State->MaxVisible - 1; // move to second line without scrolling
          } else {
            State->CurrentSelection = 0; // move to first entry
          }
        } else {
          if (State->FirstVisible > 0) {
            State->PaintAll = TRUE;
          }

          State->CurrentSelection -= State->MaxVisible; // move one page and scroll synchronously
          State->FirstVisible -= State->MaxVisible;
        }

        CONSTRAIN_MIN(State->CurrentSelection, 0);
        CONSTRAIN_MIN(State->FirstVisible, 0);

        if (State->CurrentSelection < State->FirstVisible) {
          State->PaintAll = TRUE;
          State->FirstVisible = State->CurrentSelection;
        }
      }
      break;

    case SCROLL_PAGE_DOWN:
      if (State->CurrentSelection < State->MaxIndex) {
        if (State->CurrentSelection == 0) { // currently at first entry, special treatment
          if (State->IsScrolling) {
            State->CurrentSelection += State->MaxVisible - 1; // move to second-to-last line without scrolling
          } else {
            State->CurrentSelection = State->MaxIndex; // move to last entry
          }
        } else {
          if (State->FirstVisible < State->MaxFirstVisible) {
            State->PaintAll = TRUE;
          }

          State->CurrentSelection += State->MaxVisible; // move one page and scroll synchronously
          State->FirstVisible += State->MaxVisible;
        }

        CONSTRAIN_MAX(State->CurrentSelection, State->MaxIndex);
        CONSTRAIN_MAX(State->FirstVisible, State->MaxFirstVisible);

        if (
          (State->CurrentSelection > State->LastVisible) &&
          (State->CurrentSelection <= State->MaxScroll)
        ) {
          State->PaintAll = TRUE;
          State->FirstVisible+= State->MaxVisible;
          CONSTRAIN_MAX(State->FirstVisible, State->MaxFirstVisible);
        }
      }
      break;

    case SCROLL_FIRST:
      if (State->CurrentSelection > 0) {
        State->CurrentSelection = 0;

        if (State->FirstVisible > 0) {
          State->PaintAll = TRUE;
          State->FirstVisible = 0;
        }
      }
      break;

    case SCROLL_LAST:
      if (State->CurrentSelection < State->MaxIndex) {
        State->CurrentSelection = State->MaxIndex;

        if (State->FirstVisible < State->MaxFirstVisible) {
          State->PaintAll = TRUE;
          State->FirstVisible = State->MaxFirstVisible;
        }
      }
      break;

    case SCROLL_NONE:
      // The caller has already updated CurrentSelection, but we may
      // have to scroll to make it visible.
      if (State->CurrentSelection < State->FirstVisible) {
        State->PaintAll = TRUE;
        State->FirstVisible = State->CurrentSelection; // - (State->MaxVisible >> 1);
        CONSTRAIN_MIN(State->FirstVisible, 0);
      } else if (
        (State->CurrentSelection > State->LastVisible) &&
        (State->CurrentSelection <= State->MaxScroll)
      ) {
        State->PaintAll = TRUE;
        State->FirstVisible = State->CurrentSelection - State->MaxVisible;
        CONSTRAIN_MAX(State->FirstVisible, State->MaxFirstVisible);
      }
      break;
  }

  if (!State->PaintAll && (State->CurrentSelection != State->LastSelection)) {
    State->PaintSelection = TRUE;
  }

  State->LastVisible = State->FirstVisible + State->MaxVisible;
}

//
// menu helper functions
//

UINT32
EncodeOptions (
  CHAR16  *Options
) {
  INTN    i;
  UINT32  OptionsBits = 0;

  for (i = 0; i < OptMenuOptBitNum; i++) {
    if (BootArgsExists(Options, OPT_MENU_OPTBIT[i].Args)) {
      //OptionsBits |= OPT_MENU_OPTBIT[i].Bit;
      OptionsBits = OSFLAG_SET(OptionsBits, OPT_MENU_OPTBIT[i].Bit);
    }
  }

  return OptionsBits;
}

VOID
DecodeOptions (
  LOADER_ENTRY    *Entry
) {
  INTN    i, OsType;

  if (OSTYPE_IS_OSX_GLOB(Entry->LoaderType)) {
    OsType = OSTYPE_OSX;
  } else if (OSTYPE_IS_WINDOWS_GLOB(Entry->LoaderType)) {
    OsType = OSTYPE_WIN;
  } else if  (OSTYPE_IS_LINUX_GLOB(Entry->LoaderType)) {
    OsType = OSTYPE_LIN;
  } else {
    return;
  }

  for (i = 0; i < OptMenuOptBitNum; i++) {
    if (OPT_MENU_OPTBIT[i].OsType != OsType) {
      continue;
    }

    Entry->LoadOptions = ToggleLoadOptions(
                            (gSettings.OptionsBits & OPT_MENU_OPTBIT[i].Bit),
                            Entry->LoadOptions,
                            OPT_MENU_OPTBIT[i].Args
                          );
  }
}

VOID
AddMenuInfoLine (
  IN REFIT_MENU_SCREEN  *Screen,
  IN CHAR16             *InfoLine
) {
  AddListElement((VOID ***) &(Screen->InfoLines), (UINTN*)&(Screen->InfoLineCount), InfoLine);
}

VOID
AddMenuEntry (
  IN REFIT_MENU_SCREEN  *Screen,
  IN REFIT_MENU_ENTRY   *Entry
) {
  AddListElement((VOID ***) &(Screen->Entries), (UINTN*)&(Screen->EntryCount), Entry);
}

VOID
SplitInfoLine (
  IN REFIT_MENU_SCREEN  *SubScreen,
  IN CHAR16             *Str
) {
  CHAR16    *TmpStr;
  INTN      //i = 0,
            currentLen = 0,
            stringLen = StrLen(Str),
            maxWidth = LAYOUT_TEXT_WIDTH/*(UGAWidth >> 1)*/ / GlobalConfig.CharWidth;
            //maxWidth = (644 - (TEXT_XMARGIN*2)) / GlobalConfig.CharWidth;
            //maxWidth = (UGAWidth - TITLEICON_SPACING -
              //(SubScreen->TitleImage ? SubScreen->TitleImage->Width : 0) - 2) / GlobalConfig.CharWidth;

  //DBG("### CharWidth: %d | maxWidth: %d | %s\n", GlobalConfig.CharWidth, maxWidth, Str);

  while (stringLen > currentLen) {
    INTN      TmpLen = maxWidth;

    CONSTRAIN_MAX(TmpLen, (stringLen - currentLen));

    if (!TmpLen) break;

    TmpStr = AllocateZeroPool(TmpLen);
    *TmpStr = '\0';
    StrnCat(TmpStr, Str + currentLen, TmpLen);
    //DBG("### %d: %s | Start -> %d | End -> %d\n", i++, TmpStr, currentLen, TmpLen);
    AddMenuInfoLine(SubScreen, PoolPrint(L"%a%s", currentLen ? "  " : "", TmpStr));

    FreePool(TmpStr);

    currentLen += TmpLen;
  }
}

VOID
FreeMenu (
  IN REFIT_MENU_SCREEN    *Screen
) {
  INTN                i;
  REFIT_MENU_ENTRY    *Tentry = NULL;

  //TODO - here we must FreePool for a list of Entries, Screens, InputBootArgs
  if (Screen->EntryCount > 0) {
    for (i = 0; i < Screen->EntryCount; i++) {
      Tentry = Screen->Entries[i];
      if (Tentry->SubScreen) {
        if (Tentry->SubScreen->Title) {
          FreePool(Tentry->SubScreen->Title);
          Tentry->SubScreen->Title = NULL;
        }

        // don't free image because of reusing them
        //egFreeImage(Tentry->SubScreen->Image);
        FreeMenu(Tentry->SubScreen);
        Tentry->SubScreen = NULL;
      }

      if (Tentry->Tag != TAG_RETURN) { //can't free constants
        if (Tentry->Title) {
          FreePool(Tentry->Title);
          Tentry->Title = NULL;
        }
      }
      FreePool(Tentry);
    }

    Screen->EntryCount = 0;
    FreePool(Screen->Entries);
    Screen->Entries = NULL;
  }

  if (Screen->InfoLineCount > 0) {
    for (i = 0; i < Screen->InfoLineCount; i++) {
      // TODO: call a user-provided routine for each element here
      FreePool(Screen->InfoLines[i]);
    }

    Screen->InfoLineCount = 0;
    FreePool(Screen->InfoLines);
    Screen->InfoLines = NULL;
  }
}

STATIC
INTN
FindMenuShortcutEntry (
  IN REFIT_MENU_SCREEN    *Screen,
  IN CHAR16               Shortcut
) {
  INTN i;

  if (Shortcut >= 'a' && Shortcut <= 'z') {
    Shortcut -= ('a' - 'A');
  }

  if (Shortcut) {
    for (i = 0; i < Screen->EntryCount; i++) {
      if (
        Screen->Entries[i]->ShortcutDigit == Shortcut ||
        Screen->Entries[i]->ShortcutLetter == Shortcut
      ) {
        return i;
      }
    }
  }

  return -1;
}

//
// generic input menu function
// usr-sse2
//
STATIC
UINTN
InputDialog (
  IN REFIT_MENU_SCREEN    *Screen,
  IN MENU_STYLE_FUNC      StyleFunc,
  IN SCROLL_STATE         *State
) {
  EFI_STATUS        Status;
  EFI_INPUT_KEY     key;
  UINTN             ind = 0,
                    i = 0,
                    MenuExit = 0,
                    //LogSize,
                    Pos = (Screen->Entries[State->CurrentSelection])->Row,
                    BackupPos,
                    BackupShift;
  INPUT_ITEM        *Item = ((REFIT_INPUT_DIALOG*)(Screen->Entries[State->CurrentSelection]))->Item;
  CHAR16            *Backup = EfiStrDuplicate(Item->SValue),
                    *Buffer;
  //SCROLL_STATE    StateLine;

  /*
    I would like to see a LineSize that depends on the Title width and the menu width so
    the edit dialog does not extend beyond the menu width.
    There are 3 cases:
    1) Text menu where MenuWidth is min of ConWidth - 6 and max of 50 and all StrLen(Title)
    2) Graphics menu where MenuWidth is measured in pixels and font is fixed width.
       The following works well in my case but depends on font width and minimum screen size.
         LineSize = 76 - StrLen(Screen->Entries[State->CurrentSelection]->Title);
    3) Graphics menu where font is proportional. In this case LineSize would depend on the
       current width of the displayed string which would need to be recalculated after
       every change.
    Anyway, the above will not be implemented for now, and LineSize will remain at 38
    because it works.
  */

  UINTN             LineSize = 38;
#define DBG_INPUTDIALOG 0
#if DBG_INPUTDIALOG
  UINTN             Iteration = 0;
#endif


  if (
    (Item->ItemType != BoolValue) &&
    (Item->ItemType != RadioSwitch) &&
    (Item->ItemType != CheckBit)
  ) {
    // Grow Item->SValue to SVALUE_MAX_SIZE if we want to edit a text field
    Item->SValue = EfiReallocatePool(Item->SValue, StrSize(Item->SValue), SVALUE_MAX_SIZE);
  }

  Buffer = Item->SValue;
  BackupShift = Item->LineShift;
  BackupPos = Pos;

  do {

    if (Item->ItemType == BoolValue) {
      Item->BValue = !Item->BValue;
      MenuExit = MENU_EXIT_ENTER;
    } else if (Item->ItemType == RadioSwitch) {
      switch (Item->ID) {
        case mThemes:
          OldChosenTheme = Pos;
          break;

        case mConfigs:
          OldChosenConfig = Pos;
          break;
      }

      MenuExit = MENU_EXIT_ENTER;
    } else if (Item->ItemType == CheckBit) {
      Item->IValue ^= Pos;
      MenuExit = MENU_EXIT_ENTER;
    } else {

      Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &key);

#if DBG_INPUTDIALOG
      // For debugging the InputDialog
      PrintAt(0, 0, L"%5d: Buffer:%x MaxSize:%d Line:%3d", Iteration, Buffer, SVALUE_MAX_SIZE, LineSize);
      PrintAt(0, 1, L"%5d: Size:%3d Len:%3d", Iteration, StrSize(Buffer), StrLen(Buffer));
      PrintAt(0, 2, L"%5d: Pos:%3d Shift:%3d AbsPos:%3d", Iteration, Pos, Item->LineShift, Pos+Item->LineShift);
      PrintAt(0, 3, L"%5d: KeyCode:%4d KeyChar:%4d", Iteration, key.ScanCode, (UINTN)key.UnicodeChar);
      PrintAt(0, 4, L"%5d: Title:\"%s\"", Iteration, Screen->Entries[State->CurrentSelection]->Title);
      Iteration++;
#endif

      if (Status == EFI_NOT_READY) {
        gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &ind);
        continue;
      }

      switch (key.ScanCode) {
        case SCAN_RIGHT:
          if (Pos + Item->LineShift < StrLen(Buffer)) {
            if (Pos < LineSize) {
              Pos++;
            } else {
              Item->LineShift++;
            }
          }
          break;

        case SCAN_LEFT:
          if (Pos > 0) {
            Pos--;
          } else if (Item->LineShift > 0) {
            Item->LineShift--;
          }
          break;

        case SCAN_HOME:
          Pos = 0;
          Item->LineShift=0;
          break;

        case SCAN_END:
          if (StrLen(Buffer) < LineSize) {
            Pos = StrLen(Buffer);
          } else {
            Pos = LineSize;
            Item->LineShift = StrLen(Buffer) - LineSize;
          }
          break;

        case SCAN_ESC:
          MenuExit = MENU_EXIT_ESCAPE;
          continue;
          break;

        case SCAN_F2:
          SavePreBootLog = TRUE;
          /*
            Status = SaveBooterLog(SelfRootDir, PREBOOT_LOG);
            if (EFI_ERROR(Status)) {
              Status = SaveBooterLog(NULL, PREBOOT_LOG);
            }
          */
          /*
            LogSize = msgCursor - msgbuf;
            Status = egSaveFile(SelfRootDir, PREBOOT_LOG, (UINT8*)msgbuf, LogSize);
            if (EFI_ERROR(Status)) {
              Status = egSaveFile(NULL, PREBOOT_LOG, (UINT8*)msgbuf, LogSize);
            }
          */
          break;

        case SCAN_F6:
          Status = egSaveFile(SelfRootDir, VBIOS_BIN, (UINT8*)(UINTN)0xc0000, 0x20000);
          if (EFI_ERROR(Status)) {
            Status = egSaveFile(NULL, VBIOS_BIN, (UINT8*)(UINTN)0xc0000, 0x20000);
          }
          break;

        case SCAN_F10:
          egScreenShot();
          break;

        case SCAN_DELETE:
          // forward delete
          if (Pos + Item->LineShift < StrLen(Buffer)) {
            for (i = Pos + Item->LineShift; i < StrLen(Buffer); i++) {
               Buffer[i] = Buffer[i+1];
            }
            /*
            // Commented this out because it looks weird - Forward Delete should not
            // affect anything left of the cursor even if it's just to shift more of the
            // string into view.
            if (Item->LineShift > 0 && Item->LineShift + LineSize > StrLen(Buffer)) {
              Item->LineShift--;
              Pos++;
            }
            */
          }
          break;
      }

      switch (key.UnicodeChar) {
        case CHAR_BACKSPACE:
          if (Buffer[0] != CHAR_NULL && Pos != 0) {
            for (i = Pos + Item->LineShift; i <= StrLen(Buffer); i++) {
               Buffer[i-1] = Buffer[i];
            }
            Item->LineShift > 0 ? Item->LineShift-- : Pos--;
          }
          break;

        case CHAR_LINEFEED:
        case CHAR_CARRIAGE_RETURN:
          MenuExit = MENU_EXIT_ENTER;
          Pos = 0;
          Item->LineShift = 0;
          break;

        default:
          if (
            (key.UnicodeChar >= 0x20) &&
            (key.UnicodeChar < 0x80)
          ){
            if (StrSize(Buffer) < SVALUE_MAX_SIZE) {
              for (i = StrLen(Buffer)+1; i > Pos + Item->LineShift; i--) {
                 Buffer[i] = Buffer[i-1];
              }

              Buffer[i] = key.UnicodeChar;
              Pos < LineSize ? Pos++ : Item->LineShift++;
            }
          }
          break;
      }
    }

    // Redraw the field
    (Screen->Entries[State->CurrentSelection])->Row = Pos;
    StyleFunc(Screen, State, MENU_FUNCTION_PAINT_SELECTION, NULL);
  } while (!MenuExit);

  switch (MenuExit) {
    case MENU_EXIT_ENTER:
      Item->Valid = TRUE;
      ApplyInputs();
      break;

    case MENU_EXIT_ESCAPE:
      if (StrCmp(Item->SValue, Backup) != 0) {
        UnicodeSPrint(Item->SValue, SVALUE_MAX_SIZE, L"%s", Backup);

        if (Item->ItemType != BoolValue) {
          Item->LineShift = BackupShift;
          (Screen->Entries[State->CurrentSelection])->Row = BackupPos;
        }

        StyleFunc(Screen, State, MENU_FUNCTION_PAINT_SELECTION, NULL);
      }
      break;
  }

  Item->Valid = FALSE;
  FreePool(Backup);

  if (Item->SValue) {
    MsgLog("EDITED: %s\n", Item->SValue);
  }

  return 0;
}

VOID
CheckState (
  REFIT_MENU_SCREEN   *Screen,
  SCROLL_STATE        *State,
  UINT16              ScanCode
) {
  INTN      Index = 0;
  BOOLEAN   Allow = FALSE, Down = FALSE;

  switch (ScanCode) {
    case SCAN_UP:
    case SCAN_LEFT:
    //case SCAN_PAGE_UP:
    //case SCAN_HOME:
      Index = State->CurrentSelection - 1;
      CONSTRAIN_MAX(Index, State->MaxIndex);
      Allow = (Index >= 0);
      break;

    case SCAN_DOWN:
    case SCAN_RIGHT:
    //case SCAN_PAGE_DOWN:
    //case SCAN_END:
      Index = State->CurrentSelection + 1;
      CONSTRAIN_MAX(Index, State->MaxIndex);
      Allow = (Index < State->MaxIndex);
      Down = TRUE;
      break;

    default:
      return;
  }

  if (((State->CurrentSelection == State->MaxIndex) && (Screen->Entries[0])->Tag == TAG_LABEL) && Down) {
    State->CurrentSelection = State->FirstVisible = 0;
    State->PaintAll = TRUE;
    //return;
  } else if (((Screen->Entries[Index])->Tag == TAG_LABEL) && Allow) {
    if ((Index == (State->LastVisible + 1)) && Down) {
      State->FirstVisible++;
      #if 0
      DBG("\n\n - [#%d] CurrentSelection=%d, LastSelection=%d, MaxScroll=%d, MaxIndex=%d, \
            FirstVisible=%d, LastVisible=%d, MaxVisible=%d, MaxFirstVisible=%d\n\n",
          Index, State->CurrentSelection, State->LastSelection, State->MaxScroll, State->MaxIndex,
            State->FirstVisible, State->LastVisible, State->MaxVisible, State->MaxFirstVisible
        );
      #endif
    }

    State->CurrentSelection = Index;
    State->PaintAll = TRUE;
  }
}

UINTN
RunGenericMenu (
  IN REFIT_MENU_SCREEN    *Screen,
  IN MENU_STYLE_FUNC      StyleFunc,
  IN OUT INTN             *DefaultEntryIndex,
  OUT REFIT_MENU_ENTRY    **ChosenEntry
) {
  SCROLL_STATE      State;
  EFI_STATUS        Status;
  EFI_INPUT_KEY     key;
  INTN              ShortcutEntry, TimeoutCountdown = 0;
  BOOLEAN           HaveTimeout = FALSE;
  CHAR16            *TimeoutMessage;
  UINTN             MenuExit;

  //no default - no timeout!
  if (
    (*DefaultEntryIndex != -1) &&
    (Screen->TimeoutSeconds > 0)
  ) {
    //DBG("have timeout\n");
    HaveTimeout = TRUE;
    TimeoutCountdown = Screen->TimeoutSeconds;
  }

  MenuExit = 0;

  StyleFunc(Screen, &State, MENU_FUNCTION_INIT, NULL);
  //  DBG("scroll inited\n");
  // override the starting selection with the default index, if any
  if (
    (*DefaultEntryIndex >= 0) &&
    (*DefaultEntryIndex <= State.MaxIndex)
  ) {
    State.CurrentSelection = *DefaultEntryIndex;
    UpdateScroll(&State, SCROLL_NONE);
  }

  //DBG("RunGenericMenu CurrentSelection=%d MenuExit=%d\n",
  //    State.CurrentSelection, MenuExit);

  State.ScrollMode = (GlobalConfig.TextOnly || (Screen->ID > SCREEN_MAIN)) ? SCROLL_MODE_LOOP : SCROLL_MODE_NONE;

  // exhaust key buffer and be sure no key is pressed to prevent option selection
  // when coming with a key press from timeout=0, for example
  while (ReadAllKeyStrokes()) gBS->Stall(500 * 1000);

  while (!MenuExit) {
    // update the screen
    if (State.PaintAll) {
      StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_ALL, NULL);
      State.PaintAll = FALSE;
    } else if (State.PaintSelection) {
      StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_SELECTION, NULL);
      State.PaintSelection = FALSE;
    }

    if (HaveTimeout) {
      TimeoutMessage = PoolPrint(L"%s in %d seconds", Screen->TimeoutText, TimeoutCountdown);
      StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_TIMEOUT, TimeoutMessage);
      FreePool(TimeoutMessage);
    }

    if (gEvent) { //for now used at CD eject.
      MenuExit = MENU_EXIT_ESCAPE;
      State.PaintAll = TRUE;
      gEvent = 0; //to prevent looping
      break;
    }

    key.UnicodeChar = 0;
    key.ScanCode = 0;

    if (!mGuiReady) {
      mGuiReady = TRUE;
      DBG("GUI ready\n");
    }

    Status = WaitForInputEventPoll(Screen, 1); //wait for 1 seconds.

    if (Status == EFI_TIMEOUT) {
      if (HaveTimeout) {
        if (TimeoutCountdown <= 0) {
          // timeout expired
          MenuExit = MENU_EXIT_TIMEOUT;
          break;
        } else {
          //gBS->Stall(100000);
          TimeoutCountdown--;
        }
      }
      continue;
    }

    // read key press (and wait for it if applicable)
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &key);
    //if ((Status == EFI_NOT_READY) && (gAction == ActionNone)) {
    if (Status == EFI_NOT_READY) {
      continue;
    }

    //if (gAction == ActionNone) {
      ReadAllKeyStrokes(); //clean to avoid doubles
    //}

    if (HaveTimeout) {
      // the user pressed a key, cancel the timeout
      StyleFunc(Screen, &State, MENU_FUNCTION_PAINT_TIMEOUT, L"");
      //HidePointer(); //ycr.ru
      HaveTimeout = FALSE;
    }

    //gAction = ActionNone; //do action once
    // react to key press
    switch (key.ScanCode) {
      case SCAN_UP:
      case SCAN_LEFT:
        CheckState(Screen, &State, key.ScanCode);
        UpdateScroll(&State, SCROLL_LINE_UP);
        break;

      case SCAN_DOWN:
      case SCAN_RIGHT:
        CheckState(Screen, &State, key.ScanCode);
        UpdateScroll(&State, SCROLL_LINE_DOWN);
        break;

      case SCAN_HOME:
        UpdateScroll(&State, SCROLL_FIRST);
        break;

      case SCAN_END:
        UpdateScroll(&State, SCROLL_LAST);
        break;

      case SCAN_PAGE_UP:
        UpdateScroll(&State, SCROLL_PAGE_UP);
        StyleFunc(Screen, &State, MENU_FUNCTION_INIT, NULL);
        break;

      case SCAN_PAGE_DOWN:
        UpdateScroll(&State, SCROLL_PAGE_DOWN);
        StyleFunc(Screen, &State, MENU_FUNCTION_INIT, NULL);
        break;

      case SCAN_ESC:
        MenuExit = MENU_EXIT_ESCAPE;
        break;

      case SCAN_INSERT:
        MenuExit = MENU_EXIT_OPTIONS;
        break;

      case SCAN_F1:
        MenuExit = MENU_EXIT_HELP;
        break;

      case SCAN_F2:
        SavePreBootLog = TRUE;
        //let it be twice
        Status = SaveBooterLog(SelfRootDir, PREBOOT_LOG);
        if (EFI_ERROR(Status)) {
          Status = SaveBooterLog(NULL, PREBOOT_LOG);
        }
        break;

      case SCAN_F3:
         MenuExit = MENU_EXIT_HIDE_TOGGLE;
         break;

      case SCAN_F4:
        SaveOemTables();
        break;

      case SCAN_F5:
        SaveOemDsdt(TRUE); //full patch
        break;

      case SCAN_F6:
        Status = egSaveFile(SelfRootDir, VBIOS_BIN, (UINT8*)(UINTN)0xc0000, 0x20000);
        if (EFI_ERROR(Status)) {
          Status = egSaveFile(NULL, VBIOS_BIN, (UINT8*)(UINTN)0xc0000, 0x20000);
        }
        break;

      /* just a sample code
      case SCAN_F7:
        Status = egMkDir(SelfRootDir,  L"EFI\\CLOVER\\new_folder");
        DBG("create folder %r\n", Status);
        if (!EFI_ERROR(Status)) {
          Status = egSaveFile(SelfRootDir,  L"EFI\\CLOVER\\new_folder\\new_file.txt", (UINT8*)SomeText, sizeof(*SomeText)+1);
          DBG("create file %r\n", Status);
        }
        break;

      case SCAN_F8:
        do {
          CHAR16 *Str = PoolPrint(L"%s\n%s\n%s", L"ABC", L"123456", L"xy");
          if (Str != NULL) {
            AlertMessage(L"Sample message", Str);
            FreePool(Str);
          }
        } while (0);
        //this way screen is dirty
        break;
      */

      case SCAN_F9:
        SetNextScreenMode(1);
        break;

      case SCAN_F10:
        egScreenShot();
        break;

      case SCAN_F12:
        MenuExit = MENU_EXIT_EJECT;
        State.PaintAll = TRUE;
        break;
    }

    switch (key.UnicodeChar) {
      case CHAR_LINEFEED:
      case CHAR_CARRIAGE_RETURN:
        if (
          ((Screen->Entries[State.CurrentSelection])->Tag == TAG_INPUT) ||
          ((Screen->Entries[State.CurrentSelection])->Tag == TAG_CHECKBIT)
        ) {
          MenuExit = InputDialog(Screen, StyleFunc, &State);
        } else if ((Screen->Entries[State.CurrentSelection])->Tag == TAG_SWITCH) {
          MenuExit = InputDialog(Screen, StyleFunc, &State);
          State.PaintAll = TRUE;
        }/* else if ((Screen->Entries[State.CurrentSelection])->Tag == TAG_CLOVER){
          MenuExit = MENU_EXIT_DETAILS;
        }*/ else {
          MenuExit = MENU_EXIT_ENTER;
        }
        break;

      case ' ': //CHAR_SPACE
        if (
          ((Screen->Entries[State.CurrentSelection])->Tag == TAG_INPUT) ||
          ((Screen->Entries[State.CurrentSelection])->Tag == TAG_CHECKBIT)
        ) {
          MenuExit = InputDialog(Screen, StyleFunc, &State);
        } else if ((Screen->Entries[State.CurrentSelection])->Tag == TAG_SWITCH){
          MenuExit = InputDialog(Screen, StyleFunc, &State);
          State.PaintAll = TRUE;
        } else {
          MenuExit = MENU_EXIT_DETAILS;
        }
        break;

      default:
        ShortcutEntry = FindMenuShortcutEntry(Screen, key.UnicodeChar);
        if (ShortcutEntry >= 0) {
          State.CurrentSelection = ShortcutEntry;
          MenuExit = MENU_EXIT_ENTER;
        }
        break;
    }

  }

  StyleFunc(Screen, &State, MENU_FUNCTION_CLEANUP, NULL);

  if (ChosenEntry) {
    *ChosenEntry = Screen->Entries[State.CurrentSelection];
  }

  *DefaultEntryIndex = State.CurrentSelection;

  return MenuExit;
}

//
// text-mode generic style
//

STATIC
VOID
TextMenuStyle (
  IN REFIT_MENU_SCREEN    *Screen,
  IN SCROLL_STATE         *State,
  IN UINTN                Function,
  IN CHAR16               *ParamText
) {
  INTN                i = 0, j = 0;
  static UINTN        MenuPosY = 0;
  UINTN               TextMenuWidth = 0, ItemWidth = 0, MenuHeight = 0, iSwitch = -1;
  CHAR16              *TimeoutMessage,
                      *ResultString = AllocateZeroPool(AVALUE_MAX_SIZE);

  switch (Function) {
    case MENU_FUNCTION_INIT:
      // vertical layout
      MenuPosY = 4;

      if (Screen->InfoLineCount > 0) {
        MenuPosY += Screen->InfoLineCount + 1;
      }

      MenuHeight = ConHeight - MenuPosY;

      if (Screen->TimeoutSeconds > 0) {
        MenuHeight -= 2;
      }

      switch (Screen->Entries[0]->Tag) {
        case TAG_SWITCH:
          switch (((REFIT_INPUT_DIALOG*)(Screen->Entries[0]))->Item->ID) {
            case mConfigs:
              j = OldChosenConfig;
              break;
          }
          break;

        case TAG_LABEL:
          j = 0xFFFF;
          break;
      }

      InitScroll(State, Screen->EntryCount, Screen->EntryCount, MenuHeight, j);

      // determine width of the menu
      TextMenuWidth = 50;  // minimum

      for (i = 0; i <= State->MaxIndex; i++) {
        ItemWidth = StrLen(Screen->Entries[i]->Title);

        if (TextMenuWidth < ItemWidth) {
          TextMenuWidth = ItemWidth;
        }
      }

      if (TextMenuWidth > ConWidth - 6) {
        TextMenuWidth = ConWidth - 6;
      }
      break;

    case MENU_FUNCTION_CLEANUP:
      // release temporary memory
      break;

    case MENU_FUNCTION_PAINT_ALL:
      // paint the whole screen (initially and after scrolling)
      gST->ConOut->SetAttribute (gST->ConOut, ATTR_CHOICE_BASIC);

      for (i = 0; i < (INTN)ConHeight - 4; i++) {
        gST->ConOut->SetCursorPosition (gST->ConOut, 0, 4 + i);
        gST->ConOut->OutputString (gST->ConOut, BlankLine);
      }

      BeginTextScreen(Screen->Title);

      if (Screen->InfoLineCount > 0) {
        gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);

        for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
          gST->ConOut->SetCursorPosition (gST->ConOut, 3, 4 + i);
          gST->ConOut->OutputString (gST->ConOut, Screen->InfoLines[i]);
        }
      }

      for (i = State->FirstVisible; i <= State->LastVisible && i <= State->MaxIndex; i++) {
        REFIT_MENU_ENTRY    *Entry = Screen->Entries[i];

        gST->ConOut->SetCursorPosition (gST->ConOut, 2, MenuPosY + (i - State->FirstVisible));
        if (i == State->CurrentSelection) {
          gST->ConOut->SetAttribute (gST->ConOut, ATTR_CHOICE_CURRENT);
        } else {
          gST->ConOut->SetAttribute (gST->ConOut, ATTR_CHOICE_BASIC);
        }

        switch (Entry->Tag) {
          case TAG_INPUT:
            if (((REFIT_INPUT_DIALOG*)Entry)->Item->ItemType == BoolValue) {
              ResultString = PoolPrint(L"%s %s", ((REFIT_INPUT_DIALOG*)Entry)->Item->BValue ? L"[+]" : L"[ ]", Entry->Title);
            } else {
              ResultString = PoolPrint(L"%s: %s ", Entry->Title, ((REFIT_INPUT_DIALOG*)Entry)->Item->SValue);
            }
            break;

          case TAG_SWITCH:
            switch (((REFIT_INPUT_DIALOG*)Entry)->Item->ID) {
              case mConfigs:
                iSwitch = (UINTN)OldChosenConfig;
                break;
            }

            ResultString = PoolPrint(L"%s %s", (Entry->Row == iSwitch) ? L"(*)" : L"( )", Entry->Title);
            break;

          case TAG_CHECKBIT:
            ResultString = PoolPrint(L"%s %s", (((REFIT_INPUT_DIALOG*)Entry)->Item->IValue & Entry->Row) ? L"[+]" : L"[ ]", Entry->Title);
            break;

          default:
            ResultString = EfiStrDuplicate(Entry->Title);
            break;
        }

        for (j = StrLen(ResultString); j < (INTN)TextMenuWidth; j++) {
          ResultString[j] = L' ';
        }

        ResultString[j] = 0;
        gST->ConOut->OutputString (gST->ConOut, ResultString);

        FreePool(ResultString);
      }

      // scrolling indicators
      gST->ConOut->SetAttribute (gST->ConOut, ATTR_SCROLLARROW);
      gST->ConOut->SetCursorPosition (gST->ConOut, 0, MenuPosY);

      if (State->FirstVisible > 0) {
        gST->ConOut->OutputString (gST->ConOut, ArrowUp);
      } else {
        gST->ConOut->OutputString (gST->ConOut, L" ");
      }

      gST->ConOut->SetCursorPosition (gST->ConOut, 0, MenuPosY + State->MaxVisible);

      if (State->LastVisible < State->MaxIndex) {
        gST->ConOut->OutputString (gST->ConOut, ArrowDown);
      } else {
        gST->ConOut->OutputString (gST->ConOut, L" ");
      }
      break;

    case MENU_FUNCTION_PAINT_SELECTION:
    {
      REFIT_MENU_ENTRY    *EntryL = Screen->Entries[State->LastSelection],
                          *EntryC = Screen->Entries[State->CurrentSelection];

      // redraw selection cursor
      gST->ConOut->SetCursorPosition (gST->ConOut, 2, MenuPosY + (State->LastSelection - State->FirstVisible));
      gST->ConOut->SetAttribute (gST->ConOut, ATTR_CHOICE_BASIC);

      switch(EntryL->Tag) {
        case TAG_INPUT:
          if (((REFIT_INPUT_DIALOG*)EntryL)->Item->ItemType == BoolValue) {
            ResultString = PoolPrint(L"%s %s", ((REFIT_INPUT_DIALOG*)EntryL)->Item->BValue ? L"[+]" : L"[ ]", EntryL->Title);
          } else {
            ResultString = PoolPrint(L"%s: %s ", EntryL->Title, ((REFIT_INPUT_DIALOG*)EntryL)->Item->SValue);
          }
          break;

        case TAG_SWITCH:
          switch (((REFIT_INPUT_DIALOG*)EntryL)->Item->ID) {
            case mConfigs:
              iSwitch = (UINTN)OldChosenConfig;
              break;
          }

          ResultString = PoolPrint(L"%s %s", (EntryL->Row == iSwitch) ? L"(*)" : L"( )", EntryL->Title);
          break;

        case TAG_CHECKBIT:
          StrCat(ResultString, (((REFIT_INPUT_DIALOG*)EntryL)->Item->IValue & EntryL->Row) ? L":[+]":L":[ ]");
          break;

        default:
          ResultString = EfiStrDuplicate(EntryL->Title);
          break;
      }

      for (j = StrLen(ResultString); j < (INTN)TextMenuWidth; j++) {
        ResultString[j] = L' ';
      }

      ResultString[j] = 0;
      gST->ConOut->OutputString (gST->ConOut, ResultString);

      FreePool(ResultString);

      // Current selection ///////////////////

      gST->ConOut->SetCursorPosition (gST->ConOut, 2, MenuPosY + (State->CurrentSelection - State->FirstVisible));
      gST->ConOut->SetAttribute (gST->ConOut, ATTR_CHOICE_CURRENT);

      switch(EntryC->Tag) {
        case TAG_INPUT:
          if (((REFIT_INPUT_DIALOG*)EntryC)->Item->ItemType == BoolValue) {
            ResultString = PoolPrint(L"%s %s", ((REFIT_INPUT_DIALOG*)EntryC)->Item->BValue ? L"[+]" : L"[ ]", EntryC->Title);
          } else {
            ResultString = PoolPrint(L"%s: %s ", EntryC->Title, ((REFIT_INPUT_DIALOG*)EntryC)->Item->SValue);
          }
          break;

        case TAG_SWITCH:
          switch (((REFIT_INPUT_DIALOG*)EntryC)->Item->ID) {
            case mConfigs:
              iSwitch = (UINTN)OldChosenConfig;
              break;
          }

          ResultString = PoolPrint(L"%s %s", (EntryC->Row == iSwitch) ? L"(*)" : L"( )", EntryC->Title);
          break;

        case TAG_CHECKBIT:
          StrCat(ResultString, (((REFIT_INPUT_DIALOG*)EntryC)->Item->IValue & EntryC->Row) ? L":[+]":L":[ ]");
          break;

        default:
          ResultString = EfiStrDuplicate(EntryC->Title);
          break;
      }

      for (j = StrLen(ResultString); j < (INTN)TextMenuWidth; j++) {
        ResultString[j] = L' ';
      }

      ResultString[j] = 0;
      gST->ConOut->OutputString (gST->ConOut, ResultString);

      FreePool(ResultString);
      break;
    }

    case MENU_FUNCTION_PAINT_TIMEOUT:
      if (ParamText[0] == 0) {
        // clear message
        gST->ConOut->SetAttribute (gST->ConOut, ATTR_BASIC);
        gST->ConOut->SetCursorPosition (gST->ConOut, 0, ConHeight - 1);
        gST->ConOut->OutputString (gST->ConOut, BlankLine + 1);
      } else {
        // paint or update message
        gST->ConOut->SetAttribute (gST->ConOut, ATTR_ERROR);
        gST->ConOut->SetCursorPosition (gST->ConOut, 3, ConHeight - 1);
        TimeoutMessage = PoolPrint(L"%s  ", ParamText);
        gST->ConOut->OutputString (gST->ConOut, TimeoutMessage);
        FreePool(TimeoutMessage);
      }
      break;
  }

  gST->ConOut->SetAttribute (gST->ConOut, ATTR_CHOICE_BASIC);
}

//
// graphical generic style
//

INTN
DrawTextXY (
  IN CHAR16   *Text,
  IN INTN     XPos,
  IN INTN     YPos,
  IN UINT8    XAlign
) {
  INTN        TextWidth = 0, XText = 0;
  EG_IMAGE    *TextBufferXY = NULL;

  if (!Text) {
    return 0;
  }

  egMeasureText(Text, &TextWidth, NULL);

  if (XAlign == X_IS_LEFT) {
    TextWidth = UGAWidth - XPos - 1;
    XText = XPos;
  }

  //TextBufferXY = egCreateImage(TextWidth, TextHeight, TRUE);
  //egFillImage(TextBufferXY, &MenuBackgroundPixel);
  TextBufferXY = egCreateFilledImage(TextWidth, TextHeight, TRUE, &MenuBackgroundPixel);
  // render the text
  TextWidth = egRenderText(Text, TextBufferXY, 0, 0, 0xFFFF, FALSE);

  if (XAlign != X_IS_LEFT) { // shift 64 is prohibited
    XText = XPos - (TextWidth >> XAlign);
  }

  BltImageAlpha(TextBufferXY, XText, YPos,  &MenuBackgroundPixel, 16);
  egFreeImage(TextBufferXY);

  return TextWidth;
}

VOID
DrawBCSText (
  IN CHAR16     *Text,
  IN INTN       XPos,
  IN INTN       YPos,
  IN UINT8      XAlign
) {
  INTN        ChrsNum = 12, Ellipsis = 3, TextWidth = 0, XText = 0, i = 0;
  EG_IMAGE    *TextBufferXY = NULL;
  CHAR16      *BCSText = NULL;

  if (!Text) {
    return;
  }

  if (GlobalConfig.TileXSpace >= 25 && GlobalConfig.TileXSpace < 30) {
    ChrsNum = 13;
  } else if (GlobalConfig.TileXSpace >= 30 && GlobalConfig.TileXSpace < 35) {
    ChrsNum = 14;
  } else if (GlobalConfig.TileXSpace >= 35 && GlobalConfig.TileXSpace < 40) {
    ChrsNum = 15;
  } else if (GlobalConfig.TileXSpace >= 40 && GlobalConfig.TileXSpace < 45) {
    ChrsNum = 16;
  } else if (GlobalConfig.TileXSpace >= 45 && GlobalConfig.TileXSpace < 50) {
    ChrsNum = 17;
  } else if (GlobalConfig.TileXSpace >= 50 && GlobalConfig.TileXSpace < 55) {
    ChrsNum = 18;
  }

  TextWidth = (((INTN) StrLen(Text) <= ChrsNum - Ellipsis) ? (INTN) StrLen(Text) : ChrsNum) *
              ((FontWidth > GlobalConfig.CharWidth) ? FontWidth : GlobalConfig.CharWidth);

  // render the text

  TextBufferXY = egCreateFilledImage(TextWidth, FontHeight, TRUE, &MenuBackgroundPixel);

  if (XAlign == X_IS_LEFT) {
    TextWidth = UGAWidth - XPos - 1;
    XText = XPos;
  }

  if ((INTN) StrLen(Text) > (ChrsNum - Ellipsis)) {
    BCSText = AllocatePool(sizeof(CHAR16) * ChrsNum);

    for (i = 0; i < ChrsNum; i++) {
      if (i < ChrsNum - Ellipsis) {
        BCSText[i] = Text[i];
      } else {
        BCSText[i] = L'.';
      }
    }

    BCSText[ChrsNum] = '\0';

    if (!BCSText) {
      return;
    }

    TextWidth = egRenderText(BCSText, TextBufferXY, 0, 0, 0xFFFF, FALSE);

    FreePool(BCSText);
  } else {
    TextWidth = egRenderText(Text, TextBufferXY, 0, 0, 0xFFFF, FALSE);
  }

  if (XAlign != X_IS_LEFT) { // shift 64 is prohibited
    XText = XPos - (TextWidth >> XAlign);
  }

  BltImageAlpha(TextBufferXY, XText, YPos,  &MenuBackgroundPixel, 16);
  egFreeImage(TextBufferXY);
}

VOID
DrawMenuText (
  IN CHAR16     *Text,
  IN INTN       SelectedWidth,
  IN INTN       XPos,
  IN INTN       YPos,
  IN INTN       Cursor,
  IN EG_IMAGE   *Button
) {
  INTN    PlaceCentre = (TextHeight / 2) - 7;

  //use Text=null to reinit the buffer
  if (!Text) {
    if (TextBuffer) {
      egFreeImage(TextBuffer);
      TextBuffer = NULL;
    }

    return;
  }

  if (TextBuffer && (TextBuffer->Height != TextHeight)) {
    egFreeImage(TextBuffer);
    TextBuffer = NULL;
  }

  if (TextBuffer == NULL) {
    TextBuffer = egCreateImage(UGAWidth-XPos, TextHeight, TRUE);
  }

  egFillImage(TextBuffer, &MenuBackgroundPixel);

  if (SelectedWidth > 0) {
    // draw selection bar background
    egFillImageArea(TextBuffer, 0, 0, SelectedWidth, TextBuffer->Height, &SelectionBackgroundPixel);
  }

  // render the text
  egRenderText(Text, TextBuffer, TEXT_XMARGIN, TEXT_YMARGIN, Cursor, (SelectedWidth > 0));

  if (Button != NULL) {
    egComposeImage(TextBuffer, Button, TEXT_XMARGIN, PlaceCentre);
  }

  BltImageAlpha(TextBuffer, XPos, YPos, &MenuBackgroundPixel, 16);
}

EG_PIXEL
ToPixel (
  UINTN   rgba
) {
  EG_PIXEL color;

  color.r = (rgba >> 24) & 0XFF;
  color.g = (rgba >> 16) & 0XFF;
  color.b = (rgba >> 8) & 0XFF;
  color.a = rgba & 0XFF;

  return color;
}

VOID InitSelection() {
  if (!AllowGraphicsMode) {
    return;
  }

  SelectionBackgroundPixel = ToPixel(GlobalConfig.SelectionColor);

  InitUISelection();
}

//
// graphical main menu style
//

VOID
GraphicsMenuStyle (
  IN REFIT_MENU_SCREEN    *Screen,
  IN SCROLL_STATE         *State,
  IN UINTN                Function,
  IN CHAR16               *ParamText
) {
  INTN        i, j = 0, ItemWidth = 0, X, t1, t2, VisibleHeight = 0; //assume vertical layout
  UINTN       TitleLen, iSwitch = -1;
  BOOLEAN     NeedMarginLeft;
  CHAR16      *ResultString = AllocateZeroPool(AVALUE_MAX_SIZE);

  switch (Function) {

    case MENU_FUNCTION_INIT:
      egGetScreenSize(&UGAWidth, &UGAHeight);
      InitAnime(Screen);
      SwitchToGraphicsAndClear();

      EntriesPosY = ((UGAHeight - LAYOUT_TOTAL_HEIGHT) >> 1) + GlobalConfig.LayoutBannerOffset + (TextHeight << 1);

      //VisibleHeight = (UGAHeight - EntriesPosY) / TextHeight - Screen->InfoLineCount - 1;
      //VisibleHeight = ((UGAHeight - EntriesPosY) / TextHeight) - Screen->InfoLineCount - 2 - GlobalConfig.PruneScrollRows;
      VisibleHeight = ((UGAHeight - EntriesPosY) / TextHeight) - Screen->InfoLineCount;

      if (VisibleHeight > 2) {
        VisibleHeight -= 2;
      }

      //if ((Screen->InfoLineCount > 2) && (VisibleHeight - GlobalConfig.PruneScrollRows)) {
      if ((VisibleHeight - GlobalConfig.PruneScrollRows) > 2) {
        VisibleHeight -= GlobalConfig.PruneScrollRows;
      }

      switch (Screen->Entries[0]->Tag) {
        case TAG_SWITCH:
          switch (((REFIT_INPUT_DIALOG*)(Screen->Entries[0]))->Item->ID) {
            case mThemes:
              j = OldChosenTheme;
              break;

            case mConfigs:
              j = OldChosenConfig;
              break;
          }
          break;

        case TAG_LABEL:
          j = 0xFFFF;
          break;
      }

      InitScroll(State, Screen->EntryCount, Screen->EntryCount, VisibleHeight, j);
      // determine width of the menu -- not working
      //MenuWidth = 80;  // minimum
      MenuWidth = LAYOUT_TEXT_WIDTH; //500
      DrawMenuText(NULL, 0, 0, 0, 0, NULL);

      if (Screen->TitleImage) {
        if (MenuWidth > (INTN)(UGAWidth - TITLEICON_SPACING - Screen->TitleImage->Width)) {
          MenuWidth = UGAWidth - TITLEICON_SPACING - Screen->TitleImage->Width - 2;
        }

        EntriesPosX = (UGAWidth - (Screen->TitleImage->Width + TITLEICON_SPACING + MenuWidth)) >> 1;
        //DBG("UGAWIdth=%d TitleImage=%d MenuWidth=%d\n", UGAWidth,
        //Screen->TitleImage->Width, MenuWidth);
        MenuWidth += Screen->TitleImage->Width;
      } else {
        EntriesPosX = (UGAWidth - MenuWidth) >> 1;
      }

      TimeoutPosY = EntriesPosY + (Screen->EntryCount + 1) * TextHeight;

      // initial painting
      egMeasureText(Screen->Title, &ItemWidth, NULL);

      if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_MENU_TITLE)) {
        DrawTextXY(Screen->Title, (UGAWidth >> 1), EntriesPosY - TextHeight * 2, X_IS_CENTER);
      }

      if (Screen->TitleImage) {
        INTN    FilmXPos = (INTN)(EntriesPosX - (Screen->TitleImage->Width + TITLEICON_SPACING));
        INTN    FilmYPos = (INTN)EntriesPosY;

        BltImageAlpha(Screen->TitleImage, FilmXPos, FilmYPos, &MenuBackgroundPixel, 16);

        // Update FilmPlace only if not set by InitAnime
        if ((Screen->FilmPlace.Width == 0) || (Screen->FilmPlace.Height == 0)) {
          Screen->FilmPlace.XPos = FilmXPos;
          Screen->FilmPlace.YPos = FilmYPos;
          Screen->FilmPlace.Width = Screen->TitleImage->Width;
          Screen->FilmPlace.Height = Screen->TitleImage->Height;
        }
      }

      if (Screen->InfoLineCount > 0) {
        DrawMenuText(NULL, 0, 0, 0, 0, NULL);

        for (i = 0; i < (INTN)Screen->InfoLineCount; i++) {
          DrawMenuText(Screen->InfoLines[i], 0, EntriesPosX, EntriesPosY, 0xFFFF, NULL);
          EntriesPosY += TextHeight;
        }

        EntriesPosY += TextHeight;  // also add a blank line
      }

      InitBar();
      break;

    case MENU_FUNCTION_CLEANUP:
      //HidePointer();
      break;

    case MENU_FUNCTION_PAINT_ALL:
      DrawMenuText(NULL, 0, 0, 0, 0, NULL); //should clean every line to avoid artefacts
      //DBG("PAINT_ALL: EntriesPosY=%d MaxVisible=%d\n", EntriesPosY, State->MaxVisible);
      //DBG("DownButton.Height=%d TextHeight=%d\n", DownButton.Height, TextHeight);
      t1 = EntriesPosX + TextHeight + TEXT_XMARGIN + MenuWidth + 16;
      t2 = EntriesPosY + (State->MaxVisible + 1) * TextHeight - DownButton.Height;
      //DBG("PAINT_ALL: %d %d\n", t1, t2);
      SetBar(t1, EntriesPosY, t2, State);

      // blackosx swapped this around so drawing of selection comes before drawing scrollbar.

      for (i = State->FirstVisible, j = 0; i <= State->LastVisible; i++, j++) {
        //ResultString = AllocateZeroPool(256);
        REFIT_MENU_ENTRY *Entry = Screen->Entries[i];

        NeedMarginLeft = (
          (Entry->Tag == TAG_SWITCH) || (Entry->Tag == TAG_CHECKBIT) ||
          ((Entry->Tag == TAG_INPUT) && (((REFIT_INPUT_DIALOG*)Entry)->Item->ItemType == BoolValue))
        );

        ResultString = PoolPrint(L"%a%s", NeedMarginLeft ? "   " : "", Entry->Title);

        TitleLen = StrLen(ResultString);

        Entry->Place.XPos = EntriesPosX;
        Entry->Place.YPos = EntriesPosY + j * TextHeight;
        Entry->Place.Width = TitleLen * GlobalConfig.CharWidth;
        Entry->Place.Height = (UINTN)TextHeight;

        switch(Entry->Tag) {
          case TAG_INPUT:
            if (((REFIT_INPUT_DIALOG*)Entry)->Item->ItemType == BoolValue) {
              Entry->Place.Width = StrLen(ResultString) * GlobalConfig.CharWidth;
              DrawMenuText(
                ResultString,
                (i == State->CurrentSelection) ? MenuWidth : 0,
                EntriesPosX,
                Entry->Place.YPos,
                0xFFFF,
                ButtonsImg[((REFIT_INPUT_DIALOG*)(Entry))->Item->BValue ? kCheckboxCheckedImage : kCheckboxImage].Image
              );
            } else { //text input
              //CHAR16 *keat = AllocateZeroPool(SVALUE_MAX_SIZE);
              //StrCat(ResultString, L": ");
              //StrCat(ResultString, ((REFIT_INPUT_DIALOG*)(Entry))->Item->SValue);
              //StrCat(ResultString, L" ");
              StrCat(ResultString, PoolPrint(L": %s ", ((REFIT_INPUT_DIALOG*)(Entry))->Item->SValue));
              Entry->Place.Width = StrLen(ResultString) * GlobalConfig.CharWidth;
              // Slice - suppose to use Row as Cursor in text
              DrawMenuText(
                ResultString,
                (i == State->CurrentSelection) ? MenuWidth : 0,
                EntriesPosX,
                Entry->Place.YPos,
                TitleLen + Entry->Row,
                NULL
              );
            }
            break;

          case TAG_SWITCH:
            switch (((REFIT_INPUT_DIALOG*)(Entry))->Item->ID) {
              case mThemes:
                iSwitch = (UINTN)OldChosenTheme;
                break;

              case mConfigs:
                iSwitch = (UINTN)OldChosenConfig;
                break;
            }

            DrawMenuText(
              ResultString,
              (i == State->CurrentSelection) ? MenuWidth : 0,
              EntriesPosX,
              Entry->Place.YPos,
              0xFFFF,
              ButtonsImg[(Entry->Row == iSwitch) ? kRadioSelectedImage : kRadioImage].Image
            );
            break;

          case TAG_CHECKBIT:
            DrawMenuText(
              ResultString,
              (i == State->CurrentSelection) ? MenuWidth : 0,
              EntriesPosX,
              Entry->Place.YPos,
              0xFFFF,
              ButtonsImg[(((REFIT_INPUT_DIALOG*)(Entry))->Item->IValue & Entry->Row) ? kCheckboxCheckedImage : kCheckboxImage].Image
            );
            break;

          default:
            //DBG("paint entry %d title=%s\n", i, Screen->Entries[i]->Title);
            DrawMenuText(
              ResultString,
              (i == State->CurrentSelection) ? MenuWidth : 0,
              EntriesPosX,
              Entry->Place.YPos,
              0xFFFF,
              NULL
            );
            break;
        }

        FreePool(ResultString);
      }

      ScrollingBar(State);
      break;

    case MENU_FUNCTION_PAINT_SELECTION:
    {
      REFIT_MENU_ENTRY    *EntryL = Screen->Entries[State->LastSelection],
                          *EntryC = Screen->Entries[State->CurrentSelection];
      //ResultString = AllocateZeroPool(256);

      NeedMarginLeft = (
        (EntryL->Tag == TAG_SWITCH) || (EntryL->Tag == TAG_CHECKBIT) ||
        ((EntryL->Tag == TAG_INPUT) && (((REFIT_INPUT_DIALOG*)EntryL)->Item->ItemType == BoolValue))
      );

      //ResultString = NeedMarginLeft ? PoolPrint(L"   %s", EntryL->Title) : EfiStrDuplicate(EntryL->Title);
      ResultString = PoolPrint(L"%a%s", NeedMarginLeft ? "   " : "", EntryL->Title);

      TitleLen = StrLen(ResultString);

      // blackosx swapped this around so drawing of selection comes before drawing scrollbar.

      // redraw selection cursor
      //usr-sse2
      switch(EntryL->Tag) {
        case TAG_INPUT:
          if (((REFIT_INPUT_DIALOG*)EntryL)->Item->ItemType == BoolValue) {
            DrawMenuText(
              ResultString,
              0,
              EntriesPosX,
              EntriesPosY + (State->LastSelection - State->FirstVisible) * TextHeight,
              0xFFFF,
              ButtonsImg[((REFIT_INPUT_DIALOG*)EntryL)->Item->BValue ? kCheckboxCheckedImage : kCheckboxImage].Image
            );
          } else {
            StrCat(ResultString, L": ");
            StrCat(ResultString, ((REFIT_INPUT_DIALOG*)(EntryL))->Item->SValue +
                   ((REFIT_INPUT_DIALOG*)(EntryL))->Item->LineShift);
            StrCat(ResultString, L" ");
            DrawMenuText(
              ResultString,
              0,
              EntriesPosX,
              EntriesPosY + (State->LastSelection - State->FirstVisible) * TextHeight,
              TitleLen + EntryL->Row,
              NULL
            );
          }
          break;

        case TAG_SWITCH:
          switch (((REFIT_INPUT_DIALOG*)(EntryL))->Item->ID) {
            case mThemes:
              iSwitch = (UINTN)OldChosenTheme;
              break;

            case mConfigs:
              iSwitch = (UINTN)OldChosenConfig;
              break;
          }

          DrawMenuText(
            ResultString,
            0,
            EntriesPosX,
            EntriesPosY + (State->LastSelection - State->FirstVisible) * TextHeight,
            0xFFFF,
            ButtonsImg[(EntryL->Row == iSwitch) ? kRadioSelectedImage : kRadioImage].Image
          );
          break;

        case TAG_CHECKBIT:
          //DrawMenuText(L" ", 0, EntriesPosX, EntryL->Place.YPos, 0xFFFF);
          DrawMenuText(
            ResultString,
            0,
            EntriesPosX,
            EntryL->Place.YPos,
            0xFFFF,
            ButtonsImg[(((REFIT_INPUT_DIALOG*)EntryL)->Item->IValue & EntryL->Row) ? kCheckboxCheckedImage : kCheckboxImage].Image
          );
          break;

        default:
          DrawMenuText(
            ResultString,
            0,
            EntriesPosX,
            EntriesPosY + (State->LastSelection - State->FirstVisible) * TextHeight,
            0xFFFF,
            NULL
          );
          break;
      }

      FreePool(ResultString);

      // Current selection ///////////////////

      NeedMarginLeft = (
        (EntryC->Tag == TAG_SWITCH) || (EntryC->Tag == TAG_CHECKBIT) ||
        ((EntryC->Tag == TAG_INPUT) && (((REFIT_INPUT_DIALOG*)EntryC)->Item->ItemType == BoolValue))
      );

      //ResultString = AllocateZeroPool(256);
      //ResultString = NeedMarginLeft ? PoolPrint(L"   %s", EntryC->Title) : EfiStrDuplicate(EntryC->Title);
      ResultString = PoolPrint(L"%a%s", NeedMarginLeft ? "   " : "", EntryC->Title);

      TitleLen = StrLen(ResultString);

      switch(EntryC->Tag) {
        case TAG_INPUT:
          if (((REFIT_INPUT_DIALOG*)EntryC)->Item->ItemType == BoolValue) {
            DrawMenuText(
              ResultString,
              MenuWidth,
              EntriesPosX,
              EntriesPosY + (State->CurrentSelection - State->FirstVisible) * TextHeight,
              0xFFFF,
              ButtonsImg[((REFIT_INPUT_DIALOG*)EntryC)->Item->BValue ? kCheckboxCheckedImage : kCheckboxImage].Image
            );
          } else {
            StrCat(ResultString, L": ");
            StrCat(ResultString, ((REFIT_INPUT_DIALOG*)EntryC)->Item->SValue +
                                 ((REFIT_INPUT_DIALOG*)EntryC)->Item->LineShift);
            StrCat(ResultString, L" ");
            DrawMenuText(
              ResultString,
              MenuWidth,
              EntriesPosX,
              EntriesPosY + (State->CurrentSelection - State->FirstVisible) * TextHeight,
              TitleLen + EntryC->Row,
              NULL
            );
          }
          break;

        case TAG_SWITCH:
          switch (((REFIT_INPUT_DIALOG*)(EntryC))->Item->ID) {
            case mThemes:
              iSwitch = (UINTN)OldChosenTheme;
              break;

            case mConfigs:
              iSwitch = (UINTN)OldChosenConfig;
              break;
          }

          DrawMenuText(
            ResultString,
            MenuWidth,
            EntriesPosX,
            EntriesPosY + (State->CurrentSelection - State->FirstVisible) * TextHeight,
            0xFFFF,
            ButtonsImg[(EntryC->Row == iSwitch) ? kRadioSelectedImage : kRadioImage].Image
          );
          break;

        case TAG_CHECKBIT:
          //DrawMenuText(L" ", 0, EntriesPosX, EntryC->Place.YPos, 0xFFFF);
          DrawMenuText(
            ResultString,
            MenuWidth,
            EntriesPosX,
            EntryC->Place.YPos,
            0xFFFF,
            ButtonsImg[(((REFIT_INPUT_DIALOG*)EntryC)->Item->IValue & EntryC->Row) ? kCheckboxCheckedImage : kCheckboxImage].Image
            );
          break;

        default:
          DrawMenuText(
            ResultString,
            MenuWidth,
            EntriesPosX,
            EntriesPosY + (State->CurrentSelection - State->FirstVisible) * TextHeight,
            0xFFFF,
            NULL
          );
          break;
      }

      FreePool(ResultString);

      ScrollStart.YPos = ScrollbarBackground.YPos + ScrollbarBackground.Height * State->FirstVisible / (State->MaxIndex + 1);
      Scrollbar.YPos = ScrollStart.YPos + ScrollStart.Height;
      ScrollEnd.YPos = Scrollbar.YPos + Scrollbar.Height; // ScrollEnd.Height is already subtracted
      ScrollingBar(State);

      //MouseBirth();
      break;
    }

    case MENU_FUNCTION_PAINT_TIMEOUT:
      X = (UGAWidth - StrLen(ParamText) * GlobalConfig.CharWidth) >> 1;
      DrawMenuText(ParamText, 0, X, TimeoutPosY, 0xFFFF, NULL);
      break;
  }
}

STATIC
VOID
DrawMainMenuEntry (
  REFIT_MENU_ENTRY    *Entry,
  BOOLEAN             selected,
  INTN                XPos,
  INTN                YPos
) {
  INTN Scale = GlobalConfig.MainEntriesSize >> 3;

  /*
  if (GlobalConfig.BootCampStyle && (Entry->Row == 1)) {
    return;
  }
  */

  if (
    (Entry->Tag == TAG_LOADER) &&
    !(GlobalConfig.HideBadges & HDBADGES_SWAP) &&
    (Entry->Row == 0)
  ) {
    MainImage = Entry->DriveImage;
  } else {
    MainImage = Entry->Image;
  }

  if (!MainImage) {
    //if (!IsEmbeddedTheme()) {
      MainImage = egLoadIcon(ThemeDir, GetIconsExt(L"icons\\os_mac", L"icns"), Scale << 3);
    //}

    if (!MainImage) {
      MainImage = DummyImage(Scale << 3);
    }
  }

  //  DBG("Entry title=%s; Width=%d\n", Entry->Title, MainImage->Width);
  Scale = ((Entry->Row == 0) ? (Scale * (selected ? 1 : -1)): 16) ;
  if (GlobalConfig.SelectionOnTop) {
    BltImageCompositeBadge(
      MainImage,
      SelectionImg[((Entry->Row == 0) ? kBigImage : kSmallImage) + (selected ? 0 : 1)].Image,
      selected
        ? (Entry->ImageHover ? Entry->ImageHover : ((Entry->Row == 0) ? Entry->BadgeImage : NULL))
        : ((Entry->Row == 0) ? Entry->BadgeImage : NULL),
      XPos, YPos, Scale);

  } else {
    BltImageCompositeBadge(
      SelectionImg[((Entry->Row == 0) ? kBigImage : kSmallImage) + (selected ? 0 : 1)].Image,
      MainImage,
      selected
        ? (Entry->ImageHover ? Entry->ImageHover : ((Entry->Row == 0) ? Entry->BadgeImage : NULL))
        : ((Entry->Row == 0) ? Entry->BadgeImage : NULL),
      XPos, YPos, Scale);
  }

  if (GlobalConfig.BootCampStyle && (Entry->Row == 0)) {
    BltImageAlpha(
      SelectionImg[kIndicatorImage + (selected ? 0 : 1)].Image,
      XPos + (GlobalConfig.row0TileSize / 2) - (INDICATOR_SIZE / 2),
      row0PosY + GlobalConfig.row0TileSize + ((GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL)
        ? 10
        : (FontHeight - TEXT_YMARGIN + 20)),
      &MenuBackgroundPixel, Scale
    );
  }

  Entry->Place.XPos = XPos;
  Entry->Place.YPos = YPos;
  Entry->Place.Width = MainImage->Width;
  Entry->Place.Height = MainImage->Height;
}

STATIC
VOID
FillRectAreaOfScreen (
  IN INTN       XPos,
  IN INTN       YPos,
  IN INTN       Width,
  IN INTN       Height,
  IN EG_PIXEL   *Color,
  IN UINT8      XAlign
) {
  EG_IMAGE    *TmpBuffer = NULL;
  INTN        X = XPos - (Width >> XAlign);

  if (!Width || !Height) {
    return;
  }

  TmpBuffer = egCreateImage(Width, Height, FALSE);
  if (!BackgroundImage) {
    egFillImage(TmpBuffer, Color);
  } else {
    egRawCopy(
      TmpBuffer->PixelData,
      BackgroundImage->PixelData + YPos * BackgroundImage->Width + X,
      Width, Height,
      TmpBuffer->Width,
      BackgroundImage->Width
    );
  }

  BltImage(TmpBuffer, X, YPos);
  egFreeImage(TmpBuffer);
}

STATIC
VOID
DrawMainMenuLabel (
  IN CHAR16               *Text,
  IN INTN                 XPos,
  IN INTN                 YPos,
  IN REFIT_MENU_SCREEN    *Screen,
  IN SCROLL_STATE         *State
) {
  INTN TextWidth;

  egMeasureText(Text, &TextWidth, NULL);

  //Clear old text
  if (OldTextWidth > TextWidth) {
    FillRectAreaOfScreen(OldX, OldY, OldTextWidth, TextHeight, &MenuBackgroundPixel, X_IS_CENTER);
  }

  if (
    !GlobalConfig.BootCampStyle &&
    (GlobalConfig.HideBadges & HDBADGES_INLINE) &&
    !OldRow &&
    OldTextWidth &&
    (OldTextWidth != TextWidth)
  ) {
    //Clear badge
    BltImageAlpha(
      NULL,
      (OldX - (OldTextWidth >> 1) - (BADGE_DIMENSION + 16)),
      (OldY - ((BADGE_DIMENSION - TextHeight) >> 1)),
      &MenuBackgroundPixel,
      BADGE_DIMENSION >> 3
    );
  }

  DrawTextXY(Text, XPos, YPos, X_IS_CENTER);

  //show inline badge
  if (
    !GlobalConfig.BootCampStyle &&
    (GlobalConfig.HideBadges & HDBADGES_INLINE) &&
    (Screen->Entries[State->CurrentSelection]->Row == 0)
  ) {
    // Display Inline Badge: small icon before the text
    BltImageAlpha(
      ((LOADER_ENTRY*)Screen->Entries[State->CurrentSelection])->me.Image,
      (XPos - (TextWidth >> 1) - (BADGE_DIMENSION + 16)),
      (YPos - ((BADGE_DIMENSION - TextHeight) >> 1)),
      &MenuBackgroundPixel,
      BADGE_DIMENSION >> 3
    );
  }

  OldX = XPos;
  OldY = YPos;
  OldTextWidth = TextWidth;
  OldRow = Screen->Entries[State->CurrentSelection]->Row;
}

VOID
CountItems (
  IN REFIT_MENU_SCREEN    *Screen
) {
  INTN    i;

  row0PosX = 0;
  row1PosX = Screen->EntryCount;
  // layout
  row0Count = 0; //Nr items in row0
  row1Count = 0;

  for (i = 0; i < (INTN)Screen->EntryCount; i++) {
    if (Screen->Entries[i]->Row == 0) {
      row0Count++;
      CONSTRAIN_MIN(row0PosX, i);
    } else {
      row1Count++;
      CONSTRAIN_MAX(row1PosX, i);
    }
  }
}

VOID
DrawTextCorner (
  UINTN   TextC,
  UINT8   Align
) {
  INTN      Xpos;
  CHAR16    *Text = AllocateZeroPool(256);

  if (
    // HIDEUI_ALL - included
    ((TextC == TEXT_CORNER_REVISION) && ((GlobalConfig.HideUIFlags & HIDEUI_FLAG_REVISION_TEXT) != 0)) ||
    ((TextC == TEXT_CORNER_HELP) && ((GlobalConfig.HideUIFlags & HIDEUI_FLAG_HELP_TEXT) != 0))
  ) {
    return;
  }

  switch (TextC) {
    case TEXT_CORNER_REVISION:
#ifdef FIRMWARE_REVISION
      Text = PoolPrint(FIRMWARE_REVISION);
#else
      //Text = EfiStrDuplicate(gST->FirmwareRevision);
      Text = PoolPrint(L"%d.%d", gST->FirmwareRevision >> 16, gST->FirmwareRevision & ((1 << 16) - 1));
#endif
      break;

    case TEXT_CORNER_HELP:
      Text = PoolPrint(L"F1:Help");
      break;

    default:
      return;
  }

  switch (Align) {
    case X_IS_LEFT:
      Xpos = 5;
      break;

    case X_IS_RIGHT:
      Xpos = UGAWidth - 5;//2
      break;

    case X_IS_CENTER: //not used
      Xpos = UGAWidth >> 1;
      break;

    default:
      return;
  }

  DrawTextXY(Text, Xpos, UGAHeight - 5 - TextHeight, Align);
  FreePool(Text);
}

VOID
MainMenuStyle (
  IN REFIT_MENU_SCREEN  *Screen,
  IN SCROLL_STATE       *State,
  IN UINTN              Function,
  IN CHAR16             *ParamText
) {
  INTN i;

  switch (Function) {

    case MENU_FUNCTION_INIT:
      egGetScreenSize(&UGAWidth, &UGAHeight);
      InitAnime(Screen);
      SwitchToGraphicsAndClear();

      EntriesGap = GlobalConfig.TileXSpace;
      EntriesWidth = GlobalConfig.MainEntriesSize + (16 * GlobalConfig.row0TileSize) / 144;
      EntriesHeight = GlobalConfig.MainEntriesSize + 16;

      MaxItemOnScreen = (UGAWidth - ROW0_SCROLLSIZE * 2) / (EntriesWidth + EntriesGap); //8
      CountItems(Screen);
      InitScroll(State, row0Count, Screen->EntryCount, MaxItemOnScreen, 0);

      row0PosX = (UGAWidth + 8 - (EntriesWidth + EntriesGap) *
                  ((MaxItemOnScreen < row0Count)?MaxItemOnScreen:row0Count)) >> 1;

      row0PosY = ((UGAHeight - LAYOUT_MAINMENU_HEIGHT) >> 1) + GlobalConfig.LayoutBannerOffset;

      row1PosX = (UGAWidth + 8 - (GlobalConfig.row1TileSize + TILE_XSPACING) * row1Count) >> 1;

      if (GlobalConfig.BootCampStyle) {
        row1PosY = row0PosY + GlobalConfig.row0TileSize +
                   GlobalConfig.LayoutButtonOffset + GlobalConfig.TileYSpace + INDICATOR_SIZE +
                   ((GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL) ? 15 : (FontHeight + 30));
      } else {
        row1PosY = row0PosY + EntriesHeight + GlobalConfig.TileYSpace + GlobalConfig.LayoutButtonOffset;
      }

      if (row1Count > 0) {
        if (GlobalConfig.BootCampStyle) {
          textPosY = row0PosY + GlobalConfig.row0TileSize + 10;
        } else {
          textPosY = row1PosY + GlobalConfig.row1TileSize + GlobalConfig.TileYSpace + GlobalConfig.LayoutTextOffset;
        }
      } else {
        if (GlobalConfig.BootCampStyle) {
          textPosY = row0PosY + GlobalConfig.row0TileSize + 10;
        } else {
          textPosY = row1PosY;
        }
      }

      FunctextPosY = row1PosY + GlobalConfig.row1TileSize + GlobalConfig.TileYSpace + GlobalConfig.LayoutTextOffset;

      if (!itemPosX) {
        itemPosX = AllocatePool(sizeof(UINT64) * Screen->EntryCount);
      }

      row0PosXRunning = row0PosX;
      row1PosXRunning = row1PosX;

      //DBG("EntryCount =%d\n", Screen->EntryCount);

      for (i = 0; i < (INTN)Screen->EntryCount; i++) {
        if (Screen->Entries[i]->Row == 0) {
          itemPosX[i] = row0PosXRunning;
          row0PosXRunning += EntriesWidth + EntriesGap;
        } else {
          itemPosX[i] = row1PosXRunning;
          row1PosXRunning += GlobalConfig.row1TileSize + TILE_XSPACING;
          //DBG("next item in row1 at x=%d\n", row1PosXRunning);
        }
      }

      // initial painting
      InitSelection();

      // Update FilmPlace only if not set by InitAnime
      if ((Screen->FilmPlace.Width == 0) || (Screen->FilmPlace.Height == 0)) {
        CopyMem(&Screen->FilmPlace, &BannerPlace, sizeof(BannerPlace));
      }

      //DBG("main menu inited\n");
      break;

    case MENU_FUNCTION_CLEANUP:
      FreePool(itemPosX);
      itemPosX = NULL;
      //HidePointer();
      break;

    case MENU_FUNCTION_PAINT_ALL:
      for (i = 0; i <= State->MaxIndex; i++) {
        if (Screen->Entries[i]->Row == 0) {
          if ((i >= State->FirstVisible) && (i <= State->LastVisible)) {
            DrawMainMenuEntry(
              Screen->Entries[i], (i == State->CurrentSelection) ? 1 : 0,
              itemPosX[i - State->FirstVisible], row0PosY
            );

            // create static text for the boot options if the BootCampStyle is used
            if (GlobalConfig.BootCampStyle && !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL)) {
              // clear the screen
              FillRectAreaOfScreen(
                itemPosX[i - State->FirstVisible] + (GlobalConfig.row0TileSize / 2), textPosY,
                EntriesWidth + GlobalConfig.TileXSpace, TextHeight, &MenuBackgroundPixel,
                X_IS_CENTER
              );

              // draw the text
              DrawBCSText(
                Screen->Entries[i]->Title, itemPosX[i - State->FirstVisible] + (GlobalConfig.row0TileSize / 2),
                textPosY, X_IS_CENTER
              );
            }
          }
        } else {
          DrawMainMenuEntry(
            Screen->Entries[i], (i == State->CurrentSelection)?1:0,
            itemPosX[i], row1PosY
          );
        }
      }

      // clear the text from the second row, required by the BootCampStyle
      if (
        GlobalConfig.BootCampStyle &&
        (Screen->Entries[State->LastSelection]->Row == 1) &&
        (Screen->Entries[State->CurrentSelection]->Row == 0) &&
        !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL)
      ) {
        FillRectAreaOfScreen(
          (UGAWidth >> 1), FunctextPosY,
          OldTextWidth, TextHeight, &MenuBackgroundPixel, X_IS_CENTER
        );
      }

      // something is wrong with the DrawMainMenuLabel or Screen->Entries[State->CurrentSelection]
      // and it's required to create the first selection text from here
      // used for the second row entries, when BootCampStyle is used
      if (
        GlobalConfig.BootCampStyle &&
        (Screen->Entries[State->LastSelection]->Row == 0) &&
        (Screen->Entries[State->CurrentSelection]->Row == 1) &&
        !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL)
      ) {
        DrawMainMenuLabel(
          Screen->Entries[State->CurrentSelection]->Title,
          (UGAWidth >> 1), FunctextPosY, Screen, State
        );
      }

      // something is wrong with the DrawMainMenuLabel or Screen->Entries[State->CurrentSelection]
      // and it's required to create the first selection text from here
      // used for all the entries
      if (!GlobalConfig.BootCampStyle && !(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL)) {
        DrawMainMenuLabel(
          Screen->Entries[State->CurrentSelection]->Title,
          (UGAWidth >> 1), textPosY, Screen, State
        );
      }

      DrawTextCorner(TEXT_CORNER_HELP, X_IS_LEFT);
      DrawTextCorner(TEXT_CORNER_REVISION, X_IS_RIGHT);
      //MouseBirth();
      break;

    case MENU_FUNCTION_PAINT_SELECTION:
      //HidePointer();
      if (Screen->Entries[State->LastSelection]->Row == 0) {
        DrawMainMenuEntry(
          Screen->Entries[State->LastSelection], FALSE,
          itemPosX[State->LastSelection - State->FirstVisible], row0PosY
        );
      } else {
        DrawMainMenuEntry(
          Screen->Entries[State->LastSelection], FALSE,
          itemPosX[State->LastSelection], row1PosY
        );
      }

      if (Screen->Entries[State->CurrentSelection]->Row == 0) {
        DrawMainMenuEntry(
          Screen->Entries[State->CurrentSelection], TRUE,
          itemPosX[State->CurrentSelection - State->FirstVisible], row0PosY
        );
      } else {
        DrawMainMenuEntry(
          Screen->Entries[State->CurrentSelection], TRUE,
          itemPosX[State->CurrentSelection], row1PosY
        );
      }

      // create dynamic text for the second row if BootCampStyle is used
      if (
        GlobalConfig.BootCampStyle &&
        (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL)) &&
        (Screen->Entries[State->CurrentSelection]->Row == 1)
      ) {
        DrawMainMenuLabel(
          Screen->Entries[State->CurrentSelection]->Title,
          (UGAWidth >> 1), FunctextPosY, Screen, State
        );
      }

      // create dynamic text for all the entries
      if ((!(GlobalConfig.BootCampStyle)) && (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL))) {
        DrawMainMenuLabel(
          Screen->Entries[State->CurrentSelection]->Title,
          (UGAWidth >> 1), textPosY, Screen, State
        );
      }

      DrawTextCorner(TEXT_CORNER_HELP, X_IS_LEFT);
      DrawTextCorner(TEXT_CORNER_REVISION, X_IS_RIGHT);
      break;

    case MENU_FUNCTION_PAINT_TIMEOUT:
      i = (GlobalConfig.HideBadges & HDBADGES_INLINE) ? 3 : 1;

      if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_LABEL)) {
        FillRectAreaOfScreen(
          (UGAWidth >> 1), FunctextPosY + TextHeight * i,
          OldTimeoutTextWidth, TextHeight, &MenuBackgroundPixel, X_IS_CENTER
        );
        OldTimeoutTextWidth = DrawTextXY(ParamText, (UGAWidth >> 1), FunctextPosY + TextHeight * i, X_IS_CENTER);
      }

      DrawTextCorner(TEXT_CORNER_HELP, X_IS_LEFT);
      DrawTextCorner(TEXT_CORNER_REVISION, X_IS_RIGHT);
      break;
  }
}

VOID
CreateHeaderEntries (
  REFIT_MENU_ENTRY    **Entry,
  REFIT_MENU_SCREEN   **SubScreen,
  CHAR16              *Title,
  UINTN               ID
) {
  REFIT_MENU_ENTRY    *TmpEntry = AllocateZeroPool(sizeof(REFIT_MENU_ENTRY));
  REFIT_MENU_SCREEN   *TmpSubScreen = AllocateZeroPool(sizeof(REFIT_MENU_SCREEN));

  TmpEntry->Title = PoolPrint(Title);
  TmpEntry->Image =  OptionMenu.TitleImage;
  TmpEntry->Tag = TAG_OPTIONS;

  // create the submenu
  TmpSubScreen->Title = PoolPrint(TmpEntry->Title);
  TmpSubScreen->TitleImage = TmpEntry->Image;
  TmpSubScreen->ID = ID;
  TmpSubScreen->AnimeRun = GetAnime(TmpSubScreen);

  *Entry = TmpEntry;
  *SubScreen = TmpSubScreen;
}

REFIT_MENU_ENTRY
*SubMenuGraphics () {
  REFIT_MENU_ENTRY      *Entry = NULL;
  REFIT_MENU_SCREEN     *SubScreen = NULL;
  INTN                  i = 0;

  CreateHeaderEntries(&Entry, &SubScreen, L"Devices", SCREEN_DEVICES);

  AddMenuInfoLine(SubScreen, PoolPrint(L"Number of VideoCards=%d", NGFX));

  while (i < OptMenuDevicesNum) {
    AddMenuBOOL(SubScreen, OPT_MENU_DEVICES[i].Title, NULL, OPT_MENU_DEVICES[i].ID);
    i++;
  }

  AddMenuEntry(SubScreen, &MenuEntryReturn);
  Entry->SubScreen = SubScreen;

  return Entry;
}

REFIT_MENU_ENTRY
*SubMenuBinaries () {
  REFIT_MENU_ENTRY      *Entry = NULL;
  REFIT_MENU_SCREEN     *SubScreen = NULL;
  INTN                  i = 0;

  CreateHeaderEntries(&Entry, &SubScreen, L"Patches", SCREEN_PATCHES);

  AddMenuInfoLine(SubScreen, PoolPrint(L"%a", gCPUStructure.BrandString));
  AddMenuInfoLine(SubScreen, PoolPrint(L"Real CPUID: 0x%06x", gCPUStructure.Signature));

  while (i < OptMenuPatchesNum) {
    AddMenuBOOL(SubScreen, OPT_MENU_PATCHES[i].Title, NULL, OPT_MENU_PATCHES[i].ID);
    i++;
  }

  AddMenuEntry(SubScreen, &MenuEntryReturn);
  Entry->SubScreen = SubScreen;

  return Entry;
}

REFIT_MENU_ENTRY
*SubMenuDropTables () {
  REFIT_MENU_ENTRY    *Entry = NULL;
  REFIT_MENU_SCREEN   *SubScreen = NULL;
  CHAR8               sign[5], OTID[9];
  INTN                i = 0;

  sign[4] = 0;
  OTID[8] = 0;

  CreateHeaderEntries(&Entry, &SubScreen, L"Tables", SCREEN_TABLES);

  AddSeparator(SubScreen, "Hehe");

  while (i < OptMenuDSDTFixesNum) {
    UINTN   ID = OPT_MENU_DSDT[i].ID;

    switch (ID) {
      case mDSDTName:
        AddMenuString(SubScreen, OPT_MENU_DSDT[i].Title, ID);
        break;

      case mDebugDSDT:
        AddMenuBOOL(SubScreen, OPT_MENU_DSDT[i].Title, NULL, ID);
        break;
    }

    i++;
  }

  AddSeparator(SubScreen, "DSDT Fixes");

  for (i = 0; i < OptMenuDSDTBitNum; i++) {
    AddMenuCheck(SubScreen, PoolPrint(L"%a", OPT_MENU_DSDTBIT[i].Title), OPT_MENU_DSDTBIT[i].Bit, mDSDTFix);
  }

  AddSeparator(SubScreen, "Drop Table");

  if (gSettings.ACPIDropTables) {
    ACPI_DROP_TABLE   *DropTable = gSettings.ACPIDropTables;


    while (DropTable) {
      CopyMem((CHAR8*)&sign, (CHAR8*)&(DropTable->Signature), 4);
      CopyMem((CHAR8*)&OTID, (CHAR8*)&(DropTable->TableId), 8);

      //MsgLog("adding to menu %a (%x) %a (%lx) L=%d(0x%x)\n",
      //       sign, DropTable->Signature,
      //       OTID, DropTable->TableId,
      //       DropTable->Length, DropTable->Length);
      AddMenuBOOL(SubScreen, PoolPrint(L"Drop \"%4.4a\" \"%8.8a\" %d", sign, OTID, DropTable->Length), &(DropTable->MenuItem), 0);
      DropTable = DropTable->Next;
    }
  }

  AddMenuBOOL(SubScreen, L"Drop all OEM SSDT", NULL, mDropOEM);

  if (ACPIPatchedAML) {
    ACPI_PATCHED_AML    *ACPIPatchedAMLTmp = ACPIPatchedAML;

    AddSeparator(SubScreen, "Patched ACPI");

    while (ACPIPatchedAMLTmp) {
      AddMenuBOOL(SubScreen, PoolPrint(L"Drop \"%s\"", ACPIPatchedAMLTmp->FileName), &(ACPIPatchedAMLTmp->MenuItem), 0);
      ACPIPatchedAMLTmp = ACPIPatchedAMLTmp->Next;
    }
  }

  AddMenuEntry(SubScreen, &MenuEntryReturn);
  Entry->SubScreen = SubScreen;

  return Entry;
}

REFIT_MENU_ENTRY
*SubMenuThemes () {
  REFIT_MENU_ENTRY      *Entry = NULL;
  REFIT_MENU_SCREEN     *SubScreen = NULL;
  S_FILES               *aTmp = aThemes;

  CreateHeaderEntries(&Entry, &SubScreen, L"Themes", SCREEN_THEMES);

  AddMenuInfoLine(SubScreen, L"Available themes:");

  //for (i = 0; i < ThemesNum; i++) {
  //  AddMenuRadio(SubScreen, PoolPrint(L"%s", ThemesList[i]), NULL, mThemes, i);
  //}

  while (aTmp) {
    AddMenuRadio (
      SubScreen,
      StrToTitle(aTmp->FileName),
      //PoolPrint(L"[%02d | %02d] - %s", aTmp->Index, OldChosenTheme, aTmp->FileName),
      NULL,
      mThemes,
      aTmp->Index
    );

    aTmp = aTmp->Next;
  }

  AddMenuEntry(SubScreen, &MenuEntryReturn);
  Entry->SubScreen = SubScreen;

  return Entry;
}

REFIT_MENU_ENTRY
*SubMenuConfigs () {
  REFIT_MENU_ENTRY      *Entry = NULL;
  REFIT_MENU_SCREEN     *SubScreen = NULL;
  S_FILES               *aTmp = aConfigs;

  CreateHeaderEntries(&Entry, &SubScreen, L"Configs", SCREEN_CONFIGS);

  AddMenuInfoLine(SubScreen, L"Available configs:");

  while (aTmp) {
    AddMenuRadio (
      SubScreen,
      StrToTitle(aTmp->FileName),
      //PoolPrint(L"[%02d | %02d] - %s", aTmp->Index, OldChosenConfig, aTmp->FileName),
      NULL,
      mConfigs,
      aTmp->Index
    );

    aTmp = aTmp->Next;
  }

  AddMenuEntry(SubScreen, &MenuEntryReturn);
  Entry->SubScreen = SubScreen;

  return Entry;
}

VOID
OptionsMenu (
  OUT REFIT_MENU_ENTRY    **ChosenEntry
) {
  REFIT_MENU_ENTRY        *TmpChosenEntry = NULL;
  MENU_STYLE_FUNC         Style = TextMenuStyle, SubStyle;
  UINTN                   MenuExit = 0, SubMenuExit;
  INTN                    EntryIndex = 0, SubMenuIndex;

  //GlobalConfig.Proportional = FALSE; //temporary disable proportional

  if (AllowGraphicsMode) {
    Style = GraphicsMenuStyle;
  }

  // remember, if you extended this menu then change procedures
  // FillInputs and ApplyInputs

  if (!(GlobalConfig.HideUIFlags & HIDEUI_FLAG_MENU_TITLE_IMAGE)) {
    OptionMenu.TitleImage = BuiltinIcon(BUILTIN_ICON_FUNC_OPTIONS);
  } else {
    OptionMenu.TitleImage = NULL;
  }

  gThemeOptionsChanged = FALSE;

  if (OptionMenu.EntryCount == 0) {
    gThemeOptionsChanged = TRUE;
    OptionMenu.ID = SCREEN_OPTIONS;
    OptionMenu.AnimeRun = GetAnime(&OptionMenu);

    *ChosenEntry = (REFIT_MENU_ENTRY*)AllocateZeroPool(sizeof(REFIT_INPUT_DIALOG));

    AddMenuString(&OptionMenu, L"BootArgs", mBootArgs);

    if (aConfigs) {
      AddMenuEntry(&OptionMenu, SubMenuConfigs());
    }

    if (AllowGraphicsMode && aThemes) {
      AddMenuEntry(&OptionMenu, SubMenuThemes());
    }

    AddMenuEntry(&OptionMenu, SubMenuBinaries());
    AddMenuEntry(&OptionMenu, SubMenuDropTables());
    AddMenuEntry(&OptionMenu, SubMenuGraphics());

    AddMenuEntry(&OptionMenu, &MenuEntryReturn);
    //DBG("option menu created entries=%d\n", OptionMenu.EntryCount);
  }

  while (!MenuExit) {

    MenuExit = RunGenericMenu(&OptionMenu, Style, &EntryIndex, ChosenEntry);
    if ((MenuExit == MENU_EXIT_ESCAPE ) || ((*ChosenEntry)->Tag == TAG_RETURN)) {
      if (gBootChanged && gThemeChanged && (GlobalConfig.TextOnly != gTextOnly)) {
        GlobalConfig.TextOnly = gTextOnly;
        gTextOnly = FALSE;
        InitScreen(FALSE);
      }
      break;
    }

    if (MenuExit == MENU_EXIT_ENTER) {
      //enter input dialog or subscreen
      if ((*ChosenEntry)->SubScreen != NULL) {
        SubMenuIndex = -1;
        SubMenuExit = 0;
        SubStyle = Style;

        while (!SubMenuExit) {
          SubMenuExit = RunGenericMenu((*ChosenEntry)->SubScreen, SubStyle, &SubMenuIndex, &TmpChosenEntry);

          if ((SubMenuExit == MENU_EXIT_ESCAPE) || (TmpChosenEntry->Tag == TAG_RETURN)) {
            ApplyInputs();
            //if ((*ChosenEntry)->SubScreen->ID == SCREEN_DSDT) {
            //  UnicodeSPrint((*ChosenEntry)->Title, 255, L"DSDT fix mask [0x%08x]", gSettings.FixDsdt);
            //    //MsgLog("@ESC: %s\n", (*ChosenEntry)->Title);
            //}
            break;
          }

          if (SubMenuExit == MENU_EXIT_ENTER) {
            //enter input dialog
            SubMenuExit = 0;
            if ((*ChosenEntry)->SubScreen->ID == SCREEN_DSDT) {
              //CHAR16 *TmpTitle;
              ApplyInputs();
              //TmpTitle = PoolPrint(L"DSDT fix mask [0x%08x]->", gSettings.FixDsdt);
              //MsgLog("@ENTER: tmp=%s\n", TmpTitle);
              //while (*TmpTitle) {
              //  *((*ChosenEntry)->Title)++ = *TmpTitle++;
              //}
              //MsgLog("@ENTER: chosen=%s\n", (*ChosenEntry)->Title);
            }
            //if (TmpChosenEntry->ShortcutDigit == 0xF1) {
            //  MenuExit = MENU_EXIT_ENTER;
            //  //DBG("Escape menu from input dialog\n");
            //  goto exit;
            //} //if F1
          }
        } //while(!SubMenuExit)
      }

      MenuExit = 0;

      //if ((*ChosenEntry)->ShortcutDigit == 0xF1) {
      //  MenuExit = MENU_EXIT_ENTER;
      //  //     DBG("Escape options menu\n");
      //  break;
      //} //if F1
    } // if MENU_EXIT_ENTER
  }

//exit:
  //GlobalConfig.Proportional = OldFontStyle;
  ApplyInputs();
}

//
// user-callable dispatcher functions
//

UINTN
RunMenu (
  IN  REFIT_MENU_SCREEN   *Screen,
  OUT REFIT_MENU_ENTRY    **ChosenEntry
) {
  INTN              index = -1;
  MENU_STYLE_FUNC   Style = TextMenuStyle;

  if (AllowGraphicsMode) {
    Style = GraphicsMenuStyle;
  }

  return RunGenericMenu(Screen, Style, &index, ChosenEntry);
}

UINTN RunMainMenu (
  IN  REFIT_MENU_SCREEN   *Screen,
  IN  INTN                DefaultSelection,
  OUT REFIT_MENU_ENTRY    **ChosenEntry
) {
  MENU_STYLE_FUNC     Style = TextMenuStyle,
                      MainStyle = TextMenuStyle;
  REFIT_MENU_ENTRY    *TempChosenEntry = 0;
  UINTN               MenuExit = 0;
  INTN                DefaultEntryIndex = DefaultSelection, SubMenuIndex;
  LOADER_ENTRY        *TempChosenEntryBkp = NULL;
  BOOLEAN             ESCLoader = FALSE;

  if (AllowGraphicsMode && !GlobalConfig.TextOnly) {
    Style = GraphicsMenuStyle;
    MainStyle = MainMenuStyle;
  } else {
    AllowGraphicsMode = FALSE;
  }

  while (!MenuExit) {
    Screen->AnimeRun = MainAnime;
    MenuExit = RunGenericMenu(Screen, MainStyle, &DefaultEntryIndex, &TempChosenEntry);
    Screen->TimeoutSeconds = 0;

    if ((MenuExit == MENU_EXIT_DETAILS) && (TempChosenEntry->SubScreen != NULL)) {
      SubMenuIndex = -1;

      if (TempChosenEntryBkp != NULL) {
        FreePool(TempChosenEntryBkp);
      }

      TempChosenEntryBkp = DuplicateLoaderEntry((LOADER_ENTRY*)TempChosenEntry);

      DecodeOptions((LOADER_ENTRY*)TempChosenEntry);
      gSettings.FlagsBits = ((LOADER_ENTRY*)TempChosenEntry)->Flags;
      ((LOADER_ENTRY*)TempChosenEntry)->Flags |= (UINT16)(gSettings.FlagsBits & 0x0FFF);

      MenuExit = RunGenericMenu(TempChosenEntry->SubScreen, Style, &SubMenuIndex, &TempChosenEntry);

      if ((MenuExit == MENU_EXIT_ENTER) && (TempChosenEntry->Tag == TAG_LOADER)) {
        DecodeOptions((LOADER_ENTRY*)TempChosenEntry);
        ((LOADER_ENTRY*)TempChosenEntry)->Flags |= (UINT16)(gSettings.FlagsBits & 0x0FFF);
        AsciiSPrint(gSettings.BootArgs, AVALUE_MAX_SIZE-1, "%s", ((LOADER_ENTRY*)TempChosenEntry)->LoadOptions);
        //DBG("\n\n\n** 1 boot with args: %a\n\n\n", gSettings.BootArgs);
      }

      if ((MenuExit == MENU_EXIT_ESCAPE) || (TempChosenEntry->Tag == TAG_RETURN)) {
        if (((REFIT_MENU_ENTRY*)TempChosenEntryBkp)->Tag == TAG_LOADER) {
          DecodeOptions(TempChosenEntryBkp);
          TempChosenEntryBkp->Flags |= (UINT16)(gSettings.FlagsBits & 0x0FFF);
          AsciiSPrint(gSettings.BootArgs, AVALUE_MAX_SIZE-1, "%s", TempChosenEntryBkp->LoadOptions);
          //DBG("\n\n\n** 2 boot with args: %a\n\n\n", gSettings.BootArgs);
          ESCLoader = TRUE;
        }

        MenuExit = 0;
      }
    }
  }

  if (ChosenEntry) {
    if (ESCLoader) {
      ((LOADER_ENTRY*)TempChosenEntry)->LoadOptions = AllocateCopyPool(
                                                        StrSize(TempChosenEntryBkp->LoadOptions),
                                                        TempChosenEntryBkp->LoadOptions
                                                      );
      ((LOADER_ENTRY*)TempChosenEntry)->Flags = TempChosenEntryBkp->Flags;
      FreePool(TempChosenEntryBkp);
    }

    *ChosenEntry = TempChosenEntry;
  }

  return MenuExit;
}