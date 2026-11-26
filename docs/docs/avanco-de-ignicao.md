---
title: Avanço de Ignição
slug: /avanco-de-ignicao
sidebar_position: 7
---

# Avanço de Ignição

<div style={{textAlign: 'justify'}}>

&emsp;A faísca não salta no PMS (ponto morto superior): a mistura leva um tempo para queimar, então a ECU a dispara **antes** do pistão chegar ao topo. Quantos graus antes é o **avanço de ignição**, que vem do mapa de ignição, uma tabela de rotação (RPM) por carga. Esta página descreve a implementação da task 1.5.1.7: ler o avanço no mapa e transformá-lo no **instante do disparo**, em microssegundos a partir de um evento de referência do virabrequim. Ela usa a [interpolação bilinear](/interpolacao-bilinear) para ler o mapa, e a teoria está em [Teoria do Motor](/teoria-do-motor).

&emsp;Esta task **não** faz correção por sonda lambda: o avanço é só o do mapa.

## Onde está o código

&emsp;Como no restante do núcleo, é C99 puro, sem `Arduino.h`, e compila para o ESP32 e para os testes do computador. Fica em `src/firmware/core/`:

- `ecu_ignition.h` e `ecu_ignition.c`: a conversão de ângulo em tempo e o cálculo do disparo
- `src/firmware/test/test_ignition.c`: os testes

## A conta

&emsp;O padrão de dentes da roda fônica ainda não está decidido, então o cálculo é **relativo a um evento de referência** qualquer (por exemplo, um dente). O chamador informa quantos graus esse evento vem antes do PMS do cilindro.

```text
           referência                       faísca              PMS
               |<--------------- ref_btdc ------------------------>|
               |<-- ref_btdc - avanço -->|<------ avanço -------->|

avanço usado   = min(avanço do mapa, teto de segurança)
atraso (µs)    = (ref_btdc - avanço) × 60.000.000 / (360 × RPM)
início do dwell = atraso da faísca - dwell
```

&emsp;O _dwell_ é o tempo que a bobina precisa para carregar antes da faísca. O resultado são dois instantes, o início da carga da bobina e o disparo, ambos contados a partir da referência.

## Unidades

<div align="center">

<small><strong>Quadro 1 - Unidades usadas no cálculo de ignição</strong></small>

| Grandeza | Unidade | Exemplo |
|---|---|---|
| Ângulo (avanço, referência) | décimo de grau (x10) | 25,5 graus = 255 |
| Tempo (atraso, dwell) | microssegundo (µs) | 2 ms = 2000 |
| Rotação | RPM | 3000 |

<small>Fonte: Autoria própria (`src/firmware/core/ecu_ignition.h`).</small>

</div>

## Decisões de implementação

- **Teto de segurança no avanço.** Uma célula errada no mapa (um zero a mais, 250,0 graus no lugar de 25,0) não pode adiantar a faísca além do teto e causar detonação. O avanço realmente usado é devolvido junto com o resultado.
- **Quatro resultados em vez de só verdadeiro ou falso.** `OK` quando tudo é válido. `SHORT_DWELL` quando há menos tempo que o dwell entre a referência e a faísca: a faísca está certa, mas a bobina carregaria menos que o pedido, e quem chama decide se aceita. `TOO_EARLY` quando o avanço passa da referência: a faísca teria que sair antes dela, e é preciso usar uma referência anterior. `BAD_INPUT` para ponteiro nulo ou RPM zero. Nos dois últimos nada é escrito na saída.
- **Uma única conversão.** O ângulo entre a referência e a faísca é convertido em tempo uma vez só, e o início do dwell é uma subtração exata, sem segundo arredondamento.
- **Mesmas unidades do tempo de bico**, inteiros, sem `float`.
- **Sem avanço negativo.** O avanço é sem sinal: não há faísca depois do PMS nesta versão.
- **A ordem de disparo e as faíscas perdidas ficam com o chamador.** Esta função calcula um disparo a partir de uma referência. Quem a chama escolhe a referência de cada cilindro.

