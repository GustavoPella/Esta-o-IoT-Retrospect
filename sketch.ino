
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "secrets.h"
#define PIN_DHT      4
#define PIN_LDR      34
#define PIN_SOLO     35
#define PIN_CHUVA    27
#define PIN_PIR      26
#define PIN_LED_R    25
#define PIN_LED_G    33
#define PIN_LED_B    32
#define PIN_BUZZER   13

#define OLED_LARGURA 128
#define OLED_ALTURA  64
#define OLED_RESET   -1

DHT dht(PIN_DHT, DHT22);
Adafruit_SSD1306 display(OLED_LARGURA, OLED_ALTURA, &Wire, OLED_RESET);

const char *TOPICO_CMD   = "retrospect/trilha01/cmd";

WiFiClient redeWifi;
PubSubClient mqtt(redeWifi);

unsigned long INTERVALO_MS = 10000;
const unsigned long JANELA_CHUVA_MS = 180000;
const float MM_POR_CLIQUE = 0.2;

const float LIMIAR_SOLO_VERMELHO = 80.0;
const float LIMIAR_SOLO_AMARELO  = 60.0;
const float LIMIAR_CHUVA_VERMELHO_MM = 20.0;
const float LIMIAR_TEMP_AMARELO_C = 32.0;

unsigned long ultimoEnvio = 0;
unsigned long ultimaLeituraBotao = 0;
unsigned long ultimaLeituraPir = 0;

float chuvaAcumulada_mm = 0;
unsigned long marcasChuva[40];
int totalMarcasChuva = 0;

unsigned long visitantesHoje = 0;
bool pirEstadoAnterior = LOW;

String nivelForcado = "auto";
String nivelAtual = "verde";
bool buzzerForcado = false;
unsigned long buzzerForcadoAte = 0;

void atualizarRegistroDeChuva() {
  unsigned long agora = millis();
  int restantes = 0;
  for (int i = 0; i < totalMarcasChuva; i++) {
    if (agora - marcasChuva[i] <= JANELA_CHUVA_MS) {
      marcasChuva[restantes] = marcasChuva[i];
      restantes++;
    }
  }
  totalMarcasChuva = restantes;
  chuvaAcumulada_mm = totalMarcasChuva * MM_POR_CLIQUE;
}

void registrarCliqueDeChuva() {
  if (totalMarcasChuva < 40) {
    marcasChuva[totalMarcasChuva] = millis();
    totalMarcasChuva++;
  }
  atualizarRegistroDeChuva();
}

String calcularNivel(float temperaturaC, float soloPct, float chuvaMm) {
  if (nivelForcado != "auto") {
    return nivelForcado;
  }
  if (soloPct > LIMIAR_SOLO_VERMELHO || chuvaMm > LIMIAR_CHUVA_VERMELHO_MM) {
    return "vermelho";
  }
  if (soloPct > LIMIAR_SOLO_AMARELO || temperaturaC > LIMIAR_TEMP_AMARELO_C) {
    return "amarelo";
  }
  return "verde";
}

void definirCorLed(const String &nivel) {
  bool r = false, g = false, b = false;
  if (nivel == "verde")        { g = true; }
  else if (nivel == "amarelo") { r = true; g = true; }
  else if (nivel == "vermelho"){ r = true; }
  digitalWrite(PIN_LED_R, r ? HIGH : LOW);
  digitalWrite(PIN_LED_G, g ? HIGH : LOW);
  digitalWrite(PIN_LED_B, b ? HIGH : LOW);
}

void atualizarBuzzer(const String &nivel) {
  bool ligar = (nivel == "vermelho");
  if (buzzerForcado && millis() < buzzerForcadoAte) {
    ligar = true;
  } else {
    buzzerForcado = false;
  }
  digitalWrite(PIN_BUZZER, ligar ? HIGH : LOW);
}

void atualizarDisplay(float tempC, float umidAr, int luz, float soloPct,
                      float chuvaMm, unsigned long visitantes, const String &nivel) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("Retrospect - Trilha 01");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 14);
  display.print("Temp: ");  display.print(tempC, 1);  display.println(" C");
  display.print("Umid.ar: "); display.print(umidAr, 0); display.println(" %");
  display.print("Luz: ");   display.println(luz);
  display.print("Solo: ");  display.print(soloPct, 0); display.println(" %");
  display.print("Chuva: "); display.print(chuvaMm, 1); display.println(" mm");
  display.print("Visit.: "); display.println(visitantes);

  display.setCursor(0, 56);
  display.print("Nivel: ");
  display.println(nivel);
  display.display();
}

