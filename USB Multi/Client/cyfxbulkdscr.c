
#include "cyfxbulksrcsink.h"

#define VENDOR_ID 0x09,0x12
#define DEVICE_ID 0x15,0x35


 /* Standard device descriptor for USB 3.0 */
const uint8_t CyFxUSB30DeviceDscr[] __attribute__((aligned(32))) =
{
    0x12,                           /* Descriptor size */
    CY_U3P_USB_DEVICE_DESCR,        /* Device descriptor type */
    0x10,0x03,                      /* USB 3.1 */
    0xEF,                           /* Device class */
    0x02,                           /* Device sub-class */
    0x01,                           /* Device protocol */
    0x09,                           /* Maxpacket size for EP0 : 2^9 */
    VENDOR_ID,                      /* Vendor ID (LE) */
    DEVICE_ID,                      /* Product ID (LE ) */
    0x00,0x00,                      /* Device release number */
    0x01,                           /* Manufacture string index */
    0x02,                           /* Product string index */
    0x07,                           /* Serial number string index */
    0x01                            /* Number of configurations */
};

/* Standard device descriptor for USB 2.0 */
const uint8_t CyFxUSB20DeviceDscr[] __attribute__((aligned(32))) =
{
    0x12,                           /* Descriptor size */
    CY_U3P_USB_DEVICE_DESCR,        /* Device descriptor type */
    0x10,0x02,                      /* USB 2.10 */
    0xEF,                           /* Device class */
    0x02,                           /* Device sub-class */
    0x01,                           /* Device protocol */
    0x40,                           /* Maxpacket size for EP0 : 64 bytes */
    VENDOR_ID,                      /* Vendor ID (LE) */
    DEVICE_ID,                      /* Product ID (LE) */
    0x00,0x00,                      /* Device release number */
    0x01,                           /* Manufacture string index */
    0x02,                           /* Product string index */
    0x07,                           /* Serial number string index */
    0x01                            /* Number of configurations */
};

/* Binary device object store descriptor */
const uint8_t CyFxUSBBOSDscr[] __attribute__((aligned(32))) =
{
    0x05,                           /* Descriptor size */
    CY_U3P_BOS_DESCR,               /* Device descriptor type */
    0x16,0x00,                      /* Length of this descriptor and all sub descriptors */
    0x02,                           /* Number of device capability descriptors */

    /* USB 2.0 extension */
    0x07,                           /* Descriptor size */
    CY_U3P_DEVICE_CAPB_DESCR,       /* Device capability type descriptor */
    CY_U3P_USB2_EXTN_CAPB_TYPE,     /* USB 2.0 extension capability type */
    0x1E,0x64,0x00,0x00,            /* Supported device level features: LPM support, BESL supported,
                                       Baseline BESL=400 us, Deep BESL=1000 us. */

   /* SuperSpeed device capability */
   0x0A,                           /* Descriptor size */
   CY_U3P_DEVICE_CAPB_DESCR,       /* Device capability type descriptor */
   CY_U3P_SS_USB_CAPB_TYPE,        /* SuperSpeed device capability type */
   0x00,                           /* Supported device level features  */
   0x0E,0x00,                      /* Speeds supported by the device : SS, HS and FS */
   0x03,                           /* Functionality support */
   0x0A,                           /* U1 Device Exit latency */
   0xFF,0x07                       /* U2 Device Exit latency */
};

/* Standard device qualifier descriptor */
const uint8_t CyFxUSBDeviceQualDscr[] __attribute__((aligned(32))) =
{
    0x0A,                           /* Descriptor size */
    CY_U3P_USB_DEVQUAL_DESCR,       /* Device qualifier descriptor type */
    0x00,0x02,                      /* USB 2.0 */
    0xEF,                           /* Device class */
    0x02,                           /* Device sub-class */
    0x01,                           /* Device protocol */
    0x40,                           /* Maxpacket size for EP0 : 64 bytes */
    0x01,                           /* Number of configurations */
    0x00                            /* Reserved */
};

