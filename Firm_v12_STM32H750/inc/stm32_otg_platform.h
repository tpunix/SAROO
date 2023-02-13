/*       
 *         _______                    _    _  _____ ____  
 *        |__   __|                  | |  | |/ ____|  _ \
 *           | | ___  ___ _ __  _   _| |  | | (___ | |_) |
 *           | |/ _ \/ _ \ '_ \| | | | |  | |\___ \|  _ < 
 *           | |  __/  __/ | | | |_| | |__| |____) | |_) |
 *           |_|\___|\___|_| |_|\__, |\____/|_____/|____/ 
 *                               __/ |                    
 *                              |___/                     
 *
 * TeenyUSB - light weight usb stack for STM32 micro controllers
 * 
 * Copyright (c) 2019 XToolBox  - admin@xtoolbox.org
 *                         www.tusb.org
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// teeny usb platform header for STM32F7 device
#ifndef __STM32_OTG_PLATFORM_H__
#define __STM32_OTG_PLATFORM_H__

#include "tusb_def.h"


#define PCD_ENDP0                                                     ((uint8_t)0)
#define PCD_ENDP1                                                     ((uint8_t)1)
#define PCD_ENDP2                                                     ((uint8_t)2)
#define PCD_ENDP3                                                     ((uint8_t)3)
#define PCD_ENDP4                                                     ((uint8_t)4)
#define PCD_ENDP5                                                     ((uint8_t)5)
#define PCD_ENDP6                                                     ((uint8_t)6)
#define PCD_ENDP7                                                     ((uint8_t)7)
#define PCD_ENDP8                                                     ((uint8_t)8)
#define PCD_ENDP9                                                     ((uint8_t)9)

#define USB_EP_CONTROL                                                USBD_EP_TYPE_CTRL
#define USB_EP_ISOCHRONOUS                                            USBD_EP_TYPE_ISOC
#define USB_EP_BULK                                                   USBD_EP_TYPE_BULK
#define USB_EP_INTERRUPT                                              USBD_EP_TYPE_INTR

#if defined(STM32F1)


#define USB_OTG_FS_MAX_EP_NUM   4
#define USB_OTG_FS_MAX_CH_NUM   8
#define USB_OTG_HS_MAX_EP_NUM   0
#define USB_OTG_HS_MAX_CH_NUM   0
#define MAX_HC_NUM              8


#elif defined(STM32F4) || defined(STM32F2)


#define USB_OTG_FS_MAX_EP_NUM   4
#define USB_OTG_HS_MAX_EP_NUM   6
#define USB_OTG_FS_MAX_CH_NUM   8
#define USB_OTG_HS_MAX_CH_NUM   12
#define MAX_HC_NUM              12

#define RCC_USB_OTG_FS_CLK_ENABLE()  (RCC->AHB2ENR |=  (RCC_AHB2ENR_OTGFSEN))
#define RCC_USB_OTG_FS_CLK_DISABLE() (RCC->AHB2ENR &= ~(RCC_AHB2ENR_OTGFSEN))

#define RCC_USB_OTG_HS_CLK_ENABLE()     (RCC->AHB1ENR |=  (RCC_AHB1ENR_OTGHSEN))
#define RCC_USB_OTG_HS_CLK_DISABLE()    (RCC->AHB1ENR &= ~(RCC_AHB1ENR_OTGHSEN))

#define RCC_USB_OTG_HS_ULPI_CLK_ENABLE()  (RCC->AHB1ENR |=  (RCC_AHB1ENR_OTGHSULPIEN))
#define RCC_USB_OTG_HS_ULPI_CLK_DISABLE() (RCC->AHB1ENR &= ~(RCC_AHB1ENR_OTGHSULPIEN))

#define RCC_USB_OTG_HS_FORCE_RESET()    (RCC->AHB1RSTR |=  (RCC_AHB1RSTR_OTGHRST))
#define RCC_USB_OTG_HS_RELEASE_RESET()  (RCC->AHB1RSTR &= ~(RCC_AHB1RSTR_OTGHRST))


#elif defined(STM32F7)


#define USB_OTG_FS_MAX_EP_NUM   6
#define USB_OTG_HS_MAX_EP_NUM   9
#define USB_OTG_FS_MAX_CH_NUM   12
#define USB_OTG_HS_MAX_CH_NUM   16
#define MAX_HC_NUM              16


#elif defined(STM32H7)


#define USB_OTG_FS_MAX_EP_NUM   9
#define USB_OTG_HS_MAX_EP_NUM   9
#define USB_OTG_FS_MAX_CH_NUM   16
#define USB_OTG_HS_MAX_CH_NUM   16
#define MAX_HC_NUM              16

#define RCC_USB_OTG_FS_CLK_ENABLE()  (RCC->AHB1ENR |=  (RCC_AHB1ENR_USB2OTGFSEN))
#define RCC_USB_OTG_FS_CLK_DISABLE() (RCC->AHB1ENR &= ~(RCC_AHB1ENR_USB2OTGFSEN))

#define RCC_USB_OTG_HS_CLK_ENABLE()     (RCC->AHB1ENR |=  (RCC_AHB1ENR_USB1OTGHSEN))
#define RCC_USB_OTG_HS_CLK_DISABLE()    (RCC->AHB1ENR &= ~(RCC_AHB1ENR_USB1OTGHSEN))

#define RCC_USB_OTG_HS_ULPI_CLK_ENABLE()  (RCC->AHB1ENR |=  (RCC_AHB1ENR_USB1OTGHSULPIEN))
#define RCC_USB_OTG_HS_ULPI_CLK_DISABLE() (RCC->AHB1ENR &= ~(RCC_AHB1ENR_USB1OTGHSULPIEN))

#define RCC_USB_OTG_HS_FORCE_RESET()    (RCC->AHB1RSTR |=  (RCC_AHB1RSTR_USB1OTGHSRST))
#define RCC_USB_OTG_HS_RELEASE_RESET()  (RCC->AHB1RSTR &= ~(RCC_AHB1RSTR_USB1OTGHSRST))


#else
#error unsupport chip
#endif


#define GPIO_AF10_OTG_FS        ((uint8_t)0x0A)  /* OTG_FS Alternate Function mapping */
#define GPIO_AF10_OTG_HS        ((uint8_t)0x0A)  /* OTG_HS Alternate Function mapping */
#define GPIO_AF12_OTG_HS_FS     ((uint8_t)0x0C)  /* OTG HS configured in FS, Alternate Function mapping */




