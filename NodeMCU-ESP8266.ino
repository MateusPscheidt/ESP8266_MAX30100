#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <OneWire.h>
#include "MAX30100_PulseOximeter.h"
#include <PubSubClient.h>
#include <DallasTemperature.h>
#include <Thermistor.h>
#include <ArduinoJson.h>


//constantes que define o período de relatório em milissegundos
#define REPORTING_PERIOD_MS     1000
#define WAITING_TIME            5000

//objetos que serão utilizados para interagir com sensores
Thermistor temp(0);
OneWire oneWire(2);
DallasTemperature sensors(&oneWire);
PulseOximeter pox;

const char* ssid = "SSID_WIFI";
const char* password = "PassWord_WIFI";
//const int led = 2;

ESP8266WebServer server(80);

bool isFirstBeatDetected = false; //detecção
uint32_t tsFirstBeatDetected = 0; //tempo do primeiro batimento
float bat = 10000.00;
uint32_t tsLastReport = 0; //tempo do último relatório


//Ultima leitura de batimento cardiaco
unsigned long lastZeroReadingTime = 0;
unsigned long lastNonZeroReadingTime = 0;




//callback que é acionado quando um batimento cardíaco é detectado pelo sensor de oximetria de pulso  (MAX30100) (PulseOximeter)
void onBeatDetected()
{
    Serial.println("Beat!");
    if (!isFirstBeatDetected) {
        isFirstBeatDetected = true;
        tsFirstBeatDetected = millis();
    }
}




//handleRoot manipula a rota para a raiz ("/") do servidor web. Essa função é chamada quando um cliente faz uma solicitação para a rota principal
void handleRoot() {
//    digitalWrite(led, 1);

//StaticJsonDocument armazena os dados que serão enviados como resposta. A função verifica se o primeiro batimento cardíaco foi detectado (isFirstBeatDetected)
    StaticJsonDocument<256> doc;
    
    if (isFirstBeatDetected && millis() - tsFirstBeatDetected > 10000) {
        int heartRate = pox.getHeartRate();
        // Verifica se o batimento cardíaco é zero
        if (heartRate == 0 && millis() - lastZeroReadingTime >= WAITING_TIME) {
            doc["batimento"] = "Aguardando Estabilização";
            doc["spo2"] = "Aguardando Estabilização";
            // Se o batimento cardíaco for diferente de zero, atualiza o tempo da última leitura de valor não zero
        } else {
            if (heartRate != 0) {
                lastNonZeroReadingTime = millis();
            }
            // Verifica se o tempo decorrido desde a última leitura
            if (millis() - lastNonZeroReadingTime >= WAITING_TIME) {
                doc["batimento"] = "Aguardando Estabilização";
                doc["spo2"] = "Aguardando Estabilização";
                // Atualiza o tempo da leitura zero
                lastZeroReadingTime = millis();
            } else {
              // Atribui os valores reais de batimento cardíaco e saturação de oxigênio aos campos correspondentes do objeto doc
                doc["batimento"] = String(pox.getHeartRate(), 2) + " Bpm";
                doc["spo2"] = String(pox.getSpO2()) + " %";
            }
        }
    } else {
      // Caso o primeiro batimento cardíaco não tenha sido detectado
        doc["batimento"] = "Aguardando Estabilização";
        doc["spo2"] = "Aguardando Estabilização";
    }

    float tempp = temp.getTemp() - 0.0;
    doc["temperature"] = String(tempp) + " ºC";
    
    // Serializa o objeto doc em formato JSON
    String jsonString;
    serializeJson(doc, jsonString);

    // Define o cabeçalho da resposta HTTP permitindo acesso de qualquer origem
    server.sendHeader("Access-Control-Allow-Origin", "*");

    // Envia a resposta HTTP
    server.send(200, "application/json", jsonString);

//    digitalWrite(led, 0);
}


//Essa função é chamada quando uma requisição é feita para uma rota que não foi encontrada no servidor web. Ela gera uma mensagem de erro indicando o caminho não encontrado,
void handleNotFound(){
//    digitalWrite(led, 1);
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++){
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    
    // Envia a resposta HTTP com o status 404 Not Found, o tipo de conteúdo text/plain e a mensagem de erro
    server.send(404, "text/plain", message);
//    digitalWrite(led, 0);
}

void setup(void){
    pinMode(15, INPUT_PULLUP); //resistor interno
//        pinMode(12, OUTPUT);
//    pinMode(led, OUTPUT);
//    digitalWrite(led, 0);
    Serial.begin(115200); //taxa de baud rate
    Serial.print("Initializing pulse oximeter..");

    WiFi.mode(WIFI_STA); // Configura o modo do WiFi como cliente (station)
    WiFi.begin(ssid, password); // credenciais da rede WiFi
    Serial.println("");

    sensors.begin();

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    IPAddress ip(192, 168, 137, 178); //ALterar o IP se necessário
    IPAddress gateway(192, 168, 137, 1); //ALterar o Gateway se necessário
    IPAddress subnet(255, 255, 255, 0);
  
    WiFi.config(ip, gateway, subnet);

    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Gateway IP ");
    Serial.println(WiFi.gatewayIP());

// Inicializa o servidor
    if (MDNS.begin("esp8266")) {
        Serial.println("MDNS responder started");
    }

    server.on("/", handleRoot); // Mapeia a função handleRoot para a rota "/"

    server.on("/inline", [](){
        server.send(200, "text/plain", "this works as well");
    });

    server.onNotFound(handleNotFound); // Mapeia a função handleNotFound

    server.begin(); // Inicia o servidor web
    Serial.println("HTTP server started");

    if (!pox.begin()) {
        Serial.println("FAILED");
        for (;;) ;
    } else {
        Serial.println("SUCCESS");
    }
    
  // Configura a corrente do LED IR do oxímetro de pulso (pode ser alterado, se necessário)

    //pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
    pox.setIRLedCurrent(MAX30100_LED_CURR_20_8MA);
    //pox.setIRLedCurrent(MAX30100_LED_CURR_27_1MA);

    
    pox.setOnBeatDetectedCallback(onBeatDetected);
}


//Essa função é responsável por atualizar as leituras do oxímetro de pulso
void max(){
    pox.update(); // Atualiza o oxímetro de pulso

    if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
        Serial.print("Heart rate:");
        Serial.print(pox.getHeartRate());
        Serial.print("bpm / SpO2:");
        Serial.print(pox.getSpO2());
        Serial.println("%");
        tsLastReport = millis(); // Atualiza o tempo do último relatório
        bat = pox.getHeartRate();

        int temperature = temp.getTemp() - 3;
        Serial.print("Temperatura: ");
        Serial.print(temperature);
        Serial.println("ºC");
    }
}

void loop(void){
    max(); // Chama a função max() para atualizar as leituras
    server.handleClient(); // Lida com as requisições do cliente HTTP
}
