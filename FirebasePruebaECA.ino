// Se incluye las bibliotecas de los sensores a medir, en esta prueba sólo está el chip Bosch BME280 presente
#include "DFRobot_BME280.h" 
#include "Wire.h"
#include <Arduino.h>
#include <WiFi.h>

// Biblioteca de Firebase: https://github.com/mobizt/Firebase-ESP-Client
#include <Firebase_ESP_Client.h>

// Damos la información de generación del token y del RTDB
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Insertar credenciales de Wifi
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Insertar la clave API del proyecto Firebase 
#define API_KEY ""

// Insertar el correo electrónico y contraseña personal de la cuenta de Firebase
#define USER_EMAIL ""
#define USER_PASSWORD ""

// Insertar el URL del proyecto
#define DATABASE_URL ""

// Definimos los objetos de Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

//  Variable donde se guarda el UID del usuario
String uid;

// Variables de ruta de nuestras mediciones
String databasePath;
String tempPath;
String humPath;
String presPath;

// Variable temporal
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 30000; //Aqui podemos configurar cada cuánto tiempo se envía la información, en este ejemplo són 30 segundos. 

// Configuración del BME280: https://github.com/DFRobot/DFRobot_BME280
typedef DFRobot_BME280_IIC    BME;    
/**IIC address is 0x76 when pin SDO is low  (SIM7000)*/
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

void setup()
{
  Serial.begin(115200);
//Conectamos a internet  
  initWiFi();
  bme.reset();
  Serial.println("bme read data test");
  while(bme.begin() != BME::eStatusOK) {
    Serial.println("bme begin failed");
    printLastOperateStatus(bme.lastOperateStatus);
    delay(2000);
  }
  Serial.println("bme begin success");
  delay(100);

// Asignamos nuestra clave API
  config.api_key = API_KEY;

  // Asignamos nuestros credenciales
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Asignamos nuestra URL
  config.database_url = DATABASE_URL;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  // Generación del token
  config.token_status_callback = tokenStatusCallback; //ver addons/TokenHelper.h
  config.max_token_generation_retry = 5;

  // Iniciamos la libreria de Firebase con la configuración del usuario y credenciales
  Firebase.begin(&config, &auth);

  // Obtenemos nuestro UID 
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  // Mostrar nuestro UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Nombre de la ruta
  databasePath = "/UsersData/" + uid;

  // Ruta de las mediciones en Firebase
  tempPath = databasePath + "/temperature"; // --> UsersData/<user_uid>/temperature
  humPath = databasePath + "/humidity"; // --> UsersData/<user_uid>/humidity
  presPath = databasePath + "/pressure"; // --> UsersData/<user_uid>/pressure
}


void loop()
//Tomámos las mediciones de nuestro sensor 
{
  //Asignamos la variable de medición
  float   temp = bme.getTemperature();
  float   pressure = bme.getPressure()/100.0F;
  float   alti = bme.calAltitude(SEA_LEVEL_PRESSURE, pressure);
  float   humi = bme.getHumidity();
  //Mostramos en el serial de Arduino nuestros valores medidos
  Serial.println();
  Serial.println("======== start print ========");
  Serial.print("temperature (unit Celsius): "); Serial.println(temp);
  Serial.print("pressure (unit pa):         "); Serial.println(pressure);
  Serial.print("altitude (unit meter):      "); Serial.println(alti);
  Serial.print("humidity (unit percent):    "); Serial.println(humi);
  Serial.println("========  end print  ========");
  delay(2000);

   // Envio de datos a Firebase cuando se aplique nuestra restricción de tiempo de envío. 
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();

    // Envio de mediciones:
    sendFloat(tempPath, temp);
    sendFloat(humPath, humi);
    sendFloat(presPath, pressure);
  }
}


// Función del Wifi para conectarnos a Internet
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
  Serial.println();
}
// Función de envío de valores a Firebase

void sendFloat(String path, float value){
  if (Firebase.RTDB.setFloat(&fbdo, path.c_str(), value)){
    Serial.print("Writing value: ");
    Serial.print (value);
    Serial.print(" on the following path: ");
    Serial.println(path);
    Serial.println("PASSED");
    Serial.println("PATH: " + fbdo.dataPath());
    Serial.println("TYPE: " + fbdo.dataType());
  }
  else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
}
