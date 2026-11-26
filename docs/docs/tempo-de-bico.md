---
title: Cálculo do Tempo de Bico
slug: /tempo-de-bico
sidebar_position: 6
---

# Cálculo do Tempo de Bico

<div style={{textAlign: 'justify'}}>

&emsp;O **tempo de bico**, ou _pulse width_, é por quanto tempo a ECU mantém o injetor aberto a cada ciclo. É a saída do cálculo de combustível: parte do valor lido no mapa, ajusta por correções e soma o atraso do próprio injetor. Esta página descreve a implementação da task 1.5.1.5, que usa a [interpolação bilinear](/interpolacao-bilinear) para ler o mapa. A teoria por trás (mapa, correções e dead time) está na página de [Teoria do Motor](/teoria-do-motor).

## Onde está o código

&emsp;Como no restante do núcleo, o código é C99 puro, sem `Arduino.h`, e compila tanto para o ESP32 quanto para os testes do computador. Fica em `src/firmware/core/`:

- `ecu_fuel.h` e `ecu_fuel.c`: o cálculo do tempo de bico
- `ecu_curve.h` e `ecu_curve.c`: curva 1D com interpolação linear, usada aqui para o dead time e reaproveitável nas correções por temperatura
- `src/firmware/test/test_fuel.c` e `test_curve.c`: os testes

## A conta

```text
pw = base × corr[0] × corr[1] × ...        (fatores em permil)
pw = 0                       se o resultado das correções for 0
pw = pw + dead_time(Vbat)    caso contrário
pw = min(pw, max_us)
```

&emsp;É a mesma ideia da fórmula `tempo comandado = tempo calculado + dead time(tensão da bateria)` da página de teoria, com as correções multiplicativas da etapa 3 do núcleo de controle descrito em [Arquitetura](/arquitetura). O `base` é o valor do mapa já interpolado por `ecu_table_lookup`.

## Unidades

&emsp;Todo o firmware usa as mesmas unidades, para o mapa, as correções e a futura interface falarem a mesma língua. A escolha de microssegundos para as células do mapa mantém os números do Quadro 6 da teoria (por exemplo, 2,0 ms vira 2000) sem perder precisão e entrega ao temporizador o valor na unidade que ele consome.

<div align="center">

<small><strong>Quadro 1 - Unidades usadas no cálculo</strong></small>

| Grandeza | Unidade | Exemplo |
|---|---|---|
| Tempo (células do mapa, resultado, dead time) | microssegundo (µs) | 4,55 ms = 4550 |
| Tensão da bateria | milivolt (mV) | 13,8 V = 13800 |
| Fator de correção | permil | 1000 = sem correção, 1200 = +20 %, 0 = corte |

<small>Fonte: Autoria própria (`src/firmware/core/ecu_fuel.h`).</small>

</div>

## Decisões de implementação

- **Corte de combustível não soma dead time.** Se as correções zeram o tempo, o resultado é 0 e o injetor nem é energizado. Somar só o dead time abriria o injetor sem combustível útil.
- **Dead time vem de uma curva de tensão.** Com a bateria mais fraca o injetor abre mais devagar, então o atraso cresce. A curva é obrigatória; para não compensar, usa-se uma curva com todos os `y` em 0. Assim ninguém esquece de configurar sem perceber.
- **Teto do tempo comandado (`max_us`).** O tempo de bico não pode passar do tempo do ciclo do motor. Quem chama informa o teto, e `UINT32_MAX` desliga o limite.
- **Cada fator é aplicado em sequência, com arredondamento.** A ordem dos fatores muda o resultado em no máximo 1 µs por fator (há um teste que mostra: 5996 contra 5997). É irrelevante para o motor e mantém a conta simples.
- **Saturação em `UINT32_MAX`.** Uma cadeia de fatores absurdos não pode estourar a conta e voltar como um número pequeno.
- **Entrada inválida é recusada.** Ponteiro nulo, `n_corr` maior que zero sem vetor de fatores, ou curva de dead time inválida fazem a função devolver `false` sem escrever na saída.

