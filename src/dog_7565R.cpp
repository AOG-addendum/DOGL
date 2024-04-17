/*
 * Copyright (c) 2014 by ELECTRONIC ASSEMBLY <technik@lcd-module.de>
 * EA DOG Graphic (ST7565R) software library for arduino.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */


#include "dog_7565R.h"


#define INITLEN 14
uint8_t init_DOGM128[INITLEN] = {0x40, 0xA1, 0xC0, 0xA6, 0xA2, 0x2F, 0xF8, 0x00, 0x27, 0x81, 0x16, 0xAC, 0x00, 0xAF};
uint8_t init_DOGL128[INITLEN] = {0x40, 0xA1, 0xC0, 0xA6, 0xA2, 0x2F, 0xF8, 0x00, 0x27, 0x81, 0x10, 0xAC, 0x00, 0xAF}; 
uint8_t init_DOGM132[INITLEN] = {0x40, 0xA1, 0xC0, 0xA6, 0xA2, 0x2F, 0xF8, 0x00, 0x23, 0x81, 0x1F, 0xAC, 0x00, 0xAF};

//----------------------------------------------------public Functions----------------------------------------------------
//Please use these functions in your sketch

/*----------------------------
Func: DOG-INIT
Desc: Initializes SPI Hardware/Software and DOG Displays
Vars: CS-Pin, MOSI-Pin, SCK-Pin (MOSI=SCK Hardware else Software), A0-Pin (high=data, low=command), p_res = Reset-Pin, type (1=EA DOGM128-6, 2=EA DOGL128-6)
------------------------------*/
void dog_7565R::initialize(uint8_t p_cs, uint8_t p_si, uint8_t p_clk, uint8_t p_a0, uint8_t p_res, uint8_t type) 
{
	uint8_t *ptr_init; //pointer to the correct init values
	top_view = false; //default = bottom view

	dog_7565R::p_a0 = p_a0;
	pinMode(p_a0, OUTPUT);
	spi_initialize(p_cs, p_si, p_clk); //init SPI to Mode 3

	//perform a Reset
	digitalWrite(p_res, LOW);
	pinMode(p_res, OUTPUT);
	delayMicroseconds(10);
	digitalWrite(p_res, HIGH);
	delay(1);

	//Init DOGM displays, depending on users choice
	ptr_init = init_DOGM128; //default pointer for wrong parameters
	if(type == DOGM128) 		ptr_init = init_DOGM128;
	else if(type == DOGL128) 	ptr_init = init_DOGL128;
	else if(type == DOGM132) 	ptr_init = init_DOGM132;
	
	dog_7565R::type = type;

	digitalWrite(p_a0, LOW); //init display
	spi_put(ptr_init, INITLEN);

	clear();
}

/*----------------------------
Func: clear_display
Desc: clears the entire DOG-Display
Vars: ---
------------------------------*/
void dog_7565R::clear(void) 
{
	uint8_t page, column;
	uint8_t page_cnt = 8, column_cnt = 128;
	
	if(type == DOGM132)
	{
		page_cnt = 4;
		column_cnt = 132;
	}
	
	for(page = 0; page < page_cnt; page++) //Display has 8 pages
	{
		position(0,page);
		digitalWrite(p_cs, LOW);
		digitalWrite(p_a0, HIGH);
		
		for(column = 0; column < column_cnt; column++) //clear the whole page line
			spi_out(0x00);
		
		digitalWrite(p_cs, HIGH);
	}
}

/*----------------------------
Func: contrast
Desc: sets contrast to the DOG-Display
Vars: uint8_t contrast (0..63)
------------------------------*/
void dog_7565R::contrast(uint8_t contr) 
{
	command(0x81);   		//double uint8_t command
	command(contr&0x3F);	//contrast has only 6 bits
}

/*----------------------------
Func: view
Desc: ssets the display viewing direction
Vars: direction (top view 0xC8, bottom view (default) = 0xC0)
------------------------------*/
void dog_7565R::view(uint8_t direction)  
{
	if(direction == VIEW_TOP)
	{
		top_view = true;
		command(0xA0);
	}
	else
	{
		top_view = false;
		command(0xA1);
	}
	
	command(direction);
	
	clear(); //Clear screen, as old content is not usable (mirrored)
}

