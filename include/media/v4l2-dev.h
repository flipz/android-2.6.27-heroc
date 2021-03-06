/*
 *
 *	V 4 L 2   D R I V E R   H E L P E R   A P I
 *
 * Moved from videodev2.h
 *
 *	Some commonly needed functions for drivers (v4l2-common.o module)
 */
#ifndef _V4L2_DEV_H
#define _V4L2_DEV_H

#define OBSOLETE_OWNER   1 /* to be removed soon */
#define OBSOLETE_DEVDATA 1 /* to be removed soon */

#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/compiler.h> /* need __user */
#ifdef CONFIG_VIDEO_V4L1_COMPAT
#include <linux/videodev.h>
#else
#include <linux/videodev2.h>
#endif

#define VIDEO_MAJOR	81
/* Minor device allocation */
#define MINOR_VFL_TYPE_GRABBER_MIN   0
#define MINOR_VFL_TYPE_GRABBER_MAX  63
#define MINOR_VFL_TYPE_RADIO_MIN    64
#define MINOR_VFL_TYPE_RADIO_MAX   127
#define MINOR_VFL_TYPE_VTX_MIN     192
#define MINOR_VFL_TYPE_VTX_MAX     223
#define MINOR_VFL_TYPE_VBI_MIN     224
#define MINOR_VFL_TYPE_VBI_MAX     255

#define VFL_TYPE_GRABBER	0
#define VFL_TYPE_VBI		1
#define VFL_TYPE_RADIO		2
#define VFL_TYPE_VTX		3

struct v4l2_ioctl_callbacks;

/*
 * Newer version of video_device, handled by videodev2.c
 * 	This version moves redundant code from video device code to
 *	the common handler
 */

struct video_device
{
	/* device ops */
	const struct file_operations *fops;

	/* sysfs */
	struct device dev;		/* v4l device */
	struct device *parent;		/* device parent */

	/* device info */
	char name[32];
	int vfl_type;
	int type;       /* v4l1 */
	int type2;      /* v4l2 */
	int minor;
	/* attribute to differentiate multiple indices on one physical device */
	int index;

	int debug;			/* Activates debug level*/

	/* Video standard vars */
	v4l2_std_id tvnorms;		/* Supported tv norms */
	v4l2_std_id current_norm;	/* Current tvnorm */

	/* callbacks */
	void (*release)(struct video_device *vfd);

	/* ioctl callbacks */
	const struct v4l2_ioctl_ops *ioctl_ops;

	/* VIDIOC_QUERYCAP handler */
	int (*vidioc_querycap)(struct file *file, void *fh, struct v4l2_capability *cap);

	/* Priority handling */
	int (*vidioc_g_priority)   (struct file *file, void *fh,
				    enum v4l2_priority *p);
	int (*vidioc_s_priority)   (struct file *file, void *fh,
				    enum v4l2_priority p);

	/* VIDIOC_ENUM_FMT handlers */
	int (*vidioc_enum_fmt_cap)         (struct file *file, void *fh,
					    struct v4l2_fmtdesc *f);
	int (*vidioc_enum_fmt_overlay)     (struct file *file, void *fh,
					    struct v4l2_fmtdesc *f);
	int (*vidioc_enum_fmt_vbi)         (struct file *file, void *fh,
					    struct v4l2_fmtdesc *f);
	int (*vidioc_enum_fmt_vbi_capture) (struct file *file, void *fh,
					    struct v4l2_fmtdesc *f);
	int (*vidioc_enum_fmt_video_output)(struct file *file, void *fh,
					    struct v4l2_fmtdesc *f);
	int (*vidioc_enum_fmt_output_overlay) (struct file *file, void *fh,
					    struct v4l2_fmtdesc *f);
	int (*vidioc_enum_fmt_vbi_output)  (struct file *file, void *fh,
					    struct v4l2_fmtdesc *f);
	int (*vidioc_enum_fmt_type_private)(struct file *file, void *fh,
					    struct v4l2_fmtdesc *f);

	/* VIDIOC_G_FMT handlers */
	int (*vidioc_g_fmt_cap)        (struct file *file, void *fh,
					struct v4l2_format *f);
	int (*vidioc_g_fmt_overlay)    (struct file *file, void *fh,
					struct v4l2_format *f);
	int (*vidioc_g_fmt_vbi)        (struct file *file, void *fh,
					struct v4l2_format *f);
	int (*vidioc_g_fmt_vbi_output) (struct file *file, void *fh,
					struct v4l2_format *f);
	int (*vidioc_g_fmt_vbi_capture)(struct file *file, void *fh,
					struct v4l2_format *f);
	int (*vidioc_g_fmt_video_output)(struct file *file, void *fh,
					struct v4l2_format *f);
	int (*vidioc_g_fmt_output_overlay) (struct file *file, void *fh,
					struct v4l2_format *f);
	int (*vidioc_g_fmt_type_private)(struct file *file, void *fh,
					struct v4l2_format *f);

	/* VIDIOC_S_FMT handlers */
	int (*vidioc_s_fmt_cap)        (struct file *file, void *fh,
					struct v4l2_format *f);

