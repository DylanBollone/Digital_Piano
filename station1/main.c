/***********************************************************************
* Names: Dylan Bollone, Jess Besonen, Will, Jadon
* 
*
* EGEE 355 - Lab Final Project - Spring 2026
*   Code for digital piano interface
************************************/
/***********************************************************************
* Source Code for "Digital Piano" Station 1
*   Digital Piano input, sending inputs via CAN, metronome
*   SPI, CAN, ECT, PWM
***********************************************************************/
#include <hidef.h>      /* common defines and macros */
#include "derivative.h"      /* derivative-specific definitions */
#include "LCD_header.h" /* LCD driver code - provided by Dr. Jones */
#include "my_vectors.h"      /* ISR prototypes */
#define MAX_NUM_NOTES 4

typedef unsigned char uint8;
typedef unsigned int uint16;

#pragma CODE_SEG __NEAR_SEG NON_BANKED
/* list ISR function prototypes */
/* no interrupts yet - delete these lines of code if unused */

#pragma CODE_SEG DEFAULT

/* Global Variables */
// Arrays for displaying which key(s) are being pressed to the LCD screen
// Originally this was for debugging the piano, but we have yet to get rid of it
// So now it is a feature
static char g_notes[] =       "FFGGAABCCDDEFFGGAABCCDDEFFGGAABC";
static char g_accidentals[] = " # # #  # #  # # #  # #  # # #  ";
static char g_noteNums[] =    "11111111111122222222222233333333";

/*---------------A---------------
               G
  -----------F-------------------
           E                   A
  -------D-------------------G---
       C                   F
  ---B-------------------E-------
   A                   D
  -------------------C-----------
                   B
  ---------------A--------------*/

// Arrays for playing the metronome
// PWM period is either 0 or 3000
// (3000 is an arbitrary selection unrelated to any particular frequecny)
static const uint16 tone[] = {3000,0};
static const uint16 ticks[] = {1,15};

#pragma DATA_SEG DEFAULT
volatile uint16 g_tick_cnt;     // Metronome tick variable
volatile uint16 g_tick_index;   // Metronome index
volatile uint8 g_t_flg = 0x00;  // Metronome enable/disable' flag

volatile uint16 g_bpm;          // Systems current beats per minute

// Structor for storing key data
typedef struct
{
  char note;       // Note (A-G)
  char accidental; // Either ' ' or '#'
  char num;        // Which A-G is it? (1-3)
} key;

// array of 4 keys for storing up to 4 unique key presses
volatile key notes[MAX_NUM_NOTES];

uint8 CAN_KEY_VALS[4] ={0,0,0,0};

/* List of function prototypes */
void CLK_Init(void);
void SPI_Init(void);
void get_keys(char *keys);
void get_notes(char *keys);
void disp_notes(void);
void PWM_Init(void);
void ECT_Init(void);
void LCD_String(char *string);
void disp_tempo(uint16 tempo);
void DelayMS(int ms);
void CAN0_Init(void);
uint8 CAN0_Receive(void);
void CAN0_Transmit(uint8 *ptr);

/***************************************************************
* Function: main
* Purpose: main program loop - station 1
***************************************************************/
void main(void)
{
  char pianoKeys[4]; // array of 4 characters for storing state of each key (32 piano keys)
  
  uint8 ptr[14] = {'N',  0x00, 0x00, 0x00,     // IDR 
                    0x80, 0x00, 0x00, 0x00,     // high 4 bytes of data
                    0x00, 0x00, 0x00, 0x00,     // low 4 bytes of data
                    0x04, 0x00};                // DLR, TBPR
                    
  uint8 status;
  int loop;
                     
  /* Initialization routines */
  CLK_Init();
  SPI_Init();
  LCD_Init();
  PWM_Init();
  ECT_Init();
  CAN0_Init();
  
  // Initialize bpm to 100
  g_bpm = 100;
  
	EnableInterrupts;

  for(;;)
  {
    for (loop = 0;loop<4;loop++)  //clears each channel in case channel deactivates
    {
     CAN_KEY_VALS[loop]=0;
    }
    
    get_keys(pianoKeys);  // read in 32 bits of data via SPI
    get_notes(pianoKeys); // convert piano key data to notes
    disp_notes(); // Output notes to LCD screen
    disp_tempo(g_bpm); // output current tempo to LCD
    
    //write note array to can transmit
    for (loop = 0;loop<4;loop++)  //copy over note data to CAN transmit buffer
    {
     ptr[4+loop]= CAN_KEY_VALS[loop];
    }
    
    CAN0_Transmit(ptr); // Transmit note data to station 2
    
    //check receive
    status = CAN0_Receive();
    
    if(status)
      g_bpm = status;
    
    if(g_t_flg)
      PWME = 0x00; // disable pwm
    else
      PWME = 0x20; // enable pwm

    DelayMS(5);  // update every 5ms
  }
  
}

