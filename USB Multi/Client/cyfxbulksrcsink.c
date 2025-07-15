#include "cyu3system.h"
#include "cyu3os.h"
#include "cyu3dma.h"
#include "cyu3error.h"
#include "cyfxbulksrcsink.h"
#include "cyu3usb.h"
#include "cyu3uart.h"
#include "cyu3gpio.h"
#include "cyu3utils.h"
#include "scsi.h"

CyU3PThread     bulkSrcSinkAppThread;    /* Application thread structure */
CyU3PDmaChannel glChHandleBulkCD_ROM;      /* DMA MANUAL_IN channel handle.          */

CyBool_t glIsApplnActive = CyFalse;      /* Whether the source sink application is active or not. */
uint32_t glDMARxCount = 0;               /* Counter to track the number of buffers received. */
uint32_t glDMATxCount = 0;               /* Counter to track the number of buffers transmitted. */
CyBool_t glDataTransStarted = CyFalse;   /* Whether DMA transfer has been started after enumeration. */
CyBool_t StandbyModeEnable  = CyFalse;   /* Whether standby mode entry is enabled. */
CyBool_t TriggerStandbyMode = CyFalse;   /* Request to initiate standby entry. */
CyBool_t glForceLinkU2      = CyFalse;   /* Whether the device should try to initiate U2 mode. */

uint8_t *glMscCbwBuffer = 0;            /* Scratch buffer used for CBW. */
uint8_t *glMscCswBuffer = 0;            /* Scratch buffer used for CSW. */
uint8_t *glMscDataBuffer = 0;            /* Scratch buffer used for CSW. */

volatile uint32_t glEp0StatCount = 0;           /* Number of EP0 status events received. */
uint8_t glEp0Buffer[32] __attribute__ ((aligned (32))); /* Local buffer used for vendor command handling. */

/* Control request related variables. */
CyU3PEvent glBulkLpEvent;       /* Event group used to signal the thread that there is a pending request. */
uint32_t   gl_setupdat0;        /* Variable that holds the setupdat0 value (bmRequestType, bRequest and wValue). */
uint32_t   gl_setupdat1;        /* Variable that holds the setupdat1 value (wIndex and wLength). */
#define CYFX_USB_CTRL_TASK      (1 << 0)        /* Event that indicates that there is a pending USB control request. */
#define CYFX_USB_HOSTWAKE_TASK  (1 << 1)        /* Event that indicates the a Remote Wake should be attempted. */

/* Buffer used for USB event logs. */
uint8_t *gl_UsbLogBuffer = NULL;
#define CYFX_USBLOG_SIZE        (0x1000)

/* Timer Instance */
CyU3PTimer glLpmTimer;

/* GPIO used for testing IO state retention when switching from boot firmware to full firmware. */
#define FX3_GPIO_TEST_OUT               (50)
#define FX3_GPIO_TO_LOFLAG(gpio)        (1 << (gpio))
#define FX3_GPIO_TO_HIFLAG(gpio)        (1 << ((gpio) - 32))

//CD ROM / SCSI Data

// Example static INQUIRY response for a CD-ROM
uint8_t scsiInquiryResp[] = {
	    0x40,       /* PQ and PDT */
	    0x80,       /* Removable device. */
	    0x06,       /* Version */
	    0x02,       /* Response data format */
	    0x60,       /* Addnl Length (total length minus 4 bytes) */
	    0x00,
	    0x00,
	    0x00,

	    'V',        /* Vendor Id */
	    'u',
	    'l',
	    'p',
	    'e',
	    's',
	    0x00,

	    'D',        /* Product Id */
	    'o',
	    'l',
	    'c',
	    'h',
	    ' ',
	    'C',
	    'D',
	    '-',
	    'R',
	    'O',
	    'M',
	    ' ',
	    'D',
	    'e',
	    'v',
	    'i',
	    'c',
	    'e',
	    0x00,
	    0x00,

	    '0',        /* Revision */
	    '0',
	    '0',
	    '1',

	    0, 0, 0, 0,
	    0, 0, 0, 0,
	    0, 0, 0, 0,
	    0, 0, 0, 0,
	    0, 0, 0, 0, /* 20 bytes of vendor specific info: not used. */
	    0, 0,       /* Reserved fields for SCSI over USB. */

	    0x00, 0x80, /* SAM-4 spec compliant. */
	    0x17, 0x30, /* BOT spec compliant. */
	    0x04, 0x60, /* SPC-4 spec compliant. */
	    0x04, 0xC0, /* SBC-3 spec compliant. */
	    0x00, 0x00, /* No more specs. */
	    0x00, 0x00, /* No more specs. */
	    0x00, 0x00, /* No more specs. */
	    0x00, 0x00, /* No more specs. */
	    0x00, 0x00, /* Reserved. */

	    0, 0, 0, 0, /* Reserved. */
	    0, 0, 0, 0, /* Reserved. */
	    0, 0, 0, 0, /* Reserved. */
	    0, 0, 0, 0, /* Reserved. */
	    0, 0, 0, 0  /* Reserved. */
};

