#include <stdio.h>
#include <conio.h>

#define uint8_t unsigned char
#define __attribute__(x)

#pragma pack(1)

typedef enum CyU3PUsbDescType
{
    CY_U3P_USB_DEVICE_DESCR = 0x01,      /**< Device descriptor  */
    CY_U3P_USB_CONFIG_DESCR,             /**< Configuration descriptor */
    CY_U3P_USB_STRING_DESCR,             /**< String descriptor */
    CY_U3P_USB_INTRFC_DESCR,             /**< Interface descriptor */
    CY_U3P_USB_ENDPNT_DESCR,             /**< Endpoint descriptor */
    CY_U3P_USB_DEVQUAL_DESCR,            /**< Device Qualifier descriptor */
    CY_U3P_USB_OTHERSPEED_DESCR,         /**< Other Speed Configuration descriptor */
    CY_U3P_USB_INTRFC_POWER_DESCR,       /**< Interface power descriptor descriptor */
    CY_U3P_USB_OTG_DESCR,                /**< OTG descriptor */
    CY_U3P_BOS_DESCR = 0x0F,             /**< BOS descriptor */
    CY_U3P_DEVICE_CAPB_DESCR,            /**< Device Capability descriptor */
    CY_U3P_USB_HID_DESCR = 0x21,         /**< HID descriptor */
    CY_U3P_USB_REPORT_DESCR,             /**< Report descriptor */
    CY_U3P_SS_EP_COMPN_DESCR = 0x30      /**< Endpoint companion descriptor */
} CyU3PUsbDescType;

typedef enum CyU3PUsbEpType_t
{
    CY_U3P_USB_EP_CONTROL = 0,          /**< Control Endpoint Type */
    CY_U3P_USB_EP_ISO = 1,              /**< Isochronous Endpoint Type */
    CY_U3P_USB_EP_BULK = 2,             /**< Bulk Endpoint Type */
    CY_U3P_USB_EP_INTR = 3              /**< Interrupt Endpoint Type */
} CyU3PUsbEpType_t;

#define CY_FX_USB_HID_DESC_TYPE               (0x21)          /* HID Descriptor */
#define CY_FX_HID_EP_INTR_IN                  (0x81)          /* EP 1 IN */
#define CY_FX_EP_BURST_LENGTH                   (16)

#define CY_FX_EP_PRODUCER1 0
#define CY_FX_EP_CONSUMER1 0
#define CY_FX_EP_PRODUCER2 0
#define CY_FX_EP_CONSUMER2 0
#define CY_FX_EP_PRODUCER3 0
#define CY_FX_EP_CONSUMER3 0
#define CY_FX_EP_PRODUCER4 0
#define CY_FX_EP_CONSUMER4 0

#define MIDI_AUDIO								0x01
#define MIDI_AUDIO_CONTROL						0x01
#define MIDI_CS_INTERFACE						0x24
#define MIDI_CS_ENDPOINT						0x25
#define MIDI_STREAMING							0x3
#define MIDI_JACK_EMD							0x01
#define MIDI_JACK_EXT							0X02
#define USB_ENDPOINT_TYPE_BULK                 0x02
#define MIDI_BUFFER_SIZE_SMALL			0x40,0x00
#define MIDI_BUFFER_SIZE_BIG			0x00,0x04

#define MIDI_ENDPOINT_OUT	CY_FX_EP_PRODUCER4
#define MIDI_ENDPOINT_IN	CY_FX_EP_CONSUMER4
#define MIDI_AC_INTERFACE	0x04
#define MIDI_INTERFACE 0x05

#define lowByte(w) ((uint8_t) ((w) & 0xff))
#define highByte(w) ((uint8_t) ((w) >> 8))
#define USB_ENDPOINT_OUT(addr)                 (lowByte((addr) | 0x00))
#define USB_ENDPOINT_IN(addr)                  (lowByte((addr) | 0x80))

