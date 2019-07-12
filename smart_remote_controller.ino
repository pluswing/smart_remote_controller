#include <ESP32_BME280_I2C.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include <WiFiClientSecure.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <SPIFFS.h>
#include <Ticker.h>
#include "config.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define TEMPERATURE_MAX 50
#define TEMPERATURE_MIN 0

#define HUMIDITY_MAX 100
#define HUMIDITY_MIN 0

#define PRESSURE_MAX (1013 + 25)
#define PRESSURE_MIM (1013 - 25)

#define SERIAL_BAUD 115200
#define MAX_IR_SAVE_NUM 10

int RECV_PIN = 5;
int BUTTON_PIN = 17;
int STATUS_PIN = 16;
int SEND_PIN = 4;

ESP32_BME280_I2C bme280i2c(0x76, /*scl*/ 14, /*sda*/ 27, 30000);
SSD1306Wire display(0x3c, 27, 14);
AsyncWebServer webServer(80);
Ticker bme280Ticker;

void setup()
{
  setupSerial();
  setupIO();
  setupDisplay();
  setupBME();
  setupWIFI();
  setupWebserver();
  setupTicker();
}

void loop()
{
  loopBME();
}

void setupSerial()
{
  Serial.begin(SERIAL_BAUD);
  while (!Serial)
  {
  }
}

void setupIO()
{
  pinMode(BUTTON_PIN, INPUT);
  pinMode(STATUS_PIN, OUTPUT);
  pinMode(SEND_PIN, OUTPUT);
  SPIFFS.begin();
}

void setupDisplay()
{
  display.init();
  display.flipScreenVertically();
}

void setupBME()
{
  uint8_t t_sb = 0;   //stanby 0.5ms
  uint8_t filter = 4; //IIR filter = 16
  uint8_t osrs_t = 2; //OverSampling Temperature x2
  uint8_t osrs_p = 5; //OverSampling Pressure x16
  uint8_t osrs_h = 1; //OverSampling Humidity x1
  uint8_t Mode = 3;   //Normal mode
  bme280i2c.ESP32_BME280_I2C_Init(t_sb, filter, osrs_t, osrs_p, osrs_h, Mode);

  // BMEの初期化が非同期なので、（たぶん）ちょっと待たないと
  // うまく初期化されない。
  delay(1000);
}

void setupWIFI()
{
  Serial.println("Wi-Fi SetUp");
  WiFi.config(IP_ADDRESS, GATEWAY, SUBNET, DNS);
  WiFi.begin(SSID, SSID_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.print("Wi-Fi Connected! IP address: ");
  Serial.println(WiFi.localIP());
}

void setupWebserver()
{
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String no = request->getParam("n")->value();
    Serial.println("Top" + no);
    request->send(200, "text", "ok");
  });

  webServer.on("/irrecv", HTTP_GET, [](AsyncWebServerRequest *request) {
    delay(500);
    unsigned short ir[1500];
    unsigned int len = 0;

    bool ok = irRecv(ir, &len);

    if (ok)
    {
      DynamicJsonDocument doc(1500);
      JsonArray data = doc.createNestedArray("data");
      for (int i = 0; i < len; i++)
      {
        data.add(ir[i]);
      }

      String jsonString;
      serializeJson(doc, jsonString);
      request->send(200, "application/json", jsonString);
    }
    else
    {
      request->send(500);
    }
  });

  webServer.on("/irsend_in_memory", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("n"))
    {
      request->send(200, "text", "ng");
      return;
    }

    String no = request->getParam("n")->value();
    Serial.println("IR Send IN Memory" + no);

    DynamicJsonDocument doc(1500);
    bool ok = irRead(no.toInt(), doc);

    if (!ok)
    {
      request->send(200, "text", "ng");
      return;
    }
    JsonArray data = doc["data"];
    unsigned short ir[1500];
    for (int i = 0; i < data.size(); i++)
    {
      ir[i] = data[i];
    }
    irSend(ir, data.size());
    request->send(200, "text", "ok");
  });

  AsyncCallbackJsonWebHandler *irsendHandler = new AsyncCallbackJsonWebHandler("/irsend", [](AsyncWebServerRequest *request, JsonVariant &json) {
    Serial.println("irsend");
    JsonObject jsonObj = json.as<JsonObject>();
    JsonArray data = jsonObj["data"];
    unsigned short ir[1500];
    for (int i = 0; i < data.size(); i++)
    {
      ir[i] = data[i];
    }
    irSend(ir, data.size());
    request->send(200, "text", "ok");
  },
                                                                               1500); // maxJsonBufferSize
  irsendHandler->setMethod(HTTP_POST);
  webServer.addHandler(irsendHandler);

  AsyncCallbackJsonWebHandler *irsaveHandler = new AsyncCallbackJsonWebHandler("/irsave", [](AsyncWebServerRequest *request, JsonVariant &json) {
    Serial.println("irsave");
    JsonObject jsonObj = json.as<JsonObject>();
    JsonArray data = jsonObj["data"];
    int no = jsonObj["no"];

    if (no < 0 || no >= MAX_IR_SAVE_NUM)
    {
      request->send(500, "text", "ng");
      return;
    }

    // 保存する
    // {"data":[....], "name": "あいうえお"}
    DynamicJsonDocument doc(1500);
    doc["data"] = jsonObj["data"];
    doc["name"] = jsonObj["name"];
    irSave(no, doc);

    request->send(200, "text", "ok");
  },
                                                                               1500); // maxJsonBufferSize
  irsaveHandler->setMethod(HTTP_POST);
  webServer.addHandler(irsaveHandler);

  webServer.on("/irlist", HTTP_GET, [](AsyncWebServerRequest *request) {
    // {"0": "aaa", 4: "bbbb", ....}
    DynamicJsonDocument doc(1500);
    for (int i = 0; i < MAX_IR_SAVE_NUM; i++)
    {
      DynamicJsonDocument ir(1500);
      if (irRead(i, ir))
      {
        doc[String(i)] = ir["name"];
      }
    }

    String jsonString;
    serializeJson(doc, jsonString);
    request->send(200, "application/json", jsonString);
  });

  AsyncCallbackJsonWebHandler *irrenameHandler = new AsyncCallbackJsonWebHandler("/irrename", [](AsyncWebServerRequest *request, JsonVariant &json) {
    Serial.println("irrename");
    JsonObject jsonObj = json.as<JsonObject>();
    int no = jsonObj["no"];
    String name = jsonObj["name"];

    DynamicJsonDocument ir(1500);
    if (!irRead(no, ir))
    {
      request->send(500, "text", "ng");
      return;
    }
    ir["name"] = name;
    irSave(no, ir);

    request->send(200, "text", "ok");
  },
                                                                                 1500); // maxJsonBufferSize
  irrenameHandler->setMethod(HTTP_POST);
  webServer.addHandler(irrenameHandler);

  webServer.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404);
  });

  webServer.begin();
  Serial.println("Web server started");
}

