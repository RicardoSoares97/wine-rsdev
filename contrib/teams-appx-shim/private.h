/* WinRT Windows.Management.Deployment Implementation
 *
 * Copyright (C) 2023 Mohamad Al-Jaf
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

#ifndef __WINE_WINDOWS_MANAGEMENT_DEPLOYMENT_PRIVATE_H
#define __WINE_WINDOWS_MANAGEMENT_DEPLOYMENT_PRIVATE_H

#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winstring.h"

#include "activation.h"

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#include "windows.foundation.h"
#define WIDL_using_Windows_Management_Deployment
#define WIDL_using_Windows_ApplicationModel
#include "windows.management.deployment.h"

extern IActivationFactory *package_manager_factory;
extern IActivationFactory *stage_package_options_factory;

/* appmodel.h types/functions - absent from Wine entirely, hand-declared
 * from the real Windows SDK header. */
typedef struct PACKAGE_VERSION {
    union {
        UINT64 Version;
        struct { USHORT Revision, Build, Minor, Major; } s;
    } u;
} PACKAGE_VERSION;

typedef struct PACKAGE_ID {
    UINT32          reserved;
    UINT32          processorArchitecture;
    PACKAGE_VERSION version;
    PWSTR           name;
    PWSTR           publisher;
    PWSTR           resourceId;
    PWSTR           publisherId;
} PACKAGE_ID;

typedef struct PACKAGE_INFO {
    UINT32     reserved;
    UINT32     flags;
    PWSTR      path;
    PWSTR      packageFullName;
    PWSTR      packageFamilyName;
    PACKAGE_ID packageId;
} PACKAGE_INFO;

typedef struct _PACKAGE_INFO_REFERENCE { void *reserved; } *PACKAGE_INFO_REFERENCE;

HRESULT StageAndRegisterMsix( const wchar_t *winPath, wchar_t **outFamilyName );
HRESULT UriRuntimeClassToWinPath( IUriRuntimeClass *uri, wchar_t *out, size_t outCap );

/* Windows.Management.Deployment.StagePackageOptions - not yet in Wine's
 * generated headers, hand-written from the real GUIDs/vtable layout
 * (verified against Microsoft's own windows-rs generated bindings). */
typedef INT32 StubPackageOption;

