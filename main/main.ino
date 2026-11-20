#include <SD.h>
#include <SPI.h>
#include <Wire.h>

#include <AudioFileSourceBuffer.h>
#include <AudioFileSourceSD.h>
#include <AudioOutputI2S.h>
#include <AudioGeneratorMP3.h>

#include "Animation.h"
#include <U8g2lib.h>

#define SD_CLK 2
#define SD_SI 4
#define SD_SO 3
#define SD_CS 5

#define I2C_SDA 0
#define I2C_SCL 1

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

AudioFileSourceSD *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;
AudioGeneratorMP3 *mp3;

File root;

uint32_t songDuration = 0;

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
  Wire.setClock(400000);
  while(!u8g2.begin()) {
    delay(1);
  }
  delay(500);
}

void ini_SD() {
  SPI.setSCK(SD_CLK);
  SPI.setTX(SD_SO);
  SPI.setRX(SD_SI);
  SPI.setCS(SD_CS);

  while(!SD.begin(SD_CS, 25000000)) {
    show_message("SD failed to initialize!");
    delay(1); 
  }

  root = SD.open("/"); // Choose the playlist here
}

void ini_audioOutput() {
  out = new AudioOutputI2S();
  out->SetPinout(7, 8, 6);  // BCK, WSEL (must be BCK+1), DIN
  out->SetBuffers(8, 1024);
  out->SetGain(0.5);
}

// Stulet av gemini, alldeles för komplicerat för mig annars
uint32_t getMp3DurationSeconds(const char *path) {
  File f = SD.open(path);
  if (!f) return 0;

  uint32_t fileSize = f.size();
  uint32_t pos = 0;
  uint8_t buf[12];

  // 1. Skip ID3v2 tag if present
  if (f.read(buf, 10) == 10 && buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3') {
    uint32_t tagSize = ((uint32_t)(buf[6] & 0x7F) << 21) | ((uint32_t)(buf[7] & 0x7F) << 14) | ((uint32_t)(buf[8] & 0x7F) << 7) | (uint32_t)(buf[9] & 0x7F);
    pos = tagSize + 10;
  }

  // 2. Find first valid 4-byte MP3 frame header
  f.seek(pos);
  uint8_t header[4];
  bool found = false;
  uint32_t searchLimit = pos + 4096;

  while (pos < searchLimit && (pos + 4) <= fileSize) {
    if (f.read() == 0xFF) {
      header[0] = 0xFF;
      if (f.read(&header[1], 3) != 3) break;

      uint8_t ver = (header[1] >> 3) & 0x03;
      uint8_t layer = (header[1] >> 1) & 0x03;
      uint8_t brIdx = (header[2] >> 4) & 0x0F;
      uint8_t srIdx = (header[2] >> 2) & 0x03;

      if ((header[1] & 0xE0) == 0xE0 && ver != 1 && layer == 1 && brIdx != 0 && brIdx != 15 && srIdx != 3) {
        found = true;
        break;
      }
      f.seek(pos + 1);
    }
    pos++;
  }

  if (!found) {
    f.close();
    return 0;
  }

  const uint16_t brMpeg1[16] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 };
  const uint16_t brMpeg2[16] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0 };
  const uint16_t srTable[3][3] = {
    { 44100, 48000, 32000 },  // MPEG 1
    { 22050, 24000, 16000 },  // MPEG 2
    { 11025, 12000, 8000 }    // MPEG 2.5
  };

  uint8_t ver = (header[1] >> 3) & 0x03;
  uint8_t brIdx = (header[2] >> 4) & 0x0F;
  uint8_t srIdx = (header[2] >> 2) & 0x03;
  bool isMono = ((header[3] >> 6) & 0x03) == 3;
  bool isMpeg1 = (ver == 3);

  uint32_t bitrate = (isMpeg1 ? brMpeg1[brIdx] : brMpeg2[brIdx]) * 1000UL;
  uint16_t sampleRate = srTable[isMpeg1 ? 0 : (ver == 2 ? 1 : 2)][srIdx];

  // 3. Check for Xing / Info VBR header inside the first frame
  uint8_t xingOffset = isMpeg1 ? (isMono ? 21 : 36) : (isMono ? 13 : 21);
  f.seek(pos + xingOffset);
  if (f.read(buf, 12) == 12) {
    bool isXing = (buf[0] == 'X' && buf[1] == 'i' && buf[2] == 'n' && buf[3] == 'g');
    bool isInfo = (buf[0] == 'I' && buf[1] == 'n' && buf[2] == 'f' && buf[3] == 'o');
    if ((isXing || isInfo) && (buf[7] & 0x01)) {
      uint32_t totalFrames = ((uint32_t)buf[8] << 24) | ((uint32_t)buf[9] << 16) | ((uint32_t)buf[10] << 8) | (uint32_t)buf[11];
      uint16_t samplesPerFrame = isMpeg1 ? 1152 : 576;
      f.close();
      return (uint32_t)(((uint64_t)totalFrames * samplesPerFrame) / sampleRate);
    }
  }

  f.close();

  // 4. Fallback: Constant Bitrate (CBR) estimation
  if (bitrate == 0 || fileSize <= pos) return 0;
  return ((fileSize - pos) * 8UL) / bitrate;
}

