/* command-parser.cpp
 *
 * Copyright (C) 2018 Bernhard Lang, www.bernhard-lang.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef ARDUINO
#endif
#ifdef TESTING
# include <stdio.h>
#endif
#include "state-machine.h"
#include "command-parser.h"

#define FPSTR(pstr_pointer) \
  (reinterpret_cast<const __FlashStringHelper *>(pstr_pointer))

#ifdef ARDUINO_SAM_DUE
# ifdef F
#  undef F
#  define F(arg) arg
# endif
#endif

/******************************************************************************/
/*                   functions used in state machine table                    */
/******************************************************************************/

// To be reusable the table of the state machine cannot hold methods of
// objects. The functions defined here are simple wrappers calling the
// corresponding methods in the comannd parser object.

uint8_t add_char(char c, void *data) {
  return ((c_command_parser *)data)->add_char(c);
}

uint8_t add_first_char(char c, void *data) {
  return ((c_command_parser *)data)->add_first_char(c);
}

uint8_t new_argument(char c, void *data) {
  return ((c_command_parser *)data)->new_argument(c);
}

uint8_t execute_command(char c, void *data) {
  return ((c_command_parser *)data)->execute_command(c);
}

uint8_t repeat_command(char c, void *data) {
  return ((c_command_parser *)data)->execute_command(c, 1);
}

uint8_t repeat2_command(char c, void *data) {
  return ((c_command_parser *)data)->execute_command(c, 2);
}

uint8_t command_error(char, void *data) {
  return ((c_command_parser *)data)->command_error();
}

uint8_t print_error(char c, void *data) {
  return ((c_command_parser *)data)->print_error();
}

bool is_alpha(char c) {
  return ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'));
}

bool is_num(char c) {
  return (c >= '0') && (c <= '9');
}

bool is_blank(char c) {
  return (c == ' ') || (c == '\t');
}


/******************************************************************************/
/*                              static variables                              */
/******************************************************************************/

uint8_t c_command_parser::num_command_tables = 0;
int8_t c_command_parser::cmd_parser_commands_registered = 0;

enum parse_stat_enum {
  parse_begin_line, parse_command_name, parse_begin_first_argument,
  parse_begin_argument, parse_argument, parse_repeat1a, parse_repeat1b,
  parse_repeat2a, parse_repeat2b, parse_error, parse_undefined_state=-1
};

// table for state machine, describes conditions for transitions between states
// fields: #1 character to match, #2 function for character match,
// #3 state in whitch rule is applicable, #4 final state, #5 callback
// the rule is applied when #3 matches together with #1 or #2. 
// Both #1 and #2 being null: default rule for state, applied when none of the
// rules before match, rules after, if any, are ignored

const transition_t c_command_parser::transitions[] PROGMEM = {
  /* at line start */
  { '[', 0, parse_begin_line, parse_repeat1a, NULL },
  { '\n', NULL, parse_begin_line, parse_begin_line, NULL },
  { 0, is_blank, parse_begin_line, parse_begin_line, NULL },
  { 0, 0, parse_begin_line, parse_command_name, ::add_first_char },

  { 'A', 0, parse_repeat1a, parse_repeat1b, NULL },

  { '\n', 0, parse_repeat1b, parse_begin_line, repeat_command },
  { '[', 0, parse_repeat1b, parse_repeat2a, NULL },

  { 'A', 0, parse_repeat2a, parse_repeat2b, NULL },
  { '\n', 0, parse_repeat2b, parse_begin_line, repeat2_command },

  /* command */
  { 0, is_blank, parse_command_name, parse_begin_first_argument, NULL },
  { '\n', NULL, parse_command_name, parse_begin_line, ::execute_command },
  { 0, NULL, parse_command_name, parse_command_name, ::add_char },

  /* begin first argument */
  { 0, is_blank, parse_begin_first_argument, parse_begin_first_argument, NULL },
  { '\n', NULL, parse_begin_first_argument, parse_begin_line, ::execute_command},
  { 0, NULL, parse_begin_first_argument, parse_argument, ::new_argument },

  /* begin argument */
  { 0, is_blank, parse_begin_argument, parse_begin_argument, NULL },
  { '\n', NULL, parse_begin_argument, parse_begin_line, ::command_error },
  { 0, NULL, parse_begin_argument, parse_argument, ::new_argument },

  /* argument */
  { ',' , NULL, parse_argument, parse_begin_argument, NULL },
  { '\n', NULL, parse_argument, parse_begin_line, ::execute_command },
  { 0, is_blank, parse_argument, parse_begin_line, ::command_error },
  { 0 , NULL, parse_argument, parse_argument, ::add_char },

  /* error */
  { '\n', NULL, parse_error, parse_begin_line, ::print_error },
  { 0, NULL, parse_error, parse_error, NULL },

  /* dummy at end */
  { 0, NULL, parse_undefined_state, parse_undefined_state, NULL }
};