/*********************************************************************
* Function: CLK_Init()
*           Initialize bus clock to 24 MHz
**********************************************************************/
void CLK_Init(void)
{
/*
 The internal PLL clock lets us set the speed of the processor.
 The default bus speed will be 4 MHz (half the OscFreq).

 The math used to set the PLL frequency: (OscFreq = 8 MHz)
  SYNR = 5      (PLL multiplier - 1)
  REFDV = 1     (PLL divider - 1)
  PLLCLK = 2 * OscFreq * (SYNR + 1) / (REFDV + 1)

 The OscFreq is 4 MHz, so we have PLLCLK = 2*8*6/2 = 48 MHz
 The bus clock runs at half of the PLL speed: Bus Clock = PLLCLK / 2 = 24 MHz

 Currently the fastest speed this device can safely use is 24 MHz.
 If you set a slower speed, it will reduce power consumption.
*/
// Set the PLL speed (***change this only if you want to slow down CPU***)
	CLKSEL &= 0x7F;		// disengage PLL to system
	PLLCTL |= 0x40; 	// turn on PLL
	SYNR = 0x05; 		// set PLL multiplier
	REFDV = 0x01;		// set PLL divider
	asm("nop");
	asm("nop");
	while (!(CRGFLG & 0x08));	// wait for clock to sync
	CLKSEL |= 0x80;		// engage PLL to system

// Disable watchdog timer (COPCTL register)
	COPCTL = 0x40;		// COP off - RTI and COP stopped in BDM-mode
} //CLK_Init()

/*********************************************************************
* Function: DelayMS()
* Purpose:  kill clock cycles (determined bus clock is 24 MHz)
*           accepts int for # of ms to delay
**********************************************************************/
void DelayMS(int ms) // (D) = passed value (ms)
{
  __asm {
          PSHD          ; save passed value, ms, on stack
          PSHX          
 loopin:  DBEQ D,done   ; outer loop handles ms (number of ms)
          LDX #7998     ; time delay = (total cycles - 3X+3+2) / (clk rate - 24MHZ)
 loopo:   DBNE X,loopo  ; use eq above to find val of x for specific time delay based on clk rate
          BRA loopin
       
  done:   PULX
          PULD          ; restore values
        }
  ms++; // use ms to remove compiler warning
} // DelayMS()

/********************** SPI Functions *********************************/
/*********************************************************************
* Function: SPI_init
* Purpose:  Initializes 9S12 SPI0;shift data at 12MHz   (24 bus clk)
* HC165N Max CLK speed at 4.5V Supply and Room Temp = 25MHz
*     12MHz is fastest we can do with SPI0 on the 9S12
**********************************************************************/
void SPI_Init(void) 
{
  // configure DDRS, 9s12 master - MOSI pin unused
  DDRS = DDRS | 0xC0;
  // Select baud rate diviser of 2 (maximum 9S12 SPI speed)
  SPI0BR = 0x00;
  // master mode, ints off, cpol = 0, cpha = 0 %0101xx00
  //  ss disabled, data tfr msb first
  SPI0CR1 = 0x50;
  // normal mode, ss not used by spi
  SPI0CR2 = 0x00;
  // ensure Load is high
  PTS = PTS | 0x80;
}

/*********************************************************************
* Function: get_keys
* Purpose: get serial input from the digital piano (4x 8bits = 32 bits (one per piano key))
**********************************************************************/
void get_keys(char *keys)
{
  int i;
  
  // Pulse Load pin low
  PTS &= 0x7F; // set pin low
  for(i=0;i<20;i++); // Delay for a little while
  PTS |= 0x80; // Set load pin back high
  
  // Shift 8 bits of data at a time
  for(i=0;i<4;i++) // There are a total of 4 shift registers
  {
    while(!(SPI0SR & 0x20)); // wait until SPI ready
    SPI0DR = 0x00;  // Send nothing
    while(!(SPI0SR & 0x80)); // wait until done
    keys[3-i] = SPI0DR;  // read data & clear flag
  }
  
}

