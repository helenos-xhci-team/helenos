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

/** @addtogroup libdrv usb
 * @{
 */
/** @file
 * @brief USB interface definition.
 */

#ifndef LIBDRV_USBHC_IFACE_H_
#define LIBDRV_USBHC_IFACE_H_

#include "driver.h"
#include <usb/usb.h>


/** IPC methods for communication with HC through DDF interface.
 *
 * Notes for async methods:
 *
 * Methods for sending data to device (OUT transactions)
 * - e.g. IPC_M_USBHC_INTERRUPT_OUT -
 * always use the same semantics:
 * - first, IPC call with given method is made
 *   - argument #1 is target address
 *   - argument #2 is target endpoint
 *   - argument #3 is buffer size
 * - this call is immediately followed by IPC data write (from caller)
 * - the initial call (and the whole transaction) is answer after the
 *   transaction is scheduled by the HC and acknowledged by the device
 *   or immediately after error is detected
 * - the answer carries only the error code
 *
 * Methods for retrieving data from device (IN transactions)
 * - e.g. IPC_M_USBHC_INTERRUPT_IN -
 * also use the same semantics:
 * - first, IPC call with given method is made
 *   - argument #1 is target address
 *   - argument #2 is target endpoint
 *   - argument #3 is buffer size
 * - the call is not answered until the device returns some data (or until
 *   error occurs)
 * - if the call is answered with EOK, first argument of the answer is buffer
 *   hash that could be used to retrieve the actual data
 *
 * Some special methods (NO-DATA transactions) do not send any data. These
 * might behave as both OUT or IN transactions because communication parts
 * where actual buffers are exchanged are omitted.
 *
 * The mentioned data retrieval can be done any time after receiving EOK
 * answer to IN method.
 * This retrieval is done using the IPC_M_USBHC_GET_BUFFER where
 * the first argument is buffer hash from call answer.
 * This call must be immediately followed by data read-in and after the
 * data are transferred, the initial call (IPC_M_USBHC_GET_BUFFER)
 * is answered. Each buffer can be retrieved only once.
 *
 * For all these methods, wrap functions exists. Important rule: functions
 * for IN transactions have (as parameters) buffers where retrieved data
 * will be stored. These buffers must be already allocated and shall not be
 * touch until the transaction is completed
 * (e.g. not before calling usb_wait_for() with appropriate handle).
 * OUT transactions buffers can be freed immediately after call is dispatched
 * (i.e. after return from wrapping function).
 *
 */
typedef enum {
	/** Tell USB address assigned to device.
	 * Parameters:
	 * - devman handle id
	 * Answer:
	 * - EINVAL - unknown handle or handle not managed by this driver
	 * - ENOTSUP - operation not supported by HC (shall not happen)
	 * - arbitrary error code if returned by remote implementation
	 * - EOK - handle found, first parameter contains the USB address
	 */
	IPC_M_USBHC_GET_ADDRESS,

	/** Asks for data buffer.
	 * See explanation at usb_iface_funcs_t.
	 * This function does not have counter part in functional interface
	 * as it is handled by the remote part itself.
	 */
	IPC_M_USBHC_GET_BUFFER,


	/** Send interrupt data to device.
	 * See explanation at usb_iface_funcs_t (OUT transaction).
	 */
	IPC_M_USBHC_INTERRUPT_OUT,

	/** Get interrupt data from device.
	 * See explanation at usb_iface_funcs_t (IN transaction).
	 */
	IPC_M_USBHC_INTERRUPT_IN,


	/** Start WRITE control transfer.
	 * See explanation at usb_iface_funcs_t (OUT transaction).
	 */
	IPC_M_USBHC_CONTROL_WRITE_SETUP,

	/** Send control-transfer data to device.
	 * See explanation at usb_iface_funcs_t (OUT transaction).
	 */
	IPC_M_USBHC_CONTROL_WRITE_DATA,

	/** Terminate WRITE control transfer.
	 * See explanation at usb_iface_funcs_t (NO-DATA transaction).
	 */
	IPC_M_USBHC_CONTROL_WRITE_STATUS,



	/** Start READ control transfer.
	 * See explanation at usb_iface_funcs_t (OUT transaction).
	 */
	IPC_M_USBHC_CONTROL_READ_SETUP,

	/** Get control-transfer data from device.
	 * See explanation at usb_iface_funcs_t (IN transaction).
	 */
	IPC_M_USBHC_CONTROL_READ_DATA,

	/** Terminate READ control transfer.
	 * See explanation at usb_iface_funcs_t (NO-DATA transaction).
	 */
	IPC_M_USBHC_CONTROL_READ_STATUS,


	/* IPC_M_USB_ */
} usbhc_iface_funcs_t;

/** Callback for outgoing transfer. */
typedef void (*usbhc_iface_transfer_out_callback_t)(device_t *,
    usb_transaction_outcome_t, void *);

/** Callback for incoming transfer. */
typedef void (*usbhc_iface_transfer_in_callback_t)(device_t *,
    usb_transaction_outcome_t, size_t, void *);

/** USB devices communication interface. */
typedef struct {
	int (*tell_address)(device_t *, devman_handle_t, usb_address_t *);
	int (*interrupt_out)(device_t *, usb_target_t,
	    void *, size_t,
	    usbhc_iface_transfer_out_callback_t, void *);
	int (*interrupt_in)(device_t *, usb_target_t,
	    void *, size_t,
	    usbhc_iface_transfer_in_callback_t, void *);
} usbhc_iface_t;


#endif
/**
 * @}
 */