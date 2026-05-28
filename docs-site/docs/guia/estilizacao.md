---
title: Estilização
slug: /guia/estilizacao
sidebar_position: 1
---

# Guia Prático de Estilização Markdown

<div style={{textAlign: 'justify'}}>

&emsp;Este guia apresenta alguns recursos extras que vão além do Markdown básico e que podem ser usados nas páginas desta documentação.

## Blocos de Destaque

&emsp;Use os blocos abaixo para chamar atenção para informações importantes:

:::tip Sugestão
Prefira mensagens diretas e fáceis de ler.
:::

:::warning Cuidado
Revise comandos antes de executar no terminal.
:::

:::info Contexto
Este conteúdo é apenas informativo.
:::

:::note Observação
Anotações rápidas para referência futura.
:::

## Conteúdo Expansível

&emsp;Use as tags `<details>` e `<summary>` para ocultar informações secundárias:

```html
<details>
<summary>Clique para expandir</summary>

Informações adicionais aparecem aqui.

</details>
```

**Resultado:**

<details>
<summary>Clique para expandir</summary>

Informações adicionais aparecem aqui.

</details>

## Citação

&emsp;Use `>` para destacar uma frase ou referência:

> Este é um destaque em formato de citação.

## Tabelas

&emsp;Tabelas devem ser envolvidas em `<div align="center">` com título e fonte numerados:

```md
<div align="center">
<small><strong style={{fontSize: '12px'}}>Quadro X — Título da Tabela</strong></small>

| Item | Descrição |
|------|-----------|
| A    | Descrição A |
| B    | Descrição B |

<small style={{marginTop: '4px', fontSize: '10px'}}>Fonte: Material produzido pelo grupo, 2026.</small>
</div>
```

## Blocos de Código

&emsp;Use triple backticks com o nome da linguagem:

```bash
npm install
npm run start
```

&emsp;Esses recursos ajudam a organizar a documentação de forma clara em projetos colaborativos.

</div>
