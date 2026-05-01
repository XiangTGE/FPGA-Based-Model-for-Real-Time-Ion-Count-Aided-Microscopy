/******************************************************************************
* Copyright (C) 2010 - 2022 Xilinx, Inc.  All rights reserved.
* Copyright (C) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/

/*****************************************************************************/
/**
 * @file xusbps_ch9_simple_usb.c
 *
 * This file contains the implementation of the storage specific chapter 9 code
 * for the example.
 *
 *<pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date     Changes
 * ----- ---- -------- ---------------------------------------------------------
 * 1.00a wgr  10/10/10 First release
 * 2.5	 pm   02/20/20 Added SetConfigurationApp and SetInterfaceHandler API to
 *			make ch9 common framework to all example.
 *</pre>
 ******************************************************************************/

/***************************** Include Files *********************************/

#include <string.h>
#include <xil_types.h>

#include "xparameters.h"	/* XPAR parameters */
#include "xusbps.h"		/* USB controller driver */

#include "xusbps_ch9.h"
#include "xusbps_ch9_simple_usb.h"

/************************** Constant Definitions *****************************/

/***************** Macros (Inline Functions) Definitions *********************/

/**************************** Type Definitions *******************************/


/* #define CH9_DEBUG */
#define CH9_DEBUG 1

#ifdef CH9_DEBUG
#include <stdio.h>
#define printf xil_printf
#endif


#ifdef __ICCARM__
#pragma pack(push, 1)
#endif
typedef struct {
	u8  bLength;
	u8  bDescriptorType;
	u16 bcdUSB;
	u8  bDeviceClass;
	u8  bDeviceSubClass;
	u8  bDeviceProtocol;
	u8  bMaxPacketSize0;
	u16 idVendor;
	u16 idProduct;
	u16 bcdDevice;
	u8  iManufacturer;
	u8  iProduct;
	u8  iSerialNumber;
	u8  bNumConfigurations;
#ifdef __ICCARM__
} USB_STD_DEV_DESC;
#pragma pack(pop)
#else
} __attribute__((__packed__))USB_STD_DEV_DESC;
#endif

#ifdef __ICCARM__
#pragma pack(push, 1)
#endif
typedef struct {
	u8  bLength;
	u8  bDescriptorType;
	u16 wTotalLength;
	u8  bNumInterfaces;
	u8  bConfigurationValue;
	u8  iConfiguration;
	u8  bmAttributes;
	u8  bMaxPower;
#ifdef __ICCARM__
} USB_STD_CFG_DESC;
#pragma pack(pop)
#else
} __attribute__((__packed__))USB_STD_CFG_DESC;
#endif

#ifdef __ICCARM__
#pragma pack(push, 1)
#endif
typedef struct {
	u8  bLength;
	u8  bDescriptorType;
	u8  bInterfaceNumber;
	u8  bAlternateSetting;
	u8  bNumEndPoints;
	u8  bInterfaceClass;
	u8  bInterfaceSubClass;
	u8  bInterfaceProtocol;
	u8  iInterface;
#ifdef __ICCARM__
} USB_STD_IF_DESC;
#pragma pack(pop)
#else
} __attribute__((__packed__))USB_STD_IF_DESC;
#endif

#ifdef __ICCARM__
#pragma pack(push, 1)
#endif
typedef struct {
	u8  bLength;
	u8  bDescriptorType;
	u8  bEndpointAddress;
	u8  bmAttributes;
	u16 wMaxPacketSize;
	u8  bInterval;
#ifdef __ICCARM__
} USB_STD_EP_DESC;
#pragma pack(pop)
#else
} __attribute__((__packed__))USB_STD_EP_DESC;
#endif

#ifdef __ICCARM__
#pragma pack(push, 1)
#endif
typedef struct {
	u8  bLength;
	u8  bDescriptorType;
	u16 wLANGID[1];
#ifdef __ICCARM__
} USB_STD_STRING_DESC;
#pragma pack(pop)
#else
} __attribute__((__packed__))USB_STD_STRING_DESC;
#endif

