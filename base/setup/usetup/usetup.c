/*
 *  ReactOS kernel
 *  Copyright (C) 2002, 2003, 2004 ReactOS Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/*
 * COPYRIGHT:       See COPYING in the top level directory
 * PROJECT:         ReactOS text-mode setup
 * FILE:            base/setup/usetup/usetup.c
 * PURPOSE:         Text-mode setup
 * PROGRAMMER:      Casper S. Hornstrup (chorns@users.sourceforge.net)
 *                  Herv� Poussineau (hpoussin@reactos.org)
 */

#include <usetup.h>
#include <math.h>

#include "bootsup.h"
#include "chkdsk.h"
#include "cmdcons.h"
#include "format.h"
#include "drivesup.h"
#include "settings.h"

#define NDEBUG
#include <debug.h>

// HACK!
#include <strsafe.h>


/* GLOBALS ******************************************************************/

HANDLE ProcessHeap;
static UNICODE_STRING SourceRootPath;
static UNICODE_STRING SourceRootDir;
/* static */ UNICODE_STRING SourcePath;
BOOLEAN IsUnattendedSetup = FALSE;
LONG UnattendDestinationDiskNumber;
LONG UnattendDestinationPartitionNumber;
LONG UnattendMBRInstallType = -1;
LONG UnattendFormatPartition = 0;
LONG AutoPartition = 0;
WCHAR UnattendInstallationDirectory[MAX_PATH];
PWCHAR SelectedLanguageId;
WCHAR LocaleID[9];
WCHAR DefaultLanguage[20];
WCHAR DefaultKBLayout[20];
BOOLEAN RepairUpdateFlag = FALSE;
HANDLE hPnpThread = INVALID_HANDLE_VALUE;

PPARTLIST PartitionList = NULL;
PPARTENTRY TempPartition = NULL;
FORMATMACHINESTATE FormatState = Start;


/* LOCALS *******************************************************************/

static PFILE_SYSTEM_LIST FileSystemList = NULL;

static UNICODE_STRING InstallPath;

/*
 * Path to the system partition, where the boot manager resides.
 * On x86 PCs, this is usually the active partition.
 * On ARC, (u)EFI, ... platforms, this is a dedicated partition.
 *
 * For more information, see:
 * https://en.wikipedia.org/wiki/System_partition_and_boot_partition
 * http://homepage.ntlworld.com/jonathan.deboynepollard/FGA/boot-and-system-volumes.html
 * http://homepage.ntlworld.com/jonathan.deboynepollard/FGA/arc-boot-process.html
 * http://homepage.ntlworld.com/jonathan.deboynepollard/FGA/efi-boot-process.html
 * http://homepage.ntlworld.com/jonathan.deboynepollard/FGA/determining-system-volume.html
 * http://homepage.ntlworld.com/jonathan.deboynepollard/FGA/determining-boot-volume.html
 */
static UNICODE_STRING SystemRootPath;

/* Path to the install directory inside the ReactOS boot partition */
static UNICODE_STRING DestinationPath;
static UNICODE_STRING DestinationArcPath;
static UNICODE_STRING DestinationRootPath;

static WCHAR DestinationDriveLetter;    // FIXME: Is it really useful??

static HINF SetupInf;

static HSPFILEQ SetupFileQueue = NULL;

static PGENERIC_LIST ComputerList = NULL;
static PGENERIC_LIST DisplayList = NULL;
static PGENERIC_LIST KeyboardList = NULL;
static PGENERIC_LIST LayoutList = NULL;
static PGENERIC_LIST LanguageList = NULL;

static LANGID LanguageId = 0;

static ULONG RequiredPartitionDiskSpace = ~0;

/* FUNCTIONS ****************************************************************/

static VOID
PrintString(char* fmt,...)
{
    char buffer[512];
    va_list ap;
    UNICODE_STRING UnicodeString;
    ANSI_STRING AnsiString;

    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    va_end(ap);

    RtlInitAnsiString(&AnsiString, buffer);
    RtlAnsiStringToUnicodeString(&UnicodeString, &AnsiString, TRUE);
    NtDisplayString(&UnicodeString);
    RtlFreeUnicodeString(&UnicodeString);
}


static VOID
DrawBox(IN SHORT xLeft,
        IN SHORT yTop,
        IN SHORT Width,
        IN SHORT Height)
{
    COORD coPos;
    DWORD Written;

    /* draw upper left corner */
    coPos.X = xLeft;
    coPos.Y = yTop;
    FillConsoleOutputCharacterA(StdOutput,
                                0xDA, // '+',
                                1,
                                coPos,
                                &Written);

    /* draw upper edge */
    coPos.X = xLeft + 1;
    coPos.Y = yTop;
    FillConsoleOutputCharacterA(StdOutput,
                                0xC4, // '-',
                                Width - 2,
                                coPos,
                                &Written);

    /* draw upper right corner */
    coPos.X = xLeft + Width - 1;
    coPos.Y = yTop;
    FillConsoleOutputCharacterA(StdOutput,
                                0xBF, // '+',
                                1,
                                coPos,
                                &Written);

    /* Draw right edge, inner space and left edge */
    for (coPos.Y = yTop + 1; coPos.Y < yTop + Height - 1; coPos.Y++)
    {
        coPos.X = xLeft;
        FillConsoleOutputCharacterA(StdOutput,
                                    0xB3, // '|',
                                    1,
                                    coPos,
                                    &Written);

        coPos.X = xLeft + 1;
        FillConsoleOutputCharacterA(StdOutput,
                                    ' ',
                                    Width - 2,
                                    coPos,
                                    &Written);

        coPos.X = xLeft + Width - 1;
        FillConsoleOutputCharacterA(StdOutput,
                                    0xB3, // '|',
                                    1,
                                    coPos,
                                    &Written);
    }

    /* draw lower left corner */
    coPos.X = xLeft;
    coPos.Y = yTop + Height - 1;
    FillConsoleOutputCharacterA(StdOutput,
                                0xC0, // '+',
                                1,
                                coPos,
                                &Written);

    /* draw lower edge */
    coPos.X = xLeft + 1;
    coPos.Y = yTop + Height - 1;
    FillConsoleOutputCharacterA(StdOutput,
                                0xC4, // '-',
                                Width - 2,
                                coPos,
                                &Written);

    /* draw lower right corner */
    coPos.X = xLeft + Width - 1;
    coPos.Y = yTop + Height - 1;
    FillConsoleOutputCharacterA(StdOutput,
                                0xD9, // '+',
                                1,
                                coPos,
                                &Written);
}


VOID
PopupError(PCCH Text,
           PCCH Status,
           PINPUT_RECORD Ir,
           ULONG WaitEvent)
{
    SHORT yTop;
    SHORT xLeft;
    COORD coPos;
    DWORD Written;
    ULONG Length;
    ULONG MaxLength;
    ULONG Lines;
    PCHAR p;
    PCCH pnext;
    BOOLEAN LastLine;
    SHORT Width;
    SHORT Height;

    /* Count text lines and longest line */
    MaxLength = 0;
    Lines = 0;
    pnext = Text;

    while (TRUE)
    {
        p = strchr(pnext, '\n');

        if (p == NULL)
        {
            Length = strlen(pnext);
            LastLine = TRUE;
        }
        else
        {
            Length = (ULONG)(p - pnext);
            LastLine = FALSE;
        }

        Lines++;

        if (Length > MaxLength)
            MaxLength = Length;

        if (LastLine != FALSE)
            break;

        pnext = p + 1;
    }

    /* Check length of status line */
    if (Status != NULL)
    {
        Length = strlen(Status);

        if (Length > MaxLength)
            MaxLength = Length;
    }

    Width = MaxLength + 4;
    Height = Lines + 2;

    if (Status != NULL)
        Height += 2;

    yTop = (yScreen - Height) / 2;
    xLeft = (xScreen - Width) / 2;


    /* Set screen attributes */
    coPos.X = xLeft;
    for (coPos.Y = yTop; coPos.Y < yTop + Height; coPos.Y++)
    {
        FillConsoleOutputAttribute(StdOutput,
                                   FOREGROUND_RED | BACKGROUND_WHITE,
                                   Width,
                                   coPos,
                                   &Written);
    }

    DrawBox(xLeft, yTop, Width, Height);

    /* Print message text */
    coPos.Y = yTop + 1;
    pnext = Text;
    while (TRUE)
    {
        p = strchr(pnext, '\n');

        if (p == NULL)
        {
            Length = strlen(pnext);
            LastLine = TRUE;
        }
        else
        {
            Length = (ULONG)(p - pnext);
            LastLine = FALSE;
        }

        if (Length != 0)
        {
            coPos.X = xLeft + 2;
            WriteConsoleOutputCharacterA(StdOutput,
                                         pnext,
                                         Length,
                                         coPos,
                                         &Written);
        }

        if (LastLine != FALSE)
            break;

        coPos.Y++;
        pnext = p + 1;
    }

    /* Print separator line and status text */
    if (Status != NULL)
    {
        coPos.Y = yTop + Height - 3;
        coPos.X = xLeft;
        FillConsoleOutputCharacterA(StdOutput,
                                    0xC3, // '+',
                                    1,
                                    coPos,
                                    &Written);

        coPos.X = xLeft + 1;
        FillConsoleOutputCharacterA(StdOutput,
                                    0xC4, // '-',
                                    Width - 2,
                                    coPos,
                                    &Written);

        coPos.X = xLeft + Width - 1;
        FillConsoleOutputCharacterA(StdOutput,
                                    0xB4, // '+',
                                    1,
                                    coPos,
                                    &Written);

        coPos.Y++;
        coPos.X = xLeft + 2;
        WriteConsoleOutputCharacterA(StdOutput,
                                     Status,
                                     min(strlen(Status), (SIZE_T)Width - 4),
                                     coPos,
                                     &Written);
    }

    if (WaitEvent == POPUP_WAIT_NONE)
        return;

    while (TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if (WaitEvent == POPUP_WAIT_ANY_KEY ||
            Ir->Event.KeyEvent.uChar.AsciiChar == 0x0D)
        {
            return;
        }
    }
}


/*
 * Confirm quit setup
 * RETURNS
 *   TRUE: Quit setup.
 *   FALSE: Don't quit setup.
 */
static BOOL
ConfirmQuit(PINPUT_RECORD Ir)
{
    BOOL Result = FALSE;
    MUIDisplayError(ERROR_NOT_INSTALLED, NULL, POPUP_WAIT_NONE);

    while (TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3))  /* F3 */
        {
            Result = TRUE;
            break;
        }
        else if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x0D)  /* ENTER */
        {
            Result = FALSE;
            break;
        }
    }

    return Result;
}


VOID
CheckUnattendedSetup(VOID)
{
    WCHAR UnattendInfPath[MAX_PATH];
    INFCONTEXT Context;
    HINF UnattendInf;
    UINT ErrorLine;
    INT IntValue;
    PWCHAR Value;

    CombinePaths(UnattendInfPath, ARRAYSIZE(UnattendInfPath), 2, SourcePath.Buffer, L"unattend.inf");

    if (DoesFileExist(NULL, UnattendInfPath) == FALSE)
    {
        DPRINT("Does not exist: %S\n", UnattendInfPath);
        return;
    }

    /* Load 'unattend.inf' from install media. */
    UnattendInf = SetupOpenInfFileW(UnattendInfPath,
                                    NULL,
                                    INF_STYLE_WIN4,
                                    LanguageId,
                                    &ErrorLine);

    if (UnattendInf == INVALID_HANDLE_VALUE)
    {
        DPRINT("SetupOpenInfFileW() failed\n");
        return;
    }

    /* Open 'Unattend' section */
    if (!SetupFindFirstLineW(UnattendInf, L"Unattend", L"Signature", &Context))
    {
        DPRINT("SetupFindFirstLineW() failed for section 'Unattend'\n");
        SetupCloseInfFile(UnattendInf);
        return;
    }

    /* Get pointer 'Signature' key */
    if (!INF_GetData(&Context, NULL, &Value))
    {
        DPRINT("INF_GetData() failed for key 'Signature'\n");
        SetupCloseInfFile(UnattendInf);
        return;
    }

    /* Check 'Signature' string */
    if (_wcsicmp(Value, L"$ReactOS$") != 0)
    {
        DPRINT("Signature not $ReactOS$\n");
        SetupCloseInfFile(UnattendInf);
        return;
    }

    /* Check if Unattend setup is enabled */
    if (!SetupFindFirstLineW(UnattendInf, L"Unattend", L"UnattendSetupEnabled", &Context))
    {
        DPRINT("Can't find key 'UnattendSetupEnabled'\n");
        SetupCloseInfFile(UnattendInf);
        return;
    }

    if (!INF_GetData(&Context, NULL, &Value))
    {
        DPRINT("Can't read key 'UnattendSetupEnabled'\n");
        SetupCloseInfFile(UnattendInf);
        return;
    }

    if (_wcsicmp(Value, L"yes") != 0)
    {
        DPRINT("Unattend setup is disabled by 'UnattendSetupEnabled' key!\n");
        SetupCloseInfFile(UnattendInf);
        return;
    }

    /* Search for 'DestinationDiskNumber' in the 'Unattend' section */
    if (!SetupFindFirstLineW(UnattendInf, L"Unattend", L"DestinationDiskNumber", &Context))
    {
        DPRINT("SetupFindFirstLine() failed for key 'DestinationDiskNumber'\n");
        SetupCloseInfFile(UnattendInf);
        return;
    }

    if (!SetupGetIntField(&Context, 1, &IntValue))
    {
        DPRINT("SetupGetIntField() failed for key 'DestinationDiskNumber'\n");
        SetupCloseInfFile(UnattendInf);
        return;
    }

    UnattendDestinationDiskNumber = (LONG)IntValue;

    /* Search for 'DestinationPartitionNumber' in the 'Unattend' section */
    if (!SetupFindFirstLineW(UnattendInf, L"Unattend", L"DestinationPartitionNumber", &Context))
    {
        DPRINT("SetupFindFirstLine() failed for key 'DestinationPartitionNumber'\n");
        SetupCloseInfFile(UnattendInf);
        return;
    }

    if (!SetupGetIntField(&Context, 1, &IntValue))
    {
        DPRINT("SetupGetIntField() failed for key 'DestinationPartitionNumber'\n");
        SetupCloseInfFile(UnattendInf);
        return;
    }

    UnattendDestinationPartitionNumber = (LONG)IntValue;

    /* Search for 'InstallationDirectory' in the 'Unattend' section */
    if (!SetupFindFirstLineW(UnattendInf, L"Unattend", L"InstallationDirectory", &Context))
    {
        DPRINT("SetupFindFirstLine() failed for key 'InstallationDirectory'\n");
        SetupCloseInfFile(UnattendInf);
        return;
    }

    /* Get pointer 'InstallationDirectory' key */
    if (!INF_GetData(&Context, NULL, &Value))
    {
        DPRINT("INF_GetData() failed for key 'InstallationDirectory'\n");
        SetupCloseInfFile(UnattendInf);
        return;
    }

    wcscpy(UnattendInstallationDirectory, Value);

    IsUnattendedSetup = TRUE;

    /* Search for 'MBRInstallType' in the 'Unattend' section */
    if (SetupFindFirstLineW(UnattendInf, L"Unattend", L"MBRInstallType", &Context))
    {
        if (SetupGetIntField(&Context, 1, &IntValue))
        {
            UnattendMBRInstallType = IntValue;
        }
    }

    /* Search for 'FormatPartition' in the 'Unattend' section */
    if (SetupFindFirstLineW(UnattendInf, L"Unattend", L"FormatPartition", &Context))
    {
        if (SetupGetIntField(&Context, 1, &IntValue))
        {
            UnattendFormatPartition = IntValue;
        }
    }

    if (SetupFindFirstLineW(UnattendInf, L"Unattend", L"AutoPartition", &Context))
    {
        if (SetupGetIntField(&Context, 1, &IntValue))
        {
            AutoPartition = IntValue;
        }
    }

    /* search for LocaleID in the 'Unattend' section*/
    if (SetupFindFirstLineW(UnattendInf, L"Unattend", L"LocaleID", &Context))
    {
        if (INF_GetData(&Context, NULL, &Value))
        {
            LONG Id = wcstol(Value, NULL, 16);
            swprintf(LocaleID, L"%08lx", Id);
       }
    }

    SetupCloseInfFile(UnattendInf);

    DPRINT("Running unattended setup\n");
}


VOID
UpdateKBLayout(VOID)
{
    PGENERIC_LIST_ENTRY ListEntry;
    LPCWSTR pszNewLayout;

    pszNewLayout = MUIDefaultKeyboardLayout();

    if (LayoutList == NULL)
    {
        LayoutList = CreateKeyboardLayoutList(SetupInf, DefaultKBLayout);
        if (LayoutList == NULL)
        {
            /* FIXME: Handle error! */
            return;
        }
    }

    ListEntry = GetFirstListEntry(LayoutList);

    /* Search for default layout (if provided) */
    if (pszNewLayout != NULL)
    {
        while (ListEntry != NULL)
        {
            if (!wcscmp(pszNewLayout, GetListEntryUserData(ListEntry)))
            {
                SetCurrentListEntry(LayoutList, ListEntry);
                break;
            }

            ListEntry = GetNextListEntry(ListEntry);
        }
    }
}


