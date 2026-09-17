# Pesquisa: Aprofundamento do Firmware da ECU Programável

## 1. Objetivo

Levantar o que falta para transformar o firmware atual em um núcleo robusto de ECU programável, incluindo ignição, conexão com o veículo, melhorias de firmware e organização de novas tasks. Este documento serve de base para o time criar issues estruturadas no mesmo padrão das existentes.

## 2. Controle de ignição

### 2.1 Conceitos fundamentais

**Dwell time:** tempo em que a bobina primária fica energizada. Energia armazenada \(E = \frac{1}{2} L I^2\). Dwell curto causa misfire; dwell longo aquece e danifica a bobina. ECUs comerciais usam mapa de dwell vs RPM e vs tensão da bateria.

**Spark advance:** ponto de faísca em graus antes do PMS. Controlado por mapa 3D (RPM × carga) e correções por temperatura, detonação, partida a frio etc.

**Trigger wheel:** roda fônica 36-1 (10° entre dentes) ou 60-2 (6° entre dentes). O espaço vazio dá o ponto de sincronismo. É necessário um *Trigger Angle Offset* para alinhar o primeiro dente após a falha com o PMS da cilindrada 1.

**Modos de ignição:**

| Modo | O que é | Sincronismo |
|---|---|---|
| Distribuidor | Uma bobina + rotor | Apenas CKP |
| Wasted spark | Uma bobina aciona 2 cilindros (um no escape) | Apenas CKP |
| Wasted-COP | Bobina individual, mas dispara 2 por ciclo | Apenas CKP |
| Sequential COP | Bobina individual por cilindro, só na compressão | CKP + sensor de fase (CMP) |

Para o primeiro protótipo em motor 4 cilindros, **wasted spark** é o mais simples e seguro.

### 2.2 Projetos open source analisados

- **Speeduino:** timers por ISR, proteção contra over-dwell a cada 1 ms, saídas lógicas nunca acionam bobinas diretamente, dwell separado para partida e funcionamento.
- **rusEFI:** cálculo híbrido de ângulo + tempo; dwell em ms é convertido para ângulo conforme RPM; correção de dwell por tensão da bateria.
- **VIAEMS:** scheduling de 250 ns, eventos reagendados a cada 200 µs, suporta 36-1 com e sem cam sync.
- **ESP-ECU (ESP32-S3):** prova de ESP32 controlando ignição COP e injeção sequencial, usando expansores I²C/SPI para I/O.
- **Standalone Motorsports Open Source ECM (STM32F4/F7):** mapas de avanço, multi-faísca, launch control, boost retard, watchdog e modos de falha integrados.
- **MegaSquirt:** documentação pública de *trigger offset*, *delay teeth*, wasted spark e COP.

### 2.3 Decisões consolidadas

1. Nunca acionar bobina diretamente do MCU — usar driver IGBT/MOSFET ou smart coil.
2. Dwell em ms, faísca em graus.
3. Corrigir dwell por tensão da bateria.
4. Calibrar trigger offset com luz estroboscópica.
5. Começar por wasted spark.
6. Proteger contra over-dwell no firmware e no hardware.
7. Usar timers de hardware e ISRs de alta prioridade.

### 2.4 Tasks sugeridas de ignição

| ID sugerido | Task | Entregável |
|---|---|---|
| 1.5.2.1 | Definir hardware de driver de ignição | Documento + esquema inicial |
| 1.5.2.2 | Implementar mapa de avanço 2D (RPM × MAP) | Código + testes de interpolação |
| 1.5.2.3 | Implementar tabela de dwell (RPM × bateria) | Código + testes de energia |
| 1.5.2.4 | Calcular ângulos de carga/descarga da bobina a partir do CKP | Algoritmo de agendamento |
| 1.5.2.5 | Acionar 1 saída de ignição em modo teste | Prova em bancada (LED/osciloscópio) |
| 1.5.2.6 | Implementar wasted spark 4 cilindros | 2 bobinas, prova em bancada |
| 1.5.2.7 | Calibração de trigger offset com luz estroboscópica | Procedimento documentado |
| 1.5.2.8 | Proteções de ignição (over-dwell, falha CKP, corte em over-rev) | Código + testes |