#ifdef __ICCARM__
#pragma pack(push, 1)
#endif
typedef struct {
	USB_STD_CFG_DESC stdCfg;
	USB_STD_IF_DESC ifCfg;
	USB_STD_EP_DESC epCfg1;
	USB_STD_EP_DESC epCfg2;
#ifdef __ICCARM__
} USB_CONFIG;
#pragma pack(pop)
#else
} __attribute__((__packed__))USB_CONFIG;
#endif


/****Microsoft OS 2.0 support****/

/*Microsoft 2.0 Platform Capability Descriptor*/
/*
	MS_OS_20_Platform_Capability_ID = D8DD60DF-4589-4CC7-9CD2-659D9E648A9F
*/
typedef struct {

	u8	 bLength;
	u8	 bDescriptorType;
	u8   bDevCapabilityType;
	u8   bReserved;
	u8  MS_OS_20_Platform_Capability_ID[16];
} MS_20_CAPABILITY_HEADER;

// Descriptor set that goes with a particular Windows version (and beyond)
typedef struct {

	u8	 dwWindowsVersion[4];
	u8	 wMSOSDescriptorSetTotalLength[2];
	u8   bMS_VendorCode;
	u8   bAltEnumCode;
} MS_20_DESC_SET_INFO;

// Custom platform capability descriptor for this device
typedef struct {

	MS_20_CAPABILITY_HEADER platform_header;
	MS_20_DESC_SET_INFO desc_set_info;
} CAPABILITY_DESC;


// Microsoft 2.0 BOS Descriptor
typedef struct {

	u8   bLength;						// Descriptor size (BOS part only; 5 bytes)
	u8 	 bDescriptorType;				// BOS descriptor type (0x0F)
	u8   bBOSTotalLength[2];			// Length of BOS and subordinate descriptors
	u8   bNumSubordinates;				// Number of subordinate descriptors
	CAPABILITY_DESC cap_desc;
} MS_20_BOS_DESC;


/*Microsoft OS 2.0 descriptor set contents*/

// Microsoft OS 2.0 descriptor set header
typedef struct {

	u8	  wLength[2];
	u8	  wDescriptorType[2];
	u8    dwWindowsVersion[4];
	u8    wTotalLength[2];
} MS_20_DESC_SET_HEADER;

// Microsoft OS 2.0 configuration subset header
typedef struct {

	u8    wLength[2];
	u8	  wDescriptorType[2];
	u8    bConfigurationValue;
	u8    bReserved;
	u8    wTotalLength[2];
} MS_20_CONFIG_SUBSET_HEADER;

// Microsoft OS 2.0 function subset header
typedef struct {

	u16   wLength;
	u16	  wDescriptorType;
	u8 	  bFirstInterface;
	u8    bReserved;
	u16   wSubsetLength;
} MS_20_FUNC_SUBSET_HEADER;

// Microsoft OS 2.0 Compatible ID feature descriptor
typedef struct {

	u8    wLength[2];
	u8	  wDescriptorType[2];
	u8    CompatibleID[8];
	u8    SubCompatibleID[8];
} MS_20_FEATURE_COMPAT_ID_DESC;

// Microsoft OS 2.0 Registry Property Feature Descriptor (work in progress)
typedef struct {

	u8    wLength[2];
	u8    wDescriptorType[2];
	u8    wPropertyDataType[2];
	u8    wPropertyNameLength[2];
	u8    PropertyName[40];			// Specifically for the property "DeviceInterfaceGUID"
	u8    wPropertyDataLength[2];
	u8    PropertyData[78];			// Should always be 78 bytes at least for GUID
} MS_20_FEATURE_REG_PROP_DESC;


