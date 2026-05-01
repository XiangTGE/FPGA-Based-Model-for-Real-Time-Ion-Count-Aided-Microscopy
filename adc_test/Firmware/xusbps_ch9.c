/******************************************************************************
* Copyright (C) 2010 - 2022 Xilinx, Inc.  All rights reserved.
* Copyright (C) 2023 - 2024 Advanced Micro Devices, Inc. All Rights Reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/

/*****************************************************************************/
/**
 * @file xusbps_ch9.c
 *
 * This file contains the implementation of the chapter 9 code for the example.
 *
 *<pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date     Changes
 * ----- ---- -------- ---------------------------------------------------------
 * 1.00a jz  10/10/10 First release
 * 1.04a nm  02/05/13 Fixed CR# 696550.
 *		      Added template code for Vendor request.
 * 1.04a nm  03/04/13 Fixed CR# 704022. Implemented TEST_MODE Feature.
 * 1.06a kpc 11/11/13 Always use global memory for dma operations
 * 2.1   kpc 4/29/14  Align dma buffers to cache line boundary
 * 2.4	 vak 4/01/19  Fixed IAR data_alignment warnings
 * 2.9   nd  3/18/24  Fixed failures reported by CV test suite.
 *</pre>
 ******************************************************************************/

/***************************** Include Files *********************************/


#include "xparameters.h"	/* XPAR parameters */
#include "xusbps.h"		/* USB controller driver */
#include "xusbps_hw.h"		/* USB controller driver */

#include "xusbps_ch9.h"
#include "xusbps_ch9_simple_usb.h"
#include "xil_printf.h"
#include "xil_cache.h"

#include "sleep.h"
#include <xstatus.h>

/* #define CH9_DEBUG */
#define CH9_DEBUG 1

#ifdef CH9_DEBUG
#include <stdio.h>
#define printf xil_printf
#endif

/************************** Constant Definitions *****************************/
/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

static int XUsbPs_StdDevReq(XUsbPs *InstancePtr,
			     XUsbPs_SetupData *SetupData);

static int XUsbPs_HandleVendorReq(XUsbPs *InstancePtr,
				  XUsbPs_SetupData *SetupData);
extern void XUsbPs_ClassReq(XUsbPs *InstancePtr,
			    XUsbPs_SetupData *SetupData);
extern u32 XUsbPs_Ch9SetupDevDescReply(u8 *BufPtr, u32 BufLen);
extern u32 XUsbPs_Ch9SetupCfgDescReply(u8 *BufPtr, u32 BufLen);
extern u32 XUsbPs_Ch9SetupStrDescReply(u8 *BufPtr, u32 BufLen, u8 Index);
extern void XUsbPs_SetConfiguration(XUsbPs *InstancePtr, int ConfigIdx);
extern void XUsbPs_SetConfigurationApp(XUsbPs *InstancePtr,
				       XUsbPs_SetupData *SetupData);
extern void XUsbPs_SetInterfaceHandler(XUsbPs *InstancePtr,
				       XUsbPs_SetupData *SetupData);

/************************** Variable Definitions *****************************/

#ifdef __ICCARM__
#pragma data_alignment = 32
static u8 Response;
#else
static u8 Response ALIGNMENT_CACHELINE;
#endif

/*****************************************************************************/
/**
* This function handles a Setup Data packet from the host.
*
* @param	InstancePtr is a pointer to XUsbPs instance of the controller.
* @param	SetupData is the structure containing the setup request.
*
* @return
*		- XST_SUCCESS if the function is successful.
*		- XST_FAILURE if an Error occurred.
*
* @note		None.
*
******************************************************************************/
int XUsbPs_Ch9HandleSetupPacket(XUsbPs *InstancePtr,
				XUsbPs_SetupData *SetupData)
{
	int Status = XST_SUCCESS;


	switch (SetupData->bmRequestType & XUSBPS_REQ_TYPE_MASK) {
		case XUSBPS_CMD_STDREQ:
			Status = XUsbPs_StdDevReq(InstancePtr, SetupData);
			break;

		case XUSBPS_CMD_CLASSREQ:
			XUsbPs_ClassReq(InstancePtr, SetupData);
			break;

		case XUSBPS_CMD_VENDREQ:

			Status = XUsbPs_HandleVendorReq(InstancePtr, SetupData);
			break;

		default:
			/* Stall on Endpoint 0 */
			XUsbPs_EpStall(InstancePtr, 0, XUSBPS_EP_DIRECTION_IN |
				       XUSBPS_EP_DIRECTION_OUT);
			break;
	}

	return Status;
}

