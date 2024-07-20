/*
 * Copyright 2025 Marius Schiffer
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

#include <winbase.h>

#include "winstring.h"
#include "msvcrt/locale.h"
#include "bcp47langs.h"

#include "wine/test.h"

static void test_GetUserLanguages(void)
{
    HSTRING result;
    const WCHAR *user_languages;

    ok(GetUserLanguages(',', NULL) == E_INVALIDARG, "unknown return code\n");

    setlocale(LC_ALL, "enu");
    ok(GetUserLanguages(',', &result) == 0, "unknown return code\n");
    user_languages = WindowsGetStringRawBuffer(result, NULL);
    ok(!lstrcmpW(user_languages, L"en-US"), "languages=%s\n", debugstr_w(user_languages));

    setlocale(LC_ALL, "deu");
    ok(GetUserLanguages(',', &result) == 0, "unknown return code\n");
    user_languages = WindowsGetStringRawBuffer(result, NULL);
    ok(!lstrcmpW(user_languages, L"de-DE"), "languages=%s\n", debugstr_w(user_languages));

    WindowsDeleteString(user_languages);
}

START_TEST(bcp47langs)
{
    test_get_user_languages();
}