// Custom Microsoft 2.0 descriptor set layout for this device (WORK IN PROGRESS)
typedef struct {

	MS_20_DESC_SET_HEADER desc_header;
	MS_20_FEATURE_COMPAT_ID_DESC compatible_id;
	MS_20_FEATURE_REG_PROP_DESC guid;
} DEVICE_DESC_SET;


/************************** Function Prototypes ******************************/

/************************** Variable Definitions *****************************/

#define USB_ENDPOINT0_MAXP				0x40

#define USB_BULKIN_EP					1
#define USB_BULKOUT_EP					1

#define USB_DEVICE_DESC					0x01
#define USB_CONFIG_DESC					0x02
#define USB_STRING_DESC					0x03
#define USB_INTERFACE_CFG_DESC			0x04
#define USB_ENDPOINT_CFG_DESC			0x05


/****Microsoft OS 2.0 support****/

// Descriptor types
#define MS_OS_20_SET_HEADER_DESC		0x00
#define MS_OS_20_SUBSET_HEADER_CONFIG	0x01
#define MS_OS_20_SUBSET_HEADER_FUNC		0x02
#define MS_OS_20_FEATURE_COMPATIBLE_ID	0x03
#define MS_OS_20_FEATURE_REG_PROPERTY	0x04
#define MS_OS_20_FEATURE_MIN_RESUM_TIME 0x05
#define MS_OS_20_FEATURE_MODEL_ID		0x06
#define MS_OS_20_FEATURE_CCGP_DEVICE	0x07
#define MS_OS_20_FEATURE_VENDOR_REV		0x08
#define MS_OS_20_BOS 					0x0F
#define MS_OS_20_CAPABILITY				0x10

// Capability types
#define PLATFORM_CAPABILITY 			0x05

// Capability UUID
#define MS_CAPABILITY_UUID              0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C, 0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F
// 0x9F, 0x8A, 0x64, 0x9E, 0x9D, 0x65, 0xCD, 0x79, 0xCC, 0x94, 0x58, 0xF4, 0x0D, 0xD6, 0x8D, 0x0D

// Windows versions (DWORD values)
#define WIN_8_1							0x00, 0x00, 0x03, 0x06

// wIndex values
#define MS_OS_20_DESCRIPTOR_INDEX		0x07
#define MS_OS_20_SET_ALT_ENUMERATION	0x08

/*****************************************************************************/
/**
*
* This function returns the device descriptor for the device.
*
* @param	BufPtr is pointer to the buffer that is to be filled
*		with the descriptor.
* @param	BufLen is the size of the provided buffer.
*
* @return 	Length of the descriptor in the buffer on success.
*		0 on error.
*
******************************************************************************/
u32 XUsbPs_Ch9SetupDevDescReply(u8 *BufPtr, u32 BufLen)
{
	USB_STD_DEV_DESC deviceDesc = {
		sizeof(USB_STD_DEV_DESC),	/* bLength */
		USB_DEVICE_DESC,			/* bDescriptorType */
		be2les(0x0201),				/* bcdUSB 2.1 */
		0x00,						/* bDeviceClass */
		0x00,						/* bDeviceSubClass */
		0x00,						/* bDeviceProtocol */
		USB_ENDPOINT0_MAXP,			/* bMaxPackedSize0 */
		be2les(0x0d7d),				/* idVendor */
		be2les(0x0204),				/* idProduct */
		be2les(0x0200),				/* bcdDevice */
		0x01,						/* iManufacturer */
		0x02,						/* iProduct */
		0x03,						/* iSerialNumber */
		0x01						/* bNumConfigurations */
	};

	/* Check buffer pointer is there and buffer is big enough. */
	if (!BufPtr) {
		return 0;
	}

	if (BufLen < sizeof(USB_STD_DEV_DESC)) {
		return 0;
	}

	memcpy(BufPtr, &deviceDesc, sizeof(USB_STD_DEV_DESC));

	return sizeof(USB_STD_DEV_DESC);
}



