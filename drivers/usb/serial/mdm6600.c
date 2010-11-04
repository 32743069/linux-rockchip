/*
 * Copyright (C) 2010 Google, Inc.
 * Author: Nick Pelly <npelly@google.com>
 * Based on Motorola's mdm6600_modem driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

/*
 * TODO check if we need to implement throttling
 */

#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/platform_device.h>
#include <mach/gpio.h>

static bool debug = false;
static bool debug_data = false;

#define BP_MODEM_STATUS 0x20a1
#define BP_RSP_AVAIL 0x01a1
#define BP_SPEED_CHANGE 0x2aa1

#define BP_STATUS_CAR 0x01
#define BP_STATUS_DSR 0x02
#define BP_STATUS_BREAK 0x04
#define BP_STATUS_RNG 0x08

#define READ_POOL_SZ 8
#define WRITE_POOL_SZ 32

#define MODEM_INTERFACE_NUM 4

#define MODEM_WAKELOCK_TIME	msecs_to_jiffies(2000)
#define MODEM_AUTOSUSPEND_DELAY	msecs_to_jiffies(1000)

static const struct usb_device_id mdm6600_id_table[] = {
	{ USB_DEVICE(0x22b8, 0x2a70) },
	{ },
};
MODULE_DEVICE_TABLE(usb, mdm6600_id_table);

struct mdm6600_urb_write_pool {
	spinlock_t busy_lock;  /* protects busy flags */
	bool busy[WRITE_POOL_SZ];
	struct urb *urb[WRITE_POOL_SZ];
	struct usb_anchor in_flight;
	struct usb_anchor delayed;
	int buffer_sz;  /* allocated urb buffer size */
	int pending; /* number of in flight or delayed writes */
	spinlock_t pending_lock;
};

struct mdm6600_urb_read_pool {
	struct urb *urb[READ_POOL_SZ];
	struct usb_anchor in_flight;  /* urb's owned by USB core */
	struct work_struct work;  /* bottom half */
	struct usb_anchor pending;  /* urb's waiting for driver bottom half */
	int buffer_sz;  /* allocated urb buffer size */
};

struct mdm6600_port {
	struct usb_serial *serial;
	struct usb_serial_port *port;

	struct mdm6600_urb_write_pool write;
	struct mdm6600_urb_read_pool read;

	struct wake_lock readlock;
	char readlock_name[16];
	struct wake_lock writelock;
	char writelock_name[16];

	spinlock_t susp_lock;
	int susp_count;
	int opened;
	int number;
	u16 tiocm_status;
};

static int mdm6600_wake_irq;
/*
 * Count the number of attached ports. Don't use port->number as it may
 * changed if other ttyUSB have been registered before.
 */
static int mdm6600_attached_ports;
static int mdm6600_suspended_ports;

static void mdm6600_read_bulk_work(struct work_struct *work);
static void mdm6600_read_bulk_cb(struct urb *urb);
static void mdm6600_write_bulk_cb(struct urb *urb);

static irqreturn_t mdm6600_irq_handler(int irq, void *ptr)
{
	struct mdm6600_port *modem = ptr;

	wake_lock_timeout(&modem->readlock, MODEM_WAKELOCK_TIME);

	/* let usbcore auto-resume the modem */
	if (usb_autopm_get_interface_async(modem->serial->interface) == 0)
		/* set usage count back to 0 */
		usb_autopm_put_interface_no_suspend(modem->serial->interface);

	return IRQ_HANDLED;
}

