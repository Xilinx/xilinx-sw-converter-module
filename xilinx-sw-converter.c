/*
 * Xilinx Software Format Convertor Driver
 *
 * Copyright (C) 2021 Xilinx, Inc.
 *
 * Contacts: Anil Kumar M 	<amamidal@xilinx.com>
 * 	Karthikeyan T 	<kthangav@xilinx.com>
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

#include <dt-bindings/media/xilinx-vip.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/compiler.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/types.h>
#include <linux/v4l2-subdev.h>
#include <linux/xilinx-csi2rxss.h>
#include <linux/xilinx-v4l2-controls.h>
#include <media/media-entity.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>
#include <xilinx/xilinx-vip.h>

/*
 * Max string length for SW Convertor Data type string
 */
#define MAX_XIL_SWCONV_DT_STR_LENGTH 64

/* Number of media pads */
#define XILINX_SWCONV_MEDIA_PADS	(2)

#define XSWCONV_DEFAULT_WIDTH	(1920)
#define XSWCONV_DEFAULT_HEIGHT	(1080)

/**
 * struct xswfmtconv_feature - dt or IP property structure
 * @flags: Bitmask of properties enabled in IP or dt
 */
struct xswfmtconv_feature {
	u32 flags;
};

enum SWCONV_DataTypes {
	SWCONV_DT_FRAME_START_CODE = 0x00,
	SWCONV_DT_FRAME_END_CODE,
	SWCONV_DT_LINE_START_CODE,
	SWCONV_DT_LINE_END_CODE,
	SWCONV_DT_SYNC_RSVD_04,
	SWCONV_DT_SYNC_RSVD_05,
	SWCONV_DT_SYNC_RSVD_06,
	SWCONV_DT_SYNC_RSVD_07,
	SWCONV_DT_GSPKT_08,
	SWCONV_DT_GSPKT_09,
	SWCONV_DT_GSPKT_0A,
	SWCONV_DT_GSPKT_0B,
	SWCONV_DT_GSPKT_0C,
	SWCONV_DT_GSPKT_0D,
	SWCONV_DT_GSPKT_0E,
	SWCONV_DT_GSPKT_0F,
	SWCONV_DT_GLPKT_10,
	SWCONV_DT_GLPKT_11,
	SWCONV_DT_GLPKT_12,
	SWCONV_DT_GLPKT_13,
	SWCONV_DT_GLPKT_14,
	SWCONV_DT_GLPKT_15,
	SWCONV_DT_GLPKT_16,
	SWCONV_DT_GLPKT_17,
	SWCONV_DT_YUV_420_8B,
	SWCONV_DT_YUV_420_10B,
	SWCONV_DT_YUV_420_8B_LEGACY,
	SWCONV_DT_YUV_RSVD,
	SWCONV_DT_YUV_420_8B_CSPS,
	SWCONV_DT_YUV_420_10B_CSPS,
	SWCONV_DT_YUV_422_8B,
	SWCONV_DT_YUV_422_10B,
	SWCONV_DT_Y8_8B,
	SWCONV_DT_RGB_444,
	SWCONV_DT_RGB_555,
	SWCONV_DT_RGB_565,
	SWCONV_DT_RGB_666,
	SWCONV_DT_RGB_888,
	SWCONV_DT_RGB_RSVD_25,
	SWCONV_DT_RGB_RSVD_26,
	SWCONV_DT_RGB_RSVD_27,
	SWCONV_DT_RAW_6,
	SWCONV_DT_RAW_7,
	SWCONV_DT_RAW_8,
	SWCONV_DT_RAW_10,
	SWCONV_DT_RAW_12,
	SWCONV_DT_RAW_14,
	SWCONV_DT_RAW_16,
	SWCONV_DT_RAW_20,
	SWCONV_DT_USER_30,
	SWCONV_DT_USER_31,
	SWCONV_DT_USER_32,
	SWCONV_DT_USER_33,
	SWCONV_DT_USER_34,
	SWCONV_DT_USER_35,
	SWCONV_DT_USER_36,
	SWCONV_DT_USER_37,
	SWCONV_DT_RSVD_38,
	SWCONV_DT_RSVD_39,
	SWCONV_DT_RSVD_3A,
	SWCONV_DT_RSVD_3B,
	SWCONV_DT_RSVD_3C,
	SWCONV_DT_RSVD_3D,
	SWCONV_DT_RSVD_3E,
	SWCONV_DT_RSVD_3F
};