/** @defgroup USB_Core_Mode_ USB Core Mode
  * @{
  */
#define USB_OTG_MODE_DEVICE                    0U
#define USB_OTG_MODE_HOST                      1U
#define USB_OTG_MODE_DRD                       2U


/** @defgroup USB_LL_Core_Speed USB Low Layer Core Speed
  * @{
  */
#define USB_OTG_SPEED_HIGH                     0U
#define USB_OTG_SPEED_HIGH_IN_FULL             1U
#define USB_OTG_SPEED_FULL                     3U



/** @defgroup USB_LL_CORE_Frame_Interval USB Low Layer Core Frame Interval
  * @{
  */
#define DCFG_FRAME_INTERVAL_80                 0U
#define DCFG_FRAME_INTERVAL_85                 1U
#define DCFG_FRAME_INTERVAL_90                 2U
#define DCFG_FRAME_INTERVAL_95                 3U


/** @defgroup USB_LL_Core_PHY_Frequency USB Low Layer Core PHY Frequency
  * @{
  */
#define DSTS_ENUMSPD_HS_PHY_30MHZ_OR_60MHZ     (0U << 1)
#define DSTS_ENUMSPD_FS_PHY_30MHZ_OR_60MHZ     (1U << 1)
#define DSTS_ENUMSPD_LS_PHY_6MHZ               (2U << 1)
#define DSTS_ENUMSPD_FS_PHY_48MHZ              (3U << 1)


/** @defgroup USB_LL_STS_Defines USB Low Layer STS Defines
  * @{
  */
#define STS_GOUT_NAK                           1U
#define STS_DATA_UPDT                          2U
#define STS_XFER_COMP                          3U
#define STS_SETUP_COMP                         4U
#define STS_SETUP_UPDT                         6U


/** @defgroup USB_LL_Turnaround_Timeout Turnaround Timeout Value
  * @{
  */