## 3. Conexão da ECU com o veículo

### 3.1 OBD-II como ponte na Fase 1

A porta OBD-II expõe os mesmos dados que sensores diretos forneceriam:

| PID OBD-II | Sensor direto equivalente | Grandeza |
|---|---|---|
| 0x0C Engine RPM | CKP | RPM |
| 0x05 Coolant Temp | CLT | °C |
| 0x11 Throttle Position | TPS | % |
| 0x0B Manifold Pressure | MAP | kPa |
| 0x0F Intake Air Temp | IAT | °C |
| 0x14 O2 Sensor Voltage | Lambda | tensão proporcional |

Vantagens:
- Nenhum circuito adicional.
- Validação de software com motor real.
- Interface e comunicação podem amadurecer antes do hardware próprio.

Limitação: a ECU lê, mas não controla nada. O carro continua sendo gerenciado pela ECU original.

### 3.2 CAN direto como evolução da Fase 1

A porta OBD-II usa CAN como camada física. Em vez de enviar comandos ELM327 e esperar respostas serial, a ECU pode escutar **diretamente o barramento CAN** do veículo. Isso exige:

- Transceiver CAN (MCP2551, TJA1051, SN65HVD230).
- Controlador CAN no microcontrolador (ESP32 tem controlador CAN nativo, mas requer transceiver externo).
- Mapeamento dos IDs e offsets das mensagens do veículo (DBC).

Vantagens sobre OBD-II serial:
- **Latência muito menor:** mensagens CAN chegam a 10–100 Hz sem polling.
- **Menos overhead:** sem comandos AT e timeouts do adaptador.
- **Dados críticos em tempo real:** RPM, TPS, MAP podem ser usados diretamente no loop de controle.
- **Possibilidade futura de enviar comandos:** em alguns veículos é possível acionar atuadores pela CAN, mas isso é avançado e inseguro sem estudo profundo.

Desvantagens:
- Cada fabricante usa IDs diferentes. Precisa de arquivo DBC ou engenharia reversa.
- Requer solda/módulo de transceiver.
- Maior risco de interferir no barramento do veículo se mal feito.

### 3.3 Tasks sugeridas de conexão veicular

| ID sugerido | Task | Entregável |
|---|---|---|
| 1.6.0.1 | Implementar driver OBD-II serial (ELM327) | Código lendo RPM, TPS, MAP, CLT, IAT, lambda |
| 1.6.0.2 | Validar leitura OBD-II contra aplicativo comercial | Relatório de comparação |
| 1.6.0.3 | Estudar mapeamento CAN do veículo alvo | Lista de IDs e offsets documentada |
| 1.6.0.4 | Prototipar leitura CAN direta com transceiver | Código + esquema + teste em bancada |
| 1.6.0.5 | Comparar latência OBD-II serial vs CAN direto | Medição documentada |

## 4. Melhorias gerais do firmware para ser robusto

### 4.1 Leitura real de sensores (Fase 2)

Hoje o firmware simula MAP, TPS, CLT, IAT e lambda. Para Fase 2, cada sensor precisa de:

- **Condicionamento de sinal:** divisor de tensão, filtro RC, proteção contra transientes.
- **Conversão ADC:** leitura analógica do ESP32 (12 bits, referência 3,3 V).
- **Linearização:** curvas NTC para temperatura, curva do MAP, offset do TPS.
- **Validação:** faixa mínima/máxima, detecção de curto/aberto, falha persistente.

| Sensor | Entrada típica | Detalhes |
|---|---|---|
| MAP | ADC 0–3,3 V | Sensor piezoresistivo, calibrar 0 kPa e 100 kPa |
| TPS | ADC 0–3,3 V | Potenciômetro, calibrar fechado e totalmente aberto |
| CLT | ADC + NTC | Curva Steinhart-Hart ou tabela |
| IAT | ADC + NTC | Mesma abordagem do CLT |
| Lambda | ADC 0–1 V (NB) ou interface LSU 4.9 (WB) | NB simples, WB requer CJ125/bosch LSU driver |
| Bateria | ADC com divisor | Essencial para correção de injeção e dwell |

### 4.2 Sensor de fase do comando (CAM/CMP)

