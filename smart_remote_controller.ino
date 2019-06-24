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

unsigned short irData[1500] = { 0 };
unsigned int irLength = 0;

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
  pinMode(SEND_PIN, OUTPUT);

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
  irLength = 0;
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
        irLength = 0;
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
      irData[irLength] = deltt;
      irLength++;
      Serial.print(deltt);
      Serial.print(",");
    }
    // ⑳次回While内で状態変化を検知する値を変更
    rState = !rState;
  }
}

// ⑦赤外線送信処理
void irSend () {
  // ⑧ローカル変数定義
  unsigned short irCount = 0; // HIGH,LOWの信号数
  unsigned long l_now = 0;    // 送信開始時間を保持
  unsigned long sndt = 0;     // 送信開始からの経過時間
  // ⑨HIGH,LOWの信号数を計算
  irCount = irLength;
  // ⑩送信開始時間を取得
  l_now = micros();
  // ⑪0,1の信号回数分をFor文でループ
  for (int i = 0; i < irCount; i++) {
    // ⑫送信開始からの信号終了時間を計算
    sndt += irData[i];
    Serial.print(irData[i]);
    Serial.print(",");
    do {
      // ⑬iが偶数なら赤外線ON、奇数ならOFFのまま
      // ⑭キャリア周波数38kHz(約26μSec周期の半分)でON時間で送信
      digitalWrite(SEND_PIN, !(i&1));
      microWait(13);
      // ⑮キャリア周波数38kHz(約26μSec周期の半分)でOFF時間で送信
      digitalWrite(SEND_PIN, 0);
      microWait(13);
    // ⑯送信開始からの信号終了時間が超えるまでループ
    } while (long(l_now + (sndt * 10) - micros()) > 0);
  }
}

// ⑰マイクロ秒単位で待つ
void microWait(signed long waitTime) {
  unsigned long waitStartMicros = micros();
  // ⑱指定されたマイクロ秒が経過するまでWhileでループ処理（待つ）
  while (micros() - waitStartMicros < waitTime) {};
}

int lastButtonState;

void loopIR() {
  if (irLength == 0) {
    Serial.print("receive");
    irRecv();
    return;
  }

  irSend();
  Serial.println("SndOK");
  delay(3000);
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
