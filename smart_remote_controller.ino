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

// ⑤赤外線受信（信号受信 or 15秒間を処理）
bool irRecv () {
  // ⑥irRecv関数内で利用する変数（ローカル変数）を定義
  unsigned short irCount = 0;  // HIGH,LOWの信号数
  unsigned long lastt = 0;    // 1つ前の経過時間を保持
  unsigned long deltt = 0;    // 1つ前の経過時間を保持
  unsigned long sMilli;     // 本処理の開始時間
  unsigned long sMicro;     // 処理の開始時間
  unsigned long wMicro;     // 待ち開始時間
  bool rState = 0;        // 赤外線受信モジュールの状態 0:LOW,1:HIGH
  sMilli = millis();      // ⑤現在のシステム時間を取得（ミリ秒で取得）
  // ⑦特定条件(信号受信 or 15秒経過)するまで無限ループ
  while(1) {
    // ⑧Ir受信を待つ開始時間を取得
    wMicro = micros();  // 現在のシステム時間を取得（マイクロ秒で取得）
    // ⑨反転信号の受信待ち
    while (digitalRead(RECV_PIN) == rState) {
      // ⑩待ち始めて0.5秒以上経過したら
      if (micros() - wMicro > 500000) {
        // ⑪待ち始めて0.5秒以上経過したら
        if ( irCount > 10 ) {
          return true;  // ⑫正常に完了
        }
        // ⑬0,1信号が10個以上ない場合は雑音のため再度ゼロから受信
        irCount = 0;
      }
      // ⑭処理が15秒以上経過したらT.O.
      if ( millis() - sMilli > 15000 ) {
        return false; // ⑮15秒経過で終了（受信なし）
      }
    }
    // ⑯信号受信開始時の現在の時間や経過時間を取得
    if ( irCount == 0 ) {
      sMicro = micros();
      lastt = 0;
      irCount++;
      Serial.println("ir:");
    // ⑰信号受信処理開始後の処理（irCountが1以上）
    } else {
      // ⑱赤外線受信部の状態変化が最後に変化した時間からの経過時間を計算
      deltt = ( (micros() - sMicro)/ 10 ) - lastt;
      // ⑲次回経過時間計算のため最後に変化した経過時間を保存
      lastt = lastt + deltt;
      irCount++;
      Serial.print(deltt);
      Serial.print(",");
    }
    // ⑳次回While内で状態変化を検知する値を変更
    rState = !rState;
  }
}
/*
void sendCode(int repeat) {
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
}
*/

int lastButtonState;

void loopIR() {
  if ( irRecv () ) {      // ②赤外線受信処理の実行
    Serial.println();
    Serial.println("RcvOK");  // ③信号を正常に受信した場合に表示
  } else {
    Serial.println();
    Serial.println("NoSig");  // ④30秒間信号がない場合に表示
  }
/*
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
*/
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
