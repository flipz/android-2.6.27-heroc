/*
 * Copyright (C) 2007-2008 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>  
#include <linux/freezer.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <net/sock.h>
#include <asm/gpio.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <mach/msm_iomap.h>
#include <mach/msm_rpcrouter.h>
#include <mach/vreg.h>
#include <mach/board.h>
#include <mach/s5k3e2fx.h> /* define ioctls */
#include <mach/perflock.h>
#include <linux/earlysuspend.h>

/***********************************************************
* Global variable
************************************************************/
static uint16_t chipid;
static struct i2c_client *pclient;
/* we need this to set the clock rate */
static int opened;
static int pclk_set;
static struct perf_lock camera_perf_lock;
static struct wake_lock s5k3e2fx_wake_lock;
static struct msm_camera_device_platform_data  *s5k3e2fx_sensor;

/***********************************************************
* definition
************************************************************/

#define ALLOW_USPACE_RW		0
#define S5K3E2FX_SS5M0_REG_MODEL_ID   0x0000
#define S5K3E2FX_SS5M0_MODEL_ID       0x3E2F
#define s5k3e2fx_i2c_write s5k3e2fx_sensor->sensor_info->sensor_i2c_write
#define s5k3e2fx_i2c_read s5k3e2fx_sensor->sensor_info->sensor_i2c_read



DECLARE_MUTEX(sem_s5k3e2fx);

struct s5k3e2fx_data {
	struct work_struct work;
};

static DECLARE_WAIT_QUEUE_HEAD(g_data_ready_wait_queue);
static int s5k3e2fx_i2c_sensor_init(struct s5k3e2fx_init *init);
static int s5k3e2fx_i2c_move_focus(uint16_t position);
static int s5k3e2fx_i2c_set_default_focus(uint8_t step);
static int s5k3e2fx_lens_power(int on);

/***********************************************************
* private function
************************************************************/
static inline void init_suspend(void)
{
	wake_lock_init(&s5k3e2fx_wake_lock, WAKE_LOCK_IDLE, "s5k3e2fx");
}

static inline void deinit_suspend(void)
{
	wake_lock_destroy(&s5k3e2fx_wake_lock);
}

static inline void prevent_suspend(void)
{
	wake_lock(&s5k3e2fx_wake_lock);
}

static inline void allow_suspend(void)
{
	wake_unlock(&s5k3e2fx_wake_lock);
}

int s5k3e2fx_i2c_lens_tx_data(unsigned char slave_addr, char* txData, int length)
{
	int rc;
	struct i2c_msg msg[] = {
		{
			.addr = slave_addr,
			.flags = 0,
			.len = length,
			.buf = txData,		
		},
	};    
	rc = i2c_transfer(pclient->adapter, msg, 1);
	if (rc < 0) {
		printk(KERN_ERR "s5k3e2fx_i2c_lens_tx_data: i2c_transfer error %d\n", rc);
		return rc;
	}
	return 0;
}

/* Remove unused function */
static int s5k3e2fx_i2c_lens_write(unsigned char slave_addr, unsigned char u_addr, unsigned char u_data)
{
	unsigned char buf[2] = { u_addr, u_data };
	return s5k3e2fx_i2c_lens_tx_data(slave_addr, buf, sizeof(buf));
}



static int s5k3e2fx_open(struct inode *ip, struct file *fp)
{
	int rc = -EBUSY;
	int timeout;
	timeout = wait_event_interruptible_timeout(
		s5k3e2fx_sensor->event_wait,
		s5k3e2fx_sensor->cond,
		15*HZ);
	printk("wait event : %d timeout:%d\n",
		s5k3e2fx_sensor->cond,
		timeout);
	if(timeout == 0)
		return rc;	
	down(&sem_s5k3e2fx);
	printk(KERN_INFO "s5k3e2fx: open\n");
	if (!opened) {
		printk(KERN_INFO "s5k3e2fx: prevent collapse on idle\n");
		prevent_suspend();
		perf_lock(&camera_perf_lock);
		opened = 1;
		rc = 0;
	}
	up(&sem_s5k3e2fx);
	return rc;
}