/**
 * struct pixel_format - Data Type to string name structure
 * @PixelFormat: SW Convertor Data type
 * @PixelFormatStr: String name of Data Type
 */
struct pixel_format {
	enum SWCONV_DataTypes PixelFormat;
	char PixelFormatStr[MAX_XIL_SWCONV_DT_STR_LENGTH];
};

/*
 * struct xswfmtconv_core - Core configuration SW format convertor device structure
 * @dev: Platform structure
 * @iomem: Base address of subsystem
 * @irq: requested irq number
 * @dphy_present: Flag for DPHY register interface presence
 * @dphy_offset: DPHY registers offset
 * @enable_active_lanes: If number of active lanes can be modified
 * @max_num_lanes: Maximum number of lanes present
 * @vfb: Video Format Bridge enabled or not
 * @ppc: pixels per clock
 * @vc: Virtual Channel
 * @axis_tdata_width: AXI Stream data width
 * @datatype: Data type filter
 * @pxlformat: String with SW Convertor pixel format from IP
 * @num_lanes: Number of lanes requested from application
 * @en_vcx: If more than 4 VC are enabled.
 * @cfg: Pointer to sw convertor config structure
 * @lite_aclk: AXI4-Lite interface clock
 * @video_aclk: Video clock
 * @dphy_clk_200M: 200MHz DPHY clock
 * @rst_gpio: video_aresetn
 */
struct xswfmtconv_core {
	struct device *dev;
	void __iomem *iomem;
	int irq;
	u32 dphy_offset;
	bool dphy_present;
	bool enable_active_lanes;
	u32 max_num_lanes;
	bool vfb;
	u32 ppc;
	u32 vc;
	u32 axis_tdata_width;
	u32 datatype;
	const char *pxlformat;
	u32 num_lanes;
	bool en_vcx;
	const struct xswfmtconv_feature *cfg;
	struct clk *lite_aclk;
	struct clk *video_aclk;
	struct clk *dphy_clk_200M;
	struct gpio_desc *rst_gpio;
};

/**
 * struct xswfmtconv_state - SW format convertor device structure
 * @core: Core structure for SW format convertor
 * @subdev: The v4l2 subdev structure
 * @ctrl_handler: control handler
 * @formats: Active V4L2 formats on each pad
 * @default_format: default V4L2 media bus format
 * @vip_format: format information corresponding to the active format
 * @event: Holds the short packet event
 * @lock: mutex for serializing operations
 * @pads: media pads
 * @npads: number of pads
 * @streaming: Flag for storing streaming state
 * @suspended: Flag for storing suspended state
 *
 * This structure contains the device driver related parameters
 */
struct xswfmtconv_state {
	struct xswfmtconv_core core;
	struct v4l2_subdev subdev;
	struct v4l2_ctrl_handler ctrl_handler;
	struct v4l2_mbus_framefmt formats[2];
	struct v4l2_mbus_framefmt default_format;
	const struct xvip_video_format *vip_format;
	struct v4l2_event event;
	struct mutex lock;
	struct media_pad pads[XILINX_SWCONV_MEDIA_PADS];
	unsigned int npads;
	bool streaming;
	bool suspended;
};

static const struct xswfmtconv_feature xlnx_swfmtconv_v1_0 = {
	.flags = 0,
};

static const struct of_device_id xswfmtconv_of_id_table[] = {
	{ .compatible = "xlnx,sw-convertor",
		.data = &xlnx_swfmtconv_v1_0 },
	{ }
};
MODULE_DEVICE_TABLE(of, xswfmtconv_of_id_table);

