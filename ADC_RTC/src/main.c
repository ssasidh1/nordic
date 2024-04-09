/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(Lesson6_Exercise3, LOG_LEVEL_INF);

/* STEP 3 - Include header for nrfx drivers */
#include <nrfx_saadc.h>
#include <nrfx_rtc.h>
#include <nrfx_timer.h>
#include <helpers/nrfx_gppi.h>
#if defined(DPPI_PRESENT)
#include <nrfx_dppi.h>
#else
#include <nrfx_ppi.h>
#endif

/* STEP 4.1 - Define the SAADC sample interval in microseconds */
#define SAADC_SAMPLE_INTERVAL_US 2500

/* STEP 5.1 - Define the buffer size for the SAADC */
#define SAADC_BUFFER_SIZE   400

/* STEP 4.2 - Declaring an instance of nrfx_timer for TIMER2. */
const nrfx_rtc_t timer_instance = NRFX_RTC_INSTANCE(2);

/* STEP 5.2 - Declare the buffers for the SAADC */
static nrf_saadc_value_t saadc_sample_buffer[2][SAADC_BUFFER_SIZE];

/* STEP 5.3 - Declare variable used to keep track of which buffer was last assigned to the SAADC driver */
static uint32_t saadc_current_buffer = 0;

static void rtc_handler(nrfx_rtc_int_type_t int_type)
{
    nrfx_err_t err;
	switch (int_type)
	{
		case NRFX_RTC_INT_TICK:
		//	printk("tick interrupt received\n");
			break;
		case NRFX_RTC_INT_COMPARE0:
		{
			printk("compare 0 event received\n");
			//err = nrfx_rtc_cc_set(&timer_instance, 0, 1, true);
			if(err!=NRFX_SUCCESS)
			{
				printk(" compare channel initialization error %d",err);
			}
            break;
		}
		default:
			printk("rtc interrupt %d\n", int_type);
			break;
	}
}

static void configure_RTC(void)
{
    nrfx_err_t err;

    /* STEP 4.3 - Declaring timer config and intialize nrfx_timer instance. */
    nrfx_rtc_config_t timer_config = NRFX_RTC_DEFAULT_CONFIG;
    timer_config.prescaler = NRF_RTC_FREQ_TO_PRESCALER(400) ;

    IRQ_CONNECT(DT_IRQN(DT_NODELABEL(rtc2)),
                DT_IRQ(DT_NODELABEL(rtc2), priority),
                nrfx_rtc_2_irq_handler, NULL, 0);  

    err = nrfx_rtc_init(&timer_instance, &timer_config, rtc_handler);

    if (err != NRFX_SUCCESS) {
        LOG_ERR("nrfx_timer_init error: %08x", err);
        return;
    }
     nrfx_rtc_tick_enable(&timer_instance, true);


    /* STEP 4.4 - Set compare channel 0 to generate event every SAADC_SAMPLE_INTERVAL_US. */
    // err = nrfx_rtc_cc_set(&timer_instance, 0, 1, true);
    // if(err!=NRFX_SUCCESS)
    // {
    //  printk(" compare channel initialization error %d",err);
    // }
    nrfx_rtc_enable(&timer_instance);

}

static void saadc_event_handler(nrfx_saadc_evt_t const * p_event)
{
    nrfx_err_t err;
    switch (p_event->type)
    {
        case NRFX_SAADC_EVT_READY:
        
           /* STEP 6.1 - Buffer is ready, timer (and sampling) can be started. */
            ////LOG_INF("Inside SAADC EVT READY");
            nrfx_rtc_enable(&timer_instance);
            break;                        
            
        case NRFX_SAADC_EVT_BUF_REQ:
        //LOG_INF("Inside SAADC EVT REQ");
            /* STEP 6.2 - Set up the next available buffer. Alternate between buffer 0 and 1 */
            err = nrfx_saadc_buffer_set(saadc_sample_buffer[(saadc_current_buffer++)%2], SAADC_BUFFER_SIZE);
            //err = nrfx_saadc_buffer_set(saadc_sample_buffer[((saadc_current_buffer == 0 )? saadc_current_buffer++ : 0)], SAADC_BUFFER_SIZE);
            if (err != NRFX_SUCCESS) {
                LOG_ERR("nrfx_saadc_buffer_set error: %08x", err);
                return;
            }
            break;

        case NRFX_SAADC_EVT_DONE:

            /* STEP 6.3 - Buffer has been filled. Do something with the data and proceed */
            int64_t average = 0;
            int16_t max = INT16_MIN;
            int16_t min = INT16_MAX;
           printk("--samples--\n");
            for(int i=0; i < p_event->data.done.size; i++){
                average += p_event->data.done.p_buffer[i];
                if((int16_t)p_event->data.done.p_buffer[i] > max){
                    max = p_event->data.done.p_buffer[i];
                }
                if((int16_t)p_event->data.done.p_buffer[i] < min){
                    min = p_event->data.done.p_buffer[i];
                }
                // printk("%d ",p_event->data.done.p_buffer[i]);
                // if(i%100==0)printk("\n");
            }
            printk("----\n");
            average = average/p_event->data.done.size;
            LOG_INF("SAADC buffer at 0x%x filled with %d samples", (uint32_t)p_event->data.done.p_buffer, p_event->data.done.size);
            LOG_INF("AVG=%d, MIN=%d, MAX=%d", (int16_t)average, min, max);
            break;

        default:
            //LOG_INF("Unhandled SAADC evt %d", p_event->type);
            break;
    }
}

