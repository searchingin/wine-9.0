/* WinRT Windows.Devices.Bluetooth BluetoothAdapter Implementation
 *
 * Copyright (C) 2023 Mohamad Al-Jaf
 * Copyright (C) 2025 Vibhav Pant
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
#include "setupapi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(bluetooth);

struct bluetoothadapter_statics
{
    IActivationFactory IActivationFactory_iface;
    IBluetoothAdapterStatics IBluetoothAdapterStatics_iface;
    LONG ref;
};

static inline struct bluetoothadapter_statics *impl_from_IActivationFactory( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct bluetoothadapter_statics, IActivationFactory_iface );
}

static HRESULT WINAPI factory_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    struct bluetoothadapter_statics *impl = impl_from_IActivationFactory( iface );

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

    if (IsEqualGUID( iid, &IID_IBluetoothAdapterStatics ))
    {
        *out = &impl->IBluetoothAdapterStatics_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef( IActivationFactory *iface )
{
    struct bluetoothadapter_statics *impl = impl_from_IActivationFactory( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI factory_Release( IActivationFactory *iface )
{
    struct bluetoothadapter_statics *impl = impl_from_IActivationFactory( iface );
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

DEFINE_IINSPECTABLE( bluetoothadapter_statics, IBluetoothAdapterStatics, struct bluetoothadapter_statics, IActivationFactory_iface )

static HRESULT WINAPI bluetoothadapter_statics_GetDeviceSelector( IBluetoothAdapterStatics *iface, HSTRING *result )
{
    static const WCHAR *default_res = L"System.Devices.InterfaceClassGuid:=\"{92383B0E-F90E-4AC9-8D44-8C2D0D0EBDA2}\" "
                                      L"AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

    TRACE( "iface %p, result %p.\n", iface, result );

    if (!result) return E_POINTER;
    return WindowsCreateString( default_res, wcslen(default_res), result );
}

static HRESULT WINAPI bluetoothadapter_statics_FromIdAsync( IBluetoothAdapterStatics *iface, HSTRING id, IAsyncOperation_BluetoothAdapter **operation )
{
    FIXME( "iface %p, id %s, operation %p stub!\n", iface, debugstr_hstring(id), operation );
    return E_NOTIMPL;
}

static HRESULT bluetoothadapter_create( const WCHAR *device_path, IBluetoothAdapter **adapter );

static HRESULT WINAPI bluetoothadapter_get_default_async( IUnknown *invoker, IUnknown *params, PROPVARIANT *result )
{
    char buffer[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) + MAX_PATH * sizeof( WCHAR )];
    SP_DEVICE_INTERFACE_DETAIL_DATA_W *iface_detail = (SP_DEVICE_INTERFACE_DETAIL_DATA_W *)buffer;
    SP_DEVICE_INTERFACE_DATA iface_data;
    DWORD idx = 0;
    HDEVINFO devinfo;

    iface_detail->cbSize = sizeof( *iface_detail );
    iface_data.cbSize = sizeof( iface_data );

    /* Windows.Devices.Bluetooth uses the GUID_BLUETOOTH_RADIO_INTERFACE interface class guid for radio devices,
     * which is why we don't use BluetoothFindFirstRadio, which uses GUID_BTHPORT_DEVICE_INTERFACE. */
    devinfo = SetupDiGetClassDevsW( &GUID_BLUETOOTH_RADIO_INTERFACE, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE );
    if (devinfo == INVALID_HANDLE_VALUE)
        return HRESULT_FROM_WIN32( GetLastError() );

    while (SetupDiEnumDeviceInterfaces( devinfo, NULL, &GUID_BLUETOOTH_RADIO_INTERFACE, idx++, &iface_data ))
    {
        HRESULT hr;
        IBluetoothAdapter *adapter = NULL;

        if (!SetupDiGetDeviceInterfaceDetailW( devinfo, &iface_data, iface_detail, sizeof( buffer ), NULL, NULL ))
            continue;

        if (FAILED(hr = bluetoothadapter_create( iface_detail->DevicePath, &adapter )))
            return hr;
        result->vt = VT_UNKNOWN;
        result->punkVal = (IUnknown *)adapter;
        break;
    }

    return S_OK;
}

static HRESULT WINAPI bluetoothadapter_statics_GetDefaultAsync( IBluetoothAdapterStatics *iface, IAsyncOperation_BluetoothAdapter **operation )
{
    TRACE( "iface %p, operation %p\n", iface, operation );
    return async_operation_bluetooth_adapter_create( NULL, NULL, bluetoothadapter_get_default_async, operation );
}

static const struct IBluetoothAdapterStaticsVtbl bluetoothadapter_statics_vtbl =
{
    bluetoothadapter_statics_QueryInterface,
    bluetoothadapter_statics_AddRef,
    bluetoothadapter_statics_Release,
    /* IInspectable methods */
    bluetoothadapter_statics_GetIids,
    bluetoothadapter_statics_GetRuntimeClassName,
    bluetoothadapter_statics_GetTrustLevel,
    /* IBluetoothAdapterStatics methods */
    bluetoothadapter_statics_GetDeviceSelector,
    bluetoothadapter_statics_FromIdAsync,
    bluetoothadapter_statics_GetDefaultAsync,
};

static struct bluetoothadapter_statics bluetoothadapter_statics_impl =
{
    {&factory_vtbl},
    {&bluetoothadapter_statics_vtbl},
    1,
};

IActivationFactory *bluetoothadapter_factory = &bluetoothadapter_statics_impl.IActivationFactory_iface;