#define D_IAD(_firstInterface, _count, _class, _subClass, _protocol) { 8, 11, _firstInterface, _count, _class, _subClass, _protocol, 0 }
#define D_INTERFACE(_n,_numEndpoints,_class,_subClass,_protocol) { 9, 4, _n, 0, _numEndpoints, _class,_subClass, _protocol, 0 }
#define D_AC_INTERFACE(_streamingInterfaces, _MIDIInterface) { 9, MIDI_CS_INTERFACE, 0x1, 0x0100, 0x0009, _streamingInterfaces, (uint8_t)(_MIDIInterface) }
#define D_AS_INTERFACE 	{ 0x7, MIDI_CS_INTERFACE, 0x01,0x0100, 0x0041}
#define D_MIDI_INJACK(jackProp, _jackID) { 0x06, MIDI_CS_INTERFACE, 0x02, jackProp, _jackID, 0  }
#define D_MIDI_OUTJACK(jackProp, _jackID, _nPins, _srcID, _srcPin) { 0x09, MIDI_CS_INTERFACE, 0x3, jackProp, _jackID, _nPins, _srcID, _srcPin, 0  }
#define D_MIDI_JACK_EP(_addr,_attr,_packetSize) { 9, 5, _addr,_attr,_packetSize, 0, 0, 0}
#define D_MIDI_AC_JACK_EP(_nMIDI, _iDMIDI) { 5, MIDI_CS_ENDPOINT, 0x1, _nMIDI, _iDMIDI}


#define structcheck CyFxUSBSSConfigDscr

