/* WinRT Windows.Gaming.Input implementation
 *
 * Copyright 2021 Rémi Bernon for CodeWeavers
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
#include "provider.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(input);

#define WIDL_impl_controller
#define WIDL_impl_controller_statics
#include "classes_impl.h"

static CRITICAL_SECTION controller_cs;
static CRITICAL_SECTION_DEBUG controller_cs_debug =
{
    0, 0, &controller_cs,
    { &controller_cs_debug.ProcessLocksList, &controller_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": controller_cs") }
};
static CRITICAL_SECTION controller_cs = { &controller_cs_debug, -1, 0, 0, 0, 0 };

static IVector_RawGameController *controllers;
static struct list controller_added_handlers = LIST_INIT( controller_added_handlers );
static struct list controller_removed_handlers = LIST_INIT( controller_removed_handlers );

static HRESULT init_controllers(void)
{
    static const struct vector_iids iids =
    {
        .vector = &IID_IVector_RawGameController,
        .view = &IID_IVectorView_RawGameController,
        .iterable = &IID_IIterable_RawGameController,
        .iterator = &IID_IIterator_RawGameController,
    };
    HRESULT hr;

    EnterCriticalSection( &controller_cs );
    if (controllers) hr = S_OK;
    else hr = vector_create( &iids, (void **)&controllers );
    LeaveCriticalSection( &controller_cs );

    return hr;
}

struct controller
{
    struct controller_klass klass;
    IGameControllerProvider *provider;
    IWineGameControllerProvider *wine_provider;
};

static struct controller *controller_from_klass( struct controller_klass *klass )
{
    return CONTAINING_RECORD( klass, struct controller, klass );
}

static HRESULT controller_missing_interface( struct controller *impl, REFIID iid, void **out )
{
    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static void controller_destroy( struct controller *impl )
{
    if (impl->wine_provider) IWineGameControllerProvider_Release( impl->wine_provider );
    IGameControllerProvider_Release( impl->provider );
    free( impl );
}

static HRESULT WINAPI controller_Initialize( IGameControllerImpl *iface, IGameController *outer,
                                             IGameControllerProvider *provider )
{
    struct controller *impl = controller_from_IGameControllerImpl( iface );
    HRESULT hr;

    TRACE( "iface %p, outer %p, provider %p.\n", iface, outer, provider );

    impl->klass.IGameController_outer = outer;
    IGameControllerProvider_AddRef( (impl->provider = provider) );

    hr = IGameControllerProvider_QueryInterface( provider, &IID_IWineGameControllerProvider,
                                                 (void **)&impl->wine_provider );
    if (FAILED(hr)) return hr;

    EnterCriticalSection( &controller_cs );
    if (SUCCEEDED(hr = init_controllers()))
        hr = IVector_RawGameController_Append( controllers, &impl->klass.IRawGameController_iface );
    LeaveCriticalSection( &controller_cs );

    return hr;
}

static const struct IGameControllerImplVtbl controller_vtbl =
{
    controller_IGameControllerImpl_QueryInterface,
    controller_IGameControllerImpl_AddRef,
    controller_IGameControllerImpl_Release,
    /* IInspectable methods */
    controller_IGameControllerImpl_GetIids,
    controller_IGameControllerImpl_GetRuntimeClassName,
    controller_IGameControllerImpl_GetTrustLevel,
    /* IGameControllerImpl methods */
    controller_Initialize,
};

static HRESULT WINAPI input_sink_OnInputResumed( IGameControllerInputSink *iface, UINT64 timestamp )
{
    FIXME( "iface %p, timestamp %I64u stub!\n", iface, timestamp );
    return E_NOTIMPL;
}

static HRESULT WINAPI input_sink_OnInputSuspended( IGameControllerInputSink *iface, UINT64 timestamp )
{
    FIXME( "iface %p, timestamp %I64u stub!\n", iface, timestamp );
    return E_NOTIMPL;
}

