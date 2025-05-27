#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
const char* topic_nivel = "reservatorio/nivel";
const char* topic_bomba = "reservatorio/bomba";

const int PIN_TRIGGER = 5;
const int PIN_ECHO = 18;
const int PIN_RELE = 23;
const int PIN_LED = 2;

WiFiClient espClient;
PubSubClient client(espClient);

bool bomba_ligada = false;
float nivel_agua = 0.0;
const float altura_reservatorio = 40.0;

unsigned long lastMsgTime = 0;
const long interval = 2000;

unsigned long lastReconnectAttempt = 0;

void setup_wifi() {
  Serial.print("Conectando-se ao WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Conectado!");
}

float medirDistancia() {
  digitalWrite(PIN_TRIGGER, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIGGER, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIGGER, LOW);

  long duracao = pulseIn(PIN_ECHO, HIGH, 30000);
  if (duracao == 0) return -1;

  float distancia = (duracao * 0.0343) / 2.0;
  return distancia;
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }
  msg.toLowerCase();

  Serial.print("Mensagem recebida no tópico ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(msg);

  if (String(topic) == topic_bomba) {
    if (msg == "liga") {
      bomba_ligada = true;
      Serial.println("Bomba ligada!");
    } else if (msg == "desliga") {
      bomba_ligada = false;
      Serial.println("Bomba desligada!");
    }
  }
}

bool mqtt_reconnect() {
  if (client.connect("ESP32Reservatorio")) {
    Serial.println("Conectado ao MQTT!");
    client.subscribe(topic_bomba);
  } else {
    Serial.print("Falha na conexão MQTT, rc=");
    Serial.print(client.state());
    Serial.println(" tentando novamente em 5 segundos");
  }
  return client.connected();
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_TRIGGER, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_RELE, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  digitalWrite(PIN_RELE, LOW);
  digitalWrite(PIN_LED, LOW);

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      mqtt_reconnect();
    }
  } else {
    client.loop();

    unsigned long now = millis();
    if (now - lastMsgTime > interval) {
      lastMsgTime = now;

      digitalWrite(PIN_RELE, bomba_ligada ? HIGH : LOW);
      digitalWrite(PIN_LED, bomba_ligada ? HIGH : LOW);

      if (bomba_ligada) {
        float distancia = medirDistancia();
        if (distancia >= 0 && distancia <= altura_reservatorio) {
          nivel_agua = altura_reservatorio - distancia;
        }

        Serial.print("Nível da água: ");
        Serial.print(nivel_agua);
        Serial.println(" cm");

        char nivelStr[8];
        dtostrf(nivel_agua, 4, 2, nivelStr);

        client.publish(topic_nivel, nivelStr, true);
      } else {
        Serial.print("Bomba desligada. Último nível conhecido: ");
        Serial.print(nivel_agua);
        Serial.println(" cm (não atualizando)");
      }
    }
  }
}