/*----------------------------
Func: string
Desc: shows string with selected font on position
Vars: column (0..127/131), page(0..3/7),  font adress in programm memory, stringarray
------------------------------*/
void dog_7565R::string(uint8_t column, uint8_t page, const uint8_t *font_adress, const char *str)
{
	unsigned int pos_array; 										//Postion of character data in memory array
	uint8_t x, y, column_cnt, width_max;								//temporary column and page adress, couloumn_cnt tand width_max are used to stay inside display area
	uint8_t start_code, last_code, width, page_height, uint8_ts_p_char;	//font information, needed for calculation
	const char *string;

	
	
	start_code 	 = pgm_read_byte(&font_adress[2]);  //get first defined character
	last_code	 = pgm_read_byte(&font_adress[3]);  //get last defined character
	width		 = pgm_read_byte(&font_adress[4]);  //width in pixel of one char
	page_height  = pgm_read_byte(&font_adress[6]);  //page count per char
	uint8_ts_p_char = pgm_read_byte(&font_adress[7]);  //uint8_ts per char
  
  if(type != DOGM132 && page_height + page > 8) //stay inside display area
		page_height = 8 - page;
  else  if(type == DOGM132 && page_height + page > 4)
    page_height = 4 - page;
  
	
	//The string is displayed character after character. If the font has more then one page, 
	//the top page is printed first, then the next page and so on
	for(y = 0; y < page_height; y++)
	{
		position(column, page+y); //set startpositon and page
		column_cnt = column; //store column for display last column check
		string = str;             //temporary pointer to the beginning of the string to print
		digitalWrite(p_a0, HIGH);
		digitalWrite(p_cs, LOW);
		while(*string != 0)
		{	
			if((uint8_t)*string < start_code || (uint8_t)*string > last_code) //make sure data is valid
				string++;
			else
			{							
				//calculate positon of ascii character in font array
				//uint8_ts for header + (ascii - startcode) * uint8_ts per char)
				pos_array = 8 + (unsigned int)(*string++ - start_code) * uint8_ts_p_char;
				pos_array += y*width; //get the dot pattern for the part of the char to print
        
        if(type != DOGM132 && column_cnt + width > 128) //stay inside display area
					width_max = 128-column_cnt;
        else if(type == DOGM132 && column_cnt + width > 132)
           width_max = 132-column_cnt;
				else
					width_max = width;
          
				for(x=0; x < width_max; x++) //print the whole string
				{
					spi_out(pgm_read_byte(&font_adress[pos_array+x]));
					//spi_out(pgm_read_byte(&font_adress[pos_array+x])); //double width font (bold)
				}
			}
		}
		digitalWrite(p_cs, HIGH);
	}
}

/*----------------------------
Func: rectangle
Desc: shows a pattern filled rectangle on the display
Vars: start and end column (0..127/131) and page(0..3/7), bit pattern
------------------------------*/
void dog_7565R::rectangle(uint8_t start_column, uint8_t start_page, uint8_t end_column, uint8_t end_page, uint8_t pattern)  
{
	uint8_t x, y;
  
  if(type != DOGM132 && end_column > 128) //stay inside display area
		end_column = 128;
  else if(type == DOGM132 && end_column > 132)
     end_column = 132;
	if(type != DOGM132 && end_page > 7)
		end_page = 7;
  else if (type == DOGM132 && end_page > 3)
	  end_page = 3;
    
  for(y=start_page; y<=end_page; y++)
	{
		position(start_column, y);
		digitalWrite(p_a0, HIGH);
		digitalWrite(p_cs, LOW);	
		
		for(x=start_column; x<=end_column; x++)
			spi_out(pattern);
		
		digitalWrite(p_cs, HIGH);
	}
}

/*----------------------------
Func: picture
Desc: shows a BLH-picture on the display (see BitMapEdit EA LCD-Tools (http://www.lcd-module.de/support.html))
Vars: column (0..127/131) and page(0..3/7), program memory adress of data
------------------------------*/
void dog_7565R::picture(uint8_t column, uint8_t page, const uint8_t *pic_adress)  
{
	uint8_t c,p;
	unsigned int uint8_t_cnt = 2;
	uint8_t width, page_cnt;
		
	width = pgm_read_byte(&pic_adress[0]);
	page_cnt = (pgm_read_byte(&pic_adress[1]) + 7) / 8; //height in pages, add 7 and divide by 8 for getting the used pages (uint8_t boundaries)
	     
  if(width + column > 128 && type != DOGM132) //stay inside display area
		width = 128 - column;
  else if(width + column > 132 && type == DOGM132)
    width = 132 - column;
  
  if(type != DOGM132 && page_cnt + page > 8)
		page_cnt = 8 - page;
  else if(type == DOGM132 && page_cnt + page > 4)
    page_cnt = 4 - page;
	
	for(p=0; p<page_cnt; p++)
	{
		position(column, page + p);
		digitalWrite(p_a0, HIGH);
		digitalWrite(p_cs, LOW);
		
		for(c=0; c<width; c++)
			spi_out(pgm_read_byte(&pic_adress[uint8_t_cnt++]));
		
		digitalWrite(p_cs, HIGH);
	}
}

