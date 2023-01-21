/*
 * Priority queue functions.
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

#ifndef _ZEBRA_PQUEUE_H
#define _ZEBRA_PQUEUE_H

struct pqueue
{
  void **array;
  int array_size;
  int size;

  int (*cmp) (void *, void *);
  void (*update) (void *, int);
};

#define PQUEUE_INIT_ARRAYSIZE  32

void pqueue_update_not_necessary (void *data, int index);

struct pqueue *pqueue_create ();
void pqueue_delete (struct pqueue *queue);

void pqueue_enqueue (void *data, struct pqueue *queue);
void *pqueue_dequeue (struct pqueue *queue);
void pqueue_update (int index, struct pqueue *queue);

#endif /* _ZEBRA_PQUEUE_H */