static int s5k3e2fx_release(struct inode *ip, struct file *fp)
{
	int rc = -EBADF;
	printk(KERN_INFO "s5k3e2fx: release\n");
	down(&sem_s5k3e2fx);
	if (opened) {
		printk(KERN_INFO "s5k3e2fx: release clocks\n");
		/* sensor_power_down() should be called before closing MCLK */
		/* otherwise I2C_WRITE will always fail */
		s5k3e2fx_sensor->sensor_info->sensor_power_down();
		s5k3e2fx_sensor->sensor_info->msmclk_disable(CAMIO_VFE_MDC_CLK);
		s5k3e2fx_sensor->sensor_info->msmclk_disable(CAMIO_MDC_CLK);
		s5k3e2fx_sensor->sensor_info->msmclk_disable(CAMIO_VFE_CLK);
		s5k3e2fx_sensor->config_gpio_off();
		s5k3e2fx_lens_power(1);/*For 5M*/
		perf_unlock(&camera_perf_lock);
		allow_suspend();
		rc = pclk_set = opened = 0;
	}
	up(&sem_s5k3e2fx);
	return rc;
}

#if ALLOW_USPACE_RW
#define COPY_FROM_USER(size) ({                                         \
        if (copy_from_user(rwbuf, argp, size)) rc = -EFAULT;            \
        !rc; })
#endif

/*Becker :read fuse id for AWB auto Calibration*/
#define MAX_LAYER_NUM 5
#define FIRST_LAYER 9
#define FUSE_ID_FIRST_ADDR 14
static int s5k3e2fx_i2c_read_fuseid(struct fuse_id *fuse){

	uint32_t otp_vendorid_index=0;
	uint32_t otp_fuseid_index=0;
	unsigned short otp_vendorid1[MAX_LAYER_NUM];
	unsigned short otp_vendorid2[MAX_LAYER_NUM];
	unsigned short otp_vendorid3[MAX_LAYER_NUM];
	unsigned short otp_fuseid1[MAX_LAYER_NUM];
	unsigned short otp_fuseid2[MAX_LAYER_NUM];
	unsigned short otp_fuseid3[MAX_LAYER_NUM];
	for(otp_vendorid_index=0;otp_vendorid_index<MAX_LAYER_NUM;otp_vendorid_index++)
	{
		s5k3e2fx_i2c_write(0x311A, FIRST_LAYER+otp_vendorid_index); //give OTP the address you want to read
		s5k3e2fx_i2c_read(0x311B, &otp_vendorid1[otp_vendorid_index]);
		s5k3e2fx_i2c_read(0x311C, &otp_vendorid2[otp_vendorid_index]);
		s5k3e2fx_i2c_read(0x311D, &otp_vendorid3[otp_vendorid_index]);
       		otp_vendorid1[otp_vendorid_index]=(otp_vendorid1[otp_vendorid_index]>>8 ) & 0xff;
       		otp_vendorid2[otp_vendorid_index]=(otp_vendorid2[otp_vendorid_index]>>8 ) & 0xff;
		otp_vendorid3[otp_vendorid_index]=(otp_vendorid3[otp_vendorid_index]>>8 ) & 0xff;   
		printk("s5k3e2fx: otp_vendorid1[%d]:0x%4x\n",
			otp_vendorid_index,otp_vendorid1[otp_vendorid_index]);
		printk("s5k3e2fx: otp_vendorid2[%d]:0x%4x\n",
			otp_vendorid_index,otp_vendorid2[otp_vendorid_index]);
		printk("s5k3e2fx: otp_vendorid3[%d]:0x%4x\n",
			otp_vendorid_index,otp_vendorid3[otp_vendorid_index]);
		if((otp_vendorid1[otp_vendorid_index]==0)&&
			(otp_vendorid2[otp_vendorid_index]==0)&&
			(otp_vendorid3[otp_vendorid_index]==0)&&
			(otp_vendorid_index!=0)) 
		{
	   		break;
		}
	}
	otp_vendorid_index=otp_vendorid_index-1;
	/*read fuse id from layer14~layer18. The last non-all-zero layer contains correct fuse id */
	for(otp_fuseid_index=0;otp_fuseid_index<MAX_LAYER_NUM;otp_fuseid_index++) {
		/*give OTP the address you want to read*/
		s5k3e2fx_i2c_write(0x311A, FUSE_ID_FIRST_ADDR+otp_fuseid_index); 
		s5k3e2fx_i2c_read(0x311B, &otp_fuseid1[otp_fuseid_index]);
		s5k3e2fx_i2c_read(0x311C, &otp_fuseid2[otp_fuseid_index]);
		s5k3e2fx_i2c_read(0x311D, &otp_fuseid3[otp_fuseid_index]);
		otp_fuseid1[otp_fuseid_index]=(otp_fuseid1[otp_fuseid_index]>>8 ) & 0xff;
		otp_fuseid2[otp_fuseid_index]=(otp_fuseid2[otp_fuseid_index]>>8 ) & 0xff;
		otp_fuseid3[otp_fuseid_index]=(otp_fuseid3[otp_fuseid_index]>>8 ) & 0xff;
		printk("s5k3e2fx: otp_fuseid1[%d]:0x%4x\n",
			otp_fuseid_index,otp_fuseid1[otp_fuseid_index]);
		printk("s5k3e2fx: otp_fuseid2[%d]:0x%4x\n",
			otp_fuseid_index,otp_fuseid2[otp_fuseid_index]);
		printk("s5k3e2fx: otp_fuseid3[%d]:0x%4x\n",
			otp_fuseid_index,otp_fuseid3[otp_fuseid_index]);
		if((otp_fuseid1[otp_fuseid_index]==0)&&
			(otp_fuseid2[otp_fuseid_index]==0)&&
			(otp_fuseid3[otp_fuseid_index]==0)&&
			(otp_fuseid_index!=0)) {
           		break;
       		}		
	}
	otp_fuseid_index=otp_fuseid_index-1;
	fuse->fuse_id_word1 = otp_vendorid_index;
	fuse->fuse_id_word2 = otp_fuseid_index;
	fuse->fuse_id_word3 = 
		(((uint32_t)otp_vendorid1[otp_vendorid_index])<<16) | 
		(((uint32_t)otp_vendorid2[otp_vendorid_index])<<8) | 
		((uint32_t)otp_vendorid3[otp_vendorid_index]);
	fuse->fuse_id_word4 = 
		(((uint32_t)otp_fuseid1[otp_fuseid_index])<<16) | 
		(((uint32_t)otp_fuseid2[otp_fuseid_index])<<8) | 
		((uint32_t)otp_fuseid3[otp_fuseid_index]);
	printk("s5k3e2fx: fuse->fuse_id_word1:%d\n",fuse->fuse_id_word1);
	printk("s5k3e2fx: fuse->fuse_id_word2:%d\n",fuse->fuse_id_word2);
	printk("s5k3e2fx: fuse->fuse_id_word3:0x%08x\n",fuse->fuse_id_word3);
	printk("s5k3e2fx: fuse->fuse_id_word4:0x%08x\n",fuse->fuse_id_word4);
	return 0;
}


