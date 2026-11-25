---
title: Tutorial - rodar a HMI e os testes
slug: /tutorial-hmi
sidebar_position: 8
---

<div style={{textAlign: 'justify'}}>

# Tutorial: rodar a HMI, o simulador e os testes

Projeto: Programmable ECU - TechGears Inteli  
Features: 1.6 e 1.7 (HMI na ESP32-S3), PR #193  
Mais contexto: [HMI na S3](/hmi-s3), [Protocolo UART](/protocolo-uart) e [Núcleo de cálculo e calibração](/nucleo-calibracao)

&emsp;Este tutorial mostra, passo a passo, como rodar no seu computador tudo o que foi feito para a tela da ECU. São quatro coisas: os testes do núcleo, os cenários de tela, as capturas das telas e a compilação dos firmwares. Não precisa de placa para nada disso.

## 1. O que instalar

- **Git** e **Python 3.10 ou mais novo**: [python.org](https://www.python.org/downloads/). No Windows, marque "Add Python to PATH".
- **Escolha um dos dois caminhos para compilar no PC:**
  - **Caminho A, Docker (recomendado no Windows):** instale o [Docker Desktop](https://www.docker.com/products/docker-desktop/) e deixe-o aberto ("Engine running"). Funciona mesmo com o Smart App Control do Windows ligado, que bloqueia executáveis recém-compilados.
  - **Caminho B, direto no PC:** instale CMake, Ninja e um compilador C pelo Python. Esse é o único caminho que abre a janela interativa do simulador.

    ```bash
    python -m pip install --user cmake ninja ziglang
    ```

- **PlatformIO**, só para compilar os firmwares das placas:

    ```bash
    python -m pip install --user platformio
    ```

## 2. Pegar o código

```bash
git clone https://github.com/TechGearsInteli/programmable-ecu.git
cd programmable-ecu
git switch feat/1.6-1.7-hmi-s3
```

&emsp;Depois do merge do PR, use a `main` no lugar da branch. Todos os comandos abaixo rodam **na pasta raiz do repositório**.

## 3. Rodar os testes

### Caminho A: Docker

```bash
python tools/host_build.py --docker
```

&emsp;A **primeira vez demora uns 5 minutos**: baixa a imagem Linux e o LVGL e compila tudo. Depois, a pasta `build/linux` é reaproveitada e leva segundos. No fim deve aparecer:

```text
5/5 Test #5: hmi_e2e ..........................   Passed
100% tests passed, 0 tests failed out of 5
```

### Caminho B: direto no PC

```bash
python tools/host_build.py
```

&emsp;Se não houver compilador C instalado, o script usa o Zig sozinho. O resultado esperado é o mesmo: `100% tests passed`.

## 4. Ver os cenários de tela, verificação por verificação

&emsp;Os cenários tocam nos mesmos botões que uma pessoa tocaria, sem abrir janela. A cada passo, eles conferem o que a tela mostra e o que mudou dentro do motor virtual. É o melhor comando para uma demonstração ao vivo:

```bash
python tools/host_build.py --docker --cenarios
```

&emsp;Sem Docker, use `python tools/host_build.py --cenarios`. A saída esperada é esta (trecho):

```text
== Editar uma célula com confirmação do ESP (1.6.1.3)
  [ok]     tocar na grade escolhe a célula [8,6] para editar
  [ok]     o ESP respondeu: Confirmado
  [ok]     o Pulse Width calculado subiu (6466 -> 6664 us)
== Valor absurdo é recusado e o mapa fica (1.6.1.3)
  [ok]     o ESP recusou e a tela mostra a recusa
...
54 verificações, 0 falhas
```

## 5. Gerar as imagens das telas

```bash
python tools/host_build.py --docker --capturas build/demo
```

&emsp;Abra a pasta `build/demo`: são 17 imagens PNG (800×480), uma por passo dos cenários. Para atualizar as figuras desta documentação, use `--capturas docs/docs/img/hmi`.

## 6. Abrir o simulador em janela (caminho B)

```bash
python tools/host_build.py --sim
```

&emsp;Abre uma janela de 800×480 com as telas da S3, que dá para usar com o mouse. O motor virtual repete um ciclo de 40 segundos: partida, lenta, aceleração, passagem do limite de rotação, desaceleração e cruzeiro, enquanto o motor esquenta. A "flash" do motor virtual e o backup ficam na pasta `build/host/hmi-sim-dados`.

&emsp;Se o Windows mostrar "Uma política de Controle de Aplicativo bloqueou este arquivo", o Smart App Control está ligado. Nesse caso, use os passos 3 a 5 pelo Docker.

## 7. Compilar os firmwares das placas

**Tela ESP32-S3:**

```bash
cd src/firmware/hmi/s3
python -m platformio run
```

**ESP do motor, firmware de bancada:**

```bash
cd src/firmware/motor-bench
python -m platformio run
```

&emsp;Os dois devem terminar com `[SUCCESS]`. A primeira compilação baixa o toolchain do ESP32 (cerca de 1 GB) e demora. Para gravar com a placa ligada no USB, use `python -m platformio run -t upload`. Para ver o Serial, use `python -m platformio device monitor`.

&emsp;**Antes de gravar a S3:** os pinos da placa estão em `src/firmware/hmi/s3/src/board.h`. Hoje estão configurados para a Sunton ESP32-8048S043C, que ainda não foi validada. Confira cada pino no esquemático da placa comprada.

## 8. Teste de bancada (quando houver as placas)

1. Grave o `motor-bench` num ESP32 DevKit e ligue os potenciômetros: GPIO34 RPM, GPIO35 carga, GPIO32 CLT, GPIO33 TPS (pontas em 3,3 V e GND).
2. Grave o firmware da S3 na tela.
3. Ligue só 3 fios entre as placas: TX do motor (GPIO17) no RX da S3, RX do motor (GPIO16) no TX da S3, e GND com GND. **Nunca ligue 12 V nem sinal de motor na S3.**
4. Abra o Serial do motor (`device monitor`) e compare com a tela: rotação, célula, valor e PW devem ser iguais.
5. Pot de RPM no mínimo = motor parado. Só assim o ESP aceita gravar na flash e mudar a ficha.

## 9. Problemas comuns

- **`docker` não encontrado ou "Cannot connect to the Docker daemon"**: o Docker Desktop não está aberto. Abra e espere "Engine running".
- **`Manually specified source directory is missing: FETCHCONTENT_SOURCE_DIR_...`**: o cache de build veio de outra máquina ou de outra configuração. Apague a pasta `build/linux` (ou `build/host`) e rode de novo.
- **Teste "Not Run" ou "Permission denied" no Windows**: o antivírus ou o Smart App Control segurou o executável novo. Rode de novo, ou use `--docker`.
- **`cmake` ou `ninja` não encontrado**: rode `python -m pip install --user cmake ninja ziglang`.
- **PlatformIO "pio" não reconhecido**: use `python -m platformio` no lugar de `pio`.
- **Primeira execução muito lenta**: é normal. O Docker baixa a imagem e o LVGL, e o PlatformIO baixa o toolchain. Da segunda vez em diante é rápido.

## Referências

- PYTHON SOFTWARE FOUNDATION. _Python downloads._ Disponível em: [https://www.python.org/downloads/](https://www.python.org/downloads/). Acesso em: 25 nov. 2026.
- DOCKER. _Docker Desktop._ Disponível em: [https://docs.docker.com/desktop/](https://docs.docker.com/desktop/). Acesso em: 25 nov. 2026.
- KITWARE. _CMake Documentation._ Disponível em: [https://cmake.org/cmake/help/latest/](https://cmake.org/cmake/help/latest/). Acesso em: 25 nov. 2026.
- PLATFORMIO. _PlatformIO Core (CLI)._ Disponível em: [https://docs.platformio.org/en/latest/core/index.html](https://docs.platformio.org/en/latest/core/index.html). Acesso em: 25 nov. 2026.
- LVGL. _LVGL Documentation v9.2._ Disponível em: [https://docs.lvgl.io/9.2/](https://docs.lvgl.io/9.2/). Acesso em: 25 nov. 2026.

</div>