/*****************************************************************************/
/**
* This function handles a standard device request.
*
* @param	InstancePtr is a pointer to XUsbPs instance of the controller.
* @param	SetupData is a pointer to the data structure containing the
*		setup request.
*
* @return	Will return CONFIG_SET if host asked to set config and that was 
*           done successfully. Returns XST_SUCCESS if everything else went 
*           well, and XST_FAILURE if there is an error. 
*
* @note		None.
*
******************************************************************************/
static int XUsbPs_StdDevReq(XUsbPs *InstancePtr,
			     XUsbPs_SetupData *SetupData)
{
	int Status;
	int Error = 0;
	u32 Handler;
	u32 TmpBufferLen = 6;
	u8 *TempPtr;
	XUsbPs_Local	*UsbLocalPtr;

	int ReplyLen;
#ifdef __ICCARM__
#pragma data_alignment = 32
	static u8  	Reply[XUSBPS_REQ_REPLY_LEN];
	static u8 TmpBuffer[10];
#else
	static u8  	Reply[XUSBPS_REQ_REPLY_LEN] ALIGNMENT_CACHELINE;
	static u8 TmpBuffer[10] ALIGNMENT_CACHELINE;
#endif

	TempPtr = (u8 *)&TmpBuffer;

	/* Check that the requested reply length is not bigger than our reply
	 * buffer. This should never happen...
	 */
	if (SetupData->wLength > XUSBPS_REQ_REPLY_LEN) {
		return XST_FAILURE;
	}

	UsbLocalPtr = (XUsbPs_Local *) InstancePtr->UserDataPtr;

	switch (SetupData->bRequest) {

		case XUSBPS_REQ_GET_STATUS:

			switch (SetupData->bmRequestType & XUSBPS_STATUS_MASK) {
				case XUSBPS_STATUS_DEVICE:
					/* It seems we do not have to worry about zeroing out the rest
					 * of the reply buffer even though we are only using the first
					 * two bytes.
					 */
					*((u16 *) &Reply[0]) = 0x1; /* Self powered */
					break;

				case XUSBPS_STATUS_INTERFACE:
					*((u16 *) &Reply[0]) = 0x0;
					break;

				case XUSBPS_STATUS_ENDPOINT: {
						u32 Status;
						int EpNum = SetupData->wIndex;

						Status = XUsbPs_ReadReg(InstancePtr->Config.BaseAddress,
									XUSBPS_EPCRn_OFFSET(EpNum & 0xF));

						if (EpNum & 0x80) { /* In EP */
							if (Status & XUSBPS_EPCR_TXS_MASK) {
								*((u16 *) &Reply[0]) = 1;
							} else {
								*((u16 *) &Reply[0]) = 0;
							}
						} else {	/* Out EP */
							if (Status & XUSBPS_EPCR_RXS_MASK) {
								*((u16 *) &Reply[0]) = 1;
							} else {
								*((u16 *) &Reply[0]) = 0;
							}
						}
						break;
					}

				default:
					;
			}
			XUsbPs_EpBufferSend(InstancePtr, 0, Reply, SetupData->wLength);
			break;

		case XUSBPS_REQ_SET_ADDRESS:

			/* With bit 24 set the address value is held in a shadow
			 * register until the status phase is acked. At which point it
			 * address value is written into the address register.
			 */
			XUsbPs_SetDeviceAddress(InstancePtr, SetupData->wValue);

			/* There is no data phase so ack the transaction by sending a
			 * zero length packet.
			 */
			XUsbPs_EpBufferSend(InstancePtr, 0, NULL, 0);
			break;

		case XUSBPS_REQ_GET_INTERFACE:

			Response = (u8)InstancePtr->CurrentAltSetting;

			/* Ack the host */
			XUsbPs_EpBufferSend(InstancePtr, 0, &Response, 1);

			break;

		case XUSBPS_REQ_GET_DESCRIPTOR:

			/* Get descriptor type. */
			switch ((SetupData->wValue >> 8) & 0xff) {

				case XUSBPS_TYPE_DEVICE_DESC:
				case XUSBPS_TYPE_DEVICE_QUALIFIER:

					/* Set up the reply buffer with the device descriptor
					 * data.
					 */
					ReplyLen = XUsbPs_Ch9SetupDevDescReply(
							   Reply, XUSBPS_REQ_REPLY_LEN);

					ReplyLen = ReplyLen > SetupData->wLength ?
						   SetupData->wLength : ReplyLen;

					if (((SetupData->wValue >> 8) & 0xff) ==
					    XUSBPS_TYPE_DEVICE_QUALIFIER) {
						Reply[0] = (u8)ReplyLen;
						Reply[1] = (u8)0x06;
						Reply[2] = (u8)0x10;	// Used to be 0x0
						Reply[3] = (u8)0x02;
						Reply[4] = (u8)0xFF;	// Used to be something else
						Reply[5] = (u8)0x00;
						Reply[6] = (u8)0x00;
						Reply[7] = (u8)0x40;	// Used to be 0x10
						Reply[8] = (u8)0x01;	// Used to be 0x0 for some reason
						Reply[9] = (u8)0x00;
					}
					Status = XUsbPs_EpBufferSend(InstancePtr, 0,
								     Reply, ReplyLen);
					if (XST_SUCCESS != Status) {
						/* Failure case needs to be handled */
						for (;;);
					}
					break;

				case XUSBPS_TYPE_CONFIG_DESC:

					/* Set up the reply buffer with the configuration
					 * descriptor data.
					 */
					ReplyLen = XUsbPs_Ch9SetupCfgDescReply(
							   Reply, XUSBPS_REQ_REPLY_LEN);

					ReplyLen = ReplyLen > SetupData->wLength ?
						   SetupData->wLength : ReplyLen;

					Status = XUsbPs_EpBufferSend(InstancePtr, 0,
								     Reply, ReplyLen);
					if (XST_SUCCESS != Status) {
						/* Failure case needs to be handled */
						for (;;);
					}
					break;


				case XUSBPS_TYPE_STRING_DESC:

					/* Set up the reply buffer with the string descriptor
					 * data.
					 */
					ReplyLen = XUsbPs_Ch9SetupStrDescReply(
							   Reply, XUSBPS_REQ_REPLY_LEN,
							   SetupData->wValue & 0xFF);

					ReplyLen = ReplyLen > SetupData->wLength ?
						   SetupData->wLength : ReplyLen;

					Status = XUsbPs_EpBufferSend(InstancePtr, 0,
								     Reply, ReplyLen);
					if (XST_SUCCESS != Status) {
						/* Failure case needs to be handled */
						for (;;);
					}
					break;

				// Get request for Microsoft 2.0 BOS descriptor (WORK IN PROGRESS, DEBUG)
                // NOTE: KEEP THIS COMMENTED UNTIL FURTHER NOTICE
				case MS_OS_20_REQ_GET_BOS_DESC:

                    // Check if the request is for USB BOS descriptor
					if (SetupData->wIndex == 0x0000) {
						
						// Get BOS descriptor in reply buffer
						ReplyLen = MSBOSReply(Reply, XUSBPS_REQ_REPLY_LEN);

						ReplyLen = ReplyLen > SetupData->wLength ?
							   SetupData->wLength : ReplyLen;

						Status = XUsbPs_EpBufferSend(InstancePtr, 0,
									     Reply, ReplyLen);
						if (XST_SUCCESS != Status) {
							/* Failure case needs to be handled */
							for (;;);
						}
                        
                        #ifdef CH9_DEBUG
                            printf("Outputted BOS\n"); // DEBUG
                            printf("Size of BOS: %d\n", ReplyLen);
                        #endif
					}

					break;

				default:
					Error = 1;
					break;
			}
			break;


		case XUSBPS_REQ_SET_CONFIGURATION:

			/*
			 *  allow configuration index 0 and 1.
			 */
			if (((SetupData->wValue & 0xff) != 1 ) && ((SetupData->wValue & 0xff) != 0 )) {
				Error = 1;
				break;
			}

			UsbLocalPtr->CurrentConfig = SetupData->wValue & 0xff;


			/* Call the application specific configuration function to
			 * apply the configuration with the given configuration index.
			 */
			XUsbPs_SetConfiguration(InstancePtr,
						UsbLocalPtr->CurrentConfig);

			if (InstancePtr->AppData != NULL) {
				XUsbPs_SetConfigurationApp(InstancePtr, SetupData);
			}

			/* There is no data phase so ack the transaction by sending a
			 * zero length packet.
			 */
			XUsbPs_EpBufferSend(InstancePtr, 0, NULL, 0);


            // Need to let main program know that configuration is set so data can go on buffer
            return CONFIG_SET;

			break;


		case XUSBPS_REQ_GET_CONFIGURATION:

				XUsbPs_EpBufferSend(InstancePtr, 0,
						    &UsbLocalPtr->CurrentConfig, 1);
			break;


		case XUSBPS_REQ_CLEAR_FEATURE:
			switch (SetupData->bmRequestType & XUSBPS_STATUS_MASK) {
				case XUSBPS_STATUS_ENDPOINT:
					if (SetupData->wValue == XUSBPS_ENDPOINT_HALT) {
						int EpNum = SetupData->wIndex;

						if (EpNum & 0x80) {	/* In ep */
							XUsbPs_ClrBits(InstancePtr,
								       XUSBPS_EPCRn_OFFSET(EpNum & 0xF),
								       XUSBPS_EPCR_TXS_MASK);
						} else { /* Out ep */
							XUsbPs_ClrBits(InstancePtr,
								       XUSBPS_EPCRn_OFFSET(EpNum),
								       XUSBPS_EPCR_RXS_MASK);
						}
					}
					/* Ack the host ? */
					XUsbPs_EpBufferSend(InstancePtr, 0, NULL, 0);
					break;

				default:
					Error = 1;
					break;
			}

			break;

		case XUSBPS_REQ_SET_FEATURE:
			switch (SetupData->bmRequestType & XUSBPS_STATUS_MASK) {
				case XUSBPS_STATUS_ENDPOINT:
					if (SetupData->wValue == XUSBPS_ENDPOINT_HALT) {
						int EpNum = SetupData->wIndex;

						if (EpNum & 0x80) {	/* In ep */
							XUsbPs_SetBits(InstancePtr,
								       XUSBPS_EPCRn_OFFSET(EpNum & 0xF),
								       XUSBPS_EPCR_TXS_MASK);

						} else { /* Out ep */
							XUsbPs_SetBits(InstancePtr,
								       XUSBPS_EPCRn_OFFSET(EpNum),
								       XUSBPS_EPCR_RXS_MASK);
						}
					}
					/* Ack the host ? */
					XUsbPs_EpBufferSend(InstancePtr, 0, NULL, 0);

					break;
				case XUSBPS_STATUS_DEVICE:
					if (SetupData->wValue == XUSBPS_TEST_MODE) {
						int TestSel = (SetupData->wIndex >> 8) & 0xFF;

						/* Ack the host, the transition must happen
							after status stage and < 3ms */
						XUsbPs_EpBufferSend(InstancePtr, 0, NULL, 0);
						usleep(1000);

						switch (TestSel) {
							case XUSBPS_TEST_J:
							case XUSBPS_TEST_K:
							case XUSBPS_TEST_SE0_NAK:
							case XUSBPS_TEST_PACKET:
							case XUSBPS_TEST_FORCE_ENABLE:
								XUsbPs_SetBits(InstancePtr, \
									       XUSBPS_PORTSCR1_OFFSET, \
									       TestSel << 16);
								break;
							default:
								/* Unsupported test selector */
								break;
						}
						break;
					}

				default:
					Error = 1;
					break;
			}

			break;


		/* For set interface, check the alt setting host wants */
		case XUSBPS_REQ_SET_INTERFACE:

			/* Only ISO supported */
			if (InstancePtr->AppData != NULL)
				XUsbPs_SetInterfaceHandler(
					(XUsbPs *)InstancePtr, SetupData);

			/* Ack the host after device finishes the operation */
			Error = XUsbPs_EpBufferSend(InstancePtr, 0, NULL, 0);
			if (Error) {

			}
			break;
		case XUSBPS_REQ_SET_SEL:

			Status = XUsbPs_EpBufferReceive((XUsbPs *)InstancePtr,
							0, &TempPtr, &TmpBufferLen, &Handler);
			if (XST_SUCCESS == Status) {
				/* Return the buffer. */
				XUsbPs_EpBufferRelease(Handler);
			}

			break;

		case XUSBPS_REQ_SET_ISOCH_DELAY:

			break;


		default:
			Error = 1;
			break;
	}

	/* Set the send stall bit if there was an error */
	if (Error) {
		XUsbPs_EpStall(InstancePtr, 0, XUSBPS_EP_DIRECTION_IN |
			       XUSBPS_EP_DIRECTION_OUT);
	}


    // Nothing is wrong, return XST_SUCCESS
    return XST_SUCCESS;
}

