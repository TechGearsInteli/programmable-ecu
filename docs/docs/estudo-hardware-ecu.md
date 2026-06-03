# Estudo de Hardware da ECU Programável

Projeto: Programmable ECU - TechGears Inteli  
Historia: 1.3.1 - Conhecer os componentes eletronicos da ECU antes de montar  

> Observacao importante: este material e um estudo preliminar de hardware. Ele ajuda a orientar escolhas, circuitos e testes de bancada, mas ainda nao substitui validacao eletrica, revisao de seguranca, ensaios em osciloscopio, testes termicos e aprovacao do Diretor de Engenharia antes de qualquer uso em motor real.

## Visao geral da historia

A historia 1.3.1 existe para garantir que a equipe entenda os blocos eletronicos principais antes de montar uma ECU programavel. Em uma ECU para motor de combustao interna, existem tres grupos principais de hardware:

- processamento em tempo real: microcontrolador, timers, ADC, interrupcoes, comunicacao e armazenamento de calibracao;
- sensores: CKP, MAP, TPS, CLT, IAT e lambda, que informam posicao, carga, temperatura e mistura;
- atuadores: injetores e bobinas de ignicao, que precisam ser acionados com potencia, protecao e temporizacao precisa.

O ponto central e que a ECU nao e apenas um microcontrolador lendo sensores. Ela precisa tomar decisoes em tempo real e acionar cargas indutivas em um ambiente eletricamente agressivo: bateria automotiva, ruido, vibracao, transientes de tensao e calor. Por isso, cada componente estudado precisa ser entendido em termos de sinal eletrico, circuito de interface, risco e impacto no controle do motor.

---

# 1.3.1.1 - Decidir qual microcontrolador usar na ECU (ESP32 ou STM32)

## O que esta task pede

Esta task pede uma comparacao objetiva entre ESP32 e STM32 para decidir qual microcontrolador deve ser usado na ECU. O criterio mais importante nao e apenas "qual e mais facil de programar", mas qual atende melhor a requisitos de controle em tempo real:

- gerar pulsos de injecao em microssegundos;
- calcular ignicao com base na posicao do virabrequim;
- capturar eventos do sensor CKP com baixa latencia;
- ler sensores analogicos;
- manter comunicacao com a interface de calibracao;
- operar de forma previsivel em ambiente automotivo.

## Por que isso importa na ECU

O microcontrolador e o cerebro da ECU. Se ele nao conseguir medir o tempo entre dentes da roda fonica, calcular RPM, agendar injecao e ignicao e acionar saidas com precisao, o motor pode falhar, perder eficiencia ou operar de forma insegura.

Em uma ECU, temporizacao e mais critica do que processamento bruto. Uma diferenca de poucos milissegundos na ignicao pode alterar desempenho, aquecimento, consumo e risco de detonacao. Na injecao, o tempo de abertura dos injetores costuma ser controlado em milissegundos, mas com resolucao de microssegundos.

## Comparacao objetiva

| Criterio | ESP32 | STM32 |
|---|---|---|
| Facilidade inicial | Muito acessivel, barato, comunidade grande e Wi-Fi integrado | Exige mais curva de aprendizado, especialmente com timers e HAL/LL |
| Wi-Fi/Bluetooth | Integrado, excelente para interface web local | Normalmente exige modulo externo |
| Timers para controle de motor | Possui MCPWM, captura e PWM, mas o ecossistema e mais voltado a IoT | Familia STM32 tem timers avancados, muito usados em controle de motor, PWM, captura e sincronizacao |
| Determinismo em tempo real | Bom se bem configurado, mas Wi-Fi/RTOS podem complicar tarefas criticas | Melhor vocacao para controle embarcado deterministico, especialmente sem pilhas de comunicacao pesadas no nucleo critico |
| ADC | Funcional, mas precisa calibracao e cuidado com faixa/linearidade | Em muitas familias, ADC e perifericos analogicos sao mais robustos e previsiveis |
| CAN automotivo | Depende do modelo; ESP32 classico tem TWAI, mas precisa transceiver | Muitas linhas STM32 oferecem CAN/FDCAN conforme modelo |
| Ambiente de desenvolvimento | Arduino/ESP-IDF, rapido para prototipar | STM32CubeIDE/CubeMX, HAL/LL, mais profissional para embarcado |
| Custo e disponibilidade | Muito bom para prototipos | Variavel, mas ha muitas opcoes industriais e automotivas |
| Adequacao como ECU final | Bom para prototipo com interface Wi-Fi integrada | Mais adequado como controlador principal de tempo real |

## Pontos tecnicos importantes