void PrepareCSW(void *buffer, uint32_t tag, uint32_t residue, uint8_t status)
{
	CSW *csw = (struct CSW*)buffer;
	csw->dCSWSignature = CSW_SIGNATURE;
	csw->dCSWTag = tag;
	csw->dCSWDataResidue = residue;
	csw->bCSWStatus = status;
}

/* Application Error Handler */
void
CyFxAppErrorHandler (
        CyU3PReturnStatus_t apiRetStatus    /* API return status */
        )
{
    /* Application failed with the error code apiRetStatus */

    /* Add custom debug or recovery actions here */

    /* Loop Indefinitely */
    for (;;)
    {
        /* Thread sleep : 100 ms */
        CyU3PThreadSleep (100);
    }
}

/* This function initializes the debug module. The debug prints
 * are routed to the UART and can be seen using a UART console
 * running at 115200 baud rate. */
void
CyFxBulkSrcSinkApplnDebugInit (void)
{
    CyU3PGpioClock_t  gpioClock;
    CyU3PUartConfig_t uartConfig;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    /* Initialize the GPIO block. If we are transitioning from the boot app, we can verify whether the GPIO
       state is retained. */
    gpioClock.fastClkDiv = 2;
    gpioClock.slowClkDiv = 32;
    gpioClock.simpleDiv  = CY_U3P_GPIO_SIMPLE_DIV_BY_16;
    gpioClock.clkSrc     = CY_U3P_SYS_CLK_BY_2;
    gpioClock.halfDiv    = 0;
    apiRetStatus = CyU3PGpioInit (&gpioClock, NULL);

    /* When FX3 is restarting from standby mode, the GPIO block would already be ON and need not be started
       again. */
    if ((apiRetStatus != 0) && (apiRetStatus != CY_U3P_ERROR_ALREADY_STARTED))
    {
        CyFxAppErrorHandler(apiRetStatus);
    }
    else
    {
        /* Set the test GPIO as an output and update the value to 0. */
        CyU3PGpioSimpleConfig_t testConf = {CyFalse, CyTrue, CyTrue, CyFalse, CY_U3P_GPIO_NO_INTR};

        apiRetStatus = CyU3PGpioSetSimpleConfig (FX3_GPIO_TEST_OUT, &testConf);
        if (apiRetStatus != 0)
            CyFxAppErrorHandler (apiRetStatus);
    }

    /* Initialize the UART for printing debug messages */
    apiRetStatus = CyU3PUartInit();
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        /* Error handling */
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Set UART configuration */
    CyU3PMemSet ((uint8_t *)&uartConfig, 0, sizeof (uartConfig));
    uartConfig.baudRate = CY_U3P_UART_BAUDRATE_115200;
    uartConfig.stopBit = CY_U3P_UART_ONE_STOP_BIT;
    uartConfig.parity = CY_U3P_UART_NO_PARITY;
    uartConfig.txEnable = CyTrue;
    uartConfig.rxEnable = CyFalse;
    uartConfig.flowCtrl = CyFalse;
    uartConfig.isDma = CyTrue;

    apiRetStatus = CyU3PUartSetConfig (&uartConfig, NULL);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Set the UART transfer to a really large value. */
    apiRetStatus = CyU3PUartTxSetBlockXfer (0xFFFFFFFF);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Initialize the debug module. */
    apiRetStatus = CyU3PDebugInit (CY_U3P_LPP_SOCKET_UART_CONS, 8);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyFxAppErrorHandler(apiRetStatus);
    }

    CyU3PDebugPreamble(CyFalse);
}

/* Callback funtion for the timer expiry notification. */
void TimerCb(void)
{
    /* Enable the low power mode transition on timer expiry */
    CyU3PUsbLPMEnable();
}

/* Function to initiate sending of data to the USB host. */
CyU3PReturnStatus_t CyFxSendMscDataToHost (
    uint8_t  *data,
    uint32_t length)
{
    CyU3PDmaBuffer_t    dmaBuf;
    CyU3PReturnStatus_t status;

    /* Prepare the DMA Buffer */
    dmaBuf.buffer = data;
    dmaBuf.status = 0;
    dmaBuf.size   = (length + 15) & 0xFFF0;      /* Round up to a multiple of 16.  */
    dmaBuf.count  = length;

    status = CyU3PDmaChannelSetupSendBuffer (&glChHandleBulkCD_ROM, &dmaBuf);
    return status;
}

