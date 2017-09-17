#include "usb.h"

static uint8_t send_buf_internal[16];
static uint8_t recv_buf_internal[16];
static uint8_t recv_ptr;

static USB_KeyboardReport_Data_t keyboard_report;
static USB_MouseReport_Data_t mouse_report;
static uint8_t keys_down = 0;
static bool is_report_protocol = true;
static uint16_t idle_max = 500;
static uint16_t idle_cnt = 0;

static CDC_LineEncoding_t line_encoding = {
	.BaudRateBPS = 0,
    .CharFormat  = CDC_LINEENCODING_OneStopBit,
    .ParityType  = CDC_PARITY_None,
    .DataBits    = 8
};

void EVENT_USB_Device_Connect(void) {
}

void EVENT_USB_Device_Disconnect(void) {
}

void EVENT_USB_Device_ConfigurationChanged(void) {
	bool ConfigSuccess = true;

	ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_NOTIFICATION_EPADDR, EP_TYPE_INTERRUPT, CDC_NOTIFICATION_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_TX_EPADDR, EP_TYPE_BULK, CDC_TXRX_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_RX_EPADDR, EP_TYPE_BULK,  CDC_TXRX_EPSIZE, 1);

	ConfigSuccess &= Endpoint_ConfigureEndpoint(KEYBOARD_IN_EPADDR, EP_TYPE_INTERRUPT, KEYBOARD_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(KEYBOARD_OUT_EPADDR, EP_TYPE_INTERRUPT, KEYBOARD_EPSIZE, 1);

	ConfigSuccess &= Endpoint_ConfigureEndpoint(MOUSE_IN_EPADDR, EP_TYPE_INTERRUPT, KEYBOARD_EPSIZE, 1);

	USB_Device_EnableSOFEvents();

	line_encoding.BaudRateBPS = 0;
}

void EVENT_USB_Device_ControlRequest(void) {
	switch(USB_ControlRequest.bRequest) {
		case CDC_REQ_GetLineEncoding:
			if(USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE)) {
				Endpoint_ClearSETUP();
				Endpoint_Write_Control_Stream_LE(&line_encoding, sizeof(CDC_LineEncoding_t));
				Endpoint_ClearOUT();
			}
			break;
		case CDC_REQ_SetLineEncoding:
			if(USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE)) {
				Endpoint_ClearSETUP();
				Endpoint_Read_Control_Stream_LE(&line_encoding, sizeof(CDC_LineEncoding_t));
				if(line_encoding.BaudRateBPS == 1200) {
					*(uint16_t *)0x0800 = 0x7777;
					USB_Disable();
					cli();
					wdt_enable(WDTO_15MS);
					while(1);
				}
				Endpoint_ClearIN();
			}
			break;
		case CDC_REQ_SetControlLineState:
			if(USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE)) {
				Endpoint_ClearSETUP();
				Endpoint_ClearStatusStage();
			}
			break;
		case HID_REQ_GetReport:
			if(USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE)) {
				Endpoint_ClearSETUP();

				uint8_t *data;
				uint8_t size;

				if (!(USB_ControlRequest.wIndex))
				{
					data = (uint8_t*)&keyboard_report;
					size = sizeof(keyboard_report);
				}
				else
				{
					data = (uint8_t*)&mouse_report;
					size = sizeof(mouse_report);
				}

				Endpoint_Write_Control_Stream_LE(&data, size);
				Endpoint_ClearOUT();
			}
			break;
		case HID_REQ_SetReport:
			if(USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE)) {
				Endpoint_ClearSETUP();

				while(!Endpoint_IsOUTReceived()) {
					if(USB_DeviceState == DEVICE_STATE_Unattached)
					  return;
				}

//				usb_keyboard_update_led(Endpoint_Read_8());

				Endpoint_ClearOUT();
				Endpoint_ClearStatusStage();
			}
			break;
		case HID_REQ_GetProtocol:
			if(USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE)) {
				Endpoint_ClearSETUP();

				Endpoint_Write_8(is_report_protocol);

				Endpoint_ClearIN();
				Endpoint_ClearStatusStage();
			}

			break;
		case HID_REQ_SetProtocol:
			if(USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE)) {
				Endpoint_ClearSETUP();
				Endpoint_ClearStatusStage();

				is_report_protocol = (USB_ControlRequest.wValue != 0);
			}

			break;
		case HID_REQ_SetIdle:
			if(USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE)) {
				Endpoint_ClearSETUP();
				Endpoint_ClearStatusStage();

				idle_cnt = ((USB_ControlRequest.wValue & 0xFF00) >> 6);
			}

			break;
		case HID_REQ_GetIdle:
			if(USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE)) {
				Endpoint_ClearSETUP();

				Endpoint_Write_8(idle_max >> 2);

				Endpoint_ClearIN();
				Endpoint_ClearStatusStage();
			}

			break;
	}
}