O ESP32 tem periferico MCPWM dedicado para controle de motores, com timers e operadores independentes. A propria documentacao da Espressif descreve timers MCPWM com configuracao de grupo, fonte de clock e interrupcao, o que permite gerar PWM e capturar eventos com hardware dedicado. Isso torna o ESP32 viavel para prototipos e testes iniciais.

O ponto de atencao e que o ESP32 tambem costuma rodar Wi-Fi, tarefas FreeRTOS e pilhas de comunicacao. Isso nao impede uso em controle, mas exige arquitetura cuidadosa: tarefas criticas precisam ficar em interrupcoes, timers ou perifericos de hardware, enquanto telemetria e interface web ficam isoladas.

No STM32, principalmente familias como STM32G4, STM32F3, STM32F4 e STM32H7, os timers avancados sao mais alinhados com controle de motor. Um exemplo e o STM32G491, que possui timers avancados de controle de motor com canais de captura/comparacao, PWM, saidas complementares e sincronizacao entre timers. A ST tambem possui notas de aplicacao especificas para timers, PWM e recursos avancados de motor control.

## Recomendacao preliminar

Para a ECU final, a recomendacao tecnica preliminar e usar **STM32 como microcontrolador principal de controle em tempo real**.

A justificativa e:

- timers e perifericos mais adequados para captura de CKP, agendamento de ignicao e injecao;
- melhor separacao entre controle critico e interface de usuario;
- maior maturidade em aplicacoes embarcadas de tempo real;
- melhor caminho para evoluir para CAN, protecoes, diagnostico e arquitetura automotiva.

O **ESP32 continua sendo muito util**, mas como:

- prototipo inicial rapido;
- modulo de Wi-Fi para telemetria e calibracao;
- gateway de comunicacao entre interface web e ECU principal;
- plataforma didatica para testar sensores analogicos.

## Conclusão dessa task

Decisao proposta: **STM32 como MCU principal da ECU e ESP32 como candidato a modulo auxiliar de conectividade**.

Isso equilibra os pontos fortes de cada plataforma. O STM32 fica responsavel por temporizacao critica, e o ESP32 pode cuidar da interface web/Wi-Fi sem colocar em risco o controle do motor.

## Entregavel da task

- Tabela comparativa ESP32 vs STM32.
- Decisao documentada com justificativa.
- Proxima etapa: validar com o Diretor de Engenharia e escolher uma familia/modelo especifico de STM32.

## Explicação final

"Eu comparei ESP32 e STM32 pensando nos requisitos reais de uma ECU: temporizacao de injecao, ignicao, leitura de CKP, ADC e comunicacao. O ESP32 e muito bom para prototipo e conectividade, mas o STM32 parece mais adequado como controlador principal por ter timers e perifericos mais voltados a controle em tempo real. Minha proposta e usar STM32 no nucleo critico e manter ESP32 como opcao para Wi-Fi/telemetria."

---

# 1.3.1.2 - Aprender sobre o sensor CKP e a roda fonica

## O que e o CKP

CKP significa Crankshaft Position Sensor, ou sensor de posicao do virabrequim. Ele informa para a ECU duas coisas essenciais:

- a velocidade do motor, em RPM;
- a posicao angular aproximada do virabrequim.

Sem CKP, a ECU nao sabe em que ponto do ciclo o motor esta. Portanto, nao consegue decidir com seguranca quando injetar combustivel nem quando disparar a ignicao.

## O que e a roda fonica

A roda fonica e uma roda dentada presa ao virabrequim ou ao volante do motor. O sensor CKP detecta a passagem dos dentes. Um padrao muito comum e a roda **60-2**, que teria 60 posicoes, mas possui 2 dentes ausentes. Essa falha proposital cria uma referencia angular para a ECU.

Quando a ECU percebe um intervalo maior entre pulsos, ela identifica a regiao dos dentes faltantes e sincroniza a posicao do motor. Em termos simples:

- cada dente gera um evento;
- o tempo entre eventos indica a velocidade;
- o dente faltante indica uma referencia de posicao;
- a ECU usa essa referencia para agendar injecao e ignicao.

## Como a ECU calcula RPM

Uma roda 60-2 tem 58 dentes fisicos, mas representa 60 posicoes por volta. Como uma volta completa tem 360 graus, cada posicao representa:

```text
360 graus / 60 posicoes = 6 graus por dente
```

Se a ECU mede o tempo entre dentes, ela consegue estimar a velocidade angular. Um exemplo simplificado:

```text
RPM = 60 / tempo_de_uma_volta_em_segundos
```

Se a medicao for feita por dente:

```text
tempo_de_uma_volta = tempo_medio_entre_dentes * 60
RPM = 60 / (tempo_medio_entre_dentes * 60)
RPM = 1 / tempo_medio_entre_dentes
```

