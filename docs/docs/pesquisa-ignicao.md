# Pesquisa: Controle de Ignição para ECU Programável

## 1. Objetivo

Levantar como projetos de ECU programável open source e literatura técnica tratam o controle de ignição (spark timing, dwell, acionamento de bobinas), para embasar a criação das próximas tasks de firmware do **Programmable ECU** e garantir que as decisões de arquitetura sigam práticas consolidadas.

## 2. Conceitos fundamentais

### 2.1 Dwell time

Dwell é o tempo em que a bobina primária fica energizada antes da faísca. Durante esse intervalo a corrente sobe e armazena energia no campo magnético da bobina.

- Energia armazenada: \(E = \frac{1}{2} L I^2\). Pequenas reduções de corrente causam perda quadrática de energia na faísca.
- Dwell curto → faísca fraca, falta de ignição (misfire).
- Dwell longo → saturação magnética, aquecimento excessivo da bobina, risco de danos.
- O dwell ideal depende da indutância/resistência da bobina e da tensão da bateria. ECUs comerciais usam **mapa de dwell vs RPM e vs tensão da bateria**.
- Driver inteligentes (ex.: FAN1110B-F085) limitam corrente e possuem *max-dwell timer* por hardware, desligando o IGBT se o pulso ficar ativo além do tempo programado.

### 2.2 Ângulo de ignição (spark advance)

O ponto de faísca é medido em graus do virabrequim em relação ao PMS (ponto morto superior). Avançar a ignição aumenta torque até um limite; retardar protege contra detonação.

ECUs programáveis usam:
- **Mapa 3D**: avanço × RPM × carga (MAP ou TPS).
- **Correções**: temperatura do ar, temperatura do líquido de arrefecimento, detonação, partida a frio, etc.

### 2.3 Roda fônica (trigger wheel) e referência de ângulo

A posição do virabrequim é lida por uma roda fônica. As mais comuns são **36-1** (36 dentes faltando 1) e **60-2** (60 dentes faltando 2). O espaço vazio é o ponto de sincronismo.

- Ângulo entre dentes: 36-1 → 10°; 60-2 → 6°.
- A posição do primeiro dente após a falha raramente coincide com o PMS da cilindrada 1. O firmware precisa de um parâmetro **Trigger Angle Offset** (ou *trigger angle advance*) para converter eventos de dente em ângulo real do motor.
- Calibração: trava-se o avanço em um valor fixo (ex.: 10° BTDC), lê-se com luz estroboscópica e ajusta-se o offset até o valor comandado coincidir com a marca física.

### 2.4 Modos de ignição

| Modo | Descrição | Sincronismo necessário |
|---|---|---|
| **Distribuidor** | Uma bobina e um rotor distribuem a faísca. | Apenas CKP |
| **Wasted spark** | Cada bobina alimenta dois cilindros em paralelo; um está no ciclo de escape, o outro na compressão. | Apenas CKP |
| **COP (Coil-on-Plug)** | Uma bobina por vela. | CKP + CMP (fase de 720°) para sequencial |
| **Wasted-COP** | Bobinas individuais, mas dispara dois cilindros por ciclo. | Apenas CKP |

Para o primeiro protótipo em um motor de 4 cilindros, **wasted spark** é a escolha mais simples: funciona só com roda fônica no virabrequim, sem sensor de fase do comando.

## 3. Projetos open source analisados

### 3.1 Speeduino

- **Linguagem/plataforma**: Arduino/C++ em microcontroladores de 8 bits (ATmega) e STM32.
- **Arquitetura de ignição**:
  - Cálculo de ângulos de carga e descarga da bobina: `calculateIgnitionAngles()`.
  - Dwell convertido em ângulo de virabrequim com base no RPM atual.
  - Agendamento por timer-driven interrupts: `scheduler.cpp` e `scheduler_ignition_controller.cpp`.
  - Proteção contra over-dwell: `applyOverDwellProtection()` é chamada a cada 1 ms.
- **Decisões relevantes**:
  - Saídas são sinais lógicos de baixa potência; bobinas "burras" (dumb coils) **nunca** são acionadas diretamente.
  - Dwell diferenciado para partida (`dwellCrank`) e funcionamento (`dwellRun`).
  - Suporte a wasted spark, wasted-COP e sequencial.

### 3.2 rusEFI

- **Linguagem/plataforma**: C++ em STM32.
- **Arquitetura de ignição**:
  - Decodificador de trigger flexível: suporta centenas de padrões de roda fônica e comando.
  - Ângulos baseados em virabrequim; dwell baseado em tempo (ms) e corrigido por tensão da bateria.
  - `IgnitionState` calcula `dwellDurationAngle = dwell_ms / oneDegreeTimeMs(RPM)`.
  - Eventos de faísca e carga da bobina são agendados como ângulos relativos aos eventos de trigger.