## Como usar

```c
const ecu_table_t *ign_map = /* mapa de ignição, décimos de grau */;

uint16_t adv;
if (ecu_table_lookup(ign_map, rpm, map_kpa, &adv)) {
    const ecu_spark_in_t in = {
        .rpm = rpm, .advance_x10 = adv, .max_advance_x10 = 450,
        .ref_btdc_x10 = 600, .dwell_us = 2500,
    };
    ecu_spark_out_t out;
    if (ecu_spark_schedule(&in, &out) == ECU_SPARK_OK) {
        /* agenda: começa a carregar em out.dwell_start_us e dispara em out.spark_us */
    }
}
```

## Como rodar os testes

```bash
make -C src/firmware/test
```

&emsp;Os testes usam um mapa de ignição **de teste** (não é o de nenhum motor). Os valores esperados foram calculados à mão, e os que não são redondos foram conferidos com um script Python independente do C.

<div align="center">

<small><strong>Quadro 2 - Casos de teste conferidos à mão</strong></small>

| Caso | Entrada | Cálculo | Esperado |
|---|---|---|---|
| Célula conhecida do mapa | 3000 RPM, 60 kPa | célula exata | avanço 24,0 graus |
| Entre quatro células | 2500 RPM, 75 kPa | (180 + 150 + 240 + 200) / 4 = 192,5 | avanço 19,3 graus |
| Disparo a 3000 RPM | avanço 24,0, referência 60,0 graus, dwell 1500 µs | 36,0 graus = 2000 µs | faísca em 2000 µs, dwell em 500 µs |
| Disparo a 1000 RPM | avanço 10,0, referência 90,0 graus, dwell 3000 µs | 80,0 graus = 13333,3 µs | faísca em 13333 µs, dwell em 10333 µs |
| Teto de segurança | avanço 250,0 do mapa, teto 45,0 | usa 45,0 graus | avanço usado 45,0 |
| Pouco tempo para o dwell | 6000 RPM, 30,0 graus, dwell 2500 µs | 833 µs de faísca, menos que o dwell | `SHORT_DWELL`, dwell começa em 0 |
| Avanço passa da referência | avanço 44,0, referência 40,0 graus | faísca antes da referência | `TOO_EARLY` |

<small>Fonte: Autoria própria (`src/firmware/test/test_ignition.c`).</small>

</div>

&emsp;Para provar que o teste reprova quando a conta muda, o código foi alterado de propósito de doze formas (por exemplo, trocar a constante de conversão, ignorar o teto de segurança, somar o avanço em vez de subtraí-lo ou tratar o dwell exato como curto), e o teste reprovou em todas.

## Limitações atuais

- **Não foi executado em um ESP32.** O critério de aceite da task pede que o pulso de ignição saia, na bancada, no ângulo calculado dentro de uma tolerância. Isso depende da placa e de um sinal de giro, e fica para quando o firmware do motor existir.
- **O mapa de teste não é de motor nenhum.** O mapa real vem da calibração (feature 1.6).
- **A roda fônica ainda não está definida**, por isso a referência é um parâmetro.
- **Sem correções** por temperatura, detonação ou lambda.

## Referências

- HEYWOOD, John B. _Internal Combustion Engine Fundamentals._ 2. ed. New York: McGraw-Hill, 2018.
- BOSCH. _Automotive Handbook._ 10. ed. Robert Bosch GmbH, 2018.
- ISO/IEC 9899:1999. _Programming languages — C._ Padrão C99, usado para manter o núcleo portável entre o ESP32 e o computador.
- Páginas deste projeto: [Teoria do Motor](/teoria-do-motor) (ignição e dwell) e [Arquitetura](/arquitetura) (etapa 5 do núcleo de controle).

</div>