/**************** Note Functions ***********************/
/*********************************************************************
* Function: get_notes
* Purpose: Get which notes are pressed (first 4 found left to right)
* Notes: After reading the first 4 key presses (left to right) this
*         ignores the rest of the detected key presses. 
**********************************************************************/
void get_notes(char*keys)
{
  int i,j,notesLen,temp;
  unsigned char mask;
  
  notesLen = 0;
  
  // Set notes to all zeros
  for(i=0;i<MAX_NUM_NOTES;i++)
  {
    notes[i].note = 0x00;
    notes[i].accidental = 0x00;
    notes[i].num = 0x00;
  }  
  
  // Traverse through all 32 bits of keys
  for(i=0;i<4;i++)
  {
    for(j=0;j<8;j++)
    {
      // Read bit - if 1 move on, if 0 do stuff
      mask = (0x01)<<j;
      if(!( keys[i] & mask ))
      {
        // Add to notes array if not full
        notesLen++;
        if(notesLen<=MAX_NUM_NOTES)
        {
          temp = i*8 + j; //0-31 for Dylans indexes 
          CAN_KEY_VALS[notesLen-1] = ((temp+1) | 0x80); // 1-32 for CAN Transmission . . add a high bit on MSB to denote active channel 
          notes[notesLen-1].note = g_notes[temp];
          notes[notesLen-1].accidental = g_accidentals[temp];
          notes[notesLen-1].num = g_noteNums[temp];
        }
      }
    }
  }
}

/********************* LCD Screen Functions ******************************/
/*********************************************************************
* Function: disp_notes
* Purpose: display what notes get_notes found to LCD screen - for debug and test 
**********************************************************************/
void disp_notes(void)
{
  int i;
  
  // Set cursor to first char of first line
  LCD_Cmd(0x80);
  // Output a bunch of spaces (16 - # of chars per row)
  LCD_String("                ");
  // Set cursor to first char of first line
  LCD_Cmd(0x80);
    
  // output up to 4 notes
  for(i=0;i<4;i++)
  {
    if(notes[i].note == 0x00)
      break;
    LCD_Char(notes[i].note);
    LCD_Char(notes[i].accidental);
    LCD_Char(notes[i].num);
    LCD_Char(' ');
  }
}

/*********************************************************************
* Function: disp_tempo
* Purpose: display tempo to LCD screen 
**********************************************************************/
void disp_tempo(uint16 tempo)
{
  char tempo_str[] = "Tempo: ";
  char outChar;
  
  LCD_Cmd(0xC0); // set cursor to first block of 2nd line
  
  LCD_String(tempo_str); // Print "Tempo: "
  
  // output 100's place (if exists)
  outChar = (tempo/100)%10;
  if(outChar != 0)
    LCD_Char('0' + outChar);
  else
    LCD_Char(' ');
  
  // output 10's place (if exists)
  outChar = (tempo/10)%10;
  LCD_Char('0' + outChar);
  
  // output 1's place
  outChar = tempo%10;
  LCD_Char('0' + outChar);
}

/*
Function: string_to_lcd
Purpose: send string to lcd module one byte at a time
Parameters: pointer to the string
Returns: none
*/
void LCD_String(char *string)
{
  int i = 0;
  while(string[i]) // iterate until NULL terminator
  {
    LCD_Char(string[i]); // send data
    i++;
  }
}

/******************* PWM Functinos ****************************/
/*************************
*  PWM_INIT
*    configure for 1MHz clock (given 24 MHz bus clock)
*************************/
void PWM_Init()
{
  PWMPOL = 0x20; // P5 starts high
  PWMCLK = 0x20; // Use clk SA on PWM pin 5
  PWMPRCLK = 0x01; // CLKA = bus/2, CLKSA = CLKA/12
  PWMSCLA = 6;      // Using 1MHz clk rate
  PWMCTL = 0x40; // Concatenate channels 5 and 4
  PWMCAE = 0x00;  // Left aligned
  PWMPER45 = tone[0]; // initialize to first tone
  PWMDTY45 = tone[0]/2;
  PWME = 0x20; // enable channel 5
}  // PWM_INIT()

/******************* ECT Functinos ****************************/
/***********************
* Initialize ECT	 
***********************/
void ECT_Init()
{
  TIOS = 0x01; // Ch0 output compare
  TSCR1 = 0x80; // Enable timer system
  TIE = 0x01; // interrupts on Ch0 OC
  TSCR2 = 0x06; // prescale by 64 - 375kHz ECT clk
  g_tick_cnt = 0;  // init counter and index to 0
  g_tick_index = 0;
  TC0 = TCNT + (int)(1406250/g_bpm); // set first "alarm"
}

