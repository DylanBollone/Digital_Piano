/***********************************************************************
* Names: Dylan Bollone, Jess Besonen, Will, Jadon
* 
*
* EGEE 355 - Lab Final Project - Spring 2026
*   Code for digital piano interface
************************************/
/**********************************************************************
* Source Code for "Digital Piano" Station 2
*   Speaker output, record/playback feature
*   CAN, ECT, PWM
**********************************************************************/
#include <hidef.h>      /* common defines and macros */
#include "derivative.h"      /* derivative-specific definitions */
#include "LCD_header.h" /* LCD driver code - provided by Dr. Jones */
#include "my_vectors.h"      /* ISR prototypes */

typedef unsigned char uint8;
typedef unsigned int uint16;

#pragma CODE_SEG __NEAR_SEG NON_BANKED
/* list ISR function prototypes */
/* Prototypes are in my_vectors files */

#pragma CODE_SEG DEFAULT
/* Global Variables */

static const uint16 OFF = 0xFFFF;  //plays a very low tone, doubles as an 0xFFFF length hold. (avoides buzzing of 0 period)

static volatile uint16 ch01tone = OFF;

static volatile uint16 ch23tone = OFF;

static volatile uint16 ch45tone = OFF;

static volatile uint16 ch67tone = OFF;

static volatile uint8 CAN_KEY_VALS[4] ={0,0,0,0};
static volatile uint8 next_CAN_KEY_VALS[4] = {0,0,0,0};

static const uint16 g_PWMPER_LUT[] = {1, 	11453,	10811,	10204,	 9631,	 9091,	 8581,	 8099, // 2
                                           7645,	 7216,	 6811,	 6428,	 6068,	 5727,	 5405,	 5102,	 4816,	 4545,	 4290,	 4050, // 3 
                                           3822,	 3608,	 3405, 	 3214,	 3034,	 2863,	 2703,	 2551,	 2408,	 2273, 	 2145,	 2025, // 4
                                           1911,	 1804,	 1703,   1607,	 1517,	 1432,	 1351,	 1276,	 1204,	 1136,	 1073,	 1012, // 5
                                            956};
                                            
//g_PWMPER_LUT[0] represents RST (rest), which is technically just a very very high frequency.               

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

#pragma DATA_SEG DEFAULT

volatile uint8 g_bpm;  // global BPM value - drives timer interrupts

volatile uint8 g_update_flg;            // ECT interrupt occured?
volatile uint8 metronome_muted = 0;     // Metronome enable/disable' flag
uint8 saved_bpm = 100;
volatile uint8 can_tempo_request = 0;   // 1 = send tempo now
volatile uint8 can_tempo_value = 100;   // value to send
volatile int status = 0;                // variable used in CAN recieve
volatile uint8 g_new_CAN_data = 0;      // flag when new note data has been recieved

//                B for beats (higher priority than "N")
volatile uint8 ptr[14] = {'B',   0x00, 0x00, 0x00,     // IDR 
                    0x80, 0x00, 0x00, 0x00,     // high 4 bytes of data
                    0x00, 0x00, 0x00, 0x00,     // low 4 bytes of data
                    0x02, 0x00};                // DLR, TBPR

// RECORDING DATA STRUCTURE AND VARIABLES
typedef struct
{
  uint8 action;  // bits 7-6: PWM channel, bit 5: ON/OFF, bits 4-0: note value
  uint8 delta;   // ticks since last event
} event;

#define MAX_EVENTS 2000

event recording[MAX_EVENTS];
uint16 event_index = 0;
uint8 delta_counter = 0;
uint8 r_enable = 1;
uint16 play_index = 0;
uint8 countdown = 0;
uint8 playback_enabled = 0;

static uint8 trackingbit = 0;  //retains value between loops
uint8 tempo = 100, prev_tempo = 100;

//static uint8 sw0_curr = 1, sw0_prev = 1;   // PH0 state tracking
//uint8 btns;
#define SW2_MASK 0x08
#define SW3_MASK 0x04
#define SW4_MASK 0x02
#define SW5_MASK 0x01

static uint8 last_trackingbit = 0xFF;
static uint8 redraw_screen = 1;   // force redraw initially

