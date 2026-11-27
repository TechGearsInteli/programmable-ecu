# Fundamentos do projeto

> O que é a Programmable ECU, como ela funciona, o que já está decidido e o vocabulário para conversar sobre ela.
> Síntese de `docs/docs/` em 26 nov. 2026. Quando um detalhe técnico importar, abra a página citada.

## 1. Em uma frase

Tirar a central Honda do **Fit da liga**, encaixar a nossa ECU **no mesmo plugue** e só fazer isso depois que o programa funcionar na mesa (`contexto-decisoes.md`).

A ECU é da **TechGears**, clube universitário do Inteli, com firmware próprio. FuelTech e Speeduino servem de referência de produto e de elétrica. Não copiamos o firmware deles.

## 2. Hierarquia das fontes (leia antes de confiar numa página)

1. **`docs/docs/contexto-decisoes.md`**: o recorte em vigor. É a decisão do Eduardo de 29 out. 2026, atualizada em 11 nov. 2026. Ganha de tudo.
2. As páginas das features entregues (`protocolo-uart.md`, `nucleo-calibracao.md`, `hmi-s3.md`, `tutorial-hmi.md`) descrevem o que já existe no código.
3. `arquitetura.md`, `teoria-do-motor.md` e `estudo-hardware-ecu.md` são estudo de base, parte dele **anterior ao recorte**:
   - **`visao-geral.md`** ainda fala de interface web via Wi-Fi, e **`arquitetura.md`** ainda tem a "Fase 1: leitura via porta OBD-II". **As duas coisas saíram do escopo.** As tasks 1.10.1.1 e 1.10.1.7 vão reescrever essas páginas.
   - **`estudo-hardware-ecu.md` (feature 1.3)** recomendava STM32 no motor. **O recorte escolheu o ESP32.** O STM32 só volta se o ESP reprovar no teste de capacidade (1.5.1.9: não perder dente da roda fônica).
   - O estudo de sonda lambda continua útil, mas **malha fechada de lambda está fora** do recorte.

## 3. A arquitetura do recorte

```
 sensores do Fit ──> [condicionamento 12 V → 3,3 V] ──> ESP32 "do motor" ──> MOSFETs ──> bicos
 (CKP, MAP, TPS,                                      │  mapas na RAM/NVS   ──> driver ──> bobinas
  CLT, IAT, bateria)                                  │                     ──> relé ──> bomba, ventoinha
                                                      │ UART 3,3 V (TX, RX, GND)
                                                      ▼
                                            ESP32-S3 + tela 4,3" touch
                                            (painel, alertas, edição dos mapas)
```

- **ESP do motor (ESP32 sem tela):**
  - lê os sensores e calcula o tempo de injeção (PW) e o avanço;
  - aciona os bicos, as bobinas e os relés;
  - **guarda os mapas, confere os limites e grava na flash (NVS)**.
- **ESP32-S3 com tela capacitiva de 4,3" (800×480):**
  - **só mostra e pede**. Não calcula o bico e não recebe fio de sensor, bico nem bobina;
  - se a tela travar, o motor continua igual.
- **UART com protocolo próprio:** frame `A5 | LEN | SEQ | TYPE | PAYLOAD | CRC16`, telemetria a 20 Hz, pedido e resposta com confirmação ou recusa. Detalhes em `protocolo-uart.md`.
- **Potência:**
  - os bicos usam MOSFET de gate lógico (3,3 V);
  - a ignição depende de a bobina Honda ter módulo (pulso de 5 V) ou não. Ainda não foi medido;
  - relé só para cargas que ficam ligadas, como bomba e ventoinha;
  - na mesa, uma **lâmpada** prova o pulso antes do bico de verdade;
  - PCB própria **não** é desta etapa.
- **Ligação no carro:**
  - a nossa caixa entra no conector da central Honda, com um chicote nosso;
  - **nunca cortar o chicote do Fit**;
  - para voltar atrás, a Honda volta no mesmo plugue;
  - o Gol G3 AP 1.6 é só uma referência conhecida.

## 4. Combinado x fora x falta dado

