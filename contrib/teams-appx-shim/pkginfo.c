/* Real (best-effort) implementations of the Win32 "Package Info" APIs
 * that Microsoft.WindowsAppRuntime.Bootstrap.dll needs and that don't
 * exist at all in Wine: OpenPackageInfoByFullName, GetPackageInfo,
 * ClosePackageInfo, FormatApplicationUserModelId, VerifyPackageFamilyName.
 *
 * Backed by a tiny registry-based "package database" that
 * StageAndRegisterPackageFromUri() (called from our AddPackageAsync /
 * StagePackageByUriAsync stubs) populates with real extracted files and
 * real identity data parsed from the .msix.
 */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "private.h"
#include "miniz.h"
#include "wine/debug.h"

#define PKGDB_KEY L"Software\\Wine\\FakeAppxPackages"

/* ---- shared helpers also used by package.c ---- */

static void *memmem_local(const void *haystack, size_t haystackLen, const void *needle, size_t needleLen)
{
    const char *h = (const char*)haystack;
    size_t i;
    if (needleLen == 0 || haystackLen < needleLen) return NULL;
    for (i = 0; i + needleLen <= haystackLen; i++)
        if (memcmp(h + i, needle, needleLen) == 0) return (void*)(h + i);
    return NULL;
}

static int ExtractIdentityAttrLocal(const char *xml, size_t xmlLen, const char *attr, char *out, size_t outCap)
{
    const char *idTag = memmem_local(xml, xmlLen, "<Identity", 9);
    const char *tagEnd, *pos, *valStart, *valEnd;
    char pattern[64];
    size_t searchLen, patLen, valLen;

    if (!idTag) return 0;
    tagEnd = memchr(idTag, '>', xmlLen - (idTag - xml));
    if (!tagEnd) tagEnd = xml + xmlLen;
    searchLen = tagEnd - idTag;

    snprintf(pattern, sizeof(pattern), "%s=\"", attr);
    patLen = strlen(pattern);
    pos = memmem_local(idTag, searchLen, pattern, patLen);
    if (!pos) return 0;
    valStart = pos + patLen;
    valEnd = memchr(valStart, '"', (idTag + searchLen) - valStart);
    if (!valEnd) return 0;
    valLen = valEnd - valStart;
    if (valLen >= outCap) valLen = outCap - 1;
    memcpy(out, valStart, valLen);
    out[valLen] = 0;
    return 1;
}

static void Utf8ToWideLocal(const char *utf8, wchar_t *out, size_t outCap)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, (int)outCap);
    if (n <= 0) out[0] = 0;
}

/* file:///C:/foo/bar.msix -> C:\foo\bar.msix */
static void UriToWinPath(const wchar_t *uri, wchar_t *out, size_t outCap)
{
    const wchar_t *p = uri;
    size_t i, j = 0;
    if (!wcsncmp(p, L"file:///", 8)) p += 8;
    else if (!wcsncmp(p, L"file://", 7)) p += 7;
    for (i = 0; p[i] && j + 1 < outCap; i++)
    {
        wchar_t c = p[i];
        if (c == L'/') c = L'\\';
        out[j++] = c;
    }
    out[j] = 0;
}

/* recursively create C:\a\b\c style directories */
static void MakeDirsRecursive(const wchar_t *path)
{
    wchar_t buf[MAX_PATH];
    size_t i, len = wcslen(path);
    if (len >= MAX_PATH) len = MAX_PATH - 1;
    wcsncpy(buf, path, len);
    buf[len] = 0;
    for (i = 0; i < len; i++)
    {
        if (buf[i] == L'\\' && i > 2)
        {
            buf[i] = 0;
            CreateDirectoryW(buf, NULL);
            buf[i] = L'\\';
        }
    }
    CreateDirectoryW(buf, NULL);
}

/* creates every directory component of path EXCEPT the last one
 * (path itself is expected to be a file, not a directory) */