/* Callback funtion for the DMA event notification. */
void
CyFxCDROMDmaCallback (
        CyU3PDmaChannel   *chHandle, /* Handle to the DMA channel. */
        CyU3PDmaCbType_t  type,      /* Callback type.             */
        CyU3PDmaCBInput_t *input)    /* Callback status.           */
{
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    CyU3PDebugPrint (4, "CyFxCDROMDmaCallback()\r\n");

    if (type == CY_U3P_DMA_CB_PROD_EVENT)
    {
        /* This is a produce event notification to the CPU. This notification is
         * received upon reception of every buffer. The buffer will not be sent
         * out unless it is explicitly committed. */

        CBW *cbw = (struct CBW*)input->buffer_p.buffer;
        int BufferSZ = 0;
        uint8_t *dataBuffer = 0;

        // Validate CBW signature
        if (cbw->dCBWSignature != 0x43425355)
        {
            CyU3PDebugPrint(4, "Invalid CBW signature\r\n");
            return;
        }

#ifdef DEBUG

    	switch (cbw->CBWCB[0])
    	{
			case SCSI_INQUIRY:
				CyU3PDebugPrint (1, "SCSI Req: SCSI_INQUIRY\r\n"); break;
			case SCSI_TEST_UNIT_READY:
				CyU3PDebugPrint (1, "SCSI Req: SCSI_TEST_UNIT_READY\r\n"); break;
			case SCSI_REQUEST_SENSE:
				CyU3PDebugPrint (1, "SCSI Req: SCSI_REQUEST_SENSE\r\n"); break;
			case SCSI_READ_CAPACITY:
				CyU3PDebugPrint (1, "SCSI Req: SCSI_READ_CAPACITY\r\n"); break;
			default:
				CyU3PDebugPrint (1, "SCSI Req: ?? 0xX\r\n", cbw->CBWCB[0]); break;
    	}

#endif

        switch (cbw->CBWCB[0]) {
            case SCSI_INQUIRY:
                // Copy inquiry data to buffer after CBW
            	dataBuffer = scsiInquiryResp;
                BufferSZ += sizeof(scsiInquiryResp);
                break;
            case SCSI_TEST_UNIT_READY:
            case SCSI_READ_CAPACITY:
                // Indicate failure (medium not present)
            	dataBuffer = input->buffer_p.buffer;
                PrepareCSW(&input->buffer_p.buffer[BufferSZ], cbw->dCBWTag, cbw->dCBWDataTransferLength, 1); // Status: 1 = failed
                BufferSZ += sizeof(CSW);
                break;
            case SCSI_REQUEST_SENSE:
                {
                	dataBuffer = input->buffer_p.buffer;
                	// Sense Key: NOT READY (0x02), ASC: 0x3A, ASCQ: 0x00 (Medium Not Present)
                    uint8_t senseData[18] = {
                        0x70, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x0A,
                        0x00, 0x00, 0x00, 0x00, 0x3A, 0x00, 0x00, 0x00, 0x00, 0x00
                    };
                    CyU3PMemCopy(input->buffer_p.buffer + BufferSZ, senseData, sizeof(senseData));
                    BufferSZ += sizeof(senseData);
                    PrepareCSW(&input->buffer_p.buffer[BufferSZ], cbw->dCBWTag, cbw->dCBWDataTransferLength - sizeof(senseData), 0);
                    BufferSZ += sizeof(CSW);                
                }
                break;
            default:
                // Return failed status for unsupported commands
            	dataBuffer = input->buffer_p.buffer;
            	PrepareCSW(input->buffer_p.buffer + BufferSZ, cbw->dCBWTag, cbw->dCBWDataTransferLength, 1);
                BufferSZ += sizeof(CSW);
                break;
        }

        /* Now commit the data. */
        status = CyFxSendMscDataToHost(dataBuffer, BufferSZ);
        if (status != CY_U3P_SUCCESS)
        {
            CyU3PDebugPrint (4, "CyU3PDmaChannelCommitBuffer failed, Error code = %d\n", status);
        }

        /* Increment the counter. */
        glDMARxCount++;
    }
}

static volatile CyBool_t glSrcEpFlush = CyFalse;

void
CyFxBulkSrcSinkApplnEpEvtCB (
        CyU3PUsbEpEvtType evtype,
        CyU3PUSBSpeed_t   speed,
        uint8_t           epNum)
{
    /* Hit an endpoint retry case. Need to stall and flush the endpoint for recovery. */
    if (evtype == CYU3P_USBEP_SS_RETRY_EVT)
    {
        glSrcEpFlush = CyTrue;
    }
}

/* This function starts the application. This is called
 * when a SET_CONF event is received from the USB host. The endpoints
 * are configured and the DMA pipe is setup in this function. */
