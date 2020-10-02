#include <Free_Fonts.h>
#include <M5Display.h>
#include <M5Stack.h>

#include <WiFi.h>
#include "Esp32MQTTClient.h"

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"

// ----------------------------------------------------
// secinfo.h must be configured.
//
// SAMPLE DATA
///////////////////////////
// #ifndef FILE_FOO_SEEN
// #define FILE_FOO_SEEN
//
// // Please input the SSID and password of WiFi
// const char* ssid     = "<your SSID>>";
// const char* password = "<your PSK>>";
//
// /*String containing Hostname, Device Id & Device Key in the format:                         */
// /*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"                */
// /*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessSignature=<device_sas_token>"    */
// //static const char* connectionString = "";
// 
// static const char* connectionString = "<your device copnnection string>";
//
// #endif /* !FILE_FOO_SEEN */
//------------------------------------------------------

#include "secinfo.h"

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME680 bme; // I2C

#define INTERVAL 30000
#define MESSAGE_MAX_LEN 256

const char *messageData = "{\"messageId\":%d, \"Temperature\":%f, \"Humidity\":%f, \"Pressure\":%f, \"Gas\":%f}";
static bool hasIoTHub = false;
static bool hasWifi = false;
int messageCount = 1;
static bool messageSending = true;
static uint64_t send_interval_ms;

static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
  {
    Serial.println("Send Confirmation Callback finished.");
  }
}

static void MessageCallback(const char* payLoad, int size)
{
  Serial.println("Message callback:");
  Serial.println(payLoad);
}

static void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size)
{
  char *temp = (char *)malloc(size + 1);
  if (temp == NULL)
  {
    return;
  }
  memcpy(temp, payLoad, size);
  temp[size] = '\0';
  // Display Twin message.
  Serial.println(temp);
  free(temp);
}

static int DeviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size)
{
  LogInfo("Try to invoke method %s", methodName);
  const char *responseMessage = "\"Successfully invoke device method\"";
  int result = 200;

  if (strcmp(methodName, "start") == 0)
  {
    LogInfo("Start sending temperature and humidity data");
    messageSending = true;
  }
  else if (strcmp(methodName, "stop") == 0)
  {
    LogInfo("Stop sending temperature and humidity data");
    messageSending = false;
  }
  else
  {
    LogInfo("No method %s found", methodName);
    responseMessage = "\"No method found\"";
    result = 404;
  }

  *response_size = strlen(responseMessage) + 1;
  *response = (unsigned char *)strdup(responseMessage);

  return result;
}



void setup() {
  
  M5.begin();
  
  M5.Power.begin();

  logToDisplay("Starting up...");

  Serial.begin(115200);
  Serial.println("ESP32 Device");
  Serial.println("Initializing...");
  Serial.println(" > WiFi");
  Serial.println("Starting connecting WiFi.");

  delay(10);
  WiFi.mode(WIFI_AP);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    hasWifi = false;
  }
  hasWifi = true;
  
  Serial.println("WiFi connected");
  logToDisplay("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println(" > IoT Hub");
  if (!Esp32MQTTClient_Init((const uint8_t*)connectionString, true))
  {
    hasIoTHub = false;
    Serial.println("Initializing IoT hub failed.");
    return;
  }

  logToDisplay("IoT hub connected...");
    
  hasIoTHub = true;
  Esp32MQTTClient_SetSendConfirmationCallback(SendConfirmationCallback);
  Esp32MQTTClient_SetMessageCallback(MessageCallback);
  Esp32MQTTClient_SetDeviceTwinCallback(DeviceTwinCallback);
  Esp32MQTTClient_SetDeviceMethodCallback(DeviceMethodCallback);
  Serial.println("Start sending events.");
  randomSeed(analogRead(0));

  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    
    logToDisplay("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }

  
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

  send_interval_ms = millis() - (INTERVAL - 5000);  // make sure first message is sent after 10 sec.

}

void logToDisplay(char* msg) {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(3);
  M5.Lcd.printf("%s", msg);
}

int8_t getBatteryLevel()
{
  Wire.beginTransmission(0x75);
  Wire.write(0x78);
  if (Wire.endTransmission(false) == 0
   && Wire.requestFrom(0x75, 1)) {
    switch (Wire.read() & 0xF0) {
    case 0xE0: return 25;
    case 0xC0: return 50;
    case 0x80: return 75;
    case 0x00: return 100;
    default: return 0;
    }
  }
  return -1;
}

void logToDisplay(float temp, float hum, float pres, float gas) {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  
  char outstr[15];

  M5.Lcd.setCursor(10, 10);
  
  dtostrf(temp,7, 2, outstr);
  M5.Lcd.printf("T : %s", outstr);
  
  M5.Lcd.setCursor(10, 50);
  dtostrf(hum,7, 2, outstr);
  M5.Lcd.printf("H : %s", outstr);
  M5.Lcd.print("%");

  M5.Lcd.setCursor(10, 90);
  dtostrf(pres,7, 2, outstr);
  M5.Lcd.printf("P : %s", outstr);

  M5.Lcd.setCursor(10, 130);
  dtostrf(gas,7, 2, outstr);
  M5.Lcd.printf("G : %s", outstr);
  
  M5.Lcd.setCursor(10, 210);
  M5.Lcd.print(getBatteryLevel());
  M5.Lcd.print("%");
}

void loop() {
if (hasWifi && hasIoTHub)
  {
    if (messageSending && 
        (int)(millis() - send_interval_ms) >= INTERVAL)
    {
      if (! bme.performReading()) {
        Serial.println("Failed to perform reading :(");
        return;
      }

      // Send teperature data
      char messagePayload[MESSAGE_MAX_LEN];
      float temperature = bme.temperature; //(float)random(0,500)/10;
      float humidity = bme.humidity;// / 100.0F; //(float)random(0, 1000)/10;
      float pressure = bme.pressure / 100.0F;
      float gasR = bme.gas_resistance / 1000.0;
      snprintf(messagePayload, MESSAGE_MAX_LEN, messageData, messageCount++, temperature, humidity, pressure, gasR);
      Serial.println(messagePayload);
      EVENT_INSTANCE* message = Esp32MQTTClient_Event_Generate(messagePayload, MESSAGE);
      Esp32MQTTClient_SendEventInstance(message);
      send_interval_ms = millis();
      logToDisplay(temperature, humidity, pressure, gasR);
    }
    else
    {
      Esp32MQTTClient_Check();
    }
  }
  delay(10);
}