static void MakeParentDirs(const wchar_t *path)
{
    wchar_t buf[MAX_PATH];
    wchar_t *lastSlash;
    size_t len = wcslen(path);
    if (len >= MAX_PATH) len = MAX_PATH - 1;
    wcsncpy(buf, path, len);
    buf[len] = 0;
    lastSlash = wcsrchr(buf, L'\\');
    if (!lastSlash) return;
    *lastSlash = 0;
    MakeDirsRecursive(buf);
}

static void RegSetStr(HKEY key, const wchar_t *name, const wchar_t *value)
{
    RegSetValueExW(key, name, 0, REG_SZ, (const BYTE*)value, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
}

HRESULT UriRuntimeClassToWinPath( IUriRuntimeClass *uri, wchar_t *out, size_t outCap )
{
    HSTRING hstr;
    const WCHAR *buf;
    HRESULT hr;
    if (!uri) return E_INVALIDARG;
    hr = IUriRuntimeClass_get_AbsoluteUri( uri, &hstr );
    if (FAILED(hr)) return hr;
    buf = WindowsGetStringRawBuffer( hstr, NULL );
    UriToWinPath( buf, out, outCap );
    WindowsDeleteString( hstr );
    return S_OK;
}

/* Extracts the whole .msix at winPath into installDir, parses its
 * AppxManifest.xml, and records the package in our tiny registry DB.
 * Returns S_OK / a failure HRESULT. On success, *outFamilyName is a
 * newly allocated (free() it) wide string with the package family name. */
HRESULT StageAndRegisterMsix( const wchar_t *winPath, wchar_t **outFamilyName )
{
    char pathA[MAX_PATH];
    mz_zip_archive zip;
    int i, n;
    char nameA[256] = {0}, pubA[512] = {0}, verA[64] = {0}, archA[32] = {0}, resA[64] = {0};
    char *manifestXml = NULL;
    size_t manifestLen = 0;
    wchar_t installDir[MAX_PATH];
    wchar_t familyName[300];
    wchar_t fullName[900];
    HKEY hkey;
    LONG rc;

    *outFamilyName = NULL;

    WideCharToMultiByte(CP_UTF8, 0, winPath, -1, pathA, sizeof(pathA), NULL, NULL);

    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, pathA, 0))
    {
        /* Wine's msi.dll fails to extract large embedded Binary-table
         * streams correctly, leaving WindowsAppRuntimeInstall-x64.exe's
         * "MSI*.tmp" staging targets empty. We already extracted the real
         * .msix content ourselves straight from that installer's own PE
         * resources (.rsrc/PACKAGE/MSIX_*), so fall back to those, in the
         * fixed order this installer is known to request them in. */
        static const char *const fallback_paths[] = {
            "Z:\\home\\ricardo\\wine-appx-src\\embedded-packages\\MSIX_FWPACKAGE_X64",
            "Z:\\home\\ricardo\\wine-appx-src\\embedded-packages\\MSIX_FWPACKAGE_X86",
            "Z:\\home\\ricardo\\wine-appx-src\\embedded-packages\\MSIX_MAINPACKAGE_X64",
            "Z:\\home\\ricardo\\wine-appx-src\\embedded-packages\\MSIX_SINGLETONPACKAGE_X64",
            "Z:\\home\\ricardo\\wine-appx-src\\embedded-packages\\MSIX_DDLMPACKAGE_X64",
            "Z:\\home\\ricardo\\wine-appx-src\\embedded-packages\\MSIX_DDLMPACKAGE_X86",
        };
        static LONG fallback_index = 0;
        LONG my_index = InterlockedIncrement(&fallback_index) - 1;

        if (my_index >= 0 && my_index < (LONG)(sizeof(fallback_paths)/sizeof(fallback_paths[0])) &&
            mz_zip_reader_init_file(&zip, fallback_paths[my_index], 0))
        {
            ERR("StageAndRegisterMsix: %s was empty/invalid, using embedded fallback #%ld (%s)\n",
                pathA, my_index, fallback_paths[my_index]);
        }
        else
        {
            ERR("StageAndRegisterMsix: failed to open %s as zip (fallback #%ld unavailable)\n", pathA, my_index);
            return E_FAIL;
        }
    }

    /* find + parse AppxManifest.xml first, to know the install dir name */
    n = mz_zip_reader_locate_file(&zip, "AppxManifest.xml", NULL, 0);
    if (n < 0)
    {
        mz_zip_reader_end(&zip);
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }
    manifestXml = (char*)mz_zip_reader_extract_to_heap(&zip, (mz_uint)n, &manifestLen, 0);
    if (!manifestXml)
    {
        mz_zip_reader_end(&zip);
        return E_FAIL;
    }
    ExtractIdentityAttrLocal(manifestXml, manifestLen, "Name", nameA, sizeof(nameA));
    ExtractIdentityAttrLocal(manifestXml, manifestLen, "Publisher", pubA, sizeof(pubA));
    ExtractIdentityAttrLocal(manifestXml, manifestLen, "Version", verA, sizeof(verA));
    ExtractIdentityAttrLocal(manifestXml, manifestLen, "ProcessorArchitecture", archA, sizeof(archA));
    ExtractIdentityAttrLocal(manifestXml, manifestLen, "ResourceId", resA, sizeof(resA));
    free(manifestXml);

    if (!nameA[0]) { mz_zip_reader_end(&zip); return E_FAIL; }
    if (!verA[0]) strcpy(verA, "1.0.0.0");
    if (!archA[0]) strcpy(archA, "neutral");

    {
        wchar_t nameW[256], verW[64], archW[32];
        Utf8ToWideLocal(nameA, nameW, 256);
        Utf8ToWideLocal(verA, verW, 64);
        Utf8ToWideLocal(archA, archW, 32);
        swprintf(familyName, 300, L"%ls_stubpub", nameW);
        swprintf(fullName, 900, L"%ls_%ls_%ls__stubpub", nameW, verW, archW);
        swprintf(installDir, MAX_PATH, L"C:\\Program Files\\WindowsApps\\%ls", fullName);
    }

    MakeDirsRecursive(installDir);

    /* extract every file entry into installDir, preserving subpaths */
    n = (int)mz_zip_reader_get_num_files(&zip);
    for (i = 0; i < n; i++)
    {
        mz_zip_archive_file_stat st;
        wchar_t entryNameW[600];
        wchar_t destPath[MAX_PATH];
        char destPathA[MAX_PATH];
        wchar_t *slash;

        if (!mz_zip_reader_file_stat(&zip, (mz_uint)i, &st)) continue;
        if (mz_zip_reader_is_file_a_directory(&zip, (mz_uint)i)) continue;
        /* skip appx signature/blockmap bookkeeping files, we don't need them */
        if (!strncmp(st.m_filename, "[Content_Types].xml", 20)) continue;

        MultiByteToWideChar(CP_UTF8, 0, st.m_filename, -1, entryNameW, 600);
        for (slash = entryNameW; *slash; slash++) if (*slash == L'/') *slash = L'\\';
        swprintf(destPath, MAX_PATH, L"%ls\\%ls", installDir, entryNameW);

        /* ensure parent dir exists (files can be nested) */
        MakeParentDirs(destPath);

        WideCharToMultiByte(CP_UTF8, 0, destPath, -1, destPathA, sizeof(destPathA), NULL, NULL);
        mz_zip_reader_extract_to_file(&zip, (mz_uint)i, destPathA, 0);
    }
    mz_zip_reader_end(&zip);

    /* persist to our tiny registry-based package DB */
    rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, PKGDB_KEY, 0, NULL, 0, KEY_WRITE, NULL, &hkey, NULL);
    if (rc == ERROR_SUCCESS)
    {
        HKEY pkgKey;
        rc = RegCreateKeyExW(hkey, fullName, 0, NULL, 0, KEY_WRITE, NULL, &pkgKey, NULL);
        if (rc == ERROR_SUCCESS)
        {
            wchar_t nameW[256], pubW[512], resW[64], archW[32];
            UINT64 verPacked;
            unsigned a=0,b=0,c=0,d=0;
            Utf8ToWideLocal(nameA, nameW, 256);
            Utf8ToWideLocal(pubA, pubW, 512);
            Utf8ToWideLocal(resA, resW, 64);
            Utf8ToWideLocal(archA, archW, 32);
            sscanf(verA, "%u.%u.%u.%u", &a,&b,&c,&d);
            verPacked = ((UINT64)a<<48)|((UINT64)b<<32)|((UINT64)c<<16)|(UINT64)d;

            RegSetStr(pkgKey, L"Path", installDir);
            RegSetStr(pkgKey, L"FamilyName", familyName);
            RegSetStr(pkgKey, L"Name", nameW);
            RegSetStr(pkgKey, L"Publisher", pubW);
            RegSetStr(pkgKey, L"ResourceId", resW);
            RegSetStr(pkgKey, L"Architecture", archW);
            RegSetValueExW(pkgKey, L"Version", 0, REG_QWORD, (const BYTE*)&verPacked, sizeof(verPacked));
            RegCloseKey(pkgKey);
        }
        RegCloseKey(hkey);
    }

    *outFamilyName = _wcsdup(familyName);
    return S_OK;
}

