//====================== BIBLIOTECAS ======================
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include "DHT.h"
#include <RTClib.h>

//====================== CONFIGURAÇÃO ======================
#define BUZZER_PIN    A3     // pino do buzzer
#define HYSTERESIS    10     // cm de folga para desligar o evento

//====================== ENDEREÇOS EEPROM ======================
#define CFG_UNIDADE_TEMP_ADDR   0   // uint16_t: 1=Celsius, 2=Fahrenheit
#define CFG_FLAGDIST_ADDR       2   // uint16_t: limite de água (cm)
#define CFG_COOLDOWN_ADDR       8   // uint16_t: cooldown (min) — opcional
#define ENDERECO_INICIAL_FLAGS 20   // onde começam as flags de evento

//====================== VARIÁVEIS GLOBAIS ======================
short int menuatual = 0;         // controla tela ativa
int enderecoEEPROM;              // ponteiro pra gravar flags

uint16_t unidadeTemperatura;     // 1=Celsius, 2=Fahrenheit
uint16_t flagDistancia;          // 0..500 cm
uint16_t flagCooldown;           // 1..60 min (não usado aqui)

//====================== HARDWARE ===========================
#define DHTpin    5
#define DHTmodel  DHT22
DHT dht(DHTpin, DHTmodel);

RTC_DS1307 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// keypad 4x4
const uint8_t ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
  { '1','2','3','A' },
  { '4','5','6','B' },
  { '7','8','9','C' },
  { '*','0','-','D' }
};
uint8_t colPins[COLS] = { 9,8,7,6 };
uint8_t rowPins[ROWS] = { 13,12,11,10 };
Keypad kpd = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//====================== EEPROM IO ==========================
void definevars() {
  EEPROM.get(CFG_UNIDADE_TEMP_ADDR, unidadeTemperatura);
  EEPROM.get(CFG_FLAGDIST_ADDR, flagDistancia);
  EEPROM.get(CFG_COOLDOWN_ADDR, flagCooldown);
}

void primeirosetup() {
  if (EEPROM.read(1001) != 1) {
    unidadeTemperatura = 1;
    flagDistancia     = 200;
    flagCooldown      = 10;
    EEPROM.put(CFG_UNIDADE_TEMP_ADDR, unidadeTemperatura);
    EEPROM.put(CFG_FLAGDIST_ADDR, flagDistancia);
    EEPROM.put(CFG_COOLDOWN_ADDR, flagCooldown);
    enderecoEEPROM = ENDERECO_INICIAL_FLAGS;
    EEPROM.put(1010, enderecoEEPROM);
    EEPROM.write(1001, 1);
  }
}

//====================== LEITURA DO ULTRASSÔNICO =============
int leituraDaAgua() {
  digitalWrite(4, HIGH);
  delayMicroseconds(10);
  digitalWrite(4, LOW);
  return pulseIn(3, HIGH) / 58;  // cm
}

//====================== MODO DE LEITURA ======================
void modoLeitura() {
  unsigned long timerPrint   = millis();
  unsigned long timerDisplay = millis();
  bool mostrando = true;

  static bool   eventoAtivo    = false;
  static uint32_t timestampInicio = 0;
  float somaTemp = 0, somaHum = 0;
  uint16_t countSamples = 0;

  while (true) {
    char tecla = kpd.getKey();
    if (tecla == 'C') {
      digitalWrite(BUZZER_PIN, LOW);
      lcd.clear();
      menuatual = 0;
      return;
    }

    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();
    if (isnan(temp) || isnan(hum)) {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Erro sensor");
      delay(1000);
      continue;
    }

    int nivelAgua = leituraDaAgua();
    uint32_t agora = rtc.now().unixtime();

    // alterna display a cada 5s
    if (millis() - timerDisplay >= 5000) {
      timerDisplay = millis();
      mostrando = !mostrando;
      lcd.clear();
    }

    // atualiza dados a cada 500ms
    if (millis() - timerPrint >= 500) {
      timerPrint = millis();
      lcd.clear();
      if (mostrando) {
        lcd.setCursor(0,0);
        lcd.print("Agua:");
        lcd.print(nivelAgua);
        lcd.print("cm");
        lcd.setCursor(0,1);
        lcd.print("Lim:");
        lcd.print(flagDistancia);
        lcd.print("cm");
      } else {
        lcd.setCursor(0,0);
        if (unidadeTemperatura == 2) {
          float f = temp * 1.8 + 32;
          lcd.print("T:");
          lcd.print(f,1);
          lcd.print("F");
        } else {
          lcd.print("T:");
          lcd.print(temp,1);
          lcd.print("C");
        }
        lcd.setCursor(0,1);
        lcd.print("H:");
        lcd.print(hum,1);
        lcd.print("%");
      }
    }

    if (!eventoAtivo) {
      // inicia evento
      if (nivelAgua <= flagDistancia) {
        eventoAtivo = true;
        timestampInicio = agora;
        somaTemp = somaHum = 0;
        countSamples = 0;
        digitalWrite(BUZZER_PIN, HIGH);
        Serial.println(">> Evento iniciado!");
      }
    } else {
      // dentro do evento: acumula
      somaTemp += temp;
      somaHum  += hum;
      countSamples++;

      // finaliza só quando ultrapassar limite + histerese
      if (nivelAgua > flagDistancia + HYSTERESIS) {
        eventoAtivo = false;
        digitalWrite(BUZZER_PIN, LOW);
        uint32_t timestampFim = agora;

        float avgTemp   = somaTemp / countSamples;
        float avgHumMed = somaHum  / countSamples;
        float percAcima = ((float)flagDistancia - (float)nivelAgua) / flagDistancia * 100.0;

        Serial.println(">> Evento finalizado!");
        Serial.print("Duração(s): "); Serial.println(timestampFim - timestampInicio);
        Serial.print("T avg: ");     Serial.println(avgTemp);
        Serial.print("H avg: ");     Serial.println(avgHumMed);
        Serial.print("%acima: ");    Serial.println(percAcima);

        // grava na EEPROM: tIni(4), tFim(4), avgTemp*10(2), avgHum*10(2), percAcima(2)
        if (enderecoEEPROM + 14 <= 1000) {
          EEPROM.put(enderecoEEPROM, timestampInicio);  enderecoEEPROM += 4;
          EEPROM.put(enderecoEEPROM, timestampFim);     enderecoEEPROM += 4;
          uint16_t tLog = (uint16_t)(avgTemp   * 10);
          uint16_t hLog = (uint16_t)(avgHumMed * 10);
          uint16_t pLog = (uint16_t)(percAcima);
          EEPROM.put(enderecoEEPROM, tLog);             enderecoEEPROM += 2;
          EEPROM.put(enderecoEEPROM, hLog);             enderecoEEPROM += 2;
          EEPROM.put(enderecoEEPROM, pLog);             enderecoEEPROM += 2;
          EEPROM.put(1010, enderecoEEPROM);
          Serial.println(">> FLAG gravada na EEPROM");
        } else {
          Serial.println("!!! EEPROM cheia");
        }
      }
    }

    delay(50);
  }
}