Essa simplificacao considera a roda como 60 posicoes. Na pratica, a ECU precisa lidar com aceleracao/desaceleracao do motor, ruido, dentes faltantes e variacao de velocidade dentro da propria volta.

## Sensor VR vs Hall

Existem dois tipos comuns de sensor CKP:

| Tipo | Sinal | Alimentacao | Vantagens | Cuidados |
|---|---|---|---|---|
| VR, relutor ou indutivo | Senoidal/analogico, amplitude varia com RPM | Normalmente nao precisa alimentacao | Robusto, comum em motores antigos | Precisa condicionamento de sinal; em baixa RPM o sinal e fraco |
| Hall | Digital/quadrado | Precisa alimentacao | Mais facil de ler no microcontrolador | Requer alimentacao e cuidado com nivel logico |

O sensor VR gera uma senoide conforme o dente se aproxima e se afasta. Em baixa rotacao, a amplitude e pequena; em alta rotacao, pode crescer bastante. Por isso nao deve ser ligado diretamente ao pino do microcontrolador. O ideal e usar circuito condicionador, como comparador, circuito com histerese ou CI dedicado para sensor VR.

O sensor Hall geralmente entrega uma onda quadrada, mais facil para a ECU ler por interrupcao ou input capture. Mesmo assim, e necessario proteger entrada contra ruido, transientes e tensoes fora da faixa.

## Circuito de condicionamento sugerido

Para sensor Hall:

```text
Sensor Hall -> resistor serie -> filtro/protecao -> entrada digital/input capture do MCU
                         |
                      pull-up conforme sensor
```

Para sensor VR:

```text
Sensor VR -> protecao de entrada -> condicionador VR/comparador com histerese -> entrada digital/input capture do MCU
```

O condicionador transforma o sinal analogico variavel em pulso digital limpo. Esse pulso digital e o que o firmware usa para medir tempo entre dentes.

## Pontos de projeto

- CKP deve ter prioridade alta no firmware.
- O sinal deve ser analisado com osciloscopio antes de confiar na leitura.
- A entrada precisa protecao contra ruido e sobretensao.
- Se usar apenas CKP, a ECU sabe a posicao em 360 graus, mas nao necessariamente a fase completa de 720 graus de um motor 4 tempos. Para injecao e ignicao totalmente sequenciais, normalmente tambem se usa sensor de fase no comando de valvulas.

## Entregavel da task

- Explicacao de como a ECU calcula RPM.
- Diferenca entre VR e Hall.
- Desenho preliminar do circuito de condicionamento.

## Explicação final

"O CKP e o sensor mais critico da ECU porque ele informa RPM e posicao do virabrequim. A roda fonica, por exemplo 60-2, gera pulsos onde cada dente representa uma janela angular, e os dentes faltantes servem como referencia. Eu separei o estudo entre sensor VR e Hall: Hall e mais direto por ser digital; VR e robusto, mas exige condicionamento antes de entrar no microcontrolador."

---

# 1.3.1.3 - Aprender sobre os sensores MAP e TPS e como le-los

## O que e o MAP

MAP significa Manifold Absolute Pressure. Ele mede a pressao absoluta no coletor de admissao. Essa pressao e usada para estimar a carga do motor.

Em uma estrategia speed-density, a ECU usa MAP, temperatura do ar, rotacao e parametros do motor para estimar massa de ar admitida e calcular combustivel. Em termos praticos:

- baixa pressao no coletor: borboleta pouco aberta, menor carga;
- pressao proxima da atmosferica: borboleta mais aberta, maior carga;
- pressao acima da atmosferica: motor turbo/supercharged.

## O que e o TPS

TPS significa Throttle Position Sensor. Ele mede a posicao da borboleta do acelerador. Normalmente e um potenciometro ou sensor de posicao que gera uma tensao analogica proporcional a abertura.

O TPS e importante para:

- detectar marcha lenta;
- detectar aceleracao rapida;
- enriquecer mistura em transientes;
- operar estrategias alpha-N, quando TPS e RPM sao usados como base de carga.

## Como a ECU le MAP e TPS

MAP e TPS geralmente entregam sinais analogicos. A ECU le esses sinais pelo ADC do microcontrolador.

O problema principal e compatibilidade de tensao. Muitos sensores automotivos trabalham com alimentacao de 5 V e saida de aproximadamente 0,5 V a 4,5 V. Muitos microcontroladores, como ESP32 e STM32 comuns, aceitam no maximo 3,3 V nas entradas analogicas. Logo, e preciso:

- divisor de tensao ou condicionamento analogico;
- filtro RC para reduzir ruido;
- protecao contra tensao acima do limite;
- calibracao do ADC.

