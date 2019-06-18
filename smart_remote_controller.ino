// #include <IRremote.h>
#include "ESP32_BME280_I2C.h"
#include <Wire.h>
#include "SSD1306Wire.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define TEMPERATURE_MAX 50
#define TEMPERATURE_MIN 0

#define SERIAL_BAUD 115200

int RECV_PIN = 5;
int BUTTON_PIN = 17;
int STATUS_PIN = 16;
int SEND_PIN = 4;

//IRrecv irrecv(RECV_PIN);
//IRsend irsend(SEND_PIN);
//decode_results results;
ESP32_BME280_I2C bme280i2c(0x76, /*scl*/14, /*sda*/27, 30000);

SSD1306Wire  display(0x3c, 27, 14);

void setup()
{
  Serial.begin(SERIAL_BAUD);
  while(!Serial) {}

  // init IR
  // irrecv.enableIRIn(); // Start the receiver
  pinMode(BUTTON_PIN, INPUT);
  pinMode(STATUS_PIN, OUTPUT);

  // init display
  display.init();
  display.flipScreenVertically();
  
  // init BME280
  uint8_t t_sb = 0; //stanby 0.5ms
  uint8_t filter = 4; //IIR filter = 16
  uint8_t osrs_t = 2; //OverSampling Temperature x2
  uint8_t osrs_p = 5; //OverSampling Pressure x16
  uint8_t osrs_h = 1; //OverSampling Humidity x1
  uint8_t Mode = 3; //Normal mode
  bme280i2c.ESP32_BME280_I2C_Init(t_sb, filter, osrs_t, osrs_p, osrs_h, Mode);
  delay(1000);  
}

// Storage for the recorded code
int codeType = -1; // The type of code
unsigned long codeValue; // The code value if not raw
// unsigned int rawCodes[RAWBUF]; // The durations if raw
int codeLen; // The length of the code
int toggle = 0; // The RC5/6 toggle state

// Stores the code for later playback
// Most of this code is just logging
/*
void storeCode(decode_results *results) {

  codeType = results->decode_type;
  //int count = results->rawlen;
  if (codeType == UNKNOWN) {
    Serial.println("Received unknown code, saving as raw");
    codeLen = results->rawlen - 1;
    // To store raw codes:
    // Drop first value (gap)
    // Convert from ticks to microseconds
    // Tweak marks shorter, and spaces longer to cancel out IR receiver distortion
    for (int i = 1; i <= codeLen; i++) {
      if (i % 2) {
        // Mark
        rawCodes[i - 1] = results->rawbuf[i]*USECPERTICK - MARK_EXCESS;
        Serial.print(" m");
      } 
      else {
        // Space
        rawCodes[i - 1] = results->rawbuf[i]*USECPERTICK + MARK_EXCESS;
        Serial.print(" s");
      }
      Serial.print(rawCodes[i - 1], DEC);
    }
    Serial.println("");
  }
  else {
    if (codeType == NEC) {
      Serial.print("Received NEC: ");
      if (results->value == REPEAT) {
        // Don't record a NEC repeat value as that's useless.
        Serial.println("repeat; ignoring.");
        return;
      }
    } 
    else if (codeType == SONY) {
      Serial.print("Received SONY: ");
    } 
    else if (codeType == PANASONIC) {
      Serial.print("Received PANASONIC: ");
    }
    else if (codeType == JVC) {
      Serial.print("Received JVC: ");
    }
    else if (codeType == RC5) {
      Serial.print("Received RC5: ");
    } 
    else if (codeType == RC6) {
      Serial.print("Received RC6: ");
    }
    else {
      Serial.print("Unexpected codeType ");
      Serial.print(codeType, DEC);
      Serial.println("");
    }
    Serial.println(results->value, HEX);
    codeValue = results->value;
    codeLen = results->bits;
  }

}
*/
void sendCode(int repeat) {
/*
  if (codeType == NEC) {
    if (repeat) {
      irsend.sendNEC(REPEAT, codeLen);
      Serial.println("Sent NEC repeat");
    } 
    else {
      irsend.sendNEC(codeValue, codeLen);
      Serial.print("Sent NEC ");
      Serial.println(codeValue, HEX);
    }
  } 
  else if (codeType == SONY) {
    irsend.sendSony(codeValue, codeLen);
    Serial.print("Sent Sony ");
    Serial.println(codeValue, HEX);
  } 
  else if (codeType == PANASONIC) {
    irsend.sendPanasonic(codeValue, codeLen);
    Serial.print("Sent Panasonic");
    Serial.println(codeValue, HEX);
  }
  else if (codeType == JVC) {
    irsend.sendJVC(codeValue, codeLen, false);
    Serial.print("Sent JVC");
    Serial.println(codeValue, HEX);
  }
  else if (codeType == RC5 || codeType == RC6) {
    if (!repeat) {
      // Flip the toggle bit for a new button press
      toggle = 1 - toggle;
    }
    // Put the toggle bit into the code to send
    codeValue = codeValue & ~(1 << (codeLen - 1));
    codeValue = codeValue | (toggle << (codeLen - 1));
    if (codeType == RC5) {
      Serial.print("Sent RC5 ");
      Serial.println(codeValue, HEX);
      irsend.sendRC5(codeValue, codeLen);
    } 
    else {
      irsend.sendRC6(codeValue, codeLen);
      Serial.print("Sent RC6 ");
      Serial.println(codeValue, HEX);
    }
  } 
  else if (codeType == UNKNOWN) {
    // Assume 38 KHz
    irsend.sendRaw(rawCodes, codeLen, 38);
    Serial.println("Sent raw");
  }
  */
}

