//====================== BIBLIOTECAS ======================
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include "DHT.h"
#include <RTClib.h>

//====================== VARIÁVEIS GLOBAIS ======================
short int menuatual = 0, opcao = 0;             // controla qual tela está ativa
#define CHUVA 2                                // pino do sensor de chuva (INPUT_PULLUP)

#define DHTpin 5                               // pino do sensor DHT22
#define DHTmodel DHT22
DHT dht(DHTpin, DHTmodel);

RTC_DS1307 rtc;                                // objeto para o RTC
LiquidCrystal_I2C lcd(0x27, 16, 2);            // endereço I2C 0x27, LCD 16x2

// mapeamento dos pinos do keypad 4x4
const uint8_t ROWS = 4;
const uint8_t COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '-', 'D' }
};
uint8_t colPins[COLS] = { 9, 8, 7, 6 };
uint8_t rowPins[ROWS] = { 13, 12, 11, 10 };
Keypad kpd = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//====================== ENDEREÇOS EEPROM ======================
#define CFG_UNIDADE_TEMP_ADDR  0   // uint16_t: 1=Celsius, 2=Fahrenheit
#define CFG_FLAGDIST_ADDR      2   // uint16_t: limite de distância (0..500 cm)
#define CFG_FLAGTEMP_ADDR      4   // uint16_t: limite de temperatura (0..100 °C)
#define CFG_FLAGUMID_ADDR      6   // uint16_t: limite de umidade (0..100 %)
#define CFG_COOLDOWN_ADDR      8   // uint16_t: cooldown entre flags (1..60 min)
#define ENDERECO_INICIAL_FLAGS 20  // onde começam as flags de evento

int enderecoEEPROM;                         // ponteiro pra gravar flags

//====================== VARIÁVEIS DE CONFIGURAÇÃO ======================
uint16_t unidadeTemperatura;  // 1 = Celsius, 2 = Fahrenheit
uint16_t flagDistancia;       // 0..500 cm (nível de água: quanto menor, mais perto do sensor)
uint16_t flagTemperatura;     // 0..100 °C
uint16_t flagUmidade;         // 0..100 %
uint16_t flagCooldown;        // 1..60 minutos

// constante para converter Unix time (1970) para época do RTClib (2000)
static const uint32_t EPOCH_OFFSET = 946684800UL;  // segundos de 1970 até 2000

// carrega valores do EEPROM (se já tiverem sido configurados)
void definevars() {
  EEPROM.get(CFG_UNIDADE_TEMP_ADDR, unidadeTemperatura);
  EEPROM.get(CFG_FLAGDIST_ADDR, flagDistancia);
  EEPROM.get(CFG_FLAGTEMP_ADDR, flagTemperatura);
  EEPROM.get(CFG_FLAGUMID_ADDR, flagUmidade);
  EEPROM.get(CFG_COOLDOWN_ADDR, flagCooldown);
}

// primeira vez que o sistema liga: grava valores de fábrica no EEPROM
void primeirosetup() {
  if (EEPROM.read(1001) != 1) {
    // valores padrão de fábrica
    unidadeTemperatura = 1;    // Celsius
    flagDistancia     = 200;  // 200 cm (distância mínima antes de alarmar)
    flagTemperatura   = 30;   // 30 °C
    flagUmidade       = 70;   // 70%
    flagCooldown      = 10;   // 10 minutos de cooldown

    EEPROM.put(CFG_UNIDADE_TEMP_ADDR, unidadeTemperatura);
    EEPROM.put(CFG_FLAGDIST_ADDR, flagDistancia);
    EEPROM.put(CFG_FLAGTEMP_ADDR, flagTemperatura);
    EEPROM.put(CFG_FLAGUMID_ADDR, flagUmidade);
    EEPROM.put(CFG_COOLDOWN_ADDR, flagCooldown);

    enderecoEEPROM = ENDERECO_INICIAL_FLAGS;
    EEPROM.put(1010, enderecoEEPROM);
    EEPROM.write(1001, 1);
  }
}

