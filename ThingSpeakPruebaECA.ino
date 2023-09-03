//Bibliotecas de Arduino propias utilizadas y los repositorios de sensores y cliente ThingSpeak
#include "ThingSpeak.h" //https://github.com/mathworks/thingspeak-arduino
#include "DFRobot_CCS811.h" //https://github.com/DFRobot/DFRobot_CCS811
#include "DFRobot_BME280.h" //https://github.com/DFRobot/DFRobot_BME280
#include "Wire.h"
#include <WiFi.h>


// Insertar credenciales de internet
const char* ssid = "";
const char* password = "";
//Conectamos con el cliente del Wifi
WiFiClient  client;

//Insertar el número de canal asignador y la clave API de ThingSpeak personal
unsigned long myChannelNumber = ;
const char * myWriteAPIKey = "";

// Variables temporales
unsigned long lastTime = 0;
unsigned long timerDelay = 5000; //Insertar el tiempo de subida de datos a ThingSpeak

//Configuración de conexión I2C del BME280
typedef DFRobot_BME280_IIC    BME;    // ******** use abbreviations instead of full names ********
BME   bme(&Wire, 0x76);   // select TwoWire peripheral and set sensor address
#define SEA_LEVEL_PRESSURE    1015.0f
void printLastOperateStatus(BME::eStatus_t eStatus)
{
  switch(eStatus) {
  case BME::eStatusOK:    Serial.println("everything ok"); break;
  case BME::eStatusErr:   Serial.println("unknow error"); break;
  case BME::eStatusErrDeviceNotDetected:    Serial.println("device not detected"); break;
  case BME::eStatusErrParameter:    Serial.println("parameter error"); break;
  default: Serial.println("unknow status"); break;
  }
}

//Definimos la variables del CCS811 para su uso en el código posterior
DFRobot_CCS811 CCS811;


void setup() {
  Serial.begin(115200);    
  WiFi.mode(WIFI_STA);  
//Iniciamos el BME280 y el CCS811 
  bme.reset();
  Serial.println("bme read data test");
  while(bme.begin() != BME::eStatusOK) {
    Serial.println("bme begin failed");
    printLastOperateStatus(bme.lastOperateStatus);
    delay(2000);
  }
  Serial.println("bme begin success");
  delay(100); 
   while(CCS811.begin() != 0){
        Serial.println("failed to init chip, please check if the chip connection is fine");
        delay(1000);
    }
//Iniciamos el cliente de ThingSpeak
  ThingSpeak.begin(client);  // Initialize ThingSpeak
}

void loop() {
   CCS811.writeBaseLine(0x8867); //Insertamos el valor base de calibrado del CCS811 (ver repositorio del CCS811, para el código)
    delay(1000);
//Conectamos a internet
  if ((millis() - lastTime) > timerDelay) {
    // Connect or reconnect to WiFi
    if(WiFi.status() != WL_CONNECTED){
      Serial.print("Attempting to connect");
      while(WiFi.status() != WL_CONNECTED){
        WiFi.begin(ssid, password); 
        delay(5000);     
      } 
      Serial.println("\nConnected.");
    }

//Asignamos las variables de medición 
  float   temp = bme.getTemperature();
  float    pressure = bme.getPressure()/100.0F;
  float   humi = bme.getHumidity();
  float   co2=CCS811.getCO2PPM();   

    // Asignamos los campos de ThingSpeak a las variables medidas
    ThingSpeak.setField(1, temp);
    ThingSpeak.setField(2, humi);
    ThingSpeak.setField(3, pressure);
    ThingSpeak.setField(4, co2);
    
    // Subimos nuestros datos a ThingSpeak
    int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

    if(x == 200){
      Serial.println("Channel update successful.");
    }
    else{
      Serial.println("Problem updating channel. HTTP error code " + String(x));
    }
    lastTime = millis();
  }
}
