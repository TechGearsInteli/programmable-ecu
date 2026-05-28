---
title: Integração Contínua
slug: /guia/ci
sidebar_position: 6
---

# Integração Contínua

<div style={{textAlign: 'justify'}}>

&emsp;Este documento explica como funciona a integração contínua do projeto. O pipeline possui dois estágios independentes, configurados em `.github/workflows/`.

## Estágios do pipeline

### Lint: Verificação da documentação

&emsp;Ativado quando um Pull Request é aberto ou atualizado. O job `check-docs` analisa os arquivos `.md` modificados dentro da pasta `docs-site/docs/` e valida se cada arquivo segue os padrões do projeto:

- Bloco de frontmatter (`---`) contendo `title`, `slug` e `sidebar_position`
- Conteúdo envolvido em `<div style={{textAlign: 'justify'}}>`
- Imagens dentro de `<div align="center">` com legenda numerada (`Figura X`) e fonte
- Tabelas dentro de `<div align="center">` com legenda numerada (`Quadro X`) e fonte

&emsp;Blocos de código são excluídos da análise. Arquivos fora de `docs-site/docs/` são ignorados.

### Deploy: Publicação no GitHub Pages

&emsp;Ativado quando commits chegam à branch `main`. O job executa:

1. Instalação das dependências com `npm ci`
2. Build dos arquivos estáticos com `npm run build`
3. Upload do artefato gerado em `docs-site/build/`
4. Deploy no GitHub Pages em `https://docs.techgears.app/`

## Configuração necessária antes de abrir um PR

&emsp;Todos os jobs devem passar antes do merge. Verifique antes de abrir o PR:

- [ ] Frontmatter presente em todos os arquivos `.md` novos ou editados
- [ ] Conteúdo corretamente envolvido no `<div>` de alinhamento
- [ ] Imagens e tabelas formatadas com legendas e fontes
- [ ] Nenhum link interno quebrado

</div>
