/* ir-lirc-codec.c - ir-core to classic lirc interface bridge
 *
 * Copyright (C) 2010 by Jarod Wilson <jarod@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <linux/sched.h>
#include <linux/wait.h>
#include <media/lirc.h>
#include <media/ir-core.h>
#include "ir-core-priv.h"
#include "lirc_dev.h"

#define LIRCBUF_SIZE 256

/**
 * ir_lirc_decode() - Send raw IR data to lirc_dev to be relayed to the
 *		      lircd userspace daemon for decoding.
 * @input_dev:	the struct input_dev descriptor of the device
 * @duration:	the struct ir_raw_event descriptor of the pulse/space
 *
 * This function returns -EINVAL if the lirc interfaces aren't wired up.
 */
static int ir_lirc_decode(struct input_dev *input_dev, struct ir_raw_event ev)
{
	struct ir_input_dev *ir_dev = input_get_drvdata(input_dev);

	if (!(ir_dev->raw->enabled_protocols & IR_TYPE_LIRC))
		return 0;

	if (!ir_dev->raw->lirc.drv || !ir_dev->raw->lirc.drv->rbuf)
		return -EINVAL;

	IR_dprintk(2, "LIRC data transfer started (%uus %s)\n",
		   TO_US(ev.duration), TO_STR(ev.pulse));

	ir_dev->raw->lirc.lircdata += ev.duration / 1000;
	if (ev.pulse)
		ir_dev->raw->lirc.lircdata |= PULSE_BIT;

	lirc_buffer_write(ir_dev->raw->lirc.drv->rbuf,
			  (unsigned char *) &ir_dev->raw->lirc.lircdata);
	wake_up(&ir_dev->raw->lirc.drv->rbuf->wait_poll);

	ir_dev->raw->lirc.lircdata = 0;

	return 0;
}

static ssize_t ir_lirc_transmit_ir(struct file *file, const char *buf,
				   size_t n, loff_t *ppos)
{
	struct lirc_codec *lirc;
	struct ir_input_dev *ir_dev;
	int *txbuf; /* buffer with values to transmit */
	int ret = 0, count;

	lirc = lirc_get_pdata(file);
	if (!lirc)
		return -EFAULT;

	if (n % sizeof(int))
		return -EINVAL;

	count = n / sizeof(int);
	if (count > LIRCBUF_SIZE || count % 2 == 0)
		return -EINVAL;

	txbuf = kzalloc(sizeof(int) * LIRCBUF_SIZE, GFP_KERNEL);
	if (!txbuf)
		return -ENOMEM;

	if (copy_from_user(txbuf, buf, n)) {
		ret = -EFAULT;
		goto out;
	}

	ir_dev = lirc->ir_dev;
	if (!ir_dev) {
		ret = -EFAULT;
		goto out;
	}

	if (ir_dev->props && ir_dev->props->tx_ir)
		ret = ir_dev->props->tx_ir(ir_dev->props->priv, txbuf, (u32)n);

out:
	kfree(txbuf);
	return ret;
}

static int ir_lirc_ioctl(struct inode *node, struct file *filep,
			 unsigned int cmd, unsigned long arg)
{
	struct lirc_codec *lirc;
	struct ir_input_dev *ir_dev;
	int ret = 0;
	void *drv_data;
	unsigned long val;

	lirc = lirc_get_pdata(filep);
	if (!lirc)
		return -EFAULT;

	ir_dev = lirc->ir_dev;
	if (!ir_dev || !ir_dev->props || !ir_dev->props->priv)
		return -EFAULT;

	drv_data = ir_dev->props->priv;

	switch (cmd) {
	case LIRC_SET_TRANSMITTER_MASK:
		ret = get_user(val, (unsigned long *)arg);
		if (ret)
			return ret;

		if (ir_dev->props && ir_dev->props->s_tx_mask)
			ret = ir_dev->props->s_tx_mask(drv_data, (u32)val);
		else
			return -EINVAL;
		break;

	case LIRC_SET_SEND_CARRIER:
		ret = get_user(val, (unsigned long *)arg);
		if (ret)
			return ret;

		if (ir_dev->props && ir_dev->props->s_tx_carrier)
			ir_dev->props->s_tx_carrier(drv_data, (u32)val);
		else
			return -EINVAL;
		break;

	case LIRC_GET_SEND_MODE:
		val = LIRC_CAN_SEND_PULSE & LIRC_CAN_SEND_MASK;
		ret = put_user(val, (unsigned long *)arg);
		break;

	case LIRC_SET_SEND_MODE:
		ret = get_user(val, (unsigned long *)arg);
		if (ret)
			return ret;

		if (val != (LIRC_MODE_PULSE & LIRC_CAN_SEND_MASK))
			return -EINVAL;
		break;

	default:
		return lirc_dev_fop_ioctl(node, filep, cmd, arg);
	}

	return ret;
}

