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
#include "objc/sarray.h"
#include "objc/encoding.h"

/* This is how we hack STRUCT_VALUE to be 1 or 0.   */
#define gen_rtx(args...) 1
#define gen_rtx_MEM(args...) 1
#define gen_rtx_REG(args...) 1
/* Alread defined in gcc/coretypes.h. So prevent double definition warning.  */
#undef rtx
#define rtx int

#if ! defined (STRUCT_VALUE) || STRUCT_VALUE == 0
#define INVISIBLE_STRUCT_RETURN 1
#else
#define INVISIBLE_STRUCT_RETURN 0
#endif

/* The uninstalled dispatch table */
struct sarray *__objc_uninstalled_dtable = 0;   /* !T:MUTEX */

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

/* Forward declare some functions */
static void __objc_init_install_dtable (id, SEL);

static Method_t search_for_method_in_hierarchy (Class class, SEL sel);
Method_t search_for_method_in_list (MethodList_t list, SEL op);
id nil_method (id, SEL);

/* Given a selector, return the proper forwarding implementation. */
inline
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

struct objc_slot
{
        Class owner;
        Class cachedFor;
        const char *types;
        int version;
        IMP method;
};

// Malloc slots a page at a time.
#define SLOT_POOL_SIZE ((4096) / sizeof(struct objc_slot))
static struct objc_slot *slot_pool;
static int slot_pool_left;

static struct objc_slot *
pool_alloc_slot(void)
{
  if (!slot_pool_left)
    {
      slot_pool = objc_malloc (sizeof (struct objc_slot)
                                   * SLOT_POOL_SIZE);
      slot_pool_left = SLOT_POOL_SIZE;
    }
  return &slot_pool[--slot_pool_left];
}

static inline IMP sarray_get_imp (struct sarray *dtable, size_t key)
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
      /* Not a valid method */
      if (class->dtable == __objc_uninstalled_dtable)
        {
          /* The dispatch table needs to be installed. */
          objc_mutex_lock (__objc_runtime_mutex);

           /* Double-checked locking pattern: Check
              __objc_uninstalled_dtable again in case another thread
              installed the dtable while we were waiting for the lock
              to be released.  */
         if (class->dtable == __objc_uninstalled_dtable)
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
          res = sarray_get_imp (class->dtable, (size_t) sel->sel_id);
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
  if (object->class_pointer->dtable == __objc_uninstalled_dtable)
    {
      objc_mutex_lock (__objc_runtime_mutex);
      if (object->class_pointer->dtable == __objc_uninstalled_dtable)
        {
          __objc_install_dispatch_table_for_class (object->class_pointer);
        }
      objc_mutex_unlock (__objc_runtime_mutex);
    }

  /* Get the method from the dispatch table */
  res = sarray_get_safe (object->class_pointer->dtable, (size_t) sel->sel_id);
  return (res != 0);
}

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
          /* Not a valid method */
          if (receiver->class_pointer->dtable == __objc_uninstalled_dtable)
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
              result = sarray_get_imp (receiver->class_pointer->dtable,
                                        (sidx)op->sel_id);
              if (result == 0)
                {
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

void
__objc_init_dispatch_tables ()
{
  __objc_uninstalled_dtable = sarray_new (200, 0);
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
  if (receiver->class_pointer->dtable != __objc_uninstalled_dtable)
    {
      objc_mutex_unlock (__objc_runtime_mutex);
      return;
    }
  
  if (CLS_ISCLASS (receiver->class_pointer))
    {
      /* receiver is an ordinary object */
      assert (CLS_ISCLASS (receiver->class_pointer));

      /* install instance methods table */
      __objc_install_dispatch_table_for_class (receiver->class_pointer);

      /* call +initialize -- this will in turn install the factory 
         dispatch table if not already done :-) */
      __objc_send_initialize (receiver->class_pointer);
    }
  else
    {
      /* receiver is a class object */
      assert (CLS_ISCLASS ((Class)receiver));
      assert (CLS_ISMETA (receiver->class_pointer));

      /* Install real dtable for factory methods */
      __objc_install_dispatch_table_for_class (receiver->class_pointer);

      __objc_send_initialize ((Class)receiver);
    }
  objc_mutex_unlock (__objc_runtime_mutex);
}

/* Install dummy table for class which causes the first message to
   that class (or instances hereof) to be initialized properly */
void
__objc_install_premature_dtable (Class class)
{
  assert (__objc_uninstalled_dtable);
  class->dtable = __objc_uninstalled_dtable;
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
      CLS_SETINITIALIZED (class);
      CLS_SETINITIALIZED (class->class_pointer);

      /* Create the garbage collector type memory description */
      __objc_generate_gc_type_description (class);

      if (class->super_class)
        __objc_send_initialize (class->super_class);

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
    }
}
/* Walk on the methods list of class and install the methods in the reverse
   order of the lists. Since methods added by categories are before the methods
   of class in the methods list, this allows categories to substitute methods
   declared in class. However if more than one category replaces the same
   method nothing is guaranteed about what method will be used.
   Assumes that __objc_runtime_mutex is locked down. */
