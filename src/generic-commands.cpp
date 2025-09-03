/* generic-commands.cpp
 *
 * Copyright (C) 2018-2025 Bernhard Lang, www.bernhard-lang.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#include <Arduino.h>
#include "command-parser.h"
#include "generic-commands.h"

#ifdef ARDUINO_SAM_DUE
# define MAX_PIN 54
#else
# include <avr/wdt.h>
# define MAX_PIN 20 // arduino micro
#endif

unsigned long loop_counter;  // main loop counter
unsigned long loop_time = 0; // main loop last time
unsigned long loop_dur = 0;  // main loop last duration

LIST_COMMANDS(generic) {
  {"*IDN?",   ident,       0, 0, "Ask for identification string"},
  {"++ver",   ident,       0, 0, "idem, but serial gpib compatible"},
  {"ver",     identver,    0, 0, "Ask for firmware version"},
  {"devno",   identno,     0, 1, "Ask or set device no"},
#ifdef ARDUINO_SAM_DUE
  {"hrdno",   hardwno,     0, 0, "Ask hardware no"},
#endif
  {"loop",    loopctr,     0, 0, "Ask for loop counter"},
  {"reset",   reset,       0, 0, "Software reset" },

  {"millis",  getmillis,   0, 0, "Display elapsed milliseconds"},
  {"micros",  getmicros,   0, 0, "Display elapsed microseconds"},
#ifdef NOT_YET
  {"pulsein", getpulsein,  2, 3, "Measure pulse length in microseconds"},
#endif
#ifdef NOT_YET
  {"pinmode", setpinmode,  2, 2,
   "Set pinMode for pin (0..53) (-1=P=in pullup|0=I=in|1=O=out)"},
  {"pin",     setpin,      1, 2, "Set or get pin state (0=low|1=high)"},
  {"aref",    aref,        1, 1, "Set ADC reference(Default|Intern|Extern)"},
#endif
  {"ad",      getad,       1, 1, "Get ADC value"},  

  {"compdt",  compdt,      0, 0, "Ask for compile date time"},
  {"[A",      NULL,        0, 0, "(up arrow) Redo previous command"},
  // up arrow = redo prev command
  {"[A[A",    NULL,        0, 0, "(up arrow 2x) Redo before last command"},
  // up arrow twice = redo prev prev command
};

BEGIN_ERRORS(generic)
  { er_invalid_pin, "invalid arduino pin number" },
  { er_invalid_ad,  "invalid ADC number" },
  { er_invalid_dac, "invalid DAC number" },
  { er_no_error, ""},
END_ERRORS(generic);

void init_generic_commands(void) {
  static c_generic_commands g_cmds;
}

#ifdef ARDUINO_SAM_DUE

/*
From
https://code.google.com/p/android-source-browsing/source/browse/MakefileBasedBuild/Atmel/sam3x/sam3x-ek/libraries/libchip_sam3x/source/flashd.c?spec=svn.device--google--accessory--adk2012-demo.54afe01e01fbf7f67bf19e03855fcd86b454aeff&repo=device--google--accessory--adk2012-demo&r=54afe01e01fbf7f67bf19e03855fcd86b454aeff
*/

/**
 * \brief Read the unique ID.
 *
 * \param pdwUniqueID pointer on a 4bytes char containing the unique ID value.
 * \returns 0 if successful; otherwise returns an error code.
 */
  __attribute__ ((section (".ramfunc")))