/*****************************************************************************/
/**
*
* This function returns the configuration descriptor for the device.
*
* @param	BufPtr is the pointer to the buffer that is to be filled with
*		the descriptor.
* @param	BufLen is the size of the provided buffer.
*
* @return 	Length of the descriptor in the buffer on success.
*		0 on error.
*
******************************************************************************/
u32 XUsbPs_Ch9SetupCfgDescReply(u8 *BufPtr, u32 BufLen)
{
	USB_CONFIG config = {
		/* Std Config */
		{
			sizeof(USB_STD_CFG_DESC),	/* bLength */
			USB_CONFIG_DESC,			/* bDescriptorType */
			be2les(sizeof(USB_CONFIG)),	/* wTotalLength */
			0x01,						/* bNumInterfaces */
			0x01,						/* bConfigurationValue */
			0x04,						/* iConfiguration */
			0xc0,						/* bmAttribute */
			0x00						/* bMaxPower  */
		},				

		/* Interface Config */
		{
			sizeof(USB_STD_IF_DESC),	/* bLength */
			USB_INTERFACE_CFG_DESC,		/* bDescriptorType */
			0x00,						/* bInterfaceNumber */
			0x00,						/* bAlternateSetting */
			0x02,						/* bNumEndPoints */
			0xFF,						/* bInterfaceClass */
			0xFF,						/* bInterfaceSubClass */
			0x00,						/* bInterfaceProtocol */
			0x05						/* iInterface */
		},				

		/* Bulk Out Endpoint Config */
		{
			sizeof(USB_STD_EP_DESC),	/* bLength */
			USB_ENDPOINT_CFG_DESC,		/* bDescriptorType */
			0x00 | USB_BULKOUT_EP,		/* bEndpointAddress */
			0x02,						/* bmAttribute  */
			be2les(0x200),				/* wMaxPacketSize */
			0x00						/* bInterval */
		},				

		/* Bulk In Endpoint Config */
		{
			sizeof(USB_STD_EP_DESC),	/* bLength */
			USB_ENDPOINT_CFG_DESC,		/* bDescriptorType */
			0x80 | USB_BULKIN_EP,		/* bEndpointAddress */
			0x02,						/* bmAttribute  */
			be2les(0x200),				/* wMaxPacketSize */
			0x00						/* bInterval */
		}				
	};

	/* Check buffer pointer is OK and buffer is big enough. */
	if (!BufPtr) {
		return 0;
	}

	if (BufLen < sizeof(USB_STD_DEV_DESC)) {
		return 0;
	}

	memcpy(BufPtr, &config, sizeof(USB_CONFIG));

	return sizeof(USB_CONFIG);
}



/*****************************************************************************/
/**
*
* This function returns a string descriptor for the given index.
*
* @param	BufPtr is a  pointer to the buffer that is to be filled with
*		the descriptor.
* @param	BufLen is the size of the provided buffer.
* @param	Index is the index of the string for which the descriptor
*		is requested.
*
* @return 	Length of the descriptor in the buffer on success.
*		0 on error.
*
******************************************************************************/
u32 XUsbPs_Ch9SetupStrDescReply(u8 *BufPtr, u32 BufLen, u8 Index)
{
	int i;

	static char *StringList[] = {
		"UNUSED",
		"BU_ECE",
		"Real-Time ICAM Device",
		"2A49876D9CC1AA5",        // Last char changed from "4" to "5"
		"Default Configuration",
		"Default Interface",
	};
	char *String;
	u32 StringLen;
	u32 DescLen;
	u8 TmpBuf[128];

	USB_STD_STRING_DESC *StringDesc;

	if (!BufPtr) {
		return 0;
	}

	if (Index >= sizeof(StringList) / sizeof(char *)) {
		return 0;
	}

	String = StringList[Index];
	StringLen = strlen(String);

	StringDesc = (USB_STD_STRING_DESC *) TmpBuf;

	/* Index 0 is special as we can not represent the string required in
	 * the table above. Therefore we handle index 0 as a special case.
	 */
	if (0 == Index) {
		StringDesc->bLength = 4;
		StringDesc->bDescriptorType = USB_STRING_DESC;
		StringDesc->wLANGID[0] = be2les(0x0409);
	}
	/* All other strings can be pulled from the table above. */
	else {
		StringDesc->bLength = StringLen * 2 + 2;
		StringDesc->bDescriptorType = USB_STRING_DESC;

		for (i = 0; i < StringLen; i++) {
			StringDesc->wLANGID[i] = be2les((u16) String[i]);
		}
	}
	DescLen = StringDesc->bLength;

	/* Check if the provided buffer is big enough to hold the descriptor. */
	if (DescLen > BufLen) {
		return 0;
	}

	memcpy(BufPtr, StringDesc, DescLen);

	return DescLen;
}



