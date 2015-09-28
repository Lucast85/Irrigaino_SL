/************************************************************************************************
***                           INCLUDE HEADER FILES                                            ***
************************************************************************************************/

#include <Wire.h>   //I2C driver for RTC
#include <UTFT.h>   //TFT driver for SSD1289
#include <UTouch.h> //Touchscreen driver for XPT2046

#include "time.h"
#include "status.h"

/*************************************************************************************************
***                                 DEFINE                                                     ***
*************************************************************************************************/

// define desired LCD colours
#define FRAME_COLOUR        VGA_LIME
#define BACKGROUND_COLOUR   VGA_BLACK
#define TEXT_COLOUR         VGA_WHITE
#define EN_BUTTON_COLOUR    VGA_GREEN
#define DIS_BUTTON_COLOUR   43,85,43
#define BUTTON_PRESSED_COLOUR VGA_BLUE

// RTC I2C address
#define DS1307_ADDRESS    0x68

// Soil Moisture
#define SOIL_MOISTURE_PIN                     A0   //analog input pin of the sensor

/*
*   SOIL MOISTURE THRESHOLDS:
*   if (read value >= SOILMOISTURE_DISCONNECTED_THRESHOLD) the sensor is disconnected
*   if (read value < SOILMOISTURE_DISCONNECTED_THRESHOLD && read value >= SOILMOISTURE_UPPER_THRESHOLD) soil is dry
*   if (read value < SOILMOISTURE_UPPER_THRESHOLD && read value >= SOILMOISTURE_LOWER_THRESHOLD) soil is humid
*   If (read value < SOILMOISTURE_LOWER_THRESHOLD) the sensor is in water
*/
#define SOILMOISTURE_LOWER_THRESHOLD          370  //the lower moisture threshold
#define SOILMOISTURE_UPPER_THRESHOLD          600  //the upper moisture threshold
#define SOILMOISTURE_DISCONNECTED_THRESHOLD   1000 //the lower soil moisture sensor threshold: is reed value is > of this value, the sensor maybe is disconnected

// RELAY pin
#define RELAY_PIN                         14    //the pin connected with the relay that control the water pump 

/**************************************************************************************************
***                                 Global Variables                                            ***
**************************************************************************************************/
int x, y;
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

//------------------------------- Other Functions------------------------------------//
   
// convert integer to 2-digit string
void timeInt2timeStr(char* hours_str,char* mins_str,uint8_t hours,uint8_t minutes)
{
  if (hours<10)
  {
    hours_str[0]='0';
    hours_str[1]='0'+hours;
  }
  else
  {
    hours_str[0]='0'+hours/10;
    hours_str[1]='0'+hours%10;
  }
  hours_str[2]='\0';
  
  if (minutes<10)
  {
    mins_str[0]='0';
    mins_str[1]='0'+minutes;
  }
  else
  {
    mins_str[0]='0'+minutes/10;
    mins_str[1]='0'+minutes%10;
  }
  mins_str[2]='\0';
  return;
}

//------------------------------- 3.2" TFT display functions -----------------------------//

void blinkColon(timedata_t* timedata)
{
    static uint8_t second;
    static bool graygize;
    
    if(second!=timedata->seconds)   // if seconds has been changed
    {
      if(graygize)
      {
        // Draw actual time colon 
        myGLCD.setFont(SmallFont);
        myGLCD.setColor(TEXT_COLOUR);
        myGLCD.print("o",95,40);                //[FIDOCAD] FJC B 0.5 TY 90 40 12 8 0 0 0 * o
        myGLCD.print("o",95,65);                //[FIDOCAD] FJC B 0.5 TY 90 65 12 8 0 0 0 * o
      }
      else
      {
        // Clear actual time colon 
        myGLCD.setFont(SmallFont);
        myGLCD.setColor(BACKGROUND_COLOUR);
        myGLCD.print("o",95,40);                //[FIDOCAD] FJC B 0.5 TY 90 40 12 8 0 0 0 * o
        myGLCD.print("o",95,65);                //[FIDOCAD] FJC B 0.5 TY 90 65 12 8 0 0 0 * o
      }
      graygize^=1;              // switch graygize
      second=timedata->seconds; // update second value
    }
}