## Exemplo com sensor MAP MPX4250A

O MPX4250A, da NXP, e um sensor de pressao absoluta de 20 kPa a 250 kPa, com saida analogica condicionada e compensada por temperatura. Segundo o datasheet, sua sensibilidade tipica e de 20 mV/kPa e a resposta e da ordem de 1 ms.

A formula de transferencia tipica do MPX4250A pode ser usada para converter tensao em pressao:

```text
Vout = Vs * (0,004 * P - 0,04)
```

Onde:

- `Vout` e a tensao de saida;
- `Vs` e a tensao de alimentacao, normalmente 5 V;
- `P` e a pressao em kPa.

Isolando a pressao:

```text
P = (Vout / Vs + 0,04) / 0,004
```

Exemplo:

```text
Vs = 5 V
Vout = 2,5 V
P = (2,5 / 5 + 0,04) / 0,004
P = (0,5 + 0,04) / 0,004
P = 135 kPa
```

## Leitura ADC

No ESP32, a documentacao da Espressif destaca que o ADC precisa considerar referencia, atenuacao e calibracao. A tensao calculada depende da resolucao, da referencia e do nivel de atenuacao configurado. Para uma ECU, nao basta usar `analogRead()` e assumir que o valor esta correto; e necessario calibrar e validar com multimetro/osciloscopio.

## Circuito de interface sugerido

Para sensor 0,5 V a 4,5 V entrando em ADC 3,3 V:

```text
Sensor 5 V
  Vout ---- resistor R1 ----+---- resistor serie pequeno ---- ADC MCU
                            |
                          resistor R2
                            |
                           GND
```

O divisor deve reduzir a tensao maxima para abaixo de 3,3 V. Exemplo conceitual:

```text
4,5 V -> divisor -> aproximadamente 3,0 V
```

Depois disso, a ECU precisa reverter matematicamente o divisor para recuperar a tensao original do sensor.

## Pontos de projeto

- Alimentacao de sensores deve ser estavel e preferencialmente regulada em 5 V.
- O terra dos sensores deve ser bem planejado para evitar ruido de injetores e bobinas.
- MAP e TPS precisam filtragem, mas sem atraso excessivo.
- TPS deve ser calibrado com tensao de borboleta fechada e aberta.
- MAP precisa calibracao conforme o sensor escolhido.

## Entregavel da task

- Simular ou testar leitura do MAP.
- Converter tensao para kPa.
- Desenhar circuito de interface MAP/TPS para ADC.

## Explicação final

"MAP e TPS sao sensores analogicos usados para entender carga do motor e posicao do acelerador. O MAP permite estimar pressao no coletor e o TPS ajuda a detectar abertura e transientes. O principal ponto de hardware e adaptar sinais automotivos de ate 5 V para ADC de 3,3 V, com divisor, filtro, protecao e calibracao."

---

# 1.3.1.4 - Aprender sobre sensores de temperatura NTC (CLT e IAT)

## O que sao CLT e IAT

CLT significa Coolant Temperature Sensor, sensor de temperatura do liquido de arrefecimento. IAT significa Intake Air Temperature, sensor de temperatura do ar admitido.

Ambos sao importantes para a ECU:

- CLT indica se o motor esta frio, aquecido ou superaquecendo;
- IAT ajuda a corrigir a densidade do ar admitido;
- temperaturas influenciam enriquecimento de partida, marcha lenta, avanco, protecoes e ventilador.

## O que e um NTC

NTC significa Negative Temperature Coefficient. E um resistor cuja resistencia diminui quando a temperatura aumenta.

Exemplo conceitual:

- frio: resistencia alta;
- quente: resistencia baixa.

Isso e diferente de um sensor linear. O NTC nao entrega diretamente "graus Celsius"; ele entrega uma resistencia que precisa ser convertida.

## Como ler um NTC com microcontrolador

O jeito mais comum e usar divisor de tensao:

```text
3,3 V ou 5 V ---- resistor fixo ----+---- ADC
                                    |
                                   NTC
                                    |
                                   GND
```

Conforme a temperatura muda, a resistencia do NTC muda, e a tensao no ponto do ADC tambem muda.

Com a leitura ADC, a ECU calcula:

1. tensao medida;
2. resistencia do NTC;
3. temperatura em Celsius.

## Conversao de ADC para resistencia

Se o NTC esta ligado ao GND e o resistor fixo ao Vcc:

```text
Vadc = Vcc * Rntc / (Rfixo + Rntc)
```

Isolando `Rntc`:

```text
Rntc = Rfixo * Vadc / (Vcc - Vadc)
```

Depois, usa-se a equacao Beta ou uma tabela de calibracao.

## Equacao Beta simplificada

