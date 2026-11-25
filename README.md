# Programmable ECU

ECU (Electronic Control Unit) programável de código aberto para motores de combustão interna de 4 cilindros, desenvolvida pela [TechGears](https://techgears.app), clube universitário do [Inteli](https://www.inteli.edu.br).

> Documentação completa em **[docs.techgears.app/programmable-ecu](https://docs.techgears.app/programmable-ecu/)**

## Visão Geral

A Programmable ECU lê sensores do motor, calcula o tempo de injeção de combustível e o momento de ignição em tempo real, e expõe uma interface web via Wi-Fi para calibração e telemetria.

## Estrutura do repositório

```
programmable-ecu/
├── src/
│   ├── firmware/              # Código do microcontrolador
│   ├── hardware/              # Esquemáticos e diagramas
│   ├── calibration/           # Mapas de calibração
│   └── software/              # Interface de calibração
├── docs/                      # Site de documentação (Docusaurus)
├── lint/
│   └── check-docs.js          # Validador de padrão de documentação
├── docs-meta.json             # Metadados para o portal docs.techgears.app
└── .github/
    └── workflows/
        ├── deploy-docs.yml    # Build e deploy da documentação no GitHub Pages
        ├── lint-docs.yml      # Verificação de padrão de documentação em PRs
        └── pr-description.yml # Preenchimento automático de descrição de PR
```

## Pré-requisitos

| Ferramenta | Uso | Instalação |
|-----------|-----|-----------|
| [Node.js 18+](https://nodejs.org/) | Rodar o site de documentação localmente | nodejs.org |
| [Git](https://git-scm.com/) | Controle de versão | git-scm.com |

## Rodando a documentação localmente

```bash
cd docs
npm install
npm run start
```

O site abre em `http://localhost:3000/programmable-ecu/`.

## Firmware, tela e simulador (features 1.6 e 1.7)

| Pasta | O que tem |
|-------|-----------|
| `src/firmware/common/` | Núcleo em C (protocolo UART, mapas, Pulse Width, correções, ficha, gravação) e testes Unity |
| `src/firmware/hmi/ui/` | Telas LVGL 9.2.2 (800x480), as mesmas no simulador e na S3 |
| `src/firmware/hmi/s3/` | Firmware da tela ESP32-S3 (PlatformIO) |
| `src/firmware/motor-bench/` | Firmware de bancada do ESP do motor para testar a tela (PlatformIO) |
| `src/software/hmi-sim/` | Simulador da tela no PC com motor virtual |

Testes e simulador no computador (precisa de Python 3):

```bash
python -m pip install --user cmake ninja ziglang
python tools/host_build.py          # compila e roda todos os testes
python tools/host_build.py --sim    # abre o simulador (Windows)
python tools/host_build.py --docker # tudo num container Linux
```

Firmwares: `pio run` dentro de `src/firmware/hmi/s3` ou `src/firmware/motor-bench` ([PlatformIO](https://platformio.org/)).
Detalhes em `docs/docs/protocolo-uart.md`, `docs/docs/nucleo-calibracao.md` e `docs/docs/hmi-s3.md`.

## Pipelines de CI

| Workflow | Gatilho | O que faz |
|---------|---------|-----------|
| `deploy-docs.yml` | Push em `main` com mudanças em `docs/` | Build e deploy da documentação no GitHub Pages |
| `lint-docs.yml` | Pull Request com mudanças em `docs/docs/` | Valida o padrão de documentação via `check-docs.js` |
| `pr-description.yml` | Abertura de PR | Preenche automaticamente a descrição com tasks em andamento e commits da branch |

## Licença

Distribuído sob a licença [MIT](LICENSE).

Copyright © 2026 TechGears Inteli.