/*****************************************************************************/
/**
* This function handles a "set configuration" request.
*
* @param	InstancePtr is a pointer to XUsbPs instance of the controller.
* @param	ConfigIdx is the Index of the desired configuration.
*
* @return	None
*
******************************************************************************/
void XUsbPs_SetConfiguration(XUsbPs *InstancePtr, int ConfigIdx)
{
	Xil_AssertVoid(InstancePtr != NULL);

	/* We only have one configuration. Its index is 1. Ignore anything
	 * else.
	 */
	if (1 != ConfigIdx) {
		return;
	}

	XUsbPs_EpEnable(InstancePtr, 1, XUSBPS_EP_DIRECTION_OUT);
	XUsbPs_EpEnable(InstancePtr, 1, XUSBPS_EP_DIRECTION_IN);

	/* Set BULK mode for both directions.  */
	XUsbPs_SetBits(InstancePtr, XUSBPS_EPCR1_OFFSET,
		       XUSBPS_EPCR_TXT_BULK_MASK |
		       XUSBPS_EPCR_RXT_BULK_MASK |
		       XUSBPS_EPCR_TXR_MASK |
		       XUSBPS_EPCR_RXR_MASK);

	/* Prime the OUT endpoint. */
	XUsbPs_EpPrime(InstancePtr, 1, XUSBPS_EP_DIRECTION_OUT);
}



/****************************************************************************/
/**
 * This function is called by Chapter9 handler when SET_CONFIGURATION command
 * is received from Host.
 *
 * @param	InstancePtr is pointer to XUsbPs instance of the controller.
 * @param	SetupData is the setup packet received from Host.
 *
 * @return
 *		- XST_SUCCESS if successful,
 *		- XST_FAILURE if unsuccessful.
 *
 * @note
 *		Non control endpoints must be enabled after SET_CONFIGURATION
 *		command since hardware clears all previously enabled endpoints
 *		except control endpoints when this command is received.
 *
 *****************************************************************************/
void XUsbPs_SetConfigurationApp(XUsbPs *InstancePtr,
				XUsbPs_SetupData *SetupData)
{
	(void)InstancePtr;
	(void)SetupData;
}



/****************************************************************************/
/**
 * This function is called by Chapter9 handler when SET_CONFIGURATION command
 * or SET_INTERFACE command is received from Host.
 *
 * @param	InstancePtr is pointer to XUsbPs instance of the controller.
 * @param	SetupData is the setup packet received from Host.
 *
 * @note
 *
 *****************************************************************************/
void XUsbPs_SetInterfaceHandler(XUsbPs *InstancePtr,
				XUsbPs_SetupData *SetupData)
{
	(void)InstancePtr;
	(void)SetupData;
}