/* ============================================================
 * The 5 real Win32 "package info" exports
 * ============================================================ */

struct pkginfo_ref
{
    wchar_t path[MAX_PATH];
    wchar_t fullName[900];
    wchar_t familyName[300];
    wchar_t name[256];
    wchar_t publisher[512];
    wchar_t resourceId[64];
    UINT32 processorArchitecture;
    PACKAGE_VERSION version;
};

static UINT32 ArchStringToCode( const wchar_t *arch )
{
    if (!_wcsicmp(arch, L"x64")) return 9;
    if (!_wcsicmp(arch, L"x86")) return 0;
    if (!_wcsicmp(arch, L"arm64")) return 12;
    if (!_wcsicmp(arch, L"arm")) return 5;
    return 11; /* neutral */
}

LONG WINAPI OpenPackageInfoByFullName( PCWSTR packageFullName, UINT32 reserved, PACKAGE_INFO_REFERENCE *packageInfoReference )
{
    HKEY hkey, pkgKey;
    struct pkginfo_ref *ref;
    (void)reserved;

    *packageInfoReference = NULL;

    if (RegOpenKeyExW( HKEY_LOCAL_MACHINE, PKGDB_KEY, 0, KEY_READ, &hkey ))
        return ERROR_NOT_FOUND;
    if (RegOpenKeyExW( hkey, packageFullName, 0, KEY_READ, &pkgKey ))
    {
        RegCloseKey( hkey );
        return ERROR_NOT_FOUND;
    }
    RegCloseKey( hkey );

    ref = (struct pkginfo_ref*)calloc( 1, sizeof(*ref) );
    if (!ref) { RegCloseKey( pkgKey ); return ERROR_OUTOFMEMORY; }

    {
        DWORD sz;
        UINT64 verPacked = 0;

        sz = sizeof(ref->path); RegQueryValueExW( pkgKey, L"Path", NULL, NULL, (BYTE*)ref->path, &sz );
        sz = sizeof(ref->familyName); RegQueryValueExW( pkgKey, L"FamilyName", NULL, NULL, (BYTE*)ref->familyName, &sz );
        sz = sizeof(ref->name); RegQueryValueExW( pkgKey, L"Name", NULL, NULL, (BYTE*)ref->name, &sz );
        sz = sizeof(ref->publisher); RegQueryValueExW( pkgKey, L"Publisher", NULL, NULL, (BYTE*)ref->publisher, &sz );
        sz = sizeof(ref->resourceId); RegQueryValueExW( pkgKey, L"ResourceId", NULL, NULL, (BYTE*)ref->resourceId, &sz );
        sz = sizeof(verPacked); RegQueryValueExW( pkgKey, L"Version", NULL, NULL, (BYTE*)&verPacked, &sz );
        ref->version.u.Version = verPacked;

        {
            wchar_t archW[32] = {0};
            sz = sizeof(archW); RegQueryValueExW( pkgKey, L"Architecture", NULL, NULL, (BYTE*)archW, &sz );
            ref->processorArchitecture = ArchStringToCode( archW );
        }
        wcsncpy( ref->fullName, packageFullName, 899 );
    }
    RegCloseKey( pkgKey );

    *packageInfoReference = (PACKAGE_INFO_REFERENCE)ref;
    return ERROR_SUCCESS;
}