```text
1/T = 1/T0 + (1/B) * ln(R/R0)
```

Onde:

- `T` e a temperatura em Kelvin;
- `T0` normalmente e 298,15 K, equivalente a 25 graus Celsius;
- `B` e a constante beta do termistor;
- `R` e a resistencia medida;
- `R0` e a resistencia nominal em 25 graus Celsius.

Depois de calcular Kelvin:

```text
Celsius = Kelvin - 273,15
```

## Tabela vs formula

Para ECU, muitas vezes e mais pratico usar tabela de calibracao do sensor. Isso e comum em projetos como Speeduino e MegaSquirt: o usuario informa pontos temperatura/resistencia, e o software cria uma curva.

A formula Beta e boa para estudo e prototipo. Para calibracao final, uma tabela do sensor real tende a ser mais fiel.

## Pontos de projeto

- O resistor fixo deve ser escolhido para dar boa resolucao na faixa de temperatura importante.
- CLT precisa boa resolucao entre motor frio e temperatura de operacao.
- IAT precisa resposta razoavel para mudancas de temperatura do ar.
- O circuito deve evitar autoaquecimento do NTC por corrente excessiva.
- A entrada ADC deve ter filtro e protecao.
- O terra de sensores deve ser separado logicamente de retornos de alta corrente.

## Entregavel da task

- Ler NTC pelo ADC.
- Converter ADC para resistencia e temperatura.
- Desenhar circuito de interface.

## Explicação Final

"CLT e IAT normalmente sao sensores NTC. Eles nao entregam temperatura diretamente; entregam uma resistencia que varia com a temperatura. A ECU mede isso com um divisor de tensao no ADC e converte por formula Beta ou tabela de calibracao. O proximo passo e testar um NTC real e validar a curva."

---

# 1.3.1.5 - Aprender sobre a sonda lambda e como medir a mistura ar-combustivel

## O que e sonda lambda

A sonda lambda mede a quantidade de oxigenio restante no escapamento. A partir disso, a ECU ou o preparador consegue inferir se a mistura ar-combustivel esta:

- rica: combustivel em excesso;
- pobre: ar em excesso;
- estequiometrica: proporcao ideal para combustao completa em condicoes especificas.

Para gasolina, a referencia estequiometrica costuma ser proxima de AFR 14,7:1, equivalente a lambda 1. Para outros combustiveis, o AFR muda, mas lambda continua sendo uma forma normalizada de medir a mistura.

## Banda estreita vs banda larga

| Tipo | O que mede bem | Saida | Uso comum | Limitacao |
|---|---|---|---|---|
| Banda estreita | Detecta se esta rico ou pobre perto de lambda 1 | Sinal muito sensivel perto do estequiometrico | Controle fechado em carros originais | Nao mede com precisao AFR fora da regiao estequiometrica |
| Banda larga | Mede uma faixa ampla de lambda/AFR | Normalmente via controlador 0-5 V, CAN ou serial | Calibracao, telemetria e acerto de mapa | Precisa controlador dedicado e aquecimento controlado |

Uma sonda banda estreita funciona bem para dizer "rico ou pobre" perto de lambda 1, mas nao informa com precisao se o motor esta em AFR 12,5, 13,2 ou 15,5. Para calibrar uma ECU programavel, isso e uma limitacao grande.

A sonda banda larga, como Bosch LSU 4.9, permite medir uma faixa muito maior de lambda. O datasheet da Bosch Motorsport descreve a LSU 4.9 como sensor wideband com faixa de lambda ampla e deixa claro que ela opera com um circuito/controlador especifico, nao ligada diretamente ao microcontrolador.

## Por que banda estreita nao e suficiente para calibracao

Em calibracao, especialmente em plena carga, partida, aquecimento e transientes, a ECU precisa saber "quanto" rica ou pobre esta a mistura, nao apenas o lado em relacao a lambda 1.

Exemplo:

- marcha lenta e cruzeiro podem operar perto de lambda 1;
- plena carga aspirada geralmente exige mistura mais rica;
- turbo exige ainda mais cuidado com mistura, temperatura e detonacao;
- partida fria precisa enriquecimento.

Uma banda estreita nao da informacao quantitativa suficiente para acertar esses pontos.

## Decisao sugerida

Para o projeto TechGears, a recomendacao preliminar e:

- usar **sonda lambda banda larga com controlador dedicado** para calibracao e telemetria;
- aceitar banda estreita apenas como entrada opcional ou modo simples, nao como base principal de acerto.

O controlador pode entregar:

- sinal analogico 0-5 V;
- CAN;
- serial;
- ou interface propria, dependendo do modelo.

