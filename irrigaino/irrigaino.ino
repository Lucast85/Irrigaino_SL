/************************************************************************************************
***                           INCLUDE HEADER FILES                                            ***
************************************************************************************************/

#include <Wire.h>           // I2C driver for RTC
#include <UTFT.h>           // TFT driver for SSD1289
#include <UTouch.h>         // Touchscreen driver for XPT2046
#include <SPI.h>            // SPI driver for SD card & ethernet module 
#include <SD.h>             // SD library
#include <UIPEthernet.h>    // Ethernet library 

#include "time.h"
#include "status.h"

/*************************************************************************************************
***                                 DEFINE                                                     ***
*************************************************************************************************/
//#define SERIAL_MONITOR      // comment out to shut down serial monitor

// desired LCD colours
#define FRAME_COLOUR                VGA_LIME
#define BACKGROUND_COLOUR           VGA_BLACK
#define TEXT_COLOUR                 VGA_WHITE
#define EN_BUTTON_COLOUR            VGA_GREEN
#define MANUAL_ON_EN_BUTTON_COLOUR  VGA_TEAL
#define DIS_BUTTON_COLOUR           43,85,43
#define BUTTON_PRESSED_COLOUR       VGA_BLUE
#define ALERT_COLOUR                VGA_RED

// Ethernet 
#define REQ_BUF_SZ        60                                // size of buffer used to capture HTTP requests
#define TXT_BUF_SZ        3
#define MAC_ADDRESS       {0xDE,0xAD,0xBE,0xEF,0xFE,0xED}   // the MAC address of ethernet interface
#define IP_ADDRESS        192,168,1,33                      // IP address, may need to change depending on network
#define SERVER_PORT       80                                // the port of the server  
// to change the CS pin of ethernet module (ENC28J60), define it in Enc28J60Network.h (overwrite SS with desired pin number, 15 in this application)  

// SD card
#define SDCARD_CS_PIN     53

// RTC I2C address
#define DS1307_ADDRESS    0x68

// Soil Moisture
#define SOIL_MOISTURE_PIN     A0  // analog input pin of the sensor
#define LPF                   1   // If 1 Low Pass Filter is active. If 0, LPF is inactive.
#define FILTER_SHIFT          10   // the parameter K of LPF

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
#define RELAY_PIN                         14       //the pin connected with the relay that control the water pump

/**************************************************************************************************
***                                 Global Variables                                            ***
**************************************************************************************************/
int x, y;
status_t irrigaino_sts;

//Ethernet 
  // MAC, IP & PORT
const uint8_t MAC[] = MAC_ADDRESS;
IPAddress ip(IP_ADDRESS);             // IP address, may need to change depending on network
//EthernetServer server(SERVER_PORT);   // create a server at port SERVER_PORT=80
EthernetServer server = EthernetServer(SERVER_PORT);  // create a server at port SERVER_PORT=80

  // HTTP buffer
char HTTP_req[REQ_BUF_SZ] = {0};  // buffered HTTP request stored as null terminated string
char req_index = 0;               // index into HTTP_req buffer
char buf_1[TXT_BUF_SZ] = {0};     // buffer to save line 1 text 
char buf_2[TXT_BUF_SZ] = {0};     // buffer to save line 2 text 

// files on SD 
File webFile;                     // the web page file on the SD card

// TFT-LCD
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