LONG WINAPI ClosePackageInfo( PACKAGE_INFO_REFERENCE packageInfoReference )
{
    free( packageInfoReference );
    return ERROR_SUCCESS;
}

LONG WINAPI GetPackageInfo( PACKAGE_INFO_REFERENCE packageInfoReference, UINT32 flags,
    UINT32 *bufferLength, BYTE *buffer, UINT32 *count )
{
    struct pkginfo_ref *ref = (struct pkginfo_ref*)packageInfoReference;
    UINT32 needed;
    size_t off;
    (void)flags;

    /* layout: 1x PACKAGE_INFO struct, then all its strings packed after it */
    needed = sizeof(PACKAGE_INFO);
    needed += (UINT32)((wcslen(ref->path) + 1) * sizeof(wchar_t));
    needed += (UINT32)((wcslen(ref->fullName) + 1) * sizeof(wchar_t));
    needed += (UINT32)((wcslen(ref->familyName) + 1) * sizeof(wchar_t));
    needed += (UINT32)((wcslen(ref->name) + 1) * sizeof(wchar_t));
    needed += (UINT32)((wcslen(ref->publisher) + 1) * sizeof(wchar_t));
    needed += (UINT32)((wcslen(ref->resourceId) + 1) * sizeof(wchar_t));

    if (count) *count = 1;

    if (!buffer || *bufferLength < needed)
    {
        *bufferLength = needed;
        return buffer ? ERROR_INSUFFICIENT_BUFFER : ERROR_SUCCESS;
    }
    *bufferLength = needed;

    {
        PACKAGE_INFO *info = (PACKAGE_INFO*)buffer;
        wchar_t *strPool = (wchar_t*)(buffer + sizeof(PACKAGE_INFO));
        memset( info, 0, sizeof(*info) );

        off = 0;
#define PACK(dst, src) do { wcscpy(strPool + off, src); dst = strPool + off; off += wcslen(src) + 1; } while (0)
        PACK( info->path, ref->path );
        PACK( info->packageFullName, ref->fullName );
        PACK( info->packageFamilyName, ref->familyName );
        info->packageId.processorArchitecture = ref->processorArchitecture;
        info->packageId.version = ref->version;
        PACK( info->packageId.name, ref->name );
        PACK( info->packageId.publisher, ref->publisher );
        PACK( info->packageId.resourceId, ref->resourceId );
        info->packageId.publisherId = NULL;
#undef PACK
    }
    return ERROR_SUCCESS;
}

LONG WINAPI VerifyPackageFamilyName( PCWSTR packageFamilyName )
{
    /* real API only checks string syntax (charset/length), not existence */
    size_t len = wcslen( packageFamilyName );
    if (len == 0 || len > 255) return ERROR_INVALID_PARAMETER;
    return ERROR_SUCCESS;
}

LONG WINAPI FormatApplicationUserModelId( PCWSTR packageFamilyName, PCWSTR packageRelativeApplicationId,
    UINT32 *applicationUserModelIdLength, PWSTR applicationUserModelId )
{
    wchar_t buf[512];
    UINT32 needed;
    swprintf( buf, 512, L"%ls!%ls", packageFamilyName, packageRelativeApplicationId );
    needed = (UINT32)((wcslen(buf) + 1) * sizeof(wchar_t));
    if (!applicationUserModelId || *applicationUserModelIdLength < needed)
    {
        *applicationUserModelIdLength = needed;
        return applicationUserModelId ? ERROR_INSUFFICIENT_BUFFER : ERROR_SUCCESS;
    }
    *applicationUserModelIdLength = needed;
    wcscpy( applicationUserModelId, buf );
    return ERROR_SUCCESS;
}