void updateDisplayedIrrStartTime(status_t* l_irrigaino_sts)
{
  //clear previous hours with black rectangle
  myGLCD.setColor(BACKGROUND_COLOUR);
  myGLCD.fillRect(36,125,124,155);  //[FIDOCAD] FJC B 0.5 RV 40 125 120 155 0
  
  // update irrigation starting time
  char hours_str[3], mins_str[3];
  timeInt2timeStr(hours_str,mins_str,l_irrigaino_sts->irrigationStart.hours,l_irrigaino_sts->irrigationStart.minutes);
  myGLCD.setFont(BigFont);
  myGLCD.setColor(TEXT_COLOUR); 
  myGLCD.print(hours_str,40,132);  //[FIDOCAD] FJC B 0.5 TY 40 125 16 16 0 0 0 * 00
  myGLCD.print(":",73,132);        //[FIDOCAD] FJC B 0.5 TY 73 125 16 16 0 0 0 * :
  myGLCD.print(mins_str,90,132);   //[FIDOCAD] FJC B 0.5 TY 90 125 16 16 0 0 0 * 01
}

void updateDisplayedIrrEndTime(status_t* l_irrigaino_sts)
{
  //clear previous hours with black rectangle
  myGLCD.setColor(BACKGROUND_COLOUR);
  myGLCD.fillRect(196,125,284,155);   //[FIDOCAD] FJC B 0.5 RV 200 125 280 155 0

  // update irrigation ending time
  char hours_str[3], mins_str[3];
  timeInt2timeStr(hours_str,mins_str,l_irrigaino_sts->irrigationEnd.hours,l_irrigaino_sts->irrigationEnd.minutes);
  myGLCD.setFont(BigFont);
  myGLCD.setColor(TEXT_COLOUR); 
  myGLCD.print(hours_str,200,132);  //[FIDOCAD] FJC B 0.5 TY 200 125 16 16 0 0 0 * 12
  myGLCD.print(":",233,132);        //[FIDOCAD] FJC B 0.5 TY 233 125 16 16 0 0 0 * :
  myGLCD.print(mins_str,250,132);   //[FIDOCAD] FJC B 0.5 TY 250 125 16 16 0 0 0 * 59
}

void updateDisplayedStatusAndButton(irrigation_t* irrigation)
{
  //draw "AVVIA/STOP IRRIGAZIONE" button & update text
  
  myGLCD.setColor(EN_BUTTON_COLOUR);
  myGLCD.fillRoundRect(205,65,310,95);      // [FIDOCAD] FJC B 0.5 RP 205 65 310 95 7
  myGLCD.setColor(BACKGROUND_COLOUR);
   myGLCD.fillRect(200,30,315,60);          // [FIDOCAD] FJC B 0.5 RV 200 30 315 60 0
  myGLCD.setColor(TEXT_COLOUR);
  myGLCD.drawRoundRect (205,65,310,95);
  myGLCD.setFont(SmallFont);
  
  if (*irrigation==STANDBY)
  {
    myGLCD.print("AVVIA",235,66);       // [FIDOCAD] FJC B 0.5 TY 235 66 12 8 0 0 12 * AVVIA
    myGLCD.print("STANDBY",230,35);     // [FIDOCAD] FJC B 0.5 TY 230 35 12 8 0 0 12 * standby  
    digitalWrite(RELAY_PIN,LOW);        // stop irrigation
  }
  
  else
  {
    myGLCD.print("STOP",240,66);         // [FIDOCAD] FJC B 0.5 TY 240 66 12 8 0 0 12 * STOP
    myGLCD.print("IRRIGAZIONE",210,30);  // [FIDOCAD] FJC B 0.5 TY 210 30 12 8 0 0 0 * irrigazione
    myGLCD.print("IN CORSO...",210,45);  // [FIDOCAD] FJC B 0.5 TY 210 45 12 8 0 0 0 * in corso...
    digitalWrite(RELAY_PIN,HIGH);        // activate irrigation
  }
  myGLCD.print("IRRIGAZIONE",210,80);    // [FIDOCAD] FJC B 0.5 TY 210 80 12 8 0 0 12 * IRRIGAZIONE

                   
}

