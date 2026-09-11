/*
 * Copyright (C) 2017 Alistair Leslie-Hughes
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
 */
#ifndef _APPMODEL_H_
#define _APPMODEL_H_

/* Application User Model ID (space included for NULL terminator) */
#define APPLICATION_USER_MODEL_ID_MIN_LENGTH 21
#define APPLICATION_USER_MODEL_ID_MAX_LENGTH 130

/* Package architecture (no space for NULL terminator) */
#define PACKAGE_ARCHITECTURE_MIN_LENGTH 3
#define PACKAGE_ARCHITECTURE_MAX_LENGTH 7

/* Package family name (no space for NULL terminator) */
#define PACKAGE_FAMILY_NAME_MIN_LENGTH 17
#define PACKAGE_FAMILY_NAME_MAX_LENGTH 64

/* Package full name (no space for NULL terminator) */
#define PACKAGE_FULL_NAME_MIN_LENGTH 30
#define PACKAGE_FULL_NAME_MAX_LENGTH 127

/* Package name (no space for NULL terminator) */
#define PACKAGE_NAME_MIN_LENGTH 3
#define PACKAGE_NAME_MAX_LENGTH 50

/* Package publisher ID (no space for NULL terminator) */
#define PACKAGE_PUBLISHERID_MIN_LENGTH 13
#define PACKAGE_PUBLISHERID_MAX_LENGTH 13

/* Package publisher (no space for NULL terminator) */
#define PACKAGE_PUBLISHER_MIN_LENGTH 4
#define PACKAGE_PUBLISHER_MAX_LENGTH 8192

/* Package relative application ID (space included for NULL terminator) */
#define PACKAGE_RELATIVE_APPLICATION_ID_MIN_LENGTH 2
#define PACKAGE_RELATIVE_APPLICATION_ID_MAX_LENGTH 65

/* Package resource ID (no space for NULL terminator) */
#define PACKAGE_RESOURCEID_MIN_LENGTH 0
#define PACKAGE_RESOURCEID_MAX_LENGTH 30

/* Package version (no space for NULL terminator) */
#define PACKAGE_VERSION_MIN_LENGTH 7
#define PACKAGE_VERSION_MAX_LENGTH 23

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum AppPolicyMediaFoundationCodecLoading
{
    AppPolicyMediaFoundationCodecLoading_All       = 0,
    AppPolicyMediaFoundationCodecLoading_InboxOnly = 1,
} AppPolicyMediaFoundationCodecLoading;

typedef enum AppPolicyProcessTerminationMethod
{
    AppPolicyProcessTerminationMethod_ExitProcess      = 0,
    AppPolicyProcessTerminationMethod_TerminateProcess = 1,
} AppPolicyProcessTerminationMethod;

typedef enum AppPolicyThreadInitializationType
{
    AppPolicyThreadInitializationType_None            = 0,
    AppPolicyThreadInitializationType_InitializeWinRT = 1,
} AppPolicyThreadInitializationType;

typedef enum AppPolicyShowDeveloperDiagnostic
{
    AppPolicyShowDeveloperDiagnostic_None   = 0,
    AppPolicyShowDeveloperDiagnostic_ShowUI = 1,
} AppPolicyShowDeveloperDiagnostic;

typedef enum AppPolicyWindowingModel
{
    AppPolicyWindowingModel_None           = 0,
    AppPolicyWindowingModel_Universal      = 1,
    AppPolicyWindowingModel_ClassicDesktop = 2,
    AppPolicyWindowingModel_ClassicPhone   = 3
} AppPolicyWindowingModel;

typedef struct PACKAGE_VERSION
{
    union
    {
        UINT64 Version;
        struct
        {
            USHORT Revision;
            USHORT Build;
            USHORT Minor;
            USHORT Major;
        }
        DUMMYSTRUCTNAME;
    }
    DUMMYUNIONNAME;
}
PACKAGE_VERSION;

typedef struct PACKAGE_ID
{
    UINT32 reserved;
    UINT32 processorArchitecture;
    PACKAGE_VERSION version;
    WCHAR *name;
    WCHAR *publisher;
    WCHAR *resourceId;
    WCHAR *publisherId;
}
PACKAGE_ID;

typedef struct PACKAGE_INFO
{
    UINT32     reserved;
    UINT32     flags;
    WCHAR      *path;
    WCHAR      *packageFullName;
    WCHAR      *packageFamilyName;
    PACKAGE_ID packageId;
}
PACKAGE_INFO;

typedef struct _PACKAGE_INFO_REFERENCE
{
    void *reserved;
}
*PACKAGE_INFO_REFERENCE;

