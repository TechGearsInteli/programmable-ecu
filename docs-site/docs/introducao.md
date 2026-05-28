---
id: introducao
title: Introdução
sidebar_position: 1
---

# Programmable ECU — TechGears Inteli

## O que é

A **Programmable ECU** é uma unidade de controle eletrônico (ECU) de **código aberto** projetada para motores de combustão interna de 4 cilindros. Ela substitui a ECU original de veículos de competição ou projetos acadêmicos, oferecendo controle total sobre o mapa de injeção de combustível, o timing de ignição e a telemetria do motor.

## Quem somos

Somos o **TechGears**, um clube universitário do [Inteli — Instituto de Tecnologia e Liderança](https://www.inteli.edu.br) focado em tecnologia automotiva. Reunimos estudantes de Engenharia de Software, Engenharia da Computação e Engenharia Mecatrônica para desenvolver projetos que unem **teoria e prática** em sistemas embarcados e veículos.

Nossa missão é _transformar conhecimento em tecnologia automotiva_ — e a ECU programável é um dos nossos projetos mais ambiciosos: construir do zero uma ECU capaz de rodar em um veículo real.

## Por que estamos fazendo

As ECUs comerciais são fechadas. Não é possível entender como o cálculo de injeção funciona, muito menos modificá-lo para aprender ou experimentar. Construir uma ECU do zero nos obriga a dominar:

- Eletrônica de potência (drivers de injetores e bobinas)
- Firmware de tempo real em microcontrolador (C + ESP-IDF)
- Teoria de motores (lambda, mapa de combustível, timing de ignição)
- Desenvolvimento web embarcado (WebSocket, React no browser)

O resultado é uma plataforma de aprendizado real — e um produto que pode ser instalado e testado em um veículo.

## Como pretendemos fazer

O projeto está dividido em **Épicos** (grandes entregas) rastreados no [GitHub Projects — DevGears](https://github.com/orgs/TechGearsInteli/projects/1). Cada membro do clube assume tasks no Sprint Backlog e abre Pull Requests com sua contribuição.

A arquitetura está descrita nas ADRs (Architecture Decision Records) na seção "Decisões de Projeto" desta documentação. O ponto de partida técnico são as decisões já tomadas:

- **Microcontrolador:** ESP32 (Wi-Fi integrado, custo acessível, ESP-IDF robusto)
- **Firmware:** arquitetura em camadas com HAL — facilita testes unitários e portabilidade
- **Interface de calibração:** Web (React + WebSocket) — sem instalação de software, funciona em qualquer browser
- **Mapa de combustível:** tabela 16×16 (RPM × MAP) com interpolação bilinear

## Estrutura do repositório

```
programmable-ecu/
├── firmware/          # Código do microcontrolador (C + ESP-IDF / PlatformIO)
├── hardware/          # Esquemáticos, BOM, diagramas de blocos
├── calibration/       # Mapas de calibração (.json)
├── docs-site/         # Este site de documentação (Docusaurus)
└── .github/workflows/ # CI/CD — build e deploy automático desta documentação
```

## Como participar

1. Acesse o [GitHub Projects](https://github.com/orgs/TechGearsInteli/projects/1) e escolha uma task no Sprint Backlog
2. Se auto-atribua e mova para "Fazendo"
3. Crie uma branch: `feat/1.X.X.X-descricao` ou `fix/descricao`
4. Implemente, documente e abra um Pull Request para `main`
5. Leia o [Guia de Documentação](/docs/guia-documentacao) antes de contribuir com docs

---

> **Dúvidas?** Abra uma [issue no GitHub](https://github.com/TechGearsInteli/programmable-ecu/issues) ou fale no canal do clube.
