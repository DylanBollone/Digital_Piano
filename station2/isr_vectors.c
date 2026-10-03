/************************************
* Name: ISR_VECTORS		
* Author: Andrew Jones	
* Date: 01/13/26	
*   For EGEE355, LSSU - Used with permission
* Function: map out inerrupt vector table for MC9S12DG256 
*   CodeWarrior 5.9
************************************/
#include <hidef.h>
#include <start12.h>
#include "my_vectors.h"


/* Interrupt section for this module. 
   Placement will be in NON_BANKED area. */
#pragma CODE_SEG __NEAR_SEG NON_BANKED 

/* define default interrupt */
__interrupt void UnimplementedISR(void) 
{
   /* Unimplemented ISRs trap.*/
   asm BGND;
}

/* interrupt vector table (starts at 0xFF80) */
typedef void (*near tIsrFunc)(void);
const tIsrFunc _vect[] @0xFF80 = { /* Interrupt table */
  /* ISR name                    No. Address  Description */
  UnimplementedISR,           /* 63  0xFF80   reserved */
  UnimplementedISR,           /* 62  0xFF82   reserved */
  UnimplementedISR,           /* 61  0xFF84   reserved */
  UnimplementedISR,           /* 60  0xFF86   reserved */
  UnimplementedISR,           /* 59  0xFF88   reserved */
  UnimplementedISR,           /* 58  0xFF8A   reserved */
  UnimplementedISR,           /* 57  0xFF8C   PWM EMG shutdown */
  UnimplementedISR,           /* 56  0xFF8E   PORT P */
  UnimplementedISR,           /* 55  0xFF90   CAN4 transmit */
  UnimplementedISR,           /* 54  0xFF92   CAN4 receive */
  UnimplementedISR,           /* 53  0xFF94   CAN4 errors */
  UnimplementedISR,           /* 52  0xFF96   CAN4 wake-up */
  UnimplementedISR,           /* 51  0xFF98   CAN3 transmit, unused */
  UnimplementedISR,           /* 50  0xFF9A   CAN3 receive, unused */
  UnimplementedISR,           /* 49  0xFF9C   CAN3 errors, unused */
  UnimplementedISR,           /* 48  0xFF9E   CAN3 wake-up, unused */
  UnimplementedISR,           /* 47  0xFFA0   CAN2 transmit, unused */
  UnimplementedISR,           /* 46  0xFFA2   CAN2 receive, unused */
  UnimplementedISR,           /* 45  0xFFA4   CAN2 errors, unused */
  UnimplementedISR,           /* 44  0xFFA6   CAN2 wake-up, unused */
  UnimplementedISR,           /* 43  0xFFA8   CAN1 transmit, unused */
  UnimplementedISR,           /* 42  0xFFAA   CAN1 receive, unused */
  UnimplementedISR,           /* 41  0xFFAC   CAN1 errors, unused */
  UnimplementedISR,           /* 40  0xFFAE   CAN1 wake-up, unused */
  UnimplementedISR,           /* 39  0xFFB0   CAN0 transmit */
  UnimplementedISR,           /* 38  0xFFB2   CAN0 receive */
  UnimplementedISR,           /* 37  0xFFB4   CAN0 errors */
  UnimplementedISR,           /* 36  0xFFB6   CAN0 wake-up */
  UnimplementedISR,           /* 35  0xFFB8   FLASH */
  UnimplementedISR,           /* 34  0xFFBA   EEPROM */
  UnimplementedISR,           /* 33  0xFFBC   SPI2 */
  UnimplementedISR,           /* 32  0xFFBE   SPI1 */
  UnimplementedISR,           /* 31  0xFFC0   IIC bus */
  UnimplementedISR,           /* 30  0xFFC2   BDLC, unused */
  UnimplementedISR,           /* 29  0xFFC4   CRG Self CLK */
  UnimplementedISR,           /* 28  0xFFC6   CRG PLL lock */
  UnimplementedISR,           /* 27  0xFFC8   PA B overflow */
  UnimplementedISR,           /* 26  0xFFCA   MOD counter overflow */
  UnimplementedISR,           /* 25  0xFFCC   PORTH */
  UnimplementedISR,           /* 24  0xFFCE   PORTJ */
  UnimplementedISR,           /* 23  0xFFD0   ATD1 */
  UnimplementedISR,           /* 22  0xFFD2   ATD0 */
  UnimplementedISR,           /* 21  0xFFD4   SCI1 */
  UnimplementedISR,           /* 20  0xFFD6   SCI0 */
  UnimplementedISR,           /* 19  0xFFD8   SPI0 */
  UnimplementedISR,           /* 18  0xFFDA   PA input edge */
  UnimplementedISR,           /* 17  0xFFDC   PA A overflow */
  UnimplementedISR,           /* 16  0xFFDE   timer overflow */
  UnimplementedISR,           /* 15  0xFFE0   TC7 */
  UnimplementedISR,           /* 14  0xFFE2   TC6 */
  UnimplementedISR,           /* 13  0xFFE4   TC5 */
  UnimplementedISR,           /* 12  0xFFE6   TC4 */
  UnimplementedISR,           /* 11  0xFFE8   TC3 */
  UnimplementedISR,           /* 10  0xFFEA   TC2 */
  UnimplementedISR,           /* 09  0xFFEC   TC1 */
  ISR_ECT0,           /* 08  0xFFEE   TC0 */
  UnimplementedISR,           /* 07  0xFFF0   RTI */
  UnimplementedISR,           /* 06  0xFFF2   IRQ */
  UnimplementedISR,           /* 05  0xFFF4   XIRQ */
  UnimplementedISR,           /* 04  0xFFF6   SWI */
  UnimplementedISR,           /* 03  0xFFF8   Instruction trap */
  UnimplementedISR,           /* 02  0xFFFA   COP failure reset */
  UnimplementedISR,           /* 01  0xFFFC   clock monitor reset */
//  _Startup                    /* 00  0xFFFE   Reset vector */
};