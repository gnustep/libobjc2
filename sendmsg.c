/* GNU Objective C Runtime message lookup 
   Copyright (C) 1993, 1995, 1996, 1997, 1998,
   2001, 2002, 2004, 2009 Free Software Foundation, Inc.
   Contributed by Kresten Krab Thorup

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
#include "objc/runtime-legacy.h"
#include "objc/slot.h"
#include "objc/encoding.h"
#include "lock.h"
#include "slot_pool.h"

/* Mutex protecting the pre-initialize dtable */
static mutex_t initialize_lock;

void objc_resolve_class(Class);

#define sidx uint32_t

/* Two hooks for method forwarding. If either is set, it is invoked
 * to return a function that performs the real forwarding.  If both
 * are set, the result of __objc_msg_forward2 will be preferred over
 * that of __objc_msg_forward.  If both return NULL or are unset,
 * the libgcc based functions (__builtin_apply and friends) are
 * used.
 */
IMP (*__objc_msg_forward) (SEL) = NULL;
IMP (*__objc_msg_forward2) (id, SEL) = NULL;

/* Send +initialize to class */
static void __objc_send_initialize (Class);

static void __objc_install_dispatch_table_for_class (Class);


#include "dtable.c"

/* Forward declare some functions */
static void __objc_init_install_dtable (id, SEL);

static Method_t search_for_method_in_hierarchy (Class class, SEL sel);
Method_t search_for_method_in_list (MethodList_t list, SEL op);
id nil_method (id, SEL);

/* Given a selector, return the proper forwarding implementation. */
inline static
IMP
__objc_get_forward_imp (id rcv, SEL sel)
{
  /* If a custom forwarding hook was registered, try getting a forwarding
     function from it. There are two forward routine hooks, one that
     takes the receiver as an argument and one that does not. */
  if (__objc_msg_forward2)
    {
      IMP result;
      if ((result = __objc_msg_forward2 (rcv, sel)) != NULL)
       return result;
    }
  if (__objc_msg_forward)
    {
      IMP result;
      if ((result = __objc_msg_forward (sel)) != NULL) 
        return result;
    }
  fprintf(stderr, "Object forwarding not available");
  abort();
}


static inline IMP sarray_get_imp (void *dtable, size_t key)
{
    struct objc_slot *slot = sarray_get_safe (dtable, key);
    return (NULL != slot) ? slot->method : (IMP)0;
}


/* Given a class and selector, return the selector's implementation.  */
inline
IMP
get_imp (Class class, SEL sel)
{
  /* In a vanilla implementation we would first check if the dispatch
     table is installed.  Here instead, to get more speed in the
     standard case (that the dispatch table is installed) we first try
     to get the imp using brute force.  Only if that fails, we do what
     we should have been doing from the very beginning, that is, check
     if the dispatch table needs to be installed, install it if it's
     not installed, and retrieve the imp from the table if it's
     installed.  */
  IMP res = sarray_get_imp (class->dtable, (size_t) sel->sel_id);
  if (res == 0)
    {
      /* This will block if calling +initialize from another thread. */
      void *dtable = dtable_for_class(class);
      /* Not a valid method */
      if (dtable == __objc_uninstalled_dtable)
        {
          /* The dispatch table needs to be installed. */
          objc_mutex_lock (__objc_runtime_mutex);

           /* Double-checked locking pattern: Check
              __objc_uninstalled_dtable again in case another thread
              installed the dtable while we were waiting for the lock
              to be released.  */
         if (dtable_for_class(class) == __objc_uninstalled_dtable)
           {
             __objc_install_dispatch_table_for_class (class);
           }

          objc_mutex_unlock (__objc_runtime_mutex);
          /* Call ourselves with the installed dispatch table
             and get the real method */
          res = get_imp (class, sel);
        }
      else
        {
          /* The dispatch table has been installed.  */

         /* Get the method from the dispatch table (we try to get it
            again in case another thread has installed the dtable just
            after we invoked sarray_get_safe, but before we checked
            class->dtable == __objc_uninstalled_dtable).
         */
          res = sarray_get_imp (dtable, (size_t) sel->sel_id);
          if (res == 0)
            {
              /* The dispatch table has been installed, and the method
                 is not in the dispatch table.  So the method just
                 doesn't exist for the class.  Return the forwarding
                 implementation. */
             res = __objc_get_forward_imp ((id)class, sel);
            }
        }
    }
  return res;
}

