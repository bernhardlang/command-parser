/* state-machine.h
 *
 * Copyright (C) 2017-2018 Bernhard Lang, www.bernhard-lang.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 */

#ifndef _state_machine_h_
#define _state_machine_h_

#include <stdint.h>

const uint16_t er_state_machine_no_match = 20;

// table for state machine, describes conditions for transitions between states
typedef struct {
  char character; // character to match
  bool (*check)(char c); // function for character match,
  int8_t from, to; // end points of the transition 
  uint8_t (*action)(char c, void *data); // callback in case of a match
} transition_t;

class c_state_machine {
 public:
  // Create a state machine on a given table of transition rules.
  // The state machine does not take ownership of the table. Its instance must
  // be kept valid during the whole lifetime of the state machine's instance.
  c_state_machine(const transition_t *transitions, int8_t initial_state);

  // Check the character for a match in the actual state. The data argument
  // is handed down to the callback function in case a transition rule matches.
  uint8_t step(char c, void *data);

  int8_t state(void) const { return _state; };
  void set_state(uint8_t s) { _state = s; };

 private:
  const transition_t *transitions;
  int8_t _state;
};

#endif /* _state_machine_h_ */
