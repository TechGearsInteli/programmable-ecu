---
title: Interpolação Bilinear dos Mapas
slug: /interpolacao-bilinear
sidebar_position: 5
---

# Interpolação Bilinear dos Mapas

<div style={{textAlign: 'justify'}}>

&emsp;Os mapas de combustível e de ignição da ECU são tabelas de **rotação (RPM) por carga**. Uma tabela tem um número pequeno de pontos, enquanto o motor passa por infinitos pontos de operação. Quando a rotação ou a carga medidas caem entre dois pontos da grade, a ECU precisa de um valor intermediário. A **interpolação bilinear** calcula esse valor a partir das quatro células que cercam o ponto consultado. Esta página descreve a implementação da task 1.5.1.4, que serve de base para o cálculo do tempo de bico (1.5.1.5) e do avanço de ignição (1.5.1.7).

## Onde está o código

&emsp;O código fica em `src/firmware/core/` e é C99 puro, sem `Arduino.h` e sem ESP-IDF. Por isso o mesmo arquivo compila para o ESP32 do motor e para os testes que rodam no computador, sem placa.

- `ecu_table.h` e `ecu_table.c`: a tabela e a consulta
- `src/firmware/test/test_table.c`: os testes de aceite
- `src/firmware/test/Makefile`: compila e roda os testes

## Como a conta funciona

&emsp;Seja o ponto consultado `(x, y)` = `(rpm, carga)`. Ele fica dentro de uma célula da grade, cujos cantos têm os valores `v00`, `v10`, `v01` e `v11` (o primeiro índice é o RPM, o segundo é a carga). Chamando de `x0` e `x1` os RPM dos cantos, e de `y0` e `y1` as cargas:

```text
dx  = x1 - x0            dy  = y1 - y0
wx1 = x  - x0            wx0 = dx - wx1
wy1 = y  - y0            wy0 = dy - wy1

valor = ( v00*wx0*wy0 + v10*wx1*wy0 + v01*wx0*wy1 + v11*wx1*wy1 ) / (dx*dy)
```

&emsp;O peso de cada canto é a área do retângulo oposto a ele. Quanto mais perto o ponto está de um canto, mais esse canto pesa. Sobre um nó da grade, três dos quatro pesos valem zero e o resultado é exatamente a célula, o que garante o primeiro critério de aceite.

## Decisões de implementação

- **Só inteiros.** A conta usa `uint16_t` e `uint64_t`, sem `float`. O resultado é o mesmo em qualquer máquina e não depende da unidade de ponto flutuante do chip.
- **Um único arredondamento.** Em vez de interpolar duas vezes em sequência (e arredondar duas vezes), a soma ponderada é dividida uma só vez no final, para o inteiro mais próximo, com o meio arredondando para cima.
- **64 bits.** O maior numerador possível é `65535 × 65535 × 65535`, que estoura 32 bits. Com `uint64_t` a conta é exata para qualquer valor de 16 bits.
- **Fora da faixa, vale a borda.** Se o RPM ou a carga passam do último ponto do eixo (ou ficam abaixo do primeiro), a consulta não extrapola. Extrapolar um mapa de combustível poderia pedir um tempo de bico que ninguém calibrou.
- **Tabela inválida não é consultada.** Os eixos precisam ser estritamente crescentes e ter ao menos 2 pontos. Se não forem, `ecu_table_lookup` devolve `false` e não escreve na saída, e quem chama decide o que fazer (por exemplo, entrar no mapa de emergência da feature 1.8).
- **Eixos não uniformes.** Os pontos dos eixos podem ter qualquer espaçamento, porque um mapa de ECU costuma ter mais resolução em baixa carga.

## Como usar

```c
static const uint16_t rpm_axis[]  = {1000, 2000, 4000};
static const uint16_t load_axis[] = {30, 60, 100};
static uint16_t cells[3 * 3] = {
    100, 200, 300,   /* rpm = 1000 */
    150, 250, 350,   /* rpm = 2000 */
    300, 400, 500,   /* rpm = 4000 */
};
static const ecu_table_t fuel = {3, 3, rpm_axis, load_axis, cells};

uint16_t pw;
if (ecu_table_lookup(&fuel, 2500, 70, &pw)) {
    /* pw == 313 */
}
```

&emsp;Para a HMI (task 1.5.1.10), `ecu_table_locate` devolve qual célula contém o ponto atual.

## Como rodar os testes

&emsp;Os testes precisam apenas de um compilador C (`gcc`) e do `make`. Eles não usam a placa.

```bash
make -C src/firmware/test
```

&emsp;A saída esperada é `ok: N verificacoes passaram`. Se qualquer verificação falhar, o programa imprime qual foi e sai com código diferente de zero, o que faz o CI reprovar o PR.

<div align="center">

<small><strong>Quadro 1 - Casos de teste conferidos à mão</strong></small>

| Caso | Entrada (RPM, carga) | Esperado | Cálculo |
|---|---|---|---|
| Canto inferior | (1000, 30) | 100 | célula exata |
| Canto superior | (4000, 100) | 500 | célula exata |
| Centro de uma célula | (3000, 80) | 375 | (250 + 400 + 350 + 500) / 4 |
| Fora do centro | (2500, 70) | 313 | 25.000.000 / 80.000 = 312,5, arredonda para cima |
| Sobre uma aresta | (1500, 30) | 125 | meio entre 100 e 150 |
| Acima da faixa | (65535, 65535) | 500 | usa a borda, sem extrapolar |

<small>Fonte: Autoria própria (`src/firmware/test/test_table.c`), usando a grade 3 × 3 do exemplo acima.</small>

</div>

&emsp;Os valores esperados foram calculados à mão antes da implementação, e não gerados pelo próprio código. Para conferir que o teste realmente falha quando a conta muda, o código foi alterado de propósito de onze formas (por exemplo, trocar o peso de um canto, tirar o arredondamento ou usar 32 bits no lugar de 64) e o teste reprovou em todas.

## Limitações atuais

&emsp;Esta task entrega a conta e os testes no computador. A consulta ainda **não foi executada em um ESP32**. O código evita o que costuma diferir entre plataformas (`float`, tipos de tamanho variável), mas a conferência na placa e o custo em tempo da divisão de 64 bits ficam para as tasks seguintes, quando a consulta for chamada a partir do firmware do motor.

## Referências

- PRESS, William H. et al. _Numerical Recipes: The Art of Scientific Computing._ 3. ed. Cambridge: Cambridge University Press, 2007. Seção 3.6, Interpolation in Two or More Dimensions.
- WIKIPEDIA. _Bilinear interpolation._ Disponível em: [https://en.wikipedia.org/wiki/Bilinear_interpolation](https://en.wikipedia.org/wiki/Bilinear_interpolation). Acesso em: 26 nov. 2026.
- ISO/IEC 9899:1999. _Programming languages — C._ Padrão C99, usado para manter o núcleo portável entre o ESP32 e o computador.

</div>