const uint8_t CyFxUSBSSConfigDscr[] __attribute__((aligned(32))) =
{
    /* Config Descriptor */
    0x09, CY_U3P_USB_CONFIG_DESCR,
    0x09, 0x01,                     /* wTotalLength */
    0x05,                           /* Number of interfaces */
    0x01,                           /* Configuration number */
    0x00,                           /* Configuration string index */
    0x80,                           /* Config characteristics - Bus powered */
    0x32,                           /* Max power consumption of device (in 2mA unit) : 100mA */

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

    /* Endpoint Descriptor */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint Descriptor Type */
    CY_FX_EP_CONSUMER1,              /* Endpoint address and description */
    CY_U3P_USB_EP_BULK,             /* Bulk Endpoint Type */
    0x00,0x04,                      /* Max packet size = 1024 bytes */
    0x00,                            /* Servicing interval for data transfers : 0 for bulk */

    //////////////////////////////

    //IAD Header
    0x8, 0xB, 1, 1, 0x3 /* HID */, 0 /* no subclass */, 0x0, 0x00,

    /* Interface Descriptor 2 */
    0x09,                           /* Descriptor size */
    CY_U3P_USB_INTRFC_DESCR,        /* Interface Descriptor type */
    0x01,                           /* Interface number */
    0x00,                           /* Alternate setting number */
    0x02,                           /* Number of end points */
    0x03,                           /* Interface class : HID Class */
    0x00,                           /* Interface sub class : None */
    0x00,                           /* Interface protocol code : None */
    0x04,                           /* Interface descriptor string index */

    /* HID Descriptor */
    0x09,                           /* Descriptor size */
    CY_FX_USB_HID_DESC_TYPE,        /* Descriptor Type */
    0x10,0x11,                      /* HID Class Spec 11.1 */
    0x00,                           /* Target Country */
    0x01,                           /* Total HID Class Descriptors */
    0x22,                           /* Report Descriptor Type */
    0x1A,0x00,                      /* Total Length of Report Descriptor */

    /* Endpoint Descriptor */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint Descriptor Type */
    CY_FX_EP_PRODUCER2,              /* Endpoint address and description */
    CY_U3P_USB_EP_BULK,             /* Bulk Endpoint Type */
    0x00,0x04,                      /* Max packet size = 1024 bytes */
    0x00,                            /* Servicing interval for data transfers : 0 for bulk */

    /* Endpoint Descriptor */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint Descriptor Type */
    CY_FX_EP_CONSUMER2,              /* Endpoint address and description */
    CY_U3P_USB_EP_BULK,             /* Bulk Endpoint Type */
    0x00,0x04,                      /* Max packet size = 1024 bytes */
    0x00,                            /* Servicing interval for data transfers : 0 for bulk */

    //////////////////////////////

    //IAD Header
    0x8, 0xB, 2, 1, 0x3 /* HID */, 0 /* no subclass */, 0x0, 0x00,

    /* Interface Descriptor 3 */
    0x09,                           /* Descriptor size */
    CY_U3P_USB_INTRFC_DESCR,        /* Interface Descriptor type */
    0x02,                           /* Interface number */
    0x00,                           /* Alternate setting number */
    0x02,                           /* Number of end points */
    0x03,                           /* Interface class : HID Class */
    0x00,                           /* Interface sub class : None */
    0x00,                           /* Interface protocol code : None */
    0x05,                           /* Interface descriptor string index */

    /* HID Descriptor */
    0x09,                           /* Descriptor size */
    CY_FX_USB_HID_DESC_TYPE,        /* Descriptor Type */
    0x10,0x11,                      /* HID Class Spec 11.1 */
    0x00,                           /* Target Country */
    0x01,                           /* Total HID Class Descriptors */
    0x22,                           /* Report Descriptor Type */
    0x1A,0x00,                      /* Total Length of Report Descriptor */

    /* Endpoint Descriptor */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint Descriptor Type */
    CY_FX_EP_PRODUCER3,              /* Endpoint address and description */
    CY_U3P_USB_EP_BULK,             /* Bulk Endpoint Type */
    0x00,0x04,                      /* Max packet size = 1024 bytes */
    0x00,                            /* Servicing interval for data transfers : 0 for bulk */

    /* Endpoint Descriptor */
    0x07,                           /* Descriptor size */
    CY_U3P_USB_ENDPNT_DESCR,        /* Endpoint Descriptor Type */
    CY_FX_EP_CONSUMER3,              /* Endpoint address and description */
    CY_U3P_USB_EP_BULK,             /* Bulk Endpoint Type */
    0x00,0x04,                      /* Max packet size = 1024 bytes */
    0x00,                           /* Servicing interval for data transfers : 0 for bulk */

    //////////////////////////////

    //MIDI

    //IAD Header
    0x8, 0xB, MIDI_AC_INTERFACE, 2, MIDI_AUDIO, MIDI_AUDIO_CONTROL, 0x0, 0x00,

    0x9, CY_U3P_USB_INTRFC_DESCR, MIDI_AC_INTERFACE, 0x0, 0, MIDI_AUDIO, MIDI_AUDIO_CONTROL, 0x0, 0x06,
    0x9, MIDI_CS_INTERFACE, 0x1, 0x00, 0x01, 0x09, 0x00, 0x1, MIDI_INTERFACE,
    0x9, CY_U3P_USB_INTRFC_DESCR, MIDI_INTERFACE, 0x0, 2, MIDI_AUDIO, MIDI_STREAMING, 0x0, 0x06,
    0x7, MIDI_CS_INTERFACE, 0x1, 0x00, 0x01, 0x41, 0x00,
    0x6, MIDI_CS_INTERFACE, 0x2, MIDI_JACK_EMD, 0x1, 0x6,
    0x9, MIDI_CS_INTERFACE, 0x3, MIDI_JACK_EXT, 0x4, 1, 1, 1, 0x0,
    0x6, MIDI_CS_INTERFACE, 0x2, MIDI_JACK_EXT, 0x2, 0x0,
    0x9, MIDI_CS_INTERFACE, 0x3, MIDI_JACK_EMD, 0x3, 1, 2, 1, 0x6,
    0x9, 0x5, CY_FX_EP_PRODUCER4, USB_ENDPOINT_TYPE_BULK, MIDI_BUFFER_SIZE_BIG, 0, 0, 0,

    0x5, MIDI_CS_ENDPOINT, 0x1, 1, 1,
    0x9, 0x5, CY_FX_EP_CONSUMER4, USB_ENDPOINT_TYPE_BULK, MIDI_BUFFER_SIZE_BIG, 0, 0, 0,

    0x5, MIDI_CS_ENDPOINT, 0x1, 1, 3,

};



int main()
{
    wprintf(L"sizeof() = %i 0x%X\n", (int)sizeof(structcheck), (int)sizeof(structcheck));
    _getch();
}

