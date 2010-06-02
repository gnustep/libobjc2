/* GNU Objective C Runtime Thread Interface
   Copyright (C) 1996, 1997, 2009 Free Software Foundation, Inc.
   Contributed by Galen C. Hunt (gchunt@cs.rochester.edu)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 3, or (at your option) any later version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */


#include <stdlib.h>
#include "lock.h"
#include "objc/runtime-legacy.h"

/* The hook function called when the runtime becomes multi threaded */
// The runtime is now always in multithreaded mode as there was no benefit to
// not being in multithreaded mode.

objc_thread_callback objc_set_thread_callback (objc_thread_callback func)
{
  static objc_thread_callback _objc_became_multi_threaded = NULL;
  objc_thread_callback temp = _objc_became_multi_threaded;
  // Call the function immediately and then ignore it forever because it is
  // pointless.
  func();
  _objc_became_multi_threaded = func;
  return temp;
}

/* Deprecated functions for creating and destroying mutexes. */
objc_mutex_t objc_mutex_allocate (void)
{
	mutex_t *mutex = malloc(sizeof(mutex_t));
	INIT_LOCK(*mutex);
	return mutex;
}
int objc_mutex_deallocate (objc_mutex_t mutex)
{
	DESTROY_LOCK(mutex);
	free(mutex);
	return 0;
}

/* External functions for locking and unlocking runtime mutexes. */
int
objc_mutex_lock (objc_mutex_t mutex)
{
  return LOCK((mutex_t*)mutex);
}

int
objc_mutex_unlock (objc_mutex_t mutex)
{
  return UNLOCK((mutex_t*)mutex);
}


// Legacy cruft that never did anything:
void 
objc_thread_add (void) {}

void
objc_thread_remove (void) {}

/* End of File */