## Como usar

```c
const ecu_table_t *fuel_map = /* mapa de combustível, células em us */;
const ecu_curve_t *dead_time = /* dead time do injetor, mV -> us */;

uint16_t base;
const uint16_t corr[2] = {1200, 970};      /* temperatura do motor e do ar */
uint32_t pw_us;

if (ecu_table_lookup(fuel_map, rpm, map_kpa, &base)) {
    const ecu_fuel_in_t in = {
        .base_us = base,
        .corr_permille = corr, .n_corr = 2,
        .dead_time = dead_time, .vbat_mv = vbat_mv,
        .max_us = cycle_us,
    };
    if (ecu_fuel_pulse_width_us(&in, &pw_us)) {
        /* agenda o injetor por pw_us microssegundos */
    }
}
```

## Como rodar os testes

```bash
make -C src/firmware/test
```

&emsp;A saída esperada são três blocos terminando em `ok: N verificacoes passaram`. O teste do cálculo parte do caso de mesa do próprio time: o mapa 4×4 do Quadro 6 da página de teoria, onde o ponto 2500 RPM e 60 kPa dá 4,55 ms.

<div align="center">

<small><strong>Quadro 2 - Casos de teste conferidos à mão (dead time de teste, 14 V = 700 µs)</strong></small>

| Caso | Base (µs) | Correções (permil) | Cálculo | Esperado (µs) |
|---|---|---|---|---|
| Caso de mesa, sem correção | 4550 | nenhuma | 4550 + 700 | 5250 |
| Com uma correção | 4550 | 1200 | 4550 × 1,2 = 5460, + 700 | 6160 |
| Com duas correções | 4550 | 1200 e 970 | 5460 × 0,97 = 5296,2 → 5296, + 700 | 5996 |
| Bateria em 13 V | 4550 | nenhuma | dead time 800, 4550 + 800 | 5350 |
| Corte de combustível | 4550 | 0 | resultado 0, sem dead time | 0 |
| Teto do ciclo em 5000 | 4550 | nenhuma | 5250 limitado a 5000 | 5000 |

<small>Fonte: Autoria própria (`src/firmware/test/test_fuel.c`). O mapa base é o Quadro 6 da página de Teoria do Motor, e o dead time é um valor de teste.</small>

</div>

&emsp;Os valores esperados foram calculados à mão, e o caso de saturação foi conferido com um script Python independente do C. Para provar que o teste reprova quando a conta muda, o código foi alterado de propósito de quinze formas (por exemplo, tirar o arredondamento, não somar o dead time, somá-lo no corte, remover o teto ou usar 32 bits), e o teste reprovou em todas.

## Limitações atuais

- **Não foi executado em um ESP32.** O critério de aceite da task pede que o ESP imprima o mesmo número no Serial. Isso depende de um sketch no ESP e da placa, e fica para quando o firmware do motor existir. Até lá, o número de referência é o dos testes do computador.
- **O dead time é de teste.** Os valores reais vêm da folha de dados do injetor, que só será conhecido quando o hardware definir qual é.
- **As correções ainda não existem.** Esta task recebe os fatores prontos. O cálculo de cada um (temperatura do motor, do ar, aceleração) vem nas tasks de calibração.

## Referências

- SPEEDUINO. _Injector Characteristics_ (Speeduino Manual). Disponível em: [https://wiki.speeduino.com/en/configuration/Injector_Characteristics](https://wiki.speeduino.com/en/configuration/Injector_Characteristics). Acesso em: 10 jun. 2026.
- HEYWOOD, John B. _Internal Combustion Engine Fundamentals._ 2. ed. New York: McGraw-Hill, 2018.
- ISO/IEC 9899:1999. _Programming languages — C._ Padrão C99, usado para manter o núcleo portável entre o ESP32 e o computador.
- Páginas deste projeto: [Teoria do Motor](/teoria-do-motor) (mapa, dead time e correções) e [Arquitetura](/arquitetura) (etapas do núcleo de controle).

</div>
