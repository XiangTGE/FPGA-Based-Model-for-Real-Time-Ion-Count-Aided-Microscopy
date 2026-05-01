/* 
 * Xiang Jin 19 Feb 2026
 * Updated 13 March 2026 - Used for live ADC testing
 * Updated 14 March 2026 - Fixed AXI-DMA number of transfers and transfer sizes
 * 
 * Fimware running on Eclypse Z7 (Zynq 7020 SoC) for live ADC test
 * 
 * Builds on top of ICAM Device Firmware for the AXI-DMA to USB backbone
 * Uses double buffer feature, which allows AXI DMA to load data from PL
 * while USB is offloading data from other buffer to host PC
 * 
 */



/****************************Includes******************************/

#include <string.h>
#include <xil_types.h>
#include <xparameters.h>
#include <stdio.h>
#include <stdlib.h>
#include "ps7_init.h"
#include "xil_io.h"
#include "xscugic.h"
#include "xusbps.h"
#include "xusbps_ch9.h"
#include "xusbps_ch9_simple_usb.h"
#include <sys/_intsup.h>
#include <xil_exception.h>
#include <xil_printf.h>
#include <xil_cache.h>
#include <xinterrupt_wrap.h>
#include <xstatus.h>
#include <xil_mmu.h>

#include "xscutimer.h"



// DEBUG vars
volatile int iterations = 0;



/************************Hardware Parameters************************/

// AXI DMA parameters

// AXI GPIO
#define AXI_GPIO_0_BASEADDR             XPAR_XGPIO_0_BASEADDR


// AXI DMA
#define AXIDMA_BASEADDR                 XPAR_AXI_DMA_0_BASEADDR
#define AXI_DMA_INTERRUPT_ID            61
#define INTERRUPT_PARENT                XPAR_XAXIDMA_0_INTERRUPT_PARENT
#define DMA_TRANSFER_SIZE               256*4                                   // Number of bytes that AXI-DMA will transfer at a time
#define NUM_DMA_TRANSFERS               1                                       // Number of DMA transfers needed to fill one data buffer 
                                                                                // (in this case it is 4 because each transfer is 256 bytes)
volatile int dma_transfers = 0;                                                 // Keeps track of number of DMA transfers done for a particular data buffer cycle


// Interrupt controller for AXI DMA
#define XSCUGIC_BASEADDR XPAR_INTC_BASEADDR
XScuGic InterruptController;
static XScuGic_Config *GicConfig;
u32 global_frame_counter = 0;


// DEBUG (count number of times we interrupt)
volatile int counter = 0;


// USB Parameters
#define USB_BASE_ADDR                   XPAR_USB0_BASEADDR
#define USB_INTERRUPT_ID                53                                      // Interrupt ID of USB controller
#define USB_MAX_BUFFER                  16*1024                                 // Max buffer size that is allowed to be sent at a time (16kB)
static XUsbPs UsbInstance;                                                      // Instance of USB controller
XUsbPs_Config           *UsbConfigPtr;                                          // Configuration of USB device driver
XUsbPs_DeviceConfig     DeviceConfig;                                           // Currently active usb configuration
const u8 NumEndpoints = 2;                                                      // Specify number of endpoints
volatile int usb_config_set = 0;                                                // Flags if usb configuration is set (0 for no, 1 for yes)
static volatile int NumIrqs = 0;

#define MEMORY_SIZE                     (64 * 1024)                             // Size in bytes of buffer (64 kB)
u8 Buffer[MEMORY_SIZE] ALIGNMENT_CACHELINE;                                     // Buffer aligned to 32 bit cacheline
u8 *MemPtr;   
      


