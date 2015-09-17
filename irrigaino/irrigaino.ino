#include <Wire.h>   //I2C driver for RTC
#include <UTFT.h>   //TFT driver for SSD1289
#include <UTouch.h> //Touchscreen driver for XPT2046

#include "time.h"
#include "status.h"

// define desired LCD colours
#define FRAME_COLOUR VGA_LIME
#define BACKGROUND_COLOUR VGA_BLACK
#define TEXT_COLOUR VGA_WHITE
#define BUTTON_COLOUR VGA_GREEN

#define DS1307_ADDRESS 0x68

/**************************************************************************************************
***                                 Global Variables                                            ***
**************************************************************************************************/
int x, y;
char stCurrent[20]="";
int stCurrentLen=0;
char stLast[20]="";
status_t irrigaino_sts;

// Declare which fonts we will be using
extern uint8_t BigFont[];
extern uint8_t SmallFont[];
extern uint8_t SevenSegNumFont[];

// Build two objects: LCD and Touch instances
UTFT myGLCD(ITDB32S,38,39,40,41);
UTouch myTouch( 6, 5, 4, 3, 2);

/**************************************************************************************************
***                                 Custom functions                                            ***
**************************************************************************************************/


//------------------------------- 3.2" TFT display -----------------------------//
void drawFrame_1stscreen()
{
  //draw the lines of screen 1
  myGLCD.setColor(FRAME_COLOUR); 
  myGLCD.drawLine(319,100,0,100);   //[FIDOCAD] LI 316 100 0 100 0
  myGLCD.drawLine(195,0,195,100);   //[FIDOCAD] LI 195 0 195 100 0
  myGLCD.drawLine(319,175,0,175);   //[FIDOCAD] LI 319 175 0 175 0
  myGLCD.drawLine(160,175,160,239); //[FIDOCAD] LI 160 175 160 239 0
  myGLCD.drawRect(0,0,319,239);     //[FIDOCAD] RV 0 0 319 239 0
  //draw the fixed text string of screen 1
  myGLCD.setFont(BigFont);
  myGLCD.setColor(TEXT_COLOUR); 
  myGLCD.print("ORA ATTUALE",5,1); 
  myGLCD.print("STATO",215,1); 
  myGLCD.setFont(SmallFont);
  myGLCD.print("INFORMAZIONI",30,200); 
  myGLCD.print("PROGRAMMAZIONE",185,200);  
  myGLCD.print("o",95,40);                //[FIDOCAD] FJC B 0.5 TY 90 40 12 8 0 0 0 * o
  myGLCD.print("o",95,65);                //[FIDOCAD] FJC B 0.5 TY 90 65 12 8 0 0 0 * o
}

void drawFrame_2ndscreen()
{
  //draw the lines of screen 2
  drawFrame_1stscreen();
  myGLCD.setColor(FRAME_COLOUR); 
  myGLCD.drawLine(160,115,160,175);   //[FIDOCAD] LI 160 115 160 175 0
  //draw text of screen 2
  myGLCD.setColor(TEXT_COLOUR);
  myGLCD.print("IMPOSTAZIONI IRRIGAZIONE AUTOMATICA",15,101); 
  myGLCD.print("inizio",55,160); 
  myGLCD.print("fine",225,160); 

  //draw "AVVIA IRRIGAZIONE" button
  myGLCD.setColor(BUTTON_COLOUR);
  myGLCD.fillRoundRect(205,65,310,95);             //[FIDOCAD] FJC B 0.5 RP 205 65 310 95 7
  myGLCD.setColor(TEXT_COLOUR);
  myGLCD.drawRoundRect (205,65,310,95);
  myGLCD.setFont(SmallFont);
  myGLCD.print("AVVIA",235,65);                   //[FIDOCAD] FJC B 0.5 TY 235 65 12 8 0 0 12 * AVVIA
  myGLCD.print("IRRIGAZIONE",210,80);             //[FIDOCAD] FJC B 0.5 TY 210 80 12 8 0 0 12 * IRRIGAZIONE
}
  
void drawButtons()
{
  
}

// Draw a red frame while a button is touched
void waitForIt(int x1, int y1, int x2, int y2)
{
  myGLCD.setColor(255, 0, 0);
  myGLCD.drawRoundRect (x1, y1, x2, y2);
  while (myTouch.dataAvailable())
    myTouch.read();
  myGLCD.setColor(255, 255, 255);
  myGLCD.drawRoundRect (x1, y1, x2, y2);
}

// Re-draw the time on LCD
void updateDisplayedTime(timedata_t* timedata)
{
  static uint8_t minutes;
  static uint8_t hours;
  if((minutes!=timedata->time_hm.minutes)||(hours!=timedata->time_hm.hours))
  {
    minutes=timedata->time_hm.minutes;
    hours=timedata->time_hm.hours;
    
    char hour_str[3], min_str[3];
    
    //_____________convert integer to 2-digit string_________
    if (hours<10)
    {
      hour_str[0]='0';
      hour_str[1]='0'+hours;
    }
    else
    {
      hour_str[0]='0'+hours/10;
      hour_str[1]='0'+hours%10;
    }
    hour_str[2]='\0';
  
    if (minutes<10)
    {
      min_str[0]='0';
      min_str[1]='0'+minutes;
    }
    else
    {
      min_str[0]='0'+minutes/10;
      min_str[1]='0'+minutes%10;
    }
    min_str[2]='\0';
  
      ///////////////////////////////////////////TODO HERE///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    // clear prevoius time displayed drawing two filled black rectangles
    myGLCD.setColor(BACKGROUND_COLOUR);
    myGLCD.fillRoundRect(10,28,85,90);
    myGLCD.fillRoundRect(108,28,185,90); 
    
    // draw actual time
    myGLCD.setColor(TEXT_COLOUR);
    myGLCD.setFont(SevenSegNumFont);
    myGLCD.print(hour_str,20,30);        //[FIDOCAD] FJC B 0.5 TY 20 30 50 32 0 0 0 Arial 23
    myGLCD.print(min_str,105,30);        //[FIDOCAD] FJC B 0.5 TY 105 30 50 32 0 0 0 Arial 59
  }
}