Para uma primeira ECU didatica, a rota mais simples e ler saida analogica 0-5 V do controlador wideband, reduzindo para 3,3 V se necessario e calibrando a curva tensao/lambda.

## Pontos de projeto

- A sonda wideband nao deve ser ligada diretamente ao ADC.
- O aquecedor da sonda exige controle especifico.
- O sinal analogico do controlador precisa terra bem referenciado.
- A ECU precisa saber a curva de saida do controlador.
- Para confiabilidade, CAN e melhor que analogico quando o controlador oferecer essa opcao.

## Entregavel da task

- Explicar banda estreita vs banda larga.
- Justificar por que banda larga e melhor para calibracao.
- Documentar decisao preliminar de usar wideband com controlador.

## Explicação Final

"A sonda lambda serve para medir se a mistura esta rica ou pobre. A banda estreita e util perto de lambda 1, mas nao e boa para calibrar mapa porque nao mede uma faixa ampla de AFR. Para uma ECU programavel, a melhor decisao e usar sonda banda larga com controlador dedicado, e a ECU le a saida calibrada desse controlador."

---

# 1.3.1.6 - Aprender sobre o driver eletronico para controlar os injetores

## O que e um injetor para a ECU

Do ponto de vista eletrico, o injetor e uma carga indutiva: uma bobina que abre uma valvula quando circula corrente. A ECU controla o tempo em que essa bobina fica energizada.

O tempo de injecao e chamado de pulse width. Exemplo:

```text
Injetor ligado por 3,2 ms -> injeta uma quantidade correspondente de combustivel
```

A ECU nao consegue ligar o injetor diretamente pelo pino do microcontrolador, porque:

- o pino fornece pouca corrente;
- o injetor trabalha com 12 V;
- a bobina gera picos de tensao ao desligar;
- o ambiente automotivo tem transientes.

Por isso e necessario um driver.

## Driver low-side com MOSFET

O circuito mais comum para estudo e um driver low-side:

```text
+12 V ---- injetor ---- dreno MOSFET
                         source MOSFET ---- GND
                         gate MOSFET <---- sinal MCU via resistor/driver
```

Quando o microcontrolador ativa o gate, o MOSFET conduz e fecha o caminho para o terra. O injetor energiza. Quando o gate desliga, a corrente para e o injetor fecha.

## Por que precisa diodo flyback ou clamp

Como o injetor e indutivo, ele armazena energia magnetica. Quando o MOSFET desliga, a corrente nao cai instantaneamente. A bobina tenta manter a corrente e pode gerar uma tensao alta o suficiente para danificar o MOSFET ou causar ruido.

Um diodo flyback, TVS ou clamp fornece um caminho controlado para essa energia.

Porem existe um detalhe importante: um diodo simples reduz o pico de tensao, mas pode fazer a corrente demorar mais para cair. Em injetores, isso pode atrasar o fechamento. Por isso, drivers automotivos podem usar clamp em tensao mais alta, avalanche controlada ou estrategias peak-and-hold, dependendo do tipo de injetor.

A Texas Instruments mostra em nota de aplicacao que a tensao de clamp influencia diretamente o tempo de descarga de uma carga indutiva: clamp mais alto descarrega mais rapido, mas exige componentes dimensionados para a energia. A Nexperia tambem compara topologias de driver de solenoide, incluindo freewheel, avalanche e active clamp.

## High-Z vs Low-Z

Injetores podem ser:

- High impedance / High-Z: resistencia maior, controle on/off mais simples;
- Low impedance / Low-Z: resistencia menor, corrente maior, geralmente exige controle peak-and-hold.

Para um primeiro prototipo didatico, o mais seguro e estudar e testar com injetores High-Z, pois o driver e mais simples.

## Circuito sugerido para estudo

```text
MCU GPIO -> resistor gate -> gate MOSFET logic-level
                         |
                      resistor pull-down
                         |
                        GND

+12 V -> injetor -> dreno MOSFET
source MOSFET -> GND de potencia

Protecao:
- diodo flyback, TVS ou clamp conforme topologia
- desacoplamento na alimentacao
- trilhas dimensionadas para corrente
- separacao entre GND de potencia e GND de sensores
```

## Como avaliar o MOSFET do laboratorio

Para decidir se um MOSFET serve, verificar:

- `Vds`: deve suportar tensao maior que o barramento automotivo e transientes;
- `Id`: corrente continua e pulsada suficiente;
- `Rds(on)`: quanto menor, menos aquecimento;
- `Vgs(th)` e curva de conducao: precisa conduzir bem com 3,3 V ou 5 V no gate;
- dissipacao termica;
- capacidade de lidar com energia indutiva, se for usar avalanche;
- encapsulamento e dissipador.

