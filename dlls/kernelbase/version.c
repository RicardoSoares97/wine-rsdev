/*
 * Implementation of VERSION.DLL
 *
 * Copyright 1996,1997 Marcus Meissner
 * Copyright 1997 David Cuthbert
 * Copyright 1999 Ulrich Weigand
 * Copyright 2005 Paul Vriens
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>

#include "ntstatus.h"
#include "windef.h"
#include "winbase.h"
#include "winver.h"
#include "winuser.h"
#include "winnls.h"
#include "winternl.h"
#include "winerror.h"
#include "winreg.h"
#include "appmodel.h"

#include "kernelbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ver);

typedef struct
{
    WORD offset;
    WORD length;
    WORD flags;
    WORD id;
    WORD handle;
    WORD usage;
} NE_NAMEINFO;

typedef struct
{
    WORD  type_id;
    WORD  count;
    DWORD resloader;
} NE_TYPEINFO;

struct version_info
{
    DWORD major;
    DWORD minor;
    DWORD build;
};

/***********************************************************************
 * Version Info Structure
 */

typedef struct
{
    WORD  wLength;
    WORD  wValueLength;
    CHAR  szKey[1];
#if 0   /* variable length structure */
    /* DWORD aligned */
    BYTE  Value[];
    /* DWORD aligned */
    VS_VERSION_INFO_STRUCT16 Children[];
#endif
} VS_VERSION_INFO_STRUCT16;

typedef struct
{
    WORD  wLength;
    WORD  wValueLength;
    WORD  wType; /* 1:Text, 0:Binary */
    WCHAR szKey[1];
#if 0   /* variable length structure */
    /* DWORD aligned */
    BYTE  Value[];
    /* DWORD aligned */
    VS_VERSION_INFO_STRUCT32 Children[];
#endif
} VS_VERSION_INFO_STRUCT32;

#define VersionInfoIs16( ver ) \
    ( ((const VS_VERSION_INFO_STRUCT16 *)ver)->szKey[0] >= ' ' )

#define DWORD_ALIGN( base, ptr ) \
    ( (LPBYTE)(base) + ((((LPBYTE)(ptr) - (LPBYTE)(base)) + 3) & ~3) )

#define VersionInfo16_Value( ver )  \
    DWORD_ALIGN( (ver), (ver)->szKey + strlen((ver)->szKey) + 1 )
#define VersionInfo32_Value( ver )  \
    DWORD_ALIGN( (ver), (ver)->szKey + lstrlenW((ver)->szKey) + 1 )

#define VersionInfo16_Children( ver )  \
    (const VS_VERSION_INFO_STRUCT16 *)( VersionInfo16_Value( ver ) + \
                           ( ( (ver)->wValueLength + 3 ) & ~3 ) )
#define VersionInfo32_Children( ver )  \
    (const VS_VERSION_INFO_STRUCT32 *)( VersionInfo32_Value( ver ) + \
                           ( ( (ver)->wValueLength * \
                               ((ver)->wType? 2 : 1) + 3 ) & ~3 ) )

#define VersionInfo16_Next( ver ) \
    (VS_VERSION_INFO_STRUCT16 *)( (LPBYTE)ver + (((ver)->wLength + 3) & ~3) )
#define VersionInfo32_Next( ver ) \
    (VS_VERSION_INFO_STRUCT32 *)( (LPBYTE)ver + (((ver)->wLength + 3) & ~3) )


/***********************************************************************
 * Win8 info, reported if the app doesn't provide compat GUID in the manifest and
 * doesn't have higher OS version in PE header.
 */
static const struct version_info windows8_version_info = { 6, 2, 9200 };

/***********************************************************************
 * Win8.1 info, reported if the app doesn't provide compat GUID in the manifest and
 * OS version in PE header is 8.1 or higher but below 10.
 */
static const struct version_info windows8_1_version_info = { 6, 3, 9600 };


/***********************************************************************
 * Windows versions that need compatibility GUID specified in manifest
 * in order to be reported by the APIs.
 */
static const struct
{
    struct version_info info;
    GUID guid;
} version_data[] =
{
    /* Windows 8.1 */
    {
        { 6, 3, 9600 },
        {0x1f676c76,0x80e1,0x4239,{0x95,0xbb,0x83,0xd0,0xf6,0xd0,0xda,0x78}}
    },
    /* Windows 10 */
    {
        { 10, 0, 19045 },
        {0x8e0f7a12,0xbfb3,0x4fe8,{0xb9,0xa5,0x48,0xfd,0x50,0xa1,0x5a,0x9a}}
    }
};


/******************************************************************************
 *  init_current_version
 *
 * Initialize the current_version variable.
 *
 * For compatibility, Windows 8.1 and later report Win8 version unless the app
 * has a manifest or higher OS version in the PE optional header
 * that confirms its compatibility with newer versions of Windows.
 *
 */
static RTL_OSVERSIONINFOEXW current_version;

static BOOL CALLBACK init_current_version(PINIT_ONCE init_once, PVOID parameter, PVOID *context)
{
    struct acci
    {
        DWORD ElementCount;
        COMPATIBILITY_CONTEXT_ELEMENT Elements[1];
    } *acci;
    BOOL have_os_compat_elements = FALSE;
    const struct version_info *ver;
    IMAGE_NT_HEADERS *nt;
    SIZE_T req;
    int idx;

    current_version.dwOSVersionInfoSize = sizeof(current_version);
    if (!set_ntstatus( RtlGetVersion(&current_version) )) return FALSE;

    for (idx = ARRAY_SIZE(version_data); idx--;)
        if ( current_version.dwMajorVersion >  version_data[idx].info.major ||
            (current_version.dwMajorVersion == version_data[idx].info.major &&
             current_version.dwMinorVersion >= version_data[idx].info.minor))
            break;

    if (idx < 0) return TRUE;
    ver = &windows8_version_info;

    if (RtlQueryInformationActivationContext(0, NtCurrentTeb()->Peb->ActivationContextData, NULL,
            CompatibilityInformationInActivationContext, NULL, 0, &req) != STATUS_BUFFER_TOO_SMALL
        || !req)
        goto done;

    if (!(acci = HeapAlloc(GetProcessHeap(), 0, req)))
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    if (RtlQueryInformationActivationContext(0, NtCurrentTeb()->Peb->ActivationContextData, NULL,
            CompatibilityInformationInActivationContext, acci, req, &req) == STATUS_SUCCESS)
    {
        do
        {
            DWORD i;

            for (i = 0; i < acci->ElementCount; i++)
            {
                if (acci->Elements[i].Type != ACTCTX_COMPATIBILITY_ELEMENT_TYPE_OS)
                    continue;

                have_os_compat_elements = TRUE;

                if (IsEqualGUID(&acci->Elements[i].Id, &version_data[idx].guid))
                {
                    ver = &version_data[idx].info;

                    if (ver->major == current_version.dwMajorVersion &&
                        ver->minor == current_version.dwMinorVersion)
                        ver = NULL;

                    idx = 0;  /* break from outer loop */
                    break;
                }
            }
        } while (idx--);
    }
    HeapFree(GetProcessHeap(), 0, acci);

done:
    if (!have_os_compat_elements && current_version.dwMajorVersion >= 10
            && (nt = RtlImageNtHeader(NtCurrentTeb()->Peb->ImageBaseAddress))
            && (nt->OptionalHeader.MajorOperatingSystemVersion > 6
            || (nt->OptionalHeader.MajorOperatingSystemVersion == 6
            && nt->OptionalHeader.MinorOperatingSystemVersion >= 3)))
    {
        if (current_version.dwMajorVersion > 10)
            FIXME("Unsupported current_version.dwMajorVersion %lu.\n", current_version.dwMajorVersion);

        ver = nt->OptionalHeader.MajorOperatingSystemVersion >= 10 ? NULL : &windows8_1_version_info;
    }

    if (ver)
    {
        current_version.dwMajorVersion = ver->major;
        current_version.dwMinorVersion = ver->minor;
        current_version.dwBuildNumber  = ver->build;
    }
    return TRUE;
}


/**********************************************************************
 *  find_entry_by_id
 *
 * Find an entry by id in a resource directory
 * Copied from loader/pe_resource.c
 */
static const IMAGE_RESOURCE_DIRECTORY *find_entry_by_id( const IMAGE_RESOURCE_DIRECTORY *dir,
                                                         WORD id, const void *root,
                                                         DWORD root_size )
{
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry;
    int min, max, pos;

    entry = (const IMAGE_RESOURCE_DIRECTORY_ENTRY *)(dir + 1);
    min = dir->NumberOfNamedEntries;
    max = min + dir->NumberOfIdEntries - 1;

    if (max >= (root_size - ((INT_PTR)dir - (INT_PTR)root) - sizeof(*dir)) / sizeof(*entry))
        return NULL;

    while (min <= max)
    {
        pos = (min + max) / 2;
        if (entry[pos].Id == id)
        {
            DWORD offset = entry[pos].OffsetToDirectory;
            if (offset > root_size - sizeof(*dir)) return NULL;
            return (const IMAGE_RESOURCE_DIRECTORY *)((const char *)root + offset);
        }
        if (entry[pos].Id > id) max = pos - 1;
        else min = pos + 1;
    }
    return NULL;
}


/**********************************************************************
 *  find_entry_default
 *
 * Find a default entry in a resource directory
 * Copied from loader/pe_resource.c
 */
static const IMAGE_RESOURCE_DIRECTORY *find_entry_default( const IMAGE_RESOURCE_DIRECTORY *dir,
                                                           const void *root )
{
    const IMAGE_RESOURCE_DIRECTORY_ENTRY *entry;

    entry = (const IMAGE_RESOURCE_DIRECTORY_ENTRY *)(dir + 1);
    return (const IMAGE_RESOURCE_DIRECTORY *)((const char *)root + entry->OffsetToDirectory);
}


/**********************************************************************
 *  push_language
 *
 * push a language onto the list of languages to try
 */
static inline int push_language( WORD *list, int pos, WORD lang )
{
    int i;
    for (i = 0; i < pos; i++) if (list[i] == lang) return pos;
    list[pos++] = lang;
    return pos;
}


/**********************************************************************
 *  find_entry_language
 */
static const IMAGE_RESOURCE_DIRECTORY *find_entry_language( const IMAGE_RESOURCE_DIRECTORY *dir,
                                                            const void *root, DWORD root_size,
                                                            DWORD flags )
{
    const IMAGE_RESOURCE_DIRECTORY *ret;
    WORD list[9];
    int i, pos = 0;

    if (flags & FILE_VER_GET_LOCALISED)
    {
        /* cf. LdrFindResource_U */
        pos = push_language( list, pos, MAKELANGID( LANG_NEUTRAL, SUBLANG_NEUTRAL ) );
        pos = push_language( list, pos, LANGIDFROMLCID( NtCurrentTeb()->CurrentLocale ) );
        pos = push_language( list, pos, GetUserDefaultLangID() );
        pos = push_language( list, pos, MAKELANGID( PRIMARYLANGID(GetUserDefaultLangID()), SUBLANG_NEUTRAL ));
        pos = push_language( list, pos, MAKELANGID( PRIMARYLANGID(GetUserDefaultLangID()), SUBLANG_DEFAULT ));
        pos = push_language( list, pos, GetSystemDefaultLangID() );
        pos = push_language( list, pos, MAKELANGID( PRIMARYLANGID(GetSystemDefaultLangID()), SUBLANG_NEUTRAL ));
        pos = push_language( list, pos, MAKELANGID( PRIMARYLANGID(GetSystemDefaultLangID()), SUBLANG_DEFAULT ));
        pos = push_language( list, pos, MAKELANGID( LANG_ENGLISH, SUBLANG_DEFAULT ) );
    }
    else
    {
        /* FIXME: resolve LN file here */
        pos = push_language( list, pos, MAKELANGID( LANG_ENGLISH, SUBLANG_DEFAULT ) );
    }

    for (i = 0; i < pos; i++) if ((ret = find_entry_by_id( dir, list[i], root, root_size ))) return ret;
    return find_entry_default( dir, root );
}