/* Standard super speed configuration descriptor */
const uint8_t CyFxUSBSSConfigDscr1[] __attribute__((aligned(32))) =
{
    /* Config Descriptor */
    0x09, CY_U3P_USB_CONFIG_DESCR,
    0x34, 0x00,                     /* wTotalLength */
    0x01,                           /* Number of interfaces */
    0x01,                           /* Configuration number */
    0x00,                           /* Configuration string index */
    0x80,                           /* Config characteristics - Bus powered */
    0x32,                           /* Max power consumption of device (in 8mA unit) : 400mA */

    //IAD Header
    0x8, 0xB, 0, 1, 0x8 /* Mass Storage */, 6 /* SCSI transparent */, 0x50 /* Bulk-only Transport BOT */ , 0x00,

    //CD-ROM Drive
    0x09,                   // bLength
    0x04,                   // bDescriptorType (Interface)
    0x00,                   // bInterfaceNumber
    0x00,                   // bAlternateSetting
    0x02,                   // bNumEndpoints
    0x08,                   // bInterfaceClass (Mass Storage)
    0x06,                   // bInterfaceSubClass (SCSI transparent)
    0x50,                   // bInterfaceProtocol (Bulk-Only)
    0x03,                   // iInterface

    // Endpoint Descriptor (Bulk IN)
    0x07,                   // bLength
    0x05,                   // bDescriptorType (Endpoint)
    CY_FX_EP_CD_ROM_IN,     // bEndpointAddress (IN endpoint 1)
    0x02,                   // bmAttributes (Bulk)
    0x00, 0x04,             // wMaxPacketSize
    0x00,                   // bInterval

    /* Super speed endpoint companion descriptor for consumer EP */
    0x06,                           /* Descriptor size */
    CY_U3P_SS_EP_COMPN_DESCR,       /* SS endpoint companion descriptor type */
    (CY_FX_EP_BURST_LENGTH - 1),    /* Max no. of packets in a burst(0-15) - 0: burst 1 packet at a time */
    0x00,                           /* Max streams for bulk EP = 0 (No streams) */
    0x00,0x00,                      /* Service interval for the EP : 0 for bulk */

    // Endpoint Descriptor (Bulk OUT)
    0x07,                   // bLength
    0x05,                   // bDescriptorType (Endpoint)
    CY_FX_EP_CD_ROM_OUT,     // bEndpointAddress (OUT endpoint 1)
    0x02,                   // bmAttributes (Bulk)
    0x00, 0x04,             // wMaxPacketSize
    0x00,                   // bInterval

    /* Super speed endpoint companion descriptor for consumer EP */
    0x06,                           /* Descriptor size */
    CY_U3P_SS_EP_COMPN_DESCR,       /* SS endpoint companion descriptor type */
    (CY_FX_EP_BURST_LENGTH - 1),    /* Max no. of packets in a burst(0-15) - 0: burst 1 packet at a time */
    0x00,                           /* Max streams for bulk EP = 0 (No streams) */
    0x00,0x00,                      /* Service interval for the EP : 0 for bulk */

#ifdef BLAH
    //IAD Header
    0x8, 0xB, 0, 1, 0x3 /* HID */, 0 /* no subclass */, 0x0, 0x00,

    /* Interface Descriptor 1 */
    0x09,                           /* Descriptor size */
    CY_U3P_USB_INTRFC_DESCR,        /* Interface Descriptor type */
    0x00,                           /* Interface number */
    0x00,                           /* Alternate setting number */
    0x02,                           /* Number of end points */
    0x03,                           /* Interface class : HID Class */
    0x00,                           /* Interface sub class : None */
    0x00,                           /* Interface protocol code : None */
    0x03,                           /* Interface descriptor string index */

    /* HID Descriptor */
    0x09,                           /* Descriptor size */
    CY_FX_USB_HID_DESC_TYPE,        /* Descriptor Type */
    0x10,0x11,                      /* HID Class Spec 11.1 */
    0x00,                           /* Target Country */
    0x01,                           /* Total HID Class Descriptors */
    0x22,                           /* Report Descriptor Type */
    0x1A,0x00,                      /* Total Length of Report Descriptor */

    /* Endpoint Descriptor 1 */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint Descriptor Type */
    CY_FX_EP_PRODUCER1,              /* Endpoint address and description */
    CY_U3P_USB_EP_BULK,             /* Bulk Endpoint Type */
    0x00,0x04,                      /* Max packet size = 1024 bytes */
    0x00,                            /* Servicing interval for data transfers : 0 for bulk */

    /* Super speed endpoint companion descriptor for consumer EP */
    0x06,                           /* Descriptor size */
    CY_U3P_SS_EP_COMPN_DESCR,       /* SS endpoint companion descriptor type */
    (CY_FX_EP_BURST_LENGTH - 1),    /* Max no. of packets in a burst(0-15) - 0: burst 1 packet at a time */
    0x00,                           /* Max streams for bulk EP = 0 (No streams) */
    0x00,0x00,                      /* Service interval for the EP : 0 for bulk */

    /* Endpoint Descriptor */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint Descriptor Type */
    CY_FX_EP_CONSUMER1,              /* Endpoint address and description */
    CY_U3P_USB_EP_BULK,             /* Bulk Endpoint Type */
    0x00,0x04,                      /* Max packet size = 1024 bytes */
    0x00,                            /* Servicing interval for data transfers : 0 for bulk */

    /* Super speed endpoint companion descriptor for consumer EP */
    0x06,                           /* Descriptor size */
    CY_U3P_SS_EP_COMPN_DESCR,       /* SS endpoint companion descriptor type */
    (CY_FX_EP_BURST_LENGTH - 1),    /* Max no. of packets in a burst(0-15) - 0: burst 1 packet at a time */
    0x00,                           /* Max streams for bulk EP = 0 (No streams) */
    0x00,0x00,                      /* Service interval for the EP : 0 for bulk */
#endif

};

