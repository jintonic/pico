// Waveform digitization using Pi's ADC and DMA
#include <pico/stdlib.h>
#define n 16 // FIXME: the code only record 7 or 8 waveform samples currently
uint8_t waveform[n] = {0}, max, i, j;
uint8_t *pointer2wf = &waveform[0];

#include <hardware/adc.h>
#include <hardware/dma.h>
void daq_init()
{
    adc_init();
    adc_gpio_init(26);   // initialize GP26 as ADC input (ADC0)
    adc_select_input(0); // select ADC0
    adc_fifo_setup(
        true,  // Write each completed conversion to the sample FIFO
        true,  // Enable DMA data request (DREQ)
        1,     // DREQ (and IRQ) asserted when at least 1 sample present
        false, // We won't see the ERR bit because of 8 bit reads; disable.
        true   // Shift each sample to 8 bits when pushing to FIFO
    );
    adc_set_clkdiv(0); // fastest data taking

    dma_channel_config cfg0 = dma_channel_get_default_config(0);
    channel_config_set_transfer_data_size(&cfg0, DMA_SIZE_8);
    channel_config_set_read_increment(&cfg0, false);
    channel_config_set_write_increment(&cfg0, true);
    channel_config_set_dreq(&cfg0, DREQ_ADC);
    channel_config_set_chain_to(&cfg0, 1);
    dma_channel_configure(0, &cfg0,
                          waveform,      // Destination (memory)
                          &adc_hw->fifo, // Source address (ADC FIFO)
                          n,             // Number of transfers
                          false          // Don't start immediately
    );

    dma_channel_config cfg1 = dma_channel_get_default_config(1);
    channel_config_set_transfer_data_size(&cfg1, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg1, false);
    channel_config_set_write_increment(&cfg1, false);
    channel_config_set_chain_to(&cfg1, 0);
    dma_channel_configure(1, &cfg1,
                          &dma_hw->ch[0].write_addr, // Destination
                          &pointer2wf,               // Source
                          1, false);
}

// SPI connection to an SD card
#include <hw_config.h>
static spi_t spis[] = {{
    .hw_inst = spi0,
    .miso_gpio = 12, // GPIO number (not pin number)
    .mosi_gpio = 15,
    .sck_gpio = 14,
    .baud_rate = 12500 * 1000,
}};
static sd_card_t sd_cards[] = {{
    .pcName = "0:",  // Name used to mount device
    .spi = &spis[0], // Pointer to the SPI driving this card
    .ss_gpio = 13,   // The SPI slave select GPIO for this SD card
    .use_card_detect = false,
}};
size_t sd_get_num() { return count_of(sd_cards); }
sd_card_t *sd_get_by_num(size_t num)
{
    if (num <= sd_get_num()) return &sd_cards[num];
    else return NULL;
}
size_t spi_get_num() { return count_of(spis); }
spi_t *spi_get_by_num(size_t num)
{
    if (num <= sd_get_num()) return &spis[num];
    else return NULL;
}
FIL file; // Global file object for writing to SD card
#include <rtc.h>
#include <f_util.h>
void sd_card_init()
{
    time_init(); // Initialize time for FatFs
    FRESULT fr = f_mount(&(sd_cards[0].fatfs), sd_cards[0].pcName, 1);
    if (fr != FR_OK) panic("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    const char *const filename = "data.txt";
    fr = f_open(&file, filename, FA_OPEN_APPEND | FA_WRITE);
    if (fr != FR_OK && fr != FR_EXIST)
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
}
 
// OLED display
#include <ssd1306.h>
void oled_init(ssd1306_t *display)
{
    i2c_init(i2c0, 400000);
    gpio_set_function(16, GPIO_FUNC_I2C); gpio_pull_up(16);
    gpio_set_function(17, GPIO_FUNC_I2C); gpio_pull_up(17);
    ssd1306_init(display, 128, 64, 0x3C, i2c0);
}

#include <stdio.h>
int main()
{
    stdio_init_all();

    daq_init();
    adc_run(true);
    dma_channel_start(0);

    gpio_init(22); // output of comparator
    gpio_set_dir(22, GPIO_IN);

    gpio_init(0); // buzzer
    gpio_set_dir(0, GPIO_OUT);

    gpio_init(PICO_DEFAULT_LED_PIN); // LED on Pico board
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    ssd1306_t oled;
    oled_init(&oled);
    char words[20] = "Welcome!";
    ssd1306_draw_string(&oled, 8, 24, 2, words); ssd1306_show(&oled);

    sd_card_init();
    if (f_printf(&file, "Hello, world!\n") < 0) sprintf(words, "f_printf failed\n");
    else sprintf(words, "Data logging started\n");
    ssd1306_clear(&oled);
    ssd1306_draw_string(&oled, 8, 24, 2, words); ssd1306_show(&oled);

    for (i=0; i<5000; i++) {
        if (gpio_get(22) == 0) continue;
        adc_run(false);    // stop ADC temporarily to avoid overwriting waveform
        gpio_put(0, true); // turn on buzzer
        gpio_put(PICO_DEFAULT_LED_PIN, true);

        max = 0;
        for (j = 0; j < n; j++)
            if (waveform[j] > max) max = waveform[j];
        printf("%hhu\n", max);

        sprintf(words, "%hhu\n", max);
        ssd1306_clear(&oled);
        ssd1306_draw_string(&oled, 8, 24, 2, words); ssd1306_show(&oled);

        adc_run(true);                         // restart ADC after waveform analysis
        gpio_put(0, false);                    // turn off buzzer
        gpio_put(PICO_DEFAULT_LED_PIN, false); // turn off LED
    }

    FRESULT fr = f_close(&file);
    if (fr != FR_OK) sprintf(words, "f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    ssd1306_clear(&oled);
    ssd1306_draw_string(&oled, 8, 24, 2, words); ssd1306_show(&oled);
    f_unmount(sd_cards[0].pcName);
    ssd1306_clear(&oled);
    ssd1306_draw_string(&oled, 8, 24, 2, "DAQ stopped"); ssd1306_show(&oled);

    return 0;
}
