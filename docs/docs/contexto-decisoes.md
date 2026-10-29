---
title: Contexto das decisões
slug: /contexto-decisoes
sidebar_position: 0
---

# Contexto das decisões

<div style={{textAlign: 'justify'}}>

Este texto é da equipe. Existe para ser lido e compartilhado no repositório, não para ficar na conversa de uma pessoa só. As decisões daqui foram tomadas pelo Eduardo em **29 de outubro de 2026**, depois do mapeamento, para o time partir do mesmo lugar. Não é especificação fechada e não é contrato.

O que está aqui é o que o Eduardo fechou **agora**, com o que já sabemos. Peça, firmware, carro, ordem das tasks e até uma feature inteira podem mudar quando o projeto andar: mesa, pinagem do Fit, laboratório, dinheiro ou uma conversa da equipe. Decisão nova entra neste arquivo **com a equipe ciente**: alguém combina com o time, atualiza este texto e ajusta as issues que dependem dele. Não se trata uma frase daqui como lei só porque está escrita, e não se muda o recorte no código ou no carro sem essa atualização.

As issues no GitHub (`TechGearsInteli/programmable-ecu`, features 1.4 a 1.10) foram reescritas para bater com este recorte. O quadro DevGears pode ainda não mostrar as issues novas (#179 a #191): elas existem no repositório.

## Em uma frase

Tiramos a central Honda do Fit da liga, encaixamos a nossa no mesmo plugue, e só fazemos isso depois que o programa já funcionar na mesa.

## O que a equipe já pode tratar como combinado

Estas são decisões de trabalho, não verdades eternas.

**Duas placas.** Um ESP32 sem tela é o computador do motor: lê sensores e, na hora, manda injeção, ignição e relé. Um ESP32-S3 com tela de cerca de 4,3 polegadas, touch capacitivo, é só a interface. Os dois se falam por UART. Fio de sensor, bico ou bobina não vai para o S3. Nextion não entra. STM32 no motor só entra se o teste de capacidade do ESP reprovar.

**Firmware nosso.** O programa é da TechGears. Speeduino e FuelTech são referência de elétrica e de produto (tela na ECU, mapa em tabela, relé de bomba). Não copiamos o firmware deles. Se comprarmos uma placa estilo Speeduino, ela é estágio de potência, não o cérebro.

**Carro.** A integração no veículo é o Fit da liga. O Gol G3 AP 1.6 (IAW 1AVP) fica como referência de motor que já conhecemos. O chicote do Gol não se corta. O Gol não é o carro em que vamos plugar.

**Como liga no Fit.** A Honda sai do conector. A nossa caixa entra nesse mesmo conector, com um chicote nosso, mesmo improvisado. Não cortamos o chicote do carro até cada sensor. O lado que precisamos comprar ou tirar de sucata é o conector **colado na caixinha Honda**, não outro plugue igual ao do chicote. Fêmea e macho no anúncio da loja erram; a foto da central da liga é que decide.

**Sem fase de só escutar.** Não vamos passar uma etapa obrigatória só lendo os fios com a original no comando. Mesa primeiro. No carro, a nossa ECU assume.

**Mesa antes do carro.** “Firmware bom” significa: na bancada o ESP acompanha um gerador de roda fônica sem perder dente, o cálculo de tempo de bico e de avanço faz sentido, e uma lâmpada no lugar do bico pisca no tempo certo. Sem isso o Fit não entra.

**Potência.** O ESP manda 3,3 V. Bico e faísca passam por driver (MOSFET e estágio de ignição). Relé só para o que liga e fica ligado, em especial a bomba e a ventoinha. Relé não pulsa injetor nem vela. PCB própria e esquemático completo da ECU no EasyEDA não são esta etapa: o estágio de potência se compra.

**Cuidado com peça cara.** O S3 e o MOSFET queimam com 12 V no pino errado, polaridade invertida, hot-plug e gate sem proteção. A primeira energização tem fusível, ordem escrita e lâmpada antes de bico de verdade. O desenho da tela passa por simulador LVGL no computador antes de gravar no S3. Simulador não substitui o teste de giro no ESP.

**Mapa e tela, no modelo FuelTech.** O mapa é tabela no ESP do motor: rotação por carga, mais correções (aquecimento, ar, partida, bateria, aceleração). A S3 mostra e, quando a edição existir, manda o número de volta. A tela não calcula o bico. Não há site nem API no chip do motor. Programa de PC no estilo FTManager pode existir depois, se o time quiser.

**O que a original fazia além do motor.** Bomba por relé nosso. Conta-giros do painel só se for fio; se o Fit só manda RPM por CAN, o ponteiro pode zerar e a S3 vira o instrumento. Não emulamos a rede Honda nem o imobilizador. Se a chave cortar a partida, o mapa não resolve: é o mesmo limite da FuelTech e do Speeduino. Luz, vidro e o resto dos módulos do carro continuam nos módulos deles. Câmbio automático, se o Fit não for manual, é um risco à parte e ainda não está medido.

**Voltar atrás.** Não é automático. Chave desligada, nossa caixa sai, Honda volta no mesmo plugue. Isso só existe se a gente não destruir o conector do carro. Cortar o chicote do Fit quebra essa volta.

**Demo visual.** A demo com botões simulados não faz parte do produto.

## O que este recorte deixa de fora, por enquanto

Dá para trazer de volta numa conversa. Hoje não está no caminho:

- controlar o motor por OBD, K-Line ou CAN de diagnóstico;
- uma etapa só de escuta no carro;
- cortar o chicote do Fit ou do Gol;
- uma placa só fazendo motor, tela e Wi-Fi;
- lambda em malha fechada como forma de “aprender o mapa sozinho”;
- PCB fabricada no laboratório como primeira entrega de hardware;
- turbo, nitro, controle de tração, datalog de centenas de canais, tunagem remota.

## O que ainda não está decidido porque falta dado

Não é indecisão de arquitetura. É medida que ainda não temos:

- ano, motor e câmbio do Fit no documento (o “1.6” informado para Fit no Brasil não fecha; o mercado é 1.4 ou 1.5);
- foto do conector colado na ECU Honda e a pinagem;
- se o sensor de giro é indutivo ou Hall, e o desenho da roda fônica;
- se o imobilizador impede a partida sem a Honda;
- o que o laboratório do Inteli já tem (fonte, fusível, analisador, osciloscópio);
- modelo exato da tela S3 e da placa de potência, com preço, na hora da compra;
- autorização da liga para tirar a central do Fit.

Enquanto isso, o firmware de mesa não espera o plugue.

## Como o trabalho está fatiado

A ordem prática é mesa, proteção, tela, Fit, texto. Os números são os das issues, para achar no GitHub. O detalhe de cada card pode ser reescrito sem mudar este recorte, e o recorte pode ser reescrito sem fingir que o card antigo continua valendo.

1. **1.4 Hardware.** Conector do lado da caixinha, condicionamento 12 V para 3,3 V, estágio de potência comprado, relé e lâmpada, adaptador, procedimento de ligar, simulação do MOSFET, proteção do S3.
2. **1.5 Firmware do motor.** Timers, giro na mesa, pots, mapas, tempo de bico, quatro saídas de bancada, avanço, UART, teste de não perder dente, tabelas na RAM, testes de conta no computador.
3. **1.7 e 1.6 Interface.** Primeiro o painel no simulador e na S3, alertas e log. Depois edição de célula, grades de combustível e ignição, correções, ficha do motor, gravação na flash.
4. **1.8 Segurança.** Falha de sensor, mapa de emergência, watchdog, corte da bomba sem sincronismo, e o ensaio de devolver a Honda.
5. **1.9 No Fit.** Plano, lâmpada na mesa, pátio, rua. Rua só depois do pátio.
6. **1.10 Documentação.** Alinhar o site a este recorte, inclusive tirar OBD como caminho de controle.

A feature 1.3 (estudo de hardware) não foi reaberta.

## Como mudar

Se um ensaio, uma pinagem ou uma conversa mostrar que um item daqui está errado, a equipe combina o novo recorte, atualiza este arquivo e ajusta as issues que dependem dele. A atualização fica no repositório, visível para quem for ler depois. Mudança calada no código ou no carro, sem o time ciente e sem este texto atualizado, não vale.

</div>