/*
 * Displays the LanguagePage.
 *
 * Next pages: IntroPage, QuitPage
 *
 * SIDEEFFECTS
 *  Init SelectedLanguageId
 *  Init LanguageId
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
LanguagePage(PINPUT_RECORD Ir)
{
    GENERIC_LIST_UI ListUi;
    PWCHAR NewLanguageId;
    BOOL RefreshPage = FALSE;

    /* Initialize the computer settings list */
    if (LanguageList == NULL)
    {
        LanguageList = CreateLanguageList(SetupInf, DefaultLanguage);
        if (LanguageList == NULL)
        {
           PopupError("Setup failed to initialize available translations", NULL, NULL, POPUP_WAIT_NONE);
           return INTRO_PAGE;
        }
    }

    /* Load the font */
    SelectedLanguageId = DefaultLanguage;
    SetConsoleCodePage();
    UpdateKBLayout();

    /* If there's just a single language in the list skip
     * the language selection process altogether! */
    if (GenericListHasSingleEntry(LanguageList))
    {
        LanguageId = (LANGID)(wcstol(SelectedLanguageId, NULL, 16) & 0xFFFF);
        return INTRO_PAGE;
    }

    InitGenericListUi(&ListUi, LanguageList);
    DrawGenericList(&ListUi,
                    2,
                    18,
                    xScreen - 3,
                    yScreen - 3);

    ScrollToPositionGenericList(&ListUi, GetDefaultLanguageIndex());

    MUIDisplayPage(LANGUAGE_PAGE);

    while (TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_DOWN))  /* DOWN */
        {
            ScrollDownGenericList(&ListUi);
            RefreshPage = TRUE;
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_UP))  /* UP */
        {
            ScrollUpGenericList(&ListUi);
            RefreshPage = TRUE;
        }
        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_NEXT))  /* PAGE DOWN */
        {
            ScrollPageDownGenericList(&ListUi);
            RefreshPage = TRUE;
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_PRIOR))  /* PAGE UP */
        {
            ScrollPageUpGenericList(&ListUi);
            RefreshPage = TRUE;
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3))  /* F3 */
        {
            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;
            else
                RedrawGenericList(&ListUi);
        }
        else if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x0D)  /* ENTER */
        {
            SelectedLanguageId = (PWCHAR)GetListEntryUserData(GetCurrentListEntry(LanguageList));

            LanguageId = (LANGID)(wcstol(SelectedLanguageId, NULL, 16) & 0xFFFF);

            if (wcscmp(SelectedLanguageId, DefaultLanguage))
            {
                UpdateKBLayout();
            }

            /* Load the font */
            SetConsoleCodePage();

            return INTRO_PAGE;
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar > 0x60) && (Ir->Event.KeyEvent.uChar.AsciiChar < 0x7b))
        {
            /* a-z */
            GenericListKeyPress(&ListUi, Ir->Event.KeyEvent.uChar.AsciiChar);
            RefreshPage = TRUE;
        }

        if (RefreshPage)
        {
            NewLanguageId = (PWCHAR)GetListEntryUserData(GetCurrentListEntry(LanguageList));

            if (SelectedLanguageId != NewLanguageId)
            {
                /* Clear the language page */
                MUIClearPage(LANGUAGE_PAGE);

                SelectedLanguageId = NewLanguageId;

                /* Load the font */
                SetConsoleCodePage();

                /* Redraw language selection page in native language */
                MUIDisplayPage(LANGUAGE_PAGE);
            }

            RefreshPage = FALSE;
        }
    }

    return INTRO_PAGE;
}


/*
 * Start page
 *
 * Next pages:
 *  LanguagePage (at once, default)
 *  InstallIntroPage (at once, if unattended)
 *  QuitPage
 *
 * SIDEEFFECTS
 *  Init Sdi
 *  Init SourcePath
 *  Init SourceRootPath
 *  Init SourceRootDir
 *  Init SetupInf
 *  Init RequiredPartitionDiskSpace
 *  Init IsUnattendedSetup
 *  If unattended, init *List and sets the Codepage
 *  If unattended, init SelectedLanguageId
 *  If unattended, init LanguageId
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
SetupStartPage(PINPUT_RECORD Ir)
{
    NTSTATUS Status;
    WCHAR FileNameBuffer[MAX_PATH];
    INFCONTEXT Context;
    PWCHAR Value;
    UINT ErrorLine;
    PGENERIC_LIST_ENTRY ListEntry;
    INT IntValue;

    CONSOLE_SetStatusText(MUIGetString(STRING_PLEASEWAIT));

    /* Get the source path and source root path */
    Status = GetSourcePaths(&SourcePath,
                            &SourceRootPath,
                            &SourceRootDir);
    if (!NT_SUCCESS(Status))
    {
        CONSOLE_PrintTextXY(6, 15, "GetSourcePaths() failed (Status 0x%08lx)", Status);
        MUIDisplayError(ERROR_NO_SOURCE_DRIVE, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }
    DPRINT1("SourcePath: '%wZ'", &SourcePath);
    DPRINT1("SourceRootPath: '%wZ'", &SourceRootPath);
    DPRINT1("SourceRootDir: '%wZ'", &SourceRootDir);

    /* Load txtsetup.sif from install media. */
    CombinePaths(FileNameBuffer, ARRAYSIZE(FileNameBuffer), 2, SourcePath.Buffer, L"txtsetup.sif");
    SetupInf = SetupOpenInfFileW(FileNameBuffer,
                                 NULL,
                                 INF_STYLE_WIN4,
                                 LanguageId,
                                 &ErrorLine);

    if (SetupInf == INVALID_HANDLE_VALUE)
    {
        MUIDisplayError(ERROR_LOAD_TXTSETUPSIF, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    /* Open 'Version' section */
    if (!SetupFindFirstLineW(SetupInf, L"Version", L"Signature", &Context))
    {
        MUIDisplayError(ERROR_CORRUPT_TXTSETUPSIF, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    /* Get pointer 'Signature' key */
    if (!INF_GetData(&Context, NULL, &Value))
    {
        MUIDisplayError(ERROR_CORRUPT_TXTSETUPSIF, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    /* Check 'Signature' string */
    if (_wcsicmp(Value, L"$ReactOS$") != 0)
    {
        MUIDisplayError(ERROR_SIGNATURE_TXTSETUPSIF, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    /* Open 'DiskSpaceRequirements' section */
    if (!SetupFindFirstLineW(SetupInf, L"DiskSpaceRequirements", L"FreeSysPartDiskSpace", &Context))
    {
        MUIDisplayError(ERROR_CORRUPT_TXTSETUPSIF, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    /* Get the 'FreeSysPartDiskSpace' value */
    if (!SetupGetIntField(&Context, 1, &IntValue))
    {
        MUIDisplayError(ERROR_CORRUPT_TXTSETUPSIF, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    RequiredPartitionDiskSpace = (ULONG)IntValue;

    /* Start the PnP thread */
    if (hPnpThread != INVALID_HANDLE_VALUE)
    {
        NtResumeThread(hPnpThread, NULL);
        hPnpThread = INVALID_HANDLE_VALUE;
    }

    CheckUnattendedSetup();

    if (IsUnattendedSetup)
    {
        // TODO: Read options from inf
        ComputerList = CreateComputerTypeList(SetupInf);
        DisplayList = CreateDisplayDriverList(SetupInf);
        KeyboardList = CreateKeyboardDriverList(SetupInf);
        LayoutList = CreateKeyboardLayoutList(SetupInf, DefaultKBLayout);
        LanguageList = CreateLanguageList(SetupInf, DefaultLanguage);

        /* new part */
        wcscpy(SelectedLanguageId, LocaleID);
        LanguageId = (LANGID)(wcstol(SelectedLanguageId, NULL, 16) & 0xFFFF);

        /* first we hack LanguageList */
        ListEntry = GetFirstListEntry(LanguageList);
        while (ListEntry != NULL)
        {
            if (!wcsicmp(LocaleID, GetListEntryUserData(ListEntry)))
            {
                DPRINT("found %S in LanguageList\n",GetListEntryUserData(ListEntry));
                SetCurrentListEntry(LanguageList, ListEntry);
                break;
            }

            ListEntry = GetNextListEntry(ListEntry);
        }

        /* now LayoutList */
        ListEntry = GetFirstListEntry(LayoutList);
        while (ListEntry != NULL)
        {
            if (!wcsicmp(LocaleID, GetListEntryUserData(ListEntry)))
            {
                DPRINT("found %S in LayoutList\n",GetListEntryUserData(ListEntry));
                SetCurrentListEntry(LayoutList, ListEntry);
                break;
            }

            ListEntry = GetNextListEntry(ListEntry);
        }

        SetConsoleCodePage();

        return INSTALL_INTRO_PAGE;
    }

    return LANGUAGE_PAGE;
}


/*
 * Displays the IntroPage.
 *
 * Next pages:
 *  InstallIntroPage (default)
 *  RepairIntroPage
 *  LicensePage
 *  QuitPage
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
IntroPage(PINPUT_RECORD Ir)
{
    MUIDisplayPage(START_PAGE);

    while (TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3))  /* F3 */
        {
            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;

            break;
        }
        else if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x0D) /* ENTER */
        {
            return INSTALL_INTRO_PAGE;
        }
        else if (toupper(Ir->Event.KeyEvent.uChar.AsciiChar) == 'R') /* R */
        {
            return REPAIR_INTRO_PAGE;
        }
        else if (toupper(Ir->Event.KeyEvent.uChar.AsciiChar) == 'L') /* L */
        {
            return LICENSE_PAGE;
        }
    }

    return INTRO_PAGE;
}


/*
 * Displays the License page.
 *
 * Next page:
 *  IntroPage (default)
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
LicensePage(PINPUT_RECORD Ir)
{
    MUIDisplayPage(LICENSE_PAGE);

    while (TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x0D)  /* ENTER */
        {
            return INTRO_PAGE;
        }
    }

    return LICENSE_PAGE;
}


/*
 * Displays the RepairIntroPage.
 *
 * Next pages:
 *  RebootPage (default)
 *  InstallIntroPage
 *  RecoveryPage
 *  IntroPage
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
RepairIntroPage(PINPUT_RECORD Ir)
{
    MUIDisplayPage(REPAIR_INTRO_PAGE);

    while(TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x0D)  /* ENTER */
        {
            return REBOOT_PAGE;
        }
        else if (toupper(Ir->Event.KeyEvent.uChar.AsciiChar) == 'U')  /* U */
        {
            RepairUpdateFlag = TRUE;
            return INSTALL_INTRO_PAGE;
        }
        else if (toupper(Ir->Event.KeyEvent.uChar.AsciiChar) == 'R')  /* R */
        {
            return RECOVERY_PAGE;
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE))  /* ESC */
        {
            return INTRO_PAGE;
        }
    }

    return REPAIR_INTRO_PAGE;
}

/*
 * Displays the InstallIntroPage.
 *
 * Next pages:
 *  DeviceSettingsPage  (At once if repair or update is selected)
 *  SelectPartitionPage (At once if unattended setup)
 *  DeviceSettingsPage  (default)
 *  QuitPage
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
InstallIntroPage(PINPUT_RECORD Ir)
{
    MUIDisplayPage(INSTALL_INTRO_PAGE);

    if (RepairUpdateFlag)
    {
        //return SELECT_PARTITION_PAGE;
        return DEVICE_SETTINGS_PAGE;
    }

    if (IsUnattendedSetup)
    {
        return SELECT_PARTITION_PAGE;
    }

    while (TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3)) /* F3 */
        {
            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;

            break;
        }
        else if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x0D) /* ENTER */
        {
            return DEVICE_SETTINGS_PAGE;
            // return SCSI_CONTROLLER_PAGE;
        }
    }

    return INSTALL_INTRO_PAGE;
}


#if 0
static PAGE_NUMBER
ScsiControllerPage(PINPUT_RECORD Ir)
{
    SetTextXY(6, 8, "Setup detected the following mass storage devices:");

    /* FIXME: print loaded mass storage driver descriptions */
#if 0
    SetTextXY(8, 10, "TEST device");
#endif


    SetStatusText("   ENTER = Continue   F3 = Quit");

    while (TRUE)
    {
        ConInKey(Ir);

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3)) /* F3 */
        {
            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;

            break;
        }
        else if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x0D) /* ENTER */
        {
            return DEVICE_SETTINGS_PAGE;
        }
    }

    return SCSI_CONTROLLER_PAGE;
}
#endif


/*
 * Displays the DeviceSettingsPage.
 *
 * Next pages:
 *  SelectPartitionPage (At once if repair or update is selected)
 *  ComputerSettingsPage
 *  DisplaySettingsPage
 *  KeyboardSettingsPage
 *  LayoutsettingsPage
 *  SelectPartitionPage
 *  QuitPage
 *
 * SIDEEFFECTS
 *  Init ComputerList
 *  Init DisplayList
 *  Init KeyboardList
 *  Init LayoutList
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
DeviceSettingsPage(PINPUT_RECORD Ir)
{
    static ULONG Line = 16;
    MUIDisplayPage(DEVICE_SETTINGS_PAGE);

    /* Initialize the computer settings list */
    if (ComputerList == NULL)
    {
        ComputerList = CreateComputerTypeList(SetupInf);
        if (ComputerList == NULL)
        {
            MUIDisplayError(ERROR_LOAD_COMPUTER, Ir, POPUP_WAIT_ENTER);
            return QUIT_PAGE;
        }
    }

    /* Initialize the display settings list */
    if (DisplayList == NULL)
    {
        DisplayList = CreateDisplayDriverList(SetupInf);
        if (DisplayList == NULL)
        {
            MUIDisplayError(ERROR_LOAD_DISPLAY, Ir, POPUP_WAIT_ENTER);
            return QUIT_PAGE;
        }
    }

    /* Initialize the keyboard settings list */
    if (KeyboardList == NULL)
    {
        KeyboardList = CreateKeyboardDriverList(SetupInf);
        if (KeyboardList == NULL)
        {
            MUIDisplayError(ERROR_LOAD_KEYBOARD, Ir, POPUP_WAIT_ENTER);
            return QUIT_PAGE;
        }
    }

    /* Initialize the keyboard layout list */
    if (LayoutList == NULL)
    {
        LayoutList = CreateKeyboardLayoutList(SetupInf, DefaultKBLayout);
        if (LayoutList == NULL)
        {
            /* FIXME: report error */
            MUIDisplayError(ERROR_LOAD_KBLAYOUT, Ir, POPUP_WAIT_ENTER);
            return QUIT_PAGE;
        }
    }

    MUIDisplayPage(DEVICE_SETTINGS_PAGE);


    CONSOLE_SetTextXY(25, 11, GetListEntryText(GetCurrentListEntry((ComputerList))));
    CONSOLE_SetTextXY(25, 12, GetListEntryText(GetCurrentListEntry((DisplayList))));
    CONSOLE_SetTextXY(25, 13, GetListEntryText(GetCurrentListEntry((KeyboardList))));
    CONSOLE_SetTextXY(25, 14, GetListEntryText(GetCurrentListEntry((LayoutList))));

    CONSOLE_InvertTextXY(24, Line, 48, 1);

    if (RepairUpdateFlag)
    {
        return SELECT_PARTITION_PAGE;
    }

    while (TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_DOWN))  /* DOWN */
        {
            CONSOLE_NormalTextXY(24, Line, 48, 1);

            if (Line == 14)
                Line = 16;
            else if (Line == 16)
                Line = 11;
            else
                Line++;

            CONSOLE_InvertTextXY(24, Line, 48, 1);
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_UP))  /* UP */
        {
            CONSOLE_NormalTextXY(24, Line, 48, 1);

            if (Line == 11)
                Line = 16;
            else if (Line == 16)
                Line = 14;
            else
                Line--;

            CONSOLE_InvertTextXY(24, Line, 48, 1);
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3))  /* F3 */
        {
            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;

            break;
        }
        else if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x0D) /* ENTER */
        {
            if (Line == 11)
                return COMPUTER_SETTINGS_PAGE;
            else if (Line == 12)
                return DISPLAY_SETTINGS_PAGE;
            else if (Line == 13)
                return KEYBOARD_SETTINGS_PAGE;
            else if (Line == 14)
                return LAYOUT_SETTINGS_PAGE;
            else if (Line == 16)
                return SELECT_PARTITION_PAGE;
        }
    }

    return DEVICE_SETTINGS_PAGE;
}


/*
 * Handles generic selection lists.
 *
 * PARAMS
 * GenericList: The list to handle.
 * nextPage: The page it needs to jump to after this page.
 * Ir: The PINPUT_RECORD
 */
static PAGE_NUMBER
HandleGenericList(PGENERIC_LIST_UI ListUi,
                  PAGE_NUMBER nextPage,
                  PINPUT_RECORD Ir)
{
    while (TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_DOWN))  /* DOWN */
        {
            ScrollDownGenericList(ListUi);
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_UP))  /* UP */
        {
            ScrollUpGenericList(ListUi);
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_NEXT))  /* PAGE DOWN */
        {
            ScrollPageDownGenericList(ListUi);
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_PRIOR))  /* PAGE UP */
        {
            ScrollPageUpGenericList(ListUi);
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3))  /* F3 */
        {
            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;
            else
                RedrawGenericList(ListUi);
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE))  /* ESC */
        {
            RestoreGenericListState(ListUi->List);
            return nextPage;    // Use some "prevPage;" instead?
        }
        else if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x0D) /* ENTER */
        {
            return nextPage;
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar > 0x60) && (Ir->Event.KeyEvent.uChar.AsciiChar < 0x7b))
        {
            /* a-z */
            GenericListKeyPress(ListUi, Ir->Event.KeyEvent.uChar.AsciiChar);
        }
    }
}


/*
 * Displays the ComputerSettingsPage.
 *
 * Next pages:
 *  DeviceSettingsPage
 *  QuitPage
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
ComputerSettingsPage(PINPUT_RECORD Ir)
{
    GENERIC_LIST_UI ListUi;
    MUIDisplayPage(COMPUTER_SETTINGS_PAGE);

    InitGenericListUi(&ListUi, ComputerList);
    DrawGenericList(&ListUi,
                    2,
                    18,
                    xScreen - 3,
                    yScreen - 3);

    SaveGenericListState(ComputerList);

    return HandleGenericList(&ListUi, DEVICE_SETTINGS_PAGE, Ir);
}


/*
 * Displays the DisplaySettingsPage.
 *
 * Next pages:
 *  DeviceSettingsPage
 *  QuitPage
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
DisplaySettingsPage(PINPUT_RECORD Ir)
{
    GENERIC_LIST_UI ListUi;
    MUIDisplayPage(DISPLAY_SETTINGS_PAGE);

    InitGenericListUi(&ListUi, DisplayList);
    DrawGenericList(&ListUi,
                    2,
                    18,
                    xScreen - 3,
                    yScreen - 3);

    SaveGenericListState(DisplayList);

    return HandleGenericList(&ListUi, DEVICE_SETTINGS_PAGE, Ir);
}


/*
 * Displays the KeyboardSettingsPage.
 *
 * Next pages:
 *  DeviceSettingsPage
 *  QuitPage
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
KeyboardSettingsPage(PINPUT_RECORD Ir)
{
    GENERIC_LIST_UI ListUi;
    MUIDisplayPage(KEYBOARD_SETTINGS_PAGE);

    InitGenericListUi(&ListUi, KeyboardList);
    DrawGenericList(&ListUi,
                    2,
                    18,
                    xScreen - 3,
                    yScreen - 3);

    SaveGenericListState(KeyboardList);

    return HandleGenericList(&ListUi, DEVICE_SETTINGS_PAGE, Ir);
}


/*
 * Displays the LayoutSettingsPage.
 *
 * Next pages:
 *  DeviceSettingsPage
 *  QuitPage
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
LayoutSettingsPage(PINPUT_RECORD Ir)
{
    GENERIC_LIST_UI ListUi;
    MUIDisplayPage(LAYOUT_SETTINGS_PAGE);

    InitGenericListUi(&ListUi, LayoutList);
    DrawGenericList(&ListUi,
                    2,
                    18,
                    xScreen - 3,
                    yScreen - 3);

    SaveGenericListState(LayoutList);

    return HandleGenericList(&ListUi, DEVICE_SETTINGS_PAGE, Ir);
}


static BOOL
IsDiskSizeValid(PPARTENTRY PartEntry)
{
    ULONGLONG size;

    size = PartEntry->SectorCount.QuadPart * PartEntry->DiskEntry->BytesPerSector;
    size = (size + 524288) / 1048576;  /* in MBytes */

    if (size < RequiredPartitionDiskSpace)
    {
        /* partition is too small so ask for another partition */
        DPRINT1("Partition is too small (size: %I64u MB), required disk space is %lu MB\n", size, RequiredPartitionDiskSpace);
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}