void
CyFxBulkSrcSinkApplnStart (
        void)
{
    uint16_t size = 0;
    CyU3PEpConfig_t epCfg;
    CyU3PDmaChannelConfig_t dmaCfg;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyU3PUSBSpeed_t usbSpeed = CyU3PUsbGetSpeed();

    /* First identify the usb speed. Once that is identified,
     * create a DMA channel and start the transfer on this. */

    /* Based on the Bus Speed configure the endpoint packet size */
    switch (usbSpeed)
    {
    case CY_U3P_FULL_SPEED:
        size = 64;
        break;

    case CY_U3P_HIGH_SPEED:
        size = 512;
        break;

    case CY_U3P_SUPER_SPEED:
        size = 1024;
        break;

    default:
        CyU3PDebugPrint (4, "Error! Invalid USB speed.\n");
        CyFxAppErrorHandler (CY_U3P_ERROR_FAILURE);
        break;
    }

    glMscCbwBuffer = (uint8_t*)CyU3PDmaBufferAlloc(1024);
    glMscCswBuffer = (uint8_t*)CyU3PDmaBufferAlloc(1024);
    glMscDataBuffer = (uint8_t*)CyU3PDmaBufferAlloc((1024 * 2));
    if ((glMscCbwBuffer == 0) || (glMscCswBuffer == 0) || (glMscDataBuffer == 0))
    {
        goto destroy;
    }

    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyTrue;
    epCfg.epType = CY_U3P_USB_EP_BULK;
    epCfg.burstLen = (usbSpeed == CY_U3P_SUPER_SPEED) ?
        (CY_FX_EP_BURST_LENGTH) : 1;
    epCfg.streams = 0;
    epCfg.pcktSize = size;

    /* Producer endpoint configuration */
    apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_CD_ROM_IN, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Consumer endpoint configuration */
    apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_CD_ROM_OUT, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Flush the endpoint memory */
    CyU3PUsbFlushEp(CY_FX_EP_CD_ROM_IN);
    CyU3PUsbFlushEp(CY_FX_EP_CD_ROM_OUT);

    /* Create a DMA MANUAL_IN channel for the producer socket. */
    CyU3PMemSet ((uint8_t *)&dmaCfg, 0, sizeof (dmaCfg));
    /* The buffer size will be same as packet size for the
     * full speed, high speed and super speed non-burst modes.
     * For super speed burst mode of operation, the buffers will be
     * 1024 * burst length so that a full burst can be completed.
     * This will mean that a buffer will be available only after it
     * has been filled or when a short packet is received. */
    //dmaCfg.size  = (size * CY_FX_EP_BURST_LENGTH);
    /* Multiply the buffer size with the multiplier
     * for performance improvement. */
    //dmaCfg.size *= CY_FX_DMA_SIZE_MULTIPLIER;
    dmaCfg.size = 4096;
    dmaCfg.count = CY_FX_BULKSRCSINK_DMA_BUF_COUNT;
    dmaCfg.prodSckId = CY_FX_EP_PRODUCER_SOCKET;
    dmaCfg.consSckId = CY_FX_EP_CONSUMER_SOCKET;
    dmaCfg.dmaMode = CY_U3P_DMA_MODE_BYTE;
    dmaCfg.notification = CY_U3P_DMA_CB_PROD_EVENT;
    dmaCfg.cb = CyFxCDROMDmaCallback;
    dmaCfg.prodHeader = 0;
    dmaCfg.prodFooter = 0;
    dmaCfg.consHeader = 0;
    dmaCfg.prodAvailCount = 0;

    apiRetStatus = CyU3PDmaChannelCreate (&glChHandleBulkCD_ROM,
    		CY_U3P_DMA_TYPE_MANUAL, &dmaCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PDmaChannelCreate failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Set DMA Channel transfer size */
    apiRetStatus = CyU3PDmaChannelSetXfer (&glChHandleBulkCD_ROM, CY_FX_BULKSRCSINK_DMA_TX_SIZE);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PDmaChannelSetXfer failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    CyU3PUsbRegisterEpEvtCallback (CyFxBulkSrcSinkApplnEpEvtCB, CYU3P_USBEP_SS_RETRY_EVT, 0x00, 0x02);

    /* Update the flag so that the application thread is notified of this. */
    glIsApplnActive = CyTrue;

    return;

destroy:
    if (glMscCbwBuffer)
        CyU3PDmaBufferFree(glMscCbwBuffer);
    if (glMscCswBuffer)
        CyU3PDmaBufferFree(glMscCswBuffer);
    if (glMscDataBuffer)
        CyU3PDmaBufferFree(glMscDataBuffer);

    glMscCbwBuffer = 0;
    glMscCswBuffer = 0;
    glMscDataBuffer = 0;

    CyU3PDmaChannelDestroy(&glChHandleBulkCD_ROM);

}

/* This function stops the application. This shall be called whenever a RESET
 * or DISCONNECT event is received from the USB host. The endpoints are
 * disabled and the DMA pipe is destroyed by this function. */
void
CyFxBulkSrcSinkApplnStop (
        void)
{
    CyU3PEpConfig_t epCfg;
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;

    /* Update the flag so that the application thread is notified of this. */
    glIsApplnActive = CyFalse;

    /* Destroy the channels */
    CyU3PDmaChannelDestroy (&glChHandleBulkCD_ROM);

    /* Flush the endpoint memory */
    CyU3PUsbFlushEp(CY_FX_EP_CD_ROM_IN);
    CyU3PUsbFlushEp(CY_FX_EP_CD_ROM_OUT);

    /* Disable endpoints. */
    CyU3PMemSet ((uint8_t *)&epCfg, 0, sizeof (epCfg));
    epCfg.enable = CyFalse;

    /* Producer endpoint configuration. */
    apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_CD_ROM_IN, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    /* Consumer endpoint configuration. */
    apiRetStatus = CyU3PSetEpConfig(CY_FX_EP_CD_ROM_OUT, &epCfg);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PSetEpConfig failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler (apiRetStatus);
    }

    if (glMscCbwBuffer)
        CyU3PDmaBufferFree(glMscCbwBuffer);
    if (glMscCswBuffer)
        CyU3PDmaBufferFree(glMscCswBuffer);
    if (glMscDataBuffer)
        CyU3PDmaBufferFree(glMscDataBuffer);

    glMscCbwBuffer = 0;
    glMscCswBuffer = 0;
    glMscDataBuffer = 0;
}

