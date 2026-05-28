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
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      } satisfies Preset.Options,
    ],
  ],

  markdown: {
    mermaid: true,
  },

  themes: [
    '@docusaurus/theme-mermaid',
    [
      require.resolve('@easyops-cn/docusaurus-search-local'),
      {
        hashed: true,
        language: ['pt'],
        indexBlog: false,
        docsRouteBasePath: '/',
        searchResultLimits: 8,
      },
    ],
  ],

  themeConfig: {
    image: 'img/docusaurus-social-card.jpg',
    colorMode: {
      defaultMode: 'dark',
      disableSwitch: false,
      respectPrefersColorScheme: false,
    },
    navbar: {
      title: '',
      logo: {
        alt: 'TechGears Logo',
        src: 'img/techgears-logo.png',
        href: 'https://techgears.app',
        target: '_blank',
      },
      style: 'dark',
      items: [
        {
          href: 'https://docs.techgears.app',
          label: 'Portal',
          position: 'left',
          target: '_self',
        },
        {
          to: '/',
          label: 'Documentação',
          position: 'left',
        },
        {
          href: 'https://github.com/TechGearsInteli/programmable-ecu',
          label: 'GitHub',
          position: 'right',
          target: '_self',
          className: 'navbar-github-link',
        },
      ],
    },
    footer: undefined,
    prism: {
      theme: prismThemes.oneDark,
      darkTheme: prismThemes.oneDark,
      additionalLanguages: ['c', 'cpp', 'json', 'bash'],
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
