//https://wokwi.com/projects/399307717572388865

#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include "DHTesp.h"

Servo myservoJanela; // Servo para janela
Servo myservoCortina; // Servo para cortina
Servo myservoAC; // Servo para AC

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.hivemq.com"; // Servidor MQTT

WiFiClient espClient;
PubSubClient client(espClient);

DHTesp dhtSensor;
const int DHT_PIN = 15;

#define LIGHT_SENSOR_PIN 36 // ESP32 pin GIOP36 (ADC0)
#define BUZZER_PIN 27 // Pino do buzzer
#define PIR_PIN 35  // Pino do sensor PIR

float temp = 0;
float hum = 0;
long lastMsg = 0;
char msg[50];
int value = 0;
int lotacao = 0; // Variável de lotação

void setup() {
  Serial.begin(115200);

  myservoJanela.attach(5); // Servo para janela no pino 5
  myservoCortina.attach(4); // Servo para cortina no pino 4
  myservoAC.attach(21); // Servo para AC no pino 21

  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  String string;
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i < length; i++) {
    string += ((char)payload[i]);
  }

  Serial.println(string);

  if (strcmp(topic, "/46161_50891/Janela") == 0) { // Verifica o tópico da janela
    Serial.print(" ");
    int status = string.toInt();
    int pos = map(status, 1, 100, 0, 180);
    Serial.println(pos);
    myservoJanela.write(pos);
    delay(15);
  }

  if (strcmp(topic, "/46161_50891/Cortina") == 0) { // Verifica o tópico da cortina
    Serial.print(" ");
    int status = string.toInt();
    int pos = map(status, 1, 100, 0, 180);
    Serial.println(pos);
    myservoCortina.write(pos);
    delay(15);
  }

  if (strcmp(topic, "/46161_50891/AC") == 0) { // Verifica o tópico do AC
    Serial.print(" ");
    int status = string.toInt();
    int pos = map(status, 1, 100, 0, 180); // Corrigido o mapeamento
    Serial.println(pos);
    myservoAC.write(pos);
    delay(15);

    // Adicionando condição para retornar "ligado" ou "desligado" baseado no valor de status
    if (status == 0) {
      Serial.println("ligado");
    } else if (status == 100) {
      Serial.println("desligado");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESPClient")) {
      Serial.println("connected");
      client.subscribe("/46161_50891/Janela");
      client.subscribe("/46161_50891/Cortina");
      client.subscribe("/46161_50891/AC");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;

    // Leitura do sensor DHT22
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    temp = data.temperature;
    hum = data.humidity;

    Serial.println("Temp: " + String(temp, 2) + "°C");
    Serial.println("Humidity: " + String(hum, 1) + "%");
    Serial.println("---");

    // Publicar temperatura e umidade no MQTT
    snprintf(msg, 50, "%.2f °C", temp);
    client.publish("/46161_50891/temperature", msg);
    snprintf(msg, 50, "%.1f %%", hum);
    client.publish("/46161_50891/humidity", msg);

    // Leitura do sensor de luz
    int analogValue = analogRead(LIGHT_SENSOR_PIN);
    Serial.print("Analog Value = ");
    Serial.print(analogValue); // the raw analog reading

    String lightIntensity;
    // Classificação da intensidade da luz
    if (analogValue < 40) {
      lightIntensity = "Escuro";
    } else if (analogValue < 800) {
      lightIntensity = "Meio";
    } else if (analogValue < 2000) {
      lightIntensity = "Normal";
    } else if (analogValue < 3200) {
      lightIntensity = "Luminoso";
    } else {
      lightIntensity = "Muita Luz!";
    }

    Serial.println(" => " + lightIntensity);

    // Publicar valor do sensor de luz no MQTT
    snprintf(msg, 50, "%d lux", analogValue);
    client.publish("/46161_50891/light", msg);

    // Leitura do sensor PIR
    const int IP = digitalRead(PIR_PIN);
    Serial.println(IP);

    if (IP == 1) {
      lotacao++; // Incrementa a lotação
      Serial.print("Lotação: ");
      Serial.println(lotacao);
      
      if (lotacao >= 3) {
        tone(BUZZER_PIN, 1000); // Emite um som no buzzer (frequência de 1000 Hz)
        Serial.println("Alerta! Lotação máxima atingida.");
        client.publish("/46161_50891/movement", "Lotação máxima atingida."); // Publica mensagem no MQTT
        delay(1000);       // Espera 1 segundo
        noTone(BUZZER_PIN);
      } else {
        noTone(BUZZER_PIN);    // Certifique-se de que o som está desligado
        Serial.println("Disponível");
        client.publish("/46161_50891/movement", "Disponível"); // Publica mensagem no MQTT
        delay(1000);       // Espera 1 segundo
      }
    } else {
      if (lotacao > 0) { // Se não houver movimento, mas houver lotação registrada
        lotacao = 0; // Reinicia a lotação
        Serial.println("Lotação reiniciada.");
      }
    }
  }

  delay(100);
}