/* Standard high speed configuration descriptor */
const uint8_t CyFxUSBHSConfigDscr1[] __attribute__((aligned(32))) =
{
    /* Config Descriptor */
    0x09, CY_U3P_USB_CONFIG_DESCR,
    0xE5, 0x00,                     /* wTotalLength */
    0x05,                           /* Number of interfaces */
    0x01,                           /* Configuration number */
    0x00,                           /* Configuration string index */
    0x80,                           /* Config characteristics - Bus powered */
    0x32,                           /* Max power consumption of device (in 2mA unit) : 100mA */


    ///////////////////////

};

/* Standard full speed configuration descriptor */
const uint8_t CyFxUSBFSConfigDscr1[] __attribute__((aligned(32))) =
{
    /* Config Descriptor */
    0x09, CY_U3P_USB_CONFIG_DESCR,
    0xE5, 0x00,                     /* wTotalLength */
    0x05,                           /* Number of interfaces */
    0x01,                           /* Configuration number */
    0x00,                           /* Configuration string index */
    0x80,                           /* Config characteristics - Bus powered */
    0x32,                           /* Max power consumption of device (in 2mA unit) : 100mA */

    //////////////////////////////

};

/* Standard language ID string descriptor */
const uint8_t CyFxUSBStringLangIDDscr[] __attribute__((aligned(32))) =
{
    0x04,                           /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    0x09,0x04                       /* Language ID supported */
};

/* Standard manufacturer string descriptor */
const uint8_t CyFxUSBManufactureDscr[] __attribute__((aligned(32))) =
{
    0xE,                           /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'V',0x00,
    'u',0x00,
    'l',0x00,
    'p',0x00,
    'e',0x00,
    's',0x00
};

/* Standard product string descriptor */
const uint8_t CyFxUSBProductDscr[] __attribute__((aligned(32))) =
{
    0x46,                           /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'V',0x00,
    'u',0x00,
    'l',0x00,
    'p',0x00,
    'e',0x00,
    's',0x00,
    ' ',0x00,
    'D',0x00,
    'o',0x00,
    'l',0x00,
    'c',0x00,
    'h',0x00,
    ' ',0x00,
    '-',0x00,
    ' ',0x00,
    'M',0x00,
    'u',0x00,
    'l',0x00,
    't',0x00,
    'i',0x00,
    ' ',0x00,
    'D',0x00,
    'e',0x00,
    'v',0x00,
    'i',0x00,
    'c',0x00,
    'e',0x00,
    ' ',0x00,
    'C',0x00,
    'l',0x00,
    'i',0x00,
    'e',0x00,
    'n',0x00,
    't',0x00,
};


/* Interface descriptor */
const uint8_t CyFxUSBInterfaceDesc1[] __attribute__((aligned(32))) =
{
    0x54,                           /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'V',0x00,
    'u',0x00,
    'l',0x00,
    'p',0x00,
    'e',0x00,
    's',0x00,
    ' ',0x00,
    'D',0x00,
    'o',0x00,
    'l',0x00,
    'c',0x00,
    'h',0x00,
    ' ',0x00,
    '-',0x00,
    ' ',0x00,
    'M',0x00,
    'u',0x00,
    'l',0x00,
    't',0x00,
    'i',0x00,
    ' ',0x00,
    'D',0x00,
    'e',0x00,
    'v',0x00,
    'i',0x00,
    'c',0x00,
    'e',0x00,
    ' ',0x00,
    'C',0x00,
    'D',0x00,
    '-',0x00,
    'R',0x00,
    'O',0x00,
    'M',0x00,
    ' ',0x00,
    'C',0x00,
    'l',0x00,
    'i',0x00,
    'e',0x00,
    'n',0x00,
    't',0x00,
};


/* Interface descriptor */
const uint8_t CyFxUSBInterfaceDesc2[] __attribute__((aligned(32))) =
{
    0x4A,                           /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'V',0x00,
    'u',0x00,
    'l',0x00,
    'p',0x00,
    'e',0x00,
    's',0x00,
    ' ',0x00,
    'D',0x00,
    'o',0x00,
    'l',0x00,
    'c',0x00,
    'h',0x00,
    ' ',0x00,
    '-',0x00,
    ' ',0x00,
    'M',0x00,
    'u',0x00,
    'l',0x00,
    't',0x00,
    'i',0x00,
    ' ',0x00,
    'D',0x00,
    'e',0x00,
    'v',0x00,
    'i',0x00,
    'c',0x00,
    'e',0x00,
    ' ',0x00,
    'H',0x00,
    'D',0x00,
    'D',0x00,
    ' ',0x00,
    'H',0x00,
    'o',0x00,
    's',0x00,
    't',0x00,
};