/* Callback to handle the USB setup requests. */
CyBool_t
CyFxBulkSrcSinkApplnUSBSetupCB (
        uint32_t setupdat0, /* SETUP Data 0 */
        uint32_t setupdat1  /* SETUP Data 1 */
    )
{
    /* Fast enumeration is used. Only requests addressed to the interface, class,
     * vendor and unknown control requests are received by this function.
     * This application does not support any class or vendor requests. */

    uint8_t  bRequest, bReqType;
    uint8_t  bType, bTarget;
    uint16_t wValue, wIndex;
    CyBool_t isHandled = CyFalse;

    /* Decode the fields from the setup request. */
    bReqType = (setupdat0 & CY_U3P_USB_REQUEST_TYPE_MASK);
    bType    = (bReqType & CY_U3P_USB_TYPE_MASK);
    bTarget  = (bReqType & CY_U3P_USB_TARGET_MASK);
    bRequest = ((setupdat0 & CY_U3P_USB_REQUEST_MASK) >> CY_U3P_USB_REQUEST_POS);
    wValue   = ((setupdat0 & CY_U3P_USB_VALUE_MASK)   >> CY_U3P_USB_VALUE_POS);
    wIndex   = ((setupdat1 & CY_U3P_USB_INDEX_MASK)   >> CY_U3P_USB_INDEX_POS);

    if (bType == CY_U3P_USB_STANDARD_RQT)
    {
        /* Handle SET_FEATURE(FUNCTION_SUSPEND) and CLEAR_FEATURE(FUNCTION_SUSPEND)
         * requests here. It should be allowed to pass if the device is in configured
         * state and failed otherwise. */
        if ((bTarget == CY_U3P_USB_TARGET_INTF) && ((bRequest == CY_U3P_USB_SC_SET_FEATURE)
                    || (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)) && (wValue == 0))
        {
            if (glIsApplnActive)
            {
                CyU3PUsbAckSetup ();

                /* As we have only one interface, the link can be pushed into U2 state as soon as
                   this interface is suspended.
                 */
                if (bRequest == CY_U3P_USB_SC_SET_FEATURE)
                {
                    glDataTransStarted = CyFalse;
                    glForceLinkU2      = CyTrue;
                }
                else
                {
                    glForceLinkU2 = CyFalse;
                }
            }
            else
                CyU3PUsbStall (0, CyTrue, CyFalse);

            isHandled = CyTrue;
        }

        /* CLEAR_FEATURE request for endpoint is always passed to the setup callback
         * regardless of the enumeration model used. When a clear feature is received,
         * the previous transfer has to be flushed and cleaned up. This is done at the
         * protocol level. Since this is just a loopback operation, there is no higher
         * level protocol. So flush the EP memory and reset the DMA channel associated
         * with it. If there are more than one EP associated with the channel reset both
         * the EPs. The endpoint stall and toggle / sequence number is also expected to be
         * reset. Return CyFalse to make the library clear the stall and reset the endpoint
         * toggle. Or invoke the CyU3PUsbStall (ep, CyFalse, CyTrue) and return CyTrue.
         * Here we are clearing the stall. */
        if ((bTarget == CY_U3P_USB_TARGET_ENDPT) && (bRequest == CY_U3P_USB_SC_CLEAR_FEATURE)
                && (wValue == CY_U3P_USBX_FS_EP_HALT))
        {
            if (glIsApplnActive)
            {
                if (wIndex == CY_FX_EP_CD_ROM_IN)
                {
                    CyU3PUsbSetEpNak (CY_FX_EP_CD_ROM_IN, CyTrue);
                    CyU3PBusyWait (125);

                    CyU3PDmaChannelReset (&glChHandleBulkCD_ROM);
                    CyU3PUsbFlushEp(CY_FX_EP_CD_ROM_IN);
                    CyU3PUsbResetEp (CY_FX_EP_CD_ROM_IN);
                    CyU3PUsbSetEpNak (CY_FX_EP_CD_ROM_IN, CyFalse);

                    CyU3PDmaChannelSetXfer (&glChHandleBulkCD_ROM, CY_FX_BULKSRCSINK_DMA_TX_SIZE);
                    CyU3PUsbStall (wIndex, CyFalse, CyTrue);
                    isHandled = CyTrue;
                    CyU3PUsbAckSetup ();
                }

                if (wIndex == CY_FX_EP_CD_ROM_OUT)
                {
                    CyU3PUsbSetEpNak (CY_FX_EP_CD_ROM_OUT, CyTrue);
                    CyU3PBusyWait (125);

                    CyU3PDmaChannelReset (&glChHandleBulkCD_ROM);
                    CyU3PUsbFlushEp(CY_FX_EP_CD_ROM_OUT);
                    CyU3PUsbResetEp (CY_FX_EP_CD_ROM_OUT);
                    CyU3PUsbSetEpNak (CY_FX_EP_CD_ROM_OUT, CyFalse);

                    CyU3PDmaChannelSetXfer (&glChHandleBulkCD_ROM, CY_FX_BULKSRCSINK_DMA_TX_SIZE);
                    CyU3PUsbStall (wIndex, CyFalse, CyTrue);
                    isHandled = CyTrue;
                    CyU3PUsbAckSetup ();
                }
            }
        }
    }

    if ((bType == CY_U3P_USB_VENDOR_RQT) && (bTarget == CY_U3P_USB_TARGET_DEVICE))
    {
        /* We set an event here and let the application thread below handle these requests.
         * isHandled needs to be set to True, so that the driver does not stall EP0. */
        isHandled = CyTrue;
        gl_setupdat0 = setupdat0;
        gl_setupdat1 = setupdat1;
        CyU3PEventSet (&glBulkLpEvent, CYFX_USB_CTRL_TASK, CYU3P_EVENT_OR);
    }

    /* Class specific descriptors such as HID Report descriptor need to handled by the callback. */
    bReqType = ((setupdat0 & CY_U3P_USB_VALUE_MASK) >> 24);
    if ((bRequest == CY_U3P_USB_SC_GET_DESCRIPTOR) && (bReqType == CY_FX_GET_REPORT_DESC))
    {
        isHandled = CyTrue;

        CyU3PDebugPrint (4, "CyFxBulkSrcSinkApplnUSBSetupCB REQ CY_U3P_USB_SC_GET_DESCRIPTOR setupdat0 0x%X setupdat1 0x%X\r\n", setupdat0, setupdat1);

        switch (wIndex)
        {
        case 0:
        case 1:
        case 2:
        {
            int status = CyU3PUsbSendEP0Data (0x1C, (uint8_t *)CyFxUSBReportDscr);
            if (status != CY_U3P_SUCCESS)
            {
                /* There was some error. We should try stalling EP0. */
                CyU3PUsbStall(0, CyTrue, CyFalse);
                break;
            }
            CyU3PDebugPrint (4, "CyFxBulkSrcSinkApplnUSBSetupCB REQ CY_U3P_USB_SC_GET_DESCRIPTOR Pass CyFxUSBReportDscr for index 0x%X\r\n", wIndex);
            break;
        }
        default:
            CyU3PDebugPrint (4, "CyFxBulkSrcSinkApplnUSBSetupCB REQ CY_U3P_USB_SC_GET_DESCRIPTOR index 0x%X not set\r\n", wIndex);
            CyU3PUsbStall(0, CyTrue, CyFalse);
            break;
        }
    }

    return isHandled;
}

