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

#include <stdlib.h>
#include "objc/runtime-legacy.h"
#include "objc/encoding.h"
#include "lock.h"
#include "magic_objects.h"

/* The version number of this runtime.  This must match the number 
   defined in gcc (objc-act.c).  */
#define PROTOCOL_VERSION 2
#define OBJC2_PROTOCOL_VERSION 3

void __objc_sync_init(void);
void __objc_resolve_class_links(void);
void objc_load_class(struct objc_class *class);

/* This list contains all proto_list's not yet assigned class links.  */
static struct objc_list *unclaimed_proto_list = 0; 	/* !T:MUTEX */

/* List of unresolved static instances.  */
static struct objc_list *uninitialized_statics = 0; 	/* !T:MUTEX */

/* Global runtime "write" mutex.  */
static mutex_t objc_runtime_mutex;
objc_mutex_t __objc_runtime_mutex = &objc_runtime_mutex;

/* Number of threads that are alive.  */
int __objc_runtime_threads_alive = 1;			/* !T:MUTEX */

void __objc_register_selector_array(SEL selectors, unsigned long count);

/* Check compiler vs runtime version.  */
static void init_check_module_version (Module_t);

/* Assign isa links to protos.  */
void __objc_init_protocols (struct objc_protocol_list *protos);

/* Add protocol to class.  */
static void __objc_class_add_protocols (Class, struct objc_protocol_list *);

/* Is all categories/classes resolved?  */
BOOL __objc_dangling_categories = NO;           /* !T:UNUSED */

/* Extern function used to reference the Object and NXConstantString
   classes.  */

extern void __objc_force_linking (void);

void
__objc_force_linking (void)
{
  extern void __objc_linking (void);
  __objc_linking ();
}

/* Run through the statics list, removing modules as soon as all its
   statics have been initialized.  */

static void
objc_init_statics (void)
{
  struct objc_list **cell = &uninitialized_statics;
  struct objc_static_instances **statics_in_module;

  LOCK(__objc_runtime_mutex);

  while (*cell)
    {
      int module_initialized = 1;

      for (statics_in_module = (*cell)->head;
	   *statics_in_module; statics_in_module++)
	{
	  struct objc_static_instances *statics = *statics_in_module;
	  Class class = objc_lookup_class (statics->class_name);
	  if (strcmp(statics->class_name, "NXConstantString") == 0)
	  {
		  Class constStr = objc_lookup_class(CONSTANT_STRING_CLASS);
		  if (constStr)
		  {
			  class = constStr;
		  }
	  }

	  if (! class)
	    module_initialized = 0;
	  /* Actually, the static's class_pointer will be NULL when we
             haven't been here before.  However, the comparison is to be
             reminded of taking into account class posing and to think about
             possible semantics...  */
	  else if (class != statics->instances[0]->class_pointer)
	    {
	      id *inst;

	      for (inst = &statics->instances[0]; *inst; inst++)
		{
		  (*inst)->class_pointer = class;

		  /* ??? Make sure the object will not be freed.  With
                     refcounting, invoke `-retain'.  Without refcounting, do
                     nothing and hope that `-free' will never be invoked.  */

		  /* ??? Send the object an `-initStatic' or something to
                     that effect now or later on?  What are the semantics of
                     statically allocated instances, besides the trivial
                     NXConstantString, anyway?  */
		}
	    }
	}
      if (module_initialized)
	{
	  /* Remove this module from the uninitialized list.  */
	  struct objc_list *this = *cell;
	  *cell = this->tail;
	  objc_free (this);
	}
      else
	cell = &(*cell)->tail;
    }

  UNLOCK(__objc_runtime_mutex);
} /* objc_init_statics */

void __objc_init_protocol_table(void);
/* This function is called by constructor functions generated for each
   module compiled.  (_GLOBAL_$I$...) The purpose of this function is
   to gather the module pointers so that they may be processed by the
   initialization routines as soon as possible.  */

BOOL objc_check_abi_version(unsigned long version, unsigned long module_size);

