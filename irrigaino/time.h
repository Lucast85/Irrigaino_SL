#ifndef TIME_H_
#define TIME_H_

// struct datatype holding time data (hh and mm)
typedef struct
{
  uint8_t hours;    //hh
  uint8_t minutes;  //mm
}time_hm_t;


// struct datatype holding all time data from RTC module
typedef struct
{
  time_hm_t time_hm;
  uint8_t seconds;
  uint8_t weekDay;
  uint8_t monthDay;
  uint8_t month;
  uint8_t year;
}timedata_t;

#endif  /* TIME_H_ */
