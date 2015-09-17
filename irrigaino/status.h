#ifndef STATUS_H_
#define STATUS_H_

#include "time.h"

// Emumerated that define the soil status: HUMID, OK or DRY
typedef enum
{
  DRY=0,
  OK=1,
  HUMID=2,
}soil_t;

// Emumerated that define the active screen: SCREEN_1 or SCREEN_2
typedef enum
{
  SCREEN_1=0,
  SCREEN_2=1,
}active_screen_t;

// Emumerated that define the irrigation status: irrigation underway or irrigation in standby
typedef enum
{
  STANDBY=0,
  UNDERWAY=1,
}irrigation_t;

// struct datatype holding all main information of irrigaino
typedef struct
{
  timedata_t timedata;          // the current time taken by RTC module
  time_hm_t irrigationStart;    // the starting irrigation time in hh and mm
  time_hm_t irrigationEnd;      // the ending irrigation time in hh and mm
  soil_t soil;                  // the soil status
  active_screen_t active_screen;// the active screen on LCD
  irrigation_t irrigation;      // the irrigation status
}status_t;

#endif  /* STATUS_H_*/