A injeção sequencial atual funciona com `camRev` simulado. Para sequencial real é necessário um sensor de fase (um dente no comando ou sinal do distribuidor). Sem ele, o firmware deve cair para **batch injection** ou **wasted spark**.

### 4.3 Controle auxiliar básico

- **Bomba de combustível:** ligar junto com o motor e manter por alguns segundos após desligar.
- **Ventoinha:** acionada por CLT > limiar.
- **Relé de ignição / main relay:** sequência de partida segura.

### 4.4 Segurança e diagnóstico

- **Watchdog:** reiniciar o MCU se o loop principal travar.
- **Detecção de falha de sensor:** substituir por valor default e sinalizar na interface.
- **Limp mode:** mapa conservador quando sensores críticos falham.
- **Datalog estruturado:** buffer circular com timestamp, RPM, sensores, PW, proteções ativas.

### 4.5 Qualidade de código

- Separar `ecu.ino` em módulos: `engine.cpp`, `sensors.cpp`, `display.cpp`, `storage.cpp`.
- Todos os cálculos do núcleo devem ser testáveis off-line (funções puras sem dependência de hardware).
- Definir uma HAL para trocar entre simulação, OBD-II, ADC real e CAN sem alterar o núcleo.

## 5. Sugestão de issues estruturadas no padrão atual

Abaixo issues no formato `[X.Y.Z.W] Verbo ...`, seguindo o padrão já usado no projeto.

### Ignição

- `[1.5.2.1] Definir hardware de driver de ignição (smart coil / IGBT / FAN1110B)`
- `[1.5.2.2] Implementar mapa de avanço de ignição 2D (RPM × MAP)`
- `[1.5.2.3] Implementar tabela de dwell (RPM × tensão da bateria)`
- `[1.5.2.4] Calcular ângulos de carga e descarga da bobina a partir do CKP`
- `[1.5.2.5] Acionar 1 canal de ignição em modo teste (LED/osciloscópio)`
- `[1.5.2.6] Implementar ignição wasted spark para 4 cilindros (ordem 1-3-4-2)`
- `[1.5.2.7] Documentar procedimento de calibração de trigger offset com luz estroboscópica`
- `[1.5.2.8] Implementar proteções de ignição (over-dwell, falha CKP, corte em over-rev)`

### Conexão veicular e sensores reais

- `[1.6.0.1] Implementar driver de leitura OBD-II serial (ELM327)`
- `[1.6.0.2] Validar leituras OBD-II contra aplicativo comercial`
- `[1.6.0.3] Mapear IDs CAN do veículo alvo para leitura direta`
- `[1.6.0.4] Prototipar leitura CAN direta com transceiver no ESP32`
- `[1.5.3.1] Implementar leitura ADC real de MAP, TPS, CLT, IAT e bateria`
- `[1.5.3.2] Implementar leitura de sonda lambda narrowband`
- `[1.5.3.3] Implementar detecção de sensor de fase (CAM) para injeção/ignição sequencial`

### Robustez e arquitetura

- `[1.5.4.1] Refatorar firmware em módulos (engine, sensors, display, storage)`
- `[1.5.4.2] Criar HAL para alternar entre simulação, OBD-II, ADC e CAN`
- `[1.5.4.3] Implementar watchdog de hardware no ESP32`
- `[1.5.4.4] Implementar detecção de falha de sensores e modo degradado`
- `[1.5.4.5] Implementar controle de bomba de combustível e ventoinha`
- `[1.5.4.6] Implementar datalog estruturado em buffer circular`

## 6. O desafio de processar tudo do carro e mostrar na tela

Rodar um núcleo de controle de motor em tempo real **e** manter uma interface gráfica responsiva no mesmo microcontrolador é um trade-off clássico. Pontos para conversar com o colega:

### 6.1 Processamento real-time

- O loop de CKP/injeção/ignição precisa de microssegundos de precisão.
- Qualquer operação longa — escrita em NVS, envio de Wi-Fi, desenho de tela complexo — pode atrasar uma interrupção e causar falha de faísca/injeção.
- A tela deve ser atualizada em uma frequência confortável para o olho (5–10 Hz é suficiente), não a cada milissegundo.

### 6.2 Isolamento de tarefas

