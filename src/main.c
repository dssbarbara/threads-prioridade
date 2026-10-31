#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>

// --- Configuração de LEDs via DeviceTree ---
#define LED_A_NODE DT_ALIAS(led0)  // LED verde
#define LED_B_NODE DT_ALIAS(led2)  // LED vermelho

static const struct gpio_dt_spec ledA = GPIO_DT_SPEC_GET(LED_A_NODE, gpios);
static const struct gpio_dt_spec ledB = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);

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

// ----------------------------------------------------
// Função principal
// ----------------------------------------------------
void main(void)
{
    // Inicializa GPIOs dos LEDs
    if (!device_is_ready(ledA.port) || !device_is_ready(ledB.port)) {
        return;
    }
    gpio_pin_configure_dt(&ledA, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledB, GPIO_OUTPUT_INACTIVE);

    printk("=== Demonstração de Preempção com LEDs ===\n");
    printk("Thread A (vermelho): prioridade %d\n", PRIO_THREAD_A);
    printk("Thread B (verde): prioridade %d\n", PRIO_THREAD_B);

    while (1) {
        k_sleep(K_FOREVER); // Main dorme para liberar CPU.
    }
}
