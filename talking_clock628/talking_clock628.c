
// TALKING CLOCK OLED DFPlayer.  
// By moty22.co.uk
// PIC16F628A

#include <htc.h>
#include "oled_font.c"

#pragma config WDTE=OFF, MCLRE=OFF, BOREN=OFF, FOSC=INTOSCIO, CP=OFF, CPD=OFF, LVP=OFF

#define _XTAL_FREQ 4000000
#define __delay_us(x) _delay((unsigned long)((x)*(_XTAL_FREQ/4000000.0)))
#define __delay_ms(x) _delay((unsigned long)((x)*(_XTAL_FREQ/4000.0)))

#define talk RB5	//pin11 play/talk PB
#define min_pb RB4	//pin10 set minute PB
#define hour_pb RB3	//pin9 set hour PB
#define SCL RA3 //pin2
#define SDA RA2 //pin1
#define busy RA1    //pin18 input for dfplayer playing signal

//prototypes
void play_df (void);
void cmnd (unsigned char track);
void display(void);
void interrupt clock_tick(void);
void command( unsigned char comm);
void oled_init();
void clrScreen();
void sendData(unsigned char dataB);
void startBit(void);
void stopBit(void);
void clock(void);
void drawChar2(char fig, unsigned char y, unsigned char x);
void second(void);
void time(void);

unsigned char min,sec,hour,adv=0;
unsigned char addr=0x78;  //0b1111000

//int every sec 
void interrupt clock_tick(void){
    if (CCP1IF) {
        CCP1IF=0;
        ++adv;
    }    
}
 
void main(void){
	
    	// PIC I/O init
    CMCON = 0b111;		//comparator off
    TRISA = 0b110011;   //SCL, SDA, busy
	TRISB = 0b111111;		 //
    nRBPU = 0;  //pullup
    SCL=1;
    SDA=1;
    
	//UART init
	RCSTA = 0b10010000;		//rx enable, continuous receive
	TXSTA = 0b100100;		//tx=ON, high speed 
	SPBRG = 25;				//baud rate 9600bps=25, 1200bps=207
    
    CCP1CON=0b1011;		//compare mode, resetTMR1
    CCPR1H=0x7F;        // sets CCP1 for count of: 7FFF=32767
    CCPR1L=0xFF;		//CCP in compare mode sets TMR1 to a period of 10 sec
    T1CON = 0b1111;        //TMR1 prescale 1:1, osc=on, timer on
    GIE=1; PEIE=1;      //interrupts
    CCP1IE=1;
    
	min=0; sec=0; hour=0;
    __delay_ms(1000);
    oled_init();

    clrScreen();       // clear screen
    drawChar2(10, 1, 3);  //:
    drawChar2(10, 1, 6);  //:
    second();
    display();
    
	while(1) {

        if(!talk && busy){play_df();}
    
            //set minutes        
        while(!min_pb){
        ++min;
        if(min>59){min=0;}
        display();
        __delay_ms(500);
        }
            //set hours
        while(!hour_pb){
        ++hour;
        if(hour>23){hour=0;}
        display();
        __delay_ms(500);
        }
        
        time();

	}
	
}

void time(void){
    if(adv){
        sec +=adv;
        adv=0;
        if(sec>59){
            sec=sec-60;
            ++min;
            if(min > 59){
                min=0; 
                ++hour;
                if(hour > 23){
                    hour=0;
                }
            }
            display();
        }
        second();
    }
}

void second(void){
    unsigned char d[2];
    
    d[0]=(sec/10) %10; 	//digit on left
    d[1]=sec %10;	
 	
    drawChar2(d[0], 1, 7); 
	drawChar2(d[1], 1, 8);
}

void display(void){
    unsigned char d[4];
    
    d[0]=(hour/10) %10; 	//digit on left
    d[1]=hour %10;	
    d[2]=(min/10) %10; 	//digit on left
    d[3]=min %10;
 	
    drawChar2(d[0], 1, 1); 
	drawChar2(d[1], 1, 2);
	drawChar2(d[2], 1, 4);
    drawChar2(d[3], 1, 5);
}

void play_df (void){
    
    if(hour>20){
        cmnd(21);
        cmnd(hour - 19);
    }else{
        cmnd(hour + 1);
    }
    time();
    cmnd(25);       //HOUR
    time();
    if(min<21){
    cmnd(min + 1);       //minutes
    }
    if(min>20 && min<30){
        cmnd(21);       //20
        cmnd(min - 19);       //minutes
    }
    if(min>30 && min<40){
        cmnd(22);       //30
        cmnd(min - 29);       //minutes
    }
    if(min>40 && min<50){
        cmnd(23);       //40
        cmnd(min - 39);       //minutes
    }
    if(min>50 && min<60){
        cmnd(24);       //50
        cmnd(min - 49);       //minutes
    }
    time();
    cmnd(26);       //minutes
    cmnd(27);       //message
    
}