/* called after probe for each of 5 usb_serial interfaces */
static int mdm6600_attach(struct usb_serial *serial)
{
	int i;
	int status;
	struct mdm6600_port *modem;
	struct usb_host_interface *host_iface =
		serial->interface->cur_altsetting;
	struct usb_endpoint_descriptor *epwrite = NULL;
	struct usb_endpoint_descriptor *epread = NULL;

	modem = kzalloc(sizeof(*modem), GFP_KERNEL);
	if (!modem)
		return -ENOMEM;
	usb_set_serial_data(serial, modem);

	modem->serial = serial;
	modem->port = modem->serial->port[0]; /* always 1 port per usb_serial */
	modem->tiocm_status = 0;
	modem->number = mdm6600_attached_ports++;

	/* find endpoints */
	for (i = 0; i < host_iface->desc.bNumEndpoints; i++) {
		struct usb_endpoint_descriptor *ep =
			&host_iface->endpoint[i].desc;
		if (usb_endpoint_is_bulk_out(ep))
			epwrite = ep;
		if (usb_endpoint_is_bulk_in(ep))
			epread = ep;
	}
	if (!epwrite) {
		pr_err("%s No bulk out endpoint\n", __func__);
		return -EIO;
	}
	if (!epread) {
		pr_err("%s No bulk in endpoint\n", __func__);
		return -EIO;
	}

	/* setup write pool */
	spin_lock_init(&modem->write.busy_lock);
	init_usb_anchor(&modem->write.in_flight);
	init_usb_anchor(&modem->write.delayed);
	modem->write.buffer_sz = le16_to_cpu(epwrite->wMaxPacketSize) * 4;
	for (i = 0; i < WRITE_POOL_SZ; i++) {
		struct urb *u = usb_alloc_urb(0, GFP_KERNEL);
		if (!u)
			return -ENOMEM;
		u->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
		u->transfer_buffer = usb_alloc_coherent(serial->dev,
			modem->write.buffer_sz, GFP_KERNEL, &u->transfer_dma);
		if (!u->transfer_buffer)
			return -ENOMEM;
		usb_fill_bulk_urb(u, serial->dev,
			usb_sndbulkpipe(serial->dev, epwrite->bEndpointAddress),
			u->transfer_buffer, modem->write.buffer_sz,
			mdm6600_write_bulk_cb, modem);
		modem->write.urb[i] = u;
		modem->write.busy[i] = false;
	}

	/* read pool */
	INIT_WORK(&modem->read.work, mdm6600_read_bulk_work);
	init_usb_anchor(&modem->read.in_flight);
	init_usb_anchor(&modem->read.pending);
	modem->read.buffer_sz = le16_to_cpu(epread->wMaxPacketSize) * 4;
	for (i = 0; i < READ_POOL_SZ; i++) {
		struct urb *u = usb_alloc_urb(0, GFP_KERNEL);
		if (!u)
			return -ENOMEM;
		u->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
		u->transfer_buffer = usb_alloc_coherent(serial->dev,
			modem->read.buffer_sz, GFP_KERNEL, &u->transfer_dma);
		if (!u->transfer_buffer)
			return -ENOMEM;
		usb_fill_bulk_urb(u, serial->dev,
			usb_rcvbulkpipe(serial->dev, epread->bEndpointAddress),
			u->transfer_buffer, modem->read.buffer_sz,
			mdm6600_read_bulk_cb, modem);
		modem->read.urb[i] = u;
	}

	spin_lock_init(&modem->susp_lock);
	spin_lock_init(&modem->write.pending_lock);

	snprintf(modem->readlock_name, sizeof(modem->readlock_name),
					"mdm6600_read.%d", modem->number);
	wake_lock_init(&modem->readlock, WAKE_LOCK_SUSPEND, modem->readlock_name);

	snprintf(modem->writelock_name, sizeof(modem->writelock_name),
					"mdm6600_write.%d", modem->number);
	wake_lock_init(&modem->writelock, WAKE_LOCK_SUSPEND, modem->writelock_name);

	usb_enable_autosuspend(serial->dev);
	usb_mark_last_busy(serial->dev);

	/* the modem triggers wakeup requests only if remote wakeup is enabled */
	device_init_wakeup(&serial->dev->dev, 1);
	serial->interface->needs_remote_wakeup = 1;
	serial->dev->autosuspend_delay = MODEM_AUTOSUSPEND_DELAY;
	serial->dev->parent->autosuspend_delay = 0;

	if (modem->number == MODEM_INTERFACE_NUM) {
		status = request_irq(mdm6600_wake_irq, mdm6600_irq_handler,
				IRQ_TYPE_EDGE_FALLING, "usb_wake_host", modem);
		if (status) {
			pr_err("request_irq failed; err=%d", status);
			return -ENXIO;
		}
		enable_irq_wake(mdm6600_wake_irq);
		disable_irq(mdm6600_wake_irq);
	}

	return 0;
}