//====================== FUNÇÃO DE INPUT (digitação na 2ª linha) ======================
int modoInput(int valorAtual, int minimo, int maximo) {
  // mostra o intervalo na primeira linha: "min--max"
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(minimo);
  lcd.print("--");
  lcd.print(maximo);

  // move pra segunda linha e aguarda digitação
  lcd.setCursor(0, 1);
  char key = '\0';
  char numChar[9] = "";
  int digitosvalidos = 0;

  while (key != 'D') {
    key = kpd.getKey();
    // se for dígito ou '-' no início
    if ((key >= '0' && key <= '9' && digitosvalidos < 8) || (key == '-' && digitosvalidos == 0)) {
      numChar[digitosvalidos] = key;
      lcd.setCursor(digitosvalidos, 1);
      lcd.print(numChar[digitosvalidos]);
      digitosvalidos++;
    } else if (key == 'C') {
      // aborta e retorna o valor anterior
      return valorAtual;
    }
  }

  numChar[digitosvalidos] = '\0';
  int num = atoi(numChar);

  if (num < minimo || num > maximo) {
    // valor fora de intervalo, avisa e tenta de novo
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Valor invalido");
    delay(1000);
    return modoInput(valorAtual, minimo, maximo);
  } else {
    return num;
  }
}

//====================== SEQUÊNCIA DE CONFIGURAÇÃO ======================
void sequencialConfig() {
  // 1) Limite Distância (nível de água – quanto menor, mais perto do sensor)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Limite Distancia");
  lcd.setCursor(0, 1);
  lcd.print("(0-500 cm)");
  delay(1000);
  flagDistancia = modoInput(flagDistancia, 0, 500);
  EEPROM.put(CFG_FLAGDIST_ADDR, flagDistancia);

  // 2) Limite Temperatura
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Limite Temperat.");
  lcd.setCursor(0, 1);
  lcd.print("(0-100 C)");
  delay(1000);
  flagTemperatura = modoInput(flagTemperatura, 0, 100);
  EEPROM.put(CFG_FLAGTEMP_ADDR, flagTemperatura);

  // 3) Limite Umidade
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Limite Umidade");
  lcd.setCursor(0, 1);
  lcd.print("(0-100 %)");
  delay(1000);
  flagUmidade = modoInput(flagUmidade, 0, 100);
  EEPROM.put(CFG_FLAGUMID_ADDR, flagUmidade);

  // 4) Unidade Temperatura (Celsius ou Fahrenheit)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Unidade Temp");
  lcd.setCursor(0, 1);
  lcd.print("1=C  2=F");
  delay(1000);
  unidadeTemperatura = modoInput(unidadeTemperatura, 1, 2);
  EEPROM.put(CFG_UNIDADE_TEMP_ADDR, unidadeTemperatura);

  // 5) Cooldown entre flags (minutos)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Cooldown Flags");
  lcd.setCursor(0, 1);
  lcd.print("(1-60 min)");
  delay(1000);
  flagCooldown = modoInput(flagCooldown, 1, 60);
  EEPROM.put(CFG_COOLDOWN_ADDR, flagCooldown);

  // finaliza a configuração
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Setup Concluido");
  delay(1500);

  menuatual = 0;  // volta ao menu principal
}

//====================== LEITURA ULTRASSÔNICA ======================
int leituraDaAgua() {
  // dispara pulso e mede retorno em cm
  digitalWrite(4, HIGH);
  delayMicroseconds(10);
  digitalWrite(4, LOW);
  int distancia = pulseIn(3, HIGH) / 58;  // converte para cm
  return distancia;
}

