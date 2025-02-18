#include <stdio.h>
#include "pico/stdlib.h"
#include <math.h>
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/pwm.h"
#include "hardware/adc.h" // ADC library
//Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"

// Definições do display
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
// Definições de pinos leds e botões
#define led_red 13
#define led_blue 12
#define led_green 11
#define button_A 5
uint const button_B = 6;
//Definições de pinos ADC
#define JOYSTICK_X_PIN 27  // GPIO para eixo X
#define JOYSTICK_Y_PIN 26  // GPIO para eixo Y
#define JOYSTICK_PB 22 // GPIO para botão do Joystick
// Controle de tempo para debouncing
static volatile uint32_t last_time = 0; // Armazena o tempo do último evento (em microssegundos)
static bool green_led_state = false;   // estado do LED verde
static bool init_pwm_rgb = true; // Inicialização do PWM para os LEDs RGB
void gpio_irq_handler(uint gpio, uint32_t events);


// Configuração pwm
uint pwm_init_gpio(uint gpio, uint wrap){
    gpio_set_function(gpio, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_wrap(slice_num, wrap);

    pwm_set_enabled(slice_num, true);
    return slice_num;
}

//

int main()
{
    stdio_init_all();

    // inicialização dos leds
    gpio_init(led_red);
    gpio_set_dir(led_red, GPIO_OUT);
    gpio_init(led_blue);
    gpio_set_dir(led_blue, GPIO_OUT);
    gpio_init(led_green);
    gpio_set_dir(led_green, GPIO_OUT);
    // inicialização dos botões de interrupção
    gpio_init(button_A);
    gpio_set_dir(button_A, GPIO_IN);
    gpio_pull_up(button_A);
    //BOOTSEL com botão B
    gpio_init(button_B);
    gpio_set_dir(button_B, GPIO_IN);
    gpio_pull_up(button_B);

    gpio_set_irq_enabled_with_callback(button_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    // inicialização do ADC
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN);
    adc_gpio_init(JOYSTICK_Y_PIN);
    // Configuração do botão do Joystick
    adc_gpio_init(JOYSTICK_PB);
    gpio_init(JOYSTICK_PB);
    gpio_set_dir(JOYSTICK_PB, GPIO_IN);
    gpio_pull_up(JOYSTICK_PB);

    // Inicializa o I2C a 400kHz
    i2c_init(I2C_PORT, 400 * 1000);
    // Define os pinos SDA e SCL
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // GPIO 14
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // GPIO 15
    gpio_pull_up(I2C_SDA); // SDA é pull-up
    gpio_pull_up(I2C_SCL); // SCL é pull-up
    ssd1306_t ssd; // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display
    //Limpa o display. O display inicia com todos os pixels apagados
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    //Variáveis para armazenar os valores dos eixos X e Y
    uint16_t adc_value_x, adc_value_y;

    //PWM configuração
    uint pwm_wrap = 2045;
    uint pwm_slice_red = pwm_init_gpio(led_red, pwm_wrap);
    uint pwm_slice_blue = pwm_init_gpio(led_blue, pwm_wrap);

    // Interrupção do botão do Joystick
    gpio_set_irq_enabled_with_callback(JOYSTICK_PB, GPIO_IRQ_EDGE_RISE, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(button_A, GPIO_IRQ_EDGE_RISE, true, &gpio_irq_handler);


    bool cor = true;


    while (true) {
      
      adc_select_input(0); // Seleciona o ADC para eixo X. O pino 26 como entrada analógica
      adc_value_y = adc_read();
      adc_select_input(1); // Seleciona o ADC para eixo Y. O pino 27 como entrada analógica
      adc_value_x = adc_read();    
      //sprintf(str_x, "%d", adc_value_x);  // Converte o inteiro em string
      //sprintf(str_y, "%d", adc_value_y);  // Converte o inteiro em string
      printf("X: %d Y: %d\n", adc_value_x, adc_value_y); // Imprime os valores dos eixos X e Y
      if(init_pwm_rgb == true){
        // Ajuste correto do PWM sem valores negativos
            if (adc_value_x > 2048) {
              pwm_set_gpio_level(led_red, adc_value_x - 2047);
          } else {
              pwm_set_gpio_level(led_red, 2047 - adc_value_x);
          }

          if (adc_value_y > 2048) {
              pwm_set_gpio_level(led_blue, adc_value_y - 2047);
          } else {
              pwm_set_gpio_level(led_blue, 2047 - adc_value_y);
          }
        }
        if(init_pwm_rgb == false){
          pwm_set_gpio_level(led_red, 0);
          pwm_set_gpio_level(led_blue, 0);
        }
          // Mapeia o valor do ADC para as coordenadas do display
          // Como o quadrado tem 8 pixels, a posição máxima do canto superior-esquerdo será:
          // display_width - SQUARE_SIZE (128 - 8 = 120) e display_height - SQUARE_SIZE (64 - 8 = 56)
          uint8_t pos_x = (adc_value_x * (WIDTH - 8)) / 4095;
          //Logica por conta da inversão do eixo Y
          uint8_t pos_y = ((4095 - adc_value_y) * (HEIGHT - 8)) / 4095;

        // Limpa o buffer do display
        ssd1306_fill(&ssd, false);

        // Desenha o quadrado de 8x8 pixels na posição mapeada
        // Na função ssd1306_rect: o primeiro parâmetro é a coordenada Y (linha) e o segundo é a coordenada X (coluna)
        ssd1306_rect(&ssd, pos_y, pos_x, 8, 8, true, true);

        if(green_led_state == false){
          ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor); // Borda simples
        }
        if(green_led_state == true){
          ssd1306_rect(&ssd, 2, 2, 122, 58, cor, !cor); // Borda externa
          ssd1306_rect(&ssd, 4, 4, 118, 54, cor, !cor); // Borda interna
        }
        // Atualiza o display
        ssd1306_send_data(&ssd);

        sleep_ms(50); // Atualiza a ~20 fps
    }
}


//Trecho para modo BOOTSEL com botão B

void gpio_irq_handler(uint gpio, uint32_t events)
{
  // Obtém o tempo atual em microssegundos
  uint32_t current_time = to_us_since_boot(get_absolute_time());
  // Verifica se passou tempo suficiente desde o último evento
  if (current_time - last_time > 200000) // 200 ms de debouncing
  {
    last_time = current_time; // Atualiza o tempo do último evento
    //
    bool est = false;
      if(gpio == button_B){
        reset_usb_boot(0, 0);
      }
      if(gpio == JOYSTICK_PB){
        // 1) Inverte o LED verde
        green_led_state = !green_led_state;
        gpio_put(led_green, green_led_state);
        printf("Botão do Joystick pressionado\n");

      }
      if(gpio == button_A){
        printf("Botão A pressionado\n");
        init_pwm_rgb = !init_pwm_rgb;
      }
  }
}