void EVENT_USB_Device_StartOfFrame(void) {
	if(idle_cnt) {
		--idle_cnt;
	}
}

void usb_init(void) {
	send_buf = send_buf_internal + 1;
	recv_buf = recv_buf_internal + 1;

	MCUSR &= ~(1 << WDRF);
	wdt_disable();
	clock_prescale_set(clock_div_1);

	USB_Init();
	GlobalInterruptEnable();
}

void usb_cdc_send_packet(uint8_t *buf, uint8_t len) {
	if(USB_DeviceState != DEVICE_STATE_Configured || !line_encoding.BaudRateBPS)
		return;

	Endpoint_SelectEndpoint(CDC_TX_EPADDR);

	// Endpoint_Write_Stream_LE(send_buf_internal, (send_len & 0xF) + 1, NULL);
	Endpoint_Write_Stream_LE(buf, len, NULL);

	bool full = Endpoint_BytesInEndpoint() == CDC_TXRX_EPSIZE;

	Endpoint_ClearIN();

	if(full) {
		Endpoint_WaitUntilReady();
		Endpoint_ClearIN();
	}
}

void usb_mouse_send_report(USB_MouseReport_Data_t mouse_report) {
	Endpoint_SelectEndpoint(MOUSE_IN_EPADDR);

	Endpoint_Write_Stream_LE(&mouse_report, sizeof(mouse_report), NULL);
	Endpoint_ClearIN();
}

void usb_keyboard_send_report(void) {
	Endpoint_SelectEndpoint(KEYBOARD_IN_EPADDR);

	Endpoint_Write_Stream_LE(&keyboard_report, sizeof(keyboard_report), NULL);
	Endpoint_ClearIN();
}

void usb_keyboard_key_down(uint8_t key) {
	for(uint8_t i = 0; i < 6; ++i) {
		if(!keyboard_report.KeyCode[i]) {
			keyboard_report.KeyCode[i] = key;
			++keys_down;
			break;
		} else if(keyboard_report.KeyCode[i] == key) {
			return;
		}
	}
	usb_keyboard_send_report();
}

void usb_keyboard_key_up(uint8_t key) {
	for(uint8_t i = 0; i < 6; ++i) {
		if(keyboard_report.KeyCode[i] == key) {
			keyboard_report.KeyCode[i] = 0;
			--keys_down;
			break;
		}
	}
	usb_keyboard_send_report();
}

void usb_keyboard_modifier_down(uint8_t modifier) {
	keyboard_report.Modifier |= 1 << modifier;
	usb_keyboard_send_report();
}

void usb_keyboard_modifier_up(uint8_t modifier) {
	keyboard_report.Modifier &= ~(1 << modifier);
	usb_keyboard_send_report();
}

void usb_run(void) {
	if(USB_DeviceState != DEVICE_STATE_Configured)
		return;

	if(line_encoding.BaudRateBPS) {
		Endpoint_SelectEndpoint(CDC_RX_EPADDR);

		if(Endpoint_IsOUTReceived()) {
			while(Endpoint_IsReadWriteAllowed()) {
				uint8_t recv = Endpoint_Read_8();
				if(!recv_ptr) {
					recv_op = recv >> 4;
					recv_len = recv & 0xF;
				}
				recv_buf_internal[recv_ptr++] = recv;
				if(recv_ptr == recv_len + 1) {
					//usb_cdc_process_packet();
					Endpoint_SelectEndpoint(CDC_RX_EPADDR);
					recv_ptr = 0;
				}
			}
			Endpoint_ClearOUT();
		}
	}

	Endpoint_SelectEndpoint(KEYBOARD_IN_EPADDR);
	
	if(Endpoint_IsOUTReceived() && Endpoint_IsReadWriteAllowed()) {
		//usb_keyboard_update_led(Endpoint_Read_8());
		Endpoint_SelectEndpoint(KEYBOARD_IN_EPADDR);
		Endpoint_ClearOUT();
	}

	if(!idle_cnt && idle_max) {
		idle_cnt = idle_max;
		usb_keyboard_send_report();
	}
}

