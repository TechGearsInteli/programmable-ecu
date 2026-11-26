# CLAUDE.md — Programmable ECU (TechGears Inteli)

ECU programável de código aberto, no modelo FuelTech, para o Honda Fit da liga. São duas placas:
- **ESP do motor (ESP32)**: é o cérebro. Lê os sensores, calcula a injeção e a ignição, guarda os mapas e grava na flash.
- **ESP32-S3 com tela de 4,3"**: é só a interface. Mostra os dados e pede mudanças.

As duas placas conversam por UART. A equipe trabalha e documenta em **português (pt-BR)**.

## Antes de pegar uma task, leia

1. `docs/refs/README.md`: índice das referências e ordem de leitura.
2. `docs/refs/fundamentos-do-projeto.md`: o que é o projeto, o recorte, os conceitos de motor e o glossário.
3. `docs/refs/gestao-do-projeto.md`: o time, o board, como abrir task e PR, e o que está pendente.
4. `docs/docs/contexto-decisoes.md`: **é a fonte da verdade do recorte.** Se outra página discordar dela, vale esta.
5. A página da área em que você vai mexer:
   - `docs/docs/protocolo-uart.md`
   - `docs/docs/nucleo-calibracao.md`
   - `docs/docs/hmi-s3.md`
   - `docs/docs/tutorial-hmi.md`

## Mapa do repositório

| Pasta | O que tem |
|-------|-----------|
| `src/firmware/common/` | Núcleo em C99, sem dependência de placa: protocolo UART, tabelas, cálculo do PW, correções, ficha e gravação. Os testes Unity ficam em `test/` |
| `src/firmware/hmi/ui/` | Telas LVGL 9.2.2 (800×480). O mesmo código roda no simulador e na S3 |
| `src/firmware/hmi/s3/` | Firmware da tela (PlatformIO, Arduino-ESP32). Os pinos ficam em `src/board.h` e ainda estão **a confirmar** |
| `src/firmware/motor-bench/` | Firmware de bancada do ESP do motor, com potenciômetros no lugar dos sensores |
| `src/software/hmi-sim/` | Simulador da tela no PC, com motor virtual e cenários automáticos (`--headless`) |
| `tools/` | `host_build.py` (build e testes no PC), gerador de fontes e conversor BMP→PNG |
| `docs/docs/` | Site Docusaurus, publicado em docs.techgears.app/programmable-ecu |
| `docs/refs/` | Referências de contexto e gestão, fora do site |
| `lint/check-docs.js` | Lint obrigatório das páginas de `docs/docs/` |

## Comandos

```bash
python -m pip install --user cmake ninja ziglang    # uma vez
python tools/host_build.py                # compila o núcleo, as telas e o simulador e roda todos os testes (ctest)
python tools/host_build.py --cenarios     # mostra cada verificação de tela
python tools/host_build.py --docker       # o mesmo num container Linux (para máquinas que bloqueiam .exe novos)
python tools/host_build.py --sim          # abre o simulador em janela (Windows)
cd src/firmware/hmi/s3 && python -m platformio run          # firmware da tela
cd src/firmware/motor-bench && python -m platformio run     # firmware de bancada
node lint/check-docs.js                   # lint da documentação
cd docs && npm install && npm run build   # build do site
```

Resultado esperado hoje:
- ctest: 5/5, sendo 37 testes do núcleo mais `hmi_e2e`, com 54 verificações de tela;
- os dois firmwares com `[SUCCESS]`.

## Regras que não se quebram

- **O ESP do motor é a autoridade.** A tela nunca calcula injeção nem grava mapa sozinha. Ela pede, e o motor confere o limite e responde com o valor que está valendo.
- **Gravar na flash e mudar a ficha só com o motor parado** (rpm = 0). A gravação roda fora da ISR.
- **Nunca ligar 12 V nem sinal de motor na S3.** Entre as placas vão só três fios: TX, RX e GND, em 3,3 V.
- **Código do núcleo:**
  - é C99 portátil, sem `malloc` no caminho de controle;
  - toda função nova ganha teste Unity em `src/firmware/common/test/`;
  - inteiros com escala fixa: ms×100, °×10 e °C×10.
- **Protocolo:** qualquer mudança no frame ou nas mensagens exige atualizar `ecu_protocol.h`, os testes e `docs/docs/protocolo-uart.md`, tudo junto.
- **Páginas em `docs/docs/`** precisam passar no `check-docs.js`:
  - frontmatter com `title`, `slug` e `sidebar_position`;
  - o conteúdo dentro de `<div style={{textAlign: 'justify'}}>`;
  - tabelas e imagens dentro de `<div align="center">`, com "Quadro N" ou "Figura N" e "Fonte:".
- **Mudança de recorte** (placa, escopo, carro) só entra com a equipe ciente, e atualizando `contexto-decisoes.md`.
- **Board e PR:** siga `docs/refs/gestao-do-projeto.md`. Cada pessoa move só os próprios cards. O Totti (`tottithome`) organiza o board e revisa os PRs. Abra o mínimo de PRs possível.
