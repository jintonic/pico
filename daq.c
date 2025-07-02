#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include <stdio.h>

// DMA Configuration
#define ADC_DMA_CHAN            0
#define ADC_NUM_CHANNELS        1
#define ADC_CHANNEL_0           0
#define ADC_VREF                3.3f
#define ADC_RANGE               (1 << 12) // 12-bit ADC

// Ring Buffer Configuration
#define RING_BUFFER_SIZE_WORDS  256 // Number of 16-bit words in the buffer
uint16_t adc_ring_buffer[RING_BUFFER_SIZE_WORDS];

// DMA Control Block
dma_channel_config dma_config;

// --- Function Prototypes ---
void adc_dma_handler();

int main() {
    stdio_init_all();

    // 1. Initialize ADC
    adc_init();
    adc_gpio_init(26); // Initialize GP26 as ADC input (ADC0)
    adc_select_input(ADC_CHANNEL_0); // Select ADC0

    // Set up ADC for continuous sampling (free-running mode)
    // The ADC needs to be triggered. In this case, we're using
    // a timer to trigger it, or if you want max speed, rely on
    // a PIO state machine to trigger conversions. For simplicity,
    // we'll assume a continuous conversion is happening, and the
    // DMA will just pull data when available. For real world,
    // consider `adc_set_clkdiv` and a PIO state machine for precise
    // triggering. For this example, we'll rely on the ADC
    // continuously updating its result register after a
    // `adc_run(true)`

    // 2. Configure DMA Channel
    dma_config = dma_channel_get_default_config(ADC_DMA_CHAN);

    // Set transfer data size to 16 bits (ADC result is 12-bit, but reads as 16-bit)
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_16);

    // Increment destination address (the ring buffer), but not source (ADC result register)
    channel_config_set_read_increment(&dma_config, false);
    channel_config_set_write_increment(&dma_config, true);

    // DREQ (DMA Request) from ADC
    channel_config_set_dreq(&dma_config, DREQ_ADC);

    // Chain to itself (ring buffer) - this is crucial for continuous operation
    channel_config_set_chain_to(&dma_config, ADC_DMA_CHAN);

    // Enable DMA IRQ on completion (optional, but good for signaling when buffer is full/half full)
    channel_config_set_sniff_enable(&dma_config, true);

    // 3. Initialize and Start DMA Transfer
    dma_channel_configure(
        ADC_DMA_CHAN,
        &dma_config,
        adc_ring_buffer,            // Destination address
        &adc_hw->result,            // Source address (ADC result register)
        RING_BUFFER_SIZE_WORDS,     // Number of transfers
        true                        // Start immediately
    );

    // 4. Set up DMA Interrupt Handler (optional, but highly recommended)
    irq_set_exclusive_handler(DMA_IRQ_0, adc_dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // Start ADC conversions (this initiates the ADC to start putting data into its result register)
    adc_run(true);

    printf("ADC DMA Ring Buffer Example\n");
    printf("Reading from ADC0 (GP26). Data stored in ring buffer.\n");

    // Main loop: continuously read from the ring buffer
    // In a real application, you'd process data here,
    // perhaps average it, send it over UART/USB, etc.
    while (1) {
        // You can check dma_channel_is_busy(ADC_DMA_CHAN) to see if DMA is active.
        // The current read pointer for the DMA is not directly exposed for the ring
        // buffer in the SDK. Instead, you'll generally know how much data has been
        // transferred by comparing the `dma_channel_hw_addr(ADC_DMA_CHAN)->write_addr`
        // with the `adc_ring_buffer` start.

        // For simplicity here, we'll just print out a few samples periodically.
        // In a real application, you'd manage your own read pointer and read
        // data that has been written by the DMA.
        static uint32_t last_print_time = 0;
        if (to_ms_since_boot(get_absolute_time()) - last_print_time > 1000) {
            last_print_time = to_ms_since_boot(get_absolute_time());

            // Example of reading a few samples from the buffer
            // In a real application, you'd want to track your own read pointer
            // to ensure you're only reading data that has been written by the DMA.
            // The `dma_hw->ch[chan].write_addr` can give you the current write position
            // of the DMA, which you can use to determine how much new data is available.

            printf("Current samples from buffer:\n");
            for (int i = 0; i < 5; ++i) {
                float voltage = (float)adc_ring_buffer[i] * ADC_VREF / ADC_RANGE;
                printf("Sample %d: Raw = %u, Voltage = %.2fV\n", i, adc_ring_buffer[i], voltage);
            }
            printf("---\n");
        }
        sleep_ms(10); // Small delay to avoid busy-waiting too much
    }

    return 0;
}

// DMA Interrupt Handler
void adc_dma_handler() {
    if (dma_channel_get_irq0_status(ADC_DMA_CHAN)) {
        dma_channel_acknowledge_irq0(ADC_DMA_CHAN);
        // This interrupt fires when the current DMA transfer completes.
        // Since we chained the DMA to itself, it will immediately restart.
        // This interrupt can be useful for:
        // 1. Notifying your main loop that a certain amount of data is available.
        //    For example, if you set the transfer count to half the buffer size,
        //    you could get an interrupt when the first half is full, and then
        //    another when the second half is full. This allows for double buffering.
        // 2. Monitoring for underruns/overruns if your processing isn't keeping up.
        //
        // For a simple ring buffer that chains to itself with the full buffer size
        // as the transfer count, this interrupt will fire *each time the DMA
        // completes filling the entire buffer*.
        printf("DMA IRQ: Buffer completed a cycle!\n");
    }
}