No ESP32 existem **dois núcleos** (Core 0 e Core 1). A prática comum é:

- **Core 1:** executa o núcleo de controle do motor (CKP, injeção, ignição, cálculos) sem interrupções longas.
- **Core 0:** executa Wi-Fi, interface web, atualização da tela, NVS, datalog.

Isso exige sincronismo seguro entre os núcleos, normalmente com variáveis voláteis e critical sections curtas.

### 6.3 Memória e recursos

- O ESP32 tem RAM limitada. Buffers de tela, buffers de rede e buffer de datalog competem pela mesma memória.
- LovyanGFX permite usar sprites parciais para reduzir o trabalho de redraw.
- Comunicação em excesso (WebSocket a 60 Hz) pode saturar o processador.

### 6.4 Recomendação prática

Para o protótipo físico:

1. Mantenha a tela como **apenas display de leitura** no protótipo inicial.
2. Faça o cálculo do motor no Core 1 e a atualização da tela no Core 0.
3. Atualize a tela a cada 100–200 ms com partial redraw.
4. Não grave NVS durante injeção/ignição; só na inicialização ou comando explícito.
5. Wi-Fi/interface começa só depois do núcleo de controle estável.

## 7. Riscos e observações

- **Segurança:** testar ignição primeiro sem vela, depois com vela fora do motor, depois em motor real com extrema cautela.
- **Trigger real:** sensores Hall/VR têm ruído, duty cycle variável e necessitam de condicionamento.
- **Bateria:** sem leitura de bateria, PW e dwell ficam expostos a variações.
- **CAN:** leitura direta exige transceiver e mapeamento do veículo; comece por OBD-II serial.
- **Wi-Fi:** manter Wi-Fi ativo pode interferir em timers no Core 0; usar Core 1 para controle ajuda.

## 9. Arquitetura e organização da base do firmware

Um firmware de ECU robusto não é um único arquivo grande. Os projetos open source organizam o código em três camadas comuns:

1. **Camada de configuração/interface:** comunicação com TunerStudio, interface web, armazenamento de calibração.
2. **Camada core do motor:** sensores, decoders, correções, cálculo de injeção/ignição, scheduler.
3. **Camada de abstração de hardware (HAL):** I/O digital/analógico, timers, EEPROM/flash, CAN, Wi-Fi.

### 9.1 Speeduino

- Arquivos separados: `sensors.cpp`, `decoders.cpp`, `corrections.cpp`, `scheduler.cpp`, `timers.cpp`, `comms.cpp`.
- `initialiseAll()` no `init.cpp` faz startup: hardware, config, timers, sensores, pin mapping, trigger.
- O `loop()` principal processa operações baseadas em tempo e eventos; o scheduling crítico roda em ISRs de timer.
- HAL permite portar o mesmo core para ATmega, STM32 e outros.

### 9.2 rusEFI

- O "main loop" publica status; **o controle do motor não está nele**.
- Cada evento do trigger chama `main_trigger_callback`, que processa injeção e ignição.
- Usa cálculo híbrido: faísca em graus, dwell em ms convertido para ângulo conforme RPM.

### 9.3 ESP-ECU (ESP32-S3)

- **Core 1:** controle em tempo real — crank/cam ISR, RPM, spark dwell/fire, injector timing.
  - Sem Wi-Fi, sem logging, sem heap allocation.
- **Core 0:** aplicação — ADC, mapas, tuning, closed-loop O2, web server, MQTT, logging, OTA.
- Arquivos separados: `CrankSensor.cpp`, `CamSensor.cpp`, `IgnitionManager.cpp`, `InjectionManager.cpp`, `SensorManager.cpp`, `TuneTable.cpp`.

### 9.4 VCU universitário (FreeRTOS STM32)

- Múltiplas tasks FreeRTOS com prioridades distintas.
- ISRs só colocam dados em queues/notificações e saem imediatamente.
- Nenhuma lógica de negócio roda dentro de ISR.
- Alocação 100% estática: sem `malloc` em runtime.

## 10. Separação de tarefas no ESP32 (dual-core)

A arquitetura dual-core do ESP32 é ideal para separar o que é crítico do que é conforto:

