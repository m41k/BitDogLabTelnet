#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/binary_info.h"
#include "inc/ssd1306.h"
#include "hardware/i2c.h"

#define TELNET_PORT 23
#define LED_PIN 12        // Pino do LED
#define BUZZER_PIN 10  // Pino do Buzzer
#define WIFI_SSID "SSID"  // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASS "SENHA" // Substitua pela senha da sua rede Wi-Fi

const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

//TELNET
static struct tcp_pcb *telnet_pcb;
static bool led_blinking = false; // Variável para controlar o estado do LED

static err_t telnet_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char buffer[128] = {0};
    pbuf_copy_partial(p, buffer, p->tot_len, 0);
    printf("Recebido: %s\n", buffer);

    // Remover espaços extras e nova linha
    buffer[strcspn(buffer, "\r\n")] = 0;

    // Verifica o comando
    if (strcmp(buffer, "help") == 0) {
        const char *help_msg = "---------------------------\n"
                               "Comandos disponíveis:\n"
                               "---------------------------\n"
                               "ip - Mostra o endereço IP\n"
                               "help - Mostra esta mensagem\n"
                               "beep - Emite um som do buzzer\n"
                               "exit - Fecha a conexão\n"
                               "piscar - Faz o LED piscar\n"
                               "]====run=====> ";
        tcp_write(tpcb, help_msg, strlen(help_msg), TCP_WRITE_FLAG_COPY);
    } else if (strcmp(buffer, "exit") == 0) {
        tcp_write(tpcb, "Fechando conexão...\n", 20, TCP_WRITE_FLAG_COPY);
        tcp_close(tpcb);
    } else if (strcmp(buffer, "piscar") == 0) {
        // Ativa ou desativa o piscar do LED
        led_blinking = !led_blinking;
        if (led_blinking) {
            tcp_write(tpcb, "LED piscando!\n", 14, TCP_WRITE_FLAG_COPY);
        } else {
            tcp_write(tpcb, "LED parado!\n", 12, TCP_WRITE_FLAG_COPY);
        }
    } else if (strcmp(buffer, "ip") == 0) {
        // Exibe o endereço IP
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
        char ip_msg[32];
        snprintf(ip_msg, sizeof(ip_msg), "Endereço IP: %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);
        tcp_write(tpcb, ip_msg, strlen(ip_msg), TCP_WRITE_FLAG_COPY);
    } else if (strcmp(buffer, "beep") == 0) {
        // Emite um beep no buzzer
        gpio_put(BUZZER_PIN, 1);  // Ativa o buzzer
        sleep_ms(200);             // Aguarda 200ms
        gpio_put(BUZZER_PIN, 0);  // Desativa o buzzer
        tcp_write(tpcb, "Beep emitido!\n", 14, TCP_WRITE_FLAG_COPY);
    } else {
        tcp_write(tpcb, "Digite 'help' para ajuda. \n", 43, TCP_WRITE_FLAG_COPY);
    }

    pbuf_free(p);
    return ERR_OK;
}

static err_t telnet_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    printf("Cliente conectado!\n");
    tcp_recv(newpcb, telnet_recv);
    return ERR_OK;
}

void start_telnet_server() {
    telnet_pcb = tcp_new();
    tcp_bind(telnet_pcb, IP_ADDR_ANY, TELNET_PORT);
    telnet_pcb = tcp_listen(telnet_pcb);
    tcp_accept(telnet_pcb, telnet_accept);
    //printf("Servidor Telnet rodando na porta %d\n", TELNET_PORT);
    printf("]====run=====> %d\n", TELNET_PORT);
}

void blink_led() {
    gpio_put(LED_PIN, 1);  // Acende o LED
    sleep_ms(500);          // Espera por 500ms
    gpio_put(LED_PIN, 0);  // Apaga o LED
    sleep_ms(500);          // Espera por mais 500ms
}
//-TELNET


int main() {
    stdio_init_all();  // Inicializa a saída padrão

    // Inicializa o Wi-Fi
    if (cyw43_arch_init()) {
        printf("Erro ao inicializar o Wi-Fi\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return 1;
    } else {
        printf("Connected.\n");
        // Ler o endereço IP
        uint8_t *ip_address = (uint8_t*)&(cyw43_state.netif[0].ip_addr.addr);
        printf("Endereço IP %d.%d.%d.%d\n", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);

        // Criar um buffer para armazenar o IP formatado como string
        char ip_str[16];
        sprintf(ip_str, "%d.%d.%d.%d", ip_address[0], ip_address[1], ip_address[2], ip_address[3]);

        // Inicializa o LED e o Buzzer
        gpio_init(LED_PIN);
        gpio_set_dir(LED_PIN, GPIO_OUT);  // Define o LED como saída
        gpio_init(BUZZER_PIN);
        gpio_set_dir(BUZZER_PIN, GPIO_OUT);  // Define o Buzzer como saída


        // DISPLAY - Inicializa I2C e OLED
        i2c_init(i2c1, ssd1306_i2c_clock * 1000);
        gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
        gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
        gpio_pull_up(I2C_SDA);
        gpio_pull_up(I2C_SCL);

        ssd1306_init();

        struct render_area frame_area = {
            .start_column = 0,
            .end_column = ssd1306_width - 1,
            .start_page = 0,
            .end_page = ssd1306_n_pages - 1
        };

        calculate_render_area_buffer_length(&frame_area);

        // Limpar o display
        uint8_t ssd[ssd1306_buffer_length];
        memset(ssd, 0, ssd1306_buffer_length);
        render_on_display(ssd, &frame_area);

        // Exibir o IP no display
        ssd1306_draw_string(ssd, 5, 8, "IP Address:");
        ssd1306_draw_string(ssd, 5, 16, ip_str);
        render_on_display(ssd, &frame_area);
    }

    printf("Wi-Fi conectado!\n");

    // Loop principal
    //while (true) {
    //    cyw43_arch_poll();  // Necessário para manter o Wi-Fi ativo
    //    sleep_ms(100);      // Reduz o uso da CPU
    //}

    start_telnet_server();

    // Loop principal
    while (true) {
        if (led_blinking) {
            blink_led();  // Pisca o LED se o comando "piscar" for dado
        } else {
            sleep_ms(1000);  // Aguarda sem fazer nada
        }
    }

    cyw43_arch_deinit();  // Desliga o Wi-Fi (não será chamado, pois o loop é infinito)
    return 0;
}
