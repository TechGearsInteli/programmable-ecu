# Vídeo: HMI S3 da ECU (features 1.6 e 1.7)

- **Arquivo:** `hmi-s3-explicacao.mp4`: 3min41s, 1280×720, 6 MB, narração em pt-BR.
- **Conteúdo:** explica para o time o que entrou no PR #193. São 15 cenas, alternando cartões de texto com capturas do simulador (as mesmas de `docs/docs/img/hmi/`).
- Este roteiro existe para quem não pode ver o vídeo, inclusive um agente, saber o que ele mostra.

## Cenas do vídeo atual

| # | Início | Na tela | Narração (resumo) |
|---|--------|---------|-------------------|
| 1 | 0:00 | Cartão "A tela da ECU", PR #193 · 11 tasks | Apresenta as 11 tasks das features 1.6 e 1.7 |
| 2 | 0:16 | Cartão "Duas placas: o cérebro fica no motor" | O ESP do motor lê, calcula e guarda os mapas. A S3 só mostra e pede. Três fios de UART |
| 3 | 0:39 | Cartão do protocolo: `A5 \| LEN \| SEQ \| TYPE \| PAYLOAD \| CRC16` | Início, tamanho, número do pedido, tipo e CRC. O motor confere o limite e responde OK ou FORA DO LIMITE, sempre com o valor valendo |
| 4 | 1:03 | Cartão com a fórmula do PW | O PW parte da célula interpolada, aplica a ficha e as correções e soma o tempo morto. Com as correções em zero, PW = célula |
| 5 | 1:24 | Captura `01_painel_marcha_lenta` | Painel no simulador: rpm, PW, avanço, carga e temperaturas, a 10 Hz |
| 6 | 1:41 | Captura `02_painel_alerta_rpm` | Passou de 6500 rpm: alerta vermelho, que só avisa e não mexe no mapa |
| 7 | 1:52 | Captura `04_combustivel_celula_atual` | A célula atual fica em laranja na grade de combustível |
| 8 | 2:00 | Captura `06_combustivel_confirmado` | Tocar, ajustar e enviar. O ESP confirma e o PW sobe na hora |
| 9 | 2:14 | Captura `08_combustivel_recusado` | 99,99 ms é recusado e o valor anterior continua |
| 10 | 2:25 | Captura `09_ignicao_grade` | Ignição na mesma grade, de −10 a 45° |
| 11 | 2:33 | Captura `10_correcoes_aquecimento` | Cinco curvas de correção e o valor valendo agora |
| 12 | 2:41 | Captura `13_ficha_do_motor` | A ficha só muda com o motor parado. Com 3 cilindros, a ordem vira 1-2-3 |
| 13 | 2:53 | Captura `15_backup_importado` | Gravar na flash e backup em arquivo, com a versão e o CRC conferidos |
| 14 | 3:07 | Cartão "Tudo verde no computador" | 37 testes do núcleo, 54 verificações de tela e 2 firmwares compilando, com um comando |
| 15 | 3:25 | Cartão "Falta a bancada" | Placa real na bancada, integração com a 1.5 e revisão do Totti |

A narração deste vídeo usa a voz padrão do Windows (SAPI).

## Versão nova (pendente): voz neural masculina e animações

Em 26 nov. 2026, o Celso aprovou uma amostra com a voz **`pt-BR-AntonioNeural`**, gerada com a biblioteca `edge-tts` (`rate -5%`). Faltou renderizar as animações, porque o Docker da máquina estava com problema.

Para gerar a versão nova:
1. Gere o áudio de cada cena com a narração abaixo, usando `edge_tts.Communicate(texto, "pt-BR-AntonioNeural", rate="-5%", boundary="WordBoundary")`. O tempo de cada palavra serve para sincronizar as animações e gerar a legenda.
2. Anime as cenas:
   - bytes andando no fio da UART;
   - o frame se montando byte a byte;
   - a fórmula do PW termo a termo;
   - zoom e destaques nas capturas;
   - contadores dos testes;
   - um terminal digitando o comando.
3. Monte o vídeo em 1920×1080 e substitua `hmi-s3-explicacao.mp4`.