void updateDisplayedSoilMoisture(soilmoisture_t* soilMoisture)
{
  //clear previous value
  myGLCD.setColor(BACKGROUND_COLOUR);
  myGLCD.fillRect (205,165,315,110);        //[FIDOCAD] FJC B 0.5 RV 205 165 315 110 2
  // draw actual value
  myGLCD.setFont(BigFont);
  myGLCD.setColor(TEXT_COLOUR);
  if (*soilMoisture==DRY)                   // terreno secco 
  {
    myGLCD.print("SECCO",220,125);          //[FIDOCAD] FJC B 0.5 TY 220 125 16 16 0 0 12 * SECCO
  }
  else if (*soilMoisture==DISCONNECTED)     // sensore disconnesso
  {
    myGLCD.setFont(SmallFont);
    myGLCD.print("SENSORE",230,125);        //[FIDOCAD] FJC B 0.5 TY 230 125 12 8 0 0 12 * SENSORE
    myGLCD.print("DISCONNESSO?",210,140);   //[FIDOCAD] FJC B 0.5 TY 210 140 12 8 0 0 12 * DISCONNESSO?
   
  }
  else if (*soilMoisture==OK)               // terreno OK
  {
    myGLCD.print("OK",245,125);             //[FIDOCAD] FJC B 0.5 TY 245 125 16 16 0 0 12 * OK
  }
  else                                      // sensore in acqua?
  { 
    myGLCD.setFont(SmallFont);
    myGLCD.print("SENSORE",230,125);        //[FIDOCAD] FJC B 0.5 TY 230 125 12 8 0 0 12 * SENSORE 
    myGLCD.print("IN ACQUA?",225,140);      //[FIDOCAD] FJC B 0.5 TY 225 140 12 8 0 0 12 * IN ACQUA?

  }
}

// draw a 30*20 button with up arrow with upper left corner at point passed as parameter (x,y)
void drawUpButton(uint16_t x,uint16_t y)
{
  myGLCD.setColor(EN_BUTTON_COLOUR);
  myGLCD.fillRoundRect(x,y,x+30,y+20);
  myGLCD.setColor(TEXT_COLOUR);
  myGLCD.drawRoundRect(x,y,x+30,y+20);
  myGLCD.drawLine(x+5,y+15,x+15,y+5);
  myGLCD.drawLine(x+15,y+5,x+25,y+15);
}
// draw a 30*20 button with down arrow with upper left corner at point passed as parameter (x,y)
void drawDownButton(uint16_t x,uint16_t y)
{
  myGLCD.setColor(EN_BUTTON_COLOUR);
  myGLCD.fillRoundRect(x,y,x+30,y+20);
  myGLCD.setColor(TEXT_COLOUR);
  myGLCD.drawRoundRect(x,y,x+30,y+20);  //RV 5 150
  myGLCD.drawLine(x+5,y+5,x+15,y+15);   //LI 10 155 20 165
  myGLCD.drawLine(x+15,y+15,x+25,y+5);  //LI 20 165 30 155
}

