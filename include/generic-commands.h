/* generic-commands.h
 *
 * Copyright (C) 2018 Bernhard Lang, www.b-lang.ch
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _generic_commands_h_
#define _generic_commands_h_

class c_command_parser;

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

#endif // _generic_commands_h_