static void mdm6600_kill_urbs(struct mdm6600_port *modem)
{
	dbg("%s: port %d", __func__, modem->number);

	/* cancel pending writes */
	usb_kill_anchored_urbs(&modem->write.in_flight);
	usb_scuttle_anchored_urbs(&modem->write.in_flight);

	/* stop reading from mdm6600 */
	usb_kill_anchored_urbs(&modem->read.in_flight);
	usb_scuttle_anchored_urbs(&modem->read.in_flight);

	usb_kill_urb(modem->port->interrupt_in_urb);

	if (!usb_wait_anchor_empty_timeout(&modem->read.pending, 1000))
		usb_scuttle_anchored_urbs(&modem->read.pending);
}

static void mdm6600_disconnect(struct usb_serial *serial)
{
	struct mdm6600_port *modem = usb_get_serial_data(serial);

	dbg("%s: port %d", __func__, modem->number);

	modem->opened = 0;

	if (modem->number == MODEM_INTERFACE_NUM) {
		disable_irq_wake(mdm6600_wake_irq);
		free_irq(mdm6600_wake_irq, modem);
	}

	mdm6600_kill_urbs(modem);

	/* cancel read bottom half */
	cancel_work_sync(&modem->read.work);

	modem->tiocm_status = 0;

	wake_lock_destroy(&modem->readlock);
	wake_lock_destroy(&modem->writelock);

	mdm6600_attached_ports--;
}

static void mdm6600_release_urb(struct urb *u, int sz)
{
	usb_free_coherent(u->dev, sz, u->transfer_buffer, u->transfer_dma);
	u->transfer_buffer = NULL;
	usb_free_urb(u);
}

static void mdm6600_release(struct usb_serial *serial)
{
	struct mdm6600_port *modem = usb_get_serial_data(serial);
	int i;

	for (i = 0; i < WRITE_POOL_SZ; i++) {
		mdm6600_release_urb(modem->write.urb[i],
			modem->write.buffer_sz);
		modem->write.urb[i] = NULL;
	}
	for (i = 0; i < READ_POOL_SZ; i++) {
		mdm6600_release_urb(modem->read.urb[i], modem->read.buffer_sz);
		modem->read.urb[i] = NULL;
	}
}

static int mdm6600_submit_urbs(struct mdm6600_port *modem)
{
	int i;
	int rc;

	dbg("%s: port %d", __func__, modem->number);

	if (modem->number == MODEM_INTERFACE_NUM) {
		WARN_ON_ONCE(!modem->port->interrupt_in_urb);
		rc = usb_submit_urb(modem->port->interrupt_in_urb, GFP_KERNEL);
		if (rc) {
			pr_err("%s: failed to submit interrupt urb, error %d\n",
			    __func__, rc);
			return rc;
		}
	}
	for (i = 0; i < READ_POOL_SZ; i++) {
		usb_anchor_urb(modem->read.urb[i], &modem->read.in_flight);
		rc = usb_submit_urb(modem->read.urb[i], GFP_KERNEL);
		if (rc) {
			usb_unanchor_urb(modem->read.urb[i]);
			pr_err("%s: failed to submit bulk read urb, error %d\n",
			    __func__, rc);
			return rc;
		}
	}

	return 0;
}