//====================== DEBUG EEPROM ======================
void debugEEPROM() {
  Serial.println("===== DEBUG FLAGS =====");
  int end = ENDERECO_INICIAL_FLAGS;
  int endSalvo; EEPROM.get(1010, endSalvo);
  int count = 0;

  while (end < endSalvo && end + 14 <= 1000) {
    uint32_t tIni, tFim;
    uint16_t tLog, hLog, pLog;
    EEPROM.get(end, tIni); end += 4;
    EEPROM.get(end, tFim); end += 4;
    EEPROM.get(end, tLog); end += 2;
    EEPROM.get(end, hLog); end += 2;
    EEPROM.get(end, pLog); end += 2;

    DateTime dIni(tIni), dFim(tFim);
    Serial.print("[FLAG "); Serial.print(++count); Serial.println("]");
    Serial.print("Início: "); Serial.println(dIni.timestamp());
    Serial.print("Fim:    "); Serial.println(dFim.timestamp());
    Serial.print("T méd:  "); Serial.println((float)tLog / 10.0);
    Serial.print("H méd:  "); Serial.println((float)hLog / 10.0);
    Serial.print("%acima: "); Serial.println(pLog);
  }
  Serial.print("Total: "); Serial.println(count);
}

//====================== LIMPA FLAGS ======================
void limparEEPROMFlags() {
  for (int i = ENDERECO_INICIAL_FLAGS; i <= 1000; i++) {
    EEPROM.update(i, 0xFF);
  }
  enderecoEEPROM = ENDERECO_INICIAL_FLAGS;
  EEPROM.put(1010, enderecoEEPROM);
  Serial.println("EEPROM limpa");
}

//====================== INICIALIZAÇÃO HARDWARE ============
void begins() {
  dht.begin();
  lcd.init(); lcd.backlight();
  kpd.setDebounceTime(5);
  rtc.begin();
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(3, INPUT);
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

//====================== SETUP =============================
void setup() {
  Serial.begin(9600);
  definevars();
  primeirosetup();
  EEPROM.get(1010, enderecoEEPROM);
  if (enderecoEEPROM < ENDERECO_INICIAL_FLAGS || enderecoEEPROM > 1000) {
    enderecoEEPROM = ENDERECO_INICIAL_FLAGS;
    EEPROM.put(1010, enderecoEEPROM);
  }
  begins();
  Serial.print("EEPROM ptr: "); Serial.println(enderecoEEPROM);
}

//====================== LOOP ==============================
void loop() {
  static int escolha = 0;
  const char* op[4]   = { "1.Leitura", "2.Config", "3.Logs", "4.Limpar" };
  const char* desc[4] = { "Sensores",  "Ajustes",   "Ver Flags", "Apagar" };

  lcd.clear();
  lcd.setCursor(0,0); lcd.print(op[escolha]);
  lcd.setCursor(0,1); lcd.print(desc[escolha]);

  char k = kpd.getKey();
  if (k == 'A') escolha = (escolha + 3) % 4;
  if (k == 'B') escolha = (escolha + 1) % 4;
  if (k == 'D') menuatual = escolha + 1;

  switch (menuatual) {
    case 1: modoLeitura();       break;
    case 2: /* sequencialConfig() */ break;
    case 3: debugEEPROM(); delay(2000); break;
    case 4: limparEEPROMFlags(); delay(1000); break;
  }
  if (menuatual > 0) menuatual = 0;
}