/*
 * Displays the SelectPartitionPage.
 *
 * Next pages:
 *  SelectFileSystemPage (At once if unattended)
 *  SelectFileSystemPage (Default if free space is selected)
 *  CreatePrimaryPartitionPage
 *  CreateExtendedPartitionPage
 *  CreateLogicalPartitionPage
 *  ConfirmDeleteSystemPartitionPage (if the selected partition is the system partition, aka with the boot flag set)
 *  DeletePartitionPage
 *  QuitPage
 *
 * SIDEEFFECTS
 *  Init DestinationDriveLetter (only if unattended or not free space is selected)
 *  Set InstallShortcut (only if not unattended + free space is selected)
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
SelectPartitionPage(PINPUT_RECORD Ir)
{
    PARTLIST_UI ListUi;
    ULONG Error;

    MUIDisplayPage(SELECT_PARTITION_PAGE);

    if (PartitionList == NULL)
    {
        PartitionList = CreatePartitionList();
        if (PartitionList == NULL)
        {
            /* FIXME: show an error dialog */
            MUIDisplayError(ERROR_DRIVE_INFORMATION, Ir, POPUP_WAIT_ENTER);
            return QUIT_PAGE;
        }
        else if (IsListEmpty(&PartitionList->DiskListHead))
        {
            MUIDisplayError(ERROR_NO_HDD, Ir, POPUP_WAIT_ENTER);
            return QUIT_PAGE;
        }

        TempPartition = NULL;
        FormatState = Start;
    }

    InitPartitionListUi(&ListUi, PartitionList,
                        2,
                        23,
                        xScreen - 3,
                        yScreen - 3);
    DrawPartitionList(&ListUi);

    if (IsUnattendedSetup)
    {
        if (!SelectPartition(PartitionList, UnattendDestinationDiskNumber, UnattendDestinationPartitionNumber))
        {
            if (AutoPartition)
            {
                if (PartitionList->CurrentPartition->LogicalPartition)
                {
                    CreateLogicalPartition(PartitionList,
                                           PartitionList->CurrentPartition->SectorCount.QuadPart,
                                           TRUE);
                }
                else
                {
                    CreatePrimaryPartition(PartitionList,
                                           PartitionList->CurrentPartition->SectorCount.QuadPart,
                                           TRUE);
                }

// FIXME?? Aren't we going to enter an infinite loop, if this test fails??
                if (!IsDiskSizeValid(PartitionList->CurrentPartition))
                {
                    MUIDisplayError(ERROR_INSUFFICIENT_PARTITION_SIZE, Ir, POPUP_WAIT_ANY_KEY,
                                    RequiredPartitionDiskSpace);
                    return SELECT_PARTITION_PAGE; /* let the user select another partition */
                }

                DestinationDriveLetter = (WCHAR)PartitionList->CurrentPartition->DriveLetter;

                return SELECT_FILE_SYSTEM_PAGE;
            }
        }
        else
        {
            DrawPartitionList(&ListUi);

// FIXME?? Aren't we going to enter an infinite loop, if this test fails??
            if (!IsDiskSizeValid(PartitionList->CurrentPartition))
            {
                MUIDisplayError(ERROR_INSUFFICIENT_PARTITION_SIZE, Ir, POPUP_WAIT_ANY_KEY,
                                RequiredPartitionDiskSpace);
                return SELECT_PARTITION_PAGE; /* let the user select another partition */
            }

            DestinationDriveLetter = (WCHAR)PartitionList->CurrentPartition->DriveLetter;

            return SELECT_FILE_SYSTEM_PAGE;
        }
    }

    while (TRUE)
    {
        /* Update status text */
        if (PartitionList->CurrentPartition == NULL)
        {
            CONSOLE_SetStatusText(MUIGetString(STRING_INSTALLCREATEPARTITION));
        }
        else if (PartitionList->CurrentPartition->LogicalPartition)
        {
            if (PartitionList->CurrentPartition->IsPartitioned)
            {
                CONSOLE_SetStatusText(MUIGetString(STRING_DELETEPARTITION));
            }
            else
            {
                CONSOLE_SetStatusText(MUIGetString(STRING_INSTALLCREATELOGICAL));
            }
        }
        else
        {
            if (PartitionList->CurrentPartition->IsPartitioned)
            {
                if (IsContainerPartition(PartitionList->CurrentPartition->PartitionType))
                {
                    CONSOLE_SetStatusText(MUIGetString(STRING_DELETEPARTITION));
                }
                else
                {
                    CONSOLE_SetStatusText(MUIGetString(STRING_INSTALLDELETEPARTITION));
                }
            }
            else
            {
                CONSOLE_SetStatusText(MUIGetString(STRING_INSTALLCREATEPARTITION));
            }
        }

        CONSOLE_ConInKey(Ir);

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3))  /* F3 */
        {
            if (ConfirmQuit(Ir) != FALSE)
            {
                DestroyPartitionList(PartitionList);
                PartitionList = NULL;
                return QUIT_PAGE;
            }

            break;
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_DOWN))  /* DOWN */
        {
            ScrollDownPartitionList(&ListUi);
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_UP))  /* UP */
        {
            ScrollUpPartitionList(&ListUi);
        }
        else if (Ir->Event.KeyEvent.wVirtualKeyCode == VK_RETURN)  /* ENTER */
        {
            if (IsContainerPartition(PartitionList->CurrentPartition->PartitionType))
                continue; // return SELECT_PARTITION_PAGE;

            if (PartitionList->CurrentPartition == NULL ||
                PartitionList->CurrentPartition->IsPartitioned == FALSE)
            {
                if (PartitionList->CurrentPartition->LogicalPartition)
                {
                    CreateLogicalPartition(PartitionList,
                                           0ULL,
                                           TRUE);
                }
                else
                {
                    CreatePrimaryPartition(PartitionList,
                                           0ULL,
                                           TRUE);
                }
            }

            if (!IsDiskSizeValid(PartitionList->CurrentPartition))
            {
                MUIDisplayError(ERROR_INSUFFICIENT_PARTITION_SIZE, Ir, POPUP_WAIT_ANY_KEY,
                                RequiredPartitionDiskSpace);
                return SELECT_PARTITION_PAGE; /* let the user select another partition */
            }

            DestinationDriveLetter = (WCHAR)PartitionList->CurrentPartition->DriveLetter;

            return SELECT_FILE_SYSTEM_PAGE;
        }
        else if (Ir->Event.KeyEvent.wVirtualKeyCode == 'P')  /* P */
        {
            if (PartitionList->CurrentPartition->LogicalPartition == FALSE)
            {
                Error = PrimaryPartitionCreationChecks(PartitionList);
                if (Error != NOT_AN_ERROR)
                {
                    MUIDisplayError(Error, Ir, POPUP_WAIT_ANY_KEY);
                    return SELECT_PARTITION_PAGE;
                }

                return CREATE_PRIMARY_PARTITION_PAGE;
            }
        }
        else if (Ir->Event.KeyEvent.wVirtualKeyCode == 'E')  /* E */
        {
            if (PartitionList->CurrentPartition->LogicalPartition == FALSE)
            {
                Error = ExtendedPartitionCreationChecks(PartitionList);
                if (Error != NOT_AN_ERROR)
                {
                    MUIDisplayError(Error, Ir, POPUP_WAIT_ANY_KEY);
                    return SELECT_PARTITION_PAGE;
                }

                return CREATE_EXTENDED_PARTITION_PAGE;
            }
        }
        else if (Ir->Event.KeyEvent.wVirtualKeyCode == 'L')  /* L */
        {
            if (PartitionList->CurrentPartition->LogicalPartition != FALSE)
            {
                Error = LogicalPartitionCreationChecks(PartitionList);
                if (Error != NOT_AN_ERROR)
                {
                    MUIDisplayError(Error, Ir, POPUP_WAIT_ANY_KEY);
                    return SELECT_PARTITION_PAGE;
                }

                return CREATE_LOGICAL_PARTITION_PAGE;
            }
        }
        else if (Ir->Event.KeyEvent.wVirtualKeyCode == 'D')  /* D */
        {
            if (PartitionList->CurrentPartition->IsPartitioned == FALSE)
            {
                MUIDisplayError(ERROR_DELETE_SPACE, Ir, POPUP_WAIT_ANY_KEY);
                return SELECT_PARTITION_PAGE;
            }

            if (PartitionList->CurrentPartition->BootIndicator ||
                PartitionList->CurrentPartition == PartitionList->SystemPartition)
            {
                return CONFIRM_DELETE_SYSTEM_PARTITION_PAGE;
            }

            return DELETE_PARTITION_PAGE;
        }
    }

    return SELECT_PARTITION_PAGE;
}


#define PARTITION_SIZE_INPUT_FIELD_LENGTH 9
/* Restriction for MaxSize: pow(10, (PARTITION_SIZE_INPUT_FIELD_LENGTH - 1)) - 1 */
#define PARTITION_MAXSIZE (pow(10, (PARTITION_SIZE_INPUT_FIELD_LENGTH - 1)) - 1)

static VOID
ShowPartitionSizeInputBox(SHORT Left,
                          SHORT Top,
                          SHORT Right,
                          SHORT Bottom,
                          ULONG MaxSize,
                          PWSTR InputBuffer,
                          PBOOLEAN Quit,
                          PBOOLEAN Cancel)
{
    INPUT_RECORD Ir;
    COORD coPos;
    DWORD Written;
    CHAR Buffer[128];
    INT Length, Pos;
    WCHAR ch;
    SHORT iLeft;
    SHORT iTop;

    if (Quit != NULL)
        *Quit = FALSE;

    if (Cancel != NULL)
        *Cancel = FALSE;

    DrawBox(Left, Top, Right - Left + 1, Bottom - Top + 1);

    /* Print message */
    coPos.X = Left + 2;
    coPos.Y = Top + 2;
    strcpy(Buffer, MUIGetString(STRING_PARTITIONSIZE));
    iLeft = coPos.X + strlen(Buffer) + 1;
    iTop = coPos.Y;

    WriteConsoleOutputCharacterA(StdOutput,
                                 Buffer,
                                 strlen(Buffer),
                                 coPos,
                                 &Written);

    sprintf(Buffer, MUIGetString(STRING_MAXSIZE), MaxSize);
    coPos.X = iLeft + PARTITION_SIZE_INPUT_FIELD_LENGTH + 1;
    coPos.Y = iTop;
    WriteConsoleOutputCharacterA(StdOutput,
                                 Buffer,
                                 strlen(Buffer),
                                 coPos,
                                 &Written);

    swprintf(InputBuffer, L"%lu", MaxSize);
    Length = wcslen(InputBuffer);
    Pos = Length;
    CONSOLE_SetInputTextXY(iLeft,
                           iTop,
                           PARTITION_SIZE_INPUT_FIELD_LENGTH,
                           InputBuffer);
    CONSOLE_SetCursorXY(iLeft + Length, iTop);
    CONSOLE_SetCursorType(TRUE, TRUE);

    while (TRUE)
    {
        CONSOLE_ConInKey(&Ir);

        if ((Ir.Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir.Event.KeyEvent.wVirtualKeyCode == VK_F3))  /* F3 */
        {
            if (Quit != NULL)
                *Quit = TRUE;

            InputBuffer[0] = UNICODE_NULL;
            CONSOLE_SetCursorType(TRUE, FALSE);
            break;
        }
        else if (Ir.Event.KeyEvent.wVirtualKeyCode == VK_RETURN)    /* ENTER */
        {
            CONSOLE_SetCursorType(TRUE, FALSE);
            break;
        }
        else if (Ir.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE)    /* ESCAPE */
        {
            if (Cancel != NULL)
                *Cancel = TRUE;

            InputBuffer[0] = UNICODE_NULL;
            CONSOLE_SetCursorType(TRUE, FALSE);
            break;
        }
        else if ((Ir.Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir.Event.KeyEvent.wVirtualKeyCode == VK_HOME))  /* HOME */
        {
            Pos = 0;
            CONSOLE_SetCursorXY(iLeft + Pos, iTop);
        }
        else if ((Ir.Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir.Event.KeyEvent.wVirtualKeyCode == VK_END))  /* END */
        {
            Pos = Length;
            CONSOLE_SetCursorXY(iLeft + Pos, iTop);
        }
        else if ((Ir.Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir.Event.KeyEvent.wVirtualKeyCode == VK_LEFT))  /* LEFT */
        {
            if (Pos > 0)
            {
                Pos--;
                CONSOLE_SetCursorXY(iLeft + Pos, iTop);
            }
        }
        else if ((Ir.Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir.Event.KeyEvent.wVirtualKeyCode == VK_RIGHT))  /* RIGHT */
        {
            if (Pos < Length)
            {
                Pos++;
                CONSOLE_SetCursorXY(iLeft + Pos, iTop);
            }
        }
        else if ((Ir.Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir.Event.KeyEvent.wVirtualKeyCode == VK_DELETE))  /* DEL */
        {
            if (Pos < Length)
            {
                memmove(&InputBuffer[Pos],
                        &InputBuffer[Pos + 1],
                        (Length - Pos - 1) * sizeof(WCHAR));
                InputBuffer[Length - 1] = UNICODE_NULL;

                Length--;
                CONSOLE_SetInputTextXY(iLeft,
                                       iTop,
                                       PARTITION_SIZE_INPUT_FIELD_LENGTH,
                                       InputBuffer);
                CONSOLE_SetCursorXY(iLeft + Pos, iTop);
            }
        }
        else if (Ir.Event.KeyEvent.wVirtualKeyCode == VK_BACK)  /* BACKSPACE */
        {
            if (Pos > 0)
            {
                if (Pos < Length)
                    memmove(&InputBuffer[Pos - 1],
                            &InputBuffer[Pos],
                            (Length - Pos) * sizeof(WCHAR));
                InputBuffer[Length - 1] = UNICODE_NULL;

                Pos--;
                Length--;
                CONSOLE_SetInputTextXY(iLeft,
                                       iTop,
                                       PARTITION_SIZE_INPUT_FIELD_LENGTH,
                                       InputBuffer);
                CONSOLE_SetCursorXY(iLeft + Pos, iTop);
            }
        }
        else if (Ir.Event.KeyEvent.uChar.AsciiChar != 0x00)
        {
            if (Length < PARTITION_SIZE_INPUT_FIELD_LENGTH - 1)
            {
                ch = (WCHAR)Ir.Event.KeyEvent.uChar.AsciiChar;

                if ((ch >= L'0') && (ch <= L'9'))
                {
                    if (Pos < Length)
                        memmove(&InputBuffer[Pos + 1],
                                &InputBuffer[Pos],
                                (Length - Pos) * sizeof(WCHAR));
                    InputBuffer[Length + 1] = UNICODE_NULL;
                    InputBuffer[Pos] = ch;

                    Pos++;
                    Length++;
                    CONSOLE_SetInputTextXY(iLeft,
                                           iTop,
                                           PARTITION_SIZE_INPUT_FIELD_LENGTH,
                                           InputBuffer);
                    CONSOLE_SetCursorXY(iLeft + Pos, iTop);
                }
            }
        }
    }
}


/*
 * Displays the CreatePrimaryPartitionPage.
 *
 * Next pages:
 *  SelectPartitionPage
 *  SelectFileSystemPage (default)
 *  QuitPage
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
CreatePrimaryPartitionPage(PINPUT_RECORD Ir)
{
    PDISKENTRY DiskEntry;
    PPARTENTRY PartEntry;
    BOOLEAN Quit;
    BOOLEAN Cancel;
    WCHAR InputBuffer[50];
    ULONG MaxSize;
    ULONGLONG PartSize;
    ULONGLONG DiskSize;
    ULONGLONG SectorCount;
    PCHAR Unit;

    if (PartitionList == NULL ||
        PartitionList->CurrentDisk == NULL ||
        PartitionList->CurrentPartition == NULL)
    {
        /* FIXME: show an error dialog */
        return QUIT_PAGE;
    }

    DiskEntry = PartitionList->CurrentDisk;
    PartEntry = PartitionList->CurrentPartition;

    CONSOLE_SetStatusText(MUIGetString(STRING_PLEASEWAIT));

    CONSOLE_SetTextXY(6, 8, MUIGetString(STRING_CHOOSENEWPARTITION));

    DiskSize = DiskEntry->SectorCount.QuadPart * DiskEntry->BytesPerSector;
#if 0
    if (DiskSize >= 10737418240) /* 10 GB */
    {
        DiskSize = DiskSize / 1073741824;
        Unit = MUIGetString(STRING_GB);
    }
    else