/* List of function prototypes */
void CLK_Init(void);
void PWM_Init(void);
void ECT_Init(void);
void LCD_String(char *string);
void disp_tempo(uint16 tempo);
void DelayMS(int ms);
void baseLCDdisp(void);
void playbackrecord(uint8 btns);
void tempocontrols(uint8 btns);
uint8 Read_Buttons(void);
void CAN0_Init(void);
void CAN0_Receive(void);
void CAN0_Transmit(uint8 *msg_ptr);
void PWM_Init(void);
void LCD_Status_Update(char *str); 

/***************************************************************
* Function: main
* Purpose: main program loop - station 2
***************************************************************/
void main(void)
{
  /* Initialization routines */
  CLK_Init();
  PWM_Init(); 
  LCD_Init();
  ECT_Init();
  CAN0_Init();
  
  g_bpm = 100;

  LCD_Cmd(0xCF);
  
  //setup pushbuttons for STATION
  //SW3: Enable/Disable Tempo
  //SW4: Lower Tempo
  //SW5: Raise Tempo
  DDRH = 0x00;
  PERH = 0x0F;
  PPSH = 0x00;

	EnableInterrupts;

  for(;;)
  {   
    // Read the current state of all pushbuttons (PH0-PH3)
    uint8 btns = Read_Buttons();

    // Check if PH0 (bit 0) is pressed screen selection / mode change button
    if(btns & 0x01)
    {
        // Increment mode tracking variable
        trackingbit++;
        
        // Wrap around after 2 to cycle through the three available screens
        if(trackingbit > 2)
            trackingbit = 0;
    }
    
    // Detect if the active screen has changed since the last loop iteration
    if (trackingbit != last_trackingbit)
    {
        // Update stored mode to current value
        last_trackingbit = trackingbit;
        
        // Request a full screen redraw for the newly selected mode
        redraw_screen = 1;
    }
    
    // State machine -> call the appropriate screen handler based on current mode
    if(trackingbit == 0)
        baseLCDdisp();           // Mode 0: User guide / navigation screen
        
    else if(trackingbit == 1)
        playbackrecord(btns);    // Mode 1: Recording and playback controls
        
    else if(trackingbit == 2)
        tempocontrols(btns);     // Mode 2: Tempo adjustment and metronome
    
    CAN0_Receive();  // Receive CAN messages - do independently of timer interrupt flag
                     // Greatly decreased input delay on lower BPM range
    
    // ECT (timer) flag indicates it is time to handle periodic tasks
    if(g_update_flg)
    {
        // Clear the update flag to acknowledge timer interrupt
        g_update_flg = 0;
        
        // Transmit a CAN message if a tempo change was requested by the mute toggle
        if(can_tempo_request)
        {
            // Check that at least one transmit buffer (0,1,2) is empty
            if(CAN0TFLG & 0x07)
            {
                // Clear the request flag now that transmission is starting
                can_tempo_request = 0;
                
                // Load the new tempo value into the CAN payload (byte index 4)
                ptr[4] = can_tempo_value;
                ptr[5] = 0xFF; // Tell station 1 to turn off the metronome
                
                // Send the prepared CAN message
                CAN0_Transmit((uint8*)ptr);
                
                ptr[5] = 0x00; // reset metronome "flag" byte to 0
                
                // Record the transmitted tempo as the previous known value
                prev_tempo = can_tempo_value;
            }
            // If no buffer free, leave flag set and try again next ECT cycle
        }
        // If metronome is not muted and the BPM has changed from last transmitted value
        //else if(g_bpm != prev_tempo && !metronome_muted)
        else if(g_bpm != prev_tempo)
        {
            // Check for an available transmit buffer
            if(CAN0TFLG & 0x07)
            {
                // Place the new BPM into the payload
                ptr[4] = g_bpm;
                
                // Send CAN message with updated tempo
                CAN0_Transmit((uint8*)ptr);
                
                // Update the stored previous tempo
                prev_tempo = g_bpm;
            }
            // If buffers are full, skip this update; the change will be sent later
          }
      }
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

/************************************************************************
*  PWM_INIT
*   configure for 1MHz clock (given 24 MHz bus clock)
*   ch5 ch4 concatenated (Referred to as 54)
*   CLKA = 12MHz . . CLKSA = 1MHz
*   Left alligned, High duty, enable ch5
************************************************************************/
void PWM_Init()                                                     
{
  PWMCTL = 0xF0;    // 1111 xxxx Concatenate all channels
  PWMPOL = 0x0F;    // 1111 1111 All channels ACTIVE HIGH 
  PWMCLK = 0xFF;    // 1111 1111 All channels on Scaled Clock
  PWMPRCLK = 0x11;  // x001 x001 PRESCALE A and B = BUS / 2^prclk = 24MHz / 2^1 = 12MHz
  PWMCAE = 0x00;    // 0000 0000 LEFT ALLIGNED
  PWMSCLA = 6;      // SCALE A = PRESCALE / (2*saclk) = 12MHz / (2*6) = 1MHz
  PWMSCLB = 6;      // SCALE B = SCALE A
  PWMPER67 = PWMDTY67 = 0;  //Initialize all periods *and duties* to 0
  PWMPER45 = PWMDTY45 = 0;  //effectively mute channels before enabling
  PWMPER23 = PWMDTY23 = 0;
  PWMPER01 = PWMDTY01 = 0;
  PWME = 0xFF;      // 1111 1111 Enable all channels
}  // PWM_INIT()

/********************* LCD Screen Functions ******************************/
/*********************************************************************
* Function: disp_tempo
* Purpose: display tempo to LCD screen 
**********************************************************************/
void disp_tempo(uint16 tempo)
{
  char tempo_str[] = "Tempo: ";
  char outChar;
  
  LCD_Cmd(0xC0); // set cursor to first block of 2nd line <- this broke something (ask Dylan)
  
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
  g_update_flg = 0;
  TC0 = TCNT + (int)(1406250/g_bpm);  // Set first alarm
}

/************************* CAN Functinos ****************************/
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
					 		
 	 CAN0IDAR0 = 'N'; 	                   	
   
   CAN0IDMR0 = 0x00; 	// acceptance mask for "C"
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
* Transmit CAN frame
***********************/
void CAN0_Transmit(uint8 *msg_ptr)
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
      *pt2++ = *msg_ptr++;
   }
		
   if (tb==0)
      CAN0TFLG = 0x01; 	// mark buffer 0 ready for transmission
   else if (tb==1)
      CAN0TFLG = 0x02; 	// mark buffer 1 ready for transmission
   else 
      CAN0TFLG = 0x04; 	// mark buffer 2 ready for transmission			
}