static DWORD read_data( HANDLE handle, DWORD offset, void *data, DWORD len )
{
    DWORD res;

    SetFilePointer( handle, offset, NULL, FILE_BEGIN );
    if (!ReadFile( handle, data, len, &res, NULL )) res = 0;
    return res;
}

/***********************************************************************
 *           find_ne_resource         [internal]
 */
static BOOL find_ne_resource( HANDLE handle, DWORD *resLen, DWORD *resOff )
{
    const WORD typeid = VS_FILE_INFO | 0x8000;
    const WORD resid = VS_VERSION_INFO | 0x8000;
    IMAGE_OS2_HEADER nehd;
    NE_TYPEINFO *typeInfo;
    NE_NAMEINFO *nameInfo;
    DWORD nehdoffset = *resOff;
    LPBYTE resTab;
    DWORD resTabSize;
    int count;

    /* Read in NE header */
    if (read_data( handle, nehdoffset, &nehd, sizeof(nehd) ) != sizeof(nehd)) return FALSE;

    resTabSize = nehd.ne_restab - nehd.ne_rsrctab;
    if ( !resTabSize )
    {
        TRACE("No resources in NE dll\n" );
        return FALSE;
    }

    /* Read in resource table */
    resTab = HeapAlloc( GetProcessHeap(), 0, resTabSize );
    if ( !resTab ) return FALSE;

    if (read_data( handle, nehd.ne_rsrctab + nehdoffset, resTab, resTabSize ) != resTabSize)
    {
        HeapFree( GetProcessHeap(), 0, resTab );
        return FALSE;
    }

    /* Find resource */
    typeInfo = (NE_TYPEINFO *)(resTab + 2);
    while (typeInfo->type_id)
    {
        if (typeInfo->type_id == typeid) goto found_type;
        typeInfo = (NE_TYPEINFO *)((char *)(typeInfo + 1) +
                                   typeInfo->count * sizeof(NE_NAMEINFO));
    }
    TRACE("No typeid entry found\n" );
    HeapFree( GetProcessHeap(), 0, resTab );
    return FALSE;

 found_type:
    nameInfo = (NE_NAMEINFO *)(typeInfo + 1);

    for (count = typeInfo->count; count > 0; count--, nameInfo++)
        if (nameInfo->id == resid) goto found_name;

    TRACE("No resid entry found\n" );
    HeapFree( GetProcessHeap(), 0, resTab );
    return FALSE;

 found_name:
    /* Return resource data */
    *resLen = nameInfo->length << *(WORD *)resTab;
    *resOff = nameInfo->offset << *(WORD *)resTab;

    HeapFree( GetProcessHeap(), 0, resTab );
    return TRUE;
}

/***********************************************************************
 *           find_pe_resource         [internal]
 */
static BOOL find_pe_resource( HANDLE handle, DWORD *resLen, DWORD *resOff, DWORD flags )
{
    union
    {
        IMAGE_NT_HEADERS32 nt32;
        IMAGE_NT_HEADERS64 nt64;
    } pehd;
    DWORD pehdoffset = *resOff;
    PIMAGE_DATA_DIRECTORY resDataDir;
    PIMAGE_SECTION_HEADER sections;
    LPBYTE resSection;
    DWORD len, section_size, data_size, resDirSize;
    const void *resDir;
    const IMAGE_RESOURCE_DIRECTORY *resPtr;
    const IMAGE_RESOURCE_DATA_ENTRY *resData;
    int i, nSections;
    BOOL ret = FALSE;

    /* Read in PE header */
    len = read_data( handle, pehdoffset, &pehd, sizeof(pehd) );
    if (len < sizeof(pehd.nt32.FileHeader)) return FALSE;
    if (len < sizeof(pehd)) memset( (char *)&pehd + len, 0, sizeof(pehd) - len );

    switch (pehd.nt32.OptionalHeader.Magic)
    {
    case IMAGE_NT_OPTIONAL_HDR32_MAGIC:
        resDataDir = pehd.nt32.OptionalHeader.DataDirectory + IMAGE_DIRECTORY_ENTRY_RESOURCE;
        break;
    case IMAGE_NT_OPTIONAL_HDR64_MAGIC:
        resDataDir = pehd.nt64.OptionalHeader.DataDirectory + IMAGE_DIRECTORY_ENTRY_RESOURCE;
        break;
    default:
        return FALSE;
    }

    if ( !resDataDir->Size )
    {
        TRACE("No resources in PE dll\n" );
        return FALSE;
    }

    /* Read in section table */
    nSections = pehd.nt32.FileHeader.NumberOfSections;
    sections = HeapAlloc( GetProcessHeap(), 0,
                          nSections * sizeof(IMAGE_SECTION_HEADER) );
    if ( !sections ) return FALSE;

    len = FIELD_OFFSET( IMAGE_NT_HEADERS32, OptionalHeader ) + pehd.nt32.FileHeader.SizeOfOptionalHeader;
    if (read_data( handle, pehdoffset + len, sections, nSections * sizeof(IMAGE_SECTION_HEADER) ) !=
        nSections * sizeof(IMAGE_SECTION_HEADER))
    {
        HeapFree( GetProcessHeap(), 0, sections );
        return FALSE;
    }

    /* Find resource section */
    for ( i = 0; i < nSections; i++ )
        if (    resDataDir->VirtualAddress >= sections[i].VirtualAddress
             && resDataDir->VirtualAddress <  sections[i].VirtualAddress +
                                              sections[i].SizeOfRawData )
            break;

    if ( i == nSections )
    {
        HeapFree( GetProcessHeap(), 0, sections );
        TRACE("Couldn't find resource section\n" );
        return FALSE;
    }

    /* Read in resource section */
    data_size = sections[i].SizeOfRawData;
    section_size = max( data_size, sections[i].Misc.VirtualSize );
    resSection = HeapAlloc( GetProcessHeap(), 0, section_size );
    if ( !resSection )
    {
        HeapFree( GetProcessHeap(), 0, sections );
        return FALSE;
    }

    if (read_data( handle, sections[i].PointerToRawData, resSection, data_size ) != data_size) goto done;
    if (data_size < section_size) memset( (char *)resSection + data_size, 0, section_size - data_size );

    /* Find resource */
    resDir = resSection + (resDataDir->VirtualAddress - sections[i].VirtualAddress);
    resDirSize = section_size - (resDataDir->VirtualAddress - sections[i].VirtualAddress);

    resPtr = resDir;
    resPtr = find_entry_by_id( resPtr, VS_FILE_INFO, resDir, resDirSize );
    if ( !resPtr )
    {
        TRACE("No typeid entry found\n" );
        goto done;
    }
    resPtr = find_entry_by_id( resPtr, VS_VERSION_INFO, resDir, resDirSize );
    if ( !resPtr )
    {
        TRACE("No resid entry found\n" );
        goto done;
    }
    resPtr = find_entry_language( resPtr, resDir, resDirSize, flags );
    if ( !resPtr )
    {
        TRACE("No default language entry found\n" );
        goto done;
    }

    /* Find resource data section */
    resData = (const IMAGE_RESOURCE_DATA_ENTRY*)resPtr;
    for ( i = 0; i < nSections; i++ )
        if (    resData->OffsetToData >= sections[i].VirtualAddress
             && resData->OffsetToData <  sections[i].VirtualAddress +
                                         sections[i].SizeOfRawData )
            break;

    if ( i == nSections )
    {
        TRACE("Couldn't find resource data section\n" );
        goto done;
    }

    /* Return resource data */
    *resLen = resData->Size;
    *resOff = resData->OffsetToData - sections[i].VirtualAddress + sections[i].PointerToRawData;
    ret = TRUE;

 done:
    HeapFree( GetProcessHeap(), 0, resSection );
    HeapFree( GetProcessHeap(), 0, sections );
    return ret;
}


/***********************************************************************
 *           find_version_resource         [internal]
 */
static DWORD find_version_resource( HANDLE handle, DWORD *reslen, DWORD *offset, DWORD flags )
{
    IMAGE_DOS_HEADER mzh;
    WORD magic;

    if (read_data( handle, 0, &mzh, sizeof(mzh) ) != sizeof(mzh)) return 0;
    if (mzh.e_magic != IMAGE_DOS_SIGNATURE) return 0;

    if (read_data( handle, mzh.e_lfanew, &magic, sizeof(magic) ) != sizeof(magic)) return 0;
    *offset = mzh.e_lfanew;

    switch (magic)
    {
    case IMAGE_OS2_SIGNATURE:
        if (!find_ne_resource( handle, reslen, offset )) magic = 0;
        break;
    case IMAGE_NT_SIGNATURE:
        if (!find_pe_resource( handle, reslen, offset, flags )) magic = 0;
        break;
    }
    WARN( "Can't handle %04x files.\n", magic );
    return magic;
}

/******************************************************************************
 *   This function will print via standard TRACE, debug info regarding
 *   the file info structure vffi.
 */