/*****************************************************************************/
/**
* This function does nothing because the device class is configured as vendor-specific
*
* @param	InstancePtr is a pointer to XUsbPs instance of the controller.
* @param	SetupData is the setup data structure containing the setup
*		request.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
void XUsbPs_ClassReq(XUsbPs *InstancePtr, XUsbPs_SetupData *SetupData) {

    (void)InstancePtr;
    (void)SetupData;
}



/*****************************************************************************/
/**
*
* This function will return the MS BOS descriptor that has the capability
* descriptor
*
* @param	BufPtr is a  pointer to the buffer that is to be filled with
*		the descriptor.
* @param	BufLen is the size of the provided buffer.
*
* @return 	Length of the descriptor in the buffer on success.
*		0 on error.
*
******************************************************************************/
u32 MSBOSReply (u8 *BufPtr, u32 BufLen) {

	// Create capability descriptor (subordinate to BOS descriptor)
	// CAPABILITY_DESC cap_desc = {

	// 	// Capability header
	// 	{
	// 		//sizeof(CAPABILITY_DESC),		        // bLength (of the entire capability descriptor)
	// 		0x1C,
    //         MS_OS_20_CAPABILITY,			        // bDescriptorType
	// 		PLATFORM_CAPABILITY,	        		// bDevCapabilityType
	// 		0x00,									// bReserved
	// 		MS_CAPABILITY_UUID,		                // MS_OS_20_Platform_Capability_ID
	// 	},

	// 	// Descriptor set info
	// 	{
	// 		WIN_8_1,								// dwWindowsVersion (WIN32_WINNT_WINBLUE from sdkddver.h - Windows 8.1 or later)
	// 		//be2les(sizeof(DEVICE_DESC_SET)),		// wMSOSDescriptorSetTotalLength
	// 		0x9E, 0x00,
    //         0x01,									// bMS_VendorCode
	// 		0x00									// bAltEnumCode
	// 	}
	// };

	// BOS descriptor that will identify this device as compliant with Microsoft OS 2.0 Descriptors 
	// MS_20_BOS_DESC bos = {

    //     // BOS "header"
	// 	{
    //         0x05,										// Descriptor size
    //         MS_OS_20_BOS,								// Descriptor type
    //         //be2les(5+sizeof(CAPABILITY_DESC)),		// Length of this + subordinate descriptors
    //         0x21, 0x00,
    //         0x01                                        // Number of subordinates
    //     },										

    //     // Capability descriptor header        
    //     {
	// 		//sizeof(CAPABILITY_DESC),		        // bLength (of the entire capability descriptor)
	// 		0x1C,
    //         MS_OS_20_CAPABILITY,			        // bDescriptorType
	// 		PLATFORM_CAPABILITY,	        		// bDevCapabilityType
	// 		0x00,									// bReserved
	// 		MS_CAPABILITY_UUID,		                // MS_OS_20_Platform_Capability_ID
	// 	},

	// 	// Descriptor set info
	// 	{
	// 		WIN_8_1,								// dwWindowsVersion (WIN32_WINNT_WINBLUE from sdkddver.h - Windows 8.1 or later)
	// 		//be2les(sizeof(DEVICE_DESC_SET)),		// wMSOSDescriptorSetTotalLength
	// 		0x9E, 0x00,
    //         0x01,									// bMS_VendorCode
	// 		0x00									// bAltEnumCode
	// 	}
	// };


    MS_20_BOS_DESC bos = {

        0x05,
        0x0F,
        0x21, 0x00,
        0x01,

        0x1C,
        0x10,
        0x05,
        0x00,

        0xDF, 0x60, 0xDD, 0xD8,
        0x89, 0x45, 
        0xC7, 0x4C,
        0x9C, 0xD2,
        0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F,

        0x00, 0x00, 0x03, 0x06,
        0x9E, 0x00,
        0x01, 0x00
    };


	/* Check buffer pointer is OK and buffer is big enough. */
	if (!BufPtr) {
		return 0;
	}

	if (BufLen < sizeof(MS_20_BOS_DESC)) {
		return 0;
	}

	memcpy(BufPtr, &bos, 0x21);

	return 0x21;
}