	int (*vidioc_s_fmt_overlay)    (struct file *file, void *fh,
					struct v4l2_format *f);
	int (*vidioc_s_fmt_vbi)        (struct file *file, void *fh,
					struct v4l2_format *f);
	int (*vidioc_s_fmt_vbi_output) (struct file *file, void *fh,
					struct v4l2_format *f);
	int (*vidioc_s_fmt_vbi_capture)(struct file *file, void *fh,
					struct v4l2_format *f);
	int (*vidioc_s_fmt_video_output)(struct file *file, void *fh,
					struct v4l2_format *f);
	int (*vidioc_s_fmt_output_overlay) (struct file *file, void *fh,
					struct v4l2_format *f);
	int (*vidioc_s_fmt_type_private)(struct file *file, void *fh,
					struct v4l2_format *f);

	/* VIDIOC_TRY_FMT handlers */
	int (*vidioc_try_fmt_cap)        (struct file *file, void *fh,
					  struct v4l2_format *f);
	int (*vidioc_try_fmt_overlay)    (struct file *file, void *fh,
					  struct v4l2_format *f);
	int (*vidioc_try_fmt_vbi)        (struct file *file, void *fh,
					  struct v4l2_format *f);
	int (*vidioc_try_fmt_vbi_output) (struct file *file, void *fh,
					  struct v4l2_format *f);
	int (*vidioc_try_fmt_vbi_capture)(struct file *file, void *fh,
					  struct v4l2_format *f);
	int (*vidioc_try_fmt_video_output)(struct file *file, void *fh,
					  struct v4l2_format *f);
	int (*vidioc_try_fmt_output_overlay)(struct file *file, void *fh,
					  struct v4l2_format *f);
	int (*vidioc_try_fmt_type_private)(struct file *file, void *fh,
					  struct v4l2_format *f);

	/* Buffer handlers */
	int (*vidioc_reqbufs) (struct file *file, void *fh, struct v4l2_requestbuffers *b);
	int (*vidioc_querybuf)(struct file *file, void *fh, struct v4l2_buffer *b);
	int (*vidioc_qbuf)    (struct file *file, void *fh, struct v4l2_buffer *b);
	int (*vidioc_dqbuf)   (struct file *file, void *fh, struct v4l2_buffer *b);


	int (*vidioc_overlay) (struct file *file, void *fh, unsigned int i);
#ifdef CONFIG_VIDEO_V4L1_COMPAT
			/* buffer type is struct vidio_mbuf * */
	int (*vidiocgmbuf)  (struct file *file, void *fh, struct video_mbuf *p);
#endif
	int (*vidioc_g_fbuf)   (struct file *file, void *fh,
				struct v4l2_framebuffer *a);
	int (*vidioc_s_fbuf)   (struct file *file, void *fh,
				struct v4l2_framebuffer *a);

		/* Stream on/off */
	int (*vidioc_streamon) (struct file *file, void *fh, enum v4l2_buf_type i);
	int (*vidioc_streamoff)(struct file *file, void *fh, enum v4l2_buf_type i);

		/* Standard handling
			G_STD and ENUMSTD are handled by videodev.c
		 */
	int (*vidioc_s_std) (struct file *file, void *fh, v4l2_std_id *norm);
	int (*vidioc_querystd) (struct file *file, void *fh, v4l2_std_id *a);

		/* Input handling */
	int (*vidioc_enum_input)(struct file *file, void *fh,
				 struct v4l2_input *inp);
	int (*vidioc_g_input)   (struct file *file, void *fh, unsigned int *i);
	int (*vidioc_s_input)   (struct file *file, void *fh, unsigned int i);

		/* Output handling */
	int (*vidioc_enumoutput) (struct file *file, void *fh,
				  struct v4l2_output *a);
	int (*vidioc_g_output)   (struct file *file, void *fh, unsigned int *i);
	int (*vidioc_s_output)   (struct file *file, void *fh, unsigned int i);

		/* Control handling */
	int (*vidioc_queryctrl)        (struct file *file, void *fh,
					struct v4l2_queryctrl *a);
	int (*vidioc_g_ctrl)           (struct file *file, void *fh,
					struct v4l2_control *a);
	int (*vidioc_s_ctrl)           (struct file *file, void *fh,
					struct v4l2_control *a);
	int (*vidioc_g_ext_ctrls)      (struct file *file, void *fh,
					struct v4l2_ext_controls *a);
	int (*vidioc_s_ext_ctrls)      (struct file *file, void *fh,
					struct v4l2_ext_controls *a);
	int (*vidioc_try_ext_ctrls)    (struct file *file, void *fh,
					struct v4l2_ext_controls *a);
	int (*vidioc_querymenu)        (struct file *file, void *fh,
					struct v4l2_querymenu *a);

	/* Audio ioctls */
	int (*vidioc_enumaudio)        (struct file *file, void *fh,
					struct v4l2_audio *a);
	int (*vidioc_g_audio)          (struct file *file, void *fh,
					struct v4l2_audio *a);
	int (*vidioc_s_audio)          (struct file *file, void *fh,
					struct v4l2_audio *a);