//====================== MODO DE LEITURA (tela de sensores) - CORRIGIDO ======================
void modoLeitura() {
  unsigned long timerPrint = millis();
  unsigned long timerDisplay = millis();
  bool mostrandoLeitura1 = true;  // alterna entre água vs. temp/umid

  // variáveis estáticas pra manter estado entre loops
  static bool eventoAtivo = false;
  static uint32_t timestampInicio = 0;
  static int picoNivel = 0;

  while (true) {
    char tecla = kpd.getKey();
    if (tecla == 'C') {
      lcd.clear();
      menuatual = 0;
      return;  // volta ao menu principal
    }

    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();
    if (isnan(temp) || isnan(hum)) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Erro sensor");
      delay(1000);
      continue;
    }

    int nivelAgua = leituraDaAgua();

    // alterna entre os dois displays a cada 5 segundos
    if (millis() - timerDisplay >= 5000) {
      timerDisplay = millis();
      mostrandoLeitura1 = !mostrandoLeitura1;
      lcd.clear();
    }

    // atualiza a cada 500ms
    if (millis() - timerPrint >= 500) {
      timerPrint = millis();
      if (mostrandoLeitura1) {
        // mostra nível de água e limite configurado
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Agua:");
        lcd.print(nivelAgua);
        lcd.print("cm");
        lcd.setCursor(0, 1);
        lcd.print("Lim:");
        lcd.print(flagDistancia);
        lcd.print("cm");
      } else {
        // mostra temperatura e umidade
        lcd.clear();
        lcd.setCursor(0, 0);
        if (unidadeTemperatura == 2) {
          float f = temp * 1.8 + 32;
          lcd.print("T:");
          lcd.print(f, 1);
          lcd.print("F");
        } else {
          lcd.print("T:");
          lcd.print(temp, 1);
          lcd.print("C");
        }
        lcd.setCursor(0, 1);
        lcd.print("H:");
        lcd.print(hum, 1);
        lcd.print("%");
      }
    }

    bool chovendo = (digitalRead(CHUVA) == HIGH);
    uint32_t agoraUnix = rtc.now().unixtime();  // segundos desde 1970

    // Se ainda não iniciou evento, condição invertida: água próxima (<= limite)
    if (!eventoAtivo) {
      if (nivelAgua <= flagDistancia && chovendo) {
        eventoAtivo = true;
        timestampInicio = agoraUnix;
        picoNivel = nivelAgua;
        Serial.println(">> Evento iniciado!");

        // Mostra a data corretamente
        DateTime dtIni = rtc.now();
        Serial.print("Início: ");
        Serial.print(dtIni.year());   Serial.print("-");
        Serial.print(dtIni.month());  Serial.print("-");
        Serial.print(dtIni.day());    Serial.print(" ");
        Serial.print(dtIni.hour());   Serial.print(":");
        Serial.print(dtIni.minute()); Serial.print(":");
        Serial.println(dtIni.second());
      }
    } else {
      // atualiza pico nível (menor distância = pico)
      if (nivelAgua < picoNivel) {
        picoNivel = nivelAgua;
      }
      // termina evento quando parar de chover e água ficar longe do sensor (> limite)
      if (!chovendo && nivelAgua > flagDistancia) {
        eventoAtivo = false;
        uint32_t timestampFim = agoraUnix;
        Serial.println(">> Evento finalizado!");

        // Usa DateTime diretamente do RTC
        DateTime dtInicio = DateTime(timestampInicio);
        DateTime dtFim = rtc.now();

        Serial.print("Início: ");
        Serial.print(dtInicio.year());   Serial.print("-");
        Serial.print(dtInicio.month());  Serial.print("-");
        Serial.print(dtInicio.day());    Serial.print(" ");
        Serial.print(dtInicio.hour());   Serial.print(":");
        Serial.print(dtInicio.minute()); Serial.print(":");
        Serial.println(dtInicio.second());

        Serial.print("Fim: ");
        Serial.print(dtFim.year());      Serial.print("-");
        Serial.print(dtFim.month());     Serial.print("-");
        Serial.print(dtFim.day());       Serial.print(" ");
        Serial.print(dtFim.hour());      Serial.print(":");
        Serial.print(dtFim.minute());    Serial.print(":");
        Serial.println(dtFim.second());

        Serial.print("Pico nível: ");
        Serial.print(picoNivel);
        Serial.println(" cm");

        // grava na EEPROM: tIni (4 bytes), tFim (4 bytes), picoNivel (2 bytes)
        if (enderecoEEPROM + 10 <= 1000) {
          EEPROM.put(enderecoEEPROM, timestampInicio);
          enderecoEEPROM += 4;
          EEPROM.put(enderecoEEPROM, timestampFim);
          enderecoEEPROM += 4;
          uint16_t picouint = (uint16_t)picoNivel;
          EEPROM.put(enderecoEEPROM, picouint);
          enderecoEEPROM += 2;
          
          // IMPORTANTE: Salva o novo endereço na EEPROM!
          EEPROM.put(1010, enderecoEEPROM);
          
          Serial.println(">> FLAG gravada na EEPROM");
          Serial.print("Próximo endereço: ");
          Serial.println(enderecoEEPROM);
        } else {
          Serial.println("!!! EEPROM cheia");
        }
      }
    }

    delay(50);
  }
}