/* This is the callback function to handle the USB events. */
void
CyFxBulkSrcSinkApplnUSBEventCB (
        CyU3PUsbEventType_t evtype, /* Event type */
        uint16_t            evdata  /* Event data */
        )
{
    CyU3PDebugPrint (2, "USB EVENT: %d %d\r\n", evtype, evdata);

    switch (evtype)
    {
    case CY_U3P_USB_EVENT_CONNECT:
        break;

    case CY_U3P_USB_EVENT_SETCONF:
        /* If the application is already active
         * stop it before re-enabling. */
        if (glIsApplnActive)
        {
            CyFxBulkSrcSinkApplnStop ();
        }

        /* Start the source sink function. */
        CyFxBulkSrcSinkApplnStart ();
        break;

    case CY_U3P_USB_EVENT_RESET:
    case CY_U3P_USB_EVENT_DISCONNECT:
        glForceLinkU2 = CyFalse;

        /* Stop the source sink function. */
        if (glIsApplnActive)
        {
            CyFxBulkSrcSinkApplnStop ();
        }

        glDataTransStarted = CyFalse;
        break;

    case CY_U3P_USB_EVENT_EP0_STAT_CPLT:
        glEp0StatCount++;
        break;

    case CY_U3P_USB_EVENT_VBUS_REMOVED:
        if (StandbyModeEnable)
        {
            TriggerStandbyMode = CyTrue;
            StandbyModeEnable  = CyFalse;
        }
        break;

    default:
        break;
    }
}

/* Callback function to handle LPM requests from the USB 3.0 host. This function is invoked by the API
   whenever a state change from U0 -> U1 or U0 -> U2 happens. If we return CyTrue from this function, the
   FX3 device is retained in the low power state. If we return CyFalse, the FX3 device immediately tries
   to trigger an exit back to U0.

   This application does not have any state in which we should not allow U1/U2 transitions; and therefore
   the function always return CyTrue.
 */
