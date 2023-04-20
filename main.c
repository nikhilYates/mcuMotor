/*
 * Nikhil Yates 218 121 517
 * Hatim Tayabally 217 888 199
 */

#include "include/dbug12.h"
#include "include/hcs12.h"
#include "include/keypad.h"
#include "include/lcd.h"
#include "include/util.h"

/**
 * 7 segments LED decoder
 * 0,1,2,3,4,5,6,7,8,9,A,B,C,D,E,F,G,H
 *
 * Example: if you want to show "1" on LED segments
 * you should do the following:
 * DDRB = 0xff; //set all pin on port b to output
 * PORTB = segment_decoder[1]; //which means one decodes to 0x06:
 * G F E D C B A
 * 0 0 0 0 1 1 0
 *
 *		 A
 * 		----
 * 	   |	| B
 * 	 F |  	|
 * 		--G-        ===> if B and C segments are one we get the shape of 1 (number one)
 * 	   |	| C
 * 	 E |	|
 * 		----
 *       D
 */
unsigned int segment_decoder[]={
                                 0x3f,0x06,0x5b,0x4f,0x66,
                                 0x6d,0x7d,0x07,0x7f,0x6f,
                                 0x77,0x7c,0x39,0x5e,0x79,
                                 0x71,0x3d,0x76
                               };

volatile char message_index_on_7segment_LEDs = 0;
volatile unsigned int counter_for_real_time_interrupt;
volatile unsigned int display_counter = 0;
volatile unsigned int counter_for_real_time_interrupt_limit;

unsigned int charToIntsDuty(char *inputStr);
void mSDelay(unsigned int itime);



void display_hex_number_on_7segment_LEDs(unsigned int number)
{
  static int index_on_7segment_LEDs = 0;

  //DDRB = 0xff; // PortB is set to be output.
  DDRP = 0xff;

  PTP = ~ (1 << (3 - index_on_7segment_LEDs)); //notice it is negative logic
  PORTB = segment_decoder[( number >> (char) (4*(index_on_7segment_LEDs)) ) & 0xf];

  index_on_7segment_LEDs++;
  /**
   * Index should be 1,2,4,8 ... we shift to left each time
   * example: 0001 << 1 will be: 0010 = 2
   * and 2 = 0010 << 1 will be: 0100 = 4
   * and so on ...
   */

  if (index_on_7segment_LEDs > 3) //means we reach the end of 4 segments LEDs we have
    index_on_7segment_LEDs = 0;

  /**
   * simple example of showing "7" on the first LEDs (the most left one)
   DDRB  = 0xff; // PortB is set to be output.
   DDRP  = 0xff;
   PTP   = ~0x1; //negative logic - means "7" will be shown on first LEDs
   PORTB = 0x07;
   */
}

volatile unsigned int keypad_debounce_timer = 0;



/*
 * Here, we change the nature of the execute the jobs function
 * Now, when we are ready to execute the ISR (i.e., when the counter = limit) we simply call this method and enable keyboard again
 * This gets rid of overhead chewed up by a busy loop
 * 
 */
void execute_the_jobs()
{
  //put the jobs you want to be done here ...

 // printf("keyboard enabled again");
  EnableKeyboardAgain();

}

void INTERRUPT rti_isr(void)
{
  //clear the RTI - don't block the other interrupts
  CRGFLG = 0x80;

  //for instance if limit is "10", every 10 interrupts do something ...
  if (counter_for_real_time_interrupt == counter_for_real_time_interrupt_limit)
    {

	 // printf("execute the jobs\n");
      //reset the counter
      counter_for_real_time_interrupt = 0;

      //do some work
      execute_the_jobs();
    }
  else
    counter_for_real_time_interrupt ++;

    // delete after
  //	printf("ISR incrementing\n");

}

/**
 * initialize the rti: rti_ctl_value will set the pre-scaler ...
 */
void rti_init(unsigned char rti_ctl_value, unsigned int counter_limit)
{
  UserRTI = (unsigned int) & rti_isr; //register the ISR unit

  /**
   * set the maximum limit for the counter:
   * if max set to be 10, every 10 interrupts some work will be done
   */
  counter_for_real_time_interrupt_limit = counter_limit;


  /**
   * RTICTL can be calculated like:
   * i.e: RTICTL == 0x63 == set rate to 16.384 ms:
   * The clock divider is set in register RTICTL and is: (N+1)*2^(M+9),
   * where N is the bit field RTR3 through RTR0  (N is lower bits)
   * 	and M is the bit field RTR6 through RTR4. (M is higher bits)
   * 0110 0011 = 0x63 ==> 1 / (8MHz / 4*2^15)
   * 	which means RTI will happen every 16.384 ms
   * Another example:
   * 0111 1111 = 0x7F ==> 1 / (8MHz / 16*2^16)
   * 	which means RTI will happen every 131.072 ms
   * Another example:
   * 0001 0001 = 0x11 ==> 1 / (8MHz / 2*2^10)   = 256us
   */
  RTICTL = rti_ctl_value;

  // How many times we had RTI interrupts
  counter_for_real_time_interrupt = 0;

  // Enable RTI interrupts
  CRGINT |= 0x80;
  // Clear RTI Flag			11111111111
  CRGFLG = 0x80;
}


/*
 * Main method
 * NOTE: this program only computes the sum or product of two unsigned 4 digit (maximum) integers
 * therefore, for example 9999*9999 will not be displayed
 */