Importante: `Vgs(th)` nao e a tensao para o MOSFET "ligar bem"; e apenas o ponto em que comeca a conduzir uma corrente pequena. Para driver com microcontrolador, o ideal e MOSFET logic-level com baixa resistencia especificada em `Vgs = 4,5 V` ou menor.

## Entregavel da task

- Explicar o que acontece sem flyback/clamp.
- Simular driver no Tinkercad ou ferramenta equivalente.
- Identificar MOSFET disponivel e avaliar datasheet.

## Explicação Final

"O injetor e uma carga indutiva, entao nao pode ser ligado direto no microcontrolador. O circuito base e um MOSFET low-side, onde a ECU controla o gate e o MOSFET chaveia o terra do injetor. O ponto critico e o desligamento: sem flyback ou clamp, a energia da bobina gera pico de tensao que pode queimar o driver. Para prototipo, o melhor caminho e testar primeiro com injetor High-Z e MOSFET logic-level."

---

# 1.3.1.7 - Aprender sobre o driver para controlar as bobinas de ignicao

## O que a bobina de ignicao faz

A bobina de ignicao transforma energia eletrica em alta tensao para gerar centelha na vela. Ela tem um enrolamento primario, alimentado em baixa tensao, e um secundario, que gera alta tensao.

A ECU controla o primario. Ela energiza a bobina por um periodo chamado dwell e depois interrompe a corrente. Quando a corrente e interrompida, o campo magnetico colapsa e a alta tensao aparece no secundario, disparando a vela.

## O que e dwell time

Dwell time e o tempo em que a bobina fica carregando antes da centelha.

Se o dwell for curto demais:

- a bobina nao carrega energia suficiente;
- a centelha pode ficar fraca;
- pode haver falha de ignicao.

Se o dwell for longo demais:

- a bobina aquece;
- o driver aquece;
- pode ocorrer saturacao;
- pode danificar bobina ou modulo.

A documentacao do Speeduino define dwell como o periodo em que a corrente e aplicada ao primario da bobina para carrega-la antes da faisca. Esse conceito e central para qualquer ECU programavel.

## MOSFET vs IGBT

MOSFET e IGBT sao chaves semicondutoras, mas com comportamentos diferentes.

| Criterio | MOSFET | IGBT |
|---|---|---|
| Melhor uso comum | Baixa/media tensao, chaveamento rapido | Tensoes/correntes maiores, cargas como ignicao |
| Controle | Gate por tensao | Gate por tensao |
| Queda em conducao | Depende de Rds(on) | Tem queda Vce(sat) |
| Uso em injetor | Muito comum | Menos comum |
| Uso em bobina passiva | Pode existir, mas IGBT e mais tradicional | Muito comum em drivers de ignicao |

Em bobinas passivas, o driver precisa suportar alta energia indutiva e picos durante o desligamento. Por isso, modulos e projetos de ignicao costumam usar IGBTs automotivos dedicados, muitas vezes com protecoes internas.

## Bobina ativa vs passiva

Existem duas familias importantes:

- Bobina ativa: ja possui igniter/driver interno. A ECU manda um pulso logico, geralmente 5 V, e a propria bobina lida com a potencia.
- Bobina passiva: nao possui driver interno. A ECU precisa chavear corrente do primario com IGBT ou modulo externo.

Para uma ECU universitaria, bobinas ativas podem simplificar muito o hardware inicial. Bobinas passivas sao mais didaticas para entender ignicao, mas exigem mais cuidado com energia, calor e protecao.

## Circuito conceitual para bobina passiva

```text
+12 V ---- primario da bobina ---- coletor IGBT
                                emissor IGBT ---- GND potencia
                                gate IGBT <---- driver/opto/estagio de controle
```

O microcontrolador nao deve acionar diretamente um IGBT de ignicao sem estudar:

- corrente de gate;
- resistor de gate;
- pull-down;
- isolamento ou buffer;
- protecao contra transientes;
- dissipacao;
- aterramento;
- polaridade do sinal.

## Papel do optoacoplador PC817

O PC817 e um optoacoplador simples com LED interno e fototransistor. Ele pode isolar logicamente duas partes do circuito, mas precisa ser usado com cuidado:

- e relativamente lento para algumas aplicacoes rapidas;
- tem variacao grande de CTR;
- pode distorcer bordas se mal dimensionado;
- nao substitui um driver de gate robusto.

Para estudo, ele ajuda a entender isolamento. Para uma ECU final, pode ser melhor avaliar driver dedicado, buffer, isolador digital ou bobinas ativas.

## Pontos de projeto