CyBool_t
CyFxBulkSrcSinkApplntCB (
        CyU3PUsbLinkPowerMode link_mode)
{
    return CyTrue;
}

/* This function initializes the USB Module, sets the enumeration descriptors.
 * This function does not start the bulk streaming and this is done only when
 * SET_CONF event is received. */
void
CyFxBulkSrcSinkApplnInit (void)
{
    CyU3PReturnStatus_t apiRetStatus = CY_U3P_SUCCESS;
    CyBool_t no_renum = CyFalse;

    /* The fast enumeration is the easiest way to setup a USB connection,
     * where all enumeration phase is handled by the library. Only the
     * class / vendor requests need to be handled by the application. */
    CyU3PUsbRegisterSetupCallback(CyFxBulkSrcSinkApplnUSBSetupCB, CyTrue);

    /* Setup the callback to handle the USB events. */
    CyU3PUsbRegisterEventCallback(CyFxBulkSrcSinkApplnUSBEventCB);

    /* Start the USB functionality. */
    apiRetStatus = CyU3PUsbStart();
    if (apiRetStatus == CY_U3P_ERROR_NO_REENUM_REQUIRED)
        no_renum = CyTrue;
    else if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "CyU3PUsbStart failed to Start, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Change GPIO state again. */
    CyU3PGpioSimpleSetValue (FX3_GPIO_TEST_OUT, CyTrue);

    /* Set the USB Enumeration descriptors */

    /* Super speed device descriptor. */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_DEVICE_DESCR, 0, (uint8_t *)CyFxUSB30DeviceDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set device descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* High speed device descriptor. */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_DEVICE_DESCR, 0, (uint8_t *)CyFxUSB20DeviceDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set device descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* BOS descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_BOS_DESCR, 0, (uint8_t *)CyFxUSBBOSDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set configuration descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Device qualifier descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_DEVQUAL_DESCR, 0, (uint8_t *)CyFxUSBDeviceQualDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set device qualifier descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    //=============================================

    /* Super speed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_SS_CONFIG_DESCR, 0, (uint8_t *)CyFxUSBSSConfigDscr1);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set configuration descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* High speed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_HS_CONFIG_DESCR, 0, (uint8_t *)CyFxUSBHSConfigDscr1);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB Set Other Speed Descriptor failed, Error Code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* Full speed configuration descriptor */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_FS_CONFIG_DESCR, 0, (uint8_t *)CyFxUSBFSConfigDscr1);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB Set Configuration Descriptor failed, Error Code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* String descriptor 0 */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 0, (uint8_t *)CyFxUSBStringLangIDDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* String descriptor 1 */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 1, (uint8_t *)CyFxUSBManufactureDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* String descriptor 2 */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 2, (uint8_t *)CyFxUSBProductDscr);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* String descriptor 3 */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 3, (uint8_t *)CyFxUSBInterfaceDesc1);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* String descriptor 4 */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 4, (uint8_t *)CyFxUSBInterfaceDesc2);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* String descriptor 5 */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 5, (uint8_t *)CyFxUSBInterfaceDesc3);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* String descriptor 6 */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 6, (uint8_t *)CyFxUSBInterfaceDesc4);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }

    /* String descriptor 7 - Serial Number */
    apiRetStatus = CyU3PUsbSetDesc(CY_U3P_USB_SET_STRING_DESCR, 7, (uint8_t *)CyFxUsbSN);
    if (apiRetStatus != CY_U3P_SUCCESS)
    {
        CyU3PDebugPrint (4, "USB set string descriptor failed, Error code = %d\n", apiRetStatus);
        CyFxAppErrorHandler(apiRetStatus);
    }


    /* Register a buffer into which the USB driver can log relevant events. */
    gl_UsbLogBuffer = (uint8_t *)CyU3PDmaBufferAlloc (CYFX_USBLOG_SIZE);
    if (gl_UsbLogBuffer)
        CyU3PUsbInitEventLog (gl_UsbLogBuffer, CYFX_USBLOG_SIZE);

    CyU3PDebugPrint (4, "About to connect to USB host\r\n");

    /* Connect the USB Pins with super speed operation enabled. */
    if (!no_renum) {

        apiRetStatus = CyU3PConnectState(CyTrue, CyTrue);
        if (apiRetStatus != CY_U3P_SUCCESS)
        {
            CyU3PDebugPrint (4, "USB Connect failed, Error code = %d\n", apiRetStatus);
            CyFxAppErrorHandler(apiRetStatus);
        }
    }
    else
    {
        /* USB connection is already active. Configure the endpoints and DMA channels. */
        CyFxBulkSrcSinkApplnStart ();
    }

    CyU3PDebugPrint (8, "CyFxBulkSrcSinkApplnInit complete\r\n");
}

