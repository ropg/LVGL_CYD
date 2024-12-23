#include <LVGL_CYD.h>

#include <spi_lcd_read.h>

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// display
#define LCD_DC    2  // Data / Command
#define LCD_SCK  14  // SPI clock
#define LCD_MISO 12  // SPI MISO
#define LCD_MOSI 13  // SPI MOSI
#define LCD_CS   15  // LCD chip select

// capacitive touch
#include <Wire.h>
#define I2C_SDA         33
#define I2C_SCL         32
#define TP_RST          25
#define TP_INT          21
#define I2C_ADDR_CST820 0x15

// resistive touch
#include <SPI.h>
#define XPT2046_IRQ     36
#define XPT2046_MOSI    32
#define XPT2046_MISO    39
#define XPT2046_CLK     25
#define XPT2046_CS      33
#define Z_THRESHOLD     300
#define SPI_SETTING     SPISettings(2000000, MSBFIRST, SPI_MODE0)

// backlight
#define BACKLIGHT_CAPACITIVE  27
#define BACKLIGHT_RESISTIVE   21

// RGB-LED
#define LED_R         4
#define R_CORRECTION  0.25
#define LED_G         16
#define G_CORRECTION  1
#define LED_B         17
#define B_CORRECTION  0.4
#define LED_FREQ      5000
#define LED_RES       8

// Light sensor
#define LDR        34

SPIClass touchSPI = SPIClass(VSPI);

// spi_lcd_read
SPIClass displaySPI = SPIClass(HSPI);
SPI_LCD_READ slr;

bool LVGL_CYD::capacitive;

bool LVGL_CYD::resistive;

int16_t LVGL_CYD::pressure;

TFT_eSPI * LVGL_CYD::tft;

static void touch_read(lv_indev_t * indev, lv_indev_data_t * data);

static int16_t besttwoavg(int16_t a, int16_t b, int16_t c);

void LVGL_CYD::begin(lv_display_rotation_t rotation) {

  Serial.begin(115200);

  Serial.printf("LVGL_CYD version: %s\n", LVGL_CYD_VERSION);
  Serial.printf("TFT_eSPI version: %s\n", TFT_ESPI_VERSION);
  Serial.printf("LVGL version: %i.%i.%i\n", lv_version_major(), lv_version_minor(), lv_version_patch());

  // detect capacitive touch chip (when something answers at its I2C address)
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.beginTransmission(I2C_ADDR_CST820);
  LVGL_CYD::capacitive = Wire.endTransmission() == 0;
  Wire.end();
  if (LVGL_CYD::capacitive) {
    Serial.println("Capacitive touch detected");
    pinMode(BACKLIGHT_CAPACITIVE, OUTPUT);
  } else {
    // detect resistive touch chip (when something pulls up the IRQ)
    pinMode(XPT2046_IRQ, INPUT_PULLDOWN);
    LVGL_CYD::resistive = digitalRead(XPT2046_IRQ);
    pinMode(XPT2046_IRQ, INPUT);
    if (LVGL_CYD::resistive) {
      Serial.println("Resistive touch detected");
      pinMode(XPT2046_CS, OUTPUT);
      digitalWrite(XPT2046_CS, HIGH);
      touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
      pinMode(BACKLIGHT_RESISTIVE, OUTPUT);
    } else {
      Serial.println("No touch screen detected");
    }
  }

  // display detection
  bool ili9341;
  slr.begin(&displaySPI, LCD_SCK, LCD_MISO, LCD_MOSI, LCD_CS, LCD_DC);
  // see if it's an ILI9341 or not
  uint8_t ID4[4];
  slr.readBufferIndex(0xD3, ID4, 4);
  if (ID4[2] == 0x93 && ID4[3] == 0x41) {
    ili9341 = true;
    Serial.println("ILI9341 detected");
  } else {
    ili9341 = false;
    Serial.println("ST7789 assumed (needs invert command)");
  }
  slr.end();


  // start LVGL
  lv_init();

  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  // initialize display using TFT_eSPI library

  lv_disp_t * display = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(display, rotation);

  // pointer to a TFT_eSPI object for the screen
  LVGL_CYD::tft = * (TFT_eSPI * *) lv_display_get_driver_data(display);

  if (!ili9341) {
    // ST7789 needs to be inverted
    LVGL_CYD::tft->invertDisplay(true);

    // gamma fix for ST7789
    LVGL_CYD::tft->writecommand(ILI9341_GAMMASET); //Gamma curve selected
    LVGL_CYD::tft->writedata(2);
    delay(120);
    LVGL_CYD::tft->writecommand(ILI9341_GAMMASET); //Gamma curve selected
    LVGL_CYD::tft->writedata(1);
  }

  // if there's a touch screen, set up corresponding LVGL input device
  if (LVGL_CYD::capacitive || LVGL_CYD::resistive) {
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    // Set the callback function to read Touchscreen input
    lv_indev_set_read_cb(indev, touch_read);
  }

  // backlight to full brightness
  LVGL_CYD::backlight(0xff);

}