static void print_vffi_debug(const VS_FIXEDFILEINFO *vffi)
{
    BOOL    versioned_printer = FALSE;

    if((vffi->dwFileType == VFT_DLL) || (vffi->dwFileType == VFT_DRV))
    {
        if(vffi->dwFileSubtype == VFT2_DRV_VERSIONED_PRINTER)
            /* this is documented for newer w2k Drivers and up */
            versioned_printer = TRUE;
        else if( (vffi->dwFileSubtype == VFT2_DRV_PRINTER) &&
                 (vffi->dwFileVersionMS != vffi->dwProductVersionMS) &&
                 (vffi->dwFileVersionMS > 0) &&
                 (vffi->dwFileVersionMS <= 3) )
            /* found this on NT 3.51, NT4.0 and old w2k Drivers */
            versioned_printer = TRUE;
    }

    TRACE("structversion=%u.%u, ",
            HIWORD(vffi->dwStrucVersion),LOWORD(vffi->dwStrucVersion));
    if(versioned_printer)
    {
        WORD mode = LOWORD(vffi->dwFileVersionMS);
        WORD ver_rev = HIWORD(vffi->dwFileVersionLS);
        TRACE("fileversion=%lu.%u.%u.%u (%s.major.minor.release), ",
            (vffi->dwFileVersionMS),
            HIBYTE(ver_rev), LOBYTE(ver_rev), LOWORD(vffi->dwFileVersionLS),
            (mode == 3) ? "Usermode" : ((mode <= 2) ? "Kernelmode" : "?") );
    }
    else
    {
        TRACE("fileversion=%u.%u.%u.%u, ",
            HIWORD(vffi->dwFileVersionMS),LOWORD(vffi->dwFileVersionMS),
            HIWORD(vffi->dwFileVersionLS),LOWORD(vffi->dwFileVersionLS));
    }
    TRACE("productversion=%u.%u.%u.%u\n",
          HIWORD(vffi->dwProductVersionMS),LOWORD(vffi->dwProductVersionMS),
          HIWORD(vffi->dwProductVersionLS),LOWORD(vffi->dwProductVersionLS));

    TRACE("flagmask=0x%lx, flags=0x%lx %s%s%s%s%s%s\n",
          vffi->dwFileFlagsMask, vffi->dwFileFlags,
          (vffi->dwFileFlags & VS_FF_DEBUG) ? "DEBUG," : "",
          (vffi->dwFileFlags & VS_FF_PRERELEASE) ? "PRERELEASE," : "",
          (vffi->dwFileFlags & VS_FF_PATCHED) ? "PATCHED," : "",
          (vffi->dwFileFlags & VS_FF_PRIVATEBUILD) ? "PRIVATEBUILD," : "",
          (vffi->dwFileFlags & VS_FF_INFOINFERRED) ? "INFOINFERRED," : "",
          (vffi->dwFileFlags & VS_FF_SPECIALBUILD) ? "SPECIALBUILD," : "");

    TRACE("(");

    TRACE("OS=0x%x.0x%x ", HIWORD(vffi->dwFileOS), LOWORD(vffi->dwFileOS));

    switch (vffi->dwFileOS&0xFFFF0000)
    {
    case VOS_DOS:TRACE("DOS,");break;
    case VOS_OS216:TRACE("OS/2-16,");break;
    case VOS_OS232:TRACE("OS/2-32,");break;
    case VOS_NT:TRACE("NT,");break;
    case VOS_UNKNOWN:
    default:
        TRACE("UNKNOWN(0x%lx),",vffi->dwFileOS&0xFFFF0000);break;
    }

    switch (LOWORD(vffi->dwFileOS))
    {
    case VOS__BASE:TRACE("BASE");break;
    case VOS__WINDOWS16:TRACE("WIN16");break;
    case VOS__WINDOWS32:TRACE("WIN32");break;
    case VOS__PM16:TRACE("PM16");break;
    case VOS__PM32:TRACE("PM32");break;
    default:
        TRACE("UNKNOWN(0x%x)",LOWORD(vffi->dwFileOS));break;
    }

    TRACE(")\n");

    switch (vffi->dwFileType)
    {
    case VFT_APP:TRACE("filetype=APP");break;
    case VFT_DLL:
        TRACE("filetype=DLL");
        if(vffi->dwFileSubtype != 0)
        {
            if(versioned_printer) /* NT3.x/NT4.0 or old w2k Driver  */
                TRACE(",PRINTER");
            TRACE(" (subtype=0x%lx)", vffi->dwFileSubtype);
        }
        break;
    case VFT_DRV:
        TRACE("filetype=DRV,");
        switch(vffi->dwFileSubtype)
        {
        case VFT2_DRV_PRINTER:TRACE("PRINTER");break;
        case VFT2_DRV_KEYBOARD:TRACE("KEYBOARD");break;
        case VFT2_DRV_LANGUAGE:TRACE("LANGUAGE");break;
        case VFT2_DRV_DISPLAY:TRACE("DISPLAY");break;
        case VFT2_DRV_MOUSE:TRACE("MOUSE");break;
        case VFT2_DRV_NETWORK:TRACE("NETWORK");break;
        case VFT2_DRV_SYSTEM:TRACE("SYSTEM");break;
        case VFT2_DRV_INSTALLABLE:TRACE("INSTALLABLE");break;
        case VFT2_DRV_SOUND:TRACE("SOUND");break;
        case VFT2_DRV_COMM:TRACE("COMM");break;
        case VFT2_DRV_INPUTMETHOD:TRACE("INPUTMETHOD");break;
        case VFT2_DRV_VERSIONED_PRINTER:TRACE("VERSIONED_PRINTER");break;
        case VFT2_UNKNOWN:
        default:
            TRACE("UNKNOWN(0x%lx)",vffi->dwFileSubtype);break;
        }
        break;
    case VFT_FONT:
        TRACE("filetype=FONT,");
        switch (vffi->dwFileSubtype)
        {
        case VFT2_FONT_RASTER:TRACE("RASTER");break;
        case VFT2_FONT_VECTOR:TRACE("VECTOR");break;
        case VFT2_FONT_TRUETYPE:TRACE("TRUETYPE");break;
        default:TRACE("UNKNOWN(0x%lx)",vffi->dwFileSubtype);break;
        }
        break;
    case VFT_VXD:TRACE("filetype=VXD");break;
    case VFT_STATIC_LIB:TRACE("filetype=STATIC_LIB");break;
    case VFT_UNKNOWN:
    default:
        TRACE("filetype=Unknown(0x%lx)",vffi->dwFileType);break;
    }

    TRACE("\n");
    TRACE("filedate=0x%lx.0x%lx\n",vffi->dwFileDateMS,vffi->dwFileDateLS);
}

/***********************************************************************
 *           GetFileVersionInfoSizeW         (kernelbase.@)
 */
DWORD WINAPI GetFileVersionInfoSizeW( LPCWSTR filename, LPDWORD handle )
{
    return GetFileVersionInfoSizeExW( FILE_VER_GET_LOCALISED, filename, handle );
}

/***********************************************************************
 *           GetFileVersionInfoSizeA         (kernelbase.@)
 */
DWORD WINAPI GetFileVersionInfoSizeA( LPCSTR filename, LPDWORD handle )
{
    return GetFileVersionInfoSizeExA( FILE_VER_GET_LOCALISED, filename, handle );
}

/******************************************************************************
 *           GetFileVersionInfoSizeExW       (kernelbase.@)
 */
DWORD WINAPI GetFileVersionInfoSizeExW( DWORD flags, LPCWSTR filename, LPDWORD ret_handle )
{
    DWORD len, offset, magic = 1;
    HMODULE hModule;

    TRACE("(0x%lx,%s,%p)\n", flags, debugstr_w(filename), ret_handle );

    if (ret_handle) *ret_handle = 0;

    if (!filename)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (!*filename)
    {
        SetLastError(ERROR_BAD_PATHNAME);
        return 0;
    }
    if (flags & ~FILE_VER_GET_LOCALISED)
        FIXME("flags 0x%lx ignored\n", flags & ~FILE_VER_GET_LOCALISED);

    if ((hModule = LoadLibraryExW( filename, 0, LOAD_LIBRARY_AS_IMAGE_RESOURCE )))
    {
        HRSRC hRsrc = NULL;
        if (!(flags & FILE_VER_GET_LOCALISED))
        {
            LANGID english = MAKELANGID( LANG_ENGLISH, SUBLANG_DEFAULT );
            hRsrc = FindResourceExW( hModule, (LPWSTR)VS_FILE_INFO,
                                     MAKEINTRESOURCEW(VS_VERSION_INFO), english );
        }
        if (!hRsrc)
            hRsrc = FindResourceW( hModule, MAKEINTRESOURCEW(VS_VERSION_INFO),
                                   (LPWSTR)VS_FILE_INFO );
        if (hRsrc)
        {
            magic = IMAGE_NT_SIGNATURE;
            len = SizeofResource( hModule, hRsrc );
        }
        FreeLibrary( hModule );
    }
    else
    {
        HANDLE handle = CreateFileW( filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     NULL, OPEN_EXISTING, 0, 0 );
        if (handle == INVALID_HANDLE_VALUE) return 0;
        magic = find_version_resource( handle, &len, &offset, flags );
        CloseHandle( handle );
    }

    switch (magic)
    {
    case IMAGE_OS2_SIGNATURE:
        /* We have a 16bit resource.
         *
         * XP/W2K/W2K3 uses a buffer which is more than the actual needed space:
         *
         * (info->wLength - sizeof(VS_FIXEDFILEINFO)) * 4
         *
         * This extra buffer is used for ANSI to Unicode conversions in W-Calls.
         * info->wLength should be the same as len. Currently it isn't but that
         * doesn't seem to be a problem (len is bigger than info->wLength).
         */
        SetLastError(0);
        return (len - sizeof(VS_FIXEDFILEINFO)) * 4;

    case IMAGE_NT_SIGNATURE:
        /* We have a 32bit resource.
         *
         * XP/W2K/W2K3 uses a buffer which is 2 times the actual needed space + 4 bytes "FE2X"
         * This extra buffer is used for Unicode to ANSI conversions in A-Calls
         */
        SetLastError(0);
        return (len * 2) + 4;

    default:
        if (GetVersion() & 0x80000000) /* Windows 95/98 */
            SetLastError(ERROR_FILE_NOT_FOUND);
        else
            SetLastError(ERROR_RESOURCE_DATA_NOT_FOUND);
        return 0;
    }
}

/******************************************************************************
 *           GetFileVersionInfoSizeExA       (kernelbase.@)
 */
DWORD WINAPI GetFileVersionInfoSizeExA( DWORD flags, LPCSTR filename, LPDWORD handle )
{
    UNICODE_STRING filenameW;
    DWORD retval;

    TRACE("(0x%lx,%s,%p)\n", flags, debugstr_a(filename), handle );

    if(filename)
        RtlCreateUnicodeStringFromAsciiz(&filenameW, filename);
    else
        filenameW.Buffer = NULL;

    retval = GetFileVersionInfoSizeExW(flags, filenameW.Buffer, handle);

    RtlFreeUnicodeString(&filenameW);

    return retval;
}

/***********************************************************************
 *           GetFileVersionInfoExW           (kernelbase.@)
 */