const command_t c_command_parser::commands[] PROGMEM = {
  {"help",    ::help,     0, 1, "Ask for help for command"},
  {"list",    ::list,     0, 0, "List application commands"},
  {"echo",    ::echo,     0, 1, "Ask or set command echo flag"},
  {"term",    ::termchar, 0, 1,
   "get/set term. char (0=None|1=LF|2=CR|3=LFCR)"}
};

const error_t c_command_parser::error_messages[] PROGMEM = {
  { er_overflow,           "command line too long" },
  { er_too_few_arguments,  "too few arguments" },
  { er_too_many_arguments, "too many arguments" },
  { er_undefined_command,  "undefined command" },
  { er_syntax,             "syntax error" },
  { er_illegal_value,      "illegal value" },
  { er_no_error,           "" }
};


/******************************************************************************/
/*                             command parser class                           */
/******************************************************************************/

c_command::c_command(void)
  : argc(0) {
  argv[0] = command_line;
}

c_command_parser::c_command_parser(Stream &_stream)
  : stream(_stream), state_machine(transitions, parse_begin_line), write_pos(0),
    _break_hit(false), error_code(0) {
  actual_command = command_buffers;
  previous_command = command_buffers + 1;
  before_previous_command = command_buffers + 2;
  if (!cmd_parser_commands_registered) {
    register_commands(commands, sizeof(commands) / sizeof(command_t),
		      error_messages, sizeof(error_messages) / sizeof(error_t));
    cmd_parser_commands_registered = 1;
  }
#ifdef DEBUG_PARSER
  debug_parser = NULL;
#endif
}

#ifdef DEBUG_PARSER
void c_command_parser::set_debug_parser(c_command_parser *p) {
  debug_parser = p;
}
#endif

bool c_command_parser::begin(const c_command_parser::param_t *_params) {
  if (_params)
    memcpy(&params, _params, sizeof(c_command_parser::param_t));
  if (_params && (params.magic == CMD_PARSER_MAGIC))
    return true;
  params.magic = CMD_PARSER_MAGIC;
  params.echo = true;
  params.termination = LFCR;

  return false;
}

void c_command_parser::register_commands(const command_t *commands, uint8_t nc,
					 const error_t *errors, uint8_t ne) {
  if (num_command_tables < max_command_tables) {
    command_tables[num_command_tables] = commands;
    error_tables[num_command_tables] = errors;
    num_commands[num_command_tables] = nc;
    num_errors[num_command_tables++] = ne;
  } else {
    cmd_parser_commands_registered = -1;
  }
}

char line_buf[64];
uint8_t buf_pos = 0;

void c_command_parser::handle_input(int c) {
  int8_t result;

  if (c <= 0) return;
  if (c == 27) {
    _break_hit = true;
    return;
  }
  if (c == '\r') c = '\n';
  if (params.echo && (c != '\r') && (c != '\n')) {
    stream.write(c);
#ifdef DEBUG_PARSER
    if (debug_parser) debug_parser->s().write(c);
#endif
  }
  line_buf[buf_pos++] = c;
  result = state_machine.step(c, this);
  if (c == '\n') {
    line_buf[buf_pos] = 0;
    buf_pos = 0;
  }
  if (result) {
    if (c == '\n') {
      print_error(result);
      respond("faulty line was '%s'", line_buf);
    } else {
      if (error_code) {
        state_machine.set_state(parse_error);
        error_code = result;
      }
    }
  }

#ifdef DEBUG_PARSER
  /*
  if (debug_parser) {
    print_state(debug_parser->s());
    debug_parser->respond("");
  }
  */
  //print_state(state_machine.state());
#endif
}

void c_command_parser::process_input(void) {
  char c;
  while (stream.available()) {
    c = stream.read();
    handle_input(c);
  }
}

char c_command_parser::read_from_input(void) {
  char c;

  while (!stream.available()) {};
  c = stream.read();
  if (c == 27)
    _break_hit = true;
  return c;
}

char temp_buf[20];

