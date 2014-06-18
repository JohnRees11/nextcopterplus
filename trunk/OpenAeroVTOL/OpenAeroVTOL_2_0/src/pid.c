//***********************************************************
//* pid.c
//***********************************************************

//***********************************************************
//* Includes
//***********************************************************

#include "compiledefs.h"
#include <avr/pgmspace.h> 
#include <avr/io.h>
#include <stdbool.h>
#include <util/delay.h>
#include <stdlib.h>
#include "io_cfg.h"
#include "gyros.h"
#include "main.h"
#include "init.h"
#include "acc.h"
#include "imu.h"
#include "rc.h"
#include "mixer.h"
#include "isr.h"

//************************************************************
// Defines
//************************************************************

#define GYRO_DEADBAND	5			// Region where no gyro input is added to I-term
#define PID_SCALE 6					// Empirical amount to reduce the PID values by to make them most useful
#define RESET_ITERM_RC 50			// Stick position beyond which the relevant I-term is reset

//************************************************************
// Notes
//************************************************************
//
// Servo output range is 2500 to 5000, centered on 3750.
// RC and PID values are added to this then rescaled at the the output stage to 1000 to 2000.
// As such, the maximum usable value that the PID section can output is +/-1250.
// So working backwards, prior to rescaling (/64) the max values are +/-80,000.
// Prior to this, PID_Gyro_I_actual has been divided by 32 so the values are now +/- 2,560,000
// However the I-term gain can be up to 127 which means the values are now limited to +/-20,157 for full scale authority.
// For reference, a constant gyro value of 50 would go full scale in about 1 second at max gain of 127 if incremented at 400Hz.
// This seems about right for heading hold usage.
//
// On the KK2.1 the gyros are configured to read +/-2000 deg/sec at full scale, or 16.4 deg/sec for each LSB value.  
// I divide that by 16 to give 0.976 deg/sec for each digit the gyros show. So "50" is about 48.8 degrees per second.
// 360 deg/sec would give a reading of 368 on the sensor calibration screen. Full stick is about 1000 or so. 
// So with no division of stick value by "Axis rate", full stick would equate to (1000/368 * 360) = 978 deg/sec. 
// With axis rate set to 2, the stick amount is quartered (250) or 244 deg/sec. A value of 3 would result in 122 deg/sec. 
//
// Stick rates: /64 (15.25), /32 (30.5), /16 (61*), /8 (122), /4 (244)
		
//************************************************************
// Prototypes
//************************************************************

void Sensor_PID(void);
void Calculate_PID(void);

//************************************************************
// Code
//************************************************************

// PID globals for each [Profile] and [axis]
int16_t PID_Gyros[FLIGHT_MODES][NUMBEROFAXIS];
int16_t PID_ACCs[FLIGHT_MODES][NUMBEROFAXIS];
int32_t	IntegralGyro[FLIGHT_MODES][NUMBEROFAXIS];	// PID I-terms (gyro) for each axis

int32_t PID_AvgAccVert = 0;
float 	gyroSmooth[NUMBEROFAXIS];					// Filtered gyro data
	
