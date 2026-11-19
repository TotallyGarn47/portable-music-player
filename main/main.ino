#include <SD.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>

#include <AudioFileSourceBuffer.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

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

unsigned int currently_playing = 0; // Index of the song currently playing
const char *current_playlist = "test.csv";

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

bool ini_SD() {
  SPI.setSCK(SD_CLK);
  SPI.setTX(SD_SO);
  SPI.setRX(SD_SI);
  SPI.setCS(SD_CS);

  if (!SD.begin(SD_CS, 25000000)) {
    show_message("SD failed");
    return false;
  }
  show_message("SD OK");

  root = SD.open("/"); // Choose playlist here
  return true;
}

void ini_audioOutput() {
  out = new AudioOutputI2S();
  out->SetPinout(7, 8, 6); // BCK, WSEL (must be BCK+1), DIN
  out->SetBuffers(8, 1024);
  out->SetGain(0.5);
}

// Returns the path of the next .mp3 in the root dir
String nextMp3Path() {
  String fetched_string = getCSVValue(current_playlist, currently_playing * 2 + 1, 0);
  int lastSlash = fetched_string.lastIndexOf('\\');
  return (lastSlash != -1) ? "/" + fetched_string.substring(lastSlash + 1) : fetched_string;
}

String getCSVValue(const char* filename, int targetRow, int targetCol) {
  File file = SD.open(filename);
  if (!file) {
    return "";
  }

  int currentRow = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');

    if (currentRow == targetRow) {
      file.close();
      return parseColumn(line, targetCol);
    }
    currentRow++;
  }

  file.close();
  return "";
}

String parseColumn(String data, int targetCol) {
  int currentCol = 0;
  int startIndex = 0;
  int commaIndex = data.indexOf(',');

  while (commaIndex != -1) {
    if (currentCol == targetCol) {
      return data.substring(startIndex, commaIndex);
    }
    startIndex = commaIndex + 1;
    commaIndex = data.indexOf(',', startIndex);
    currentCol++;
  }

  if (currentCol == targetCol) {
    return data.substring(startIndex);
  }
  return "";
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
  if (path.length() == 0) {
    show_message("No MP3 files");
    return false;
  }

  file = new AudioFileSourceSD(path.c_str());
  buff = new AudioFileSourceBuffer(file, 32 * 1024); // 32 KB read-ahead
  mp3 = new AudioGeneratorMP3();
  currently_playing++;

  if (!mp3->begin(buff, out)) {
    show_message("MP3 begin failed!");
    stopPlayback();
    return false;
  }

  show_message(path.c_str() + 1); // skip leading '/'
  return true;
}

void setup() {
  Serial.begin(115200);

  ini_u8g2();
  if (!ini_SD())
    return;
  ini_audioOutput();
  playNext();
}

void loop() {
  if (mp3 && mp3->isRunning()) {
    if (!mp3->loop()) {
      playNext(); // song finished -> next one
    }
  } else if (root && out) {
    delay(2000); // nothing playing; retry occasionally
    playNext();
  }
}

void loop1() {
  delay(1000);
  const char* str = nextMp3Path().c_str();
  show_message(str);
}