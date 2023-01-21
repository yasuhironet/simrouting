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

#ifndef _GRAPH_CMD_H_
#define _GRAPH_CMD_H_

void graph_clear_config (struct graph *g);
void graph_save_config (struct graph *g);

void graph_command_install (struct command_set *cmdset);
struct graph *graph_lookup (unsigned long id);

EXTERN_COMMAND (graph_enter);
EXTERN_COMMAND (show_graph_id);

void graph_init ();
void graph_finish ();

#endif /*_GRAPH_CMD_H_*/