void setupTicker()
{
  bme280Ticker.attach_ms(1000, setCollectBME280Flag);
}

bool irRecv(unsigned short *irData, unsigned int *irLength)
{
  // ⑥irRecv関数内で利用する変数（ローカル変数）を定義
  unsigned short irCount = 0; // HIGH,LOWの信号数
  unsigned long lastt = 0;    // 1つ前の経過時間を保持
  unsigned long deltt = 0;    // 1つ前の経過時間を保持
  unsigned long sMilli;       // 本処理の開始時間
  unsigned long sMicro;       // 処理の開始時間
  unsigned long wMicro;       // 待ち開始時間
  bool rState = 0;            // 赤外線受信モジュールの状態 0:LOW,1:HIGH
  sMilli = millis();          // ⑤現在のシステム時間を取得（ミリ秒で取得）
  *irLength = 0;
  // ⑦特定条件(信号受信 or 15秒経過)するまで無限ループ
  while (1)
  {
    // ⑧Ir受信を待つ開始時間を取得
    wMicro = micros(); // 現在のシステム時間を取得（マイクロ秒で取得）
    // ⑨反転信号の受信待ち
    while (digitalRead(RECV_PIN) == rState)
    {
      // ⑩待ち始めて0.5秒以上経過したら
      if (micros() - wMicro > 500000)
      {
        // ⑪待ち始めて0.5秒以上経過したら
        if (irCount > 10)
        {
          return true; // ⑫正常に完了
        }
        // ⑬0,1信号が10個以上ない場合は雑音のため再度ゼロから受信
        irCount = 0;
        *irLength = 0;
      }
      // ⑭処理が15秒以上経過したらT.O.
      if (millis() - sMilli > 15000)
      {
        return false; // ⑮15秒経過で終了（受信なし）
      }
    }
    // ⑯信号受信開始時の現在の時間や経過時間を取得
    if (irCount == 0)
    {
      sMicro = micros();
      lastt = 0;
      irCount++;
      Serial.println("ir:");
      // ⑰信号受信処理開始後の処理（irCountが1以上）
    }
    else
    {
      // ⑱赤外線受信部の状態変化が最後に変化した時間からの経過時間を計算
      deltt = ((micros() - sMicro) / 10) - lastt;
      // ⑲次回経過時間計算のため最後に変化した経過時間を保存
      lastt = lastt + deltt;
      irCount++;
      irData[*irLength] = deltt;
      (*irLength)++;
      Serial.print(deltt);
      Serial.print(",");
    }
    // ⑳次回While内で状態変化を検知する値を変更
    rState = !rState;
  }
}