static int ir_lirc_open(void *data)
{
	return 0;
}

static void ir_lirc_close(void *data)
{
	return;
}

static struct file_operations lirc_fops = {
	.owner		= THIS_MODULE,
	.write		= ir_lirc_transmit_ir,
	.ioctl		= ir_lirc_ioctl,
	.read		= lirc_dev_fop_read,
	.poll		= lirc_dev_fop_poll,
	.open		= lirc_dev_fop_open,
	.release	= lirc_dev_fop_close,
};

static int ir_lirc_register(struct input_dev *input_dev)
{
	struct ir_input_dev *ir_dev = input_get_drvdata(input_dev);
	struct lirc_driver *drv;
	struct lirc_buffer *rbuf;
	int rc = -ENOMEM;
	unsigned long features;

	drv = kzalloc(sizeof(struct lirc_driver), GFP_KERNEL);
	if (!drv)
		return rc;

	rbuf = kzalloc(sizeof(struct lirc_buffer), GFP_KERNEL);
	if (!drv)
		goto rbuf_alloc_failed;

	rc = lirc_buffer_init(rbuf, sizeof(int), LIRCBUF_SIZE);
	if (rc)
		goto rbuf_init_failed;

	features = LIRC_CAN_REC_MODE2;
	if (ir_dev->props->tx_ir) {
		features |= LIRC_CAN_SEND_PULSE;
		if (ir_dev->props->s_tx_mask)
			features |= LIRC_CAN_SET_TRANSMITTER_MASK;
		if (ir_dev->props->s_tx_carrier)
			features |= LIRC_CAN_SET_SEND_CARRIER;
	}

	snprintf(drv->name, sizeof(drv->name), "ir-lirc-codec (%s)",
		 ir_dev->driver_name);
	drv->minor = -1;
	drv->features = features;
	drv->data = &ir_dev->raw->lirc;
	drv->rbuf = rbuf;
	drv->set_use_inc = &ir_lirc_open;
	drv->set_use_dec = &ir_lirc_close;
	drv->code_length = sizeof(struct ir_raw_event) * 8;
	drv->fops = &lirc_fops;
	drv->dev = &ir_dev->dev;
	drv->owner = THIS_MODULE;

	drv->minor = lirc_register_driver(drv);
	if (drv->minor < 0) {
		rc = -ENODEV;
		goto lirc_register_failed;
	}

	ir_dev->raw->lirc.drv = drv;
	ir_dev->raw->lirc.ir_dev = ir_dev;
	ir_dev->raw->lirc.lircdata = PULSE_MASK;

	return 0;

lirc_register_failed:
rbuf_init_failed:
	kfree(rbuf);
rbuf_alloc_failed:
	kfree(drv);

	return rc;
}

static int ir_lirc_unregister(struct input_dev *input_dev)
{
	struct ir_input_dev *ir_dev = input_get_drvdata(input_dev);
	struct lirc_codec *lirc = &ir_dev->raw->lirc;

	lirc_unregister_driver(lirc->drv->minor);
	lirc_buffer_free(lirc->drv->rbuf);
	kfree(lirc->drv);

	return 0;
}

static struct ir_raw_handler lirc_handler = {
	.protocols	= IR_TYPE_LIRC,
	.decode		= ir_lirc_decode,
	.raw_register	= ir_lirc_register,
	.raw_unregister	= ir_lirc_unregister,
};

static int __init ir_lirc_codec_init(void)
{
	ir_raw_handler_register(&lirc_handler);

	printk(KERN_INFO "IR LIRC bridge handler initialized\n");
	return 0;
}

static void __exit ir_lirc_codec_exit(void)
{
	ir_raw_handler_unregister(&lirc_handler);
}

module_init(ir_lirc_codec_init);
module_exit(ir_lirc_codec_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jarod Wilson <jarod@redhat.com>");
MODULE_AUTHOR("Red Hat Inc. (http://www.redhat.com)");
MODULE_DESCRIPTION("LIRC IR handler bridge");