const command_t *c_command_parser::find(const char *name) {
  uint8_t i = 0, j;
#ifndef ARDUINO_SAM_DUE
  const command_t *pc;
#endif

  while (i < num_command_tables) {
#ifdef ARDUINO_SAM_DUE
    for (j = 0; command_tables[i][j].name[0]; j++)
      if (!strcmp(command_tables[i][j].name, name))
        return &command_tables[i][j];
#else
    for (j = 0; j < num_commands[i]; j++) {
      pc = command_tables[i] + j;
      if (!strcmp_P(name, pc->name))
	return pc;
    }
#endif
    i++;
  }

  return NULL;
}

uint8_t c_command_parser::add_char(char c) {
  if (write_pos >= MAX_COMMAND_LINE_LEN)
    return er_overflow;
  actual_command->command_line[write_pos++] = c;
  actual_command->command_line[write_pos] = 0;
  return 0;
}

uint8_t c_command_parser::add_first_char(char c) {
  add_char(c);
  actual_command->argc = 0;
  return 0;
}

uint8_t c_command_parser::new_argument(char c) {
  if (actual_command->argc == MAX_ARGUMENTS)
    return er_too_many_arguments;
  actual_command->argv[++actual_command->argc]
    = &actual_command->command_line[++write_pos];
  add_char(c);
  return 0;
}

uint8_t c_command_parser::execute_command(char, uint8_t no) {
  int8_t result;
  uint8_t n_args;
  const command_t *pc;
#ifndef ARDUINO_SAM_DUE
  int8_t (*func)(uint8_t, char **, c_command_parser *);
  int n;
#endif

  gave_response = false;
  write_pos = 0;
  switch (no) { /* repeate previsous command? */
  case 1:
    result = previous_command->func(previous_command->argc, actual_command->argv,
                                    this);
    goto finish;

  case 2:
    result = before_previous_command->func(before_previous_command->argc,
                                           actual_command->argv, this);
    goto finish;
  }

  n_args = actual_command->argc++;

  /* is new command */
  pc = find(actual_command->command_line);
  if (!pc)
    return er_undefined_command;

#ifdef ARDUINO_SAM_DUE
  if (n_args < pc->min_args)
    return er_too_few_arguments;
  if (n_args > pc->max_args)
    return er_too_many_arguments;
  result = pc->func(actual_command->argc, actual_command->argv, this);
# ifdef DEBUG_PARSER
  if (debug_parser)
    debug_parser->respond("executing command '%s'", actual_command->argv[0]);
# endif
#else // !ARDUINO_SAM_DUE
  func = pgm_read_ptr(&pc->func);
  if (!func)
    return er_undefined_command;

  n = pgm_read_byte(&pc->min_args);
  if (n_args < n)
    return er_too_few_arguments;
  n = pgm_read_byte(&pc->max_args);
  if (n_args > n)
    return er_too_many_arguments;
# ifdef DEBUG_PARSER
  //if (debug_parser)
  //  debug_parser->respond("executing command '%s'", actual_command->argv[0]);
# endif

  result = func(actual_command->argc, actual_command->argv, this);
#endif // !ARDUINO_SAM_DUE

  if (!result) {
    /* rotate command pointers on valid execution */
#ifdef ARDUINO_SAM_DUE
    actual_command->func = pc->func;
#else
    actual_command->func = pgm_read_ptr(&pc->func);
#endif
    c_command *temp = before_previous_command;
    before_previous_command = previous_command;
    previous_command = actual_command;
    actual_command = temp;
  }

 finish:
#ifndef NEWLINE_PATCH
  if (!gave_response)
    insert_new_line();
#endif

  return result;
}

void c_command_parser::insert_new_line(void) {
  switch (params.termination) {
  case LF:   stream.write('\n'); break;
  case CR:   stream.write('\r'); break;
  case LFCR: stream.write("\n\r"); break;
  default: break;
  }
#ifdef DEBUG_PARSER
  if (!debug_parser)
    return;
  switch (params.termination) {
  case LF:   debug_parser->s().write('\n'); break;
  case CR:   debug_parser->s().write('\r'); break;
  case LFCR: debug_parser->s().write("\n\r"); break;
  default: break;
  }
#endif
}

void c_command_parser::respond(const char *fmt, ...) {
  va_list ap;
  size_t size;

  va_start(ap, fmt);
  size = vsnprintf(output_buffer, OUTPUT_BUF_SIZE, fmt, ap);
  va_end(ap);

  if (!gave_response)
    insert_new_line();
  stream.write(output_buffer, size);
#ifdef DEBUG_PARSER
  if (debug_parser)
    debug_parser->s().write(output_buffer, size);
#endif
  insert_new_line();
  gave_response = true;
}