typedef struct IStagePackageOptions IStagePackageOptions;
typedef struct IStagePackageOptionsVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IStagePackageOptions*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IStagePackageOptions*);
    ULONG   (STDMETHODCALLTYPE *Release)(IStagePackageOptions*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(IStagePackageOptions*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(IStagePackageOptions*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(IStagePackageOptions*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *get_DependencyPackageUris)(IStagePackageOptions*, void**);
    HRESULT (STDMETHODCALLTYPE *get_TargetVolume)(IStagePackageOptions*, void**);
    HRESULT (STDMETHODCALLTYPE *put_TargetVolume)(IStagePackageOptions*, void*);
    HRESULT (STDMETHODCALLTYPE *get_OptionalPackageFamilyNames)(IStagePackageOptions*, void**);
    HRESULT (STDMETHODCALLTYPE *get_OptionalPackageUris)(IStagePackageOptions*, void**);
    HRESULT (STDMETHODCALLTYPE *get_RelatedPackageUris)(IStagePackageOptions*, void**);
    HRESULT (STDMETHODCALLTYPE *get_ExternalLocationUri)(IStagePackageOptions*, void**);
    HRESULT (STDMETHODCALLTYPE *put_ExternalLocationUri)(IStagePackageOptions*, void*);
    HRESULT (STDMETHODCALLTYPE *get_StubPackageOption)(IStagePackageOptions*, StubPackageOption*);
    HRESULT (STDMETHODCALLTYPE *put_StubPackageOption)(IStagePackageOptions*, StubPackageOption);
    HRESULT (STDMETHODCALLTYPE *get_DeveloperMode)(IStagePackageOptions*, boolean*);
    HRESULT (STDMETHODCALLTYPE *put_DeveloperMode)(IStagePackageOptions*, boolean);
    HRESULT (STDMETHODCALLTYPE *get_ForceUpdateFromAnyVersion)(IStagePackageOptions*, boolean*);
    HRESULT (STDMETHODCALLTYPE *put_ForceUpdateFromAnyVersion)(IStagePackageOptions*, boolean);
    HRESULT (STDMETHODCALLTYPE *get_InstallAllResources)(IStagePackageOptions*, boolean*);
    HRESULT (STDMETHODCALLTYPE *put_InstallAllResources)(IStagePackageOptions*, boolean);
    HRESULT (STDMETHODCALLTYPE *get_RequiredContentGroupOnly)(IStagePackageOptions*, boolean*);
    HRESULT (STDMETHODCALLTYPE *put_RequiredContentGroupOnly)(IStagePackageOptions*, boolean);
    HRESULT (STDMETHODCALLTYPE *get_StageInPlace)(IStagePackageOptions*, boolean*);
    HRESULT (STDMETHODCALLTYPE *put_StageInPlace)(IStagePackageOptions*, boolean);
    HRESULT (STDMETHODCALLTYPE *get_AllowUnsigned)(IStagePackageOptions*, boolean*);
    HRESULT (STDMETHODCALLTYPE *put_AllowUnsigned)(IStagePackageOptions*, boolean);
} IStagePackageOptionsVtbl;
struct IStagePackageOptions { IStagePackageOptionsVtbl *lpVtbl; };
#define IStagePackageOptions_AddRef(T) (T)->lpVtbl->AddRef(T)
#define IStagePackageOptions_Release(T) (T)->lpVtbl->Release(T)
#define IStagePackageOptions_QueryInterface(T,r,p) (T)->lpVtbl->QueryInterface(T,r,p)

DEFINE_GUID(IID_IStagePackageOptions_, 0x0b110c9c,0xb95d,0x4c56,0xbd,0x36,0x6d,0x65,0x68,0x00,0xd0,0x6b);

/* Windows.Management.Deployment.IPackageManager9 - also not in Wine's
 * generated headers yet; verified real GUID/vtable from windows-rs. */
typedef struct IPackageManager9 IPackageManager9;
typedef struct IPackageManager9Vtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IPackageManager9*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IPackageManager9*);
    ULONG   (STDMETHODCALLTYPE *Release)(IPackageManager9*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(IPackageManager9*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(IPackageManager9*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(IPackageManager9*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *FindProvisionedPackages)(IPackageManager9*, void**);
    HRESULT (STDMETHODCALLTYPE *AddPackageByUriAsync)(IPackageManager9*, IUriRuntimeClass*, IStagePackageOptions*, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress**);
    HRESULT (STDMETHODCALLTYPE *StagePackageByUriAsync)(IPackageManager9*, IUriRuntimeClass*, IStagePackageOptions*, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress**);
    HRESULT (STDMETHODCALLTYPE *RegisterPackageByUriAsync)(IPackageManager9*, IUriRuntimeClass*, void*, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress**);
    HRESULT (STDMETHODCALLTYPE *RegisterPackagesByFullNameAsync)(IPackageManager9*, void*, void*, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress**);
    HRESULT (STDMETHODCALLTYPE *SetPackageStubPreference)(IPackageManager9*, HSTRING, INT32);
    HRESULT (STDMETHODCALLTYPE *GetPackageStubPreference)(IPackageManager9*, HSTRING, INT32*);
} IPackageManager9Vtbl;
struct IPackageManager9 { IPackageManager9Vtbl *lpVtbl; };
#define IPackageManager9_AddRef(T) (T)->lpVtbl->AddRef(T)
#define IPackageManager9_Release(T) (T)->lpVtbl->Release(T)
#define IPackageManager9_QueryInterface(T,r,p) (T)->lpVtbl->QueryInterface(T,r,p)

DEFINE_GUID(IID_IPackageManager9_, 0x1aa79035,0xcc71,0x4b2e,0x80,0xa6,0xc7,0x04,0x1d,0x85,0x79,0xa7);

/* Windows.Management.Deployment.IPackageManager6 */
typedef INT32 AddPackageByAppInstallerOptions;
typedef struct IPackageManager6 IPackageManager6;
typedef struct IPackageManager6Vtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IPackageManager6*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IPackageManager6*);
    ULONG   (STDMETHODCALLTYPE *Release)(IPackageManager6*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(IPackageManager6*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(IPackageManager6*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(IPackageManager6*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *ProvisionPackageForAllUsersAsync)(IPackageManager6*, HSTRING, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress**);
    HRESULT (STDMETHODCALLTYPE *AddPackageByAppInstallerFileAsync)(IPackageManager6*, IUriRuntimeClass*, AddPackageByAppInstallerOptions, void*, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress**);
    HRESULT (STDMETHODCALLTYPE *RequestAddPackageByAppInstallerFileAsync)(IPackageManager6*, IUriRuntimeClass*, AddPackageByAppInstallerOptions, void*, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress**);
    HRESULT (STDMETHODCALLTYPE *AddPackageToVolumeAndRelatedSetAsync)(IPackageManager6*, IUriRuntimeClass*, void*, INT32, void*, void*, void*, void*, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress**);
    HRESULT (STDMETHODCALLTYPE *StagePackageToVolumeAndRelatedSetAsync)(IPackageManager6*, IUriRuntimeClass*, void*, INT32, void*, void*, void*, void*, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress**);
    HRESULT (STDMETHODCALLTYPE *RequestAddPackageAsync)(IPackageManager6*, IUriRuntimeClass*, void*, INT32, void*, void*, void*, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress**);
} IPackageManager6Vtbl;
struct IPackageManager6 { IPackageManager6Vtbl *lpVtbl; };
#define IPackageManager6_AddRef(T) (T)->lpVtbl->AddRef(T)
#define IPackageManager6_Release(T) (T)->lpVtbl->Release(T)
#define IPackageManager6_QueryInterface(T,r,p) (T)->lpVtbl->QueryInterface(T,r,p)

DEFINE_GUID(IID_IPackageManager6_, 0x0847e909,0x53cd,0x4e4f,0x83,0x2e,0x57,0xd1,0x80,0xf6,0xe4,0x47);

/* Windows.ApplicationModel.LimitedAccessFeatures - not in Wine's generated
 * headers; GUIDs/vtable layout verified against Microsoft's own windows-rs
 * generated bindings (crates/libs/windows/src/Windows/ApplicationModel/mod.rs). */
typedef INT32 LimitedAccessFeatureStatus;
#define LimitedAccessFeatureStatus_Unavailable          0
#define LimitedAccessFeatureStatus_Available            1
#define LimitedAccessFeatureStatus_AvailableWithoutToken 2
#define LimitedAccessFeatureStatus_Unknown              3

typedef struct ILimitedAccessFeatureRequestResult ILimitedAccessFeatureRequestResult;
typedef struct ILimitedAccessFeatureRequestResultVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ILimitedAccessFeatureRequestResult*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ILimitedAccessFeatureRequestResult*);
    ULONG   (STDMETHODCALLTYPE *Release)(ILimitedAccessFeatureRequestResult*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(ILimitedAccessFeatureRequestResult*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(ILimitedAccessFeatureRequestResult*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(ILimitedAccessFeatureRequestResult*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *get_FeatureId)(ILimitedAccessFeatureRequestResult*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *get_Status)(ILimitedAccessFeatureRequestResult*, LimitedAccessFeatureStatus*);
    HRESULT (STDMETHODCALLTYPE *get_EstimatedRemovalDate)(ILimitedAccessFeatureRequestResult*, void**);
} ILimitedAccessFeatureRequestResultVtbl;
struct ILimitedAccessFeatureRequestResult { ILimitedAccessFeatureRequestResultVtbl *lpVtbl; };
#define ILimitedAccessFeatureRequestResult_Release(T) (T)->lpVtbl->Release(T)

DEFINE_GUID(IID_ILimitedAccessFeatureRequestResult_, 0xd45156a6,0x1e24,0x5ddd,0xab,0xb4,0x61,0x88,0xab,0xa4,0xd5,0xbf);

typedef struct ILimitedAccessFeaturesStatics ILimitedAccessFeaturesStatics;
typedef struct ILimitedAccessFeaturesStaticsVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ILimitedAccessFeaturesStatics*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ILimitedAccessFeaturesStatics*);
    ULONG   (STDMETHODCALLTYPE *Release)(ILimitedAccessFeaturesStatics*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(ILimitedAccessFeaturesStatics*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(ILimitedAccessFeaturesStatics*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(ILimitedAccessFeaturesStatics*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *TryUnlockFeature)(ILimitedAccessFeaturesStatics*, HSTRING, HSTRING, HSTRING, ILimitedAccessFeatureRequestResult**);
} ILimitedAccessFeaturesStaticsVtbl;
struct ILimitedAccessFeaturesStatics { ILimitedAccessFeaturesStaticsVtbl *lpVtbl; };

DEFINE_GUID(IID_ILimitedAccessFeaturesStatics_, 0x8be612d4,0x302b,0x5fbf,0xa6,0x32,0x1a,0x99,0xe4,0x3e,0x89,0x25);

extern IActivationFactory *limited_access_features_factory;

/* ============================================================
 * Windows.UI.Composition (minimal real subset) - not part of Wine's
 * generated headers at all. GUIDs/method order verified verbatim against
 * the real Windows 10 SDK IDL (windows.ui.composition.idl / .interop.h,
 * via a public WDK mirror) - never guessed. Only the members needed to
 * let an app hand a DXGI composition swapchain to a window are declared;
 * everything else on ICompositor is a real, addressable vtable slot but
 * returns E_NOTIMPL until something actually calls it.
 * ============================================================ */
#include "wingdi.h"
#include "winuser.h"
#include "d3d11.h"
#include "dxgi1_2.h"

typedef struct ICompositionSurface ICompositionSurface;
typedef struct ICompositionSurfaceVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICompositionSurface*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ICompositionSurface*);
    ULONG   (STDMETHODCALLTYPE *Release)(ICompositionSurface*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(ICompositionSurface*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(ICompositionSurface*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(ICompositionSurface*, TrustLevel*);
} ICompositionSurfaceVtbl;
struct ICompositionSurface { ICompositionSurfaceVtbl *lpVtbl; };
DEFINE_GUID(IID_ICompositionSurface_, 0x1527540d,0x42c7,0x47a6,0xa4,0x08,0x66,0x8f,0x79,0xa9,0x0d,0xfb);

typedef struct ICompositionBrush ICompositionBrush;
typedef struct ICompositionBrushVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICompositionBrush*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ICompositionBrush*);
    ULONG   (STDMETHODCALLTYPE *Release)(ICompositionBrush*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(ICompositionBrush*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(ICompositionBrush*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(ICompositionBrush*, TrustLevel*);
} ICompositionBrushVtbl;
struct ICompositionBrush { ICompositionBrushVtbl *lpVtbl; };
DEFINE_GUID(IID_ICompositionBrush_, 0xab0d7608,0x30c0,0x40e9,0xb5,0x68,0xb6,0x0a,0x6b,0xd1,0xfb,0x46);

typedef struct ICompositionSurfaceBrush ICompositionSurfaceBrush;
typedef struct ICompositionSurfaceBrushVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICompositionSurfaceBrush*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ICompositionSurfaceBrush*);
    ULONG   (STDMETHODCALLTYPE *Release)(ICompositionSurfaceBrush*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(ICompositionSurfaceBrush*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(ICompositionSurfaceBrush*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(ICompositionSurfaceBrush*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *get_BitmapInterpolationMode)(ICompositionSurfaceBrush*, INT32*);
    HRESULT (STDMETHODCALLTYPE *put_BitmapInterpolationMode)(ICompositionSurfaceBrush*, INT32);
    HRESULT (STDMETHODCALLTYPE *get_HorizontalAlignmentRatio)(ICompositionSurfaceBrush*, float*);
    HRESULT (STDMETHODCALLTYPE *put_HorizontalAlignmentRatio)(ICompositionSurfaceBrush*, float);
    HRESULT (STDMETHODCALLTYPE *get_Stretch)(ICompositionSurfaceBrush*, INT32*);
    HRESULT (STDMETHODCALLTYPE *put_Stretch)(ICompositionSurfaceBrush*, INT32);
    HRESULT (STDMETHODCALLTYPE *get_Surface)(ICompositionSurfaceBrush*, ICompositionSurface**);
    HRESULT (STDMETHODCALLTYPE *put_Surface)(ICompositionSurfaceBrush*, ICompositionSurface*);
    HRESULT (STDMETHODCALLTYPE *get_VerticalAlignmentRatio)(ICompositionSurfaceBrush*, float*);
    HRESULT (STDMETHODCALLTYPE *put_VerticalAlignmentRatio)(ICompositionSurfaceBrush*, float);
} ICompositionSurfaceBrushVtbl;
struct ICompositionSurfaceBrush { ICompositionSurfaceBrushVtbl *lpVtbl; };
DEFINE_GUID(IID_ICompositionSurfaceBrush_, 0xad016d79,0x1e4c,0x4c0d,0x9c,0x29,0x83,0x33,0x8c,0x87,0xc1,0x62);

typedef struct IVisual IVisual;
typedef struct IVisualVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IVisual*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IVisual*);
    ULONG   (STDMETHODCALLTYPE *Release)(IVisual*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(IVisual*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(IVisual*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(IVisual*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *get_AnchorPoint)(IVisual*, void*);
    HRESULT (STDMETHODCALLTYPE *put_AnchorPoint)(IVisual*, UINT64);
    HRESULT (STDMETHODCALLTYPE *get_BackfaceVisibility)(IVisual*, INT32*);
    HRESULT (STDMETHODCALLTYPE *put_BackfaceVisibility)(IVisual*, INT32);
    HRESULT (STDMETHODCALLTYPE *get_BorderMode)(IVisual*, INT32*);
    HRESULT (STDMETHODCALLTYPE *put_BorderMode)(IVisual*, INT32);
    HRESULT (STDMETHODCALLTYPE *get_CenterPoint)(IVisual*, void*);
    HRESULT (STDMETHODCALLTYPE *put_CenterPoint)(IVisual*, float, float, float);
    HRESULT (STDMETHODCALLTYPE *get_Clip)(IVisual*, void**);
    HRESULT (STDMETHODCALLTYPE *put_Clip)(IVisual*, void*);
    HRESULT (STDMETHODCALLTYPE *get_CompositeMode)(IVisual*, INT32*);
    HRESULT (STDMETHODCALLTYPE *put_CompositeMode)(IVisual*, INT32);
    HRESULT (STDMETHODCALLTYPE *get_IsVisible)(IVisual*, boolean*);
    HRESULT (STDMETHODCALLTYPE *put_IsVisible)(IVisual*, boolean);
    HRESULT (STDMETHODCALLTYPE *get_Offset)(IVisual*, void*);
    HRESULT (STDMETHODCALLTYPE *put_Offset)(IVisual*, float, float, float);
    HRESULT (STDMETHODCALLTYPE *get_Opacity)(IVisual*, float*);
    HRESULT (STDMETHODCALLTYPE *put_Opacity)(IVisual*, float);
    HRESULT (STDMETHODCALLTYPE *get_Orientation)(IVisual*, void*);
    HRESULT (STDMETHODCALLTYPE *put_Orientation)(IVisual*, float, float, float, float);
    HRESULT (STDMETHODCALLTYPE *get_Parent)(IVisual*, void**);
    HRESULT (STDMETHODCALLTYPE *get_RotationAngle)(IVisual*, float*);
    HRESULT (STDMETHODCALLTYPE *put_RotationAngle)(IVisual*, float);
    HRESULT (STDMETHODCALLTYPE *get_RotationAngleInDegrees)(IVisual*, float*);
    HRESULT (STDMETHODCALLTYPE *put_RotationAngleInDegrees)(IVisual*, float);
    HRESULT (STDMETHODCALLTYPE *get_RotationAxis)(IVisual*, void*);
    HRESULT (STDMETHODCALLTYPE *put_RotationAxis)(IVisual*, float, float, float);
    HRESULT (STDMETHODCALLTYPE *get_Scale)(IVisual*, void*);
    HRESULT (STDMETHODCALLTYPE *put_Scale)(IVisual*, float, float, float);
    HRESULT (STDMETHODCALLTYPE *get_Size)(IVisual*, void*);
    HRESULT (STDMETHODCALLTYPE *put_Size)(IVisual*, float, float);
    HRESULT (STDMETHODCALLTYPE *get_TransformMatrix)(IVisual*, void*);
    HRESULT (STDMETHODCALLTYPE *put_TransformMatrix)(IVisual*, void*);
} IVisualVtbl;
struct IVisual { IVisualVtbl *lpVtbl; };
DEFINE_GUID(IID_IVisual_, 0x117e202d,0xa859,0x4c89,0x87,0x3b,0xc2,0xaa,0x56,0x67,0x88,0xe3);

/* IVisual2 - a version-probe interface real apps routinely QueryInterface
 * for on any Visual (checked via windows-rs tag 74, since - like
 * IDesktopWindowTarget - it's absent from the pruned "master" branch and
 * from the old 10586 WDK IDL). Missing this made ms-teams.exe's own
 * winrt::hresult_error get thrown for a perfectly ordinary capability
 * check, which Wine's C++ exception delivery then failed to route back
 * to Teams' own catch handler - a real, fixable cause behind what first
 * looked like an unfixable exception-delivery gap. */
typedef struct IVisual2 IVisual2;
typedef struct IVisual2Vtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IVisual2*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IVisual2*);
    ULONG   (STDMETHODCALLTYPE *Release)(IVisual2*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(IVisual2*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(IVisual2*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(IVisual2*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *get_ParentForTransform)(IVisual2*, void**);
    HRESULT (STDMETHODCALLTYPE *put_ParentForTransform)(IVisual2*, void*);
    HRESULT (STDMETHODCALLTYPE *get_RelativeOffsetAdjustment)(IVisual2*, void*);
    HRESULT (STDMETHODCALLTYPE *put_RelativeOffsetAdjustment)(IVisual2*, float, float, float);
    HRESULT (STDMETHODCALLTYPE *get_RelativeSizeAdjustment)(IVisual2*, void*);
    HRESULT (STDMETHODCALLTYPE *put_RelativeSizeAdjustment)(IVisual2*, float, float);
} IVisual2Vtbl;
struct IVisual2 { IVisual2Vtbl *lpVtbl; };
DEFINE_GUID(IID_IVisual2_, 0x3052b611,0x56c3,0x4c3e,0x8b,0xf3,0xf6,0xe1,0xad,0x47,0x3f,0x06);

typedef struct ISpriteVisual ISpriteVisual;
typedef struct ISpriteVisualVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ISpriteVisual*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ISpriteVisual*);
    ULONG   (STDMETHODCALLTYPE *Release)(ISpriteVisual*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(ISpriteVisual*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(ISpriteVisual*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(ISpriteVisual*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *get_Brush)(ISpriteVisual*, ICompositionBrush**);
    HRESULT (STDMETHODCALLTYPE *put_Brush)(ISpriteVisual*, ICompositionBrush*);
} ISpriteVisualVtbl;
struct ISpriteVisual { ISpriteVisualVtbl *lpVtbl; };
DEFINE_GUID(IID_ISpriteVisual_, 0x08e05581,0x1ad1,0x4f97,0x97,0x57,0x40,0x2d,0x76,0xe4,0x23,0x3b);

typedef struct IVisualCollection IVisualCollection;
typedef struct IVisualCollectionVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IVisualCollection*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IVisualCollection*);
    ULONG   (STDMETHODCALLTYPE *Release)(IVisualCollection*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(IVisualCollection*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(IVisualCollection*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(IVisualCollection*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *get_Count)(IVisualCollection*, INT32*);
    HRESULT (STDMETHODCALLTYPE *InsertAbove)(IVisualCollection*, IVisual*, IVisual*);
    HRESULT (STDMETHODCALLTYPE *InsertAtBottom)(IVisualCollection*, IVisual*);
    HRESULT (STDMETHODCALLTYPE *InsertAtTop)(IVisualCollection*, IVisual*);
    HRESULT (STDMETHODCALLTYPE *InsertBelow)(IVisualCollection*, IVisual*, IVisual*);
    HRESULT (STDMETHODCALLTYPE *Remove)(IVisualCollection*, IVisual*);
    HRESULT (STDMETHODCALLTYPE *RemoveAll)(IVisualCollection*);
} IVisualCollectionVtbl;
struct IVisualCollection { IVisualCollectionVtbl *lpVtbl; };
DEFINE_GUID(IID_IVisualCollection_, 0x8b745505,0xfd3e,0x4a98,0x84,0xa8,0xe9,0x49,0x46,0x8c,0x6b,0xcb);

typedef struct IContainerVisual IContainerVisual;
typedef struct IContainerVisualVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IContainerVisual*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IContainerVisual*);
    ULONG   (STDMETHODCALLTYPE *Release)(IContainerVisual*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(IContainerVisual*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(IContainerVisual*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(IContainerVisual*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *get_Children)(IContainerVisual*, IVisualCollection**);
} IContainerVisualVtbl;
struct IContainerVisual { IContainerVisualVtbl *lpVtbl; };
DEFINE_GUID(IID_IContainerVisual_, 0x02f6bc74,0xed20,0x4773,0xaf,0xe6,0xd4,0x9b,0x4a,0x93,0xdb,0x32);

/* ICompositionSupportsSystemBackdrop - Windows 11 Mica/Acrylic backdrop
 * probe on a composition target; GUID/vtable verified against real
 * windows-rs generated bindings (tag 74, since - like IDesktopWindowTarget
 * and IVisual2 - it's absent from the pruned "master" branch). */
typedef struct ICompositionSupportsSystemBackdrop ICompositionSupportsSystemBackdrop;
typedef struct ICompositionSupportsSystemBackdropVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICompositionSupportsSystemBackdrop*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ICompositionSupportsSystemBackdrop*);
    ULONG   (STDMETHODCALLTYPE *Release)(ICompositionSupportsSystemBackdrop*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(ICompositionSupportsSystemBackdrop*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(ICompositionSupportsSystemBackdrop*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(ICompositionSupportsSystemBackdrop*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *get_SystemBackdrop)(ICompositionSupportsSystemBackdrop*, void**);
    HRESULT (STDMETHODCALLTYPE *put_SystemBackdrop)(ICompositionSupportsSystemBackdrop*, void*);
} ICompositionSupportsSystemBackdropVtbl;
struct ICompositionSupportsSystemBackdrop { ICompositionSupportsSystemBackdropVtbl *lpVtbl; };
DEFINE_GUID(IID_ICompositionSupportsSystemBackdrop_, 0x397dafe4,0xb6c2,0x5bb9,0x95,0x1d,0xf5,0x70,0x7d,0xe8,0xb7,0xbc);

typedef struct ICompositionTarget ICompositionTarget;
typedef struct ICompositionTargetVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICompositionTarget*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ICompositionTarget*);
    ULONG   (STDMETHODCALLTYPE *Release)(ICompositionTarget*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(ICompositionTarget*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(ICompositionTarget*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(ICompositionTarget*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *get_Root)(ICompositionTarget*, IVisual**);
    HRESULT (STDMETHODCALLTYPE *put_Root)(ICompositionTarget*, IVisual*);
} ICompositionTargetVtbl;
struct ICompositionTarget { ICompositionTargetVtbl *lpVtbl; };
DEFINE_GUID(IID_ICompositionTarget_, 0xa1bea8ba,0xd726,0x4663,0x81,0x29,0x6b,0x5e,0x79,0x27,0xff,0xa6);

/* Windows.UI.Composition.Desktop.IDesktopWindowTarget - the object
 * CreateDesktopWindowTarget hands back really implements THIS (which
 * itself has almost nothing on it - just IsTopmost) plus ICompositionTarget
 * (Root/SetRoot) via QueryInterface; verified against real windows-rs
 * generated bindings (crates/libs/windows/src/Windows/UI/Composition/Desktop/mod.rs,
 * tag 74 - this namespace was pruned from the "master" branch's default
 * feature set, so it must be fetched from an actual tagged release). */
typedef struct IDesktopWindowTarget IDesktopWindowTarget;
typedef struct IDesktopWindowTargetVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IDesktopWindowTarget*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IDesktopWindowTarget*);
    ULONG   (STDMETHODCALLTYPE *Release)(IDesktopWindowTarget*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(IDesktopWindowTarget*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(IDesktopWindowTarget*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(IDesktopWindowTarget*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *get_IsTopmost)(IDesktopWindowTarget*, boolean*);
} IDesktopWindowTargetVtbl;
struct IDesktopWindowTarget { IDesktopWindowTargetVtbl *lpVtbl; };
DEFINE_GUID(IID_IDesktopWindowTarget_, 0x6329d6ca,0x3366,0x490e,0x9d,0xb3,0x25,0x31,0x29,0x29,0xac,0x51);

typedef struct ICompositor ICompositor;
typedef struct ICompositorVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICompositor*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ICompositor*);
    ULONG   (STDMETHODCALLTYPE *Release)(ICompositor*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(ICompositor*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(ICompositor*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(ICompositor*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *CreateColorKeyFrameAnimation)(ICompositor*, void**);
    HRESULT (STDMETHODCALLTYPE *CreateColorBrush)(ICompositor*, void**);
    HRESULT (STDMETHODCALLTYPE *CreateColorBrushWithColor)(ICompositor*, UINT64, void**);
    HRESULT (STDMETHODCALLTYPE *CreateContainerVisual)(ICompositor*, void**);
    HRESULT (STDMETHODCALLTYPE *CreateCubicBezierEasingFunction)(ICompositor*, UINT64, UINT64, void**);
    HRESULT (STDMETHODCALLTYPE *CreateEffectFactory)(ICompositor*, void*, void**);
    HRESULT (STDMETHODCALLTYPE *CreateEffectFactoryWithProperties)(ICompositor*, void*, void*, void**);
    HRESULT (STDMETHODCALLTYPE *CreateExpressionAnimation)(ICompositor*, void**);
    HRESULT (STDMETHODCALLTYPE *CreateExpressionAnimationWithExpression)(ICompositor*, HSTRING, void**);
    HRESULT (STDMETHODCALLTYPE *CreateInsetClip)(ICompositor*, void**);
    HRESULT (STDMETHODCALLTYPE *CreateInsetClipWithInsets)(ICompositor*, float, float, float, float, void**);
    HRESULT (STDMETHODCALLTYPE *CreateLinearEasingFunction)(ICompositor*, void**);
    HRESULT (STDMETHODCALLTYPE *CreatePropertySet)(ICompositor*, void**);
    HRESULT (STDMETHODCALLTYPE *CreateQuaternionKeyFrameAnimation)(ICompositor*, void**);
    HRESULT (STDMETHODCALLTYPE *CreateScalarKeyFrameAnimation)(ICompositor*, void**);
    HRESULT (STDMETHODCALLTYPE *CreateScopedBatch)(ICompositor*, INT32, void**);
    HRESULT (STDMETHODCALLTYPE *CreateSpriteVisual)(ICompositor*, ISpriteVisual**);
    HRESULT (STDMETHODCALLTYPE *CreateSurfaceBrush)(ICompositor*, ICompositionSurfaceBrush**);
    HRESULT (STDMETHODCALLTYPE *CreateSurfaceBrushWithSurface)(ICompositor*, ICompositionSurface*, ICompositionSurfaceBrush**);
    HRESULT (STDMETHODCALLTYPE *CreateTargetForCurrentView)(ICompositor*, void**);
    HRESULT (STDMETHODCALLTYPE *CreateVector2KeyFrameAnimation)(ICompositor*, void**);
    HRESULT (STDMETHODCALLTYPE *CreateVector3KeyFrameAnimation)(ICompositor*, void**);
    HRESULT (STDMETHODCALLTYPE *CreateVector4KeyFrameAnimation)(ICompositor*, void**);
    HRESULT (STDMETHODCALLTYPE *GetCommitBatch)(ICompositor*, INT32, void**);
} ICompositorVtbl;
struct ICompositor { ICompositorVtbl *lpVtbl; };
DEFINE_GUID(IID_ICompositor_, 0xb403ca50,0x7f8c,0x4e83,0x98,0x5f,0xcc,0x45,0x06,0x00,0x36,0xd8);

typedef struct ICompositorInterop ICompositorInterop;
typedef struct ICompositorInteropVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICompositorInterop*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ICompositorInterop*);
    ULONG   (STDMETHODCALLTYPE *Release)(ICompositorInterop*);
    HRESULT (STDMETHODCALLTYPE *CreateCompositionSurfaceForHandle)(ICompositorInterop*, HANDLE, ICompositionSurface**);
    HRESULT (STDMETHODCALLTYPE *CreateCompositionSurfaceForSwapChain)(ICompositorInterop*, IUnknown*, ICompositionSurface**);
    HRESULT (STDMETHODCALLTYPE *CreateGraphicsDevice)(ICompositorInterop*, IUnknown*, void**);
} ICompositorInteropVtbl;
struct ICompositorInterop { ICompositorInteropVtbl *lpVtbl; };
DEFINE_GUID(IID_ICompositorInterop_, 0x25297d5c,0x3ad4,0x4c9c,0xb5,0xcf,0xe3,0x6a,0x38,0x51,0x23,0x30);

typedef struct ICompositorDesktopInterop ICompositorDesktopInterop;
typedef struct ICompositorDesktopInteropVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICompositorDesktopInterop*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ICompositorDesktopInterop*);
    ULONG   (STDMETHODCALLTYPE *Release)(ICompositorDesktopInterop*);
    HRESULT (STDMETHODCALLTYPE *CreateDesktopWindowTarget)(ICompositorDesktopInterop*, HWND, BOOL, IDesktopWindowTarget**);
    HRESULT (STDMETHODCALLTYPE *EnsureOnThread)(ICompositorDesktopInterop*, DWORD);
} ICompositorDesktopInteropVtbl;
struct ICompositorDesktopInterop { ICompositorDesktopInteropVtbl *lpVtbl; };
DEFINE_GUID(IID_ICompositorDesktopInterop_, 0x29e691fa,0x4567,0x4dca,0xb3,0x19,0xd0,0xf2,0x07,0xeb,0x68,0x07);

DEFINE_GUID(IID_IClosable_, 0x30d5a829,0x7fa4,0x4026,0x83,0xbb,0xd7,0x5b,0xae,0x4e,0xa9,0x9e);

extern IActivationFactory *compositor_factory;

/* Windows.UI.Composition.CompositionCapabilities - static-only probe class
 * apps use to check for effects/backdrop support; GUIDs/vtables verified
 * against real windows-rs generated bindings (tag 74). */
typedef struct ICompositionCapabilities ICompositionCapabilities;
typedef struct ICompositionCapabilitiesVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICompositionCapabilities*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ICompositionCapabilities*);
    ULONG   (STDMETHODCALLTYPE *Release)(ICompositionCapabilities*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(ICompositionCapabilities*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(ICompositionCapabilities*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(ICompositionCapabilities*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *AreEffectsSupported)(ICompositionCapabilities*, boolean*);
    HRESULT (STDMETHODCALLTYPE *AreEffectsFast)(ICompositionCapabilities*, boolean*);
    HRESULT (STDMETHODCALLTYPE *add_Changed)(ICompositionCapabilities*, void*, INT64*);
    HRESULT (STDMETHODCALLTYPE *remove_Changed)(ICompositionCapabilities*, INT64);
} ICompositionCapabilitiesVtbl;
struct ICompositionCapabilities { ICompositionCapabilitiesVtbl *lpVtbl; };
DEFINE_GUID(IID_ICompositionCapabilities_, 0x8253353e,0xb517,0x48bc,0xb1,0xe8,0x4b,0x35,0x61,0xa2,0xe1,0x81);

typedef struct ICompositionCapabilitiesStatics ICompositionCapabilitiesStatics;
typedef struct ICompositionCapabilitiesStaticsVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICompositionCapabilitiesStatics*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(ICompositionCapabilitiesStatics*);
    ULONG   (STDMETHODCALLTYPE *Release)(ICompositionCapabilitiesStatics*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(ICompositionCapabilitiesStatics*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(ICompositionCapabilitiesStatics*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(ICompositionCapabilitiesStatics*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *GetForCurrentView)(ICompositionCapabilitiesStatics*, ICompositionCapabilities**);
} ICompositionCapabilitiesStaticsVtbl;
struct ICompositionCapabilitiesStatics { ICompositionCapabilitiesStaticsVtbl *lpVtbl; };
DEFINE_GUID(IID_ICompositionCapabilitiesStatics_, 0xf7b7a86e,0x6416,0x49e5,0x8d,0xdf,0xaf,0xe9,0x49,0xe2,0x05,0x62);

extern IActivationFactory *composition_capabilities_factory;

/* hand-rolled COBJMACROS-style helpers - these interfaces aren't
 * widl-generated, so nothing auto-provides the usual IFoo_Method(p, ...)
 * wrapper macros. Only the members composition.c actually calls. */
#define ICompositionSurface_AddRef(p) (p)->lpVtbl->AddRef(p)
#define ICompositionSurface_Release(p) (p)->lpVtbl->Release(p)

#define ICompositionBrush_AddRef(p) (p)->lpVtbl->AddRef(p)
#define ICompositionBrush_Release(p) (p)->lpVtbl->Release(p)
#define ICompositionBrush_QueryInterface(p,r,o) (p)->lpVtbl->QueryInterface(p,r,o)

#define ICompositionSurfaceBrush_Release(p) (p)->lpVtbl->Release(p)
#define ICompositionSurfaceBrush_get_Surface(p,v) (p)->lpVtbl->get_Surface(p,v)

#define IVisual_AddRef(p) (p)->lpVtbl->AddRef(p)
#define IVisual_Release(p) (p)->lpVtbl->Release(p)
#define IVisual_QueryInterface(p,r,o) (p)->lpVtbl->QueryInterface(p,r,o)

#define IVisual2_AddRef(p) (p)->lpVtbl->AddRef(p)
#define IVisual2_Release(p) (p)->lpVtbl->Release(p)

#define ISpriteVisual_AddRef(p) (p)->lpVtbl->AddRef(p)
#define ISpriteVisual_Release(p) (p)->lpVtbl->Release(p)
#define ISpriteVisual_get_Brush(p,v) (p)->lpVtbl->get_Brush(p,v)

#define ICompositionTarget_AddRef(p) (p)->lpVtbl->AddRef(p)

#define ICompositor_AddRef(p) (p)->lpVtbl->AddRef(p)

#define IContainerVisual_AddRef(p) (p)->lpVtbl->AddRef(p)
#define IContainerVisual_Release(p) (p)->lpVtbl->Release(p)
#define IContainerVisual_get_Children(p,v) (p)->lpVtbl->get_Children(p,v)

#define IVisualCollection_AddRef(p) (p)->lpVtbl->AddRef(p)
#define IVisualCollection_Release(p) (p)->lpVtbl->Release(p)
#define IVisualCollection_get_Count(p,v) (p)->lpVtbl->get_Count(p,v)

/* Windows.System.Profile.WindowsIntegrityPolicy - static-only WinRT class,
 * GUID/vtable verified against Microsoft's own windows-rs generated bindings
 * (crates/libs/windows/src/Windows/System/Profile/mod.rs). */
typedef struct IWindowsIntegrityPolicyStatics IWindowsIntegrityPolicyStatics;
typedef struct IWindowsIntegrityPolicyStaticsVtbl
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(IWindowsIntegrityPolicyStatics*, REFIID, void**);
    ULONG   (STDMETHODCALLTYPE *AddRef)(IWindowsIntegrityPolicyStatics*);
    ULONG   (STDMETHODCALLTYPE *Release)(IWindowsIntegrityPolicyStatics*);
    HRESULT (STDMETHODCALLTYPE *GetIids)(IWindowsIntegrityPolicyStatics*, ULONG*, IID**);
    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(IWindowsIntegrityPolicyStatics*, HSTRING*);
    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(IWindowsIntegrityPolicyStatics*, TrustLevel*);
    HRESULT (STDMETHODCALLTYPE *get_IsEnabled)(IWindowsIntegrityPolicyStatics*, boolean*);
    HRESULT (STDMETHODCALLTYPE *get_IsEnabledForTrial)(IWindowsIntegrityPolicyStatics*, boolean*);
    HRESULT (STDMETHODCALLTYPE *get_CanDisable)(IWindowsIntegrityPolicyStatics*, boolean*);
    HRESULT (STDMETHODCALLTYPE *get_IsDisableSupported)(IWindowsIntegrityPolicyStatics*, boolean*);
    HRESULT (STDMETHODCALLTYPE *add_PolicyChanged)(IWindowsIntegrityPolicyStatics*, void*, INT64*);
    HRESULT (STDMETHODCALLTYPE *remove_PolicyChanged)(IWindowsIntegrityPolicyStatics*, INT64);
} IWindowsIntegrityPolicyStaticsVtbl;
struct IWindowsIntegrityPolicyStatics { IWindowsIntegrityPolicyStaticsVtbl *lpVtbl; };

DEFINE_GUID(IID_IWindowsIntegrityPolicyStatics_, 0x7d1d81db,0x8d63,0x4789,0x9e,0xa5,0xdd,0xcf,0x65,0xa9,0x4f,0x3c);

extern IActivationFactory *windows_integrity_policy_factory;

#define DEFINE_IINSPECTABLE_( pfx, iface_type, impl_type, impl_from, iface_mem, expr )             \
    static inline impl_type *impl_from( iface_type *iface )                                        \
    {                                                                                              \
        return CONTAINING_RECORD( iface, impl_type, iface_mem );                                   \
    }                                                                                              \
    static HRESULT WINAPI pfx##_QueryInterface( iface_type *iface, REFIID iid, void **out )        \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_QueryInterface( (IInspectable *)(expr), iid, out );                    \
    }                                                                                              \
    static ULONG WINAPI pfx##_AddRef( iface_type *iface )                                          \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_AddRef( (IInspectable *)(expr) );                                      \
    }                                                                                              \
    static ULONG WINAPI pfx##_Release( iface_type *iface )                                         \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_Release( (IInspectable *)(expr) );                                     \
    }                                                                                              \
    static HRESULT WINAPI pfx##_GetIids( iface_type *iface, ULONG *iid_count, IID **iids )         \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_GetIids( (IInspectable *)(expr), iid_count, iids );                    \
    }                                                                                              \
    static HRESULT WINAPI pfx##_GetRuntimeClassName( iface_type *iface, HSTRING *class_name )      \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_GetRuntimeClassName( (IInspectable *)(expr), class_name );             \
    }                                                                                              \
    static HRESULT WINAPI pfx##_GetTrustLevel( iface_type *iface, TrustLevel *trust_level )        \
    {                                                                                              \
        impl_type *impl = impl_from( iface );                                                      \
        return IInspectable_GetTrustLevel( (IInspectable *)(expr), trust_level );                  \
    }
#define DEFINE_IINSPECTABLE( pfx, iface_type, impl_type, base_iface )                              \
    DEFINE_IINSPECTABLE_( pfx, iface_type, impl_type, impl_from_##iface_type, iface_type##_iface, &impl->base_iface )

#endif
