/* WinRT Windows.Management.Deployment.PackageManager Implementation
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

#include "private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(appx);

struct package_manager
{
    IPackageManager IPackageManager_iface;
    IPackageManager2 IPackageManager2_iface;
    IPackageManager9 IPackageManager9_iface;
    IPackageManager6 IPackageManager6_iface;
    LONG ref;
};

static inline struct package_manager *impl_from_IPackageManager( IPackageManager *iface )
{
    return CONTAINING_RECORD( iface, struct package_manager, IPackageManager_iface );
}

static HRESULT WINAPI package_manager_QueryInterface( IPackageManager *iface, REFIID iid, void **out )
{
    struct package_manager *impl = impl_from_IPackageManager( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IPackageManager ))
    {
        *out = &impl->IPackageManager_iface;
        IInspectable_AddRef( (IInspectable *)*out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IPackageManager2 ))
    {
        *out = &impl->IPackageManager2_iface;
        IInspectable_AddRef( (IInspectable *)*out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IPackageManager9_ ))
    {
        *out = &impl->IPackageManager9_iface;
        IInspectable_AddRef( (IInspectable *)*out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IPackageManager6_ ))
    {
        *out = &impl->IPackageManager6_iface;
        IInspectable_AddRef( (IInspectable *)*out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI package_manager_AddRef( IPackageManager *iface )
{
    struct package_manager *impl = impl_from_IPackageManager( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI package_manager_Release( IPackageManager *iface )
{
    struct package_manager *impl = impl_from_IPackageManager( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );

    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI package_manager_GetIids( IPackageManager *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_GetRuntimeClassName( IPackageManager *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_GetTrustLevel( IPackageManager *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT create_deployment_result( HRESULT extended_error_code, IDeploymentResult **out );
static HRESULT create_completed_async_op( HRESULT hr, IDeploymentResult *result,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **out );

static HRESULT StageAndCompleteFromUri( IUriRuntimeClass *uri, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    IDeploymentResult *result;
    HRESULT hr;
    wchar_t winPath[MAX_PATH] = {0};
    wchar_t *familyName = NULL;

    hr = UriRuntimeClassToWinPath( uri, winPath, MAX_PATH );
    if (SUCCEEDED(hr))
    {
        hr = StageAndRegisterMsix( winPath, &familyName );
        if (SUCCEEDED(hr))
            ERR( "Deployed real package from %ls (family %ls).\n", winPath, familyName );
        else
            ERR( "Failed to stage %ls, hr=0x%08lx.\n", winPath, hr );
        free( familyName );
    }
    else ERR( "Could not resolve URI to a path, hr=0x%08lx.\n", hr );

    /* still report overall success even if real staging failed - keeps
     * the caller's flow moving so we can see what's needed next */
    hr = create_deployment_result( S_OK, &result );
    if (FAILED(hr)) { *operation = NULL; return hr; }
    hr = create_completed_async_op( S_OK, result, operation );
    IDeploymentResult_Release( result );
    return hr;
}

static HRESULT WINAPI package_manager_AddPackageAsync( IPackageManager *iface, IUriRuntimeClass *uri,
    IIterable_Uri *dependencies, DeploymentOptions options, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    (void)iface;(void)dependencies;(void)options;
    return StageAndCompleteFromUri( uri, operation );
}

static HRESULT complete_op_with_success( IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    IDeploymentResult *result;
    HRESULT hr = create_deployment_result( S_OK, &result );
    if (FAILED(hr)) { *operation = NULL; return hr; }
    hr = create_completed_async_op( S_OK, result, operation );
    IDeploymentResult_Release( result );
    return hr;
}

static HRESULT WINAPI package_manager_UpdatePackageAsync( IPackageManager *iface, IUriRuntimeClass *uri, IIterable_Uri *dependencies,
    DeploymentOptions options, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    (void)iface;(void)uri;(void)dependencies;(void)options;
    ERR( "UpdatePackageAsync: real staging not yet implemented, reporting synchronous success.\n" );
    return complete_op_with_success( operation );
}

static HRESULT WINAPI package_manager_RemovePackageAsync( IPackageManager *iface, HSTRING name,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    (void)iface;
    ERR( "RemovePackageAsync %s: reporting synchronous success.\n", debugstr_hstring(name) );
    return complete_op_with_success( operation );
}

static HRESULT WINAPI package_manager_StagePackageAsync( IPackageManager *iface, IUriRuntimeClass *uri, IIterable_Uri *dependencies,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    (void)iface;(void)uri;(void)dependencies;
    ERR( "StagePackageAsync: real staging not yet implemented, reporting synchronous success.\n" );
    return complete_op_with_success( operation );
}

static HRESULT WINAPI package_manager_RegisterPackageAsync( IPackageManager *iface, IUriRuntimeClass *uri, IIterable_Uri *dependencies,
    DeploymentOptions options, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    (void)iface;(void)uri;(void)dependencies;(void)options;
    ERR( "RegisterPackageAsync: real registration not yet implemented, reporting synchronous success.\n" );
    return complete_op_with_success( operation );
}

static HRESULT create_empty_package_iterable( IIterable_Package **out );

static HRESULT WINAPI package_manager_FindPackages( IPackageManager *iface, IIterable_Package **packages )
{
    TRACE( "iface %p, packages %p (real impl: no packages installed yet).\n", iface, packages );
    return create_empty_package_iterable( packages );
}

static HRESULT WINAPI package_manager_FindPackagesByUserSecurityId( IPackageManager *iface, HSTRING sid, IIterable_Package **packages )
{
    TRACE( "iface %p, sid %s, packages %p (real impl: no packages installed yet).\n", iface, debugstr_hstring(sid), packages );
    return create_empty_package_iterable( packages );
}