void conectarWifi() {
  Serial.print("Conectando ao Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_SENHA);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println(" conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void aoReceberMensagem(char *topico, byte *payload, unsigned int tamanho) {
  String msg;
  for (unsigned int i = 0; i < tamanho; i++) {
    msg += (char)payload[i];
  }
  Serial.print("Comando recebido: ");
  Serial.println(msg);

  int posIntervalo = msg.indexOf("\"intervalo\"");
  if (posIntervalo >= 0) {
    int doisPontos = msg.indexOf(':', posIntervalo);
    long novoValor = msg.substring(doisPontos + 1).toInt();
    if (novoValor > 0) {
      INTERVALO_MS = (unsigned long)novoValor * 1000UL;
      Serial.print("Novo intervalo (ms): ");
      Serial.println(INTERVALO_MS);
    }
  }

  if (msg.indexOf("\"alerta\":\"vermelho\"") >= 0) nivelForcado = "vermelho";
  else if (msg.indexOf("\"alerta\":\"amarelo\"") >= 0) nivelForcado = "amarelo";
  else if (msg.indexOf("\"alerta\":\"verde\"") >= 0) nivelForcado = "verde";
  else if (msg.indexOf("\"alerta\":\"auto\"") >= 0) nivelForcado = "auto";

  if (msg.indexOf("\"buzzer\":true") >= 0) {
    buzzerForcado = true;
    buzzerForcadoAte = millis() + 5000;
  }
}

void reconectarMqtt() {
  while (!mqtt.connected()) {
    Serial.print("Conectando ao broker MQTT...");
    String idCliente = "retrospect-trilha01-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    bool conectado;
    if (strlen(MQTT_USUARIO) > 0) {
      conectado = mqtt.connect(idCliente.c_str(), MQTT_USUARIO, MQTT_SENHA);
    } else {
      conectado = mqtt.connect(idCliente.c_str());
    }
    if (conectado) {
      Serial.println(" conectado!");
      mqtt.subscribe(TOPICO_CMD);
    } else {
      Serial.print("falhou, codigo = ");
      Serial.print(mqtt.state());
      Serial.println(" tentando de novo em 2 s");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_CHUVA, INPUT_PULLUP);
  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  dht.begin();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Nao achei o display OLED");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 20);
    display.println("Retrospect");
    display.println("Iniciando estacao...");
    display.display();
  }

  conectarWifi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORTA);
  mqtt.setCallback(aoReceberMensagem);
}

void loop() {
  if (!mqtt.connected()) {
    reconectarMqtt();
  }
  mqtt.loop();

  if (digitalRead(PIN_CHUVA) == LOW && millis() - ultimaLeituraBotao > 300) {
    registrarCliqueDeChuva();
    ultimaLeituraBotao = millis();
    Serial.println("Clique de chuva registrado (+0,2 mm)");
  }
  atualizarRegistroDeChuva();

  bool pirAgora = digitalRead(PIN_PIR);
  if (pirAgora == HIGH && pirEstadoAnterior == LOW && millis() - ultimaLeituraPir > 500) {
    visitantesHoje++;
    ultimaLeituraPir = millis();
    Serial.println("Visitante detectado");
  }
  pirEstadoAnterior = pirAgora;

  if (millis() - ultimoEnvio >= INTERVALO_MS) {
    ultimoEnvio = millis();

    float temperaturaC = dht.readTemperature();
    float umidadeAr = dht.readHumidity();
    if (isnan(temperaturaC) || isnan(umidadeAr)) {
      Serial.println("Falha ao ler o DHT22, usando ultima leitura valida");
      temperaturaC = isnan(temperaturaC) ? 25.0 : temperaturaC;
      umidadeAr = isnan(umidadeAr) ? 60.0 : umidadeAr;
    }

    int luzBruta = analogRead(PIN_LDR);
    int luzPct = map(luzBruta, 0, 4095, 0, 100);

    int soloBruto = analogRead(PIN_SOLO);
    float soloPct = map(soloBruto, 0, 4095, 0, 100);

    String nivel = calcularNivel(temperaturaC, soloPct, chuvaAcumulada_mm);
    nivelAtual = nivel;
    definirCorLed(nivel);
    atualizarBuzzer(nivel);
    atualizarDisplay(temperaturaC, umidadeAr, luzPct, soloPct, chuvaAcumulada_mm, visitantesHoje, nivel);

    char payload[220];
    snprintf(payload, sizeof(payload),
      "{\"temp\":%.1f,\"umid_ar\":%.0f,\"luz\":%d,\"solo\":%.0f,\"chuva_mm\":%.1f,\"visitantes\":%lu,\"nivel\":\"%s\"}",
      temperaturaC, umidadeAr, luzPct, soloPct, chuvaAcumulada_mm, visitantesHoje, nivel.c_str());

    mqtt.publish(TOPICO_DADOS, payload, true);
    Serial.print("Publicado: ");
    Serial.println(payload);
  }

  atualizarBuzzer(nivelAtual);
}
