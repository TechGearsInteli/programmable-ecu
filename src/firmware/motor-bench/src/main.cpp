/*
 * ESP do motor - firmware de bancada para a HMI (tasks 1.6.1.2 a 1.6.1.8).
 *
 * O que faz:
 *   - le 4 potenciometros no lugar dos sensores (RPM, carga/MAP, CLT e TPS);
 *   - calcula celula, Pulse Width e avanco com o nucleo (ecu_engine);
 *   - imprime no Serial a cada 0,5 s (para comparar com o que a S3 mostra);
 *   - conversa com a S3 pela UART2 numa task no nucleo 0 (ecu_motor_link);
 *   - guarda a calibracao na NVS quando a tela manda gravar.
 *
 * O que NAO faz: ler a roda fonica, agendar injecao/ignicao e acionar saidas.
 * Isso e do firmware do motor (feature 1.5, Lucas), que deve usar o mesmo
 * nucleo: ecu_engine_compute() no laco e ecu_motor_link_feed() na task da UART.
 *
 * Ligacao com a S3 (so 3 fios, 3,3 V):  GPIO17 (TX) -> RX da S3,
 *                                        GPIO16 (RX) <- TX da S3,  GND - GND.
 * Nunca ligue 12 V nem saidas de motor na S3.
 */
#include <Arduino.h>
#include <Preferences.h>

#include "ecu/ecu_engine.h"
#include "ecu/ecu_motor_link.h"

/* Pinos da bancada (ADC1, funcionam junto com o resto) */
static const int PIN_POT_RPM = 34;
static const int PIN_POT_MAP = 35;
static const int PIN_POT_CLT = 32;
static const int PIN_POT_TPS = 33;
static const int PIN_HMI_RX = 16;
static const int PIN_HMI_TX = 17;

static ecu_calib_t calib;            /* calibracao viva (RAM) */
static ecu_engine_state_t engine_state;
static ecu_inputs_t inputs;
static ecu_outputs_t outputs;
static ecu_motor_link_t link_hmi;
static ecu_storage_t storage;
static Preferences prefs;
static SemaphoreHandle_t calib_mutex; /* protege a troca da calibracao inteira (backup) */
static portMUX_TYPE snapshot_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint16_t rpm_now;

/* ---------------- NVS ----------------
 * A NVS grava de forma atomica: se faltar energia no meio, fica o valor
 * antigo ou o novo, nunca meio blob. Gravar na flash pausa o cache do ESP32,
 * por isso so acontece na task da UART e com o motor parado. */
static bool nvs_write(void *ctx, const uint8_t *data, size_t len)
{
    (void)ctx;
    return prefs.putBytes("calib", data, len) == len;
}

static size_t nvs_read(void *ctx, uint8_t *buf, size_t cap)
{
    (void)ctx;
    const size_t n = prefs.getBytesLength("calib");
    if (n == 0 || n > cap) return 0;
    return prefs.getBytes("calib", buf, n);
}

static void uart_send(void *ctx, const uint8_t *data, size_t len)
{
    (void)ctx;
    Serial2.write(data, len);
}

static void lock_calib(void *ctx)
{
    (void)ctx;
    xSemaphoreTake(calib_mutex, portMAX_DELAY);
}

static void unlock_calib(void *ctx)
{
    (void)ctx;
    xSemaphoreGive(calib_mutex);
}

