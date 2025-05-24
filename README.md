# Controle Inteligente de Reservatório com ESP32 e MQTT

Este projeto implementa um sistema de controle automático para o nível de água em um reservatório, utilizando um microcontrolador ESP32, um sensor ultrassônico HC-SR04 e comunicação via protocolo MQTT para monitoramento e controle remoto.

## Descrição do Funcionamento

O sistema monitora continuamente o nível da água no reservatório usando o sensor ultrassônico HC-SR04. O ESP32 processa a leitura do sensor, calcula o nível atual da água e compara com os níveis mínimo e máximo pré-definidos.

- Se o nível cair abaixo do mínimo, o ESP32 aciona um relé que liga uma bomba d'água de 12V para encher o reservatório.
- Se o nível atingir o máximo, o ESP32 desliga a bomba.
- O ESP32 conecta-se a uma rede Wi-Fi e publica o nível atual da água e o status da bomba (LIGADA/DESLIGADA) em um broker MQTT.
- O sistema também pode receber comandos via MQTT para ligar/desligar a bomba manualmente ou retornar ao modo automático.

## Hardware Utilizado

1.  **Placa Microcontroladora ESP32:** (Ex: ESP32 DevKitC V4)
2.  **Sensor Ultrassônico HC-SR04:** Para medição de distância/nível.
3.  **Módulo Relé DC 5V:** (Ex: SRD-05VDC-SL-C) Para acionar a bomba DC. **Importante:** Não usar relé AC (como o SSR 40DA) para cargas DC.
4.  **Bomba d'Água Submersível 12V DC:** Atuador para bombear a água.
5.  **Módulo Conversor de Nível Lógico Bidirecional 3.3V/5V:** Para comunicação segura entre ESP32 (3.3V) e HC-SR04 (5V).
6.  **Fonte de Alimentação Chaveada:** Com saídas de 12V DC (para a bomba) e 5V DC (para ESP32, relé, sensor, conversor).
7.  **Protoboard e Jumpers:** Para conexões.

*(Opcional: Caixa/Gabinete para montagem final)*

## Software e Bibliotecas

1.  **Arduino IDE:** Ambiente de desenvolvimento para programar o ESP32.
2.  **Bibliotecas Arduino:**
    *   `WiFi.h` (Padrão do ESP32 Core)
    *   `PubSubClient.h` (Instalar via Gerenciador de Bibliotecas da Arduino IDE)
3.  **Broker MQTT:** Um servidor MQTT (Ex: Mosquitto, EMQX, HiveMQ) rodando localmente ou na nuvem.
4.  **Cliente MQTT:** (Opcional, para teste/monitoramento) Ex: MQTT Explorer, Node-RED, Home Assistant.

## Montagem e Conexões

As conexões elétricas detalhadas estão descritas no arquivo `Aplicando_Conhecimento_3_ABNT.docx` (Seção 3) ou no arquivo `circuit_connections.txt` gerado anteriormente. Um resumo:

- **Alimentação:** Conecte as saídas 5V e 12V da fonte aos respectivos componentes, garantindo um GND comum para todo o sistema.
- **Sensor HC-SR04:** Conecte VCC e GND à fonte 5V/GND. Conecte TRIG e ECHO aos pinos HV do conversor de nível.
- **Conversor de Nível:** Conecte HV/GND(HV) à fonte 5V/GND. Conecte LV/GND(LV) aos pinos 3V3/GND do ESP32. Conecte LV1 e LV2 aos pinos GPIO definidos no código para TRIG e ECHO (ex: 23 e 22).
- **Módulo Relé DC 5V:** Conecte VCC e GND à fonte 5V/GND. Conecte IN ao pino GPIO definido no código para controle do relé (ex: 21). Conecte COM à saída 12V+ da fonte. Conecte NO ao terminal positivo (+) da bomba.
- **Bomba 12V DC:** Conecte o terminal positivo (+) ao NO do relé. Conecte o terminal negativo (-) ao GND comum (que está conectado à saída 12V- da fonte).
- **ESP32:** Conecte VIN e GND à fonte 5V/GND.

*(Recomenda-se criar um diagrama Fritzing com base na descrição textual para visualização)*

## Configuração do Código (`esp32_controle_reservatorio.ino`)

Antes de carregar o código no ESP32, você **precisa** editar as seguintes seções no arquivo `.ino`:

1.  **Configurações de Rede:**
    ```cpp
    const char* ssid = "SEU_WIFI_SSID";
    const char* password = "SUA_WIFI_SENHA";
    ```
2.  **Configurações do Broker MQTT:**
    ```cpp
    const char* mqtt_server = "SEU_MQTT_BROKER_IP"; // IP ou hostname
    const char* mqtt_user = ""; // Usuário (se aplicável)
    const char* mqtt_password = ""; // Senha (se aplicável)
    ```
3.  **(Opcional) Pinos do Hardware:** Se você usar pinos GPIO diferentes dos definidos:
    ```cpp
    const int pino_trig = 23;
    const int pino_echo = 22;
    const int pino_rele = 21;
    ```
4.  **Parâmetros de Controle:** Ajuste conforme seu reservatório:
    ```cpp
    const float altura_reservatorio_cm = 100.0;
    const float nivel_minimo_cm = 20.0;
    const float nivel_maximo_cm = 80.0;
    ```

## Interfaces e Protocolos (MQTT)

O sistema utiliza o protocolo MQTT para comunicação via TCP/IP.

**Tópicos Publicados pelo ESP32:**

*   `reservatorio/nivel_cm`: Publica o nível atual da água em centímetros (ex: `65.75`).
*   `reservatorio/status_bomba`: Publica o estado atual da bomba (string: `LIGADA` ou `DESLIGADA`).

**Tópicos Subscritos pelo ESP32 (para controle):**

*   `reservatorio/comando/bomba`: Recebe comandos para controle manual.
    *   Payload `ON`: Liga a bomba (ativa modo manual).
    *   Payload `OFF`: Desliga a bomba (ativa modo manual).
    *   Payload `AUTO`: Retorna ao controle automático baseado nos níveis.

## Como Usar e Reproduzir

1.  **Monte o Hardware:** Conecte os componentes conforme descrito na seção de Montagem.
2.  **Configure o Broker MQTT:** Instale e configure um broker MQTT (como Mosquitto) em um servidor local ou use um broker público (ex: `test.mosquitto.org`, `broker.hivemq.com`).
3.  **Configure o Código:** Edite o arquivo `esp32_controle_reservatorio.ino` com suas credenciais de Wi-Fi e detalhes do broker MQTT.
4.  **Carregue o Código:** Abra o arquivo `.ino` na Arduino IDE, selecione a placa ESP32 correta e a porta serial, e carregue o código.
5.  **Monitore a Saída Serial:** Abra o Monitor Serial na Arduino IDE (baud rate 115200) para ver logs de conexão, leituras de sensor e status.
6.  **Monitore/Controle via MQTT:** Use um cliente MQTT (como MQTT Explorer) para:
    *   Subscrever aos tópicos `reservatorio/nivel_cm` e `reservatorio/status_bomba` para ver os dados.
    *   Publicar mensagens (`ON`, `OFF`, `AUTO`) no tópico `reservatorio/comando/bomba` para controlar o sistema.

## Documentação Adicional

- O código fonte (`esp32_controle_reservatorio.ino`) contém comentários explicando as seções principais.
- O artigo completo do projeto (`Aplicando_Conhecimento_3_ABNT.docx` ou similar) contém mais detalhes sobre a metodologia, funcionamento e referências.