void c_command_parser::compose_response(const char *fmt, ...) {
  va_list ap;
  size_t size;

  va_start(ap, fmt);
  size = vsnprintf(output_buffer, OUTPUT_BUF_SIZE, fmt, ap);
  va_end(ap);

  if (!gave_response) insert_new_line();
  stream.write(output_buffer, size);
#ifdef DEBUG_PARSER
  if (debug_parser)
    debug_parser->s().write(output_buffer, size);
#endif
  gave_response = true;
}

void c_command_parser::respond(Stream &s, const char *fmt, ...) {
  va_list ap;
  size_t size;

  va_start(ap, fmt);
  size = vsnprintf(output_buffer, OUTPUT_BUF_SIZE, fmt, ap);
  va_end(ap);

  s.write(output_buffer, size);
#ifdef DEBUG_PARSER
  if (debug_parser)
    debug_parser->s().write(output_buffer, size);
#endif
}

#ifndef ARDUINO_SAM_DUE
void c_command_parser::respond(const __FlashStringHelper *s) {
  // stream.write used to work, but ceased suddenly :-(
  stream.print(s);
#ifdef DEBUG_PARSER
  if (debug_parser)
    debug_parser->s().print(s);
#endif
}
#endif

void c_command_parser::help(void) {
#ifdef ARDUINO_SAM_DUE
  uint8_t i, j;
#else
  uint8_t i, j, n;
#endif
  const command_t *pc;

  insert_new_line();

  for (i = 0; i < num_command_tables; i++)
    for (j = 0, pc = command_tables[i]; j < num_commands[i]; j++, pc++) {
#ifdef ARDUINO_SAM_DUE
      if (strlen(pc->name) < 8)
        sprintf(output_buffer, "%s\t\t%d,%d\t%s", pc->name, pc->min_args,
                pc->max_args, pc->help);
      else
        sprintf(output_buffer, "%s\t%d,%d\t%s", pc->name, pc->min_args,
                pc->max_args, pc->help);
      stream.write(output_buffer);
#ifdef DEBUG_PARSER
      if (debug_parser) debug_parser->s().write(output_buffer);
#endif
      insert_new_line();
#else
      stream.print(FPSTR(pc->name));
      if (strlen_P(pc->name) < 8)
	stream.write('\t');
      else
	stream.print(F("\t\t"));
      n = pgm_read_byte(&pc->min_args);
      stream.print(n);
      stream.print(',');
      n = pgm_read_byte(&pc->max_args);
      stream.print(n);
      stream.print('\t');
      stream.print(FPSTR(pc->help));

#ifdef DEBUG_PARSER
      if (debug_parser) {
        debug_parser->s().print(FPSTR(pc->name));
        if (strlen_P(pc->name) < 8)
	  debug_parser->s().write('\t');
        else
	  debug_parser->s().print(F("\t\t"));
        n = pgm_read_byte(&pc->min_args);
        debug_parser->s().print(n);
        debug_parser->s().print(',');
        n = pgm_read_byte(&pc->max_args);
        debug_parser->s().print(n);
        debug_parser->s().print('\t');
        debug_parser->s().print(FPSTR(pc->help));
      }
#endif
      insert_new_line();
#endif
    }

  if (cmd_parser_commands_registered < 0) {
    stream.print(F("*** Error: command table overflow"));
#ifdef DEBUG_PARSER
    if (debug_parser)
      debug_parser->s().print(F("*** Error: command table overflow"));
#endif
    insert_new_line();
  }
}

void c_command_parser::list(void) {
  uint8_t i, j, is_first = 1;
  const command_t *pc;

  insert_new_line();

  for (i = 0; i < num_command_tables; i++)
    for (j = 0; j < num_commands[i]; j++) {
      pc = command_tables[i] + j;
      if (is_first) {
	is_first = 0;
      } else {
        stream.print(",");
#ifdef DEBUG_PARSER
        if (debug_parser)
	  debug_parser->s().print(",");
#endif
      }
#ifdef ARDUINO_SAM_SUE
      stream.print(pc->name);
# ifdef DEBUG_PARSER
      if (debug_parser)
	debug_parser->s().print(pc->name);
# endif
#else
      stream.print(FPSTR(pc->name));
# ifdef DEBUG_PARSER
      if (debug_parser)
	debug_parser->s().print(FPSTR(pc->name));
# endif
#endif
    }

  insert_new_line();
}

void c_command_parser::set_termination(c_command_parser::term_t t) {
  if ((t >= none) && (t <= LFCR)) params.termination = t;
}

