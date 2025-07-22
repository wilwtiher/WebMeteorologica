#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "aht20.h"
#include "bmp280.h"
#include "ssd1306.h"
#include "font.h"
#include <math.h>
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include <string.h>
#include <stdlib.h>

#include "pico/bootrom.h"

// Definições de pinos para botões e LEDs
#define BOTAO_B 6
#define LED_AMARELO 4
#define LED_VERDE 9
#define LED_VERMELHO 8
#define BOTAO_A 5
#define BOTAO_JOY 22
#define POT_PIN 28
#define ADC_INPUT 2                 // ADC2 (GPIO28) para leitura do sensor de nível
#define VREF 3.3f                   // Tensão de referência do ADC (3.3V)
#define ADC_MAX 4095.0f             // Resolução do ADC de 12 bits
#define I2C_PORT i2c0               // i2c0 pinos 0 e 1, i2c1 pinos 2 e 3
#define I2C_SDA 0                   // 0 ou 2
#define I2C_SCL 1                   // 1 ou 3
#define SEA_LEVEL_PRESSURE 101325.0 // Pressão ao nível do mar em Pa
// Display na I2C
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define endereco 0x3C

// Configurações de Wi-Fi
#define WIFI_SSID "Jorge"
#define WIFI_PASS "0123456789"

int t_max = 30;
int u_max = 80;
int p_max = 100;
int t_min = 15;
int u_min = 30;
int p_min = 70;
bool certo = true;

int32_t temp;
float pres;
float hum;
double altitude;

// Função para calcular a altitude a partir da pressão atmosférica
double calculate_altitude(double pressure)
{
    return 44330.0 * (1.0 - pow(pressure / SEA_LEVEL_PRESSURE, 0.1903));
}

// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

// Página HTML para interface web
const char HTML_BODY[] =
"<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'>"
"<title>Sensores</title><style>"
"body{font-family:sans-serif;text-align:center;background:#f2f2f2;margin:0;padding:0;}"
".container{max-width:600px;margin:20px auto;background:white;padding:20px;border-radius:10px;box-shadow:0 0 10px rgba(0,0,0,0.1);}"
"canvas{width:100%;max-width:300px;height:150px;margin-bottom:10px;}"
"input{width:70px;padding:5px;margin:5px;border:1px solid #ccc;border-radius:5px;}"
"button{padding:10px 20px;background:#4CAF50;color:white;border:none;border-radius:5px;cursor:pointer;}"
"button:hover{background:#45a049;}"
"</style><script>"
"let temp=[],hum=[],pres=[];"
"function plot(id, data, maxVal){let c = document.getElementById(id), ctx = c.getContext('2d');ctx.clearRect(0, 0, c.width, c.height);let h = c.height;let escala = h / maxVal;ctx.beginPath();ctx.moveTo(0, h - data[0] * escala);for(let i = 1; i < data.length; i++) {ctx.lineTo(i * 10, h - data[i] * escala);}ctx.stroke();}"
"function atualizar(){fetch('/estado').then(r=>r.json()).then(d=>{"
"document.getElementById('temp').innerText=d.t.toFixed(1);"
"document.getElementById('hum').innerText=d.u.toFixed(1);"
"document.getElementById('pres').innerText=d.p.toFixed(1);"
"document.getElementById('tmin').value=d.tmin;document.getElementById('tmax').value=d.tmax;"
"document.getElementById('umin').value=d.umin;document.getElementById('umax').value=d.umax;"
"document.getElementById('pmin').value=d.pmin;document.getElementById('pmax').value=d.pmax;"
"temp.push(d.t);hum.push(d.u);pres.push(d.p);if(temp.length>30)temp.shift();if(hum.length>30)hum.shift();if(pres.length>30)pres.shift();"
"plot('g1',temp,40);plot('g2',hum,100);plot('g3',pres,1000);});}"
"function salvar(){let q='?tmin='+tmin.value+'&tmax='+tmax.value+'&umin='+umin.value+'&umax='+umax.value+'&pmin='+pmin.value+'&pmax='+pmax.value;"
"fetch('/set?' + q).then(r=>r.text()).then(a=>alert(a));}"
"setInterval(atualizar,2000);"
"</script></head><body><div class='container'>"
"<h2>Monitoramento de Sensores</h2>"
"<p>Temperatura: <span id='temp'>--</span>°C | Umidade: <span id='hum'>--</span>% | Pressão: <span id='pres'>--</span>Pa</p>"
"<canvas id='g1' width='300' height='150'></canvas>"
"<canvas id='g2' width='300' height='150'></canvas>"
"<canvas id='g3' width='300' height='150'></canvas>"
"<h3>Limites</h3>"
"Tmin:<input id='tmin' type='number'> Tmax:<input id='tmax' type='number'><br>"
"Umin:<input id='umin' type='number'> Umax:<input id='umax' type='number'><br>"
"Pmin:<input id='pmin' type='number'> Pmax:<input id='pmax' type='number'><br><br>"
"<button onclick='salvar()'>Salvar</button>"
"</div></body></html>";



