/*
 * Copyright (c) 2013 Jan Vesely
 * Copyright (c) 2017 Ondrej Hlavaty <aearsis@eideo.cz>
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
/**  @addtogroup libusbhost
 * @{
 */
/** @file
 */

#include <macros.h>
#include <str_error.h>
#include <usb/debug.h>
#include <usb/descriptor.h>

#include "ddf_helpers.h"
#include "utility.h"


/**
 * Get max packet size for the control endpoint 0.
 *
 * For LS, HS, and SS devices this value is fixed. For FS devices we must fetch
 * the first 8B of the device descriptor to determine it.
 *
 * @return Max packet size for EP 0
 */
int hc_get_ep0_max_packet_size(uint16_t *mps, bus_t *bus, device_t *dev)
{
	assert(mps);

	static const uint16_t mps_fixed [] = {
		[USB_SPEED_LOW] = 8,
		[USB_SPEED_HIGH] = 64,
		[USB_SPEED_SUPER] = 512,
	};

	if (dev->speed < ARRAY_SIZE(mps_fixed) && mps_fixed[dev->speed] != 0) {
		*mps = mps_fixed[dev->speed];
		return EOK;
	}

	const usb_target_t control_ep = {{
		.address = dev->address,
		.endpoint = 0,
	}};

	usb_standard_device_descriptor_t desc = { 0 };
	const usb_device_request_setup_packet_t get_device_desc_8 =
	    GET_DEVICE_DESC(CTRL_PIPE_MIN_PACKET_SIZE);

	usb_log_debug("Requesting first 8B of device descriptor to determine MPS.");
	ssize_t got = bus_device_send_batch_sync(dev, control_ep, USB_DIRECTION_IN,
	    (char *) &desc, CTRL_PIPE_MIN_PACKET_SIZE, *(uint64_t *)&get_device_desc_8,
	    "read first 8 bytes of dev descriptor");

	if (got != CTRL_PIPE_MIN_PACKET_SIZE) {
		const int err = got < 0 ? got : EOVERFLOW;
		usb_log_error("Failed to get 8B of dev descr: %s.", str_error(err));
		return err;
	}

	if (desc.descriptor_type != USB_DESCTYPE_DEVICE) {
		usb_log_error("The device responded with wrong device descriptor.");
		return EIO;
	}

	uint16_t version = uint16_usb2host(desc.usb_spec_version);
	if (version < 0x0300) {
		/* USB 2 and below have MPS raw in the field */
		*mps = desc.max_packet_size;
	} else {
		/* USB 3 have MPS as an 2-based exponent */
		*mps = (1 << desc.max_packet_size);
	}
	return EOK;
}

/** Check setup packet data for signs of toggle reset.
 *
 * @param[in] requst Setup requst data.
 *
 * @retval -1 No endpoints need reset.
 * @retval 0 All endpoints need reset.
 * @retval >0 Specified endpoint needs reset.
 *
 */
toggle_reset_mode_t hc_get_request_toggle_reset_mode(const usb_device_request_setup_packet_t *request)
{
	assert(request);
	switch (request->request)
	{
	/* Clear Feature ENPOINT_STALL */
	case USB_DEVREQ_CLEAR_FEATURE: /*resets only cleared ep */
		/* 0x2 ( HOST to device | STANDART | TO ENPOINT) */
		if ((request->request_type == 0x2) &&
		    (request->value == USB_FEATURE_ENDPOINT_HALT))
			return RESET_EP;
		break;
	case USB_DEVREQ_SET_CONFIGURATION:
	case USB_DEVREQ_SET_INTERFACE:
		/* Recipient must be device, this resets all endpoints,
		 * In fact there should be no endpoints but EP 0 registered
		 * as different interfaces use different endpoints,
		 * unless you're changing configuration or alternative
		 * interface of an already setup device. */
		if (!(request->request_type & SETUP_REQUEST_TYPE_DEVICE_TO_HOST))
			return RESET_ALL;
		break;
	default:
		break;
	}

	return RESET_NONE;
}

int hc_get_device_desc(device_t *device, usb_standard_device_descriptor_t *desc)
{
	const usb_target_t control_ep = {{
		.address = device->address,
		.endpoint = 0,
	}};

	/* Get std device descriptor */
	const usb_device_request_setup_packet_t get_device_desc =
	    GET_DEVICE_DESC(sizeof(*desc));

	usb_log_debug("Device(%d): Requesting full device descriptor.",
	    device->address);
	ssize_t got = bus_device_send_batch_sync(device, control_ep, USB_DIRECTION_IN,
	    (char *) desc, sizeof(*desc), *(uint64_t *)&get_device_desc,
	    "read device descriptor");

	if (got < 0)
		return got;

	return got == sizeof(*desc) ? EOK : EOVERFLOW;
}

int hc_device_explore(device_t *device)
{
	int err;
	usb_standard_device_descriptor_t desc = { 0 };

	if ((err = hc_get_device_desc(device, &desc))) {
		usb_log_error("Device(%d): Failed to get dev descriptor: %s",
		    device->address, str_error(err));
		return err;
	}

	if ((err = hcd_ddf_setup_match_ids(device, &desc))) {
		usb_log_error("Device(%d): Failed to setup match ids: %s", device->address, str_error(err));
		return err;
	}

	return EOK;
}

/** Announce root hub to the DDF
 *
 * @param[in] device Host controller ddf device
 * @return Error code
 */
int hc_setup_virtual_root_hub(hc_device_t *hcd)
{
	int err;

	assert(hcd);

	device_t *dev = hcd_ddf_fun_create(hcd, USB_SPEED_MAX);
	if (!dev) {
		usb_log_error("Failed to create function for the root hub.");
		return ENOMEM;
	}

	ddf_fun_set_name(dev->fun, "roothub");

	/* Assign an address to the device */
	if ((err = bus_device_enumerate(dev))) {
		usb_log_error("Failed to enumerate roothub device: %s", str_error(err));
		goto err_usb_dev;
	}

	if ((err = ddf_fun_bind(dev->fun))) {
		usb_log_error("Failed to register roothub: %s.", str_error(err));
		goto err_enumerated;
	}

	return EOK;

err_enumerated:
	bus_device_gone(dev);
err_usb_dev:
	hcd_ddf_fun_destroy(dev);
	return err;
}

/**
 * @}
 */