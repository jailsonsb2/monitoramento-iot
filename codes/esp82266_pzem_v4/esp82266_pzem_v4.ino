/*
 * CÓDIGO V4.1 - PFE DE MONITORAMENTO DE ENERGIA
 * Versão com Autenticação MQTT
 * Inclui: WiFiManager (Tolerância a Falhas de Comunicação)
 * Inclui: Estrutura Não-Bloqueante (Tolerância a Falhas de Sistema/WDT)
 * Inclui: LED de Status (Heartbeat)
 */

// --- 1. BIBLIOTECAS ---
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <PZEM004Tv30.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <WiFiManager.h>      
#include <ArduinoOTA.h>       

// --- 2. CONFIGURAÇÕES ---
const char* MQTT_SERVIDOR = "192.168.3.3"; // O IP PÚBLICO DO SERVIDOR OCI
const int   MQTT_PORTA = 1883;
const char* MQTT_TOPICO = "tcc/pzem/data"; 

// --- SUAS CREDENCIAIS MQTT ---
const char* MQTT_USUARIO = "pfeuser"; 
const char* MQTT_SENHA = "pfeiot";   

// Variáveis para o cronômetro não-bloqueante (Envio a cada 10s)
long lastMsg = 0; 
const int INTERVALO_ENVIO = 10000; // 10 segundos (10000 ms)

// --- 3. CONFIGURAÇÃO DO SENSOR PZEM ---
#define PZEM_RX_PIN 14  
#define PZEM_TX_PIN 12  
SoftwareSerial pzemSerial;
PZEM004Tv30 pzem(pzemSerial); 

// --- 4. CONFIGURAÇÃO DO WIFI E MQTT ---
WiFiClient espClient;
PubSubClient client(espClient);

// --- NOVO: PINO DO LED INTERNO ---
#define LED_PIN LED_BUILTIN 

// --- 5. FUNÇÃO DE SETUP (INICIALIZAÇÃO) ---
void setup() {
  Serial.begin(115200); 
  Serial.println("\nIniciando...");

  pzemSerial.begin(9600, SWSERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);

  // --- NOVO: CONFIGURA O LED ---
  pinMode(LED_PIN, OUTPUT); 
  digitalWrite(LED_PIN, HIGH); // Garante que o LED comece desligado (HIGH = OFF) 

  // --- INICIALIZAÇÃO DO WIFIMANAGER ---
  WiFiManager wm; 
  wm.setTimeout(180); 
  Serial.println("Tentando conectar ao Wi-Fi...");
  if (!wm.autoConnect("MeuPZEM-Setup")) { 
    Serial.println("Falha ao conectar e o tempo limite expirou. Reiniciando...");
    delay(3000);
    ESP.restart(); 
  } 
  
  Serial.println("\nWiFi conectado!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());

  
  client.setServer(MQTT_SERVIDOR, MQTT_PORTA);

  // --- INICIALIZAÇÃO DO OTA ---
  configurarOTA();
}


// --- 6. LOOP PRINCIPAL (NÃO-BLOQUEANTE) ---
void loop() {
  ArduinoOTA.handle(); // Verifica por novas atualizações OTA
  client.loop();       // Mantém o cliente MQTT vivo

  // Checa se o tempo de 10 segundos passou
  if (millis() - lastMsg > INTERVALO_ENVIO) { 
    lastMsg = millis(); // Reseta o cronômetro

    if (!client.connected()) {
      reconectarMQTT();
    }
    
    // Ler TODOS os dados do sensor PZEM
    float tensao = pzem.voltage();
    float corrente = pzem.current();
    float potencia = pzem.power();
    float energia = pzem.energy();
    float frequencia = pzem.frequency();
    float fator_potencia = pzem.pf();

    if (!isnan(tensao) && !isnan(corrente) && !isnan(potencia) && !isnan(energia) && !isnan(frequencia) && !isnan(fator_potencia)) {
      Serial.println("--- NOVA LEITURA ---");
      enviarDadosMQTT(tensao, corrente, potencia, energia, frequencia, fator_potencia);
    } else {
      Serial.println("Erro ao ler dados do PZEM. Verifique a fiação.");
    }
  } 
}


// --- 7. FUNÇÕES AUXILIARES ---

void reconectarMQTT() {
  while (!client.connected()) {
    Serial.print("Tentando conectar ao MQTT (Broker)... ");
    
    // --- USA AS CREDENCIAIS AQUI ---
    if (client.connect("ESP8266_Client_TCC", MQTT_USUARIO, MQTT_SENHA)) {
      Serial.println("Conectado!");
    } else {
      Serial.print("Falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000); 
    }
  }
}

void enviarDadosMQTT(float tensao, float corrente, float potencia, float energia, float frequencia, float fator_potencia) {
  StaticJsonDocument<256> doc; 
  doc["tensao"] = tensao;
  doc["corrente"] = corrente;
  doc["potencia"] = potencia;
  doc["energia"] = energia;
  doc["frequencia"] = frequencia;
  doc["fator_potencia"] = fator_potencia; 

  char buffer_json[256]; 
  serializeJson(doc, buffer_json);

  Serial.print("Publicando no tópico ");
  Serial.print(MQTT_TOPICO);
  Serial.print(" -> ");
  Serial.println(buffer_json);
  
  boolean publicado = client.publish(MQTT_TOPICO, buffer_json);
  
  if (publicado) {
    Serial.println("Publicado com sucesso!");

    // --- NOVO: PISCA O LED PARA CONFIRMAR O ENVIO ---
    digitalWrite(LED_PIN, LOW); // Liga o LED (lógica invertida) 
    delay(500); // Deixa o LED aceso por 0.5 segundo para ser visível 
    digitalWrite(LED_PIN, HIGH); // Desliga o LED 

  } else {
    Serial.println("Falha ao publicar.");
  }
}

// --- 8. FUNÇÃO DE CONFIGURAÇÃO DO OTA ---
void configurarOTA() { 
  ArduinoOTA.setHostname("PFE-TCC-OTA"); 
  ArduinoOTA.setPassword("PFEIOT"); // <-- SENHA DO OTA (não é a senha do MQTT)

  ArduinoOTA.onStart([]() {
    Serial.println("Iniciando atualização OTA...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nFim da atualização.");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progresso: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Erro[%u]: ", error);
  });
  
  ArduinoOTA.begin();
  Serial.println("OTA (Atualização pela rede) está pronto!");
}