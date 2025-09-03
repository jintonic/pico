#include <pico/stdlib.h>
#define n 16 // FIXME: the code only record 7 or 8 waveform samples currently
uint8_t waveform[n] = {0}, max, i = 0, smpl_ch, ctrl_ch, comparator = 28;
uint8_t *pointer2wf = &waveform[0];

#include <hardware/adc.h>
#include <hardware/dma.h>
void daq_init() // Waveform digitization using Pi's ADC and DMA
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

    smpl_ch = dma_claim_unused_channel(true); // avoid conflict with SD DMA usage
    ctrl_ch = dma_claim_unused_channel(true);

    dma_channel_config smpl_cfg = dma_channel_get_default_config(smpl_ch);
    channel_config_set_transfer_data_size(&smpl_cfg, DMA_SIZE_8);
    channel_config_set_read_increment(&smpl_cfg, false);
    channel_config_set_write_increment(&smpl_cfg, true);
    channel_config_set_dreq(&smpl_cfg, DREQ_ADC);
    channel_config_set_chain_to(&smpl_cfg, ctrl_ch);
    dma_channel_configure(smpl_ch, &smpl_cfg,
                          waveform,      // Destination (memory)
                          &adc_hw->fifo, // Source address (ADC FIFO)
                          n,             // Number of transfers
                          false          // Don't start immediately
    );

    dma_channel_config ctrl_cfg = dma_channel_get_default_config(ctrl_ch);
    channel_config_set_transfer_data_size(&ctrl_cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&ctrl_cfg, false);
    channel_config_set_write_increment(&ctrl_cfg, false);
    channel_config_set_chain_to(&ctrl_cfg, smpl_ch);
    dma_channel_configure(ctrl_ch, &ctrl_cfg,
                          &dma_hw->ch[smpl_ch].write_addr, // Destination
                          &pointer2wf,                     // Source
                          1, false);
}

#include <ssd1306.h>
ssd1306_t oled; // OLED display
void oled_init()
{
    i2c_init(i2c0, 400000);
    gpio_set_function(16, GPIO_FUNC_I2C);
    gpio_pull_up(16);
    gpio_set_function(17, GPIO_FUNC_I2C);
    gpio_pull_up(17);
    oled.external_vcc = false;                // despite OLED is powered by Pico
    ssd1306_init(&oled, 128, 64, 0x3C, i2c0); // must be after previous line
}

#include <hw_config.h> // declaring spi_t, sd_card_t & the following 4 functions
static spi_t spis[] = {{
    .hw_inst = spi1,
    .miso_gpio = 12, // GPIO number (not pin number)
    .mosi_gpio = 15,
    .sck_gpio = 14,
    .baud_rate = 12500 * 1000,
}};
static sd_card_t sd_cards[] = {{
    .pcName = "0:",  // name used to mount device
    .spi = &spis[0], // pointer to the SPI driving this card
    .ss_gpio = 13,   // SPI slave select GPIO for this SD card
    .use_card_detect = false,
}};
size_t sd_get_num() { return count_of(sd_cards); }
sd_card_t *sd_get_by_num(size_t num)
{
    if (num <= sd_get_num())
        return &sd_cards[num];
    else
        return NULL;
}
size_t spi_get_num() { return count_of(spis); }
spi_t *spi_get_by_num(size_t num)
{
    if (num <= sd_get_num())
        return &spis[num];
    else
        return NULL;
}

#include <rtc.h>    // provide time_init()
#include <stdio.h>  // provide sprintf()
#include <f_util.h> // provide f_...()
FIL file;           // file object
DIR dir;            // directory object
FILINFO fo;         // file information object
char filename[12], msg[100];
void sd_card_init()
{
    time_init();
    f_mount(&sd_cards[0].fatfs, sd_cards[0].pcName, 1);
    f_opendir(&dir, "0:/");
    while (1)
    {
        FRESULT fr = f_readdir(&dir, &fo); // read an item in directory
        if (fr != FR_OK || fo.fname[0] == 0)
            break; // error or end of directory
        if (fo.fname[0] == 'r' & fo.fname[1] == 'u' & fo.fname[2] == 'n')
            i++; // count files named like run*.csv
    }
    f_closedir(&dir);
    sprintf(filename, "run%03u.csv", i);
    f_open(&file, filename, FA_CREATE_NEW | FA_WRITE);
}

int main()
{
    stdio_init_all();

    gpio_init(0); // buzzer using GPIO0
    gpio_set_dir(0, GPIO_OUT);

    gpio_init(PICO_DEFAULT_LED_PIN); // LED on Pico board
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    oled_init(&oled);     // OLED display using I2C
    ssd1306_clear(&oled); // its impact is visible only after ssd1306_show()
    ssd1306_draw_string(&oled, 0, 0, 2, "open file");
    ssd1306_show(&oled);

    sd_card_init(); // SD card using DMA and SPI
    if (f_printf(&file, "# ms, \t height\n") < 0)
        ssd1306_draw_string(&oled, 0, 24, 2, "failed!");
    else
        ssd1306_draw_string(&oled, 0, 24, 2, filename);
    ssd1306_show(&oled);

    daq_init();                 // DAQ using ADC and DMA on Pico board
    adc_run(true);              // start ADC
    dma_channel_start(smpl_ch); // start DMA

    gpio_init(comparator); // trigger signal from comparator
    gpio_set_dir(comparator, GPIO_IN);

    uint64_t ms;        // time in milliseconds since program start
    uint32_t nevts = 0; // count of recorded events
    while (true)
    {
        if (gpio_get(comparator) == 0) // if no signal from comparator
            continue;                  // check again

        adc_run(false);                       // stop to avoid overwriting wf
        gpio_put(0, true);                    // turn on buzzer
        gpio_put(PICO_DEFAULT_LED_PIN, true); // turn on LED

        max = 0; // max ADC value in this waveform
        for (i = 0; i < n; i++)
            if (waveform[i] > max)
                max = waveform[i];
        ms = to_ms_since_boot(get_absolute_time());
        sprintf(msg, "%llu, \t %hhu\n", ms, max);
        f_printf(&file, msg);

        if (nevts % 10 == 0) // update OLED every 10 events
        {
            ssd1306_clear(&oled);
            ssd1306_draw_string(&oled, 0, 0, 2, filename);
            sprintf(msg, "%u evts", nevts + 1);
            ssd1306_draw_string(&oled, 0, 24, 2, msg);
            sprintf(msg, "in %u s", ms / 1000);
            ssd1306_draw_string(&oled, 0, 49, 2, msg);
            ssd1306_show(&oled);
        }

        adc_run(true);                         // restart after wf analysis
        gpio_put(0, false);                    // turn off buzzer
        gpio_put(PICO_DEFAULT_LED_PIN, false); // turn off LED
        nevts++;
        if (nevts >= 2000)
            break;
    }
    adc_run(false); // stop ADC
    f_close(&file);
    f_unmount(sd_cards[0].pcName);

    ssd1306_clear(&oled);
    sprintf(msg, "%d events", nevts);
    ssd1306_draw_string(&oled, 0, 0, 2, msg);
    ssd1306_draw_string(&oled, 0, 24, 2, filename);
    sprintf(msg, "%.2f Hz", (float)nevts / ms * 1000);
    ssd1306_draw_string(&oled, 0, 49, 2, msg);
    ssd1306_show(&oled);

    return 0;
}