// Run each loop to average gyro data and also accVert data
void Sensor_PID(void)
{
	float tempf = 0;
	float gyroADCf = 0;
	int8_t i = 0;
	int8_t	axis = 0;	
	int16_t	stick_P1 = 0;
	int16_t	stick_P2 = 0;
	
	// Cross-reference table for actual RCinput elements
	// Note that axes are reversed here with respect to their gyros
	// So why is AILERON different? Well on the KK hardware the sensors are arranged such that
	// RIGHT roll = +ve gyro, UP pitch = +ve gyro and LEFT yaw = +ve gyro.
	// However the way we have organised stick polarity, RIGHT roll and yaw are +ve, and DOWN elevator is too.
	// When combining with the gyro signals, the sticks have to be in the opposite polarity as the gyros.
	// As described above, pitch and yaw are already opposed, but roll needs to be reversed.

	int16_t	RCinputsAxis[NUMBEROFAXIS] = {-RCinputs[AILERON], RCinputs[ELEVATOR], RCinputs[RUDDER]};
	
	int8_t Stick_rates[FLIGHT_MODES][NUMBEROFAXIS] =
	{
		{Config.FlightMode[P1].Roll_Rate, Config.FlightMode[P1].Pitch_Rate, Config.FlightMode[P1].Yaw_Rate},
		{Config.FlightMode[P2].Roll_Rate, Config.FlightMode[P2].Pitch_Rate, Config.FlightMode[P2].Yaw_Rate}
	};
	
	// Gyro LPF scale
	tempf = pgm_read_byte(&LPF_lookup[Config.Gyro_LPF]); // Lookup actual LPF value promote
	
	for (axis = 0; axis <= YAW; axis ++)
	{
		//************************************************************
		// Increment and limit gyro I-terms, handle heading hold nicely
		//************************************************************

		// Work out stick rate divider. 0 is slowest, 4 is fastest.
		// /64 (15.25), /32 (30.5), /16 (61*), /8 (122), /4 (244)
		stick_P1 = RCinputsAxis[axis] >> (4 - (Stick_rates[P1][axis] - 2));
		stick_P2 = RCinputsAxis[axis] >> (4 - (Stick_rates[P2][axis] - 2));

		// Calculate I-term from gyro and stick data 
		// These may look similar, but they are constrained quite differently.
		IntegralGyro[P1][axis] += (gyroADC[axis] + stick_P1);
		IntegralGyro[P2][axis] += (gyroADC[axis] + stick_P2);

#ifdef KK21			
		// If Hands-free is enabled, reset I-term of 
		// any axis whose control input is activated more than 5% (+/-20)
		if (Config.Handsfree == ON)
		{
			// If axis-relevant stick input larger than limit 
			if ((RCinputsAxis[axis] < -RESET_ITERM_RC) || (RCinputsAxis[axis] > RESET_ITERM_RC))
			{
				IntegralGyro[P1][axis] = 0;
				IntegralGyro[P2][axis] = 0;
			}
		}
#endif

		// Limit the I-terms to the user-set limits
		for (i = P1; i <= P2; i++)
		{
			if (IntegralGyro[i][axis] > Config.Raw_I_Constrain[i][axis])
			{
				IntegralGyro[i][axis] = Config.Raw_I_Constrain[i][axis];
			}
			if (IntegralGyro[i][axis] < -Config.Raw_I_Constrain[i][axis])
			{
				IntegralGyro[i][axis] = -Config.Raw_I_Constrain[i][axis];
			}
		}

		//************************************************************
		// Gyro LPF
		//************************************************************	
			
		gyroADCf = gyroADC[axis]; // Promote

		if (tempf > 1)
		{
			// Gyro LPF
			gyroSmooth[axis] = (gyroSmooth[axis] * (tempf - 1.0f) + gyroADCf) / tempf;
		}
		else
		{
			// Use raw gyroADC[axis] as source for gyro values
			gyroSmooth[axis] =  gyroADCf;
		}		
		
		// Demote back to int16_t
		gyroADC[axis] = (int16_t)gyroSmooth[axis];		
	}
	
	// Average accVert prior to Calculate_PID()
	PID_AvgAccVert += accVert;
			
}