/* called when tty is opened */
static int mdm6600_open(struct tty_struct *tty, struct usb_serial_port *port)
{
	struct mdm6600_port *modem = usb_get_serial_data(port->serial);
	int status;

	dbg("%s: port %d", __func__, modem->number);

	WARN_ON_ONCE(modem->port != port);

	if (tty)
		tty->low_latency = 1;

	modem->tiocm_status = 0;

	modem->opened = 1;
	status = mdm6600_submit_urbs(modem);

	usb_autopm_put_interface(modem->serial->interface);
	return status;
}

static void mdm6600_close(struct usb_serial_port *port)
{
	struct mdm6600_port *modem = usb_get_serial_data(port->serial);

	dbg("%s: port %d", __func__, modem->number);

	usb_autopm_get_interface(modem->serial->interface);

	modem->opened = 0;
	mdm6600_kill_urbs(modem);

	/* cancel read bottom half */
	cancel_work_sync(&modem->read.work);

	modem->tiocm_status = 0;
}

static struct urb *mdm6600_get_unused_write_urb(
	struct mdm6600_urb_write_pool *p)
{
	int i;
	unsigned long flags;
	struct urb *u = NULL;

	spin_lock_irqsave(&p->busy_lock, flags);

	for (i = 0; i < WRITE_POOL_SZ; i++)
		if (!p->busy[i])
			break;
	if (i >= WRITE_POOL_SZ)
		goto out;

	u = p->urb[i];
	p->busy[i] = true;

out:
	spin_unlock_irqrestore(&p->busy_lock, flags);
	return u;
}

static int mdm6600_mark_write_urb_unused(struct mdm6600_urb_write_pool *p,
	struct urb *u)
{
	int i;
	unsigned long flags;
	int rc = -EINVAL;

	spin_lock_irqsave(&p->busy_lock, flags);

	for (i = 0; i < WRITE_POOL_SZ; i++)
		if (p->urb[i] == u)
			break;
	if (i >= WRITE_POOL_SZ)
		goto out;

	p->busy[i] = false;
	rc = 0;

out:
	spin_unlock_irqrestore(&p->busy_lock, flags);
	return rc;
}

static void mdm6600_write_bulk_cb(struct urb *u)
{
	int status;
	unsigned long flags;
	struct mdm6600_port *modem = u->context;

	dbg("%s: urb %p status %d", __func__, u, u->status);

	/* remove urb from in_flight list */
	usb_unanchor_urb(u);

	status = u->status;
	if (status)
		pr_warn("%s non-zero status %d\n", __func__, u->status);

	if (!status)
		usb_serial_port_softint(modem->port);

	if (mdm6600_mark_write_urb_unused(&modem->write, u))
		pr_warn("%s unknown urb %p\n", __func__, u);

	spin_lock_irqsave(&modem->write.pending_lock, flags);
	if (--modem->write.pending == 0) {
		usb_autopm_put_interface_async(modem->serial->interface);
		wake_unlock(&modem->writelock);
	}
	spin_unlock_irqrestore(&modem->write.pending_lock, flags);
}