/* Task da UART (nucleo 0): pedidos da S3 e telemetria a 20 Hz. */
static void hmi_task(void *arg)
{
    (void)arg;
    uint8_t buf[128];
    uint32_t last_rx = millis();
    uint32_t last_tlm = 0;
    for (;;) {
        const uint32_t now = millis();
        int n = Serial2.available();
        if (n > 0) {
            n = Serial2.readBytes(buf, n > (int)sizeof(buf) ? sizeof(buf) : (size_t)n);
            ecu_motor_link_feed(&link_hmi, buf, (size_t)n, rpm_now);
            last_rx = now;
        } else if (now - last_rx > ECU_IDLE_FLUSH_MS) {
            ecu_motor_link_line_idle(&link_hmi);
            last_rx = now;
        }
        if (now - last_tlm >= ECU_TELEMETRY_PERIOD_MS) {
            last_tlm = now;
            ecu_inputs_t in;
            ecu_outputs_t out;
            portENTER_CRITICAL(&snapshot_mux);
            in = inputs;
            out = outputs;
            portEXIT_CRITICAL(&snapshot_mux);
            ecu_motor_link_send_telemetry(&link_hmi, &in, &out, now);
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

static int read_pot(int pin)
{
    long sum = 0;
    for (int i = 0; i < 8; i++) sum += analogRead(pin);
    return (int)(sum / 8); /* 0..4095 */
}

static void read_bench_sensors(ecu_inputs_t *in)
{
    const int rpm_raw = read_pot(PIN_POT_RPM);
    /* abaixo de ~3 % do pot = motor parado (permite gravar e mexer na ficha) */
    in->rpm = rpm_raw < 120 ? 0 : (uint16_t)map(rpm_raw, 120, 4095, 250, 7000);
    in->map_kpa_x10 = (uint16_t)map(read_pot(PIN_POT_MAP), 0, 4095, 200, 1000);
    in->clt_x10 = (int16_t)map(read_pot(PIN_POT_CLT), 0, 4095, -100, 1100);
    in->tps_pct = (uint8_t)map(read_pot(PIN_POT_TPS), 0, 4095, 0, 100);
    in->iat_x10 = 250;    /* fixo na bancada */
    in->vbat_mv = 13800;  /* fixo na bancada */
    in->sync = in->rpm > 0;
}

static void print_serial(const ecu_inputs_t *in, const ecu_outputs_t *o)
{
    uint8_t order[8];
    uint16_t spacing = 0;
    const uint8_t n = ecu_firing_order((uint8_t)calib.param[ECU_PAR_CYLINDERS], order, &spacing);
    char seq[32] = "";
    int len = 0;
    for (uint8_t i = 0; i < n; i++) len += snprintf(seq + len, sizeof(seq) - len, i ? "-%u" : "%u", order[i]);
    Serial.printf("rpm=%u map=%u.%ukPa clt=%d.%dC tps=%u%% | comb[%u,%u]=%u.%02ums interp=%u.%02ums pw=%u.%03ums "
                  "| ign[%u,%u]=%d.%d avanco=%d.%d | ordem %s (%u graus)\n",
                  in->rpm, in->map_kpa_x10 / 10, in->map_kpa_x10 % 10, in->clt_x10 / 10, abs(in->clt_x10 % 10),
                  in->tps_pct, o->fuel_row, o->fuel_col, o->fuel_cell / 100, o->fuel_cell % 100,
                  o->fuel_interp_x100 / 100, o->fuel_interp_x100 % 100, o->pw_us / 1000, o->pw_us % 1000, o->ign_row,
                  o->ign_col, o->ign_cell / 10, abs(o->ign_cell % 10), o->advance_x10 / 10, abs(o->advance_x10 % 10),
                  seq, spacing);
}

void setup()
{
    Serial.begin(115200);
    Serial2.begin(ECU_UART_BAUD, SERIAL_8N1, PIN_HMI_RX, PIN_HMI_TX);
    analogReadResolution(12);
    prefs.begin("ecu", false);
    calib_mutex = xSemaphoreCreateMutex();

    storage.write = nvs_write;
    storage.read = nvs_read;
    storage.ctx = nullptr;
    ecu_motor_link_init(&link_hmi, &calib, &storage, uart_send, nullptr, ECU_NODE_MOTOR);
    link_hmi.lock = lock_calib;
    link_hmi.unlock = unlock_calib;
    ecu_engine_init(&engine_state);
    Serial.printf("ECU bancada: calibracao %s\n", link_hmi.calib_default ? "PADRAO (NVS vazia)" : "lida da NVS");

    xTaskCreatePinnedToCore(hmi_task, "hmi", 8192, nullptr, 2, nullptr, 0);
}

void loop()
{
    static uint32_t last_print = 0;
    ecu_inputs_t in;
    read_bench_sensors(&in);
    rpm_now = in.rpm;

    ecu_outputs_t out;
    xSemaphoreTake(calib_mutex, portMAX_DELAY);
    ecu_engine_compute(&calib, &in, &engine_state, millis(), &out);
    xSemaphoreGive(calib_mutex);

    portENTER_CRITICAL(&snapshot_mux);
    inputs = in;
    outputs = out;
    portEXIT_CRITICAL(&snapshot_mux);

    if (millis() - last_print >= 500) {
        last_print = millis();
        print_serial(&in, &out);
    }
    delay(10);
}
