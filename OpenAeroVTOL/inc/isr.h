/*********************************************************************
 * isr.h
 ********************************************************************/

#include "..\inc\io_cfg.h"

//***********************************************************
//* Externals
//***********************************************************

extern volatile bool Interrupted;
extern volatile uint16_t RxChannel[MAX_RC_SOURCES]; 
extern volatile uint8_t max_chan;		// Number of the channel that is before the acceptible gap
extern volatile uint8_t ch_num;
extern volatile uint16_t checksum;