static int mdm6600_write(struct tty_struct *tty, struct usb_serial_port *port,
			const unsigned char *buf, int count)
{
	int rc;
	struct urb *u;
	struct usb_serial *serial = port->serial;
	struct mdm6600_port *modem = usb_get_serial_data(port->serial);
	unsigned long flags;

	dbg("%s: port %d count %d pool %p", __func__, modem->number, count,
		&modem->write);

	if (!count || !serial->num_bulk_out)
		return 0;

	u = mdm6600_get_unused_write_urb(&modem->write);
	if (!u) {
		pr_info("%s: port %d all buffers busy!\n", __func__, modem->number);
		return 0;
	}

	count = min(count, modem->write.buffer_sz);
	memcpy(u->transfer_buffer, buf, count);
	u->transfer_buffer_length = count;
	usb_serial_debug_data(debug_data, &port->dev, __func__,
		u->transfer_buffer_length, u->transfer_buffer);

	spin_lock_irqsave(&modem->write.pending_lock, flags);
	if (modem->write.pending++ == 0) {
		wake_lock(&modem->writelock);
		usb_autopm_get_interface_async(modem->serial->interface);
	}
	spin_unlock_irqrestore(&modem->write.pending_lock, flags);

	spin_lock_irqsave(&modem->susp_lock, flags);
	if (modem->susp_count) {
		usb_anchor_urb(u, &modem->write.delayed);
		spin_unlock_irqrestore(&modem->susp_lock, flags);
		return count;
	}
	spin_unlock_irqrestore(&modem->susp_lock, flags);

	usb_anchor_urb(u, &modem->write.in_flight);
	rc = usb_submit_urb(u, GFP_KERNEL);
	if (rc < 0) {
		pr_err("%s: submit bulk urb failed %d\n", __func__, rc);
		usb_unanchor_urb(u);
		spin_lock_irqsave(&modem->write.pending_lock, flags);
		if (--modem->write.pending == 0) {
			usb_autopm_put_interface_async(serial->interface);
			wake_unlock(&modem->writelock);
		}
		spin_unlock_irqrestore(&modem->write.pending_lock, flags);
		return rc;
	}
	return count;
}

static int mdm6600_write_room(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct mdm6600_port *modem = usb_get_serial_data(port->serial);
	unsigned long flags;
	int room = 0;

	dbg("%s - port %d", __func__, modem->number);

	spin_lock_irqsave(&modem->write.pending_lock, flags);
	if (modem->write.pending != WRITE_POOL_SZ)
		room = modem->write.buffer_sz;
	spin_unlock_irqrestore(&modem->write.pending_lock, flags);

	dbg("%s - returns %d", __func__, room);
	return room;
}

static int mdm6600_chars_in_buffer(struct tty_struct *tty)
{
	struct usb_serial_port *port = tty->driver_data;
	struct mdm6600_port *modem = usb_get_serial_data(port->serial);
	unsigned long flags;
	int chars;

	dbg("%s - port %d", __func__, modem->number);

	spin_lock_irqsave(&modem->write.pending_lock, flags);
	chars = modem->write.pending * modem->write.buffer_sz;
	spin_unlock_irqrestore(&modem->write.pending_lock, flags);

	dbg("%s - returns %d", __func__, chars);
	return chars;
}

static int mdm6600_tiocmget(struct tty_struct *tty, struct file *file)
{
	struct usb_serial_port *port = tty->driver_data;
	struct mdm6600_port *modem = usb_get_serial_data(port->serial);

	dbg("%s: port %d modem_status %x\n", __func__, modem->number,
		modem->tiocm_status);

	return modem->tiocm_status;
}

static int mdm6600_dtr_control(struct usb_serial_port *port, int ctrl)
{
	struct usb_device *dev = port->serial->dev;
	struct usb_interface *iface = port->serial->interface;
	struct mdm6600_port *modem = usb_get_serial_data(port->serial);
	u8 request = 0x22;
	u8 request_type = USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT;
	int timeout = HZ * 5;
	int rc;

	rc = usb_autopm_get_interface(iface);
	if (rc < 0) {
		pr_err("%s %s autopm failed %d", dev_driver_string(&iface->dev),
			dev_name(&iface->dev), rc);
		return rc;
	}

	rc = usb_control_msg(dev, usb_sndctrlpipe(dev, 0), request,
		request_type, ctrl, modem->number, NULL, 0, timeout);
	usb_autopm_put_interface(iface);

	return rc;
}

