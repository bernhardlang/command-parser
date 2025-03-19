/* command-parser.h
 *
 * Copyright (C) 2018 Bernhard Lang, www.bernhard-lang.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#ifndef _command_parser_h_
#define _command_parser_h_

#include <stdint.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif
#ifdef ARDUINO_SAM_DUE
# define PROGMEM
#endif
#ifdef TESTING
#include "debug-stream.h"
#endif
#include "state-machine.h"

#ifdef ARDUINO_SAM_DUE
#define MAX_CMD_LEN            15
#define MAX_HELP_LEN          127
#define MAX_ERROR_LEN          63
#define MAX_COMMAND_LINE_LEN  127
#define MAX_ARGUMENTS          16
#define OUTPUT_BUF_SIZE       128
#else // !ARDUINO_SAM_DUE
#define MAX_CMD_LEN            15
#define MAX_HELP_LEN           45
#define MAX_ERROR_LEN          51
#define MAX_COMMAND_LINE_LEN   63
#define MAX_ARGUMENTS          16
#define OUTPUT_BUF_SIZE       128
#endif

#define CMD_PARSER_MAGIC 0x81

const uint16_t er_no_error               = 0;
const uint16_t er_overflow               = 1;
const uint16_t er_too_few_arguments      = 2;
const uint16_t er_too_many_arguments     = 3;
const uint16_t er_undefined_command      = 4;
const uint16_t er_syntax                 = 5;
const uint16_t er_illegal_value          = 6;

const uint16_t er_invalid_pin     = 101;
const uint16_t er_invalid_ad      = 102;
const uint16_t er_no_adc_instance = 103;
const uint16_t er_invalid_dac     = 104;

typedef struct {
  uint16_t code;
  const char msg[MAX_ERROR_LEN+1];
} error_t;

extern const char *application_name;
extern const uint8_t application_version;
extern const uint8_t application_revision;
extern const uint8_t max_pin;

class c_command_parser;
class c_state_machine;
class c_ring_buffer;

/*! c_command holds the data preprocessed by the command parser once a command
    has been recognized on the input line. The callback field is copied from
    the command's definition for repeated execution.
*/

class c_command {
 public:
  c_command(void);

  char command_line[MAX_COMMAND_LINE_LEN+1]; // space for arguments
  char *argv[MAX_ARGUMENTS]; // c-like command line argument pointer
  uint8_t argc;
  int8_t (*func)(uint8_t argc, char **argv, c_command_parser *); // callback
};

//! defintion of a command accessible through a serial line interface
typedef struct {
  const char name[MAX_CMD_LEN+1];
  int8_t (*func)(uint8_t argc, char **argv, c_command_parser *);
  uint8_t min_args, max_args;
  const char help[MAX_HELP_LEN+1];
} command_t;

class c_command_parser {
 public:
  typedef enum { none=0, LF=1, CR=2, LFCR=3 } term_t;

  typedef struct {
    term_t termination;
    bool echo;
    uint8_t magic;
  } param_t;

  //! constructs a parser which is attached to the given stream.
  c_command_parser(Stream &stream);

#ifdef DEBUG_PARSER
  void set_debug_parser(c_command_parser *);
#endif

  //! register a set of commands.
  static void register_commands(const command_t *commands, uint8_t n_commands,
				const error_t *errors, uint8_t n_errors);
  
  bool begin(const c_command_parser::param_t *);
  //! When echoing is on, each received character is immediately sent back.
  void set_echo(bool e) { params.echo = e; };

  //! report echo state
  bool echo(void) const { return params.echo; };

  //! Set character(s) used as line end when sending back text.
  void set_termination(term_t t);
  term_t termination(void) const { return params.termination; };

  uint8_t add_char(char c);
  uint8_t add_first_char(char c);
  uint8_t new_argument(char c);
  uint8_t execute_command(char c, uint8_t no=0);
  uint8_t command_error(void);
  uint8_t print_error(uint8_t code=0);
#ifdef __no_sprintf__
  void respond(const char *string);
#else
  void respond(const char *format, ...);
  void respond(Stream &, const char *format, ...);
  void compose_response(const char *fmt, ...);
#endif
#ifndef ARDUINO_SAM_DUE
  void respond(const __FlashStringHelper *s);
#endif

  void process_input(void);
  void handle_input(int c);
  char read_from_input(void);
  void help(void);
  void list(void);
  bool break_hit(void) const { return _break_hit; };
  //bool clear_break(void) { _break_hit = false; };
  void insert_new_line(void);

  const command_t *find(const char *name);

  Stream &s(void) { return stream; };
  void print_state(Stream &);

  const param_t *p_params(void) const { return &params; };

 private:
#ifdef DEBUG
   void print_state(int8_t state, Stream &);
#endif // DEBUG
  
   //private:
 public:
  static const command_t *command_tables[];
  static uint8_t num_command_tables;
  static uint8_t num_commands[];
  static uint8_t num_errors[];
  static const uint8_t max_command_tables;
  static const error_t *error_tables[];
  static const transition_t transitions[];
  static const command_t commands[];
  static const error_t error_messages[];
  static int8_t cmd_parser_commands_registered;

  Stream &stream;
#ifdef DEBUG_PARSER
  c_command_parser *debug_parser;
#endif
  c_state_machine state_machine;
  c_command *actual_command, *previous_command, *before_previous_command;
  c_command command_buffers[3];
  uint8_t write_pos;
  bool gave_response, _break_hit;
  uint8_t error_code;
  char output_buffer[OUTPUT_BUF_SIZE];
  param_t params;
};

int8_t help(uint8_t argc, char **argv, c_command_parser *);
int8_t list(uint8_t argc, char **argv, c_command_parser *);
int8_t echo(uint8_t argc, char **argv, c_command_parser *);
int8_t termchar(uint8_t argc, char **argv, c_command_parser *);


#define BEGIN_COMMANDS(name)                                \
class c_##name##_commands {                                 \
 public:                                                    \
  c_##name##_commands(void);				    \
 private:                                                   \
  static const command_t commands[];                        \
  static const error_t errors[];                            \
};                                                          \
const command_t c_##name##_commands::commands[] PROGMEM = {

#define END_COMMANDS(name) }; c_##name##_commands name##_commands \

#define BEGIN_ERRORS(name) \
  const error_t c_##name##_commands::errors[] PROGMEM = {

#ifdef ARDUINO_SAM_DUE
# define DUMMY_ERROR { 0, "" }
#else
# define DUMMY_ERROR
#endif

#define END_ERRORS(name) DUMMY_ERROR};                                         \
c_##name##_commands::c_##name##_commands(void) {                               \
c_command_parser::register_commands(commands,                                  \
				    sizeof(commands) / sizeof(command_t),      \
				    errors, sizeof(errors) / sizeof(error_t)); \
}

#define NO_ERRORS(name) BEGIN_ERRORS(name) END_ERRORS(name)

#define IMPLEMENT_COMMANDS(MAX_command_tables)                               \
  const command_t *c_command_parser::command_tables[MAX_command_tables+2];   \
  uint8_t c_command_parser::num_commands[MAX_command_tables+2];              \
  uint8_t c_command_parser::num_errors[MAX_command_tables+2];                \
  const error_t *c_command_parser::error_tables[MAX_command_tables+2];       \
  const uint8_t c_command_parser::max_command_tables = MAX_command_tables+2;

#endif // _command_parser_h_
