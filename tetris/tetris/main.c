//**************************************************************//
//  Name    : Matrix output - Tetris							//
//  Author  : Joey Corbett										 //
//	Git     : https://github.com/sparpo                          //
//  Date    : 04 April, 2020                                     //
//  Version : 1.0                                                //
//  Notes   : Code for using a 74HC595 Shift Register            //
//          : to make Tetris on 8x8 LED matrix                   //
//**************************************************************//

/*
_/_/_/_/  _/_/_/_/  _/_/_/_/  _/_/_/_/  _/_/_/_/  _/_/_/_/
_/        _/    _/  _/    _/  _/    _/  _/    _/  _/    _/
_/_/_/_/  _/_/_/_/  _/_/_/_/  _/_/_/_/  _/_/_/_/  _/    _/
	  _/  _/        _/    _/  _/  _/    _/        _/    _/
_/_/_/_/  _/        _/    _/  _/    _/  _/        _/_/_/_/
*/

/* 74HC595 pinout
	_______   _______
	|d1	  |___|  +5V|
	|d2			  d0|
	|d3		  Serial|
	|d4		   EnOut|
	|d5		   Latch|
	|d6			 Clk|
	|d7			 Clr|
	|GND		 d7'|
	|_______________|
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#define F_CPU 16000000UL
#include <util/delay.h> //needed for blocking delays

//pins 2(latch), 4(clock), 7(data). Same as pins on arduino board
#define latchPin 2
#define clockPin 4
#define dataPin 7
#define leftInputPin 0
#define rightInputPin 1

#define blockDelay 27

//functions
void init_ports(void);
void init_USART(void);
void init_timer0(void);
void sendmsg (char *s);
void point(uint8_t x,uint8_t y, int state);
void rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, int state);
void newBlock(void);
void moveBlock(void);
void clearData(void);
void drawBlock(uint8_t x, uint8_t y, int type, int state);
void drawData(uint8_t myData[]);
void shiftOut(uint8_t myDataOut);
void checkRow(void);
void gameOver(void);

unsigned char qcntr = 0,sndcntr = 0;   /*indexes into the queue*/
unsigned char queue[100];       /*character queue*/
int updateDelay;
volatile int updateTimeCount;
volatile int updateFlag; // Flag for updating game state
volatile int refreshFlag;// Flag for refreshing display

//Data to be drawn on matrix
enum block{longD, longA, cornerL,cornerR, sqr, dot};

// uint8_t types are used a lot to make sure that data that will be outputted is 8 bits long
uint8_t data[] = {	0b00000001,
	0b01111110,
	0b01000110,
	0b01001010,
	0b01010010,
	0b01100010,
	0b01111110,
0b00000000 };
uint8_t blockX;
uint8_t blockY;
int blockType;
char msg[6] = "hello";

int main(void) {
	init_USART();// Serial for debugging purposes
	init_ports();
	init_timer0();
	clearData();
	newBlock();
	
	sei();
	updateTimeCount = 0;
	updateDelay = 35; // 35* 16ms = 560ms. Not a definition because we may want to potentially change it
	updateFlag = 0;
	refreshFlag = 0;

	
	while (1) {
		/********************************************************************************/
		/* Game update loop																*/
		/********************************************************************************/

		if(updateFlag) {
			checkRow();
			drawBlock(blockX,blockY,blockType, 0); //clear old position
			if((PINC & (1<<rightInputPin)) == 0) { //move block right
				if((blockX < 7) && !(data[blockY]&(1<<(blockX+1)))) {
					blockX++;
				}
			}
			if((PINC & (1<<leftInputPin)) == 0) { //move block left
				if((blockX > 0) && !(data[blockY]&(1<<(blockX-1)))) {
					blockX--;
				}
			}
			moveBlock();
			updateFlag = 0;
		}
		
		/********************************************************************************/
		/* Display update loop. Every 16ms												*/
		/********************************************************************************/
		if(refreshFlag) {
			drawData(data);
			refreshFlag = 0;
		}
		
	}
	return 0;
}

void gameOver() {
	_delay_us(1000000);
	for(uint8_t i = 0; i <= 7; i++) {
		data[i] = 0;
	}

}
void newBlock() { //create new block
	if(data[0] != 0)
	gameOver();
	blockX = 1+rand()%7; //random position
	blockY = -1;
	blockType = rand()%7; //random block type
}
void moveBlock() { //move block down
	blockY++;
	drawBlock(blockX,blockY,blockType,1);
	if(blockY>=7 ) {
		newBlock(); //reset at bottom
	}
	if((data[blockY+1]&(1<<blockX))) {
		newBlock();
	}
}
void checkRow() {
	for(uint8_t i = 0; i <= 7; i++) {
		if(data[i]==0xFF) {
			data[i] = 0;
			
			for(uint8_t j = i; j > 0; j--) {
				//uint8_t temp = data[j];
				data[j] = data[j-1];
			}
			data[0] = 0;
		}
	}
}
void clearData() {
	for(uint8_t i=0; i<8; i++) {
		data[i] = 0;
	}
}
void drawBlock(uint8_t x, uint8_t y, int type, int state) {
	switch (type) {
		case longD:
		rect(x,y,1,3,state);
		break;

		case longA:
		rect(x,y,3,1,state);
		break;
		
		case cornerR:
		rect(x,y,1,2,state);
		point(x-1,y,state);
		break;
		
		case cornerL:
		rect(x,y,1,2,state);
		point(x+1,y,state);
		break;
		
		case sqr:
		rect(x,y,2,2,state);
		break;
		case dot:
		point(x,y,state);
		break;
	}
}

void rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, int state) { //draws rectangle
	for(int i = 0; i<w; i++) {
		for(int j =0; j<h; j++) {
			point(x+i,y-j, state);
		}
	}
}
void point(uint8_t x, uint8_t y, int state) { //actually puts data in array
	if(state)
	data[y] = data[y] | (1<<x); //sets to 1
	else
	data[y] = data[y] & ~(1<<x); //sets to 0
	
}
void drawData(uint8_t myData[]) { //takes data and outputs it to 8x8 matrix
	uint8_t j = 0;
	for (j = 0; j <= 7; j++) {//560*20 = 11200us = 11.2ms
		
		//ground latchPin and hold low for as long as you are transmitting
		PORTD = PORTD & ~(1<<latchPin); //latch pin low
		_delay_us(blockDelay);
		//shift data into regs
		
		shiftOut(~(1<<j)); //grounds row. For example 10111111 means 6th row is on
		
		shiftOut(myData[j]);  //outputs column data.

		//return the latch pin high to signal chip that it
		//no longer needs to listen for information
		
		PORTD = PORTD | (1<<latchPin); //latch pin high
		_delay_us(blockDelay);
	}
	
}
void shiftOut(uint8_t myDataOut) { //shifts 8 bits out to register
	
	// This shifts 8 bits out LSB first,
	//on the rising edge of the clock,
	//clock idles low

	//internal function setup
	uint8_t i=0;
	int pinState;
	
	//clear everything out just in case to
	//prepare shift register for bit shifting
	
	PORTD = PORTD & ~((1<<dataPin) | (1<<clockPin)); //clock & data pin low
	_delay_us(blockDelay);
	//for each bit in the byte myDataOut?
	for (i=0; i<=7; i++)  {
		
		PORTD = PORTD & ~(1<<clockPin); //clock low
		_delay_us(blockDelay);
		//if the value passed to myDataOut and a bitmask result
		// true then... so if we are at i=6 and our value is
		// %11010100 it would the code compares it to %01000000
		// and proceeds to set pinState to 1.
		if ( myDataOut & (1<<i) ) {
			pinState= 1;
		}
		else {
			pinState= 0;
		}
		
		
		//Sets the pin to HIGH or LOW depending on pinState
		PORTD = PORTD | (pinState<<dataPin);
		_delay_us(blockDelay);
		//register shifts bits on upstroke of clock pin
		PORTD = PORTD | (1<<clockPin);
		_delay_us(blockDelay);
		//zero the data pin after shift to prevent bleed through
		PORTD = PORTD & ~(1<<dataPin);
		_delay_us(blockDelay);
	}
	
	//stop shifting
	PORTD = PORTD & ~(1<<clockPin);
	_delay_us(blockDelay);
}

void sendmsg (char *s)
{
	qcntr = 0;    /*preset indices*/
	sndcntr = 1;  /*set to one because first character already sent*/
	
	queue[qcntr++] = 0x0d;   /*put CRLF into the queue first*/
	queue[qcntr++] = 0x0a;
	while (*s)
	queue[qcntr++] = *s++;   /*put characters into queue*/
	
	UDR0 = queue[0];  /*send first character to start process*/
}

/********************************************************************************/
/* Interrupt Service Routines													*/
/********************************************************************************/

/*this interrupt occurs whenever the */
/*USART has completed sending a character*/

ISR(USART_TX_vect)
{
	/*send next character and increment index*/
	if (qcntr != sndcntr)
	UDR0 = queue[sndcntr++];
}

ISR(TIMER0_OVF_vect) { //overflow every 16ms
	refreshFlag = 1; // Call for updating screen
	TCNT0 = 6; // 256-250 = 6
	++updateTimeCount;
	if(updateTimeCount >= updateDelay) { //after 208ms, update game state
		updateFlag = 1; // Call for updating game
		updateTimeCount = 0;
	}
}

void init_ports() {
	DDRC = 0; //all pins are input
	PORTC = (1<<rightInputPin) | (1<<leftInputPin); //enable pullup resistors
	
	DDRD = (1<<latchPin) | (1<<clockPin) | (1<<dataPin); //enable pins as output
	PORTD = 0; //set all pins low
}
void init_USART() {
	UCSR0B = (1<<TXEN0) | (1<<TXCIE0); //enable transmitter and tx interrupt
	UBRR0 = 103; //baud 9600
}
void init_timer0() { //16Mhz/1024 = 15625Hz = 0.064ms
	
	TCCR0A = 0;
	TIMSK0 = (1<<TOIE0);	// enable interrupt
	TCCR0B = (5<<CS00); // prescalar 1024
	// For refresh rate of 62.5Hz, 16ms /0.064ms = 250
	TCNT0 = 6; // 256-250 = 6
}
