---
title: Padrão de documentação
slug: /guia/padrao-documentacao
sidebar_position: 3
---

# Padrão de Documentação

<div style={{textAlign: 'justify'}}>

&emsp;Este guia define as regras de escrita, organização e apresentação visual para manter as páginas do projeto no mesmo padrão.

## 1. Escrita de Parágrafos

&emsp;Todo parágrafo deve começar com o comando `&emsp;`. Isso se aplica a todo texto corrido: aberturas, explicações, transições e encerramentos.

```md
## Nome da Seção

&emsp;Texto de abertura.

&emsp;Explicação detalhada.

&emsp;Texto de encerramento.
```

## 2. Formato esperado de seções

&emsp;Siga esta sequência ao escrever cada seção:

1. Título objetivo
2. Breve introdução
3. Desenvolvimento do assunto
4. Encerramento conectando ao projeto

**Modelo base:**

```md
## Título da Seção

&emsp;Apresentação do tema.

&emsp;Explicação dos principais detalhes.

&emsp;Síntese do ponto central conectando ao projeto.
```

## 3. Padrão de imagens e tabelas

&emsp;Toda imagem deve ter número, título e fonte:

```html
<div align="center">
<small><strong style={{fontSize: '12px'}}>Figura X — Título da Imagem</strong></small>

![imagem](/img/NOME_DA_IMAGEM.png)

<small style={{marginTop: '4px', fontSize: '10px'}}>Fonte: Material produzido pelo grupo, 2026.</small>
</div>
```

&emsp;Toda tabela (Quadro) segue o mesmo padrão:

```html
<div align="center">
<small><strong style={{fontSize: '12px'}}>Quadro X — Título da Tabela</strong></small>

| Coluna A | Coluna B |
|----------|----------|
| Dado 1   | Dado 2   |

<small style={{marginTop: '4px', fontSize: '10px'}}>Fonte: Material produzido pelo grupo, 2026.</small>
</div>
```

&emsp;Formato da fonte:
- Externa: `Fonte: Nome do Autor/Instituição, ANO`
- Interna: `Fonte: Material produzido pelo grupo, 2026`

## 4. Tom de escrita

&emsp;A linguagem deve ser **clara, objetiva e formal**:

- Evite gírias e linguagem informal
- Defina termos técnicos na primeira menção
- Nunca use primeira pessoa
- Use voz passiva ou organizacional: "O sistema realiza", "O projeto propõe", "A solução desenvolvida"

## 5. Hierarquia de títulos no Docusaurus

- `#` = título principal da página
- `##` = seções
- `###` = subseções
- Nunca pule níveis de hierarquia

## 6. Checklist de revisão final

&emsp;Antes de abrir o Pull Request, verifique:

- [ ] Parágrafos com `&emsp;` no início
- [ ] Seções com introdução, desenvolvimento e encerramento
- [ ] Imagens e tabelas numeradas com fonte
- [ ] Tom formal e impessoal mantido
- [ ] Conteúdo alinhado com os objetivos do projeto

</div>