/* Query if an object can respond to a selector, returns YES if the
object implements the selector otherwise NO.  Does not check if the
method can be forwarded. */
inline
BOOL
__objc_responds_to (id object, SEL sel)
{
  void *res;

  /* Install dispatch table if need be */
  if (dtable_for_class(object->class_pointer) == __objc_uninstalled_dtable)
    {
      objc_mutex_lock (__objc_runtime_mutex);
      if (dtable_for_class(object->class_pointer) == __objc_uninstalled_dtable)
        {
          __objc_install_dispatch_table_for_class (object->class_pointer);
        }
      objc_mutex_unlock (__objc_runtime_mutex);
    }

  /* Get the method from the dispatch table */
  res = sarray_get_safe (dtable_for_class(object->class_pointer), 
		  (size_t) sel->sel_id);
  return (res != 0);
}

extern id (*objc_proxy_lookup)(id receiver, SEL op);
/* This is the lookup function.  All entries in the table are either a 
   valid method *or* zero.  If zero then either the dispatch table
   needs to be installed or it doesn't exist and forwarding is attempted. */
inline
IMP
objc_msg_lookup (id receiver, SEL op)
{
  IMP result;
  if (receiver)
    {
      result = sarray_get_imp (receiver->class_pointer->dtable, 
                                (sidx)op->sel_id);
      if (result == 0)
        {
          /** Get the dtable that we should be using for lookup.  This will
           * block if we are in the middle of running +initialize in another
           * thread. */
          void *dtable = dtable_for_class(receiver->class_pointer);
          /* Not a valid method */
          if (dtable == __objc_uninstalled_dtable)
            {
              /* The dispatch table needs to be installed.
                 This happens on the very first method call to the class. */
              __objc_init_install_dtable (receiver, op);

              /* Get real method for this in newly installed dtable */
              result = get_imp (receiver->class_pointer, op);
            }
          else
            {
              /* The dispatch table has been installed.  Check again
                 if the method exists (just in case the dispatch table
                 has been installed by another thread after we did the
                 previous check that the method exists).
              */
              result = sarray_get_imp (dtable, (sidx)op->sel_id);
              if (result == 0)
                {
                  /* Try again after giving the code a chance to install new
                   * methods.  This lookup mechanism doesn't support forwarding
                   * to other objects, so only call the lookup recursively if
                   * the receiver is not changed.
                   */
                  id newReceiver = objc_proxy_lookup (receiver, op);
                  if (newReceiver == receiver)
                    {
                      return objc_msg_lookup(receiver, op);
                    }
                  /* If the method still just doesn't exist for the
                     class, attempt to forward the method. */
                  result = __objc_get_forward_imp (receiver, op);
                }
            }
        }
      return result;
    }
  else
    return (IMP)nil_method;
}

IMP
objc_msg_lookup_super (Super_t super, SEL sel)
{
  if (super->self)
    return get_imp (super->class, sel);
  else
    return (IMP)nil_method;
}

int method_get_sizeof_arguments (Method *);

/*
 * TODO: This never worked properly.  Delete it after checking no one is
 * misguided enough to be using it.
 */
retval_t
objc_msg_sendv (id object, SEL op, arglist_t arg_frame)
{
        fprintf(stderr, "objc_msg_sendv() never worked correctly.  Don't use it.\n");
        abort();
}


/* Send +initialize to class if not already done */
static void
__objc_send_initialize (Class class)
{
  /* This *must* be a class object */
  assert (CLS_ISCLASS (class));
  assert (! CLS_ISMETA (class));

  if (! CLS_ISINITIALIZED (class))
    {
      /* This is always called with the runtime lock, which guarantees
       * atomicity, but we also need to make sure that the initialize lock is
       * held so that we can create the premature dtable. */
      LOCK(&initialize_lock);
      if (! CLS_ISRESOLV (class))
        objc_resolve_class(class);


      /* Create the garbage collector type memory description */
      __objc_generate_gc_type_description (class);

      if (class->super_class)
        __objc_send_initialize (class->super_class);
      // Superclass +initialize might possibly send a message to this class, in
      // which case this method would be called again.  See NSObject and
      // NSAutoreleasePool +initialize interaction in GNUstep.
      if (CLS_ISINITIALIZED (class))
        {
          UNLOCK(&initialize_lock);
          return;
        }
      CLS_SETINITIALIZED (class);
      CLS_SETINITIALIZED (class->class_pointer);
      /* Create the dtable, but don't install it on the class quite yet. */

      void *dtable = create_dtable_for_class(class);
      /* Taking a pointer to an on-stack object and storing it in a global is
       * usually a silly idea.  It is safe here, because we invert this
       * transform before we return, and we use initialize_lock to make sure no
       * other threads have access to this pointer. */
      InitializingDtable buffer = { class, dtable, temporary_dtables };
      temporary_dtables = &buffer;

      {
        SEL              op = sel_register_name ("initialize");
        IMP             imp = 0;
        MethodList_t method_list = class->class_pointer->methods;

        while (method_list) {
          int i;
          Method_t method;

          for (i = 0; i < method_list->method_count; i++) {
            method = &(method_list->method_list[i]);
            if (method->method_name
                && method->method_name->sel_id == op->sel_id) {
              imp = method->method_imp;
              break;
            }
          }

          if (imp)
            break;

          method_list = method_list->method_next;

        }
        if (imp)
            (*imp) ((id) class, op);
                
      }
      class->dtable = dtable;
      /* Note: We don't free the cache entry; it was allocated on the stack. */
      if (temporary_dtables == &buffer)
      {
          temporary_dtables = temporary_dtables->next;
      }
      else
      {
        InitializingDtable *prev = temporary_dtables;
        while (prev->next->class != class)
        {
            prev = prev->next;
        }
        prev->next = buffer.next;
      }
      UNLOCK(&initialize_lock);
    }
}

