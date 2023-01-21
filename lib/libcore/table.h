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

#ifndef _TRIE_H_
#define _TRIE_H_

struct table_node
{
  unsigned char *key;
  int key_len;
  void *data;
  unsigned int lock;
  struct table *table;
  struct table_node *parent;
  struct table_node *link[2];
#define left_child  link[0]
#define right_child link[1]
};

struct table
{
  struct table_node *root;
};

/* OCTET_SPACE() returns the number of bytes neccessary to store the bits */
#define OCTET_SPACE(bits) ((((bits)-1)/8)+1)

/* CHECK_BIT() returns L-th bit in the key K */
#define CHECK_BIT(K,L) ((((unsigned char *)(K))[(L)/8] >> (7 - (L)%8)) & 1)

int key_match (void *keyp1, void *keyp2, int len);

struct table_node *
table_lookup_node (void *keyp, int len, struct table *table);
void table_add_node (struct table_node *node, struct table *table);
void table_remove_node (struct table_node *node, struct table *table);

void *table_lookup (void *keyp, int len, struct table *table);
void table_add (void *keyp, int len, void *data, struct table *table);
void table_remove (void *keyp, int len, void *data, struct table *table);

struct table_node *table_head (struct table *table);
struct table_node *table_next (struct table_node *node);

struct table *table_create ();
void table_delete (struct table *table);

void table_dump (struct table *table);

#endif /*_TRIE_H_*/