LONG WINAPI AppPolicyGetMediaFoundationCodecLoading(HANDLE token, AppPolicyMediaFoundationCodecLoading *policy);
LONG WINAPI AppPolicyGetProcessTerminationMethod(HANDLE token, AppPolicyProcessTerminationMethod *policy);
LONG WINAPI AppPolicyGetShowDeveloperDiagnostic(HANDLE token, AppPolicyShowDeveloperDiagnostic *policy);
LONG WINAPI AppPolicyGetThreadInitializationType(HANDLE token, AppPolicyThreadInitializationType *policy);
LONG WINAPI AppPolicyGetWindowingModel(HANDLE processToken, AppPolicyWindowingModel *policy);
LONG WINAPI PackageFullNameFromId(const PACKAGE_ID *id, UINT32 *name_length, WCHAR *name);
LONG WINAPI PackageIdFromFullName(const WCHAR *full_name, UINT32 flags, UINT32 *buffer_length, BYTE *buffer);
LONG WINAPI OpenPackageInfoByFullName(const WCHAR *full_name, UINT32 reserved, PACKAGE_INFO_REFERENCE *info_reference);
LONG WINAPI ClosePackageInfo(PACKAGE_INFO_REFERENCE info_reference);
LONG WINAPI GetPackageInfo(PACKAGE_INFO_REFERENCE info_reference, UINT32 flags, UINT32 *buffer_length, BYTE *buffer, UINT32 *count);
LONG WINAPI VerifyPackageFamilyName(const WCHAR *package_family_name);
LONG WINAPI FindPackagesByPackageFamily(const WCHAR *package_family_name, UINT32 package_filters,
    UINT32 *count, WCHAR **package_full_names, UINT32 *buffer_length, WCHAR *buffer, UINT32 *package_properties);
LONG WINAPI FormatApplicationUserModelId(const WCHAR *package_family_name, const WCHAR *package_relative_app_id,
                                          UINT32 *application_user_model_id_length, WCHAR *application_user_model_id);

/* Dynamic Dependencies (Windows 10 1809+ / Windows App SDK bootstrap) */
typedef UINT32 PackageDependencyProcessorArchitectures;
typedef INT32 PackageDependencyLifetimeKind;
typedef UINT32 CreatePackageDependencyOptions;
#define CreatePackageDependencyOptions_None 0
#define CreatePackageDependencyOptions_ScopeIsSystem 2
#define CreatePackageDependencyOptions_DoNotVerifyDependencyResolution 1
typedef UINT32 AddPackageDependencyOptions;
typedef UINT32 AddPackageDependencyOptions2;

typedef struct PACKAGEDEPENDENCY_CONTEXT__ { void *unused; } *PACKAGEDEPENDENCY_CONTEXT;

HRESULT WINAPI TryCreatePackageDependency(void *user, const WCHAR *package_family_name, PACKAGE_VERSION min_version,
    PackageDependencyProcessorArchitectures architectures, PackageDependencyLifetimeKind lifetime_kind,
    const WCHAR *lifetime_artifact, CreatePackageDependencyOptions options, WCHAR **package_dependency_id);
HRESULT WINAPI TryCreatePackageDependency2(void *user, const WCHAR *package_family_name, PACKAGE_VERSION min_version,
    PackageDependencyProcessorArchitectures architectures, PackageDependencyLifetimeKind lifetime_kind,
    const WCHAR *lifetime_artifact, CreatePackageDependencyOptions options, const FILETIME *lifetime_expiration,
    WCHAR **package_dependency_id);
HRESULT WINAPI DeletePackageDependency(const WCHAR *package_dependency_id);
HRESULT WINAPI AddPackageDependency(const WCHAR *package_dependency_id, INT32 rank, AddPackageDependencyOptions options,
    PACKAGEDEPENDENCY_CONTEXT *package_dependency_context, WCHAR **package_full_name);
HRESULT WINAPI AddPackageDependency2(const WCHAR *package_dependency_id, INT32 rank, AddPackageDependencyOptions2 options,
    PACKAGEDEPENDENCY_CONTEXT *package_dependency_context, WCHAR **package_full_name);
HRESULT WINAPI RemovePackageDependency(PACKAGEDEPENDENCY_CONTEXT package_dependency_context);
HRESULT WINAPI GetResolvedPackageFullNameForPackageDependency(const WCHAR *package_dependency_id, WCHAR **package_full_name);
HRESULT WINAPI GetResolvedPackageFullNameForPackageDependency2(const WCHAR *package_dependency_id, WCHAR **package_full_name);
HRESULT WINAPI GetIdForPackageDependencyContext(PACKAGEDEPENDENCY_CONTEXT package_dependency_context, WCHAR **package_dependency_id);
UINT32 WINAPI GetPackageGraphRevisionId(void);

typedef INT32 PackagePathType;
LONG WINAPI GetCurrentPackageInfo2(UINT32 flags, PackagePathType path_type, UINT32 *buffer_length, BYTE *buffer, UINT32 *count);
LONG WINAPI GetCurrentPackageInfo3(UINT32 flags, PackagePathType path_type, UINT32 *buffer_length, BYTE *buffer, UINT32 *count);

#if defined(__cplusplus)
}
#endif

#endif /* _APPMODEL_H_ */