#ifndef USBD_HS_TRDT_VALUE
#define USBD_HS_TRDT_VALUE           9U
#endif /* USBD_HS_TRDT_VALUE */
#ifndef USBD_FS_TRDT_VALUE
#define USBD_FS_TRDT_VALUE           5U
#define USBD_DEFAULT_TRDT_VALUE      9U
#endif /* USBD_HS_TRDT_VALUE */


/** @defgroup USB_LL_EP_Type USB Low Layer EP Type
  * @{
  */
#define EP_TYPE_CTRL                           0U
#define EP_TYPE_ISOC                           1U
#define EP_TYPE_BULK                           2U
#define EP_TYPE_INTR                           3U
#define EP_TYPE_MSK                            3U


/** @defgroup USB_LL_HCFG_SPEED_Defines USB Low Layer HCFG Speed Defines
  * @{
  */
#define HCFG_30_60_MHZ                         0U
#define HCFG_48_MHZ                            1U
#define HCFG_6_MHZ                             2U

/** @defgroup USB_LL_HPRT0_PRTSPD_SPEED_Defines USB Low Layer HPRT0 PRTSPD Speed Defines
  * @{
  */
#define HPRT0_PRTSPD_HIGH_SPEED                0U
#define HPRT0_PRTSPD_FULL_SPEED                1U
#define HPRT0_PRTSPD_LOW_SPEED                 2U


#define HCCHAR_CTRL                            0U
#define HCCHAR_ISOC                            1U
#define HCCHAR_BULK                            2U
#define HCCHAR_INTR                            3U


#define HC_PID_DATA0                           0U
#define HC_PID_DATA2                           1U
#define HC_PID_DATA1                           2U
#define HC_PID_SETUP                           3U


#define GRXSTS_PKTSTS_IN                       2U
#define GRXSTS_PKTSTS_IN_XFER_COMP             3U
#define GRXSTS_PKTSTS_DATA_TOGGLE_ERR          5U
#define GRXSTS_PKTSTS_CH_HALTED                7U



#define USBx_PCGCCTL    *(__IO uint32_t *)((uint32_t)USBx_BASE + USB_OTG_PCGCCTL_BASE)
#define USBx_HPRT0      *(__IO uint32_t *)((uint32_t)USBx_BASE + USB_OTG_HOST_PORT_BASE)

#define USBx_DEVICE     ((USB_OTG_DeviceTypeDef *)(USBx_BASE + USB_OTG_DEVICE_BASE))
#define USBx_INEP(i)    ((USB_OTG_INEndpointTypeDef *)(USBx_BASE + USB_OTG_IN_ENDPOINT_BASE + ((i) * USB_OTG_EP_REG_SIZE)))
#define USBx_OUTEP(i)   ((USB_OTG_OUTEndpointTypeDef *)(USBx_BASE + USB_OTG_OUT_ENDPOINT_BASE + ((i) * USB_OTG_EP_REG_SIZE)))
#define USBx_DFIFO(i)   *(__IO uint32_t *)(USBx_BASE + USB_OTG_FIFO_BASE + ((i) * USB_OTG_FIFO_SIZE))

#define USBx_HOST       ((USB_OTG_HostTypeDef *)(USBx_BASE + USB_OTG_HOST_BASE))
#define USBx_HC(i)      ((USB_OTG_HostChannelTypeDef *)(USBx_BASE + USB_OTG_HOST_CHANNEL_BASE + ((i) * USB_OTG_HOST_CHANNEL_SIZE)))

#define USB_MASK_INTERRUPT(__INSTANCE__, __INTERRUPT__)     ((__INSTANCE__)->GINTMSK &= ~(__INTERRUPT__))
#define USB_UNMASK_INTERRUPT(__INSTANCE__, __INTERRUPT__)   ((__INSTANCE__)->GINTMSK |= (__INTERRUPT__))


typedef USB_OTG_GlobalTypeDef  PCD_TypeDef;

#if defined(USB_OTG_FS) && defined(USB_OTG_HS)
#define  USB_CORE_HANDLE_TYPE        PCD_TypeDef*
#define  GetUSB(dev)           ((dev)->handle)
#define  SetUSB(dev, usbx)     do{ (dev)->handle = usbx;  }while(0)