static inline struct xswfmtconv_state *
to_xswfmtconvstate(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct xswfmtconv_state, subdev);
}


static const struct pixel_format pixel_formats[] = {
	{ SWCONV_DT_YUV_420_8B, "YUV420_8bit" },
	{ SWCONV_DT_YUV_420_10B, "YUV420_10bit" },
	{ SWCONV_DT_YUV_420_8B_LEGACY, "Legacy_YUV420_8bit" },
	{ SWCONV_DT_YUV_420_8B_CSPS, "YUV420_8bit_CSPS" },
	{ SWCONV_DT_YUV_420_10B_CSPS, "YUV420_10bit_CSPS" },
	{ SWCONV_DT_YUV_422_8B, "YUV422_8bit" },
	{ SWCONV_DT_YUV_422_10B, "YUV422_10bit" },
	{ SWCONV_DT_RGB_444, "RGB444" },
	{ SWCONV_DT_RGB_555, "RGB555" },
	{ SWCONV_DT_RGB_565, "RGB565" },
	{ SWCONV_DT_RGB_666, "RGB666" },
	{ SWCONV_DT_RGB_888, "RGB888" },
	{ SWCONV_DT_RAW_6, "RAW6" },
	{ SWCONV_DT_RAW_7, "RAW7" },
	{ SWCONV_DT_RAW_8, "RAW8" },
	{ SWCONV_DT_RAW_10, "RAW10" },
	{ SWCONV_DT_RAW_12, "RAW12" },
	{ SWCONV_DT_RAW_14, "RAW14"},
	{ SWCONV_DT_RAW_16, "RAW16"},
	{ SWCONV_DT_RAW_20, "RAW20"}
};

/**
 * xswfmtconv_pxlfmtstrtodt - Convert pixel format string got from dts
 * to data type.
 * @pxlfmtstr: String obtained while parsing device node
 *
 * This function takes a SW convertor pixel format string obtained while parsing
 * device tree node and converts it to data type.
 *
 * Return: Equivalent pixel format value from table
 */
static u32 xswfmtconv_pxlfmtstrtodt(const char *pxlfmtstr)
{
	u32 Index;
	u32 MaxEntries = ARRAY_SIZE(pixel_formats);

	for (Index = 0; Index < MaxEntries; Index++) {
		if (!strncmp(pixel_formats[Index].PixelFormatStr,
				pxlfmtstr, MAX_XIL_SWCONV_DT_STR_LENGTH))
			return pixel_formats[Index].PixelFormat;
	}

	return -EINVAL;
}

/**
 * xswfmtconv_pxlfmtdttostr - Convert pixel format data type to string.
 * @datatype: SW convertor Data Type
 *
 * Return: Equivalent pixel format string from table
 */
static const char *xswfmtconv_pxlfmtdttostr(u32 datatype)
{
	u32 Index;
	u32 MaxEntries = ARRAY_SIZE(pixel_formats);

	for (Index = 0; Index < MaxEntries; Index++) {
		if (pixel_formats[Index].PixelFormat == datatype)
			return pixel_formats[Index].PixelFormatStr;
	}

	return NULL;
}

/**
 * xswfmtconv_s_stream - Empty function
 * to be compatible with V4L2
 */
static int xswfmtconv_s_stream(struct v4l2_subdev *sd, int enable)
{
	return 0;
}

static struct v4l2_mbus_framefmt *
__xswfmtconv_get_pad_format(struct xswfmtconv_state *xswfmtconv,
				struct v4l2_subdev_pad_config *cfg,
				unsigned int pad, u32 which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(&xswfmtconv->subdev, cfg, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &xswfmtconv->formats[pad];
	default:
		return NULL;
	}
}

/**
 * xswfmtconv_get_format - Get the pad format
 * @sd: Pointer to V4L2 Sub device structure
 * @cfg: Pointer to sub device pad information structure
 * @fmt: Pointer to pad level media bus format
 *
 * This function is used to get the pad format information.
 *
 * Return: 0 on success
 */
