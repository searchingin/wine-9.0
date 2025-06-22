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

static HRESULT controller_Initialize( struct controller *impl, IGameController *outer, IGameControllerProvider *provider )
{
    HRESULT hr;

    TRACE( "controller %p, outer %p, provider %p.\n", impl, outer, provider );

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

static HRESULT controller_OnInputResumed( struct controller *impl, UINT64 timestamp )
{
    FIXME( "controller %p, timestamp %I64u stub!\n", impl, timestamp );
    return E_NOTIMPL;
}

static HRESULT controller_OnInputSuspended( struct controller *impl, UINT64 timestamp )
{
    FIXME( "controller %p, timestamp %I64u stub!\n", impl, timestamp );
    return E_NOTIMPL;
}

static HRESULT controller_get_AxisCount( struct controller *impl, INT32 *value )
{
    return IWineGameControllerProvider_get_AxisCount( impl->wine_provider, value );
}

static HRESULT controller_get_ButtonCount( struct controller *impl, INT32 *value )
{
    return IWineGameControllerProvider_get_ButtonCount( impl->wine_provider, value );
}

static HRESULT controller_get_ForceFeedbackMotors( struct controller *impl, IVectorView_ForceFeedbackMotor **value )
{
    static const struct vector_iids iids =
    {
        .vector = &IID_IVector_ForceFeedbackMotor,
        .view = &IID_IVectorView_ForceFeedbackMotor,
        .iterable = &IID_IIterable_ForceFeedbackMotor,
        .iterator = &IID_IIterator_ForceFeedbackMotor,
    };
    IVector_ForceFeedbackMotor *vector;
    IForceFeedbackMotor *motor;
    HRESULT hr;

    TRACE( "controller %p, value %p\n", impl, value );

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

static HRESULT controller_get_HardwareProductId( struct controller *impl, UINT16 *value )
{
    return IGameControllerProvider_get_HardwareProductId( impl->provider, value );
}

static HRESULT controller_get_HardwareVendorId( struct controller *impl, UINT16 *value )
{
    return IGameControllerProvider_get_HardwareVendorId( impl->provider, value );
}

static HRESULT controller_get_SwitchCount( struct controller *impl, INT32 *value )
{
    return IWineGameControllerProvider_get_SwitchCount( impl->wine_provider, value );
}

static HRESULT controller_GetButtonLabel( struct controller *impl, INT32 index, enum GameControllerButtonLabel *value )
{
    FIXME( "controller %p, index %d, value %p stub!\n", impl, index, value );
    return E_NOTIMPL;
}

static HRESULT controller_GetCurrentReading( struct controller *impl, UINT32 buttons_size, BOOLEAN *buttons,
                                             UINT32 switches_size, enum GameControllerSwitchPosition *switches,
                                             UINT32 axes_size, DOUBLE *axes, UINT64 *timestamp )
{
    WineGameControllerState state;
    HRESULT hr;

    TRACE( "controller %p, buttons_size %u, buttons %p, switches_size %u, switches %p, axes_size %u, axes %p, timestamp %p.\n",
           impl, buttons_size, buttons, switches_size, switches, axes_size, axes, timestamp );

    if (FAILED(hr = IWineGameControllerProvider_get_State( impl->wine_provider, &state ))) return hr;

    memcpy( axes, state.axes, axes_size * sizeof(*axes) );
    memcpy( buttons, state.buttons, buttons_size * sizeof(*buttons) );
    memcpy( switches, state.switches, switches_size * sizeof(*switches) );
    *timestamp = state.timestamp;

    return hr;
}

static HRESULT controller_GetSwitchKind( struct controller *impl, INT32 index, enum GameControllerSwitchKind *value )
{
    FIXME( "controller %p, index %d, value %p stub!\n", impl, index, value );
    return E_NOTIMPL;
}

static HRESULT controller_get_SimpleHapticsControllers( struct controller *impl, IVectorView_SimpleHapticsController **value )
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

    FIXME( "controller %p, value %p stub!\n", impl, value );

    if (SUCCEEDED(hr = vector_create( &iids, (void **)&vector )))
    {
        hr = IVector_SimpleHapticsController_GetView( vector, value );
        IVector_SimpleHapticsController_Release( vector );
    }

    return hr;
}

static HRESULT controller_get_NonRoamableId( struct controller *impl, HSTRING *value )
{
    FIXME( "controller %p, value %p stub!\n", impl, value );
    return E_NOTIMPL;
}

static HRESULT controller_get_DisplayName( struct controller *impl, HSTRING *value )
{
    FIXME( "controller %p, value %p stub!\n", impl, value );
    return E_NOTIMPL;
}

static const struct controller_funcs controller_funcs = CONTROLLER_FUNCS_INIT;

static HRESULT controller_statics_missing_interface( struct controller_statics *statics, REFIID iid, void **out )
{
    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static void controller_statics_destroy( struct controller_statics *statics )
{
}

static HRESULT controller_statics_ActivateInstance( struct controller_statics *statics, IInspectable **instance )
{
    FIXME( "statics %p, instance %p stub!\n", statics, instance );
    return E_NOTIMPL;
}

static HRESULT controller_statics_add_RawGameControllerAdded( struct controller_statics *statics, IEventHandler_RawGameController *handler,
                                                              EventRegistrationToken *token )
{
    TRACE( "statics %p, handler %p, token %p.\n", statics, handler, token );
    if (!handler) return E_INVALIDARG;
    return event_handlers_append( &controller_added_handlers, (IEventHandler_IInspectable *)handler, token );
}

static HRESULT controller_statics_remove_RawGameControllerAdded( struct controller_statics *statics, EventRegistrationToken token )
{
    TRACE( "statics %p, token %#I64x.\n", statics, token.value );
    return event_handlers_remove( &controller_added_handlers, &token );
}

static HRESULT controller_statics_add_RawGameControllerRemoved( struct controller_statics *statics, IEventHandler_RawGameController *handler,
                                                                EventRegistrationToken *token )
{
    TRACE( "statics %p, handler %p, token %p.\n", statics, handler, token );
    if (!handler) return E_INVALIDARG;
    return event_handlers_append( &controller_removed_handlers, (IEventHandler_IInspectable *)handler, token );
}

static HRESULT controller_statics_remove_RawGameControllerRemoved( struct controller_statics *statics, EventRegistrationToken token )
{
    TRACE( "statics %p, token %#I64x.\n", statics, token.value );
    return event_handlers_remove( &controller_removed_handlers, &token );
}

static HRESULT controller_statics_get_RawGameControllers( struct controller_statics *statics, IVectorView_RawGameController **value )
{
    HRESULT hr;

    TRACE( "statics %p, value %p.\n", statics, value );

    EnterCriticalSection( &controller_cs );
    if (SUCCEEDED(hr = init_controllers()))
        hr = IVector_RawGameController_GetView( controllers, value );
    LeaveCriticalSection( &controller_cs );

    return hr;
}

static HRESULT controller_statics_FromGameController( struct controller_statics *statics, IGameController *game_controller, IRawGameController **value )
{
    IGameController *controller;
    HRESULT hr;

    TRACE( "statics %p, game_controller %p, value %p.\n", statics, game_controller, value );

    *value = NULL;
    hr = IGameControllerFactoryManagerStatics2_TryGetFactoryControllerFromGameController( manager_factory, &statics->ICustomGameControllerFactory_iface,
                                                                                          game_controller, &controller );
    if (FAILED(hr) || !controller) return hr;

    hr = IGameController_QueryInterface( controller, &IID_IRawGameController, (void **)value );
    IGameController_Release( controller );

    return hr;
}

static HRESULT controller_statics_CreateGameController( struct controller_statics *statics, IGameControllerProvider *provider, IInspectable **value )
{
    struct controller *impl;

    TRACE( "statics %p, provider %p, value %p.\n", statics, provider, value );

    if (!(impl = calloc( 1, sizeof(*impl) ))) return E_OUTOFMEMORY;
    impl->klass.IGameControllerImpl_iface.lpVtbl = &controller_IGameControllerImpl_vtbl;
    impl->klass.IGameControllerInputSink_iface.lpVtbl = &controller_IGameControllerInputSink_vtbl;
    impl->klass.IRawGameController_iface.lpVtbl = &controller_IRawGameController_vtbl;
    impl->klass.IRawGameController2_iface.lpVtbl = &controller_IRawGameController2_vtbl;
    impl->klass.class_name = RuntimeClass_Windows_Gaming_Input_RawGameController;
    impl->klass.ref = 1;

    TRACE( "created RawGameController %p\n", statics );

    *value = (IInspectable *)&impl->klass.IGameControllerImpl_iface;
    return S_OK;
}

static HRESULT controller_statics_OnGameControllerAdded( struct controller_statics *statics, IGameController *value )
{
    IRawGameController *controller;
    HRESULT hr;

    TRACE( "statics %p, value %p.\n", statics, value );

    if (FAILED(hr = IGameController_QueryInterface( value, &IID_IRawGameController, (void **)&controller )))
        return hr;
    event_handlers_notify( &controller_added_handlers, (IInspectable *)controller );
    IRawGameController_Release( controller );

    return S_OK;
}

static HRESULT controller_statics_OnGameControllerRemoved( struct controller_statics *statics, IGameController *value )
{
    IRawGameController *controller;
    BOOLEAN found;
    UINT32 index;
    HRESULT hr;

    TRACE( "statics %p, value %p.\n", statics, value );

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
        TRACE( "Removed statics %p.\n", controller );
        event_handlers_notify( &controller_removed_handlers, (IInspectable *)controller );
    }
    IRawGameController_Release( controller );

    return S_OK;
}

static const struct controller_statics_funcs controller_statics_funcs = CONTROLLER_STATICS_FUNCS_INIT;
static struct controller_statics controller_statics =
{
    {&controller_statics_IActivationFactory_vtbl},
    {&controller_statics_IRawGameControllerStatics_vtbl},
    {&controller_statics_ICustomGameControllerFactory_vtbl},
    {&controller_statics_IAgileObject_vtbl},
    1,
};

ICustomGameControllerFactory *controller_factory = &controller_statics.ICustomGameControllerFactory_iface;