#endif
    {
        DiskSize = DiskSize / 1048576;
        if (DiskSize == 0)
            DiskSize = 1;

        Unit = MUIGetString(STRING_MB);
    }

    if (DiskEntry->DriverName.Length > 0)
    {
        CONSOLE_PrintTextXY(6, 10,
                            MUIGetString(STRING_HDINFOPARTCREATE),
                            DiskSize,
                            Unit,
                            DiskEntry->DiskNumber,
                            DiskEntry->Port,
                            DiskEntry->Bus,
                            DiskEntry->Id,
                            &DiskEntry->DriverName);
    }
    else
    {
        CONSOLE_PrintTextXY(6, 10,
                            MUIGetString(STRING_HDDINFOUNK1),
                            DiskSize,
                            Unit,
                            DiskEntry->DiskNumber,
                            DiskEntry->Port,
                            DiskEntry->Bus,
                            DiskEntry->Id);
    }

    CONSOLE_SetTextXY(6, 12, MUIGetString(STRING_HDDSIZE));

#if 0
    CONSOLE_PrintTextXY(8, 10, "Maximum size of the new partition is %I64u MB",
                        PartitionList->CurrentPartition->SectorCount * DiskEntry->BytesPerSector / 1048576);
#endif

    CONSOLE_SetStatusText(MUIGetString(STRING_CREATEPARTITION));

    PartEntry = PartitionList->CurrentPartition;
    while (TRUE)
    {
        MaxSize = (PartEntry->SectorCount.QuadPart * DiskEntry->BytesPerSector) / 1048576;  /* in MBytes (rounded) */

        if (MaxSize > PARTITION_MAXSIZE)
            MaxSize = PARTITION_MAXSIZE;

        ShowPartitionSizeInputBox(12, 14, xScreen - 12, 17, /* left, top, right, bottom */
                                  MaxSize, InputBuffer, &Quit, &Cancel);

        if (Quit != FALSE)
        {
            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;

            break;
        }
        else if (Cancel != FALSE)
        {
            return SELECT_PARTITION_PAGE;
        }
        else
        {
            PartSize = _wcstoui64(InputBuffer, NULL, 10);

            if (PartSize < 1)
            {
                /* Too small */
                continue;
            }

            if (PartSize > MaxSize)
            {
                /* Too large */
                continue;
            }

            /* Convert to bytes */
            if (PartSize == MaxSize)
            {
                /* Use all of the unpartitioned disk space */
                SectorCount = PartEntry->SectorCount.QuadPart;
            }
            else
            {
                /* Calculate the sector count from the size in MB */
                SectorCount = PartSize * 1048576 / DiskEntry->BytesPerSector;

                /* But never get larger than the unpartitioned disk space */
                if (SectorCount > PartEntry->SectorCount.QuadPart)
                    SectorCount = PartEntry->SectorCount.QuadPart;
            }

            DPRINT ("Partition size: %I64u bytes\n", PartSize);

            CreatePrimaryPartition(PartitionList,
                                   SectorCount,
                                   FALSE);

            return SELECT_PARTITION_PAGE;
        }
    }

    return CREATE_PRIMARY_PARTITION_PAGE;
}


/*
 * Displays the CreateExtendedPartitionPage.
 *
 * Next pages:
 *  SelectPartitionPage (default)
 *  QuitPage
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
CreateExtendedPartitionPage(PINPUT_RECORD Ir)
{
    PDISKENTRY DiskEntry;
    PPARTENTRY PartEntry;
    BOOLEAN Quit;
    BOOLEAN Cancel;
    WCHAR InputBuffer[50];
    ULONG MaxSize;
    ULONGLONG PartSize;
    ULONGLONG DiskSize;
    ULONGLONG SectorCount;
    PCHAR Unit;

    if (PartitionList == NULL ||
        PartitionList->CurrentDisk == NULL ||
        PartitionList->CurrentPartition == NULL)
    {
        /* FIXME: show an error dialog */
        return QUIT_PAGE;
    }

    DiskEntry = PartitionList->CurrentDisk;
    PartEntry = PartitionList->CurrentPartition;

    CONSOLE_SetStatusText(MUIGetString(STRING_PLEASEWAIT));

    CONSOLE_SetTextXY(6, 8, MUIGetString(STRING_CHOOSE_NEW_EXTENDED_PARTITION));

    DiskSize = DiskEntry->SectorCount.QuadPart * DiskEntry->BytesPerSector;
#if 0
    if (DiskSize >= 10737418240) /* 10 GB */
    {
        DiskSize = DiskSize / 1073741824;
        Unit = MUIGetString(STRING_GB);
    }
    else
#endif
    {
        DiskSize = DiskSize / 1048576;
        if (DiskSize == 0)
            DiskSize = 1;

        Unit = MUIGetString(STRING_MB);
    }

    if (DiskEntry->DriverName.Length > 0)
    {
        CONSOLE_PrintTextXY(6, 10,
                            MUIGetString(STRING_HDINFOPARTCREATE),
                            DiskSize,
                            Unit,
                            DiskEntry->DiskNumber,
                            DiskEntry->Port,
                            DiskEntry->Bus,
                            DiskEntry->Id,
                            &DiskEntry->DriverName);
    }
    else
    {
        CONSOLE_PrintTextXY(6, 10,
                            MUIGetString(STRING_HDDINFOUNK1),
                            DiskSize,
                            Unit,
                            DiskEntry->DiskNumber,
                            DiskEntry->Port,
                            DiskEntry->Bus,
                            DiskEntry->Id);
    }

    CONSOLE_SetTextXY(6, 12, MUIGetString(STRING_HDDSIZE));

#if 0
    CONSOLE_PrintTextXY(8, 10, "Maximum size of the new partition is %I64u MB",
                        PartitionList->CurrentPartition->SectorCount * DiskEntry->BytesPerSector / 1048576);
#endif

    CONSOLE_SetStatusText(MUIGetString(STRING_CREATEPARTITION));

    PartEntry = PartitionList->CurrentPartition;
    while (TRUE)
    {
        MaxSize = (PartEntry->SectorCount.QuadPart * DiskEntry->BytesPerSector) / 1048576;  /* in MBytes (rounded) */

        if (MaxSize > PARTITION_MAXSIZE)
            MaxSize = PARTITION_MAXSIZE;

        ShowPartitionSizeInputBox(12, 14, xScreen - 12, 17, /* left, top, right, bottom */
                                  MaxSize, InputBuffer, &Quit, &Cancel);

        if (Quit != FALSE)
        {
            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;

            break;
        }
        else if (Cancel != FALSE)
        {
            return SELECT_PARTITION_PAGE;
        }
        else
        {
            PartSize = _wcstoui64(InputBuffer, NULL, 10);

            if (PartSize < 1)
            {
                /* Too small */
                continue;
            }

            if (PartSize > MaxSize)
            {
                /* Too large */
                continue;
            }

            /* Convert to bytes */
            if (PartSize == MaxSize)
            {
                /* Use all of the unpartitioned disk space */
                SectorCount = PartEntry->SectorCount.QuadPart;
            }
            else
            {
                /* Calculate the sector count from the size in MB */
                SectorCount = PartSize * 1048576 / DiskEntry->BytesPerSector;

                /* But never get larger than the unpartitioned disk space */
                if (SectorCount > PartEntry->SectorCount.QuadPart)
                    SectorCount = PartEntry->SectorCount.QuadPart;
            }

            DPRINT ("Partition size: %I64u bytes\n", PartSize);

            CreateExtendedPartition(PartitionList,
                                    SectorCount);

            return SELECT_PARTITION_PAGE;
        }
    }

    return CREATE_EXTENDED_PARTITION_PAGE;
}


/*
 * Displays the CreateLogicalPartitionPage.
 *
 * Next pages:
 *  SelectFileSystemPage (default)
 *  QuitPage
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
CreateLogicalPartitionPage(PINPUT_RECORD Ir)
{
    PDISKENTRY DiskEntry;
    PPARTENTRY PartEntry;
    BOOLEAN Quit;
    BOOLEAN Cancel;
    WCHAR InputBuffer[50];
    ULONG MaxSize;
    ULONGLONG PartSize;
    ULONGLONG DiskSize;
    ULONGLONG SectorCount;
    PCHAR Unit;

    if (PartitionList == NULL ||
        PartitionList->CurrentDisk == NULL ||
        PartitionList->CurrentPartition == NULL)
    {
        /* FIXME: show an error dialog */
        return QUIT_PAGE;
    }

    DiskEntry = PartitionList->CurrentDisk;
    PartEntry = PartitionList->CurrentPartition;

    CONSOLE_SetStatusText(MUIGetString(STRING_PLEASEWAIT));

    CONSOLE_SetTextXY(6, 8, MUIGetString(STRING_CHOOSE_NEW_LOGICAL_PARTITION));

    DiskSize = DiskEntry->SectorCount.QuadPart * DiskEntry->BytesPerSector;
#if 0
    if (DiskSize >= 10737418240) /* 10 GB */
    {
        DiskSize = DiskSize / 1073741824;
        Unit = MUIGetString(STRING_GB);
    }
    else
#endif
    {
        DiskSize = DiskSize / 1048576;
        if (DiskSize == 0)
            DiskSize = 1;

        Unit = MUIGetString(STRING_MB);
    }

    if (DiskEntry->DriverName.Length > 0)
    {
        CONSOLE_PrintTextXY(6, 10,
                            MUIGetString(STRING_HDINFOPARTCREATE),
                            DiskSize,
                            Unit,
                            DiskEntry->DiskNumber,
                            DiskEntry->Port,
                            DiskEntry->Bus,
                            DiskEntry->Id,
                            &DiskEntry->DriverName);
    }
    else
    {
        CONSOLE_PrintTextXY(6, 10,
                            MUIGetString(STRING_HDDINFOUNK1),
                            DiskSize,
                            Unit,
                            DiskEntry->DiskNumber,
                            DiskEntry->Port,
                            DiskEntry->Bus,
                            DiskEntry->Id);
    }

    CONSOLE_SetTextXY(6, 12, MUIGetString(STRING_HDDSIZE));

#if 0
    CONSOLE_PrintTextXY(8, 10, "Maximum size of the new partition is %I64u MB",
                        PartitionList->CurrentPartition->SectorCount * DiskEntry->BytesPerSector / 1048576);
#endif

    CONSOLE_SetStatusText(MUIGetString(STRING_CREATEPARTITION));

    PartEntry = PartitionList->CurrentPartition;
    while (TRUE)
    {
        MaxSize = (PartEntry->SectorCount.QuadPart * DiskEntry->BytesPerSector) / 1048576;  /* in MBytes (rounded) */

        if (MaxSize > PARTITION_MAXSIZE)
            MaxSize = PARTITION_MAXSIZE;

        ShowPartitionSizeInputBox(12, 14, xScreen - 12, 17, /* left, top, right, bottom */
                                  MaxSize, InputBuffer, &Quit, &Cancel);

        if (Quit != FALSE)
        {
            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;

            break;
        }
        else if (Cancel != FALSE)
        {
            return SELECT_PARTITION_PAGE;
        }
        else
        {
            PartSize = _wcstoui64(InputBuffer, NULL, 10);

            if (PartSize < 1)
            {
                /* Too small */
                continue;
            }

            if (PartSize > MaxSize)
            {
                /* Too large */
                continue;
            }

            /* Convert to bytes */
            if (PartSize == MaxSize)
            {
                /* Use all of the unpartitioned disk space */
                SectorCount = PartEntry->SectorCount.QuadPart;
            }
            else
            {
                /* Calculate the sector count from the size in MB */
                SectorCount = PartSize * 1048576 / DiskEntry->BytesPerSector;

                /* But never get larger than the unpartitioned disk space */
                if (SectorCount > PartEntry->SectorCount.QuadPart)
                    SectorCount = PartEntry->SectorCount.QuadPart;
            }

            DPRINT("Partition size: %I64u bytes\n", PartSize);

            CreateLogicalPartition(PartitionList,
                                   SectorCount,
                                   FALSE);

            return SELECT_PARTITION_PAGE;
        }
    }

    return CREATE_LOGICAL_PARTITION_PAGE;
}


/*
 * Displays the ConfirmDeleteSystemPartitionPage.
 *
 * Next pages:
 *  DeletePartitionPage (default)
 *  SelectPartitionPage
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
ConfirmDeleteSystemPartitionPage(PINPUT_RECORD Ir)
{
    MUIDisplayPage(CONFIRM_DELETE_SYSTEM_PARTITION_PAGE);

    while (TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3))  /* F3 */
        {
            if (ConfirmQuit(Ir) == TRUE)
                return QUIT_PAGE;

            break;
        }
        else if (Ir->Event.KeyEvent.wVirtualKeyCode == VK_RETURN) /* ENTER */
        {
            return DELETE_PARTITION_PAGE;
        }
        else if (Ir->Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE)  /* ESC */
        {
            return SELECT_PARTITION_PAGE;
        }
    }

    return CONFIRM_DELETE_SYSTEM_PARTITION_PAGE;
}


/*
 * Displays the DeletePartitionPage.
 *
 * Next pages:
 *  SelectPartitionPage (default)
 *  QuitPage
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
DeletePartitionPage(PINPUT_RECORD Ir)
{
    PDISKENTRY DiskEntry;
    PPARTENTRY PartEntry;
    ULONGLONG DiskSize;
    ULONGLONG PartSize;
    PCHAR Unit;
    CHAR PartTypeString[32];

    if (PartitionList == NULL ||
        PartitionList->CurrentDisk == NULL ||
        PartitionList->CurrentPartition == NULL)
    {
        /* FIXME: show an error dialog */
        return QUIT_PAGE;
    }

    DiskEntry = PartitionList->CurrentDisk;
    PartEntry = PartitionList->CurrentPartition;

    MUIDisplayPage(DELETE_PARTITION_PAGE);

    GetPartTypeStringFromPartitionType(PartEntry->PartitionType,
                                       PartTypeString,
                                       ARRAYSIZE(PartTypeString));

    PartSize = PartEntry->SectorCount.QuadPart * DiskEntry->BytesPerSector;
#if 0
    if (PartSize >= 10737418240) /* 10 GB */
    {
        PartSize = PartSize / 1073741824;
        Unit = MUIGetString(STRING_GB);
    }
    else
#endif
    if (PartSize >= 10485760) /* 10 MB */
    {
        PartSize = PartSize / 1048576;
        Unit = MUIGetString(STRING_MB);
    }
    else
    {
        PartSize = PartSize / 1024;
        Unit = MUIGetString(STRING_KB);
    }

    if (*PartTypeString == '\0') // STRING_FORMATUNKNOWN ??
    {
        CONSOLE_PrintTextXY(6, 10,
                            MUIGetString(STRING_HDDINFOUNK2),
                            (PartEntry->DriveLetter == 0) ? '-' : PartEntry->DriveLetter,
                            (PartEntry->DriveLetter == 0) ? '-' : ':',
                            PartEntry->PartitionType,
                            PartSize,
                            Unit);
    }
    else
    {
        CONSOLE_PrintTextXY(6, 10,
                            "   %c%c  %s    %I64u %s",
                            (PartEntry->DriveLetter == 0) ? '-' : PartEntry->DriveLetter,
                            (PartEntry->DriveLetter == 0) ? '-' : ':',
                            PartTypeString,
                            PartSize,
                            Unit);
    }

    DiskSize = DiskEntry->SectorCount.QuadPart * DiskEntry->BytesPerSector;
#if 0
    if (DiskSize >= 10737418240) /* 10 GB */
    {
        DiskSize = DiskSize / 1073741824;
        Unit = MUIGetString(STRING_GB);
    }
    else
#endif
    {
        DiskSize = DiskSize / 1048576;
        if (DiskSize == 0)
            DiskSize = 1;

        Unit = MUIGetString(STRING_MB);
    }

    if (DiskEntry->DriverName.Length > 0)
    {
        CONSOLE_PrintTextXY(6, 12,
                            MUIGetString(STRING_HDINFOPARTDELETE),
                            DiskSize,
                            Unit,
                            DiskEntry->DiskNumber,
                            DiskEntry->Port,
                            DiskEntry->Bus,
                            DiskEntry->Id,
                            &DiskEntry->DriverName);
    }
    else
    {
        CONSOLE_PrintTextXY(6, 12,
                            MUIGetString(STRING_HDDINFOUNK3),
                            DiskSize,
                            Unit,
                            DiskEntry->DiskNumber,
                            DiskEntry->Port,
                            DiskEntry->Bus,
                            DiskEntry->Id);
    }

    while (TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3))  /* F3 */
        {
            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;

            break;
        }
        else if (Ir->Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE)  /* ESC */
        {
            return SELECT_PARTITION_PAGE;
        }
        else if (Ir->Event.KeyEvent.wVirtualKeyCode == 'D') /* D */
        {
            DeleteCurrentPartition(PartitionList);

            return SELECT_PARTITION_PAGE;
        }
    }

    return DELETE_PARTITION_PAGE;
}


