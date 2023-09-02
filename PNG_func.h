void *myOpen(const char *filename, int32_t *size)
{
  /* Wio Terminal */
  //#if defined(ARDUINO_ARCH_SAMD) && defined(SEEED_GROVE_UI_WIRELESS)
  //  pngFile = SD.open(filename, "r");
  //#elif defined(ARDUINO_RASPBERRY_PI_PICO)
  //  pngFile = LittleFS.open(filename, "r");
  //  // pngFile = SD.open(filename, "r");
  //#elif defined(ESP32)
  //  // pngFile = FFat.open(filename, "r");
  //  pngFile = LittleFS.open(filename, "r");
  //  // pngFile = SPIFFS.open(filename, "r");
  //  // pngFile = SD.open(filename, "r");
  //#elif defined(ESP8266)
  //  pngFile = LittleFS.open(filename, "r");
  //  // pngFile = SD.open(filename, "r");
  //#else
  //  pngFile = SD.open(filename, FILE_READ);
  //#endif

  pngFile = LITTLEFS.open(filename, "r");

  if (!pngFile || pngFile.isDirectory())
  {
    Serial.println(F("ERROR: Failed to open file for reading"));
    gfx->println(F("ERROR: Failed to open file for reading"));
  }
  else
  {
    *size = pngFile.size();
    Serial.printf("Opened '%s', size: %d\n", filename, *size);
  }

  return &pngFile;
}

void myClose(void *handle)
{
  if (pngFile)
    pngFile.close();
}

int32_t myRead(PNGFILE *handle, uint8_t *buffer, int32_t length)
{
  if (!pngFile)
    return 0;
  return pngFile.read(buffer, length);
}

int32_t mySeek(PNGFILE *handle, int32_t position)
{
  if (!pngFile)
    return 0;
  return pngFile.seek(position);
}

// Function to draw pixels to the display
void PNGDraw(PNGDRAW *pDraw)
{
  uint16_t usPixels[320];
  uint8_t usMask[320];

  // Serial.printf("Draw pos = 0,%d. size = %d x 1\n", pDraw->y, pDraw->iWidth);
  png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
  //  png.getAlphaMask(pDraw, usMask, 1);
  //  gfx->draw16bitRGBBitmap(xOffset, yOffset + pDraw->y, usPixels, usMask, pDraw->iWidth, 1);
  gfx->draw16bitRGBBitmap(xOffset, yOffset + pDraw->y, usPixels, pDraw->iWidth, 1);

}

// Draw PNG Image
void DrawPNG(const char* filename, int xImg, int yImg) {
  unsigned long start = millis();
  int rc;
  // PNG_4BPP_FILENAME
  rc = png.open(filename, myOpen, myClose, myRead, mySeek, PNGDraw);
  if (rc == PNG_SUCCESS)
  {
    // random draw position
    int16_t pw = png.getWidth();
    int16_t ph = png.getHeight();
    //    xOffset = random(w) - (pw / 2);
    //    yOffset = random(h) - (ph / 2);
    //carefull, xOffset and yOffset are global variables
    xOffset = xImg;
    yOffset = yImg;

    rc = png.decode(NULL, 0);

    Serial.printf("Draw offset: (%d, %d), time used: %lu\n", xOffset, yOffset, millis() - start);
    Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
    png.close();
  }
  else
  {
    Serial.println("png.open() failed!");
  }
}
// this function is needed in order to print with size 1 using GFX_Arduino library...
void PrintCharTFT(String Str, int Xinit, int Yinit, int ColFont, int ColBack, int SizeFont) {
  gfx->setTextSize(SizeFont);
  unsigned char Data[500];
  int TempNumOne = Str.length();
  for (int a = 0; a <= TempNumOne; a++) //TempNumOne;a++)
  {
    Data[a] = Str[a];
    gfx->drawChar(Xinit + a * 6 * SizeFont, Yinit, Data[a] , ColFont, ColBack);
  }
}

void readFileTFTScreen(fs::FS &fs, const char * path) {
  gfx->fillScreen(BLACK);
  PrintCharTFT("SD MMC File: " + String(path), 0, 0, RED, BLACK, 1);

  fs::File file = fs.open(path);
  if (!file) {
    PrintCharTFT("Failed to open file for reading", 0, 100, WHITE, BLACK, 1);
    return;
  }
  PrintCharTFT("Reading: ", 0, 10, WHITE, BLACK, 1);
  int SizeFont = 1;
  int Xinit = 0;
  int Yinit = 20;
  int x = Xinit;
  int y = Yinit;
  char ch;
  while (file.available()) {
    //    gfx->write(file.read());
    //    PrintCharTFT(String(file.read()), 0, 30, WHITE, BLACK, 1);
    x = x + 6 * SizeFont;

    ch = file.read();
    if (ch == '\n') { //if  new line in file
      x = Xinit;
      y = y + 10; //increase row by 10 px
    }
    else if (ch == '\r') {
      //do nothing
      x = x - 6 * SizeFont;
    }
    else {
      if (x > 220) { //reached end of 240 px wide screen
        x = Xinit;
        y = y + 10; //increase row by 10 px
      }
      gfx->drawChar(x, y, ch, WHITE, BLACK);
    }

    if (y > 290) { //end of screen, create a new clean page after a delay
      PrintCharTFT("END OF PAGE", 0, 300, BLUE, BLACK, 1);
      y = 0;
      delay(3000);
      gfx->fillScreen(BLACK);
    }

  }
  file.close();
}