// Run just before PWM output, using averaged data
void Calculate_PID(void)
{
	int32_t PID_gyro_temp1 = 0;				// P1
	int32_t PID_gyro_temp2 = 0;				// P2
	int32_t PID_acc_temp1 = 0;				// P1
	int32_t PID_acc_temp2 = 0;				// P2
	int32_t PID_Gyro_I_actual1 = 0;			// Actual unbound i-terms P1
	int32_t PID_Gyro_I_actual2 = 0;			// P2
	int8_t	axis = 0;

#ifdef KK21	
	int32_t temp32 = 0;						// Needed for 32-bit dynamic gain calculations
	int32_t mult32 = 0;
	int16_t temp16 = 0;
	int32_t PID_Gyros_32;

	int16_t	RCinputsAxis[NUMBEROFAXIS] = {RCinputs[AILERON], RCinputs[ELEVATOR], RCinputs[RUDDER]};
#endif

	// Initialise arrays with gain values.
	int8_t 	P_gain[FLIGHT_MODES][NUMBEROFAXIS] = 
		{
			{Config.FlightMode[P1].Roll_P_mult, Config.FlightMode[P1].Pitch_P_mult, Config.FlightMode[P1].Yaw_P_mult},
		 	{Config.FlightMode[P2].Roll_P_mult, Config.FlightMode[P2].Pitch_P_mult, Config.FlightMode[P2].Yaw_P_mult}
		};

	int8_t 	I_gain[FLIGHT_MODES][NUMBEROFAXIS] = 
		{
			{Config.FlightMode[P1].Roll_I_mult, Config.FlightMode[P1].Pitch_I_mult, Config.FlightMode[P1].Yaw_I_mult},
			{Config.FlightMode[P2].Roll_I_mult, Config.FlightMode[P2].Pitch_I_mult, Config.FlightMode[P2].Yaw_I_mult}
		};

	int8_t 	L_gain[FLIGHT_MODES][NUMBEROFAXIS] = 
		{
			{Config.FlightMode[P1].A_Roll_P_mult, Config.FlightMode[P1].A_Pitch_P_mult, Config.FlightMode[P1].A_Zed_P_mult},
			{Config.FlightMode[P2].A_Roll_P_mult, Config.FlightMode[P2].A_Pitch_P_mult, Config.FlightMode[P2].A_Zed_P_mult}
		};

	// Only for roll and pitch acc trim
	int16_t	L_trim[FLIGHT_MODES][2] =
		{
			{Config.Rolltrim[P1], Config.Pitchtrim[P1]},
			{Config.Rolltrim[P2], Config.Pitchtrim[P2]}
		};

	// Average accVert
	accVert = (int16_t)(PID_AvgAccVert / LoopCount);
	PID_AvgAccVert = 0;							// Reset average

	//************************************************************
	// PID loop
	//************************************************************
	for (axis = 0; axis <= YAW; axis ++)
	{
#ifdef KK21	
		//************************************************************
		// Set up dynamic gain variable once for each axis
		//************************************************************

		// Channel controlling the dynamic gain
		temp16 = RCinputsAxis[axis]; // -1000 to 1000 range

		// Scale 0 - 500 down to 0 - Config.Progressive. 
		temp16 = abs(temp16) / Config.ProgressiveDiv;
		
		// Limit maximum value to 100
		if (temp16 > 100) temp16 = 100;
#endif			
		//************************************************************
		// Filter and calculate gyro error
		//************************************************************

		// Reduce Gyro drift noise before adding into I-term
		if ((gyroADC[axis] > -GYRO_DEADBAND) && (gyroADC[axis] < GYRO_DEADBAND)) 
		{
			gyroADC[axis] = 0;
		}
		
		//************************************************************
		// Add in gyro Yaw trim
		//************************************************************

		if (axis == YAW)
		{
			PID_gyro_temp1 = (int32_t)(Config.FlightMode[P1].Yaw_trim << 6);
			PID_gyro_temp2 = (int32_t)(Config.FlightMode[P2].Yaw_trim << 6);
		}
		// Reset PID_gyro variables to that data does not accumulate cross-axis
		else
		{
			PID_gyro_temp1 = 0;
			PID_gyro_temp2 = 0;
		}

		//************************************************************
		// Calculate PID gains
		//************************************************************

		// Gyro P-term													// Profile P1
		PID_gyro_temp1 += gyroADC[axis] * P_gain[P1][axis];				// Multiply P-term (Max gain of 127)
		PID_gyro_temp1 = PID_gyro_temp1 * (int32_t)3;					// Multiply by 3

		// Gyro I-term
		PID_Gyro_I_actual1 = IntegralGyro[P1][axis] * I_gain[P1][axis];	// Multiply I-term (Max gain of 127)
		PID_Gyro_I_actual1 = PID_Gyro_I_actual1 >> 5;					// Divide by 32

		// Gyro P-term
		PID_gyro_temp2 += gyroADC[axis] * P_gain[P2][axis];				// Profile P2
		PID_gyro_temp2 = PID_gyro_temp2 * (int32_t)3;

		// Gyro I-term
		PID_Gyro_I_actual2 = IntegralGyro[P2][axis] * I_gain[P2][axis];
		PID_Gyro_I_actual2 = PID_Gyro_I_actual2 >> 5;

		//************************************************************
		// I-term output limits
		//************************************************************

		// P1 limits
		if (PID_Gyro_I_actual1 > Config.Raw_I_Limits[P1][axis]) 
		{
			PID_Gyro_I_actual1 = Config.Raw_I_Limits[P1][axis];
		}
		else if (PID_Gyro_I_actual1 < -Config.Raw_I_Limits[P1][axis]) 
		{
			PID_Gyro_I_actual1 = -Config.Raw_I_Limits[P1][axis];	
		}

		// P2 limits
		if (PID_Gyro_I_actual2 > Config.Raw_I_Limits[P2][axis]) 
		{
			PID_Gyro_I_actual2 = Config.Raw_I_Limits[P2][axis];
		}
		else if (PID_Gyro_I_actual2 < -Config.Raw_I_Limits[P2][axis]) 
		{
			PID_Gyro_I_actual2 = -Config.Raw_I_Limits[P2][axis];	
		}

		//************************************************************
		// Sum Gyro P, I and D terms and rescale
		//************************************************************

		PID_Gyros[P1][axis] = (int16_t)((PID_gyro_temp1 + PID_Gyro_I_actual1) >> PID_SCALE);  // PID_SCALE was 6, now 5
		PID_Gyros[P2][axis] = (int16_t)((PID_gyro_temp2 + PID_Gyro_I_actual2) >> PID_SCALE);

#ifdef KK21
		//************************************************************
		// Modify gains dynamically as required
		// The gain is determined by the relevant stick position. 
		// The maximum gain reduction is determined by Config.Progressive
		// temp16 holds the current amount of gain reduction 
		//************************************************************
		// If progressive dynamic gain is set up 
		if (Config.Progressive != 0)
		{
			temp32 = 0;
			mult32 = 100 - temp16;					// Max (100%) - Current setting (0 to Config.Progressive)

			// P1
			temp32 = PID_Gyros[P1][axis];			// Promote to 32 bits
			PID_Gyros_32 = temp32 * mult32;			// Multiply by suppression value (100% down to 0%)
			temp32 = (PID_Gyros_32 / (int32_t)100);	// Scale back to 0-100%
			PID_Gyros[P1][axis] = (int16_t)temp32;	// Cast back to native size

			// P2
			temp32 = PID_Gyros[P2][axis];			// Promote to 32 bits
			PID_Gyros_32 = temp32 * mult32;			// Multiply by suppression value (100% down to 0%)
			temp32 = (PID_Gyros_32 / (int32_t)100);	// Scale back to 0-100%
			PID_Gyros[P2][axis] = (int16_t)temp32;	// Cast back to native size
		}
#endif
		//************************************************************
		// Calculate error from angle data and trim (roll and pitch only)
		//************************************************************

		if (axis < YAW)
		{
			
#ifdef KK21
			// If Hands-free is enabled, remove Autolevel reading of
			// any axis whose control input is activated more than 5% (+/-20)
			if (Config.Handsfree == ON)
			{
				// If axis-relevant stick input larger than limit
				if ((RCinputsAxis[axis] < -RESET_ITERM_RC) || (RCinputsAxis[axis] > RESET_ITERM_RC))
				{
					angle[axis] = 0;
				}
			}
#endif

			PID_acc_temp1 = angle[axis] - L_trim[P1][axis];				// Offset angle with trim
			PID_acc_temp2 = angle[axis] - L_trim[P2][axis];

			PID_acc_temp1 *= L_gain[P1][axis];							// P-term of accelerometer (Max gain of 127)
			PID_ACCs[P1][axis] = (int16_t)(PID_acc_temp1 >> 8);			// Reduce and convert to integer

			PID_acc_temp2 *= L_gain[P2][axis];							// Same for P2
			PID_ACCs[P2][axis] = (int16_t)(PID_acc_temp2 >> 8);	
		}

	} // PID loop

	//************************************************************
	// Calculate an Acc-Z value 
	//************************************************************

	PID_acc_temp1 = -accVert;				// Get and copy Z-acc value. Negate to oppose G
	PID_acc_temp2 = PID_acc_temp1;

	PID_acc_temp1 *= L_gain[P1][YAW];		// Multiply P-term (Max gain of 127)
	PID_acc_temp2 *= L_gain[P2][YAW];		// Multiply P-term (Max gain of 127)

	PID_acc_temp1 = PID_acc_temp1 >> 4;		// Moderate Z-acc to reasonable values
	PID_acc_temp2 = PID_acc_temp2 >> 4;	

	if (PID_acc_temp1 > MAX_ZGAIN)			// Limit to +/-MAX_ZGAIN
	{
		PID_acc_temp1 = MAX_ZGAIN;
	}
	if (PID_acc_temp1 < -MAX_ZGAIN)
	{
		PID_acc_temp1 = -MAX_ZGAIN;
	}

	if (PID_acc_temp2 > MAX_ZGAIN)
	{
		PID_acc_temp2 = MAX_ZGAIN;
	}
	if (PID_acc_temp2 < -MAX_ZGAIN)
	{
		PID_acc_temp2 = -MAX_ZGAIN;
	}

	PID_ACCs[P1][YAW] = (int16_t)PID_acc_temp1; // Copy to global values
	PID_ACCs[P2][YAW] = (int16_t)PID_acc_temp2;	
}