// Double buffers
#define BUFFER_BASE_ADDR                0xa000000                               // Base address of double buffers
#define BUFFER_SIZE                     256*4                                   // Size (in bytes) of data buffers
#define DATA_BUFFER_SIZE_DEBUG          256*4//2097152 //2MB                    // Buffer size for debugging
#define USB_Num_Transfers_Debug         1                                       // Number of USB transfers needed to fully transfer one buffer
volatile int transfer_count = 0;                                                         // Keeps track of USB transfers queued so far for a frame transfer
volatile int transfers_done = 0;                                                         // Keeps track of USB transfers that are completed for a frame transfer
volatile int bufs_free_ep0 = 8;                                                          // Number of transfer descriptors that are free for EP0
u8 Data1[DATA_BUFFER_SIZE_DEBUG] ALIGNMENT_CACHELINE;
u8 Data2[DATA_BUFFER_SIZE_DEBUG] ALIGNMENT_CACHELINE;
u8 *Data_Buffer0 = Data1;//(u8 *)BUFFER_BASE_ADDR;                                      // Data buffers
u8 *Data_Buffer1 = Data2;//(u8 *)(BUFFER_BASE_ADDR + DATA_BUFFER_SIZE_DEBUG);
volatile int armed_buffer = -1;                                                 // Indicates which buffer is currently used by AXI DMA
                                                                                // -1 means no buffer armed, 0, 1 means either Buffer0 or Buffer1 armed
volatile int USB_DONE = 1;                                                      // Flags whether USB transfer has been completed; if so, we can swap (set to 1 initially so that first USB transfer can start) 



/*********************Function Declarations*************************/

// AXI GPIO
int EnableADC();                                                                // Enables ADC through asserting AXI_GPIO output to 1 
                                                                                // NOTE: Cannot be deasserted afterwards


// AXI DMA
int InitializeAXIDma (void);                                                    // Initialize AXI DMA
int EnableSampleGenerator(unsigned int);                                        // Enable sample generator
void StartDMATransfer (unsigned int, unsigned int);                             // Start a DMA transfer (dest addr, length)
void InterruptHandler (void *CallBackRef);                                      // AXI DMA interrupt handler
int SetUpInterruptController(XScuGic *IntrController, u32 BaseAddr);            // Connect interrupt handler of interrupt controller to processor
//int InitializeInterruptSystem (u32);                                            // Set up interrupt system for AXI DMA


// USB
static void XUsbPs_Ep0EventHandler(void *CallBackRef, u8 EpNum,
				   u8 EventType, void *Data);									// Handler for EP 0 (Control)
static void XUsbPs_Ep1EventRxHandler(void *CallBackRef, u8 EpNum,
				   u8 EventType, void *Data);									// Handler for EP 1 (BULK OUT)
static void XUsbPs_Ep1EventTxHandler(void *CallBackRef, u8 EpNum,
				   u8 EventType, void *Data);									// Handler for EP 1 (BULK IN)
static void UsbIntrHandler(void *CallBackRef, u32 Mask);