/*
 * Displays the SelectFileSystemPage.
 *
 * Next pages:
 *  CheckFileSystemPage (At once if RepairUpdate is selected)
 *  CheckFileSystemPage (At once if Unattended and not UnattendFormatPartition)
 *  FormatPartitionPage (At once if Unattended and UnattendFormatPartition)
 *  SelectPartitionPage (If the user aborts)
 *  FormatPartitionPage (Default)
 *  QuitPage
 *
 * SIDEEFFECTS
 *  Sets PartEntry->DiskEntry->LayoutBuffer->PartitionEntry[PartEntry->PartitionIndex].PartitionType (via UpdatePartitionType)
 *  Calls CheckActiveSystemPartition()
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
SelectFileSystemPage(PINPUT_RECORD Ir)
{
    PDISKENTRY DiskEntry;
    PPARTENTRY PartEntry;
    ULONGLONG DiskSize;
    ULONGLONG PartSize;
    PCHAR DiskUnit;
    PCHAR PartUnit;
    CHAR PartTypeString[32];
    FORMATMACHINESTATE PreviousFormatState;

    DPRINT("SelectFileSystemPage()\n");

    if (PartitionList == NULL ||
        PartitionList->CurrentDisk == NULL ||
        PartitionList->CurrentPartition == NULL)
    {
        /* FIXME: show an error dialog */
        return QUIT_PAGE;
    }

    /* Find or set the active system partition */
    CheckActiveSystemPartition(PartitionList);
    if (PartitionList->SystemPartition == NULL)
    {
        /* FIXME: show an error dialog */
        //
        // Error dialog should say that we cannot find a suitable
        // system partition and create one on the system. At this point,
        // it may be nice to ask the user whether he wants to continue,
        // or use an external drive as the system drive/partition
        // (e.g. floppy, USB drive, etc...)
        //
        return QUIT_PAGE;
    }

    PreviousFormatState = FormatState;
    switch (FormatState)
    {
        case Start:
        {
            if (PartitionList->CurrentPartition != PartitionList->SystemPartition)
            {
                TempPartition = PartitionList->SystemPartition;
                TempPartition->NeedsCheck = TRUE;

                FormatState = FormatSystemPartition;
                DPRINT1("FormatState: Start --> FormatSystemPartition\n");
            }
            else
            {
                TempPartition = PartitionList->CurrentPartition;
                TempPartition->NeedsCheck = TRUE;

                FormatState = FormatInstallPartition;
                DPRINT1("FormatState: Start --> FormatInstallPartition\n");
            }
            break;
        }

        case FormatSystemPartition:
        {
            TempPartition = PartitionList->CurrentPartition;
            TempPartition->NeedsCheck = TRUE;

            FormatState = FormatInstallPartition;
            DPRINT1("FormatState: FormatSystemPartition --> FormatInstallPartition\n");
            break;
        }

        case FormatInstallPartition:
        {
            if (GetNextUnformattedPartition(PartitionList,
                                            NULL,
                                            &TempPartition))
            {
                FormatState = FormatOtherPartition;
                TempPartition->NeedsCheck = TRUE;
                DPRINT1("FormatState: FormatInstallPartition --> FormatOtherPartition\n");
            }
            else
            {
                FormatState = FormatDone;
                DPRINT1("FormatState: FormatInstallPartition --> FormatDone\n");
                return CHECK_FILE_SYSTEM_PAGE;
            }
            break;
        }

        case FormatOtherPartition:
        {
            if (GetNextUnformattedPartition(PartitionList,
                                            NULL,
                                            &TempPartition))
            {
                FormatState = FormatOtherPartition;
                TempPartition->NeedsCheck = TRUE;
                DPRINT1("FormatState: FormatOtherPartition --> FormatOtherPartition\n");
            }
            else
            {
                FormatState = FormatDone;
                DPRINT1("FormatState: FormatOtherPartition --> FormatDone\n");
                return CHECK_FILE_SYSTEM_PAGE;
            }
            break;
        }

        default:
        {
            DPRINT1("FormatState: Invalid value %ld\n", FormatState);
            /* FIXME: show an error dialog */
            return QUIT_PAGE;
        }
    }

    PartEntry = TempPartition;
    DiskEntry = PartEntry->DiskEntry;

    /* Adjust disk size */
    DiskSize = DiskEntry->SectorCount.QuadPart * DiskEntry->BytesPerSector;
    if (DiskSize >= 10737418240) /* 10 GB */
    {
        DiskSize = DiskSize / 1073741824;
        DiskUnit = MUIGetString(STRING_GB);
    }
    else
    {
        DiskSize = DiskSize / 1048576;
        DiskUnit = MUIGetString(STRING_MB);
    }

    /* Adjust partition size */
    PartSize = PartEntry->SectorCount.QuadPart * DiskEntry->BytesPerSector;
    if (PartSize >= 10737418240) /* 10 GB */
    {
        PartSize = PartSize / 1073741824;
        PartUnit = MUIGetString(STRING_GB);
    }
    else
    {
        PartSize = PartSize / 1048576;
        PartUnit = MUIGetString(STRING_MB);
    }

    /* Adjust partition type */
    GetPartTypeStringFromPartitionType(PartEntry->PartitionType,
                                       PartTypeString,
                                       ARRAYSIZE(PartTypeString));

    if (PartEntry->AutoCreate != FALSE)
    {
        CONSOLE_SetTextXY(6, 8, MUIGetString(STRING_NEWPARTITION));

#if 0
        CONSOLE_PrintTextXY(8, 10, "Partition %lu (%I64u %s) %s of",
                            PartEntry->PartitionNumber,
                            PartSize,
                            PartUnit,
                            PartTypeString);
#endif

        CONSOLE_PrintTextXY(8, 10, MUIGetString(STRING_HDINFOPARTZEROED),
                            DiskEntry->DiskNumber,
                            DiskSize,
                            DiskUnit,
                            DiskEntry->Port,
                            DiskEntry->Bus,
                            DiskEntry->Id,
                            &DiskEntry->DriverName);

        CONSOLE_SetTextXY(6, 12, MUIGetString(STRING_PARTFORMAT));

        PartEntry->AutoCreate = FALSE;
    }
    else if (PartEntry->New != FALSE)
    {
        switch (FormatState)
        {
            case FormatSystemPartition:
                CONSOLE_SetTextXY(6, 8, MUIGetString(STRING_NONFORMATTEDSYSTEMPART));
                break;

            case FormatInstallPartition:
                CONSOLE_SetTextXY(6, 8, MUIGetString(STRING_NONFORMATTEDPART));
                break;

            case FormatOtherPartition:
                CONSOLE_SetTextXY(6, 8, MUIGetString(STRING_NONFORMATTEDOTHERPART));
                break;

            default:
                break;
        }

        CONSOLE_SetTextXY(6, 10, MUIGetString(STRING_PARTFORMAT));
    }
    else
    {
        CONSOLE_SetTextXY(6, 8, MUIGetString(STRING_INSTALLONPART));

        if (*PartTypeString == '\0') // STRING_FORMATUNKNOWN ??
        {
            CONSOLE_PrintTextXY(8, 10,
                                MUIGetString(STRING_HDDINFOUNK4),
                                (PartEntry->DriveLetter == 0) ? '-' : PartEntry->DriveLetter,
                                (PartEntry->DriveLetter == 0) ? '-' : ':',
                                PartEntry->PartitionType,
                                PartSize,
                                PartUnit);
        }
        else
        {
            CONSOLE_PrintTextXY(8, 10,
                                "%c%c  %s    %I64u %s",
                                (PartEntry->DriveLetter == 0) ? '-' : PartEntry->DriveLetter,
                                (PartEntry->DriveLetter == 0) ? '-' : ':',
                                PartTypeString,
                                PartSize,
                                PartUnit);
        }

        CONSOLE_PrintTextXY(6, 12, MUIGetString(STRING_HDINFOPARTEXISTS),
                            DiskEntry->DiskNumber,
                            DiskSize,
                            DiskUnit,
                            DiskEntry->Port,
                            DiskEntry->Bus,
                            DiskEntry->Id,
                            &DiskEntry->DriverName);
    }

    MUIDisplayPage(SELECT_FILE_SYSTEM_PAGE);

    if (FileSystemList == NULL)
    {
        /* Create the file system list, and by default select the "FAT" file system */
        FileSystemList = CreateFileSystemList(6, 26, PartEntry->New, L"FAT");
        if (FileSystemList == NULL)
        {
            /* FIXME: show an error dialog */
            return QUIT_PAGE;
        }
    }

    DrawFileSystemList(FileSystemList);

    if (RepairUpdateFlag)
    {
        return CHECK_FILE_SYSTEM_PAGE;
        //return SELECT_PARTITION_PAGE;
    }

    if (IsUnattendedSetup)
    {
        if (UnattendFormatPartition)
        {
            /*
             * We use whatever currently selected file system we have
             * (by default, this is "FAT", as per the initialization
             * performed above). Note that it may be interesting to specify
             * which file system to use in unattended installations, in the
             * txtsetup.sif file.
             */
            return FORMAT_PARTITION_PAGE;
        }

        return CHECK_FILE_SYSTEM_PAGE;
    }

    while (TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3))  /* F3 */
        {
            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;

            break;
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE))  /* ESC */
        {
            FormatState = Start;
            return SELECT_PARTITION_PAGE;
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_DOWN))  /* DOWN */
        {
            ScrollDownFileSystemList(FileSystemList);
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_UP))  /* UP */
        {
            ScrollUpFileSystemList(FileSystemList);
        }
        else if (Ir->Event.KeyEvent.wVirtualKeyCode == VK_RETURN) /* ENTER */
        {
            if (!FileSystemList->Selected->FileSystem)
                return SELECT_FILE_SYSTEM_PAGE;
            else
                return FORMAT_PARTITION_PAGE;
        }
    }

    FormatState = PreviousFormatState;

    return SELECT_FILE_SYSTEM_PAGE;
}


/*
 * Displays the FormatPartitionPage.
 *
 * Next pages:
 *  InstallDirectoryPage (At once if IsUnattendedSetup or InstallShortcut)
 *  SelectPartitionPage  (At once)
 *  QuitPage
 *
 * SIDEEFFECTS
 *  Sets PartitionList->CurrentPartition->FormatState
 *  Sets DestinationRootPath
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
FormatPartitionPage(PINPUT_RECORD Ir)
{
    UNICODE_STRING PartitionRootPath;
    WCHAR PathBuffer[MAX_PATH];
    PDISKENTRY DiskEntry;
    PPARTENTRY PartEntry;
    PFILE_SYSTEM_ITEM SelectedFileSystem;
    NTSTATUS Status;

#ifndef NDEBUG
    ULONG Line;
    ULONG i;
    PPARTITION_INFORMATION PartitionInfo;
#endif

    DPRINT("FormatPartitionPage()\n");

    MUIDisplayPage(FORMAT_PARTITION_PAGE);

    if (PartitionList == NULL || TempPartition == NULL)
    {
        /* FIXME: show an error dialog */
        return QUIT_PAGE;
    }

    PartEntry = TempPartition;
    DiskEntry = PartEntry->DiskEntry;

    SelectedFileSystem = FileSystemList->Selected;

    while (TRUE)
    {
        if (!IsUnattendedSetup)
        {
            CONSOLE_ConInKey(Ir);
        }

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3))  /* F3 */
        {
            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;

            break;
        }
        else if (Ir->Event.KeyEvent.wVirtualKeyCode == VK_RETURN || IsUnattendedSetup) /* ENTER */
        {
            CONSOLE_SetStatusText(MUIGetString(STRING_PLEASEWAIT));

            if (!PreparePartitionForFormatting(PartEntry, SelectedFileSystem->FileSystem))
            {
                /* FIXME: show an error dialog */
                return QUIT_PAGE;
            }

#ifndef NDEBUG
            CONSOLE_PrintTextXY(6, 12,
                                "Cylinders: %I64u  Tracks/Cyl: %lu  Sectors/Trk: %lu  Bytes/Sec: %lu  %c",
                                DiskEntry->Cylinders,
                                DiskEntry->TracksPerCylinder,
                                DiskEntry->SectorsPerTrack,
                                DiskEntry->BytesPerSector,
                                DiskEntry->Dirty ? '*' : ' ');

            Line = 13;

            for (i = 0; i < DiskEntry->LayoutBuffer->PartitionCount; i++)
            {
                PartitionInfo = &DiskEntry->LayoutBuffer->PartitionEntry[i];

                CONSOLE_PrintTextXY(6, Line,
                                    "%2u:  %2lu  %c  %12I64u  %12I64u  %02x",
                                    i,
                                    PartitionInfo->PartitionNumber,
                                    PartitionInfo->BootIndicator ? 'A' : '-',
                                    PartitionInfo->StartingOffset.QuadPart / DiskEntry->BytesPerSector,
                                    PartitionInfo->PartitionLength.QuadPart / DiskEntry->BytesPerSector,
                                    PartitionInfo->PartitionType);
                Line++;
            }
#endif

            /* Commit the partition changes to the disk */
            if (!WritePartitionsToDisk(PartitionList))
            {
                DPRINT("WritePartitionsToDisk() failed\n");
                MUIDisplayError(ERROR_WRITE_PTABLE, Ir, POPUP_WAIT_ENTER);
                return QUIT_PAGE;
            }

            /* Set PartitionRootPath */
            StringCchPrintfW(PathBuffer, ARRAYSIZE(PathBuffer),
                    L"\\Device\\Harddisk%lu\\Partition%lu",
                    DiskEntry->DiskNumber,
                    PartEntry->PartitionNumber);
            RtlInitUnicodeString(&PartitionRootPath, PathBuffer);
            DPRINT("PartitionRootPath: %wZ\n", &PartitionRootPath);

            /* Format the partition */
            if (SelectedFileSystem->FileSystem)
            {
                Status = FormatPartition(&PartitionRootPath,
                                         SelectedFileSystem);
                if (!NT_SUCCESS(Status))
                {
                    DPRINT1("FormatPartition() failed with status 0x%08lx\n", Status);
                    MUIDisplayError(ERROR_FORMATTING_PARTITION, Ir, POPUP_WAIT_ANY_KEY, PathBuffer);
                    return QUIT_PAGE;
                }

                PartEntry->FormatState = Formatted;
                // PartEntry->FileSystem  = FileSystem;
                PartEntry->New = FALSE;
            }

#ifndef NDEBUG
            CONSOLE_SetStatusText("   Done.  Press any key ...");
            CONSOLE_ConInKey(Ir);
#endif

            return SELECT_FILE_SYSTEM_PAGE;
        }
    }

    return FORMAT_PARTITION_PAGE;
}


/*
 * Displays the CheckFileSystemPage.
 *
 * Next pages:
 *  InstallDirectoryPage (At once)
 *  QuitPage
 *
 * SIDEEFFECTS
 *  Inits or reloads FileSystemList
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
CheckFileSystemPage(PINPUT_RECORD Ir)
{
    PFILE_SYSTEM CurrentFileSystem;
    UNICODE_STRING PartitionRootPath;
    WCHAR PathBuffer[MAX_PATH];
    CHAR Buffer[MAX_PATH];
    PDISKENTRY DiskEntry;
    PPARTENTRY PartEntry;
    NTSTATUS Status;

    if (PartitionList == NULL)
    {
        /* FIXME: show an error dialog */
        return QUIT_PAGE;
    }

    if (!GetNextUncheckedPartition(PartitionList, &DiskEntry, &PartEntry))
    {
        return INSTALL_DIRECTORY_PAGE;
    }

    /* Set PartitionRootPath */
    StringCchPrintfW(PathBuffer, ARRAYSIZE(PathBuffer),
            L"\\Device\\Harddisk%lu\\Partition%lu",
            DiskEntry->DiskNumber,
            PartEntry->PartitionNumber);
    RtlInitUnicodeString(&PartitionRootPath, PathBuffer);
    DPRINT("PartitionRootPath: %wZ\n", &PartitionRootPath);

    CONSOLE_SetTextXY(6, 8, MUIGetString(STRING_CHECKINGPART));

    CONSOLE_SetStatusText(MUIGetString(STRING_PLEASEWAIT));

    CurrentFileSystem = PartEntry->FileSystem;
    DPRINT1("CheckFileSystemPage -- PartitionType: 0x%02X ; FileSystemName: %S\n",
            PartEntry->PartitionType, (CurrentFileSystem ? CurrentFileSystem->FileSystemName : L"n/a"));

    /* HACK: Do not try to check a partition with an unknown filesystem */
    if (CurrentFileSystem == NULL)
    {
        PartEntry->NeedsCheck = FALSE;
        return CHECK_FILE_SYSTEM_PAGE;
    }

    if (CurrentFileSystem->ChkdskFunc == NULL)
    {
        sprintf(Buffer,
                "Setup is currently unable to check a partition formatted in %S.\n"
                "\n"
                "  \x07  Press ENTER to continue Setup.\n"
                "  \x07  Press F3 to quit Setup.",
                CurrentFileSystem->FileSystemName);

        PopupError(Buffer,
                   MUIGetString(STRING_QUITCONTINUE),
                   NULL, POPUP_WAIT_NONE);

        while (TRUE)
        {
            CONSOLE_ConInKey(Ir);

            if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x00 &&
                Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3)  /* F3 */
            {
                if (ConfirmQuit(Ir))
                    return QUIT_PAGE;
                else
                    return CHECK_FILE_SYSTEM_PAGE;
            }
            else if (Ir->Event.KeyEvent.uChar.AsciiChar == VK_RETURN) /* ENTER */
            {
                PartEntry->NeedsCheck = FALSE;
                return CHECK_FILE_SYSTEM_PAGE;
            }
        }
    }
    else
    {
        Status = ChkdskPartition(&PartitionRootPath, CurrentFileSystem);
        if (!NT_SUCCESS(Status))
        {
            DPRINT("ChkdskPartition() failed with status 0x%08lx\n", Status);
            // sprintf(Buffer, "Setup failed to verify the selected partition.\n"
            sprintf(Buffer, "ChkDsk detected some disk errors.\n"
                    "(Status 0x%08lx).\n", Status);
            PopupError(Buffer,
                       // MUIGetString(STRING_REBOOTCOMPUTER),
                       MUIGetString(STRING_CONTINUE),
                       Ir, POPUP_WAIT_ENTER);

            // return QUIT_PAGE;
        }

        PartEntry->NeedsCheck = FALSE;
        return CHECK_FILE_SYSTEM_PAGE;
    }
}


static
VOID
BuildInstallPaths(PWCHAR InstallDir,
                  PDISKENTRY DiskEntry,
                  PPARTENTRY PartEntry)
{
    WCHAR PathBuffer[MAX_PATH];

    /* Create 'InstallPath' string */
    RtlFreeUnicodeString(&InstallPath);
    RtlCreateUnicodeString(&InstallPath, InstallDir);

    /* Create 'DestinationRootPath' string */
    RtlFreeUnicodeString(&DestinationRootPath);
    StringCchPrintfW(PathBuffer, ARRAYSIZE(PathBuffer),
            L"\\Device\\Harddisk%lu\\Partition%lu\\",
            DiskEntry->DiskNumber,
            PartEntry->PartitionNumber);
    RtlCreateUnicodeString(&DestinationRootPath, PathBuffer);
    DPRINT("DestinationRootPath: %wZ\n", &DestinationRootPath);

    /* Create 'DestinationPath' string */
    RtlFreeUnicodeString(&DestinationPath);
    CombinePaths(PathBuffer, ARRAYSIZE(PathBuffer), 2,
                 DestinationRootPath.Buffer, InstallDir);
    RtlCreateUnicodeString(&DestinationPath, PathBuffer);

    /* Create 'DestinationArcPath' */
    RtlFreeUnicodeString(&DestinationArcPath);
    StringCchPrintfW(PathBuffer, ARRAYSIZE(PathBuffer),
            L"multi(0)disk(0)rdisk(%lu)partition(%lu)\\",
            DiskEntry->BiosDiskNumber,
            PartEntry->PartitionNumber);
    ConcatPaths(PathBuffer, ARRAYSIZE(PathBuffer), 1, InstallDir);
    RtlCreateUnicodeString(&DestinationArcPath, PathBuffer);
}


