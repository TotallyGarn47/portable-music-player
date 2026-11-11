#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <U8g2lib.h>

#include <AudioFileSourceSD.h>
#include <AudioFileSourceBuffer.h>
#include <AudioOutputI2S.h>

#include <AudioGeneratorFLAC.h>
#include <AudioGeneratorWAV.h>
#include <AudioGeneratorMP3.h>

#define SD_CLK 2
#define SD_SI 4
#define SD_SO 3
#define SD_CS 5

#define I2C_SDA 0
#define I2C_SCL 1

<<<<<<< HEAD
// SSD1306 128x64, full buffer, hardware I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
=======
// -------- Player variable setup --------
// All (non-hidden) files in the specified folder, using SPI
// ext = "" means "don't filter by extension".
AudioSourceSD source("/Speciella vibbar", "", SD_CS, SPI);
>>>>>>> 237381cc89ea421d690572befec5b6cd3746d6e1

AudioFileSourceSD *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;

AudioGeneratorFLAC *flac;
AudioGeneratorMP3 *mp3;
AudioGeneratorWAV *wav;

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

  file = new AudioFileSourceSD("/Dina Ögon - Berget (SPOTISAVER).mp3");
  // file = new AudioFileSourceSD("/Berget - Dina Ögon.flac");
  buff = new AudioFileSourceBuffer(file, 65536);  // 128 KB read-ahead

  mp3 = new AudioGeneratorMP3();
  flac = new AudioGeneratorFLAC();
  wav = new AudioGeneratorWAV();

  // if (!mp3->begin(buff, out)) {
  //   show_message("MP3 begin failed!");
  // }
  // if (!flac->begin(buff, out)) {  // pass buff, not file
  //   show_message("FLAC begin failed");
  // }

  if (!wav->begin(buff, out)) {
    show_message("WAV begin failed!");
  }
}

void setup() {
  Serial.begin(115200);

<<<<<<< HEAD
  ini_u8g2();
  ini_SD();
  ini_audioOutput();
}

void loop() {
  // if (mp3->isRunning()) {
  //   if (!mp3->loop()) {
  //     mp3->stop();
  //     show_message("Finished");
  //   }
  // }
  if (wav->isRunning()) {
    if (!wav->loop()) {
      wav->stop();
      show_message("Finished");
=======
  // ---- I2C OLED ----
  u8g2.begin();

  // ---- SD card ----
  SPI.begin(SD_CLK, SD_SO, SD_SI, SD_CS);

  if (!SD.begin(SD_CS, SPI, 40000000)) {   // Mounted here at 40 MHz.
    Serial.println("SD init failed - check wiring / card format (FAT32)");
    while (1)
      delay(1000);
  }

  // ---- I2S output ----
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.buffer_count = 8;
  cfg.buffer_size = 1023;
  cfg.pin_bck = DAC_BCK;
  cfg.pin_ws = DAC_WSEL;
  cfg.pin_data = DAC_DIN;
  cfg.pin_mck = DAC_MCK;
  // sample rate / channels / bits are set automatically from the file's header
  i2s.begin(cfg);

  // ---- Decoders, selected by the file's extension ----
  multi.addDecoder(mp3, "audio/mpeg");
  multi.addDecoder(wav, "audio/vnd.wave");
  multi.addDecoder(flac, "audio/flac");

  // ---- Player ----
  player.setOnStreamChangeCallback(onTrackChange);
  player.setSilenceOnInactive(true);                // keep feeding zeros to the DAC while paused
  player.setVolume(volume); // 0.0 - 1.0
  if (!player.begin()) { // starts with the first file on the card
    Serial.println("Player start failed - no playable files in the root folder?");
    while (1)
      delay(1000);
  }
}

void loop() {
  player.copy();  // does the actual playing, must be called continuously. 
                  // NO EXCEPTIONS, this includes slow functions

  // Serial input commands
  if (Serial.available()) {
    switch (Serial.read()) {
    case 'p':
      player.setActive(true);
      break; // resume
    case 's':
      player.setActive(false);
      break; // pause
    case 'n':
      player.next();
      break; // next track
    case 'b':
      player.previous();
      break; // previous track
    case '+':
      volume = min(1.0f, volume + 0.1f);
      player.setVolume(volume);
      break;
    case '-':
      volume = max(0.0f, volume - 0.1f);
      player.setVolume(volume);
      break;
>>>>>>> 237381cc89ea421d690572befec5b6cd3746d6e1
    }
  }
  // if (flac->isRunning()) {
  //   if (!flac->loop()) {
  //     flac->stop();
  //     show_message("Finished");
  //   }
  // }
}