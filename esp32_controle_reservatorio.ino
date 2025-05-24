# Código Fonte - Controle de Nível de Reservatório com ESP32 e MQTT

```cpp
#include <WiFi.h>
#include <PubSubClient.h>

// --- Configurações de Rede ---
const char* ssid = "SEU_WIFI_SSID";         // Substitua pelo SSID da sua rede Wi-Fi
const char* password = "SUA_WIFI_SENHA";     // Substitua pela senha da sua rede Wi-Fi

// --- Configurações do Broker MQTT ---
const char* mqtt_server = "SEU_MQTT_BROKER_IP"; // Substitua pelo IP ou hostname do seu broker MQTT (ex: "192.168.1.100" ou "test.mosquitto.org")
const int mqtt_port = 1883;                     // Porta padrão do MQTT
const char* mqtt_user = "";                      // Usuário MQTT (se houver)
const char* mqtt_password = "";                  // Senha MQTT (se houver)
const char* clientID = "ESP32_Reservatorio_Client"; // ID único para este cliente MQTT

// --- Tópicos MQTT ---
const char* topic_nivel_agua = "reservatorio/nivel_cm";       // Tópico para publicar o nível da água em cm
const char* topic_status_bomba = "reservatorio/status_bomba"; // Tópico para publicar o status da bomba (LIGADA/DESLIGADA)
const char* topic_comando_bomba = "reservatorio/comando/bomba"; // Tópico para receber comandos manuais (ON/OFF)

// --- Pinos do Hardware ---
const int pino_trig = 23; // Pino TRIG do HC-SR04 conectado ao GPIO 23 (via conversor de nível)
const int pino_echo = 22; // Pino ECHO do HC-SR04 conectado ao GPIO 22 (via conversor de nível)
const int pino_rele = 21; // Pino IN do Módulo Relé conectado ao GPIO 21

// --- Parâmetros de Controle ---
const float altura_reservatorio_cm = 100.0; // Altura total do reservatório em cm (ou altura do sensor ao fundo)
const float nivel_minimo_cm = 20.0;       // Nível mínimo para ligar a bomba
const float nivel_maximo_cm = 80.0;       // Nível máximo para desligar a bomba

// --- Variáveis Globais ---
WiFiClient espClient;
PubSubClient client(espClient);

long duration;          // Duração do pulso ECHO
float distancia_cm;     // Distância medida pelo sensor
float nivel_agua_cm;    // Nível calculado da água
bool estado_bomba = false; // false = DESLIGADA, true = LIGADA
bool controle_manual = false; // Indica se o controle manual está ativo

unsigned long tempo_ultima_leitura = 0;
const long intervalo_leitura = 5000; // Intervalo entre leituras do sensor (ms) - 5 segundos

unsigned long tempo_ultima_publicacao = 0;
const long intervalo_publicacao = 15000; // Intervalo entre publicações MQTT (ms) - 15 segundos

// --- Protótipos das Funções ---
void setup_wifi();
void reconnect_mqtt();
void callback_mqtt(char* topic, byte* payload, unsigned int length);
float ler_nivel_agua();
void controlar_bomba(float nivel);
void publicar_status();

// --- Configuração Inicial (Setup) ---
void setup() {
  Serial.begin(115200);
  Serial.println("\nIniciando Sistema de Controle de Reservatório...");

  pinMode(pino_trig, OUTPUT);
  pinMode(pino_echo, INPUT);
  pinMode(pino_rele, OUTPUT);
  digitalWrite(pino_rele, LOW); // Garante que a bomba comece desligada
  estado_bomba = false;

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback_mqtt);
}

// --- Loop Principal ---
void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop(); // Mantém a conexão MQTT e processa mensagens recebidas

  unsigned long tempo_atual = millis();

  // Realiza leitura do sensor em intervalos definidos
  if (tempo_atual - tempo_ultima_leitura >= intervalo_leitura) {
    tempo_ultima_leitura = tempo_atual;
    nivel_agua_cm = ler_nivel_agua();

    if (nivel_agua_cm >= 0) { // Verifica se a leitura foi válida
      Serial.print("Nível da Água: ");
      Serial.print(nivel_agua_cm);
      Serial.println(" cm");

      // Controla a bomba apenas se o controle manual não estiver ativo
      if (!controle_manual) {
          controlar_bomba(nivel_agua_cm);
      }
    } else {
      Serial.println("Erro na leitura do sensor.");
    }
  }

  // Publica status via MQTT em intervalos definidos
  if (tempo_atual - tempo_ultima_publicacao >= intervalo_publicacao) {
    tempo_ultima_publicacao = tempo_atual;
    if (client.connected()) {
        publicar_status();
    }
  }
}

// --- Funções Auxiliares ---

// Conecta ao Wi-Fi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) { // Tenta por ~10 segundos
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if(WiFi.status() == WL_CONNECTED){
    Serial.println("\nWiFi conectado!");
    Serial.print("Endereço IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha ao conectar ao WiFi. Verifique as credenciais ou sinal.");
    // Poderia tentar reiniciar o ESP32 aqui ou entrar em modo de configuração
  }
}

// Reconecta ao Broker MQTT
void reconnect_mqtt() {
  while (!client.connected()) {
    Serial.print("Tentando conectar ao MQTT Broker...");
    // Tenta conectar com usuário/senha se definidos
    bool conectado;
    if (strlen(mqtt_user) > 0) {
        conectado = client.connect(clientID, mqtt_user, mqtt_password);
    } else {
        conectado = client.connect(clientID);
    }

    if (conectado) {
      Serial.println("Conectado!");
      // Subscreve ao tópico de comando da bomba após conectar
      if(client.subscribe(topic_comando_bomba)){
          Serial.print("Subscrito ao tópico: ");
          Serial.println(topic_comando_bomba);
      } else {
          Serial.println("Falha ao subscrever ao tópico de comando.");
      }
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos");
      delay(5000); // Espera 5 segundos antes de tentar novamente
    }
  }
}

// Callback para mensagens MQTT recebidas
void callback_mqtt(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida [ ");
  Serial.print(topic);
  Serial.print(" ]: ");
  payload[length] = '\0'; // Adiciona terminador nulo para tratar como string
  String mensagem = String((char*)payload);
  Serial.println(mensagem);

  // Verifica se a mensagem é para o tópico de comando da bomba
  if (strcmp(topic, topic_comando_bomba) == 0) {
    if (mensagem.equalsIgnoreCase("ON")) {
      Serial.println("Comando manual: Ligar bomba");
      digitalWrite(pino_rele, HIGH);
      estado_bomba = true;
      controle_manual = true; // Ativa controle manual
      publicar_status(); // Publica o novo status imediatamente
    } else if (mensagem.equalsIgnoreCase("OFF")) {
      Serial.println("Comando manual: Desligar bomba");
      digitalWrite(pino_rele, LOW);
      estado_bomba = false;
      controle_manual = true; // Ativa controle manual
      publicar_status(); // Publica o novo status imediatamente
    } else if (mensagem.equalsIgnoreCase("AUTO")) {
        Serial.println("Comando: Retornar ao controle automático");
        controle_manual = false; // Desativa controle manual
        // A próxima leitura do sensor aplicará a lógica automática
    }
  }
}

// Lê a distância do sensor HC-SR04 e calcula o nível da água
float ler_nivel_agua() {
  // Gera o pulso TRIG
  digitalWrite(pino_trig, LOW);
  delayMicroseconds(2);
  digitalWrite(pino_trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pino_trig, LOW);

  // Lê a duração do pulso ECHO
  duration = pulseIn(pino_echo, HIGH, 30000); // Timeout de 30ms

  // Calcula a distância em cm (Velocidade do som ~343 m/s ou 0.0343 cm/us)
  distancia_cm = duration * 0.0343 / 2.0;

  // Calcula o nível da água
  if (distancia_cm > 0 && distancia_cm < altura_reservatorio_cm) { // Leitura válida dentro da altura
    nivel_agua_cm = altura_reservatorio_cm - distancia_cm;
    return nivel_agua_cm;
  } else if (distancia_cm >= altura_reservatorio_cm) { // Sensor detectou o fundo (ou muito perto)
      return 0.0; // Nível zero
  } else {
    return -1.0; // Indica erro de leitura (timeout ou valor inválido)
  }
}

// Controla a bomba com base no nível da água (lógica de histerese)
void controlar_bomba(float nivel) {
  if (nivel <= nivel_minimo_cm && !estado_bomba) { // Abaixo do mínimo e bomba desligada
    Serial.println("Nível baixo, ligando a bomba...");
    digitalWrite(pino_rele, HIGH);
    estado_bomba = true;
    publicar_status(); // Publica mudança de estado
  } else if (nivel >= nivel_maximo_cm && estado_bomba) { // Acima do máximo e bomba ligada
    Serial.println("Nível alto, desligando a bomba...");
    digitalWrite(pino_rele, LOW);
    estado_bomba = false;
    publicar_status(); // Publica mudança de estado
  }
  // Se estiver entre os níveis, mantém o estado atual
}

// Publica o nível da água e o status da bomba via MQTT
void publicar_status() {
  if (!client.connected()) {
      Serial.println("MQTT não conectado, não é possível publicar.");
      return;
  }

  // Publica Nível da Água
  char nivel_str[8];
  dtostrf(nivel_agua_cm, 4, 2, nivel_str); // Converte float para string
  if(client.publish(topic_nivel_agua, nivel_str)){
      //Serial.print("Nível publicado: "); Serial.println(nivel_str);
  } else {
      Serial.println("Falha ao publicar nível.");
  }

  // Publica Status da Bomba
  const char* status_bomba_str = estado_bomba ? "LIGADA" : "DESLIGADA";
  if(client.publish(topic_status_bomba, status_bomba_str)){
      //Serial.print("Status da bomba publicado: "); Serial.println(status_bomba_str);
  } else {
      Serial.println("Falha ao publicar status da bomba.");
  }
  Serial.print("Status publicado - Nível: "); Serial.print(nivel_str); Serial.print(" cm, Bomba: "); Serial.println(status_bomba_str);
}

```

