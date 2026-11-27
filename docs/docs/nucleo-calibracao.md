---
title: Núcleo de cálculo e calibração
slug: /nucleo-calibracao
sidebar_position: 6
---

<div style={{textAlign: 'justify'}}>

# Núcleo de cálculo e calibração

Projeto: Programmable ECU - TechGears Inteli  
Tasks: 1.6.1.6 (correções 2D, #184), 1.6.1.7 (ficha do motor, #185) e 1.6.1.8 (gravação e backup, #186)  
Código: `src/firmware/common/` (C99, sem Arduino e sem LVGL)

## Por que um núcleo separado

&emsp;O cálculo do motor, os limites da calibração e a gravação ficam num **núcleo portátil em C**. O mesmo código roda em três lugares:

- no ESP do motor;
- no motor virtual do simulador;
- nos testes do computador.

&emsp;Quando a tela mostra "Pulse Width 5,31 ms", o número foi calculado pela mesma função que vai rodar no carro. E um teste no PC confere essa função sem precisar de placa.

&emsp;O núcleo **não** agenda o pulso no tempo. Ele calcula quanto injetar e qual avanço usar. Ler a roda fônica, os timers e as saídas é trabalho do firmware do motor (feature 1.5), que deve chamar `ecu_engine_compute()` no laço e `ecu_motor_link_feed()` na task da UART.

<div align="center">

<small><strong>Quadro 1 - Módulos do núcleo</strong></small>

| Arquivo | O que faz |
|---|---|
| `ecu_crc` | CRC-16 dos frames e CRC-32 da calibração |
| `ecu_protocol` | Frames, parser e mensagens da UART ([Protocolo UART](/protocolo-uart)) |
| `ecu_calib` | Tabelas, limites, ficha do motor, calibração padrão e blob de gravação |
| `ecu_engine` | Interpolação, Pulse Width, correções, avanço e ordem de ignição |
| `ecu_motor_link` | Lado do motor: responde pedidos, aplica limites, grava, telemetria |
| `ecu_hmi_link` | Lado da tela: cópia das tabelas, fila de pedidos, reenvio e eventos |

Fonte: elaborado pelos autores.

</div>

## Como o tempo de injeção é calculado

&emsp;O mapa de combustível guarda o **tempo de injeção em milissegundos**, como no modelo FuelTech e no exemplo da [Teoria do motor](/teoria-do-motor). Ele tem 16 colunas de rotação (500 a 7000 rpm) e 16 linhas de carga (20 a 100 kPa). O ESP interpola entre as quatro células vizinhas e depois aplica a ficha e as correções:

```text
PW = célula_interpolada × (vazão_ref / vazão_bico)
       × (1 + aquecimento) × (1 + ar) × (1 + partida) × (1 + aceleração)
     + tempo_morto(tensão da bateria)
```

&emsp;Com todas as correções em zero, vazões iguais e tempo morto zero, o **PW é exatamente o valor da célula**. É o critério da task 1.6.1.6, e há um teste que confere isso.

&emsp;A conta é feita em inteiros com ponto fixo (frações de 1/65536), sem `float`. Assim o ESP32, o PC e os testes chegam ao mesmo número até o último dígito. O exemplo da Teoria do motor (2500 rpm e 60 kPa entre 3,6, 4,2, 4,8 e 5,6 ms) dá 4,55 ms, e isso também está nos testes.

&emsp;**Célula atual.** Para a tela destacar onde o motor está, usamos a célula **mais próxima** nos dois eixos. O valor usado no cálculo continua sendo o interpolado. A telemetria manda os dois, para a tela poder mostrar "célula [8,6] = 5,79 ms · interpolado 5,72 ms".

## Correções 2D (1.6.1.6)

&emsp;Cada correção é uma curva de 8 pontos que a tela edita pelo mesmo protocolo das células. O motor interpola a curva pelo sensor e aplica no PW. As correções em % usam **0 como "sem correção"**: +20 % multiplica por 1,20 e -10 % por 0,90.

<div align="center">

<small><strong>Quadro 2 - Correções sobre o mapa principal</strong></small>

| Curva | Entrada | Efeito no PW | Padrão de fábrica |
|---|---|---|---|
| Aquecimento | Temperatura do motor (CLT) | Multiplica por (1 + %) | +45 % a -10 °C, chegando a 0 a 90 °C |
| Temperatura do ar | IAT | Multiplica por (1 + %) | +6 % a -10 °C, chegando a -8 % a 60 °C |
| Partida | CLT, só com rpm abaixo da rotação de partida | Multiplica por (1 + %) | +250 % frio, +60 % quente |
| Bateria | Tensão | Soma o tempo morto do bico (µs) | 1600 µs a 8 V, chegando a 660 µs a 15 V |
| Aceleração rápida | Velocidade do TPS (%/s) | Multiplica por (1 + %) e segura 300 ms | 0 abaixo de 50 %/s, até +45 % |

Fonte: elaborado pelos autores, a partir de Speeduino (Warmup, Acceleration Wizard e Injector Characteristics) e da Teoria do motor.

</div>

&emsp;A tela mostra a correção que está valendo agora, porque ela vem na telemetria. Os valores padrão são um ponto de partida para a bancada, não um acerto do Fit.

## Ficha do motor (1.6.1.7)

&emsp;A ficha é o que dá unidade ao mapa. Um valor de "5,00 ms" só quer dizer alguma quantidade de combustível se soubermos o bico. Por isso a ficha guarda a **vazão do bico instalado** e a **vazão de referência do mapa** (o bico com que o mapa foi feito). Trocar por um bico 25 % maior (240 → 300 cc/min) faz a mesma célula gerar um PW 20 % menor (× 240/300).

<div align="center">

<small><strong>Quadro 3 - Itens da ficha</strong></small>

| Item | Padrão | Limite | Efeito |
|---|---|---|---|
| Cilindros | 4 | 1 a 8 (sem 7) | Ordem de ignição (1-3-4-2) e disparo a cada 720°/n |
| Dentes da roda fônica | 36 | 4 a 60 | Leitura do giro (1.5) |
| Dentes faltantes | 1 | 0 a 2 | 36-1; precisa ser menor que metade dos dentes |
| Vazão do bico | 240 cc/min | 50 a 2000 | Escala o PW |
| Vazão de referência do mapa | 240 cc/min | 50 a 2000 | Escala o PW |
| Dwell da bobina | 3,0 ms | 1,0 a 6,0 ms | Tempo de carga da bobina |
| Rotação de partida | 400 rpm | 100 a 800 | Abaixo disso vale a curva de partida |
| Alerta de rotação | 6500 rpm | 1000 a 9000 | Alerta na tela (1.7.1.3) |
| Alerta de temperatura | 105,0 °C | 60,0 a 130,0 | Alerta na tela (1.7.1.3) |

Fonte: elaborado pelos autores. A roda do Fit ainda será medida (ver Contexto das decisões).

</div>

&emsp;**Mudar a ficha só com o motor parado.** Trocar cilindros ou a roda fônica muda o cálculo inteiro. Por isso o ESP responde MOTOR_GIRANDO se a rotação não for zero. Mapas e curvas, ao contrário, podem ser acertados com o motor ligado: é para isso que existe a tela.

## Gravação e backup (1.6.1.8)

&emsp;As edições valem na hora, mas ficam na RAM. **Nada vai para a flash sem o comando Gravar.** Quando grava, o ESP transforma a calibração inteira num blob de 1342 bytes:

```text
"TGCL" (4 bytes) | versão (2) | tamanho (2) | CRC-32 do conteúdo (4) | valores int16 little-endian
```

- **NVS (Non-Volatile Storage).** No ESP do motor, o blob vai para a NVS da Espressif. Ela grava em forma de diário: se faltar energia no meio, fica o blob antigo ou o novo, nunca metade. Isso atende o que a [Arquitetura](/arquitetura) pede ("uma gravação incompleta não corrompe o mapa em uso").
- **Nunca dentro da interrupção do giro.** Enquanto grava a flash, o ESP32 desliga o cache e trava qualquer código que não esteja na IRAM. Por isso a gravação roda na task da UART, e só com o motor parado. Se o motor estiver girando, a resposta é MOTOR_GIRANDO.
- **Ao ligar,** o ESP confere magic, versão, tamanho, CRC e os limites de cada valor. Se algo não fechar (flash vazia ou corrompida), carrega a calibração padrão e liga o bit "calibração padrão" na telemetria.
- **Backup.** Exportar copia o blob, em blocos de 200 bytes, para um arquivo (`/backup.tgcal` na LittleFS da S3, ou `backup.tgcal` no simulador). Importar manda o arquivo de volta. O ESP confere tudo de novo, e um backup com CRC errado ou valor fora do limite é recusado inteiro. O backup aceito vale na RAM, e para manter depois de reiniciar é preciso gravar.

## Como foi verificado

<div align="center">

<small><strong>Quadro 4 - Testes do núcleo (python tools/host_build.py)</strong></small>

| Conjunto | O que confere |
|---|---|
| `test_protocol` (10 testes) | CRC, frame, parser, mensagens |
| `test_calib` (8 testes) | Limites da célula, ficha, blob ida e volta, blob corrompido recusado, gravar e "reiniciar" |
| `test_engine` (8 testes) | Interpolação (exemplo da teoria), correções em zero = célula, cada correção muda o PW, aceleração, troca de bico, ordem de ignição |
| `test_links` (11 testes) | Tela e motor conversando: aceita e recusa, ficha só parado, gravar e reiniciar, backup exportado e importado com os mesmos bytes, reenvio sem aplicar duas vezes, NACK |

Fonte: elaborado pelos autores.

</div>

## Referências

- ESPRESSIF. _ESP-IDF Programming Guide (v4.4): Non-Volatile Storage Library._ Disponível em: [https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/storage/nvs_flash.html](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/storage/nvs_flash.html). Acesso em: 25 nov. 2026.
- ESPRESSIF. _ESP-IDF Programming Guide (v4.4): Concurrency Constraints for Flash on SPI1._ Disponível em: [https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/storage/spi_flash_concurrency.html](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/storage/spi_flash_concurrency.html). Acesso em: 25 nov. 2026.
- SPEEDUINO. _Injector Characteristics_ (Speeduino Manual). Disponível em: [https://wiki.speeduino.com/en/configuration/Injector_Characteristics](https://wiki.speeduino.com/en/configuration/Injector_Characteristics). Acesso em: 25 nov. 2026.
- SPEEDUINO. _Warmup_ (Speeduino Manual). Disponível em: [https://wiki.speeduino.com/en/configuration/Warmup](https://wiki.speeduino.com/en/configuration/Warmup). Acesso em: 25 nov. 2026.
- SPEEDUINO. _Acceleration Wizard_ (Speeduino Manual). Disponível em: [https://wiki.speeduino.com/en/configuration/Acceleration_Wizard](https://wiki.speeduino.com/en/configuration/Acceleration_Wizard). Acesso em: 25 nov. 2026.
- FUELTECH. _FT450: manual de instalação e operação._ Porto Alegre: FuelTech, s.d.
- HEYWOOD, John B. _Internal Combustion Engine Fundamentals._ 2. ed. New York: McGraw-Hill, 2018.
- THROWTHESWITCH. _Unity: unit testing for C._ Disponível em: [https://www.throwtheswitch.org/unity](https://www.throwtheswitch.org/unity). Acesso em: 25 nov. 2026.

</div>