/*
 * Displays the InstallDirectoryPage.
 *
 * Next pages:
 *  PrepareCopyPage (As the direct result of InstallDirectoryPage1)
 *  QuitPage
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
InstallDirectoryPage(PINPUT_RECORD Ir)
{
    PDISKENTRY DiskEntry;
    PPARTENTRY PartEntry;
    WCHAR InstallDir[51];
    WCHAR c;
    ULONG Length, Pos;

    /* We do not need the filesystem list any more */
    if (FileSystemList != NULL)
    {
        DestroyFileSystemList(FileSystemList);
        FileSystemList = NULL;
    }

    if (PartitionList == NULL ||
        PartitionList->CurrentDisk == NULL ||
        PartitionList->CurrentPartition == NULL)
    {
        /* FIXME: show an error dialog */
        return QUIT_PAGE;
    }

    DiskEntry = PartitionList->CurrentDisk;
    PartEntry = PartitionList->CurrentPartition;

    if (IsUnattendedSetup)
    {
        if (!IsValidPath(UnattendInstallationDirectory))
        {
            /* FIXME: Log the error? */
            return QUIT_PAGE;
        }

        BuildInstallPaths(UnattendInstallationDirectory,
                          DiskEntry,
                          PartEntry);

        return PREPARE_COPY_PAGE;
    }

    wcscpy(InstallDir, L"\\ReactOS");

    Length = wcslen(InstallDir);
    Pos = Length;
    CONSOLE_SetInputTextXY(8, 11, 51, InstallDir);
    CONSOLE_SetCursorXY(8 + Pos, 11);
    CONSOLE_SetCursorType(TRUE, TRUE);
    MUIDisplayPage(INSTALL_DIRECTORY_PAGE);

    while (TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3))  /* F3 */
        {
            CONSOLE_SetCursorType(TRUE, FALSE);

            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;

            CONSOLE_SetCursorType(TRUE, TRUE);
            break;
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_DELETE))  /* DEL */
        {
            if (Pos < Length)
            {
                memmove(&InstallDir[Pos],
                        &InstallDir[Pos + 1],
                        (Length - Pos - 1) * sizeof(WCHAR));
                InstallDir[Length - 1] = UNICODE_NULL;

                Length--;
                CONSOLE_SetInputTextXY(8, 11, 51, InstallDir);
                CONSOLE_SetCursorXY(8 + Pos, 11);
            }
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_HOME))  /* HOME */
        {
            Pos = 0;
            CONSOLE_SetCursorXY(8 + Pos, 11);
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_END))  /* END */
        {
            Pos = Length;
            CONSOLE_SetCursorXY(8 + Pos, 11);
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_LEFT))  /* LEFT */
        {
            if (Pos > 0)
            {
                Pos--;
                CONSOLE_SetCursorXY(8 + Pos, 11);
            }
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_RIGHT))  /* RIGHT */
        {
            if (Pos < Length)
            {
                Pos++;
                CONSOLE_SetCursorXY(8 + Pos, 11);
            }
        }
        else if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x0D) /* ENTER */
        {
            CONSOLE_SetCursorType(TRUE, FALSE);

            /*
             * Check for the validity of the installation directory and pop up
             * an error if it is not the case. Then the user can fix its input.
             */
            if (!IsValidPath(InstallDir))
            {
                MUIDisplayError(ERROR_DIRECTORY_NAME, Ir, POPUP_WAIT_ENTER);
                return INSTALL_DIRECTORY_PAGE;
            }

            BuildInstallPaths(InstallDir,
                              DiskEntry,
                              PartEntry);

            return PREPARE_COPY_PAGE;
        }
        else if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x08) /* BACKSPACE */
        {
            if (Pos > 0)
            {
                if (Pos < Length)
                    memmove(&InstallDir[Pos - 1],
                            &InstallDir[Pos],
                            (Length - Pos) * sizeof(WCHAR));
                InstallDir[Length - 1] = UNICODE_NULL;

                Pos--;
                Length--;
                CONSOLE_SetInputTextXY(8, 11, 51, InstallDir);
                CONSOLE_SetCursorXY(8 + Pos, 11);
            }
        }
        else if (isprint(Ir->Event.KeyEvent.uChar.AsciiChar))
        {
            if (Length < 50)
            {
                c = (WCHAR)Ir->Event.KeyEvent.uChar.AsciiChar;
                if (iswalpha(c) || iswdigit(c) || c == '.' || c == '\\' || c == '-' || c == '_')
                {
                    if (Pos < Length)
                        memmove(&InstallDir[Pos + 1],
                                &InstallDir[Pos],
                                (Length - Pos) * sizeof(WCHAR));
                    InstallDir[Length + 1] = UNICODE_NULL;
                    InstallDir[Pos] = c;

                    Pos++;
                    Length++;
                    CONSOLE_SetInputTextXY(8, 11, 51, InstallDir);
                    CONSOLE_SetCursorXY(8 + Pos, 11);
                }
            }
        }
    }

    return INSTALL_DIRECTORY_PAGE;
}


static BOOLEAN
AddSectionToCopyQueueCab(HINF InfFile,
                         PWCHAR SectionName,
                         PWCHAR SourceCabinet,
                         PCUNICODE_STRING DestinationPath,
                         PINPUT_RECORD Ir)
{
    INFCONTEXT FilesContext;
    INFCONTEXT DirContext;
    PWCHAR FileKeyName;
    PWCHAR FileKeyValue;
    PWCHAR DirKeyValue;
    PWCHAR TargetFileName;

    /*
     * This code enumerates the list of files in reactos.dff / reactos.inf
     * that need to be extracted from reactos.cab and be installed in their
     * respective directories.
     */

    /* Search for the SectionName section */
    if (!SetupFindFirstLineW(InfFile, SectionName, NULL, &FilesContext))
    {
        CHAR Buffer[128];
        sprintf(Buffer, MUIGetString(STRING_TXTSETUPFAILED), SectionName);
        PopupError(Buffer, MUIGetString(STRING_REBOOTCOMPUTER), Ir, POPUP_WAIT_ENTER);
        return FALSE;
    }

    /*
     * Enumerate the files in the section and add them to the file queue.
     */
    do
    {
        /* Get source file name and target directory id */
        if (!INF_GetData(&FilesContext, &FileKeyName, &FileKeyValue))
        {
            /* FIXME: Handle error! */
            DPRINT1("INF_GetData() failed\n");
            break;
        }

        /* Get optional target file name */
        if (!INF_GetDataField(&FilesContext, 2, &TargetFileName))
            TargetFileName = NULL;

        DPRINT("FileKeyName: '%S'  FileKeyValue: '%S'\n", FileKeyName, FileKeyValue);

        /* Lookup target directory */
        if (!SetupFindFirstLineW(InfFile, L"Directories", FileKeyValue, &DirContext))
        {
            /* FIXME: Handle error! */
            DPRINT1("SetupFindFirstLine() failed\n");
            break;
        }

        if (!INF_GetData(&DirContext, NULL, &DirKeyValue))
        {
            /* FIXME: Handle error! */
            DPRINT1("INF_GetData() failed\n");
            break;
        }

        if (!SetupQueueCopy(SetupFileQueue,
                            SourceCabinet,
                            SourceRootPath.Buffer,
                            SourceRootDir.Buffer,
                            FileKeyName,
                            DirKeyValue,
                            TargetFileName))
        {
            /* FIXME: Handle error! */
            DPRINT1("SetupQueueCopy() failed\n");
        }
    } while (SetupFindNextLine(&FilesContext, &FilesContext));

    return TRUE;
}


static BOOLEAN
AddSectionToCopyQueue(HINF InfFile,
                      PWCHAR SectionName,
                      PWCHAR SourceCabinet,
                      PCUNICODE_STRING DestinationPath,
                      PINPUT_RECORD Ir)
{
    INFCONTEXT FilesContext;
    INFCONTEXT DirContext;
    PWCHAR FileKeyName;
    PWCHAR FileKeyValue;
    PWCHAR DirKeyValue;
    PWCHAR TargetFileName;
    WCHAR CompleteOrigDirName[512]; // FIXME: MAX_PATH is not enough?

    if (SourceCabinet)
        return AddSectionToCopyQueueCab(InfFile, L"SourceFiles", SourceCabinet, DestinationPath, Ir);

    /*
     * This code enumerates the list of files in txtsetup.sif
     * that need to be installed in their respective directories.
     */

    /* Search for the SectionName section */
    if (!SetupFindFirstLineW(InfFile, SectionName, NULL, &FilesContext))
    {
        CHAR Buffer[128];
        sprintf(Buffer, MUIGetString(STRING_TXTSETUPFAILED), SectionName);
        PopupError(Buffer, MUIGetString(STRING_REBOOTCOMPUTER), Ir, POPUP_WAIT_ENTER);
        return FALSE;
    }

    /*
     * Enumerate the files in the section and add them to the file queue.
     */
    do
    {
        /* Get source file name and target directory id */
        if (!INF_GetData(&FilesContext, &FileKeyName, &FileKeyValue))
        {
            /* FIXME: Handle error! */
            DPRINT1("INF_GetData() failed\n");
            break;
        }

        /* Get target directory id */
        if (!INF_GetDataField(&FilesContext, 13, &FileKeyValue))
        {
            /* FIXME: Handle error! */
            DPRINT1("INF_GetData() failed\n");
            break;
        }

        /* Get optional target file name */
        if (!INF_GetDataField(&FilesContext, 11, &TargetFileName))
            TargetFileName = NULL;
        else if (!*TargetFileName)
            TargetFileName = NULL;

        DPRINT("FileKeyName: '%S'  FileKeyValue: '%S'\n", FileKeyName, FileKeyValue);

        /* Lookup target directory */
        if (!SetupFindFirstLineW(InfFile, L"Directories", FileKeyValue, &DirContext))
        {
            /* FIXME: Handle error! */
            DPRINT1("SetupFindFirstLine() failed\n");
            break;
        }

        if (!INF_GetData(&DirContext, NULL, &DirKeyValue))
        {
            /* FIXME: Handle error! */
            DPRINT1("INF_GetData() failed\n");
            break;
        }

        if ((DirKeyValue[0] == UNICODE_NULL) || (DirKeyValue[0] == L'\\' && DirKeyValue[1] == UNICODE_NULL))
        {
            /* Installation path */
            DPRINT("InstallationPath: '%S'\n", DirKeyValue);

            StringCchCopyW(CompleteOrigDirName, ARRAYSIZE(CompleteOrigDirName),
                           SourceRootDir.Buffer);

            DPRINT("InstallationPath(2): '%S'\n", CompleteOrigDirName);
        }
        else if (DirKeyValue[0] == L'\\')
        {
            /* Absolute path */
            DPRINT("AbsolutePath: '%S'\n", DirKeyValue);

            StringCchCopyW(CompleteOrigDirName, ARRAYSIZE(CompleteOrigDirName),
                           DirKeyValue);

            DPRINT("AbsolutePath(2): '%S'\n", CompleteOrigDirName);
        }
        else // if (DirKeyValue[0] != L'\\')
        {
            /* Path relative to the installation path */
            DPRINT("RelativePath: '%S'\n", DirKeyValue);

            CombinePaths(CompleteOrigDirName, ARRAYSIZE(CompleteOrigDirName), 2,
                         SourceRootDir.Buffer, DirKeyValue);

            DPRINT("RelativePath(2): '%S'\n", CompleteOrigDirName);
        }

        if (!SetupQueueCopy(SetupFileQueue,
                            SourceCabinet,
                            SourceRootPath.Buffer,
                            CompleteOrigDirName,
                            FileKeyName,
                            DirKeyValue,
                            TargetFileName))
        {
            /* FIXME: Handle error! */
            DPRINT1("SetupQueueCopy() failed\n");
        }
    } while (SetupFindNextLine(&FilesContext, &FilesContext));

    return TRUE;
}


static BOOLEAN
PrepareCopyPageInfFile(HINF InfFile,
                       PWCHAR SourceCabinet,
                       PINPUT_RECORD Ir)
{
    NTSTATUS Status;
    INFCONTEXT DirContext;
    PWCHAR AdditionalSectionName = NULL;
    PWCHAR DirKeyValue;
    WCHAR PathBuffer[MAX_PATH];

    /* Add common files */
    if (!AddSectionToCopyQueue(InfFile, L"SourceDisksFiles", SourceCabinet, &DestinationPath, Ir))
        return FALSE;

    /* Add specific files depending of computer type */
    if (SourceCabinet == NULL)
    {
        if (!ProcessComputerFiles(InfFile, ComputerList, &AdditionalSectionName))
            return FALSE;

        if (AdditionalSectionName)
        {
            if (!AddSectionToCopyQueue(InfFile, AdditionalSectionName, SourceCabinet, &DestinationPath, Ir))
                return FALSE;
        }
    }

    /* Create directories */

    /*
     * FIXME:
     * Copying files to DestinationRootPath should be done from within
     * the SystemPartitionFiles section.
     * At the moment we check whether we specify paths like '\foo' or '\\' for that.
     * For installing to DestinationPath specify just '\' .
     */

    /* Get destination path */
    StringCchCopyW(PathBuffer, ARRAYSIZE(PathBuffer), DestinationPath.Buffer);

    DPRINT("FullPath(1): '%S'\n", PathBuffer);

    /* Create the install directory */
    Status = SetupCreateDirectory(PathBuffer);
    if (!NT_SUCCESS(Status) && Status != STATUS_OBJECT_NAME_COLLISION)
    {
        DPRINT1("Creating directory '%S' failed: Status = 0x%08lx\n", PathBuffer, Status);
        MUIDisplayError(ERROR_CREATE_INSTALL_DIR, Ir, POPUP_WAIT_ENTER);
        return FALSE;
    }

    /* Search for the 'Directories' section */
    if (!SetupFindFirstLineW(InfFile, L"Directories", NULL, &DirContext))
    {
        if (SourceCabinet)
        {
            MUIDisplayError(ERROR_CABINET_SECTION, Ir, POPUP_WAIT_ENTER);
        }
        else
        {
            MUIDisplayError(ERROR_TXTSETUP_SECTION, Ir, POPUP_WAIT_ENTER);
        }

        return FALSE;
    }

    /* Enumerate the directory values and create the subdirectories */
    do
    {
        if (!INF_GetData(&DirContext, NULL, &DirKeyValue))
        {
            DPRINT1("break\n");
            break;
        }

        if ((DirKeyValue[0] == UNICODE_NULL) || (DirKeyValue[0] == L'\\' && DirKeyValue[1] == UNICODE_NULL))
        {
            /* Installation path */
            DPRINT("InstallationPath: '%S'\n", DirKeyValue);

            StringCchCopyW(PathBuffer, ARRAYSIZE(PathBuffer),
                           DestinationPath.Buffer);

            DPRINT("InstallationPath(2): '%S'\n", PathBuffer);
        }
        else if (DirKeyValue[0] == L'\\')
        {
            /* Absolute path */
            DPRINT("AbsolutePath: '%S'\n", DirKeyValue);

            CombinePaths(PathBuffer, ARRAYSIZE(PathBuffer), 2,
                         DestinationRootPath.Buffer, DirKeyValue);

            DPRINT("AbsolutePath(2): '%S'\n", PathBuffer);

            Status = SetupCreateDirectory(PathBuffer);
            if (!NT_SUCCESS(Status) && Status != STATUS_OBJECT_NAME_COLLISION)
            {
                DPRINT("Creating directory '%S' failed: Status = 0x%08lx", PathBuffer, Status);
                MUIDisplayError(ERROR_CREATE_DIR, Ir, POPUP_WAIT_ENTER);
                return FALSE;
            }
        }
        else // if (DirKeyValue[0] != L'\\')
        {
            /* Path relative to the installation path */
            DPRINT("RelativePath: '%S'\n", DirKeyValue);

            CombinePaths(PathBuffer, ARRAYSIZE(PathBuffer), 2,
                         DestinationPath.Buffer, DirKeyValue);

            DPRINT("RelativePath(2): '%S'\n", PathBuffer);

            Status = SetupCreateDirectory(PathBuffer);
            if (!NT_SUCCESS(Status) && Status != STATUS_OBJECT_NAME_COLLISION)
            {
                DPRINT("Creating directory '%S' failed: Status = 0x%08lx", PathBuffer, Status);
                MUIDisplayError(ERROR_CREATE_DIR, Ir, POPUP_WAIT_ENTER);
                return FALSE;
            }
        }
    } while (SetupFindNextLine(&DirContext, &DirContext));

    return TRUE;
}