uint32_t FLASHD_ReadUniqueID(uint32_t* pdwUniqueID) {
    uint32_t status ;

    if (pdwUniqueID == NULL) {
        return 1;
    }

    pdwUniqueID[0] = 0 ;
    pdwUniqueID[1] = 0 ;
    pdwUniqueID[2] = 0 ;
    pdwUniqueID[3] = 0 ;

    /* Send the Start Read unique Identifier command (STUI) by writing the Flash
       Command Register with the STUI command.*/
    EFC0->EEFC_FCR = (0x5A << 24) | EFC_FCMD_STUI;

    /* When the Unique Identifier is ready to be read, the FRDY bit in the Flash
       Programming Status Register (EEFC_FSR) falls. */
    do
    {
        status = EFC0->EEFC_FSR ;
    } while ( (status & EEFC_FSR_FRDY) == EEFC_FSR_FRDY ) ;

    /* The Unique Identifier is located in the first 128 bits of the Flash
       memory mapping.*/
    pdwUniqueID[0] = *(uint32_t *)IFLASH0_ADDR;
    pdwUniqueID[1] = *(uint32_t *)(IFLASH0_ADDR + 4);
    pdwUniqueID[2] = *(uint32_t *)(IFLASH0_ADDR + 8);
    pdwUniqueID[3] = *(uint32_t *)(IFLASH0_ADDR + 12);

    /* To stop the Unique Identifier mode, the user needs to send the Stop Read
       unique Identifier command (SPUI) by writing the Flash Command Register
       with the SPUI command. */
    EFC0->EEFC_FCR = (0x5A << 24) | EFC_FCMD_SPUI ;

    /* When the Stop read Unique Unique Identifier command (SPUI) has been
       performed, the FRDY bit in the Flash Programming Status Register
       (EEFC_FSR) rises. */
     
    do {
        status = EFC0->EEFC_FSR ;
    } while ( (status & EEFC_FSR_FRDY) != EEFC_FSR_FRDY ) ;
    
    return 0;
}

#endif // ARDUINO_SAM_DUE


/******************************************************************************/
/*                              commands                                      */
/******************************************************************************/

uint8_t device_no = 0;

#ifdef IDENT_HOOK
extern void ident_hook(c_command_parser *);
#endif

/**  ident command : ask for identification string **/
int8_t ident(uint8_t argc, char **argv, c_command_parser *p) {
#ifdef IDENT_HOOK
  if (!strcmp(argv[0], "*IDN?")) {
    ident_hook(p);
    return 0;
  }
#endif
  p->respond("%s V%d.%d", application_name, application_version,
             application_revision);
  return 0;
}

/**  ver command : ask for firmware version number **/
int8_t identver(uint8_t argc, char **argv, c_command_parser *p) {
  p->respond("%d.%d", application_version, application_revision);
  return 0;
}

/** ask or set identification no **/
int8_t identno(uint8_t argc, char **argv, c_command_parser *p) {
  int val;

  if (argc > 0) {
    val = atoi(argv[1]);
    device_no = (uint8_t)(val & 0xff);
  } else
    p->respond("%d", device_no);

  return 0;
}

#ifdef ARDUINO_SAM_DUE
/** ask hardware identification no **/
int8_t hardwno(uint8_t argc, char **argv, c_command_parser *p) {
  uint32_t uid[5]={0,0,0,0,0};
  FLASHD_ReadUniqueID(uid);
  p->respond("%lx %lx %lx %lx   %s", uid[0], uid[1], uid[2],
           uid[3], (char *)uid);
  //#else
  //p->respond("%d", HARDWARE_NO);

  return 0;
}
#endif // ARDUINO_SAM_DUE

/**  loop command : ask for elapsed loop count  **/
int8_t loopctr(uint8_t argc, char **argv, c_command_parser *p) {
  p->respond("%ld, %ld", loop_counter, loop_dur);
  return 0;
}

/**  command reset : restart proc **/
int8_t reset(uint8_t argc, char **argv, c_command_parser *p) {
#ifdef ARDUINO_SAM_DUE
// from http://forum.arduino.cc/index.php/topic,152885.msg1149434.html#msg1149434
const int RSTC_KEY = 0xA5;
  for (unsigned int i=0;i<PINS_COUNT;i++) pinMode(i,INPUT);  // PINS_COUNT (79u)  Number of pins defined in PinDescription array  (in variant.h)
  RSTC->RSTC_CR = RSTC_CR_KEY(RSTC_KEY) | RSTC_CR_PROCRST | RSTC_CR_PERRST;
#else
  wdt_enable(WDTO_30MS);
#endif // ARDUINO_SAM_DUE
  while (true);
  return 0; // dummy
}

int8_t compdt(uint8_t argc, char **argv, c_command_parser *p) {
  p->respond("%s %s",__DATE__,__TIME__);
  return 0;
}

///////// Arduino commands ///////////////////////////////////////////

/**  millis command : get elapsed milliseconds **/
int8_t getmillis(uint8_t argc, char **argv, c_command_parser *p) {
  p->respond("%ld", millis());
  return 0;
}

/**  micros command : get elapsed microseconds **/
int8_t getmicros(uint8_t argc, char **argv, c_command_parser *p) {
  p->respond("%ld", micros());
  return 0;
}