int main () {

    int Status;

    // Enable the PL
    ps7_post_config();

    
    // Clear both data buffers
    // Clear out both data buffers
    memset(Data_Buffer0, 0, DATA_BUFFER_SIZE_DEBUG); 
    memset(Data_Buffer1, 0, DATA_BUFFER_SIZE_DEBUG); 
    Xil_DCacheFlushRange((UINTPTR)Data_Buffer0, DATA_BUFFER_SIZE_DEBUG);
    Xil_DCacheFlushRange((UINTPTR)Data_Buffer1, DATA_BUFFER_SIZE_DEBUG);

    
    // Set data buffers as non-cachable
    Xil_SetTlbAttributes(Data_Buffer0, 0x14de2);
    Xil_SetTlbAttributes(Data_Buffer1, 0x14de2);

    
    
    // DEBUG: check whether there is buffer overlap
    xil_printf("Data1 addr: %08x\r\n", (u32)Data1);
    xil_printf("Data2 addr: %08x\r\n", (u32)Data2);
    xil_printf("Buffer addr: %08x\r\n", (u32)Buffer);

    // DEBUG: check first 8 bytes of buffer Data1
    for(int i=0;i<8;i++)
        xil_printf("%02x ", Data1[i]);
    xil_printf("\r\n");



    // Set up interrupt controller
    Status = SetUpInterruptController(&InterruptController, (UINTPTR) XSCUGIC_BASEADDR);
    if (Status != XST_SUCCESS) {

        return XST_FAILURE;
    }


    // Init AXI DMA
    xil_printf("Initializing axi dma ...\n\r"); 
    InitializeAXIDma(); 


    // Connect out interrupt handler to the interrupt controller
    Status = XScuGic_Connect(&InterruptController, AXI_DMA_INTERRUPT_ID, (Xil_InterruptHandler) InterruptHandler, NULL);
    if (Status != XST_SUCCESS) {

        return XST_FAILURE; 
    }

    // Enable interrupts
    XScuGic_Enable(&InterruptController, AXI_DMA_INTERRUPT_ID); 



    /*********************Set up USB***********************/
    UsbConfigPtr = XUsbPs_LookupConfig(USB_BASE_ADDR); 
    if (NULL == UsbConfigPtr) {

        return XST_FAILURE; 
    }

    Status = XUsbPs_CfgInitialize(&UsbInstance, UsbConfigPtr, UsbConfigPtr->BaseAddress);
    if (XST_SUCCESS != Status) {

        return XST_FAILURE; 
    }


    // Connect interrupts to GIC, enable them
    Status = XScuGic_Connect(&InterruptController, USB_INTERRUPT_ID, &XUsbPs_IntrHandler, &UsbInstance); 
    // Status = XSetupInterruptSystem(&UsbInstance, &XUsbPs_IntrHandler,
    //                                 UsbConfigPtr->IntrId,
    //                                 UsbConfigPtr->IntrParent,
    //                                 XINTERRUPT_DEFAULT_PRIORITY);
    if (XST_SUCCESS != Status) {

        return XST_FAILURE;
    }

    XScuGic_Enable(&InterruptController, USB_INTERRUPT_ID); 


    // Device configuration
    DeviceConfig.EpCfg[0].Out.Type		= XUSBPS_EP_TYPE_CONTROL;
	DeviceConfig.EpCfg[0].Out.NumBufs	= 2;
	DeviceConfig.EpCfg[0].Out.BufSize	= 64;
	DeviceConfig.EpCfg[0].Out.MaxPacketSize	= 64;
	DeviceConfig.EpCfg[0].In.Type		= XUSBPS_EP_TYPE_CONTROL;
	DeviceConfig.EpCfg[0].In.NumBufs	= 2;
	DeviceConfig.EpCfg[0].In.MaxPacketSize	= 64;

	DeviceConfig.EpCfg[1].Out.Type		= XUSBPS_EP_TYPE_BULK;
	DeviceConfig.EpCfg[1].Out.NumBufs	= 16;
	DeviceConfig.EpCfg[1].Out.BufSize	= 512;
	DeviceConfig.EpCfg[1].Out.MaxPacketSize	= 512;
	DeviceConfig.EpCfg[1].In.Type		= XUSBPS_EP_TYPE_BULK;
	DeviceConfig.EpCfg[1].In.NumBufs	= 8;        // WORK IN PROGRESS
	DeviceConfig.EpCfg[1].In.MaxPacketSize	= 512;

    DeviceConfig.NumEndpoints = NumEndpoints;

    MemPtr = (u8 *)&Buffer[0];
	memset(MemPtr, 0, MEMORY_SIZE);
	Xil_DCacheFlushRange((unsigned int)MemPtr, MEMORY_SIZE);

    DeviceConfig.DMAMemPhys = (u32) MemPtr;

    Status = XUsbPs_ConfigureDevice(&UsbInstance, &DeviceConfig);
    if (XST_SUCCESS != Status) {

        return XST_FAILURE;
    }

    /* Set the handler for receiving frames. */
    // WORK IN PROGRESS - may want to change intr handler
	Status = XUsbPs_IntrSetHandler(&UsbInstance, UsbIntrHandler, NULL,
				       XUSBPS_IXR_UE_MASK);
	if (XST_SUCCESS != Status) {
		return XST_FAILURE;
	}

    

    /* Set the handler for handling endpoint 0 events. This is where we
	 * will receive and handle the Setup packet from the host.
	 */
	Status = XUsbPs_EpSetHandler(&UsbInstance, 0,
				     XUSBPS_EP_DIRECTION_OUT,
				     XUsbPs_Ep0EventHandler, &UsbInstance);
    if (XST_SUCCESS != Status) {
		return XST_FAILURE;
	}

    /* Set the handlers for handling endpoint 1 events.
	 */
    Status = XUsbPs_EpSetHandler(&UsbInstance, 1,
				     XUSBPS_EP_DIRECTION_OUT,
				     XUsbPs_Ep1EventRxHandler, &UsbInstance);
    if (XST_SUCCESS != Status) {
		return XST_FAILURE;
	}

    Status = XUsbPs_EpSetHandler(&UsbInstance, 1,
				     XUSBPS_EP_DIRECTION_IN,
				     XUsbPs_Ep1EventTxHandler, &UsbInstance);
    if (XST_SUCCESS != Status) {

        return XST_FAILURE;
    }



    // Enable the interrupts from the USB controller side
    XUsbPs_IntrEnable(&UsbInstance, XUSBPS_IXR_UR_MASK | XUSBPS_IXR_UI_MASK);

    // Start USB engine
    XUsbPs_Start(&UsbInstance);



    // Enable interrupts to the processor (IMPORTANT)
    Xil_ExceptionEnableMask(XIL_EXCEPTION_IRQ);


    // Enable ADC controller
    EnableADC();


    // Perform first transfer to get chain of transfers started 
    xil_printf("Performing the first DMA transfer ...\n\r");
    armed_buffer = 0; 
    StartDMATransfer(Data_Buffer0, DMA_TRANSFER_SIZE); 


    // Idle; do nothing in main
    while (1) {

    } 


    return 0;
}