static int xswfmtconv_get_format(struct v4l2_subdev *sd,
					struct v4l2_subdev_pad_config *cfg,
					struct v4l2_subdev_format *fmt)
{

	struct xswfmtconv_state *xswfmtconv = to_xswfmtconvstate(sd);
	struct v4l2_mbus_framefmt *get_fmt;
	int ret = 0;

	mutex_lock(&xswfmtconv->lock);
	get_fmt = __xswfmtconv_get_pad_format(xswfmtconv, cfg,
							fmt->pad, fmt->which);

	if (!get_fmt) {
			ret = -EINVAL;
			goto unlock_get_format;
	}
	fmt->format = *get_fmt;

unlock_get_format:
	mutex_unlock(&xswfmtconv->lock);

	return ret;
}

/**
 * xswfmtconv_set_format - This is used to set the pad format
 * @sd: Pointer to V4L2 Sub device structure
 * @cfg: Pointer to sub device pad information structure
 * @fmt: Pointer to pad level media bus format
 *
 * This function is used to set the pad format.
 * Since the pad format is fixed in hardware, it can't be
 * modified on run time. So when a format set is requested by
 * application, all parameters except the format type is
 * saved for the pad and the original pad format is sent
 * back to the application.
 *
 * Return: 0 on success
 */
static int xswfmtconv_set_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *format;
	struct xswfmtconv_state *xswfmtconv = to_xswfmtconvstate(sd);
	struct xswfmtconv_core *core = &xswfmtconv->core;
	u32 code;
	int ret = 0;

	mutex_lock(&xswfmtconv->lock);

	format = __xswfmtconv_get_pad_format(xswfmtconv, cfg,
						fmt->pad, fmt->which);
	if (!format) {
		dev_err(core->dev, "get pad format error\n");
		ret = -EINVAL;
		goto unlock_set_fmt;
	}

	*format = fmt->format;

	/* Restore the original pad format code */
	format->code = fmt->format.code;;
	format->width = fmt->format.width;
	format->height = fmt->format.height;
	fmt->format = *format;
unlock_set_fmt:
	mutex_unlock(&xswfmtconv->lock);

	return ret;
}

/**
 * xswfmtconv_open - Called on v4l2_open()
 * @sd: Pointer to V4L2 sub device structure
 * @fh: Pointer to V4L2 File handle
 *
 * This function is called on v4l2_open(). It sets the default format
 * for both pads.
 *
 * Return: 0 on success
 */
static int xswfmtconv_open(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct v4l2_mbus_framefmt *format;
	struct xswfmtconv_state *xswfmtconv = to_xswfmtconvstate(sd);

	format = v4l2_subdev_get_try_format(sd, fh->pad, 0);
	*format = xswfmtconv->default_format;

	format = v4l2_subdev_get_try_format(sd, fh->pad, 1);
	*format = xswfmtconv->default_format;

	return 0;
}

static int xswfmtconv_close(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	return 0;
}



/**
 *  * xswfmtconv_log_status - Empty function
 * 		to be compatible with V4L2
 *        */
static int xswfmtconv_log_status(struct v4l2_subdev *sd)
{
	return 0;
}

/* -----------------------------------------------------------------------------
 * Media Operations
 */

static const struct media_entity_operations xswfmtconv_media_ops = {
	.link_validate = v4l2_subdev_link_validate
};

static struct v4l2_subdev_video_ops xswfmtconv_video_ops = {
	.s_stream = xswfmtconv_s_stream
};

static struct v4l2_subdev_pad_ops xswfmtconv_pad_ops = {
	.get_fmt = xswfmtconv_get_format,
	.set_fmt = xswfmtconv_set_format,
};


static const struct v4l2_subdev_core_ops xswfmtconv_core_ops = {
    .log_status = xswfmtconv_log_status,
};

static struct v4l2_subdev_ops xswfmtconv_ops = {
	.core = &xswfmtconv_core_ops,
	.video = &xswfmtconv_video_ops,
	.pad = &xswfmtconv_pad_ops
};