/**  pulsein command : get digital input pulse length **/
int8_t getpulsein(uint8_t argc, char **argv, c_command_parser *p) {
  int arg1n, arg2n;
  unsigned long larg3n, lpul;  
  arg1n = atoi(argv[1]);  // get pin no
  if ((arg1n >= 0) && (arg1n < MAX_PIN)) {
    arg2n = atoi(argv[2]);  // get a numeric value
    if (arg2n == 0) {                  // or it could be a text argument
      if (strchr(argv[2], 'H') != NULL) arg2n = 1;       // H || HIGH
    }
    larg3n = 100000L ;
    if (argc > 2) larg3n = atoi(argv[3]);
    if (arg2n > 0) lpul = pulseIn(arg1n, HIGH, larg3n);
    else lpul = pulseIn(arg1n, LOW, larg3n);
    p->respond("%ld", lpul);
    return 0;
  }
  p->respond("Error: invalid PIN no %d (%d..%d)", arg1n, 0, MAX_PIN - 1);

  return er_invalid_pin;
}

uint8_t pinmodes[MAX_PIN];

int8_t mypinMode(uint32_t ulPin, uint32_t ulMode) {
  pinMode(ulPin, ulMode);
  if ((ulPin >= 0) && (ulPin < MAX_PIN)) pinmodes[ulPin] = (ulMode && 0xff);
  return 0;
}

/**  pinmode command : set pinmode **/
int8_t setpinmode(uint8_t argc, char **argv, c_command_parser *p) {
  int arg1n, arg2n;
  int pm;

  arg1n = atoi(argv[1]);  // get pin no
  if ((arg1n >= 0) && (arg1n < MAX_PIN)) {
    if (argc > 1) {
      arg2n = atoi(argv[2]);  // get a numeric value
      if (arg2n == 0) {                  // or it could be a text argument
        if (strchr(argv[2], 'O') != NULL) arg2n = 1; // O || OUT  || OUTPUT
        else
          if (strchr(argv[2], 'I') != NULL) arg2n = 0; // I || IN   || INPUT
          else
            if (strchr(argv[2], 'P') != NULL) arg2n = -1;
            // P || PULL || PULLUP
      }
      if (arg2n > 1) mypinMode(arg1n, INPUT_PULLUP);
      else
        if (arg2n == 1) mypinMode(arg1n, OUTPUT);
        else
          if (arg2n < 0) mypinMode(arg1n, INPUT_PULLUP);
          else  mypinMode(arg1n, INPUT);
    } else { // argc <= 1
      pm = pinmodes[arg1n];
      p->respond("%d", pm);
    }
    return 0;
  }

  p->respond("Error: invalid PIN no %d (%d..%d)", arg1n, 0, MAX_PIN - 1);

  return er_invalid_pin;
}

/**  pin command : set pin output to HIGH or LOW **/
int8_t setpin(uint8_t argc, char **argv, c_command_parser *p) {
  int arg1n, arg2n;

  arg1n = atoi(argv[1]);
  if ((arg1n >= 0) && (arg1n < MAX_PIN)) {
    if (argc > 1) {
       arg2n = atoi(argv[2]);
       if (arg2n > 0) digitalWrite(arg1n, HIGH);
       else digitalWrite(arg1n, LOW);
    } else
       p->respond("%d", digitalRead(arg1n));

    return 0;
  }

  p->respond("Error: invalid PIN no %d (%d..%d)", arg1n, 0, MAX_PIN - 1);

  return er_invalid_pin;
}

static char analogReferenceValue = 'D' ;

/**  aref command : ask or set analog reference type (D|E)  **/
int8_t aref(uint8_t argc, char **argv, c_command_parser *p) {
  if (argc > 0) {
    if (strchr(argv[1],'D') != NULL) {
#ifdef ARDUINO_SAM_DUE  // board type
      analogReference((eAnalogReference)DEFAULT);
#else
      analogReference(DEFAULT);
#endif
      analogReferenceValue = 'D' ;
#ifdef HasInternalAnalogReference      
    } else if (strchr(argv[1],'I') != NULL) {
      analogReference((eAnalogReference)INTERNAL);
      analogReferenceValue = 'I';
#endif
    } else if (strchr(argv[1],'E') != NULL) {
#ifdef ARDUINO_SAM_DUE  // board type
      analogReference((eAnalogReference)EXTERNAL);
#else
      analogReference(EXTERNAL);
#endif
      analogReferenceValue = 'E' ;
    }
  } else {
    p->respond("%c", analogReferenceValue);
  } 

  return 0;
}

