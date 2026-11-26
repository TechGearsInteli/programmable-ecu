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
│   │   ├── core/              # Lógica em C puro, sem Arduino, testável no PC
│   │   └── test/              # Testes que rodam no computador (make)
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
        ├── test-firmware.yml  # Testes do firmware no computador (make)
        └── pr-description.yml # Preenchimento automático de descrição de PR
```

## Pré-requisitos

| Ferramenta | Uso | Instalação |
|-----------|-----|-----------|
| [Node.js 18+](https://nodejs.org/) | Rodar o site de documentação localmente | nodejs.org |
| [Git](https://git-scm.com/) | Controle de versão | git-scm.com |
| `gcc` e `make` | Rodar os testes do firmware no computador | Linux/macOS: já vêm ou via gerenciador de pacotes. Windows: `winget install BrechtSanders.WinLibs.POSIX.UCRT` |

## Rodando a documentação localmente

```bash
cd docs
npm install
npm run start
```

O site abre em `http://localhost:3000/programmable-ecu/`.

## Rodando os testes do firmware

```bash
make -C src/firmware/test
```

No Windows, o executável do `make` instalado pelo WinLibs se chama `mingw32-make`.

## Pipelines de CI

| Workflow | Gatilho | O que faz |
|---------|---------|-----------|
| `deploy-docs.yml` | Push em `main` com mudanças em `docs/` | Build e deploy da documentação no GitHub Pages |
| `lint-docs.yml` | Pull Request com mudanças em `docs/docs/` | Valida o padrão de documentação via `check-docs.js` |
| `test-firmware.yml` | Pull Request ou push em `main` com mudanças em `src/firmware/` | Compila e roda os testes do firmware no computador |
| `pr-description.yml` | Abertura de PR | Preenche automaticamente a descrição com tasks em andamento e commits da branch |

## Licença

Distribuído sob a licença [MIT](LICENSE).

Copyright © 2026 TechGears Inteli.