int lastButtonState;

void loopIR() {
  // If button pressed, send the code.
  int buttonState = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && buttonState == LOW) {
    Serial.println("Released");
    // irrecv.enableIRIn(); // Re-enable receiver
  }

  if (buttonState) {
    Serial.println("Pressed, sending");
    digitalWrite(STATUS_PIN, HIGH);
    // sendCode(lastButtonState == buttonState);
    digitalWrite(STATUS_PIN, LOW);
    delay(50); // Wait a bit between retransmissions
  } 
/*
  else if (irrecv.decode(&results)) {
    digitalWrite(STATUS_PIN, HIGH);
    // storeCode(&results);
    // irrecv.resume(); // resume receiver
    digitalWrite(STATUS_PIN, LOW);
  }
*/  lastButtonState = buttonState;
}

double temperatures[SCREEN_WIDTH] = {0};
int currentIndex = 0;

int counter = 0;
void loopBME() {
  // FIXME
  counter += 1;
  if (counter < 1000000) {
    return;
  }
  counter = 0;

  double temperature, pressure, humidity;
  bme280i2c.Read_All(&temperature, &pressure, &humidity);

  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.print("C");
  Serial.print(" Humidity: ");
  Serial.print(humidity);
  Serial.print("%");
  Serial.print(" tPressure: ");
  Serial.print(pressure);
  Serial.println("hPa");

  temperatures[currentIndex] = temperature;
  currentIndex = (currentIndex + 1) % SCREEN_WIDTH;

  display.clear();
  display.setColor(WHITE);
  for (int i = 0; i < SCREEN_WIDTH; i++) {
    int idx = currentIndex - i;
    if (idx < 0) idx += SCREEN_WIDTH;
    double m = (double) SCREEN_HEIGHT / TEMPERATURE_MAX;
    double t = temperatures[idx] * m;
    display.setPixel(SCREEN_WIDTH-i, SCREEN_HEIGHT - t);
  }
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0,  "Temp    :" + String(temperature) + "C");
  //display.drawString(0, 20, "Humidity:" + String(humidity) + "%");
  //display.drawString(0, 40, "Pressure:" + String(pressure) + "hPa");
  display.display();
}

void loop() {
  loopIR();
  loopBME();
}