/***********************
* monitor CAN0	
*  if message matches identifier 
*  1. copy message into array pointed to by first argument
*  2. return non-zero value
***********************/
void CAN0_Receive()
{	
  uint8 tmp = 0;
  
  if(!(CAN0RFLG & 0x01))
  {
    status = 0;
    return;
  }
   
  tmp = CAN0IDAC & 0x07;  // extract filter hit

  if (!tmp)	// buffer full (filter 0 [C] was "hit")
  {
  
     if (CAN0RXDLR == 0)
     {
        CAN0RFLG &= 0x01;	// clear RxF (release buffer)
        status = 0;	// message contained no data (remote frame)
        return;
     }
     
     else 
     {
      g_new_CAN_data = 1;
      *(next_CAN_KEY_VALS) = (CAN0RXDSR0);
      *(next_CAN_KEY_VALS+1) = (CAN0RXDSR1);
      *(next_CAN_KEY_VALS+2) = (CAN0RXDSR2);
      *(next_CAN_KEY_VALS+3) = (CAN0RXDSR3);
      
      if (!(*(next_CAN_KEY_VALS) & 0x80)) 
      {
        *(next_CAN_KEY_VALS) = 0x00; //No Note / REST
      }
      if (!(*(next_CAN_KEY_VALS+1) & 0x80))
      {
        *(next_CAN_KEY_VALS+1) = 0x00; //No Note / REST
      }
      if (!(*(next_CAN_KEY_VALS+2) & 0x80))
      {
        *(next_CAN_KEY_VALS+2) = 0x00; //No Note / REST
      }
      if (!(*(next_CAN_KEY_VALS+3) & 0x80)) 
      {
        *(next_CAN_KEY_VALS+3) = 0x00; //No Note / REST
      } 
      
      *(next_CAN_KEY_VALS)    &= 0x7F;
      *(next_CAN_KEY_VALS+1)  &= 0x7F;;
      *(next_CAN_KEY_VALS+2)  &= 0x7F;
      *(next_CAN_KEY_VALS+3)  &= 0x7F;;
        
      CAN0RFLG &= 0x01;	// clear RxF (release buffer)
      status = 1;
      return;
     }
     
  }
  status = 0;
  return;  
}

