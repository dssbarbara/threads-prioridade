#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>

// --- Configuração de LEDs via DeviceTree ---
#define LED_A_NODE DT_ALIAS(led0)  // LED verde
#define LED_B_NODE DT_ALIAS(led2)  // LED vermelho
#define LED_C_NODE DT_ALIAS(led1)

static const struct gpio_dt_spec ledA = GPIO_DT_SPEC_GET(LED_A_NODE, gpios);
static const struct gpio_dt_spec ledB = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);
static const struct gpio_dt_spec ledC = GPIO_DT_SPEC_GET(LED_C_NODE, gpios);

static K_SEM_DEFINE(isr_sem, 0, 1);
static volatile uint32_t isr_t_ms;
static volatile k_tid_t  isr_interrompida;

// --- Prioridades e tempos ---
#define PRIO_THREAD_A 5   // Maior prioridade (número menor)
#define PRIO_THREAD_B 7   // Menor prioridade
#define TEMPO_A_MS   1500   // Thread A dorme
#define TEMPO_B_MS   1000   // Thread B dorme


void thread_A(void *p1, void *p2, void *p3)
{
    while (1) {
        int64_t  t_ini = k_uptime_get();     // instante de início (ms)
        uint32_t c_ini = k_cycle_get_32();   // início em ciclos

        gpio_pin_set_dt(&ledA, 1);
        for (volatile int i = 0; i < 200000; i++) {}
        gpio_pin_set_dt(&ledA, 0);

        uint32_t dc = k_cycle_get_32() - c_ini;
        // imprime DEPOIS de medir, para o printk não entrar na medição
        printk("[A] inicio=%lld ms | duracao=%u us\n",
               t_ini, k_cyc_to_us_near32(dc));

        k_msleep(TEMPO_A_MS);
    }
}

void thread_B(void *p1, void *p2, void *p3)
{
    while (1) {
        int64_t  t_ini = k_uptime_get();
        uint32_t c_ini = k_cycle_get_32();

        gpio_pin_set_dt(&ledB, 1);
        for (volatile int i = 0; i < 10000000; i++) {
            if (i % 100000 == 0) { }
        }
        gpio_pin_set_dt(&ledB, 0);

        int64_t t_fim = k_uptime_get();
        uint32_t dc = k_cycle_get_32() - c_ini;
        printk("[B] inicio=%lld ms | fim=%lld ms | duracao=%u us\n",
               t_ini, t_fim, k_cyc_to_us_near32(dc));

        k_msleep(TEMPO_B_MS);
    }
}

// ----------------------------------------------------
// Definição das threads
// ----------------------------------------------------
K_THREAD_DEFINE(a_tid, 512, thread_A, NULL, NULL, NULL,
                PRIO_THREAD_A, 0, 0);
K_THREAD_DEFINE(b_tid, 512, thread_B, NULL, NULL, NULL,
                PRIO_THREAD_B, 0, 0);


void timer_isr(struct k_timer *t)
{
    isr_t_ms = k_uptime_get_32();
    isr_interrompida = k_current_get();   // thread que estava rodando
    gpio_pin_toggle_dt(&ledC);
    k_sem_give(&isr_sem);
}
K_TIMER_DEFINE(isr_timer, timer_isr, NULL);

int main(void)
{
    if (!device_is_ready(ledA.port) || !device_is_ready(ledB.port) ||
        !gpio_is_ready_dt(&ledC)) {
        return 0;
    }
    gpio_pin_configure_dt(&ledA, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledB, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledC, GPIO_OUTPUT_INACTIVE);

    // 2300 ms não é múltiplo dos períodos de A e B, então a ISR
    // cai em momentos diferentes a cada disparo
    k_timer_start(&isr_timer, K_MSEC(2300), K_MSEC(2300));

    printk("=== ISR periodica (timer a cada 2300 ms) ===\n");

    while (1) {
        k_sem_take(&isr_sem, K_FOREVER);
        const char *quem =
            (isr_interrompida == a_tid) ? "thread A (prio 5)" :
            (isr_interrompida == b_tid) ? "thread B (prio 7)" :
                                          "ociosa (idle)";
        printk("[ISR] t=%u ms | interrompeu: %s\n", isr_t_ms, quem);
    }
}