void cmnd (unsigned char track){
    
    while(!busy){}  //wait for prev track to complete
    
    //These 8 bytes is a request to the dfplayer to play a track in the root folder
    //The request is without CRC, 7E FF 06 03 00 xx EF 
    while (!TRMT){}	TXREG=0x7e;	//send byte after UART finished sending the prev byte	
    while (!TRMT){} TXREG=0xff;	
    while (!TRMT){}	TXREG=0x06;	
    while (!TRMT){}	TXREG=0x03;	
    while (!TRMT){}	TXREG=0x00;	
    while (!TRMT){}	TXREG=0x00;	
    while (!TRMT){} TXREG=track;
    while (!TRMT){} TXREG=0xef;
    __delay_ms(100);
}

    //size 2 chars
void drawChar2(char fig, unsigned char y, unsigned char x)
{
    unsigned char i, line, btm, top;    //
    
    command(0x20);    // vert mode
    command(0x01);

    command(0x21);     //col addr
    command(13 * x); //col start
    command(13 * x + 9);  //col end
    command(0x22);    //0x22
    command(y); // Page start
    command(y+1); // Page end
    
    startBit();
    sendData(addr);            // address
    sendData(0x40);

    for (i = 0; i < 5; i++){
        line=font[5*(fig)+i];
        btm=0; top=0;
            // expend char    
        if(line & 64) {btm +=192;}
        if(line & 32) {btm +=48;}
        if(line & 16) {btm +=12;}           
        if(line & 8) {btm +=3;}

        if(line & 4) {top +=192;}
        if(line & 2) {top +=48;}
        if(line & 1) {top +=12;}        

         sendData(top); //top page
         sendData(btm);  //second page
         sendData(top);
         sendData(btm);
    }
    stopBit();
        
    command(0x20);      // horizontal mode
    command(0x00);    
        
}

void clrScreen()    //fill screen with 0
{
    unsigned char y, i;
    
    for ( y = 0; y < 8; y++ ) {
        command(0x21);     //col addr
        command(0); //col start
        command(127);  //col end
        command(0x22);    //0x22
        command(y); // Page start
        command(y+1); // Page end    
        startBit();
        sendData(addr);            // address
        sendData(0x40);
        for (i = 0; i < 128; i++){
             sendData(0x00);
        }
        stopBit();
    }    
}

//Software I2C
void sendData(unsigned char dataB)
{
    for(unsigned char b=0;b<8;b++){
       SDA=(dataB >> (7-b)) % 2;
       clock();
    }
    TRISA2=1;   //SDA input
    clock();
    __delay_us(5);
    TRISA2=0;   //SDA output

}

void clock(void)
{
   __delay_us(1);
   SCL=1;
   __delay_us(5);
   SCL=0;
   __delay_us(1);
}

void startBit(void)
{
    SDA=0;
    __delay_us(5);
    SCL=0;

}

void stopBit(void)
{
    SCL=1;
    __delay_us(5);
    SDA=1;

}

void command( unsigned char comm){
    
    startBit();
    sendData(addr);            // address
    sendData(0x00);
    sendData(comm);             // command code
    stopBit();
}

void oled_init() {
    
    command(0xAE);   // DISPLAYOFF
    command(0x8D);         // CHARGEPUMP *
    command(0x14);     //0x14-pump on
    command(0x20);         // MEMORYMODE
    command(0x0);      //0x0=horizontal, 0x01=vertical, 0x02=page
    command(0xA1);        //SEGREMAP * A0/A1=top/bottom 
    command(0xC8);     //COMSCANDEC * C0/C8=left/right
    command(0xDA);         // SETCOMPINS *
    command(0x22);   //0x22=4rows, 0x12=8rows
    command(0x81);        // SETCONTRAST
    command(0xFF);     //0x8F
    //next settings are set by default
//    command(0xD5);  //SETDISPLAYCLOCKDIV 
//    command(0x80);  
//    command(0xA8);       // SETMULTIPLEX
//    command(0x3F);     //0x1F
//    command(0xD3);   // SETDISPLAYOFFSET
//    command(0x0);  
//    command(0x40); // SETSTARTLINE  
//    command(0xD9);       // SETPRECHARGE
//    command(0xF1);
//    command(0xDB);      // SETVCOMDETECT
//    command(0x40);
//    command(0xA4);     // DISPLAYALLON_RESUME
 //   command(0xA6);      // NORMALDISPLAY
    command(0xAF);          //DISPLAYON

}

   