int main(void)
{


	set_clock_24mhz();
	rti_init(0x7A, 7);
	__asm("cli");

	DispInitPort();
	DispInit (2, 16);
//	DispStr(0, 0, "Dragon Motor");
//	DispStr(1, 0, "Machine");



	DDRB = 0xff; // PORTP is output
	PORTB = 0x1; // this gives us access to the H-Bridge I/0 usage PB0, PB1

	DDRP = 0xff;
	PTP = 0x1;	// EN12 for H-Bridge

	/*
	 * All values initialized for PWM function outside
	 * of main while loop
	 */
	PWMCLK = 0; // choose clk A
	PWMPOL = ~0x10; // channel 0 output is high at the start of the period
	PWMCTL = 0x0C; // 8 bit mode
	PWMPRCLK = 0x07; // pre scaler for clock a = 2
	PWMCAE = 0; // left alight mode selected
	PWMPER0 = 0x64; // period value = 100 (1ms)
	PWMPRCLK = 0x22;
	PWME = 0x1; // enable PWM0 channel

	PWMDTY0 = 0x0;

	PWMSDN = 0x80;
	PWMSCLA = 0x75;
	PWMSCLB = 0xFE;


	char inputStr[2]; /* char string to hold input from keypad
					   * kept as a string so we can use our char to ints function
					   */
	unsigned int dutyCycle; // to store the duty cycle signal
	int lastSpeed = 1; // holds the last speed - initialiazed as 1 since thats the first speed when we start the motor
	int currentSpeed; // holds the current speed

	int lastDuty = 20;
	int currentDuty;

	int driveMode = 0; // default drive mode is eco

		while(1) {

			DDRH = 0x00;
			KeypadInitPort();

			unsigned char c = KeypadReadPort();

			if(c != KEYPAD_KEY_NONE) { //check if a key is pressed


				counter_for_real_time_interrupt = 0;

				// engages eco drive mode
				if(c == 'A') {
					DispStr(0, 0, "     ");
					DispStr(0, 0, "Eco");
					driveMode = 0;

				}

				// engages sport drive mode
				else if(c == 'B') {
					DispStr(0, 0, "Sport");
					driveMode = 1;

				}


				else if(c == 'C') { // "clear display" button


					// regardless of drive mode, we want smooth braking
					DispStr(1, 0, "Stopping");

					while(PWMDTY0 > 0) {
						mSDelay(10000);
						PWMDTY0 -= 1;
					}

					PWMDTY0 = 0;
					DispStr(1, 15, "0");
					lastSpeed = 1;


				}

				// motor start button (i.e., drive)
				else if(c == 'D') {
					DispStr(1, 0, "        ");
					PWMDTY0 = 20;
					currentSpeed = 1;
					currentDuty = 20;
					DispStr(1, 15, "1");
				}

				/*
				 * After we have input, we display the duty cycle
				 * by sending the PWM signal via PTP
				 */
				else {

					currentSpeed = c - '0'; // set current speed
					inputStr[0] = c;
					inputStr[1] = '\0';
					dutyCycle = charToIntsDuty(inputStr);



					/* check to see if the next shift is more than 1 greater than the current speed
					 * if it is, display bad shift message, dont allow the shift
					 */
					if(currentSpeed - lastSpeed > 1 || lastSpeed - currentSpeed > 1) {
						DispStr(1, 0, "Bad Shift");
						for(int i = 0; i < 10; i++) {
							busy_loop_delay(1000);
						}
					}
					else {
						DispStr(1, 0, "         ");
						/*
						 * We will use a switch statement to send the
						 * hex value to the MCU
						 */
						switch(dutyCycle) {

							case 1:
								dutyCycle = 20; // 20% duty
								DispStr(1, 15, "1");
								break;
							case 2:
								dutyCycle = 25; // 25% duty
								DispStr(1, 15, "2");
								break;
							case 3:
								dutyCycle = 30; // 30% duty
								DispStr(1, 15, "3");
								break;
							case 4:
								dutyCycle = 40; // 40% duty
								DispStr(1, 15, "4");
								break;
							case 5:
								dutyCycle = 50;// 50% duty
								DispStr(1, 15, "5");
								break;
							default:
								printf("Invalid Entry %d\n", dutyCycle);
						}
						currentDuty = dutyCycle;

						if(driveMode == 0) { // if in eco drive mode, we have gradual gear shift

							while(lastDuty != currentDuty) {
								mSDelay(50000);
								if(lastDuty > currentDuty) {
									PWMDTY0 -= 1;
									lastDuty --;
								}
								else {
									PWMDTY0 += 1;
									lastDuty++;
								}
							}

							PWMDTY0 = dutyCycle;
						}

						else { // if we are in sport mode, then the shifts are instant
							PWMDTY0 = dutyCycle;
						}

						lastDuty = currentDuty;
						lastSpeed = currentSpeed; // set the last speed to what was just inputed
					}

					inputStr[0] = '\0';

				}

				rti_isr();


			}
		}
}


/*
 * This is the char to int conversion specifically for Lab 4
 * It converts the input to an integer to be compared in the switch statement in main
 */
unsigned int charToIntsDuty(char *inputStr) {

	unsigned int duty = 0;

	// converting the char to integer
	for(int i = 0; i < 4; i++) {

		if(inputStr[i] == '\0') {
			return duty;
		}
		else {
			duty *= 10;
			duty += inputStr[i] - '0';
		}
	}
}
/*
 * Delay function takes an unsigned integer argument and 
 * performs nothing for the specified time
 */
void mSDelay(unsigned int itime){
unsigned int i; unsigned int j;
   for(i=0;i<itime;i++)
      for(j=0;j<4000;j++);
}
