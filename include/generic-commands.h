/* generic-commands.h
 *
 * Copyright (C) 2018-2025 Bernhard Lang, www.bernhard-lang.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#ifndef _generic_commands_h_
#define _generic_commands_h_

#include "command-parser.h"

DECLARE_COMMANDS(generic);

extern unsigned long loop_counter;

int8_t ident(uint8_t argc, char **argv, c_command_parser *);
int8_t identver(uint8_t argc, char **argv, c_command_parser *);
int8_t identno(uint8_t argc, char **argv, c_command_parser *); 
int8_t hardwno(uint8_t argc, char **argv, c_command_parser *);
int8_t loopctr(uint8_t argc, char **argv, c_command_parser *);
int8_t reset(uint8_t argc, char **argv, c_command_parser *);
int8_t compdt(uint8_t argc, char **argv, c_command_parser *);

// Arduino
int8_t getmillis(uint8_t argc, char **argv, c_command_parser *);
int8_t getmicros(uint8_t argc, char **argv, c_command_parser *);
int8_t getpulsein(uint8_t argc, char **argv, c_command_parser *);
int8_t setpinmode(uint8_t argc, char **argv, c_command_parser *);
int8_t setpin(uint8_t argc, char **argv, c_command_parser *);
int8_t aref(uint8_t argc, char **argv, c_command_parser *);
int8_t getad(uint8_t argc, char **argv, c_command_parser *);
int8_t setda(uint8_t argc, char **argv, c_command_parser *);
int8_t setdav(uint8_t argc, char **argv, c_command_parser *);

int8_t pioconf(uint8_t argc, char **argv, c_command_parser *p);

// Arduino Due only

#ifdef USE_TWI
int8_t i2cwr(uint8_t argc, char **argv, c_command_parser *);
int8_t i2crd(uint8_t argc, char **argv, c_command_parser *);
int8_t i2cask(uint8_t argc, char **argv, c_command_parser *);
#endif
#ifdef  USE_FLASH
int8_t  prgdump(uint8_t argc, char **argv, c_command_parser *);
#endif

void init_generic_commands(void);

#endif // _generic_commands_h_
