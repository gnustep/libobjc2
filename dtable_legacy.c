/* GNU Objective C Runtime initialization 
   Copyright (C) 1993, 1995, 1996, 1997, 2002, 2009
   Free Software Foundation, Inc.
   Contributed by Kresten Krab Thorup
   +load support contributed by Ovidiu Predescu <ovidiu@net-community.com>

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

/* The uninstalled dispatch table */
struct sarray *__objc_uninstalled_dtable = 0;   /* !T:MUTEX */

static struct sarray *create_dtable_for_class (Class class);


void
__objc_init_dispatch_tables ()
{
  INIT_LOCK(initialize_lock);
  __objc_uninstalled_dtable = sarray_new (200, 0);
}

/**
 * Returns the dtable for a given class.  If we are currently in an +initialize
 * method then this will block if called from a thread other than the one
 * running the +initialize method.  
 */
static inline struct sarray *dtable_for_class(Class cls)
{
  if (cls->dtable != __objc_uninstalled_dtable)
    {
      return cls->dtable;
    }
  LOCK(&initialize_lock);
  if (cls->dtable != __objc_uninstalled_dtable)
    {
      UNLOCK(&initialize_lock);
      return cls->dtable;
    }
  /* This is a linear search, and so, in theory, could be very slow.  It is
   * O(n) where n is the number of +initialize methods on the stack.  In
   * practice, this is a very small number.  Profiling with GNUstep showed that
   * this peaks at 8. */
  struct sarray *dtable = __objc_uninstalled_dtable;
  InitializingDtable *buffer = temporary_dtables;
  while (NULL != buffer)
  {
      if (buffer->class == cls)
      {
          dtable = buffer->dtable;
          break;
      }
      buffer = buffer->next;
  }
  UNLOCK(&initialize_lock);
  if (dtable == 0)
    dtable = __objc_uninstalled_dtable;
  return dtable;
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

/* Install dummy table for class which causes the first message to
   that class (or instances hereof) to be initialized properly */
void
__objc_install_premature_dtable (Class class)
{
  assert (__objc_uninstalled_dtable);
  class->dtable = __objc_uninstalled_dtable;
}   

/* Walk on the methods list of class and install the methods in the reverse
   order of the lists. Since methods added by categories are before the methods
   of class in the methods list, this allows categories to substitute methods
   declared in class. However if more than one category replaces the same
   method nothing is guaranteed about what method will be used.
   Assumes that __objc_runtime_mutex is locked down. */
static void
__objc_install_methods_in_dtable (Class class, MethodList_t method_list, 
        struct sarray *dtable)
{
  int i;

  if (! method_list)
    return;

  if (method_list->method_next)
    __objc_install_methods_in_dtable (class, method_list->method_next, dtable);


  for (i = 0; i < method_list->method_count; i++)
    {
      Method_t method = &(method_list->method_list[i]);
      size_t sel_id = (size_t)method->method_name->sel_id;
      /* If there is an existing slot with this value, just update it. */
      struct objc_slot *slot = sarray_get_safe(dtable, sel_id);
      if (NULL != slot && slot->owner == class)
        {
          slot->method = method->method_imp;
          slot->version++;
        }
      else
        {
          //NOTE: We can improve this by sharing slots between subclasses where
          // the IMPs are the same.
          slot = new_slot_for_method_in_class(method, class);
          sarray_at_put_safe (dtable, sel_id, slot);
          /* Invalidate the superclass's slot, if it has one. */
          slot = (NULL != class->super_class) ? 
              sarray_get_safe(dtable_for_class(class->super_class), sel_id) : NULL;
          if (NULL != slot)
            {
              slot->version++;
            }
        }
    }
}

/** Returns the dispatch table for the given class, but does not install it.
 */
static struct sarray *create_dtable_for_class (Class class)
{
  Class super;
  struct sarray *dtable = dtable_for_class(class);
  if (dtable != __objc_uninstalled_dtable) return dtable;

  /* If the class has not yet had its class links resolved, we must 
     re-compute all class links */
  if (! CLS_ISRESOLV (class))
    objc_resolve_class(class);

  super = class->super_class;

  if (super != 0 && (dtable_for_class(super) == __objc_uninstalled_dtable))
    __objc_install_dispatch_table_for_class (super);

  /* Allocate dtable if necessary */
  if (super == 0)
    {
      dtable = sarray_new (__objc_selector_max_index, 0);
    }
  else
    {
      dtable = sarray_lazy_copy (dtable_for_class(super));
    }

  __objc_install_methods_in_dtable (class, class->methods, dtable);
  return dtable;
}

/* Assumes that __objc_runtime_mutex is locked down. */
static void
__objc_install_dispatch_table_for_class(Class class)
{
  class->dtable = create_dtable_for_class(class);
}


static void merge_method_list_to_class (Class class,
        MethodList_t method_list,
        struct sarray *dtable,
        struct sarray *super_dtable)
{
  // Sometimes we get method lists with no methods in them.  This is weird and
  // probably caused by someone else writing stupid code, but just ignore it,
  // nod, smile, and move on.
  if (method_list->method_count == 0)
    {
      if (method_list->method_next)
        {
          merge_method_list_to_class(class, method_list->method_next, dtable, super_dtable);
        }
      return;
    }
  /*
  struct objc_slot *firstslot = 
  //    sarray_get_safe(dtable, (size_t)method_list->method_list[0].method_name->sel_id);
  // If we've already got the methods from this method list, we also have all
  // of the methods from all of the ones further along the chain, so don't
  // bother adding them again.
   * FIXME: This doesn't take into account the lazy copying stuff in the sarray.
  if (NULL != firstslot &&
      firstslot->owner == class &&
      firstslot->method == method_list->method_list[0].method_imp)
    {
      return;
    }
	*/
  // If we haven't already visited this method list, then we might not have
  // already visited the one after it either...
  if (method_list->method_next)
    {
      merge_method_list_to_class(class, method_list->method_next, dtable, super_dtable);
    }

  {
    int i;

    /* Search the method list.  */
    for (i = 0; i < method_list->method_count; ++i)
      {
        Method_t method = &method_list->method_list[i];
        size_t sel_id = (size_t)method->method_name->sel_id;
        struct objc_slot *slot = sarray_get_safe(dtable, sel_id);
        struct objc_slot *superslot = sarray_get_safe(super_dtable, sel_id);

        // If there is no existing slot, then just use the superclass's one.
        if (NULL == slot)
          {
            sarray_at_put_safe (dtable, sel_id, superslot);
          }
        else
          {
            // If there is, we need to find whether it comes from a subclass of
            // the modified class.  If it does, then we don't want to override it.
            Class owner = slot->owner;
            do
              {
                if (owner == superslot->owner)
                  {
                    break;
                  }
                owner = owner->super_class;
              } while(NULL != owner);
            // This is reached if class is currently inheriting a method from a
            // class up the hierarchy and a new method is added to a class
            // somewhere in the middle that overrides it.
            if (owner != superslot->owner)
              {
                sarray_at_put_safe (dtable, sel_id, superslot);
                slot->version++;
              }
          }
      }
  } 
}
static void merge_methods_up_from_superclass (Class class, Class super, Class modifedClass,
        struct sarray *dtable,
        struct sarray *super_dtable)
{
  if (super->super_class && super != modifedClass)
    {
      merge_methods_up_from_superclass(class, super->super_class, modifedClass,
              dtable, super_dtable);
    }
  if (super->methods)
    {
      merge_method_list_to_class(class, super->methods, dtable, super_dtable);
    }
}

static void merge_methods_to_subclasses (Class class, Class modifedClass)
{
  struct sarray *super_dtable = dtable_for_class(class);
  // Don't merge things into the uninitialised dtable.  That would be very bad.
  if (super_dtable == __objc_uninstalled_dtable) { return; }


  if (class->subclass_list)        /* Traverse subclasses */
    {
      for (Class next = class->subclass_list; next; next = next->sibling_class)
        {
          struct sarray *dtable = dtable_for_class(next);
          // Don't merge things into the uninitialised dtable.  That would be very bad.
          if (dtable != __objc_uninstalled_dtable)
            {
              merge_methods_up_from_superclass(next, class, modifedClass, dtable,
                      super_dtable);
              merge_methods_to_subclasses(next, modifedClass);
            }
        }
    }
}

void
__objc_update_dispatch_table_for_class (Class class)
{
  /* not yet installed -- skip it */
  if (dtable_for_class(class) == __objc_uninstalled_dtable) 
    return;

  objc_mutex_lock (__objc_runtime_mutex);

  __objc_install_methods_in_dtable (class, class->methods,
      dtable_for_class(class));
  // Don't merge things into the uninitialised dtable.  That would be very bad.
  if (dtable_for_class(class) != __objc_uninstalled_dtable)
    {
      merge_methods_to_subclasses(class, class);
    }

  objc_mutex_unlock (__objc_runtime_mutex);
}

void objc_resize_uninstalled_dtable(void)
{
	assert(__objc_uninstalled_dtable != NULL);
	sarray_realloc (__objc_uninstalled_dtable, __objc_selector_max_index + 1);
}