- **Decisões relevantes**:
  - Hibridização entre ângulo e tempo: o firmware sabe que a faísca deve ocorrer em 700° BTDC, mas calcula o início de carga (dwell) em milissegundos antes, convertendo para graus conforme o RPM.
  - Correção de dwell por tensão da bateria é essencial para estabilidade da faísca.

### 3.3 VIAEMS

- **Linguagem/plataforma**: C em STM32 (inicialmente pensado para cortex-m3/m4).
- **Arquitetura**:
  - Decodificadores: *even-tooth* e *missing-tooth* em virabrequim ou comando.
  - 36-1 = 36 posições incluindo o dente ausente.
  - Sem sensor de fase, o sistema trabalha em 360° (wasted spark/batch injection).
  - Até 16 outputs configuráveis; cada evento pode ser ignição ou injeção.
  - Resolução de agendamento de 250 ns.
  - Dwell pode ser tabela vs tensão da bateria, fixo ou baseado em RPM.
- **Decisões relevantes**:
  - Lista de eventos reagendada a cada 200 µs, permitindo qualquer ângulo de 0 a 719°.
  - Ignição é um evento cuja **descarga** ocorre no ângulo alvo; a carga inicia antes, automaticamente, considerando o dwell.

### 3.4 ESP-ECU (ESP32-S3)

- **Linguagem/plataforma**: C++ no ESP32-S3.
- **Arquitetura**:
  - Roda fônica 36-1 com detecção de fase do comando para modo sequencial.
  - Até 8 coils (COP) diretamente em GPIOs, expansível via MCP23017.
  - `CrankSensor.cpp`: decodificação do trigger e cálculo de RPM.
  - `IgnitionManager.cpp`: controle de dwell e timing de faísca.
  - Comunicação: REST API, WebSocket, MQTT.
- **Decisões relevantes**:
  - Prova de que ESP32 pode controlar ignição sequencial, mas usa expansores I²C/SPI para I/O.
  - Inclui CJ125/LSU 4.9 para lambda de banda larga — referência para closed-loop futuro.

### 3.5 Standalone Motorsports Open Source ECM

- **Linguagem/plataforma**: C/C++ em STM32F4/F7.
- **Arquitetura**:
  - Ignição wasted spark e sequencial até 12 cilindros.
  - Mapas de avanço para base, partida, marcha lenta, multi-faísca, *boost retard*, *launch control*.
  - Agendamento preciso por hardware timers sincronizados ao virabrequim.
  - Entradas: VR/Hall CKP/CMP, sensores analógicos, O2, flex-fuel.
  - Saídas: drivers de injetor, bobinas, ETB, ventoinhas, bombas.
  - Tunagem via TunerStudio / FOME Console.
- **Decisões relevantes**:
  - Separação clara entre camada de sensores, controladores de combustível/ignição e drivers.
  - Watchdog e modos de falha são parte integrante do design de segurança.

### 3.6 MegaSquirt (referência, firmware parcialmente aberto)

- **Decodificador missing-tooth**: permite 36-1, 60-2 e outras variantes.
- Conceito de *Delay Teeth* e *Trigger Offset* para alinhar dente físico com PMS.
- Modos de ignição: single coil, wasted spark, COP.
- Apesar de não ser totalmente open source, sua documentação pública (`megamanual.com`) é um padrão de referência para calibração de trigger e ignição.

## 4. Decisões de arquitetura consolidadas entre os projetos

1. **Nunca acionar bobina diretamente do MCU**: o microcontrolador fornece sinal lógico; a potência é chaveada por IGBT/MOSFET + driver dedicado.
2. **Dwell em ms, não em graus**: o tempo de carga é físico (corrente da bobina); só a posição da faísca é angular.
3. **Correção de dwell por tensão da bateria**: bateria fraca precisa de mais tempo de carga.
4. **Calibração de trigger é crítica**: sem *trigger angle offset* correto, todo o avanço de ignição estará deslocado.
5. **Começar por wasted spark**: elimina a necessidade de sensor de fase do comando no primeiro estágio.
6. **Proteção contra over-dwell**: implementar no firmware e, de preferência, também no driver de hardware.
7. **Precisão de timer importa**: ISRs de alta prioridade e timers de hardware; evitar delays bloqueantes.

## 5. Implicações para o Programmable ECU

O firmware atual já possui:
- Decodificador de roda fônica **36-1** simulada.
- Cálculo de RPM a partir do gap da roda fônica.
- Injeção sequencial baseada em CKP + revolução simulada (camRev).

Para adicionar ignição, as próximas tasks devem cobrir:

### 5.1 Pré-requisitos já existentes (podem ser reaproveitados)

- Leitura de CKP real (hoje simulado por timer interno).
- Estrutura de mapa 2D com interpolação bilinear (hoje usada para combustível).
- Framework de proteções (RPM, CLT).