| Core | Tarefas | Prioridade |
|---|---|---|
| **Core 1** | ISR CKP/CAM, cálculo de RPM, scheduling de injeção/ignição, proteções imediatas | Tempo real |
| **Core 0** | Leitura de sensores analógicos, lookup de mapas, Wi-Fi, web server, display, datalog, OTA, OBD-II/CAN | Aplicação |

### 10.1 Regras para o Core 1

- Não chamar `Serial.print`, `delay`, `malloc`, `Preferences`, Wi-Fi, BLE.
- ISRs devem ser curtas: capturar timestamp e notificar uma task de alta prioridade.
- Se uma task de tempo real precisar de dados de outra task, use queues ou variáveis voláteis com mutex/spinlock.

### 10.2 Comunicação entre cores

- `volatile` sozinho não é suficiente no ESP-IDF SMP.
- Usar `portENTER_CRITICAL` / `portEXIT_CRITICAL` (spinlock) ao acessar estado compartilhado.
- Preferir task notifications ou queues para passar eventos do ISR para a task.

### 10.3 ESP-IDF vs Arduino core

- O Arduino core do ESP32 já usa FreeRTOS por baixo.
- Para pinning de tasks, usar `xTaskCreatePinnedToCore(..., 1)` ou `xTaskCreatePinnedToCore(..., 0)`.
- O loop padrão do Arduino roda no Core 1 por padrão em algumas versões; confirmar na configuração.

## 11. Scheduler e eventos

### 11.1 Speeduino scheduler

- Estruturas `FuelSchedule` e `IgnitionSchedule`.
- Máquina de estados por canal: pending → running → off.
- Cada canal tem sua própria ISR; callback executa ação de hardware.
- Permite enfileirar eventos (`allowQueuedSchedule`).

### 11.2 VIAEMS scheduler

- Lista de até 16 eventos reagendada a cada 200 µs.
- Cada evento é output de ignição ou injeção.
- Resolução de 250 ns.
- Ignição: início do dwell calculado automaticamente antes do ângulo alvo.

### 11.3 rusEFI scheduler

- Eventos agendados até o próximo dente do trigger (`scheduleEventsUntilNextTriggerTooth`).
- Combina eventos de trigger com offsets de tempo.

### 11.4 Lição para o nosso firmware

- Nunca usar `delay()` ou `micros()` em espera ocupada para acionar injetor/bobina.
- Sempre usar timer de hardware com alarme/ISR.
- Separar "calcular quando deve acontecer" de "executar no momento certo".

## 12. Testes e integração contínua

### 12.1 Speeduino unit tests

- Framework Unity.
- Pastas separadas: `test_fuel`, `test_math`, `test_init`, `test_tables`, `test_schedules`.
- Testes rodam no computador (host), não no microcontrolador.
- Isso permite testar milhares de casos de interpolação, PW e scheduling rapidamente.

### 12.2 Como aplicar no Programmable ECU

1. Extrair funções puras (`lerpInt`, `lookupFuelPwX100`, `applyFuelCorrections`, etc.) para um arquivo separado.
2. Compilar essas funções no PC com um `main.cpp` de teste usando Unity ou simples asserts.
3. Rodar os testes no CI a cada PR.
4. Manter os self-tests no hardware (ESP32) como prova de integração, não como única forma de teste.

### 12.3 Benefícios

- Encontrar erros de cálculo antes de queimar hardware.
- Refatorar com segurança.
- Documentar o comportamento esperado de cada função.

## 13. Padrões de comunicação entre tasks/ISRs

### 13.1 ISR fina

Toda ISR deve ser curta:

```cpp
void IRAM_ATTR onToothRise() {
  const uint32_t now = micros();
  // guarda timestamp e notifica task
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(engineTaskHandle, &higherPriorityTaskWoken);
  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}
```

### 13.2 Task processa a lógica

```cpp
void engineTask(void *) {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // calcula RPM, scheduling, protecoes
  }
}
```

### 13.3 Queues não bloqueantes

- Task de comunicação (Wi-Fi/CAN) consome dados de uma queue.
- Se a queue estiver cheia, a task de controle incrementa um contador de drops e continua.
- Isso impede que uma falha de rede atrase o controle do motor.

### 13.4 Alocação estática

