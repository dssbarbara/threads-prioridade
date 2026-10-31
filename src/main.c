#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(prio_demo, LOG_LEVEL_DBG);

#define LED_A_NODE DT_ALIAS(led0)
#define LED_B_NODE DT_ALIAS(led2)
#define LED_C_NODE DT_ALIAS(led1)

static const struct gpio_dt_spec ledA = GPIO_DT_SPEC_GET(LED_A_NODE, gpios);
static const struct gpio_dt_spec ledB = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);
static const struct gpio_dt_spec ledC = GPIO_DT_SPEC_GET(LED_C_NODE, gpios);

#define PRIO_THREAD_A 5
#define PRIO_THREAD_B 7
#define TEMPO_A_MS    1500
#define TEMPO_B_MS    1000
#define TOTAL_B       10000000
#define PASSO_B       2500000      // 4 marcos: 25%, 50%, 75%, 100%

void thread_A(void *p1, void *p2, void *p3)
{
    while (1) {
        uint32_t c_ini = k_cycle_get_32();

        gpio_pin_set_dt(&ledA, 1);
        for (volatile int i = 0; i < 200000; i++) {}
        gpio_pin_set_dt(&ledA, 0);

        uint32_t dc = k_cycle_get_32() - c_ini;
        LOG_INF("A: trabalho=%u us", k_cyc_to_us_near32(dc));
        LOG_DBG("A: dormindo %d ms", TEMPO_A_MS);

        k_msleep(TEMPO_A_MS);
    }
}

void thread_B(void *p1, void *p2, void *p3)
{
    while (1) {
        uint32_t c_ini = k_cycle_get_32();

        gpio_pin_set_dt(&ledB, 1);
        for (volatile int i = 0; i < TOTAL_B; i++) {
            int n = i;
            if (n > 0 && (n % PASSO_B) == 0) {
                uint32_t parcial = k_cyc_to_ms_near32(k_cycle_get_32() - c_ini);
                LOG_DBG("B: %d%% | parcial=%u ms", n * 100 / TOTAL_B, parcial);
            }
        }
        gpio_pin_set_dt(&ledB, 0);

        uint32_t total = k_cyc_to_ms_near32(k_cycle_get_32() - c_ini);
        LOG_INF("B: trabalho total=%u ms", total);

        k_msleep(TEMPO_B_MS);
    }
}

K_THREAD_DEFINE(a_tid, 1024, thread_A, NULL, NULL, NULL, PRIO_THREAD_A, 0, 0);
K_THREAD_DEFINE(b_tid, 1024, thread_B, NULL, NULL, NULL, PRIO_THREAD_B, 0, 0);

// Callback do timer = contexto de interrupção. Fica depois dos K_THREAD_DEFINE
// porque usa a_tid e b_tid.
void timer_isr(struct k_timer *t)
{
    k_tid_t atual = k_current_get();
    const char *quem = (atual == a_tid) ? "A (prio 5)" :
                       (atual == b_tid) ? "B (prio 7)" : "idle";

    gpio_pin_toggle_dt(&ledC);
    LOG_WRN("ISR do timer interrompeu: %s", quem);
}
K_TIMER_DEFINE(isr_timer, timer_isr, NULL);

int main(void)
{
    if (!device_is_ready(ledA.port) || !device_is_ready(ledB.port) ||
        !gpio_is_ready_dt(&ledC)) {
        LOG_ERR("GPIO dos LEDs nao esta pronto");
        return 0;
    }
    gpio_pin_configure_dt(&ledA, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledB, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledC, GPIO_OUTPUT_INACTIVE);

    LOG_INF("Preempcao: A prio=%d (%d ms), B prio=%d (%d ms)",
            PRIO_THREAD_A, TEMPO_A_MS, PRIO_THREAD_B, TEMPO_B_MS);

    k_timer_start(&isr_timer, K_MSEC(2300), K_MSEC(2300));
    return 0;
}