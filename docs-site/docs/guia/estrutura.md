---
title: Estrutura
slug: /guia/estrutura
sidebar_position: 2
---

# Estrutura do projeto

<div style={{textAlign: 'justify'}}>

&emsp;Este documento explica como o conteúdo do site está organizado e como publicar novas páginas.

## Pastas principais

- `docs/`: guarda as páginas de documentação escritas em `.md` ou `.mdx`
- `src/`: concentra telas personalizadas, estilos e componentes React
- `static/`: armazena imagens, ícones e outros arquivos públicos

## Como isso vira navegação

&emsp;O Docusaurus gera a sidebar automaticamente a partir da estrutura de pastas dentro de `docs/`. A hierarquia de pastas determina a ordem e o agrupamento dos tópicos. Cada pasta pode ter um arquivo `_category_.json` para definir o nome, posição e comportamento da seção.

&emsp;Exemplo: `docs/guia/exemplo.md` aparece como "Exemplo" dentro da seção "Guia de Uso".

## Publicando uma nova página

&emsp;Crie um arquivo `.md` dentro de `docs-site/docs/` (ou numa subpasta para organizar):

```
docs/guia/minha-pagina.md
```

&emsp;Adicione o frontmatter no topo do arquivo:

```md
---
title: "Título da Página"
slug: /guia/minha-pagina
sidebar_position: 1
---
```

&emsp;Envolva o conteúdo em um `<div>` para garantir o alinhamento justificado padrão do projeto:

```md
<div style={{textAlign: 'justify'}}>

&emsp;Conteúdo aqui...

</div>
```

&emsp;Descrição dos campos do frontmatter:

- `title`: nome exibido no topo da página e na sidebar
- `slug`: rota final no site (URL)
- `sidebar_position`: posição do item no menu dentro da seção

## Trabalhando com imagens

&emsp;Salve as imagens em `docs-site/static/img/` e referencie com caminho absoluto no Markdown:

```md
<div align="center">
<small><strong style={{fontSize: '12px'}}>Figura X — Título da Imagem</strong></small>

![descrição](/img/nome-da-imagem.png)

<small style={{marginTop: '4px', fontSize: '10px'}}>Fonte: Material produzido pelo grupo, 2026.</small>
</div>
```

## Atualizando uma imagem

&emsp;Substitua o arquivo em `docs-site/static/img/` pelo novo arquivo com o mesmo nome. Se o nome mudar, atualize também o caminho no Markdown.

</div>