/********************************** Menu & Switch Logic *************************/
/*********************************************************************
* Function: Read_Buttons
* Purpose:  Return debounced rising-edge (press) of PH0-PH3.
* Returns:  bit0 = PH0 pressed, bit1 = PH1, bit2 = PH2, bit3 = PH3.
**********************************************************************/
uint8 Read_Buttons(void)
{
    static uint8 last = 0x0F;   // assume pulled high
    uint8 curr, stable;
    
    curr = PTIH & 0x0F;
    DelayMS(20);
    stable = PTIH & 0x0F;
    
    if(stable != curr)           // bouncing
        return 0;
    if(stable == last)           // no change
        return 0;
    
    last = stable;
    return (~stable) & 0x0F;     // bits high where button was pressed
}

/*************************************
* Function: DelayMS
* Purpose: Kill clock cycles (determined bus clock is 24 MHz)
*           accepts int for # of ms to delay
*************************************/
void DelayMS(int ms)
{
  __asm {               ; save passed value, ms, on stack
          PSHD
          PSHX
loopin:   DBEQ D,done   ; outer loop handles ms (number of ms)
          LDX #7998     ; time delay = (total cylcles - 3X+3+2) / (clk rate - 24MHz)
loopo:    DBNE X,loopo  ; use eq above to find val of x for specific time delay based on clk rate
          BRA loopin
          
done:     PULX          ; restore values
          PULD
        }
  ms++; // use ms to remove compiler warning
} // DelayMS()

/*********************************************************************
* Function: baseLCDdisp
* Purpose:  Display normal operating mode screen.
*           Show device info and menu hints.
**********************************************************************/
void baseLCDdisp(void)
{
    // Check if the screen needs to be redrawn
    if (redraw_screen)
    {
        // Set cursor to beginning of first line 
        LCD_Cmd(0x80);
        // Display static guide text on line 1
        LCD_String("User Guide PH3-0");
        
        LCD_Status_Update("PH0->Next screen");
        
        // Mark screen as up-to-date to prevent unnecessary redraws
        redraw_screen = 0;
    }
}

/*********************************************************************
* Function: playbackrecord
* Purpose:  Handle recording and playback via pushbuttons.
*           PH1: Start playback (1Play)
*           PH2: Stop recording / pause (2Stop)
*           PH3: Start recording (3Rec)
**********************************************************************/
void playbackrecord(uint8 btns)
{
    uint16 i;

    // Redraw the screen layout if a full refresh is requested
    if (redraw_screen)
    {
        // Set cursor to first line
        LCD_Cmd(0x80);
        
        // Display control labels for the three buttons
        LCD_String("3Rec 2Stop 1Play");
        
        LCD_Status_Update("Status Waiting");
        
        // Clear redraw flag after updating display
        redraw_screen = 0;
    }

    // Check if PH3 button (bit 3 of btns) is pressed "3Rec"
    if(btns & 0x08)
    {
        // Start recording only if playback is not active
        if(!playback_enabled)
        {
            // Enable recording mode
            r_enable = 1;
            
            // Reset event storage index to beginning
            event_index = 0;
            
            // Reset delta time counter for first event timing
            delta_counter = 0;
            
            LCD_Status_Update("Status Recording");
        }
    }
    
    // Check if PH2 button (bit 2) is pressed "2Stop"
    if(btns & 0x04)
    {
        // Stop recording if it is currently active and playback is not running
        if(r_enable && !playback_enabled)
        {
            // Disable recording mode
            r_enable = 0;
            
            LCD_Status_Update("Status Stopped");
        }
    }
    
    // Check if PH1 button (bit 1) is pressed "1Play"
    if(btns & 0x02)
    {
        // If no events have been recorded, inform the user
        if(event_index == 0)
        {
            // Update second line with "No recording" message
            LCD_Cmd(0xC0);
            LCD_String("No recording    ");
        }
        // Otherwise, start playback if not already playing
        else if(!playback_enabled)
        {
            // Enable playback mode
            playback_enabled = 1;
            
            // Ensure recording is disabled during playback
            r_enable = 0;
            
            // Skip leading events that have no action (action bits 0-5 are zero)
            i = 0;
            while(i < event_index && (recording[i].action & 0x3F) == 0)
                i++;
            // Set playback index to first non-zero action event
            play_index = i;
            
            // Initialize countdown timer with the delta of the first playable event
            countdown = (i < event_index) ? recording[play_index].delta : 0;
            
            // Update status display
            LCD_Status_Update("Status Playing");
        }
    }
}