/**  ad command : ask for adc code **/
int8_t getad(uint8_t argc, char **argv, c_command_parser *p) {
  int arg1n ;  

  arg1n = atoi(argv[1]);
  if ((arg1n >= 0) && (arg1n < 12)) {
     p->respond("%d", (int)analogRead(arg1n));
     return 0;
  }

  p->respond("Error: invalid AD no %d (%d..%d)", arg1n, 0, 11);

  return er_invalid_ad;
}

// Arduino Due only

#ifdef ARDUINO_SAM_DUE
static uint8_t analogReadResolutionValue = 10 ;

/**  adres command : ask or set ad resolution bits  **/
int8_t adres(uint8_t argc, char **argv, c_command_parser *p) {
  int arg1n ;  
  if (argc > 0) {
    arg1n = atoi(argv[1]);
    analogReadResolution(arg1n);
    analogReadResolutionValue = arg1n ;
  } else
    p->respond("%d", analogReadResolutionValue);

  return 0;
}
#endif // ARDUINO_SAM_DUE



/////// TWI (I2C) ///////////////////////////////////////////////////////

#ifdef USE_TWI
/** i2cwr command : i2c bus write **/
int8_t i2cwr(uint8_t argc, char **argv, c_command_parser *p) {
  uint16_t addr, port=0xffff, wrpacket[5], wrps, n ;
  sscanf(argv[1], "%hx", &addr);
  if (strlen(argv[2])) sscanf(argv[2], "%hx", &port);
  n = 0 ;
  while ((n < 5) && (argc > (n+2))) {
    sscanf(argv[n+3], "%hx", &wrpacket[n]);
    n++;
  }
  wrps = n ;
  Wire.beginTransmission(addr);
  if (!(port & 0xff00)) Wire.write(port);
  n = 0 ;
  while (n < wrps) Wire.write(wrpacket[n++]);
  Wire.endTransmission();

  return 0;
}

/** i2crd command : i2c bus read  **/
int8_t i2crd(uint8_t argc, char **argv, c_command_parser *p) {
  uint16_t addr, port=0xffff, rdb, rdps ;
  int ndat = 1;
  char buf[8];
  sscanf(argv[1], "%hx", &addr);
  if (strlen(argv[2])) sscanf(argv[2], "%hx", &port);
  if (argc > 2) sscanf(argv[3], "%d", &ndat);
  if (ndat < 1) ndat = 1 ;
  if (ndat > 8) ndat = 8 ;
  Wire.beginTransmission(addr);
  if (!(port & 0xff00)) Wire.write(port);       // MSB
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)addr, (uint8_t)ndat);
  rdps = 0;
  while (Wire.available() && (rdps++ < ndat)) {
    rdb = Wire.read();
    sprintf(buf, " %02x", rdb);
    strcat(cmd->answ, buf);
  }
}

/** i2cask command : i2c write and read  **/
int8_t i2cask(uint8_t argc, char **argv, c_command_parser *p) {
   i2cwr(cmd);
   i2crd(cmd);
}
#endif

//////////// FLash //////////////////////////////////////////////

#define MAXBUF1 128

#ifdef USE_FLASH
#define PICOCOM_EOF_CHAR  0x1a      // Control-Z, ascii 26, is the default EOF char

