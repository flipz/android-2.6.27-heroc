#ifndef _PROXIMITY_SENSOR_H
#define _PROXIMITY_SENSOR_H
#include<linux/earlysuspend.h>

#define PROXIMITY_SENSOR_NAME "proximity_sensor"

struct proximity_platform_data {
	struct input_dev *input_dev;
	struct work_struct work;
	struct early_suspend early_suspend_proximity;
	int enable;
	int intr;
	int auto_proximity;
};


#endif
