/* A splay-tree datatype.  
   Copyright (C) 1998, 1999, 2000, 2001, 2009,
   2010, 2011 Free Software Foundation, Inc.
   Contributed by Mark Mitchell (mark@markmitchell.com).

This file is part of GNU CC.
   
GNU CC is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street - Fifth Floor,
Boston, MA 02110-1301, USA.  */

/* For an easily readable description of splay-trees, see:

     Lewis, Harry R. and Denenberg, Larry.  Data Structures and Their
     Algorithms.  Harper-Collins, Inc.  1991.  */

//MemoryAccessTracker stores allocations in a Splay Tree. This allows a
//faster mapping of memory accesses to the correpsonding memory
//block which is done in order to compute offsets. MemoryAccessTracker 
//uses the Splay-Tree implementation from gcc. The implementation is modified 
//to support the lookup of address ranges.


extern "C" {
#include <stdint.h>
#include "ansidecl.h"
#include <inttypes.h>

typedef uint64_t splay_tree_key;
typedef uint64_t splay_tree_value;

typedef struct splay_tree_node_s *splay_tree_node;

typedef int (*splay_tree_compare_fn) (splay_tree_node, splay_tree_key);

typedef void (*splay_tree_delete_key_fn) (splay_tree_key);

typedef void (*splay_tree_delete_value_fn) (splay_tree_value);

typedef uint64_t (*splay_tree_foreach_fn) (splay_tree_node, void*);

typedef void *(*splay_tree_allocate_fn) (int, void *);

typedef void (*splay_tree_deallocate_fn) (void *, void *);

struct splay_tree_node_s {
  splay_tree_key key;
  splay_tree_value value;
  splay_tree_node left;
  splay_tree_node right;
};

struct splay_tree_s {
  splay_tree_node root;
  splay_tree_compare_fn comp;
  splay_tree_delete_key_fn delete_key;
  splay_tree_delete_value_fn delete_value;
  splay_tree_allocate_fn allocate;
  splay_tree_deallocate_fn deallocate;
  void *allocate_data;
};

typedef struct splay_tree_s *splay_tree;

extern splay_tree splay_tree_new (splay_tree_compare_fn,
				  splay_tree_delete_key_fn,
				  splay_tree_delete_value_fn);
extern splay_tree splay_tree_new_with_allocator (splay_tree_compare_fn,
						 splay_tree_delete_key_fn,
						 splay_tree_delete_value_fn,
						 splay_tree_allocate_fn,
						 splay_tree_deallocate_fn,
						 void *);
extern splay_tree splay_tree_new_typed_alloc (splay_tree_compare_fn,
					      splay_tree_delete_key_fn,
					      splay_tree_delete_value_fn,
					      splay_tree_allocate_fn,
					      splay_tree_allocate_fn,
					      splay_tree_deallocate_fn,
					      void *);
extern void splay_tree_delete (splay_tree);
extern splay_tree_node splay_tree_insert (splay_tree,
					  splay_tree_key,
					  splay_tree_value);
extern void splay_tree_remove	(splay_tree, splay_tree_key);
extern splay_tree_node splay_tree_lookup (splay_tree, splay_tree_key);
extern int splay_tree_splay_compare_for_delete(splay_tree_node, splay_tree_key);
extern int splay_tree_compare(splay_tree_node, splay_tree_key, splay_tree_value);
extern int splay_tree_compare_ints (splay_tree_node, splay_tree_key);
extern int splay_tree_splay_compare_lookup(splay_tree_node k1,  splay_tree_key k2);
}