static void
__objc_install_methods_in_dtable (Class class, MethodList_t method_list)
{
  int i;

  if (! method_list)
    return;

  if (method_list->method_next)
    __objc_install_methods_in_dtable (class, method_list->method_next);

  for (i = 0; i < method_list->method_count; i++)
    {
      Method_t method = &(method_list->method_list[i]);
      size_t sel_id = (size_t)method->method_name->sel_id;
      /* If there is an existing slot with this value, just update it. */
      struct objc_slot *slot = sarray_get_safe(class->dtable, sel_id);
      if (NULL != slot && slot->owner == class)
        {
          slot->method = method->method_imp;
          slot->version++;
        }
      else
        {
          //NOTE: We can improve this by sharing slots between subclasses where
          // the IMPs are the same.
          slot = pool_alloc_slot();
          slot->owner = class;
          slot->types = method->method_types;
          slot->method = method->method_imp;
          slot->version = 1;
          sarray_at_put_safe (class->dtable, sel_id, slot);
          /* Invalidate the superclass's slot, if it has one. */
          slot = (NULL != class->super_class) ? 
              sarray_get_safe(class->super_class->dtable, sel_id) : NULL;
          if (NULL != slot)
            {
              slot->version++;
            }
        }
    }
}

/* Assumes that __objc_runtime_mutex is locked down. */
static void
__objc_install_dispatch_table_for_class (Class class)
{
  Class super;

  /* If the class has not yet had its class links resolved, we must 
     re-compute all class links */
  if (! CLS_ISRESOLV (class))
    __objc_resolve_class_links ();

  super = class->super_class;

  if (super != 0 && (super->dtable == __objc_uninstalled_dtable))
    __objc_install_dispatch_table_for_class (super);

  /* Allocate dtable if necessary */
  if (super == 0)
    {
      objc_mutex_lock (__objc_runtime_mutex);
      class->dtable = sarray_new (__objc_selector_max_index, 0);
      objc_mutex_unlock (__objc_runtime_mutex);
    }
  else
    class->dtable = sarray_lazy_copy (super->dtable);

  __objc_install_methods_in_dtable (class, class->methods);
}

static void merge_methods_from_superclass (Class class)
{
  Class super = class->super_class;
  do
    {
      MethodList_t method_list = class->methods;
      while (method_list)
        {
          int i;

      /* Search the method list.  */
          for (i = 0; i < method_list->method_count; ++i)
            {
              Method_t method = &method_list->method_list[i];
              size_t sel_id = (size_t)method->method_name->sel_id;
              struct objc_slot *slot = 
                  sarray_get_safe(class->dtable, sel_id);
              // If the slot already exists in this dtable, we have either
              // overridden it in the subclass, or it is already pointing to
              // the same slot as the superclass.  If not, then we just install
              // the slot pointer into this dtable.
              if (NULL == slot)
                {
                  slot = sarray_get_safe(super->dtable, sel_id);
                  // If slot is NULL here, something has gone badly wrong with
                  // the superclass already.
                  sarray_at_put_safe (class->dtable, sel_id, slot);
                }
            }
          method_list = method_list->method_next;
        }
    } 
  while (super = super->super_class);
  if (class->subclass_list)        /* Traverse subclasses */
    for (Class next = class->subclass_list; next; next = next->sibling_class)
      merge_methods_from_superclass (next);
}

void
__objc_update_dispatch_table_for_class (Class class)
{
  Class next;
  struct sarray *arr;

  /* not yet installed -- skip it */
  if (class->dtable == __objc_uninstalled_dtable) 
    return;

  __objc_install_methods_in_dtable (class, class->methods);
  objc_mutex_lock (__objc_runtime_mutex);

  if (class->subclass_list)        /* Traverse subclasses */
    for (next = class->subclass_list; next; next = next->sibling_class)
      merge_methods_from_superclass (next);

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

void
__objc_print_dtable_stats ()
{
  int total = 0;

  objc_mutex_lock (__objc_runtime_mutex);

#ifdef OBJC_SPARSE2
  printf ("memory usage: (%s)\n", "2-level sparse arrays");
#else
  printf ("memory usage: (%s)\n", "3-level sparse arrays");
#endif

  printf ("arrays: %d = %ld bytes\n", narrays, 
          (long) ((size_t) narrays * sizeof (struct sarray)));
  total += narrays * sizeof (struct sarray);
  printf ("buckets: %d = %ld bytes\n", nbuckets, 
          (long) ((size_t) nbuckets * sizeof (struct sbucket)));
  total += nbuckets * sizeof (struct sbucket);

  printf ("idxtables: %d = %ld bytes\n",
          idxsize, (long) ((size_t) idxsize * sizeof (void *)));
  total += idxsize * sizeof (void *);
  printf ("-----------------------------------\n");
  printf ("total: %d bytes\n", total);
  printf ("===================================\n");

  objc_mutex_unlock (__objc_runtime_mutex);
}

/* Returns the uninstalled dispatch table indicator.
 If a class' dispatch table points to __objc_uninstalled_dtable
 then that means it needs its dispatch table to be installed. */
inline
struct sarray *
objc_get_uninstalled_dtable ()
{
  return __objc_uninstalled_dtable;
}

// This is an ugly hack to make sure that the compiler can do inlining into
// sendmsg2.c from here.  It should be removed and the two compiled separately
// once we drop support for compilers that are too primitive to do cross-module
// inlining.
#include "sendmsg2.c"
