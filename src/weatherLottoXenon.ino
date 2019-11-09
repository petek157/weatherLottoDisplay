/*
 * Project weatherLottoXenon
 * Description: Recieves weather information and Lotto number to display on e ink display
 * Author: Peter Koruga
 * Date: 11/8/19
 */

#include "Adafruit_EPD_RK.h"


#define EPD_CS     D4
#define EPD_DC      D4
#define SRAM_CS     D3
#define EPD_RESET   D2 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY    -1 // can set to -1 to not use a pin (will wait a fixed delay)

Adafruit_IL0398 display(300, 400, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);


#define COLOR1 EPD_BLACK
#define COLOR2 EPD_RED


void setup() {
  Serial.begin(115200);
  //while (!Serial) { delay(10); }
  Serial.println("Adafruit EPD test");
  
  display.begin();

  // large block of text
  display.clearBuffer();
  testdrawtext("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur adipiscing ante sed nibh tincidunt feugiat. Maecenas enim massa, fringilla sed malesuada et, malesuada sit amet turpis. Sed porttitor neque ut ante pretium vitae malesuada nunc bibendum. Nullam aliquet ultrices massa eu hendrerit. Ut sed nisi lorem. In vestibulum purus a tortor imperdiet posuere. ", COLOR1);
  display.display();

  delay(5000);

  display.clearBuffer();
  for (int16_t i=0; i<display.width(); i+=4) {
    display.drawLine(0, 0, i, display.height()-1, COLOR1);
  }

  for (int16_t i=0; i<display.height(); i+=4) {
    display.drawLine(display.width()-1, 0, 0, i, COLOR2);  // on grayscale this will be mid-gray
  }
  display.display();
}

void loop() {
  //don't do anything!
}


void testdrawtext(char *text, uint16_t color) {
  display.setCursor(0, 0);
  display.setTextColor(color);
  display.setTextWrap(true);
  display.print(text);
}