/* A splay-tree datatype.  
   Copyright 1998, 1999, 2000, 2002, 2005, 2007, 2009, 2010
   Free Software Foundation, Inc.
   Contributed by Mark Mitchell (mark@markmitchell.com).

   This file is part of GCC.
   
   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to
   the Free Software Foundation, 51 Franklin Street - Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* For an easily readable description of splay-trees, see:

     Lewis, Harry R. and Denenberg, Larry.  Data Structures and Their
     Algorithms.  Harper-Collins, Inc.  1991.  

   The major feature of splay trees is that all basic tree operations
   are amortized O(log n) time for a tree with n nodes.  */

//MemoryAccessTracker stores allocations in a Splay Tree. This allows a
//faster mapping of memory accesses to the correpsonding memory
//block which is done in order to compute offsets. MemoryAccessTracker 
//uses the Splay-Tree implementation from gcc. The implementation is modified 
//to support the lookup of address ranges.

#include <stdlib.h>
#include "splay-tree.h"
#include "stdio.h"

static void splay_tree_delete_helper (splay_tree, splay_tree_node);
static inline void rotate_left (splay_tree_node *,
				splay_tree_node, splay_tree_node);
static inline void rotate_right (splay_tree_node *,
				splay_tree_node, splay_tree_node);
static void splay_tree_splay (splay_tree, splay_tree_key, splay_tree_value=0);
static uint64_t splay_tree_foreach_helper (splay_tree, splay_tree_node,
                                      splay_tree_foreach_fn, void*);

static void splay_tree_delete_helper (splay_tree sp, splay_tree_node node)
{
  splay_tree_node pending = 0;
  splay_tree_node active = 0;

  if (!node)
    return;

#define KDEL(x)  if (sp->delete_key) (*sp->delete_key)(x);
#define VDEL(x)  if (sp->delete_value) (*sp->delete_value)(x);

  KDEL (node->key);
  VDEL (node->value);

  node->key = (splay_tree_key)pending;
  pending = (splay_tree_node)node;

  while (pending)
    {
      active = pending;
      pending = 0;
      while (active)
	{
	  splay_tree_node temp;

	  if (active->left)
	    {
	      KDEL (active->left->key);
	      VDEL (active->left->value);
	      active->left->key = (splay_tree_key)pending;
	      pending = (splay_tree_node)(active->left);
	    }
	  if (active->right)
	    {
	      KDEL (active->right->key);
	      VDEL (active->right->value);
	      active->right->key = (splay_tree_key)pending;
	      pending = (splay_tree_node)(active->right);
	    }

	  temp = active;
	  active = (splay_tree_node)(temp->key);
	  (*sp->deallocate) ((char*) temp, sp->allocate_data);
	}
    }
#undef KDEL
#undef VDEL
}

static inline void rotate_left (splay_tree_node *pp, splay_tree_node p, splay_tree_node n)
{  
  splay_tree_node tmp;
  tmp = n->right;
  n->right = p;
  p->left = tmp;
  *pp = n;
}

static inline void rotate_right (splay_tree_node *pp, splay_tree_node p, splay_tree_node n)
{
  splay_tree_node tmp;
  tmp = n->left;
  n->left = p;
  p->right = tmp;
  *pp = n;
}

static void splay_tree_splay (splay_tree sp, splay_tree_key key, splay_tree_value value)
{ 
  if (sp->root == 0) return;

  do {
    int cmp1, cmp2;
    splay_tree_node n, c;

    n = sp->root;
    cmp1 = splay_tree_compare(n, key, value);

    if (cmp1 == 0) return;
   
    if (cmp1 < 0)
      c = n->left;
    else
      c = n->right;
    if (!c)
      return;

    cmp2 = splay_tree_compare(c, key, value);
    if (cmp2 == 0
        || (cmp2 < 0 && !c->left)
        || (cmp2 > 0 && !c->right))
      {
	if (cmp1 < 0)
	  rotate_left (&sp->root, n, c);
	else
	  rotate_right (&sp->root, n, c);
        return;
      }

    if (cmp1 < 0 && cmp2 < 0)
      {
	rotate_left (&n->left, c, c->left);
	rotate_left (&sp->root, n, n->left);
      }
    else if (cmp1 > 0 && cmp2 > 0)
      {
	rotate_right (&n->right, c, c->right);
	rotate_right (&sp->root, n, n->right);
      }
    else if (cmp1 < 0 && cmp2 > 0)
      {
	rotate_right (&n->left, c, c->right);
	rotate_left (&sp->root, n, n->left);
      }
    else if (cmp1 > 0 && cmp2 < 0)
      {
	rotate_left (&n->right, c, c->left);
	rotate_right (&sp->root, n, n->right);
      }
  } while (1);
}

static uint64_t splay_tree_foreach_helper (splay_tree sp, splay_tree_node node,
                           splay_tree_foreach_fn fn, void *data)
{
  uint64_t val;

  if (!node)
    return 0;

  val = splay_tree_foreach_helper (sp, node->left, fn, data);
  if (val)
    return val;

  val = (*fn)(node, data);
  if (val)
    return val;

  return splay_tree_foreach_helper (sp, node->right, fn, data);
}