struct bluetoothadapter
{
    IBluetoothAdapter IBluetoothAdapter_iface;
    HSTRING id;
    LONG ref;
};

static inline struct bluetoothadapter *impl_from_IBluetoothAdapter( IBluetoothAdapter *iface )
{
    return CONTAINING_RECORD( iface, struct bluetoothadapter, IBluetoothAdapter_iface );
}

static HRESULT WINAPI bluetoothadapter_QueryInterface( IBluetoothAdapter *iface, REFIID iid, void **out )
{
    struct bluetoothadapter *impl = impl_from_IBluetoothAdapter( iface );

    TRACE( "iface %p, iid %s, out %p.\n", iface, debugstr_guid( iid ), out );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IBluetoothAdapter ))
    {
        *out = &impl->IBluetoothAdapter_iface;
        IInspectable_AddRef( *out );
        return S_OK;
    }

    FIXME( "%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI bluetoothadapter_AddRef( IBluetoothAdapter *iface )
{
    struct bluetoothadapter *impl = impl_from_IBluetoothAdapter( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    TRACE( "iface %p increasing refcount to %lu.\n", iface, ref );
    return ref;
}

static ULONG WINAPI bluetoothadapter_Release( IBluetoothAdapter *iface )
{
    struct bluetoothadapter *impl = impl_from_IBluetoothAdapter( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    TRACE( "iface %p decreasing refcount to %lu.\n", iface, ref );

    if (!ref)
    {
        WindowsDeleteString( impl->id );
        free( impl );
    }
    return ref;
}

static HRESULT WINAPI bluetoothadapter_GetIids( IBluetoothAdapter *iface, ULONG *iid_count, IID **iids )
{
    FIXME( "iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids );
    return E_NOTIMPL;
}

static HRESULT WINAPI bluetoothadapter_GetRuntimeClassName( IBluetoothAdapter *iface, HSTRING *class_name )
{
    TRACE( "iface %p, class_name %p\n", iface, class_name );
    return WindowsCreateString( RuntimeClass_Windows_Devices_Bluetooth_BluetoothAdapter,
                                ARRAY_SIZE( RuntimeClass_Windows_Devices_Bluetooth_BluetoothAdapter ) - 1,
                                class_name );
}

static HRESULT WINAPI bluetoothadapter_GetTrustLevel( IBluetoothAdapter *iface, TrustLevel *trust_level )
{
    FIXME( "iface %p, trust_level %p stub!\n", iface, trust_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI bluetoothadapter_get_DeviceId( IBluetoothAdapter *iface, HSTRING *device_id )
{
    struct bluetoothadapter *impl = impl_from_IBluetoothAdapter( iface );
    TRACE( "iface %p, device_id %p\n", iface, device_id );
    return WindowsDuplicateString( impl->id, device_id );
}

static HRESULT WINAPI bluetoothadapter_get_BluetoothAddress( IBluetoothAdapter *iface, UINT64 *addr )
{
    FIXME( "iface %p, addr %p stub!\n", iface, addr );
    return E_NOTIMPL;
}

static HRESULT WINAPI bluetoothadapter_get_IsClassicSupported( IBluetoothAdapter *iface, boolean *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI bluetoothadapter_get_IsLowEnergySupported( IBluetoothAdapter *iface, boolean *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI bluetoothadapter_get_IsPeripheralRoleSupported( IBluetoothAdapter *iface, boolean *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI bluetoothadapter_get_IsCentralRoleSupported( IBluetoothAdapter *iface, boolean *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI bluetoothadapter_get_IsAdvertisementOffloadSupported( IBluetoothAdapter *iface, boolean *value )
{
    FIXME( "iface %p, value %p stub!\n", iface, value );
    return E_NOTIMPL;
}

static HRESULT WINAPI bluetoothadapter_GetRadioAsync( IBluetoothAdapter *iface, IAsyncOperation_Radio **op )
{
    FIXME( "iface %p, op %p stub!\n", iface, op );
    return E_NOTIMPL;
}

static const IBluetoothAdapterVtbl bluetooth_adapter_vtbl =
{
    /* QueryInterface methods */
    bluetoothadapter_QueryInterface,
    bluetoothadapter_AddRef,
    bluetoothadapter_Release,
    /* IInspectable methods */
    bluetoothadapter_GetIids,
    bluetoothadapter_GetRuntimeClassName,
    bluetoothadapter_GetTrustLevel,
    /* IBluetoothAdapter methods */
    bluetoothadapter_get_DeviceId,
    bluetoothadapter_get_BluetoothAddress,
    bluetoothadapter_get_IsClassicSupported,
    bluetoothadapter_get_IsLowEnergySupported,
    bluetoothadapter_get_IsPeripheralRoleSupported,
    bluetoothadapter_get_IsCentralRoleSupported,
    bluetoothadapter_get_IsAdvertisementOffloadSupported,
    bluetoothadapter_GetRadioAsync
};

static HRESULT bluetoothadapter_create( const WCHAR *device_path, IBluetoothAdapter **adapter )
{
    HRESULT ret;
    struct bluetoothadapter *impl;

    impl = calloc( 1, sizeof( *impl ) );
    if (!impl)
        return E_OUTOFMEMORY;

    ret = WindowsCreateString( device_path, wcslen( device_path ), &impl->id );
    if (FAILED( ret ))
    {
        free( impl );
        return ret;
    }

    impl->IBluetoothAdapter_iface.lpVtbl = &bluetooth_adapter_vtbl;
    impl->ref = 1;
    *adapter = &impl->IBluetoothAdapter_iface;
    return S_OK;
}
