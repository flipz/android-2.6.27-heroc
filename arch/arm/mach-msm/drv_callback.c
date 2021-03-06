/* linux/arch/arm/mach-msm/drv_callback.c
 *
 * Copyright (C) 2009 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/list.h>
#include <linux/rwsem.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <mach/drv_callback.h>

static DECLARE_RWSEM(cnf_driver_list_lock);
static LIST_HEAD(cnf_driver_list);

int cnf_driver_register(struct cnf_driver *driver)
{
	if (driver) {
		rwlock_init(&driver->cnfdrv_list_lock);

		/* TODO: check if there is any driver already registered
		 * with same name */

		down_write(&cnf_driver_list_lock);
		list_add_tail(&driver->next_drv, &cnf_driver_list);
		up_write(&cnf_driver_list_lock);

		return 0;
	} else {
		printk(KERN_WARNING "Configurable driver %s failed to "
				"register (NULL driver)\n", driver->name);
		return -1;
	}
}

int cnf_driver_event(const char *name, void *argu)
{
	struct list_head *listptr;
	int ret = -EINVAL;

	down_read(&cnf_driver_list_lock);
	list_for_each(listptr, &cnf_driver_list) {
		struct cnf_driver *driver;

		driver = list_entry(listptr, struct cnf_driver, next_drv);
		if (strcmp(driver->name, name) == 0) {
			ret = driver->func(argu);
			break;
		}
	}
	up_read(&cnf_driver_list_lock);

	return ret;
}

int cnf_driver_unregister(const char *name)
{
	struct list_head *listptr;
	int matching = 0;

	if (list_empty(&cnf_driver_list)) {
		printk(KERN_WARNING "No registered driver\n");
		return 0;
	}

	down_write(&cnf_driver_list_lock);
	list_for_each(listptr, &cnf_driver_list) {
		struct cnf_driver *driver;

		driver = list_entry(listptr, struct cnf_driver, next_drv);
		if (strcmp(driver->name, name) == 0) {
			list_del(&driver->next_drv);
			kfree(driver);
			matching = 1;
			break;
		}
	}
	up_write(&cnf_driver_list_lock);

	if (!matching) {
		printk(KERN_WARNING "Configurable driver %s failed to "
			"unregister (No matching driver registered)\n", name);
		return -1;
	}

	return 0;
}

int cnf_driver_show()
{
	struct list_head *listptr;

	printk(KERN_INFO "Driver registerd:\n");

	if (list_empty(&cnf_driver_list)) {
		printk(KERN_WARNING "  No driver\n");
		return 0;
	}

	down_read(&cnf_driver_list_lock);
	list_for_each(listptr, &cnf_driver_list) {
		struct cnf_driver *driver;

		driver = list_entry(listptr, struct cnf_driver, next_drv);
		printk(KERN_INFO "  %s \n", driver->name);
	}
	up_read(&cnf_driver_list_lock);

	return 0;
}
