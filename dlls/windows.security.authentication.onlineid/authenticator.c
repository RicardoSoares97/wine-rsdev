/* WinRT Windows.Security.Authentication.Onlineid Implementation
 *
 * Copyright (C) 2024 Mohamad Al-Jaf
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

WINE_DEFAULT_DEBUG_CHANNEL(onlineid);

struct authenticator_statics
{
    IActivationFactory IActivationFactory_iface;
    IOnlineIdSystemAuthenticatorStatics IOnlineIdSystemAuthenticatorStatics_iface;
    LONG ref;
};

static inline struct authenticator_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct authenticator_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct authenticator_statics *impl = impl_from_IActivationFactory( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    if (IsEqualGUID( iid, &IID_IOnlineIdSystemAuthenticatorStatics ))
    {
        *out = &impl->IOnlineIdSystemAuthenticatorStatics_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct authenticator_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct authenticator_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p, ref %lu.\n", iface, ref );
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
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
    return E_NOTIMPL;
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

struct authenticator
{
    IOnlineIdSystemAuthenticatorForUser IOnlineIdSystemAuthenticatorForUser_iface;
    LONG ref;
    GUID application_id;
};

static inline struct authenticator *impl_from_IOnlineIdSystemAuthenticatorForUser( IOnlineIdSystemAuthenticatorForUser *iface )
{
    return CONTAINING_RECORD( iface, struct authenticator, IOnlineIdSystemAuthenticatorForUser_iface );
}

static HRESULT WINAPI authenticator_QueryInterface( IOnlineIdSystemAuthenticatorForUser *iface, REFIID iid, void **out )
{
    struct authenticator *impl = impl_from_IOnlineIdSystemAuthenticatorForUser( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IOnlineIdSystemAuthenticatorForUser ))
    {
        *out = &impl->IOnlineIdSystemAuthenticatorForUser_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI authenticator_AddRef( IOnlineIdSystemAuthenticatorForUser *iface )
{
    struct authenticator *impl = impl_from_IOnlineIdSystemAuthenticatorForUser( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI authenticator_Release( IOnlineIdSystemAuthenticatorForUser *iface )
{
    struct authenticator *impl = impl_from_IOnlineIdSystemAuthenticatorForUser( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );

    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );

    if (!ref) free( impl );
    return ref;
}

static HRESULT WINAPI authenticator_GetIids( IOnlineIdSystemAuthenticatorForUser *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI authenticator_GetRuntimeClassName( IOnlineIdSystemAuthenticatorForUser *iface, HSTRING *class_name )
{
    FIXME( "iface %p, class_name %p stub!\n", iface, class_name );
    return E_NOTIMPL;
}

static HRESULT WINAPI authenticator_GetTrustLevel( IOnlineIdSystemAuthenticatorForUser *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

/* ============================================================
 * OnlineIdSystemTicketResult - real object, always reporting Error
 * since there is no signed-in Microsoft account / online-id identity
 * to obtain a genuine ticket from. This matches what a real Windows
 * machine with no online-id account configured also reports; returning
 * a normal result object (rather than failing the async call outright)
 * avoids relying on WinRT exception delivery for this routine case.
 * ============================================================ */
struct ticket_result
{
    IOnlineIdSystemTicketResult IOnlineIdSystemTicketResult_iface;
    LONG ref;
};

static inline struct ticket_result *impl_from_IOnlineIdSystemTicketResult( IOnlineIdSystemTicketResult *iface )
{
    return CONTAINING_RECORD( iface, struct ticket_result, IOnlineIdSystemTicketResult_iface );
}

