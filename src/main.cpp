#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h>       // Hardware-specific library
#include "esp_adc_cal.h"
#include <INA219_WE.h>

#define PROJECT_NAME "PUI-Gauge"

#define ADC_EN              14  //ADC_EN is the ADC detection enable port
#define ADC_PIN             34

#define I2C_ADDRESS 0x40

#define COLOR_GREY 0xBDF7
#define COLOR_BACKGROUND 0x1082 // 0x2104  // 0x8410    // TFT_DARKGREY
#define BATTERY_POS_Y 210
#define BATTERY_HIGH  26
#define BATTERY_WIDTH 36

#define VOLTAGE_POS_Y 30
#define CURRENT_POS_Y 90
#define POWER_POS_Y   150
#define GAUGE_HIGH    55
#define GAUGE_WIDTH   TFT_WIDTH
#define GAUGE_FRAME_W 3

TFT_eSPI tft = TFT_eSPI();  // Invoke custom library
INA219_WE ina219 = INA219_WE(I2C_ADDRESS);

boolean initial = 1;
uint32_t targetTime = 0;
int vref = 1100;

void showBattery()
{
    static uint64_t timeStamp = 0;
    if (millis() - timeStamp > 1000) {
        timeStamp = millis();
        digitalWrite(ADC_EN, 1);
        delay(100);
        uint16_t v = analogRead(ADC_PIN);
        float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
        // printf("Voltage : %f V\n", battery_voltage);

        //tft.drawRoundRect(5, 70, TFT_WIDTH - 10, 32, 2, TFT_BROWN);
        tft.fillRoundRect(10, BATTERY_POS_Y + 3, TFT_WIDTH - 20, BATTERY_HIGH - 6, 3, COLOR_BACKGROUND);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(TFT_SKYBLUE, COLOR_BACKGROUND);  // Adding a black background colour erases previous text automatically
        tft.drawFloat(battery_voltage, 2, TFT_WIDTH / 2 , BATTERY_POS_Y + 5, 2);
        tft.drawString("V", TFT_WIDTH / 2 + 35, BATTERY_POS_Y + 5, 2);

        //tft.fillScreen(TFT_BLACK);
        //tft.setTextDatum(MC_DATUM);
        //tft.drawString(voltage,  tft.width() / 2, tft.height() / 2 );

        digitalWrite(ADC_EN, 0);
    }
}

void showGauge(const char* text, uint32_t pos_y, uint16_t color) {
  tft.fillRoundRect(0, pos_y, GAUGE_WIDTH, GAUGE_HIGH, 4, color);
  tft.fillRoundRect(GAUGE_FRAME_W, pos_y + GAUGE_FRAME_W, \
        GAUGE_WIDTH - 2 * GAUGE_FRAME_W, GAUGE_HIGH - 2 * GAUGE_FRAME_W, 3, COLOR_BACKGROUND);
  tft.setTextColor(color, COLOR_BACKGROUND);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(text, 10, pos_y + 5, 2);
}

void updateGauge(float value, int32_t pos_y, uint16_t color) {
  tft.fillRect(GAUGE_FRAME_W, pos_y + GAUGE_FRAME_W + 18, \
        GAUGE_WIDTH - 2 * GAUGE_FRAME_W, GAUGE_HIGH - 2 * GAUGE_FRAME_W - 20, COLOR_BACKGROUND);
  tft.setTextColor(color, COLOR_BACKGROUND);
  tft.setTextDatum(TC_DATUM);
  tft.drawFloat(value, 3, TFT_WIDTH / 2 , pos_y + 25, 4);
}