### 5.2 Novos componentes necessários

1. **Mapa de avanço de ignição** (RPM × MAP/TPS), com interpolação bilinear.
2. **Mapa/tabela de dwell** (RPM × tensão da bateria).
3. **Driver de bobina**: escolha entre smart coil, driver IGBT discreto ou CI como FAN1110B-F085.
4. **Agendamento de faísca**: calcular ângulo de início de carga e ângulo de faísca; converter para tempo com base no RPM.
5. **Calibração do trigger offset**: procedimento com luz estroboscópica para alinhar CKP real com PMS.
6. **Modo wasted spark**: primeiro passo funcional sem CMP.
7. **(Futuro) Modo sequencial COP**: requer sensor de fase do comando.
8. **Proteções de ignição**: over-dwell, detecção de falha de CKP, corte de faísca em over-rev.

## 6. Próximas tasks sugeridas

| ID sugerido | Task | Entregável |
|---|---|---|
| 1.5.2.1 | Definir hardware de driver de ignição (smart coil vs IGBT vs FAN1110B) | Documento de decisão e esquema elétrico inicial |
| 1.5.2.2 | Implementar mapa de avanço de ignição 2D (RPM × MAP) no firmware | Código + testes de interpolação |
| 1.5.2.3 | Implementar tabela de dwell (RPM × bateria) | Código + testes de cálculo de energia |
| 1.5.2.4 | Calcular ângulos de carga/descarga da bobina a partir do CKP | Algoritmo de agendamento no `setup()`/`loop` |
| 1.5.2.5 | Acionar 1 saída de ignição em modo teste (LED/osciloscópio) | Prova em bancada |
| 1.5.2.6 | Implementar wasted spark para 4 cilindros (ordem 1-3-4-2) | 2 bobinas acionadas, prova em bancada |
| 1.5.2.7 | Calibração de trigger offset com luz estroboscópica | Procedimento documentado no Docusaurus |
| 1.5.2.8 | Proteções de ignição (over-dwell, falha CKP, corte em over-rev) | Código + testes |

## 7. Riscos e observações

- **Segurança**: testar ignição em motor real requer extrema cautela. Início em bancada com LED/osciloscópio e só depois com bobinas sem vela/sem combustível.
- **Precisão do ESP32**: ajustes de ignição exigem microssegundos de precisão. O firmware atual usa `timerAttachInterrupt` e ISRs; isso é adequado para prova de conceito, mas pode precisar de `MCPWM` ou `GPTimer` do ESP-IDF se o jitter do Arduino core se mostrar alto.
- **Tensão da bateria**: sem leitura de bateria, o dwell fica exposto a variações. Adicionar divisor resistivo + ADC para bateria é recomendado antes de rodar no carro.
- **Trigger real vs simulado**: a roda fônica simulada é perfeita; sensores reais (Hall/VR) têm ruído, variação de duty cycle e necessitam de condicionamento de sinal.

## 8. Referências

1. Speeduino Doxygen — `scheduler_ignition_controller.cpp/.h`, `scheduler.cpp`, `scheduledIO_ign.cpp`: https://speeduino.github.io/speeduino-doxygen/
2. rusEFI Wiki — Trigger Decoding: https://rusefi.com/docs/html/md_controllers_2trigger_2readme.html
3. rusEFI Wiki — All Supported Triggers: https://github.com/rusefi/rusefi/wiki/All-Supported-Triggers
4. rusEFI — `ignition_state.cpp`: https://rusefi.com/docs/html/ignition__state__8cpp_source.html
5. VIAEMS — Engine management system: https://github.com/via/viaems/
6. endthedrugwar/esp-ecu — ESP32-S3 ECU: https://github.com/endthedrugwar/esp-ecu
7. HimalayPandit/Standalone-Motorsports-Open-Source-ECM: https://github.com/HimalayPandit/Standalone-Motorsports-Open-Source-ECM
8. MegaSquirt-II Missing Tooth Trigger Wheel Decoder: http://www.megamanual.com/ms2/wheel.htm
9. DIYAutoTune — Wasted Spark Ignition: https://diyautotune.com/blogs/tech-corner/wasted-spark-ignition
10. AutoSpeed — Ignition coil dwell time: http://www.autospeed.com/cms/A_113140/printArticle.html
11. HP Academy — Webinar "What is Dwell Time": https://www.hpacademy.com/previous-webinars/208-what-is-dwell-time/
12. ON Semiconductor — FAN1110B-F085 Ignition Gate Driver Datasheet: https://www.onsemi.com/products/power-management/gate-drivers/fan1110b-f085
13. ESP-IDF — General Purpose Timer (GPTimer): https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gptimer.html
14. ESP-IDF — Motor Control PWM (MCPWM): https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/mcpwm.html