/************************Function Implementations***********************/


// Enable ADC controller (sets AXI-GPIO to 1, can never be deasserted)
int EnableADC () {

    // Set Enable signal for ADC controller
    Xil_Out32 (AXI_GPIO_0_BASEADDR, 1);

    return 0;
}



// Initialize AXI DMA
int InitializeAXIDma (void) {

    unsigned int tmpVal;

    // 2SMM_DMACR.RS    1
    tmpVal = Xil_In32 (AXIDMA_BASEADDR + 0x30);
    tmpVal = tmpVal | 0x5001;   // Might want to make it 0x5001 to see if setting S2MM_DMACR.Err_IrqEn = 1 could enable interrupts
    Xil_Out32 (AXIDMA_BASEADDR + 0x30, tmpVal);
    tmpVal = Xil_In32(AXIDMA_BASEADDR + 0x30);
    xil_printf("Value for dma control register : %x\n\r", tmpVal);

    return XST_SUCCESS;
}



// Start new DMA transfer
void StartDMATransfer (unsigned int destAddr, unsigned int len) {

    // Write destination address to S2MM_SA register (this has to come before len write)
    Xil_Out32(AXIDMA_BASEADDR + 0x48, destAddr);

    xil_printf("Before length write: DMASR=%08x\n", Xil_In32(AXIDMA_BASEADDR + 0x34)); // DEBUG

    // Write length to S2MM_LENGTH register
    Xil_Out32(AXIDMA_BASEADDR + 0x58, len);
}



