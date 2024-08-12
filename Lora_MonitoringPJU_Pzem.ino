//Lora Parameter
#include <lorawan.h>
const sRFM_pins RFM_pins = {
  .CS = 33,
  .RST = 5,
  .DIO0 = 26,
  .DIO1 = 27,
};
String devAddr = "66325057";
String appSKey = "663250570a7d73c01db1afabbfc11014";
String nwkSKey = "66325057a50e84f45554fe914cf2493a";
int uplinkInterval = 1; //dalam menit

//LDR dan relay
byte ldrPin = 34;
byte relayPin = 4;
bool relayState = false;

//LCD Parameter
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

//PZEM Parameter
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
#include <PZEM004Tv30.h>
PZEM004Tv30 pzem1(&Serial2, PZEM_RX_PIN, PZEM_TX_PIN, 0x01);
PZEM004Tv30 pzem2(&Serial2, PZEM_RX_PIN, PZEM_TX_PIN, 0x02);

float voltage1, voltage2;
float current1, current2;
float power1, power2;
float ldrVoltage;

float onThreshold = 2.6;
float offThreshold = 1.2;

unsigned long prevLdr = 0;
unsigned long prevSampling = 0;
unsigned long prevSend = 0;


void setup() {
  Serial.begin(115200);
  pinMode(relayPin, OUTPUT);
  loraSetup();
  lcdSetup();
  xTaskCreatePinnedToCore(
    loop2,   /* Task function. */
    "loop2", /* name of task. */
    20000,   /* Stack size of task */
    NULL,    /* parameter of the task */
    1,       /* priority of the task */
    NULL,    /* Task handle to keep track of created task */
    0);      /* pin task to core 0 */
}

void loop() {
  if (millis() - prevLdr > 500){
    ldrVoltage = analogRead(ldrPin) / 4094.0 * 3.3;
    Serial.println("LDR : " +String(ldrVoltage)+ " V");
    if (ldrVoltage >onThreshold ) relayState = true;  //nyalakan relay
    else if (ldrVoltage < offThreshold ) relayState = false;  //matikan relay
    digitalWrite(relayPin, relayState);
    prevLdr = millis();
  }

  if (relayState == true) {
    if (millis() - prevSampling > 2000) {
      Serial.print("PZEM 1 - Address:");
      Serial.println(pzem1.getAddress(), HEX);
      voltage1 = pzem1.voltage();
      if(isnan(voltage1)) voltage1 = 0.0;
      current1 = pzem1.current();
      if(isnan(current1)) current1 = 0.0;
      power1 = pzem1.power();
      if(isnan(power1)) power1 = 0.0;
      delay(100);
      Serial.println("V :" + String(voltage1));
      Serial.println("I :" + String(current1,3));
      Serial.println("P :" + String(power1));

      Serial.print("PZEM 2 - Address:");
      Serial.println(pzem2.getAddress(), HEX);
      voltage2 = pzem2.voltage();
      if(isnan(voltage2)) voltage2 = 0.0;
      current2 = pzem2.current();
      if(isnan(current2)) current2 = 0.0;
      power2 = pzem2.power();
      if(isnan(power2)) power2 = 0.0;
      prevSampling = millis();
      Serial.println("V :" + String(voltage2));
      Serial.println("I :" + String(current2,3));
      Serial.println("P :" + String(power2));
      Serial.println();
      delay(100);
    }
  }
}

void loop2(void* pvParameters) {
  while (1) {
    if (relayState == true) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("PZEM1");
      lcd.setCursor(9, 0);
      lcd.print("P:" + String(power1));
      lcd.setCursor(0, 1);
      lcd.print("V:" + String(voltage1));
      lcd.setCursor(9, 1);
      lcd.print("I:" + String(current1,3));
      delay(2000);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("PZEM2");
      lcd.setCursor(9, 0);
      lcd.print("P:" + String(power2));
      lcd.setCursor(0, 1);
      lcd.print("V:" + String(voltage2));
      lcd.setCursor(9, 1);
      lcd.print("I:" + String(current2,3));
      delay(2000);
    } 
    else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("LDR:" + String(ldrVoltage) + "V");
      lcd.setCursor(0, 1);
      lcd.print(" Kondisi Terang ");
      delay(1000);
    }

    
    if (millis() - prevSend > uplinkInterval * 60000 and relayState == true) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Mengirim Data...");
      String data;
      data = "{\"V1\":" + String(voltage1, 2) + ",\"I1\":" + String(current1, 2) + ",\"P1\":" + String(power1, 2) + "}";
      sendData_Lora(data);
      data = "{\"V2\":" + String(voltage2, 2) + ",\"I2\":" + String(current2, 2) + ",\"P2\":" + String(power2, 2) + "}";
      sendData_Lora(data);
      prevSend = millis();
    }
    delay(100);  //core idle handling
  }
}

void lcdSetup() {
  lcd.init();
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print(" Monitoring PJU ");
  lcd.setCursor(0, 1);
  lcd.print("    LoRaWAN     ");
  delay(2000);
}

void loraSetup() {
  if (!lora.init()) {
    Serial.println("Problem : Lora");
    delay(2000);
    return;
  }
  lora.setDeviceClass(CLASS_A);  // Set LoRaWAN Class change CLASS_A
  lora.setDataRate(SF10BW125);
  lora.setFramePortTx(5);
  lora.setChannel(MULTI);
  lora.setTxPower(20);
  lora.setNwkSKey(nwkSKey.c_str());
  lora.setAppSKey(appSKey.c_str());
  lora.setDevAddr(devAddr.c_str());
}
void sendData_Lora(String msg) {
  char myStr[msg.length() + 1];
  msg.toCharArray(myStr, msg.length() + 1);
  lora.sendUplink(myStr, strlen(myStr), 0);
  lora.update();
  Serial.println("send Data " + String(myStr));  //debug
  Serial.println(" ");
  delay(3000);
}
