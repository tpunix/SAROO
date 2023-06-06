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

#ifndef __TUSBH_HID_H__
#define __TUSBH_HID_H__

#include "tusbh.h"


#define HID_BOOT_CODE                0x01    
#define HID_KEYBRD_BOOT_CODE         0x01
#define HID_MOUSE_BOOT_CODE          0x02

#define USB_HID_GET_REPORT           0x01
#define USB_HID_GET_IDLE             0x02
#define USB_HID_GET_PROTOCOL         0x03
#define USB_HID_SET_REPORT           0x09
#define USB_HID_SET_IDLE             0x0A
#define USB_HID_SET_PROTOCOL         0x0B    

#define HID_INPUT_REPORT             0x01
#define HID_OUTPUT_REPORT            0x02
#define HID_FEATURE_REPORT           0x03


typedef struct _tusbh_hid_driver tusbh_hid_driver_t;

typedef struct _tusbh_hid_info
{
	tusbh_device_t*  dev;
	tusbh_interface_t* interface;

    tusbh_ep_info_t* ep_in;
    tusbh_ep_info_t* ep_out;
    uint8_t*         report_desc;
    uint32_t         report_desc_len;

	tusbh_hid_driver_t *driver;
}tusbh_hid_info_t;


typedef struct hid_item_t{
	u8  id;
	u8  type;
	u8  flag;
	u8  size;
	u8  count;
	u8  usize;
	u16 usage;
	u16 page;
}HID_ITEM;


struct _tusbh_hid_driver
{
	tusbh_hid_info_t *info;
	char *name;

	uint16_t vid;
	uint16_t pid;
	HID_ITEM *main_items;
	int items_nr;

	int (*init)(tusbh_hid_info_t *info);
	int (*exit)(tusbh_hid_info_t *info);
	int (*on_recv_data)(tusbh_ep_info_t* ep, const uint8_t* data, uint32_t len);
	int (*on_send_done)(tusbh_ep_info_t* ep, channel_state_t state);
};


extern tusbh_class_t  tusbh_class_hid;
extern tusbh_hid_driver_t **tusbh_hid_driver_list;

int tusbh_hid_set_idle(tusbh_device_t *dev, int idle, int report_id);
int tusbh_hid_set_protocol(tusbh_device_t *dev, int protocol);
int tusbh_hid_set_report(tusbh_device_t *dev, int report_type, int report_id, void *buf, int len);
int tusbh_hid_get_report(tusbh_device_t *dev, int report_type, int report_id, void *buf, int len);

int tusbh_hid_send_data(tusbh_ep_info_t* ep, void* data, uint32_t len);

void dump_report_desc(u8 *desc_buf, int desc_size);
int match_report_desc(u8 *desc_buf, int desc_size, HID_ITEM *items, int items_size);

#endif
