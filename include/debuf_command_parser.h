/* debug.h
 *
 * Copyright (C) 2023 Bernhard Lang, www.bernhard-lang.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#ifndef _debug_h_
#define _debug_h_

class c_command_parser;

void set_debug_parser(c_command_parser *p);
void debug_print(const char *fmt, ...);


#endif // _debug_h_