// Estrutura para gerenciar respostas HTTP
struct http_state
{
    char response[8192]; // Buffer para resposta HTTP
    size_t len;          // Tamanho da resposta
    size_t sent;         // Quantidade de dados enviados
};

// Callback para envio de dados HTTP
static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len)
    {
        tcp_close(tpcb); // Fecha conexão após envio completo
        free(hs);
    }
    return ERR_OK;
}

// Manipula requisições HTTP recebidas
static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs)
    {
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    hs->sent = 0;

    if (strstr(req, "GET /estado"))
    {
        char json[256];
        int len = snprintf(json, sizeof(json),
                           "{\"t\":%.2f,\"u\":%.2f,\"p\":%.2f,\"tmin\":%d,\"tmax\":%d,\"umin\":%d,\"umax\":%d,\"pmin\":%d,\"pmax\":%d}",
                           temp / 100.0, hum, pres / 100.0, // <-- Divida pressão se necessário
                           t_min, t_max, u_min, u_max, p_min, p_max);

        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n\r\n%s",
                           len, json);
    }

    else if (strstr(req, "GET /set?"))
    {
        char *ptr;
        if ((ptr = strstr(req, "tmin=")))
            sscanf(ptr, "tmin=%d", &t_min);
        if ((ptr = strstr(req, "tmax=")))
            sscanf(ptr, "tmax=%d", &t_max);
        if ((ptr = strstr(req, "umin=")))
            sscanf(ptr, "umin=%d", &u_min);
        if ((ptr = strstr(req, "umax=")))
            sscanf(ptr, "umax=%d", &u_max);
        if ((ptr = strstr(req, "pmin=")))
            sscanf(ptr, "pmin=%d", &p_min);
        if ((ptr = strstr(req, "pmax=")))
            sscanf(ptr, "pmax=%d", &p_max);

        const char *msg = "Valores atualizados!";
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n\r\n%s",
                           (int)strlen(msg), msg);
    }

    else
    {
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n\r\n%s",
                           (int)strlen(HTML_BODY), HTML_BODY);
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);

    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);

    pbuf_free(p);
    return ERR_OK;
}

// Callback para novas conexões TCP
static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

// Inicia o servidor HTTP na porta 80
static void start_http_server(void)
{
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb)
    {
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}