static long s5k3e2fx_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){

	void __user *argp = (void __user *)arg;
	int rc = 0;
	int ret;
#if ALLOW_USPACE_RW
	unsigned short addr = 0;
	unsigned short data = 0;
	char rwbuf[4];
#endif
	down(&sem_s5k3e2fx);
	switch(cmd) {
#if ALLOW_USPACE_RW
	case S5K3E2FX_I2C_IOCTL_W:
		if (COPY_FROM_USER(4)) {
			addr = *((unsigned short *)rwbuf);
			data = *((unsigned short *)(rwbuf+2));
			rc = s5k3e2fx_i2c_write_w(addr, data);
		} else
			printk(KERN_ERR "s5k3e2fx: write: err %d\n", rc);
		break;

	case S5K3E2FX_I2C_IOCTL_R:
		if (COPY_FROM_USER(4)) {
			addr = *((unsigned short*) rwbuf);
			rc = s5k3e2fx_i2c_read(addr, (unsigned short *)(rwbuf+2));
			if (!rc) {
				if (copy_to_user(argp, rwbuf, 4)) {
					printk(KERN_ERR "s5k3e2fx: read: err " \
							"writeback -EFAULT\n");
					rc = -EFAULT;
				}
			}
		} else
			printk(KERN_ERR "s5k3e2fx: read: err %d\n", rc);
		break;

	case S5K3E2FX_I2C_IOCTL_AF_W:
		if (COPY_FROM_USER(3))
			rc = s5k3e2fx_i2c_lens_write(*rwbuf, *(rwbuf + 1), *(rwbuf + 2));
		else
			printk(KERN_ERR "s5k3e2fx: af write: err %d\n", rc);
		break;
#endif /* ALLOW_USPACE_RW */

	case S5K3E2FX_I2C_IOCTL_CAMIF_PAD_REG_RESET:
		printk(KERN_INFO "s5k3e2fx: CAMIF_PAD_REG_RESET\n"); 
		s5k3e2fx_sensor->sensor_info->msmio_camif_pad_reg_reset();
		break;

	case S5K3E2FX_I2C_IOCTL_CAMIF_PAD_REG_RESET_2:
		printk(KERN_INFO "s5k3e2fx: CAMIF_PAD_REG_RESET_2 (pclk_set %d)\n",
				pclk_set);
		if (!pclk_set)
			rc = -EIO;
		else 
			s5k3e2fx_sensor->sensor_info->msmio_camif_Reset2();
		break;

	case S5K3E2FX_I2C_IOCTL_CAMIF_APPS_RESET:
		printk(KERN_INFO "s5k3e2fx: CAMIF_APPS_RESET\n"); 
		s5k3e2fx_sensor->sensor_info->msmio_camif_app_reset();
		break;

	case CAMERA_LENS_POWER_ON:
		rc = s5k3e2fx_lens_power(0);/*For 5M*/
		break;

	case CAMERA_LENS_POWER_OFF:
		rc = s5k3e2fx_lens_power(1);/*For 5M*/
		break;

	case S5K3E2FX_I2C_IOCTL_CLK_ENABLE:
		printk(KERN_INFO "s5k3e2fx: clk enable %ld\n", arg);
		s5k3e2fx_sensor->sensor_info->msmclk_enable(arg);
		break;

	case S5K3E2FX_I2C_IOCTL_CLK_DISABLE:
		printk(KERN_INFO "s5k3e2fx: clk disable %ld\n", arg);
		s5k3e2fx_sensor->sensor_info->msmclk_disable(arg);
		break;

	case S5K3E2FX_I2C_IOCTL_CLK_SELECT:
		printk(KERN_INFO "s5k3e2fx: clk select %ld\n", arg);
		s5k3e2fx_sensor->sensor_info->msmclk_camif_clk_select(!!arg);
		break;

	case S5K3E2FX_I2C_IOCTL_CLK_FREQ_PROG:
		printk(KERN_INFO "s5k3e2fx: clk rate select %ld\n", arg);
		s5k3e2fx_sensor->sensor_info->msmclk_rate_set(arg);
		break;

	case S5K3E2FX_I2C_IOCTL_GET_REGISTERS:
		printk(KERN_INFO "s5k3e2fx: get registers\n");
		#if 0
		if (copy_to_user(argp, &s5k3e2fx_reg_pattern.reg, sizeof(s5k3e2fx_reg_pattern.reg)))
			rc = -EFAULT;
		#endif
		break;

	case S5K3E2FX_I2C_IOCTL_SENSOR_SETTING:
		printk(KERN_INFO "s5k3e2fx: sensor setting 0x%lx\n", arg);
		/*becker take off for temp*/
		rc = s5k3e2fx_sensor->sensor_info->sensor_setting(arg);
		break;

	case S5K3E2FX_I2C_IOCTL_EXPOSURE_GAIN: {
		struct s5k3e2fx_exposure_gain exp;
		if (copy_from_user(&exp, argp, sizeof(exp))) {
			printk(KERN_ERR "s5k3e2fx: (exposure gain) invalid user pointer\n");
			rc = -EFAULT;
			break;
		}
		rc = s5k3e2fx_sensor->sensor_info->sensor_write_exposuregain
			(exp.mode, exp.line, exp.gain, exp.linelengthpck, exp.framelengthlines);
		}
		break;

	case S5K3E2FX_I2C_IOCTL_MOVE_FOCUS:
		rc = s5k3e2fx_i2c_move_focus((uint16_t)arg);
		break;

	case S5K3E2FX_I2C_IOCTL_SET_DEFAULT_FOCUS:
		printk(KERN_INFO "s5k3e2fx: set default focus %ld\n", arg);
		rc = s5k3e2fx_i2c_set_default_focus((uint8_t)arg);
		break;

	case S5K3E2FX_I2C_IOCTL_POWER_DOWN:
		s5k3e2fx_sensor->sensor_info->sensor_power_down();
		pclk_set = 0;
		break;

	case S5K3E2FX_I2C_IOCTL_INIT: {
		struct s5k3e2fx_init init;
		printk(KERN_INFO "s5k3e2fx: init\n");
		if (copy_from_user(&init, argp, sizeof(init))) {
			printk(KERN_ERR "s5k3e2fx: (init) invalid user pointer\n");
			rc = -EFAULT;
			break;
		}
		rc = s5k3e2fx_i2c_sensor_init(&init);
		if (copy_to_user(argp, &init, sizeof(init)))
			rc = -EFAULT;
		}
		break;

	case CAMERA_CONFIGURE_GPIOS:
	case CAMERA_UNCONFIGURE_GPIOS: 
		break;
	/*Becker for AWB Calibration*/
	case S5K3E2FX_I2C_IOCTL_R_OTP:{
		struct fuse_id fuse;
		if(copy_from_user(&fuse, argp, sizeof(struct fuse_id))){
			printk(KERN_ERR "s5k3e2fx: (init) invalid user pointer\n");
			rc = -EFAULT;
			break;
		}
		rc = s5k3e2fx_i2c_read_fuseid(&fuse);
		if (copy_to_user(argp, &fuse, sizeof(struct fuse_id)))
			rc = -EFAULT;
		}
		break;
	case CAMERA_READ_STREAM:{
		struct streaming i2c_data;
		if(copy_from_user(&i2c_data, argp, sizeof(struct streaming))){
			printk(KERN_ERR
				"s5k3e2fx: (init) invalid user pointer\n");
			rc = -EFAULT;
			break;
		}
		/*read sensor status*/
		s5k3e2fx_i2c_read(0x0100, &i2c_data.stream);
		s5k3e2fx_i2c_read(0x3150, &i2c_data.hi_z);
		s5k3e2fx_i2c_read(0x0300, &i2c_data.vt_pix_clk_div);
		s5k3e2fx_i2c_read(0x0302, &i2c_data.vt_sys_clk_div);
		s5k3e2fx_i2c_read(0x0304, &i2c_data.pre_pll_clk_div);
		s5k3e2fx_i2c_read(0x0306, &i2c_data.pll_multiplexer);
		s5k3e2fx_i2c_read(0x0308, &i2c_data.op_pix_clk_div);
		s5k3e2fx_i2c_read(0x030a, &i2c_data.op_sys_clk_div);
		printk(KERN_INFO "streaming 0x0100 (0x%x)\n", i2c_data.stream);
		printk(KERN_INFO "hi_z 0x3150 (0x%x)\n", i2c_data.hi_z);
		printk(KERN_INFO "vt_pix_clk_div 0x0300 (0x%x)\n", i2c_data.vt_pix_clk_div);
		printk(KERN_INFO "vt_sys_clk_div 0x0302 (0x%x)\n", i2c_data.vt_sys_clk_div);
		printk(KERN_INFO "pre_pll_clk_div 0x0304 (0x%x)\n", i2c_data.pre_pll_clk_div);
		printk(KERN_INFO "multiplexer 0x0306 (0x%x)\n", i2c_data.pll_multiplexer);
		printk(KERN_INFO "op_pix_clk_div 0x0308 (0x%x)\n", i2c_data.op_pix_clk_div);
		printk(KERN_INFO "op_sys_clk_div 0x030a (0x%x)\n", i2c_data.op_sys_clk_div);
		
		if (copy_to_user(argp, &i2c_data, sizeof(struct streaming)))
			rc = -EFAULT;
		}
		break;
	default:
		printk(KERN_INFO "s5k3e2fx: unknown ioctl %d\n", cmd);
		break;
	}
	up(&sem_s5k3e2fx);
	return rc;
}

