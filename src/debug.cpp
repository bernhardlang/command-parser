/* debug.cpp
 *
 * Copyright (C) 2023 Bernhard Lang, www.bernhard-lang.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#include "debug.h"
#include "command-parser.h"

#ifdef ARDUINO_SAM_DUE

char output_buffer[OUTPUT_BUF_SIZE+1];
c_command_parser *p_debug_parser;

void set_debug_parser(c_command_parser *p) {
  p_debug_parser = p;
}


void debug_print(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(output_buffer, OUTPUT_BUF_SIZE, fmt, ap);
  va_end(ap);
  p_debug_parser->respond("%s", output_buffer);
}

#else // ndef ARDUINO_SAM_DUE
# include <cstdio>
# include <stdarg.h>
//# define PROGMEM

void debug_print(const char *fmt, ...) {
  va_list ap;
  size_t size;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end/(ap);
}

#endif // ndef ARDUINO_SAM_DUE