// prgdump                          dump prog. to Serial output with xxd format
// With                             picocom -b 115200 -fx --receive-cmd "ascii-xfr -rv" /dev/ttyACMx
// type Ctrl - { C A R } to start receive ASCFILE, Ctrl-C to interrupt
// To get back binary file, type    cat ASCFILE | xxd -r BINFILE
// To program back asc or bin file, use dueupload
// ascii-xfr is included in package minicom
int8_t  prgdump(uint8_t argc, char **argv, c_command_parser *p) {       // dump program from flash memory
  char buf[MAXBUF1] ;
  char bufc[32] ;
  char temp[8];
  uint8_t b;
  uint8_t c = 0;
  bool allff;         // true if all ff
  uint8_t allffcount ;   // count all ff lines
  unsigned long msend, msdel=30;

  cmd->cmdparser->breaked = false ;

  if (argc>0) sscanf(argv[1], "%ld", &msdel);
  
  sprintf(buf, "Type Ctrl-C A R to start receive a file after %ld S", msdel);
  cmd->cmdparser->SendMessageTerm(buf);
  cmd->cmdparser->ProcessIO();
  cmd->cmdparser->breaked = false ;
  msend = millis() + 1000UL * msdel;
  while ((millis() < msend) && !cmd->cmdparser->breaked)
    cmd->cmdparser->ProcessIO();
  
// open flash access  
  DueFlashStorage * dueFlashStorage = new DueFlashStorage(FZ_PROG);

// dump like xxd
// read back with cat FILE | xxd -r > OUTPUTFILE

  buf[0] = 0;
  bufc[0] = 0;
  allff = false;
  allffcount = 0 ;
  if (!cmd->cmdparser->breaked)
  for (uint32_t addr=0;addr < 0x80000; addr++) { // max 512k flash
    if ((addr % 16) == 0) {
      strcat(buf, "  ");
      strcat(buf, bufc);
// print buffer      
      cmd->cmdparser->SendMessageTerm(buf);
      cmd->cmdparser->ProcessIO();
      if (cmd->cmdparser->breaked) break ;

// check if exit condition achieved
      if (allff) {
        if (++allffcount >= 8)  break;   // exit loop after 8 ff lines
      } else {
        allffcount = 0 ;
      }

// start a new line with an offset address      
      sprintf(buf, "%06lx:", addr);
      bufc[0] = 0;
      c = 0 ;      
      allff = true;
    }
    if ((addr % 2) == 0) strcat(buf, " ");

// get next byte and store it in hex
    b = dueFlashStorage->read(addr);
    sprintf(temp, "%02x", b);
    strcat(buf, temp);

// conversion to char
    if (b != 0xff) allff = false ;
    if ((b < 0x20) || (b > 0x7f)) b = '.' ;
    bufc[c++] = b ;
    bufc[c] = 0 ;    
  }
  delete(dueFlashStorage);
  
// end up
  if (cmd->cmdparser->breaked) {
    p->respond("%c%s aborted", PICOCOM_EOF_CHAR, argv[0]);
  } else {
    p->respond("%cEnd of transmission", PICOCOM_EOF_CHAR);
  }
}
#endif

////////////////////////////////////////////////////////////////////

#ifdef USE_MYRTC_CLOCK

// Select the Slowclock source
//RTC_clock rtc_clock(RC);
RTC_clock rtc_clock(XTAL);

int8_t rtcl(uint8_t argc, char **argv, c_command_parser *p) { // rtc clock
  int ymdhms[6];
//  rtc_clock.get_date(&dow, &ymdhms[0], &ymdhms[1], &ymdhms[2]);
//  rtc_clock.get_time(&ymdhms[3], &ymdhms[4], &ymdhms[5]);
  ymdhms[0] = rtc_clock.get_years();
  ymdhms[1] = rtc_clock.get_months();
  ymdhms[2] = rtc_clock.get_days();
  ymdhms[3] = rtc_clock.get_hours();
  ymdhms[4] = rtc_clock.get_minutes();
  ymdhms[5] = rtc_clock.get_seconds();
  if (argc > 1) {
    switch (argv[1][0]) {
      case 'D' :
      case 'd' :
        sscanf(argv[2], "%d", &ymdhms[2]);
        if (argc > 2) sscanf(argv[3], "%d", &ymdhms[1]);
        if (argc > 3) sscanf(argv[4], "%d", &ymdhms[0]);
        rtc_clock.set_date(ymdhms[2], ymdhms[1], (uint16_t)ymdhms[0]);
        break;
      case 'T':
      case 't':  
        sscanf(argv[2], "%d", &ymdhms[5]);
        if (argc > 2) sscanf(argv[3], "%d", &ymdhms[4]);
        if (argc > 3) sscanf(argv[4], "%d", &ymdhms[3]);
        rtc_clock.set_time(ymdhms[3], ymdhms[4], ymdhms[5]);
        break;
    }
  } else if (argc > 0) {
    switch (argv[1][0]) {
      case 'D' :
      case 'd' :
        p->respond("%d.%02d.%02d", ymdhms[0], ymdhms[1], ymdhms[2]);
        break;
      case 'T':
      case 't':  
        p->respond("%d:%02d:%02d", ymdhms[3], ymdhms[4], ymdhms[5]);
        break;
    }
  } else {
    p->respond("%d.%02d.%02d %d:%02d:%02d", ymdhms[0], ymdhms[1], ymdhms[2],
               ymdhms[3], ymdhms[4], ymdhms[5]);
  }
}
#endif