**Combinado** (decisões de trabalho, que mudam só com a equipe ciente):
- duas placas;
- firmware nosso;
- mesa antes do carro;
- sem etapa de só escutar o carro;
- mapa e tela no modelo FuelTech;
- bomba por relé nosso;
- sem emular a rede Honda nem o imobilizador;
- simulador LVGL antes de gravar na S3;
- a demo visual (PR #178) não é produto.

**Fora do recorte, por enquanto:**
- controle por OBD, K-Line ou CAN de diagnóstico;
- cortar chicote;
- uma placa só, com motor, tela e Wi-Fi;
- site ou API no chip do motor;
- lambda em malha fechada;
- PCB fabricada como primeira entrega;
- turbo, nitro, controle de tração, datalog de centenas de canais e tunagem remota.

**Falta dado (não é indecisão):**
- ano, motor e câmbio do Fit;
- foto e pinagem do conector da central;
- se o CKP é indutivo ou Hall, e o desenho da roda fônica;
- se o imobilizador impede a partida;
- o que o laboratório do Inteli tem;
- o modelo exato da S3 e do MOSFET;
- a autorização da liga.

## 5. Ordem do trabalho

1. **Mesa:** 1.4 hardware e 1.5 firmware do motor.
2. **Interface:** 1.7 painel, depois 1.6 calibração.
3. **Segurança:** 1.8.
4. **Fit:** 1.9, na ordem mesa, pátio e rua.
5. **Documentação:** 1.10.

"Firmware bom" quer dizer que, na bancada:
- o ESP acompanha um gerador de roda fônica sem perder dente;
- o PW e o avanço fazem sentido;
- a lâmpada pisca no tempo certo.

## 6. Teoria mínima de motor (o que a ECU calcula)

- **Quatro tempos:** admissão, compressão, explosão e escape, em 720° de virabrequim por ciclo. Num 4 cilindros, há uma ignição a cada 180°.
- **Mistura:** a estequiométrica da gasolina é **14,7:1** (λ = 1). Mistura rica (λ < 1) dá potência e proteção térmica. Mistura pobre (λ > 1) dá economia, com risco de detonação.
- **Speed-density:**
  - a carga do motor é medida pelo **MAP** (pressão no coletor, em kPa) e a rotação pelo **CKP** (roda fônica);
  - o **mapa de combustível** é uma tabela de rotação × MAP com o tempo de injeção base em **ms**;
  - o **mapa de ignição** é a mesma tabela, em **graus** de avanço.
- **Interpolação bilinear:**
  - o valor entre células sai das quatro vizinhas;
  - exemplo da `teoria-do-motor.md`: 2500 rpm e 60 kPa dão **4,55 ms**. Esse exemplo virou teste no núcleo.
- **Correções multiplicativas:** aquecimento (CLT), temperatura do ar (IAT), partida e aceleração (TPSdot), mais o **tempo morto** do bico, que depende da tensão da bateria e é somado.
- **Como o núcleo calcula** (`nucleo-calibracao.md`):
  `PW = célula × (vazão_ref ÷ vazão_bico) × Π(1 + correção/100) + tempo_morto(Vbat)`. Com as correções em zero, o PW é igual à célula.
- **Ignição:**
  - o **dwell** é o tempo carregando a bobina;
  - o **avanço** vem do mapa, mais as correções;
  - a ordem de ignição depende do número de cilindros: com 4 cilindros, 1-3-4-2.

## 7. O que já existe em código (26 nov. 2026)

| Área | Estado |
|------|--------|
| Núcleo C99 (`src/firmware/common`) | Protocolo, mapas 16×16, PW, correções, ficha, gravação TGCL com CRC-32, backup. 37 testes Unity |
| Telas LVGL (`src/firmware/hmi/ui`) | Painel, alertas, grades de combustível e ignição, correções, ficha e gravação |
| Simulador (`src/software/hmi-sim`) | Janela no Windows, mais o modo headless com 54 verificações e capturas |
| Firmware S3 (`src/firmware/hmi/s3`) | Compila, mas não foi validado em placa. Os pinos em `board.h` estão a confirmar |
| Firmware de bancada (`src/firmware/motor-bench`) | Potenciômetros mais o núcleo. **Não aciona saídas** (isso é da 1.5) |
| Firmware do motor de verdade (1.5) | **Ainda não existe na `main`.** Deve chamar `ecu_engine_compute()` no laço e `ecu_motor_link_feed()` na task da UART |

Tudo isso está no PR #193 (`feat/1.6-1.7-hmi-s3`), aguardando revisão.

## 8. Glossário

| Termo | Significado |
|-------|-------------|
| **ECU** | Electronic Control Unit: a central que comanda injeção e ignição |
| **HMI** | Human-Machine Interface: aqui, a S3 com tela |
| **CKP** | Sensor de posição do virabrequim. Lê a roda fônica (ex.: 60-2 ou 36-1, dentes menos falhas) |
| **CMP** | Sensor de fase (comando). Distingue o ciclo de 720° |
| **MAP** | Pressão absoluta no coletor (kPa): a carga do motor |
| **TPS** | Posição da borboleta (%). A derivada (TPSdot) dispara a correção de aceleração |
| **CLT / IAT** | Temperatura da água / do ar admitido (sensores NTC) |
| **PW** | Pulse Width: o tempo que o bico fica aberto, em ms |
| **Tempo morto** | Atraso de abertura do bico, que depende da tensão da bateria. É somado ao PW |
| **Dwell** | Tempo de carga da bobina antes da faísca |
| **Avanço** | Graus antes do PMS em que a faísca acontece |
| **λ / AFR** | Razão ar/combustível relativa (λ) ou absoluta (AFR). Gasolina: 14,7:1 |
| **Célula** | Um ponto do mapa (linha = MAP, coluna = rpm). A "célula atual" é a mais próxima do ponto de operação |
| **Ficha do motor** | Cilindros, dentes e falhas da roda, vazão do bico e de referência, dwell, rpm de partida e limites dos alertas |
| **NVS** | Non-Volatile Storage do ESP32: a flash onde a calibração é gravada |
| **TGCL** | Formato do blob de calibração: magic, versão, tamanho e CRC-32 (1342 bytes) |
| **Limp mode** | Mapa de emergência quando um sensor falha (feature 1.8) |
| **FTManager** | Software de PC da FuelTech. Referência de produto, fora do escopo agora |
