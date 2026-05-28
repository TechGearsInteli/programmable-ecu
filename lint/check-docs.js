#!/usr/bin/env node

// Esse script é responsável por verificar a padronização na documentação do projeto.
const fs = require("fs");

const FIGURA_PATTERN = /<small[^>]*>\s*<strong[^>]*>\s*Figura\s+\d+/;
const QUADRO_PATTERN = /<small[^>]*>\s*<strong[^>]*>\s*Quadro\s+\d+/;
const FONTE_PATTERN = /Fonte:/;
const IMAGE_PATTERN = /!\[.*?\]\(.*?\)/g;

function removeCodeBlocks(text) {
  let result = text.replace(/```[\s\S]*?```/g, (m) => " ".repeat(m.length));
  result = result.replace(/`[^`\n]+`/g, (m) => " ".repeat(m.length));
  return result;
}

function removeFrontmatter(text) {
  return text.replace(/^---\n[\s\S]*?\n---\n/, "");
}

function getCenteredDivRanges(text) {
  const marker = '<div align="center">';
  const endMarker = "</div>";
  const ranges = [];
  let pos = 0;
  while (pos < text.length) {
    const start = text.indexOf(marker, pos);
    if (start === -1) break;
    const end = text.indexOf(endMarker, start + marker.length);
    if (end === -1) break;
    ranges.push([start, end + endMarker.length]);
    pos = end + endMarker.length;
  }
  return ranges;
}

function isInRange(pos, ranges) {
  return ranges.some(([s, e]) => pos >= s && pos < e);
}

function isInReferencesSection(pos, text) {
  const refMatch = text.match(/^##+ Referências/m);
  if (!refMatch) return false;
  const start = refMatch.index;
  const after = text.slice(start + refMatch[0].length);
  const nextHeading = after.match(/^##+ /m);
  const end = nextHeading
    ? start + refMatch[0].length + nextHeading.index
    : text.length;
  return pos >= start && pos < end;
}

function getContainingBlock(pos, ranges, text) {
  const range = ranges.find(([s, e]) => pos >= s && pos < e);
  return range ? text.slice(range[0], range[1]) : null;
}

function checkFile(filepath) {
  const raw = fs.readFileSync(filepath, "utf8");
  const errors = [];

  // Frontmatter
  if (!raw.startsWith("---")) {
    errors.push("Frontmatter ausente (o arquivo deve começar com ---)");
    return errors;
  }
  const fmMatch = raw.match(/^---\n([\s\S]*?)\n---/);
  if (fmMatch) {
    const fm = fmMatch[1];
    if (!/^title:\s*.+/m.test(fm))
      errors.push("Campo 'title' ausente ou vazio no frontmatter");
    if (!/^slug:\s*.+/m.test(fm))
      errors.push("Campo 'slug' ausente ou vazio no frontmatter");
    if (!/^sidebar_position:\s*\d+/m.test(fm))
      errors.push("Campo 'sidebar_position' ausente ou vazio no frontmatter");
  }

  // div de justificação
  const body = removeFrontmatter(raw);
  if (!body.includes("<div style={{textAlign: 'justify'}}")) {
    errors.push("Tag <div style={{textAlign: 'justify'}}> ausente");
  }
  if (!body.includes("</div>")) {
    errors.push("Fechamento </div> ausente");
  }

  const clean = removeCodeBlocks(body);
  const divRanges = getCenteredDivRanges(clean);

  // Imagens
  IMAGE_PATTERN.lastIndex = 0;
  let imgMatch;
  while ((imgMatch = IMAGE_PATTERN.exec(clean)) !== null) {
    const label = `"${imgMatch[0].slice(0, 60)}"`;
    if (!isInRange(imgMatch.index, divRanges)) {
      errors.push(`Imagem fora de <div align="center">: ${label}`);
      continue;
    }
    const block = getContainingBlock(imgMatch.index, divRanges, clean);
    if (!FIGURA_PATTERN.test(block)) {
      errors.push(
        `Imagem sem legenda numerada (ex: "Figura 1 - Título"): ${label}`,
      );
    }
    if (!FONTE_PATTERN.test(block)) {
      errors.push(`Imagem sem fonte: ${label}`);
    }
  }

  // Tabelas — detecta pelo padrão de linha separadora |---|
  const lines = clean.split("\n");
  const linePositions = [];
  let charPos = 0;
  for (const line of lines) {
    linePositions.push(charPos);
    charPos += line.length + 1;
  }

  let tableStart = -1;
  const tableBlocks = [];
  for (let i = 0; i < lines.length; i++) {
    const trimmed = lines[i].trim();
    if (trimmed.startsWith("|") && tableStart === -1) {
      tableStart = i;
    } else if (!trimmed.startsWith("|") && tableStart !== -1) {
      tableBlocks.push({
        startLine: tableStart,
        charStart: linePositions[tableStart],
        lines: lines.slice(tableStart, i),
      });
      tableStart = -1;
    }
  }
  if (tableStart !== -1) {
    tableBlocks.push({
      startLine: tableStart,
      charStart: linePositions[tableStart],
      lines: lines.slice(tableStart),
    });
  }

  for (const table of tableBlocks) {
    if (!table.lines.some((l) => /^\|[\s\-|:]+\|$/.test(l.trim()))) continue;

    const pos = table.charStart;
    if (isInReferencesSection(pos, clean)) continue;

    if (!isInRange(pos, divRanges)) {
      errors.push('Tabela fora de <div align="center">');
      continue;
    }
    const block = getContainingBlock(pos, divRanges, clean);
    if (!QUADRO_PATTERN.test(block)) {
      errors.push('Tabela sem legenda numerada (ex: "Quadro 1 - Título")');
    }
    if (!FONTE_PATTERN.test(block)) {
      errors.push("Tabela sem fonte");
    }
  }

  return errors;
}

// Main
const files = process.argv.slice(2).filter((f) => f.endsWith(".md"));
let failed = false;

if (files.length === 0) {
  console.log("Nenhum arquivo .md alterado em docs/docs/.");
  process.exit(0);
}

for (const file of files) {
  if (!fs.existsSync(file)) continue;
  process.stdout.write(`Verificando: ${file} ... `);
  const errors = checkFile(file);
  if (errors.length > 0) {
    console.log("ERRO");
    errors.forEach((e) => console.log(`  - ${e}`));
    failed = true;
  } else {
    console.log("OK");
  }
}

if (failed) {
  console.log("\nUm ou mais arquivos não seguem o padrão de documentação.");
  console.log("Consulte docs/docs/guia/ para referência.");
  process.exit(1);
}

console.log("\nTodos os arquivos verificados estão no padrão.");