void setup(void) {
  // Ein-/Ausgänge initialisieren
  pinMode(ADC_EN, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, 1);

  Wire.begin();

  printf("===============================================================\n");
  printf(PROJECT_NAME);
  printf("\n\n");

  ESP_LOGI(TAG, PROJECT_NAME);

  printf("Run on Core   : %d @ %d MHz\n", xPortGetCoreID(), ESP.getCpuFreqMHz());
  printf("Flash size    : %d\n", ESP.getFlashChipSize());
  printf("Sketch size   : %d ; used : %d\n", ESP.getFreeSketchSpace(), ESP.getSketchSize());
  printf("RAM size      : %d ; free : %d\n", ESP.getHeapSize(), ESP.getFreeHeap());
  if (psramFound()) {
    printf("PSRAM         : %d \n", ESP.getPsramSize());
  } else {
    printf("PSRAM not available\n");
  }

  if(ina219.init()){
    printf("INA219 connected : OK\n");
  } else {
    printf("INA219 not connected!\n");
  }


  // ADC Calibrierung
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);    //Check type of calibration value used to characterize ADC
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    printf("eFuse Vref:%u mV\n", adc_chars.vref);
    vref = adc_chars.vref;
  } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
    printf("Two Point --> coeff_a:%umV coeff_b:%umV\n", adc_chars.coeff_a, adc_chars.coeff_b);
  } else {
    printf("Default Vref: 1100mV\n");
  }

  // Display initialisieren
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Adding a black background colour erases previous text automatically
  
  // Only font numbers 2,4,6,7 are valid. Font 6 only contains characters [space] 0 1 2 3 4 5 6 7 8 9 : . a p m
  // Font 7 is a 7 segment font and only contains characters [space] 0 1 2 3 4 5 6 7 8 9 : .
  tft.drawCentreString(PROJECT_NAME, TFT_WIDTH / 2, 1, 4);

  /*
  tft.setTextColor(TFT_DARKGREEN, TFT_BLACK);  // Adding a black background colour erases previous text automatically
  tft.setCursor(1,25);
  tft.printf("Core       : %d MHz\n", ESP.getCpuFreqMHz());
  tft.setCursor(1,35);
  tft.printf("Flash size : %d MB\n", ESP.getFlashChipSize() >> 20);
  tft.setCursor(1,45);
  tft.printf("RAM free   : %d kB\n", ESP.getFreeHeap() >> 10);
  tft.setCursor(1,55);
  if (psramFound()) {
    tft.printf("PSRAM      : %d MB\n", ESP.getPsramSize() >> 20);
  } else {
    tft.printf("PSRAM not available\n");
  }
  */

  // Settings for INA219 Sensor
  ina219.setMeasureMode(TRIGGERED);
  ina219.setADCMode(SAMPLE_MODE_128);
  ina219.setPGain(PG_160);
  ina219.setBusRange(BRNG_16);
  ina219.setCorrectionFactor(0.5);

  // Hintergrund für Batteriespannung zeichnen
  tft.fillRoundRect(7, BATTERY_POS_Y, TFT_WIDTH - 14, BATTERY_HIGH, 3, TFT_SKYBLUE);
  tft.fillRoundRect(10, BATTERY_POS_Y + 3, TFT_WIDTH - 20, BATTERY_HIGH - 6, 3, COLOR_BACKGROUND);

  showGauge("Voltage (V)", VOLTAGE_POS_Y, TFT_GREENYELLOW);
  showGauge("Current (mA)", CURRENT_POS_Y, TFT_ORANGE);
  showGauge("Power (mW)", POWER_POS_Y, TFT_PINK);
  // updateGauge(2.34, VOLTAGE_POS_Y, TFT_GREENYELLOW);

  // Display Refresh Zeit initialisieren
  targetTime = millis() + 1000;
}

void loop() {
  if (targetTime < millis()) {
    targetTime = millis()+1000;

    showBattery();

    // float shuntVoltage_mV = 0.0;
    float busVoltage_V = 0.0;
    float current_mA = 0.0;
    float power_mW = 0.0; 
    bool ina219_overflow = false;

    ina219.startSingleMeasurement(); // triggers single-shot measurement and waits until completed
    // shuntVoltage_mV = ina219.getShuntVoltage_mV();
    busVoltage_V = ina219.getBusVoltage_V();
    current_mA = ina219.getCurrent_mA();
    power_mW = ina219.getBusPower();
    ina219_overflow = ina219.getOverflow();

    updateGauge(busVoltage_V, VOLTAGE_POS_Y, TFT_GREENYELLOW);
    updateGauge(current_mA, CURRENT_POS_Y, TFT_ORANGE);
    updateGauge(power_mW, POWER_POS_Y, TFT_PINK);
    // updateGauge(shuntVoltage_mV, POWER_POS_Y, TFT_PINK);

    // printf("Shunt Voltage [mV]: %f\n", shuntVoltage_mV);
    // printf("Bus Voltage [V]: %f\n", busVoltage_V);
    // printf("Current[mA]: %f\n", current_mA);
    // printf("Bus Power [mW]: %f\n", power_mW);
    if(!ina219_overflow){
      // printf("Values OK - no overflow\n");
    } else {
      printf("Overflow! Choose higher PGAIN\n");
    }
  }
}
