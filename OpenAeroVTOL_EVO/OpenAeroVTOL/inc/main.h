/*********************************************************************
 * main.h
 ********************************************************************/
#include <stdbool.h>

//***********************************************************
//* External defines
//***********************************************************

#define	PBUFFER_SIZE 16 // Print buffer
#define	SBUFFER_SIZE 25 // Serial input buffer (25 for S-Bus)

//***********************************************************
//* Externals
//***********************************************************

// Buffers
extern char pBuffer[PBUFFER_SIZE];
extern uint8_t	buffer[1024];
extern char sBuffer[SBUFFER_SIZE];

extern bool	RefreshStatus;
extern uint32_t ticker_32;	
extern int16_t	transition_counter;	
extern uint8_t	Transition_state;
extern int16_t	transition;

// Flags
extern volatile uint8_t	General_error;
extern volatile uint8_t	Flight_flags;
extern volatile uint8_t	Alarm_flags;

// Misc
extern volatile uint16_t InterruptCount;
extern volatile uint16_t LoopStartTCNT1;
extern volatile bool Overdue;
extern volatile uint8_t	LoopCount;
extern volatile bool SlowRC;

extern volatile uint32_t PWM_Available_Timer;	// debug
extern volatile uint32_t interval;				// IMU interval
extern volatile uint32_t RC_Rate_Timer;
extern volatile int16_t PWM_pulses_global;

extern volatile uint32_t PWM_interval;