/*******************************************
* Function: LCD_Status_Update
* Purpose: Update status line on LCD screen
*******************************************/
void LCD_Status_Update(char *str)
{
  LCD_Cmd(0xC0);                  // Set cursor to start of 2nd line
  LCD_String("                "); // output spaces to "clear" screen
  LCD_Cmd(0xC0);                  // reset cursor to start of 2nd line
  LCD_String(str);                // output string passed to function
}

/*********************************************************************
* Function: tempocontrols
* Purpose:  Handle tempo adjustment and metronome controls.
*           PH1: Lower tempo by 10 BPM
*           PH2: Raise tempo by 10 BPM
*           PH3: Stop metronome and send CAN message
**********************************************************************/
void tempocontrols(uint8 btns)
{
    // Store last displayed BPM value to avoid unnecessary LCD updates
    static uint8 last_displayed = 0xFF;
    
    // Determine current displayed tempo (0 if muted, otherwise actual BPM)
    uint8 current_display = metronome_muted ? 0 : g_bpm;

    // Perform full screen redraw when requested
    if (redraw_screen)
    {
        // Clear second line
        LCD_Cmd(0xC0);
        LCD_String("                ");
        
        // Set cursor to first line for control hints
        LCD_Cmd(0x80);
        LCD_String("3:Stop 2:T+ 1:T-");
        
        // Force tempo value to be redrawn on next update
        last_displayed = 0xFF;
        
        // Clear redraw flag
        redraw_screen = 0;
    }

    // Refresh tempo display only if the displayed value has changed
    if (current_display != last_displayed)
    {
        disp_tempo(current_display);
        last_displayed = current_display;
    }
    
    // Handle PH1 button (bit 1)  Decrease tempo by 10 BPM
    if(btns & 0x02)
    {
        // Lower BPM but respect lower limit of 40 (min)
        if(g_bpm >= 50)
            g_bpm -= 10;
    }
    
    // Handle PH2 button (bit 2)  Increase tempo by 10 BPM
    if(btns & 0x04)
    {
        // Raise BPM but respect upper limit of 200 (max)
        if(g_bpm <= 190)
            g_bpm += 10;
    }
    
    // Handle PH3 button (bit 3)  Toggle metronome mute state
    if(btns & 0x08)
    {
        // If currently muted, restore saved BPM and unmute
        if(metronome_muted)
        {
            metronome_muted = 0;
            g_bpm = saved_bpm;
            can_tempo_value = g_bpm;
        }
        
        // Otherwise, save current BPM, mute, and set CAN tempo to 0
        else
        {
            saved_bpm = g_bpm;
            metronome_muted = 1;
            can_tempo_value = 0;
        }

        // Request a CAN message transmission with the new tempo state
        can_tempo_request = 1;
    }
}


#pragma CODE_SEG __NEAR_SEG NON_BANKED /****** Interrupt Service Routines ****/
/*************************
*  ISR_ECT0
*    Creates a metronome type sound using PWM and counting ticks.
*    On-board buzzer is on for 1/10th of a beat
*************************/

