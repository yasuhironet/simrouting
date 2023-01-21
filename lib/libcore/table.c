/*
 * Copyright (C) 2007  Yasuhiro Ohara
 * 
 * This file is part of SimRouting.
 * 
 * SimRouting is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * SimRouting is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <includes.h>
#include "table.h"

/* patricia table (trie with oneway branch compression) */

static unsigned char bitmask[] =
  {0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};

static void
table_node_key_set (struct table_node *node, void *keyp, int key_len)
{
  int i;
  unsigned char *new_key;
  unsigned char *key = (unsigned char *) keyp;

  new_key = (unsigned char *) malloc (OCTET_SPACE (key_len) + 1);

  for (i = 0; i < key_len / 8; i++)
    new_key[i] = key[i];
  if (key_len % 8)
    new_key[i] = key[i] & bitmask[key_len % 8];

  node->key = new_key;
  node->key_len = key_len;
}

static void
table_node_key_free (struct table_node *node)
{
  free (node->key);
}

static struct table_node *
table_node_create ()
{
  struct table_node *node;
  node = (struct table_node *) malloc (sizeof (struct table_node));
  memset (node, 0, sizeof (struct table_node));
  return node;
}

static void
table_node_delete (struct table_node *node)
{
  if (node->key)
    table_node_key_free (node);
  free (node);
}

void
table_node_lock (struct table_node *node)
{
  node->lock++;
}

void
table_node_unlock (struct table_node *node)
{
  node->lock--;
  if (node->lock)
    return;

  /* if parent does not have any data then
     also check the oneway-ness of the parent */
  if (node->parent && ! node->parent->data)
    table_remove_node (node->parent, node->table);

  table_node_delete (node);
}

int
key_match (void *keyp1, void *keyp2, int len)
{
  unsigned char *key1 = (unsigned char *) keyp1;
  unsigned char *key2 = (unsigned char *) keyp2;
  int i;
  for (i = 0; i < len / 8; i++)
    if (key1[i] != key2[i])
      return 0;
  if (len % 8 && (key1[i] ^ key2[i]) & bitmask[len % 8])
    return 0;
  return 1;
}

struct table_node *
table_lookup_node_bestmatch (void *keyp, int key_len, struct table *table)
{
  unsigned char *key = (unsigned char *) keyp;
  struct table_node *match = NULL;
  struct table_node *node = table->root;

  while (node && node->key_len <= key_len &&
         key_match (node->key, key, node->key_len))
    {
      match = node;
      if (node->key_len == key_len)
        break;
      node = node->link[CHECK_BIT (key, node->key_len)];
    }

  return match;
}

struct table_node *
table_lookup_node (void *keyp, int key_len, struct table *table)
{
  unsigned char *key = (unsigned char *) keyp;
  struct table_node *node;
  node = table_lookup_node_bestmatch (key, key_len, table);
  if (! node || node->key_len != key_len)
    return NULL;
  assert (key_match (node->key, key, key_len));
  return node;
}

void
set_link (struct table_node *parent, struct table_node *child)
{
  parent->link[CHECK_BIT(child->key, parent->key_len)] = child;
  child->parent = parent;
}

void
set_root (struct table *table, struct table_node *child)
{
  table->root = child;
  child->parent = NULL;
}

static struct table_node *
table_node_branch (struct table_node *a, struct table_node *b)
{
  int i, min_key_len;
  struct table_node *branch;

  min_key_len = MIN (a->key_len, b->key_len);

  branch = table_node_create ();
  branch->key_len = 0;

  for (i = 0; i < min_key_len / 8; i++)
    {
      if (a->key[i] == b->key[i])
        branch->key_len += 8;
      else
        break;
    }

  while (branch->key_len < min_key_len)
    {
      if (CHECK_BIT(a->key, branch->key_len) !=
          CHECK_BIT(b->key, branch->key_len))
        break;
      branch->key_len++;
    }

  if (branch->key_len == a->key_len)
    {
      table_node_delete (branch);
      branch = a;
      return branch;
    }

  if (branch->key_len == b->key_len)
    {
      table_node_delete (branch);
      branch = b;
      return branch;
    }

  table_node_key_set (branch, a->key, branch->key_len);
  return branch;
}

