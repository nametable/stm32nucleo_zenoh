//
// Copyright (c) 2022 ZettaScale Technology
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Apache License, Version 2.0
// which is available at https://www.apache.org/licenses/LICENSE-2.0.
//
// SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
//
// Contributors:
//   ZettaScale Zenoh Team, <zenoh@zettascale.tech>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zenoh-pico.h>
#include <zenoh-pico/transport/unicast/lease.h>
#include <zenoh-pico/session/session.h>

#define MODE "client"
#define CONNECT0 "serial/serial@40004400#baudrate=115200"
// serial@40004400 -- this is the one!
// serial@40011000
#define KEYEXPR "clk_man"
#define SUB_KEYEXPR "hw/led0"
#define VALUE "[STSTM32]{nucleo-F466RE} Pub from Zenoh-Pico!"

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#define BUTTON_NODE DT_ALIAS(sw0)
#define LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

static inline uint32_t bswap32(uint32_t input_long)
{
    // manual byte swap
    return ((input_long & 0x000000FF) << 24) |
           ((input_long & 0x0000FF00) << 8) |
           ((input_long & 0x00FF0000) >> 8) |
           ((input_long & 0xFF000000) >> 24);
}

void data_handler(const z_loaned_sample_t *sample, void *arg) {
    z_view_string_t keystr;
    z_keyexpr_as_view_string(z_sample_keyexpr(sample), &keystr);
    z_owned_slice_t value;
    z_bytes_deserialize_into_slice(z_sample_payload(sample), &value);

    uint8_t * data = z_slice_data(z_loan(value));
    uint8_t len = z_slice_len(z_loan(value));

    unsigned int val = 0;
    if (len == 4) {
        val = *(data + 3);
    } else if (len == 8) {
        val = *(data + 7);
    }
    gpio_pin_set_dt(&led, val);
    // z_owned_str_t keystr = z_keyexpr_to_string(sample->keyexpr);
    // printf(" >> [Subscriber handler] Received ('%s': '%.*s')\n", z_loan(keystr), (int)sample->payload.len, sample->payload.start);
    // z_drop(z_move(keystr));
}

#if Z_FEATURE_PUBLICATION == 1
int main(int argc, char **argv) {
    printk("Sleeping for 5s...\n");
    // char* blah = z_malloc(256);
    // memcpy(blah, "Hello Logan, this is a test of something that fills up a bunch of heap space!", strlen("Hello Logan, this is a test of something that fills up a bunch of heap space!"));
    // *((char*)0x20005b38) = 'J';
    // char a = *((char*)0x200052B0);
    // a++;
    // printk("I didn't die\n");
    // printk("a: %d\n", a);
    sleep(5);


    // https://github.com/vortex314/zenoh-projects/blob/1cf8b706074bb4ef03190e21a6365c433bed21e6/espidf_serial/src/main.cpp

    printk("Starting Zenoh-Pico Publisher...\n");
    // Initialize Zenoh Session and other parameters
    z_owned_config_t config;
    z_config_default(&config);
    printk("Inserting config mode key: '%s'...\n", MODE);
    zp_config_insert(z_loan_mut(config), Z_CONFIG_MODE_KEY, MODE);
    printk("Inserting config connect key: '%s'...\n", CONNECT0);
    zp_config_insert(z_loan_mut(config), Z_CONFIG_CONNECT_KEY, CONNECT0);
    // if (strcmp(CONNECT, "") != 0) {
    //     zp_config_insert(z_loan(config), Z_CONFIG_CONNECT_KEY, z_string_make(CONNECT));
    //     zp_config_insert(z_loan(config), Z_CONFIG_CONNECT_KEY, z_string_make(CONNECT));
    // }

    // Open Zenoh session
    printf("Opening Zenoh Session...");
    z_owned_session_t s;

    int8_t ret8 = z_open(&s, z_move(config));
    if (ret8 < 0) {
        printf("Unable to open session!\n");
        exit(-1);
    }
    printf("OK\n");

    // Start the receive and the session lease loop for zenoh-pico
    zp_start_read_task(z_loan(s), NULL);
    zp_start_lease_task(z_loan(s), NULL);

    z_view_keyexpr_t ke;
    z_view_keyexpr_from_str_unchecked(&ke, KEYEXPR);

    printf("Declaring publisher for '%s'...", KEYEXPR);
    z_owned_publisher_t pub;
    if (z_declare_publisher(&pub, z_loan(s), z_loan(ke), NULL) < 0)
    {
        printf("Unable to declare publisher for key expression!\n");
        exit(-1);
    }
    printf("OK\n");

	int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0)
	{
		return;
	}

    z_owned_closure_sample_t callback;
    z_closure(&callback, data_handler);

    z_view_keyexpr_t sub_ke;
    z_view_keyexpr_from_str_unchecked(&sub_ke, SUB_KEYEXPR);

    z_owned_subscriber_t sub;
    if (z_declare_subscriber(&sub, z_loan(s), z_loan(sub_ke), z_move(callback), NULL) < 0) {
        printf("Unable to declare subscriber.\n");
        exit(-1);
    }

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0)
	{
		return;
	}

    // char buf[256];
    int64_t next_keep_alive = k_uptime_get();
    int last_sent_value = -1;
    for (int idx = 0; 1; ++idx) {
        usleep(2000);
        int button_value = bswap32(gpio_pin_get_dt(&button));
        // sprintf(buf, "[%4d] %s", idx, VALUE);
        // printf("Putting Data ('%s': '%s')... button: %d\n", KEYEXPR, buf, button_value);
        // z_publisher_put(z_loan(pub), (const uint8_t *)buf, strlen(buf), NULL);
        z_publisher_put_options_t options;
        z_publisher_put_options_default(&options);
        // options.encoding = z_encoding(Z_ENCODING_PREFIX_APP_OCTET_STREAM, NULL);

        if (button_value != last_sent_value) {
            // Create payload
            z_owned_bytes_t payload;
            z_bytes_serialize_from_buf(&payload, (const uint8_t *)&button_value, 4);
            // z_bytes_serialize_from_str(&payload, (const uint8_t *)&button_value);

            z_publisher_put(z_loan(pub), z_move(payload), &options);
            last_sent_value = button_value;
        }

        // zp_send_keep_alive_options_t options = zp_send_keep_alive_options_default();

        // without multi-thread enabled, the lease task will not run, so
        // we need to manually send keep-alives
        // TODO: and also check for lease expiration to listen and re-establish the session

        // send a keep alive every second
        int64_t now = k_uptime_get();
        if (now > next_keep_alive) {
            next_keep_alive = now + 1000;
            zp_send_keep_alive(z_loan(s), NULL);
        }

        // zp_read_options_t read_options;
        // zp_read_options_default(&read_options);

        // uart_pending_in

        // bool ready = *((unsigned int*)USART2_BASE) & USART_SR_RXNE; // check if RXNE is set, data is ready to read
        // if (ready) {
        //     int8_t ret = zp_read(z_loan(s), NULL);
        // }
    }

    printf("Closing Zenoh Session...");
    z_undeclare_publisher(z_move(pub));
    z_undeclare_subscriber(z_move(sub));

    // Stop the receive and the session lease loop for zenoh-pico
    zp_stop_read_task(z_loan(s));
    zp_stop_lease_task(z_loan(s));

    z_close(z_move(s));
    printf("OK!\n");

    return 0;
}
#else
int main(void) {
    printf("ERROR: Zenoh pico was compiled without Z_FEATURE_PUBLICATION but this example requires it.\n");
    return -2;
}
#endif
