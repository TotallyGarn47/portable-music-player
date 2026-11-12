#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <U8g2lib.h>

#include <AudioFileSourceSD.h>
#include <AudioFileSourceBuffer.h>
#include <AudioOutputI2S.h>

#include <AudioGeneratorFLAC.h>
#include <AudioGeneratorMP3.h>

#define SD_CLK 2
#define SD_SI 4
#define SD_SO 3
#define SD_CS 5

#define I2C_SDA 0
#define I2C_SCL 1

// SSD1306 128x64, full buffer, hardware I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

AudioFileSourceSD *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;

AudioGeneratorFLAC *flac;
AudioGeneratorMP3 *mp3;

void show_message(const char *text) {
  Serial.println(text);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tr);
  u8g2.drawStr(0, 20, text);
  u8g2.sendBuffer();
}

void ini_u8g2() {
  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);

  if (!u8g2.begin()) {
    Serial.println("u8g2 failed");
    return;
  }
  Serial.println("u8g2 OK");

  delay(500);
}

void ini_SD() {
  SPI.setSCK(SD_CLK);
  SPI.setTX(SD_SO);
  SPI.setRX(SD_SI);
  SPI.setCS(SD_CS);

  if (!SD.begin(SD_CS, 25000000)) {
    show_message("SD failed");
    return;
  }
  show_message("SD OK");
}

void ini_audioOutput() {
  out = new AudioOutputI2S();
  out->SetPinout(7, 8, 6);  // BCK, WSEL, DIN
  out->SetBuffers(8, 1024);
  out->SetGain(1.0);

  // file = new AudioFileSourceSD("/Dina Ögon - Berget (SPOTISAVER).mp3");
  file = new AudioFileSourceSD("/Berget - Dina Ögon.flac");
  buff = new AudioFileSourceBuffer(file, 34048);  // 64 KB read-ahead

  mp3 = new AudioGeneratorMP3();
  flac = new AudioGeneratorFLAC();

  // if (!mp3->begin(buff, out)) {
   // show_message("MP3 begin failed!");
  //}
   if (!flac->begin(buff, out)) {  // pass buff, not file
     show_message("FLAC begin failed");
  }
}

void setup() {
  Serial.begin(115200);

  ini_u8g2();
  ini_SD();
  ini_audioOutput();
}

void loop() {
   //if (mp3->isRunning()) {
    // if (!mp3->loop()) {
     //  mp3->stop();
      // show_message("Finished");
    // }
  // }
  if (flac->isRunning()) {
     if (!flac->loop()) {
       flac->stop();
       show_message("Finished");
     }
   }
}