**Observações sobre o Código:**

1.  **Substitua os Placeholders:** É **essencial** substituir `SEU_WIFI_SSID`, `SUA_WIFI_SENHA` e `SEU_MQTT_BROKER_IP` pelos valores corretos da sua rede e do seu broker MQTT. Se o broker exigir autenticação, preencha `mqtt_user` e `mqtt_password`.
2.  **Bibliotecas:** Certifique-se de ter as bibliotecas `PubSubClient` instaladas na sua Arduino IDE (Ferramentas -> Gerenciar Bibliotecas...). A biblioteca `WiFi.h` já vem com o suporte ao ESP32.
3.  **Sensor Ultrassônico:** Este código usa `pulseIn` para ler o sensor HC-SR04. Para leituras mais robustas ou sensores diferentes (como o JSN-SR04T à prova d'água), pode ser preferível usar bibliotecas como `NewPing`.
4.  **Conversor de Nível:** O código assume que o conversor de nível lógico está sendo usado corretamente entre o ESP32 (3.3V) e o HC-SR04 (5V) para os pinos TRIG e ECHO.
5.  **Relé:** O código controla o `pino_rele` (GPIO 21). Certifique-se de que este pino está conectado ao pino de controle (IN) do seu **Módulo Relé DC 5V** (e não ao SSR 40DA).
6.  **Parâmetros:** Ajuste `altura_reservatorio_cm`, `nivel_minimo_cm` e `nivel_maximo_cm` de acordo com as dimensões e necessidades do seu reservatório.
7.  **Controle Manual:** O código inclui a capacidade de controlar a bomba manualmente enviando mensagens "ON" ou "OFF" para o tópico `reservatorio/comando/bomba`. Enviando "AUTO" retorna ao controle automático.
8.  **Error Handling:** O código inclui verificações básicas de conexão Wi-Fi e MQTT e validação da leitura do sensor, mas pode ser expandido com tratamento de erros mais robusto.
9.  **Intervalos:** Os intervalos de leitura (`intervalo_leitura`) e publicação MQTT (`intervalo_publicacao`) podem ser ajustados conforme necessário.