static int s5k3e2fx_lens_power(int on)
{
	int rc = 0;
	printk(KERN_INFO "s5k3e2fx: lens power %d\n", on);
	#if 0
	rc = gpio_request(s5k3e2fx_sensor->vcm_pwd, "s5k3e2fx");
	if (!rc)
		gpio_direction_output(s5k3e2fx_sensor->vcm_pwd, !on);
	else printk(KERN_ERR "s5k3e2fx error: request gpio %d failed:"
		" %d\n", s5k3e2fx_sensor->vcm_pwd, rc);
	gpio_free(s5k3e2fx_sensor->vcm_pwd);
	#endif
	return rc;
}

static int s5k3e2fx_i2c_sensor_init(struct s5k3e2fx_init *init)
{
	int rc;
	chipid = 0;
	printk("-----------------------s5k3e2fx_i2c_sensor_init-----------\n");
	s5k3e2fx_sensor->config_gpio_on();
	s5k3e2fx_sensor->sensor_info->sensor_power_up();
	if ((rc = s5k3e2fx_i2c_read(S5K3E2FX_SS5M0_REG_MODEL_ID, &chipid)) < 0) {
		printk(KERN_ERR "s5k3e2fx_probe: could not read chip id, rc:%d\n", rc);
		return rc;
	}
	printk(KERN_INFO "s5k3e2fx_probe: chip id: %d(0x%x)\n", chipid, chipid);

	if (chipid != S5K3E2FX_SS5M0_MODEL_ID) {
		printk(KERN_INFO "s5k3e2fx_probe: chip id %d(0x%x) is invalid to \n",
							chipid, chipid);
		return -EINVAL;
	}
	
	if ((rc = s5k3e2fx_sensor->sensor_info->sensor_setting(CAMSENSOR_REG_INIT |
					((init->preview ? 0 : 1) << 1))) < 0) {
		printk(KERN_INFO "s5k3e2fx: failed to configure the sensor\n");
		return rc;
	}else
	printk("-----------------s5k3e2fx_i2c_sensor_init end-----------\n");
	pclk_set = 1;
	return 0;
}

