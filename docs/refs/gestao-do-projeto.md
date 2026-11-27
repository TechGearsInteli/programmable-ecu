# Gestão do projeto

> Quem é quem, como o trabalho é organizado no GitHub e o que está pendente.
> A foto do board é de **26 nov. 2026**. Antes de planejar, confira o board ao vivo.

## 1. Time e papéis (Épico 1, ECU)

| Pessoa | GitHub | Papel agora (divisão de 11 nov. 2026) |
|--------|--------|----------------------------------------|
| Eduardo ("Totti") | `tottithome` | O carro (plugues, pinos, cabo até a caixa) e as compras. **Organiza o board e as issues do Épico 1 e revisa os PRs** |
| Lucas | `Ludelll` | Protótipo na mesa e firmware do motor: features 1.4 e 1.5 |
| Celso | `celsick` | A tela: simulador, painel e calibração. Features 1.6 e 1.7 |
| — | `FeLisak` | Admin do repositório. Fez a feature 1.1 (repositório, Docusaurus, diagramas) |

- O repositório é `TechGearsInteli/programmable-ecu`. O time do GitHub é "programmable-ecu".
- O mesmo board tem outros épicos, que aqui servem só de contexto:
  - Épico 2: OBD-II;
  - Épico 3: Hub TechGears.