/* Interface descriptor */
const uint8_t CyFxUSBInterfaceDesc3[] __attribute__((aligned(32))) =
{
    0x58,                           /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'V',0x00,
    'u',0x00,
    'l',0x00,
    'p',0x00,
    'e',0x00,
    's',0x00,
    ' ',0x00,
    'D',0x00,
    'o',0x00,
    'l',0x00,
    'c',0x00,
    'h',0x00,
    ' ',0x00,
    '-',0x00,
    ' ',0x00,
    'M',0x00,
    'u',0x00,
    'l',0x00,
    't',0x00,
    'i',0x00,
    ' ',0x00,
    'D',0x00,
    'e',0x00,
    'v',0x00,
    'i',0x00,
    'c',0x00,
    'e',0x00,
    ' ',0x00,
    'K',0x00,
    'e',0x00,
    'y',0x00,
    'b',0x00,
    '/',0x00,
    'M',0x00,
    'o',0x00,
    'u',0x00,
    's',0x00,
    'e',0x00,
    ' ',0x00,
    'H',0x00,
    'o',0x00,
    's',0x00,
    't',0x00,
};

/* Interface descriptor */
const uint8_t CyFxUSBInterfaceDesc4[] __attribute__((aligned(32))) =
{
    0x4C,                           /* Descriptor size */
    CY_U3P_USB_STRING_DESCR,        /* Device descriptor type */
    'V',0x00,
    'u',0x00,
    'l',0x00,
    'p',0x00,
    'e',0x00,
    's',0x00,
    ' ',0x00,
    'D',0x00,
    'o',0x00,
    'l',0x00,
    'c',0x00,
    'h',0x00,
    ' ',0x00,
    '-',0x00,
    ' ',0x00,
    'M',0x00,
    'u',0x00,
    'l',0x00,
    't',0x00,
    'i',0x00,
    ' ',0x00,
    'D',0x00,
    'e',0x00,
    'v',0x00,
    'i',0x00,
    'c',0x00,
    'e',0x00,
    ' ',0x00,
    'M',0x00,
    'I',0x00,
    'D',0x00,
    'I',0x00,
    ' ',0x00,
    'H',0x00,
    'o',0x00,
    's',0x00,
    't',0x00,
};

/* Microsoft OS Descriptor. */
const uint8_t CyFxUsbOSDscr[] __attribute__((aligned(32))) =
{
    0x0E,
    CY_U3P_USB_STRING_DESCR,
    'O', 0x00,
    'S', 0x00,
    ' ', 0x00,
    'D', 0x00,
    'e', 0x00,
    's', 0x00,
    'c', 0x00
};

/* Serial Number */
const uint8_t CyFxUsbSN[] __attribute__((aligned(32))) =
{
    0x40,
    CY_U3P_USB_STRING_DESCR,
    '2', 0x00,
    'S', 0x00,
    'E', 0x00,
    'R', 0x00,
    'I', 0x00,
    'A', 0x00,
    'L', 0x00,
    'S', 0x00,
    'E', 0x00,
    'R', 0x00,
    'I', 0x00,
    'A', 0x00,
    'L', 0x00,
    'S', 0x00,
    'E', 0x00,
    'R', 0x00,
    'I', 0x00,
    'A', 0x00,
    'L', 0x00,
    'S', 0x00,
    'E', 0x00,
    'R', 0x00,
    'I', 0x00,
    'A', 0x00,
    'L', 0x00,
    'S', 0x00,
    'E', 0x00,
    'R', 0x00,
    'I', 0x00,
    'A', 0x00,
    'L', 0x00
};

/* HID Report Descriptor */
const uint8_t CyFxUSBReportDscr[] __attribute__((aligned(32))) =
{
        0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
        0x09, 0x00,                    // USAGE (Undefined)
        0xa1, 0x01,                    // COLLECTION (Application)
        0x95, 0x01,                    //   REPORT_COUNT (1)
        0x75, 0x08,                    //   REPORT_SIZE (8)
        0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
        0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
        0x05, 0x0a,                    //   USAGE_PAGE (Ordinals)
        0x19, 0x01,                    //   USAGE_MINIMUM (Instance 1)
        0x29, 0x01,                    //   USAGE_MAXIMUM (Instance 1)
        0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
        0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs)
        0xc0                           // END_COLLECTION
};

/* Place this buffer as the last buffer so that no other variable / code shares
 * the same cache line. Do not add any other variables / arrays in this file.
 * This will lead to variables sharing the same cache line. */
const uint8_t CyFxUsbDscrAlignBuffer[32] __attribute__((aligned(32)));

/* [ ] */

