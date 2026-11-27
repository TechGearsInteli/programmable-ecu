/*
 * Tela (HMI) da ECU no ESP32-S3 - tasks 1.7.1.2, 1.7.1.3 e 1.6.1.2 a 1.6.1.8.
 *
 * A S3 so mostra e pede: o calculo, os limites e a flash da calibracao ficam
 * no ESP do motor. Pela UART chegam a telemetria (20 Hz) e as respostas; daqui
 * saem os pedidos (ler/escrever celula, ficha, gravar, backup).
 *
 * Ligacao: so TX, RX e GND (3,3 V) com o ESP do motor. 12 V e GPIO de motor
 * nao entram nesta placa.
 */
#include <Arduino.h>
#include <LittleFS.h>

#include "board.h"
#include "ecu/ecu_hmi_link.h"
#include "hmi/ui.h"
#include "lvgl.h"

static ecu_hmi_link_t link_motor;
static hmi_platform_t platform;
static const char *BACKUP_FILE = "/backup.tgcal";

static uint32_t tick_ms(void)
{
    return millis();
}

static void uart_send(void *ctx, const uint8_t *data, size_t len)
{
    (void)ctx;
    Serial1.write(data, len);
}

/* Backup em arquivo na flash da propria S3 (LittleFS). */
static bool backup_write(void *ctx, const uint8_t *data, size_t len)
{
    (void)ctx;
    File f = LittleFS.open(BACKUP_FILE, "w");
    if (!f) return false;
    const bool ok = f.write(data, len) == len;
    f.close();
    return ok;
}

static size_t backup_read(void *ctx, uint8_t *buf, size_t cap)
{
    (void)ctx;
    File f = LittleFS.open(BACKUP_FILE, "r");
    if (!f) return 0;
    const size_t n = f.read(buf, cap);
    f.close();
    return n;
}

void setup()
{
    Serial.begin(115200);
    Serial1.begin(ECU_UART_BAUD, SERIAL_8N1, PIN_MOTOR_RX, PIN_MOTOR_TX);
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS indisponivel: backup em arquivo desligado");
    }

    lv_init();
    lv_tick_set_cb(tick_ms);
    if (!board_init()) {
        Serial.println("Falha ao iniciar a tela: confira os pinos em board.h");
    }

    platform.backup_write = backup_write;
    platform.backup_read = backup_read;
    platform.ctx = nullptr;
    platform.backup_name = BACKUP_FILE;
    ecu_hmi_link_init(&link_motor, uart_send, nullptr, hmi_ui_on_link_event, nullptr, millis());
    hmi_ui_init(&link_motor, &platform);
}

void loop()
{
    static uint32_t last_rx = 0;
    uint8_t buf[128];
    const uint32_t now = millis();
    int n = Serial1.available();
    if (n > 0) {
        n = Serial1.readBytes(buf, n > (int)sizeof(buf) ? sizeof(buf) : (size_t)n);
        ecu_hmi_link_feed(&link_motor, buf, (size_t)n, now);
        last_rx = now;
    } else if (now - last_rx > ECU_IDLE_FLUSH_MS) {
        ecu_hmi_link_line_idle(&link_motor);
        last_rx = now;
    }
    ecu_hmi_link_poll(&link_motor, now);
    lv_timer_handler(); /* a tela redesenha so o que mudou; o motor nao espera por ela */
    delay(2);
}