interrupt void ISR_ECT0(void)
{
  static volatile uint8 curr_tone1, prev_tone1,
                         curr_tone2, prev_tone2,
                         curr_tone3, prev_tone3,
                         curr_tone4, prev_tone4;
  static int i;
  static uint8 channel, note;
  TFLG1 &= 0x01; // clear device flag
  
  TC0 = TCNT + (int)(1406250 / g_bpm);
  
  g_update_flg = 1;
  
  if(playback_enabled)
  {
    if(countdown > 0)
    {  
      countdown--;
    }
    else
    {
      channel = recording[play_index].action & 0xC0;  // upper 2 bits hold channel info
      channel = (channel>>6);
      note = recording[play_index].action & 0x3F; // low 5 bits hold tone value
      CAN_KEY_VALS[channel] = note;
      play_index++;
    
      if(play_index >= event_index)
      { 
        playback_enabled = 0;
        r_enable = 1;
        event_index = 0;
        delta_counter = 0;
    
        // Update second line to "Paused"
        LCD_Status_Update("Status Paused");
    
        for(i=0;i<4;i++)
        CAN_KEY_VALS[i] = 0;
      }
      else
      {  
        // if countdown == 0, apply the changes
        while(recording[play_index].delta <= 1)  // If events occur very close together, apply them concurrently
        {                                                   
          channel = recording[play_index].action & 0xC0;
          channel = channel>>6;
          note = recording[play_index].action & 0x3F;
          CAN_KEY_VALS[channel] = note;
          play_index++;
        }
        countdown = recording[play_index].delta; // go to next event
      }
    }
  }
  
  
  if(delta_counter == 255) 
  {
   if(r_enable && event_index < MAX_EVENTS && !playback_enabled)
   {
     if(event_index > 0)
       recording[event_index].action = recording[event_index-1].action;
     else
       recording[event_index].action = 0;
     recording[event_index].delta = delta_counter; // store counter
     event_index++; // move to next event
     delta_counter = 0; // reset delta counter
   }
   if(event_index >= MAX_EVENTS)
     r_enable = 0; // stop recording
  }
  
  if(r_enable && !playback_enabled)
    delta_counter++;
  
  if(g_new_CAN_data && !playback_enabled)
  {
    for(i=0;i<4;i++)
      CAN_KEY_VALS[i] = next_CAN_KEY_VALS[i];
    g_new_CAN_data = 0;
  }
   
  curr_tone1 = CAN_KEY_VALS[0];
   
  curr_tone2 = CAN_KEY_VALS[1];
   
  curr_tone3 = CAN_KEY_VALS[2];
   
  curr_tone4 = CAN_KEY_VALS[3];
   
  //if tone changes, update duty cycle, else skip (reuse previous)
  if(curr_tone1 != prev_tone1) 
  {
   PWMPER01 = g_PWMPER_LUT[curr_tone1];
   PWMDTY01 = PWMPER01/2;
   if(r_enable && event_index < MAX_EVENTS && !playback_enabled)
   {
     recording[event_index].action = (curr_tone1&0x3F) | 0x00; // store tone and channel data
     recording[event_index].delta = delta_counter; // store counter
     event_index++; // move to next event
     delta_counter = 0; // reset delta counter
   }
  }
  if(curr_tone2 != prev_tone2) 
  {
   PWMPER23 = g_PWMPER_LUT[curr_tone2];
   PWMDTY23 = PWMPER23/2;
   if(r_enable && event_index < MAX_EVENTS && !playback_enabled)
   {
     recording[event_index].action = (curr_tone2&0x3F) | 0x40; // store tone and channel data
     recording[event_index].delta = delta_counter; // store counter
     event_index++; // move to next event
     delta_counter = 0; // reset delta counter
   }
  }
  if(curr_tone3 != prev_tone3) 
  {
   PWMPER45 = g_PWMPER_LUT[curr_tone3];
   PWMDTY45 = PWMPER45/2;
   if(r_enable && event_index < MAX_EVENTS && !playback_enabled)
   {
     recording[event_index].action = (curr_tone3&0x3F) | 0x80; // store tone and channel data
     recording[event_index].delta = delta_counter; // store counter
     event_index++; // move to next event
     delta_counter = 0; // reset delta counter
   }
  }
  if(curr_tone4 != prev_tone4) 
  {
   PWMPER67 = g_PWMPER_LUT[curr_tone4];
   PWMDTY67 = PWMPER67/2;
   if(r_enable && event_index < MAX_EVENTS && !playback_enabled)
   {
     recording[event_index].action = (curr_tone4&0x3F) | 0xC0; // store tone and channel data
     recording[event_index].delta = delta_counter; // store counter
     event_index++; // move to next event
     delta_counter = 0; // reset delta counter
   }
  }   
   
  //set previous tone to equal current for next note
  prev_tone1 = curr_tone1;
  prev_tone2 = curr_tone2;
  prev_tone3 = curr_tone3;
  prev_tone4 = curr_tone4;
}
