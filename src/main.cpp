/*
 * DMDESP library usage example
 * 
 * email : bonny@grobak.net - www.grobak.net - www.elektronmart.com
*/

#include <DMDESP.h>
#include <fonts/ElektronMart6x8.h>

// Function declaration
void ScrollingText(int y, uint8_t speed);

//SETUP DMD
#define DISPLAYS_WIDE 1 // Panel Columns
#define DISPLAYS_HIGH 1 // Panel Rows
DMDESP Disp(DISPLAYS_WIDE, DISPLAYS_HIGH);  // Number of P10 panels used (COLUMNS, ROWS)



//----------------------------------------------------------------------
// SETUP

void setup() {

  // DMDESP Setup
  Disp.start(); // Start DMDESP library
  Disp.setBrightness(100); // Brightness level
  Disp.setFont(ElektronMart6x8); // Set font
  
}



//----------------------------------------------------------------------
// LOOP

void loop() {

  Disp.loop(); // Run Disp loop to refresh LED

  ScrollingText(0, 50); // Display scrolling text using full display height (y position, speed);

}


//--------------------------
// DISPLAY SCROLLING TEXT

static const char *text[] = {"Scrolling text with DMDESP"};

void ScrollingText(int y, uint8_t speed) {

  static uint32_t pM;
  static uint32_t x;
  uint32_t width = Disp.width();
  uint32_t height = Disp.height();
  Disp.setFont(ElektronMart6x8);
  uint32_t fullScroll = Disp.textWidth(text[0]) + width;
  
  if((millis() - pM) > speed) { 
    pM = millis();
    if (x < fullScroll) {
      ++x;
    } else {
      x = 0;
    }
    
    // Clear the display before drawing new text
    Disp.clear();
    
    // Center the text vertically in the full display
    Disp.drawText(width - x, (height / 2) - 4, text[0]);
  }  

}