BOOL WINAPI GetFileVersionInfoExW( DWORD flags, LPCWSTR filename, DWORD ignored, DWORD datasize, LPVOID data )
{
    static const char signature[4] = "FE2X";
    DWORD len, offset, magic = 1;
    HMODULE hModule;
    VS_VERSION_INFO_STRUCT32* vvis = data;

    TRACE("(0x%lx,%s,%ld,size=%ld,data=%p)\n",
          flags, debugstr_w(filename), ignored, datasize, data );

    if (!data)
    {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (flags & ~FILE_VER_GET_LOCALISED)
        FIXME("flags 0x%lx ignored\n", flags & ~FILE_VER_GET_LOCALISED);

    if ((hModule = LoadLibraryExW( filename, 0, LOAD_LIBRARY_AS_IMAGE_RESOURCE )))
    {
        HRSRC hRsrc = NULL;
        if (!(flags & FILE_VER_GET_LOCALISED))
        {
            LANGID english = MAKELANGID( LANG_ENGLISH, SUBLANG_DEFAULT );
            hRsrc = FindResourceExW( hModule, (LPWSTR)VS_FILE_INFO,
                                     MAKEINTRESOURCEW(VS_VERSION_INFO), english );
        }
        if (!hRsrc)
            hRsrc = FindResourceW( hModule, MAKEINTRESOURCEW(VS_VERSION_INFO),
                                   (LPWSTR)VS_FILE_INFO );
        if (hRsrc)
        {
            HGLOBAL hMem = LoadResource( hModule, hRsrc );
            magic = IMAGE_NT_SIGNATURE;
            len = min( SizeofResource(hModule, hRsrc), datasize );
            memcpy( data, LockResource( hMem ), len );
            FreeResource( hMem );
        }
        FreeLibrary( hModule );
    }
    else
    {
        HANDLE handle = CreateFileW( filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     NULL, OPEN_EXISTING, 0, 0 );
        if (handle == INVALID_HANDLE_VALUE) return 0;
        if ((magic = find_version_resource( handle, &len, &offset, flags )))
            len = read_data( handle, offset, data, min( len, datasize ));
        CloseHandle( handle );
    }

    switch (magic)
    {
    case IMAGE_OS2_SIGNATURE:
        /* We have a 16bit resource. */
        if (TRACE_ON(ver))
            print_vffi_debug( (VS_FIXEDFILEINFO *)VersionInfo16_Value( (VS_VERSION_INFO_STRUCT16 *)data ));
        SetLastError(0);
        return TRUE;

    case IMAGE_NT_SIGNATURE:
        /* We have a 32bit resource.
         *
         * XP/W2K/W2K3 uses a buffer which is 2 times the actual needed space + 4 bytes "FE2X"
         * This extra buffer is used for Unicode to ANSI conversions in A-Calls
         */
        len = vvis->wLength + sizeof(signature);
        if (datasize >= len) memcpy( (char*)data + vvis->wLength, signature, sizeof(signature) );
        if (TRACE_ON(ver))
            print_vffi_debug( (VS_FIXEDFILEINFO *)VersionInfo32_Value( vvis ));
        SetLastError(0);
        return TRUE;

    default:
        SetLastError( ERROR_RESOURCE_DATA_NOT_FOUND );
        return FALSE;
    }
}

/***********************************************************************
 *           GetFileVersionInfoExA           (kernelbase.@)
 */
BOOL WINAPI GetFileVersionInfoExA( DWORD flags, LPCSTR filename, DWORD handle, DWORD datasize, LPVOID data )
{
    UNICODE_STRING filenameW;
    BOOL retval;

    TRACE("(0x%lx,%s,%ld,size=%ld,data=%p)\n",
          flags, debugstr_a(filename), handle, datasize, data );

    if(filename)
        RtlCreateUnicodeStringFromAsciiz(&filenameW, filename);
    else
        filenameW.Buffer = NULL;

    retval = GetFileVersionInfoExW(flags, filenameW.Buffer, handle, datasize, data);

    RtlFreeUnicodeString(&filenameW);

    return retval;
}

/***********************************************************************
 *           GetFileVersionInfoW             (kernelbase.@)
 */
BOOL WINAPI GetFileVersionInfoW( LPCWSTR filename, DWORD handle, DWORD datasize, LPVOID data )
{
    return GetFileVersionInfoExW(FILE_VER_GET_LOCALISED, filename, handle, datasize, data);
}

/***********************************************************************
 *           GetFileVersionInfoA             (kernelbase.@)
 */
BOOL WINAPI GetFileVersionInfoA( LPCSTR filename, DWORD handle, DWORD datasize, LPVOID data )
{
    return GetFileVersionInfoExA(FILE_VER_GET_LOCALISED, filename, handle, datasize, data);
}

/***********************************************************************
 *           VersionInfo16_FindChild             [internal]
 */
static const VS_VERSION_INFO_STRUCT16 *VersionInfo16_FindChild( const VS_VERSION_INFO_STRUCT16 *info,
                                                                LPCSTR key, UINT len )
{
    const VS_VERSION_INFO_STRUCT16 *child = VersionInfo16_Children( info );

    while ((char *)child < (char *)info + info->wLength )
    {
        if (!strnicmp( child->szKey, key, len ) && !child->szKey[len])
            return child;

        if (!(child->wLength)) return NULL;
        child = VersionInfo16_Next( child );
    }

    return NULL;
}

/***********************************************************************
 *           VersionInfo32_FindChild             [internal]
 */
static const VS_VERSION_INFO_STRUCT32 *VersionInfo32_FindChild( const VS_VERSION_INFO_STRUCT32 *info,
                                                                LPCWSTR key, UINT len )
{
    const VS_VERSION_INFO_STRUCT32 *child = VersionInfo32_Children( info );

    while ((char *)child < (char *)info + info->wLength )
    {
        if (!wcsnicmp( child->szKey, key, len ) && !child->szKey[len])
            return child;

        if (!(child->wLength)) return NULL;
        child = VersionInfo32_Next( child );
    }

    return NULL;
}

/***********************************************************************
 *           VersionInfo16_QueryValue              [internal]
 *
 *    Gets a value from a 16-bit NE resource
 */
static BOOL VersionInfo16_QueryValue( const VS_VERSION_INFO_STRUCT16 *info, LPCSTR lpSubBlock,
                               LPVOID *lplpBuffer, UINT *puLen )
{
    while ( *lpSubBlock )
    {
        /* Find next path component */
        LPCSTR lpNextSlash;
        for ( lpNextSlash = lpSubBlock; *lpNextSlash; lpNextSlash++ )
            if ( *lpNextSlash == '\\' )
                break;

        /* Skip empty components */
        if ( lpNextSlash == lpSubBlock )
        {
            lpSubBlock++;
            continue;
        }

        /* We have a non-empty component: search info for key */
        info = VersionInfo16_FindChild( info, lpSubBlock, lpNextSlash-lpSubBlock );
        if ( !info )
        {
            if (puLen) *puLen = 0 ;
            SetLastError( ERROR_RESOURCE_TYPE_NOT_FOUND );
            return FALSE;
        }

        /* Skip path component */
        lpSubBlock = lpNextSlash;
    }

    /* Return value */
    *lplpBuffer = VersionInfo16_Value( info );
    if (puLen)
        *puLen = info->wValueLength;

    return TRUE;
}

/***********************************************************************
 *           VersionInfo32_QueryValue              [internal]
 *
 *    Gets a value from a 32-bit PE resource
 */
static BOOL VersionInfo32_QueryValue( const VS_VERSION_INFO_STRUCT32 *info, LPCWSTR lpSubBlock,
                                      LPVOID *lplpBuffer, UINT *puLen, BOOL *pbText )
{
    PVOID ptr;
    TRACE("lpSubBlock : (%s)\n", debugstr_w(lpSubBlock));

    while ( *lpSubBlock )
    {
        /* Find next path component */
        LPCWSTR lpNextSlash;
        for ( lpNextSlash = lpSubBlock; *lpNextSlash; lpNextSlash++ )
            if ( *lpNextSlash == '\\' )
                break;

        /* Skip empty components */
        if ( lpNextSlash == lpSubBlock )
        {
            lpSubBlock++;
            continue;
        }

        /* We have a non-empty component: search info for key */
        info = VersionInfo32_FindChild( info, lpSubBlock, lpNextSlash-lpSubBlock );
        if ( !info )
        {
            if (puLen) *puLen = 0 ;
            SetLastError( ERROR_RESOURCE_TYPE_NOT_FOUND );
            return FALSE;
        }

        /* Skip path component */
        lpSubBlock = lpNextSlash;
    }

    /* Return value */
    ptr = VersionInfo32_Value(info);
    if ((PBYTE)ptr >= ((PBYTE)info + info->wLength))  /* empty value */
        ptr = (WCHAR*)info->szKey + wcslen(info->szKey);

    *lplpBuffer = ptr;
    if (puLen)
        *puLen = info->wValueLength;
    if (pbText)
        *pbText = info->wType;

    return TRUE;
}

/***********************************************************************
 *           VerQueryValueA              (kernelbase.@)
 */
BOOL WINAPI VerQueryValueA( LPCVOID pBlock, LPCSTR lpSubBlock,
                               LPVOID *lplpBuffer, PUINT puLen )
{
    static const char rootA[] = "\\";
    const VS_VERSION_INFO_STRUCT16 *info = pBlock;

    TRACE("(%p,%s,%p,%p)\n",
                pBlock, debugstr_a(lpSubBlock), lplpBuffer, puLen );

     if (!pBlock)
        return FALSE;

    if (lpSubBlock == NULL || lpSubBlock[0] == '\0')
        lpSubBlock = rootA;

    if ( !VersionInfoIs16( info ) )
    {
        BOOL ret, isText;
        INT len;
        LPWSTR lpSubBlockW;
        UINT value_len;

        len  = MultiByteToWideChar(CP_ACP, 0, lpSubBlock, -1, NULL, 0);
        lpSubBlockW = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));

        if (!lpSubBlockW)
            return FALSE;

        MultiByteToWideChar(CP_ACP, 0, lpSubBlock, -1, lpSubBlockW, len);

        ret = VersionInfo32_QueryValue(pBlock, lpSubBlockW, lplpBuffer, &value_len, &isText);
        if (puLen) *puLen = value_len;

        HeapFree(GetProcessHeap(), 0, lpSubBlockW);

        if (ret && isText)
        {
            /* Set lpBuffer so it points to the 'empty' area where we store
             * the converted strings
             */
            LPSTR lpBufferA = (LPSTR)pBlock + info->wLength + 4;
            DWORD pos = (LPCSTR)*lplpBuffer - (LPCSTR)pBlock;
            len = WideCharToMultiByte(CP_ACP, 0, *lplpBuffer, value_len,
                                      lpBufferA + pos, info->wLength - pos, NULL, NULL);
            *lplpBuffer = lpBufferA + pos;
            if (puLen) *puLen = len;
        }
        return ret;
    }

    return VersionInfo16_QueryValue(info, lpSubBlock, lplpBuffer, puLen);
}

/***********************************************************************
 *           VerQueryValueW              (kernelbase.@)
 */
BOOL WINAPI VerQueryValueW( LPCVOID pBlock, LPCWSTR lpSubBlock,
                               LPVOID *lplpBuffer, PUINT puLen )
{
    const VS_VERSION_INFO_STRUCT32 *info = pBlock;

    TRACE("(%p,%s,%p,%p)\n",
                pBlock, debugstr_w(lpSubBlock), lplpBuffer, puLen );

    if (!pBlock)
        return FALSE;

    if (!lpSubBlock || !lpSubBlock[0])
        lpSubBlock = L"\\";

    if ( VersionInfoIs16( info ) )
    {
        BOOL ret;
        int len;
        LPSTR lpSubBlockA;

        len = WideCharToMultiByte(CP_ACP, 0, lpSubBlock, -1, NULL, 0, NULL, NULL);
        lpSubBlockA = HeapAlloc(GetProcessHeap(), 0, len * sizeof(char));

        if (!lpSubBlockA)
            return FALSE;

        WideCharToMultiByte(CP_ACP, 0, lpSubBlock, -1, lpSubBlockA, len, NULL, NULL);

        ret = VersionInfo16_QueryValue(pBlock, lpSubBlockA, lplpBuffer, puLen);

        HeapFree(GetProcessHeap(), 0, lpSubBlockA);

        if (ret && wcscmp( lpSubBlock, L"\\" ) && wcsicmp( lpSubBlock, L"\\VarFileInfo\\Translation" ))
        {
            /* Set lpBuffer so it points to the 'empty' area where we store
             * the converted strings
             */
            LPWSTR lpBufferW = (LPWSTR)((LPSTR)pBlock + info->wLength);
            DWORD pos = (LPCSTR)*lplpBuffer - (LPCSTR)pBlock;
            DWORD max = (info->wLength - sizeof(VS_FIXEDFILEINFO)) * 4 - info->wLength;

            len = MultiByteToWideChar(CP_ACP, 0, *lplpBuffer, -1,
                                      lpBufferW + pos, max/sizeof(WCHAR) - pos );
            *lplpBuffer = lpBufferW + pos;
            if (puLen) *puLen = len;
        }
        return ret;
    }

    return VersionInfo32_QueryValue(info, lpSubBlock, lplpBuffer, puLen, NULL);
}


