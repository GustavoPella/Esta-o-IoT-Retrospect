#ifndef SECRETS_H
#define SECRETS_H

/*
  Modelo de arquivo de credenciais.

  1. Copie este arquivo para "secrets.h" (mesma pasta).
  2. Preencha os valores abaixo.
  3. NUNCA versione o "secrets.h" real — ele já está no .gitignore.

  Os valores abaixo são os de uma instalação REAL (produção), com Wi-Fi
  próprio e um broker MQTT privado com autenticação — ver seção 4.2 do
  relatório (docs/projeto_iot_trilhas.pdf) sobre por que o broker público
  usado na simulação não deve ir para produção.
*/

// Wi-Fi da instalação real
const char *WIFI_SSID  = "NOME_DA_REDE_WIFI";
const char *WIFI_SENHA = "SENHA_DA_REDE_WIFI";

// Broker MQTT próprio (produção) — recomenda-se TLS (porta 8883) e autenticação
const char *MQTT_BROKER  = "seu-broker-mqtt.exemplo.com";
const int   MQTT_PORTA   = 8883;
const char *MQTT_USUARIO = "usuario_mqtt";
const char *MQTT_SENHA   = "senha_mqtt";

#endif