void updateDisplayedStatusAndButton(irrigation_t* irrigation, manualIrrBtn_t* manualIrrBtn)
{
  //draw "AVVIA/STOP IRRIGAZIONE" button & update text
  if(*manualIrrBtn) myGLCD.setColor(MANUAL_ON_EN_BUTTON_COLOUR);
  else myGLCD.setColor(EN_BUTTON_COLOUR);
  myGLCD.fillRoundRect(205,65,310,95);      // [FIDOCAD] FJC B 0.5 RP 205 65 310 95 7
  myGLCD.setColor(BACKGROUND_COLOUR);
  myGLCD.fillRect(200,30,315,60);           // [FIDOCAD] FJC B 0.5 RV 200 30 315 60 0
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

void drawAlert(char* alert_msg_1st_row, char* alert_msg_2nd_row, char* alert_msg_3rd_row, char* alert_msg_4th_row)
{
  myGLCD.clrScr();  //clear sreeen
  myGLCD.setColor(ALERT_COLOUR);
  myGLCD.setFont(BigFont);
  myGLCD.print(alert_msg_1st_row,20,20);   //[FIDOCAD] FJC B 0.5 TY 60 75 16 16 0 0 0 * Initializing
  myGLCD.print(alert_msg_2nd_row,20,70);   //[FIDOCAD] FJC B 0.5 TY 60 115 16 16 0 0 0 * SD card ...
  myGLCD.print(alert_msg_3rd_row,20,120);  
  myGLCD.print(alert_msg_4th_row,20,170);   
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
          l_irrigaino_sts->irrigation=(irrigation_t)(l_irrigaino_sts->irrigation^1);              // switch between irrigation ON & OFF
          l_irrigaino_sts->manualIrrBtn = (manualIrrBtn_t)(l_irrigaino_sts->manualIrrBtn^true);   // switch between button pressed ON & OFF
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
          l_irrigaino_sts->manualIrrBtn = (manualIrrBtn_t)(l_irrigaino_sts->manualIrrBtn^1);  // switch between button pressed ON & OFF
        }
        
        if((y>=120) && (y<=140))   // up & down button: upper row  // the buttons are 30x20 pixels
        {
          if((x>=5) && (x<=35))         // +1h Start irrigation time
          {
            waitForIt(5,120,35,140);
            l_irrigaino_sts->irrigationStart.hours = (l_irrigaino_sts->irrigationStart.hours + 1) % 24;
            updateDisplayedIrrStartTime(l_irrigaino_sts);
          }
          if((x>=125) && (x<=155))      // +1m Start irrigation time
          {
            waitForIt(125,120,155,140);
            l_irrigaino_sts->irrigationStart.minutes = (l_irrigaino_sts->irrigationStart.minutes + 1) % 60;
            updateDisplayedIrrStartTime(l_irrigaino_sts);
          }
          if((x>=165) && (x<=195))      // +1h End irrigation time
          {
            waitForIt(165,120,195,140);
            l_irrigaino_sts->irrigationEnd.hours = (l_irrigaino_sts->irrigationEnd.hours + 1) % 24;
            updateDisplayedIrrEndTime(l_irrigaino_sts);
          }
          if((x>=285) && (x<=315))      // +1m End irrigation time
          {
            waitForIt(285,120,315,140);
            l_irrigaino_sts->irrigationEnd.minutes = (l_irrigaino_sts->irrigationEnd.minutes + 1) % 60;
            updateDisplayedIrrEndTime(l_irrigaino_sts);
          }                                    
        }
        if((y>=150) && (y<=170))   // up & down button: lower row  // the buttons are 30x20 pixels
        {
          if((x>=5) && (x<=35))         // -1h Start irrigation time
          {
            waitForIt(5,150,35,170);
            if(l_irrigaino_sts->irrigationStart.hours == 0) l_irrigaino_sts->irrigationStart.hours = 24 ;
            l_irrigaino_sts->irrigationStart.hours = (l_irrigaino_sts->irrigationStart.hours - 1) % 24;
            updateDisplayedIrrStartTime(l_irrigaino_sts);
          }
          if((x>=125) && (x<=155))      // -1m Start irrigation time
          {
            waitForIt(125,150,155,170);
            if(l_irrigaino_sts->irrigationStart.minutes == 0) l_irrigaino_sts->irrigationStart.minutes = 60 ;
            l_irrigaino_sts->irrigationStart.minutes = (l_irrigaino_sts->irrigationStart.minutes - 1) % 60;
            updateDisplayedIrrStartTime(l_irrigaino_sts);
          }
          if((x>=165) && (x<=195))      // -1h End irrigation time
          {
            waitForIt(165,150,195,170);
            if(l_irrigaino_sts->irrigationEnd.hours == 0) l_irrigaino_sts->irrigationEnd.hours = 24 ;
            l_irrigaino_sts->irrigationEnd.hours = (l_irrigaino_sts->irrigationEnd.hours - 1) % 24;
            updateDisplayedIrrEndTime(l_irrigaino_sts);
          }
          if((x>=285) && (x<=315))      // -1m End irrigation time
          {
            waitForIt(285,150,315,170);
            if(l_irrigaino_sts->irrigationEnd.minutes == 0) l_irrigaino_sts->irrigationEnd.minutes = 60 ;
            l_irrigaino_sts->irrigationEnd.minutes = (l_irrigaino_sts->irrigationEnd.minutes - 1) % 60;
            updateDisplayedIrrEndTime(l_irrigaino_sts);
          }             
        }
      }
}

    
//------------------------------- RTC & Time manipulation functions-----------------------------//

// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)  
{
  return ( (val/16*10) + (val%16) );
}

// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val)
{
  return ( (val/10*16) + (val%10) );
}

// Set the time into RCT
void setDateTime(uint8_t hours, uint8_t minutes)
{
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0); //stop Oscillator. Workaround for issue #527

  Wire.write(decToBcd(30));       // seconds
  Wire.write(decToBcd(minutes));  
  Wire.write(decToBcd(hours));
  Wire.write(decToBcd(1));        //weekday 
  Wire.write(decToBcd(1));        //monthday
  Wire.write(decToBcd(1));        //month
  Wire.write(decToBcd(15));       //year

  Wire.write(0); //start. Workaround for issue #527

  Wire.endTransmission();
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
  #if LPF
  // Simple LPF to soil moisture sensor: more informations in http://www.edn.com/design/systems-design/4320010/A-simple-software-lowpass-filter-suits-embedded-system-applications
  static int32_t filter_reg;  // delay element
  int16_t filter_input;       // filter_input
  int16_t soilMoistureValue;  // filter output
  
  filter_input = analogRead(SOIL_MOISTURE_PIN);  //read input
  filter_reg = filter_reg - (filter_reg >> FILTER_SHIFT) + filter_input;  //update filter with current sample
  soilMoistureValue = filter_reg >> FILTER_SHIFT;  //scale output for unity gain
  #else
  uint16_t soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);  //read input
  #endif
  
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

//------------------------------- Ethernet ------------------------------------//
// checks if received HTTP request wants to change "irrigaino_sts.irrigation" status.
void Set(void)
{
    if (StrContains(HTTP_req, "Pompa=1")) {
        irrigaino_sts.irrigation=UNDERWAY;
        irrigaino_sts.manualIrrBtn=irrigaino_sts.manualIrrBtn^1;    // irrigation forced by web client
    }
    else if (StrContains(HTTP_req, "Pompa=0")) {
        irrigaino_sts.irrigation=STANDBY;
        irrigaino_sts.manualIrrBtn=irrigaino_sts.manualIrrBtn^1;    // irrigation stop forced by web client
    }
}

// send the XML file with following irrigaino data. 
// - irrigaino_sts.irrigation (ENUM, see status.h)
// - irrigaino_sts.irrigationStart.hours & irrigaino_sts.irrigationStart.minutes (uint8_t see status.h)
// - irrigaino_sts.irrigationEnd.hours & irrigaino_sts.irrigationEnd.minutes (uint8_t see status.h)
// - irrigaino_sts.soilMoisture (ENUM, see status.h)
// - irrigaino_sts.manualIrrBtn (boolean, see status.h)                 
void XML_response(EthernetClient cl)      // Irrigaino (i.e. the webserver) send the XML response with data values that web client will print on its webpage. This function is invoked constantly to update webpage
{
  Serial.println("on XML_response function");
    cl.print("<?xml version = \"1.0\" ?>");
    cl.print("<inputs>");
    cl.print("<terreno>");
    cl.print(irrigaino_sts.soilMoisture);   
    cl.print("</terreno>");
    cl.print("<irrigation>");
    cl.print(irrigaino_sts.irrigation);
    cl.print("</irrigation>");
    cl.print("<manualIrrBtn>");
    cl.print(irrigaino_sts.manualIrrBtn);
    cl.print("</manualIrrBtn>"); 
    cl.print("<starthours>");
    cl.print(irrigaino_sts.irrigationStart.hours);
    cl.print("</starthours>");
    cl.print("<startminutes>");
    cl.print(irrigaino_sts.irrigationStart.minutes);
    cl.print("</startminutes>");
    cl.print("<stophours>"); 
    cl.print(irrigaino_sts.irrigationEnd.hours);
    cl.print("</stophours>"); 
    cl.print("<stopminutes>"); 
    cl.print(irrigaino_sts.irrigationEnd.minutes);
    cl.print("</stopminutes>");
    cl.print("</inputs>");
}

