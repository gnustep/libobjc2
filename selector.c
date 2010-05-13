/* GNU Objective C Runtime selector related functions
   Copyright (C) 1993, 1995, 1996, 1997, 2002, 2004, 2009 Free Software Foundation, Inc.
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


#include "objc/runtime-legacy.h"
#include "objc/sarray.h"
#include "objc/encoding.h"

/* Register instance methods as class methods for root classes */
void __objc_register_instance_methods_to_class (Class class)
{
  MethodList_t method_list;
  MethodList_t class_method_list;
  int max_methods_no = 32;
  MethodList_t new_list;
  Method_t curr_method;

  /* Only if a root class. */
  if (class->super_class)
    return;

  /* Allocate a method list to hold the new class methods */
  new_list = objc_calloc (sizeof (struct objc_method_list)
			    + sizeof (struct objc_method[max_methods_no]), 1);
  method_list = class->methods;
  class_method_list = class->class_pointer->methods;
  curr_method = &new_list->method_list[0];

  /* Iterate through the method lists for the class */
  while (method_list)
    {
      int i;

      /* Iterate through the methods from this method list */
      for (i = 0; i < method_list->method_count; i++)
	{
	  Method_t mth = &method_list->method_list[i];
	  if (mth->method_name
	      && ! search_for_method_in_list (class_method_list,
					      mth->method_name))
	    {
	      /* This instance method isn't a class method. 
		  Add it into the new_list. */
	      *curr_method = *mth;
  
	      /* Reallocate the method list if necessary */
	      if (++new_list->method_count == max_methods_no)
		new_list =
		  objc_realloc (new_list, sizeof (struct objc_method_list)
				+ sizeof (struct 
					objc_method[max_methods_no += 16]));
	      curr_method = &new_list->method_list[new_list->method_count];
	    }
	}

      method_list = method_list->method_next;
    }

  /* If we created any new class methods
     then attach the method list to the class */
  if (new_list->method_count)
    {
      new_list =
 	objc_realloc (new_list, sizeof (struct objc_method_list)
		     + sizeof (struct objc_method[new_list->method_count]));
      new_list->method_next = class->class_pointer->methods;
      class->class_pointer->methods = new_list;
    }
  else
    objc_free(new_list);

    __objc_update_dispatch_table_for_class (class->class_pointer);
}
