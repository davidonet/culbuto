#include "CTS_Layer.h"
#include "usi_i2c.h"

enum MMA8452Q_Register {
	STATUS = 0x00,
	OUT_X_MSB = 0x01,
	OUT_X_LSB = 0x02,
	OUT_Y_MSB = 0x03,
	OUT_Y_LSB = 0x04,
	OUT_Z_MSB = 0x05,
	OUT_Z_LSB = 0x06,
	SYSMOD = 0x0B,
	INT_SOURCE = 0x0C,
	WHO_AM_I = 0x0D,
	XYZ_DATA_CFG = 0x0E,
	HP_FILTER_CUTOFF = 0x0F,
	PL_STATUS = 0x10,
	PL_CFG = 0x11,
	PL_COUNT = 0x12,
	PL_BF_ZCOMP = 0x13,
	P_L_THS_REG = 0x14,
	FF_MT_CFG = 0x15,
	FF_MT_SRC = 0x16,
	FF_MT_THS = 0x17,
	FF_MT_COUNT = 0x18,
	TRANSIENT_CFG = 0x1D,
	TRANSIENT_SRC = 0x1E,
	TRANSIENT_THS = 0x1F,
	TRANSIENT_COUNT = 0x20,
	PULSE_CFG = 0x21,
	PULSE_SRC = 0x22,
	PULSE_THSX = 0x23,
	PULSE_THSY = 0x24,
	PULSE_THSZ = 0x25,
	PULSE_TMLT = 0x26,
	PULSE_LTCY = 0x27,
	PULSE_WIND = 0x28,
	ASLP_COUNT = 0x29,
	CTRL_REG1 = 0x2A,
	CTRL_REG2 = 0x2B,
	CTRL_REG3 = 0x2C,
	CTRL_REG4 = 0x2D,
	CTRL_REG5 = 0x2E,
	OFF_X = 0x2F,
	OFF_Y = 0x30,
	OFF_Z = 0x31
};

struct Element * keyPressed;
static const uint16_t mma8452_read_interrupt_source[] = { 0x3a, OUT_Y_MSB,
I2C_RESTART, 0x3b, I2C_READ, I2C_READ };
static const uint16_t mma8452_stop[] = { 0x3a, CTRL_REG1, 0x00, I2C_RESTART,
		0x3b, I2C_READ };
static const uint16_t mma8452_start[] = { 0x3a, CTRL_REG1, 0x11, I2C_RESTART,
		0x3b, I2C_READ };

void initAccel() {
	i2c_init(USIDIV_2, USISSEL_2);
	uint8_t status;
	i2c_send_sequence(mma8452_stop, 6, &status, LPM0_bits);
	LPM0;
	i2c_send_sequence(mma8452_start, 6, &status, LPM0_bits);
	LPM0;
}