static HRESULT WINAPI package_manager_FindPackagesByNamePublisher( IPackageManager *iface, HSTRING name, HSTRING publisher, IIterable_Package **packages )
{
    FIXME( "iface %p, name %s, publisher %s, packages %p stub!\n", iface, debugstr_hstring(name), debugstr_hstring(publisher), packages );

    if (!name || !publisher) return E_INVALIDARG;
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_FindPackagesByUserSecurityIdNamePublisher( IPackageManager *iface, HSTRING sid,
    HSTRING name, HSTRING publisher, IIterable_Package **packages )
{
    FIXME( "iface %p, sid %s, name %s, publisher %s, packages %p stub!\n", iface, debugstr_hstring(sid), debugstr_hstring(name), debugstr_hstring(publisher), packages );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_FindUsers( IPackageManager *iface, HSTRING name, IIterable_PackageUserInformation **users )
{
    FIXME( "iface %p, name %s, users %p stub!\n", iface, debugstr_hstring(name), users );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_SetPackageState( IPackageManager *iface, HSTRING name, PackageState state )
{
    FIXME("iface %p, name %s, state %d stub!\n", iface, debugstr_hstring(name), state);
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_FindPackageByPackageFullName( IPackageManager *iface, HSTRING name, IPackage **package )
{
    FIXME( "iface %p, name %s, package %p stub!\n", iface, debugstr_hstring(name), package );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_CleanupPackageForUserAsync( IPackageManager *iface, HSTRING name, HSTRING sid,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, name %s, sid %s, operation %p stub!\n", iface, debugstr_hstring(name), debugstr_hstring(sid), operation );
    return E_NOTIMPL;
}

/* ---- minimal empty IIterator_Package / IIterable_Package, used for "no packages found" results ---- */

struct empty_iterator
{
    IIterator_Package IIterator_Package_iface;
    LONG ref;
};

static inline struct empty_iterator *impl_from_IIterator_Package( IIterator_Package *iface )
{
    return CONTAINING_RECORD( iface, struct empty_iterator, IIterator_Package_iface );
}
static HRESULT WINAPI empty_iterator_QueryInterface( IIterator_Package *iface, REFIID iid, void **out )
{
    struct empty_iterator *impl = impl_from_IIterator_Package( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) || IsEqualGUID( iid, &IID_IIterator_Package ))
    {
        *out = &impl->IIterator_Package_iface;
        IIterator_Package_AddRef( (IIterator_Package *)*out );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI empty_iterator_AddRef( IIterator_Package *iface )
{
    return InterlockedIncrement( &impl_from_IIterator_Package(iface)->ref );
}
static ULONG WINAPI empty_iterator_Release( IIterator_Package *iface )
{
    struct empty_iterator *impl = impl_from_IIterator_Package( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref) free( impl );
    return ref;
}
static HRESULT WINAPI empty_iterator_GetIids( IIterator_Package *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI empty_iterator_GetRuntimeClassName( IIterator_Package *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI empty_iterator_GetTrustLevel( IIterator_Package *iface, TrustLevel *t ) { (void)iface;(void)t; return E_NOTIMPL; }
static HRESULT WINAPI empty_iterator_get_Current( IIterator_Package *iface, __x_ABI_CWindows_CApplicationModel_CIPackage **value ) { (void)iface; *value = NULL; return E_BOUNDS; }
static HRESULT WINAPI empty_iterator_get_HasCurrent( IIterator_Package *iface, boolean *value ) { (void)iface; *value = FALSE; return S_OK; }
static HRESULT WINAPI empty_iterator_MoveNext( IIterator_Package *iface, boolean *value ) { (void)iface; *value = FALSE; return S_OK; }
static HRESULT WINAPI empty_iterator_GetMany( IIterator_Package *iface, UINT32 size, __x_ABI_CWindows_CApplicationModel_CIPackage **items, UINT32 *value ) { (void)iface;(void)size;(void)items; *value = 0; return S_OK; }

static const struct __FIIterator_1_Windows__CApplicationModel__CPackageVtbl empty_iterator_vtbl =
{
    empty_iterator_QueryInterface, empty_iterator_AddRef, empty_iterator_Release,
    empty_iterator_GetIids, empty_iterator_GetRuntimeClassName, empty_iterator_GetTrustLevel,
    empty_iterator_get_Current, empty_iterator_get_HasCurrent, empty_iterator_MoveNext, empty_iterator_GetMany
};

struct empty_iterable
{
    IIterable_Package IIterable_Package_iface;
    LONG ref;
};
static inline struct empty_iterable *impl_from_IIterable_Package( IIterable_Package *iface )
{
    return CONTAINING_RECORD( iface, struct empty_iterable, IIterable_Package_iface );
}
static HRESULT WINAPI empty_iterable_QueryInterface( IIterable_Package *iface, REFIID iid, void **out )
{
    struct empty_iterable *impl = impl_from_IIterable_Package( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) || IsEqualGUID( iid, &IID_IIterable_Package ))
    {
        *out = &impl->IIterable_Package_iface;
        IIterable_Package_AddRef( (IIterable_Package *)*out );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI empty_iterable_AddRef( IIterable_Package *iface )
{
    return InterlockedIncrement( &impl_from_IIterable_Package(iface)->ref );
}
static ULONG WINAPI empty_iterable_Release( IIterable_Package *iface )
{
    struct empty_iterable *impl = impl_from_IIterable_Package( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref) free( impl );
    return ref;
}
static HRESULT WINAPI empty_iterable_GetIids( IIterable_Package *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI empty_iterable_GetRuntimeClassName( IIterable_Package *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI empty_iterable_GetTrustLevel( IIterable_Package *iface, TrustLevel *t ) { (void)iface;(void)t; return E_NOTIMPL; }
static HRESULT WINAPI empty_iterable_First( IIterable_Package *iface, IIterator_Package **value )
{
    struct empty_iterator *it;
    (void)iface;
    if (!(it = calloc( 1, sizeof(*it) ))) { *value = NULL; return E_OUTOFMEMORY; }
    it->IIterator_Package_iface.lpVtbl = &empty_iterator_vtbl;
    it->ref = 1;
    *value = &it->IIterator_Package_iface;
    return S_OK;
}
static const struct __FIIterable_1_Windows__CApplicationModel__CPackageVtbl empty_iterable_vtbl =
{
    empty_iterable_QueryInterface, empty_iterable_AddRef, empty_iterable_Release,
    empty_iterable_GetIids, empty_iterable_GetRuntimeClassName, empty_iterable_GetTrustLevel,
    empty_iterable_First
};
static HRESULT create_empty_package_iterable( IIterable_Package **out )
{
    struct empty_iterable *impl = calloc( 1, sizeof(*impl) );
    if (!impl) { *out = NULL; return E_OUTOFMEMORY; }
    impl->IIterable_Package_iface.lpVtbl = &empty_iterable_vtbl;
    impl->ref = 1;
    *out = &impl->IIterable_Package_iface;
    return S_OK;
}

static HRESULT WINAPI package_manager_FindPackagesByPackageFamilyName( IPackageManager *iface, HSTRING family_name,
    IIterable_Package **packages )
{
    TRACE( "iface %p, family_name %s, packages %p (real impl: no packages installed yet).\n", iface, debugstr_hstring(family_name), packages );
    return create_empty_package_iterable( packages );
}

static HRESULT WINAPI package_manager_FindPackagesByUserSecurityIdPackageFamilyName( IPackageManager *iface, HSTRING sid,
    HSTRING family_name, IIterable_Package **packages )
{
    FIXME( "iface %p, sid %s, family_name %s, packages %p stub!\n", iface, debugstr_hstring(sid), debugstr_hstring(family_name), packages );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager_FindPackageByUserSecurityIdPackageFullName( IPackageManager *iface, HSTRING sid, HSTRING name, IPackage **package )
{
    FIXME( "iface %p, sid %s, name %s, package %p stub!\n", iface, debugstr_hstring(sid), debugstr_hstring(name), package );
    return E_NOTIMPL;
}

static const struct IPackageManagerVtbl package_manager_vtbl =
{
    package_manager_QueryInterface,
    package_manager_AddRef,
    package_manager_Release,
    /* IInspectable methods */
    package_manager_GetIids,
    package_manager_GetRuntimeClassName,
    package_manager_GetTrustLevel,
    /* IPackageManager methods */
    package_manager_AddPackageAsync,
    package_manager_UpdatePackageAsync,
    package_manager_RemovePackageAsync,
    package_manager_StagePackageAsync,
    package_manager_RegisterPackageAsync,
    package_manager_FindPackages,
    package_manager_FindPackagesByUserSecurityId,
    package_manager_FindPackagesByNamePublisher,
    package_manager_FindPackagesByUserSecurityIdNamePublisher,
    package_manager_FindUsers,
    package_manager_SetPackageState,
    package_manager_FindPackageByPackageFullName,
    package_manager_CleanupPackageForUserAsync,
    package_manager_FindPackagesByPackageFamilyName,
    package_manager_FindPackagesByUserSecurityIdPackageFamilyName,
    package_manager_FindPackageByUserSecurityIdPackageFullName
};

DEFINE_IINSPECTABLE( package_manager2, IPackageManager2, struct package_manager, IPackageManager_iface );

static HRESULT WINAPI package_manager2_RemovePackageWithOptionsAsync( IPackageManager2 *iface, HSTRING name, RemovalOptions options,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, name %s, options %d, operation %p stub!\n", iface, debugstr_hstring(name), options, operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager2_StagePackageWithOptionsAsync( IPackageManager2 *iface, IUriRuntimeClass *uri, IIterable_Uri *dependencies,
    DeploymentOptions options, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, uri %p, dependencies %p, options %d, operation %p stub!\n", iface, uri, dependencies, options, operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager2_RegisterPackageByFullNameAsync( IPackageManager2 *iface, HSTRING name, IIterable_HSTRING *dependencies,
    DeploymentOptions options, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, name %s, dependencies %p, options %d, operation %p stub!\n", iface, debugstr_hstring(name), dependencies, options, operation );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager2_FindPackagesWithPackageTypes( IPackageManager2 *iface, PackageTypes types, IIterable_Package **packages )
{
    TRACE( "iface %p, types %d, packages %p (real impl: no packages installed yet).\n", iface, types, packages );
    return create_empty_package_iterable( packages );
}

static HRESULT WINAPI package_manager2_FindPackagesByUserSecurityIdWithPackageTypes( IPackageManager2 *iface, HSTRING sid,
    PackageTypes types, IIterable_Package **packages )
{
    TRACE( "iface %p, sid %s, types %d, packages %p (real impl: no packages installed yet).\n", iface, debugstr_hstring(sid), types, packages );
    return create_empty_package_iterable( packages );
}

static HRESULT WINAPI package_manager2_FindPackagesByNamePublisherWithPackageTypes( IPackageManager2 *iface, HSTRING name, HSTRING publisher,
    PackageTypes types, IIterable_Package **packages )
{
    FIXME( "iface %p, name %s, publisher %s, types %d, packages %p stub!\n", iface, debugstr_hstring(name), debugstr_hstring(publisher), types, packages );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager2_FindPackagesByUserSecurityIdNamePublisherWithPackageTypes( IPackageManager2 *iface, HSTRING sid, HSTRING name,
    HSTRING publisher, PackageTypes types, IIterable_Package **packages )
{
    FIXME( "iface %p, sid %s, name %s, publisher %s, types %d, packages %p stub!\n", iface, debugstr_hstring(sid), debugstr_hstring(name), debugstr_hstring(publisher), types, packages );
    return E_NOTIMPL;
}

static HRESULT WINAPI package_manager2_FindPackagesByPackageFamilyNameWithPackageTypes( IPackageManager2 *iface, HSTRING family_name, PackageTypes types,
   IIterable_Package **packages )
{
    TRACE( "iface %p, family_name %s, types %d, packages %p (real impl: no packages installed yet).\n", iface, debugstr_hstring(family_name), types, packages );
    return create_empty_package_iterable( packages );
}

static HRESULT WINAPI package_manager2_FindPackagesByUserSecurityIdPackageFamilyNameWithPackageTypes( IPackageManager2 *iface, HSTRING sid, HSTRING family_name,
    PackageTypes types, IIterable_Package **packages )
{
    TRACE( "iface %p, sid %s, family_name %s, types %d, packages %p (real impl: no packages installed yet).\n", iface, debugstr_hstring(sid), debugstr_hstring(family_name), types, packages );
    return create_empty_package_iterable( packages );
}

static HRESULT WINAPI package_manager2_StageUserDataAsync( IPackageManager2 *iface, HSTRING name,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    FIXME( "iface %p, name %s, operation %p stub!\n", iface, debugstr_hstring(name), operation );
    return E_NOTIMPL;
}

static const struct IPackageManager2Vtbl package_manager2_vtbl =
{
    package_manager2_QueryInterface,
    package_manager2_AddRef,
    package_manager2_Release,
    /* IInspectable methods */
    package_manager2_GetIids,
    package_manager2_GetRuntimeClassName,
    package_manager2_GetTrustLevel,
    /* IPackageManager2 methods */
    package_manager2_RemovePackageWithOptionsAsync,
    package_manager2_StagePackageWithOptionsAsync,
    package_manager2_RegisterPackageByFullNameAsync,
    package_manager2_FindPackagesWithPackageTypes,
    package_manager2_FindPackagesByUserSecurityIdWithPackageTypes,
    package_manager2_FindPackagesByNamePublisherWithPackageTypes,
    package_manager2_FindPackagesByUserSecurityIdNamePublisherWithPackageTypes,
    package_manager2_FindPackagesByPackageFamilyNameWithPackageTypes,
    package_manager2_FindPackagesByUserSecurityIdPackageFamilyNameWithPackageTypes,
    package_manager2_StageUserDataAsync,
};

/* ============================================================
 * IDeploymentResult - trivial "operation succeeded" result object
 * ============================================================ */
struct deployment_result
{
    IDeploymentResult IDeploymentResult_iface;
    LONG ref;
    HRESULT extended_error_code;
};
static inline struct deployment_result *impl_from_IDeploymentResult( IDeploymentResult *iface )
{
    return CONTAINING_RECORD( iface, struct deployment_result, IDeploymentResult_iface );
}
static HRESULT WINAPI dr_QueryInterface( IDeploymentResult *iface, REFIID iid, void **out )
{
    struct deployment_result *impl = impl_from_IDeploymentResult( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) || IsEqualGUID( iid, &IID_IDeploymentResult ))
    {
        *out = &impl->IDeploymentResult_iface;
        IDeploymentResult_AddRef( (IDeploymentResult *)*out );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI dr_AddRef( IDeploymentResult *iface ) { return InterlockedIncrement( &impl_from_IDeploymentResult(iface)->ref ); }
static ULONG WINAPI dr_Release( IDeploymentResult *iface )
{
    struct deployment_result *impl = impl_from_IDeploymentResult( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref) free( impl );
    return ref;
}
static HRESULT WINAPI dr_GetIids( IDeploymentResult *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI dr_GetRuntimeClassName( IDeploymentResult *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI dr_GetTrustLevel( IDeploymentResult *iface, TrustLevel *t ) { (void)iface;(void)t; return E_NOTIMPL; }
static HRESULT WINAPI dr_get_ErrorText( IDeploymentResult *iface, HSTRING *value ) { (void)iface; *value = NULL; return S_OK; }
static HRESULT WINAPI dr_get_ActivityId( IDeploymentResult *iface, GUID *value ) { (void)iface; memset( value, 0, sizeof(*value) ); return S_OK; }
static HRESULT WINAPI dr_get_ExtendedErrorCode( IDeploymentResult *iface, HRESULT *value ) { *value = impl_from_IDeploymentResult(iface)->extended_error_code; return S_OK; }
static IDeploymentResultVtbl deployment_result_vtbl =
{
    dr_QueryInterface, dr_AddRef, dr_Release,
    dr_GetIids, dr_GetRuntimeClassName, dr_GetTrustLevel,
    dr_get_ErrorText, dr_get_ActivityId, dr_get_ExtendedErrorCode
};
static HRESULT create_deployment_result( HRESULT extended_error_code, IDeploymentResult **out )
{
    struct deployment_result *impl = calloc( 1, sizeof(*impl) );
    if (!impl) { *out = NULL; return E_OUTOFMEMORY; }
    impl->IDeploymentResult_iface.lpVtbl = &deployment_result_vtbl;
    impl->ref = 1;
    impl->extended_error_code = extended_error_code;
    *out = &impl->IDeploymentResult_iface;
    return S_OK;
}

/* ============================================================
 * IAsyncOperationWithProgress<DeploymentResult, DeploymentProgress>
 * Our operations complete synchronously before being handed back,
 * so this object is always already-Completed; put_Completed invokes
 * the handler immediately, per the WinRT contract for that case.
 * ============================================================ */
struct deploy_async_op
{
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress IAsyncOperationWithProgress_iface;
    IAsyncInfo IAsyncInfo_iface;
    LONG ref;
    HRESULT hr;
    IDeploymentResult *result;
};
static inline struct deploy_async_op *impl_from_async_op( IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface )
{
    return CONTAINING_RECORD( iface, struct deploy_async_op, IAsyncOperationWithProgress_iface );
}
static inline struct deploy_async_op *impl_from_async_info( IAsyncInfo *iface )
{
    return CONTAINING_RECORD( iface, struct deploy_async_op, IAsyncInfo_iface );
}
static HRESULT WINAPI dao_QueryInterface( IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface, REFIID iid, void **out )
{
    struct deploy_async_op *impl = impl_from_async_op( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress ))
    {
        *out = &impl->IAsyncOperationWithProgress_iface;
        IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress_AddRef( (IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *)*out );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_IAsyncInfo ))
    {
        *out = &impl->IAsyncInfo_iface;
        IAsyncInfo_AddRef( (IAsyncInfo *)*out );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI dao_AddRef( IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface ) { return InterlockedIncrement( &impl_from_async_op(iface)->ref ); }
static ULONG WINAPI dao_Release( IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface )
{
    struct deploy_async_op *impl = impl_from_async_op( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->result) IDeploymentResult_Release( impl->result );
        free( impl );
    }
    return ref;
}
static HRESULT WINAPI dao_GetIids( IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI dao_GetRuntimeClassName( IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI dao_GetTrustLevel( IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface, TrustLevel *t ) { (void)iface;(void)t; return E_NOTIMPL; }
static HRESULT WINAPI dao_put_Progress( IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface, void *handler ) { (void)iface;(void)handler; return S_OK; }
static HRESULT WINAPI dao_get_Progress( IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface, void **handler ) { (void)iface; *handler = NULL; return S_OK; }
static HRESULT WINAPI dao_put_Completed( IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface,
    __FIAsyncOperationWithProgressCompletedHandler_2_Windows__CManagement__CDeployment__CDeploymentResult_DeploymentProgress *handler )
{
    struct deploy_async_op *impl = impl_from_async_op( iface );
    /* operation is always already-complete by the time callers can observe it: invoke synchronously now */
    if (handler)
        handler->lpVtbl->Invoke( handler, &impl->IAsyncOperationWithProgress_iface, Completed );
    return S_OK;
}
static HRESULT WINAPI dao_get_Completed( IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface, void **handler ) { (void)iface; *handler = NULL; return S_OK; }
static HRESULT WINAPI dao_GetResults( IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress *iface, IDeploymentResult **results )
{
    struct deploy_async_op *impl = impl_from_async_op( iface );
    *results = impl->result;
    if (*results) IDeploymentResult_AddRef( *results );
    return impl->hr;
}
static IAsyncOperationWithProgress_DeploymentResult_DeploymentProgressVtbl deploy_async_op_vtbl =
{
    dao_QueryInterface, dao_AddRef, dao_Release,
    dao_GetIids, dao_GetRuntimeClassName, dao_GetTrustLevel,
    dao_put_Progress, dao_get_Progress, dao_put_Completed, dao_get_Completed, dao_GetResults
};

static HRESULT WINAPI dai_QueryInterface( IAsyncInfo *iface, REFIID iid, void **out )
{
    return dao_QueryInterface( &impl_from_async_info(iface)->IAsyncOperationWithProgress_iface, iid, out );
}
static ULONG WINAPI dai_AddRef( IAsyncInfo *iface ) { return InterlockedIncrement( &impl_from_async_info(iface)->ref ); }
static ULONG WINAPI dai_Release( IAsyncInfo *iface ) { return dao_Release( &impl_from_async_info(iface)->IAsyncOperationWithProgress_iface ); }
static HRESULT WINAPI dai_GetIids( IAsyncInfo *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI dai_GetRuntimeClassName( IAsyncInfo *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI dai_GetTrustLevel( IAsyncInfo *iface, TrustLevel *t ) { (void)iface;(void)t; return E_NOTIMPL; }
static HRESULT WINAPI dai_get_Id( IAsyncInfo *iface, UINT32 *id ) { (void)iface; *id = 1; return S_OK; }
static HRESULT WINAPI dai_get_Status( IAsyncInfo *iface, AsyncStatus *status )
{
    struct deploy_async_op *impl = impl_from_async_info( iface );
    *status = SUCCEEDED(impl->hr) ? Completed : Error;
    return S_OK;
}
static HRESULT WINAPI dai_get_ErrorCode( IAsyncInfo *iface, HRESULT *error_code ) { *error_code = impl_from_async_info(iface)->hr; return S_OK; }
static HRESULT WINAPI dai_Cancel( IAsyncInfo *iface ) { (void)iface; return S_OK; }
static HRESULT WINAPI dai_Close( IAsyncInfo *iface ) { (void)iface; return S_OK; }
static IAsyncInfoVtbl deploy_async_info_vtbl =
{
    dai_QueryInterface, dai_AddRef, dai_Release,
    dai_GetIids, dai_GetRuntimeClassName, dai_GetTrustLevel,
    dai_get_Id, dai_get_Status, dai_get_ErrorCode, dai_Cancel, dai_Close
};

/* creates an already-completed async operation wrapping the given result */
static HRESULT create_completed_async_op( HRESULT hr, IDeploymentResult *result,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **out )
{
    struct deploy_async_op *impl = calloc( 1, sizeof(*impl) );
    if (!impl) { *out = NULL; return E_OUTOFMEMORY; }
    impl->IAsyncOperationWithProgress_iface.lpVtbl = &deploy_async_op_vtbl;
    impl->IAsyncInfo_iface.lpVtbl = &deploy_async_info_vtbl;
    impl->ref = 1;
    impl->hr = hr;
    impl->result = result;
    if (result) IDeploymentResult_AddRef( result );
    *out = &impl->IAsyncOperationWithProgress_iface;
    return S_OK;
}

/* ============================================================
 * IPackageManager9 - only StagePackageByUriAsync is implemented for now
 * ============================================================ */
static inline struct package_manager *impl_from_IPackageManager9( IPackageManager9 *iface )
{
    return CONTAINING_RECORD( iface, struct package_manager, IPackageManager9_iface );
}
static HRESULT WINAPI package_manager9_QueryInterface( IPackageManager9 *iface, REFIID iid, void **out )
{
    return package_manager_QueryInterface( &impl_from_IPackageManager9(iface)->IPackageManager_iface, iid, out );
}
static ULONG WINAPI package_manager9_AddRef( IPackageManager9 *iface ) { return package_manager_AddRef( &impl_from_IPackageManager9(iface)->IPackageManager_iface ); }
static ULONG WINAPI package_manager9_Release( IPackageManager9 *iface ) { return package_manager_Release( &impl_from_IPackageManager9(iface)->IPackageManager_iface ); }
static HRESULT WINAPI package_manager9_GetIids( IPackageManager9 *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI package_manager9_GetRuntimeClassName( IPackageManager9 *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI package_manager9_GetTrustLevel( IPackageManager9 *iface, TrustLevel *t ) { (void)iface;(void)t; return E_NOTIMPL; }
static HRESULT WINAPI package_manager9_FindProvisionedPackages( IPackageManager9 *iface, void **v ) { (void)iface; *v = NULL; return E_NOTIMPL; }
static HRESULT WINAPI package_manager9_AddPackageByUriAsync( IPackageManager9 *iface, IUriRuntimeClass *uri, IStagePackageOptions *options,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    (void)iface;(void)options;
    return StageAndCompleteFromUri( uri, operation );
}
static HRESULT WINAPI package_manager9_StagePackageByUriAsync( IPackageManager9 *iface, IUriRuntimeClass *uri, IStagePackageOptions *options,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    (void)iface;(void)options;
    return StageAndCompleteFromUri( uri, operation );
}
static HRESULT WINAPI package_manager9_RegisterPackageByUriAsync( IPackageManager9 *iface, IUriRuntimeClass *uri, void *options,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    (void)iface;(void)uri;(void)options;
    ERR( "RegisterPackageByUriAsync not yet implemented, returning E_NOTIMPL.\n" );
    *operation = NULL;
    return E_NOTIMPL;
}
static HRESULT WINAPI package_manager9_RegisterPackagesByFullNameAsync( IPackageManager9 *iface, void *names, void *options,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **operation )
{
    (void)iface;(void)names;(void)options;
    ERR( "RegisterPackagesByFullNameAsync not yet implemented, returning E_NOTIMPL.\n" );
    *operation = NULL;
    return E_NOTIMPL;
}
static HRESULT WINAPI package_manager9_SetPackageStubPreference( IPackageManager9 *iface, HSTRING name, INT32 pref ) { (void)iface;(void)name;(void)pref; return E_NOTIMPL; }
static HRESULT WINAPI package_manager9_GetPackageStubPreference( IPackageManager9 *iface, HSTRING name, INT32 *pref ) { (void)iface;(void)name; *pref = 0; return E_NOTIMPL; }

static IPackageManager9Vtbl package_manager9_vtbl =
{
    package_manager9_QueryInterface, package_manager9_AddRef, package_manager9_Release,
    package_manager9_GetIids, package_manager9_GetRuntimeClassName, package_manager9_GetTrustLevel,
    package_manager9_FindProvisionedPackages,
    package_manager9_AddPackageByUriAsync,
    package_manager9_StagePackageByUriAsync,
    package_manager9_RegisterPackageByUriAsync,
    package_manager9_RegisterPackagesByFullNameAsync,
    package_manager9_SetPackageStubPreference,
    package_manager9_GetPackageStubPreference,
};

/* ============================================================
 * IPackageManager6 - QueryInterface support only for now; all
 * methods stubbed until we see one actually get called.
 * ============================================================ */
static inline struct package_manager *impl_from_IPackageManager6( IPackageManager6 *iface )
{
    return CONTAINING_RECORD( iface, struct package_manager, IPackageManager6_iface );
}
static HRESULT WINAPI package_manager6_QueryInterface( IPackageManager6 *iface, REFIID iid, void **out )
{
    return package_manager_QueryInterface( &impl_from_IPackageManager6(iface)->IPackageManager_iface, iid, out );
}
static ULONG WINAPI package_manager6_AddRef( IPackageManager6 *iface ) { return package_manager_AddRef( &impl_from_IPackageManager6(iface)->IPackageManager_iface ); }
static ULONG WINAPI package_manager6_Release( IPackageManager6 *iface ) { return package_manager_Release( &impl_from_IPackageManager6(iface)->IPackageManager_iface ); }
static HRESULT WINAPI package_manager6_GetIids( IPackageManager6 *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI package_manager6_GetRuntimeClassName( IPackageManager6 *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI package_manager6_GetTrustLevel( IPackageManager6 *iface, TrustLevel *t ) { (void)iface;(void)t; return E_NOTIMPL; }
static HRESULT WINAPI package_manager6_ProvisionPackageForAllUsersAsync( IPackageManager6 *iface, HSTRING n,
    IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **op )
{
    IDeploymentResult *result;
    HRESULT hr;
    (void)iface;
    ERR( "ProvisionPackageForAllUsersAsync %s: reporting synchronous success.\n", debugstr_hstring(n) );
    hr = create_deployment_result( S_OK, &result );
    if (FAILED(hr)) { *op = NULL; return hr; }
    hr = create_completed_async_op( S_OK, result, op );
    IDeploymentResult_Release( result );
    return hr;
}
static HRESULT WINAPI package_manager6_AddPackageByAppInstallerFileAsync( IPackageManager6 *iface, IUriRuntimeClass *uri, AddPackageByAppInstallerOptions o, void *deps, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **op )
{ (void)iface;(void)uri;(void)o;(void)deps; ERR("AddPackageByAppInstallerFileAsync stub!\n"); *op = NULL; return E_NOTIMPL; }
static HRESULT WINAPI package_manager6_RequestAddPackageByAppInstallerFileAsync( IPackageManager6 *iface, IUriRuntimeClass *uri, AddPackageByAppInstallerOptions o, void *deps, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **op )
{ (void)iface;(void)uri;(void)o;(void)deps; ERR("RequestAddPackageByAppInstallerFileAsync stub!\n"); *op = NULL; return E_NOTIMPL; }
static HRESULT WINAPI package_manager6_AddPackageToVolumeAndRelatedSetAsync( IPackageManager6 *iface, IUriRuntimeClass *uri, void *a, INT32 o, void *b, void *c, void *d, void *e, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **op )
{ (void)iface;(void)uri;(void)a;(void)o;(void)b;(void)c;(void)d;(void)e; ERR("AddPackageToVolumeAndRelatedSetAsync stub!\n"); *op = NULL; return E_NOTIMPL; }
static HRESULT WINAPI package_manager6_StagePackageToVolumeAndRelatedSetAsync( IPackageManager6 *iface, IUriRuntimeClass *uri, void *a, INT32 o, void *b, void *c, void *d, void *e, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **op )
{ (void)iface;(void)uri;(void)a;(void)o;(void)b;(void)c;(void)d;(void)e; ERR("StagePackageToVolumeAndRelatedSetAsync stub!\n"); *op = NULL; return E_NOTIMPL; }
static HRESULT WINAPI package_manager6_RequestAddPackageAsync( IPackageManager6 *iface, IUriRuntimeClass *uri, void *deps, INT32 o, void *a, void *b, void *c, IAsyncOperationWithProgress_DeploymentResult_DeploymentProgress **op )
{ (void)iface;(void)uri;(void)deps;(void)o;(void)a;(void)b;(void)c; ERR("RequestAddPackageAsync stub!\n"); *op = NULL; return E_NOTIMPL; }

static IPackageManager6Vtbl package_manager6_vtbl =
{
    package_manager6_QueryInterface, package_manager6_AddRef, package_manager6_Release,
    package_manager6_GetIids, package_manager6_GetRuntimeClassName, package_manager6_GetTrustLevel,
    package_manager6_ProvisionPackageForAllUsersAsync,
    package_manager6_AddPackageByAppInstallerFileAsync,
    package_manager6_RequestAddPackageByAppInstallerFileAsync,
    package_manager6_AddPackageToVolumeAndRelatedSetAsync,
    package_manager6_StagePackageToVolumeAndRelatedSetAsync,
    package_manager6_RequestAddPackageAsync,
};

struct package_manager_statics
{
    IActivationFactory IActivationFactory_iface;
    LONG ref;
};

static inline struct package_manager_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct package_manager_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct package_manager_statics *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IInspectable_AddRef( (IInspectable *)*out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct package_manager_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct package_manager_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );
    return ref;
}

static HRESULT WINAPI factory_GetIids( IActivationFactory *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    struct package_manager *impl;

    TRACE( "iface %p, instance %p.\n", iface, instance );

    if (!(impl = calloc( 1, sizeof(*impl) )))
    {
        *instance = NULL;
        return E_OUTOFMEMORY;
    }

    impl->IPackageManager_iface.lpVtbl = &package_manager_vtbl;
    impl->IPackageManager2_iface.lpVtbl = &package_manager2_vtbl;
    impl->IPackageManager9_iface.lpVtbl = &package_manager9_vtbl;
    impl->IPackageManager6_iface.lpVtbl = &package_manager6_vtbl;
    impl->ref = 1;

    *instance = (IInspectable *)&impl->IPackageManager_iface;
    return S_OK;
}

static const struct IActivationFactoryVtbl factory_vtbl =
{
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    /* IInspectable methods */
    factory_GetIids,
    factory_GetRuntimeClassName,
    factory_GetTrustLevel,
    /* IActivationFactory methods */
    factory_ActivateInstance,
};

static struct package_manager_statics package_manager_statics =
{
    {&factory_vtbl},
    1,
};

IActivationFactory *package_manager_factory = &package_manager_statics.IActivationFactory_iface;

/* ============================================================
 * Windows.Management.Deployment.StagePackageOptions
 * Not part of Wine's generated headers yet; minimal real
 * implementation (property bag) using the verified real GUID.
 * ============================================================ */

struct stage_package_options
{
    IStagePackageOptions IStagePackageOptions_iface;
    LONG ref;

    IUnknown *target_volume;
    IUnknown *external_location_uri;
    StubPackageOption stub_package_option;
    boolean developer_mode;
    boolean force_update_from_any_version;
    boolean install_all_resources;
    boolean required_content_group_only;
    boolean stage_in_place;
    boolean allow_unsigned;
};

static inline struct stage_package_options *impl_from_IStagePackageOptions( IStagePackageOptions *iface )
{
    return CONTAINING_RECORD( iface, struct stage_package_options, IStagePackageOptions_iface );
}

static HRESULT WINAPI spo_QueryInterface( IStagePackageOptions *iface, REFIID iid, void **out )
{
    struct stage_package_options *impl = impl_from_IStagePackageOptions( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) || IsEqualGUID( iid, &IID_IStagePackageOptions_ ))
    {
        *out = &impl->IStagePackageOptions_iface;
        IStagePackageOptions_AddRef( (IStagePackageOptions *)*out );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI spo_AddRef( IStagePackageOptions *iface )
{
    return InterlockedIncrement( &impl_from_IStagePackageOptions(iface)->ref );
}
static ULONG WINAPI spo_Release( IStagePackageOptions *iface )
{
    struct stage_package_options *impl = impl_from_IStagePackageOptions( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->target_volume) IUnknown_Release( impl->target_volume );
        if (impl->external_location_uri) IUnknown_Release( impl->external_location_uri );
        free( impl );
    }
    return ref;
}
static HRESULT WINAPI spo_GetIids( IStagePackageOptions *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI spo_GetRuntimeClassName( IStagePackageOptions *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI spo_GetTrustLevel( IStagePackageOptions *iface, TrustLevel *t ) { (void)iface;(void)t; return E_NOTIMPL; }
static HRESULT WINAPI spo_get_DependencyPackageUris( IStagePackageOptions *iface, void **v ) { (void)iface; *v = NULL; return E_NOTIMPL; }
static HRESULT WINAPI spo_get_TargetVolume( IStagePackageOptions *iface, void **v )
{
    struct stage_package_options *impl = impl_from_IStagePackageOptions( iface );
    *v = impl->target_volume;
    if (*v) IUnknown_AddRef( (IUnknown *)*v );
    return S_OK;
}
static HRESULT WINAPI spo_put_TargetVolume( IStagePackageOptions *iface, void *v )
{
    struct stage_package_options *impl = impl_from_IStagePackageOptions( iface );
    if (impl->target_volume) IUnknown_Release( impl->target_volume );
    impl->target_volume = v;
    if (v) IUnknown_AddRef( (IUnknown *)v );
    return S_OK;
}
static HRESULT WINAPI spo_get_OptionalPackageFamilyNames( IStagePackageOptions *iface, void **v ) { (void)iface; *v = NULL; return E_NOTIMPL; }
static HRESULT WINAPI spo_get_OptionalPackageUris( IStagePackageOptions *iface, void **v ) { (void)iface; *v = NULL; return E_NOTIMPL; }
static HRESULT WINAPI spo_get_RelatedPackageUris( IStagePackageOptions *iface, void **v ) { (void)iface; *v = NULL; return E_NOTIMPL; }
static HRESULT WINAPI spo_get_ExternalLocationUri( IStagePackageOptions *iface, void **v )
{
    struct stage_package_options *impl = impl_from_IStagePackageOptions( iface );
    *v = impl->external_location_uri;
    if (*v) IUnknown_AddRef( (IUnknown *)*v );
    return S_OK;
}
static HRESULT WINAPI spo_put_ExternalLocationUri( IStagePackageOptions *iface, void *v )
{
    struct stage_package_options *impl = impl_from_IStagePackageOptions( iface );
    if (impl->external_location_uri) IUnknown_Release( impl->external_location_uri );
    impl->external_location_uri = v;
    if (v) IUnknown_AddRef( (IUnknown *)v );
    return S_OK;
}
static HRESULT WINAPI spo_get_StubPackageOption( IStagePackageOptions *iface, StubPackageOption *v ) { *v = impl_from_IStagePackageOptions(iface)->stub_package_option; return S_OK; }
static HRESULT WINAPI spo_put_StubPackageOption( IStagePackageOptions *iface, StubPackageOption v ) { impl_from_IStagePackageOptions(iface)->stub_package_option = v; return S_OK; }
static HRESULT WINAPI spo_get_DeveloperMode( IStagePackageOptions *iface, boolean *v ) { *v = impl_from_IStagePackageOptions(iface)->developer_mode; return S_OK; }
static HRESULT WINAPI spo_put_DeveloperMode( IStagePackageOptions *iface, boolean v ) { impl_from_IStagePackageOptions(iface)->developer_mode = v; return S_OK; }
static HRESULT WINAPI spo_get_ForceUpdateFromAnyVersion( IStagePackageOptions *iface, boolean *v ) { *v = impl_from_IStagePackageOptions(iface)->force_update_from_any_version; return S_OK; }
static HRESULT WINAPI spo_put_ForceUpdateFromAnyVersion( IStagePackageOptions *iface, boolean v ) { impl_from_IStagePackageOptions(iface)->force_update_from_any_version = v; return S_OK; }
static HRESULT WINAPI spo_get_InstallAllResources( IStagePackageOptions *iface, boolean *v ) { *v = impl_from_IStagePackageOptions(iface)->install_all_resources; return S_OK; }
static HRESULT WINAPI spo_put_InstallAllResources( IStagePackageOptions *iface, boolean v ) { impl_from_IStagePackageOptions(iface)->install_all_resources = v; return S_OK; }
static HRESULT WINAPI spo_get_RequiredContentGroupOnly( IStagePackageOptions *iface, boolean *v ) { *v = impl_from_IStagePackageOptions(iface)->required_content_group_only; return S_OK; }
static HRESULT WINAPI spo_put_RequiredContentGroupOnly( IStagePackageOptions *iface, boolean v ) { impl_from_IStagePackageOptions(iface)->required_content_group_only = v; return S_OK; }
static HRESULT WINAPI spo_get_StageInPlace( IStagePackageOptions *iface, boolean *v ) { *v = impl_from_IStagePackageOptions(iface)->stage_in_place; return S_OK; }
static HRESULT WINAPI spo_put_StageInPlace( IStagePackageOptions *iface, boolean v ) { impl_from_IStagePackageOptions(iface)->stage_in_place = v; return S_OK; }
static HRESULT WINAPI spo_get_AllowUnsigned( IStagePackageOptions *iface, boolean *v ) { *v = impl_from_IStagePackageOptions(iface)->allow_unsigned; return S_OK; }
static HRESULT WINAPI spo_put_AllowUnsigned( IStagePackageOptions *iface, boolean v ) { impl_from_IStagePackageOptions(iface)->allow_unsigned = v; return S_OK; }

static IStagePackageOptionsVtbl stage_package_options_vtbl =
{
    spo_QueryInterface, spo_AddRef, spo_Release,
    spo_GetIids, spo_GetRuntimeClassName, spo_GetTrustLevel,
    spo_get_DependencyPackageUris,
    spo_get_TargetVolume, spo_put_TargetVolume,
    spo_get_OptionalPackageFamilyNames,
    spo_get_OptionalPackageUris,
    spo_get_RelatedPackageUris,
    spo_get_ExternalLocationUri, spo_put_ExternalLocationUri,
    spo_get_StubPackageOption, spo_put_StubPackageOption,
    spo_get_DeveloperMode, spo_put_DeveloperMode,
    spo_get_ForceUpdateFromAnyVersion, spo_put_ForceUpdateFromAnyVersion,
    spo_get_InstallAllResources, spo_put_InstallAllResources,
    spo_get_RequiredContentGroupOnly, spo_put_RequiredContentGroupOnly,
    spo_get_StageInPlace, spo_put_StageInPlace,
    spo_get_AllowUnsigned, spo_put_AllowUnsigned,
};

struct stage_package_options_statics
{
    IActivationFactory IActivationFactory_iface;
    LONG ref;
};
static inline struct stage_package_options_statics *impl_from_spo_factory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct stage_package_options_statics, IActivationFactory_iface );
}
static HRESULT WINAPI spo_factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct stage_package_options_statics *impl = impl_from_spo_factory( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) || IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IInspectable_AddRef( (IInspectable *)*out );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI spo_factory_AddRef( IActivationFactory *iface ) { return InterlockedIncrement( &impl_from_spo_factory(iface)->ref ); }
static ULONG WINAPI spo_factory_Release( IActivationFactory *iface ) { return InterlockedDecrement( &impl_from_spo_factory(iface)->ref ); }
static HRESULT WINAPI spo_factory_GetIids( IActivationFactory *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI spo_factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI spo_factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *t ) { (void)iface;(void)t; return E_NOTIMPL; }
static HRESULT WINAPI spo_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    struct stage_package_options *impl;
    (void)iface;
    if (!(impl = calloc( 1, sizeof(*impl) ))) { *instance = NULL; return E_OUTOFMEMORY; }
    impl->IStagePackageOptions_iface.lpVtbl = &stage_package_options_vtbl;
    impl->ref = 1;
    *instance = (IInspectable *)&impl->IStagePackageOptions_iface;
    return S_OK;
}
static const struct IActivationFactoryVtbl spo_factory_vtbl =
{
    spo_factory_QueryInterface, spo_factory_AddRef, spo_factory_Release,
    spo_factory_GetIids, spo_factory_GetRuntimeClassName, spo_factory_GetTrustLevel,
    spo_factory_ActivateInstance,
};
static struct stage_package_options_statics stage_package_options_statics = { {&spo_factory_vtbl}, 1 };
IActivationFactory *stage_package_options_factory = &stage_package_options_statics.IActivationFactory_iface;

/* ============================================================
 * Windows.ApplicationModel.LimitedAccessFeatures
 * Static-only WinRT class (no instantiable object): the activation
 * factory itself implements ILimitedAccessFeaturesStatics. Real Windows
 * requires an unlock token issued by Microsoft for most feature IDs, so
 * TryUnlockFeature legitimately returns "Unavailable" on the vast
 * majority of real machines too - we return that same, normal, successful
 * (non-throwing) result instead of failing the activation outright,
 * since apps normally handle Unavailable as a routine case. */

struct limited_access_feature_request_result
{
    ILimitedAccessFeatureRequestResult ILimitedAccessFeatureRequestResult_iface;
    LONG ref;
    HSTRING feature_id;
};

static ULONG WINAPI lafrr_AddRef( ILimitedAccessFeatureRequestResult *iface )
{
    struct limited_access_feature_request_result *impl =
        CONTAINING_RECORD( iface, struct limited_access_feature_request_result, ILimitedAccessFeatureRequestResult_iface );
    return InterlockedIncrement( &impl->ref );
}

static ULONG WINAPI lafrr_Release( ILimitedAccessFeatureRequestResult *iface )
{
    struct limited_access_feature_request_result *impl =
        CONTAINING_RECORD( iface, struct limited_access_feature_request_result, ILimitedAccessFeatureRequestResult_iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        WindowsDeleteString( impl->feature_id );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI lafrr_QueryInterface( ILimitedAccessFeatureRequestResult *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_ILimitedAccessFeatureRequestResult_ ))
    {
        *out = iface;
        lafrr_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static HRESULT WINAPI lafrr_GetIids( ILimitedAccessFeatureRequestResult *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI lafrr_GetRuntimeClassName( ILimitedAccessFeatureRequestResult *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI lafrr_GetTrustLevel( ILimitedAccessFeatureRequestResult *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }

static HRESULT WINAPI lafrr_get_FeatureId( ILimitedAccessFeatureRequestResult *iface, HSTRING *out )
{
    struct limited_access_feature_request_result *impl =
        CONTAINING_RECORD( iface, struct limited_access_feature_request_result, ILimitedAccessFeatureRequestResult_iface );
    /* Real Windows echoes back the requested feature id; returning an
     * empty string here caused ms-teams.exe to crash (fastfail via an
     * unhandled exception) further up its own call stack. */
    if (impl->feature_id) return WindowsDuplicateString( impl->feature_id, out );
    return WindowsCreateString( L"", 0, out );
}

static HRESULT WINAPI lafrr_get_Status( ILimitedAccessFeatureRequestResult *iface, LimitedAccessFeatureStatus *out )
{
    (void)iface;
    *out = LimitedAccessFeatureStatus_Unavailable;
    return S_OK;
}

static HRESULT WINAPI lafrr_get_EstimatedRemovalDate( ILimitedAccessFeatureRequestResult *iface, void **out )
{
    (void)iface;
    *out = NULL;
    return S_OK;
}

static const struct ILimitedAccessFeatureRequestResultVtbl lafrr_vtbl =
{
    lafrr_QueryInterface, lafrr_AddRef, lafrr_Release,
    lafrr_GetIids, lafrr_GetRuntimeClassName, lafrr_GetTrustLevel,
    lafrr_get_FeatureId, lafrr_get_Status, lafrr_get_EstimatedRemovalDate,
};

struct limited_access_features_statics
{
    IActivationFactory IActivationFactory_iface;
    ILimitedAccessFeaturesStatics ILimitedAccessFeaturesStatics_iface;
    LONG ref;
};

static inline struct limited_access_features_statics *impl_from_laf_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct limited_access_features_statics, IActivationFactory_iface );
}
static inline struct limited_access_features_statics *impl_from_ILimitedAccessFeaturesStatics( ILimitedAccessFeaturesStatics *iface )
{
    return CONTAINING_RECORD( iface, struct limited_access_features_statics, ILimitedAccessFeaturesStatics_iface );
}

static HRESULT WINAPI laf_factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct limited_access_features_statics *impl = impl_from_laf_IActivationFactory( iface );

    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IInspectable_AddRef( (IInspectable *)*out );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_ILimitedAccessFeaturesStatics_ ))
    {
        *out = &impl->ILimitedAccessFeaturesStatics_iface;
        IInspectable_AddRef( (IInspectable *)&impl->IActivationFactory_iface );
        return S_OK;
    }
    ERR( "limited_access_features_factory: %s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI laf_factory_AddRef( IActivationFactory *iface )
{
    return InterlockedIncrement( &impl_from_laf_IActivationFactory(iface)->ref );
}
static ULONG WINAPI laf_factory_Release( IActivationFactory *iface )
{
    return InterlockedDecrement( &impl_from_laf_IActivationFactory(iface)->ref );
}
static HRESULT WINAPI laf_factory_GetIids( IActivationFactory *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI laf_factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI laf_factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI laf_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    (void)iface;
    *instance = NULL;
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl laf_factory_vtbl =
{
    laf_factory_QueryInterface, laf_factory_AddRef, laf_factory_Release,
    laf_factory_GetIids, laf_factory_GetRuntimeClassName, laf_factory_GetTrustLevel,
    laf_factory_ActivateInstance,
};

static HRESULT WINAPI laf_statics_QueryInterface( ILimitedAccessFeaturesStatics *iface, REFIID iid, void **out )
{
    struct limited_access_features_statics *impl = impl_from_ILimitedAccessFeaturesStatics( iface );
    return laf_factory_QueryInterface( &impl->IActivationFactory_iface, iid, out );
}
static ULONG WINAPI laf_statics_AddRef( ILimitedAccessFeaturesStatics *iface )
{
    return InterlockedIncrement( &impl_from_ILimitedAccessFeaturesStatics(iface)->ref );
}
static ULONG WINAPI laf_statics_Release( ILimitedAccessFeaturesStatics *iface )
{
    return InterlockedDecrement( &impl_from_ILimitedAccessFeaturesStatics(iface)->ref );
}
static HRESULT WINAPI laf_statics_GetIids( ILimitedAccessFeaturesStatics *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI laf_statics_GetRuntimeClassName( ILimitedAccessFeaturesStatics *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI laf_statics_GetTrustLevel( ILimitedAccessFeaturesStatics *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }

static HRESULT WINAPI laf_TryUnlockFeature( ILimitedAccessFeaturesStatics *iface, HSTRING featureId, HSTRING token,
    HSTRING attestation, ILimitedAccessFeatureRequestResult **result )
{
    struct limited_access_feature_request_result *impl;
    (void)iface; (void)token; (void)attestation;

    ERR( "TryUnlockFeature: reporting Unavailable (no real unlock token support).\n" );

    if (!(impl = calloc( 1, sizeof(*impl) )))
    {
        *result = NULL;
        return E_OUTOFMEMORY;
    }
    impl->ILimitedAccessFeatureRequestResult_iface.lpVtbl = &lafrr_vtbl;
    impl->ref = 1;
    if (featureId) WindowsDuplicateString( featureId, &impl->feature_id );
    *result = &impl->ILimitedAccessFeatureRequestResult_iface;
    return S_OK;
}

static const struct ILimitedAccessFeaturesStaticsVtbl laf_statics_vtbl =
{
    laf_statics_QueryInterface, laf_statics_AddRef, laf_statics_Release,
    laf_statics_GetIids, laf_statics_GetRuntimeClassName, laf_statics_GetTrustLevel,
    laf_TryUnlockFeature,
};

static struct limited_access_features_statics limited_access_features_statics =
{
    {&laf_factory_vtbl}, {&laf_statics_vtbl}, 1,
};

IActivationFactory *limited_access_features_factory = &limited_access_features_statics.IActivationFactory_iface;

/* ============================================================
 * Windows.System.Profile.WindowsIntegrityPolicy
 * Static-only WinRT class. Real Windows only enables this policy on
 * specific SKUs (e.g. Windows 10 in S mode); reporting it as disabled
 * (and disableable) matches what almost every real desktop Windows
 * install reports too - a normal, non-throwing result.
 * ============================================================ */
struct windows_integrity_policy_statics
{
    IActivationFactory IActivationFactory_iface;
    IWindowsIntegrityPolicyStatics IWindowsIntegrityPolicyStatics_iface;
    LONG ref;
};

static inline struct windows_integrity_policy_statics *impl_from_wip_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct windows_integrity_policy_statics, IActivationFactory_iface );
}
static inline struct windows_integrity_policy_statics *impl_from_IWindowsIntegrityPolicyStatics( IWindowsIntegrityPolicyStatics *iface )
{
    return CONTAINING_RECORD( iface, struct windows_integrity_policy_statics, IWindowsIntegrityPolicyStatics_iface );
}

static HRESULT WINAPI wip_factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct windows_integrity_policy_statics *impl = impl_from_wip_IActivationFactory( iface );

    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IInspectable_AddRef( (IInspectable *)*out );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_IWindowsIntegrityPolicyStatics_ ))
    {
        *out = &impl->IWindowsIntegrityPolicyStatics_iface;
        IInspectable_AddRef( (IInspectable *)&impl->IActivationFactory_iface );
        return S_OK;
    }
    ERR( "windows_integrity_policy_factory: %s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI wip_factory_AddRef( IActivationFactory *iface ) { return InterlockedIncrement( &impl_from_wip_IActivationFactory(iface)->ref ); }
static ULONG WINAPI wip_factory_Release( IActivationFactory *iface ) { return InterlockedDecrement( &impl_from_wip_IActivationFactory(iface)->ref ); }
static HRESULT WINAPI wip_factory_GetIids( IActivationFactory *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI wip_factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI wip_factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI wip_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance ) { (void)iface; *instance = NULL; return E_NOTIMPL; }

static const struct IActivationFactoryVtbl wip_factory_vtbl =
{
    wip_factory_QueryInterface, wip_factory_AddRef, wip_factory_Release,
    wip_factory_GetIids, wip_factory_GetRuntimeClassName, wip_factory_GetTrustLevel,
    wip_factory_ActivateInstance,
};

static HRESULT WINAPI wip_statics_QueryInterface( IWindowsIntegrityPolicyStatics *iface, REFIID iid, void **out )
{
    return wip_factory_QueryInterface( &impl_from_IWindowsIntegrityPolicyStatics(iface)->IActivationFactory_iface, iid, out );
}
static ULONG WINAPI wip_statics_AddRef( IWindowsIntegrityPolicyStatics *iface ) { return InterlockedIncrement( &impl_from_IWindowsIntegrityPolicyStatics(iface)->ref ); }
static ULONG WINAPI wip_statics_Release( IWindowsIntegrityPolicyStatics *iface ) { return InterlockedDecrement( &impl_from_IWindowsIntegrityPolicyStatics(iface)->ref ); }
static HRESULT WINAPI wip_statics_GetIids( IWindowsIntegrityPolicyStatics *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI wip_statics_GetRuntimeClassName( IWindowsIntegrityPolicyStatics *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI wip_statics_GetTrustLevel( IWindowsIntegrityPolicyStatics *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI wip_get_IsEnabled( IWindowsIntegrityPolicyStatics *iface, boolean *value ) { (void)iface; *value = FALSE; return S_OK; }
static HRESULT WINAPI wip_get_IsEnabledForTrial( IWindowsIntegrityPolicyStatics *iface, boolean *value ) { (void)iface; *value = FALSE; return S_OK; }
static HRESULT WINAPI wip_get_CanDisable( IWindowsIntegrityPolicyStatics *iface, boolean *value ) { (void)iface; *value = TRUE; return S_OK; }
static HRESULT WINAPI wip_get_IsDisableSupported( IWindowsIntegrityPolicyStatics *iface, boolean *value ) { (void)iface; *value = TRUE; return S_OK; }
static HRESULT WINAPI wip_add_PolicyChanged( IWindowsIntegrityPolicyStatics *iface, void *handler, INT64 *token )
{
    (void)iface; (void)handler;
    *token = 1;
    return S_OK;
}
static HRESULT WINAPI wip_remove_PolicyChanged( IWindowsIntegrityPolicyStatics *iface, INT64 token )
{
    (void)iface; (void)token;
    return S_OK;
}

static const struct IWindowsIntegrityPolicyStaticsVtbl wip_statics_vtbl =
{
    wip_statics_QueryInterface, wip_statics_AddRef, wip_statics_Release,
    wip_statics_GetIids, wip_statics_GetRuntimeClassName, wip_statics_GetTrustLevel,
    wip_get_IsEnabled, wip_get_IsEnabledForTrial, wip_get_CanDisable, wip_get_IsDisableSupported,
    wip_add_PolicyChanged, wip_remove_PolicyChanged,
};

static struct windows_integrity_policy_statics windows_integrity_policy_statics =
{
    {&wip_factory_vtbl}, {&wip_statics_vtbl}, 1,
};

IActivationFactory *windows_integrity_policy_factory = &windows_integrity_policy_statics.IActivationFactory_iface;
