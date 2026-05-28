# Programmable ECU — TechGears Inteli

ECU (Electronic Control Unit) programável para motores de combustão interna de 4 cilindros. O projeto controla injeção de combustível e ignição via microcontrolador, com interface de calibração web e telemetria em tempo real.

## O que é este projeto?

Uma ECU do zero: lemos sensores (CKP, MAP, TPS, CLT, IAT, lambda), calculamos o tempo de injeção via mapa de combustível 3D com interpolação bilinear, acionamos os injetores e bobinas em sequência, e expõe uma interface web para calibração em tempo real.

## Estrutura do repositório

```
programmable-ecu/
├── firmware/      # Código do microcontrolador (C/C++ com PlatformIO)
├── hardware/      # Esquemáticos, PCB, diagramas de blocos
├── calibration/   # Mapas de calibração (.json / .msq)
└── docs-site/     # Site de documentação (Docusaurus) → docs.techgears.app/programmable-ecu/
```

## Como participar

1. Acesse a documentação em https://docs.techgears.app/programmable-ecu
2. Veja as issues abertas no [GitHub Projects — DevGears](https://github.com/orgs/TechGearsInteli/projects/1)
3. Escolha uma task no Sprint Backlog, se auto-atribua e mova para "Fazendo"
4. Crie uma branch com o padrão `feat/1.X.X.X-descricao` ou `fix/descricao`
5. Abra um Pull Request para `main` descrevendo o que foi feito e por quê

## Requisitos de ambiente

- [PlatformIO](https://platformio.org/) para compilar e flashar o firmware
- [Node.js 18+](https://nodejs.org/) para rodar o site de documentação localmente (`cd docs-site && npm start`)
- Microcontrolador: ESP32

## Licença

MIT
