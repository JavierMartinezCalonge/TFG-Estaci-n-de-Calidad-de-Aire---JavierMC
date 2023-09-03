/*
 * Código principal del TFG: Desarollo de una estación de calidad del aire económica mediante el Internet de las Cosas
 * Autor: Javier Martínez Calonge
 * Director: Dr. Javier Fernández García
 * Grado en Ingenieria en Tecnologias Industriales
 * IQS 2021-2023
 * Última actualización: 3 de Septiembre 2023
 * 
 */
//Bibliotecas de Arduino
#define DEVICE "ESP32"
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#include "Wire.h"

//Bibliotecas de sensores: 
#include "DFRobot_BME280.h" //https://github.com/DFRobot/DFRobot_BME280
#include "SDS011.h" // https://github.com/ricki-z/SDS011/tree/master
#include "SparkFun_SCD30_Arduino_Library.h" // https://github.com/sparkfun/SparkFun_SCD30_Arduino_Library
#include <Multichannel_Gas_GMXXX.h>  //https://github.com/Seeed-Studio/Seeed_Arduino_MultiGas

//Bibloteca de InfluxDB https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino
#include <InfluxDbClient.h> 
#include <InfluxDbCloud.h>

//Bibliotecas para la Pantalla Olimex-MOD-LCD2.8RTP y su configuración en la placa OlimexESP32

#include "SPI.h" 
#include "Adafruit_GFX.h" //https://github.com/moononournation/Arduino_GFX
#include "Adafruit_ILI9341.h" //https://github.com/adafruit/Adafruit_ILI9341
#include "Board_Pinout.h"
#include "Adafruit_STMPE610.h" //https://github.com/adafruit/Adafruit_STMPE610
#include <Arduino_GFX_Library.h> //https://github.com/adafruit/Adafruit-GFX-Library
#define TFT_DC 15
#define TFT_CS 17
#define TFT_MOSI 2
#define TFT_CLK 14
#define TFT_MISO 0
#define TFT_RST 0
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_CLK, TFT_MOSI, TFT_MISO);
Arduino_GFX *gfx = new Arduino_ILI9341(bus, TFT_RST, 0 /* rotation */);
#define TS_MINX 290                                 
#define TS_MINY 285
#define TS_MAXX 7520
#define TS_MAXY 7510
#define TS_I2C_ADDRESS 0x4d
Adafruit_STMPE610 ts = Adafruit_STMPE610();

//Insertamos nuestros credenciales del Wifi
#define WIFI_SSID ""
#define WIFI_PASSWORD ""


//Insertamos nuestros credenciales de InfluxDB  
  #define INFLUXDB_URL ""
  #define INFLUXDB_TOKEN ""
  #define INFLUXDB_ORG ""
  #define INFLUXDB_BUCKET "AirQuality"
  
  // Zona UTZ 
  #define TZ_INFO "UTC-2"
  
  // Declaramos nuestros credenciales en el cliente de InfluxDB
  InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
  
  //Data point
  Point sensor("AirQuality");

//Configuración del BME280
typedef DFRobot_BME280_IIC    BME; 
BME   bme(&Wire, 0x76);   //Dirección I2C del dispositivo
#define SEA_LEVEL_PRESSURE    1015.0f

//Configuración del Multichannel Gas v2
#ifdef SOFTWAREWIRE
    #include <SoftwareWire.h>
    SoftwareWire myWire(3, 2);
    GAS_GMXXX<SoftwareWire> gas;
#else
    #include <Wire.h>
    GAS_GMXXX<TwoWire> gas;
#endif

//Configuración del SCD30
SCD30 airSensor;

//Configuración del SDS011
float p10, p25;
int error;
SDS011 my_sds;

//Incluimos las librerias para usar las memorias internas y la tarjeta SD de la placa, asi como su coexistencia.
#define FS_NO_GLOBALS 
#include "FS.h"           //https://github.com/lorol/LITTLEFS
#include "LITTLEFS.h"     //https://github.com/Bodmer/TJpg_Decoder 
#include "SD_MMC.h"       //Ver archivo del repositorio con las funciones para la SD y la memoria interna
#define SPIFFS LITTLEFS
#include "SDMMC_func.h"   //Ver archivo incluido en el repositorio, donde se encuentran funciones extra para el uso de la libreria


//Biblioteca para poner imágenes en la pantalla -- Logo IQS
#include <PNGdec.h> //https://github.com/bitbank2/PNGdec
PNG png;
fs::File pngFile;
int16_t w, h, xOffset, yOffset;
#include "PNG_func.h"   //ver archivo del repositorio con funciones para el uso de imagenes


//Definimos variables globales de configuración para el correcto uso de las funciones del código 
int i = 0; //Iterar guardado
char *pathChar; //Objeto de ruta par enviar los datos de la memoria a la SD


//Configuración del horario en el ESP32
const char* ntpServer = "europe.pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
struct tm timeinfoStart;
struct tm timeinfoNow;

//Tiempo interno del ESP32
#include <ESP32Time.h> //https://github.com/fbiego/ESP32Time
ESP32Time rtc;