static void touch_read(lv_indev_t * indev, lv_indev_data_t * data) {

  int x, y;

  lv_disp_t * display = lv_disp_get_default();

  if (LVGL_CYD::capacitive) {
  
    uint8_t touchdata[5];
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.beginTransmission(I2C_ADDR_CST820);
    Wire.write(0x02);
    Wire.endTransmission(false);
    Wire.requestFrom(I2C_ADDR_CST820, 5);
    for (int i = 0; i < 5; i++) {
      touchdata[i] = Wire.read();
    }
    Wire.end();
    if (touchdata[0] == 0 || touchdata[0] == 0xFF) {
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }
    x = ((touchdata[1] & 0x0f) << 8) | touchdata[2];
    y = ((touchdata[3] & 0x0f) << 8) | touchdata[4];
  
  } else if (LVGL_CYD::resistive) {
    
    int16_t touchdata[6] = {0};
    touchSPI.beginTransaction(SPI_SETTING);
    digitalWrite(XPT2046_CS, LOW);
		touchSPI.transfer(0xB1 /* Z1 */);
		LVGL_CYD::pressure = (touchSPI.transfer16(0xC1) >> 3) + 4095;
		LVGL_CYD::pressure -= touchSPI.transfer16(0x91) >> 3;
		if (LVGL_CYD::pressure >= Z_THRESHOLD) {
			touchSPI.transfer16(0x91 /* X */);  // dummy X measure, 1st is always noisy
			touchdata[0] = touchSPI.transfer16(0xD1 /* Y */) >> 3;
			touchdata[1] = touchSPI.transfer16(0x91 /* X */) >> 3; // make 3 x-y measurements
			touchdata[2] = touchSPI.transfer16(0xD1 /* Y */) >> 3;
			touchdata[3] = touchSPI.transfer16(0x91 /* X */) >> 3;
		}
		touchdata[4] = touchSPI.transfer16(0xD0 /* Y */) >> 3;	// Last Y touch power down
		touchdata[5] = touchSPI.transfer16(0) >> 3;
		digitalWrite(XPT2046_CS, HIGH);
		touchSPI.endTransaction();
    if (LVGL_CYD::pressure < Z_THRESHOLD) {
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }
    // get average of closest two for each axis
  	x = besttwoavg( touchdata[0], touchdata[2], touchdata[4] );
	  y = besttwoavg( touchdata[1], touchdata[3], touchdata[5] );

    // Serial.printf("Raw: x = %i, y = %i, pressure = %i\n", x, y, pressure);

    // correct raw XPT2046 touch values, adjust to size of display
    y = map(y, 200, 3700, 239, 0);
    y = constrain(y, 0, 239);
    x = map(x, 200, 3750, 0, 319);
    x = constrain(x, 0, 319);

    // Swap axes to match the capacitive
    int temp = x;
    x = y;
    y = temp;

  }

  // Correct for orientation.
  // (Mostly done by LVGL, it just has the axes inverted in landscape somehow.)
  lv_display_rotation_t rotation = lv_display_get_rotation(display);
  if (rotation == LV_DISPLAY_ROTATION_90 || rotation == LV_DISPLAY_ROTATION_270) {
    x = SCREEN_WIDTH - x;
    y = SCREEN_HEIGHT - y;
  } 

  data->point.x = x;
  data->point.y = y;
  data->state = LV_INDEV_STATE_PRESSED;

  // Serial.printf("Processed: x=%i, y=%i\n", x, y);

}

// Out of three values, returns average of the two closest together
static int16_t besttwoavg(int16_t a, int16_t b, int16_t c) {
  int16_t dab = abs(a - b);
  int16_t dbc = abs(b - c);
  int16_t dca = abs(c - a);
  if (dab <= dbc && dab <= dca ) return (a + b) / 2;
  else if (dbc <= dab && dbc <= dca) return (b + c) / 2;
  else return (a + c) / 2;
}

void LVGL_CYD::backlight(uint8_t brightness) {
  analogWrite(LVGL_CYD::capacitive ? BACKLIGHT_CAPACITIVE : BACKLIGHT_RESISTIVE, brightness);
}

// prevent brief PWM startup flash when LED is never used.
static bool led_used_r = false;
static bool led_used_g = false;
static bool led_used_b = false;

void LVGL_CYD::led(uint8_t red, uint8_t green, uint8_t blue, bool true_color) {
  // Serial.printf("LED: r=%i, g=%i, b=%i, true=%i\n", red, green, blue, true);
  if (true_color) {
    red   = 255 - (red   * R_CORRECTION);
    green = 255 - (green * G_CORRECTION);
    blue  = 255 - (blue  * B_CORRECTION);
  } else {
    red   = 255 - red;
    green = 255 - green;
    blue  = 255 - blue;
  }
  if (red < 255)   led_used_r = true;
  if (green < 255) led_used_g = true;
  if (blue < 255)  led_used_b = true;
  if (led_used_r) analogWrite(LED_R, red);
  if (led_used_g) analogWrite(LED_G, green);
  if (led_used_b) analogWrite(LED_B, blue);    
}