static int s5k3e2fx_i2c_move_focus(uint16_t position)
{
	uint8_t next_position_msb, next_position_lsb;
	next_position_msb = position >> 8;
	next_position_lsb = position & 0x00FF;
	s5k3e2fx_i2c_write(0x3131, next_position_msb);
	s5k3e2fx_i2c_write(0x3132, next_position_lsb);
	return 0;
}

static int s5k3e2fx_i2c_set_default_focus(uint8_t step)
{
	s5k3e2fx_i2c_write(0x3131, 0x00);
	s5k3e2fx_i2c_write(0x3132, 0x00);
	return 0;
}

#undef I2C_AF_WRITE

static int s5k3e2fx_init_client(struct i2c_client *client)
{
	/* Initialize the s5k3e2fx Chip */
	init_waitqueue_head(&g_data_ready_wait_queue);
	return 0;
}

static struct file_operations s5k3e2fx_fops = {
        .owner 	= THIS_MODULE,
        .open 	= s5k3e2fx_open,
        .release = s5k3e2fx_release,
        .unlocked_ioctl = s5k3e2fx_ioctl,
};

static struct miscdevice s5k3e2fx_device = {
        .minor 	= MISC_DYNAMIC_MINOR,
        .name 	= "s5k3e2fx",
        .fops 	= &s5k3e2fx_fops,
};