uint8_t c_command_parser::command_error(void) {
  return er_syntax;
}

uint8_t c_command_parser::print_error(uint8_t code) {
  uint8_t i, j;
#ifndef ARDUINO_SAM_DUE
  uint8_t _code;
  const error_t *pe;
#endif
  if (!code) code = error_code;
  error_code = 0;

  insert_new_line();

#ifdef ARDUINO_SAM_DUE
  for (i = 0; i < num_command_tables; i++)
    for (j = 0; error_tables[i][j].code; j++)
      if (error_tables[i][j].code == code) {
        respond("*** error: %s ***", error_tables[i][j].msg);
        error_code = 0;
        return 0;
      }

  respond("*** error: unknown error code %d ***", code);
#else // avr arduinos
  for (i = 0; i < num_command_tables; i++) {
    j = 0;
    while (1) {
      pe = error_tables[i] + j;
      if (!(_code = pgm_read_byte(&pe->code)))
	break;
      if (_code == code) {
        stream.print(F("*** error: "));
        stream.print(FPSTR(pe->msg));
        stream.print(" ***");
# ifdef DEBUG_PARSER
      if (debug_parser) {
        debug_parser->s().print(F("*** error: "));
        debug_parser->s().print(FPSTR(pe->msg));
        debug_parser->s().print(" ***");
      }
# endif
        insert_new_line();
        error_code = 0;
        return 0;
      }
      j++;
    }
  }

  stream.print(F("*** error: unknown error code "));
  stream.print(code);
  stream.print(F(" ***"));
# ifdef DEBUG_PARSER
  if (debug_parser) {
    debug_parser->s().print(F("*** error: unknown error code "));
    debug_parser->s().print(code);
    debug_parser->s().print(F(" ***"));
  }
# endif
#endif

  insert_new_line();

  return 0;
}

#ifdef DEBUG
void c_command_parser::print_state(Stream &s) {
  print_state(state_machine.state(), s);
}

void c_command_parser::print_state(int8_t state, Stream &s) {
  switch (state) {
  case parse_begin_line: respond(s, "state is now 'begin-line'");         break;
  case parse_command_name: respond(s, "state is now 'command-name'");     break;
  case parse_begin_first_argument:
    respond(s, "state is now 'begin-first-argument'");
    break;
  case parse_begin_argument: respond(s, "state is now 'begin-argument'"); break;
  case parse_argument: respond(s, "state is now 'argument'");             break;
  case parse_repeat1a: respond(s, "state is now 'repeat1a'");             break;
  case parse_repeat1b: respond(s, "state is now 'repeat1b'");             break;
  case parse_repeat2a: respond(s, "state is now 'repeat2a'");             break;
  case parse_repeat2b: respond(s, "state is now 'repeat2b'");             break;
  case parse_undefined_state:
    respond(s, "state is now 'undefined-state'"); break;
  default: respond(s, "state is now ??unknown??"); break;
  }
}
#endif // DEBUG


/******************************************************************************/
/*                       callbacks issued by command parser                   */
/******************************************************************************/

int8_t help(uint8_t argc, char **argv, c_command_parser *p) {
  if (argc > 1) {  // help for single command
    const command_t *pc = p->find(argv[1]);
    if (pc) p->respond("%s: %s", argv[1], pc->help);
    else p->respond("help: undefined command '%s'", argv[1]);
  } else {
    p->help();
  }

  return 0;
}

/**  command list : print all available command names **/
int8_t list(uint8_t argc, char **argv, c_command_parser *p) {
  p->list();
  return 0;
}

/**  term command : set or ask termination char flag **/
int8_t termchar(uint8_t argc, char **argv, c_command_parser *p) {
// command: term [value] : control serial termination characters
  if (argc > 1) {
    uint8_t t = atoi(argv[1]);
    if (t > 3)
      return er_illegal_value;
    p->set_termination((c_command_parser::term_t) t);
  }

  switch (p->termination()) {
  case 0: p->respond(F("none")); break;
  case 1: p->respond(F("LF"));   break;
  case 2: p->respond(F("CR"));   break;
  case 3: p->respond(F("LFCR")); break;
  default: p->respond("undefined term char %d", p->termination()); break;
  }

  return 0;
}

/**  echo command : ask or set echo flag **/
int8_t echo(uint8_t argc, char **argv, c_command_parser *p) {
  if (argc > 1)
    p->set_echo(atoi(argv[1]) != 0);
  else
    p->respond("%d", p->echo());

  return 0;
}