String nextMp3Path() {
  for (int pass = 0; pass < 2; pass++) {
    File f;
    while (f = root.openNextFile()) {
      bool isDir = f.isDirectory(); 
      String name = f.name();
      f.close();
      if(isDir || name.startsWith(".")) continue;

      String lower = name;
      lower.toLowerCase();
      if(lower.endsWith(".mp3")) return "/" + name;
    }
    root.rewindDirectory();
  }
  return ""; // Returns nothing if there are no files/mp3 files in the specified directory
}

void stopPlayback() {
  if (mp3) {
    mp3->stop();
    delete mp3;
    mp3 = nullptr;
  }
  if (buff) {
    buff->close();
    delete buff;
    buff = nullptr;
  }
  if (file) {
    delete file;
    file = nullptr;
  }
}

bool playNext() {
  stopPlayback();

  String path = nextMp3Path();
  if(path.length() == 0 ) {
    songDuration = 0;
    return false;
  }

  songDuration = getMp3DurationSeconds(path.c_str());

  file = new AudioFileSourceSD(path.c_str());
  buff = new AudioFileSourceBuffer(file, 32 * 1024); // 32 KB of read ahead, some files require upwards of 48 - 64 KB. 
  mp3 = new AudioGeneratorMP3();

  if(!mp3->begin(buff, out)) {
    songDuration = 0;
    show_message("MP3 begin failed"); 
    stopPlayback(); 
    return false; 
  }

  show_message(path.c_str() + 1);
  return true;
}

// [BEGIN lopaka generated]
void updateAnimations() {
    Animation_frame = millis() / 333 % 28;
}

void drawAnimation_Animation(void) {
    u8g2.setDrawColor(1);
    u8g2.drawXBMP(87, 5, 32, 32, Animation_frames[Animation_frame]);
}

void drawPlaying_screen(const char *song_name, const char *artist) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_timR12_tr);
    u8g2.drawStr(4, 15, "Song name");
    u8g2.setFont(u8g2_font_profont10_tr);
    u8g2.drawStr(4, 29, "Artist");
    u8g2.drawFrame(24, 42, 82, 10);
    u8g2.drawBox(24, 43, 71, 8);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(2, 50, "16:53");
    u8g2.drawStr(108, 49, "16:53");
    drawAnimation_Animation();
}

void setup() {
  ini_SD();
  ini_audioOutput();
  playNext();

  show_message("Initialized!"); 
  delay(500);
}

void loop() {
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      playNext();  // song finished -> next one
    }
  } else if (root && out) {
    delay(2000);  // nothing playing; retry occasionally
    playNext();
  }
}

void setup1() {
  ini_u8g2();
}

void loop1() {
    updateAnimations();
    u8g2.firstPage();
    do {
        drawPlaying_screen("Berget", "Dina ögon");
    } while (u8g2.nextPage());
    delay(900);
}