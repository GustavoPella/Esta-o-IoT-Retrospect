# Estação IoT de Condições de Trilhas Ecológicas — Retrospect

Projeto de Internet das Coisas que simula, no [Wokwi](https://wokwi.com/), uma estação
instalada no início de uma trilha ecológica. Ela mede temperatura, umidade do ar,
luminosidade, umidade do solo, chuva acumulada e número de visitantes, calcula um
**nível de segurança da trilha** (verde / amarelo / vermelho) e publica tudo por
Wi-Fi + MQTT para um painel do aplicativo Retrospect — além de mostrar o resultado
localmente num LED RGB e num display OLED.

Projeto Wokwi original: https://wokwi.com/projects/479795922649225217

Relatório completo (contexto, requisitos, justificativa dos componentes, cronograma
e limitações): [`docs/projeto_iot_trilhas.pdf`](docs/projeto_iot_trilhas.pdf).

## Como rodar no Wokwi

1. Abra o projeto original no link acima **ou** crie um novo projeto ESP32 no Wokwi e
   substitua `diagram.json` e `sketch.ino` pelos deste repositório.
2. Garanta que `libraries.txt` está na mesma pasta — o Wokwi baixa as bibliotecas
   automaticamente a partir dele.
3. O `secrets.h` já incluído neste pacote funciona sem nenhuma alteração: ele usa a
   rede virtual `Wokwi-GUEST` (sem senha) e o broker público `broker.hivemq.com`
   (sem autenticação) — valores públicos, sem nada sensível.
4. Clique em ▶️ para iniciar a simulação.

## Credenciais (`secrets.h`)

O Wi-Fi/MQTT são escritos direto no `.ino` — eles vêm de um arquivo
separado, `secrets.h`, que o `.gitignore` já exclui do controle de versão.

- **Para rodar a simulação como está**: não precisa fazer nada, o `secrets.h` deste
  pacote já tem os valores públicos de teste.
- **Para uma instalação real**: copie `secrets.example.h` para `secrets.h` e preencha
  com o Wi-Fi do local e um broker MQTT próprio, com autenticação e,
  idealmente, TLS (porta 8883) — o broker público usado na simulação não tem senha
  nem criptografia e serve só para demonstração (ver seção 4.2 do relatório).

## Hardware simulado

| Componente | Função | Pino no ESP32 |
|---|---|---|
| DHT22 | temperatura e umidade do ar | GPIO 4 |
| LDR (fotoresistor) | luminosidade | GPIO 34 (ADC1) |
| Potenciômetro | simula umidade do solo (0–100%) | GPIO 35 (ADC1) |
| Botão | simula pluviômetro (cada clique = 0,2 mm) | GPIO 27 |
| PIR HC-SR501 | conta visitantes | GPIO 26 |
| LED RGB | nível da trilha | GPIO 25 / 33 / 32 (R/G/B) |
| Buzzer | alerta sonoro | GPIO 13 |
| Display OLED SSD1306 (I2C) | leituras + nível na estação | GPIO 21 / 22 (SDA/SCL) |

Pinos analógicos ficam no ADC1 (GPIO 32–39) de propósito: o ADC2 tem conflito
conhecido com o Wi-Fi no ESP32.

## Comunicação MQTT

| Tópico | Direção | Conteúdo |
|---|---|---|
| `retrospect/trilha01/dados` | estação → painel | leituras + nível atual, em JSON, a cada `INTERVALO_MS` (retido) |
| `retrospect/trilha01/cmd` | painel → estação | comandos para a estação |

Exemplo de payload publicado:
```json
{"temp":24.5,"umid_ar":71,"luz":38,"solo":62,"chuva_mm":3.2,"visitantes":14,"nivel":"amarelo"}
```

Comandos aceitos em `retrospect/trilha01/cmd`:
```json
{"intervalo":60}
{"alerta":"vermelho"}   // também aceita "amarelo", "verde" ou "auto"
{"buzzer":true}
```

## Regra do nível da trilha

| Nível | Condição |
|---|---|
| Vermelho | umidade do solo > 80% OU chuva acumulada > 20 mm na janela |
| Amarelo | umidade do solo entre 60–80% OU temperatura > 32 °C |
| Verde | nenhuma das anteriores |

Os limiares são ponto de partida e precisariam ser calibrados com dados reais numa
instalação em campo.

## Compilando fora do Wokwi (opcional)

O código foi validado com o `arduino-cli`, configurado para a placa ESP32, usando as
bibliotecas de `libraries.txt`:

```
arduino-cli core install esp32:esp32
arduino-cli lib install "DHT sensor library" "Adafruit Unified Sensor" \
  "Adafruit GFX Library" "Adafruit SSD1306" "PubSubClient"
arduino-cli compile --fqbn esp32:esp32:esp32 .
```