/*****************************************************************************/
/**
* This function handles a vendor request.
*
* @param	InstancePtr is a pointer to XUsbPs instance of the controller.
* @param	SetupData is a pointer to the data structure containing the
*		setup request.
*
* @return
*		- XST_SUCCESS if successful.
*		- XST_FAILURE if an Error occurred.
*
* @note
*		This function is a template to handle vendor request for control
*		IN and control OUT endpoints. The control OUT endpoint can
*		receive only 64 bytes of data per dTD. For receiving more than
*		64 bytes of vendor data on control OUT endpoint, change the
*		buffer size of the control OUT endpoint. Otherwise the results
*		are unexpected.
*
******************************************************************************/
static int XUsbPs_HandleVendorReq(XUsbPs *InstancePtr,
				  XUsbPs_SetupData *SetupData)
{
	int 	ReplyLen = XUSBPS_REQ_REPLY_LEN;	// We will get 512 byte buffer as reply
	int 	ReplySize;
	u8      *BufferPtr;
	u32     BufferLen;
	u32     Handle;
	u32	Reg;

#ifdef __ICCARM__
#pragma data_alignment = 32
	static u8	Reply[XUSBPS_REQ_REPLY_LEN];// = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
#else
	static u8	Reply[XUSBPS_REQ_REPLY_LEN] ALIGNMENT_CACHELINE;/* =
	{0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};*/
#endif

	u8	    EpNum = 0;
	int 	Status;
	int 	Direction;
	int 	Timeout;

	/* Check the direction, USB 2.0 section 9.3 */
	Direction = SetupData->bmRequestType & (1 << 7);

	if (!Direction) {
		/* Control OUT vendor request */
		if (SetupData->wLength > 0) {
			/* Re-Prime the endpoint to receive Setup DATA */
			XUsbPs_EpPrime(InstancePtr, 0, XUSBPS_EP_DIRECTION_OUT);

			/* Check whether EP prime is successful or not */
			Timeout = XUSBPS_TIMEOUT_COUNTER;
			do {
				Reg = XUsbPs_ReadReg(InstancePtr->Config.BaseAddress,
						     XUSBPS_EPPRIME_OFFSET);
			} while (((Reg & (1 << EpNum)) == 1) && --Timeout);

			if (!Timeout) {
				return XST_FAILURE;
			}

			/* Get the Setup DATA, don't wait for the interrupt */
			Timeout = XUSBPS_TIMEOUT_COUNTER;
			do {
				Status = XUsbPs_EpBufferReceive(InstancePtr,
								EpNum, &BufferPtr, &BufferLen, &Handle);
			} while ((Status != XST_SUCCESS) && --Timeout);

			if (!Timeout) {
				return XST_FAILURE;
			}

			Xil_DCacheInvalidateRange((unsigned int)BufferPtr,
						  BufferLen);

			if (Status == XST_SUCCESS) {
				/* Zero length ACK */
				Status = XUsbPs_EpBufferSend(InstancePtr, EpNum,
							     NULL, 0);
				if (Status != XST_SUCCESS) {
					return XST_FAILURE;
				}
			}
		}
	} else {

		// Support for MS 2.0 descriptor set request
		// If vendor code is 0x1, which is what we set it as (WORK IN PROGRESS)
		if (SetupData->bRequest == MS_OS_20_VENDOR_CODE && SetupData->wValue == 0 && SetupData->wIndex == MS_OS_20_DESCRIPTOR_INDEX) {

			ReplySize = MSDescSetReply(Reply, ReplyLen, SetupData->bRequest);

            ReplySize = ReplyLen > SetupData->wLength ?
                    SetupData->wLength : ReplyLen;

			if (ReplySize == 0) {
				return XST_FAILURE;
			} else {

				Status = XUsbPs_EpBufferSend(InstancePtr, EpNum,
								     Reply, ReplySize);
				if (XST_SUCCESS != Status) {
					/* Failure case needs to be handled */
					for (;;);

					// DEBUG
					#ifdef CH9_DEBUG
						printf("Failed to return MS 2.0 Capability Descriptor\n");
					#endif

                    return XST_FAILURE;
				} else {

                    // DEBUG
					#ifdef CH9_DEBUG
						printf("Successfully returned MS 2.0 Capability Descriptor!\n");
					#endif

                    return XST_SUCCESS;
                }
			}
		} //else if (SetupData->bRequest == MS_OS_20_REQ_GET_CAP_DESC && SetupData->wValue == 0 && SetupData->wIndex == MS_OS_20_DESCRIPTOR_INDEX) {

        //     // Get BOS descriptor in reply buffer
        //     ReplySize = MSBOSReply(Reply, ReplyLen);

        //     ReplySize = ReplyLen > SetupData->wLength ?
        //             SetupData->wLength : ReplyLen;

        //     if (ReplySize == 0) {

        //         return XST_FAILURE;
        //     } else {
            
        //         Status = XUsbPs_EpBufferSend(InstancePtr, EpNum,
        //                             Reply, ReplySize);
        //         if (XST_SUCCESS != Status) {
        //             /* Failure case needs to be handled */
        //             for (;;);

        //             // DEBUG
		// 			#ifdef CH9_DEBUG
		// 				printf("Failed to return MS 2.0 Capability Descriptor\n");
		// 				while (1);	// Stall
		// 			#endif

        //             return XST_FAILURE;
        //         } else {

        //             return XST_SUCCESS;
        //         }
        //     }
        // }


		// Not sure what this does...
		if (SetupData->wLength > 0) {
			/* Control IN vendor request */
			Status = XUsbPs_EpBufferSend(InstancePtr, EpNum, Reply,
						     SetupData->wLength);
			if (Status != XST_SUCCESS) {
				return XST_FAILURE;
			}
		}
	}
	return XST_SUCCESS;
}

/****************************************************************************/
/**
 * Set the Config state
 *
 * @param	InstancePtr is a private member of Usb_DevData instance.
 * @param	Flag is the config value.
 *
 * @return	None.
 *
 * @note		None.
 *
 *****************************************************************************/
void XUsbPs_SetConfigDone(void *InstancePtr, u8 Flag)
{
	((XUsbPs *)InstancePtr)->IsConfigDone = Flag;
}

/****************************************************************************/
/**
 * Get the Config state
 *
 * @param	InstancePtr is a private member of Usb_DevData instance.
 *
 * @return	Current configuration value
 *
 * @note		None.
 *
 *****************************************************************************/
u8 XUsbPs_GetConfigDone(void *InstancePtr)
{
	return (((XUsbPs *)InstancePtr)->IsConfigDone);
}