//====================== DEBUG EEPROM (via Serial) - CORRIGIDO ======================
void debugEEPROM() {
  Serial.println("===== DEBUG EEPROM =====");
  uint16_t val16;

  EEPROM.get(CFG_UNIDADE_TEMP_ADDR, val16);
  Serial.print("Unidade Temp: "); Serial.println(val16);

  EEPROM.get(CFG_FLAGDIST_ADDR, val16);
  Serial.print("Limite Dist (cm): "); Serial.println(val16);

  EEPROM.get(CFG_FLAGTEMP_ADDR, val16);
  Serial.print("Limite Temp (C): "); Serial.println(val16);

  EEPROM.get(CFG_FLAGUMID_ADDR, val16);
  Serial.print("Limite Umid (%): "); Serial.println(val16);

  EEPROM.get(CFG_COOLDOWN_ADDR, val16);
  Serial.print("Cooldown (min): "); Serial.println(val16);

  // Lê o endereço atual da EEPROM ANTES de usar
  int enderecoSalvo;
  EEPROM.get(1010, enderecoSalvo);
  Serial.print("End EEPROM flags: "); Serial.println(enderecoSalvo);

  Serial.println("\n===== FLAGS SALVAS =====");
  int endereco = ENDERECO_INICIAL_FLAGS;
  int count = 0;

  // Usa o endereço salvo, não a variável global
  if (enderecoSalvo <= ENDERECO_INICIAL_FLAGS || enderecoSalvo > 1000) {
    Serial.println("Nenhuma flag registrada.");
    return;
  }

  // Lê apenas até o endereço salvo
  while (endereco < enderecoSalvo && endereco + 10 <= 1000) {
    uint32_t tIni, tFim;
    uint16_t pico;

    EEPROM.get(endereco, tIni);
    
    // Verifica se é um timestamp válido
    if (tIni == 0xFFFFFFFF || tIni == 0) {
      break;
    }
    
    endereco += 4;
    EEPROM.get(endereco, tFim);
    
    // Verifica se o timestamp de fim é válido
    if (tFim == 0xFFFFFFFF || tFim == 0 || tFim < tIni) {
      break;
    }
    
    endereco += 4;
    EEPROM.get(endereco, pico);
    
    // Verifica se o pico é válido (0-500 cm)
    if (pico == 0xFFFF || pico > 500) {
      break;
    }
    
    endereco += 2;

    // Usa DateTime diretamente com timestamp Unix
    DateTime dIni(tIni);
    DateTime dFim(tFim);
    
    // Verifica se as datas são razoáveis
    if (dIni.year() < 2000 || dIni.year() > 2100 || 
        dFim.year() < 2000 || dFim.year() > 2100) {
      Serial.println("Data inválida detectada");
      continue;
    }

    Serial.print("[FLAG "); Serial.print(++count); Serial.println("]");
    Serial.print("Início: ");
    Serial.print(dIni.year()); Serial.print("-");
    Serial.print(dIni.month()); Serial.print("-");
    Serial.print(dIni.day()); Serial.print(" ");
    Serial.print(dIni.hour()); Serial.print(":");
    Serial.print(dIni.minute()); Serial.print(":");
    Serial.println(dIni.second());

    Serial.print("Fim: ");
    Serial.print(dFim.year()); Serial.print("-");
    Serial.print(dFim.month()); Serial.print("-");
    Serial.print(dFim.day()); Serial.print(" ");
    Serial.print(dFim.hour()); Serial.print(":");
    Serial.print(dFim.minute()); Serial.print(":");
    Serial.println(dFim.second());

    Serial.print("Pico nível: ");
    Serial.print(pico); Serial.println(" cm");
    Serial.println("---------------------");
  }

  if (count == 0) {
    Serial.println("Nenhuma flag válida encontrada.");
  } else {
    Serial.print("Total de flags: ");
    Serial.println(count);
  }
}