- Este repositório e esta pasta tratam do **Épico 1 (#100)**.

## 2. Hierarquia das tasks

`Épico (#100) → Feature (1.N) → História (1.N.1) → Task (1.N.1.M)`

O número aparece no título da issue, entre colchetes. Por exemplo, `[1.6.1.3] Permitir editar uma célula...`.

Toda task segue `.github/ISSUE_TEMPLATE/task.md`:
- **Descrição**;
- **Critérios de Aceite**;
- **DoR**: título claro, descrição detalhada, estimativa e critérios de aceite;
- **DoD**: testes realizados, código implementado, documentado com referências bibliográficas, código revisado, explicação clara na PR e justificativa das tecnologias na PR.

## 3. O board DevGears (GitHub Project #1)

- **Colunas:** `Backlog → Sprint Backlog → Fazendo → Review → Concluído`.
- **Regras:**
  - cada pessoa move **só os próprios cards**: Fazendo quando começa e Review quando o PR abre;
  - **Concluído só depois da revisão do Totti**;
  - quem organiza o board é o Totti. Não se reorganiza, não se reescreve issue de outra pessoa e não se muda o pai de uma issue sem combinar;
  - não há regra de tempo de card nem de formato de commit.
- **Leitura pela CLI:**

  ```bash
  gh project item-list 1 --owner TechGearsInteli --limit 400 --format json
  ```

  Faça isso **uma vez** e reaproveite o resultado. A consulta é cara, e repetir por card estoura o limite da API GraphQL.
- IDs, para `gh project item-edit`:
  - projeto `PVT_kwDOERZsBs4BYZQ9`;
  - campo Status `PVTSSF_lADOERZsBs4BYZQ9zhTfbn0`;
  - opções: Backlog `f75ad846`, Sprint Backlog `bba1a0fb`, Fazendo `47fc9ee4`, Review `81a56fb5`, Concluído `98236657`.

## 4. Foto do Épico 1 (26 nov. 2026)

| Feature | Dono | Estado | Observação |
|---------|------|--------|------------|
| 1.1 Infraestrutura (#101–107) | FeLisak | Concluído | Repositório, Docusaurus, diagramas e ADRs |
| 1.2 Estudo de motores (#108–115) | tottithome | Concluído | Base de `teoria-do-motor.md` |
| 1.3 Estudo de hardware (#116–124) | celsick | Concluído | `estudo-hardware-ecu.md`. A recomendação de STM32 foi superada pelo recorte |
| 1.4 Hardware (#125–130, #179–182) | Ludelll e tottithome | Backlog | Conector do Fit, condicionamento, potência, relé e lâmpada, adaptador, energização, proteções |
| 1.5 Firmware do motor (#131–142, #183) | Ludelll | Backlog | **Nenhum firmware do motor na `main`.** O núcleo do PR #193 já cobre parte de #136, #137, #139, #140 e #142 |
| 1.6 Calibração na S3 (#143–149, #184–186) | celsick | **Review** | PR #193 |
| 1.7 Painel e datalog (#150–154, #187, #188) | celsick | #150 Fazendo. #152, #153 e #187 em Review | O datalogger (#154) e a revisão do log com exportação CSV (#188) estão sem dono |
| 1.8 Segurança (#155–160, #189, #190) | sem dono | Backlog | FMEA, falha de sensor, limp mode, watchdog, fallback Honda, corte da bomba |
| 1.9 Testes no Fit (#161–166) | Ludelll (#164) | Backlog | Plano, lâmpada, pátio e rua |
| 1.10 Documentação (#167–174, #191) | sem dono | #169 e #170 em Review | Alinhar o site ao recorte, retirando OBD e interface web |

Contagem geral do board (todos os épicos, 216 itens): Backlog 134, Sprint Backlog 7, Fazendo 8, Review 31 e Concluído 36.

## 5. Fluxo de branch, commit e PR

1. **Branch** por entrega: `feat/<feature>-<assunto>`, por exemplo `feat/1.6-1.7-hmi-s3`.
2. **Commits** agrupados por assunto, sem exagero (cerca de 8 numa feature grande). Formato livre. O time costuma usar `feat(1.6.1.1): ...`, `build: ...` e `docs: ...`.
3. **PR:** o mínimo possível, de preferência um por entrega, com **revisor `tottithome`**. O corpo segue `.github/pull_request_template.md`: Branch, Tasks, Commits, Descrição (o quê e por quê), Revisor, Teste, Documentação e Colaboradores.
4. **Pegadinha do workflow `pr-description.yml`:**
   - quando o PR abre, ele **sobrescreve o corpo** com o template e preenche as Tasks (os cards do autor em "Fazendo") e os Commits;
   - a consulta só lê os 50 primeiros itens do board, então tasks recentes costumam ficar de fora;
   - por isso, depois que o PR abrir, edite o corpo (`gh pr edit` ou a API REST) e complete à mão;
   - não rode o workflow de novo, porque ele apaga o que foi escrito.
5. **CI:**
   - `lint-docs.yml` roda `lint/check-docs.js` quando o PR mexe em `docs/docs/`;
   - `deploy-docs.yml` publica o site a cada push na `main`.
6. **Regras do lint de docs** (só para `docs/docs/`):
   - frontmatter com `title`, `slug` e `sidebar_position`;
   - o conteúdo dentro de `<div style={{textAlign: 'justify'}}>`;
   - toda tabela e imagem dentro de `<div align="center">`, com legenda "Quadro N" ou "Figura N" e uma linha "Fonte:". Tabelas sob o título "Referências" são exceção.
7. **Documentação junto com o código:** cada feature entregue ganha uma página em `docs/docs/` e, se ajudar, material em `docs/refs/`.

## 6. Pendências conhecidas (26 nov. 2026)

**Da HMI (features 1.6 e 1.7, PR #193):**
- validar na bancada:
  - a tela e o touch reais, conferindo os pinos de `src/firmware/hmi/s3/src/board.h`;
  - a fiação da UART;
  - a NVS real e o reinício pela chave;
- **escolher e comprar a placa S3.** O firmware está configurado para a Sunton ESP32-8048S043C, que ainda não foi validada;
- #150 (Feature 1.7) continua em Fazendo. O datalogger (#154) e a revisão e exportação do log (#188) não foram feitos;
- gerar a versão nova do vídeo, com voz neural masculina. O roteiro está pronto em `docs/refs/video/roteiro.md`, mas o Docker estava com problema em 26 nov. 2026.

**Integração com a 1.5 (Lucas):**
- o firmware do motor deve usar o núcleo `src/firmware/common`:
  - `ecu_engine_compute()` no laço;
  - `ecu_motor_link_feed()` e `ecu_motor_link_send_telemetry()` na task da UART;
  - a gravação fora da ISR;
- medir o PW e o avanço na lâmpada, junto com o firmware da 1.5.

**Do recorte (Eduardo):** os dados que faltam estão em `fundamentos-do-projeto.md`, seção 4.