//------------------------------- RTC & Time manipulation -----------------------------//

// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)  
{
  return ( (val/16*10) + (val%16) );
}

// Update the actual date from RTC
void updateDate(timedata_t* timedata)
{
  // Reset the register pointer
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_ADDRESS, 7);

    timedata->seconds = bcdToDec(Wire.read());
    timedata->time_hm.minutes = bcdToDec(Wire.read() );
    timedata->time_hm.hours = bcdToDec(Wire.read() & 0b111111); //24 hour time
//    timedata->weekDay = bcdToDec(Wire.read()); //0-6 -> sunday - Saturday
//    timedata->monthDay = bcdToDec(Wire.read());
//    timedata->month = bcdToDec(Wire.read());
//    timedata->year = bcdToDec(Wire.read());
}


/**************************************************************************************************
***                                 Required functions                                          ***
**************************************************************************************************/


void setup()
{
// Initial setup

  // Only for debug purposes
  Serial.begin(9600);

  // Initialize display
  myGLCD.InitLCD();
  myGLCD.clrScr();
  myGLCD.setFont(BigFont);
  myGLCD.setBackColor(VGA_TRANSPARENT);
  
  // Initialize touchscreen
  myTouch.InitTouch();
  myTouch.setPrecision(PREC_HI);

  // Initialize I2C & for RTC module
  Wire.begin();
  
  // Draw main screen
  drawFrame_1stscreen();
  // Get RTC data
  updateDate(&irrigaino_sts.timedata);

  //ONLY FOR DEBUG !!!!!!!!!!!!!!!!!!!!!!!!!!!!
  Serial.print(irrigaino_sts.timedata.time_hm.hours);
  Serial.print(":");
  Serial.print(irrigaino_sts.timedata.time_hm.minutes);
  Serial.print("\n");
}

void loop()
{
  while (true)
  {
    // Get RTC data
    updateDate(&irrigaino_sts.timedata);
    // Update displayed time
    updateDisplayedTime(&irrigaino_sts.timedata);

    if (myTouch.dataAvailable())
    {
      myTouch.read();
      x=myTouch.getX();
      y=myTouch.getY();
      
      if ((y>=10) && (y<=60))  // Upper row
      {
        if ((x>=10) && (x<=60))  // Button: 1
        {
          waitForIt(10, 10, 60, 60);
          //updateStr('1');
        }
        if ((x>=70) && (x<=120))  // Button: 2
        {
          waitForIt(70, 10, 120, 60);
          //updateStr('2');
        }
        if ((x>=130) && (x<=180))  // Button: 3
        {
          waitForIt(130, 10, 180, 60);
          //updateStr('3');
        }
        if ((x>=190) && (x<=240))  // Button: 4
        {
          waitForIt(190, 10, 240, 60);
          //updateStr('4');
        }
        if ((x>=250) && (x<=300))  // Button: 5
        {
          waitForIt(250, 10, 300, 60);
          //updateStr('5');
        }
      }

      if ((y>=70) && (y<=120))  // Center row
      {
        if ((x>=10) && (x<=60))  // Button: 6
        {
          waitForIt(10, 70, 60, 120);
          //updateStr('6');
        }
        if ((x>=70) && (x<=120))  // Button: 7
        {
          waitForIt(70, 70, 120, 120);
          //updateStr('7');
        }
        if ((x>=130) && (x<=180))  // Button: 8
        {
          waitForIt(130, 70, 180, 120);
          //updateStr('8');
        }
        if ((x>=190) && (x<=240))  // Button: 9
        {
          waitForIt(190, 70, 240, 120);
          //updateStr('9');
        }
        if ((x>=250) && (x<=300))  // Button: 0
        {
          waitForIt(250, 70, 300, 120);
          //updateStr('0');
        }
      }

      if ((y>=130) && (y<=180))  // Upper row
      {
        if ((x>=10) && (x<=150))  // Button: Clear
        {
          waitForIt(10, 130, 150, 180);
          stCurrent[0]='\0';
          stCurrentLen=0;
          myGLCD.setColor(0, 0, 0);
          myGLCD.fillRect(0, 224, 319, 239);
        }
        if ((x>=160) && (x<=300))  // Button: Enter
        {
          waitForIt(160, 130, 300, 180);
          if (stCurrentLen>0)
          {
            for (x=0; x<stCurrentLen+1; x++)
            {
              stLast[x]=stCurrent[x];
            }
            stCurrent[0]='\0';
            stCurrentLen=0;
            myGLCD.setColor(0, 0, 0);
            myGLCD.fillRect(0, 208, 319, 239);
            myGLCD.setColor(0, 255, 0);
            myGLCD.print(stLast, LEFT, 208);
          }
          else
          {
            myGLCD.setColor(255, 0, 0);
            myGLCD.print("BUFFER EMPTY", CENTER, 192);
            delay(500);
            myGLCD.print("            ", CENTER, 192);
            delay(500);
            myGLCD.print("BUFFER EMPTY", CENTER, 192);
            delay(500);
            myGLCD.print("            ", CENTER, 192);
            myGLCD.setColor(0, 255, 0);
          }
        }
      }
    }
  }
}

