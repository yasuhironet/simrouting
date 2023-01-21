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

#ifndef _DEFLECTION_H_
#define _DEFLECTION_H_

struct prev_hop_entry
{
  struct node *prev_hop;
  struct vector *deflection_set_2;
  struct vector *deflection_set_3;
};

struct deflection_info
{
  int nnodes;
  struct candidate **table; /* SPF result */
  struct vector **deflection_set_1;
  struct vector **prev_hop_table;
};

EXTERN_COMMAND (routing_algorithm_deflection);
EXTERN_COMMAND (show_deflection_set);
EXTERN_COMMAND (show_deflection_number);

EXTERN_COMMAND (show_deflection_path_1);
EXTERN_COMMAND (show_deflection_path_2);
EXTERN_COMMAND (show_deflection_path_3);

EXTERN_COMMAND (save_deflection_set);
EXTERN_COMMAND (load_deflection_set);

EXTERN_COMMAND (show_deflection_path_number_rule_1);
EXTERN_COMMAND (show_deflection_path_number_rule_2);
EXTERN_COMMAND (show_deflection_path_number_rule_3);

#endif /*_DEFLECTION_H_*/

