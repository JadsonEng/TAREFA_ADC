#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "inc/ssd1306.h"
#include "inc/font.h"

// Configurando LEDs e Botões
#define LED_G 11
#define LED_B 12
#define LED_R 13
#define BTN_A 5
#define BTN_JOY 22

// Configurando Joystick
#define EIXO_Y 26    // ADC0
#define EIXO_X 27    // ADC1
#define PWM_WRAP 4095

// Configurando I2C
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15

// Configurando Display
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define SQUARE_SIZE 8

// Configurando posições do quadrado no display
int pos_x = (DISPLAY_WIDTH - SQUARE_SIZE) / 2;
int pos_y = (DISPLAY_HEIGHT - SQUARE_SIZE) / 2;
const int SPEED = 2;
const int MAX_X = DISPLAY_WIDTH - SQUARE_SIZE;
const int MAX_Y = DISPLAY_HEIGHT - SQUARE_SIZE;

// Declarando variáveis globais
volatile bool pwm_on = true;
volatile bool borda = false;
volatile bool led_g_estado = false;
bool cor = true;
absolute_time_t last_interrupt_time = 0;


// Protótipos de funções
void gpio_callback(uint gpio, uint32_t events);
void JOYSTICK(uint slice1, uint slice2);


int main()
{
    stdio_init_all();

    // Iniciando e configurando os LEDs
    gpio_set_function(LED_B, GPIO_FUNC_PWM);
    gpio_set_function(LED_R, GPIO_FUNC_PWM);
    gpio_init(LED_G);
    gpio_set_dir(LED_G, GPIO_OUT);
    gpio_put(LED_G, 0);

    // Iniciando e configurando os botões
    gpio_init(BTN_A);
    gpio_init(BTN_JOY);
    gpio_set_dir(BTN_A, GPIO_IN);
    gpio_set_dir(BTN_JOY, GPIO_IN);
    gpio_pull_up(BTN_A);
    gpio_pull_up(BTN_JOY);

    //  Habilitando Interrupção
    gpio_set_irq_enabled(BTN_A, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BTN_JOY, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_callback(gpio_callback);
    irq_set_enabled(IO_IRQ_BANK0, true);
    
    //  Iniciando I2C
    i2c_init(I2C_PORT, 400*5000);    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    //  Iniciando ADC
    adc_init();
    adc_gpio_init(EIXO_Y);
    adc_gpio_init(EIXO_X);

    //  Iniciando PWM
    uint slice1 = pwm_gpio_to_slice_num(LED_B);
    uint slice2 = pwm_gpio_to_slice_num(LED_R);
    pwm_set_wrap(slice1, PWM_WRAP);
    pwm_set_wrap(slice2, PWM_WRAP);
    pwm_set_clkdiv(slice1, 2.0f);
    pwm_set_clkdiv(slice2, 2.0f);
    pwm_set_enabled(slice1, true);
    pwm_set_enabled(slice2, true);

    // Iniciando e configurando o Display
    ssd1306_t ssd;
    ssd1306_init(&ssd, DISPLAY_WIDTH, DISPLAY_HEIGHT, false, 0x3c, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
 
    while (true) {
        JOYSTICK(slice1, slice2);    // Lê os eixos do Joystick
        cor = !cor;

        // Limpa e redesenha o display
        ssd1306_fill(&ssd, false);
        if (borda){
            ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);  // borda variável
        }
        else{
            ssd1306_rect(&ssd, 3, 3, 122, 58, true, false);  // borda fixa
        }
        ssd1306_rect(&ssd, pos_y, pos_x, SQUARE_SIZE, SQUARE_SIZE, true, true);  // Quadrado
        ssd1306_send_data(&ssd);    
        sleep_ms(30);
    }
}

// Função de Callback
void gpio_callback(uint gpio, uint32_t events) {
    absolute_time_t now = get_absolute_time();
    int64_t diff = absolute_time_diff_us(last_interrupt_time, now);

    if (diff < 250000) return;
    last_interrupt_time = now;

    if (gpio == BTN_A) {
        pwm_on = !pwm_on;

    } else if (gpio == BTN_JOY) {
        led_g_estado = !led_g_estado;
        gpio_put(LED_G, led_g_estado);
        borda = !borda;
    }
}

// Função do Joystick
void JOYSTICK(uint slice1, uint slice2) {
    const uint16_t CENTER = 2047;
    const uint16_t DEADZONE = 170;  // zona morta do Joystick

    // Lê o eixo Y (ADC0)
    adc_select_input(0);
    uint16_t y_value = adc_read();
    
    // Lê o eixo X (ADC1)
    adc_select_input(1);
    uint16_t x_value = adc_read();
    
    int16_t x_diff = (int16_t)x_value - CENTER;
    int16_t y_diff = (int16_t)y_value - CENTER;

    // Corrigindo o movimento no eixo X
    if (abs(x_diff) > DEADZONE) {
        pos_x += (x_diff > 0) ? SPEED : -SPEED;
        pos_x = (pos_x < 0) ? 0 : (pos_x > MAX_X) ? MAX_X : pos_x;
    }
    
    // Corrigindo o movimento no eixo Y
    if (abs(y_diff) > DEADZONE) {
        pos_y += (y_diff > 0) ? -SPEED : SPEED;  
        pos_y = (pos_y < 0) ? 0 : (pos_y > MAX_Y) ? MAX_Y : pos_y;
    }

    // Verificação do pwm em relação a deadzone
    uint16_t pwm_y = (abs(y_diff) <= DEADZONE) ? 0 : abs(y_diff) * 2;
    uint16_t pwm_x = (abs(x_diff) <= DEADZONE) ? 0 : abs(x_diff) * 2;

    if (pwm_on) {
        pwm_set_gpio_level(LED_B, pwm_y);
        pwm_set_gpio_level(LED_R, pwm_x);
    }
}