/******************************************************************************
 *   file_existsA
 */
static BOOL file_existsA( char const * path, char const * file, BOOL excl )
{
    DWORD sharing = excl ? 0 : FILE_SHARE_READ | FILE_SHARE_WRITE;
    char filename[MAX_PATH];
    int len;
    HANDLE handle;

    if (path)
    {
        strcpy( filename, path );
        len = strlen(filename);
        if (len && filename[len - 1] != '\\') strcat( filename, "\\" );
        strcat( filename, file );
    }
    else if (!SearchPathA( NULL, file, NULL, MAX_PATH, filename, NULL )) return FALSE;

    handle = CreateFileA( filename, 0, sharing, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
    if (handle == INVALID_HANDLE_VALUE) return FALSE;
    CloseHandle( handle );
    return TRUE;
}

/******************************************************************************
 *   file_existsW
 */
static BOOL file_existsW( const WCHAR *path, const WCHAR *file, BOOL excl )
{
    DWORD sharing = excl ? 0 : FILE_SHARE_READ | FILE_SHARE_WRITE;
    WCHAR filename[MAX_PATH];
    int len;
    HANDLE handle;

    if (path)
    {
        lstrcpyW( filename, path );
        len = lstrlenW(filename);
        if (len && filename[len - 1] != '\\') lstrcatW( filename, L"\\" );
        lstrcatW( filename, file );
    }
    else if (!SearchPathW( NULL, file, NULL, MAX_PATH, filename, NULL )) return FALSE;

    handle = CreateFileW( filename, 0, sharing, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0 );
    if (handle == INVALID_HANDLE_VALUE) return FALSE;
    CloseHandle( handle );
    return TRUE;
}

/*****************************************************************************
 *   VerFindFileA (kernelbase.@)
 *
 *   Determines where to install a file based on whether it locates another
 *   version of the file in the system.  The values VerFindFile returns are
 *   used in a subsequent call to the VerInstallFile function.
 */
DWORD WINAPI VerFindFileA( DWORD flags, LPCSTR filename, LPCSTR win_dir, LPCSTR app_dir,
                           LPSTR cur_dir, PUINT curdir_len, LPSTR dest, PUINT dest_len )
{
    DWORD  retval = 0;
    const char *curDir;
    const char *destDir;
    char winDir[MAX_PATH], systemDir[MAX_PATH];

    TRACE("flags = %lx filename=%s windir=%s appdir=%s curdirlen=%p(%u) destdirlen=%p(%u)\n",
          flags, debugstr_a(filename), debugstr_a(win_dir), debugstr_a(app_dir),
          curdir_len, curdir_len ? *curdir_len : 0, dest_len, dest_len ? *dest_len : 0 );

    /* Figure out where the file should go; shared files default to the
       system directory */

    GetSystemDirectoryA(systemDir, sizeof(systemDir));
    curDir = "";

    if(flags & VFFF_ISSHAREDFILE)
    {
        destDir = systemDir;
        /* Were we given a filename?  If so, try to find the file. */
        if(filename)
        {
            if(file_existsA(destDir, filename, FALSE)) curDir = destDir;
            else if(app_dir && file_existsA(app_dir, filename, FALSE))
                curDir = app_dir;

            if(!file_existsA(systemDir, filename, FALSE))
                retval |= VFF_CURNEDEST;
        }
    }
    else /* not a shared file */
    {
        destDir = app_dir ? app_dir : "";
        if(filename)
        {
            GetWindowsDirectoryA( winDir, MAX_PATH );
            if(file_existsA(destDir, filename, FALSE)) curDir = destDir;
            else if(file_existsA(winDir, filename, FALSE))
                curDir = winDir;
            else if(file_existsA(systemDir, filename, FALSE))
                curDir = systemDir;

            if (app_dir && app_dir[0])
            {
                if(!file_existsA(app_dir, filename, FALSE))
                    retval |= VFF_CURNEDEST;
            }
            else if(file_existsA(NULL, filename, FALSE))
                retval |= VFF_CURNEDEST;
        }
    }

    /* Check to see if the file exists and is in use by another application */
    if (filename && file_existsA(curDir, filename, FALSE))
    {
        if (filename && !file_existsA(curDir, filename, TRUE))
           retval |= VFF_FILEINUSE;
    }

    if (dest_len && dest)
    {
        UINT len = strlen(destDir) + 1;
        if (*dest_len < len) retval |= VFF_BUFFTOOSMALL;
        lstrcpynA(dest, destDir, *dest_len);
        *dest_len = len;
    }
    if (curdir_len && cur_dir)
    {
        UINT len = strlen(curDir) + 1;
        if (*curdir_len < len) retval |= VFF_BUFFTOOSMALL;
        lstrcpynA(cur_dir, curDir, *curdir_len);
        *curdir_len = len;
    }

    TRACE("ret = %lu (%s%s%s) curdir=%s destdir=%s\n", retval,
          (retval & VFF_CURNEDEST) ? "VFF_CURNEDEST " : "",
          (retval & VFF_FILEINUSE) ? "VFF_FILEINUSE " : "",
          (retval & VFF_BUFFTOOSMALL) ? "VFF_BUFFTOOSMALL " : "",
          debugstr_a(cur_dir), debugstr_a(dest));

    return retval;
}

/*****************************************************************************
 * VerFindFileW (kernelbase.@)
 */
DWORD WINAPI VerFindFileW( DWORD flags, LPCWSTR filename, LPCWSTR win_dir, LPCWSTR app_dir,
                           LPWSTR cur_dir, PUINT curdir_len, LPWSTR dest, PUINT dest_len )
{
    DWORD retval = 0;
    const WCHAR *curDir;
    const WCHAR *destDir;

    TRACE("flags = %lx filename=%s windir=%s appdir=%s curdirlen=%p(%u) destdirlen=%p(%u)\n",
          flags, debugstr_w(filename), debugstr_w(win_dir), debugstr_w(app_dir),
          curdir_len, curdir_len ? *curdir_len : 0, dest_len, dest_len ? *dest_len : 0 );

    /* Figure out where the file should go; shared files default to the
       system directory */

    curDir = L"";

    if(flags & VFFF_ISSHAREDFILE)
    {
        destDir = system_dir;
        /* Were we given a filename?  If so, try to find the file. */
        if(filename)
        {
            if(file_existsW(destDir, filename, FALSE)) curDir = destDir;
            else if(app_dir && file_existsW(app_dir, filename, FALSE))
            {
                curDir = app_dir;
                retval |= VFF_CURNEDEST;
            }
        }
    }
    else /* not a shared file */
    {
        destDir = app_dir ? app_dir : L"";
        if(filename)
        {
            if(file_existsW(destDir, filename, FALSE)) curDir = destDir;
            else if(file_existsW(windows_dir, filename, FALSE))
            {
                curDir = windows_dir;
                retval |= VFF_CURNEDEST;
            }
            else if (file_existsW(system_dir, filename, FALSE))
            {
                curDir = system_dir;
                retval |= VFF_CURNEDEST;
            }
        }
    }

    if (filename && !file_existsW(curDir, filename, TRUE))
        retval |= VFF_FILEINUSE;

    if (dest_len && dest)
    {
        UINT len = lstrlenW(destDir) + 1;
        if (*dest_len < len) retval |= VFF_BUFFTOOSMALL;
        lstrcpynW(dest, destDir, *dest_len);
        *dest_len = len;
    }
    if (curdir_len && cur_dir)
    {
        UINT len = lstrlenW(curDir) + 1;
        if (*curdir_len < len) retval |= VFF_BUFFTOOSMALL;
        lstrcpynW(cur_dir, curDir, *curdir_len);
        *curdir_len = len;
    }

    TRACE("ret = %lu (%s%s%s) curdir=%s destdir=%s\n", retval,
          (retval & VFF_CURNEDEST) ? "VFF_CURNEDEST " : "",
          (retval & VFF_FILEINUSE) ? "VFF_FILEINUSE " : "",
          (retval & VFF_BUFFTOOSMALL) ? "VFF_BUFFTOOSMALL " : "",
          debugstr_w(cur_dir), debugstr_w(dest));
    return retval;
}


/***********************************************************************
 *         GetProductInfo   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH GetProductInfo( DWORD os_major, DWORD os_minor,
                                              DWORD sp_major, DWORD sp_minor, DWORD *type )
{
    return RtlGetProductInfo( os_major, os_minor, sp_major, sp_minor, type );
}


/***********************************************************************
 *         GetVersion   (kernelbase.@)
 */
DWORD WINAPI GetVersion(void)
{
    OSVERSIONINFOEXW info;
    DWORD result;

    info.dwOSVersionInfoSize = sizeof(info);
    if (!GetVersionExW( (OSVERSIONINFOW *)&info )) return 0;

    result = MAKELONG( MAKEWORD( info.dwMajorVersion, info.dwMinorVersion ),
                       (info.dwPlatformId ^ 2) << 14 );

    if (info.dwPlatformId == VER_PLATFORM_WIN32_NT)
        result |= LOWORD(info.dwBuildNumber) << 16;
    return result;
}


/***********************************************************************
 *         GetVersionExA   (kernelbase.@)
 */
BOOL WINAPI GetVersionExA( OSVERSIONINFOA *info )
{
    OSVERSIONINFOEXW infoW;

    if (info->dwOSVersionInfoSize != sizeof(OSVERSIONINFOA) &&
        info->dwOSVersionInfoSize != sizeof(OSVERSIONINFOEXA))
    {
        WARN( "wrong OSVERSIONINFO size from app (got: %ld)\n", info->dwOSVersionInfoSize );
        SetLastError( ERROR_INSUFFICIENT_BUFFER );
        return FALSE;
    }

    infoW.dwOSVersionInfoSize = sizeof(infoW);
    if (!GetVersionExW( (OSVERSIONINFOW *)&infoW )) return FALSE;

    info->dwMajorVersion = infoW.dwMajorVersion;
    info->dwMinorVersion = infoW.dwMinorVersion;
    info->dwBuildNumber  = infoW.dwBuildNumber;
    info->dwPlatformId   = infoW.dwPlatformId;
    WideCharToMultiByte( CP_ACP, 0, infoW.szCSDVersion, -1,
                         info->szCSDVersion, sizeof(info->szCSDVersion), NULL, NULL );

    if (info->dwOSVersionInfoSize == sizeof(OSVERSIONINFOEXA))
    {
        OSVERSIONINFOEXA *vex = (OSVERSIONINFOEXA *)info;
        vex->wServicePackMajor = infoW.wServicePackMajor;
        vex->wServicePackMinor = infoW.wServicePackMinor;
        vex->wSuiteMask        = infoW.wSuiteMask;
        vex->wProductType      = infoW.wProductType;
    }
    return TRUE;
}


/***********************************************************************
 *         GetVersionExW   (kernelbase.@)
 */
BOOL WINAPI GetVersionExW( OSVERSIONINFOW *info )
{
    static INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;

    if (info->dwOSVersionInfoSize != sizeof(OSVERSIONINFOW) &&
        info->dwOSVersionInfoSize != sizeof(OSVERSIONINFOEXW))
    {
        WARN( "wrong OSVERSIONINFO size from app (got: %ld)\n", info->dwOSVersionInfoSize );
        return FALSE;
    }

    if (!InitOnceExecuteOnce(&init_once, init_current_version, NULL, NULL)) return FALSE;

    info->dwMajorVersion = current_version.dwMajorVersion;
    info->dwMinorVersion = current_version.dwMinorVersion;
    info->dwBuildNumber  = current_version.dwBuildNumber;
    info->dwPlatformId   = current_version.dwPlatformId;
    wcscpy( info->szCSDVersion, current_version.szCSDVersion );

    if (info->dwOSVersionInfoSize == sizeof(OSVERSIONINFOEXW))
    {
        OSVERSIONINFOEXW *vex = (OSVERSIONINFOEXW *)info;
        vex->wServicePackMajor = current_version.wServicePackMajor;
        vex->wServicePackMinor = current_version.wServicePackMinor;
        vex->wSuiteMask        = current_version.wSuiteMask;
        vex->wProductType      = current_version.wProductType;
    }
    return TRUE;
}

/***********************************************************************
 *         GetCurrentApplicationUserModelId   (kernelbase.@)
 */
LONG WINAPI /* DECLSPEC_HOTPATCH */ GetCurrentApplicationUserModelId( UINT32 *length, WCHAR *id )
{
    FIXME( "(%p %p): stub\n", length, id );
    return APPMODEL_ERROR_NO_APPLICATION;
}

/***********************************************************************
 *         GetCurrentPackageFamilyName   (kernelbase.@)
 */
LONG WINAPI /* DECLSPEC_HOTPATCH */ GetCurrentPackageFamilyName( UINT32 *length, WCHAR *name )
{
    FIXME( "(%p %p): stub\n", length, name );
    return APPMODEL_ERROR_NO_PACKAGE;
}


/***********************************************************************
 *         GetCurrentPackageFullName   (kernelbase.@)
 */
LONG WINAPI /* DECLSPEC_HOTPATCH */ GetCurrentPackageFullName( UINT32 *length, WCHAR *name )
{
    FIXME( "(%p %p): stub\n", length, name );
    return APPMODEL_ERROR_NO_PACKAGE;
}


/***********************************************************************
 *         GetCurrentPackageId   (kernelbase.@)
 */
LONG WINAPI /* DECLSPEC_HOTPATCH */ GetCurrentPackageId( UINT32 *len, BYTE *buffer )
{
    FIXME( "(%p %p): stub\n", len, buffer );
    return APPMODEL_ERROR_NO_PACKAGE;
}

/***********************************************************************
 *         GetCurrentPackageInfo   (kernelbase.@)
 */
LONG WINAPI GetCurrentPackageInfo( const UINT32 flags, UINT32 *buffer_size, BYTE *buffer, UINT32 *count )
{
    FIXME( "(%#x %p %p %p): stub\n", flags, buffer_size, buffer, count );
    return APPMODEL_ERROR_NO_PACKAGE;
}

/***********************************************************************
 *         GetCurrentPackagePath   (kernelbase.@)
 */
LONG WINAPI /* DECLSPEC_HOTPATCH */ GetCurrentPackagePath( UINT32 *length, WCHAR *path )
{
    FIXME( "(%p %p): stub\n", length, path );
    return APPMODEL_ERROR_NO_PACKAGE;
}


/***********************************************************************
 *         GetPackageFullName   (kernelbase.@)
 */
LONG WINAPI /* DECLSPEC_HOTPATCH */ GetPackageFullName( HANDLE process, UINT32 *length, WCHAR *name )
{
    FIXME( "(%p %p %p): stub\n", process, length, name );
    return APPMODEL_ERROR_NO_PACKAGE;
}


/***********************************************************************
 *         GetPackageFamilyName   (kernelbase.@)
 */
LONG WINAPI /* DECLSPEC_HOTPATCH */ GetPackageFamilyName( HANDLE process, UINT32 *length, WCHAR *name )
{
    FIXME( "(%p %p %p): stub\n", process, length, name );
    return APPMODEL_ERROR_NO_PACKAGE;
}

/***********************************************************************
 *         GetPackagesByPackageFamily   (kernelbase.@)
 */
LONG WINAPI DECLSPEC_HOTPATCH GetPackagesByPackageFamily(const WCHAR *family_name, UINT32 *count,
                                                         WCHAR *full_names, UINT32 *buffer_len, WCHAR *buffer)
{
    FIXME( "(%s %p %p %p %p): stub\n", debugstr_w(family_name), count, full_names, buffer_len, buffer );

    if (!count || !buffer_len)
        return ERROR_INVALID_PARAMETER;

    *count = 0;
    *buffer_len = 0;
    return ERROR_SUCCESS;
}

/***********************************************************************
 *         GetPackagePathByFullName   (kernelbase.@)
 */
LONG WINAPI GetPackagePathByFullName(const WCHAR *name, UINT32 *len, WCHAR *path)
{
    if (!len || !name)
        return ERROR_INVALID_PARAMETER;

    FIXME( "(%s %p %p): stub\n", debugstr_w(name), len, path );

    return APPMODEL_ERROR_NO_PACKAGE;
}

static const struct
{
    UINT32 code;
    const WCHAR *name;
}
arch_names[] =
{
    {PROCESSOR_ARCHITECTURE_INTEL,         L"x86"},
    {PROCESSOR_ARCHITECTURE_ARM,           L"arm"},
    {PROCESSOR_ARCHITECTURE_AMD64,         L"x64"},
    {PROCESSOR_ARCHITECTURE_NEUTRAL,       L"neutral"},
    {PROCESSOR_ARCHITECTURE_ARM64,         L"arm64"},
    {PROCESSOR_ARCHITECTURE_UNKNOWN,       L"unknown"},
};

static UINT32 processor_arch_from_string(const WCHAR *str, unsigned int len)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(arch_names); ++i)
        if (lstrlenW(arch_names[i].name) == len && !wcsnicmp(str, arch_names[i].name, len))
            return arch_names[i].code;
    return ~0u;
}

