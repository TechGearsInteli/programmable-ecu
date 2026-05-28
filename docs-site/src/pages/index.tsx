import type {ReactNode} from 'react';
import Link from '@docusaurus/Link';
import Layout from '@theme/Layout';

export default function Home(): ReactNode {
  return (
    <Layout
      title="Programmable ECU"
      description="ECU programável de código aberto desenvolvida pelo clube universitário TechGears no Inteli."
    >
      <header className="hero--techgears">
        <div className="container">
          <h1 className="hero__title">
            Tech<span>Gears</span> — Programmable ECU
          </h1>
          <p className="hero__subtitle">
            ECU programável de código aberto para motores de 4 cilindros,
            desenvolvida pelo clube universitário TechGears no Inteli.
          </p>
          <div style={{display: 'flex', gap: '1rem', justifyContent: 'center', flexWrap: 'wrap'}}>
            <Link className="button button--primary button--lg" to="/docs/introducao">
              Começar
            </Link>
            <Link
              className="button button--outline button--lg"
              style={{color: '#fff', borderColor: 'rgba(255,255,255,0.4)'}}
              href="https://github.com/TechGearsInteli/programmable-ecu"
            >
              GitHub
            </Link>
          </div>
        </div>
      </header>

      <main className="features-section">
        <div className="container">
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(auto-fit, minmax(240px, 1fr))',
            gap: '1.5rem',
            marginTop: '1rem',
          }}>
            <div className="feature-card">
              <h3>Hardware Aberto</h3>
              <p>Esquemáticos, lista de componentes e diagramas de blocos disponíveis para qualquer um replicar ou evoluir.</p>
            </div>
            <div className="feature-card">
              <h3>Firmware em C</h3>
              <p>ESP32 + ESP-IDF com arquitetura em camadas, interpolação bilinear e controle lambda em malha fechada.</p>
            </div>
            <div className="feature-card">
              <h3>Calibração Web</h3>
              <p>Interface React + WebSocket para editar o mapa de combustível e acompanhar telemetria em tempo real via Wi-Fi.</p>
            </div>
            <div className="feature-card">
              <h3>Documentação Viva</h3>
              <p>Toda a decisão de projeto, teoria e guias de instalação documentados aqui, mantidos pelo clube.</p>
            </div>
          </div>
        </div>
      </main>
    </Layout>
  );
}
