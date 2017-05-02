/*
 * Copyright (c) 2017 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <zephyr.h>
#include "arduino101_main.h"


/* size of stack area used by each thread */
#define MAIN_STACKSIZE      2048
#define SKETCH_STACKSIZE    2048
#define CDCACM_STACKSIZE    512
#define RESET_STACKSIZE     512
#define USBSERIAL_STACKSIZE 512

/* scheduling priority used by each thread */
#define PRIORITY 7
#define TASK_PRIORITY 8

#define ARDUINO_101_VID		0x8087
#define ARDUINO_101_PID		0x0AB6

#define MAX_DESCRIPTOR_LENGTH	64

char __noinit __stack sketch_stack_area[SKETCH_STACKSIZE];
char __noinit __stack cdcacm_setup_stack_area[CDCACM_STACKSIZE];
char __noinit __stack baudrate_reset_stack_area[RESET_STACKSIZE];
char __noinit __stack usb_serial_stack_area[USBSERIAL_STACKSIZE];

const char *vendor = "Intel";
const char *product =  "Arduino 101";
const char *serial = "00.01";

struct gpio_callback cb;

void copy_device_descriptor(char *dest, uint8_t *source, const char *default_value, uint8_t len, uint8_t max)
{
	if ((len > 0) && (len < max))
	{
		memcpy(dest, source, len);
		dest[len] = '\0';
	}
	else
	{
		memcpy(dest, default_value, strlen(default_value));
		dest[strlen(default_value)] = '\0';
	}
}

void cdc_acm_descriptor_callback (cdc_acm_cfg_t *cfg)
{
	customer_data_t *otp_data = (customer_data_t *)CUSTOMER_DATA_ADDRESS;

	//read the USB descriptors from OTP and use some default values if it has not been properly populated
	copy_device_descriptor(cfg->vendor_string, otp_data->vendor_name, vendor,
		otp_data->vendor_name_len, MAX_DESCRIPTOR_LENGTH);

	copy_device_descriptor(cfg->product_string, otp_data->board_name, product,
		otp_data->board_name_len, MAX_DESCRIPTOR_LENGTH);

	copy_device_descriptor(cfg->serial_string, otp_data->product_sn, serial,
		otp_data->product_sn_len, MAX_DESCRIPTOR_LENGTH);

	if((otp_data->product_vid != 0xFFFF) && (otp_data->product_vid != 0x0000)
		&& (otp_data->product_pid != 0xFFFF) && (otp_data->product_pid != 0x0000))
	{
		cfg->vendor_id = otp_data->product_vid;
		cfg->product_id = otp_data->product_pid;
	}
	else
	{
		//use VID/PID of Arduino 101
		cfg->vendor_id = ARDUINO_101_VID;
		cfg->product_id = ARDUINO_101_PID;
	}
}

void threadMain(void *dummy1, void *dummy2, void *dummy3)
{
	init_cdc_acm();
	init_sharedMemory_com();
	curie_shared_data->error_code = 0;
	// start ARC core
	uint32_t *reset_vector;
	reset_vector = (uint32_t *)RESET_VECTOR;
	start_arc(*reset_vector);
	
	
	k_thread_spawn(sketch_stack_area, SKETCH_STACKSIZE, sketch, NULL, NULL,
			NULL, TASK_PRIORITY, 0, K_NO_WAIT);
	
	k_thread_spawn(cdcacm_setup_stack_area, CDCACM_STACKSIZE, cdcacm_setup, NULL, NULL,
			NULL, TASK_PRIORITY, 0, K_NO_WAIT);
	
	k_thread_spawn(baudrate_reset_stack_area, RESET_STACKSIZE, baudrate_reset, NULL, NULL,
			NULL, TASK_PRIORITY, 0, K_NO_WAIT);
	
	k_thread_spawn(usb_serial_stack_area, USBSERIAL_STACKSIZE, usb_serial, NULL, NULL,
			NULL, TASK_PRIORITY, 0, K_NO_WAIT);

}

K_THREAD_DEFINE(threadMain_id, MAIN_STACKSIZE, threadMain, NULL, NULL, NULL,
		PRIORITY, 0, K_NO_WAIT);