/*****************************************************************************/
/**
*
* This function will return the specified 
*
* @param	BufPtr is a  pointer to the buffer that is to be filled with
*		the descriptor.
* @param	BufLen is the size of the provided buffer.
* param 	VendorCode is the vendor code that specifies the particular 
* descriptor set
*
* @return 	Length of the descriptor in the buffer on success.
*		0 on error.
*
******************************************************************************/
u32 MSDescSetReply (u8 *BufPtr, u32 BufLen, u8 VendorCode) {

	// Create char array for property name
	// char PropertyNameOG[] = "DeviceInterfaceGUID";
	// char PropertyName[2*sizeof(PropertyNameOG)];
	// for (int i = 0; i < sizeof(PropertyNameOG); i++) {

	// 	PropertyName[2*i] = PropertyNameOG[i];
	// 	PropertyName[2*i+1] = 0x00;
	// }

	// Create char array for GUID (property data)
	// char GUIDOG[] = "{0e135747-d2cf-45eb-aa15-80abeb6bafc7}";
	// char GUID[2*sizeof(GUIDOG)];
	// for (int i = 0; i < sizeof(GUIDOG); i++) {

	// 	GUID[2*i] = GUIDOG[i];
	// 	GUID[2*i+1] = 0x00;
	// }

    // Construct the descriptor set (vendor id = 0x01)
    // DEVICE_DESC_SET desc_set = {

    //     // Device descriptor header
    //     {
    //         be2les(sizeof(MS_20_DESC_SET_HEADER)),			// wLength
    //         be2les(MS_OS_20_SET_HEADER_DESC),				// wDescriptorType	
    //         WIN_8_1,										// dwWindowsVersion
    //         be2les(sizeof(DEVICE_DESC_SET))					// wTotalLength
    //     },

    //     // Compatible ID feature descriptor
    //     {
    //         be2les(sizeof(MS_20_FEATURE_COMPAT_ID_DESC)),	// wLength
    //         be2les(MS_OS_20_FEATURE_COMPATIBLE_ID),			// wDescriptorType
    //         'W','I','N','U','S','B','\0','\0',				// CompatibleID
    //         0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0			// SubCompatibleID
    //     },

    //     // GUID (reg property) feature descriptor (work in progress)
    //     {
    //         be2les(128),									// wLength
    //         be2les(MS_OS_20_FEATURE_REG_PROPERTY),			// wDescriptorTyp
    //         be2les(0x0001),									// wPropertyDataType (NULL terminated Unicode string)
    //         be2les(sizeof(PropertyName)),					// wPropertyNameLength
    //         *PropertyName,									// PropertyName
    //         be2les(sizeof(GUID)),							// wPropertyDataLength
    //         *GUID											// PropertyData
    //     }
	// }; 

    // Descriptor set with explicit bytes
    DEVICE_DESC_SET desc_set = {

        // Device descriptor header
            0x0A, 0x00,
            0x00, 0x00,
            0x00, 0x00, 0x03, 0x06,
            0x9E, 0x00,

        // Compatible ID feature descriptor
            0x14, 0x00,
            0x03, 0x00,
            0x57, 0x49, 0x4E, 0x55, 0x53, 0x42, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

        // GUID (reg property) feature descriptor
            0x80, 0x00,
            0x04, 0x00,
            0x01, 0x00,
            0x28, 0x00,

            // Property name - "DeviceInterfaceGUID"
            0x44, 0x00, 0x65, 0x00, 0x76, 0x00, 0x69, 0x00, 0x63, 0x00, 0x65, 0x00,
            0x49, 0x00, 0x6E, 0x00, 0x74, 0x00, 0x65, 0x00, 0x72, 0x00, 0x66, 0x00,
            0x61, 0x00, 0x63, 0x00, 0x65, 0x00, 0x47, 0x00, 0x55, 0x00, 0x49, 0x00,
            0x44, 0x00, 0x00, 0x00,

            // Size of property data
            0x4E, 0x00,

            // Property data - "{0e135747-d2cf-45eb-aa15-80abeb6bafc7}", null terminated unicode
            0x7B, 0x00, 0x30, 0x00, 0x65, 0x00, 0x31, 0x00, 0x33, 0x00, 0x35, 0x00, 
            0x37, 0x00, 0x34, 0x00, 0x37, 0x00, 0x2D, 0x00, 0x64, 0x00, 0x32, 0x00,
            0x63, 0x00, 0x66, 0x00, 0x2D, 0x00, 0x34, 0x00, 0x35, 0x00, 0x65, 0x00,
            0x62, 0x00, 0x2D, 0x00, 0x61, 0x00, 0x61, 0x00, 0x31, 0x00, 0x35, 0x00,
            0x2D, 0x00, 0x38, 0x00, 0x30, 0x00, 0x61, 0x00, 0x62, 0x00, 0x65, 0x00,
            0x62, 0x00, 0x36, 0x00, 0x62, 0x00, 0x61, 0x00, 0x66, 0x00, 0x63, 0x00,
            0x37, 0x00, 0x7D, 0x00, 0x00, 0x00 
    };

    // u8 desc_set[158] = {
            
    //         0x0A, 0x00,
    //         0x00, 0x00,
    //         0x00, 0x00, 0x03, 0x06,
    //         0x9E, 0x00,

    //         0x14, 0x00,
    //         0x03, 0x00,
    //         0x57, 0x49, 0x4E, 0x55, 0x53, 0x42, 0x00, 0x00,
    //         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,


    //         0x80, 0x00,
    //         0x04, 0x00,
    //         0x01, 0x00,
    //         0x28, 0x00,

    //         0x44, 0x00, 0x65, 0x00, 0x76, 0x00, 0x69, 0x00, 0x63, 0x00, 0x65, 0x00,
    //         0x49, 0x00, 0x6E, 0x00, 0x74, 0x00, 0x65, 0x00, 0x72, 0x00, 0x66, 0x00,
    //         0x61, 0x00, 0x63, 0x00, 0x65, 0x00, 0x47, 0x00, 0x55, 0x00, 0x49, 0x00,
    //         0x44, 0x00, 0x00, 0x00,

    //         0x4E, 0x00,

    //         0x7B, 0x00, 0x30, 0x00, 0x65, 0x00, 0x31, 0x00, 0x33, 0x00, 0x35, 0x00, 
    //         0x37, 0x00, 0x34, 0x00, 0x37, 0x00, 0x2D, 0x00, 0x64, 0x00, 0x32, 0x00,
    //         0x63, 0x00, 0x66, 0x00, 0x2D, 0x00, 0x34, 0x00, 0x35, 0x00, 0x65, 0x00,
    //         0x62, 0x00, 0x2D, 0x00, 0x61, 0x00, 0x61, 0x00, 0x31, 0x00, 0x35, 0x00,
    //         0x2D, 0x00, 0x38, 0x00, 0x30, 0x00, 0x61, 0x00, 0x62, 0x00, 0x65, 0x00,
    //         0x62, 0x00, 0x36, 0x00, 0x62, 0x00, 0x61, 0x00, 0x66, 0x00, 0x63, 0x00,
    //         0x37, 0x00, 0x7D, 0x00, 0x00, 0x00 
    //     };


	// Set desc_set according to the vendor code
	if (VendorCode == 0x01) {
		
		// Do nothing; we already constructed this 

        // DEBUG
        #ifdef CH9_DEBUG
            printf("Vendor code is valid!\n");
        #endif
	} else {

        memset(&desc_set, 0, sizeof(desc_set));

		// No valid vendor id found
		return 0;
	}


	/* Check buffer pointer is OK and buffer is big enough. */
	if (!BufPtr) {
		return 0;
	}

	if (BufLen < sizeof(DEVICE_DESC_SET)) {
		return 0;
	}

	memcpy(BufPtr, &desc_set, 0x9E);

	return 0x9E;
}