// Interrupt handler for AXI DMA on completion of an AXI transfer
void InterruptHandler (void *CallBackRef) {

    counter++; //DEBUG

    u32 tmpVal;

    // Update number of DMA transfers for current data buffer cycle
    dma_transfers++;

    xil_printf("dma_transfers: %d\n", dma_transfers); // DEBUG


    // Clear interrupt; just write to bit 12 of S2MM_DMASR
    tmpVal = Xil_In32(AXIDMA_BASEADDR + 0x34);
    tmpVal = tmpVal | 0x1000;           // Sets S2MM_DMASR.IOC_Irq [12] to 1 which clears the bit
    Xil_Out32(AXIDMA_BASEADDR + 0x34, tmpVal);



    // Invalidate cache so CPU gets the retrieved data
    if (armed_buffer == 0)
        Xil_DCacheInvalidateRange((UINTPTR)Data_Buffer0, DATA_BUFFER_SIZE_DEBUG);
    else
        Xil_DCacheInvalidateRange((UINTPTR)Data_Buffer1, DATA_BUFFER_SIZE_DEBUG);

 

    // Check if data buffer cycle complete
    if (dma_transfers == NUM_DMA_TRANSFERS) {

        // Have USB send loaded buffer to user through EP1 bulk transfer, 
        // start next DMA transfer (WORK IN PROGRESS)
        // If USB transfer done, need to perform swap
        // Otherwise DMA rewrites designated buffer
        xil_printf("USB_DONE=%d usb_config_set=%d armed_buffer=%d dma_transfers=%d\r\n",
           USB_DONE, usb_config_set, armed_buffer, dma_transfers); // DEBUG
        if (USB_DONE && usb_config_set) { // SWAP, start next USB transfer

            // Reset USB transfer flag
            USB_DONE = 0;    

            if (armed_buffer == 0) {

                armed_buffer = 1;


                // Clear buffer that was just sent
                memset(Data_Buffer1, 0, DATA_BUFFER_SIZE_DEBUG);
                Xil_DCacheFlushRange(Data_Buffer1, DATA_BUFFER_SIZE_DEBUG);


                xil_printf("Staged Buffer 0 for USB transfer\n"); // DEBUG
                xil_printf("USB send: ");
                for (int i = 0; i < 10; i++) 
                    xil_printf("%02x ", Data_Buffer0[i]);
                int status = XUsbPs_EpBufferSend(&UsbInstance, 1, Data_Buffer0, DATA_BUFFER_SIZE_DEBUG);
                if (status != XST_SUCCESS) {

                    while(1); // debug
                }
            } else {

                armed_buffer = 0;


                // Clear buffer that was just sent
                memset(Data_Buffer0, 0, DATA_BUFFER_SIZE_DEBUG);
                Xil_DCacheFlushRange(Data_Buffer0, DATA_BUFFER_SIZE_DEBUG);


                xil_printf("Staged Buffer 1 for USB transfer\n");   // DEBUG
                xil_printf("USB send: ");
                for (int i = 0; i < 10; i++) 
                    xil_printf("%02x ", Data_Buffer1[i]);
                int status = XUsbPs_EpBufferSend(&UsbInstance, 1, Data_Buffer1, DATA_BUFFER_SIZE_DEBUG);
                if (status != XST_SUCCESS) {

                    while(1); // debug
                }
            } 
        }


        // Don't switch data buffers, get ready for new data buffer cycle
        dma_transfers = 0;
    }

    
    // Initialize next DMA transfer
    if (armed_buffer == 0) {

        StartDMATransfer (Data_Buffer0 + dma_transfers*DMA_TRANSFER_SIZE, DMA_TRANSFER_SIZE);
    } else if (armed_buffer == 1) {

        StartDMATransfer (Data_Buffer1 + dma_transfers*DMA_TRANSFER_SIZE, DMA_TRANSFER_SIZE);
    } else {

        // Shouldn't be here! 
        xil_printf("Error in AXI DMA Transfer Handling...");
    }
}