- Não usar `new`, `malloc`, `String` em runtime.
- Buffers, stacks de tasks, mutexes e queues devem ser alocados estaticamente.
- Isso evita fragmentação de heap e falhas em tempo de execução.

## 14. Sugestões concretas de melhoria para o firmware atual

Com base na pesquisa, as melhorias de maior impacto para tornar o firmware "bom de verdade" são:

### 14.1 Curto prazo (antes de ligar no carro)

1. **Criar uma HAL de sensores** para trocar entre simulação, OBD-II serial, ADC real e CAN direto sem mudar o núcleo.
2. **Separar `ecu.ino` em módulos**:
   - `engine.cpp` — cálculos de combustível/ignição/proteções.
   - `sensors.cpp` — leitura e conversão de sensores.
   - `scheduler.cpp` — agendamento de injetores/bobinas.
   - `display.cpp` — desenho da tela.
   - `storage.cpp` — NVS e calibrações.
   - `comm.cpp` — Serial/OBD-II/CAN.
3. **Extrair funções puras para testes host** (`lerpInt`, lookup de mapa, correções, proteções).
4. **Mover controle em tempo real para Core 1** e aplicação (Wi-Fi, display, datalog) para Core 0.
5. **Adicionar watchdog de hardware** e **failsafe de injetor** (não pode ficar aberto se firmware travar).

### 14.2 Médio prazo

1. **Buffer circular de datalog** com exportação CSV.
2. **Detecção de falha de sensores** com fallback para valores default.
3. **Limp mode** com mapa conservador.
4. **Closed-loop lambda** com sonda real (NB ou WB).
5. **Controle de bomba de combustível**, ventoinha e relés auxiliares.

### 14.3 Longo prazo

1. **Ignição real** com wasted spark e driver IGBT.
2. **CAN direto** do veículo para leitura de sensores com baixa latência.
3. **Interface web completa** com calibração de mapas em tempo real.

## 15. Referências

1. Speeduino Doxygen: https://speeduino.github.io/speeduino-doxygen/
2. Speeduino System Architecture: https://deepwiki.com/speeduino/speeduino/1.1-system-architecture
3. Speeduino Scheduler System: https://deepwiki.com/speeduino/speeduino/3-scheduler-system
4. Speeduino Unit Testing: https://deepwiki.com/speeduino/speeduino/9.1-unit-testing
5. rusEFI Trigger Decoding: https://rusefi.com/docs/html/md_controllers_2trigger_2readme.html
6. rusEFI Wiki — Supported Triggers: https://github.com/rusefi/rusefi/wiki/All-Supported-Triggers
7. rusEFI main loop: https://rusefi.com/docs/html/main__loop_8cpp_source.html
8. rusEFI main trigger callback: https://rusefi.com/docs/html/main__trigger__callback_8h.html
9. VIAEMS: https://github.com/via/viaems/
10. endthedrugwar/esp-ecu: https://github.com/endthedrugwar/esp-ecu
11. alpauna/ESP-ECU (dual-core architecture): https://github.com/alpauna/ESP-ECU
12. HimalayPandit/Standalone-Motorsports-Open-Source-ECM: https://github.com/HimalayPandit/Standalone-Motorsports-Open-Source-ECM
13. MegaSquirt Missing Tooth Decoder: http://www.megamanual.com/ms2/wheel.htm
14. DIYAutoTune — Wasted Spark: https://diyautotune.com/blogs/tech-corner/wasted-spark-ignition
15. AutoSpeed — Dwell Time: http://www.autospeed.com/cms/A_113140/printArticle.html
16. HP Academy — Dwell Time: https://www.hpacademy.com/previous-webinars/208-what-is-dwell-time/
17. ON Semiconductor FAN1110B-F085: https://www.onsemi.com/products/power-management/gate-drivers/fan1110b-f085
18. ESP-IDF GPTimer: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gptimer.html
19. ESP-IDF MCPWM: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/mcpwm.html
20. ESP-IDF FreeRTOS (dual-core): https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html
21. Homebrew ECU + touchscreen dash (ESP32-S3): https://www.reddit.com/r/esp32/comments/1nmpiy4/homebrew_ecu_touchscreen_dash_rev_4_esp32s3_espidf/
