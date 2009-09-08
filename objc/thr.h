/* Thread and mutex controls for Objective C.
   Copyright (C) 1996, 1997, 2002, 2004, 2009 Free Software Foundation, Inc.
   Contributed by Galen C. Hunt (gchunt@cs.rochester.edu)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

#ifndef __thread_INCLUDE_GNU
#define __thread_INCLUDE_GNU

#include "objc.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Recursive mutex type used to protect the runtime.  */
typedef void *objc_mutex_t;

/**
 * Global runtime mutex.  Acquire this before calling any unsafe runtime
 * functions.  Note that this is never needed if you restrict yourself to the
 * public interfaces, which are strongly recommended.
 */
extern objc_mutex_t __objc_runtime_mutex;
/* Frontend mutex functions */
int objc_mutex_lock (objc_mutex_t mutex);
int objc_mutex_unlock (objc_mutex_t mutex);

/**
 * Callback for when the runtime becomes multithreaded.  As the runtime is now
 * always in multithreaded mode, the callback is called as soon as it is
 * registered and then discarded.  This interface is maintained solely for
 * legacy compatibility.
 */
typedef void (*objc_thread_callback) (void);
objc_thread_callback objc_set_thread_callback (objc_thread_callback func);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* not __thread_INCLUDE_GNU */