static const struct v4l2_subdev_internal_ops xswfmtconv_internal_ops = {
	.open = xswfmtconv_open,
	.close = xswfmtconv_close
};

/* -----------------------------------------------------------------------------
 * Platform Device Driver
 */

static int xswfmtconv_parse_of(struct xswfmtconv_state *xswfmtconv)
{
	struct device_node *node = xswfmtconv->core.dev->of_node;
	struct device_node *ports = NULL;
	struct device_node *port = NULL;
	unsigned int nports = 0;
	struct xswfmtconv_core *core = &xswfmtconv->core;
	int ret;
	bool iic_present;

	ret = of_property_read_string(node, "xlnx,pxl-format",
					&core->pxlformat);
	if (ret < 0) {
		dev_err(core->dev, "missing xlnx,pxl-format property\n");
		return ret;
	}

	core->datatype = xswfmtconv_pxlfmtstrtodt(core->pxlformat);
	if ((core->datatype < SWCONV_DT_YUV_420_8B) ||
		(core->datatype > SWCONV_DT_RAW_20)) {
		dev_err(core->dev, "Invalid xlnx,pxl-format string\n");
		return -EINVAL;
	}

	ports = of_get_child_by_name(node, "ports");
	if (ports == NULL)
		ports = node;

	for_each_child_of_node(ports, port) {
		int ret;
		const struct xvip_video_format *format;
		struct device_node *endpoint;
		struct v4l2_fwnode_endpoint v4lendpoint = { 0 };

		if (!port->name || of_node_cmp(port->name, "port"))
			continue;

		/*
		 * Currently only a subset of VFB enabled formats present in
		 * xvip are supported in the  driver.
		 *
		 * If the VFB is disabled, the pixels per clock don't matter.
		 * The data width is either 32 or 64 bit as selected in design.
		 *
		 * For e.g. If Data Type is RGB888, VFB is disabled and
		 * data width is 32 bits.
		 *
		 * Clk Cycle  |  Byte 0  |  Byte 1  |  Byte 2  |  Byte 3
		 * -----------+----------+----------+----------+----------
		 *     1      |     B0   |     G0   |     R0   |     B1
		 *     2      |     G1   |     R1   |     B2   |     G2
		 *     3      |     R2   |     B3   |     G3   |     R3
		 */
		format = xvip_of_get_format(port);
		if (IS_ERR(format)) {
			dev_err(core->dev, "invalid format in DT");
			return PTR_ERR(format);
		}

		if (core->vfb &&
			(format->vf_code != XVIP_VF_YUV_422) &&
			(format->vf_code != XVIP_VF_YUV_420) &&
			(format->vf_code != XVIP_VF_RBG) &&
			(format->vf_code != XVIP_VF_MONO_SENSOR) &&
			(format->vf_code != XVIP_VF_Y_GREY)) {
			dev_err(core->dev, "Invalid UG934 video format set.\n");
			return -EINVAL;
		}

		xswfmtconv->vip_format = format;

		endpoint = of_get_next_child(port, NULL);
		if (!endpoint) {
			dev_err(core->dev, "No port at\n");
			return -EINVAL;
		}

		ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(endpoint),
						 &v4lendpoint);
		if (ret) {
			of_node_put(endpoint);
			return ret;
		}

		of_node_put(endpoint);
		dev_dbg(core->dev, "%s : port %d bus type = %d\n",
				__func__, nports, v4lendpoint.bus_type);

		/* Count the number of ports. */
		nports++;
	}

	if (nports != 2) {
		dev_err(core->dev, "invalid number of ports %u\n", nports);
		return -EINVAL;
	}
	xswfmtconv->npads = nports;

	return 0;
}

