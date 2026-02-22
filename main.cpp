//    Outputs:
//    0.6W on 40/80M
//    0.2W on 20/30M
//
//   Step sizes: 1kHz, 10kHz, 100kHz
//   Twist to go up/down, press encoder to change step size, hold encoder for about 1 second to change band.

#include <Arduino.h>
#include <Wire.h>
#include <si5351.h>
#include <U8g2lib.h>

// Display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// SI5351
Si5351 si5351;

// Encoder pins 
#define ENC_A 7
#define ENC_B 6
#define ENC_SW 5

struct Band
{
  const char *name;
  long minFreq;
  long maxFreq;
  long defaultFreq;
};



Band bands[] = {
    {"80m", 3500000L, 4000000L, 3700000L},
    {"40m", 7000000L, 7200000L, 7000000L},
    {"30m", 10100000L, 10150000L, 10100000L},
    {"20m", 14000000L, 14070000L, 14000000L}};

int currentBand = 0;
long frequency = bands[0].defaultFreq;

const long stepSizes[] = {1000L, 10000L, 100000L}; // 1k, 10k, 100k Hz
int stepIndex = 0;

uint8_t prev_AB = 0x00;
int8_t encoder_acc = 0;


void updateFrequency()
{
  si5351.set_freq((uint64_t)frequency * 100ULL, SI5351_CLK0);
}


void drawDisplay()
{
  u8g2.clearBuffer();

  long mhz = frequency / 1000000;
  long khz = (frequency / 1000) % 1000;

  char bufMHz[4];
  char bufKHz[4];
  snprintf(bufMHz, sizeof(bufMHz), "%ld", mhz);
  snprintf(bufKHz, sizeof(bufKHz), "%03ld", khz);

  // Top line showing band in MHz
  u8g2.setFont(u8g2_font_profont22_tf);
  int x = (128 - u8g2.getStrWidth(bufMHz)) / 2;
  int y = 40;
  u8g2.drawStr(x, y, bufMHz);

  // Bottom line showing kHz
  u8g2.setFont(u8g2_font_profont22_tf);
  x = (128 - u8g2.getStrWidth(bufKHz)) / 2;
  y += 19;
  u8g2.drawStr(x, y, bufKHz);

  // Step size info
  u8g2.setFont(u8g2_font_5x7_tf);
  char stepBuf[10];
  snprintf(stepBuf, sizeof(stepBuf), "%ld", stepSizes[stepIndex]);
  u8g2.drawStr(128 - u8g2.getStrWidth(stepBuf) - 12, 30, stepBuf);

  u8g2.drawStr(2, 60, bands[currentBand].name);

  u8g2.sendBuffer();
}


void setup()
{
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);

  Wire.begin(8, 9);

  u8g2.begin();
  u8g2.setContrast(100);

  if (!si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000UL, +75000))
  {
    while (true)
      delay(100);
  }

  si5351.output_enable(SI5351_CLK0, 1);
  si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA);

  Wire.beginTransmission(SI5351_BUS_BASE_ADDR);
  Wire.write(149);
  Wire.write(0x00);
  Wire.endTransmission();

  Wire.beginTransmission(SI5351_BUS_BASE_ADDR);
  Wire.write(177);
  Wire.write(0xA0);
  Wire.endTransmission();

  si5351.set_ms_source(SI5351_CLK0, SI5351_PLLA);

  updateFrequency();
  drawDisplay();
}


void loop()
{
  uint8_t AB = 0;
  if (digitalRead(ENC_A))
    AB |= 0x02;
  if (digitalRead(ENC_B))
    AB |= 0x01;

  if (AB != prev_AB)
  {
    uint8_t transition = (prev_AB << 2) | AB;
    switch (transition)
    {
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1010:
      encoder_acc++;
      break;
    case 0b0010:
    case 0b1011:
    case 0b1101:
    case 0b0100:
      encoder_acc--;
      break;
    default:
      break;
    }
    prev_AB = AB;

    if (encoder_acc >= 4 || encoder_acc <= -4)
    {
      long delta = (encoder_acc > 0) ? stepSizes[stepIndex] : -stepSizes[stepIndex];
      frequency += delta;

      
      if (frequency < bands[currentBand].minFreq)
        frequency = bands[currentBand].minFreq;
      if (frequency > bands[currentBand].maxFreq)
        frequency = bands[currentBand].maxFreq;

      updateFrequency();
      drawDisplay();
      encoder_acc = 0;
    }
  }


  static unsigned long pressStart = 0;
  static bool buttonHeld = false;

  if (digitalRead(ENC_SW) == LOW)
  {
    if (!buttonHeld)
    {
      pressStart = millis();
      buttonHeld = true;
    }


    if (millis() - pressStart > 800)
    {
      currentBand = (currentBand + 1) % (sizeof(bands) / sizeof(Band));
      frequency = bands[currentBand].defaultFreq;

      updateFrequency();
      drawDisplay();

      while (digitalRead(ENC_SW) == LOW)
        delay(10);

      buttonHeld = false;
    }
  }
  else
  {
    if (buttonHeld)
    {
      if (millis() - pressStart < 800)
      {
        stepIndex = (stepIndex + 1) % 3;
        drawDisplay();
      }
    }
    buttonHeld = false;
  }
}
