#ifndef usb_h 
#define usb_h 

#include <avr/wdt.h>
#include <avr/power.h>

#include "Descriptors.h"

#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>

void usb_cdc_process_packet(void);
void usb_cdc_update_led(uint8_t led);

void usb_init(void);
void usb_run(void);
void usb_cdc_send_packet(uint8_t *buf, uint8_t len);
void usb_keyboard_key_down(uint8_t key);
void usb_keyboard_key_up(uint8_t key);
void usb_keyboard_modifier_down(uint8_t modifier);
void usb_keyboard_modifier_up(uint8_t modifier);

uint8_t send_op;
uint8_t send_len;
uint8_t *send_buf;

uint8_t recv_op;
uint8_t recv_len;
uint8_t *recv_buf;

#endif
