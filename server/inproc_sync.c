/*
 * In-process synchronization primitives
 *
 * Copyright (C) 2021-2022 Elizabeth Figura for CodeWeavers
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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"

#include "file.h"
#include "handle.h"
#include "request.h"
#include "thread.h"

#ifdef HAVE_LINUX_NTSYNC_H
# include <linux/ntsync.h>
#endif

#ifdef NTSYNC_IOC_EVENT_READ

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

static int get_linux_device(void)
{
    static int fd = -2;

    if (fd == -2)
        fd = open( "/dev/ntsync", O_CLOEXEC | O_RDONLY );

    return fd;
}

int use_inproc_sync(void)
{
    return 0;
}

int create_inproc_event( int manual_reset, int signaled )
{
    struct ntsync_event_args args;
    int device;

    if ((device = get_linux_device()) < 0) return -1;

    args.signaled = signaled;
    args.manual = manual_reset;
    return ioctl( device, NTSYNC_IOC_CREATE_EVENT, &args );
}

void set_inproc_event( int event )
{
    __u32 count;

    if (!use_inproc_sync()) return;

    if (debug_level) fprintf( stderr, "set_inproc_event %d\n", event );

    ioctl( event, NTSYNC_IOC_EVENT_SET, &count );
}

void reset_inproc_event( int event )
{
    __u32 count;

    if (!use_inproc_sync()) return;

    if (debug_level) fprintf( stderr, "reset_inproc_event %d\n", event );

    ioctl( event, NTSYNC_IOC_EVENT_RESET, &count );
}

#else

int use_inproc_sync(void)
{
    return 0;
}

int create_inproc_event( int manual_reset, int signaled )
{
    return -1;
}

void set_inproc_event( int event )
{
}

void reset_inproc_event( int event )
{
}

#endif


DECL_HANDLER(get_linux_sync_device)
{
#ifdef NTSYNC_IOC_EVENT_READ
    int fd;

    if ((fd = get_linux_device()) >= 0)
        send_client_fd( current->process, fd, 0 );
    else
        set_error( STATUS_NOT_IMPLEMENTED );
#else
    set_error( STATUS_NOT_IMPLEMENTED );
#endif
}