static int mdm6600_tiocmset(struct tty_struct *tty, struct file *file,
			unsigned int set, unsigned int clear)
{
	struct usb_serial_port *port = tty->driver_data;
	struct mdm6600_port *modem = usb_get_serial_data(port->serial);

	dbg("%s: port %d set %x clear %x\n", __func__, modem->number, set,
		clear);

	if (modem->number != MODEM_INTERFACE_NUM)
		return 0;
	if (clear & TIOCM_DTR)
		return mdm6600_dtr_control(port, 0);
	if (set & TIOCM_DTR)
		return mdm6600_dtr_control(port, 1);
	return 0;
}

static void mdm6600_apply_bp_status(u8 bp_status, u16 *tiocm_status)
{
	if (bp_status & BP_STATUS_CAR)
		*tiocm_status |= TIOCM_CAR;
	else
		*tiocm_status &= ~TIOCM_CAR;
	if (bp_status & BP_STATUS_DSR)
		*tiocm_status |= TIOCM_DSR;
	else
		*tiocm_status &= ~TIOCM_DSR;
	if (bp_status & BP_STATUS_RNG)
		*tiocm_status |= TIOCM_RNG;
	else
		*tiocm_status &= ~TIOCM_RNG;
}

static void mdm6600_read_int_callback(struct urb *u)
{
	int rc;
	u16 request;
	u8 *data = u->transfer_buffer;
	struct usb_serial_port *port = u->context;
	struct mdm6600_port *modem = usb_get_serial_data(port->serial);

	dbg("%s: urb %p", __func__, u);

	switch (u->status) {
	case 0:
		/* success */
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		dbg("%s: urb terminated, status %d", __func__, u->status);
		return;
	default:
		pr_warn("%s non-zero status %d\n", __func__, u->status);
		goto exit;
	}

	usb_mark_last_busy(port->serial->dev);
	usb_serial_debug_data(debug_data, &port->dev, __func__,
		u->actual_length, data);

	if (u->actual_length < 2) {
		dbg("%s: interrupt transfer too small %d",
			__func__, u->actual_length);
		goto exit;
	}
	request = *((u16 *)data);

	switch (request) {
	case BP_MODEM_STATUS:
		if (u->actual_length < 9) {
			pr_err("%s: modem status urb too small %d\n",
				__func__, u->actual_length);
			break;
		}
		if (modem->number != MODEM_INTERFACE_NUM)
			break;
		mdm6600_apply_bp_status(data[8], &modem->tiocm_status);
		dbg("%s: modem_status now %x", __func__, modem->tiocm_status);
		break;
	case BP_RSP_AVAIL:
		dbg("%s: BP_RSP_AVAIL", __func__);
		break;
	case BP_SPEED_CHANGE:
		dbg("%s: BP_SPEED_CHANGE", __func__);
		break;
	default:
		dbg("%s: undefined BP request type %d", __func__, request);
		break;
	}

exit:
	rc = usb_submit_urb(u, GFP_ATOMIC);
	if (rc)
		pr_err("%s: Error %d re-submitting interrupt urb\n",
			__func__, rc);
}

static size_t mdm6600_pass_to_tty(struct tty_struct *tty, void *buf, size_t sz)
{
	unsigned char *b = buf;
	size_t c;
	size_t s = sz;

	tty_buffer_request_room(tty, sz);
	while (s > 0) {
		c = tty_insert_flip_string(tty, b, s);
		if (c != s)
			dbg("%s passed only %u of %u bytes\n",
				__func__, c, s);
		if (c == 0)
			break;
		tty_flip_buffer_push(tty);
		s -= c;
		b += c;
	}

	return sz - s;
}