/* This function is called by objc_msg_lookup when the
   dispatch table needs to be installed; thus it is called once
   for each class, namely when the very first message is sent to it. */
static void
__objc_init_install_dtable (id receiver, SEL op __attribute__ ((__unused__)))
{
  objc_mutex_lock (__objc_runtime_mutex);
  
  /* This may happen, if the programmer has taken the address of a 
     method before the dtable was initialized... too bad for him! */
  if (dtable_for_class(receiver->class_pointer) != __objc_uninstalled_dtable)
    {
      objc_mutex_unlock (__objc_runtime_mutex);
      return;
    }
  
  if (CLS_ISCLASS (receiver->class_pointer))
    {
      /* receiver is an ordinary object */
      assert (CLS_ISCLASS (receiver->class_pointer));

      /* call +initialize -- this will in turn install the factory 
         dispatch table if not already done :-) */
      __objc_send_initialize (receiver->class_pointer);
    }
  else
    {
      /* receiver is a class object */
      assert (CLS_ISCLASS ((Class)receiver));
      assert (CLS_ISMETA (receiver->class_pointer));
      __objc_send_initialize ((Class)receiver);
    }
  objc_mutex_unlock (__objc_runtime_mutex);
}

/* This function adds a method list to a class.  This function is
   typically called by another function specific to the run-time.  As
   such this function does not worry about thread safe issues.

   This one is only called for categories. Class objects have their
   methods installed right away, and their selectors are made into
   SEL's by the function __objc_register_selectors_from_class. */
void
class_add_method_list (Class class, MethodList_t list)
{
  /* Passing of a linked list is not allowed.  Do multiple calls.  */
  assert (! list->method_next);

  __objc_register_selectors_from_list(list);

  /* Add the methods to the class's method list.  */
  list->method_next = class->methods;
  class->methods = list;

  /* Update the dispatch table of class */
  __objc_update_dispatch_table_for_class (class);
}

Method_t
class_get_instance_method (Class class, SEL op)
{
  return search_for_method_in_hierarchy (class, op);
}

Method_t
class_get_class_method (MetaClass class, SEL op)
{
  return search_for_method_in_hierarchy (class, op);
}


/* Search for a method starting from the current class up its hierarchy.
   Return a pointer to the method's method structure if found.  NULL
   otherwise. */   

static Method_t
search_for_method_in_hierarchy (Class cls, SEL sel)
{
  Method_t method = NULL;
  Class class;

  if (! sel_is_mapped (sel))
    return NULL;

  /* Scan the method list of the class.  If the method isn't found in the
     list then step to its super class. */
  for (class = cls; ((! method) && class); class = class->super_class)
    method = search_for_method_in_list (class->methods, sel);

  return method;
}



/* Given a linked list of method and a method's name.  Search for the named
   method's method structure.  Return a pointer to the method's method
   structure if found.  NULL otherwise. */  
Method_t
search_for_method_in_list (MethodList_t list, SEL op)
{
  MethodList_t method_list = list;

  if (! sel_is_mapped (op))
    return NULL;

  /* If not found then we'll search the list.  */
  while (method_list)
    {
      int i;

      /* Search the method list.  */
      for (i = 0; i < method_list->method_count; ++i)
        {
          Method_t method = &method_list->method_list[i];

          if (method->method_name)
            if (method->method_name->sel_id == op->sel_id)
              return method;
        }

      /* The method wasn't found.  Follow the link to the next list of
         methods.  */
      method_list = method_list->method_next;
    }

  return NULL;
}

/* Returns the uninstalled dispatch table indicator.
 If a class' dispatch table points to __objc_uninstalled_dtable
 then that means it needs its dispatch table to be installed. */
inline
struct sarray *
objc_get_uninstalled_dtable ()
{
  return (void*)__objc_uninstalled_dtable;
}

// This is an ugly hack to make sure that the compiler can do inlining into
// sendmsg2.c from here.  It should be removed and the two compiled separately
// once we drop support for compilers that are too primitive to do cross-module
// inlining.
#include "sendmsg2.c"