void drawCommonFrameObjects()
{
  //draw the lines of the frame
  myGLCD.setColor(FRAME_COLOUR); 
  myGLCD.drawLine(319,100,0,100);   //[FIDOCAD] LI 319 100 0 100 0
  myGLCD.drawLine(195,0,195,100);   //[FIDOCAD] LI 195 0 195 100 0
  myGLCD.drawLine(319,175,0,175);   //[FIDOCAD] LI 319 175 0 175 0
  myGLCD.drawLine(160,175,160,239); //[FIDOCAD] LI 160 175 160 239 0
  myGLCD.drawRect(0,0,319,239);     //[FIDOCAD] RV 0 0 319 239 0
    //draw the fixed text string of screens 1 & 2
  myGLCD.setFont(BigFont);
  myGLCD.setColor(TEXT_COLOUR); 
  myGLCD.print("ORA ATTUALE",5,1); 
  myGLCD.print("STATO",215,1); 
//  // draw the colon between hours and minutes
//  myGLCD.setFont(SmallFont);
//  myGLCD.print("o",95,40);                //[FIDOCAD] FJC B 0.5 TY 90 40 12 8 0 0 0 * o
//  myGLCD.print("o",95,65);                //[FIDOCAD] FJC B 0.5 TY 90 65 12 8 0 0 0 * o
}

void draw1stScreen()
{
  drawCommonFrameObjects();
  //clear 2nd and 3rd rows
  myGLCD.setColor(BACKGROUND_COLOUR);
  myGLCD.fillRoundRect(1,101,318,174);
  myGLCD.fillRoundRect(10,185,145,230);   //[FIDOCAD] FJC B 0.5 RP 10 185 145 230 8
  myGLCD.fillRoundRect(175,185,310,230);  //[FIDOCAD] FJC B 0.5 RP 175 185 310 230 7

  //draw "informazioni" & "programmazione" buttons
  myGLCD.setColor(DIS_BUTTON_COLOUR);     // informazioni button is disabled
  myGLCD.fillRoundRect(10,185,145,230);   //[FIDOCAD] FJC B 0.5 RP 10 185 145 230 8
  myGLCD.setColor(EN_BUTTON_COLOUR);      // programmazione button is enabled
  myGLCD.fillRoundRect(175,185,310,230);  //[FIDOCAD] FJC B 0.5 RP 175 185 310 230 7
  myGLCD.setColor(TEXT_COLOUR);
  myGLCD.drawRoundRect (10,185,145,230);  //[FIDOCAD] FJC B 0.5 RV 10 185 145 230 12
  myGLCD.drawRoundRect (175,185,310,230); //[FIDOCAD] FJC B 0.5 RV 175 185 310 230 12
  myGLCD.setFont(SmallFont);
  myGLCD.print("INFORMAZIONI",30,200);    //[FIDOCAD] FJC B 0.5 TY 30 200 12 8 0 0 12 * INFORMAZIONI
  myGLCD.print("PROGRAMMAZIONE",185,200); //[FIDOCAD] FJC B 0.5 TY 185 200 12 8 0 0 12 * PROGRAMMAZIONE

  //draw "umidità del terreno"
  myGLCD.setFont(BigFont);
  myGLCD.print("UMIDITA'",5,110);//[FIDOCAD] FJC B 0.5 TY 5 110 16 16 0 0 12 * UMIDITA'
  myGLCD.print("DEL TERRENO",5,135);//[FIDOCAD] FJC B 0.5 TY 5 135 16 16 0 0 12 * DEL TERRENO

  updateDisplayedSoilMoisture(&irrigaino_sts.soilMoisture);
}