static int xswfmtconv_probe(struct platform_device *pdev)
{
	struct v4l2_subdev *subdev;
	struct xswfmtconv_state *xswfmtconv;
	struct resource *res;
	const struct of_device_id *match;
	struct device_node *node = pdev->dev.of_node;
	u32 i;
	int ret;
	int num_ctrls;

	xswfmtconv = devm_kzalloc(&pdev->dev, sizeof(*xswfmtconv), GFP_KERNEL);
	if (!xswfmtconv)
		return -ENOMEM;

	mutex_init(&xswfmtconv->lock);

	xswfmtconv->core.dev = &pdev->dev;

	match = of_match_node(xswfmtconv_of_id_table, node);
	if (!match)
		return -ENODEV;

	xswfmtconv->core.cfg = match->data;

	ret = xswfmtconv_parse_of(xswfmtconv);
	if (ret < 0) {
		dev_err(&pdev->dev, "xswfmtconv_parse_of ret = %d\n", ret);
		return ret;
	}

	/* Initialize V4L2 subdevice and media entity */
	xswfmtconv->pads[0].flags = MEDIA_PAD_FL_SOURCE;
	xswfmtconv->pads[1].flags = MEDIA_PAD_FL_SINK;

	/* Initialize the default format */
	memset(&xswfmtconv->default_format, 0,
		sizeof(xswfmtconv->default_format));
	xswfmtconv->default_format.code = xswfmtconv->vip_format->code;
	xswfmtconv->default_format.field = V4L2_FIELD_NONE;
	xswfmtconv->default_format.colorspace = V4L2_COLORSPACE_SRGB;
	xswfmtconv->default_format.width = XSWCONV_DEFAULT_WIDTH;
	xswfmtconv->default_format.height = XSWCONV_DEFAULT_HEIGHT;

	xswfmtconv->formats[0] = xswfmtconv->default_format;
	xswfmtconv->formats[1] = xswfmtconv->default_format;

	/* Initialize V4L2 subdevice and media entity */
	subdev = &xswfmtconv->subdev;

	v4l2_subdev_init(subdev, &xswfmtconv_ops);

	subdev->dev = &pdev->dev;
	subdev->internal_ops = &xswfmtconv_internal_ops;
	strlcpy(subdev->name, dev_name(&pdev->dev), sizeof(subdev->name));

	subdev->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	subdev->entity.ops = &xswfmtconv_media_ops;

	v4l2_set_subdevdata(subdev, xswfmtconv);

	ret = media_entity_pads_init(&subdev->entity, 2, xswfmtconv->pads);
	if (ret < 0) {
		dev_err(&pdev->dev, "media pad init failed = %d\n", ret);
		goto error;
	}

	platform_set_drvdata(pdev, xswfmtconv);

	ret = v4l2_async_register_subdev(subdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register subdev\n");
		goto error;
	}

	/* default states for streaming and suspend */
	xswfmtconv->streaming = false;
	xswfmtconv->suspended = false;
	return 0;

error:
	v4l2_ctrl_handler_free(&xswfmtconv->ctrl_handler);
	media_entity_cleanup(&subdev->entity);
	mutex_destroy(&xswfmtconv->lock);

	return ret;
}

static int xswfmtconv_remove(struct platform_device *pdev)
{
	struct xswfmtconv_state *xswfmtconv = platform_get_drvdata(pdev);
	struct v4l2_subdev *subdev = &xswfmtconv->subdev;

	v4l2_async_unregister_subdev(subdev);
	media_entity_cleanup(&subdev->entity);
	mutex_destroy(&xswfmtconv->lock);

	return 0;
}

static struct platform_driver xswfmtconv_driver = {
	.driver = {
		.name		= "xlnx,sw-convertor",
		.of_match_table	= xswfmtconv_of_id_table,
	},
	.probe			= xswfmtconv_probe,
	.remove			= xswfmtconv_remove,
};

module_platform_driver(xswfmtconv_driver);

MODULE_AUTHOR("Anil Kumar M <amamidal@xilinx.com");
MODULE_AUTHOR("Karthikeyan T <kthangav@xilinx.com");
MODULE_DESCRIPTION("Xilinx SW Format Convertor Driver");
MODULE_LICENSE("GPL v2");