// Connects interrupt handler of the interrupt controller to the processor
// Sets up GIC system, connects and enables interrupts
// NOTE: CALL THIS ONLY ONCE!!
int SetUpInterruptController(XScuGic *XScuGicInstancePtr, u32 BaseAddr)
{

    int Status;

    // Look up intr config for AXI DMA block
    GicConfig = XScuGic_LookupConfig(BaseAddr);
    if (NULL == GicConfig) {

        return XST_FAILURE;
    }

    // Configure specific interrupt controller instance
    Status = XScuGic_CfgInitialize(&InterruptController, GicConfig, GicConfig->CpuBaseAddress);
    if (Status != XST_SUCCESS) {

        return XST_FAILURE;
    }

    Xil_ExceptionInit();


	/*
	 * Connect the interrupt controller interrupt handler to the hardware
	 * interrupt handling logic in the ARM processor.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
				     (Xil_ExceptionHandler) XScuGic_InterruptHandler,
				     XScuGicInstancePtr);



	return XST_SUCCESS;
}



/*****************************************************************************/
/**
* This function is registered to handle callbacks for endpoint 0 (Control).
*
* It is called from an interrupt context such that the amount of processing
* performed should be minimized.
* 
*
* @param	CallBackRef is the reference passed in when the function
*		was registered.
* @param	EpNum is the Number of the endpoint on which the event occurred.
* @param	EventType is type of the event that occurred.
*
* @return	None.
*
******************************************************************************/
static void XUsbPs_Ep0EventHandler(void *CallBackRef, u8 EpNum,
				   u8 EventType, void *Data)
{
	XUsbPs			*InstancePtr;
	int			Status;
	XUsbPs_SetupData	SetupData;
	u8	*BufferPtr;
	u32	BufferLen;
	u32	Handle;


	Xil_AssertVoid(NULL != CallBackRef);

	InstancePtr = (XUsbPs *) CallBackRef;

	switch (EventType) {

		/* Handle the Setup Packets received on Endpoint 0. */
		case XUSBPS_EP_EVENT_SETUP_DATA_RECEIVED:
			Status = XUsbPs_EpGetSetupData(InstancePtr, EpNum, &SetupData);
			if (XST_SUCCESS == Status) {
				/* Handle the setup packet. */
				Status = XUsbPs_Ch9HandleSetupPacket(InstancePtr,
								  &SetupData);


                // If configuration is set, then set flag
                if (Status == CONFIG_SET) {

                    usb_config_set = 1;
                }
			}
			break;

		/* We get data RX events for 0 length packets on endpoint 0. We receive
		 * and immediately release them again here, but there's no action to be
		 * taken.
		 */
		case XUSBPS_EP_EVENT_DATA_RX:
			/* Get the data buffer. */
			Status = XUsbPs_EpBufferReceive(InstancePtr, EpNum,
							&BufferPtr, &BufferLen, &Handle);
			if (XST_SUCCESS == Status) {
				/* Return the buffer. */
				XUsbPs_EpBufferRelease(Handle);
			}
			break;

        

		default:
			/* Unhandled event. Ignore. */
			break;
	}
}



/*****************************************************************************/
/**
* This function is registered to handle callbacks for endpoint 1 Rx (Bulk data).
*
* It is called from an interrupt context such that the amount of processing
* performed should be minimized.
*
*
* @param	CallBackRef is the reference passed in when the function was
*		registered.
* @param	EpNum is the Number of the endpoint on which the event occurred.
* @param	EventType is type of the event that occurred.
*
* @return	None.
*
* @note 	None.
*
******************************************************************************/
static void XUsbPs_Ep1EventRxHandler(void *CallBackRef, u8 EpNum,
				   u8 EventType, void *Data)
{
	XUsbPs *InstancePtr;
	int Status;
	u8	*BufferPtr;
	u32	BufferLen;
	u32 InavalidateLen;
	u32	Handle;


	Xil_AssertVoid(NULL != CallBackRef);

	InstancePtr = (XUsbPs *) CallBackRef;

	switch (EventType) {

		case XUSBPS_EP_EVENT_DATA_RX:

			/* Get the data buffer.*/
			Status = XUsbPs_EpBufferReceive(InstancePtr, EpNum,
							&BufferPtr, &BufferLen, &Handle);
			/* Invalidate the Buffer Pointer */
			InavalidateLen =  BufferLen;
			if (BufferLen % 32) {
				InavalidateLen = (BufferLen / 32) * 32 + 32;
			}

			Xil_DCacheInvalidateRange((unsigned int)BufferPtr,
						  InavalidateLen); 
			if (XST_SUCCESS == Status) {
				/* Handle the request. */ 
				
				/* Release the buffer. */
				XUsbPs_EpBufferRelease(Handle);
			}
			break;

		default:

			// DO nothing
			break;
	}
}



/*****************************************************************************/
/**
* This function is registered to handle callbacks for endpoint 1 Tx (Bulk data).
*
* It is called from an interrupt context such that the amount of processing
* performed should be minimized.
*
*
* @param	CallBackRef is the reference passed in when the function was
*		registered.
* @param	EpNum is the Number of the endpoint on which the event occurred.
* @param	EventType is type of the event that occurred.
*
* @return	None.
*
* @note 	None.
*
******************************************************************************/
// WORK IN PROGRESS, may need to work on getting this done
static void XUsbPs_Ep1EventTxHandler(void *CallBackRef, u8 EpNum,
				   u8 EventType, void *Data)
{
	XUsbPs *InstancePtr;
	int Status;
	u8	*BufferPtr;
	u32	BufferLen;
	u32 InavalidateLen;
	u32	Handle;


    // DEBUG

	Xil_AssertVoid(NULL != CallBackRef);

	InstancePtr = (XUsbPs *) CallBackRef; 

	switch (EventType) {

		// Data buffer has been sent
		case XUSBPS_EP_EVENT_DATA_TX:

            // Update number of free buffers
            // bufs_free_ep0++;
            // transfers_done++;


            // // Stage more transfers as necessary
            // if (bufs_free_ep0 > 0 && transfer_count < 1) {

            //     for (int i = 0; i < bufs_free_ep0; i++) {
                    
            //         // Stop queueing if we queued the entire frame
            //         if (transfer_count == USB_Num_Transfers_Debug) {

            //             break;
            //         }


            //         if (armed_buffer == 0) {

            //             Status = XUsbPs_EpBufferSend(&UsbInstance, 1, Data_Buffer1 + transfer_count*DATA_BUFFER_SIZE_DEBUG, DATA_BUFFER_SIZE_DEBUG);
            //             if (Status != XST_SUCCESS) {

            //                 while (1); // debug
            //             }
            //             bufs_free_ep0--;
            //         } else {

            //             Status = XUsbPs_EpBufferSend(&UsbInstance, 1, Data_Buffer0 + transfer_count*DATA_BUFFER_SIZE_DEBUG, DATA_BUFFER_SIZE_DEBUG);
            //             if (Status != XST_SUCCESS) {

            //                 while (1); // debug
            //             }
            //             bufs_free_ep0--;
            //         }

            //         transfer_count++;
            //     }
            // }


            // Determine if full frame has been queued for transfer
			// if (transfers_done == USB_Num_Transfers_Debug) {
                
            //     // Reset counters and flags to prepare for next frame transfer
            //     transfer_count = 0;
            //     transfers_done = 0;
            //     USB_DONE = 1;           // Raise flag indicating the USB transfer is done
            // }

            USB_DONE = 1;
			break;

		default:

			// Do nothing
			break;
	}
}



/*****************************************************************************/
/**
 *
 * This function is the handler which performs processing for the USB driver.
 * It is called from an interrupt context such that the amount of processing
 * performed should be minimized.
 *
 * This handler provides an example of how to handle USB interrupts and
 * is application specific.
 *
 * @param	CallBackRef is the Upper layer callback reference passed back
 *		when the callback function is invoked.
 * @param 	Mask is the Interrupt Mask.
 * @param	CallBackRef is the User data reference.
 *
 * @return
 * 		- XST_SUCCESS if successful
 * 		- XST_FAILURE on error
 *
 * @note	None.
 *
 ******************************************************************************/
static void UsbIntrHandler(void *CallBackRef, u32 Mask)
{
	
    return;

}