void
__objc_exec_class (Module_t module)
{
  /* Have we processed any constructors previously?  This flag is used to
     indicate that some global data structures need to be built.  */
  static BOOL previous_constructors = 0;

  static struct objc_list *unclaimed_categories = 0;

  /* The symbol table (defined in objc-api.h) generated by gcc */
  Symtab_t symtab = module->symtab;

  /* The statics in this module */
  struct objc_static_instances **statics
    = symtab->defs[symtab->cls_def_cnt + symtab->cat_def_cnt];

  /* Entry used to traverse hash lists */
  struct objc_list **cell;

  /* dummy counter */
  int i;

  DEBUG_PRINTF ("received module: %s\n", module->name);

  /* check compiler version */
  assert(objc_check_abi_version(module->version, module->size));

  /* On the first call of this routine, initialize some data structures.  */
  // FIXME: This should really be using a pthread_once or equivalent.
  if (! previous_constructors)
    {
	/* Initialize thread-safe system */
      INIT_LOCK(objc_runtime_mutex);
      __objc_init_thread_system ();
      __objc_sync_init();
      __objc_runtime_threads_alive = 1;

      __objc_init_selector_tables ();
      __objc_init_protocol_table ();
      __objc_init_class_tables ();
      __objc_init_dispatch_tables ();
      previous_constructors = 1;
    }

  /* Save the module pointer for later processing. (not currently used) */
  LOCK(__objc_runtime_mutex);

  /* Replace referenced selectors from names to SEL's.  */
  if (symtab->refs)
    {
      __objc_register_selector_array(symtab->refs, symtab->sel_ref_cnt);
    }

  /* Parse the classes in the load module and gather selector information.  */
  DEBUG_PRINTF ("gathering selectors from module: %s\n", module->name);
  for (i = 0; i < symtab->cls_def_cnt; ++i)
    {
		objc_load_class((Class) symtab->defs[i]);
   }

  /* Process category information from the module.  */
  for (i = 0; i < symtab->cat_def_cnt; ++i)
    {
      Category_t category = symtab->defs[i + symtab->cls_def_cnt];
      Class class = objc_lookup_class (category->class_name);
      
      /* If the class for the category exists then append its methods.  */
      if (class)
	{

	  DEBUG_PRINTF ("processing categories from (module,object): %s, %s\n",
			module->name,
			class->name);

	  /* Do instance methods.  */
	  if (category->instance_methods)
	    class_add_method_list (class, category->instance_methods);

	  /* Do class methods.  */
	  if (category->class_methods)
	    class_add_method_list ((Class) class->class_pointer, 
				   category->class_methods);

	  if (category->protocols)
	    {
	      __objc_init_protocols (category->protocols);
	      __objc_class_add_protocols (class, category->protocols);
	    }
	}
      else
	{
	  /* The object to which the category methods belong can't be found.
	     Save the information.  */
	  unclaimed_categories = list_cons (category, unclaimed_categories);
	}
    }

  if (statics)
    uninitialized_statics = list_cons (statics, uninitialized_statics);
  if (uninitialized_statics)
    objc_init_statics ();

  /* Scan the unclaimed category hash.  Attempt to attach any unclaimed
     categories to objects.  */
  for (cell = &unclaimed_categories; *cell; )
    {
      Category_t category = (*cell)->head;
      Class class = objc_lookup_class (category->class_name);
      
      if (class)
	{
	  DEBUG_PRINTF ("attaching stored categories to object: %s\n",
			class->name);
	  
	  list_remove_head (cell);
	  
	  if (category->instance_methods)
	    class_add_method_list (class, category->instance_methods);
	  
	  if (category->class_methods)
	    class_add_method_list ((Class) class->class_pointer,
				   category->class_methods);

	  if (category->protocols)
	    {
	      __objc_init_protocols (category->protocols);
	      __objc_class_add_protocols (class, category->protocols);
	    }
	}
      else
	cell = &(*cell)->tail;
    }
  
  if (unclaimed_proto_list && objc_lookup_class ("Protocol"))
    {
      list_mapcar (unclaimed_proto_list,
		   (void (*) (void *))__objc_init_protocols);
      list_free (unclaimed_proto_list);
      unclaimed_proto_list = 0;
    }

	__objc_resolve_class_links();

  UNLOCK(__objc_runtime_mutex);
}

void __objc_compute_ivar_offsets(Class class);


struct objc_protocol *__objc_unique_protocol(struct objc_protocol*);
void __objc_init_protocols (struct objc_protocol_list *protos)
{
  size_t i;
  static Class proto_class = 0;
  static Class proto_class2 = 0;

  if (! protos)
    return;

  LOCK(__objc_runtime_mutex);

  if (! proto_class)
    proto_class = objc_lookup_class ("Protocol");
  if (! proto_class2)
    proto_class2 = objc_lookup_class ("Protocol2");

  /* Protocol2 will always exist if Protocol exists */
  if (! proto_class2)
    {
      unclaimed_proto_list = list_cons (protos, unclaimed_proto_list);
      UNLOCK(__objc_runtime_mutex);
      return;
    }

#if 0
  assert (protos->next == 0);	/* only single ones allowed */
#endif

  for (i = 0; i < protos->count; i++)
    {
      struct objc_protocol *aProto = protos->list[i];
      switch (((size_t)aProto->class_pointer))
        {
	  case PROTOCOL_VERSION:
	    {
	      /* assign class pointer */
	      aProto->class_pointer = proto_class;

	      /* init super protocols */
	      __objc_init_protocols (aProto->protocol_list);
	      protos->list[i] = __objc_unique_protocol (aProto);
		  break;
	    }
	    // FIXME: Initialize empty protocol by updating fields to reflect
	    // those of a real protocol with the same name
	  case OBJC2_PROTOCOL_VERSION:
	    {
	      /* assign class pointer */
	      aProto->class_pointer = proto_class2;
	      /* init super protocols */
	      __objc_init_protocols (aProto->protocol_list);
	      protos->list[i] = __objc_unique_protocol (aProto);
	    }
	  default:
	    {
	      if (protos->list[i]->class_pointer != proto_class
		  && protos->list[i]->class_pointer != proto_class2)
	        {
	          objc_error (nil, OBJC_ERR_PROTOCOL_VERSION,
		     "Version %d doesn't match runtime protocol version %d\n",
		     (int) ((char *) protos->list[i]->class_pointer
			    - (char *) 0),
		     PROTOCOL_VERSION);
	        }
	    }
	}
    }

  UNLOCK(__objc_runtime_mutex);
}

static void
__objc_class_add_protocols (Class class, struct objc_protocol_list *protos)
{
  /* Well...  */
  if (! protos)
    return;

  /* Add it...  */
  protos->next = class->protocols;
  class->protocols = protos;
}