void setup() {
  Serial.begin(115200);
  Wire.begin(); //Iniciamos conexión a los sensores 
  infoLITTLEFS (); //Mostramos información de la memoria interna
  infoNTP_RTC ();  //Sincronizamos el tiempo en la placa
  nuevoData();  //Creamos el archivo .csv para la recolección de datos

  my_sds.begin(36, 04); //Definimos los pines (TX,RX) de connexión UART con el SDS011

//Llamamos a la función displayPantalla inicializar la pantalla
  displayPantalla();

//Iniciamos el BME280 y se comprueba el funcionamiento correcto
bme.reset();
  Serial.println("bme read data test");
  while(bme.begin() != BME::eStatusOK) {
    Serial.println("bme begin faild");
    printLastOperateStatus(bme.lastOperateStatus);
    delay(2000);
  }
  Serial.println("bme begin success");
  delay(100);

//Iniciamos el Multichannel Gas Sensor V2 y comprobamos el correcto funcionamiento
  gas.begin(Wire, 0x08); // Definimos la dirección I2C interna del Multichannel Gas Sensor V2
  if (airSensor.begin() == false)
  {
    Serial.println("Air sensor not detected. Please check wiring. Freezing...");
    while (1)
      ;
  }
  
    // Configuramos la estación Wifi y nos conectamos a la red de Internet. 
    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  
    Serial.print("Connecting to wifi");
    while (wifiMulti.run() != WL_CONNECTED) {
      Serial.print(".");
      delay(100);
    }
    Serial.println();

    //Sincronizamos del tiempo horario para la base de datos a tiempo real
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  
  
    // Inciamos la comprovación de conexión con el servidor de InfluxDB
    if (client.validateConnection()) {
      Serial.print("Connected to InfluxDB: ");
      Serial.println(client.getServerUrl());
    } else {
      Serial.print("InfluxDB connection failed: ");
      Serial.println(client.getLastErrorMessage());
    }

//Mensajes de bienvenida y activación correcta de la estación
gfx->begin();
gfx->fillScreen(WHITE);
PrintCharTFT("¡Estación lista!", 20, 70, BLACK, WHITE, 1);
delay(500);

//FIN DEL SETUP
}

  
void loop() {
//Este código se repetirá en bucle:

//Se mostrará el Logo de IQS, nombre y tiempo
gfx->fillScreen(BLACK);
DrawPNG("/IQSSE240_82.png", 0, 0); //Logo de IQS
PrintCharTFT(rtc.getTime("%D, %H:%M:%S"), 80, 40, BLACK, WHITE, 1);

//Toma de medición del sensor de partículas SDS011
    error = my_sds.read(&p25, &p10);
  if (!error) {
    Serial.println("P2.5: " + String(p25));
    Serial.println("P10:  " + String(p10));
  }
//Toma de medición del resto de sensores (BME280, SCD30, Multichannel Gas V2)
float temp = airSensor.getTemperature();
float humid = airSensor.getHumidity();
float co2 = airSensor.getCO2();
float co = gas.measure_CO();
float no2 = gas.measure_NO2();
float temp2 = bme.getTemperature();
float humid2 = bme.getHumidity();
float pm25=p25;
float pm10=p10;

//Mostramos en el serial de Arduino (en el PC) los valores medidos
Serial.println(temp);
Serial.println(humid);
Serial.println(temp2);
Serial.println(humid2);
Serial.println(co2);
Serial.println(co);
Serial.println(no2);
Serial.println(p25);
Serial.println(p10);

//Creamos y enviamos los datos medidos al servidor de InfluxDB
sensor.clearFields();

sensor.addField("PM2.5", p25);
sensor.addField("PM10", p10);
sensor.addField("Temperatura", temp);
sensor.addField("Humedad", humid);
sensor.addField("CO2", co2);
sensor.addField("CO", co);
sensor.addField("NO2", no2);
sensor.addField("Temperatura BME", temp2);
sensor.addField("Humedad BME", humid2);

//Controlamos que tengamos conexión a Internet
if (wifiMulti.run() !=WL_CONNECTED)
  Serial.println("Wifi connection lost");

//Comprobamos que se ha enviado correctamente los datos a InfluxDB
if(!client.writePoint(sensor))
  {
    Serial.print("InfluxDB write failed:  ");
    Serial.print(client.getLastErrorMessage());
  }

//Representamos en pantalla los últimos valores medidos por la estación 
gfx->setTextColor(BLUE);  gfx->setTextSize(2);
gfx->setCursor(10, 100); gfx->print("Temperatura:");
gfx->print(temp2); gfx->print("C");

gfx->setCursor(10, 120); gfx->print("Humedad: ");
  gfx->setTextColor(BLUE); gfx->print(humid2); gfx->print("%");

  gfx->setCursor(10, 140); gfx->print("CO2: ");
  gfx->setTextColor(BLUE); gfx->print(co2); gfx->print("ppm");

  gfx->setCursor(10, 160); gfx->print("PM2.5: ");
  gfx->setTextColor(BLUE); gfx->print(p25); gfx->print("ug/m3");

  gfx->setCursor(10, 180); gfx->print("PM10: ");
  gfx->setTextColor(BLUE); gfx->print(p10); gfx->print("ug/m3");

  gfx->setCursor(10, 200); gfx->print("NO2: ");
  gfx->setTextColor(BLUE); gfx->print(no2); gfx->print("ppm");
  
  gfx->setCursor(10, 220); gfx->print("CO: ");
  gfx->setTextColor(BLUE); gfx->print(co); gfx->print("ppm");

//Guardamos los datos en la SD como backup en caso de perder la conexión a internet
//Llamámos a las funciones de guardado en SD

escribirLITTLEFS (); //Guardamos en la memória interna
LITTLEFStoSD (); //Transferimos los datos de la memória interna a la SD

delay(1200000); //Cada 20 minutos enviamos y guardamos los datos de la estación. Se puede modificar según los requerimientos del usuario.
}

