#ifndef TIME_H_
#define TIME_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define DS1307_ADDRESS 0x68

typedef struct
{
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  uint8_t weekDay;
  uint8_t monthDay;
  uint8_t month;
  uint8_t year;
}timedata_t;

#endif  /* TIME_H_ */
