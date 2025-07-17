/*
 * Copyright (C) 2025 Tim Clem for CodeWeavers
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

#include <stdarg.h>
#include <stdint.h>

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(icu);

typedef WCHAR UChar;
typedef int32_t UErrorCode;  /* Actually an enum */

int32_t CDECL ucal_getTimeZoneIDForWindowsID( const UChar *winid, int32_t len, const char *region,
                                              UChar *id, int32_t idCapacity, UErrorCode *status )
{
    FIXME("%s %s %p %d %p - stub\n",
          len == -1 ? debugstr_w(winid) : debugstr_wn(winid, len),
          debugstr_a(region), id, idCapacity, status);

    /* This is ICU's documented behavior when the Windows ID is unmappable or
       unknown. It does not touch status or id in this case. */
    return 0;
}

const char * CDECL u_errorName( UErrorCode code )
{
    FIXME("%d - stub\n", code);
    return "U_UNSUPPORTED_ERROR";
}