/******************* CAN Functinos ****************************/
/***********************
* Initialize CAN0	 
***********************/
void CAN0_Init(void)
{
   CAN0CTL1 |= 0x80; // enable CAN, required after reset
   
   CAN0CTL0 |= 0x01; // request to enter initialization mode
   while (!(CAN0CTL1 & 0x01)); // wait until initialization mode is entered
   CAN0CTL1 = 0x84;  // enable CAN0, select oscillator as MSCAN clock
							// enable wake up filter
							
   CAN0BTR0 = 0x41;  // set SJW to 2 & prescale factor to 2 (trans freq=0.166 kbaud)
   CAN0BTR1 = 0x18;  // set phase_seg1 and phase_seg2 to 2 Tq
					 		// and prop_seg to 7 Tq
					 		
 	 CAN0IDAR0 = 'B';   //ID B for tempo update 	                   	
   
   CAN0IDMR0 = 0x00; 	// acceptance mask for "B"
   CAN0IDMR1 = 0xFF; 	// Ignore I.D. 1-7 -->
   CAN0IDMR2 = 0xFF; 	// "
   CAN0IDMR3 = 0xFF; 	// "
   
   CAN0IDMR4 = 0xFF; 	// "
   CAN0IDMR5 = 0xFF; 	// "
   CAN0IDMR6 = 0xFF; 	// "
   CAN0IDMR7 = 0xFF; 	// "
   
   CAN0IDAC = 0x20; 	// xx10xxxx x8 8 bit filters (individual character filters)
	
   CAN0CTL0 &= 0xFE; // exit initialization mode
   CAN0CTL1 |= 0x04; // stop clock on wait mode, enable wake up	
}

/***********************
* monitor CAN0	
*  if message matches identifier 
*  1. copy message into array pointed to by first argument
*  2. return non-zero value
***********************/
uint8 CAN0_Receive()
{	
  uint8 tmp = 0, value;
		
  if (!(CAN0RFLG & 0x01))
    return 0; 	// buffer not full
      
  //Otherwise, if filter registers a hit...
   
  tmp = CAN0IDAC & 0x07;  // extract filter hit

  if (!tmp)	// buffer full (filter 0 [B] was "hit")
  {
  
     if (CAN0RXDLR == 0)
     {
        CAN0RFLG |= 0x01;	// clear RxF (release buffer)
        return 0;	// message contained no data (remote frame)
     }
     
     else 
     {  
      //for (i=0; i<len; i++)	// copy message to array
           //*(msg_ptr+i) = *(&CAN0RXDSR0 + i);
      value = CAN0RXDSR0; 

      if(CAN0RXDSR1 == 0xFF)
        g_t_flg = ~g_t_flg;       
 
      CAN0RFLG |= 0x01;	// clear RxF (release buffer)

      return (value);
     }
     
  }
  return(0);
}

/***********************
* Transmit CAN frame
***********************/
void CAN0_Transmit(uint8 *ptr)
{
   char tb, i;
   uint8 *pt2;  //changed this from uint16 to uint8, this fixed transmission?
   
   for(;;)	// wait for an empty transmit buffer
   {
      if (CAN0TFLG & 0x01)
      {
         tb = 0;
         break;
      }
      if (CAN0TFLG & 0x02)
      {
         tb = 1;
         break;
      }
      if (CAN0TFLG & 0x04)
      {
         tb = 2;
         break;
      }
   }
	
	 //CAN0FLG ALREADY BITMASKED FROM 'IF' CONDITIONS ABOVE ^ ?
	 
   CAN0TBSEL = CAN0TFLG; 	// make empty transmit buffer accessible
   pt2 = (uint8 *) &CAN0TXIDR0; 	// make pt2 point to CAN0TXIDR0
		
   for(i=0; i<14; i++) 	// copy the whole transmit buffer
   {
      *pt2++ = *ptr++;
   }
		
   if (tb==0)
      CAN0TFLG = 0x01; 	// mark buffer 0 ready for transmission
   else if (tb==1)
      CAN0TFLG = 0x02; 	// mark buffer 1 ready for transmission
   else 
      CAN0TFLG = 0x04; 	// mark buffer 2 ready for transmission			
}

#pragma CODE_SEG __NEAR_SEG NON_BANKED /****** Interrupt Service Routines ****/
/*************************
*  ISR_ECT0
*    Creates a metronome type sound using PWM and counting ticks.
*    On-board buzzer is on for 1/10th of a beat
*************************/
interrupt void ISR_ECT0(void)
{
  TFLG1 &= 0x01; // clear device flag
  
  TC0 = TCNT + (int)(1406250/g_bpm); // EXPLAIN THIS MATH
  
  g_tick_cnt = g_tick_cnt + 1; // update tick counter
  if(g_tick_cnt >= ticks[g_tick_index]) // if past allowed number of ticks
  {
    g_tick_cnt = 0; // reset counter
    g_tick_index = (g_tick_index+1)%2; // move to next index
    PWMDTY45 = tone[g_tick_index]/2; // set duty value
  }
}