static const WCHAR *processor_arch_from_code(UINT32 code)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(arch_names); ++i)
        if (arch_names[i].code == code)
            return arch_names[i].name;

    return NULL;
}

/***********************************************************************
 *         PackageIdFromFullName   (kernelbase.@)
 */
LONG WINAPI PackageIdFromFullName(const WCHAR *full_name, UINT32 flags, UINT32 *buffer_length, BYTE *buffer)
{
    const WCHAR *name, *version_str, *arch_str, *resource_id, *publisher_id, *s;
    PACKAGE_ID *id = (PACKAGE_ID *)buffer;
    UINT32 size, buffer_size, len;

    TRACE("full_name %s, flags %#x, buffer_length %p, buffer %p.\n",
            debugstr_w(full_name), flags, buffer_length, buffer);

    if (flags)
        FIXME("Flags %#x are not supported.\n", flags);

    if (!full_name || !buffer_length)
        return ERROR_INVALID_PARAMETER;

    if (!buffer && *buffer_length)
        return ERROR_INVALID_PARAMETER;

    name = full_name;
    if (!(version_str = wcschr(name, L'_')))
        return ERROR_INVALID_PARAMETER;
    ++version_str;

    if (!(arch_str = wcschr(version_str, L'_')))
        return ERROR_INVALID_PARAMETER;
    ++arch_str;

    if (!(resource_id = wcschr(arch_str, L'_')))
        return ERROR_INVALID_PARAMETER;
    ++resource_id;

    if (!(publisher_id = wcschr(resource_id, L'_')))
        return ERROR_INVALID_PARAMETER;
    ++publisher_id;

    /* Publisher id length should be 13. */
    size = sizeof(*id) + sizeof(WCHAR) * ((version_str - name) + (publisher_id - resource_id) + 13 + 1);
    buffer_size = *buffer_length;
    *buffer_length = size;
    if (buffer_size < size)
        return ERROR_INSUFFICIENT_BUFFER;

    memset(id, 0, sizeof(*id));
    if ((id->processorArchitecture = processor_arch_from_string(arch_str, resource_id - arch_str - 1)) == ~0u)
    {
        FIXME("Unrecognized arch %s.\n", debugstr_w(arch_str));
        return ERROR_INVALID_PARAMETER;
    }
    buffer += sizeof(*id);

    id->version.Major = wcstol(version_str, NULL, 10);
    if (!(s = wcschr(version_str, L'.')))
        return ERROR_INVALID_PARAMETER;
    ++s;
    id->version.Minor = wcstol(s, NULL, 10);
    if (!(s = wcschr(s, L'.')))
        return ERROR_INVALID_PARAMETER;
    ++s;
    id->version.Build = wcstol(s, NULL, 10);
    if (!(s = wcschr(s, L'.')))
        return ERROR_INVALID_PARAMETER;
    ++s;
    id->version.Revision = wcstol(s, NULL, 10);

    id->name = (WCHAR *)buffer;
    len = version_str - name - 1;
    memcpy(id->name, name, sizeof(*id->name) * len);
    id->name[len] = 0;
    buffer += sizeof(*id->name) * (len + 1);

    id->resourceId = (WCHAR *)buffer;
    len = publisher_id - resource_id - 1;
    memcpy(id->resourceId, resource_id, sizeof(*id->resourceId) * len);
    id->resourceId[len] = 0;
    buffer += sizeof(*id->resourceId) * (len + 1);

    id->publisherId = (WCHAR *)buffer;
    len = lstrlenW(publisher_id);
    if (len != 13)
        return ERROR_INVALID_PARAMETER;
    memcpy(id->publisherId, publisher_id, sizeof(*id->publisherId) * len);
    id->publisherId[len] = 0;

    return ERROR_SUCCESS;
}

/***********************************************************************
 *         PackageFullNameFromId   (kernelbase.@)
 */
