/* state-machine.cpp
 *
 * Copyright (C) 2017-2018 Bernhard Lang, www.bernhard-lang.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#ifndef ARDUINO_SAM_DUE
#include <avr/pgmspace.h>
#endif
#include "state-machine.h"

c_state_machine::c_state_machine(const transition_t *_transitions,
                                 int8_t initial_state)
  : transitions(_transitions), _state(initial_state) {
}


#ifdef ARDUINO_SAM_DUE
uint8_t c_state_machine::step(char c, void *data) {
  uint8_t i, result;
  char char_to_match;

  for (i = 0; transitions[i].from >= 0; i++) {
    if (transitions[i].from != _state)
      continue; // initial state doesn't match
    char_to_match = transitions[i].character;
    if (char_to_match && (char_to_match != c))
      continue;
    if (transitions[i].check && !transitions[i].check(c))
      continue;
    _state = transitions[i].to; // match
    if (transitions[i].action) // callback defined?
      result = transitions[i].action(c, data); // call it
    return result;
  }

  return er_state_machine_no_match;
}
#else // !ARDUINO_SAM_DUE
uint8_t c_state_machine::step(char c, void *data) {
  uint8_t i = 0, result, from;
  char char_to_match;
  uint8_t (*action)(char, void *);
  bool (*check)(char c);

  while (1) {
    const transition_t *t = &transitions[i++];
    from = pgm_read_byte(&t->from);
    if (from != _state)
      continue; // initial state doesn't match

    char_to_match = pgm_read_byte(&t->character);
    if (char_to_match && (char_to_match != c))
      continue;
    check = pgm_read_ptr(&t->check);
    if (check && !check(c))
      continue;
    _state = pgm_read_byte(&t->to); // match
    result = 0;
    if ((action = pgm_read_ptr(&t->action))) // callback defined?
      result = action(c, data); // call it
    return result;
  }

  return er_state_machine_no_match;
}
#endif // ARDUINO_SAM_DUE
