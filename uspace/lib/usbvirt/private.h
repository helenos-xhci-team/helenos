/*
 * Copyright (c) 2010 Vojtech Horky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup libusbvirt usb
 * @{
 */
/** @file
 * @brief Virtual USB private header.
 */
#ifndef LIBUSBVIRT_PRIVATE_H_
#define LIBUSBVIRT_PRIVATE_H_

#include "device.h"
#include "hub.h"


#define DEVICE_HAS_OP(dev, op) \
	( \
		(  ((dev)->ops) != NULL  ) \
		&& \
		(  ((dev)->ops->op) != NULL  ) \
	)

int usbvirt_data_to_host(struct usbvirt_device *dev,
    usb_endpoint_t endpoint, void *buffer, size_t size);

int handle_incoming_data(struct usbvirt_device *dev,
    usb_endpoint_t endpoint, void *buffer, size_t size);

int control_pipe(usbvirt_device_t *device, usbvirt_control_transfer_t *transfer);

int handle_std_request(usbvirt_device_t *device, usb_device_request_setup_packet_t *request, uint8_t *data);

void device_callback_connection(usbvirt_device_t *device, ipc_callid_t iid, ipc_call_t *icall);

int transaction_setup(usbvirt_device_t *device, usb_endpoint_t endpoint,
    void *buffer, size_t size);
int transaction_out(usbvirt_device_t *device, usb_endpoint_t endpoint,
    void *buffer, size_t size);
int transaction_in(usbvirt_device_t *device, usb_endpoint_t endpoint,
    void *buffer, size_t size, size_t *data_size);


void user_debug(usbvirt_device_t *device, int level, uint8_t tag,
    const char *format, ...);
void lib_debug(usbvirt_device_t *device, int level, uint8_t tag,
    const char *format, ...);
    
static inline const char *str_device_state(usbvirt_device_state_t state)
{
	switch (state) {
		case USBVIRT_STATE_DEFAULT:
			return "default";
		case USBVIRT_STATE_ADDRESS:
			return "address";
		case USBVIRT_STATE_CONFIGURED:
			return "configured";
		default:
			return "unknown";
	}
}

#endif
/**
 * @}
 */