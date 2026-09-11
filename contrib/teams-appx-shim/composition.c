/* Windows.UI.Composition - minimal real implementation.
 *
 * Just enough of the real Compositor/Visual/Brush/Target object graph for
 * an app to hand a DXGI composition swapchain to a window and have SOMETHING
 * appear: Wine has no real compositor to route a composition swapchain's
 * buffer to a window's surface, so once a SpriteVisual with a surface brush
 * wrapping a swapchain becomes a target's Root, we spin up a background
 * thread that periodically reads the swapchain's current backbuffer back to
 * the CPU (via a staging texture) and blits it into the window with plain
 * GDI. No real visual-tree semantics (transforms, animations, effects) are
 * implemented - only enough for the underlying already-rendering DXGI
 * content to become visible at all.
 */

#include "private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(appx);

/* ============================================================
 * ICompositionSurface - opaque wrapper around whatever was handed to
 * ICompositorInterop::CreateCompositionSurfaceForSwapChain/ForHandle.
 * ============================================================ */
struct composition_surface
{
    ICompositionSurface ICompositionSurface_iface;
    LONG ref;
    IUnknown *swapchain_unk; /* AddRef'd; typically an IDXGISwapChain(1) */
};

static inline struct composition_surface *impl_from_ICompositionSurface( ICompositionSurface *iface )
{
    return CONTAINING_RECORD( iface, struct composition_surface, ICompositionSurface_iface );
}