	/* Audio out ioctls */
	int (*vidioc_enumaudout)       (struct file *file, void *fh,
					struct v4l2_audioout *a);
	int (*vidioc_g_audout)         (struct file *file, void *fh,
					struct v4l2_audioout *a);
	int (*vidioc_s_audout)         (struct file *file, void *fh,
					struct v4l2_audioout *a);
	int (*vidioc_g_modulator)      (struct file *file, void *fh,
					struct v4l2_modulator *a);
	int (*vidioc_s_modulator)      (struct file *file, void *fh,
					struct v4l2_modulator *a);
	/* Crop ioctls */
	int (*vidioc_cropcap)          (struct file *file, void *fh,
					struct v4l2_cropcap *a);
	int (*vidioc_g_crop)           (struct file *file, void *fh,
					struct v4l2_crop *a);
	int (*vidioc_s_crop)           (struct file *file, void *fh,
					struct v4l2_crop *a);
	/* Compression ioctls */
	int (*vidioc_g_jpegcomp)       (struct file *file, void *fh,
					struct v4l2_jpegcompression *a);
	int (*vidioc_s_jpegcomp)       (struct file *file, void *fh,
					struct v4l2_jpegcompression *a);
	int (*vidioc_g_enc_index)      (struct file *file, void *fh,
					struct v4l2_enc_idx *a);
	int (*vidioc_encoder_cmd)      (struct file *file, void *fh,
					struct v4l2_encoder_cmd *a);
	int (*vidioc_try_encoder_cmd)  (struct file *file, void *fh,
					struct v4l2_encoder_cmd *a);

	/* Stream type-dependent parameter ioctls */
	int (*vidioc_g_parm)           (struct file *file, void *fh,
					struct v4l2_streamparm *a);
	int (*vidioc_s_parm)           (struct file *file, void *fh,
					struct v4l2_streamparm *a);

	/* Tuner ioctls */
	int (*vidioc_g_tuner)          (struct file *file, void *fh,
					struct v4l2_tuner *a);
	int (*vidioc_s_tuner)          (struct file *file, void *fh,
					struct v4l2_tuner *a);
	int (*vidioc_g_frequency)      (struct file *file, void *fh,
					struct v4l2_frequency *a);
	int (*vidioc_s_frequency)      (struct file *file, void *fh,
					struct v4l2_frequency *a);

	/* Sliced VBI cap */
	int (*vidioc_g_sliced_vbi_cap) (struct file *file, void *fh,
					struct v4l2_sliced_vbi_cap *a);

	/* Log status ioctl */
	int (*vidioc_log_status)       (struct file *file, void *fh);


	/* Debugging ioctls */
#ifdef CONFIG_VIDEO_ADV_DEBUG
	int (*vidioc_g_register)       (struct file *file, void *fh,
					struct v4l2_register *reg);
	int (*vidioc_s_register)       (struct file *file, void *fh,
					struct v4l2_register *reg);
#endif
	int (*vidioc_g_chip_ident)     (struct file *file, void *fh,
					struct v4l2_chip_ident *chip);


#ifdef OBSOLETE_OWNER /* to be removed soon */
/* obsolete -- fops->owner is used instead */
struct module *owner;

#endif

#ifdef OBSOLETE_DEVDATA /* to be removed soon */
	/* dev->driver_data will be used instead some day.
	 * Use the video_{get|set}_drvdata() helper functions,
	 * so the switch over will be transparent for you.
	 * Or use {pci|usb}_{get|set}_drvdata() directly. */
	void *priv;
#endif

	/* for videodev.c internal usage -- please don't touch */
	int users;                     /* video_exclusive_{open|close} ... */
	struct mutex lock;             /* ... helper function uses these   */
};

/* Class-dev to video-device */
#define to_video_device(cd) container_of(cd, struct video_device, dev)

/* Version 2 functions */
extern int video_register_device(struct video_device *vfd, int type, int nr);
int video_register_device_index(struct video_device *vfd, int type, int nr,
					int index);
void video_unregister_device(struct video_device *);
extern int video_ioctl2(struct inode *inode, struct file *file,
			  unsigned int cmd, unsigned long arg);

/* helper functions to alloc / release struct video_device, the
   later can be used for video_device->release() */
struct video_device *video_device_alloc(void);
void video_device_release(struct video_device *vfd);

#ifdef OBSOLETE_DEVDATA /* to be removed soon */
/* helper functions to access driver private data. */
static inline void *video_get_drvdata(struct video_device *dev)
{
	return dev->priv;
}

static inline void video_set_drvdata(struct video_device *dev, void *data)
{
	dev->priv = data;
}

/* Obsolete stuff - Still needed for radio devices and obsolete drivers */
extern struct video_device* video_devdata(struct file*);
extern int video_exclusive_open(struct inode *inode, struct file *file);
extern int video_exclusive_release(struct inode *inode, struct file *file);
#endif

#endif /* _V4L2_DEV_H */