//----------------------------------------------------private Functions----------------------------------------------------
//normally you don't need those functions in your sketch

/*----------------------------
Func: position
Desc: sets write pointer in DOG-Display
Vars: column (0..127/131), page(0..3/7)
------------------------------*/
void dog_7565R::position(uint8_t column, uint8_t page)  
{
	if(top_view && type != DOGM132)
		column += 4;
		
	command(0x10 + (column>>4)); 	//MSB adress column
	command(0x00 + (column&0x0F));	//LSB adress column
	command(0xB0 + (page&0x0F)); 	//adress page	
}

/*----------------------------
Func: command
Desc: Sends a command to the DOG-Display
Vars: data
------------------------------*/
void dog_7565R::command(uint8_t dat) 
{
	digitalWrite(p_a0, LOW);
	spi_put_uint8_t(dat);
}

/*----------------------------
Func: data
Desc: Sends data to the DOG-Display
Vars: data
------------------------------*/
void dog_7565R::data(uint8_t dat) 
{
	 digitalWrite(p_a0, HIGH);
	 spi_put_uint8_t(dat);
}

/*----------------------------
Func: spi_initialize
Desc: Initializes SPI Hardware/Software
Vars: CS-Pin, MOSI-Pin, SCK-Pin (MOSI=SCK Hardware else Software)
------------------------------*/
void dog_7565R::spi_initialize(uint8_t cs, uint8_t si, uint8_t clk) 
{
	//Set pin Configuration
	p_cs = cs;
	
	if(si == clk)
	{
		hardware = true;
		p_si = MOSI;
		p_clk = SCK;
	}
	else 
	{
		hardware = false;
		p_si = si;
		p_clk = clk;
	}
	
	// Set CS to deselct slaves
	digitalWrite(p_cs, HIGH);
	pinMode(p_cs, OUTPUT);
	
	// Set Data pin as output
	pinMode(p_si, OUTPUT);
	
	// Set SPI-Mode 3: CLK idle high, rising edge, MSB first
	digitalWrite(p_clk, HIGH);
	pinMode(p_clk, OUTPUT);
	if(hardware)
	{	
		SPI.begin();
		SPI.setBitOrder(MSBFIRST);
		SPI.setDataMode(SPI_MODE3);
		SPI.setClockDivider(SPI_CLOCK_DIV4);
	}
}

/*----------------------------
Func: spi_put_uint8_t
Desc: Sends one uint8_t using CS
Vars: data
------------------------------*/
void dog_7565R::spi_put_uint8_t(uint8_t dat) 
{
	digitalWrite(p_cs, LOW);
	spi_out(dat);
	digitalWrite(p_cs, HIGH);
}

/*----------------------------
Func: spi_put
Desc: Sends uint8_ts using CS
Vars: ptr to data and len
------------------------------*/
void dog_7565R::spi_put(uint8_t *dat, int len) 
{
	digitalWrite(p_cs, LOW);
	do
	{
		spi_out(*dat++);
	}while(--len);
	
	digitalWrite(p_cs, HIGH);
}

/*----------------------------
Func: spi_out
Desc: Sends one uint8_t, no CS
Vars: data
------------------------------*/
void dog_7565R::spi_out(uint8_t dat) 
{
	uint8_t i = 8;
	if(hardware) 
	{
		SPI.transfer(dat);
	}
	else 
	{
		do 
		{
			if(dat & 0x80)
				digitalWrite(p_si, HIGH);
			else
				digitalWrite(p_si, LOW);
			digitalWrite(p_clk, LOW);
			dat <<= 1;
			digitalWrite(p_clk, HIGH);
	  }while(--i);
	}
}

