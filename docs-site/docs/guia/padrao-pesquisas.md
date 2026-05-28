---
title: Padrão de pesquisas
slug: /guia/padrao-pesquisas
sidebar_position: 4
---

# Padrão de Documentação de Pesquisas

<div style={{textAlign: 'justify'}}>

&emsp;Este guia define como escrever uma pesquisa dentro da documentação para seguir uma estrutura padronizada.

## Estrutura de uma Pesquisa

&emsp;Toda pesquisa comparativa deve seguir quatro seções obrigatórias:

### Seção 1: Introdução

&emsp;Explica o que está sendo pesquisado e por que. Contextualiza a necessidade da decisão dentro do projeto.

### Seção 2: Análise Individual

&emsp;Para cada opção avaliada, descreva:

- **O que é**
- **O que tem de bom**
- **O que tem de ruim**
- **Critérios analisados**

### Seção 3: Tabela Comparativa

&emsp;Coloca todas as opções lado a lado com os mesmos critérios, tornando a comparação mais direta:

```md
<div align="center">
<small><strong style={{fontSize: '12px'}}>Quadro X — Comparativo entre opções</strong></small>

| Critério     | Opção A | Opção B |
|--------------|---------|---------|
| Critério 1   | Sim     | Não     |
| Critério 2   | Alto    | Médio   |

<small style={{marginTop: '4px', fontSize: '10px'}}>Fonte: Material produzido pelo grupo, 2026.</small>
</div>
```

### Seção 4: Conclusão

&emsp;Declara qual opção foi escolhida e justifica usando os critérios de avaliação. Inclua os trade-offs considerados.

## Referências

&emsp;Liste as fontes no padrão ABNT ao final da página.

## Template Completo

```md
---
title: "Nome da Pesquisa"
slug: /secao/nome-da-pesquisa
sidebar_position: 1
---

# Nome da Pesquisa

<div style={{textAlign: 'justify'}}>

## Introdução

&emsp;Contexto e motivação da pesquisa.

## Análise Individual

### Opção A

&emsp;Descrição, pontos positivos, negativos e critérios.

### Opção B

&emsp;Descrição, pontos positivos, negativos e critérios.

## Tabela Comparativa

<div align="center">
<small><strong style={{fontSize: '12px'}}>Quadro X — Comparativo</strong></small>

| Critério | Opção A | Opção B |
|----------|---------|---------|
| ...      | ...     | ...     |

<small style={{marginTop: '4px', fontSize: '10px'}}>Fonte: Material produzido pelo grupo, 2026.</small>
</div>

## Conclusão

&emsp;Opção escolhida e justificativa.

## Referências

[01] ...

</div>
```

## Checklist antes de publicar

- [ ] Frontmatter presente com `title`, `slug` e `sidebar_position`
- [ ] Texto iniciando com `&emsp;`
- [ ] Análise individual para cada opção avaliada
- [ ] Tabela comparativa com os mesmos critérios para todas as opções
- [ ] Conclusão declarando a escolha e justificativa
- [ ] Referências em ABNT
- [ ] Linguagem formal e impessoal

</div>