static HRESULT WINAPI ticket_result_QueryInterface( IOnlineIdSystemTicketResult *iface, REFIID iid, void **out )
{
    struct ticket_result *impl = impl_from_IOnlineIdSystemTicketResult( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IOnlineIdSystemTicketResult ))
    {
        *out = &impl->IOnlineIdSystemTicketResult_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }
    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI ticket_result_AddRef( IOnlineIdSystemTicketResult *iface )
{
    return InterlockedIncrement( &impl_from_IOnlineIdSystemTicketResult(iface)->ref );
}
static ULONG WINAPI ticket_result_Release( IOnlineIdSystemTicketResult *iface )
{
    struct ticket_result *impl = impl_from_IOnlineIdSystemTicketResult( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref) free( impl );
    return ref;
}
static HRESULT WINAPI ticket_result_GetIids( IOnlineIdSystemTicketResult *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI ticket_result_GetRuntimeClassName( IOnlineIdSystemTicketResult *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI ticket_result_GetTrustLevel( IOnlineIdSystemTicketResult *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI ticket_result_get_Identity( IOnlineIdSystemTicketResult *iface, IOnlineIdSystemIdentity **value )
{
    (void)iface;
    *value = NULL;
    return S_OK;
}
static HRESULT WINAPI ticket_result_get_Status( IOnlineIdSystemTicketResult *iface, OnlineIdSystemTicketStatus *value )
{
    (void)iface;
    *value = OnlineIdSystemTicketStatus_Error;
    return S_OK;
}
static HRESULT WINAPI ticket_result_get_ExtendedError( IOnlineIdSystemTicketResult *iface, HRESULT *value )
{
    (void)iface;
    *value = HRESULT_FROM_WIN32( ERROR_NOT_LOGGED_ON );
    return S_OK;
}
static const struct IOnlineIdSystemTicketResultVtbl ticket_result_vtbl =
{
    ticket_result_QueryInterface, ticket_result_AddRef, ticket_result_Release,
    ticket_result_GetIids, ticket_result_GetRuntimeClassName, ticket_result_GetTrustLevel,
    ticket_result_get_Identity, ticket_result_get_Status, ticket_result_get_ExtendedError,
};

/* ============================================================
 * IAsyncOperation<OnlineIdSystemTicketResult> - synchronously completed,
 * same completed-before-observed pattern as appxdeploymentclient's
 * deploy_async_op.
 * ============================================================ */
struct ticket_async_op
{
    IAsyncOperation_OnlineIdSystemTicketResult IAsyncOperation_iface;
    IAsyncInfo IAsyncInfo_iface;
    LONG ref;
    IOnlineIdSystemTicketResult *result;
};
static inline struct ticket_async_op *impl_from_ticket_async_op( IAsyncOperation_OnlineIdSystemTicketResult *iface )
{
    return CONTAINING_RECORD( iface, struct ticket_async_op, IAsyncOperation_iface );
}
static inline struct ticket_async_op *impl_from_ticket_async_info( IAsyncInfo *iface )
{
    return CONTAINING_RECORD( iface, struct ticket_async_op, IAsyncInfo_iface );
}
static HRESULT WINAPI tao_QueryInterface( IAsyncOperation_OnlineIdSystemTicketResult *iface, REFIID iid, void **out )
{
    struct ticket_async_op *impl = impl_from_ticket_async_op( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAsyncOperation_OnlineIdSystemTicketResult ))
    {
        *out = &impl->IAsyncOperation_iface;
        IAsyncOperation_OnlineIdSystemTicketResult_AddRef( (IAsyncOperation_OnlineIdSystemTicketResult *)*out );
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
static ULONG WINAPI tao_AddRef( IAsyncOperation_OnlineIdSystemTicketResult *iface ) { return InterlockedIncrement( &impl_from_ticket_async_op(iface)->ref ); }
static ULONG WINAPI tao_Release( IAsyncOperation_OnlineIdSystemTicketResult *iface )
{
    struct ticket_async_op *impl = impl_from_ticket_async_op( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->result) IOnlineIdSystemTicketResult_Release( impl->result );
        free( impl );
    }
    return ref;
}
static HRESULT WINAPI tao_GetIids( IAsyncOperation_OnlineIdSystemTicketResult *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI tao_GetRuntimeClassName( IAsyncOperation_OnlineIdSystemTicketResult *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI tao_GetTrustLevel( IAsyncOperation_OnlineIdSystemTicketResult *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI tao_put_Completed( IAsyncOperation_OnlineIdSystemTicketResult *iface, IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult *handler )
{
    struct ticket_async_op *impl = impl_from_ticket_async_op( iface );
    if (handler) handler->lpVtbl->Invoke( handler, &impl->IAsyncOperation_iface, Completed );
    return S_OK;
}
static HRESULT WINAPI tao_get_Completed( IAsyncOperation_OnlineIdSystemTicketResult *iface, IAsyncOperationCompletedHandler_OnlineIdSystemTicketResult **handler )
{
    (void)iface;
    *handler = NULL;
    return S_OK;
}
static HRESULT WINAPI tao_GetResults( IAsyncOperation_OnlineIdSystemTicketResult *iface, IOnlineIdSystemTicketResult **results )
{
    struct ticket_async_op *impl = impl_from_ticket_async_op( iface );
    *results = impl->result;
    if (*results) IOnlineIdSystemTicketResult_AddRef( *results );
    return S_OK;
}
static const struct IAsyncOperation_OnlineIdSystemTicketResultVtbl ticket_async_op_vtbl =
{
    tao_QueryInterface, tao_AddRef, tao_Release,
    tao_GetIids, tao_GetRuntimeClassName, tao_GetTrustLevel,
    tao_put_Completed, tao_get_Completed, tao_GetResults,
};
static HRESULT WINAPI taio_QueryInterface( IAsyncInfo *iface, REFIID iid, void **out )
{
    return tao_QueryInterface( &impl_from_ticket_async_info(iface)->IAsyncOperation_iface, iid, out );
}
static ULONG WINAPI taio_AddRef( IAsyncInfo *iface ) { return InterlockedIncrement( &impl_from_ticket_async_info(iface)->ref ); }
static ULONG WINAPI taio_Release( IAsyncInfo *iface ) { return tao_Release( &impl_from_ticket_async_info(iface)->IAsyncOperation_iface ); }
static HRESULT WINAPI taio_GetIids( IAsyncInfo *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI taio_GetRuntimeClassName( IAsyncInfo *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI taio_GetTrustLevel( IAsyncInfo *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI taio_get_Id( IAsyncInfo *iface, UINT32 *id ) { (void)iface; *id = 1; return S_OK; }
static HRESULT WINAPI taio_get_Status( IAsyncInfo *iface, AsyncStatus *status ) { (void)iface; *status = Completed; return S_OK; }
static HRESULT WINAPI taio_get_ErrorCode( IAsyncInfo *iface, HRESULT *error_code ) { (void)iface; *error_code = S_OK; return S_OK; }
static HRESULT WINAPI taio_Cancel( IAsyncInfo *iface ) { (void)iface; return S_OK; }
static HRESULT WINAPI taio_Close( IAsyncInfo *iface ) { (void)iface; return S_OK; }
static const struct IAsyncInfoVtbl ticket_async_info_vtbl =
{
    taio_QueryInterface, taio_AddRef, taio_Release,
    taio_GetIids, taio_GetRuntimeClassName, taio_GetTrustLevel,
    taio_get_Id, taio_get_Status, taio_get_ErrorCode, taio_Cancel, taio_Close,
};

static HRESULT WINAPI authenticator_GetTicketAsync( IOnlineIdSystemAuthenticatorForUser *iface, IOnlineIdServiceTicketRequest *request,
                                                    IAsyncOperation_OnlineIdSystemTicketResult **operation )
{
    struct ticket_result *result;
    struct ticket_async_op *op;

    TRACE( "iface %p, request %p, operation %p.\n", iface, request, operation );

    if (!(result = calloc( 1, sizeof(*result) ))) return E_OUTOFMEMORY;
    result->IOnlineIdSystemTicketResult_iface.lpVtbl = &ticket_result_vtbl;
    result->ref = 1;

    if (!(op = calloc( 1, sizeof(*op) )))
    {
        IOnlineIdSystemTicketResult_Release( &result->IOnlineIdSystemTicketResult_iface );
        return E_OUTOFMEMORY;
    }
    op->IAsyncOperation_iface.lpVtbl = &ticket_async_op_vtbl;
    op->IAsyncInfo_iface.lpVtbl = &ticket_async_info_vtbl;
    op->ref = 1;
    op->result = &result->IOnlineIdSystemTicketResult_iface;

    *operation = &op->IAsyncOperation_iface;
    return S_OK;
}

static HRESULT WINAPI authenticator_put_ApplicationId( IOnlineIdSystemAuthenticatorForUser *iface, GUID value )
{
    struct authenticator *impl = impl_from_IOnlineIdSystemAuthenticatorForUser( iface );
    TRACE( "iface %p, value %s.\n", iface, debugstr_guid( &value ) );
    impl->application_id = value;
    return S_OK;
}

static HRESULT WINAPI authenticator_get_ApplicationId( IOnlineIdSystemAuthenticatorForUser *iface, GUID *value )
{
    struct authenticator *impl = impl_from_IOnlineIdSystemAuthenticatorForUser( iface );
    TRACE( "iface %p, value %p.\n", iface, value );
    *value = impl->application_id;
    return S_OK;
}

static HRESULT WINAPI authenticator_get_User( IOnlineIdSystemAuthenticatorForUser *iface, __x_ABI_CWindows_CSystem_CIUser **user )
{
    FIXME( "iface %p, user %p stub!\n", iface, user );
    return E_NOTIMPL;
}

static const struct IOnlineIdSystemAuthenticatorForUserVtbl authenticator_vtbl =
{
    authenticator_QueryInterface,
    authenticator_AddRef,
    authenticator_Release,
    /* IInspectable methods */
    authenticator_GetIids,
    authenticator_GetRuntimeClassName,
    authenticator_GetTrustLevel,
    /* IOnlineIdSystemAuthenticatorForUser methods */
    authenticator_GetTicketAsync,
    authenticator_put_ApplicationId,
    authenticator_get_ApplicationId,
    authenticator_get_User,
};

DEFINE_IINSPECTABLE( authenticator_statics, IOnlineIdSystemAuthenticatorStatics, struct authenticator_statics, IActivationFactory_iface )

static HRESULT WINAPI authenticator_statics_get_Default( IOnlineIdSystemAuthenticatorStatics *iface,
                                                         IOnlineIdSystemAuthenticatorForUser **value )
{
    struct authenticator *impl;

    TRACE( "iface %p, value %p\n", iface, value );

    if (!value) return E_POINTER;
    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;

    impl->IOnlineIdSystemAuthenticatorForUser_iface.lpVtbl = &authenticator_vtbl;
    impl->ref = 1;

    *value = &impl->IOnlineIdSystemAuthenticatorForUser_iface;
    TRACE( "created IOnlineIdSystemAuthenticatorForUser %p.\n", *value );
    return S_OK;
}

static HRESULT WINAPI authenticator_statics_GetForUser( IOnlineIdSystemAuthenticatorStatics *iface, __x_ABI_CWindows_CSystem_CIUser *user,
                                                        IOnlineIdSystemAuthenticatorForUser **value )
{
    FIXME( "iface %p, user %p, value %p stub!\n", iface, user, value );
    return E_NOTIMPL;
}

static const struct IOnlineIdSystemAuthenticatorStaticsVtbl authenticator_statics_vtbl =
{
    authenticator_statics_QueryInterface,
    authenticator_statics_AddRef,
    authenticator_statics_Release,
    /* IInspectable methods */
    authenticator_statics_GetIids,
    authenticator_statics_GetRuntimeClassName,
    authenticator_statics_GetTrustLevel,
    /* IOnlineIdSystemAuthenticatorStatics methods */
    authenticator_statics_get_Default,
    authenticator_statics_GetForUser,
};

static struct authenticator_statics authenticator_statics =
{
    {&factory_vtbl},
    {&authenticator_statics_vtbl},
    1,
};

IActivationFactory *authenticator_factory = &authenticator_statics.IActivationFactory_iface;