static HRESULT WINAPI surface_QueryInterface( ICompositionSurface *iface, REFIID iid, void **out )
{
    struct composition_surface *impl = impl_from_ICompositionSurface( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_ICompositionSurface_ ))
    {
        *out = iface;
        iface->lpVtbl->AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI surface_AddRef( ICompositionSurface *iface ) { return InterlockedIncrement( &impl_from_ICompositionSurface(iface)->ref ); }
static ULONG WINAPI surface_Release( ICompositionSurface *iface )
{
    struct composition_surface *impl = impl_from_ICompositionSurface( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->swapchain_unk) IUnknown_Release( impl->swapchain_unk );
        free( impl );
    }
    return ref;
}
static HRESULT WINAPI surface_GetIids( ICompositionSurface *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI surface_GetRuntimeClassName( ICompositionSurface *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI surface_GetTrustLevel( ICompositionSurface *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }

static const struct ICompositionSurfaceVtbl surface_vtbl =
{
    surface_QueryInterface, surface_AddRef, surface_Release,
    surface_GetIids, surface_GetRuntimeClassName, surface_GetTrustLevel,
};

static HRESULT create_composition_surface_for_unknown( IUnknown *unk, ICompositionSurface **out )
{
    struct composition_surface *impl = calloc( 1, sizeof(*impl) );
    if (!impl) { *out = NULL; return E_OUTOFMEMORY; }
    impl->ICompositionSurface_iface.lpVtbl = &surface_vtbl;
    impl->ref = 1;
    if (unk) { impl->swapchain_unk = unk; IUnknown_AddRef( unk ); }
    *out = &impl->ICompositionSurface_iface;
    return S_OK;
}

/* ============================================================
 * ICompositionBrush / ICompositionSurfaceBrush
 * ============================================================ */
struct composition_surface_brush
{
    ICompositionBrush ICompositionBrush_iface;
    ICompositionSurfaceBrush ICompositionSurfaceBrush_iface;
    LONG ref;
    ICompositionSurface *surface;
    INT32 interpolation_mode, stretch;
    float h_align, v_align;
};

static inline struct composition_surface_brush *impl_from_ICompositionBrush( ICompositionBrush *iface )
{
    return CONTAINING_RECORD( iface, struct composition_surface_brush, ICompositionBrush_iface );
}
static inline struct composition_surface_brush *impl_from_ICompositionSurfaceBrush( ICompositionSurfaceBrush *iface )
{
    return CONTAINING_RECORD( iface, struct composition_surface_brush, ICompositionSurfaceBrush_iface );
}

static HRESULT WINAPI csb_brush_QueryInterface( ICompositionBrush *iface, REFIID iid, void **out )
{
    struct composition_surface_brush *impl = impl_from_ICompositionBrush( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_ICompositionBrush_ ))
    {
        *out = &impl->ICompositionBrush_iface;
        ICompositionBrush_AddRef( &impl->ICompositionBrush_iface );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_ICompositionSurfaceBrush_ ))
    {
        *out = &impl->ICompositionSurfaceBrush_iface;
        ICompositionBrush_AddRef( &impl->ICompositionBrush_iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI csb_brush_AddRef( ICompositionBrush *iface ) { return InterlockedIncrement( &impl_from_ICompositionBrush(iface)->ref ); }
static ULONG WINAPI csb_brush_Release( ICompositionBrush *iface )
{
    struct composition_surface_brush *impl = impl_from_ICompositionBrush( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->surface) ICompositionSurface_Release( impl->surface );
        free( impl );
    }
    return ref;
}
static HRESULT WINAPI csb_brush_GetIids( ICompositionBrush *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI csb_brush_GetRuntimeClassName( ICompositionBrush *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI csb_brush_GetTrustLevel( ICompositionBrush *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }

static const struct ICompositionBrushVtbl composition_surface_brush_brush_vtbl =
{
    csb_brush_QueryInterface, csb_brush_AddRef, csb_brush_Release,
    csb_brush_GetIids, csb_brush_GetRuntimeClassName, csb_brush_GetTrustLevel,
};

static HRESULT WINAPI csb_QueryInterface( ICompositionSurfaceBrush *iface, REFIID iid, void **out )
{
    return csb_brush_QueryInterface( &impl_from_ICompositionSurfaceBrush(iface)->ICompositionBrush_iface, iid, out );
}
static ULONG WINAPI csb_AddRef( ICompositionSurfaceBrush *iface ) { return InterlockedIncrement( &impl_from_ICompositionSurfaceBrush(iface)->ref ); }
static ULONG WINAPI csb_Release( ICompositionSurfaceBrush *iface ) { return csb_brush_Release( &impl_from_ICompositionSurfaceBrush(iface)->ICompositionBrush_iface ); }
static HRESULT WINAPI csb_GetIids( ICompositionSurfaceBrush *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI csb_GetRuntimeClassName( ICompositionSurfaceBrush *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI csb_GetTrustLevel( ICompositionSurfaceBrush *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI csb_get_BitmapInterpolationMode( ICompositionSurfaceBrush *iface, INT32 *v ) { *v = impl_from_ICompositionSurfaceBrush(iface)->interpolation_mode; return S_OK; }
static HRESULT WINAPI csb_put_BitmapInterpolationMode( ICompositionSurfaceBrush *iface, INT32 v ) { impl_from_ICompositionSurfaceBrush(iface)->interpolation_mode = v; return S_OK; }
static HRESULT WINAPI csb_get_HorizontalAlignmentRatio( ICompositionSurfaceBrush *iface, float *v ) { *v = impl_from_ICompositionSurfaceBrush(iface)->h_align; return S_OK; }
static HRESULT WINAPI csb_put_HorizontalAlignmentRatio( ICompositionSurfaceBrush *iface, float v ) { impl_from_ICompositionSurfaceBrush(iface)->h_align = v; return S_OK; }
static HRESULT WINAPI csb_get_Stretch( ICompositionSurfaceBrush *iface, INT32 *v ) { *v = impl_from_ICompositionSurfaceBrush(iface)->stretch; return S_OK; }
static HRESULT WINAPI csb_put_Stretch( ICompositionSurfaceBrush *iface, INT32 v ) { impl_from_ICompositionSurfaceBrush(iface)->stretch = v; return S_OK; }
static HRESULT WINAPI csb_get_Surface( ICompositionSurfaceBrush *iface, ICompositionSurface **v )
{
    struct composition_surface_brush *impl = impl_from_ICompositionSurfaceBrush( iface );
    *v = impl->surface;
    if (*v) ICompositionSurface_AddRef( *v );
    return S_OK;
}
static HRESULT WINAPI csb_put_Surface( ICompositionSurfaceBrush *iface, ICompositionSurface *v )
{
    struct composition_surface_brush *impl = impl_from_ICompositionSurfaceBrush( iface );
    if (impl->surface) ICompositionSurface_Release( impl->surface );
    impl->surface = v;
    if (v) ICompositionSurface_AddRef( v );
    return S_OK;
}
static HRESULT WINAPI csb_get_VerticalAlignmentRatio( ICompositionSurfaceBrush *iface, float *v ) { *v = impl_from_ICompositionSurfaceBrush(iface)->v_align; return S_OK; }
static HRESULT WINAPI csb_put_VerticalAlignmentRatio( ICompositionSurfaceBrush *iface, float v ) { impl_from_ICompositionSurfaceBrush(iface)->v_align = v; return S_OK; }

static const struct ICompositionSurfaceBrushVtbl composition_surface_brush_vtbl =
{
    csb_QueryInterface, csb_AddRef, csb_Release,
    csb_GetIids, csb_GetRuntimeClassName, csb_GetTrustLevel,
    csb_get_BitmapInterpolationMode, csb_put_BitmapInterpolationMode,
    csb_get_HorizontalAlignmentRatio, csb_put_HorizontalAlignmentRatio,
    csb_get_Stretch, csb_put_Stretch,
    csb_get_Surface, csb_put_Surface,
    csb_get_VerticalAlignmentRatio, csb_put_VerticalAlignmentRatio,
};

static HRESULT create_composition_surface_brush( ICompositionSurface *surface, ICompositionSurfaceBrush **out )
{
    struct composition_surface_brush *impl = calloc( 1, sizeof(*impl) );
    if (!impl) { *out = NULL; return E_OUTOFMEMORY; }
    impl->ICompositionBrush_iface.lpVtbl = &composition_surface_brush_brush_vtbl;
    impl->ICompositionSurfaceBrush_iface.lpVtbl = &composition_surface_brush_vtbl;
    impl->ref = 1;
    impl->h_align = impl->v_align = 0.5f;
    if (surface) { impl->surface = surface; ICompositionSurface_AddRef( surface ); }
    *out = &impl->ICompositionSurfaceBrush_iface;
    return S_OK;
}

/* ============================================================
 * IVisual / ISpriteVisual
 * ============================================================ */
struct sprite_visual
{
    IVisual IVisual_iface;
    IVisual2 IVisual2_iface;
    ISpriteVisual ISpriteVisual_iface;
    LONG ref;
    ICompositionBrush *brush;
    float offset[3], size[2], scale[3], center[3], anchor[2], rotation, rotation_deg;
    boolean is_visible;
    float rel_offset_adj[3], rel_size_adj[2];
};

static inline struct sprite_visual *impl_from_IVisual( IVisual *iface )
{
    return CONTAINING_RECORD( iface, struct sprite_visual, IVisual_iface );
}
static inline struct sprite_visual *impl_from_IVisual2( IVisual2 *iface )
{
    return CONTAINING_RECORD( iface, struct sprite_visual, IVisual2_iface );
}
static inline struct sprite_visual *impl_from_ISpriteVisual( ISpriteVisual *iface )
{
    return CONTAINING_RECORD( iface, struct sprite_visual, ISpriteVisual_iface );
}

static HRESULT WINAPI visual_QueryInterface( IVisual *iface, REFIID iid, void **out )
{
    struct sprite_visual *impl = impl_from_IVisual( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IVisual_ ))
    {
        *out = &impl->IVisual_iface;
        IVisual_AddRef( &impl->IVisual_iface );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_ISpriteVisual_ ))
    {
        *out = &impl->ISpriteVisual_iface;
        IVisual_AddRef( &impl->IVisual_iface );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_IVisual2_ ))
    {
        *out = &impl->IVisual2_iface;
        IVisual_AddRef( &impl->IVisual_iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI visual_AddRef( IVisual *iface ) { return InterlockedIncrement( &impl_from_IVisual(iface)->ref ); }
static ULONG WINAPI visual_Release( IVisual *iface )
{
    struct sprite_visual *impl = impl_from_IVisual( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->brush) ICompositionBrush_Release( impl->brush );
        free( impl );
    }
    return ref;
}
static HRESULT WINAPI visual_GetIids( IVisual *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI visual_GetRuntimeClassName( IVisual *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI visual_GetTrustLevel( IVisual *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI visual_get_AnchorPoint( IVisual *iface, void *v ) { memcpy( v, impl_from_IVisual(iface)->anchor, sizeof(float)*2 ); return S_OK; }
static HRESULT WINAPI visual_put_AnchorPoint( IVisual *iface, UINT64 v ) { memcpy( impl_from_IVisual(iface)->anchor, &v, sizeof(float)*2 ); return S_OK; }
static HRESULT WINAPI visual_get_BackfaceVisibility( IVisual *iface, INT32 *v ) { (void)iface; *v = 0; return S_OK; }
static HRESULT WINAPI visual_put_BackfaceVisibility( IVisual *iface, INT32 v ) { (void)iface;(void)v; return S_OK; }
static HRESULT WINAPI visual_get_BorderMode( IVisual *iface, INT32 *v ) { (void)iface; *v = 0; return S_OK; }
static HRESULT WINAPI visual_put_BorderMode( IVisual *iface, INT32 v ) { (void)iface;(void)v; return S_OK; }
static HRESULT WINAPI visual_get_CenterPoint( IVisual *iface, void *v ) { memcpy( v, impl_from_IVisual(iface)->center, sizeof(float)*3 ); return S_OK; }
static HRESULT WINAPI visual_put_CenterPoint( IVisual *iface, float x, float y, float z ) { struct sprite_visual *i = impl_from_IVisual(iface); i->center[0]=x; i->center[1]=y; i->center[2]=z; return S_OK; }
static HRESULT WINAPI visual_get_Clip( IVisual *iface, void **v ) { (void)iface; *v = NULL; return S_OK; }
static HRESULT WINAPI visual_put_Clip( IVisual *iface, void *v ) { (void)iface;(void)v; return S_OK; }
static HRESULT WINAPI visual_get_CompositeMode( IVisual *iface, INT32 *v ) { (void)iface; *v = 0; return S_OK; }
static HRESULT WINAPI visual_put_CompositeMode( IVisual *iface, INT32 v ) { (void)iface;(void)v; return S_OK; }
static HRESULT WINAPI visual_get_IsVisible( IVisual *iface, boolean *v ) { *v = impl_from_IVisual(iface)->is_visible; return S_OK; }
static HRESULT WINAPI visual_put_IsVisible( IVisual *iface, boolean v ) { impl_from_IVisual(iface)->is_visible = v; return S_OK; }
static HRESULT WINAPI visual_get_Offset( IVisual *iface, void *v ) { memcpy( v, impl_from_IVisual(iface)->offset, sizeof(float)*3 ); return S_OK; }
static HRESULT WINAPI visual_put_Offset( IVisual *iface, float x, float y, float z ) { struct sprite_visual *i = impl_from_IVisual(iface); i->offset[0]=x; i->offset[1]=y; i->offset[2]=z; return S_OK; }
static HRESULT WINAPI visual_get_Opacity( IVisual *iface, float *v ) { (void)iface; *v = 1.0f; return S_OK; }
static HRESULT WINAPI visual_put_Opacity( IVisual *iface, float v ) { (void)iface;(void)v; return S_OK; }
static HRESULT WINAPI visual_get_Orientation( IVisual *iface, void *v ) { (void)iface; memset(v,0,sizeof(float)*4); return S_OK; }
static HRESULT WINAPI visual_put_Orientation( IVisual *iface, float x, float y, float z, float w ) { (void)iface;(void)x;(void)y;(void)z;(void)w; return S_OK; }
static HRESULT WINAPI visual_get_Parent( IVisual *iface, void **v ) { (void)iface; *v = NULL; return S_OK; }
static HRESULT WINAPI visual_get_RotationAngle( IVisual *iface, float *v ) { *v = impl_from_IVisual(iface)->rotation; return S_OK; }
static HRESULT WINAPI visual_put_RotationAngle( IVisual *iface, float v ) { impl_from_IVisual(iface)->rotation = v; return S_OK; }
static HRESULT WINAPI visual_get_RotationAngleInDegrees( IVisual *iface, float *v ) { *v = impl_from_IVisual(iface)->rotation_deg; return S_OK; }
static HRESULT WINAPI visual_put_RotationAngleInDegrees( IVisual *iface, float v ) { impl_from_IVisual(iface)->rotation_deg = v; return S_OK; }
static HRESULT WINAPI visual_get_RotationAxis( IVisual *iface, void *v ) { (void)iface; memset(v,0,sizeof(float)*3); return S_OK; }
static HRESULT WINAPI visual_put_RotationAxis( IVisual *iface, float x, float y, float z ) { (void)iface;(void)x;(void)y;(void)z; return S_OK; }
static HRESULT WINAPI visual_get_Scale( IVisual *iface, void *v ) { memcpy( v, impl_from_IVisual(iface)->scale, sizeof(float)*3 ); return S_OK; }
static HRESULT WINAPI visual_put_Scale( IVisual *iface, float x, float y, float z ) { struct sprite_visual *i = impl_from_IVisual(iface); i->scale[0]=x; i->scale[1]=y; i->scale[2]=z; return S_OK; }
static HRESULT WINAPI visual_get_Size( IVisual *iface, void *v ) { memcpy( v, impl_from_IVisual(iface)->size, sizeof(float)*2 ); return S_OK; }
static HRESULT WINAPI visual_put_Size( IVisual *iface, float x, float y ) { struct sprite_visual *i = impl_from_IVisual(iface); i->size[0]=x; i->size[1]=y; return S_OK; }
static HRESULT WINAPI visual_get_TransformMatrix( IVisual *iface, void *v ) { (void)iface; memset(v,0,sizeof(float)*16); return S_OK; }
static HRESULT WINAPI visual_put_TransformMatrix( IVisual *iface, void *v ) { (void)iface;(void)v; return S_OK; }

static const struct IVisualVtbl sprite_visual_visual_vtbl =
{
    visual_QueryInterface, visual_AddRef, visual_Release,
    visual_GetIids, visual_GetRuntimeClassName, visual_GetTrustLevel,
    visual_get_AnchorPoint, visual_put_AnchorPoint,
    visual_get_BackfaceVisibility, visual_put_BackfaceVisibility,
    visual_get_BorderMode, visual_put_BorderMode,
    visual_get_CenterPoint, visual_put_CenterPoint,
    visual_get_Clip, visual_put_Clip,
    visual_get_CompositeMode, visual_put_CompositeMode,
    visual_get_IsVisible, visual_put_IsVisible,
    visual_get_Offset, visual_put_Offset,
    visual_get_Opacity, visual_put_Opacity,
    visual_get_Orientation, visual_put_Orientation,
    visual_get_Parent,
    visual_get_RotationAngle, visual_put_RotationAngle,
    visual_get_RotationAngleInDegrees, visual_put_RotationAngleInDegrees,
    visual_get_RotationAxis, visual_put_RotationAxis,
    visual_get_Scale, visual_put_Scale,
    visual_get_Size, visual_put_Size,
    visual_get_TransformMatrix, visual_put_TransformMatrix,
};

static HRESULT WINAPI sv_QueryInterface( ISpriteVisual *iface, REFIID iid, void **out )
{
    return visual_QueryInterface( &impl_from_ISpriteVisual(iface)->IVisual_iface, iid, out );
}
static ULONG WINAPI sv_AddRef( ISpriteVisual *iface ) { return InterlockedIncrement( &impl_from_ISpriteVisual(iface)->ref ); }
static ULONG WINAPI sv_Release( ISpriteVisual *iface ) { return visual_Release( &impl_from_ISpriteVisual(iface)->IVisual_iface ); }
static HRESULT WINAPI sv_GetIids( ISpriteVisual *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI sv_GetRuntimeClassName( ISpriteVisual *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI sv_GetTrustLevel( ISpriteVisual *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI sv_get_Brush( ISpriteVisual *iface, ICompositionBrush **v )
{
    struct sprite_visual *impl = impl_from_ISpriteVisual( iface );
    *v = impl->brush;
    if (*v) ICompositionBrush_AddRef( *v );
    return S_OK;
}
static HRESULT WINAPI sv_put_Brush( ISpriteVisual *iface, ICompositionBrush *v )
{
    struct sprite_visual *impl = impl_from_ISpriteVisual( iface );
    if (impl->brush) ICompositionBrush_Release( impl->brush );
    impl->brush = v;
    if (v) ICompositionBrush_AddRef( v );
    return S_OK;
}

static const struct ISpriteVisualVtbl sprite_visual_vtbl =
{
    sv_QueryInterface, sv_AddRef, sv_Release,
    sv_GetIids, sv_GetRuntimeClassName, sv_GetTrustLevel,
    sv_get_Brush, sv_put_Brush,
};

static HRESULT WINAPI v2_QueryInterface( IVisual2 *iface, REFIID iid, void **out )
{
    return visual_QueryInterface( &impl_from_IVisual2(iface)->IVisual_iface, iid, out );
}
static ULONG WINAPI v2_AddRef( IVisual2 *iface ) { return InterlockedIncrement( &impl_from_IVisual2(iface)->ref ); }
static ULONG WINAPI v2_Release( IVisual2 *iface ) { return visual_Release( &impl_from_IVisual2(iface)->IVisual_iface ); }
static HRESULT WINAPI v2_GetIids( IVisual2 *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI v2_GetRuntimeClassName( IVisual2 *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI v2_GetTrustLevel( IVisual2 *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI v2_get_ParentForTransform( IVisual2 *iface, void **v ) { (void)iface; *v = NULL; return S_OK; }
static HRESULT WINAPI v2_put_ParentForTransform( IVisual2 *iface, void *v ) { (void)iface;(void)v; return S_OK; }
static HRESULT WINAPI v2_get_RelativeOffsetAdjustment( IVisual2 *iface, void *v ) { memcpy( v, impl_from_IVisual2(iface)->rel_offset_adj, sizeof(float)*3 ); return S_OK; }
static HRESULT WINAPI v2_put_RelativeOffsetAdjustment( IVisual2 *iface, float x, float y, float z ) { struct sprite_visual *i = impl_from_IVisual2(iface); i->rel_offset_adj[0]=x; i->rel_offset_adj[1]=y; i->rel_offset_adj[2]=z; return S_OK; }
static HRESULT WINAPI v2_get_RelativeSizeAdjustment( IVisual2 *iface, void *v ) { memcpy( v, impl_from_IVisual2(iface)->rel_size_adj, sizeof(float)*2 ); return S_OK; }
static HRESULT WINAPI v2_put_RelativeSizeAdjustment( IVisual2 *iface, float x, float y ) { struct sprite_visual *i = impl_from_IVisual2(iface); i->rel_size_adj[0]=x; i->rel_size_adj[1]=y; return S_OK; }

static const struct IVisual2Vtbl sprite_visual2_vtbl =
{
    v2_QueryInterface, v2_AddRef, v2_Release,
    v2_GetIids, v2_GetRuntimeClassName, v2_GetTrustLevel,
    v2_get_ParentForTransform, v2_put_ParentForTransform,
    v2_get_RelativeOffsetAdjustment, v2_put_RelativeOffsetAdjustment,
    v2_get_RelativeSizeAdjustment, v2_put_RelativeSizeAdjustment,
};

static HRESULT create_sprite_visual( ISpriteVisual **out )
{
    struct sprite_visual *impl = calloc( 1, sizeof(*impl) );
    if (!impl) { *out = NULL; return E_OUTOFMEMORY; }
    impl->IVisual_iface.lpVtbl = &sprite_visual_visual_vtbl;
    impl->IVisual2_iface.lpVtbl = &sprite_visual2_vtbl;
    impl->ISpriteVisual_iface.lpVtbl = &sprite_visual_vtbl;
    impl->ref = 1;
    impl->scale[0] = impl->scale[1] = impl->scale[2] = 1.0f;
    impl->is_visible = TRUE;
    *out = &impl->ISpriteVisual_iface;
    return S_OK;
}

/* ============================================================
 * IVisualCollection / IContainerVisual - only enough to hold a handful of
 * children and let us walk the tree looking for a surface to blit; no
 * real ordering/z-index semantics.
 * ============================================================ */
#define MAX_VISUAL_CHILDREN 8

struct visual_collection
{
    IVisualCollection IVisualCollection_iface;
    LONG ref;
    IVisual *children[MAX_VISUAL_CHILDREN];
    INT32 count;
};

static inline struct visual_collection *impl_from_IVisualCollection( IVisualCollection *iface )
{
    return CONTAINING_RECORD( iface, struct visual_collection, IVisualCollection_iface );
}

static HRESULT WINAPI vc_QueryInterface( IVisualCollection *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IVisualCollection_ ))
    {
        *out = iface;
        iface->lpVtbl->AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI vc_AddRef( IVisualCollection *iface ) { return InterlockedIncrement( &impl_from_IVisualCollection(iface)->ref ); }
static ULONG WINAPI vc_Release( IVisualCollection *iface )
{
    struct visual_collection *impl = impl_from_IVisualCollection( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        INT32 i;
        for (i = 0; i < impl->count; i++) IVisual_Release( impl->children[i] );
        free( impl );
    }
    return ref;
}
static HRESULT WINAPI vc_GetIids( IVisualCollection *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI vc_GetRuntimeClassName( IVisualCollection *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI vc_GetTrustLevel( IVisualCollection *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI vc_get_Count( IVisualCollection *iface, INT32 *v ) { *v = impl_from_IVisualCollection(iface)->count; return S_OK; }
static HRESULT vc_insert( struct visual_collection *impl, IVisual *child )
{
    if (impl->count >= MAX_VISUAL_CHILDREN) return E_OUTOFMEMORY;
    impl->children[impl->count++] = child;
    IVisual_AddRef( child );
    return S_OK;
}
static HRESULT WINAPI vc_InsertAbove( IVisualCollection *iface, IVisual *new_child, IVisual *sibling ) { (void)sibling; return vc_insert( impl_from_IVisualCollection(iface), new_child ); }
static HRESULT WINAPI vc_InsertAtBottom( IVisualCollection *iface, IVisual *new_child ) { return vc_insert( impl_from_IVisualCollection(iface), new_child ); }
static HRESULT WINAPI vc_InsertAtTop( IVisualCollection *iface, IVisual *new_child ) { return vc_insert( impl_from_IVisualCollection(iface), new_child ); }
static HRESULT WINAPI vc_InsertBelow( IVisualCollection *iface, IVisual *new_child, IVisual *sibling ) { (void)sibling; return vc_insert( impl_from_IVisualCollection(iface), new_child ); }
static HRESULT WINAPI vc_Remove( IVisualCollection *iface, IVisual *child )
{
    struct visual_collection *impl = impl_from_IVisualCollection( iface );
    INT32 i;
    for (i = 0; i < impl->count; i++)
    {
        if (impl->children[i] == child)
        {
            IVisual_Release( impl->children[i] );
            memmove( &impl->children[i], &impl->children[i+1], (impl->count - i - 1) * sizeof(IVisual *) );
            impl->count--;
            return S_OK;
        }
    }
    return S_OK;
}
static HRESULT WINAPI vc_RemoveAll( IVisualCollection *iface )
{
    struct visual_collection *impl = impl_from_IVisualCollection( iface );
    INT32 i;
    for (i = 0; i < impl->count; i++) IVisual_Release( impl->children[i] );
    impl->count = 0;
    return S_OK;
}

static const struct IVisualCollectionVtbl visual_collection_vtbl =
{
    vc_QueryInterface, vc_AddRef, vc_Release,
    vc_GetIids, vc_GetRuntimeClassName, vc_GetTrustLevel,
    vc_get_Count, vc_InsertAbove, vc_InsertAtBottom, vc_InsertAtTop, vc_InsertBelow, vc_Remove, vc_RemoveAll,
};

struct container_visual
{
    IVisual IVisual_iface;
    IVisual2 IVisual2_iface;
    IContainerVisual IContainerVisual_iface;
    LONG ref;
    struct visual_collection *children;
    float offset[3], size[2], scale[3], center[3], anchor[2], rotation, rotation_deg;
    boolean is_visible;
    float rel_offset_adj[3], rel_size_adj[2];
};

static inline struct container_visual *impl_from_container_IVisual( IVisual *iface )
{
    return CONTAINING_RECORD( iface, struct container_visual, IVisual_iface );
}
static inline struct container_visual *impl_from_container_IVisual2( IVisual2 *iface )
{
    return CONTAINING_RECORD( iface, struct container_visual, IVisual2_iface );
}
static inline struct container_visual *impl_from_IContainerVisual( IContainerVisual *iface )
{
    return CONTAINING_RECORD( iface, struct container_visual, IContainerVisual_iface );
}

static HRESULT WINAPI cv_visual_QueryInterface( IVisual *iface, REFIID iid, void **out )
{
    struct container_visual *impl = impl_from_container_IVisual( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IVisual_ ))
    {
        *out = &impl->IVisual_iface;
        IVisual_AddRef( &impl->IVisual_iface );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_IContainerVisual_ ))
    {
        *out = &impl->IContainerVisual_iface;
        IVisual_AddRef( &impl->IVisual_iface );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_IVisual2_ ))
    {
        *out = &impl->IVisual2_iface;
        IVisual_AddRef( &impl->IVisual_iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cv_visual_AddRef( IVisual *iface ) { return InterlockedIncrement( &impl_from_container_IVisual(iface)->ref ); }
static ULONG WINAPI cv_visual_Release( IVisual *iface )
{
    struct container_visual *impl = impl_from_container_IVisual( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        IVisualCollection_Release( &impl->children->IVisualCollection_iface );
        free( impl );
    }
    return ref;
}
static HRESULT WINAPI cv_visual_GetIids( IVisual *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI cv_visual_GetRuntimeClassName( IVisual *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI cv_visual_GetTrustLevel( IVisual *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI cv_get_AnchorPoint( IVisual *iface, void *v ) { memcpy( v, impl_from_container_IVisual(iface)->anchor, sizeof(float)*2 ); return S_OK; }
static HRESULT WINAPI cv_put_AnchorPoint( IVisual *iface, UINT64 v ) { memcpy( impl_from_container_IVisual(iface)->anchor, &v, sizeof(float)*2 ); return S_OK; }
static HRESULT WINAPI cv_get_BackfaceVisibility( IVisual *iface, INT32 *v ) { (void)iface; *v = 0; return S_OK; }
static HRESULT WINAPI cv_put_BackfaceVisibility( IVisual *iface, INT32 v ) { (void)iface;(void)v; return S_OK; }
static HRESULT WINAPI cv_get_BorderMode( IVisual *iface, INT32 *v ) { (void)iface; *v = 0; return S_OK; }
static HRESULT WINAPI cv_put_BorderMode( IVisual *iface, INT32 v ) { (void)iface;(void)v; return S_OK; }
static HRESULT WINAPI cv_get_CenterPoint( IVisual *iface, void *v ) { memcpy( v, impl_from_container_IVisual(iface)->center, sizeof(float)*3 ); return S_OK; }
static HRESULT WINAPI cv_put_CenterPoint( IVisual *iface, float x, float y, float z ) { struct container_visual *i = impl_from_container_IVisual(iface); i->center[0]=x; i->center[1]=y; i->center[2]=z; return S_OK; }
static HRESULT WINAPI cv_get_Clip( IVisual *iface, void **v ) { (void)iface; *v = NULL; return S_OK; }
static HRESULT WINAPI cv_put_Clip( IVisual *iface, void *v ) { (void)iface;(void)v; return S_OK; }
static HRESULT WINAPI cv_get_CompositeMode( IVisual *iface, INT32 *v ) { (void)iface; *v = 0; return S_OK; }
static HRESULT WINAPI cv_put_CompositeMode( IVisual *iface, INT32 v ) { (void)iface;(void)v; return S_OK; }
static HRESULT WINAPI cv_get_IsVisible( IVisual *iface, boolean *v ) { *v = impl_from_container_IVisual(iface)->is_visible; return S_OK; }
static HRESULT WINAPI cv_put_IsVisible( IVisual *iface, boolean v ) { impl_from_container_IVisual(iface)->is_visible = v; return S_OK; }
static HRESULT WINAPI cv_get_Offset( IVisual *iface, void *v ) { memcpy( v, impl_from_container_IVisual(iface)->offset, sizeof(float)*3 ); return S_OK; }
static HRESULT WINAPI cv_put_Offset( IVisual *iface, float x, float y, float z ) { struct container_visual *i = impl_from_container_IVisual(iface); i->offset[0]=x; i->offset[1]=y; i->offset[2]=z; return S_OK; }
static HRESULT WINAPI cv_get_Opacity( IVisual *iface, float *v ) { (void)iface; *v = 1.0f; return S_OK; }
static HRESULT WINAPI cv_put_Opacity( IVisual *iface, float v ) { (void)iface;(void)v; return S_OK; }
static HRESULT WINAPI cv_get_Orientation( IVisual *iface, void *v ) { (void)iface; memset(v,0,sizeof(float)*4); return S_OK; }
static HRESULT WINAPI cv_put_Orientation( IVisual *iface, float x, float y, float z, float w ) { (void)iface;(void)x;(void)y;(void)z;(void)w; return S_OK; }
static HRESULT WINAPI cv_get_Parent( IVisual *iface, void **v ) { (void)iface; *v = NULL; return S_OK; }
static HRESULT WINAPI cv_get_RotationAngle( IVisual *iface, float *v ) { *v = impl_from_container_IVisual(iface)->rotation; return S_OK; }
static HRESULT WINAPI cv_put_RotationAngle( IVisual *iface, float v ) { impl_from_container_IVisual(iface)->rotation = v; return S_OK; }
static HRESULT WINAPI cv_get_RotationAngleInDegrees( IVisual *iface, float *v ) { *v = impl_from_container_IVisual(iface)->rotation_deg; return S_OK; }
static HRESULT WINAPI cv_put_RotationAngleInDegrees( IVisual *iface, float v ) { impl_from_container_IVisual(iface)->rotation_deg = v; return S_OK; }
static HRESULT WINAPI cv_get_RotationAxis( IVisual *iface, void *v ) { (void)iface; memset(v,0,sizeof(float)*3); return S_OK; }
static HRESULT WINAPI cv_put_RotationAxis( IVisual *iface, float x, float y, float z ) { (void)iface;(void)x;(void)y;(void)z; return S_OK; }
static HRESULT WINAPI cv_get_Scale( IVisual *iface, void *v ) { memcpy( v, impl_from_container_IVisual(iface)->scale, sizeof(float)*3 ); return S_OK; }
static HRESULT WINAPI cv_put_Scale( IVisual *iface, float x, float y, float z ) { struct container_visual *i = impl_from_container_IVisual(iface); i->scale[0]=x; i->scale[1]=y; i->scale[2]=z; return S_OK; }
static HRESULT WINAPI cv_get_Size( IVisual *iface, void *v ) { memcpy( v, impl_from_container_IVisual(iface)->size, sizeof(float)*2 ); return S_OK; }
static HRESULT WINAPI cv_put_Size( IVisual *iface, float x, float y ) { struct container_visual *i = impl_from_container_IVisual(iface); i->size[0]=x; i->size[1]=y; return S_OK; }
static HRESULT WINAPI cv_get_TransformMatrix( IVisual *iface, void *v ) { (void)iface; memset(v,0,sizeof(float)*16); return S_OK; }
static HRESULT WINAPI cv_put_TransformMatrix( IVisual *iface, void *v ) { (void)iface;(void)v; return S_OK; }

static const struct IVisualVtbl container_visual_visual_vtbl =
{
    cv_visual_QueryInterface, cv_visual_AddRef, cv_visual_Release,
    cv_visual_GetIids, cv_visual_GetRuntimeClassName, cv_visual_GetTrustLevel,
    cv_get_AnchorPoint, cv_put_AnchorPoint,
    cv_get_BackfaceVisibility, cv_put_BackfaceVisibility,
    cv_get_BorderMode, cv_put_BorderMode,
    cv_get_CenterPoint, cv_put_CenterPoint,
    cv_get_Clip, cv_put_Clip,
    cv_get_CompositeMode, cv_put_CompositeMode,
    cv_get_IsVisible, cv_put_IsVisible,
    cv_get_Offset, cv_put_Offset,
    cv_get_Opacity, cv_put_Opacity,
    cv_get_Orientation, cv_put_Orientation,
    cv_get_Parent,
    cv_get_RotationAngle, cv_put_RotationAngle,
    cv_get_RotationAngleInDegrees, cv_put_RotationAngleInDegrees,
    cv_get_RotationAxis, cv_put_RotationAxis,
    cv_get_Scale, cv_put_Scale,
    cv_get_Size, cv_put_Size,
    cv_get_TransformMatrix, cv_put_TransformMatrix,
};

static HRESULT WINAPI icv_QueryInterface( IContainerVisual *iface, REFIID iid, void **out )
{
    return cv_visual_QueryInterface( &impl_from_IContainerVisual(iface)->IVisual_iface, iid, out );
}
static ULONG WINAPI icv_AddRef( IContainerVisual *iface ) { return InterlockedIncrement( &impl_from_IContainerVisual(iface)->ref ); }
static ULONG WINAPI icv_Release( IContainerVisual *iface ) { return cv_visual_Release( &impl_from_IContainerVisual(iface)->IVisual_iface ); }
static HRESULT WINAPI icv_GetIids( IContainerVisual *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI icv_GetRuntimeClassName( IContainerVisual *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI icv_GetTrustLevel( IContainerVisual *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI icv_get_Children( IContainerVisual *iface, IVisualCollection **v )
{
    struct container_visual *impl = impl_from_IContainerVisual( iface );
    *v = &impl->children->IVisualCollection_iface;
    IVisualCollection_AddRef( *v );
    return S_OK;
}

static const struct IContainerVisualVtbl container_visual_vtbl =
{
    icv_QueryInterface, icv_AddRef, icv_Release,
    icv_GetIids, icv_GetRuntimeClassName, icv_GetTrustLevel,
    icv_get_Children,
};

static HRESULT WINAPI cv2_QueryInterface( IVisual2 *iface, REFIID iid, void **out )
{
    return cv_visual_QueryInterface( &impl_from_container_IVisual2(iface)->IVisual_iface, iid, out );
}
static ULONG WINAPI cv2_AddRef( IVisual2 *iface ) { return InterlockedIncrement( &impl_from_container_IVisual2(iface)->ref ); }
static ULONG WINAPI cv2_Release( IVisual2 *iface ) { return cv_visual_Release( &impl_from_container_IVisual2(iface)->IVisual_iface ); }
static HRESULT WINAPI cv2_GetIids( IVisual2 *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI cv2_GetRuntimeClassName( IVisual2 *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI cv2_GetTrustLevel( IVisual2 *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI cv2_get_ParentForTransform( IVisual2 *iface, void **v ) { (void)iface; *v = NULL; return S_OK; }
static HRESULT WINAPI cv2_put_ParentForTransform( IVisual2 *iface, void *v ) { (void)iface;(void)v; return S_OK; }
static HRESULT WINAPI cv2_get_RelativeOffsetAdjustment( IVisual2 *iface, void *v ) { memcpy( v, impl_from_container_IVisual2(iface)->rel_offset_adj, sizeof(float)*3 ); return S_OK; }
static HRESULT WINAPI cv2_put_RelativeOffsetAdjustment( IVisual2 *iface, float x, float y, float z ) { struct container_visual *i = impl_from_container_IVisual2(iface); i->rel_offset_adj[0]=x; i->rel_offset_adj[1]=y; i->rel_offset_adj[2]=z; return S_OK; }
static HRESULT WINAPI cv2_get_RelativeSizeAdjustment( IVisual2 *iface, void *v ) { memcpy( v, impl_from_container_IVisual2(iface)->rel_size_adj, sizeof(float)*2 ); return S_OK; }
static HRESULT WINAPI cv2_put_RelativeSizeAdjustment( IVisual2 *iface, float x, float y ) { struct container_visual *i = impl_from_container_IVisual2(iface); i->rel_size_adj[0]=x; i->rel_size_adj[1]=y; return S_OK; }

static const struct IVisual2Vtbl container_visual2_vtbl =
{
    cv2_QueryInterface, cv2_AddRef, cv2_Release,
    cv2_GetIids, cv2_GetRuntimeClassName, cv2_GetTrustLevel,
    cv2_get_ParentForTransform, cv2_put_ParentForTransform,
    cv2_get_RelativeOffsetAdjustment, cv2_put_RelativeOffsetAdjustment,
    cv2_get_RelativeSizeAdjustment, cv2_put_RelativeSizeAdjustment,
};

static HRESULT create_container_visual( IVisual **out )
{
    struct container_visual *impl = calloc( 1, sizeof(*impl) );
    struct visual_collection *coll;
    if (!impl) { *out = NULL; return E_OUTOFMEMORY; }
    if (!(coll = calloc( 1, sizeof(*coll) ))) { free( impl ); *out = NULL; return E_OUTOFMEMORY; }
    coll->IVisualCollection_iface.lpVtbl = &visual_collection_vtbl;
    coll->ref = 1;

    impl->IVisual_iface.lpVtbl = &container_visual_visual_vtbl;
    impl->IVisual2_iface.lpVtbl = &container_visual2_vtbl;
    impl->IContainerVisual_iface.lpVtbl = &container_visual_vtbl;
    impl->ref = 1;
    impl->children = coll;
    impl->scale[0] = impl->scale[1] = impl->scale[2] = 1.0f;
    impl->is_visible = TRUE;
    *out = &impl->IVisual_iface;
    return S_OK;
}

/* ============================================================
 * ICompositionTarget (DesktopWindowTarget) + the blit thread that makes
 * the swapchain content actually show up on screen.
 * ============================================================ */
struct composition_target
{
    ICompositionTarget ICompositionTarget_iface;
    IDesktopWindowTarget IDesktopWindowTarget_iface;
    ICompositionSupportsSystemBackdrop ICompositionSupportsSystemBackdrop_iface;
    LONG ref;
    HWND hwnd;
    IVisual *root;
    HANDLE blit_thread;
    LONG blit_stop;
    boolean topmost;
    void *system_backdrop;
};

static inline struct composition_target *impl_from_ICompositionTarget( ICompositionTarget *iface )
{
    return CONTAINING_RECORD( iface, struct composition_target, ICompositionTarget_iface );
}
static inline struct composition_target *impl_from_IDesktopWindowTarget( IDesktopWindowTarget *iface )
{
    return CONTAINING_RECORD( iface, struct composition_target, IDesktopWindowTarget_iface );
}
static inline struct composition_target *impl_from_ICompositionSupportsSystemBackdrop( ICompositionSupportsSystemBackdrop *iface )
{
    return CONTAINING_RECORD( iface, struct composition_target, ICompositionSupportsSystemBackdrop_iface );
}

/* walks Root -> ISpriteVisual -> Brush -> ICompositionSurfaceBrush -> Surface
 * -> the swapchain IUnknown stashed in our composition_surface, without
 * taking any extra refs (caller must not outlive the visual tree). */
static IUnknown *find_swapchain_unknown( IVisual *root )
{
    ISpriteVisual *sprite;
    IContainerVisual *container;
    IUnknown *result = NULL;

    if (!root) return NULL;

    if (SUCCEEDED( IVisual_QueryInterface( root, &IID_ISpriteVisual_, (void **)&sprite ) ))
    {
        ICompositionBrush *brush;
        if (SUCCEEDED( ISpriteVisual_get_Brush( sprite, &brush ) ) && brush)
        {
            ICompositionSurfaceBrush *surf_brush;
            if (SUCCEEDED( ICompositionBrush_QueryInterface( brush, &IID_ICompositionSurfaceBrush_, (void **)&surf_brush ) ))
            {
                ICompositionSurface *surface;
                if (SUCCEEDED( ICompositionSurfaceBrush_get_Surface( surf_brush, &surface ) ) && surface)
                {
                    struct composition_surface *surf_impl = impl_from_ICompositionSurface( surface );
                    result = surf_impl->swapchain_unk;
                    ICompositionSurface_Release( surface );
                }
                ICompositionSurfaceBrush_Release( surf_brush );
            }
            ICompositionBrush_Release( brush );
        }
        ISpriteVisual_Release( sprite );
        if (result) return result;
    }

    if (SUCCEEDED( IVisual_QueryInterface( root, &IID_IContainerVisual_, (void **)&container ) ))
    {
        IVisualCollection *children;
        if (SUCCEEDED( IContainerVisual_get_Children( container, &children ) ) && children)
        {
            struct visual_collection *coll = impl_from_IVisualCollection( children );
            INT32 i;
            for (i = 0; i < coll->count && !result; i++)
                result = find_swapchain_unknown( coll->children[i] );
            IVisualCollection_Release( children );
        }
        IContainerVisual_Release( container );
    }
    return result;
}

static DWORD WINAPI blit_thread_proc( void *param )
{
    struct composition_target *target = param;
    ID3D11Texture2D *staging = NULL;
    UINT staging_w = 0, staging_h = 0;

    TRACE( "composition blit thread starting for hwnd %p\n", target->hwnd );

    while (!InterlockedCompareExchange( &target->blit_stop, 0, 0 ))
    {
        IUnknown *swapchain_unk;
        IDXGISwapChain *swapchain;
        RECT client;
        HDC hdc;

        Sleep( 33 );

        swapchain_unk = find_swapchain_unknown( target->root );
        if (!swapchain_unk) continue;
        if (!IsWindow( target->hwnd )) break;

        if (FAILED( IUnknown_QueryInterface( swapchain_unk, &IID_IDXGISwapChain, (void **)&swapchain ) ))
            continue;

        {
            ID3D11Texture2D *backbuffer = NULL;
            ID3D11Device *device = NULL;
            ID3D11DeviceContext *context = NULL;
            D3D11_TEXTURE2D_DESC desc;
            D3D11_MAPPED_SUBRESOURCE mapped;

            if (SUCCEEDED( IDXGISwapChain_GetBuffer( swapchain, 0, &IID_ID3D11Texture2D, (void **)&backbuffer ) ))
            {
                ID3D11Texture2D_GetDesc( backbuffer, &desc );
                ID3D11Texture2D_GetDevice( backbuffer, &device );
                if (device)
                {
                    ID3D11Device_GetImmediateContext( device, &context );

                    if (!staging || staging_w != desc.Width || staging_h != desc.Height)
                    {
                        D3D11_TEXTURE2D_DESC sdesc = desc;
                        if (staging) { ID3D11Texture2D_Release( staging ); staging = NULL; }
                        sdesc.Usage = D3D11_USAGE_STAGING;
                        sdesc.BindFlags = 0;
                        sdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                        sdesc.MiscFlags = 0;
                        if (SUCCEEDED( ID3D11Device_CreateTexture2D( device, &sdesc, NULL, &staging ) ))
                        {
                            staging_w = desc.Width;
                            staging_h = desc.Height;
                        }
                    }

                    if (staging && context)
                    {
                        ID3D11DeviceContext_CopyResource( context, (ID3D11Resource *)staging, (ID3D11Resource *)backbuffer );
                        if (SUCCEEDED( ID3D11DeviceContext_Map( context, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &mapped ) ))
                        {
                            if (GetClientRect( target->hwnd, &client ) && (hdc = GetDC( target->hwnd )))
                            {
                                BITMAPINFO bmi;
                                memset( &bmi, 0, sizeof(bmi) );
                                bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
                                bmi.bmiHeader.biWidth = desc.Width;
                                bmi.bmiHeader.biHeight = -(LONG)desc.Height; /* top-down */
                                bmi.bmiHeader.biPlanes = 1;
                                bmi.bmiHeader.biBitCount = 32;
                                bmi.bmiHeader.biCompression = BI_RGB;

                                StretchDIBits( hdc, 0, 0, client.right - client.left, client.bottom - client.top,
                                               0, 0, desc.Width, desc.Height, mapped.pData, &bmi,
                                               DIB_RGB_COLORS, SRCCOPY );
                                ReleaseDC( target->hwnd, hdc );
                            }
                            ID3D11DeviceContext_Unmap( context, (ID3D11Resource *)staging, 0 );
                        }
                    }
                    if (context) ID3D11DeviceContext_Release( context );
                    ID3D11Device_Release( device );
                }
                ID3D11Texture2D_Release( backbuffer );
            }
        }
        IDXGISwapChain_Release( swapchain );
    }

    if (staging) ID3D11Texture2D_Release( staging );
    TRACE( "composition blit thread exiting for hwnd %p\n", target->hwnd );
    return 0;
}

static HRESULT WINAPI target_QueryInterface( ICompositionTarget *iface, REFIID iid, void **out )
{
    struct composition_target *impl = impl_from_ICompositionTarget( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_ICompositionTarget_ ))
    {
        *out = &impl->ICompositionTarget_iface;
        ICompositionTarget_AddRef( &impl->ICompositionTarget_iface );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_IDesktopWindowTarget_ ))
    {
        *out = &impl->IDesktopWindowTarget_iface;
        ICompositionTarget_AddRef( &impl->ICompositionTarget_iface );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_ICompositionSupportsSystemBackdrop_ ))
    {
        *out = &impl->ICompositionSupportsSystemBackdrop_iface;
        ICompositionTarget_AddRef( &impl->ICompositionTarget_iface );
        return S_OK;
    }
    ERR( "composition_target: %s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI target_AddRef( ICompositionTarget *iface ) { return InterlockedIncrement( &impl_from_ICompositionTarget(iface)->ref ); }
static ULONG WINAPI target_Release( ICompositionTarget *iface )
{
    struct composition_target *impl = impl_from_ICompositionTarget( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref)
    {
        if (impl->blit_thread)
        {
            InterlockedExchange( &impl->blit_stop, 1 );
            WaitForSingleObject( impl->blit_thread, 2000 );
            CloseHandle( impl->blit_thread );
        }
        if (impl->root) IVisual_Release( impl->root );
        free( impl );
    }
    return ref;
}
static HRESULT WINAPI target_GetIids( ICompositionTarget *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI target_GetRuntimeClassName( ICompositionTarget *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI target_GetTrustLevel( ICompositionTarget *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI target_get_Root( ICompositionTarget *iface, IVisual **v )
{
    struct composition_target *impl = impl_from_ICompositionTarget( iface );
    *v = impl->root;
    if (*v) IVisual_AddRef( *v );
    return S_OK;
}
static HRESULT WINAPI target_put_Root( ICompositionTarget *iface, IVisual *v )
{
    struct composition_target *impl = impl_from_ICompositionTarget( iface );

    if (impl->root) IVisual_Release( impl->root );
    impl->root = v;
    if (v) IVisual_AddRef( v );

    ERR( "put_Root: hwnd %p, root %p - %s a swapchain to blit.\n", impl->hwnd, v,
         find_swapchain_unknown( v ) ? "found" : "did not find" );

    if (v && !impl->blit_thread)
        impl->blit_thread = CreateThread( NULL, 0, blit_thread_proc, impl, 0, NULL );

    return S_OK;
}

static const struct ICompositionTargetVtbl composition_target_vtbl =
{
    target_QueryInterface, target_AddRef, target_Release,
    target_GetIids, target_GetRuntimeClassName, target_GetTrustLevel,
    target_get_Root, target_put_Root,
};

/* IDesktopWindowTarget is the interface CreateDesktopWindowTarget's out
 * parameter is actually typed as (real callers cast to ICompositionTarget
 * separately to get at Root/SetRoot) - handing back our ICompositionTarget
 * vtable directly here would make the caller's IsTopmost() call land on
 * our get_Root slot instead (wrong signature, stack corruption). */
static HRESULT WINAPI dwt_QueryInterface( IDesktopWindowTarget *iface, REFIID iid, void **out )
{
    return target_QueryInterface( &impl_from_IDesktopWindowTarget(iface)->ICompositionTarget_iface, iid, out );
}
static ULONG WINAPI dwt_AddRef( IDesktopWindowTarget *iface ) { return InterlockedIncrement( &impl_from_IDesktopWindowTarget(iface)->ref ); }
static ULONG WINAPI dwt_Release( IDesktopWindowTarget *iface ) { return target_Release( &impl_from_IDesktopWindowTarget(iface)->ICompositionTarget_iface ); }
static HRESULT WINAPI dwt_GetIids( IDesktopWindowTarget *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI dwt_GetRuntimeClassName( IDesktopWindowTarget *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI dwt_GetTrustLevel( IDesktopWindowTarget *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI dwt_get_IsTopmost( IDesktopWindowTarget *iface, boolean *v ) { *v = impl_from_IDesktopWindowTarget(iface)->topmost; return S_OK; }

static const struct IDesktopWindowTargetVtbl desktop_window_target_vtbl =
{
    dwt_QueryInterface, dwt_AddRef, dwt_Release,
    dwt_GetIids, dwt_GetRuntimeClassName, dwt_GetTrustLevel,
    dwt_get_IsTopmost,
};

static HRESULT WINAPI backdrop_QueryInterface( ICompositionSupportsSystemBackdrop *iface, REFIID iid, void **out )
{
    return target_QueryInterface( &impl_from_ICompositionSupportsSystemBackdrop(iface)->ICompositionTarget_iface, iid, out );
}
static ULONG WINAPI backdrop_AddRef( ICompositionSupportsSystemBackdrop *iface ) { return InterlockedIncrement( &impl_from_ICompositionSupportsSystemBackdrop(iface)->ref ); }
static ULONG WINAPI backdrop_Release( ICompositionSupportsSystemBackdrop *iface ) { return target_Release( &impl_from_ICompositionSupportsSystemBackdrop(iface)->ICompositionTarget_iface ); }
static HRESULT WINAPI backdrop_GetIids( ICompositionSupportsSystemBackdrop *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI backdrop_GetRuntimeClassName( ICompositionSupportsSystemBackdrop *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI backdrop_GetTrustLevel( ICompositionSupportsSystemBackdrop *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI backdrop_get_SystemBackdrop( ICompositionSupportsSystemBackdrop *iface, void **v )
{
    (void)iface;
    *v = NULL; /* no real Mica/Acrylic backdrop support; matches an unset backdrop on real Windows */
    return S_OK;
}
static HRESULT WINAPI backdrop_put_SystemBackdrop( ICompositionSupportsSystemBackdrop *iface, void *v )
{
    (void)iface; (void)v;
    return S_OK;
}

static const struct ICompositionSupportsSystemBackdropVtbl composition_supports_system_backdrop_vtbl =
{
    backdrop_QueryInterface, backdrop_AddRef, backdrop_Release,
    backdrop_GetIids, backdrop_GetRuntimeClassName, backdrop_GetTrustLevel,
    backdrop_get_SystemBackdrop, backdrop_put_SystemBackdrop,
};

static HRESULT create_desktop_window_target( HWND hwnd, BOOL topmost, IDesktopWindowTarget **out )
{
    struct composition_target *impl = calloc( 1, sizeof(*impl) );
    if (!impl) { *out = NULL; return E_OUTOFMEMORY; }
    impl->ICompositionTarget_iface.lpVtbl = &composition_target_vtbl;
    impl->IDesktopWindowTarget_iface.lpVtbl = &desktop_window_target_vtbl;
    impl->ICompositionSupportsSystemBackdrop_iface.lpVtbl = &composition_supports_system_backdrop_vtbl;
    impl->ref = 1;
    impl->hwnd = hwnd;
    impl->topmost = !!topmost;
    *out = &impl->IDesktopWindowTarget_iface;
    return S_OK;
}

/* ============================================================
 * Compositor: activatable runtime class. Real Windows creates it via
 * ActivateInstance; QueryInterface on the resulting instance also hands
 * out the native interop interfaces used to bridge to Win32/DXGI.
 * ============================================================ */
struct compositor
{
    IActivationFactory IActivationFactory_iface; /* only meaningful on the *factory* object */
    ICompositor ICompositor_iface;
    ICompositorInterop ICompositorInterop_iface;
    ICompositorDesktopInterop ICompositorDesktopInterop_iface;
    LONG ref;
};

static inline struct compositor *impl_from_ICompositor( ICompositor *iface )
{
    return CONTAINING_RECORD( iface, struct compositor, ICompositor_iface );
}
static inline struct compositor *impl_from_ICompositorInterop( ICompositorInterop *iface )
{
    return CONTAINING_RECORD( iface, struct compositor, ICompositorInterop_iface );
}
static inline struct compositor *impl_from_ICompositorDesktopInterop( ICompositorDesktopInterop *iface )
{
    return CONTAINING_RECORD( iface, struct compositor, ICompositorDesktopInterop_iface );
}

static HRESULT WINAPI compositor_QueryInterface( ICompositor *iface, REFIID iid, void **out )
{
    struct compositor *impl = impl_from_ICompositor( iface );

    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_ICompositor_ ))
    {
        *out = &impl->ICompositor_iface;
        ICompositor_AddRef( &impl->ICompositor_iface );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_ICompositorInterop_ ))
    {
        *out = &impl->ICompositorInterop_iface;
        ICompositor_AddRef( &impl->ICompositor_iface );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_ICompositorDesktopInterop_ ))
    {
        *out = &impl->ICompositorDesktopInterop_iface;
        ICompositor_AddRef( &impl->ICompositor_iface );
        return S_OK;
    }
    if (IsEqualGUID( iid, &IID_IClosable_ ))
    {
        /* IClosable::Close is slot 6 (after IUnknown/IInspectable); we
         * never actually get called through it since nothing else in
         * this shim needs to close a Compositor, so just decline. */
        *out = NULL;
        return E_NOINTERFACE;
    }
    ERR( "compositor: %s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI compositor_AddRef( ICompositor *iface ) { return InterlockedIncrement( &impl_from_ICompositor(iface)->ref ); }
static ULONG WINAPI compositor_Release( ICompositor *iface )
{
    struct compositor *impl = impl_from_ICompositor( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    if (!ref) free( impl );
    return ref;
}
static HRESULT WINAPI compositor_GetIids( ICompositor *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI compositor_GetRuntimeClassName( ICompositor *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI compositor_GetTrustLevel( ICompositor *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }

static HRESULT WINAPI compositor_CreateColorKeyFrameAnimation( ICompositor *iface, void **r ) { (void)iface; ERR("stub\n"); *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateColorBrush( ICompositor *iface, void **r ) { (void)iface; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateColorBrushWithColor( ICompositor *iface, UINT64 c, void **r ) { (void)iface;(void)c; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateContainerVisual( ICompositor *iface, void **r )
{
    (void)iface;
    return create_container_visual( (IVisual **)r );
}
static HRESULT WINAPI compositor_CreateCubicBezierEasingFunction( ICompositor *iface, UINT64 a, UINT64 b, void **r ) { (void)iface;(void)a;(void)b; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateEffectFactory( ICompositor *iface, void *e, void **r ) { (void)iface;(void)e; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateEffectFactoryWithProperties( ICompositor *iface, void *e, void *p, void **r ) { (void)iface;(void)e;(void)p; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateExpressionAnimation( ICompositor *iface, void **r ) { (void)iface; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateExpressionAnimationWithExpression( ICompositor *iface, HSTRING e, void **r ) { (void)iface;(void)e; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateInsetClip( ICompositor *iface, void **r ) { (void)iface; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateInsetClipWithInsets( ICompositor *iface, float l, float t, float rr, float b, void **r ) { (void)iface;(void)l;(void)t;(void)rr;(void)b; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateLinearEasingFunction( ICompositor *iface, void **r ) { (void)iface; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreatePropertySet( ICompositor *iface, void **r ) { (void)iface; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateQuaternionKeyFrameAnimation( ICompositor *iface, void **r ) { (void)iface; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateScalarKeyFrameAnimation( ICompositor *iface, void **r ) { (void)iface; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateScopedBatch( ICompositor *iface, INT32 t, void **r ) { (void)iface;(void)t; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateSpriteVisual( ICompositor *iface, ISpriteVisual **r )
{
    (void)iface;
    TRACE( "iface %p, result %p.\n", iface, r );
    return create_sprite_visual( r );
}
static HRESULT WINAPI compositor_CreateSurfaceBrush( ICompositor *iface, ICompositionSurfaceBrush **r )
{
    (void)iface;
    return create_composition_surface_brush( NULL, r );
}
static HRESULT WINAPI compositor_CreateSurfaceBrushWithSurface( ICompositor *iface, ICompositionSurface *s, ICompositionSurfaceBrush **r )
{
    (void)iface;
    TRACE( "iface %p, surface %p, result %p.\n", iface, s, r );
    return create_composition_surface_brush( s, r );
}
static HRESULT WINAPI compositor_CreateTargetForCurrentView( ICompositor *iface, void **r ) { (void)iface; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateVector2KeyFrameAnimation( ICompositor *iface, void **r ) { (void)iface; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateVector3KeyFrameAnimation( ICompositor *iface, void **r ) { (void)iface; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_CreateVector4KeyFrameAnimation( ICompositor *iface, void **r ) { (void)iface; *r=NULL; return E_NOTIMPL; }
static HRESULT WINAPI compositor_GetCommitBatch( ICompositor *iface, INT32 t, void **r ) { (void)iface;(void)t; *r=NULL; return E_NOTIMPL; }

static const struct ICompositorVtbl compositor_vtbl =
{
    compositor_QueryInterface, compositor_AddRef, compositor_Release,
    compositor_GetIids, compositor_GetRuntimeClassName, compositor_GetTrustLevel,
    compositor_CreateColorKeyFrameAnimation,
    compositor_CreateColorBrush,
    compositor_CreateColorBrushWithColor,
    compositor_CreateContainerVisual,
    compositor_CreateCubicBezierEasingFunction,
    compositor_CreateEffectFactory,
    compositor_CreateEffectFactoryWithProperties,
    compositor_CreateExpressionAnimation,
    compositor_CreateExpressionAnimationWithExpression,
    compositor_CreateInsetClip,
    compositor_CreateInsetClipWithInsets,
    compositor_CreateLinearEasingFunction,
    compositor_CreatePropertySet,
    compositor_CreateQuaternionKeyFrameAnimation,
    compositor_CreateScalarKeyFrameAnimation,
    compositor_CreateScopedBatch,
    compositor_CreateSpriteVisual,
    compositor_CreateSurfaceBrush,
    compositor_CreateSurfaceBrushWithSurface,
    compositor_CreateTargetForCurrentView,
    compositor_CreateVector2KeyFrameAnimation,
    compositor_CreateVector3KeyFrameAnimation,
    compositor_CreateVector4KeyFrameAnimation,
    compositor_GetCommitBatch,
};

static HRESULT WINAPI ci_QueryInterface( ICompositorInterop *iface, REFIID iid, void **out )
{
    return compositor_QueryInterface( &impl_from_ICompositorInterop(iface)->ICompositor_iface, iid, out );
}
static ULONG WINAPI ci_AddRef( ICompositorInterop *iface ) { return InterlockedIncrement( &impl_from_ICompositorInterop(iface)->ref ); }
static ULONG WINAPI ci_Release( ICompositorInterop *iface ) { return compositor_Release( &impl_from_ICompositorInterop(iface)->ICompositor_iface ); }
static HRESULT WINAPI ci_CreateCompositionSurfaceForHandle( ICompositorInterop *iface, HANDLE h, ICompositionSurface **out )
{
    (void)iface;
    TRACE( "iface %p, handle %p, out %p.\n", iface, h, out );
    return create_composition_surface_for_unknown( NULL, out );
}
static HRESULT WINAPI ci_CreateCompositionSurfaceForSwapChain( ICompositorInterop *iface, IUnknown *swapchain, ICompositionSurface **out )
{
    (void)iface;
    ERR( "CreateCompositionSurfaceForSwapChain: iface %p, swapchain %p, out %p.\n", iface, swapchain, out );
    return create_composition_surface_for_unknown( swapchain, out );
}
static HRESULT WINAPI ci_CreateGraphicsDevice( ICompositorInterop *iface, IUnknown *renderer, void **out )
{
    (void)iface; (void)renderer;
    *out = NULL;
    return E_NOTIMPL;
}

static const struct ICompositorInteropVtbl compositor_interop_vtbl =
{
    ci_QueryInterface, ci_AddRef, ci_Release,
    ci_CreateCompositionSurfaceForHandle,
    ci_CreateCompositionSurfaceForSwapChain,
    ci_CreateGraphicsDevice,
};

static HRESULT WINAPI cdi_QueryInterface( ICompositorDesktopInterop *iface, REFIID iid, void **out )
{
    return compositor_QueryInterface( &impl_from_ICompositorDesktopInterop(iface)->ICompositor_iface, iid, out );
}
static ULONG WINAPI cdi_AddRef( ICompositorDesktopInterop *iface ) { return InterlockedIncrement( &impl_from_ICompositorDesktopInterop(iface)->ref ); }
static ULONG WINAPI cdi_Release( ICompositorDesktopInterop *iface ) { return compositor_Release( &impl_from_ICompositorDesktopInterop(iface)->ICompositor_iface ); }
static HRESULT WINAPI cdi_CreateDesktopWindowTarget( ICompositorDesktopInterop *iface, HWND hwnd, BOOL topmost, IDesktopWindowTarget **out )
{
    (void)iface;
    ERR( "CreateDesktopWindowTarget: iface %p, hwnd %p, topmost %d.\n", iface, hwnd, topmost );
    return create_desktop_window_target( hwnd, topmost, out );
}
static HRESULT WINAPI cdi_EnsureOnThread( ICompositorDesktopInterop *iface, DWORD tid ) { (void)iface;(void)tid; return S_OK; }

static const struct ICompositorDesktopInteropVtbl compositor_desktop_interop_vtbl =
{
    cdi_QueryInterface, cdi_AddRef, cdi_Release,
    cdi_CreateDesktopWindowTarget, cdi_EnsureOnThread,
};

/* --- activation factory: creates new Compositor instances --- */
struct compositor_statics
{
    IActivationFactory IActivationFactory_iface;
    LONG ref;
};

static inline struct compositor_statics *impl_from_compositor_factory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct compositor_statics, IActivationFactory_iface );
}

static HRESULT WINAPI compositor_factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct compositor_statics *impl = impl_from_compositor_factory( iface );
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IActivationFactory ))
    {
        *out = &impl->IActivationFactory_iface;
        IInspectable_AddRef( (IInspectable *)*out );
        return S_OK;
    }
    ERR( "compositor_factory: %s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI compositor_factory_AddRef( IActivationFactory *iface ) { return InterlockedIncrement( &impl_from_compositor_factory(iface)->ref ); }
static ULONG WINAPI compositor_factory_Release( IActivationFactory *iface ) { return InterlockedDecrement( &impl_from_compositor_factory(iface)->ref ); }
static HRESULT WINAPI compositor_factory_GetIids( IActivationFactory *iface, ULONG *c, IID **i ) { (void)iface;(void)c;(void)i; return E_NOTIMPL; }
static HRESULT WINAPI compositor_factory_GetRuntimeClassName( IActivationFactory *iface, HSTRING *n ) { (void)iface;(void)n; return E_NOTIMPL; }
static HRESULT WINAPI compositor_factory_GetTrustLevel( IActivationFactory *iface, TrustLevel *t ) { (void)iface; *t = BaseTrust; return S_OK; }
static HRESULT WINAPI compositor_factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    struct compositor *impl;
    (void)iface;

    TRACE( "iface %p, instance %p.\n", iface, instance );

    if (!(impl = calloc( 1, sizeof(*impl) )))
    {
        *instance = NULL;
        return E_OUTOFMEMORY;
    }
    impl->ICompositor_iface.lpVtbl = &compositor_vtbl;
    impl->ICompositorInterop_iface.lpVtbl = &compositor_interop_vtbl;
    impl->ICompositorDesktopInterop_iface.lpVtbl = &compositor_desktop_interop_vtbl;
    impl->ref = 1;

    *instance = (IInspectable *)&impl->ICompositor_iface;
    return S_OK;
}

static const struct IActivationFactoryVtbl compositor_factory_vtbl =
{
    compositor_factory_QueryInterface, compositor_factory_AddRef, compositor_factory_Release,
    compositor_factory_GetIids, compositor_factory_GetRuntimeClassName, compositor_factory_GetTrustLevel,
    compositor_factory_ActivateInstance,
};

static struct compositor_statics compositor_statics = { {&compositor_factory_vtbl}, 1 };

IActivationFactory *compositor_factory = &compositor_statics.IActivationFactory_iface;