// get the two strings for the LCD from the incoming HTTP GET request. This function is invoked constantly to update webpage
void GetText()
{
  String req = String(HTTP_req);
  int commaIndex = req.indexOf('@');
  int secondcommaIndex = req.indexOf('@',commaIndex+1);
  int thirdcommaIndex = req.indexOf('@',secondcommaIndex+1);
  int fourthcommaIndex = req.indexOf('@',thirdcommaIndex+1);
  int fifthcommaIndex = req.indexOf('@',fourthcommaIndex+1);
  int sixthcommaIndex = req.indexOf('@',fifthcommaIndex+1);

  String START_H = req.substring(commaIndex+1, secondcommaIndex);
  String START_M = req.substring(secondcommaIndex+1, thirdcommaIndex);
  String STOP_H = req.substring(thirdcommaIndex+1, fourthcommaIndex);
  String STOP_M = req.substring(fourthcommaIndex+1,fifthcommaIndex);

  irrigaino_sts.irrigationStart.hours = START_H.toInt();
  irrigaino_sts.irrigationStart.minutes = START_M.toInt();
  irrigaino_sts.irrigationEnd.hours = STOP_H.toInt();
  irrigaino_sts.irrigationEnd.minutes = STOP_M.toInt();
}

void GetTime()
{ 
   String req = String(HTTP_req);
  int commaIndex = req.indexOf('@');
  int secondcommaIndex = req.indexOf('@',commaIndex+1);
  int thirdcommaIndex = req.indexOf('@',secondcommaIndex+1);
  String H = req.substring(commaIndex+1, secondcommaIndex);
  String M = req.substring(secondcommaIndex+1, thirdcommaIndex);

  setDateTime(H.toInt(), M.toInt());    // set RTC time from webpage
}
// sets every element of str to 0 (clears array)
void StrClear(char *str, char length)
{
    for (int i = 0; i < length; i++) {
        str[i] = 0;
    }
}

// searches for the string sfind in the string str returns -1 if string found -0 if string not found
char StrContains(char *str, char *sfind)
{
    char found = 0;
    char index = 0;
    char len;

    len = strlen(str);
    
    if (strlen(sfind) > len) {
        return 0;
    }
    while (index < len) {
        if (str[index] == sfind[found]) {
            found++;
            if (strlen(sfind) == found) {
                return 1;
            }
        }
        else {
            found = 0;
        }
        index++;
    }
    return 0;
}
    
/**************************************************************************************************
***                                       Setup                                                 ***
**************************************************************************************************/

void setup()
{
// Initial setup

  // Only for debug purposes
  #ifdef SERIAL_MONITOR
    Serial.begin(9600);
  #endif
      
  // Initialize display
  myGLCD.InitLCD();
  myGLCD.clrScr();
  myGLCD.setFont(BigFont);
  myGLCD.setBackColor(VGA_TRANSPARENT);

  // Initialize I2C for RTC module
  Wire.begin();

  // Initialize pins mode
  pinMode(RELAY_PIN,OUTPUT);          
  pinMode(SDCARD_CS_PIN,OUTPUT);    
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  
  drawAlert("", "  Waiting for","  IP ADDRESS","");
  
  // initialize ethernet
  if (Ethernet.begin(MAC) == 0) // initialize Ethernet device using no DHCP server (use a standard IP)
  {
    drawAlert("Failed", "to configure","Ethernet","using DHCP");
    delay(3000);
    Ethernet.begin(MAC,ip);     // initialize Ethernet device using standard IP (See define IP. Default: 192.168.1.33)
    char buf[20];
    sprintf(buf,"%d:%d:%d:%d", Ethernet.localIP()[0],Ethernet.localIP()[1],Ethernet.localIP()[2],Ethernet.localIP()[3]);  // get assigned IP
    drawAlert("Default ", "IP address","is",buf);
  }
  else
  {
    char buf[20];
    sprintf(buf,"%d:%d:%d:%d", Ethernet.localIP()[0],Ethernet.localIP()[1],Ethernet.localIP()[2],Ethernet.localIP()[3]);  // get assigned IP
    drawAlert("DHCP server","has assigned:",buf,""); // print local IP address
  }
  delay(3000);
  
  server.begin();           // start to listen for clients
  delay(400);
    
  // initialize SD card
  drawAlert("Initializing ", "SD card...","","");
  delay(600);
  if (!SD.begin(SDCARD_CS_PIN))                                         // init failed 
  {
    drawAlert("ERROR - ", "SD card", "initialization", "failed!" );   
    delay(1000);
    return;    
  }
  drawAlert("SUCCESS - ", "SD card", "initialized.", "" );
  delay(800);
  // check for index.htm file
  if (!SD.exists("index.htm"))                                          // can't find index file
  {
    drawAlert("ERROR - ", "SD card", "Can't find","index.htm file!" );    
    delay(1000);
    return;  
  }
  drawAlert("SUCCESS - ", "SD card", "Found ","index.htm file." );
  delay(800);
  myGLCD.clrScr();

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
  updateDisplayedStatusAndButton(&irrigaino_sts.irrigation, &irrigaino_sts.manualIrrBtn);

  // Initialize touchscreen
  myTouch.InitTouch();
  myTouch.setPrecision(PREC_MEDIUM);
}
  