void draw2ndScreen()
{
  drawCommonFrameObjects();
  //clear 2nd and 3rd rows
  myGLCD.setColor(BACKGROUND_COLOUR);
  myGLCD.fillRoundRect(1,101,318,174);
  myGLCD.fillRoundRect(10,185,145,230);   //[FIDOCAD] FJC B 0.5 RP 10 185 145 230 8
  myGLCD.fillRoundRect(175,185,310,230);  //[FIDOCAD] FJC B 0.5 RP 175 185 310 230 7

  //draw "informazioni" & "programmazione" buttons
  myGLCD.setColor(EN_BUTTON_COLOUR);      // informazioni button is enabled
  myGLCD.fillRoundRect(10,185,145,230);   //[FIDOCAD] FJC B 0.5 RP 10 185 145 230 8
  myGLCD.setColor(DIS_BUTTON_COLOUR);     // programmazione button is disabled
  myGLCD.fillRoundRect(175,185,310,230);  //[FIDOCAD] FJC B 0.5 RP 175 185 310 230 7
  myGLCD.setColor(TEXT_COLOUR);
  myGLCD.drawRoundRect (10,185,145,230);  //[FIDOCAD] FJC B 0.5 RV 10 185 145 230 12
  myGLCD.drawRoundRect (175,185,310,230); //[FIDOCAD] FJC B 0.5 RV 175 185 310 230 12
  myGLCD.setFont(SmallFont);
  myGLCD.print("INFORMAZIONI",30,200);    //[FIDOCAD] FJC B 0.5 TY 30 200 12 8 0 0 12 * INFORMAZIONI
  myGLCD.print("PROGRAMMAZIONE",185,200); //[FIDOCAD] FJC B 0.5 TY 185 200 12 8 0 0 12 * PROGRAMMAZIONE
  
  //draw centered vertical line of the frame
  myGLCD.setColor(FRAME_COLOUR); 
  myGLCD.drawLine(160,115,160,175);       //[FIDOCAD] LI 160 115 160 175 0
  //draw text of screen 2
  myGLCD.setColor(TEXT_COLOUR);
  myGLCD.print("IMPOSTAZIONI IRRIGAZIONE AUTOMATICA",15,101); 
  myGLCD.print("inizio",55,160); 
  myGLCD.print("fine",225,160); 

  //draw the buttons (x8) to select start irrigation time & end irrigation time
  drawUpButton(5,120);
  drawUpButton(125,120);
  drawUpButton(165,120);
  drawUpButton(285,120);
  drawDownButton(5,150);
  drawDownButton(125,150);
  drawDownButton(165,150);
  drawDownButton(285,150);

  updateDisplayedIrrStartTime(&irrigaino_sts);
  updateDisplayedIrrEndTime(&irrigaino_sts);
}

// Re-draw the time on LCD
void updateDisplayedTime(timedata_t* timedata)
{
  blinkColon(timedata);    // Blink colon between hours & minutes every second
  static uint8_t minutes;
  static uint8_t hours;
  if((minutes!=timedata->time_hm.minutes)||(hours!=timedata->time_hm.hours))
  {
    char hours_str[3], mins_str[3];
    minutes=timedata->time_hm.minutes;
    hours=timedata->time_hm.hours;
    timeInt2timeStr(hours_str,mins_str,hours,minutes);
   
    // clear prevoius time displayed drawing two filled black rectangles
    myGLCD.setColor(BACKGROUND_COLOUR);
    myGLCD.fillRoundRect(10,28,85,90);
    myGLCD.fillRoundRect(108,28,185,90); 
    
    // draw actual time
    myGLCD.setColor(TEXT_COLOUR);
    myGLCD.setFont(SevenSegNumFont);
    myGLCD.print(hours_str,20,30);        //[FIDOCAD] FJC B 0.5 TY 20 30 50 32 0 0 0 Arial 23
    myGLCD.print(mins_str,105,30);        //[FIDOCAD] FJC B 0.5 TY 105 30 50 32 0 0 0 Arial 59
  }
}



//------------------------------------- Touchscreen functions-----------------------------------//

// Draw a coloured frame while a button is touched
void waitForIt(int x1, int y1, int x2, int y2)
{
  myGLCD.setColor(BUTTON_PRESSED_COLOUR);
  myGLCD.drawRoundRect (x1, y1, x2, y2);
  while (myTouch.dataAvailable())
    myTouch.read();
  myGLCD.setColor(TEXT_COLOUR);
  myGLCD.drawRoundRect (x1, y1, x2, y2);
}

