# pluvia-sensor

**Grupo:** Pluvia  
**Descrição:** Sistema embarcado de monitoramento ambiental com detecção de chuva e nível da água, utilizando Arduino, sensores digitais e armazenamento em EEPROM para registros históricos.

## Simulação Interativa

Para visualizar o funcionamento do sistema em tempo real, acesse o projeto simulado no Wokwi:

[Acessar Simulação no Wokwi](https://wokwi.com/projects/432776839423735809)

O ambiente virtual permite testar sensores, interações no LCD, comportamento do sistema sob chuva e visualizar os registros de eventos diretamente via terminal serial.

![projeto-montado](https://i.imgur.com/ZwtZY0B.png)

---

## Objetivo

Este projeto tem como finalidade detectar eventos críticos relacionados à proximidade de água em situações de chuva. O sistema faz a leitura contínua de sensores de **nível de água (ultrassônico)**, **temperatura e umidade (DHT22)** e **presença de chuva (sensor digital)**. Eventos são registrados com data/hora utilizando um **RTC DS1307**, e persistidos na **EEPROM** do microcontrolador para futura auditoria.

---

## Componentes Utilizados

- Arduino Uno/Nano
- Sensor ultrassônico (HC-SR04)
- Sensor de chuva digital
- Sensor DHT22 (temperatura e umidade)
- RTC DS1307 (com bateria)
- LCD 16x2 com módulo I2C (0x27)
- Teclado matricial 4x4 (Keypad)
- Buzzer (opcional)
- EEPROM interna (via `EEPROM.h`)

---

## Funcionalidades

- **Leitura em tempo real** de:
  - Nível da água (em cm)
  - Temperatura (°C ou °F)
  - Umidade relativa do ar (%)
  - Estado do sensor de chuva

- **Menu interativo via LCD + Keypad**
  - Navegação com teclas `A`, `B`, `D` e `C`
  - Leitura, Configuração, Logs e Limpeza de dados

- **Detecção de eventos**:
  - Início quando: está chovendo **e** nível da água **abaixo** do limite (ou seja, água muito próxima)
  - Fim quando: parou de chover **e** nível de água acima do limite
  - Armazena: data/hora de início, data/hora de fim, e pico do nível de água (menor distância detectada)

- **EEPROM**
  - Registro permanente de eventos
  - Endereços 0–20 reservados para configuração
  - Endereço 20 em diante: flags de eventos (máx. 98 eventos)
  - Flag: `tInicio (4B) + tFim (4B) + pico (2B) = 10B/evento`

---

## Parâmetros Configuráveis

Ajustáveis pelo menu `Configuração`:

| Parâmetro               | Intervalo        | Endereço EEPROM |
|-------------------------|------------------|------------------|
| Unidade de Temperatura  | 1 = °C, 2 = °F    | `0`              |
| Limite Nível de Água    | 0–500 cm          | `2`              |
| Limite Temperatura      | 0–100 °C          | `4`              |
| Limite Umidade          | 0–100 %           | `6`              |
| Cooldown entre Flags    | 1–60 minutos      | `8`              |

---

## Lógica de Funcionamento

1. O sistema inicializa sensores e carrega configurações da EEPROM.
2. O usuário pode navegar entre os modos disponíveis:
   - **Leitura**: alterna entre valores de sensores em tempo real.
   - **Configuração**: ajusta parâmetros via digitação com Keypad.
   - **Logs**: imprime via Serial os eventos registrados.
   - **LimparFlags**: apaga todos os eventos salvos na EEPROM.
3. Quando um evento de risco é detectado:
   - O tempo de início é armazenado.
   - O menor nível de distância (pico) é monitorado.
   - Ao fim do evento, os dados são persistidos na EEPROM.

---

## Comportamento Esperado

- Durante a leitura:
  - LCD alterna a cada 5 segundos entre:
    - Nível da água (cm) e limite
    - Temperatura (°C/°F) e umidade (%)
- Flags são criadas apenas se:
  - Sensor de chuva está **ativado**
  - Nível da água está **abaixo ou igual ao limite**
- Eventos são ignorados se o cooldown entre flags ainda não expirou (futura implementação)

---

## Debug e Logs

Via `Serial Monitor (9600 baud)`:

- Exibição das configurações atuais
- Listagem completa de eventos gravados com:
  - Data/hora de início
  - Data/hora de término
  - Pico do nível de água (cm)

---

## Organização de Código

| Seção                      | Descrição                                                      |
|----------------------------|----------------------------------------------------------------|
| Bibliotecas                | Inclusão de todas as dependências externas                     |
| Variáveis Globais          | Estados do menu, mapeamento do Keypad, endereços EEPROM        |
| Inicialização              | Setup de sensores, LCD, RTC e EEPROM                           |
| Menu Principal             | Navegação entre funcionalidades com descrições no LCD          |
| Funções Utilitárias        | Entrada de dados, leitura ultrassônica, escrita na EEPROM      |
| Modo de Leitura            | Lógica central de detecção e monitoramento de eventos          |
| Debug e Reset              | Funções para ver ou limpar dados persistentes                  |

---

## Observações Técnicas

- Os valores de tempo são armazenados em **Unix Time (desde 1970)**.
- Para compatibilidade com o RTC (`RTClib`), é necessário aplicar o **offset de época de 946684800 segundos**.
- O sistema usa `pulseIn` para calcular o tempo de retorno do ultrassônico.
- O endereço de EEPROM para flags (`1010`) é usado para manter controle do próximo slot de gravação.

---

## Desenvolvido por

Grupo **Pluvia** – Projeto embarcado com foco em automação, monitoramento ambiental e resiliência de dados.

---

## Status

**Concluído** – Versão estável com funcionalidades completas para operação em campo simulado. Futuras melhorias podem incluir integração com comunicação remota (LoRa/Wi-Fi) e painel gráfico via dashboard.