static void mdm6600_read_bulk_work(struct work_struct *work)
{
	int rc;
	size_t c;
	struct urb *u;
	struct tty_struct *tty;
	unsigned long flags;
	struct mdm6600_port *modem = container_of(work, struct mdm6600_port,
		read.work);
	struct usb_anchor *anchor = &modem->read.pending;

	dbg("%s", __func__);

	while (true) {
		spin_lock_irqsave(&anchor->lock, flags);
		if (list_empty(&anchor->urb_list)) {
			spin_unlock_irqrestore(&anchor->lock, flags);
			return;
		}

		u = list_entry(anchor->urb_list.next, struct urb,
				    anchor_list);
		usb_get_urb(u);
		spin_unlock_irqrestore(&anchor->lock, flags);

		dbg("%s: processing urb %p len %u", __func__, u,
			u->actual_length);
		usb_serial_debug_data(debug_data, &modem->port->dev, __func__,
			u->actual_length, u->transfer_buffer);
		tty = tty_port_tty_get(&modem->port->port);
		if (!tty) {
			pr_warn("%s: could not find tty\n", __func__);
			goto next;
		}
		c = mdm6600_pass_to_tty(tty, u->transfer_buffer,
			u->actual_length);
		if (c != u->actual_length)
			pr_warn("%s: port %d: dropped %u of %u bytes\n",
				__func__, modem->number, u->actual_length - c,
				u->actual_length);
		tty_kref_put(tty);

next:
		usb_unanchor_urb(u);
		spin_lock_irqsave(&modem->susp_lock, flags);
		if (modem->susp_count || !modem->opened) {
			spin_unlock_irqrestore(&modem->susp_lock, flags);
			usb_put_urb(u);
			continue;
		}
		spin_unlock_irqrestore(&modem->susp_lock, flags);

		usb_anchor_urb(u, &modem->read.in_flight);
		usb_put_urb(u);
		rc = usb_submit_urb(u, GFP_KERNEL);
		if (rc) {
			pr_err("%s: Error %d re-submitting read urb %p\n",
				__func__, rc, u);
			usb_unanchor_urb(u);
		}
	}
}

static void mdm6600_read_bulk_cb(struct urb *u)
{
	int rc;
	struct mdm6600_port *modem = u->context;

	dbg("%s: urb %p", __func__, u);

	switch (u->status) {
	case 0:
		break;  /* success */
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		dbg("%s: urb terminated, status %d", __func__, u->status);
		return;
	default:
		pr_warn("%s non-zero status %d\n", __func__, u->status);
		/* straight back into use */
		rc = usb_submit_urb(u, GFP_ATOMIC);
		if (rc)
			pr_err("%s: Error %d re-submitting read urb\n",
				__func__, rc);
		return;
	}

	wake_lock_timeout(&modem->readlock, MODEM_WAKELOCK_TIME);
	usb_mark_last_busy(modem->serial->dev);

	/* remove urb from in_flight list */
	usb_unanchor_urb(u);

	/* process urb in bottom half */
	usb_anchor_urb(u, &modem->read.pending);
	schedule_work(&modem->read.work);
}

static int mdm6600_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct usb_serial *serial = usb_get_intfdata(intf);
	struct mdm6600_port *modem = usb_get_serial_data(serial);

	dbg("%s: event=%d", __func__, message.event);

	spin_lock_irq(&modem->susp_lock);

	if (!modem->susp_count++ && modem->opened) {
		if (!mdm6600_suspended_ports++)
			enable_irq(mdm6600_wake_irq);

		spin_unlock_irq(&modem->susp_lock);
		dbg("%s: kill urbs", __func__);
		mdm6600_kill_urbs(modem);
		return 0;
	}

	spin_unlock_irq(&modem->susp_lock);
	return 0;
}