static const struct IGameControllerInputSinkVtbl input_sink_vtbl =
{
    controller_IGameControllerInputSink_QueryInterface,
    controller_IGameControllerInputSink_AddRef,
    controller_IGameControllerInputSink_Release,
    /* IInspectable methods */
    controller_IGameControllerInputSink_GetIids,
    controller_IGameControllerInputSink_GetRuntimeClassName,
    controller_IGameControllerInputSink_GetTrustLevel,
    /* IGameControllerInputSink methods */
    input_sink_OnInputResumed,
    input_sink_OnInputSuspended,
};

static HRESULT WINAPI raw_controller_get_AxisCount( IRawGameController *iface, INT32 *value )
{
    struct controller *impl = controller_from_IRawGameController( iface );
    return IWineGameControllerProvider_get_AxisCount( impl->wine_provider, value );
}

static HRESULT WINAPI raw_controller_get_ButtonCount( IRawGameController *iface, INT32 *value )
{
    struct controller *impl = controller_from_IRawGameController( iface );
    return IWineGameControllerProvider_get_ButtonCount( impl->wine_provider, value );
}

static HRESULT WINAPI raw_controller_get_ForceFeedbackMotors( IRawGameController *iface, IVectorView_ForceFeedbackMotor **value )
{
    static const struct vector_iids iids =
    {
        .vector = &IID_IVector_ForceFeedbackMotor,
        .view = &IID_IVectorView_ForceFeedbackMotor,
        .iterable = &IID_IIterable_ForceFeedbackMotor,
        .iterator = &IID_IIterator_ForceFeedbackMotor,
    };
    struct controller *impl = controller_from_IRawGameController( iface );
    IVector_ForceFeedbackMotor *vector;
    IForceFeedbackMotor *motor;
    HRESULT hr;

    TRACE( "iface %p, value %p\n", iface, value );

    if (FAILED(hr = vector_create( &iids, (void **)&vector ))) return hr;

    if (SUCCEEDED(IWineGameControllerProvider_get_ForceFeedbackMotor( impl->wine_provider, &motor )) && motor)
    {
        hr = IVector_ForceFeedbackMotor_Append( vector, motor );
        IForceFeedbackMotor_Release( motor );
    }

    if (SUCCEEDED(hr)) hr = IVector_ForceFeedbackMotor_GetView( vector, value );
    IVector_ForceFeedbackMotor_Release( vector );

    return hr;
}

static HRESULT WINAPI raw_controller_get_HardwareProductId( IRawGameController *iface, UINT16 *value )
{
    struct controller *impl = controller_from_IRawGameController( iface );
    return IGameControllerProvider_get_HardwareProductId( impl->provider, value );
}

static HRESULT WINAPI raw_controller_get_HardwareVendorId( IRawGameController *iface, UINT16 *value )
{
    struct controller *impl = controller_from_IRawGameController( iface );
    return IGameControllerProvider_get_HardwareVendorId( impl->provider, value );
}

static HRESULT WINAPI raw_controller_get_SwitchCount( IRawGameController *iface, INT32 *value )
{
    struct controller *impl = controller_from_IRawGameController( iface );
    return IWineGameControllerProvider_get_SwitchCount( impl->wine_provider, value );
}