void displayPantalla () {
gfx->begin();
gfx->fillScreen(BLUE);
gfx->setTextColor(YELLOW);  gfx->setTextSize(2);
gfx->setCursor(10, 90); gfx->print("Inicializando...");
delay(1000);
}

//Función para definir el tiempo en el ESP32 y la tarjeta microSD
void infoNTP_RTC () {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)){ 
    rtc.setTimeStruct(timeinfo);
  }
}
//Función del Bosch BME280 para comprobar el correcto funcionamiento 
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

//Comprovación de la memória interna del OlimexESP32
void infoLITTLEFS () {
  Serial.println("");
  Serial.print(F("Inizializing FS..."));
  if (LITTLEFS.begin()){
    Serial.println(F("done."));
  }else{
    Serial.println(F("fail."));
  }
  unsigned int totalBytes = LITTLEFS.totalBytes();
  unsigned int usedBytes = LITTLEFS.usedBytes();
  unsigned int freeBytes  = totalBytes - usedBytes; 
  Serial.println("File sistem info:");
  Serial.print("Total space:      ");
  Serial.print(totalBytes);
  Serial.println("byte");
  Serial.print("Total space used: ");
  Serial.print(usedBytes);
  Serial.println("byte");
  Serial.print("Total space free: ");
  Serial.print(freeBytes);
  Serial.println("byte");
}


//Función creada para escribir en la memoria interna del OlimexESP32
void escribirLITTLEFS () {
  fs::File dataFile = LITTLEFS.open(F("/Dades.csv"), "a");
  if (dataFile){
    dataFile.print(rtc.getTime("%D")); dataFile.print(';');
    dataFile.print(rtc.getTime("%H:%M:%S")); dataFile.print(';');
    dataFile.print(temp); dataFile.print(';');
    dataFile.print(humid); dataFile.print(';');
    dataFile.print(temp2); dataFile.print(';');
    dataFile.print(humid2); dataFile.print(';');
    dataFile.print(co); dataFile.print(';');
    dataFile.print(no2); dataFile.print(';');
    dataFile.print(co2); dataFile.print(';');
    dataFile.print(pm25); dataFile.print(';');
    dataFile.print(pm10); dataFile.println(';');
    dataFile.close(); 
    }
    delay(1000);
  i++;
}

//Función para crear archivo de datos
void nuevoData() {
  if (LITTLEFS.remove("/Dades.csv")) {
    Serial.println("Old Dades.csv file deleted");
  }
  fs::File dataFile = LITTLEFS.open("/Dades.csv", FILE_APPEND);  
  if (dataFile){
    Serial.println("Archivo creados correctamente");
    dataFile.print("Data"); dataFile.print(';');
    dataFile.print("Hora"); dataFile.print(';');
    dataFile.print("TempSCD ºC"); dataFile.print(';');
    dataFile.print("HRSCD %"); dataFile.print(';');
    dataFile.print("TempBME ºC"); dataFile.print(';');
    dataFile.print("HR2BME %"); dataFile.print(';');
    dataFile.print("CO "); dataFile.print(';');
    dataFile.print("NO2 "); dataFile.print(';');     
    dataFile.print("CO2 PPM"); dataFile.print(';');
    dataFile.print("PM2.5"); dataFile.print(';');
    dataFile.print("PM10"); dataFile.println(';');    
    dataFile.close();
  }else{
    Serial.println("Error en la copia");
  }
}

//Pasamos los datos de la memoria del ESP32 a la microSD
void LITTLEFStoSD () {
  if (!SD_MMC.begin()) {
    Serial.println("Card Mount Failed");
    return;
  }
  else {
    Serial.println("Card Mount Successful");
  }
  String filename = "/" + rtc.getTime("%Y%m%d_%H%M") + ".csv";
  pathChar = const_cast<char*>(filename.c_str());
  fs::File sourceFile = LITTLEFS.open("/Dades.csv");
  fs::File destFile = SD_MMC.open(pathChar, FILE_WRITE);
  if (!destFile) {
    Serial.println("Cannot open data SD file");
    return;
  }
  static uint8_t buf[512];
  while ( sourceFile.read( buf, 512) ) {
    destFile.write( buf, 512 );
  }
  delay(100);
  destFile.close();
  sourceFile.close();
  SD_MMC.end();
}