void checkPressedBtn_screen1(status_t* l_irrigaino_sts)
{
  if (myTouch.dataAvailable())
      {
        myTouch.read();
        x=myTouch.getX();
        y=myTouch.getY();  
        
        if ((y>=185) && (y<=230))   // Lower row
        {
//          if ((x>=10) && (x<=145))  // "INFORMAZIONI" button  //[FIDOCAD] FJC B 0.5 RV 10 185 145 230 12
//          {
//            waitForIt(10,185,145,230);
//            l_irrigaino_sts->activeScreen=SCREEN_1;
//          }
          if ((x>=175) && (x<=310)) // "PROGRAMMAZIONE" button  //[FIDOCAD] FJC B 0.5 RV 175 185 310 230 12
          {
            waitForIt(175,185,310,230);             
            l_irrigaino_sts->activeScreen=SCREEN_2;   // change active Screen value to SCREEN_2
          }
        }
        if ((y>=65 ) && (y<=95 ) && (x>=205) && (x<=310))   // "AVVIA/STOP IRRIGAZIONE" button  // [FIDOCAD] FJC B 0.5 RP 205 65 310 95 7
        {
          waitForIt(205,65,310,95); 
          l_irrigaino_sts->irrigation=(irrigation_t)(l_irrigaino_sts->irrigation^1);   // switch between irrigation ON & OFF
        }
      }
}

void checkPressedBtn_screen2(status_t* l_irrigaino_sts)
{
  if (myTouch.dataAvailable())
      {
        myTouch.read();
        x=myTouch.getX();
        y=myTouch.getY();  
        
        if ((y>=185) && (y<=230))   // Lower row
        {
          if ((x>=10) && (x<=145))  // "INFORMAZIONI" button  //[FIDOCAD] FJC B 0.5 RV 10 185 145 230 12
          {
            waitForIt(10,185,145,230);
            l_irrigaino_sts->activeScreen=SCREEN_1;
          }
//          if ((x>=175) && (x<=310)) // "PROGRAMMAZIONE" button  //[FIDOCAD] FJC B 0.5 RV 175 185 310 230 12
//          {
//            waitForIt(175,185,310,230);             
//            irrigaino_sts->activeScreen=SCREEN_2;   // change active Screen value to SCREEN_2
//          }
        }
        if ((y>=65 ) && (y<=95 ) && (x>=205) && (x<=310))   // "AVVIA/STOP IRRIGAZIONE" button  // [FIDOCAD] FJC B 0.5 RP 205 65 310 95 7
        {
          waitForIt(205,65,310,95); 
          l_irrigaino_sts->irrigation=(irrigation_t)(l_irrigaino_sts->irrigation^1);   // switch between irrigation ON & OFF
        }
        
        if((y>=120) && (y<=140))   // up & down button: upper row  // the buttons are 30x20 pixels
        {
          if((x>=5) && (x<=35))         // +1h Start irrigation time
          {
            waitForIt(5,120,35,140);
            l_irrigaino_sts->irrigationStart.hours = (l_irrigaino_sts->irrigationStart.hours + 1) % 24;
          }
          if((x>=125) && (x<=155))      // +1m Start irrigation time
          {
            waitForIt(125,120,155,140);
            l_irrigaino_sts->irrigationStart.minutes = (l_irrigaino_sts->irrigationStart.minutes + 1) % 60;
          }
          if((x>=165) && (x<=195))      // +1h End irrigation time
          {
            waitForIt(165,120,195,140);
            l_irrigaino_sts->irrigationEnd.hours = (l_irrigaino_sts->irrigationEnd.hours + 1) % 24;
          }
          if((x>=285) && (x<=315))      // +1m End irrigation time
          {
            waitForIt(285,120,315,140);
            l_irrigaino_sts->irrigationEnd.minutes = (l_irrigaino_sts->irrigationEnd.minutes + 1) % 60;
          }                                    
        }
        if((y>=150) && (y<=170))   // up & down button: lower row  // the buttons are 30x20 pixels
        {
          if((x>=5) && (x<=35))         // -1h Start irrigation time
          {
            waitForIt(5,150,35,170);
            if(l_irrigaino_sts->irrigationStart.hours == 0) l_irrigaino_sts->irrigationStart.hours = 24 ;
            l_irrigaino_sts->irrigationStart.hours = (l_irrigaino_sts->irrigationStart.hours - 1) % 24;
          }
          if((x>=125) && (x<=155))      // -1m Start irrigation time
          {
            waitForIt(125,150,155,170);
            if(l_irrigaino_sts->irrigationStart.minutes == 0) l_irrigaino_sts->irrigationStart.minutes = 60 ;
            l_irrigaino_sts->irrigationStart.minutes = (l_irrigaino_sts->irrigationStart.minutes - 1) % 60;
          }
          if((x>=165) && (x<=195))      // -1h End irrigation time
          {
            waitForIt(165,150,195,170);
            if(l_irrigaino_sts->irrigationEnd.hours == 0) l_irrigaino_sts->irrigationEnd.hours = 24 ;
            l_irrigaino_sts->irrigationEnd.hours = (l_irrigaino_sts->irrigationEnd.hours - 1) % 24;
          }
          if((x>=285) && (x<=315))      // -1m End irrigation time
          {
            waitForIt(285,150,315,170);
            if(l_irrigaino_sts->irrigationEnd.minutes == 0) l_irrigaino_sts->irrigationEnd.minutes = 60 ;
            l_irrigaino_sts->irrigationEnd.minutes = (l_irrigaino_sts->irrigationEnd.minutes - 1) % 60;
          }             
        }
        if(x>159) updateDisplayedIrrEndTime(l_irrigaino_sts);
        else updateDisplayedIrrStartTime(l_irrigaino_sts);
      }
}

    
//------------------------------- RTC & Time manipulation functions-----------------------------//

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

