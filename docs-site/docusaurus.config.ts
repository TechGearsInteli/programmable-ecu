import {themes as prismThemes} from 'prism-react-renderer';
import type {Config} from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';

// This runs in Node.js - Don't use client-side code here (browser APIs, JSX...)

const config: Config = {
  title: 'Programmable ECU',
  tagline: 'ECU programável de código aberto para motores de 4 cilindros',
  favicon: 'img/favicon.png',

  future: {
    v4: true,
  },

  url: 'https://docs.techgears.app',
  baseUrl: '/programmable-ecu/',

  organizationName: 'TechGearsInteli',
  projectName: 'programmable-ecu',
  trailingSlash: false,

  onBrokenLinks: 'throw',

  i18n: {
    defaultLocale: 'pt-BR',
    locales: ['pt-BR'],
  },

  presets: [
    [
      'classic',
      {
        docs: {
          routeBasePath: '/',
          sidebarPath: './sidebars.ts',
          editUrl: 'https://github.com/TechGearsInteli/programmable-ecu/edit/main/docs-site/',
        },
        blog: {
          showReadingTime: true,
          feedOptions: {
            type: ['rss', 'atom'],
            xslt: true,
          },
          editUrl: 'https://github.com/TechGearsInteli/programmable-ecu/edit/main/docs-site/',
          onInlineTags: 'warn',
          onInlineAuthors: 'warn',
          onUntruncatedBlogPosts: 'warn',
          blogTitle: 'Diário Técnico',
          blogDescription: 'Registro cronológico do desenvolvimento da ECU',
        },
        theme: {
          customCss: './src/css/custom.css',
        },
      } satisfies Preset.Options,
    ],
  ],

  markdown: {
    mermaid: true,
  },

  themes: ['@docusaurus/theme-mermaid'],

  themeConfig: {
    image: 'img/docusaurus-social-card.jpg',
    colorMode: {
      defaultMode: 'dark',
      disableSwitch: false,
      respectPrefersColorScheme: false,
    },
    navbar: {
      title: 'Programmable ECU',
      logo: {
        alt: 'TechGears Logo',
        src: 'img/techgears-logo.png',
      },
      style: 'dark',
      items: [],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'Documentação',
          items: [
            {label: 'Visão Geral', to: '/'},
            {label: 'Guia de Uso', to: '/category/guia-de-uso'},
          ],
        },
        {
          title: 'Projeto',
          items: [
            {label: 'GitHub', href: 'https://github.com/TechGearsInteli/programmable-ecu'},
            {label: 'Issues', href: 'https://github.com/TechGearsInteli/programmable-ecu/issues'},
            {label: 'Projeto DevGears', href: 'https://github.com/orgs/TechGearsInteli/projects/1'},
          ],
        },
        {
          title: 'TechGears',
          items: [
            {label: 'Site', href: 'https://techgears.app'},
            {label: 'Instagram', href: 'https://www.instagram.com/tech.gears01/'},
            {label: 'LinkedIn', href: 'https://www.linkedin.com/company/tech-gears01/'},
          ],
        },
      ],
      copyright: `Copyright © ${new Date().getFullYear()} TechGears Inteli. Built with Docusaurus.`,
    },
    prism: {
      theme: prismThemes.oneDark,
      darkTheme: prismThemes.oneDark,
      additionalLanguages: ['c', 'cpp', 'json', 'bash'],
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