static void configure_saadc(void)
{
    nrfx_err_t err;

    /* STEP 5.4 - Connect ADC interrupt to nrfx interrupt handler */
    IRQ_CONNECT(DT_IRQN(DT_NODELABEL(adc)),
                DT_IRQ(DT_NODELABEL(adc), priority),
                nrfx_isr, nrfx_saadc_irq_handler, 0);

    
    /* STEP 5.5 - Initialize the nrfx_SAADC driver */
    err = nrfx_saadc_init(DT_IRQ(DT_NODELABEL(adc), priority));
    if (err != NRFX_SUCCESS) {
        LOG_ERR("nrfx_saadc_init error: %08x", err);
        return;
    }

    
    /* STEP 5.6 - Declare the struct to hold the configuration for the SAADC channel used to sample the battery voltage */
    nrfx_saadc_channel_t channel = NRFX_SAADC_DEFAULT_CHANNEL_SE(NRF_SAADC_INPUT_AIN0, 0);

    /* STEP 5.7 - Change gain config in default config and apply channel configuration */
    channel.channel_config.gain = NRF_SAADC_GAIN1_6;
    err = nrfx_saadc_channels_config(&channel, 1);
    if (err != NRFX_SUCCESS) {
        LOG_ERR("nrfx_saadc_channels_config error: %08x", err);
        return;
    }

    /* STEP 5.8 - Configure channel 0 in advanced mode with event handler (non-blocking mode) */
    nrfx_saadc_adv_config_t saadc_adv_config = NRFX_SAADC_DEFAULT_ADV_CONFIG;
    err = nrfx_saadc_advanced_mode_set(BIT(0),
                                        NRF_SAADC_RESOLUTION_12BIT,
                                        &saadc_adv_config,
                                        saadc_event_handler);
    if (err != NRFX_SUCCESS) {
        LOG_ERR("nrfx_saadc_advanced_mode_set error: %08x", err);
        return;
    }
                                            
    /* STEP 5.9 - Configure two buffers to make use of double-buffering feature of SAADC */
    err = nrfx_saadc_buffer_set(saadc_sample_buffer[0], SAADC_BUFFER_SIZE);
    if (err != NRFX_SUCCESS) {
        LOG_ERR("nrfx_saadc_buffer_set error: %08x", err);
        return;
    }
    err = nrfx_saadc_buffer_set(saadc_sample_buffer[1], SAADC_BUFFER_SIZE);
    if (err != NRFX_SUCCESS) {
        LOG_ERR("nrfx_saadc_buffer_set error: %08x", err);
        return;
    }

    /* STEP 5.10 - Trigger the SAADC. This will not start sampling, but will prepare buffer for sampling triggered through PPI */
    err = nrfx_saadc_mode_trigger();
    if (err != NRFX_SUCCESS) {
        LOG_ERR("nrfx_saadc_mode_trigger error: %08x", err);
        return;
    }

}

static void configure_ppi(void)
{
    /* STEP 7.1 - Declare variables used to hold the (D)PPI channel number */
    // Trigger task sample from timer
        uint8_t m_saadc_sample_ppi_channel;
        uint8_t m_saadc_start_ppi_channel;

	nrfx_err_t err = nrfx_gppi_channel_alloc(&m_saadc_sample_ppi_channel);
	if(err!=NRFX_SUCCESS)
	{
	printk(" PPI channel allocation error %d",err);
	}

    /* STEP 7.3 - Trigger task sample from timer */
    //nrfx_rtc_cc_set()
   	 nrfx_gppi_channel_endpoints_setup(m_saadc_sample_ppi_channel, 
	                                   nrfx_rtc_event_address_get(&timer_instance, NRF_RTC_EVENT_TICK),
	                                   nrf_saadc_task_address_get(NRF_SAADC,NRF_SAADC_TASK_SAMPLE));

     nrfx_gppi_channel_endpoints_setup(m_saadc_start_ppi_channel, 
                                      nrf_saadc_event_address_get(NRF_SAADC, NRF_SAADC_EVENT_END),
                                      nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_START));
	
	if(err!=NRFX_SUCCESS)
	{
		printk(" PPI channel assignment error %d",err);
	}

	nrfx_gppi_channels_enable(BIT(m_saadc_sample_ppi_channel));
    nrfx_gppi_channels_enable(BIT(m_saadc_start_ppi_channel));

	if(err!=NRFX_SUCCESS)
	{
		printk(" PPI channel enable error %d",err);
	}
}


int main(void)
{
    LOG_INF("Started");
    configure_RTC();
    configure_saadc();  
    configure_ppi();
    LOG_INF("config done: logger");
    printk("config done\n");
    k_sleep(K_FOREVER);
}