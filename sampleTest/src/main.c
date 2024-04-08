#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h> 
#include <zephyr/logging/log.h>
#include <nrfx_saadc.h>

LOG_MODULE_REGISTER(ss,LOG_LEVEL_DBG);//

static nrfx_saadc_channel_t channel = NRFX_SAADC_DEFAULT_CHANNEL_SE(NRF_SAADC_INPUT_AIN0,0);
static int16_t sample;

#define BATTERY_SAMPLE_INTERVAL_MS 2000
static void battery_sample_timer_handler(struct k_timer * timer);
K_TIMER_DEFINE(battery_sample_timer, battery_sample_timer_handler, NULL);
// IRQ_CONNECT(DT_IRQN(DT_NODELABEL(adc)),
//             DT_IRQ(DT_NODELABEL(adc), priority),
//             nrfx_isr, nrfx_saadc_irq_handler, 0);

void configureADC(){
       

        nrfx_err_t err = nrfx_saadc_init(DT_IRQ(DT_NODELABEL(adc), priority));
        if (err != NRFX_SUCCESS) {
        printk("nrfx_saadc_mode_trigger error: %08x", err);
        return;
        }
        channel.channel_config.gain = NRF_SAADC_GAIN1_6;
        err = nrfx_saadc_channels_config(&channel, 1);
        if (err != NRFX_SUCCESS) {
        printk("nrfx_saadc_channels_config error: %08x", err);
        return;
        }

        err = nrfx_saadc_simple_mode_set(BIT(0),
                                 NRF_SAADC_RESOLUTION_12BIT,
                                 NRF_SAADC_OVERSAMPLE_DISABLED,
                                 NULL);
        if (err != NRFX_SUCCESS) {
        printk("nrfx_saadc_simple_mode_set error: %08x", err);
        return;
        }
        
        err = nrfx_saadc_buffer_set(&sample, 1);
        if (err != NRFX_SUCCESS) {
        printk("nrfx_saadc_buffer_set error: %08x", err);
        return;
        }
        k_timer_start(&battery_sample_timer, K_NO_WAIT, K_MSEC(BATTERY_SAMPLE_INTERVAL_MS));
} 

void battery_sample_timer_handler(struct k_timer *timer)
{
        LOG_INF("Inside timer ");
        nrfx_err_t err = nrfx_saadc_mode_trigger();
        if (err != NRFX_SUCCESS) {
                printk("nrfx_saadc_mode_trigger error: %08x", err);
                return;
        }
        int battery_voltage = ((600*6) * sample) / ((1<<12));
        printk("SAADC sample: %d\n", sample);
        printk("Battery Voltage: %d mV\n", battery_voltage);
}

int main(void)
{
        configureADC();

        printk("in Main function\n");
        while(1) {}
        return 0;   
}
