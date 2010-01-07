/* GNU Objective C Runtime class related functions
   Copyright (C) 1993, 1995, 1996, 1997, 2001, 2002, 2009
     Free Software Foundation, Inc.
   Contributed by Kresten Krab Thorup and Dennis Glatting.

   Lock-free class table code designed and written from scratch by
   Nicola Pero, 2001.

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

/*
  The code in this file critically affects class method invocation
  speed.  This long preamble comment explains why, and the issues
  involved.  


  One of the traditional weaknesses of the GNU Objective-C runtime is
  that class method invocations are slow.  The reason is that when you
  write
  
  array = [NSArray new];
  
  this gets basically compiled into the equivalent of 
  
  array = [(objc_get_class ("NSArray")) new];
  
  objc_get_class returns the class pointer corresponding to the string
  `NSArray'; and because of the lookup, the operation is more
  complicated and slow than a simple instance method invocation.  
  
  Most high performance Objective-C code (using the GNU Objc runtime)
  I had the opportunity to read (or write) work around this problem by
  caching the class pointer:
  
  Class arrayClass = [NSArray class];
  
  ... later on ...
  
  array = [arrayClass new];
  array = [arrayClass new];
  array = [arrayClass new];
  
  In this case, you always perform a class lookup (the first one), but
  then all the [arrayClass new] methods run exactly as fast as an
  instance method invocation.  It helps if you have many class method
  invocations to the same class.  
  
  The long-term solution to this problem would be to modify the
  compiler to output tables of class pointers corresponding to all the
  class method invocations, and to add code to the runtime to update
  these tables - that should in the end allow class method invocations
  to perform precisely as fast as instance method invocations, because
  no class lookup would be involved.  I think the Apple Objective-C
  runtime uses this technique.  Doing this involves synchronized
  modifications in the runtime and in the compiler.  
  
  As a first medicine to the problem, I [NP] have redesigned and
  rewritten the way the runtime is performing class lookup.  This
  doesn't give as much speed as the other (definitive) approach, but
  at least a class method invocation now takes approximately 4.5 times
  an instance method invocation on my machine (it would take approx 12
  times before the rewriting), which is a lot better.  

  One of the main reason the new class lookup is so faster is because
  I implemented it in a way that can safely run multithreaded without
  using locks - a so-called `lock-free' data structure.  The atomic
  operation is pointer assignment.  The reason why in this problem
  lock-free data structures work so well is that you never remove
  classes from the table - and the difficult thing with lock-free data
  structures is freeing data when is removed from the structures.  */

#include "objc/runtime-legacy.h"            /* the kitchen sink */
#include "objc/sarray.h"

#include "objc/objc.h"
#include "objc/objc-api.h"

#include "lock.h"

#include <stdlib.h>

/* We use a table which maps a class name to the corresponding class
 * pointer.  The first part of this file defines this table, and
 * functions to do basic operations on the table.  The second part of
 * the file implements some higher level Objective-C functionality for
 * classes by using the functions provided in the first part to manage
 * the table. */

/* Insert a class in the table (used when a new class is registered).  */
void class_table_insert (Class class_pointer);

/* Get a class from the table.  This does not need mutex protection.
   Currently, this function is called each time you call a static
   method, this is why it must be very fast.  */

/* Enumerate over the class table.  */
Class class_table_next (void **e);

/**
 ** Objective-C runtime functions
 **/

/* This is a hook which is called by objc_get_class and
   objc_lookup_class if the runtime is not able to find the class.  
   This may e.g. try to load in the class using dynamic loading.  */
Class (*_objc_lookup_class) (const char *name) = 0;      /* !T:SAFE */

Class class_table_get_safe(const char*);

/* This function adds a class to the class hash table, and assigns the
   class a number, unless it's already known.  */
void
__objc_add_class_to_hash (Class class)
{
  Class h_class;

  LOCK(__objc_runtime_mutex);

  /* Make sure it's not a meta class.  */
  assert (CLS_ISCLASS (class));

  /* Check to see if the class is already in the hash table.  */
  h_class = class_table_get_safe (class->name);
  if (! h_class)
    {
      /* The class isn't in the hash table.  Add the class and assign a class
         number.  */
      static unsigned int class_number = 1;

      CLS_SETNUMBER (class, class_number);
      CLS_SETNUMBER (class->class_pointer, class_number);

      ++class_number;
      class_table_insert (class);
    }

  UNLOCK(__objc_runtime_mutex);
}