void
table_add_node (struct table_node *new, struct table *table)
{
  struct table_node *node;
  struct table_node *branch;
  struct table_node *match = NULL;

  assert (new->table == NULL);
  assert (new->lock == 0);

  new->table = table;
  table_node_lock (new);

  node = table->root;
  while (node && node->key_len <= new->key_len &&
         key_match (node->key, new->key, node->key_len))
    {
      match = node;
      node = node->link[CHECK_BIT(new->key, node->key_len)];
    }

  /* found already existing entry */
  if (match && match->key_len == new->key_len)
    {
      /* replace existing with new */
      if (! match->parent)
        set_root (table, new);
      else
        set_link (match->parent, new);
      if (match->left_child)
        set_link (new, match->left_child);
      if (match->right_child)
        set_link (new, match->right_child);

      /* free existing */
      table_node_unlock (match);
      return;
    }

  if (node)
    {
      branch = table_node_branch (new, node);
      if (! branch->table)
        {
          branch->table = table;
          table_node_lock (branch);
        }

      if (match)
        set_link (match, branch);
      else
        set_root (table, branch);
      if (branch != node)
        set_link (branch, node);
      if (branch != new)
        set_link (branch, new);
    }
  else
    {
      if (match)
        set_link (match, new);
      else
        set_root (table, new);
    }
}

void
table_remove_node (struct table_node *node, struct table *table)
{
  struct table_node *child = NULL;
  struct table_node *parent = NULL;

  assert (table == node->table);
  assert (node == table_lookup_node (node->key, node->key_len, table));

  node->data = NULL;

  if (node->left_child && node->right_child)
    return;

  /* one-way path compression */
  parent = node->parent;

  if (node->left_child)
    child = node->left_child;
  else if (node->right_child)
    child = node->right_child;

  if (child)
    {
      if (parent)
        set_link (parent, child);
      else
        set_root (table, child);
    }
  else
    {
      if (parent)
        parent->link[CHECK_BIT (node->key, parent->key_len)] = NULL;
      else
        table->root = NULL;
    }

  table_node_unlock (node);
}

void *
table_lookup (void *keyp, int len, struct table *table)
{
  unsigned char *key = (unsigned char *) keyp;
  struct table_node *node;
  node = table_lookup_node (key, len, table);
  if (node)
    return node->data;
  return NULL;
}

void
table_add (void *keyp, int len, void *data, struct table *table)
{
  unsigned char *key = (unsigned char *) keyp;
  struct table_node *node;
  node = table_node_create ();
  table_node_key_set (node, key, len);
  node->data = data;
  table_add_node (node, table);
}

void
table_remove (void *keyp, int len, void *data, struct table *table)
{
  unsigned char *key = (unsigned char *) keyp;
  struct table_node *node;
  node = table_lookup_node (key, len, table);
  if (! node || node->data != data)
    return;
  table_remove_node (node, table);
}

struct table_node *
table_head_node (struct table *table)
{
  struct table_node *head = table->root;
  if (head)
    table_node_lock (head);
  return head;
}

struct table_node *
table_next_node (struct table_node *node)
{
  struct table_node *next = NULL;
  struct table_node *prev = node;

  if (node->left_child)
    {
      next = node->left_child;
      table_node_lock (next);
      table_node_unlock (prev);
      assert (prev != next);
      return next;
    }

  if (node->right_child)
    {
      next = node->right_child;
      table_node_lock (next);
      table_node_unlock (prev);
      assert (prev != next);
      return next;
    }

  next = NULL;
  while (node->parent)
    {
      if (node->parent->left_child == node && node->parent->right_child)
        {
          next = node->parent->right_child;
          break;
        }
      node = node->parent;
    }

  if (next)
    table_node_lock (next);
  table_node_unlock (prev);

  assert (prev != next);

  return next;
}

struct table_node *
table_head (struct table *table)
{
  struct table_node *node = table_head_node (table);
  while (node && ! node->data)
    node = table_next_node (node);
  return node;
}

struct table_node *
table_next (struct table_node *node)
{
  do {
    node = table_next_node (node);
  } while (node && ! node->data);
  return node;
}

struct table *
table_create ()
{
  struct table *table;
  table = (struct table *) malloc (sizeof (struct table));
  memset (table, 0, sizeof (struct table));
  return table;
}

void
table_delete (struct table *table)
{
  struct table_node *node;
  for (node = table_head_node (table); node; node = table_next_node (node))
    table_remove_node (node, table);
  free (table);
}

void
table_dump (struct table *table)
{
  int i;
  struct table_node *node;

  fprintf (stdout, "table dump [%p]:\n", table);

  for (node = table_head_node (table); node; node = table_next_node (node))
    {
      fprintf (stdout, "[%p]:", node);
      for (i = 0; i < node->key_len / 8 + 1; i++)
        {
          if (i % 4 == 0) printf (":");
          fprintf (stdout, "%x", ((u_char *)node->key)[i]);
        }
      fprintf (stdout, "/%d: ", node->key_len);
      if (node->data)
        fprintf (stdout, "%p (%s)", node->data, (char *)node->data);
      fprintf (stdout, "\n");
    }
}


