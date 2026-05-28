# Programmable ECU

ECU (Electronic Control Unit) programável de código aberto para motores de combustão interna de 4 cilindros, desenvolvida pela [TechGears](https://techgears.app), clube universitário do [Inteli](https://www.inteli.edu.br).

> Documentação completa em **[docs.techgears.app/programmable-ecu](https://docs.techgears.app/programmable-ecu/)**

---

## Visão Geral

A Programmable ECU lê sensores do motor (CKP, MAP, TPS, CLT, IAT e sonda lambda), calcula em tempo real o tempo de abertura dos injetores (Pulse Width) a partir de um mapa de combustível 3D com interpolação bilinear, aciona os 4 injetores em sequência e as bobinas de ignição, e expõe uma interface web via Wi-Fi para que o calibrador ajuste os mapas e acompanhe a telemetria em tempo real sem instalar nenhum software.

**Microcontrolador:** ESP32 (ESP-IDF, não Arduino)  
**Firmware:** C, arquitetura em camadas (HAL → Sensores → Controle → Comunicação)  
**Interface de calibração:** React + WebSocket, servida pelo próprio ESP32

---

## Estrutura do repositório

```
programmable-ecu/
├── firmware/                  # Código do microcontrolador
│   ├── hal/                   # Hardware Abstraction Layer (GPIO, ADC, Timers, Wi-Fi)
│   ├── sensors/               # Leitura e conversão de sensores
│   ├── control/               # Cálculo de injeção, ignição e controle lambda
│   ├── comms/                 # Servidor WebSocket e persistência NVS
│   ├── web/                   # Assets da interface React (servidos via SPIFFS)
│   └── main.c                 # Entry point e tasks FreeRTOS
├── hardware/                  # Esquemáticos, diagramas de blocos e BOM
├── calibration/               # Mapas de calibração (.json)
├── docs-site/                 # Site de documentação (Docusaurus)
├── scripts/
│   └── check-docs.js          # Validador de padrão de documentação
├── docs-meta.json             # Metadados para o portal docs.techgears.app
└── .github/
    └── workflows/
        ├── deploy-docs.yml    # Build e deploy do Docusaurus no GitHub Pages
        ├── lint-docs.yml      # Verificação de padrão de documentação em PRs
        └── pr-description.yml # Preenchimento automático de descrição de PR
```

---

## Pré-requisitos

| Ferramenta | Uso | Instalação |
|-----------|-----|-----------|
| [PlatformIO](https://platformio.org/) | Compilar e flashar o firmware no ESP32 | Via VS Code extension ou `pip install platformio` |
| [Node.js 18+](https://nodejs.org/) | Rodar o site de documentação localmente | nodejs.org |
| [Git](https://git-scm.com/) | Controle de versão | git-scm.com |

---

## Rodando a documentação localmente

```bash
cd docs-site
npm install
npm run start
```

O site abre em `http://localhost:3000/programmable-ecu/`.

---

## Como contribuir

### 1. Escolha uma task

Acesse o [GitHub Projects](https://github.com/orgs/TechGearsInteli/projects/1), escolha uma task no **Sprint Backlog**, se auto-atribua e mova para **Fazendo**.

### 2. Crie uma branch

```bash
git checkout -b feat/1.X.X.X-descricao-curta
# ou
git checkout -b fix/descricao-do-bug
```

### 3. Implemente e documente

- Siga o padrão de código do firmware descrito em `docs-site/docs/`
- Se criar ou editar arquivos `.md` em `docs-site/docs/`, siga o [Guia de Documentação](https://docs.techgears.app/guia)
- O lint de documentação roda automaticamente em todo PR

### 4. Abra um Pull Request

Use o template de PR disponível em `.github/pull_request_template.md`. Descreva o que foi feito, por que foi necessário e como testar.

### Padrão de branches

| Prefixo | Uso |
|---------|-----|
| `feat/` | Nova funcionalidade ou entrega de task |
| `fix/` | Correção de bug |
| `docs/` | Apenas documentação |
| `refactor/` | Refatoração sem mudança de comportamento |
| `ci/` | Mudanças em workflows e scripts de CI |

---

## Pipelines de CI

| Workflow | Gatilho | O que faz |
|---------|---------|-----------|
| `deploy-docs.yml` | Push em `main` com mudanças em `docs-site/` | Build do Docusaurus e deploy no GitHub Pages |
| `lint-docs.yml` | Pull Request com mudanças em `docs-site/docs/` | Valida o padrão de documentação via `check-docs.js` |
| `pr-description.yml` | Abertura de PR | Preenche automaticamente a descrição com tasks em andamento e commits da branch |

---

## Decisões de arquitetura

As decisões técnicas relevantes estão documentadas como ADRs (Architecture Decision Records) em `docs-site/docs/`:

- **ADR-001:** Escolha do microcontrolador (ESP32 vs STM32)
- **ADR-002:** Arquitetura do firmware em camadas com HAL
- **ADR-003:** Interface de calibração web (React + WebSocket)
- **ADR-004:** Protocolo WebSocket com JSON tipado

---

## Licença

Distribuído sob a licença [MIT](LICENSE).

Copyright © 2026 TechGears Inteli.