| # | Cena | Narração |
|---|------|----------|
| 1 | Abertura | Fala, time! Neste vídeo eu mostro o que entrou no PR 193: as onze tasks das features um ponto seis e um ponto sete. É a tela da nossa ECU, rodando numa ESP32-S3. |
| 2 | Duas placas | A ECU tem duas placas. O ESP do motor é o cérebro: ele lê os sensores, calcula a injeção e guarda os mapas. A S3, com a tela de 4,3 polegadas, só mostra e pede. Entre as duas, só três fios de UART. Se a tela travar, o motor continua igual. |
| 3 | O frame | Para conversar, as placas usam um protocolo nosso. Cada mensagem começa com o byte A5. Depois vêm o tamanho, o número do pedido, o tipo, os dados, e um CRC, que pega erro de cabo. A resposta tem sempre o tipo do pedido, mais oitenta em hexadecimal. |
| 4 | O motor decide | E quem decide é o motor. A tela pede: muda esta célula. O motor confere o limite e responde: confirmado, ou fora do limite. E sempre devolve o valor que está valendo. Se a resposta não chega em duzentos milissegundos, a tela tenta de novo, até três vezes. E o motor nunca aplica o mesmo pedido duas vezes. |
| 5 | Telemetria | Vinte vezes por segundo, o motor manda a telemetria: rotação, carga, temperaturas, tempo de injeção, avanço, e a célula em que ele está agora. |
| 6 | Tempo de injeção | E como o ESP calcula o tempo de injeção? Ele parte da célula do mapa, interpolada entre os quatro vizinhos. Ajusta pela vazão do bico. Aplica as correções de aquecimento, ar, partida e aceleração. E soma o tempo morto, que depende da bateria. Com as correções em zero, o tempo é exatamente o valor da célula. |
| 7 | Painel | Agora, as telas. Este é o painel, rodando no simulador do computador, com o mesmo código que vai para a S3. Rotação, injeção, avanço, carga e temperaturas, tudo ao vivo. |
| 8 | Alerta | Quando a rotação passa do limite da ficha, de seis mil e quinhentos, a faixa de alerta fica vermelha. O alerta só avisa. Ele nunca mexe no mapa. |
| 9 | Célula atual | Na grade de combustível, de dezesseis por dezesseis, a célula em que o motor está agora fica em laranja. Quanto mais quente a cor, maior o tempo de injeção. |
| 10 | Editar | Para editar, eu toco numa célula, ajusto o valor, e envio. O ESP confirma, o valor passa a valer na hora, e o tempo de injeção sobe. |
| 11 | Recusa | Se eu mandar um valor absurdo, como noventa e nove milissegundos, o ESP recusa. A tela mostra a recusa, e o valor anterior continua valendo. |
| 12 | Ignição | A ignição usa a mesma grade, em graus, com limite de menos dez a quarenta e cinco. |
| 13 | Correções | Na tela de correções, eu edito as cinco curvas: aquecimento, temperatura do ar, partida, tempo morto e aceleração. E vejo a correção que está valendo agora. |
| 14 | Ficha do motor | A ficha do motor guarda os cilindros, a roda fônica, a vazão dos bicos e os limites dos alertas. Ela só muda com o motor parado. Com três cilindros, por exemplo, a ordem de ignição vira um, dois, três. |
| 15 | Gravar e backup | As edições valem na hora, mas ficam na memória. Só o botão Gravar leva para a flash, e só com o motor parado. O backup vai para um arquivo, e o ESP confere a versão e o CRC antes de aceitar uma restauração. |
| 16 | Testes | Tudo isso foi testado no computador. São trinta e sete testes do núcleo, cinquenta e quatro verificações de tela, que tocam nos botões de verdade, e os dois firmwares compilando: o da tela, e o de bancada. |
| 17 | Como rodar | Para rodar, é um comando só, com o Docker. Ele compila tudo e mostra cada verificação, uma por uma. O passo a passo completo está no tutorial da documentação. |
| 18 | Próximos passos | O que falta é a bancada: a placa S3 de verdade, a fiação da UART, e o tempo de injeção medido na lâmpada, junto com o firmware do motor, da feature um ponto cinco. O PR está com o Totti, para revisão. Valeu, time! |