//====================== LIMPA FLAGS DE EVENTO - CORRIGIDO ======================
void limparEEPROMFlags() {
  // Limpa apenas a área de flags
  for (int i = ENDERECO_INICIAL_FLAGS; i <= 1000; i++) {
    EEPROM.update(i, 0xFF);
  }
  
  // Reseta o ponteiro para o início
  enderecoEEPROM = ENDERECO_INICIAL_FLAGS;
  EEPROM.put(1010, enderecoEEPROM);
  
  Serial.println("EEPROM de flags limpa.");
  Serial.print("Próximo endereço de escrita: ");
  Serial.println(enderecoEEPROM);
}

//====================== INICIALIZAÇÃO DO HARDWARE ======================
void begins() {
  dht.begin();
  lcd.init();
  lcd.backlight();
  kpd.setDebounceTime(5);
  rtc.begin();
  // se o RTC não estiver rodando, seta com a data/hora da compilação
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

//====================== SETUP CORRIGIDO ======================
void setup() {
  Serial.begin(9600);
  
  // Primeiro carrega as variáveis
  definevars();
  
  // Depois faz o primeiro setup se necessário
  primeirosetup();
  
  // Carrega o endereço atual da EEPROM
  EEPROM.get(1010, enderecoEEPROM);
  
  // Valida o endereço carregado
  if (enderecoEEPROM < ENDERECO_INICIAL_FLAGS || enderecoEEPROM > 1000) {
    enderecoEEPROM = ENDERECO_INICIAL_FLAGS;
    EEPROM.put(1010, enderecoEEPROM);
  }
  
  // Continua com o resto da inicialização
  begins();
  pinMode(CHUVA, INPUT_PULLUP);
  pinMode(4, OUTPUT);
  pinMode(3, INPUT);
  
  // Debug: mostra o endereço carregado
  Serial.print("Endereço EEPROM carregado: ");
  Serial.println(enderecoEEPROM);
}

//====================== MENU PRINCIPAL COM NAVEGAÇÃO A/B ======================
void loop() {
  static int escolhaPrincipal = 0;
  const int numOpcoes = 4;
  // máximo 16 caracteres por linha
  const char* opcoes[numOpcoes] = {
    "1. Leitura",
    "2. Configuracao",
    "3. Logs",
    "4. LimparFlags"
  };
  const char* descricoes[numOpcoes] = {
    "Mostra sensores",
    "Ajusta parametros",
    "Mostra flags",
    "Apaga flags"
  };

  switch (menuatual) {
    case 1:  // Leitura de sensores
      modoLeitura();
      break;

    case 2:  // Configuração sequencial
      sequencialConfig();
      break;

    case 3:  // Logs (debug na Serial)
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Visualizar Logs");
      lcd.setCursor(0, 1);
      lcd.print("A=D  C=Voltar");
      while (true) {
        char k = kpd.getKey();
        if (k == 'D') {
          debugEEPROM();
          delay(2000);
          break;
        } else if (k == 'C') {
          break;
        }
      }
      menuatual = 0;
      break;

    case 4:  // Limpar Flags
      limparEEPROMFlags();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Flags apagadas");
      delay(1000);
      menuatual = 0;
      break;

    case 0:  // Menu Principal (padrão)
    default:
      // desenha a opção e descrição atuais
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(opcoes[escolhaPrincipal]);
      lcd.setCursor(0, 1);
      lcd.print(descricoes[escolhaPrincipal]);

      // espera usuário navegar ou selecionar
      while (true) {
        char k = kpd.getKey();
        if (k == 'A') {
          // sobe no menu (wrap-around)
          escolhaPrincipal = (escolhaPrincipal - 1 + numOpcoes) % numOpcoes;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(opcoes[escolhaPrincipal]);
          lcd.setCursor(0, 1);
          lcd.print(descricoes[escolhaPrincipal]);
        }
        else if (k == 'B') {
          // desce no menu
          escolhaPrincipal = (escolhaPrincipal + 1) % numOpcoes;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(opcoes[escolhaPrincipal]);
          lcd.setCursor(0, 1);
          lcd.print(descricoes[escolhaPrincipal]);
        }
        else if (k == 'D') {
          // seleciona a opção
          menuatual = escolhaPrincipal + 1; // mapeia pra case 1..4
          break;
        }
        // ignora outras teclas (incluindo C)
      }
      break;
  }
}