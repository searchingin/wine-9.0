/*
 * Copyright (C) 2023 Paul Gofman for CodeWeavers
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
#include <stdlib.h>

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windef.h>
#include <winbase.h>
#include <winerror.h>
#include <winternl.h>
#include <ioringapi.h>

#include "wine/test.h"

static HRESULT (WINAPI *pQueryIoRingCapabilities)(IORING_CAPABILITIES *);
static HRESULT (WINAPI *pGetFinalPathNameByHandleW)(HANDLE, WCHAR *, DWORD, DWORD);

static void test_ioring_caps(void)
{
    IORING_CAPABILITIES caps;
    HRESULT hr;

    if (!pQueryIoRingCapabilities)
    {
        win_skip("QueryIoRingCapabilities is not available, skipping tests.\n");
        return;
    }

    memset(&caps, 0xcc, sizeof(caps));
    hr = pQueryIoRingCapabilities(&caps);
    todo_wine ok(hr == S_OK, "got %#lx.\n", hr);
}

static void test_longpath_support(void)
{
    HANDLE file;
    DWORD ret;
    WCHAR dummy[128] = {};
    BOOL success;
    /* Individual path components cannot be more than lpMaximumComponentLength as returned by
     * GetVolumeInformation. This is typically only 255 characters, so testing long path support
     * requires making a containing folder first */
    static const WCHAR testfolder[] =
        L"\\\\?\\C:\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    static const WCHAR testfile[] =
        L"\\\\?\\C:\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

    if (!pGetFinalPathNameByHandleW)
    {
        win_skip("GetFinalPathNameByHandleW is missing\n");
        return;
    }

    SetLastError(0xdeadbeef);
    success = CreateDirectoryW(testfolder, NULL);
    ok(success != 0, "CreateDirectoryW error %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    file = CreateFileW(testfile, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
            FILE_FLAG_DELETE_ON_CLOSE, 0);
    ok(file != INVALID_HANDLE_VALUE, "CreateFileW error %lu\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetFinalPathNameByHandleW(file, dummy, 128, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    ok(ret > 128, "GetFinalPathNameByHandleW returned %ld but expected more than 128, error %lu\n",
            ret, GetLastError());

    CloseHandle(file);
    RemoveDirectoryW(testfolder);
}

START_TEST(file)
{
    HMODULE hmod;

    hmod = LoadLibraryA("kernelbase.dll");
    pQueryIoRingCapabilities = (void *)GetProcAddress(hmod, "QueryIoRingCapabilities");
    pGetFinalPathNameByHandleW = (void *)GetProcAddress(hmod, "GetFinalPathNameByHandleW");

    test_ioring_caps();
    test_longpath_support();
}
