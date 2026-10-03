/************************************
* Name: MY_VECTORS		
* Author: Andrew Jones	
* Date: 01/13/26	
*   For EGEE355, LSSU - Used with permission
* Function: contain all funtion prototypes for 
*     user defined interrupt service routines 
*   CodeWarrior 5.9
************************************/
#ifndef _MY_VECTORS_H
#define _MY_VECTORS_H

#pragma CODE_SEG __NEAR_SEG NON_BANKED /* Interrupt section for this module. Placement will be in NON_BANKED area. */

/* make changes in "isr_vectors.c" in the interrupt vector table */

/* list all ISR function prototypes */
//interrupt void ISR_RTI(void);	// ISR (receive) to insert into input buffer 
interrupt void ISR_ECT0(void);

#pragma CODE_SEG DEFAULT

#endif