/* Entry function for the BulkSrcSinkAppThread. */
void
BulkSrcSinkAppThread_Entry (
        uint32_t input)
{
    /* Initialize the debug module */
    CyFxBulkSrcSinkApplnDebugInit();
    CyU3PDebugPrint (1, "\n\ndebug initialized\r\n");

    /* Initialize the application */
    CyFxBulkSrcSinkApplnInit();

    /* Create a timer with 100 ms expiry to enable/disable LPM transitions */ 
    CyU3PTimerCreate (&glLpmTimer, TimerCb, 0, 100, 100, CYU3P_NO_ACTIVATE);

    for (;;)
    {
        CyU3PThreadSleep (1000);
    }
}

/* Application define function which creates the threads. */
void
CyFxApplicationDefine (
        void)
{
    void *ptr = NULL;
    uint32_t ret = CY_U3P_SUCCESS;

    /* Create an event flag group that will be used for signalling the application thread. */
    ret = CyU3PEventCreate (&glBulkLpEvent);
    if (ret != 0)
    {
        /* Loop indefinitely */
        while (1);
    }

    /* Allocate the memory for the threads */
    ptr = CyU3PMemAlloc (CY_FX_BULKSRCSINK_THREAD_STACK);

    /* Create the thread for the application */
    ret = CyU3PThreadCreate (&bulkSrcSinkAppThread,                /* App thread structure */
                          "21:Bulk_src_sink",                      /* Thread ID and thread name */
                          BulkSrcSinkAppThread_Entry,              /* App thread entry function */
                          0,                                       /* No input parameter to thread */
                          ptr,                                     /* Pointer to the allocated thread stack */
                          CY_FX_BULKSRCSINK_THREAD_STACK,          /* App thread stack size */
                          CY_FX_BULKSRCSINK_THREAD_PRIORITY,       /* App thread priority */
                          CY_FX_BULKSRCSINK_THREAD_PRIORITY,       /* App thread priority */
                          CYU3P_NO_TIME_SLICE,                     /* No time slice for the application thread */
                          CYU3P_AUTO_START                         /* Start the thread immediately */
                          );

    /* Check the return code */
    if (ret != 0)
    {
        /* Thread Creation failed with the error code retThrdCreate */

        /* Add custom recovery or debug actions here */

        /* Application cannot continue */
        /* Loop indefinitely */
        while(1);
    }
}

/*
 * Main function
 */
int
main (void)
{
    CyU3PIoMatrixConfig_t io_cfg;
    CyU3PReturnStatus_t status = CY_U3P_SUCCESS;

    /* Initialize the device */
    CyU3PSysClockConfig_t clockConfig;
    clockConfig.setSysClk400  = CyFalse;
    clockConfig.cpuClkDiv     = 2;
    clockConfig.dmaClkDiv     = 2;
    clockConfig.mmioClkDiv    = 2;
    clockConfig.useStandbyClk = CyFalse;
    clockConfig.clkSrc         = CY_U3P_SYS_CLK;
    status = CyU3PDeviceInit (&clockConfig);
    if (status != CY_U3P_SUCCESS)
    {
        goto handle_fatal_error;
    }

    /* Initialize the caches. The D-Cache is kept disabled. Enabling this will cause performance to drop,
       as the driver will start doing a lot of un-necessary cache clean/flush operations.
       Enable the D-Cache only if there is a need to process the data being transferred by firmware code.
     */
    status = CyU3PDeviceCacheControl (CyTrue, CyFalse, CyFalse);
    if (status != CY_U3P_SUCCESS)
    {
        goto handle_fatal_error;
    }

    /* Configure the IO matrix for the device. On the FX3 DVK board, the COM port 
     * is connected to the IO(53:56). This means that either DQ32 mode should be
     * selected or lppMode should be set to UART_ONLY. Here we are choosing
     * UART_ONLY configuration. */
    io_cfg.isDQ32Bit = CyFalse;
    io_cfg.s0Mode = CY_U3P_SPORT_INACTIVE;
    io_cfg.s1Mode = CY_U3P_SPORT_INACTIVE;
    io_cfg.useUart   = CyTrue;
    io_cfg.useI2C    = CyFalse;
    io_cfg.useI2S    = CyFalse;
    io_cfg.useSpi    = CyFalse;
    io_cfg.lppMode   = CY_U3P_IO_MATRIX_LPP_UART_ONLY;

    /* Enable the GPIO which would have been setup by 2-stage booter. */
    io_cfg.gpioSimpleEn[0]  = 0;
    io_cfg.gpioSimpleEn[1]  = FX3_GPIO_TO_HIFLAG(FX3_GPIO_TEST_OUT);
    io_cfg.gpioComplexEn[0] = 0;
    io_cfg.gpioComplexEn[1] = 0;
    status = CyU3PDeviceConfigureIOMatrix (&io_cfg);
    if (status != CY_U3P_SUCCESS)
    {
        goto handle_fatal_error;
    }

    /* This is a non returnable call for initializing the RTOS kernel */
    CyU3PKernelEntry ();

    /* Dummy return to make the compiler happy */
    return 0;

handle_fatal_error:

    /* Cannot recover from this error. */
    while (1);
}

/* [ ] */

