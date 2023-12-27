// SPDX-License-Identifier: GPL-2.0-only
/*
 * Simple USB CPU load meter driver
 *
 * Copyright 2023 Pascal Jaeger
 * Based on drivers/hid/hid-led.c
 */

#define DEBUG
#include "core.h"
#include <linux/hid.h>
#include <linux/hidraw.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#define USB_VENDOR_ID_PC_METER_PICO 0x2e8a
#define USB_DEVICE_ID_PC_METER_PICO 0xc011
#define MAX_REPORT_SIZE		64

enum hidpcmeter_report_type {
    RAW_REQUEST,
	OUTPUT_REPORT
};

enum hidpcmeter_type {
	PC_METER_PICO,
};

struct hidpcmeter_device;

struct hidpcmeter_config {
	enum hidpcmeter_type	type;
	const char		*name;
	const char		*short_name;
	size_t			report_size;
	enum hidpcmeter_report_type	report_type;
	ssize_t (*write)(struct hidpcmeter_device *ldev);
};

struct work_cont {
	struct work_struct real_work;
	int    arg;
} work_cont;

struct hidpcmeter_device {
	const struct hidpcmeter_config *config;
	struct hid_device          *hdev;
	u8			               *buf;
	struct mutex		       lock;
	struct work_cont           hid_work;
};

static void thread_function(struct work_struct *work);

static void thread_function(struct work_struct *work_arg)
{

	/* set_current_state(TASK_UNINTERRUPTIBLE); */
	if (!work_arg) goto exit;
	printk(KERN_INFO "[Deferred work]=> PID: %d; NAME: %s\n", current->pid, current->comm);
	struct work_cont *work_cont = container_of(work_arg, struct work_cont, real_work);
	if (!work_cont) goto exit;
	struct hidpcmeter_device* ldev = container_of(work_cont, struct hidpcmeter_device, hid_work);
	if (!ldev) goto exit;
	ldev->config->write(ldev);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(work_cont->arg * HZ); //Wait 1s
	schedule_work(work_arg);
	return;

exit:
	printk(KERN_INFO "Something is missing in worker %d %s", current->pid, current->comm);
	return;
}

static int hidpcmeter_send(struct hidpcmeter_device *ldev, __u8 *buf)
{
	int ret;

	mutex_lock(&ldev->lock);

	/*
	 * buffer provided to hid_hw_raw_request must not be on the stack
	 * and must not be part of a data structure
	 */
	memcpy(ldev->buf, buf, ldev->config->report_size);

	if (ldev->config->report_type == RAW_REQUEST) {
		ret = hid_hw_raw_request(ldev->hdev, buf[0], ldev->buf,
					 ldev->config->report_size,
					 HID_FEATURE_REPORT,
					 HID_REQ_SET_REPORT);
	}
	else if (ldev->config->report_type == OUTPUT_REPORT)
	{
		ret = hid_hw_output_report(ldev->hdev, ldev->buf,
					   ldev->config->report_size);
	}
	else
		ret = -EINVAL;

	mutex_unlock(&ldev->lock);

	if (ret < 0)
		return ret;

	return ret == ldev->config->report_size ? 0 : -EMSGSIZE;
}

static ssize_t pcmeter_pico_write(struct hidpcmeter_device *ldev)
{
	__u8 buf[MAX_REPORT_SIZE] = {};

	// TODO for debug only. Later get actual mem and cpu data
	buf[1] = 'C';
	buf[2] = 80;
	buf[3] = 'M';
	buf[4] = 20;

	return hidpcmeter_send(ldev, buf);
}

static const struct hidpcmeter_config hidpcmeter_configs[] = {
{
    .type = PC_METER_PICO,
	.name = "PC Meter Pico v1",
	.short_name = "pcmeter-pico",
	.report_size = 64,
	.report_type = OUTPUT_REPORT,
	.write = pcmeter_pico_write,
}
};

static int hidpcmeter_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	struct hidpcmeter_device *ldev;
	int ret, i;

	ldev = devm_kzalloc(&hdev->dev, sizeof(*ldev), GFP_KERNEL);
	if (!ldev)
		return -ENOMEM;

	ldev->buf = devm_kmalloc(&hdev->dev, MAX_REPORT_SIZE, GFP_KERNEL);
	if (!ldev->buf)
		return -ENOMEM;

	ret = hid_parse(hdev);
	if (ret)
		return ret;

	ldev->hdev = hdev;
	mutex_init(&ldev->lock);

	for (i = 0; !ldev->config && i < ARRAY_SIZE(hidpcmeter_configs); i++) {
		if (hidpcmeter_configs[i].type == id->driver_data) {
			ldev->config = &hidpcmeter_configs[i];
		}
	}

	if (!ldev->config)
		return -EINVAL;

	ret = hid_hw_start(hdev, HID_CONNECT_HIDRAW);
	if (ret)
		return ret;

	ret = hid_hw_open(hdev);
	if (ret) {
		hid_err(hdev,
			"%s:hid_hw_open\n",
			__func__);
		return ret;
	}

	INIT_WORK(&(ldev->hid_work.real_work), thread_function);
	ldev->hid_work.arg = 1; // TODO get arg from commandline
	schedule_work(&(ldev->hid_work.real_work));

	hid_info(hdev, "%s initialized\n", ldev->config->name);

	return 0;
}

static void hidpcmeter_remove(struct hid_device *hdev)
{
	printk("removed!!!");
	hid_hw_stop(hdev);
}

static const struct hid_device_id hidpcmeter_table[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_PC_METER_PICO,
	  USB_DEVICE_ID_PC_METER_PICO), .driver_data = PC_METER_PICO },
	{ }
};
MODULE_DEVICE_TABLE(hid, hidpcmeter_table);

static struct hid_driver hidpcmeter_driver = {
	.name = "hid-pc-meter",
	.probe = hidpcmeter_probe,
	.id_table = hidpcmeter_table,
	.remove = hidpcmeter_remove,
};

module_hid_driver(hidpcmeter_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pascal Jaeger <pascal.jaeger@leimstift.de");
MODULE_DESCRIPTION("Simple USB CPU load meter driver");