uint8_t data[2];
int8_t ay[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
uint8_t i = 0;

int8_t computeAccel() {
// High debouncing orientation change
	ay[i] = (int8_t) ((data[0] << 8 | data[1]) >> 8);
	i++;
	if (7 < i)
		i = 0;
	uint8_t j = 0;
	int8_t b = 0;
	for (j = 0; j < 8; j++) {
		if (ay[j] < -12)
			b--;
		if (12 < ay[j])
			b++;
	}
	int8_t r = 0;
	if (8 == b)
		r = 1;
	if (-8 == b)
		r = -1;
	return r;
}

void initCap() {
	TI_CAPT_Init_Baseline(&buttons);
	TI_CAPT_Update_Baseline(&buttons, 5);
}

uint8_t running = 0;
int16_t orient = 0;
uint16_t cycle = 0;
uint16_t onDuration = 0;
uint16_t onoffDuration = 0;
volatile uint32_t seconds = 0;
typedef enum {
	AROMA, CONTINU
} mode_t;
mode_t selectedMode = AROMA;
uint8_t isBoost = 0;

void startMotor() {
// setting valve
	if (orient < 0) {
		P1OUT |= BIT1;
	} else {
		P1OUT &= ~BIT1;
	}
	running = 1;
}

void stopMotor() {
// stop motor and valve
	running = 0;
	P1OUT &= ~BIT1;
	P1OUT &= ~BIT2;
}

void startMode() {
	P2OUT = 0;
	if (orient != 0) {
		if (isBoost)
			P2OUT |= BIT1;
		if (selectedMode == AROMA) {
			// Aroma mode
			P2OUT |= BIT0;
			onDuration = 900;
			onoffDuration = 2700;
			cycle = 3;
			seconds = 0;
		} else {
			// Continu mode
			P2OUT |= BIT2;
			onDuration = 10;
			onoffDuration = 70;
			cycle = 412;
			seconds = 0;
		}
		startMotor();
	} else {
		stopMotor();
	}
}

void checkCap() {
	keyPressed = (struct Element *) TI_CAPT_Buttons(&buttons);
	if ((orient != 0) && (keyPressed)) {
		if (keyPressed == &aroma_element) {
			selectedMode = AROMA;
			startMode();
		}
		if (keyPressed == &boost_element) {
			isBoost = !isBoost;
			if (orient != 0)
				P2OUT |= BIT1;
		}
		if (keyPressed == &continu_element) {
			selectedMode = CONTINU;
			startMode();
		}
	}
}

void sleep(unsigned int time) {
	TA0CCR0 = time;
	TA0CTL = TASSEL_1 + MC_1 + TACLR;
	TA0CCTL0 &= ~CCIFG;
	TA0CCTL0 |= CCIE;
	__bis_SR_register(LPM3_bits + GIE);
	__no_operation();
}


void timerCount(){

}

int8_t neworient = 0;
void main(void) {
	volatile uint8_t ticks = 0;
	uint8_t div = 0;

	WDTCTL = WDTPW + WDTHOLD;

	P1DIR = 0xFF;
	P1OUT = 0x00;

	P2DIR = 0xFF;
	P2OUT = 0x00;

	CCTL0 = CCIE;
	CCTL1 = OUTMOD_7;
	CCR1 = 0;

	initCap();
	initAccel();

	while (1) {

		if (i2c_state == I2C_IDLE) {
			// Always wait for i2c end communication
			ticks++;
			switch (ticks) {
			case 1:
				// retrieve accelerometer data
				i2c_send_sequence(mma8452_read_interrupt_source, 6, data, 0);
				break;

			case 2:
				// reset capacitive tracking os i2C clock interference
				TI_CAPT_Reset_Tracking();
				break;

			case 3:
				// check orientation
				neworient = computeAccel();
				if (neworient != orient) {
					orient = neworient;
					startMode();
				}
				break;

			case 4:
				// check buttons
				checkCap();
				break;

			case 5:
				// restart steps
				ticks = 0;
				break;
			}
		}

		div++;
		if (div == 64) {
			// seconds divider
			P1OUT ^= BIT0;
			if (0 < cycle) {
				// cycle going on
				seconds++;
				if (seconds == onDuration) {
					// on period
					stopMotor();
				}
				if (seconds == onoffDuration) {
					// end of off period
					cycle--;
					seconds = 0;
					if (0 == cycle) {
						// end of cycle
						stopMotor();
					} else {
						// next iteration
						startMotor();
					}
				}
			}
			div = 0;
		}
		if (running) {
			// switch on motor
			P1OUT |= BIT2;
		} else {
			// switch off motor
			P1OUT &= ~BIT2;
		}
		sleep(350); // pwm bit banging
		if (~P2OUT & BIT1) {
			P1OUT &= ~BIT2;
		}
		if (ticks == 3) {
			// capacitive sensing bias
			sleep(59);
		} else {
			sleep(159);
		}
	}
}

#pragma vector=TIMER0_A0_VECTOR
__interrupt void ISR_Timer0_A0(void) {
	TA0CTL &= ~(MC_1);
	TA0CCTL0 &= ~(CCIE);
	__bic_SR_register_on_exit(LPM3_bits + GIE);
}