#else
#if defined(USB_OTG_FS)
#define  GetUSB(dev)           USB_OTG_FS
#define  SetUSB(dev, usbx)

#elif defined(USB_OTG_HS)
#define  GetUSB(dev)           USB_OTG_HS
#define  SetUSB(dev, usbx)

#else
#error neither otg_hs or otg_fs
#endif
#endif


#define  USBx_BASE   ((uint32_t)USBx)

#if defined (NEED_MAX_PACKET)
#undef NEED_MAX_PACKET
#endif

#define  GetInMaxPacket(dev, EPn)  get_max_in_packet_size(GetUSB(dev), EPn)
#define  GetOutMaxPacket(dev, EPn) get_max_out_packet_size(GetUSB(dev), EPn)

void flush_rx(USB_OTG_GlobalTypeDef *USBx);
void flush_tx(USB_OTG_GlobalTypeDef *USBx, uint32_t num);

#define init_ep_tx(dev, EPn, type, mps)                                                    \
do{                                                                                        \
  PCD_TypeDef *USBx = GetUSB(dev);                                                         \
  uint32_t maxpacket = mps;                                                                \
  USB_OTG_INEndpointTypeDef* INEp = USBx_INEP(EPn);                                        \
  if(USBx == USB_OTG_FS && EPn == 0){                                                      \
    maxpacket = __CLZ(maxpacket) - 25;                                                     \
  }                                                                                        \
  USBx_DEVICE->DAINTMSK |=  ( (USB_OTG_DAINTMSK_IEPM) & ( (1 << (EPn))) );                 \
  if (((INEp->DIEPCTL) & USB_OTG_DIEPCTL_USBAEP) == 0U)                                    \
  INEp->DIEPCTL |= ((maxpacket & USB_OTG_DIEPCTL_MPSIZ ) | (type << 18 ) |                 \
    ((EPn) << 22 ) | (USB_OTG_DIEPCTL_SD0PID_SEVNFRM) | (USB_OTG_DIEPCTL_USBAEP));         \
}while(0)

#define init_ep_rx(dev, EPn, type, mps)                                                    \
do{                                                                                        \
  PCD_TypeDef *USBx = GetUSB(dev);                                                         \
  uint32_t maxpacket = mps;                                                                \
  if(EPn == 0){                                                                            \
    maxpacket = __CLZ(maxpacket) - 25;                                                     \
  }                                                                                        \
  USBx_DEVICE->DAINTMSK |=  ( (USB_OTG_DAINTMSK_OEPM) & ( (1 << (EPn))<<16 ) );            \
  if (((USBx_OUTEP(EPn)->DOEPCTL) & USB_OTG_DOEPCTL_USBAEP) == 0U)                         \
  USBx_OUTEP(EPn)->DOEPCTL |= ((maxpacket & USB_OTG_DOEPCTL_MPSIZ ) | (type << 18 ) |      \
    (USB_OTG_DOEPCTL_SD0PID_SEVNFRM) | (USB_OTG_DIEPCTL_USBAEP));                          \
}while(0)

#define INIT_EP_Tx(dev, bEpNum, type, maxpacket)  \
do{\
  init_ep_tx(dev, bEpNum, type, maxpacket);\
}while(0)

#define INIT_EP_Rx(dev, bEpNum, type, maxpacket) \
do{\
  init_ep_rx(dev, bEpNum, type, maxpacket);\
}while(0)

// In OTG core, a fifo is used to handle the rx/tx data, all rx ep share a same fifo, each tx ep has a fifo

#define  SET_TX_FIFO(dev, bEpNum, addr, size) \
do{\
  if(bEpNum == 0){\
    GetUSB(dev)->DIEPTXF0_HNPTXFSIZ = (uint32_t)(((uint32_t)( (size)/4) << 16) | ( (addr)/4));\
  }else{\
                       /*avoid the compiler warning */\
    GetUSB(dev)->DIEPTXF[bEpNum==0?0: bEpNum- 1] = (uint32_t)(((uint32_t)( (size)/4) << 16) | ( (addr)/4));\
  }\
}while(0)

#define  SET_RX_FIFO(dev, addr, size) \
  do{\
    GetUSB(dev)->GRXFSIZ = (size/4);\
  }while(0)

#endif