- Dwell deve ser limitado por software e, se possivel, por protecao de hardware.
- A polaridade do sinal de ignicao e critica. Saida invertida pode manter bobina carregando e queimar o conjunto.
- Bobinas e drivers precisam dissipacao.
- Terra de potencia deve ser bem planejado.
- Testes devem comecar em bancada, com fonte limitada e carga apropriada.
- E necessario medir corrente e forma de onda com osciloscopio antes de usar em motor.

## Decisao preliminar

Para reduzir risco no primeiro prototipo:

1. estudar IGBT e driver de bobina passiva para entender a arquitetura;
2. considerar bobina ativa como caminho inicial mais seguro de integracao;
3. se for usar bobina passiva, selecionar IGBT/modulo automotivo com protecoes adequadas e validar dwell em bancada.

## Entregavel da task

- Explicar MOSFET vs IGBT.
- Explicar dwell time e limite seguro.
- Desenhar circuito preliminar de driver de bobina.

## Explicação Final

"Bobina de ignicao e mais critica que injetor porque o driver precisa lidar com alta energia e o dwell errado pode queimar bobina ou modulo. O IGBT e mais comum para bobina passiva, enquanto MOSFET aparece mais em cargas como injetores. Tambem levantei a diferenca entre bobina ativa e passiva: para prototipo, bobina ativa pode reduzir risco, mas o estudo de IGBT continua importante para entender a ECU."

---

# Conclusao geral

As sete tasks formam uma base tecnica coerente para a Feature 1.3 - Estudo de Hardware da ECU. Elas nao sao tarefas de implementacao final ainda; sao tarefas de entendimento, decisao e desenho preliminar.

O principal aprendizado e que a ECU precisa ser dividida em blocos:

- MCU e temporizacao: preferencia preliminar por STM32 como nucleo critico;
- sensores de posicao: CKP com roda fonica e condicionamento de sinal;
- sensores analogicos: MAP, TPS, CLT e IAT com ADC, divisor, filtro e calibracao;
- mistura: lambda wideband com controlador dedicado;
- atuadores: MOSFET para injetores e IGBT/driver dedicado para bobinas.

Nessa historia, eu foquei em entender os componentes antes de montar a ECU. O estudo mostrou que a parte critica e separar bem sensores, atuadores e processamento em tempo real. A decisao tecnica preliminar e usar STM32 como controlador principal e considerar ESP32 para conectividade. Nos sensores, o desafio e condicionamento e calibracao. Nos atuadores, o desafio e protecao e chaveamento de cargas indutivas. O proximo passo e validar as decisoes e transformar os circuitos preliminares em esquematicos testaveis."

---

# Fontes consultadas

- Espressif - Motor Control PWM (MCPWM) para ESP32: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/mcpwm.html
- Espressif - ADC do ESP32 e calibracao: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/adc/index.html
- STMicroelectronics - STM32G491 datasheet, timers avancados de motor control: https://www.st.com/resource/en/datasheet/stm32g491ke.pdf
- STMicroelectronics - AN4013, introducao a timers para STM32: https://www.st.com/resource/en/application_note/dm00042534-timers-and-pwm-generation-using-stm32-microcontrollers-stmicroelectronics.pdf
- Haltech - Trigger System, sensores VR/Hall e sinal de referencia: https://support.haltech.com/portal/en/kb/articles/trigger
- Motorsport Electronics - Triggering Setup/Crank/Cam Sensors: https://www.motorsport-electronics.co.uk/onlinehelp/html/TriggeringSetupCrankCamSensors.html
- NXP - MPX4250A Manifold Absolute Pressure Sensor datasheet: https://www.nxp.com/docs/en/data-sheet/MPX4250A.pdf
- Microchip - AN897, medicao com termistor NTC em divisor resistivo: https://www.microchip.com/en-us/application-notes/an897
- Bosch Motorsport - Lambda Sensor LSU 4.9 datasheet: https://www.bosch-motorsport.com/content/downloads/Raceparts/Resources/pdf/Data%20sheet_69034379_Lambda_Sensor_LSU_4.9.pdf
- TI - Inductive load clamping and freewheeling behavior: https://www.ti.com/document-viewer/lit/html/slvaf04
- Nexperia - Driving solenoids in automotive applications: https://www.nexperia.com/applications/interactive-app-notes/IAN50003_driving-automotive-solenoids.html
- Speeduino Doxygen - conceito de dwell em IgnitionSchedule: https://speeduino.github.io/speeduino-doxygen/struct_ignition_schedule.html
- Motorsport Electronics - Ignition Coil Connections, bobinas ativas/passivas: https://motorsport-electronics.co.uk/onlinehelp/html/IgnitionCoilConnections.html
- DIYAutoTune - Bosch BIP373 ignition module e riscos de dwell excessivo: https://diyautotune.com/blogs/technical-articles/bosch-bip373-ignition-module