LONG WINAPI PackageFullNameFromId(const PACKAGE_ID *id, UINT32 *length, WCHAR *buffer)
{
    WCHAR full_name[PACKAGE_FULL_NAME_MAX_LENGTH + 1];
    WCHAR version[PACKAGE_VERSION_MAX_LENGTH + 1];
    const WCHAR *arch;
    size_t len;

    TRACE("id %p, length %p, buffer %p\n", id, length, buffer);

    if (!id || !length)
        return ERROR_INVALID_PARAMETER;

    len = id->name ? wcslen(id->name) : 0;
    if (len < PACKAGE_NAME_MIN_LENGTH || len > PACKAGE_NAME_MAX_LENGTH)
        return ERROR_INVALID_PARAMETER;

    *full_name = 0;
    wcscpy(full_name, id->name);
    wcscat(full_name, L"_");

    swprintf(version, ARRAYSIZE(version), L"%u.%u.%u.%u", id->version.Major, id->version.Minor,
            id->version.Build, id->version.Revision);
    wcscat(full_name, version);
    wcscat(full_name, L"_");

    arch = processor_arch_from_code(id->processorArchitecture);
    if (!arch)
    {
        WARN("Unrecognized architecture id %u.\n", id->processorArchitecture);
        return ERROR_INVALID_PARAMETER;
    }

    wcscat(full_name, arch);
    wcscat(full_name, L"_");

    if (id->resourceId)
    {
        len = wcslen(id->resourceId);

        if (len > PACKAGE_RESOURCEID_MAX_LENGTH)
            return ERROR_INVALID_PARAMETER;

        wcscat(full_name, id->resourceId);
        wcscat(full_name, L"_");
    }

    if (id->publisherId)
    {
        len = wcslen(id->publisherId);

        if (len != PACKAGE_PUBLISHERID_MAX_LENGTH)
            return ERROR_INVALID_PARAMETER;

        wcscat(full_name, id->publisherId);
    }
    else
    {
        if (!id->publisher)
            return ERROR_INVALID_PARAMETER;

        FIXME("Publisher ID generation is not implemented.\n");

        wcscat(full_name, L"123456789abcd");
    }

    len = wcslen(full_name);
    *length = len + 1;

    if (!buffer || *length <= len)
        return ERROR_INSUFFICIENT_BUFFER;

    wcscpy(buffer, full_name);
    *length = len + 1;

    return ERROR_SUCCESS;
}

#define WINE_APPX_PACKAGES_KEY L"Software\\Wine\\FakeAppxPackages"

/* shared by FindPackagesByPackageFamily: collects the full names of every
 * package in our tiny registry-based database whose FamilyName matches.
 * Returns the number of matches found (capped to max_matches). */
static UINT32 find_packages_by_family( const WCHAR *family_name, WCHAR (*matches)[900], UINT32 max_matches )
{
    HKEY root;
    UINT32 match_count = 0;
    DWORD index;

    if (RegOpenKeyExW( HKEY_LOCAL_MACHINE, WINE_APPX_PACKAGES_KEY, 0, KEY_READ, &root ))
        return 0;

    for (index = 0; match_count < max_matches; index++)
    {
        WCHAR subkey[256];
        DWORD subkey_len = ARRAY_SIZE(subkey);
        HKEY pkg_key;
        WCHAR family[300];
        DWORD size;

        if (RegEnumKeyExW( root, index, subkey, &subkey_len, NULL, NULL, NULL, NULL )) break;
        if (RegOpenKeyExW( root, subkey, 0, KEY_READ, &pkg_key )) continue;

        size = sizeof(family);
        if (!RegQueryValueExW( pkg_key, L"FamilyName", NULL, NULL, (BYTE *)family, &size ) &&
            !wcsicmp( family, family_name ))
        {
            wcsncpy( matches[match_count], subkey, 899 );
            matches[match_count][899] = 0;
            match_count++;
        }
        RegCloseKey( pkg_key );
    }
    RegCloseKey( root );
    return match_count;
}

/***********************************************************************
 *         FindPackagesByPackageFamily   (kernelbase.@)
 */
LONG WINAPI DECLSPEC_HOTPATCH FindPackagesByPackageFamily( const WCHAR *package_family_name, UINT32 package_filters,
    UINT32 *count, WCHAR **package_full_names, UINT32 *buffer_length, WCHAR *buffer, UINT32 *package_properties )
{
    WCHAR matches[16][900];
    UINT32 match_count, needed_chars = 0, i;

    TRACE( "(%s %#x %p %p %p %p %p)\n", debugstr_w(package_family_name), package_filters,
           count, package_full_names, buffer_length, buffer, package_properties );

    if (!count || !buffer_length) return ERROR_INVALID_PARAMETER;

    match_count = package_family_name ? find_packages_by_family( package_family_name, matches, ARRAY_SIZE(matches) ) : 0;
    for (i = 0; i < match_count; i++) needed_chars += wcslen( matches[i] ) + 1;

    if (*count < match_count || *buffer_length < needed_chars || (match_count && (!package_full_names || !buffer)))
    {
        *count = match_count;
        *buffer_length = needed_chars;
        return match_count ? ERROR_INSUFFICIENT_BUFFER : ERROR_SUCCESS;
    }

    {
        WCHAR *p = buffer;
        for (i = 0; i < match_count; i++)
        {
            UINT32 len = wcslen( matches[i] ) + 1;
            memcpy( p, matches[i], len * sizeof(WCHAR) );
            package_full_names[i] = p;
            if (package_properties) package_properties[i] = 0;
            p += len;
        }
    }
    *count = match_count;
    *buffer_length = needed_chars;
    return ERROR_SUCCESS;
}

/* Minimal real "package info" support, backed by a small registry-based
 * package database at HKLM\Software\Wine\FakeAppxPackages\<full name>,
 * populated by MSIX-installing applications (e.g. an AppX PackageManager
 * implementation) that record what they deployed. Without such an entry,
 * these functions behave exactly like the stubs above (no package found). */

struct package_info_ref
{
    WCHAR path[MAX_PATH];
    WCHAR full_name[900];
    WCHAR family_name[300];
    WCHAR name[256];
    WCHAR publisher[512];
    WCHAR resource_id[64];
    UINT32 processor_architecture;
    PACKAGE_VERSION version;
};

static UINT32 arch_string_to_code( const WCHAR *arch )
{
    if (!wcsicmp( arch, L"x64" )) return 9;
    if (!wcsicmp( arch, L"x86" )) return 0;
    if (!wcsicmp( arch, L"arm64" )) return 12;
    if (!wcsicmp( arch, L"arm" )) return 5;
    return 11; /* neutral */
}

/***********************************************************************
 *         OpenPackageInfoByFullName   (kernelbase.@)
 */
LONG WINAPI OpenPackageInfoByFullName( const WCHAR *full_name, UINT32 reserved, PACKAGE_INFO_REFERENCE *info_reference )
{
    HKEY root, pkg_key;
    struct package_info_ref *ref;
    LONG res;

    TRACE( "(%s %#x %p)\n", debugstr_w(full_name), reserved, info_reference );

    *info_reference = NULL;

    if (RegOpenKeyExW( HKEY_LOCAL_MACHINE, WINE_APPX_PACKAGES_KEY, 0, KEY_READ, &root ))
        return APPMODEL_ERROR_NO_PACKAGE;
    res = RegOpenKeyExW( root, full_name, 0, KEY_READ, &pkg_key );
    RegCloseKey( root );
    if (res) return APPMODEL_ERROR_NO_PACKAGE;

    if (!(ref = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*ref) )))
    {
        RegCloseKey( pkg_key );
        return ERROR_OUTOFMEMORY;
    }

    {
        DWORD size;
        UINT64 version_packed = 0;
        WCHAR arch[32] = {0};

        size = sizeof(ref->path); RegQueryValueExW( pkg_key, L"Path", NULL, NULL, (BYTE *)ref->path, &size );
        size = sizeof(ref->family_name); RegQueryValueExW( pkg_key, L"FamilyName", NULL, NULL, (BYTE *)ref->family_name, &size );
        size = sizeof(ref->name); RegQueryValueExW( pkg_key, L"Name", NULL, NULL, (BYTE *)ref->name, &size );
        size = sizeof(ref->publisher); RegQueryValueExW( pkg_key, L"Publisher", NULL, NULL, (BYTE *)ref->publisher, &size );
        size = sizeof(ref->resource_id); RegQueryValueExW( pkg_key, L"ResourceId", NULL, NULL, (BYTE *)ref->resource_id, &size );
        size = sizeof(version_packed); RegQueryValueExW( pkg_key, L"Version", NULL, NULL, (BYTE *)&version_packed, &size );
        ref->version.Version = version_packed;
        size = sizeof(arch); RegQueryValueExW( pkg_key, L"Architecture", NULL, NULL, (BYTE *)arch, &size );
        ref->processor_architecture = arch_string_to_code( arch );
        wcsncpy( ref->full_name, full_name, ARRAY_SIZE(ref->full_name) - 1 );
    }
    RegCloseKey( pkg_key );

    *info_reference = (PACKAGE_INFO_REFERENCE)ref;
    return ERROR_SUCCESS;
}

/***********************************************************************
 *         ClosePackageInfo   (kernelbase.@)
 */
LONG WINAPI ClosePackageInfo( PACKAGE_INFO_REFERENCE info_reference )
{
    TRACE( "(%p)\n", info_reference );
    HeapFree( GetProcessHeap(), 0, info_reference );
    return ERROR_SUCCESS;
}

/***********************************************************************
 *         GetPackageInfo   (kernelbase.@)
 */
LONG WINAPI GetPackageInfo( PACKAGE_INFO_REFERENCE info_reference, UINT32 flags, UINT32 *buffer_length, BYTE *buffer, UINT32 *count )
{
    struct package_info_ref *ref = (struct package_info_ref *)info_reference;
    UINT32 needed;

    TRACE( "(%p %#x %p %p %p)\n", info_reference, flags, buffer_length, buffer, count );

    if (!ref || !buffer_length) return ERROR_INVALID_PARAMETER;

    needed = sizeof(PACKAGE_INFO);
    needed += (wcslen( ref->path ) + 1) * sizeof(WCHAR);
    needed += (wcslen( ref->full_name ) + 1) * sizeof(WCHAR);
    needed += (wcslen( ref->family_name ) + 1) * sizeof(WCHAR);
    needed += (wcslen( ref->name ) + 1) * sizeof(WCHAR);
    needed += (wcslen( ref->publisher ) + 1) * sizeof(WCHAR);
    needed += (wcslen( ref->resource_id ) + 1) * sizeof(WCHAR);

    if (count) *count = 1;

    if (!buffer || *buffer_length < needed)
    {
        *buffer_length = needed;
        return buffer ? ERROR_INSUFFICIENT_BUFFER : ERROR_SUCCESS;
    }
    *buffer_length = needed;

    {
        PACKAGE_INFO *info = (PACKAGE_INFO *)buffer;
        WCHAR *pool = (WCHAR *)(buffer + sizeof(PACKAGE_INFO));
        size_t off = 0;

        memset( info, 0, sizeof(*info) );
#define PACK(dst, src) do { wcscpy( pool + off, (src) ); (dst) = pool + off; off += wcslen( (src) ) + 1; } while (0)
        PACK( info->path, ref->path );
        PACK( info->packageFullName, ref->full_name );
        PACK( info->packageFamilyName, ref->family_name );
        info->packageId.processorArchitecture = ref->processor_architecture;
        info->packageId.version = ref->version;
        PACK( info->packageId.name, ref->name );
        PACK( info->packageId.publisher, ref->publisher );
        PACK( info->packageId.resourceId, ref->resource_id );
        info->packageId.publisherId = NULL;
#undef PACK
    }
    return ERROR_SUCCESS;
}

/***********************************************************************
 *         VerifyPackageFamilyName   (kernelbase.@)
 */
