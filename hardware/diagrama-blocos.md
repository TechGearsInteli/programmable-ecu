# Diagrama de Blocos do Sistema ECU

Visão geral de todos os blocos do hardware e suas conexões.

```mermaid
flowchart TB
    subgraph SENSORES["Sensores de Entrada"]
        CKP["CKP\n(Virabrequim / RPM)\nSinal VR / Hall"]
        MAP["MAP\n(Pressão do Coletor)\nI²C / Analógico"]
        TPS["TPS\n(Posição do Acelerador)\nAnalógico 0–5 V"]
        CLT["CLT\n(Temp. Água do Motor)\nNTC / Analógico"]
        IAT["IAT\n(Temp. do Ar)\nNTC / Analógico"]
        O2["Sonda Lambda\n(Mistura A/F)\nAnalógico / Wideband"]
    end

    subgraph MCU["Microcontrolador — ESP32"]
        ADC["ADC\n(12 bits)"]
        GPIO["GPIO / Interrupções"]
        TIMERS["Timers de Hardware"]
        WIFI["Wi-Fi / BT"]
        FLASH["Flash / NVS\n(Mapas de Calibração)"]
        CALC["Núcleo de Cálculo\n(Pulse Width, Avanço)"]
    end

    subgraph DRIVERS["Drivers de Saída"]
        INJ_DRV["Driver de Injetores\n(MOSFET / L298)"]
        IGN_DRV["Driver de Bobinas\n(IGN Coil Driver)"]
    end

    subgraph ATUADORES["Atuadores"]
        INJ1["Injetor 1"]
        INJ2["Injetor 2"]
        INJ3["Injetor 3"]
        INJ4["Injetor 4"]
        BOB1["Bobina 1–2"]
        BOB2["Bobina 3–4"]
    end

    subgraph ALIMENTACAO["Fonte / Proteção"]
        BAT["Bateria 12 V"]
        REG["Regulador 5 V / 3.3 V\n(LM2596 + AMS1117)"]
        PROT["Proteção contra\nTransientes\n(TVS + Fuse)"]
    end

    subgraph INTERFACE["Interface de Calibração"]
        WEB["Interface Web\n(React)"]
        WS["Servidor WebSocket\n(ESP32)"]
    end

    %% Fluxo de alimentação
    BAT --> PROT --> REG --> MCU
    BAT --> INJ_DRV
    BAT --> IGN_DRV

    %% Entradas dos sensores
    CKP -->|"Pulso digital\n(interrupção)"| GPIO
    MAP -->|"Tensão analógica\nou I²C"| ADC
    TPS -->|"Tensão analógica\n0–5 V"| ADC
    CLT -->|"Tensão analógica\n(divisor resistivo)"| ADC
    IAT -->|"Tensão analógica\n(divisor resistivo)"| ADC
    O2  -->|"Tensão analógica\n0–1 V / 0–5 V"| ADC

    %% Processamento interno
    ADC --> CALC
    GPIO --> CALC
    FLASH <-->|"Leitura/escrita\nde mapas"| CALC
    CALC --> TIMERS

    %% Saídas para drivers
    TIMERS -->|"PWM / Pulso temporizado"| INJ_DRV
    TIMERS -->|"Sinal de ignição"| IGN_DRV

    %% Drivers para atuadores
    INJ_DRV --> INJ1 & INJ2 & INJ3 & INJ4
    IGN_DRV --> BOB1 & BOB2

    %% Interface de calibração
    WIFI <-->|"WebSocket"| WS
    WS <-->|"HTTP / WS"| WEB
    WEB -->|"Edição de mapas\ne parâmetros"| FLASH
```

## Descrição dos Blocos

| Bloco | Função | Interface |
|-------|--------|-----------|
| **CKP** | Lê a posição e velocidade do virabrequim via roda fônica | Digital (interrupção de borda) |
| **MAP** | Mede a pressão absoluta do coletor (carga do motor) | Analógico ou I²C |
| **TPS** | Lê a abertura da borboleta (0–100%) | Analógico 0–5 V |
| **CLT** | Mede a temperatura do líquido de arrefecimento | NTC → divisor resistivo → ADC |
| **IAT** | Mede a temperatura do ar admitido | NTC → divisor resistivo → ADC |
| **Sonda Lambda** | Mede a mistura ar-combustível para controle em malha fechada | Analógico (NBO2) ou protocolo wideband |
| **ESP32** | Microcontrolador principal — lê sensores, calcula e aciona saídas | — |
| **Driver Injetores** | Amplifica o sinal do ESP32 para acionar solenoides de alta corrente | PWM → MOSFET → Injetor |
| **Driver Bobinas** | Gera o pulso de ignição nas bobinas (CDI/IGN) | Sinal digital → Driver dedicado |
| **Regulador** | Converte 12 V da bateria para 5 V e 3,3 V | LM2596 (5 V) + AMS1117 (3,3 V) |
| **Interface Web** | Permite editar mapas de calibração em tempo real via browser | Wi-Fi → WebSocket |
