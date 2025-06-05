/*
 * IDL Compiler
 *
 * Copyright 2002 Ove Kaaven
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

#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#include "widl.h"
#include "utils.h"
#include "parser.h"
#include "header.h"
#include "expr.h"
#include "typetree.h"
#include "typelib.h"

static int indent;

static char *strupper( char *s )
{
    for (char *p = s; *p; p++) *p = toupper( *p );
    return s;
}

static void write_import( const char *import )
{
    char *header = replace_extension( get_basename( import ), ".idl", ".h" );
    put_str( indent, "#include <%s>\n", header );
    free( header );
}

static void write_imports( const statement_list_t *stmts )
{
    const statement_t *stmt;

    if (stmts) LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
    {
        switch (stmt->type)
        {
        case STMT_IMPORT: write_import( stmt->u.str ); break;
        case STMT_LIBRARY: write_imports( stmt->u.lib->stmts ); break;
        case STMT_TYPE:
            if (type_get_type( stmt->u.type ) != TYPE_INTERFACE) break;
            write_imports( type_iface_get_stmts( stmt->u.type ) );
            break;
        default: break;
        }
    }
}

static bool iface_inherits( const type_t *iface, const char *name )
{
    if (!iface || !strcmp( iface->name, name )) return !!iface;
    return iface_inherits( type_iface_get_inherit( iface ), name );
}

static void write_default_IUnknown( const type_t *klass, const type_t *impl, const type_t *iface, const char *refcount )
{
    const char *short_name = iface->short_name ? iface->short_name : iface->name;
    const char *prefix = strmake( "%s_%s", impl->name, short_name );

    if (!iface_inherits( iface, "IUnknown" )) return;

    if (!refcount)
    {
        put_str( indent, "static ULONG WINAPI %s_AddRef( %s *iface )\n", prefix, iface->c_name );
        put_str( indent, "{\n" );
        put_str( indent, "    return 2;\n" );
        put_str( indent, "}\n" );
        put_str( indent, "static ULONG WINAPI %s_Release( %s *iface )\n", prefix, iface->c_name );
        put_str( indent, "{\n" );
        put_str( indent, "    return 1;\n" );
        put_str( indent, "}\n" );
    }
    else
    {
        put_str( indent, "static ULONG WINAPI %s_AddRef( %s *iface )\n", prefix, iface->c_name );
        put_str( indent, "{\n" );
        put_str( indent, "    struct %s *klass = %s_from_%s( iface );\n", klass->name, klass->name, short_name );
        put_str( indent, "    struct %s *impl = %s_from_%s( iface );\n", impl->name, impl->name, short_name );
        put_str( indent, "    LONG ref = InterlockedIncrement( &klass->%s );\n", refcount );
        put_str( indent, "    TRACE( \"%s %%p increasing refcount to %%ld\\n\", impl, ref );\n", impl->name );
        put_str( indent, "    return ref;\n" );
        put_str( indent, "}\n" );
        put_str( indent, "static ULONG WINAPI %s_Release( %s *iface )\n", prefix, iface->c_name );
        put_str( indent, "{\n" );
        put_str( indent, "    struct %s *klass = %s_from_%s( iface );\n", klass->name, klass->name, short_name );
        put_str( indent, "    struct %s *impl = %s_from_%s( iface );\n", impl->name, impl->name, short_name );
        put_str( indent, "    LONG ref = InterlockedDecrement( &klass->%s );\n", refcount );
        put_str( indent, "    TRACE( \"%s %%p decreasing refcount to %%ld\\n\", impl, ref);\n", impl->name );
        put_str( indent, "    if (!ref) %s_funcs.destroy( impl );\n", impl->name );
        put_str( indent, "    return ref;\n" );
        put_str( indent, "}\n" );
    }
}

static void write_default_IInspectable( const type_t *klass, const type_t *impl, const type_t *iface, const char *class_name )
{
    const char *short_name = iface->short_name ? iface->short_name : iface->name;
    const char *prefix = strmake( "%s_%s", impl->name, short_name );

    if (!iface_inherits( iface, "IInspectable" )) return;

    put_str( indent, "static HRESULT WINAPI %s_GetIids( %s *iface, ULONG *count, IID **iids )\n", prefix, iface->c_name );
    put_str( indent, "{\n" );
    put_str( indent, "    struct %s *impl = %s_from_%s( iface );\n", impl->name, impl->name, short_name );
    put_str( indent, "    FIXME( \"%s %%p count %%p iids %%p stub!\\n\", impl, count, iids );\n", impl->name );
    put_str( indent, "    return E_NOTIMPL;\n" );
    put_str( indent, "}\n" );
    put_str( indent, "static HRESULT WINAPI %s_GetRuntimeClassName( %s *iface, HSTRING *class_name )\n", prefix, iface->c_name );
    put_str( indent, "{\n" );
    put_str( indent, "    struct %s *impl = %s_from_%s( iface );\n", impl->name, impl->name, short_name );
    if (class_name)
    {
        put_str( indent, "    struct %s *klass = %s_from_%s( iface );\n", klass->name, klass->name, short_name );
        put_str( indent, "    TRACE( \"%s %%p class_name %%p\\n\", impl, class_name );\n", impl->name );
        put_str( indent, "    if (!klass->%s) return WindowsDuplicateString( NULL, class_name );\n", class_name );
        put_str( indent, "    return WindowsCreateString( klass->%s, wcslen( klass->%s ), class_name );\n", class_name, class_name );
    }
    else
    {
        put_str( indent, "    FIXME( \"%s %%p class_name %%p stub!\\n\", impl, class_name );\n", impl->name );
        put_str( indent, "    return E_NOTIMPL;\n" );
    }
    put_str( indent, "}\n" );
    put_str( indent, "static HRESULT WINAPI %s_GetTrustLevel( %s *iface, TrustLevel *trust_level )\n", prefix, iface->c_name );
    put_str( indent, "{\n" );
    put_str( indent, "    struct %s *impl = %s_from_%s( iface );\n", impl->name, impl->name, short_name );
    put_str( indent, "    FIXME( \"%s %%p trust_level %%p stub!\\n\", impl, trust_level );\n", impl->name );
    put_str( indent, "    return E_NOTIMPL;\n" );
    put_str( indent, "}\n" );
}

static void write_iface_forwarding( const type_t *klass, const type_t *impl, const type_t *iface, const type_t *default_iface,
                                    const char *forward, const type_t *forward_iface )
{
    const char *short_name = iface->short_name ? iface->short_name : iface->name;
    const char *prefix = strmake( "%s_%s", impl->name, short_name );

    if (iface_inherits( iface, "IUnknown" ) && iface_inherits( default_iface, "IUnknown" ))
    {
        put_str( indent, "static HRESULT WINAPI %s_QueryInterface( %s *iface, REFIID iid, void **out )\n", prefix, iface->c_name );
        put_str( indent, "{\n" );
        put_str( indent, "    struct %s *klass = %s_from_%s( iface );\n", klass->name, klass->name, short_name );
        put_str( indent, "    return %s_QueryInterface( %s, iid, out );\n", forward_iface->c_name, forward );
        put_str( indent, "}\n" );
        put_str( indent, "static ULONG WINAPI %s_AddRef( %s *iface )\n", prefix, iface->c_name );
        put_str( indent, "{\n" );
        put_str( indent, "    struct %s *klass = %s_from_%s( iface );\n", klass->name, klass->name, short_name );
        put_str( indent, "    return %s_AddRef( %s );\n", forward_iface->c_name, forward );
        put_str( indent, "}\n" );
        put_str( indent, "static ULONG WINAPI %s_Release( %s *iface )\n", prefix, iface->c_name );
        put_str( indent, "{\n" );
        put_str( indent, "    struct %s *klass = %s_from_%s( iface );\n", klass->name, klass->name, short_name );
        put_str( indent, "    return %s_Release( %s );\n", forward_iface->c_name, forward );
        put_str( indent, "}\n" );
    }
    if (iface_inherits( iface, "IInspectable" ) && iface_inherits( default_iface, "IInspectable" ))
    {
        put_str( indent, "static HRESULT WINAPI %s_GetIids( %s *iface, ULONG *count, IID **iids )\n", prefix, iface->c_name );
        put_str( indent, "{\n" );
        put_str( indent, "    struct %s *klass = %s_from_%s( iface );\n", klass->name, klass->name, short_name );
        put_str( indent, "    return %s_GetIids( %s, count, iids );\n", forward_iface->c_name, forward );
        put_str( indent, "}\n" );
        put_str( indent, "static HRESULT WINAPI %s_GetRuntimeClassName( %s *iface, HSTRING *class_name )\n", prefix, iface->c_name );
        put_str( indent, "{\n" );
        put_str( indent, "    struct %s *klass = %s_from_%s( iface );\n", klass->name, klass->name, short_name );
        put_str( indent, "    return %s_GetRuntimeClassName( %s, class_name );\n", forward_iface->c_name, forward );
        put_str( indent, "}\n" );
        put_str( indent, "static HRESULT WINAPI %s_GetTrustLevel( %s *iface, TrustLevel *trust_level )\n", prefix, iface->c_name );
        put_str( indent, "{\n" );
        put_str( indent, "    struct %s *klass = %s_from_%s( iface );\n", klass->name, klass->name, short_name );
        put_str( indent, "    return %s_GetTrustLevel( %s, trust_level );\n", forward_iface->c_name, forward );
        put_str( indent, "}\n" );
    }
}

static void write_iface_impl_from( const type_t *klass, const type_t *impl, const var_t *var )
{
    const type_t *iface = var->declspec.type;
    const char *short_name = iface->short_name ? iface->short_name : iface->name;

    put_str( indent, "static inline struct %s *%s_from_%s( %s *iface )\n", klass->name, klass->name, short_name, iface->c_name );
    put_str( indent, "{\n" );
    put_str( indent, "    return CONTAINING_RECORD( iface, struct %s, %s );\n", klass->name, var->name );
    put_str( indent, "}\n" );

    if (impl == klass) return;

    put_str( indent, "static inline struct %s *%s_from_%s( %s *iface )\n", impl->name, impl->name, short_name, iface->c_name );
    put_str( indent, "{\n" );
    put_str( indent, "    return %s_funcs.from_klass( %s_from_%s( iface ) );\n", impl->name, klass->name, short_name );
    put_str( indent, "}\n" );
}

static void write_iface_query( const type_t *klass, const type_t *impl, const var_t *var )
{
    const type_t *iface = var->declspec.type;
    const char *short_name = iface->short_name ? iface->short_name : iface->name;
    const char *iid = strmake( "&IID_%s", iface->c_name );

    if (!iface_inherits( iface, "IUnknown" )) return;

    put_str( indent, "static HRESULT %s_query_%s( struct %s *klass, REFIID iid, void **out )\n", klass->name, short_name, klass->name );
    put_str( indent, "{\n" );
    put_str( indent, "    if (!(klass)->%s.lpVtbl) return E_NOINTERFACE;\n", var->name );
    put_str( indent, "    if (IsEqualGUID( (iid), %s )", iid );
    for (const type_t *base = type_iface_get_inherit( iface ); base; (base = type_iface_get_inherit( base )))
        put_str( indent, "\n     || IsEqualGUID( (iid), &IID_%s )", base->c_name );
    put_str( indent, ")\n" );
    put_str( indent, "    {\n" );
    put_str( indent, "        %s_AddRef( &(klass)->%s );\n", iface->c_name, var->name );
    put_str( indent, "        *(out) = &(klass)->%s;\n", var->name );
    put_str( indent, "        return S_OK;\n" );
    put_str( indent, "    }\n" );
    put_str( indent, "    return E_NOINTERFACE;\n" );
    put_str( indent, "}\n" );
}

static void write_impl( const type_t *impl, const type_t *klass )
{
    bool from_klass, destroy;
    struct strbuf str = {0};
    const var_t *var, *def = NULL, *out = NULL, *ref = NULL, *cls = NULL;
    const type_t *forward_iface;
    const char *forward;

    put_str( indent, "#ifdef WIDL_impl_%s\n\n", impl->name );

    if (impl != klass) put_str( indent, "struct %s;\n", impl->name );
    put_str( indent, "struct %s\n", klass->name );
    put_str( indent++, "{\n" );
    LIST_FOR_EACH_ENTRY( var, type_struct_get_fields( klass ), var_t, entry )
    {
        if (!def && strendswith( var->name, "_iface" )) def = var;
        if (!out && strendswith( var->name, "_outer" )) out = var;
        if (!ref && !strcmp( var->name, "ref" )) ref = var;
        if (!ref && !strcmp( var->name, "refcount" )) ref = var;
        if (!cls && !strcmp( var->name, "class_name" )) cls = var;
        append_declspec( &str, &var->declspec, NAME_C, "WINAPI", false, var->name );
        put_str( indent, "%s;\n", str.buf );
        str.pos = 0;
    }
    put_str( --indent, "};\n\n" );

    from_klass = impl != klass;
    destroy = ref != NULL;

    put_str( indent, "static const struct %s_funcs\n", impl->name );
    put_str( indent, "{\n" );
    if (from_klass) put_str( indent, "    struct %s *(*from_klass)( struct %s *klass );\n", impl->name, klass->name );
    if (destroy)    put_str( indent, "    void (*destroy)( struct %s *impl );\n", impl->name );
    put_str( indent, "    int placeholder;\n" );
    put_str( indent, "} %s_funcs;\n\n", impl->name );

    put_str( indent++, "#define %s_FUNCS_INIT \\\n", strupper( strdup( impl->name ) ) );
    put_str( indent++, "{ \\\n" );
    if (from_klass) put_str( indent, ".from_klass = %s_from_klass, \\\n", impl->name );
    if (destroy)    put_str( indent, ".destroy = %s_destroy, \\\n", impl->name );
    put_str( indent, "0 \\\n" );
    put_str( --indent, "}\n" );
    put_str( --indent, "\n" );

    if (out)
    {
        forward = strmake( "klass->%s", out->name );
        forward_iface = type_pointer_get_ref_type( out->declspec.type );
    }
    else
    {
        forward = strmake( "&klass->%s", def->name );
        forward_iface = def->declspec.type;
    }

    LIST_FOR_EACH_ENTRY( var, type_struct_get_fields( klass ), var_t, entry )
    {
        if (!strendswith( var->name, "_iface" )) continue;
        write_iface_impl_from( klass, impl, var );
    }
    put_str( indent, "\n" );

    LIST_FOR_EACH_ENTRY( var, type_struct_get_fields( klass ), var_t, entry )
    {
        if (!strendswith( var->name, "_iface" )) continue;
        write_iface_query( klass, impl, var );
    }
    put_str( indent, "\n" );

    LIST_FOR_EACH_ENTRY( var, type_struct_get_fields( klass ), var_t, entry )
    {
        if (!strendswith( var->name, "_iface" )) continue;
        if (var != def) write_iface_forwarding( klass, impl, var->declspec.type, def->declspec.type, forward, forward_iface );
        else
        {
            write_default_IUnknown( klass, impl, var->declspec.type, ref ? ref->name : NULL );
            write_default_IInspectable( klass, impl, var->declspec.type, cls ? cls->name : NULL );
        }
    }
    put_str( indent, "\n" );

    put_str( indent, "#endif /* WIDL_impl_%s */\n", impl->name );
}

static void write_impls( const statement_list_t *stmts )
{
    const statement_t *stmt;

    LIST_FOR_EACH_ENTRY( stmt, stmts, const statement_t, entry )
    {
        const type_t *type = stmt->u.type, *impl;
        if (stmt->type != STMT_TYPE || !stmt->is_defined) continue;
        if (!(impl = get_attrp( type->attrs, ATTR_IMPL ))) continue;
        write_impl( impl, type );
    }
}

void write_comimpl( const statement_list_t *stmts )
{
    if (!do_comimpl) return;

    init_output_buffer();
    put_str( indent, "/*** Autogenerated by WIDL %s from %s - Do not edit ***/\n\n", PACKAGE_VERSION, input_name );
    write_imports( stmts );
    put_str( indent, "\n" );

    write_impls( stmts );
    flush_output_buffer( comimpl_name );
}