static const char *S5K3E2FXVendor = "samsung";
static const char *S5K3E2FXNAME = "s5k3e2fx";
static const char *S5K3E2FXSize = "5M";

static ssize_t sensor5M_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "%s %s %s\n", S5K3E2FXVendor, S5K3E2FXNAME, S5K3E2FXSize);
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(sensor, 0444, sensor5M_vendor_show, NULL);


static struct kobject *android_s5k3e2fx;

static int s5k3e2fx_sysfs_init(void)
{
	int ret ;
	printk(KERN_INFO "s5k3e2fx:kobject creat and add\n");
	android_s5k3e2fx = kobject_create_and_add("android_camera", NULL);
	if (android_s5k3e2fx == NULL) {
		printk(KERN_INFO "s5k3e2fx_sysfs_init: subsystem_register " \
		"failed\n");
		ret = -ENOMEM;
		return ret ;
	}
	printk(KERN_INFO "s5k3e2fx:sysfs_create_file\n");
	ret = sysfs_create_file(android_s5k3e2fx, &dev_attr_sensor.attr);
	if (ret) {
		printk(KERN_INFO "s5k3e2fx_sysfs_init: sysfs_create_file " \
		"failed\n");
		kobject_del(android_s5k3e2fx);
	}
	return 0 ;
}