/*
 * Displays the PrepareCopyPage.
 *
 * Next pages:
 *  FileCopyPage(At once)
 *  QuitPage
 *
 * SIDEEFFECTS
 * Inits SetupFileQueue
 * Calls PrepareCopyPageInfFile
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
PrepareCopyPage(PINPUT_RECORD Ir)
{
    HINF InfHandle;
    WCHAR PathBuffer[MAX_PATH];
    INFCONTEXT CabinetsContext;
    ULONG InfFileSize;
    PWCHAR KeyValue;
    UINT ErrorLine;
    PVOID InfFileData;

    MUIDisplayPage(PREPARE_COPY_PAGE);

    /* Create the file queue */
    SetupFileQueue = SetupOpenFileQueue();
    if (SetupFileQueue == NULL)
    {
        MUIDisplayError(ERROR_COPY_QUEUE, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    if (!PrepareCopyPageInfFile(SetupInf, NULL, Ir))
    {
        /* FIXME: show an error dialog */
        return QUIT_PAGE;
    }

    /* Search for the 'Cabinets' section */
    if (!SetupFindFirstLineW(SetupInf, L"Cabinets", NULL, &CabinetsContext))
    {
        return FILE_COPY_PAGE;
    }

    /*
     * Enumerate the directory values in the 'Cabinets'
     * section and parse their inf files.
     */
    do
    {
        if (!INF_GetData(&CabinetsContext, NULL, &KeyValue))
            break;

        CombinePaths(PathBuffer, ARRAYSIZE(PathBuffer), 2,
                     SourcePath.Buffer, KeyValue);

        CabinetInitialize();
        CabinetSetEventHandlers(NULL, NULL, NULL);
        CabinetSetCabinetName(PathBuffer);

        if (CabinetOpen() == CAB_STATUS_SUCCESS)
        {
            DPRINT("Cabinet %S\n", CabinetGetCabinetName());

            InfFileData = CabinetGetCabinetReservedArea(&InfFileSize);
            if (InfFileData == NULL)
            {
                MUIDisplayError(ERROR_CABINET_SCRIPT, Ir, POPUP_WAIT_ENTER);
                return QUIT_PAGE;
            }
        }
        else
        {
            DPRINT("Cannot open cabinet: %S.\n", CabinetGetCabinetName());
            MUIDisplayError(ERROR_CABINET_MISSING, Ir, POPUP_WAIT_ENTER);
            return QUIT_PAGE;
        }

        InfHandle = INF_OpenBufferedFileA((CHAR*) InfFileData,
                                          InfFileSize,
                                          (const CHAR*) NULL,
                                          INF_STYLE_WIN4,
                                          LanguageId,
                                          &ErrorLine);

        if (InfHandle == INVALID_HANDLE_VALUE)
        {
            MUIDisplayError(ERROR_INVALID_CABINET_INF, Ir, POPUP_WAIT_ENTER);
            return QUIT_PAGE;
        }

        CabinetCleanup();

        if (!PrepareCopyPageInfFile(InfHandle, KeyValue, Ir))
        {
            /* FIXME: show an error dialog */
            return QUIT_PAGE;
        }
    } while (SetupFindNextLine(&CabinetsContext, &CabinetsContext));

    return FILE_COPY_PAGE;
}


VOID
NTAPI
SetupUpdateMemoryInfo(IN PCOPYCONTEXT CopyContext,
                      IN BOOLEAN First)
{
    SYSTEM_PERFORMANCE_INFORMATION PerfInfo;

    /* Get the memory information from the system */
    NtQuerySystemInformation(SystemPerformanceInformation,
                             &PerfInfo,
                             sizeof(PerfInfo),
                             NULL);

    /* Check if this is initial setup */
    if (First)
    {
        /* Set maximum limits to be total RAM pages */
        ProgressSetStepCount(CopyContext->MemoryBars[0], PerfInfo.CommitLimit);
        ProgressSetStepCount(CopyContext->MemoryBars[1], PerfInfo.CommitLimit);
        ProgressSetStepCount(CopyContext->MemoryBars[2], PerfInfo.CommitLimit);
    }

    /* Set current values */
    ProgressSetStep(CopyContext->MemoryBars[0], PerfInfo.PagedPoolPages + PerfInfo.NonPagedPoolPages);
    ProgressSetStep(CopyContext->MemoryBars[1], PerfInfo.ResidentSystemCachePage);
    ProgressSetStep(CopyContext->MemoryBars[2], PerfInfo.AvailablePages);
}


static UINT
CALLBACK
FileCopyCallback(PVOID Context,
                 UINT Notification,
                 UINT_PTR Param1,
                 UINT_PTR Param2)
{
    PCOPYCONTEXT CopyContext;

    CopyContext = (PCOPYCONTEXT)Context;

    switch (Notification)
    {
        case SPFILENOTIFY_STARTSUBQUEUE:
            CopyContext->TotalOperations = (ULONG)Param2;
            ProgressSetStepCount(CopyContext->ProgressBar,
                                 CopyContext->TotalOperations);
            SetupUpdateMemoryInfo(CopyContext, TRUE);
            break;

        case SPFILENOTIFY_STARTCOPY:
            /* Display copy message */
            CONSOLE_SetStatusText(MUIGetString(STRING_COPYING), (PWSTR)Param1);
            SetupUpdateMemoryInfo(CopyContext, FALSE);
            break;

        case SPFILENOTIFY_ENDCOPY:
            CopyContext->CompletedOperations++;

            /* SYSREG checkpoint */
            if (CopyContext->TotalOperations >> 1 == CopyContext->CompletedOperations)
                DPRINT1("CHECKPOINT:HALF_COPIED\n");

            ProgressNextStep(CopyContext->ProgressBar);
            SetupUpdateMemoryInfo(CopyContext, FALSE);
            break;
    }

    return 0;
}


/*
 * Displays the FileCopyPage.
 *
 * Next pages:
 *  RegistryPage(At once)
 *
 * SIDEEFFECTS
 *  Calls SetupCommitFileQueueW
 *  Calls SetupCloseFileQueue
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
FileCopyPage(PINPUT_RECORD Ir)
{
    COPYCONTEXT CopyContext;
    unsigned int mem_bar_width;

    MUIDisplayPage(FILE_COPY_PAGE);

    /* Create context for the copy process */
    CopyContext.DestinationRootPath = DestinationRootPath.Buffer;
    CopyContext.InstallPath = InstallPath.Buffer;
    CopyContext.TotalOperations = 0;
    CopyContext.CompletedOperations = 0;

    /* Create the progress bar as well */
    CopyContext.ProgressBar = CreateProgressBar(13,
                                                26,
                                                xScreen - 13,
                                                yScreen - 20,
                                                10,
                                                24,
                                                TRUE,
                                                MUIGetString(STRING_SETUPCOPYINGFILES));

    // fit memory bars to screen width, distribute them uniform
    mem_bar_width = (xScreen - 26) / 5;
    mem_bar_width -= mem_bar_width % 2;  // make even
    /* ATTENTION: The following progress bars are debug stuff, which should not be translated!! */
    /* Create the paged pool progress bar */
    CopyContext.MemoryBars[0] = CreateProgressBar(13,
                                                  40,
                                                  13 + mem_bar_width,
                                                  43,
                                                  13,
                                                  44,
                                                  FALSE,
                                                  "Kernel Pool");

    /* Create the non paged pool progress bar */
    CopyContext.MemoryBars[1] = CreateProgressBar((xScreen / 2)- (mem_bar_width / 2),
                                                  40,
                                                  (xScreen / 2) + (mem_bar_width / 2),
                                                  43,
                                                  (xScreen / 2)- (mem_bar_width / 2),
                                                  44,
                                                  FALSE,
                                                  "Kernel Cache");

    /* Create the global memory progress bar */
    CopyContext.MemoryBars[2] = CreateProgressBar(xScreen - 13 - mem_bar_width,
                                                  40,
                                                  xScreen - 13,
                                                  43,
                                                  xScreen - 13 - mem_bar_width,
                                                  44,
                                                  FALSE,
                                                  "Free Memory");

    /* Do the file copying */
    SetupCommitFileQueueW(NULL,
                          SetupFileQueue,
                          FileCopyCallback,
                          &CopyContext);

    /* If we get here, we're done, so cleanup the queue and progress bar */
    SetupCloseFileQueue(SetupFileQueue);
    DestroyProgressBar(CopyContext.ProgressBar);
    DestroyProgressBar(CopyContext.MemoryBars[0]);
    DestroyProgressBar(CopyContext.MemoryBars[1]);
    DestroyProgressBar(CopyContext.MemoryBars[2]);

    /* Go display the next page */
    return REGISTRY_PAGE;
}


