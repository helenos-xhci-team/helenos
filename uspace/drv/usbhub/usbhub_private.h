/*
 * Copyright (c) 2010 Matus Dekanek
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

/** @addtogroup usb
 * @{
 */
/** @file
 * @brief Hub driver.
 */

#ifndef USBHUB_PRIVATE_H
#define	USBHUB_PRIVATE_H

#include "usbhub.h"
#include "usblist.h"

#include <adt/list.h>
#include <bool.h>
#include <driver.h>
#include <futex.h>

#include <usb/usb.h>
#include <usb/usbdrv.h>
#include <usb/classes/hub.h>
#include <usb/devreq.h>
#include <usb/debug.h>

//************
//
// convenience define for malloc
//
//************
#define usb_new(type) (type*)malloc(sizeof(type))


//************
//
// convenience debug printf
//
//************
#define dprintf(level, format, ...) \
	usb_dprintf(NAME, (level), format "\n", ##__VA_ARGS__)

/**
 * create hub structure instance
 *
 * Set the address and port count information most importantly.
 *
 * @param device
 * @param hc host controller phone
 * @return
 */
usb_hub_info_t * usb_create_hub_info(device_t * device, int hc);

/** list of hubs maanged by this driver */
extern usb_general_list_t usb_hub_list;

/** lock for hub list*/
extern futex_t usb_hub_list_lock;


/**
 * perform complete control read transaction
 *
 * manages all three steps of transaction: setup, read and finalize
 * @param phone
 * @param target
 * @param request request for data
 * @param rcvd_buffer received data
 * @param rcvd_size
 * @param actual_size actual size of received data
 * @return error code
 */
int usb_drv_sync_control_read(
    int phone, usb_target_t target,
    usb_device_request_setup_packet_t * request,
    void * rcvd_buffer, size_t rcvd_size, size_t * actual_size
);

/**
 * perform complete control write transaction
 *
 * manages all three steps of transaction: setup, write and finalize
 * @param phone
 * @param target
 * @param request request to send data
 * @param sent_buffer
 * @param sent_size
 * @return error code
 */
int usb_drv_sync_control_write(
    int phone, usb_target_t target,
    usb_device_request_setup_packet_t * request,
    void * sent_buffer, size_t sent_size
);

/**
 * set the device request to be a get hub descriptor request.
 * @warning the size is allways set to USB_HUB_MAX_DESCRIPTOR_SIZE
 * @param request
 * @param addr
 */
static inline void usb_hub_set_descriptor_request(
usb_device_request_setup_packet_t * request
){
	request->index = 0;
	request->request_type = USB_HUB_REQ_TYPE_GET_DESCRIPTOR;
	request->request = USB_HUB_REQUEST_GET_DESCRIPTOR;
	request->value_high = USB_DESCTYPE_HUB;
	request->value_low = 0;
	request->length = USB_HUB_MAX_DESCRIPTOR_SIZE;
}

static inline int usb_hub_clear_port_feature(int hc, usb_address_t address,
    int port_index,
    usb_hub_class_feature_t feature) {
	usb_target_t target = {
		.address = address,
		.endpoint = 0
	};
	usb_device_request_setup_packet_t clear_request = {
		.request_type = USB_HUB_REQ_TYPE_CLEAR_PORT_FEATURE,
		.request = USB_DEVREQ_CLEAR_FEATURE,
		.length = 0,
		.index = port_index
	};
	clear_request.value = feature;
	return usb_drv_psync_control_write(hc, target, &clear_request,
	    sizeof(clear_request), NULL, 0);
}



#endif	/* USBHUB_PRIVATE_H */

/**
 * @}
 */