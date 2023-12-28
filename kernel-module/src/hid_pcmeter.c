// SPDX-License-Identifier: GPL-2.0-only
/*
 * Simple USB CPU load meter driver
 *
 * Copyright 2023 Pascal Jaeger
 * Based on drivers/hid/hid-led.c
 */

#include "core.h"
#include <linux/hid.h>
#include <linux/hidraw.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/kernel_stat.h>
#include <linux/cpumask.h>
#include <linux/tick.h>

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

struct hid_work {
	struct work_struct work_arg;
	struct hidpcmeter_device* ldev;
	int    interval;
};

struct hidpcmeter_device {
	const struct hidpcmeter_config *config;
	struct hid_device          *hdev;
	u8			               *buf;
	struct mutex		       lock;
	bool connected;
};

/* send interval in ms */
static int interval = 1000;
module_param(interval,int,0660);

static void thread_function(struct work_struct *work_arg);

static void thread_function(struct work_struct *work_arg)
{
	struct hid_work *hid_work = container_of(work_arg, struct hid_work, work_arg);
	struct hidpcmeter_device* ldev;

	if (!hid_work) goto exit;
	ldev = hid_work->ldev;
	if (!ldev || !ldev->connected) goto exit;
	ldev->config->write(ldev);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(msecs_to_jiffies(hid_work->interval));
	schedule_work(work_arg);
	return;

exit:
	kfree(hid_work);
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

static u64 my_get_idle_time(struct kernel_cpustat *kcs, int cpu)
{
	u64 idle, idle_usecs = -1ULL;

	if (cpu_online(cpu))
		idle_usecs = get_cpu_idle_time_us(cpu, NULL);

	if (idle_usecs == -1ULL)
		/* !NO_HZ or cpu offline so we can rely on cpustat.idle */
		idle = kcs->cpustat[CPUTIME_IDLE];
	else
		idle = idle_usecs * NSEC_PER_USEC;

	return idle;
}

static ssize_t pcmeter_pico_write(struct hidpcmeter_device *ldev)
{
	__u8 buf[MAX_REPORT_SIZE] = {};
	struct kernel_cpustat kcpustat;

	static u64 old_timestamp = 1;
	static u64 old_cpu_idle = 1;
	u64 idle = 0 ;
	u64 timestamp = 0;
	int cpu_percent = 0;
	int i;

	for_each_possible_cpu(i) {
		struct kernel_cpustat kcpustat;
		/* u64 *cpustat = kcpustat.cpustat; */
		kcpustat_cpu_fetch(&kcpustat, i);
		idle		+= my_get_idle_time(&kcpustat, i);
	}
	timestamp = ktime_get_ns();
	// TODO get nr of CPU from NR_CPUS bitmap
	cpu_percent = 100 - ((((idle - old_cpu_idle) / 24) *100 / (timestamp - old_timestamp)));
	/* printk("old_cpu_idle: %llu", old_cpu_idle); */
	/* printk("idle: %llu", idle); */
	/* printk("old_timestamp: %llu", old_timestamp); */
	/* printk("timestamp: %llu", timestamp); */
	printk("NRCPU: %d", NR_CPUS);
	printk("idlediff: %llu\n", (idle - old_cpu_idle) / 12);
	printk("timediff: %llu", timestamp - old_timestamp);
	printk("percent: %d", cpu_percent);

	old_timestamp = timestamp;
	old_cpu_idle = idle;

	// TODO for debug only. Later get actual mem and cpu data
	buf[1] = cpu_percent;
	buf[2] = 20;

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
	struct hid_work *hid_work;
	int ret, i;

	ldev = devm_kzalloc(&hdev->dev, sizeof(*ldev), GFP_KERNEL);
	if (!ldev)
		return -ENOMEM;
	hid_set_drvdata(hdev, ldev);

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
	ldev->connected = true;

	hid_work = kmalloc(sizeof(*hid_work), GFP_KERNEL);
	INIT_WORK(&hid_work->work_arg, thread_function);
	hid_work->ldev = ldev;
	hid_work->interval = interval;
	schedule_work(&hid_work->work_arg);

	hid_info(hdev, "%s initialized\n", ldev->config->name);

	return 0;
}

static void hidpcmeter_remove(struct hid_device *hdev)
{
	struct hidpcmeter_device *ldev = hid_get_drvdata(hdev);
	ldev->connected = false;
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