static void * splay_tree_xmalloc_allocate (int size, void *data ){//ATTRIBUTE_UNUSED){
  return (void *) malloc (size);
}

static void splay_tree_xmalloc_deallocate (void *object, void *data ){//ATTRIBUTE_UNUSED){
  free (object);
}


splay_tree splay_tree_new (splay_tree_compare_fn compare_fn,
                splay_tree_delete_key_fn delete_key_fn,
                splay_tree_delete_value_fn delete_value_fn)
{
  return (splay_tree_new_with_allocator
          (compare_fn, delete_key_fn, delete_value_fn,
           splay_tree_xmalloc_allocate, splay_tree_xmalloc_deallocate, 0));
}


splay_tree splay_tree_new_with_allocator (splay_tree_compare_fn compare_fn,
                               splay_tree_delete_key_fn delete_key_fn,
                               splay_tree_delete_value_fn delete_value_fn,
                               splay_tree_allocate_fn allocate_fn,
                               splay_tree_deallocate_fn deallocate_fn,
                               void *allocate_data)
{
  splay_tree sp = (splay_tree) (*allocate_fn) (sizeof (struct splay_tree_s),
                                               allocate_data);
  sp->root = 0;
  sp->comp = compare_fn;
  sp->delete_key = delete_key_fn;
  sp->delete_value = delete_value_fn;
  sp->allocate = allocate_fn;
  sp->deallocate = deallocate_fn;
  sp->allocate_data = allocate_data;
  
  return sp;
}

void splay_tree_delete (splay_tree sp)
{
  splay_tree_delete_helper (sp, sp->root);
  (*sp->deallocate) ((char*) sp, sp->allocate_data);
}

splay_tree_node splay_tree_insert (splay_tree sp, splay_tree_key key, splay_tree_value value)
{ 
  //printf("Insert %lu %lu\n", key, value); 
  int comparison = 0;
  splay_tree_splay (sp, key, value);
  if (sp->root)
    comparison = (*sp->comp)(sp->root, key); 
  if (sp->root && comparison == 0)
  {  
    if (sp->delete_value)
    	(*sp->delete_value)(sp->root->value);
     sp->root->value = value;
     sp->root->key = key;
   } 
  else 
    {
      splay_tree_node node;
      
      node = ((splay_tree_node)
              (*sp->allocate) (sizeof (struct splay_tree_node_s),
                               sp->allocate_data));
      node->key = key;
      node->value = value;
      if (!sp->root)
	node->left = node->right = 0;
      else if (comparison < 0)
	{
	  node->left = sp->root;
	  node->right = node->left->right;
	  node->left->right = 0;
	}
      else
	{
	  node->right = sp->root;
	  node->left = node->right->left;
	  node->right->left = 0;
	}
      sp->root = node;
    }
  return sp->root;
}


void splay_tree_remove (splay_tree sp, splay_tree_key key)
{
  splay_tree_splay (sp, key);

  if (sp->root && splay_tree_splay_compare_for_delete(sp->root,  key) == 0)
    { 
      splay_tree_node left, right;

      left = sp->root->left;
      right = sp->root->right;
      if (sp->delete_value)
	(*sp->delete_value) (sp->root->value);
      (*sp->deallocate) (sp->root, sp->allocate_data);
      if (left)
	{
	  sp->root = left;

	  if (right)
	    {
	      while (left->right)
		left = left->right;
	      left->right = right;
	    }
	}
      else
	sp->root = right;
    }
}

splay_tree_node splay_tree_lookup (splay_tree sp, splay_tree_key key)
{ 
  splay_tree_splay (sp, key);
  if ((sp->root) && splay_tree_compare(sp->root, key, key) == 0)
    return sp->root;
  else
    return 0;
}


uintptr_t splay_tree_foreach (splay_tree sp, splay_tree_foreach_fn fn, void *data){
  return splay_tree_foreach_helper (sp, sp->root, fn, data);
}


int splay_tree_compare_ints (splay_tree_node k1,  splay_tree_key k2)
{
  if (( k1->key < k2) && (k2 <  k1->value))
    return 0;
  else if (( k1->key <  k2 ) && (k1->value <= k2))
    return 1;
  else if ( k1->key >=  k2)  
    return -1;
  else 
    return -2;
}

int splay_tree_compare(splay_tree_node k1, splay_tree_key k2, splay_tree_value v2)
{
  if (( k1->key <= k2) && ( k2 <=  k1->value))  return 0;
  if (( k1->key <= v2) && ( v2 <= k1->value))  return 0;
  else if (( k1->key >= k2) && (k1->key >= v2) )  return 1;
  else if (( k1->value <= k2 ) && (k1->key <= k2))  return -1;
  else  return -2;
}

int splay_tree_splay_compare_for_delete(splay_tree_node k1, splay_tree_key k2){
    if (( k1->key == k2)) return 0;
    else if (k1->key > k2) return 1;
    else if (k1->key < k2) return -1;
    else return -2;
}