int main()
{
    // Para ser utilizado o modo BOOTSEL com botão B
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    // Fim do trecho para modo BOOTSEL com botão B

    stdio_init_all();

    // I2C do Display funcionando em 400Khz.
    i2c_init(I2C_PORT_DISP, 400 * 1000);

    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA_DISP);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL_DISP);                                        // Pull up the clock line
    ssd1306_t ssd;                                                     // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT_DISP); // Inicializa o display
    ssd1306_config(&ssd);                                              // Configura o display
    ssd1306_send_data(&ssd);                                           // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    // Inicializa o I2C
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicializa o BMP280
    bmp280_init(I2C_PORT);
    struct bmp280_calib_param params;
    bmp280_get_calib_params(I2C_PORT, &params);

    // Inicializa o AHT20
    aht20_reset(I2C_PORT);
    aht20_init(I2C_PORT);

    // Estrutura para armazenar os dados do sensor
    AHT20_Data data;
    int32_t raw_temp_bmp;
    int32_t raw_pressure;

    char str_tmp1[5]; // Buffer para armazenar a string
    char str_alt[5];  // Buffer para armazenar a string
    char str_tmp2[5]; // Buffer para armazenar a string
    char str_umi[5];  // Buffer para armazenar a string

    // Inicializa Wi-Fi
    if (cyw43_arch_init())
    {
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi => FALHA", 0, 0);
        printf("WiFi => FALHA");
        ssd1306_send_data(&ssd);
        return 1;
    }

    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi => ERRO", 0, 0);
        printf("WiFi => ERRO");
        ssd1306_send_data(&ssd);
        return 1;
    }

    // Exibe endereço IP no display
    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    char ip_str[24];
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "WiFi => OK", 0, 0);
    printf(ip_str);
    ssd1306_draw_string(&ssd, ip_str, 0, 10);
    ssd1306_send_data(&ssd);

    start_http_server(); // Inicia servidor HTTP
    bool cor = true;
    while (1)
    {
        cyw43_arch_poll(); // Atualiza estado da rede Wi-Fi
        // Leitura do BMP280
        bmp280_read_raw(I2C_PORT, &raw_temp_bmp, &raw_pressure);
        temp = bmp280_convert_temp(raw_temp_bmp, &params);
        pres = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);

        // Cálculo da altitude
        altitude = calculate_altitude(pres);

        printf("Pressao = %.3f kPa\n", pres / 1000.0);
        printf("Temperatura BMP: = %.2f C\n", temp / 100.0);
        printf("Altitude estimada: %.2f m\n", altitude);
        printf("T Max: %.2f m\n", t_max);

        // Leitura do AHT20
        if (aht20_read(I2C_PORT, &data))
        {
            printf("Temperatura AHT: %.2f C\n", data.temperature);
            printf("Umidade: %.2f %%\n\n\n", data.humidity);
            hum = data.humidity;
        }
        else
        {
            printf("Erro na leitura do AHT10!\n\n\n");
        }
        if(t_max>temp>t_min && u_max>hum>u_min && p_max>pres>p_min){
            certo = true;
        }else{
            certo = false;
        }

        sprintf(str_tmp1, "%.1fC", temp / 100.0);     // Converte o inteiro em string
        sprintf(str_alt, "%.0fm", altitude);          // Converte o inteiro em string
        sprintf(str_tmp2, "%.1fC", data.temperature); // Converte o inteiro em string
        sprintf(str_umi, "%.1f%%", data.humidity);    // Converte o inteiro em string

        //  Atualiza o conteúdo do display com animações
        ssd1306_fill(&ssd, !cor);                           // Limpa o display
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);       // Desenha um retângulo
        ssd1306_line(&ssd, 3, 25, 123, 25, cor);            // Desenha uma linha
        ssd1306_line(&ssd, 3, 37, 123, 37, cor);            // Desenha uma linha
        ssd1306_draw_string(&ssd, "  Wilton Jr.", 8, 6);    // Desenha uma string
        ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16);   // Desenha uma string
        ssd1306_draw_string(&ssd, "BMP280  AHT10", 10, 28); // Desenha uma string
        ssd1306_line(&ssd, 63, 25, 63, 60, cor);            // Desenha uma linha vertical
        ssd1306_draw_string(&ssd, str_tmp1, 14, 41);        // Desenha uma string
        ssd1306_draw_string(&ssd, str_alt, 14, 52);         // Desenha uma string
        ssd1306_draw_string(&ssd, str_tmp2, 73, 41);        // Desenha uma string
        ssd1306_draw_string(&ssd, str_umi, 73, 52);         // Desenha uma string
        ssd1306_send_data(&ssd);                            // Atualiza o display

        sleep_ms(500);
    }
    cyw43_arch_deinit(); // Finaliza Wi-Fi
    return 0;
}