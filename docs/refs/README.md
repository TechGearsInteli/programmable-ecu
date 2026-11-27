# Referências do projeto (`docs/refs`)

Esta pasta reúne o contexto que uma pessoa nova no time, ou um agente como o Claude (via `CLAUDE.md`), precisa para pegar uma task sem depender da conversa de ninguém. Ela fica **fora do site Docusaurus**: não é publicada e não passa pelo lint de `docs/docs/`.

## Ordem de leitura

| # | Arquivo | Para quê |
|---|---------|----------|
| 1 | [`fundamentos-do-projeto.md`](fundamentos-do-projeto.md) | O que é a ECU, o recorte de duas placas, o que está combinado e o que está fora, a teoria mínima de motor e o glossário |
| 2 | [`gestao-do-projeto.md`](gestao-do-projeto.md) | Quem é quem, o board DevGears, o template de task, o fluxo de PR e as pendências |
| 3 | [`hmi-s3-o-que-foi-implementado.md`](hmi-s3-o-que-foi-implementado.md) | Explicação didática das features 1.6 e 1.7 (HMI na S3), entregues no PR #193 |
| 4 | [`slides/weekly-hmi-s3.pdf`](slides/weekly-hmi-s3.pdf) | Os 14 slides da weekly de 25 nov. 2026, com o tutorial para rodar ao vivo |
| 5 | [`video/`](video/) | Vídeo explicativo da HMI S3 (3min41s) e o roteiro em texto |

## Fontes primárias (no site)

Os arquivos daqui resumem e apontam. Quando houver detalhe técnico, vale a página original:

- `docs/docs/contexto-decisoes.md`: **o recorte em vigor**. Ganha de qualquer outra página.
- `docs/docs/arquitetura.md`, `teoria-do-motor.md` e `estudo-hardware-ecu.md`: base de estudo. Têm partes anteriores ao recorte; veja os avisos em `fundamentos-do-projeto.md`.
- `docs/docs/protocolo-uart.md`, `nucleo-calibracao.md`, `hmi-s3.md` e `tutorial-hmi.md`: o que foi implementado nas features 1.6 e 1.7.

## Como manter

- Mudou o recorte? Atualize `contexto-decisoes.md` primeiro, com a equipe ciente. Depois ajuste `fundamentos-do-projeto.md`.
- Entregou uma feature? Acrescente o material de referência dela aqui (documento, slides, vídeo), cada um na sua pasta, e uma linha nesta tabela.
- A foto do board em `gestao-do-projeto.md` tem data. Atualize quando for usá-la para planejar.