/*
 * Displays the RegistryPage.
 *
 * Next pages:
 *  SuccessPage (if RepairUpdate)
 *  BootLoaderPage (default)
 *  QuitPage
 *
 * SIDEEFFECTS
 *  Calls SetInstallPathValue
 *  Calls NtInitializeRegistry
 *  Calls ImportRegistryFile
 *  Calls SetDefaultPagefile
 *  Calls SetMountedDeviceValues
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
RegistryPage(PINPUT_RECORD Ir)
{
    INFCONTEXT InfContext;
    PWSTR Action;
    PWSTR File;
    PWSTR Section;
    BOOLEAN Delete;
    NTSTATUS Status;

    MUIDisplayPage(REGISTRY_PAGE);

    if (RepairUpdateFlag)
    {
        return SUCCESS_PAGE;
    }

    /************************ HACK!!!!!!!!!!! *********************************/
    if (!SetInstallPathValue(&DestinationPath))
    {
        DPRINT1("SetInstallPathValue() failed\n");
        MUIDisplayError(ERROR_INITIALIZE_REGISTRY, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }
    /************************ HACK!!!!!!!!!!! *********************************/

    /* Create the default hives */
    Status = NtInitializeRegistry(CM_BOOT_FLAG_SETUP);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("NtInitializeRegistry() failed (Status %lx)\n", Status);
        MUIDisplayError(ERROR_CREATE_HIVE, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    /* Update registry */
    CONSOLE_SetStatusText(MUIGetString(STRING_REGHIVEUPDATE));

    if (!SetupFindFirstLineW(SetupInf, L"HiveInfs.Install", NULL, &InfContext))
    {
        DPRINT1("SetupFindFirstLine() failed\n");
        MUIDisplayError(ERROR_FIND_REGISTRY, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    do
    {
        INF_GetDataField(&InfContext, 0, &Action);
        INF_GetDataField(&InfContext, 1, &File);
        INF_GetDataField(&InfContext, 2, &Section);

        DPRINT("Action: %S  File: %S  Section %S\n", Action, File, Section);

        if (Action == NULL)
            break; // Hackfix

        if (!_wcsicmp(Action, L"AddReg"))
        {
            Delete = FALSE;
        }
        else if (!_wcsicmp(Action, L"DelReg"))
        {
            Delete = TRUE;
        }
        else
        {
            continue;
        }

        CONSOLE_SetStatusText(MUIGetString(STRING_IMPORTFILE), File);

        if (!ImportRegistryFile(File, Section, LanguageId, Delete))
        {
            DPRINT1("Importing %S failed\n", File);

            MUIDisplayError(ERROR_IMPORT_HIVE, Ir, POPUP_WAIT_ENTER);
            return QUIT_PAGE;
        }
    } while (SetupFindNextLine(&InfContext, &InfContext));

    /* Update display registry settings */
    CONSOLE_SetStatusText(MUIGetString(STRING_DISPLAYETTINGSUPDATE));
    if (!ProcessDisplayRegistry(SetupInf, DisplayList))
    {
        MUIDisplayError(ERROR_UPDATE_DISPLAY_SETTINGS, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    /* Set the locale */
    CONSOLE_SetStatusText(MUIGetString(STRING_LOCALESETTINGSUPDATE));
    if (!ProcessLocaleRegistry(LanguageList))
    {
        MUIDisplayError(ERROR_UPDATE_LOCALESETTINGS, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    /* Add keyboard layouts */
    CONSOLE_SetStatusText(MUIGetString(STRING_ADDKBLAYOUTS));
    if (!AddKeyboardLayouts())
    {
        MUIDisplayError(ERROR_ADDING_KBLAYOUTS, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    /* Set GeoID */
    if (!SetGeoID(MUIGetGeoID()))
    {
        MUIDisplayError(ERROR_UPDATE_GEOID, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    if (!IsUnattendedSetup)
    {
        /* Update keyboard layout settings */
        CONSOLE_SetStatusText(MUIGetString(STRING_KEYBOARDSETTINGSUPDATE));
        if (!ProcessKeyboardLayoutRegistry(LayoutList))
        {
            MUIDisplayError(ERROR_UPDATE_KBSETTINGS, Ir, POPUP_WAIT_ENTER);
            return QUIT_PAGE;
        }
    }

    /* Add codepage information to registry */
    CONSOLE_SetStatusText(MUIGetString(STRING_CODEPAGEINFOUPDATE));
    if (!AddCodePage())
    {
        MUIDisplayError(ERROR_ADDING_CODEPAGE, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    /* Set the default pagefile entry */
    SetDefaultPagefile(DestinationDriveLetter);

    /* Update the mounted devices list */
    SetMountedDeviceValues(PartitionList);

    CONSOLE_SetStatusText(MUIGetString(STRING_DONE));

    return BOOT_LOADER_PAGE;
}


/*
 * Displays the BootLoaderPage.
 *
 * Next pages:
 *  SuccessPage (if RepairUpdate)
 *  BootLoaderHarddiskMbrPage
 *  BootLoaderHarddiskVbrPage
 *  BootLoaderFloppyPage
 *  SuccessPage
 *  QuitPage
 *
 * SIDEEFFECTS
 *  Calls SetInstallPathValue
 *  Calls NtInitializeRegistry
 *  Calls ImportRegistryFile
 *  Calls SetDefaultPagefile
 *  Calls SetMountedDeviceValues
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
BootLoaderPage(PINPUT_RECORD Ir)
{
    UCHAR PartitionType;
    BOOLEAN InstallOnFloppy;
    USHORT Line = 12;
    WCHAR PathBuffer[MAX_PATH];

    CONSOLE_SetStatusText(MUIGetString(STRING_PLEASEWAIT));

    RtlFreeUnicodeString(&SystemRootPath);
    StringCchPrintfW(PathBuffer, ARRAYSIZE(PathBuffer),
            L"\\Device\\Harddisk%lu\\Partition%lu\\",
            PartitionList->SystemPartition->DiskEntry->DiskNumber,
            PartitionList->SystemPartition->PartitionNumber);
    RtlCreateUnicodeString(&SystemRootPath, PathBuffer);
    DPRINT1("SystemRootPath: %wZ\n", &SystemRootPath);

    PartitionType = PartitionList->SystemPartition->PartitionType;

    if (IsUnattendedSetup)
    {
        if (UnattendMBRInstallType == 0) /* skip MBR installation */
        {
            return SUCCESS_PAGE;
        }
        else if (UnattendMBRInstallType == 1) /* install on floppy */
        {
            return BOOT_LOADER_FLOPPY_PAGE;
        }
    }

    if (PartitionType == PARTITION_ENTRY_UNUSED)
    {
        DPRINT("Error: system partition invalid (unused)\n");
        InstallOnFloppy = TRUE;
    }
    else if (PartitionType == PARTITION_OS2BOOTMGR)
    {
        /* OS/2 boot manager partition */
        DPRINT("Found OS/2 boot manager partition\n");
        InstallOnFloppy = TRUE;
    }
    else if (PartitionType == PARTITION_EXT2)
    {
        /* Linux EXT2 partition */
        DPRINT("Found Linux EXT2 partition\n");
        InstallOnFloppy = FALSE;
    }
    else if (PartitionType == PARTITION_IFS)
    {
        /* NTFS partition */
        DPRINT("Found NTFS partition\n");

        // FIXME: Make it FALSE when we'll support NTFS installation!
        InstallOnFloppy = TRUE;
    }
    else if ((PartitionType == PARTITION_FAT_12) ||
             (PartitionType == PARTITION_FAT_16) ||
             (PartitionType == PARTITION_HUGE) ||
             (PartitionType == PARTITION_XINT13) ||
             (PartitionType == PARTITION_FAT32) ||
             (PartitionType == PARTITION_FAT32_XINT13))
    {
        DPRINT("Found FAT partition\n");
        InstallOnFloppy = FALSE;
    }
    else
    {
        /* Unknown partition */
        DPRINT("Unknown partition found\n");
        InstallOnFloppy = TRUE;
    }

    if (InstallOnFloppy != FALSE)
    {
        return BOOT_LOADER_FLOPPY_PAGE;
    }

    /* Unattended install on hdd? */
    if (IsUnattendedSetup && UnattendMBRInstallType == 2)
    {
        return BOOT_LOADER_HARDDISK_MBR_PAGE;
    }

    MUIDisplayPage(BOOT_LOADER_PAGE);
    CONSOLE_InvertTextXY(8, Line, 60, 1);

    while (TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_DOWN))  /* DOWN */
        {
            CONSOLE_NormalTextXY(8, Line, 60, 1);

            Line++;
            if (Line<12)
                Line=15;

            if (Line>15)
                Line=12;

            CONSOLE_InvertTextXY(8, Line, 60, 1);
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_UP))  /* UP */
        {
            CONSOLE_NormalTextXY(8, Line, 60, 1);

            Line--;
            if (Line<12)
                Line=15;

            if (Line>15)
                Line=12;

            CONSOLE_InvertTextXY(8, Line, 60, 1);
        }
        else if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
                 (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3))  /* F3 */
        {
            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;

            break;
        }
        else if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x0D)    /* ENTER */
        {
            if (Line == 12)
            {
                return BOOT_LOADER_HARDDISK_MBR_PAGE;
            }
            else if (Line == 13)
            {
                return BOOT_LOADER_HARDDISK_VBR_PAGE;
            }
            else if (Line == 14)
            {
                return BOOT_LOADER_FLOPPY_PAGE;
            }
            else if (Line == 15)
            {
                return SUCCESS_PAGE;
            }

            return BOOT_LOADER_PAGE;
        }
    }

    return BOOT_LOADER_PAGE;
}


/*
 * Displays the BootLoaderFloppyPage.
 *
 * Next pages:
 *  SuccessPage (At once)
 *  QuitPage
 *
 * SIDEEFFECTS
 *  Calls InstallFatBootcodeToFloppy()
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
BootLoaderFloppyPage(PINPUT_RECORD Ir)
{
    NTSTATUS Status;

    MUIDisplayPage(BOOT_LOADER_FLOPPY_PAGE);

//  CONSOLE_SetStatusText(MUIGetString(STRING_PLEASEWAIT));

    while (TRUE)
    {
        CONSOLE_ConInKey(Ir);

        if ((Ir->Event.KeyEvent.uChar.AsciiChar == 0x00) &&
            (Ir->Event.KeyEvent.wVirtualKeyCode == VK_F3))  /* F3 */
        {
            if (ConfirmQuit(Ir) != FALSE)
                return QUIT_PAGE;

            break;
        }
        else if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x0D)    /* ENTER */
        {
            if (DoesPathExist(NULL, L"\\Device\\Floppy0\\") == FALSE)
            {
                MUIDisplayError(ERROR_NO_FLOPPY, Ir, POPUP_WAIT_ENTER);
                return BOOT_LOADER_FLOPPY_PAGE;
            }

            Status = InstallFatBootcodeToFloppy(&SourceRootPath, &DestinationArcPath);
            if (!NT_SUCCESS(Status))
            {
                /* Print error message */
                return BOOT_LOADER_FLOPPY_PAGE;
            }

            return SUCCESS_PAGE;
        }
    }

    return BOOT_LOADER_FLOPPY_PAGE;
}


/*
 * Displays the BootLoaderHarddiskVbrPage.
 *
 * Next pages:
 *  SuccessPage (At once)
 *  QuitPage
 *
 * SIDEEFFECTS
 *  Calls InstallVBRToPartition()
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
BootLoaderHarddiskVbrPage(PINPUT_RECORD Ir)
{
    UCHAR PartitionType;
    NTSTATUS Status;

    PartitionType = PartitionList->SystemPartition->PartitionType;

    Status = InstallVBRToPartition(&SystemRootPath,
                                   &SourceRootPath,
                                   &DestinationArcPath,
                                   PartitionType);
    if (!NT_SUCCESS(Status))
    {
        MUIDisplayError(ERROR_WRITE_BOOT, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    return SUCCESS_PAGE;
}


/*
 * Displays the BootLoaderHarddiskMbrPage.
 *
 * Next pages:
 *  SuccessPage (At once)
 *  QuitPage
 *
 * SIDEEFFECTS
 *  Calls InstallVBRToPartition()
 *  Calls InstallMbrBootCodeToDisk()
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
BootLoaderHarddiskMbrPage(PINPUT_RECORD Ir)
{
    UCHAR PartitionType;
    NTSTATUS Status;
    WCHAR DestinationDevicePathBuffer[MAX_PATH];
    WCHAR SourceMbrPathBuffer[MAX_PATH];
    WCHAR DstPath[MAX_PATH];

    /* Step 1: Write the VBR */
    PartitionType = PartitionList->SystemPartition->PartitionType;

    Status = InstallVBRToPartition(&SystemRootPath,
                                   &SourceRootPath,
                                   &DestinationArcPath,
                                   PartitionType);
    if (!NT_SUCCESS(Status))
    {
        MUIDisplayError(ERROR_WRITE_BOOT, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    /* Step 2: Write the MBR */
    StringCchPrintfW(DestinationDevicePathBuffer, ARRAYSIZE(DestinationDevicePathBuffer),
            L"\\Device\\Harddisk%d\\Partition0",
            PartitionList->SystemPartition->DiskEntry->DiskNumber);

    CombinePaths(SourceMbrPathBuffer, ARRAYSIZE(SourceMbrPathBuffer), 2, SourceRootPath.Buffer, L"\\loader\\dosmbr.bin");

    if (IsThereAValidBootSector(DestinationDevicePathBuffer))
    {
        /* Save current MBR */
        CombinePaths(DstPath, ARRAYSIZE(DstPath), 2, SystemRootPath.Buffer, L"mbr.old");

        DPRINT1("Save MBR: %S ==> %S\n", DestinationDevicePathBuffer, DstPath);
        Status = SaveBootSector(DestinationDevicePathBuffer, DstPath, sizeof(PARTITION_SECTOR));
        if (!NT_SUCCESS(Status))
        {
            DPRINT1("SaveBootSector() failed (Status %lx)\n", Status);
            // Don't care if we succeeded or not saving the old MBR, just go ahead.
        }
    }

    DPRINT1("Install MBR bootcode: %S ==> %S\n",
            SourceMbrPathBuffer, DestinationDevicePathBuffer);
    Status = InstallMbrBootCodeToDisk(SourceMbrPathBuffer,
                                      DestinationDevicePathBuffer);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("InstallMbrBootCodeToDisk() failed (Status %lx)\n",
                Status);
        MUIDisplayError(ERROR_INSTALL_BOOTCODE, Ir, POPUP_WAIT_ENTER);
        return QUIT_PAGE;
    }

    return SUCCESS_PAGE;
}


/**
 * @name ProgressTimeOutStringHandler
 *
 * Handles the generation (displaying) of the timeout
 * countdown to the screen dynamically.
 *
 * @param   Bar
 *     A pointer to a progress bar.
 *
 * @param   AlwaysUpdate
 *     Constantly update the progress bar (boolean type).
 *
 * @param   Buffer
 *     A pointer to a string buffer.
 *
 * @param   cchBufferSize
 *     The buffer's size in number of characters.
 *
 * @return
 *     TRUE or FALSE on function termination.
 *
 */
static
BOOLEAN NTAPI
ProgressTimeOutStringHandler(
    IN PPROGRESSBAR Bar,
    IN BOOLEAN AlwaysUpdate,
    OUT PSTR Buffer,
    IN SIZE_T cchBufferSize)
{
    ULONG OldProgress = Bar->Progress;

    if (Bar->StepCount == 0)
    {
        Bar->Progress = 0;
    }
    else
    {
        Bar->Progress = Bar->StepCount - Bar->CurrentStep;
    }

    /* Build the progress string if it has changed */
    if (Bar->ProgressFormatText &&
        (AlwaysUpdate || (Bar->Progress != OldProgress)))
    {
        RtlStringCchPrintfA(Buffer, cchBufferSize,
                            Bar->ProgressFormatText, Bar->Progress / max(1, Bar->Width) + 1);

        return TRUE;
    }

    return FALSE;
}

/**
 * @name ProgressCountdown
 *
 * Displays and draws a red-coloured progress bar with a countdown.
 * When the timeout is reached, the flush page is displayed for reboot.
 *
 * @param   Ir
 *     A pointer to an input keyboard record.
 *
 * @param   TimeOut
 *     Initial countdown value in seconds.
 *
 * @return
 *     Nothing.
 *
 */
static VOID
ProgressCountdown(
    IN PINPUT_RECORD Ir,
    IN LONG TimeOut)
{
    NTSTATUS Status;
    ULONG StartTime, BarWidth, TimerDiv;
    LONG TimeElapsed;
    LONG TimerValue, OldTimerValue;
    LARGE_INTEGER Timeout;
    PPROGRESSBAR ProgressBar;
    BOOLEAN RefreshProgress = TRUE;

    /* Bail out if the timeout is already zero */
    if (TimeOut <= 0)
        return;

    /* Create the timeout progress bar and set it up */
    ProgressBar = CreateProgressBarEx(13,
                                      26,
                                      xScreen - 13,
                                      yScreen - 20,
                                      10,
                                      24,
                                      TRUE,
                                      FOREGROUND_RED | BACKGROUND_BLUE,
                                      0,
                                      NULL,
                                      MUIGetString(STRING_REBOOTPROGRESSBAR),
                                      ProgressTimeOutStringHandler);

    BarWidth = max(1, ProgressBar->Width);
    TimerValue = TimeOut * BarWidth;
    ProgressSetStepCount(ProgressBar, TimerValue);

    StartTime = NtGetTickCount();
    CONSOLE_Flush();

    TimerDiv = 1000 / BarWidth;
    TimerDiv = max(1, TimerDiv);
    OldTimerValue = TimerValue;
    while (TRUE)
    {
        /* Decrease the timer */

        /*
         * Compute how much time the previous operations took.
         * This allows us in particular to take account for any time
         * elapsed if something slowed down.
         */
        TimeElapsed = NtGetTickCount() - StartTime;
        if (TimeElapsed >= TimerDiv)
        {
            /* Increase StartTime by steps of 1 / ProgressBar->Width seconds */
            TimeElapsed /= TimerDiv;
            StartTime += (TimerDiv * TimeElapsed);

            if (TimeElapsed <= TimerValue)
                TimerValue -= TimeElapsed;
            else
                TimerValue = 0;

            RefreshProgress = TRUE;
        }

        if (RefreshProgress)
        {
            ProgressSetStep(ProgressBar, OldTimerValue - TimerValue);
            RefreshProgress = FALSE;
        }

        /* Stop when the timer reaches zero */
        if (TimerValue <= 0)
            break;

        /* Check for user key presses */

        /*
         * If the timer is used, use a passive wait of maximum 1 second
         * while monitoring for incoming console input events, so that
         * we are still able to display the timing count.
         */

        /* Wait a maximum of 1 second for input events */
        TimeElapsed = NtGetTickCount() - StartTime;
        if (TimeElapsed < TimerDiv)
        {
            /* Convert the time to NT Format */
            Timeout.QuadPart = (TimerDiv - TimeElapsed) * -10000LL;
            Status = NtWaitForSingleObject(StdInput, FALSE, &Timeout);
        }
        else
        {
            Status = STATUS_TIMEOUT;
        }

        /* Check whether the input event has been signaled, or a timeout happened */
        if (Status == STATUS_TIMEOUT)
        {
            continue;
        }
        if (Status != STATUS_WAIT_0)
        {
            /* An error happened, bail out */
            DPRINT1("NtWaitForSingleObject() failed, Status 0x%08lx\n", Status);
            break;
        }

        /* Check for an ENTER key press */
        while (CONSOLE_ConInKeyPeek(Ir))
        {
            if (Ir->Event.KeyEvent.uChar.AsciiChar == 0x0D) /* ENTER */
            {
                /* Found it, stop waiting */
                goto Exit;
            }
        }
    }

Exit:
    /* Destroy the progress bar and quit */
    DestroyProgressBar(ProgressBar);
}


/*
 * Displays the QuitPage.
 *
 * Next pages:
 *  FlushPage (At once)
 *
 * SIDEEFFECTS
 *  Destroy the Lists
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
QuitPage(PINPUT_RECORD Ir)
{
    MUIDisplayPage(QUIT_PAGE);

    /* Destroy the partition list */
    if (PartitionList != NULL)
    {
        DestroyPartitionList(PartitionList);
        PartitionList = NULL;
    }
    TempPartition = NULL;
    FormatState = Start;

    /* Destroy the filesystem list */
    if (FileSystemList != NULL)
    {
        DestroyFileSystemList(FileSystemList);
        FileSystemList = NULL;
    }

    /* Destroy the computer settings list */
    if (ComputerList != NULL)
    {
        DestroyGenericList(ComputerList, TRUE);
        ComputerList = NULL;
    }

    /* Destroy the display settings list */
    if (DisplayList != NULL)
    {
        DestroyGenericList(DisplayList, TRUE);
        DisplayList = NULL;
    }

    /* Destroy the keyboard settings list */
    if (KeyboardList != NULL)
    {
        DestroyGenericList(KeyboardList, TRUE);
        KeyboardList = NULL;
    }

    /* Destroy the keyboard layout list */
    if (LayoutList != NULL)
    {
        DestroyGenericList(LayoutList, TRUE);
        LayoutList = NULL;
    }

    /* Destroy the languages list */
    if (LanguageList != NULL)
    {
        DestroyGenericList(LanguageList, FALSE);
        LanguageList = NULL;
    }

    CONSOLE_SetStatusText(MUIGetString(STRING_REBOOTCOMPUTER2));

    /* Wait for maximum 15 seconds or an ENTER key before quitting */
    ProgressCountdown(Ir, 15);
    return FLUSH_PAGE;
}


/*
 * Displays the SuccessPage.
 *
 * Next pages:
 *  FlushPage (At once)
 *
 * SIDEEFFECTS
 *  Destroy the Lists
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
SuccessPage(PINPUT_RECORD Ir)
{
    MUIDisplayPage(SUCCESS_PAGE);

    if (IsUnattendedSetup)
        return FLUSH_PAGE;

    /* Wait for maximum 15 seconds or an ENTER key before quitting */
    ProgressCountdown(Ir, 15);
    return FLUSH_PAGE;
}


/*
 * Displays the FlushPage.
 *
 * Next pages:
 *  RebootPage (At once)
 *
 * RETURNS
 *   Number of the next page.
 */
static PAGE_NUMBER
FlushPage(PINPUT_RECORD Ir)
{
    MUIDisplayPage(FLUSH_PAGE);
    return REBOOT_PAGE;
}


DWORD WINAPI
PnpEventThread(IN LPVOID lpParameter);


/*
 * The start routine and page management
 */
VOID
RunUSetup(VOID)
{
    INPUT_RECORD Ir;
    PAGE_NUMBER Page;
    LARGE_INTEGER Time;
    NTSTATUS Status;
    BOOLEAN Old;

    NtQuerySystemTime(&Time);

    /* Create the PnP thread in suspended state */
    Status = RtlCreateUserThread(NtCurrentProcess(),
                                 NULL,
                                 TRUE,
                                 0,
                                 0,
                                 0,
                                 PnpEventThread,
                                 &SetupInf,
                                 &hPnpThread,
                                 NULL);
    if (!NT_SUCCESS(Status))
        hPnpThread = INVALID_HANDLE_VALUE;

    if (!CONSOLE_Init())
    {
        PrintString(MUIGetString(STRING_CONSOLEFAIL1));
        PrintString(MUIGetString(STRING_CONSOLEFAIL2));
        PrintString(MUIGetString(STRING_CONSOLEFAIL3));

        /* Raise a hard error (crash the system/BSOD) */
        NtRaiseHardError(STATUS_SYSTEM_PROCESS_TERMINATED,
                         0,0,0,0,0);
    }

    /* Initialize global unicode strings */
    RtlInitUnicodeString(&SourcePath, NULL);
    RtlInitUnicodeString(&SourceRootPath, NULL);
    RtlInitUnicodeString(&SourceRootDir, NULL);
    RtlInitUnicodeString(&InstallPath, NULL);
    RtlInitUnicodeString(&DestinationPath, NULL);
    RtlInitUnicodeString(&DestinationArcPath, NULL);
    RtlInitUnicodeString(&DestinationRootPath, NULL);
    RtlInitUnicodeString(&SystemRootPath, NULL);

    /* Hide the cursor */
    CONSOLE_SetCursorType(TRUE, FALSE);

    Page = START_PAGE;
    while (Page != REBOOT_PAGE && Page != RECOVERY_PAGE)
    {
        CONSOLE_ClearScreen();
        CONSOLE_Flush();

        //CONSOLE_SetUnderlinedTextXY(4, 3, " ReactOS " KERNEL_VERSION_STR " Setup ");
        //CONSOLE_Flush();

        switch (Page)
        {
            /* Start page */
            case START_PAGE:
                Page = SetupStartPage(&Ir);
                break;

            /* Language page */
            case LANGUAGE_PAGE:
                Page = LanguagePage(&Ir);
                break;

            /* License page */
            case LICENSE_PAGE:
                Page = LicensePage(&Ir);
                break;

            /* Intro page */
            case INTRO_PAGE:
                Page = IntroPage(&Ir);
                break;

            /* Install pages */
            case INSTALL_INTRO_PAGE:
                Page = InstallIntroPage(&Ir);
                break;

#if 0
            case SCSI_CONTROLLER_PAGE:
                Page = ScsiControllerPage(&Ir);
                break;

            case OEM_DRIVER_PAGE:
                Page = OemDriverPage(&Ir);
                break;
#endif

            case DEVICE_SETTINGS_PAGE:
                Page = DeviceSettingsPage(&Ir);
                break;

            case COMPUTER_SETTINGS_PAGE:
                Page = ComputerSettingsPage(&Ir);
                break;

            case DISPLAY_SETTINGS_PAGE:
                Page = DisplaySettingsPage(&Ir);
                break;

            case KEYBOARD_SETTINGS_PAGE:
                Page = KeyboardSettingsPage(&Ir);
                break;

            case LAYOUT_SETTINGS_PAGE:
                Page = LayoutSettingsPage(&Ir);
                break;

            case SELECT_PARTITION_PAGE:
                Page = SelectPartitionPage(&Ir);
                break;

            case CREATE_PRIMARY_PARTITION_PAGE:
                Page = CreatePrimaryPartitionPage(&Ir);
                break;

            case CREATE_EXTENDED_PARTITION_PAGE:
                Page = CreateExtendedPartitionPage(&Ir);
                break;

            case CREATE_LOGICAL_PARTITION_PAGE:
                Page = CreateLogicalPartitionPage(&Ir);
                break;

            case CONFIRM_DELETE_SYSTEM_PARTITION_PAGE:
                Page = ConfirmDeleteSystemPartitionPage(&Ir);
                break;

            case DELETE_PARTITION_PAGE:
                Page = DeletePartitionPage(&Ir);
                break;

            case SELECT_FILE_SYSTEM_PAGE:
                Page = SelectFileSystemPage(&Ir);
                break;

            case FORMAT_PARTITION_PAGE:
                Page = FormatPartitionPage(&Ir);
                break;

            case CHECK_FILE_SYSTEM_PAGE:
                Page = CheckFileSystemPage(&Ir);
                break;

            case INSTALL_DIRECTORY_PAGE:
                Page = InstallDirectoryPage(&Ir);
                break;

            case PREPARE_COPY_PAGE:
                Page = PrepareCopyPage(&Ir);
                break;

            case FILE_COPY_PAGE:
                Page = FileCopyPage(&Ir);
                break;

            case REGISTRY_PAGE:
                Page = RegistryPage(&Ir);
                break;

            case BOOT_LOADER_PAGE:
                Page = BootLoaderPage(&Ir);
                break;

            case BOOT_LOADER_FLOPPY_PAGE:
                Page = BootLoaderFloppyPage(&Ir);
                break;

            case BOOT_LOADER_HARDDISK_MBR_PAGE:
                Page = BootLoaderHarddiskMbrPage(&Ir);
                break;

            case BOOT_LOADER_HARDDISK_VBR_PAGE:
                Page = BootLoaderHarddiskVbrPage(&Ir);
                break;

            /* Repair pages */
            case REPAIR_INTRO_PAGE:
                Page = RepairIntroPage(&Ir);
                break;

            case SUCCESS_PAGE:
                Page = SuccessPage(&Ir);
                break;

            case FLUSH_PAGE:
                Page = FlushPage(&Ir);
                break;

            case QUIT_PAGE:
                Page = QuitPage(&Ir);
                break;

            case RECOVERY_PAGE:
            case REBOOT_PAGE:
                break;
        }
    }

    if (Page == RECOVERY_PAGE)
        RecoveryConsole();

    FreeConsole();

    /* Avoid bugcheck */
    Time.QuadPart += 50000000;
    NtDelayExecution(FALSE, &Time);

    /* Reboot */
    RtlAdjustPrivilege(SE_SHUTDOWN_PRIVILEGE, TRUE, FALSE, &Old);
    NtShutdownSystem(ShutdownReboot);
    RtlAdjustPrivilege(SE_SHUTDOWN_PRIVILEGE, Old, FALSE, &Old);
    NtTerminateProcess(NtCurrentProcess(), 0);
}


VOID NTAPI
NtProcessStartup(PPEB Peb)
{
    RtlNormalizeProcessParams(Peb->ProcessParameters);

    ProcessHeap = Peb->ProcessHeap;
    InfSetHeap(ProcessHeap);
    RunUSetup();
}

/* EOF */