LONG WINAPI VerifyPackageFamilyName( const WCHAR *package_family_name )
{
    size_t len;

    TRACE( "(%s)\n", debugstr_w(package_family_name) );

    if (!package_family_name) return ERROR_INVALID_PARAMETER;
    len = wcslen( package_family_name );
    if (len < PACKAGE_FAMILY_NAME_MIN_LENGTH || len > PACKAGE_FAMILY_NAME_MAX_LENGTH)
        return ERROR_INVALID_PARAMETER;
    return ERROR_SUCCESS;
}

/***********************************************************************
 *         FormatApplicationUserModelId   (kernelbase.@)
 */
LONG WINAPI FormatApplicationUserModelId( const WCHAR *package_family_name, const WCHAR *package_relative_app_id,
                                           UINT32 *application_user_model_id_length, WCHAR *application_user_model_id )
{
    WCHAR buf[APPLICATION_USER_MODEL_ID_MAX_LENGTH];
    UINT32 needed;

    TRACE( "(%s %s %p %p)\n", debugstr_w(package_family_name), debugstr_w(package_relative_app_id),
           application_user_model_id_length, application_user_model_id );

    if (!package_family_name || !package_relative_app_id || !application_user_model_id_length)
        return ERROR_INVALID_PARAMETER;

    swprintf( buf, ARRAY_SIZE(buf), L"%s!%s", package_family_name, package_relative_app_id );
    needed = (wcslen( buf ) + 1) * sizeof(WCHAR);

    if (!application_user_model_id || *application_user_model_id_length < needed)
    {
        *application_user_model_id_length = needed;
        return application_user_model_id ? ERROR_INSUFFICIENT_BUFFER : ERROR_SUCCESS;
    }
    *application_user_model_id_length = needed;
    wcscpy( application_user_model_id, buf );
    return ERROR_SUCCESS;
}

/* ============================================================
 * Dynamic Dependencies (Windows 10 1809+ / Windows App SDK bootstrap).
 * A "package dependency id" is just the package family name here: our
 * registry-based package DB is keyed by full name, and FindPackagesByPackageFamily
 * -style lookups walk it by family name, so using the family name directly as
 * the id keeps this minimal implementation self-consistent without needing a
 * separate persisted "dependency" object.
 */

static WCHAR *heap_strdupW( const WCHAR *str )
{
    WCHAR *ret;
    if (!str) return NULL;
    if ((ret = HeapAlloc( GetProcessHeap(), 0, (wcslen(str) + 1) * sizeof(WCHAR) )))
        wcscpy( ret, str );
    return ret;
}

/* looks up a package by family name in our registry DB, returns its full name (heap-allocated) or NULL */
static WCHAR *resolve_family_to_full_name( const WCHAR *family_name )
{
    HKEY root, subkey;
    WCHAR *result = NULL;
    DWORD index;

    if (RegOpenKeyExW( HKEY_LOCAL_MACHINE, WINE_APPX_PACKAGES_KEY, 0, KEY_READ, &root ))
        return NULL;

    for (index = 0; !result; index++)
    {
        WCHAR name[900];
        DWORD name_len = ARRAY_SIZE(name);
        WCHAR family[300] = {0};
        DWORD size;

        if (RegEnumKeyExW( root, index, name, &name_len, NULL, NULL, NULL, NULL )) break;
        if (RegOpenKeyExW( root, name, 0, KEY_READ, &subkey )) continue;
        size = sizeof(family);
        RegQueryValueExW( subkey, L"FamilyName", NULL, NULL, (BYTE *)family, &size );
        RegCloseKey( subkey );
        if (!wcsicmp( family, family_name )) result = heap_strdupW( name );
    }
    RegCloseKey( root );
    return result;
}

/***********************************************************************
 *         TryCreatePackageDependency   (kernelbase.@)
 */
HRESULT WINAPI TryCreatePackageDependency( void *user, const WCHAR *package_family_name, PACKAGE_VERSION min_version,
    PackageDependencyProcessorArchitectures architectures, PackageDependencyLifetimeKind lifetime_kind,
    const WCHAR *lifetime_artifact, CreatePackageDependencyOptions options, WCHAR **package_dependency_id )
{
    WCHAR *full_name;

    TRACE( "(%p %s %#I64x %#x %d %s %#x %p)\n", user, debugstr_w(package_family_name), min_version.Version,
           architectures, lifetime_kind, debugstr_w(lifetime_artifact), options, package_dependency_id );

    if (!package_family_name || !package_dependency_id) return E_INVALIDARG;

    full_name = resolve_family_to_full_name( package_family_name );
    if (!full_name && !(options & CreatePackageDependencyOptions_DoNotVerifyDependencyResolution))
        return HRESULT_FROM_WIN32( APPMODEL_ERROR_NO_PACKAGE );
    HeapFree( GetProcessHeap(), 0, full_name );

    if (!(*package_dependency_id = heap_strdupW( package_family_name ))) return E_OUTOFMEMORY;
    return S_OK;
}

/***********************************************************************
 *         TryCreatePackageDependency2   (kernelbase.@)
 */
HRESULT WINAPI TryCreatePackageDependency2( void *user, const WCHAR *package_family_name, PACKAGE_VERSION min_version,
    PackageDependencyProcessorArchitectures architectures, PackageDependencyLifetimeKind lifetime_kind,
    const WCHAR *lifetime_artifact, CreatePackageDependencyOptions options, const FILETIME *lifetime_expiration,
    WCHAR **package_dependency_id )
{
    TRACE( "(%p %s ... %p)\n", user, debugstr_w(package_family_name), package_dependency_id );
    return TryCreatePackageDependency( user, package_family_name, min_version, architectures, lifetime_kind,
                                        lifetime_artifact, options, package_dependency_id );
}

/***********************************************************************
 *         DeletePackageDependency   (kernelbase.@)
 */
HRESULT WINAPI DeletePackageDependency( const WCHAR *package_dependency_id )
{
    TRACE( "(%s)\n", debugstr_w(package_dependency_id) );
    return S_OK;
}

/***********************************************************************
 *         AddPackageDependency   (kernelbase.@)
 */
HRESULT WINAPI AddPackageDependency( const WCHAR *package_dependency_id, INT32 rank, AddPackageDependencyOptions options,
    PACKAGEDEPENDENCY_CONTEXT *package_dependency_context, WCHAR **package_full_name )
{
    WCHAR *full_name;

    TRACE( "(%s %d %#x %p %p)\n", debugstr_w(package_dependency_id), rank, options, package_dependency_context, package_full_name );

    if (!package_dependency_id) return E_INVALIDARG;

    full_name = resolve_family_to_full_name( package_dependency_id );
    if (!full_name) return HRESULT_FROM_WIN32( APPMODEL_ERROR_NO_PACKAGE );

    if (package_dependency_context)
        *package_dependency_context = (PACKAGEDEPENDENCY_CONTEXT)heap_strdupW( package_dependency_id );
    if (package_full_name) *package_full_name = full_name;
    else HeapFree( GetProcessHeap(), 0, full_name );
    return S_OK;
}

/***********************************************************************
 *         AddPackageDependency2   (kernelbase.@)
 */
HRESULT WINAPI AddPackageDependency2( const WCHAR *package_dependency_id, INT32 rank, AddPackageDependencyOptions2 options,
    PACKAGEDEPENDENCY_CONTEXT *package_dependency_context, WCHAR **package_full_name )
{
    TRACE( "(%s %d %#x %p %p)\n", debugstr_w(package_dependency_id), rank, options, package_dependency_context, package_full_name );
    return AddPackageDependency( package_dependency_id, rank, options, package_dependency_context, package_full_name );
}

/***********************************************************************
 *         RemovePackageDependency   (kernelbase.@)
 */
HRESULT WINAPI RemovePackageDependency( PACKAGEDEPENDENCY_CONTEXT package_dependency_context )
{
    TRACE( "(%p)\n", package_dependency_context );
    HeapFree( GetProcessHeap(), 0, package_dependency_context );
    return S_OK;
}

/***********************************************************************
 *         GetResolvedPackageFullNameForPackageDependency   (kernelbase.@)
 */
HRESULT WINAPI GetResolvedPackageFullNameForPackageDependency( const WCHAR *package_dependency_id, WCHAR **package_full_name )
{
    TRACE( "(%s %p)\n", debugstr_w(package_dependency_id), package_full_name );
    if (!package_dependency_id || !package_full_name) return E_INVALIDARG;
    *package_full_name = resolve_family_to_full_name( package_dependency_id );
    return *package_full_name ? S_OK : HRESULT_FROM_WIN32( APPMODEL_ERROR_NO_PACKAGE );
}

/***********************************************************************
 *         GetResolvedPackageFullNameForPackageDependency2   (kernelbase.@)
 */
HRESULT WINAPI GetResolvedPackageFullNameForPackageDependency2( const WCHAR *package_dependency_id, WCHAR **package_full_name )
{
    return GetResolvedPackageFullNameForPackageDependency( package_dependency_id, package_full_name );
}

/***********************************************************************
 *         GetIdForPackageDependencyContext   (kernelbase.@)
 */
HRESULT WINAPI GetIdForPackageDependencyContext( PACKAGEDEPENDENCY_CONTEXT package_dependency_context, WCHAR **package_dependency_id )
{
    TRACE( "(%p %p)\n", package_dependency_context, package_dependency_id );
    if (!package_dependency_context || !package_dependency_id) return E_INVALIDARG;
    if (!(*package_dependency_id = heap_strdupW( (const WCHAR *)package_dependency_context ))) return E_OUTOFMEMORY;
    return S_OK;
}

/***********************************************************************
 *         GetPackageGraphRevisionId   (kernelbase.@)
 *
 * Real Windows bumps this whenever the current process's package graph
 * (its set of resolved dependencies) changes; callers use it purely to
 * detect staleness of cached data. Since our package graph never changes
 * within a process lifetime, a constant is a valid (if minimal) answer.
 */
UINT32 WINAPI GetPackageGraphRevisionId(void)
{
    TRACE( "()\n" );
    return 1;
}

/***********************************************************************
 *         GetCurrentPackageInfo2   (kernelbase.@)
 *         GetCurrentPackageInfo3   (kernelbase.@)
 *
 * Newer variants of GetCurrentPackageInfo (adding a PackagePathType
 * selector); undocumented signature for the "3" variant guessed from the
 * established 1->2 pattern, low-risk because on this "no current package"
 * path (ms-teams.exe always runs unpackaged here) real Windows would also
 * return APPMODEL_ERROR_NO_PACKAGE without touching any output parameter,
 * which is exactly what callers of a Win32 API must already tolerate.
 */
LONG WINAPI GetCurrentPackageInfo2( UINT32 flags, PackagePathType path_type, UINT32 *buffer_length, BYTE *buffer, UINT32 *count )
{
    FIXME( "(%#x %d %p %p %p): stub\n", flags, path_type, buffer_length, buffer, count );
    return APPMODEL_ERROR_NO_PACKAGE;
}

LONG WINAPI GetCurrentPackageInfo3( UINT32 flags, PackagePathType path_type, UINT32 *buffer_length, BYTE *buffer, UINT32 *count )
{
    FIXME( "(%#x %d %p %p %p): stub\n", flags, path_type, buffer_length, buffer, count );
    return APPMODEL_ERROR_NO_PACKAGE;
}
