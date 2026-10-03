/************************************
* Name: LCD_HEADER		
* Author: Andrew Jones	
* Date: 01/13/26
*  For EGEE355, LSSU - Used with permission	
* Function: contain all funtion prototypes for 
*     LCD_code and connect to LCD module
*   CodeWarrior 5.9
************************************/
#ifndef _LCD_HEADER_H
#define _LCD_HEADER_H

#pragma CODE_SEG DEFAULT

/* list all prototype of accessable LCD functions */

void LCD_Init(void);
void LCD_Char(unsigned char);
void LCD_Cmd(unsigned char);

#endif