/*******************************************************************************************************************************
***                                                       MAIN LOOP                                                          ***
*******************************************************************************************************************************/
void loop()
{ 
  bool irrigationStartTimeExpired=false;
  bool irrigationEndTimeExpired=false;
  bool autoIrrigation=false;
  static status_t old_irrigaino_sts;  //contains old value of the system
  while (true)
  { 
    // Get RTC data 
    updateDate(&irrigaino_sts.timedata);
    // Get Soil moisture status
    updateSoilMoisture(&irrigaino_sts.soilMoisture);
    // Update displayed time
    updateDisplayedTime(&irrigaino_sts.timedata);
    // Update irrigation status
    if(old_irrigaino_sts.irrigation!=irrigaino_sts.irrigation) updateDisplayedStatusAndButton(&irrigaino_sts.irrigation , &irrigaino_sts.manualIrrBtn); // if irrigation status is changed update it
    
    // if selected screen is changed update displayed screen 
    if(old_irrigaino_sts.activeScreen!=irrigaino_sts.activeScreen)      
    {
      if(irrigaino_sts.activeScreen) draw2ndScreen();    // draw 2nd screen
      else draw1stScreen();                              // draw 1st screen                                    
    }

    // check active screen and update all values into the screen (if they've been changed)
    if(irrigaino_sts.activeScreen)  // screen 2 active!
    {
      if( (old_irrigaino_sts.irrigationStart.hours!=irrigaino_sts.irrigationStart.hours) || (old_irrigaino_sts.irrigationStart.minutes!=irrigaino_sts.irrigationStart.minutes) )
        updateDisplayedIrrStartTime(&irrigaino_sts);      //start irrigation time has changed, update it
      if( (old_irrigaino_sts.irrigationEnd.hours!=irrigaino_sts.irrigationEnd.hours) || (old_irrigaino_sts.irrigationEnd.minutes!=irrigaino_sts.irrigationEnd.minutes) )
        updateDisplayedIrrEndTime(&irrigaino_sts);        //end irrigation time has changed, update it
    }
    else                            // screen 1 active!
    {
      if(old_irrigaino_sts.soilMoisture!=irrigaino_sts.soilMoisture) updateDisplayedSoilMoisture(&irrigaino_sts.soilMoisture);  //if soilmoisture has changed, update it
    }

    
    if(old_irrigaino_sts.timedata.time_hm.minutes != irrigaino_sts.timedata.time_hm.minutes)  // a new minute has expired
    {
      if( (irrigaino_sts.timedata.time_hm.hours == irrigaino_sts.irrigationStart.hours) && (irrigaino_sts.timedata.time_hm.minutes == irrigaino_sts.irrigationStart.minutes) )  // irrigation start time is now!
      irrigationStartTimeExpired=true;     
      if( (irrigaino_sts.timedata.time_hm.hours == irrigaino_sts.irrigationEnd.hours) && (irrigaino_sts.timedata.time_hm.minutes == irrigaino_sts.irrigationEnd.minutes) )      // irrigation end time is now!
      irrigationEndTimeExpired=true;
    }

    old_irrigaino_sts=irrigaino_sts; // update the status of the system

    if(irrigationStartTimeExpired)        // executed only when actual time match irrigation start time
    {      
      irrigationStartTimeExpired=false;   // reset flag
      autoIrrigation = true;              // set another flag
    }
    if(irrigationEndTimeExpired)          // executed only when actual time match irrigation end time
    {
      irrigationEndTimeExpired=false;     // reset flag
      autoIrrigation = false;             // reset flag
      if(irrigaino_sts.manualIrrBtn == false) irrigaino_sts.irrigation=STANDBY;   // turn off irrigation
      if ((irrigaino_sts.manualIrrBtn == true) && (irrigaino_sts.irrigation==STANDBY)) irrigaino_sts.manualIrrBtn = false;    //if irrigation was forced, when IrrEndTime expired, then colour the button in green
      updateDisplayedStatusAndButton(&irrigaino_sts.irrigation, &irrigaino_sts.manualIrrBtn);
    }

    if((autoIrrigation == true) && (irrigaino_sts.manualIrrBtn == false))       // update automatic irrigation status according to soil moisture level
    {
      if (irrigaino_sts.soilMoisture == DRY) irrigaino_sts.irrigation=UNDERWAY;   // turn on irrigation if soil is dry
      else irrigaino_sts.irrigation=STANDBY;                                      // turn off irrigation if soil isn't dry (OK, sensor disconnected or in water)
    }

    if(irrigaino_sts.activeScreen == SCREEN_1) checkPressedBtn_screen1(&irrigaino_sts);   // check if a button is pressed on screen 1 or on screen 2
    else checkPressedBtn_screen2(&irrigaino_sts);

    EthernetClient client = server.available();  // try to get client
    if (client) {  // got client?
      #ifdef SERIAL_MONITOR
        Serial.println("got client = yes");
      #endif
        boolean currentLineIsBlank = true;
        while (client.connected()) {
             
            if (client.available()) {   // client data available to read
                         
                char c = client.read(); // read 1 byte (character) from client limit the size of the stored received HTTP request 
                                        // buffer first part of HTTP request in HTTP_req array (string)
                                        // leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
                if (req_index < (REQ_BUF_SZ - 1)) {
                    HTTP_req[req_index] = c;          // save HTTP request character
                    req_index++;
                }
                // last line of client request is blank and ends with \n 
                //respond to client only after last line received
                if (c == '\n' && currentLineIsBlank) {
                    // send a standard http response header
                    client.println("HTTP/1.1 200 OK");
                    // remainder of header follows below, depending on if
                    // - web page or XML page is requested
                    // - Ajax request - send XML file
             
                    if (StrContains(HTTP_req, "ajax_inputs")) {   // if client (browser) request data from Irrigaino (ajax_inputs), then it's clear that the webpage is alreay loaded by client and the client is requesting data to refresh values
                        // send rest of HTTP header
                        client.println("Content-Type: text/xml");
                        client.println("Connection: keep-alive");
                        client.println();
                        Set();
                        // send XML file containing input states
                        XML_response(client);
                        #ifdef SERIAL_MONITOR
                          Serial.println("XMLResponse executed");
                        #endif
                        // print the received text to the LCD if found
                        
                    }
                    else if (StrContains(HTTP_req, "programmazione")) { 
                            // write the received text
                            GetText();
                    }
                    
                    else if (StrContains(HTTP_req, "RTCTime")) { 
                            GetTime();
                            }

                     else if (StrContains(HTTP_req, "SetTime")) { 
                            GetTime();
                            
                            }       
                    else if (StrContains(HTTP_req, "GET / ") || StrContains(HTTP_req, "GET /index.htm")) {  // web page request from client
                        // send rest of HTTP header
                        client.println("Content-Type: text/html");
                        client.println("Connection: keep-alive");
                        client.println();
                        // send web page
                        webFile = SD.open("index.htm");        // open web page file
                        if (webFile) {
                            while(webFile.available()) {
                                client.write(webFile.read()); // send web page to client
                            }
                            webFile.close();
                        }
                    }
                    // display received HTTP request on serial port
                    #ifdef SERIAL_MONITOR
                      Serial.print(HTTP_req);
                    #endif
                    // reset buffer index and all buffer elements to 0
                    req_index = 0;
                    StrClear(HTTP_req, REQ_BUF_SZ);
                    break;
                }
                // every line of text received from the client ends with \r\n
                if (c == '\n') {
                   
                    // last character on line of received text
                    // starting new line with next character read
                    currentLineIsBlank = true;
                } 
                else if (c != '\r') {
                    // a text character was received from client
                    
                     currentLineIsBlank = false;
                }
            } // end if (client.available())
        } // end while (client.connected())
        delay(1);      // give the web browser time to receive the data
        client.stop(); // close the connection
    } // end if (client)
    
  //Ethernet.maintain();  //Allows for the renewal of DHCP leases
  
  } // end while(true)
} // end loop()