/* Get the class object for the class named NAME.  If NAME does not
   identify a known class, the hook _objc_lookup_class is called.  If
   this fails, nil is returned.  */
Class
objc_lookup_class (const char *name)
{
  Class class;

  class = class_table_get_safe (name);

  if (class)
    return class;

  if (_objc_lookup_class)
    return (*_objc_lookup_class) (name);
  else
    return 0;
}

/* Get the class object for the class named NAME.  If NAME does not
   identify a known class, the hook _objc_lookup_class is called.  If
   this fails, an error message is issued and the system aborts.  */
Class
objc_get_class (const char *name)
{
  Class class;

  class = class_table_get_safe (name);

  if (class)
    return class;

  if (_objc_lookup_class)
    class = (*_objc_lookup_class) (name);

  if (class)
    return class;
  
  objc_error (nil, OBJC_ERR_BAD_CLASS, 
              "objc runtime: cannot find class %s\n", name);
  return 0;
}

MetaClass
objc_get_meta_class (const char *name)
{
  return objc_get_class (name)->class_pointer;
}

/* This function provides a way to enumerate all the classes in the
   executable.  Pass *ENUM_STATE == NULL to start the enumeration.  The
   function will return 0 when there are no more classes.  
   For example: 
       id class; 
       void *es = NULL;
       while ((class = objc_next_class (&es)))
         ... do something with class; 
*/
Class
objc_next_class (void **enum_state)
{
  Class class;

  class = class_table_next ( enum_state);
  
  return class;
}

/* Resolve super/subclass links for all classes.  The only thing we
   can be sure of is that the class_pointer for class objects point to
   the right meta class objects.  */
void
__objc_resolve_class_links (void)
{
  void *es = NULL;
  Class object_class = objc_get_class ("Object");
  Class class1;

  assert (object_class);

  LOCK(__objc_runtime_mutex);

  /* Assign subclass links.  */
  while ((class1 = class_table_next (&es)))
    {
      /* Make sure we have what we think we have.  */
      assert (CLS_ISCLASS (class1));
      assert (CLS_ISMETA (class1->class_pointer));

      /* The class_pointer of all meta classes point to Object's meta
         class.  */
      class1->class_pointer->class_pointer = object_class->class_pointer;

      if (! CLS_ISRESOLV (class1))
        {
          CLS_SETRESOLV (class1);
          CLS_SETRESOLV (class1->class_pointer);
              
          if (class1->super_class)
            {   
              Class a_super_class 
                = objc_get_class ((char *) class1->super_class);
              
              assert (a_super_class);
              
              DEBUG_PRINTF ("making class connections for: %s\n",
                            class1->name);
              
              /* Assign subclass links for superclass.  */
              class1->sibling_class = a_super_class->subclass_list;
              a_super_class->subclass_list = class1;
              
              /* Assign subclass links for meta class of superclass.  */
              if (a_super_class->class_pointer)
                {
                  class1->class_pointer->sibling_class
                    = a_super_class->class_pointer->subclass_list;
                  a_super_class->class_pointer->subclass_list 
                    = class1->class_pointer;
                }
            }
          else /* A root class, make its meta object be a subclass of
                  Object.  */
            {
              class1->class_pointer->sibling_class 
                = object_class->subclass_list;
              object_class->subclass_list = class1->class_pointer;
            }
        }
    }

  /* Assign superclass links.  */
   es = NULL;
   while ((class1 = class_table_next (&es)))
    {
      Class sub_class;
      for (sub_class = class1->subclass_list; sub_class;
           sub_class = sub_class->sibling_class)
        {
          sub_class->super_class = class1;
          if (CLS_ISCLASS (sub_class))
            sub_class->class_pointer->super_class = class1->class_pointer;
        }
    }

  UNLOCK(__objc_runtime_mutex);
}

Class
class_pose_as (Class impostor, Class super_class)
{
	fprintf(stderr, "Class posing is no longer supported.\n");
	fprintf(stderr, "Please use class_replaceMethod() instead.\n");
	abort();
}
