---
title: Visão Geral
slug: /
sidebar_position: 1
---

# Programmable ECU

<div style={{textAlign: 'justify'}}>

&emsp;A **Programmable ECU** é uma ECU (Electronic Control Unit) de código aberto desenvolvida pelo [TechGears](https://techgears.app), clube universitário do Inteli. O projeto tem como objetivo construir do zero um sistema de gerenciamento eletrônico para motores de combustão interna de 4 cilindros, desde o hardware até a interface de calibração.

&emsp;A ECU lê sensores do motor (rotação, pressão, temperatura, posição da borboleta e mistura ar-combustível), calcula em tempo real o tempo de injeção e o momento de ignição, e expõe uma interface web via Wi-Fi para que o calibrador ajuste os mapas e acompanhe a telemetria sem precisar instalar nenhum software.

## O que você vai encontrar aqui

&emsp;Esta documentação cobre todo o projeto, do estudo inicial à validação em veículo real:

- **Teoria do Motor**: fundamentos de motores de combustão, sensores e controle de mistura ar-combustível
- **Hardware**: diagrama de blocos, lista de componentes e guia de instalação no veículo
- **Firmware**: arquitetura em camadas, leitura de sensores, cálculo de injeção e controle de ignição
- **Calibração**: passo a passo para partir o motor e ajustar o mapa de combustível
- **Decisões de Projeto**: ADRs documentando as escolhas técnicas e o escopo da V1
- **Guia de Uso**: como contribuir com a documentação, rodar localmente e entender o pipeline de CI

## Repositório

&emsp;O código-fonte, esquemáticos e mapas de calibração estão em [github.com/TechGearsInteli/programmable-ecu](https://github.com/TechGearsInteli/programmable-ecu).

</div>