static HRESULT WINAPI raw_controller_GetButtonLabel( IRawGameController *iface, INT32 index,
                                                     enum GameControllerButtonLabel *value )
{
    FIXME( "iface %p, index %d, value %p stub!\n", iface, index, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI raw_controller_GetCurrentReading( IRawGameController *iface, UINT32 buttons_size, BOOLEAN *buttons,
                                                        UINT32 switches_size, enum GameControllerSwitchPosition *switches,
                                                        UINT32 axes_size, DOUBLE *axes, UINT64 *timestamp )
{
    struct controller *impl = controller_from_IRawGameController( iface );
    WineGameControllerState state;
    HRESULT hr;

    TRACE( "iface %p, buttons_size %u, buttons %p, switches_size %u, switches %p, axes_size %u, axes %p, timestamp %p.\n",
           iface, buttons_size, buttons, switches_size, switches, axes_size, axes, timestamp );

    if (FAILED(hr = IWineGameControllerProvider_get_State( impl->wine_provider, &state ))) return hr;

    memcpy( axes, state.axes, axes_size * sizeof(*axes) );
    memcpy( buttons, state.buttons, buttons_size * sizeof(*buttons) );
    memcpy( switches, state.switches, switches_size * sizeof(*switches) );
    *timestamp = state.timestamp;

    return hr;
}

static HRESULT WINAPI raw_controller_GetSwitchKind( IRawGameController *iface, INT32 index, enum GameControllerSwitchKind *value )
{
    FIXME( "iface %p, index %d, value %p stub!\n", iface, index, value );
    return E_NOTIMPL;
}

static const struct IRawGameControllerVtbl raw_controller_vtbl =
{
    controller_IRawGameController_QueryInterface,
    controller_IRawGameController_AddRef,
    controller_IRawGameController_Release,
    /* IInspectable methods */
    controller_IRawGameController_GetIids,
    controller_IRawGameController_GetRuntimeClassName,
    controller_IRawGameController_GetTrustLevel,
    /* IRawGameController methods */
    raw_controller_get_AxisCount,
    raw_controller_get_ButtonCount,
    raw_controller_get_ForceFeedbackMotors,
    raw_controller_get_HardwareProductId,
    raw_controller_get_HardwareVendorId,
    raw_controller_get_SwitchCount,
    raw_controller_GetButtonLabel,
    raw_controller_GetCurrentReading,
    raw_controller_GetSwitchKind,
};

static HRESULT WINAPI raw_controller_2_get_SimpleHapticsControllers( IRawGameController2 *iface, IVectorView_SimpleHapticsController** value)
{
    static const struct vector_iids iids =
    {
        .vector = &IID_IVector_SimpleHapticsController,
        .view = &IID_IVectorView_SimpleHapticsController,
        .iterable = &IID_IIterable_SimpleHapticsController,
        .iterator = &IID_IIterator_SimpleHapticsController,
    };
    IVector_SimpleHapticsController *vector;
    HRESULT hr;

    FIXME( "iface %p, value %p stub!\n", iface, value );

    if (SUCCEEDED(hr = vector_create( &iids, (void **)&vector )))
    {
        hr = IVector_SimpleHapticsController_GetView( vector, value );
        IVector_SimpleHapticsController_Release( vector );
    }

    return hr;
}

static HRESULT WINAPI raw_controller_2_get_NonRoamableId( IRawGameController2 *iface, HSTRING* value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI raw_controller_2_get_DisplayName( IRawGameController2 *iface, HSTRING* value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static const struct IRawGameController2Vtbl raw_controller_2_vtbl =
{
    controller_IRawGameController2_QueryInterface,
    controller_IRawGameController2_AddRef,
    controller_IRawGameController2_Release,
    /* IInspectable methods */
    controller_IRawGameController2_GetIids,
    controller_IRawGameController2_GetRuntimeClassName,
    controller_IRawGameController2_GetTrustLevel,
    /* IRawGameController2 methods */
    raw_controller_2_get_SimpleHapticsControllers,
    raw_controller_2_get_NonRoamableId,
    raw_controller_2_get_DisplayName,
};

static const struct controller_funcs controller_funcs = CONTROLLER_FUNCS_INIT;

static HRESULT controller_statics_missing_interface( struct controller_statics *impl, REFIID iid, void **out )
{
    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static void controller_statics_destroy( struct controller_statics *impl )
{
}

static HRESULT WINAPI factory_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    FIXME( "iface %p, instance %p stub!\n", iface, instance );
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl factory_vtbl =
{
    controller_statics_IActivationFactory_QueryInterface,
    controller_statics_IActivationFactory_AddRef,
    controller_statics_IActivationFactory_Release,
    /* IInspectable methods */
    controller_statics_IActivationFactory_GetIids,
    controller_statics_IActivationFactory_GetRuntimeClassName,
    controller_statics_IActivationFactory_GetTrustLevel,
    /* IActivationFactory methods */
    factory_ActivateInstance,
};

static HRESULT WINAPI statics_add_RawGameControllerAdded( IRawGameControllerStatics *iface,
                                                          IEventHandler_RawGameController *handler,
                                                          EventRegistrationToken *token )
{
    TRACE( "iface %p, handler %p, token %p.\n", iface, handler, token );
    if (!handler) return E_INVALIDARG;
    return event_handlers_append( &controller_added_handlers, (IEventHandler_IInspectable *)handler, token );
}

static HRESULT WINAPI statics_remove_RawGameControllerAdded( IRawGameControllerStatics *iface, EventRegistrationToken token )
{
    TRACE( "iface %p, token %#I64x.\n", iface, token.value );
    return event_handlers_remove( &controller_added_handlers, &token );
}

static HRESULT WINAPI statics_add_RawGameControllerRemoved( IRawGameControllerStatics *iface,
                                                            IEventHandler_RawGameController *handler,
                                                            EventRegistrationToken *token )
{
    TRACE( "iface %p, handler %p, token %p.\n", iface, handler, token );
    if (!handler) return E_INVALIDARG;
    return event_handlers_append( &controller_removed_handlers, (IEventHandler_IInspectable *)handler, token );
}

static HRESULT WINAPI statics_remove_RawGameControllerRemoved( IRawGameControllerStatics *iface, EventRegistrationToken token )
{
    TRACE( "iface %p, token %#I64x.\n", iface, token.value );
    return event_handlers_remove( &controller_removed_handlers, &token );
}

static HRESULT WINAPI statics_get_RawGameControllers( IRawGameControllerStatics *iface, IVectorView_RawGameController **value )
{
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    EnterCriticalSection( &controller_cs );
    if (SUCCEEDED(hr = init_controllers()))
        hr = IVector_RawGameController_GetView( controllers, value );
    LeaveCriticalSection( &controller_cs );

    return hr;
}

static HRESULT WINAPI statics_FromGameController( IRawGameControllerStatics *iface, IGameController *game_controller,
                                                  IRawGameController **value )
{
    struct controller_statics *impl = controller_statics_from_IRawGameControllerStatics( iface );
    IGameController *controller;
    HRESULT hr;

    TRACE( "iface %p, game_controller %p, value %p.\n", iface, game_controller, value );

    *value = NULL;
    hr = IGameControllerFactoryManagerStatics2_TryGetFactoryControllerFromGameController( manager_factory, &impl->ICustomGameControllerFactory_iface,
                                                                                          game_controller, &controller );
    if (FAILED(hr) || !controller) return hr;

    hr = IGameController_QueryInterface( controller, &IID_IRawGameController, (void **)value );
    IGameController_Release( controller );

    return hr;
}

static const struct IRawGameControllerStaticsVtbl statics_vtbl =
{
    controller_statics_IRawGameControllerStatics_QueryInterface,
    controller_statics_IRawGameControllerStatics_AddRef,
    controller_statics_IRawGameControllerStatics_Release,
    /* IInspectable methods */
    controller_statics_IRawGameControllerStatics_GetIids,
    controller_statics_IRawGameControllerStatics_GetRuntimeClassName,
    controller_statics_IRawGameControllerStatics_GetTrustLevel,
    /* IRawGameControllerStatics methods */
    statics_add_RawGameControllerAdded,
    statics_remove_RawGameControllerAdded,
    statics_add_RawGameControllerRemoved,
    statics_remove_RawGameControllerRemoved,
    statics_get_RawGameControllers,
    statics_FromGameController,
};

static HRESULT WINAPI controller_factory_CreateGameController( ICustomGameControllerFactory *iface, IGameControllerProvider *provider,
                                                               IInspectable **value )
{
    struct controller *impl;

    TRACE( "iface %p, provider %p, value %p.\n", iface, provider, value );

    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->klass.IGameControllerImpl_iface.lpVtbl = &controller_vtbl;
    impl->klass.IGameControllerInputSink_iface.lpVtbl = &input_sink_vtbl;
    impl->klass.IRawGameController_iface.lpVtbl = &raw_controller_vtbl;
    impl->klass.IRawGameController2_iface.lpVtbl = &raw_controller_2_vtbl;
    impl->klass.class_name = RuntimeClass_Windows_Gaming_Input_RawGameController;
    impl->klass.ref = 1;

    TRACE( "created RawGameController %p\n", impl );

    *value = (IInspectable *)&impl->klass.IGameControllerImpl_iface;
    return S_OK;
}

static HRESULT WINAPI controller_factory_OnGameControllerAdded( ICustomGameControllerFactory *iface, IGameController *value )
{
    IRawGameController *controller;
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    if (FAILED(hr = IGameController_QueryInterface( value, &IID_IRawGameController, (void **)&controller )))
        return hr;
    event_handlers_notify( &controller_added_handlers, (IInspectable *)controller );
    IRawGameController_Release( controller );

    return S_OK;
}

static HRESULT WINAPI controller_factory_OnGameControllerRemoved( ICustomGameControllerFactory *iface, IGameController *value )
{
    IRawGameController *controller;
    BOOLEAN found;
    UINT32 index;
    HRESULT hr;

    TRACE( "iface %p, value %p.\n", iface, value );

    if (FAILED(hr = IGameController_QueryInterface( value, &IID_IRawGameController, (void **)&controller )))
        return hr;

    EnterCriticalSection( &controller_cs );
    if (SUCCEEDED(hr = init_controllers()))
    {
        if (FAILED(hr = IVector_RawGameController_IndexOf( controllers, controller, &index, &found )) || !found)
            WARN( "Could not find controller %p, hr %#lx!\n", controller, hr );
        else
            hr = IVector_RawGameController_RemoveAt( controllers, index );
    }
    LeaveCriticalSection( &controller_cs );

    if (FAILED(hr))
        WARN( "Failed to remove controller %p, hr %#lx!\n", controller, hr );
    else if (found)
    {
        TRACE( "Removed controller %p.\n", controller );
        event_handlers_notify( &controller_removed_handlers, (IInspectable *)controller );
    }
    IRawGameController_Release( controller );

    return S_OK;
}

static const struct ICustomGameControllerFactoryVtbl controller_factory_vtbl =
{
    controller_statics_ICustomGameControllerFactory_QueryInterface,
    controller_statics_ICustomGameControllerFactory_AddRef,
    controller_statics_ICustomGameControllerFactory_Release,
    /* IInspectable methods */
    controller_statics_ICustomGameControllerFactory_GetIids,
    controller_statics_ICustomGameControllerFactory_GetRuntimeClassName,
    controller_statics_ICustomGameControllerFactory_GetTrustLevel,
    /* ICustomGameControllerFactory methods */
    controller_factory_CreateGameController,
    controller_factory_OnGameControllerAdded,
    controller_factory_OnGameControllerRemoved,
};

static const struct IAgileObjectVtbl controller_statics_agile_vtbl =
{
    controller_statics_IAgileObject_QueryInterface,
    controller_statics_IAgileObject_AddRef,
    controller_statics_IAgileObject_Release,
};

static const struct controller_statics_funcs controller_statics_funcs = CONTROLLER_STATICS_FUNCS_INIT;
static struct controller_statics controller_statics =
{
    {&factory_vtbl},
    {&statics_vtbl},
    {&controller_factory_vtbl},
    {&controller_statics_agile_vtbl},
    1,
};

ICustomGameControllerFactory *controller_factory = &controller_statics.ICustomGameControllerFactory_iface;