static int mdm6600_resume(struct usb_interface *intf)
{
	struct usb_serial *serial = usb_get_intfdata(intf);
	struct mdm6600_port *modem = usb_get_serial_data(serial);
	struct urb *u;
	int rc;

	dbg("%s", __func__);

	spin_lock_irq(&modem->susp_lock);

	if (!--modem->susp_count && modem->opened) {
		if (!--mdm6600_suspended_ports)
			disable_irq(mdm6600_wake_irq);

		dbg("%s: submit urbs", __func__);
		spin_unlock_irq(&modem->susp_lock);

		mdm6600_submit_urbs(modem);

		while ((u = usb_get_from_anchor(&modem->write.delayed))) {
			usb_anchor_urb(u, &modem->write.in_flight);
			usb_put_urb(u);
			rc = usb_submit_urb(u, GFP_KERNEL);
			if (rc < 0) {
				usb_unanchor_urb(u);
				usb_scuttle_anchored_urbs(&modem->write.delayed);
				pr_err("%s: submit bulk urb failed %d\n", __func__, rc);
				return rc;
			}
		}

		return 0;
	}

	spin_unlock_irq(&modem->susp_lock);
	return 0;
}

static int mdm6600_reset_resume(struct usb_interface *intf)
{
	dbg("%s", __func__);

	return mdm6600_resume(intf);
}

static int mdm6600_probe(struct usb_serial *serial, const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(serial->interface);

	/* we only support 1 modem */
	if (mdm6600_attached_ports >= dev->config->desc.bNumInterfaces) {
		pr_err("%s: only one modem supported", __func__);
		return -EBUSY;
	}

	return 0;
}

static struct usb_driver mdm6600_usb_driver = {
	.name =		"mdm6600",
	.probe =	usb_serial_probe,
	.disconnect =	usb_serial_disconnect,
	.id_table =	mdm6600_id_table,
	.no_dynamic_id = 	1,
	.supports_autosuspend = 1,
	.suspend =	mdm6600_suspend,
	.resume =	mdm6600_resume,
	.reset_resume =	mdm6600_reset_resume,
};

static struct usb_serial_driver mdm6600_usb_serial_driver = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"mdm6600",
	},
	.num_ports =		1,
	.description =		"MDM 6600 modem usb-serial driver",
	.id_table =		mdm6600_id_table,
	.usb_driver =		&mdm6600_usb_driver,
	.probe =		mdm6600_probe,
	.attach =		mdm6600_attach,
	.disconnect =		mdm6600_disconnect,
	.release =		mdm6600_release,
	.open =			mdm6600_open,
	.close =		mdm6600_close,
	.write =		mdm6600_write,
	.write_room =		mdm6600_write_room,
	.chars_in_buffer =      mdm6600_chars_in_buffer,
	.tiocmset =		mdm6600_tiocmset,
	.tiocmget =		mdm6600_tiocmget,
	.read_int_callback =	mdm6600_read_int_callback,
};


static int mdm6600_modem_probe(struct platform_device *pdev)
{
	int retval;

	mdm6600_wake_irq = platform_get_irq(pdev, 0);
	if (!mdm6600_wake_irq) {
		dev_err(&pdev->dev, "Failed to get IRQ\n");
		return -ENODEV;
	}

	retval = usb_serial_register(&mdm6600_usb_serial_driver);
	if (retval)
		return retval;
	retval = usb_register(&mdm6600_usb_driver);
	if (retval)
		usb_serial_deregister(&mdm6600_usb_serial_driver);

	return retval;
}

static int __exit mdm6600_modem_remove(struct platform_device *pdev)
{
	usb_deregister(&mdm6600_usb_driver);
	usb_serial_deregister(&mdm6600_usb_serial_driver);
	return 0;
}

static struct platform_driver mdm6600_modem_driver = {
	.driver = {
		.name  = "mdm6600_modem",
	},
	.remove  = __exit_p(mdm6600_modem_remove),
	.probe   = mdm6600_modem_probe,
};

static int __init mdm6600_init(void)
{
	return platform_driver_register(&mdm6600_modem_driver);
}

static void __exit mdm6600_exit(void)
{
	platform_driver_unregister(&mdm6600_modem_driver);
}

module_init(mdm6600_init);
module_exit(mdm6600_exit);
MODULE_LICENSE("GPL");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug enabled or not");
module_param(debug_data, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug_data, "Debug enabled or not");
