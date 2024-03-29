/*
 * Project weatherLottoXenon
 * Description: Recieves weather information and Lotto number to display on e ink display
 * Author: Peter Koruga
 * Date: 11/8/19
 */
#include "Adafruit_EPD.h"
#include "SdFat.h"

// Definitions for FeatherWing EPD on Particle
// There are definitions for other platforms in the Adafruit version: https://github.com/adafruit/Adafruit_EPD
  #define SD_CS       D2
  #define SRAM_CS     D3
  #define EPD_CS      D4
  #define EPD_DC      D5


#define EPD_RESET   -1 // can set to -1 and share with microcontroller Reset!
#define EPD_BUSY    -1 // can set to -1 to not use a pin (will wait a fixed delay)

// The FeatherWing comes with the 2.13" tri-color EPD (IL0373, 212x104)
//Adafruit_IL0373 epd(212, 104 ,EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);
Adafruit_IL0398 epd(300, 400, EPD_DC, EPD_RESET, EPD_CS, SRAM_CS, EPD_BUSY);

// The original test app changes at 15 second intervals, but the docs have a warning
// not to refresh the display more often than every 3 minutes, so I added a new constant.
// Do not update more than once every 180 seconds or you may permanently damage the display
// https://learn.adafruit.com/adafruit-eink-display-breakouts/usage-expectations
const int REDRAW_DELAY_SEC = 180;

bool drawBitmap = false;

// Primary SPI with DMA
SdFat SD;

bool bmpDraw(const char *filename, int16_t x, int16_t y); // Forward declaration
uint16_t read16(File &f);
uint32_t read32(File &f);
void printDirectory(File dir, int numTabs); // Forward declaration

void setup() {
  Serial.begin(115200);
  //while (!Serial);
  Serial.println("2.13 inch Tri-Color EInk Featherwing test");

  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("failed!");
  } else {
    drawBitmap = true;
  }

  File root = SD.open("/");
  printDirectory(root, 0);
  Serial.println("done!");

  Serial.println("begin");
  epd.begin();
  epd.setRotation(1);
  epd.setTextWrap(true);
  epd.setTextSize(1);
}

void loop() {
  // Draw some pretty lines
  epd.clearBuffer();
  for (int16_t i=0; i<epd.width(); i+=4) {
    epd.drawLine(0, 0, i, epd.height()-1, EPD_BLACK);
  }
  for (int16_t i=0; i<epd.height(); i+=4) {
    epd.drawLine(epd.width()-1, 0, 0, i, EPD_RED);
  }
  epd.display();

  delay(REDRAW_DELAY_SEC*1000);

  // Draw some text
  epd.clearBuffer();
  epd.setCursor(10, 10);
  epd.setTextColor(EPD_BLACK);
  epd.print("Get as much education as you can. Nobody can take that away from you");
  epd.setCursor(50, 70);
  epd.setTextColor(EPD_RED);
  epd.print("--Eben Upton");
  epd.display();

  delay(REDRAW_DELAY_SEC*1000);

  // Draw a bitmap
  if (drawBitmap) {
    epd.clearBuffer();
    epd.fillScreen(EPD_WHITE);
    bmpDraw("/blinka.bmp",0,0);

    delay(REDRAW_DELAY_SEC*1000);
  }


}


// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance.

#define BUFFPIXEL 20

bool bmpDraw(const char *filename, int16_t x, int16_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col, x2, y2, bx1, by1;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if((x >= epd.width()) || (y >= epd.height())) return false;

  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print(F("File not found"));
    return false;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        x2 = x + bmpWidth  - 1; // Lower-right corner
        y2 = y + bmpHeight - 1;
        if((x2 >= 0) && (y2 >= 0)) { // On screen?
          w = bmpWidth; // Width/height of section to load/display
          h = bmpHeight;
          bx1 = by1 = 0; // UL coordinate in BMP file

          for (row=0; row<h; row++) { // For each scanline...

            // Seek to start of scan line.  It might seem labor-
            // intensive to be doing this on every line, but this
            // method covers a lot of gritty details like cropping
            // and scanline padding.  Also, the seek only takes
            // place if the file position actually needs to change
            // (avoids a lot of cluster math in SD library).
            if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
              pos = bmpImageoffset + (bmpHeight - 1 - (row + by1)) * rowSize;
            else     // Bitmap is stored top-to-bottom
              pos = bmpImageoffset + (row + by1) * rowSize;
            pos += bx1 * 3; // Factor in starting column (bx1)
            if(bmpFile.position() != pos) { // Need seek?
              bmpFile.seek(pos);
              buffidx = sizeof(sdbuffer); // Force buffer reload
            }
            for (col=0; col<w; col++) { // For each pixel...
              // Time to read more pixel data?
              if (buffidx >= sizeof(sdbuffer)) { // Indeed
                bmpFile.read(sdbuffer, sizeof(sdbuffer));
                buffidx = 0; // Set index to beginning
              }
              // Convert pixel from BMP to EPD format, push to display
              b = sdbuffer[buffidx++];
              g = sdbuffer[buffidx++];
              r = sdbuffer[buffidx++];

              uint8_t c;
              if ((r < 0x80) && (g < 0x80) && (b < 0x80)) {
                 c = EPD_BLACK; // try to infer black
              } else if ((r >= 0x80) && (g >= 0x80) && (b >= 0x80)) {
                 c = EPD_WHITE;
              } else if (r >= 0x80) {
                c = EPD_RED; //try to infer red color
              }

              epd.writePixel(col, row, c);
            } // end pixel
          } // end scanline
        } // end onscreen
        epd.display();
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) {
    Serial.println(F("BMP format not recognized."));
    return false;
  }
  return true;
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}


void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}