// ⑦赤外線送信処理
void irSend(unsigned short *irData, unsigned int irLength)
{
  // ⑧ローカル変数定義
  unsigned short irCount = 0; // HIGH,LOWの信号数
  unsigned long l_now = 0;    // 送信開始時間を保持
  unsigned long sndt = 0;     // 送信開始からの経過時間
  // ⑨HIGH,LOWの信号数を計算
  irCount = irLength;
  // ⑩送信開始時間を取得
  l_now = micros();
  // ⑪0,1の信号回数分をFor文でループ
  for (int i = 0; i < irCount; i++)
  {
    // ⑫送信開始からの信号終了時間を計算
    sndt += irData[i];
    Serial.print(irData[i]);
    Serial.print(",");
    do
    {
      // ⑬iが偶数なら赤外線ON、奇数ならOFFのまま
      // ⑭キャリア周波数38kHz(約26μSec周期の半分)でON時間で送信
      digitalWrite(SEND_PIN, !(i & 1));
      microWait(13);
      // ⑮キャリア周波数38kHz(約26μSec周期の半分)でOFF時間で送信
      digitalWrite(SEND_PIN, 0);
      microWait(13);
      // ⑯送信開始からの信号終了時間が超えるまでループ
    } while (long(l_now + (sndt * 10) - micros()) > 0);
  }
}

// ⑰マイクロ秒単位で待つ
void microWait(signed long waitTime)
{
  unsigned long waitStartMicros = micros();
  // ⑱指定されたマイクロ秒が経過するまでWhileでループ処理（待つ）
  while (micros() - waitStartMicros < waitTime)
  {
  };
}

// ---------------------------------------------
void irSave(int no, JsonDocument &doc)
{
  String fname = "/ir_" + String(no) + ".json";
  File f = SPIFFS.open(fname.c_str(), "w");
  String jsonString;
  serializeJson(doc, jsonString);
  f.println(jsonString.c_str());
  f.close();
}

bool irRead(int no, JsonDocument &doc)
{
  String fname = "/ir_" + String(no) + ".json";
  File f = SPIFFS.open(fname.c_str(), "r");
  if (!f)
  {
    return false;
  }
  char content[1500];
  f.readBytes(content, 1500);
  deserializeJson(doc, content);
  return true;
}
// ---------------------------------------------

double temperatures[SCREEN_WIDTH] = {0};
double pressures[SCREEN_WIDTH] = {0};
double humidities[SCREEN_WIDTH] = {0};
int currentIndex = 0;
int counter = 0;
bool collectBME280Flag = true;
typedef void (*DisplayScene)(void);
DisplayScene scenes[] = {displayTemperature, displayHumidity, displayPressure};
DisplayScene scene;

void setCollectBME280Flag()
{
  collectBME280Flag = true;
}

double currentValue(double *value)
{
  int idx = currentIndex - 1;
  if (idx < 0)
  {
    idx = SCREEN_WIDTH - 1;
  }
  return value[idx];
}

void displayTemperature()
{
  for (int i = 0; i < SCREEN_WIDTH; i++)
  {
    int idx = currentIndex - i;
    if (idx < 0)
      idx += SCREEN_WIDTH;
    double m = (double)SCREEN_HEIGHT / (TEMPERATURE_MAX - TEMPERATURE_MIN);
    double t = (temperatures[idx] - TEMPERATURE_MIN) * m;
    display.setPixel(SCREEN_WIDTH - i, SCREEN_HEIGHT - t);
  }
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Temp    :" + String(currentValue(temperatures)) + "C");
}

void displayHumidity()
{
  for (int i = 0; i < SCREEN_WIDTH; i++)
  {
    int idx = currentIndex - i;
    if (idx < 0)
      idx += SCREEN_WIDTH;
    double m = (double)SCREEN_HEIGHT / (HUMIDITY_MAX - HUMIDITY_MIN);
    double v = (humidities[idx] - HUMIDITY_MIN) * m;
    display.setPixel(SCREEN_WIDTH - i, SCREEN_HEIGHT - v);
  }
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Humidity:" + String(currentValue(humidities)) + "%");
  //display.drawString(0, 40, "Pressure:" + String(pressure) + "hPa");
}

void displayPressure()
{
  for (int i = 0; i < SCREEN_WIDTH; i++)
  {
    int idx = currentIndex - i;
    if (idx < 0)
      idx += SCREEN_WIDTH;
    double m = (double)SCREEN_HEIGHT / (PRESSURE_MAX - PRESSURE_MIM);
    double v = (pressures[idx] - PRESSURE_MIM) * m;
    display.setPixel(SCREEN_WIDTH - i, SCREEN_HEIGHT - v);
  }
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 0, "Pressure:" + String(currentValue(pressures)) + "hPa");
}

void loopBME()
{
  if (collectBME280Flag)
  {
    double temperature, pressure, humidity;
    bme280i2c.Read_All(&temperature, &pressure, &humidity);
    temperatures[currentIndex] = temperature;
    pressures[currentIndex] = pressure;
    humidities[currentIndex] = humidity;
    currentIndex = (currentIndex + 1) % SCREEN_WIDTH;
    scene = scenes[(currentIndex / 3) % 3];
  }
  collectBME280Flag = false;

  display.clear();
  display.setColor(WHITE);
  scene();
  display.display();
}