//------------------------------- Soil Moisture Sensor------------------------------------//
// Update the value read by soil moisture sensor
void updateSoilMoisture(soilmoisture_t* soilMoisture)
{
  // read the value from the sensor:
  uint16_t soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);
  if(soilMoistureValue >= SOILMOISTURE_DISCONNECTED_THRESHOLD)                                                          // the sensor is disconnected
  {
    *soilMoisture=DISCONNECTED;
  }
  else if(soilMoistureValue < SOILMOISTURE_DISCONNECTED_THRESHOLD && soilMoistureValue >= SOILMOISTURE_UPPER_THRESHOLD) // soil is dry
  {
    *soilMoisture=DRY;
  }
  else if(soilMoistureValue < SOILMOISTURE_UPPER_THRESHOLD && soilMoistureValue >= SOILMOISTURE_LOWER_THRESHOLD)        // soil is OK 
  {
    *soilMoisture=OK;
  }
  else *soilMoisture=WATER;  // then (soilMoistureValue < SOILMOISTURE_LOWER_THRESHOLD)                                 // the sensor is in water  
}

/**************************************************************************************************
***                                       Setup                                                 ***
**************************************************************************************************/

void setup()
{
// Initial setup

  // Only for debug purposes
  Serial.begin(9600);
  
  pinMode(SOIL_MOISTURE_PIN, INPUT); //set up Soil Moisture pin to be input
  
  // Initialize display
  myGLCD.InitLCD();
  myGLCD.clrScr();
  myGLCD.setFont(BigFont);
  myGLCD.setBackColor(VGA_TRANSPARENT);
  
  // Initialize touchscreen
  myTouch.InitTouch();
  myTouch.setPrecision(PREC_MEDIUM);

  // Initialize I2C for RTC module
  Wire.begin();

  // Initialize digital output (relay or pump)
  pinMode(RELAY_PIN,OUTPUT);
  
  // Draw main screen
  draw1stScreen();

  // Initialize irrigaino_sts global variable
  updateDate(&irrigaino_sts.timedata);              // Get RTC data and update relative variable
  updateSoilMoisture(&irrigaino_sts.soilMoisture);  // Get SoilMoisture data and update relative variable
  irrigaino_sts.activeScreen=SCREEN_1;              // set active screen to 1st screen
  irrigaino_sts.irrigation=STANDBY;                 // put irrigation status in standby 
  irrigaino_sts.irrigationStart.hours=0;            // set irrigation start @ 00:00
  irrigaino_sts.irrigationStart.minutes=0;
  irrigaino_sts.irrigationEnd.hours=0;              // set irrigation end @ 00:00
  irrigaino_sts.irrigationEnd.minutes=0;
  
  // Draw irrigation status and manual button 
  updateDisplayedStatusAndButton(&irrigaino_sts.irrigation);
}



