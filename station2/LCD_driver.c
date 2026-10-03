/************************************
* EGEE 355 
*	
* Name: LCD_driver		
* Author: Andrew Jones	
* Date: 01/13/26	
*  For EGEE355, LSSU - Used with permission	
* Function: provide interfact code to LCD screen
*   Made for Dragon12-light (port K, 4 bit interface)
************************************/
//
//  This code uses 4-bit transfer via port K:
//  PK0 ------- RS ( register select, 0 = register transfer, 1 = data transfer).
//  PK1 ------- Enable ( write pulse )
//  PK2 ------- Data Bit 4 of LCD
//  PK3 ------- Data Bit 5 of LCD
//  PK4 ------- Data Bit 6 of LCD
//  PK5 ------- Data Bit 7 of LCD
//
// Timing of 4-bit data transfer is shown on page 44 of the Hantronix
// application notes included in "standard_LCD.pdf" handout
//
#include <hidef.h>      /* common defines and macros */
#include "derivative.h"      /* derivative-specific definitions */
#include "LCD_header.h"      /* function prototypes for LCD_driver.c */

#pragma CODE_SEG DEFAULT  /* banked code */

// local function prototypes
void _LCDMicroDelay(int);
void _LCDNibble(unsigned char, unsigned char);


/*************************
*  LCD_INIT
*    initialize the LCD (portK)
*************************/
void LCD_Init()
{
  DDRK = 0x3F;
  PORTK = 0x00;
  
  _LCDNibble(0x30,0);
  _LCDMicroDelay(5000);
  _LCDNibble(0x30,0);
  _LCDMicroDelay(5000);
  LCD_Cmd(0x32);
  LCD_Cmd(0x2C);
  LCD_Cmd(0x14);
  LCD_Cmd(0x0C);
  LCD_Cmd(0x01);
  _LCDMicroDelay(1640);   // clear screen - extra time
}  // LCD_Init()


/*************************
*  LCD_CMD
*    Send command to LCD module
*************************/
void LCD_Cmd(unsigned char data) 
{
  _LCDNibble(data,0);
  _LCDNibble((data<<4),0);
  _LCDMicroDelay(100);
} // LCD_Cmd()


/*************************
*  LCD_Char
*    send data (character) to LCD module
*************************/
void LCD_Char(unsigned char data) 
{
  _LCDNibble(data,1);
  _LCDNibble((data<<4),1);
  _LCDMicroDelay(50);
} // LCD_Char()


//  -- internal functions - do not call from outside of LCD_driver.c --
/*************************
*  _LCDNibble
*    send nibble to LCD
*    which: 0=cmd, 1=data
*************************/
void _LCDNibble(unsigned char nibble, unsigned char which)
{
  unsigned char send;
  
  unsigned char off[2]= {0x02,0x03};
  
  send = ((nibble>>2)&0x3C)|off[(which&0x03)];
  PORTK = send;
    __asm {
         PSHA	; delay to strobe enable
         PULA 
    }
  PORTK = (send&0xFD);
} // _LCDNibble()


/*********************************************************************
* Function: _LCDMicroDelay() 
* Purpose:  kill clock cycles (determined bus clock is 24 MHz) 
*           accepts int for # of microseconds to delay
**********************************************************************/
void _LCDMicroDelay(int ms) 
{    // (D) = passed value (ms)
  __asm {
         PSHD
         PSHX
loopout: LDX #6
loopin:  NOP
         DBNE X,loopin    ; 4 cycles in (X) loop
         DBNE D,loopout   ; (D) # of milliseconds
         PULX
         PULD
        }
  ms++;
} // _LCDMicroDelay()