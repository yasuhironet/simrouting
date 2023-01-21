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

#include <includes.h>

#include "pqueue.h"

/* priority queue using heap sort */

/* pqueue->cmp() controls the order of sorting (i.e, ascending or
   descending). If you want the left node to move upper of the heap
   binary tree, make cmp() to return less than 0.  for example, if cmp
   (10, 20) returns -1, the sorting is ascending order. if cmp (10,
   20) returns 1, the sorting is descending order. if cmp (10, 20)
   returns 0, this library does not do sorting (which will not be what
   you want).  To be brief, if the contents of cmp_func (left, right)
   is left - right, dequeue () returns the smallest node.  Otherwise
   (if the contents is right - left), dequeue () returns the largest
   node.  */

void
pqueue_update_not_necessary (void *data, int index)
{
}

#define DATA_SIZE (sizeof (void *))
#define PARENT_OF(x) ((x - 1) / 2)
#define LEFT_OF(x)  (2 * x + 1)
#define RIGHT_OF(x) (2 * x + 2)
#define HAVE_CHILD(x,q) (x < (q)->size / 2)

static int
trickle_up (int start_index, struct pqueue *queue)
{
  int index = start_index;
  void *data;

  /* Save current node as tmp node.  */
  data = queue->array[index];

  /* Continue until the node reaches top or the place where the parent
     node should be upper than the tmp node.  */
  while (index > 0 &&
         (*queue->cmp) (data, queue->array[PARENT_OF (index)]) < 0)
    {
      /* actually trickle up */
      queue->array[index] = queue->array[PARENT_OF (index)];

      /* update the index of the moved node */
      (*queue->update) (queue->array[index], index);

      index = PARENT_OF (index);
    }

  /* Restore the tmp node to appropriate place.  */
  queue->array[index] = data;

  /* update the index of the moved node */
  (*queue->update) (queue->array[index], index);

  /* return resulting index */
  return index;
}

static int
trickle_down (int start_index, struct pqueue *queue)
{
  int index = start_index;
  void *data;
  int which;

  /* Save current node as tmp node.  */
  data = queue->array[index];

  /* Continue until the node have at least one (left) child.  */
  while (HAVE_CHILD (index, queue))
    {
      /* If right child exists, and if the right child is more proper
         to be moved upper.  */
      if (RIGHT_OF (index) < queue->size &&
          (*queue->cmp) (queue->array[LEFT_OF (index)],
                         queue->array[RIGHT_OF (index)]) > 0)
        which = RIGHT_OF (index);
      else
        which = LEFT_OF (index);

      /* If the tmp node should be upper than the child, break.  */
      if ((*queue->cmp) (queue->array[which], data) > 0)
        break;

      /* Actually trickle down the tmp node.  */
      queue->array[index] = queue->array[which];

      /* update the index of the moved node */
      (*queue->update) (queue->array[index], index);

      index = which;
    }

  /* Restore the tmp node to appropriate place.  */
  queue->array[index] = data;

  /* update the index of the moved node */
  (*queue->update) (queue->array[index], index);

  /* return resulting index */
  return index;
}

struct pqueue *
pqueue_create ()
{
  struct pqueue *queue;

  queue = (struct pqueue *) malloc (sizeof (struct pqueue));
  memset (queue, 0, sizeof (struct pqueue));

  queue->array = (void **)
    malloc (DATA_SIZE * PQUEUE_INIT_ARRAYSIZE);
  memset (queue->array, 0, DATA_SIZE * PQUEUE_INIT_ARRAYSIZE);
  queue->array_size = PQUEUE_INIT_ARRAYSIZE;

  return queue;
}

void
pqueue_delete (struct pqueue *queue)
{
  free (queue->array);
  free (queue);
}

static int
pqueue_expand (struct pqueue *queue)
{
  void **newarray;

  newarray = (void **) malloc (queue->array_size * DATA_SIZE * 2);
  if (newarray == NULL)
    return 0;

  memset (newarray, 0, queue->array_size * DATA_SIZE * 2);
  memcpy (newarray, queue->array, queue->array_size * DATA_SIZE);

  free (queue->array);
  queue->array = newarray;
  queue->array_size *= 2;

  return 1;
}

void
pqueue_enqueue (void *data, struct pqueue *queue)
{
  int index;

  if (queue->size + 2 >= queue->array_size && ! pqueue_expand (queue))
    return;

  index = queue->size;
  queue->array[index] = data;
  queue->size ++;
  trickle_up (index, queue);
}

void *
pqueue_dequeue (struct pqueue *queue)
{
  void *data = queue->array[0];

  queue->array[0] = queue->array[queue->size - 1];
  queue->size --;
  trickle_down (0, queue);
  return data;
}

void
pqueue_update (int start_index, struct pqueue *queue)
{
  int index = start_index;
  index = trickle_up (index, queue);
  index = trickle_down (index, queue);
}