/*******************************************************************************************************************************
***                                                       MAIN LOOP                                                          ***
*******************************************************************************************************************************/
void loop()
{ 
  static status_t old_irrigaino_sts;  //contains old value of the system
  while (true)
  {  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////TODO HERE\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\   

  //-------------------------------------------------------------------HERE START CODE FOR DEBUG ONLY--------------------------------------------------------------------
//      updateSoilMoisture(&irrigaino_sts.soilMoisture);
//      updateDisplayedSoilMoisture(&irrigaino_sts.soilMoisture);
//      
//  Serial.print("please insert the irrigation status: 1 = avvia irrigazione, 0 = stop irigazione\n");
//  // Read serial input:
//    while (Serial.available() > 0) 
//    {
//      // read the incoming byte:
//      int incomingByte=Serial.read();
//      if (incomingByte==0x30) irrigaino_sts.irrigation=STANDBY;
//      if (incomingByte==0x31) irrigaino_sts.irrigation=UNDERWAY;
//    }
//  Serial.print("irrigaino_sts.irrigation: ");
//  Serial.println(irrigaino_sts.irrigation, DEC);

//  Serial.print("please insert desired screen: 1 or 2 \n");
//  // Read serial input:
//    while (Serial.available() > 0) 
//    {
//      // read the incoming byte:
//      int incomingByte=Serial.read();
//      if (incomingByte==0x31)
//      {
//        draw1stScreen();
//        irrigaino_sts.irrigation=STANDBY;
//      }
//      if (incomingByte==0x32) 
//      {
//        draw2ndScreen();
//        irrigaino_sts.irrigation=UNDERWAY;
//      }
//    }
//  
//  delay(1000);
  //-------------------------------------------------------------------HERE FINISH CODE FOR DEBUG ONLY---------------------------------------------------------------------

    // Get RTC data 
    updateDate(&irrigaino_sts.timedata);
    // Update displayed time
    updateDisplayedTime(&irrigaino_sts.timedata);
    
    if(old_irrigaino_sts.irrigation!=irrigaino_sts.irrigation) updateDisplayedStatusAndButton(&irrigaino_sts.irrigation); // if irrigation status is changed update it
    if(old_irrigaino_sts.activeScreen!=irrigaino_sts.activeScreen)  // if selected screen is changed update displayed screen 
    {
      if(irrigaino_sts.activeScreen==SCREEN_1) draw1stScreen();   // draw 1st screen
      else draw2ndScreen();                                       // draw 2nd screen
    }
    old_irrigaino_sts=irrigaino_sts; // update the status of the system
    checkPressedBtn_screen1(&irrigaino_sts);
    checkPressedBtn_screen2(&irrigaino_sts);
        

  }
}