static int s5k3e2fx_remove(struct i2c_client *client)
{
	struct s5k3e2fx_data *mt;

	printk(KERN_INFO "s5k3e2fx: remove\n");
	if (!pclient) {
		printk(KERN_INFO "pclient is NULL, stop remove !\n");
		return 0;
	}
	mt = i2c_get_clientdata(client);
	free_irq(client->irq, mt);
	deinit_suspend();
	i2c_detach_client(client);
	pclient = NULL;
	misc_deregister(&s5k3e2fx_device);
	kfree(mt);
	return 0;
}

static void s5k3e2fx_late_resume(struct early_suspend *handler)
{
	printk("s5k3e2fx_late_resume\n");
	if (s5k3e2fx_sensor->cond == 0)
	{
		s5k3e2fx_sensor->sensor_info->sensor_late_resume(handler);
		s5k3e2fx_sensor->cond = 1;
		wake_up(&s5k3e2fx_sensor->event_wait);
	}else{
		return;
	}
}

static int s5k3e2fx_suspend(struct i2c_client *client,
	pm_message_t mesg)
{
	printk("s5k3e2fx_suspend\n");
	s5k3e2fx_sensor->cond = 0;
	return s5k3e2fx_sensor->sensor_info->sensor_suspend(client,mesg);
}

static int s5k3e2fx_resume(struct i2c_client *client)
{
	printk("s5k3e2fx_resume\n");
	return s5k3e2fx_sensor->sensor_info->sensor_resume(client);
}

static const struct i2c_device_id s5k3e2fx_id[] = {
	{ "s5k3e2fx", 0 },
	{ }
};

static struct early_suspend early_suspend_s5k3e2fx = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN+1,
	.resume = s5k3e2fx_late_resume,
	.suspend = NULL,
};

static int s5k3e2fx_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	struct s5k3e2fx_data *mt;
	int err = ENODEV;
	int rc;

	printk(KERN_INFO "s5k3e2fx: probe start\n");
	s5k3e2fx_sensor = client->dev.platform_data;
	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		goto exit_check_functionality_failed;		
	
	if(!(mt = kzalloc( sizeof(struct s5k3e2fx_data), GFP_KERNEL))) {
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	i2c_set_clientdata(client, mt);
	s5k3e2fx_init_client(client);
	pclient = client;
	/*sensor initial in probe */
	rc = s5k3e2fx_sensor->sensor_info->sensor_probe_initial(client);
	if(rc<0)
		return rc;
	s5k3e2fx_sensor->cond = 1 ;/*init condiction*/
	register_early_suspend(&early_suspend_s5k3e2fx);
	/* Register a misc device */
	err = misc_register(&s5k3e2fx_device);
	if(err) {
		printk(KERN_ERR "s5k3e2fx_probe: misc_register failed \n");
		goto exit_misc_device_register_failed;
	}
	init_waitqueue_head(&s5k3e2fx_sensor->event_wait);
	perf_lock_init(&camera_perf_lock, PERF_LOCK_HIGHEST, "camera_s5k3e2fx");
	init_suspend();
	s5k3e2fx_sysfs_init();
	return 0;
	
exit_misc_device_register_failed:
exit_alloc_data_failed:
exit_check_functionality_failed:
	
	return err;
}

static struct i2c_driver s5k3e2fx_driver = {
	.probe = s5k3e2fx_probe,
	.remove = s5k3e2fx_remove,
	.id_table = s5k3e2fx_id,
	.driver = {		
		.name   = "s5k3e2fx",
	},
	.suspend	= s5k3e2fx_suspend,
	.resume = s5k3e2fx_resume,
};


static int __init s5k3e2fx_init(void)
{
	return i2c_add_driver(&s5k3e2fx_driver);
}

static void __exit s5k3e2fx_exit(void)
{
	i2c_del_driver(&s5k3e2fx_driver);
}

module_init(s5k3e2fx_init);
module_exit(s5k3e2fx_exit);

MODULE_AUTHOR("Horng Chuang");
MODULE_DESCRIPTION("S5K3E2FX Driver");
MODULE_LICENSE("GPL");

