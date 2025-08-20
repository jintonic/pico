#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#define n 16 // FIXME: the code only record 7 or 8 waveform samples currently
uint8_t waveform[n] = {0}, max, i;
uint8_t *pointer2wf = &waveform[0];

int main()
{
   stdio_init_all();

   adc_init();
   adc_gpio_init(26); // initialize GP26 as ADC input (ADC0)
   adc_select_input(0); // select ADC0
   adc_fifo_setup(
       true, // Write each completed conversion to the sample FIFO
       true, // Enable DMA data request (DREQ)
       1,    // DREQ (and IRQ) asserted when at least 1 sample present
       false,// We won't see the ERR bit because of 8 bit reads; disable.
       true  // Shift each sample to 8 bits when pushing to FIFO
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
   channel_config_set_write_increment(&cfg1,false);
   channel_config_set_chain_to(&cfg1, 0);
   dma_channel_configure(1, &cfg1,
       &dma_hw->ch[0].write_addr, // Destination
       &pointer2wf,               // Source
       1, false
   );

   adc_run(true);
   dma_channel_start(0);

   gpio_init(16); // GP16: output of comparator
   gpio_set_dir(16, GPIO_IN);
   gpio_init(PICO_DEFAULT_LED_PIN);
   gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

   while (true) {
       if (gpio_get(16)==0) continue;
       adc_run(false); // stop ADC temporarily to avoid overwriting waveform
       gpio_put(PICO_DEFAULT_LED_PIN, true);

       max=0;
       for (i=0; i<n; i++) if (waveform[i]>max) max=waveform[i];
       printf("%hhu\n", max);
           
       adc_run(true); // restart ADC after waveform analysis
       gpio_put(PICO_DEFAULT_LED_PIN, false);
   }

   return 0;
}
