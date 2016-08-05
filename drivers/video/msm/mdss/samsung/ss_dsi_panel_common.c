/*
 * =================================================================
 *
 *
 *	Description:  samsung display common file
 *
 *	Author: jb09.kim
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2015, Samsung Electronics. All rights reserved.

*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
*/
#include "ss_dsi_panel_common.h"
#include "../mdss_debug.h"

static void mdss_samsung_event_osc_te_fitting(struct mdss_panel_data *pdata, int event, void *arg);
static irqreturn_t samsung_te_check_handler(int irq, void *handle);
static void samsung_te_check_done_work(struct work_struct *work);
static void mdss_samsung_event_esd_recovery_init(struct mdss_panel_data *pdata, int event, void *arg);
static void mdss_samsung_panel_lpm_ctrl(struct mdss_panel_data *pdata, int event);
struct samsung_display_driver_data vdd_data;

struct dsi_cmd_desc default_cmd = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};

#if defined(CONFIG_LCD_CLASS_DEVICE)
/* SYSFS RELATED VALUE */
#define MAX_FILE_NAME 128
#define TUNING_FILE_PATH "/sdcard/"
static char tuning_file[MAX_FILE_NAME];
#endif
u8 csc_update = 1;
u8 csc_change = 0;

void __iomem *virt_mmss_gp_base;

struct samsung_display_driver_data *check_valid_ctrl(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = NULL;

	if (IS_ERR_OR_NULL(ctrl)) {
		pr_err("%s: Invalid data ctrl : 0x%zx\n", __func__, (size_t)ctrl);
		return NULL;
	}

	vdd = (struct samsung_display_driver_data *)ctrl->panel_data.panel_private;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid vdd_data : 0x%zx\n", __func__, (size_t)vdd);
		return NULL;
	}

	return vdd;
}

char mdss_panel_id0_get(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}

	return (vdd->manufacture_id_dsi[display_ndx_check(ctrl)] & 0xFF0000) >> 16;
}

char mdss_panel_id1_get(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}

	return (vdd->manufacture_id_dsi[display_ndx_check(ctrl)] & 0xFF00) >> 8;
}

char mdss_panel_id2_get(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}

	return vdd->manufacture_id_dsi[display_ndx_check(ctrl)] & 0xFF;
}

int mdss_panel_attach_get(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}
		
	return (vdd->panel_attach_status & (0x01 << ctrl->ndx)) > 0 ? true : false;
}

int mdss_panel_attach_set(struct mdss_dsi_ctrl_pdata *ctrl, int status)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return -ERANGE;
	}

	/* 0bit->DSI0 1bit->DSI1 */
	/* check the lcd id for DISPLAY_1 and DISPLAY_2 */
	if (likely(mdss_panel_attached(DISPLAY_1) || mdss_panel_attached(DISPLAY_2)) && status) {
		if (ctrl->cmd_sync_wait_broadcast)
			vdd->panel_attach_status |= (BIT(1) | BIT(0));
		else {
			/* One more time check dual dsi */
			if (!IS_ERR_OR_NULL(vdd->ctrl_dsi[DSI_CTRL_0]) &&
				!IS_ERR_OR_NULL(vdd->ctrl_dsi[DSI_CTRL_1]))
				vdd->panel_attach_status |= (BIT(1) | BIT(0));
			else
				vdd->panel_attach_status |= BIT(0) << ctrl->ndx;
		}
	} else {
		if (ctrl->cmd_sync_wait_broadcast)
			vdd->panel_attach_status &= ~(BIT(1) | BIT(0));
		else {
			/* One more time check dual dsi */
			if (!IS_ERR_OR_NULL(vdd->ctrl_dsi[DSI_CTRL_0]) &&
				!IS_ERR_OR_NULL(vdd->ctrl_dsi[DSI_CTRL_1]))
				vdd->panel_attach_status &= ~(BIT(1) | BIT(0));
			else
				vdd->panel_attach_status &= ~(BIT(0) << ctrl->ndx);
		}
	}

	pr_info("%s panel_attach_status : %d\n", __func__, vdd->panel_attach_status);

	return vdd->panel_attach_status;
}

/*
 * Check the lcd id for DISPLAY_1 and DISPLAY_2 using the ndx
 */
int mdss_panel_attached(int ndx)
{
	int lcd_id = 0;

	/*
	 * ndx 0 means DISPLAY_1 and ndx 1 means DISPLAY_2
	 */
	if (ndx == 0)
		lcd_id = get_lcd_attached("GET");
	else if (ndx == 1)
		lcd_id = get_lcd_attached_secondary("GET");

	/*
	 * The 0xFFFFFF is the id for PBA booting
	 * if the id is same with 0xFFFFFF, this function
	 * will return 0
	 */
	return !(lcd_id == PBA_ID);
}

static int mdss_samsung_parse_panel_id(char *panel_id)
{
	char *pt;
	int lcd_id = 0;

	if (!IS_ERR_OR_NULL(panel_id))
		pt = panel_id;
	else
		return lcd_id;

	for (pt = panel_id; *pt != 0; pt++)  {
		lcd_id <<= 4;
		switch (*pt) {
		case '0' ... '9':
			lcd_id += *pt - '0';
			break;
		case 'a' ... 'f':
			lcd_id += 10 + *pt - 'a';
			break;
		case 'A' ... 'F':
			lcd_id += 10 + *pt - 'A';
			break;
		}
	}
	pr_err("%s: LCD_ID = 0x%X\n", __func__, lcd_id);

	return lcd_id;
}

int get_lcd_attached(char *mode)
{
	static int lcd_id = 0;

	pr_debug("%s: %s", __func__, mode);

	if (mode == NULL)
		return true;

	if (!strncmp(mode, "GET", 3)) {
		goto end;
	} else {
		lcd_id = 0;
		lcd_id = mdss_samsung_parse_panel_id(mode);
	}

end:
	return lcd_id;
}
EXPORT_SYMBOL(get_lcd_attached);
__setup("lcd_id=0x", get_lcd_attached);

int get_lcd_attached_secondary(char *mode)
{
	static int lcd_id = 0;

	pr_debug("%s: %s", __func__, mode);

	if (mode == NULL)
		return true;

	if (!strncmp(mode, "GET", 3))
		goto end;
	else
		lcd_id = mdss_samsung_parse_panel_id(mode);

end:
	return lcd_id;
}
EXPORT_SYMBOL(get_lcd_attached_secondary);
__setup("lcd_id2=0x", get_lcd_attached_secondary);

int get_hall_ic_status(char *mode)
{
	if (mode == NULL)
		return true;

	if (*mode - '0')
		vdd_data.display_status_dsi[DISPLAY_1].hall_ic_status = HALL_IC_CLOSE;
	else
		vdd_data.display_status_dsi[DISPLAY_1].hall_ic_status = HALL_IC_OPEN;

	pr_err("%s hall_ic : %s\n", __func__, vdd_data.display_status_dsi[DISPLAY_1].hall_ic_status ? "CLOSE" : "OPEN");

	return true;
}
EXPORT_SYMBOL(get_hall_ic_status);
__setup("hall_ic=0x", get_hall_ic_status);
static int samsung_nv_read(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *cmds, unsigned char *destBuffer)
{
	int loop_limit = 0;
	int read_pos = 0;
	int read_count = 0;
	int show_cnt;
	int i, j;
	char show_buffer[256] = {0,};
	int show_buffer_pos = 0;

	int read_size = 0;
	int srcLength = 0;
	int startoffset = 0;
	struct samsung_display_driver_data *vdd = NULL;
	int ndx = 0;
	int packet_size = 0;

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(cmds)) {
		pr_err("%s: Invalid ctrl data\n", __func__);
		return -EINVAL;
	}

	vdd = (struct samsung_display_driver_data *)ctrl->panel_data.panel_private;

	ndx = display_ndx_check(ctrl);

	packet_size = vdd->dtsi_data[ndx].packet_size_tx_cmds[vdd->panel_revision].cmds->payload[0];

	srcLength = cmds->read_size[0];
	startoffset = read_pos = cmds->read_startoffset[0];

	show_buffer_pos += snprintf(show_buffer, sizeof(show_buffer), "read_reg : %X[%d] : ", cmds->cmds->payload[0], srcLength);

	loop_limit = (srcLength + packet_size - 1) / packet_size;
	mdss_samsung_send_cmd(ctrl, PANEL_PACKET_SIZE);

	show_cnt = 0;
	for (j = 0; j < loop_limit; j++) {
		vdd->dtsi_data[ndx].reg_read_pos_tx_cmds[vdd->panel_revision].cmds->payload[1] = read_pos;
		read_size = ((srcLength - read_pos + startoffset) < packet_size) ?
					(srcLength - read_pos + startoffset) : packet_size;
		mdss_samsung_send_cmd(ctrl, PANEL_REG_READ_POS);

		mutex_lock(&vdd->vdd_lock);
		read_count = mdss_samsung_panel_cmd_read(ctrl, cmds, read_size);
		mutex_unlock(&vdd->vdd_lock);

		for (i = 0; i < read_count; i++, show_cnt++) {
			show_buffer_pos += snprintf(show_buffer + show_buffer_pos, sizeof(show_buffer)-show_buffer_pos, "%02x ",
					ctrl->rx_buf.data[i]);
			if (destBuffer != NULL && show_cnt < srcLength) {
					destBuffer[show_cnt] =
					ctrl->rx_buf.data[i];
			}
		}

		show_buffer_pos += snprintf(show_buffer + show_buffer_pos, sizeof(show_buffer)-show_buffer_pos, ".");
		read_pos += read_count;

		if (read_pos-startoffset >= srcLength)
			break;
	}

	pr_err("mdss DSI%d %s\n", ndx, show_buffer);

	return read_pos-startoffset;
}

static int mdss_samsung_read_nv_mem(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *cmds, unsigned char *buffer, int level_key)
{
	int nv_read_cnt = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)ctrl->panel_data.panel_private;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return 0;
	}

	if ((level_key & PANEL_LEVE1_KEY) && !IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].level1_key_enable_tx_cmds[vdd->panel_revision].cmds))
		mdss_samsung_send_cmd(ctrl, PANEL_LEVE1_KEY_ENABLE);
	if ((level_key & PANEL_LEVE2_KEY) && !IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].level2_key_enable_tx_cmds[vdd->panel_revision].cmds))
		mdss_samsung_send_cmd(ctrl, PANEL_LEVE2_KEY_ENABLE);

	nv_read_cnt = samsung_nv_read(ctrl, cmds, buffer);

	if ((level_key & PANEL_LEVE1_KEY) && !IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].level1_key_disable_tx_cmds[vdd->panel_revision].cmds))
		mdss_samsung_send_cmd(ctrl, PANEL_LEVE1_KEY_DISABLE);
	if ((level_key & PANEL_LEVE2_KEY) && !IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].level2_key_disable_tx_cmds[vdd->panel_revision].cmds))
		mdss_samsung_send_cmd(ctrl, PANEL_LEVE2_KEY_DISABLE);

	return nv_read_cnt;
}

static void mdss_samsung_event_frame_update(struct mdss_panel_data *pdata, int event, void *arg)
{
	int ndx;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)pdata->panel_private;
	struct panel_func *panel_func = NULL;
	char rddpm_reg = 0;
	struct mdss_dsi_ctrl_pdata *dsi_ctrl = mdss_dsi_get_ctrl(0);

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	panel_func = &vdd->panel_func;

	ndx = display_ndx_check(ctrl);

	if (ctrl->cmd_sync_wait_broadcast) {
		if (ctrl->cmd_sync_wait_trigger) {
			if (vdd->display_status_dsi[ndx].wait_disp_on) {
				ATRACE_BEGIN(__func__);
				mdss_samsung_send_cmd(ctrl, PANEL_DISPLAY_ON);
				vdd->display_status_dsi[ndx].wait_disp_on = 0;

				if (vdd->panel_func.samsung_backlight_late_on)
					vdd->panel_func.samsung_backlight_late_on(ctrl);

				if (vdd->dtsi_data[0].hmt_enabled) {
					if (!vdd->hmt_stat.hmt_is_first) {
						if (vdd->hmt_stat.hmt_on)
							vdd->hmt_stat.hmt_bright_update(ctrl);
					} else
						vdd->hmt_stat.hmt_is_first = 0;
				}
				LCD_INFO("DISPLAY_ON\n");
				ATRACE_END(__func__);
			}
		} else
			vdd->display_status_dsi[ndx].wait_disp_on = 0;
	} else {
		/* Check TE duration when the panel turned on */
		/*
		if (vdd->display_status_dsi[ctrl->ndx].wait_disp_on) {
			vdd->te_fitting_info.status &= ~TE_FITTING_DONE;
			vdd->te_fitting_info.te_duration = 0;
		}
		*/

		if (vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting &&
				!(vdd->te_fitting_info.status & TE_FITTING_DONE)) {
			if (panel_func->mdss_samsung_event_osc_te_fitting)
				panel_func->mdss_samsung_event_osc_te_fitting(pdata, event, arg);
		}

		if (vdd->display_status_dsi[ndx].wait_disp_on) {
			MDSS_XLOG(ndx);
			ATRACE_BEGIN(__func__);
			if (!IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].ldi_debug0_rx_cmds[vdd->panel_revision].cmds))
				mdss_samsung_read_nv_mem(&dsi_ctrl[DISPLAY_1], &vdd->dtsi_data[DISPLAY_1].ldi_debug0_rx_cmds[vdd->panel_revision],
					&rddpm_reg, PANEL_LEVE1_KEY);
			if (!IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].display_on_tx_cmds[vdd->panel_revision].cmds))
			mdss_samsung_send_cmd(ctrl, PANEL_DISPLAY_ON);
			vdd->display_status_dsi[ndx].wait_disp_on = 0;

			if (vdd->panel_func.samsung_backlight_late_on)
				vdd->panel_func.samsung_backlight_late_on(ctrl);

			if (vdd->dtsi_data[0].hmt_enabled) {
				if (!vdd->hmt_stat.hmt_is_first) {
					if (vdd->hmt_stat.hmt_on)
						vdd->hmt_stat.hmt_bright_update(ctrl);
				} else
					vdd->hmt_stat.hmt_is_first = 0;
			}
			MDSS_XLOG(ndx);
			LCD_INFO("DISPLAY_ON\n");
			ATRACE_END(__func__);
		}
	}
}

static void mdss_samsung_event_fb_event_callback(struct mdss_panel_data *pdata, int event, void *arg)
{
	struct samsung_display_driver_data *vdd = NULL;
	struct panel_func *panel_func = NULL;

	if (IS_ERR_OR_NULL(pdata)) {
		pr_err("%s: Invalid data pdata : 0x%zx\n",
				__func__, (size_t)pdata);
		return;
	}

	vdd = (struct samsung_display_driver_data *)pdata->panel_private;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx\n", __func__, (size_t)vdd);
		return;
	}

	panel_func = &vdd->panel_func;

	if (IS_ERR_OR_NULL(panel_func)) {
		pr_err("%s: Invalid data panel_func : 0x%zx\n",
				__func__, (size_t)panel_func);
		return;
	}

	if (panel_func->mdss_samsung_event_esd_recovery_init)
		panel_func->mdss_samsung_event_esd_recovery_init(pdata, event, arg);
}


static int mdss_samsung_dsi_panel_event_handler(
		struct mdss_panel_data *pdata, int event, void *arg)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)pdata->panel_private;
	struct panel_func *panel_func = NULL;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n",
				__func__, (size_t)ctrl, (size_t)vdd);
		return -EINVAL;
	}

	pr_debug("%s : %d\n", __func__, event);

	panel_func = &vdd->panel_func;

	if (IS_ERR_OR_NULL(panel_func)) {
		pr_err("%s: Invalid data panel_func : 0x%zx\n", __func__, (size_t)panel_func);
		return -EINVAL;
	}

	switch (event) {
		case MDSS_SAMSUNG_EVENT_FRAME_UPDATE:
			if (!IS_ERR_OR_NULL(panel_func->mdss_samsung_event_frame_update))
				panel_func->mdss_samsung_event_frame_update(pdata, event, arg);
			break;
		case MDSS_SAMSUNG_EVENT_FB_EVENT_CALLBACK:
			if (!IS_ERR_OR_NULL(panel_func->mdss_samsung_event_fb_event_callback))
				panel_func->mdss_samsung_event_fb_event_callback(pdata, event, arg);
			break;
		case MDSS_SAMSUNG_EVENT_ULP:
		case MDSS_SAMSUNG_EVENT_LP:
			mdss_samsung_panel_lpm_ctrl(pdata, event);
		default:
			pr_debug("%s: unhandled event=%d\n", __func__, event);
			break;
	}

	return 0;
}

void mdss_samsung_panel_init(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	/* At this time ctrl_pdata->ndx is not set */
	struct mdss_panel_info *pinfo = NULL;
	int ndx = ctrl_pdata->panel_data.panel_info.pdest;
	int loop, loop2;

	ctrl_pdata->panel_data.panel_private = &vdd_data;

	mutex_init(&vdd_data.vdd_lock);
	/* To guarantee BLANK & UNBLANK mode change operation*/
	mutex_init(&vdd_data.vdd_blank_unblank_lock);

	/* To guarantee ALPM ON or OFF mode change operation*/
	mutex_init(&vdd_data.vdd_panel_lpm_lock);

	vdd_data.ctrl_dsi[ndx] = ctrl_pdata;

	pinfo = &ctrl_pdata->panel_data.panel_info;

	/* Set default link_state of brightness command */
	for (loop = 0; loop < SUPPORT_PANEL_COUNT; loop++)
		vdd_data.brightness[loop].brightness_packet_tx_cmds_dsi.link_state = DSI_HS_MODE;

	if (pinfo && pinfo->mipi.mode == DSI_CMD_MODE) {
		vdd_data.panel_func.mdss_samsung_event_osc_te_fitting =
			mdss_samsung_event_osc_te_fitting;
	}

	vdd_data.panel_func.mdss_samsung_event_frame_update =
		mdss_samsung_event_frame_update;
	vdd_data.panel_func.mdss_samsung_event_fb_event_callback =
		mdss_samsung_event_fb_event_callback;
	vdd_data.panel_func.mdss_samsung_event_esd_recovery_init =
		mdss_samsung_event_esd_recovery_init;

	vdd_data.manufacture_id_dsi[ndx] = PBA_ID;

	if (IS_ERR_OR_NULL(vdd_data.panel_func.samsung_panel_init))
		pr_err("%s samsung_panel_init is error", __func__);
	else
		vdd_data.panel_func.samsung_panel_init(&vdd_data);

	vdd_data.ctrl_dsi[ndx]->event_handler = mdss_samsung_dsi_panel_event_handler;

	if (vdd_data.support_mdnie_lite || vdd_data.support_cabc)
		/* check the lcd id for DISPLAY_1 and DISPLAY_2 */
		if (((ndx == 0) && mdss_panel_attached(ndx)) ||
				((ndx == 1) && mdss_panel_attached(ndx)))
			vdd_data.mdnie_tune_state_dsi[ndx] = init_dsi_tcon_mdnie_class(ndx, &vdd_data);

	/*
		Below for loop are same as initializing sturct brightenss_pasket_dsi.
	vdd_data.brightness[ndx].brightness_packet_dsi[0] = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};
	vdd_data.brightness[ndx].brightness_packet_dsi[1] = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};
	...
	vdd_data.brightness[ndx].brightness_packet_dsi[BRIGHTNESS_MAX_PACKET - 2] = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};
	vdd_data.brightness[ndx].brightness_packet_dsi[BRIGHTNESS_MAX_PACKET - 1 ] = {{DTYPE_DCS_LWRITE, 1, 0, 0, 0, 0}, NULL};
	*/
	for (loop = 0; loop < SUPPORT_PANEL_COUNT; loop++)
		for (loop2 = 0; loop2 < BRIGHTNESS_MAX_PACKET; loop2++)
			vdd_data.brightness[loop].brightness_packet_dsi[loop2] = default_cmd;

	for (loop = 0; loop < SUPPORT_PANEL_COUNT; loop++) {
		vdd_data.brightness[loop].brightness_packet_tx_cmds_dsi.cmds = &vdd_data.brightness[loop].brightness_packet_dsi[0];
		vdd_data.brightness[loop].brightness_packet_tx_cmds_dsi.cmd_cnt = 0;
	}

	spin_lock_init(&vdd_data.esd_recovery.irq_lock);

	vdd_data.hmt_stat.hmt_enable = hmt_enable;
	vdd_data.hmt_stat.hmt_reverse_update = hmt_reverse_update;
	vdd_data.hmt_stat.hmt_bright_update = hmt_bright_update;

	mdss_panel_attach_set(ctrl_pdata, true);

	/* Set init brightness level */
	vdd_data.bl_level = DEFAULT_BRIGHTNESS;

	/* Init blank_mode */
	for (loop = 0; loop < SUPPORT_PANEL_COUNT; loop++)
		vdd_data.vdd_blank_mode[loop] = FB_BLANK_POWERDOWN;

	/* Init Hall ic related things */
	mutex_init(&vdd_data.vdd_hall_ic_lock); /* To guarantee HALL IC switching */
	mutex_init(&vdd_data.vdd_hall_ic_blank_unblank_lock); /* To guarantee HALL IC operation(PMS BLANK & UNBLAMK) */

	/* Init bl_level related things */
	mutex_init(&vdd_data.vdd_bl_level_lock); /* To guarantee update bl_level at mdss_dsi_panel.c & mdss_samsung_panel_on_post() */

	/*init image dump queue */
	if (IS_ERR_OR_NULL(vdd_data.image_dump_workqueue)) {
		vdd_data.image_dump_workqueue = create_singlethread_workqueue("image_dump_workqueue");
		INIT_WORK(&vdd_data.image_dump_work, samsung_image_dump_worker);
	}

	/* Init Other line panel support */
	if (vdd_data.other_line_panel_support) {
		INIT_DELAYED_WORK(&vdd_data.other_line_panel_support_work, (work_func_t)other_read_panel_data_work_fn);
		vdd_data.other_line_panel_support_workq = create_singlethread_workqueue("other_line_panel_support_wq");

		if (vdd_data.other_line_panel_support_workq) {
			vdd_data.other_line_panel_work_cnt = 50;
			queue_delayed_work(vdd_data.other_line_panel_support_workq,
					&vdd_data.other_line_panel_support_work, msecs_to_jiffies(500));
		}
	}

	/* Init panel LPM status */
	if (!IS_ERR_OR_NULL(vdd_data.panel_func.samsung_get_panel_lpm_mode)) {
		vdd_data.panel_lpm.param_changed = true;
		vdd_data.panel_lpm.mode = MAX_LPM_MODE + ALPM_MODE_ON;
		vdd_data.panel_func.samsung_get_panel_lpm_mode(ctrl_pdata, &vdd_data.panel_lpm.mode);
	}
}

void mdss_samsung_dsi_panel_registered(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return;
	}

	if ((struct msm_fb_data_type *)registered_fb[ctrl->ndx]) {
		vdd->mfd_dsi[ctrl->ndx] = (struct msm_fb_data_type *)registered_fb[ctrl->ndx]->par;
	} else {
		pr_err("%s : no registered_fb[%d]\n", __func__, ctrl->ndx);
		return;
	}

	mdss_samsung_create_sysfs(vdd);
	pr_info("%s DSI%d success", __func__, ctrl->ndx);
}

int mdss_samsung_parse_candella_lux_mapping_table(struct device_node *np,
		struct candella_lux_map *table, char *keystring)
{
	const __be32 *data;
	int data_offset, len = 0 , i = 0;
	int cdmap_start = 0, cdmap_end = 0;

	data = of_get_property(np, keystring, &len);
	if (!data) {
		pr_debug("%s:%d, Unable to read table %s ", __func__, __LINE__, keystring);
		return -EINVAL;
	} else
		pr_err("%s:Success to read table %s\n", __func__, keystring);

	if ((len % 4) != 0) {
		pr_err("%s:%d, Incorrect table entries for %s",
					__func__, __LINE__, keystring);
		return -EINVAL;
	}

	table->lux_tab_size = len / (sizeof(int)*4);
	table->lux_tab = kzalloc((sizeof(int) * table->lux_tab_size), GFP_KERNEL);
	if (!table->lux_tab)
		return -ENOMEM;

	table->cmd_idx = kzalloc((sizeof(int) * table->lux_tab_size), GFP_KERNEL);
	if (!table->cmd_idx)
		goto error;

	data_offset = 0;
	for (i = 0 ; i < table->lux_tab_size; i++) {
		table->cmd_idx[i] = be32_to_cpup(&data[data_offset++]);	/* 1rst field => <idx> */
		cdmap_start = be32_to_cpup(&data[data_offset++]);		/* 2nd field => <from> */
		cdmap_end = be32_to_cpup(&data[data_offset++]);			/* 3rd field => <till> */
		table->lux_tab[i] = be32_to_cpup(&data[data_offset++]);	/* 4th field => <candella> */

		table->bkl_subdivision[cdmap_start] = i;

		/* Fill the backlight level to lux mapping array */
		do {
			if (i+1 == table->lux_tab_size)
				table->bkl_subdivision[cdmap_start+1] = i;
			else
				table->bkl_subdivision[cdmap_start+1] = i+1;
			table->bkl[cdmap_start++] = i;
		} while (cdmap_start <= cdmap_end);
	}
	return 0;
error:
	kfree(table->lux_tab);

	return -ENOMEM;
}

int mdss_samsung_parse_panel_table(struct device_node *np,
		struct cmd_map *table, char *keystring)
{
	const __be32 *data;
	int  data_offset, len = 0 , i = 0;

	data = of_get_property(np, keystring, &len);
	if (!data) {
		pr_debug("%s:%d, Unable to read table %s ", __func__, __LINE__, keystring);
		return -EINVAL;
	} else
		pr_err("%s:Success to read table %s\n", __func__, keystring);

	if ((len % 2) != 0) {
		pr_err("%s:%d, Incorrect table entries for %s",
					__func__, __LINE__, keystring);
		return -EINVAL;
	}

	table->size = len / (sizeof(int)*2);
	table->bl_level = kzalloc((sizeof(int) * table->size), GFP_KERNEL);
	if (!table->bl_level)
		return -ENOMEM;

	table->cmd_idx = kzalloc((sizeof(int) * table->size), GFP_KERNEL);
	if (!table->cmd_idx)
		goto error;

	data_offset = 0;
	for (i = 0 ; i < table->size; i++) {
		table->bl_level[i] = be32_to_cpup(&data[data_offset++]);
		table->cmd_idx[i] = be32_to_cpup(&data[data_offset++]);
	}

	return 0;
error:
	kfree(table->cmd_idx);

	return -ENOMEM;
}

struct dsi_panel_cmds *mdss_samsung_cmds_select(struct mdss_dsi_ctrl_pdata *ctrl, enum mipi_samsung_cmd_list cmd)
{
	int ndx;
	struct dsi_panel_cmds *cmds = NULL;
	struct mdss_panel_info *pinfo = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)ctrl->panel_data.panel_private;

	if (IS_ERR_OR_NULL(vdd))
		return NULL;

	ndx = display_ndx_check(ctrl);

	pinfo = &(ctrl->panel_data.panel_info);
	/*
	 * Skip commands that are not related to ALPM
	 */
	if (pinfo->blank_state == MDSS_PANEL_BLANK_LOW_POWER) {
		switch (cmd) {
			case PANEL_LPM_ON:
			case PANEL_LPM_OFF:
			case PANEL_LPM_HZ_NONE:
			case PANEL_LPM_1HZ:
			case PANEL_LPM_2HZ:
			case PANEL_LPM_30HZ:
			case PANEL_LPM_AOD_ON:
			case PANEL_LPM_AOD_OFF:
			case PANEL_DISPLAY_ON:
			case PANEL_DISPLAY_OFF:
			case PANEL_LEVE1_KEY_ENABLE:
			case PANEL_LEVE1_KEY_DISABLE:
			case PANEL_LEVE2_KEY_ENABLE:
			case PANEL_LEVE2_KEY_DISABLE:
			case PANEL_LEVE1_KEY:
			case PANEL_LEVE2_KEY:
				break;
			default:
				LCD_INFO("Skip commands that are not related to ALPM\n");
				goto end;
		}
	}

	switch (cmd) {
		case PANEL_DISPLAY_ON:
			cmds = &vdd->dtsi_data[ndx].display_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_DISPLAY_OFF:
			cmds = &vdd->dtsi_data[ndx].display_off_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_BRIGHT_CTRL:
			cmds = &vdd->brightness[ndx].brightness_packet_tx_cmds_dsi;
			break;
		case PANEL_LEVE1_KEY_ENABLE:
			cmds = &vdd->dtsi_data[ndx].level1_key_enable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LEVE1_KEY_DISABLE:
			cmds = &vdd->dtsi_data[ndx].level1_key_disable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LEVE2_KEY_ENABLE:
			cmds = &vdd->dtsi_data[ndx].level2_key_enable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LEVE2_KEY_DISABLE:
			cmds = &vdd->dtsi_data[ndx].level2_key_disable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_PACKET_SIZE:
			cmds = &vdd->dtsi_data[ndx].packet_size_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_REG_READ_POS:
			cmds = &vdd->dtsi_data[ndx].reg_read_pos_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_MDNIE_TUNE:
			cmds = &vdd->mdnie_tune_data[ndx].mdnie_tune_packet_tx_cmds_dsi;
			break;
		case PANEL_OSC_TE_FITTING:
			cmds = &vdd->dtsi_data[ndx].osc_te_fitting_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_AVC_ON:
			cmds = &vdd->dtsi_data[ndx].avc_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LDI_FPS_CHANGE:
			cmds = &vdd->dtsi_data[ndx].ldi_fps_change_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HMT_ENABLE:
			cmds = &vdd->dtsi_data[ndx].hmt_enable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HMT_DISABLE:
			cmds = &vdd->dtsi_data[ndx].hmt_disable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HMT_REVERSE:
			cmds = &vdd->dtsi_data[ndx].hmt_reverse_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HMT_FORWARD:
			cmds = &vdd->dtsi_data[ndx].hmt_forward_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LDI_SET_VDD_OFFSET:
			cmds = &vdd->dtsi_data[ndx].write_vdd_offset_cmds[vdd->panel_revision];
			break;
		case PANEL_LDI_SET_VDDM_OFFSET:
			cmds = &vdd->dtsi_data[ndx].write_vddm_offset_cmds[vdd->panel_revision];
			break;
		case PANEL_LPM_ON:
			cmds = &vdd->dtsi_data[ndx].alpm_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LPM_OFF:
			cmds = &vdd->dtsi_data[ndx].alpm_off_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LPM_HZ_NONE:
			cmds = &vdd->dtsi_data[ndx].lpm_hz_none_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LPM_1HZ:
			cmds = &vdd->dtsi_data[ndx].lpm_1hz_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LPM_2HZ:
			cmds = &vdd->dtsi_data[ndx].lpm_2hz_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LPM_30HZ:
			cmds = &vdd->dtsi_data[ndx].lpm_hz_none_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LPM_AOD_ON:
			cmds = &vdd->dtsi_data[ndx].lpm_aod_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_LPM_AOD_OFF:
			cmds = &vdd->dtsi_data[ndx].lpm_aod_off_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_HSYNC_ON:
			cmds = &vdd->dtsi_data[ndx].hsync_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_CABC_ON:
			cmds = &vdd->dtsi_data[ndx].cabc_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_CABC_OFF:
			cmds = &vdd->dtsi_data[ndx].cabc_off_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_BLIC_DIMMING:
			cmds = &vdd->dtsi_data[ndx].blic_dimming_cmds[vdd->panel_revision];
			break;
		case PANEL_CABC_ON_DUTY:
			cmds = &vdd->dtsi_data[ndx].cabc_on_duty_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_CABC_OFF_DUTY:
			cmds = &vdd->dtsi_data[ndx].cabc_off_duty_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_SPI_ENABLE:
			cmds = &vdd->dtsi_data[ndx].spi_enable_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_COLOR_WEAKNESS_ENABLE:
			cmds = &vdd->dtsi_data[ndx].ccb_on_tx_cmds[vdd->panel_revision];
			break;
		case PANEL_COLOR_WEAKNESS_DISABLE:
			cmds = &vdd->dtsi_data[ndx].ccb_off_tx_cmds[vdd->panel_revision];
			break;
		default:
			pr_err("%s : unknown_command(%d)..\n", __func__, cmd);
			break;
	}

end:
	return cmds;
}

int mdss_samsung_send_cmd(struct mdss_dsi_ctrl_pdata *ctrl, enum mipi_samsung_cmd_list cmd)
{
	struct mdss_panel_info *pinfo = NULL;
	struct samsung_display_driver_data *vdd = NULL;
	struct dsi_panel_cmds *pcmds = NULL;

	if (!mdss_panel_attach_get(ctrl)) {
		pr_err("%s: mdss_panel_attach_get(%d) : %d\n",
				__func__, ctrl->ndx, mdss_panel_attach_get(ctrl));
		return -EAGAIN;
	}

	vdd = (struct samsung_display_driver_data *)ctrl->panel_data.panel_private;
	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Skip to tx command %d\n", __func__, __LINE__);
		return 0;
	}

	pinfo = &(ctrl->panel_data.panel_info);
	if ((pinfo->blank_state == MDSS_PANEL_BLANK_BLANK) || (vdd->panel_func.samsung_lvds_write_reg)) {
		pr_err("%s: Skip to tx command %d\n", __func__, __LINE__);
		return 0;
	}

	mutex_lock(&vdd->vdd_blank_unblank_lock); /* To block blank & unblank operation while sending cmds */

	/* To check registered FB */
	if (IS_ERR_OR_NULL(vdd->mfd_dsi[ctrl->ndx])) {
		/* Do not send any CMD data under FB_BLANK_POWERDOWN condition*/
		if (vdd->vdd_blank_mode[DISPLAY_1] == FB_BLANK_POWERDOWN) {
			mutex_unlock(&vdd->vdd_blank_unblank_lock); /* To block blank & unblank operation while sending cmds */
			pr_err("%s: Skip to tx command %d\n", __func__, __LINE__);
			return 0;
		}
	} else {
		/* Do not send any CMD data under FB_BLANK_POWERDOWN condition*/
		if (vdd->vdd_blank_mode[ctrl->ndx] == FB_BLANK_POWERDOWN) {
			mutex_unlock(&vdd->vdd_blank_unblank_lock); /* To block blank & unblank operation while sending cmds*/
			pr_err("%s: Skip to tx command %d\n", __func__, __LINE__);
			return 0;
		}
	}

	mutex_lock(&vdd->vdd_lock);
	pcmds = mdss_samsung_cmds_select(ctrl, cmd);
	if (!IS_ERR_OR_NULL(pcmds) && !IS_ERR_OR_NULL(pcmds->cmds))
		mdss_dsi_panel_cmds_send(ctrl, pcmds);
	else
		LCD_INFO("Fail to tx cmds(line : %d)\n", __LINE__);
	mutex_unlock(&vdd->vdd_lock);

	mutex_unlock(&vdd->vdd_blank_unblank_lock); /* To block blank & unblank operation while sending cmds */

	return 0;
}

void mdss_samsung_panel_data_read(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *cmds, char *buffer, int level_key)
{
	if (!ctrl) {
		pr_err("%s: Invalid ctrl data\n", __func__);
		return;
	}

	if (!mdss_panel_attach_get(ctrl)) {
		pr_err("%s: mdss_panel_attach_get(%d) : %d\n",
				__func__, ctrl->ndx, mdss_panel_attach_get(ctrl));
		return;
	}

	if (!cmds->cmd_cnt) {
		pr_err("%s : cmds_count is zero..\n", __func__);
		return;
	}

	mdss_samsung_read_nv_mem(ctrl, cmds, buffer, level_key);
}

static unsigned char mdss_samsung_manufacture_id(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *cmds)
{
	char manufacture_id = 0;

	mdss_samsung_panel_data_read(ctrl, cmds, &manufacture_id, PANEL_LEVE1_KEY);

	return manufacture_id;
}

static int mdss_samsung_manufacture_id_full(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_panel_cmds *cmds)
{
	char manufacture_id[3];
	int ret = 0;

	mdss_samsung_panel_data_read(ctrl, cmds, manufacture_id, PANEL_LEVE1_KEY);

	ret |= (manufacture_id[0] << 16);
	ret |= (manufacture_id[1] << 8);
	ret |= (manufacture_id[2]);

	return ret;
}

int mdss_samsung_panel_on_pre(struct mdss_panel_data *pdata)
{
	int ndx;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	ndx = display_ndx_check(ctrl);

	vdd->display_status_dsi[ndx].disp_on_pre = 1;

#if !defined(CONFIG_SEC_FACTORY)
	/* LCD ID read every wake_up time incase of factory binary */
	if (vdd->dtsi_data[ndx].tft_common_support)
		return false;
#endif

	if (!mdss_panel_attach_get(ctrl)) {
		pr_err("%s: mdss_panel_attach_get(%d) : %d\n",
				__func__, ndx, mdss_panel_attach_get(ctrl));
		return false;
	}

	pr_info("%s+: ndx=%d\n", __func__, ndx);

#if defined(CONFIG_SEC_FACTORY)
	/* LCD ID read every wake_up time incase of factory binary */
	vdd->manufacture_id_dsi[ndx] = PBA_ID;

	/* Factory Panel Swap*/
	if (vdd->dtsi_data[ndx].samsung_support_factory_panel_swap) {
		vdd->manufacture_date_loaded_dsi[ndx] = 0;
		vdd->ddi_id_loaded_dsi[ndx] = 0;
		vdd->hbm_loaded_dsi[ndx] = 0;
		vdd->mdnie_loaded_dsi[ndx] = 0;
		vdd->smart_dimming_loaded_dsi[ndx] = 0;
		vdd->smart_dimming_hmt_loaded_dsi[ndx] = 0;
	}
#endif

	if (vdd->manufacture_id_dsi[ndx] == PBA_ID) {
		/*
		*	At this time, panel revision it not selected.
		*	So last index(SUPPORT_PANEL_REVISION-1) used.
		*/
		vdd->panel_revision = SUPPORT_PANEL_REVISION-1;

		/*
		*	Some panel needs to update register at init time to read ID & MTP
		*	Such as, dual-dsi-control or sleep-out so on.
		*/

		if (!IS_ERR_OR_NULL(vdd->dtsi_data[ndx].manufacture_id_rx_cmds[vdd->panel_revision].cmds)) {
			vdd->manufacture_id_dsi[ndx] = mdss_samsung_manufacture_id_full(ctrl, &vdd->dtsi_data[ndx].manufacture_id_rx_cmds[SUPPORT_PANEL_REVISION-1]);
			pr_info("%s: DSI%d manufacture_id(04h)=0x%x\n", __func__, ndx, vdd->manufacture_id_dsi[ndx]);
		} else {
			if (!IS_ERR_OR_NULL(vdd->dtsi_data[ndx].manufacture_read_pre_tx_cmds[vdd->panel_revision].cmds)) {
				mutex_lock(&vdd->vdd_lock);
				mdss_dsi_panel_cmds_send(ctrl, &vdd->dtsi_data[ndx].manufacture_read_pre_tx_cmds[vdd->panel_revision]);
				mutex_unlock(&vdd->vdd_lock);
				pr_err("%s DSI%d manufacture_read_pre_tx_cmds ", __func__, ndx);
			}

			if (!IS_ERR_OR_NULL(vdd->dtsi_data[ndx].manufacture_id0_rx_cmds[vdd->panel_revision].cmds)) {
				vdd->manufacture_id_dsi[ndx] = mdss_samsung_manufacture_id(ctrl, &vdd->dtsi_data[ndx].manufacture_id0_rx_cmds[SUPPORT_PANEL_REVISION-1]);
				vdd->manufacture_id_dsi[ndx] <<= 8;
			} else
				pr_err("%s DSI%d manufacture_id0_rx_cmds NULL", __func__, ndx);

			if (!IS_ERR_OR_NULL(vdd->dtsi_data[ndx].manufacture_id1_rx_cmds[vdd->panel_revision].cmds)) {
				vdd->manufacture_id_dsi[ndx] |= mdss_samsung_manufacture_id(ctrl, &vdd->dtsi_data[ndx].manufacture_id1_rx_cmds[SUPPORT_PANEL_REVISION-1]);
				vdd->manufacture_id_dsi[ndx] <<= 8;
			} else
				pr_err("%s DSI%d manufacture_id1_rx_cmds NULL", __func__, ndx);

			if (!IS_ERR_OR_NULL(vdd->dtsi_data[ndx].manufacture_id2_rx_cmds[vdd->panel_revision].cmds))
				vdd->manufacture_id_dsi[ndx] |= mdss_samsung_manufacture_id(ctrl, &vdd->dtsi_data[ndx].manufacture_id2_rx_cmds[SUPPORT_PANEL_REVISION-1]);
			else
				pr_err("%s DSI%d manufacture_id2_rx_cmds NULL", __func__, ndx);

			pr_info("%s: DSI%d manufacture_id=0x%x\n", __func__, ndx, vdd->manufacture_id_dsi[ndx]);
		}

		/* Panel revision selection */
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_revision))
			pr_err("%s: DSI%d panel_revision_selection_error\n", __func__, ndx);
		else
			vdd->panel_func.samsung_panel_revision(ctrl);

		pr_info("%s: DSI%d Panel_Revision = %c %d\n", __func__, ndx, vdd->panel_revision + 'A', vdd->panel_revision);
	}

	/* Manufacture date */
	if (!vdd->manufacture_date_loaded_dsi[ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_manufacture_date_read))
			pr_err("%s: DSI%d no samsung_manufacture_date_read function\n", __func__, ndx);
		else
			vdd->manufacture_date_loaded_dsi[ndx] = vdd->panel_func.samsung_manufacture_date_read(ctrl);
	}

	/* DDI ID */
	if (!vdd->ddi_id_loaded_dsi[ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_ddi_id_read))
			pr_err("%s: DSI%d no samsung_ddi_id_read function\n", __func__, ndx);
		else
			vdd->ddi_id_loaded_dsi[ndx] = vdd->panel_func.samsung_ddi_id_read(ctrl);
	}

	/* Panel Unique Cell ID */
	if (!vdd->cell_id_loaded_dsi[ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_cell_id_read))
			pr_err("%s: DSI%d no samsung_cell_id_read function\n", __func__, ndx);
		else
			vdd->cell_id_loaded_dsi[ndx] = vdd->panel_func.samsung_cell_id_read(ctrl);
	}

	/* ELVSS read */
	if (!vdd->elvss_loaded_dsi[ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_elvss_read))
			pr_err("%s: DSI%d no samsung_elvss_read function\n", __func__, ndx);
		else
			vdd->elvss_loaded_dsi[ndx] = vdd->panel_func.samsung_elvss_read(ctrl);
	}

	/* HBM */
	if (!vdd->hbm_loaded_dsi[ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_hbm_read))
			pr_err("%s: DSI%d no samsung_hbm_read function\n", __func__, ndx);
		else
			vdd->hbm_loaded_dsi[ndx] = vdd->panel_func.samsung_hbm_read(ctrl);
	}

	/* MDNIE X,Y */
	if (!vdd->mdnie_loaded_dsi[ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_mdnie_read))
			pr_err("%s: DSI%d no samsung_mdnie_read function\n", __func__, ndx);
		else
			vdd->mdnie_loaded_dsi[ndx] = vdd->panel_func.samsung_mdnie_read(ctrl);
	}

	/* Smart dimming*/
	if (!vdd->smart_dimming_loaded_dsi[ndx]) {
		if (IS_ERR_OR_NULL(vdd->panel_func.samsung_smart_dimming_init))
			pr_err("%s: DSI%d no samsung_smart_dimming_init function\n", __func__, ndx);
		else
			vdd->smart_dimming_loaded_dsi[ndx] = vdd->panel_func.samsung_smart_dimming_init(ctrl);
	}

	/* Smart dimming for hmt */
	if (vdd->dtsi_data[0].hmt_enabled) {
		if (!vdd->smart_dimming_hmt_loaded_dsi[ndx]) {
			if (IS_ERR_OR_NULL(vdd->panel_func.samsung_smart_dimming_hmt_init))
				pr_err("%s: DSI%d no samsung_smart_dimming_hmt_init function\n", __func__, ndx);
			else
				vdd->smart_dimming_hmt_loaded_dsi[ndx] = vdd->panel_func.samsung_smart_dimming_hmt_init(ctrl);
		}
	}

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_on_pre))
		vdd->panel_func.samsung_panel_on_pre(ctrl);

	pr_info("%s-: ndx=%d\n", __func__, ndx);

	return true;
}

int mdss_samsung_panel_on_post(struct mdss_panel_data *pdata)
{
	int ndx;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = NULL;
	struct mdss_panel_info *pinfo = NULL;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(ctrl)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	if (!mdss_panel_attach_get(ctrl)) {
		pr_err("%s: mdss_panel_attach_get(%d) : %d\n",
				__func__, ctrl->ndx, mdss_panel_attach_get(ctrl));
		return false;
	}

	pinfo = &(ctrl->panel_data.panel_info);

	if (IS_ERR_OR_NULL(pinfo)) {
		pr_err("%s: Invalid data pinfo : 0x%zx\n", __func__,  (size_t)pinfo);
		return false;
	}

	ndx = display_ndx_check(ctrl);

	pr_info("%s+: ndx=%d\n", __func__, ndx);

	MDSS_XLOG(ndx);

	mdss_samsung_read_loading_detection();

	if (vdd->support_cabc && !vdd->auto_brightness)
		mdss_samsung_cabc_update();
	else if (vdd->mdss_panel_tft_outdoormode_update && vdd->auto_brightness)
		vdd->mdss_panel_tft_outdoormode_update(ctrl);
	else if (vdd->support_cabc && vdd->auto_brightness)
		mdss_tft_autobrightness_cabc_update(ctrl);

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_on_post))
		vdd->panel_func.samsung_panel_on_post(ctrl);

	/* Recovery Mode : Set some default brightness */
	if (vdd->recovery_boot_mode)
		vdd->bl_level = DEFAULT_BRIGHTNESS;

	mutex_lock(&vdd->vdd_bl_level_lock);

	if (vdd->support_hall_ic) {
		/*
		* Brightenss cmds sent by samsung_display_hall_ic_status() at panel switching operation
		*/
		if ((vdd->ctrl_dsi[DISPLAY_1]->bklt_ctrl == BL_DCS_CMD) &&
			(vdd->display_status_dsi[DISPLAY_1].hall_ic_mode_change_trigger == false))
			mdss_samsung_brightness_dcs(ctrl, vdd->bl_level);
	} else {
		if ((vdd->ctrl_dsi[DISPLAY_1]->bklt_ctrl == BL_DCS_CMD))
			mdss_samsung_brightness_dcs(ctrl, vdd->bl_level);
	}

	mutex_unlock(&vdd->vdd_bl_level_lock);

	if (vdd->support_mdnie_lite)
		update_dsi_tcon_mdnie_register(vdd);

	vdd->display_status_dsi[ndx].wait_disp_on = true;

	if (pinfo->esd_check_enabled) {
		vdd->esd_recovery.esd_irq_enable(true, true, (void *)vdd);
		vdd->esd_recovery.is_enabled_esd_recovery = true;
	}

	pr_info("%s-: ndx=%d\n", __func__, ndx);

	MDSS_XLOG(ndx);

	return true;
}

int mdss_samsung_panel_off_pre(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__,
				(size_t)ctrl, (size_t)vdd);
		return false;
	}

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_off_pre))
		vdd->panel_func.samsung_panel_off_pre(ctrl);

	return ret;
}

int mdss_samsung_panel_off_post(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)pdata->panel_private;
	struct mdss_panel_info *pinfo = NULL;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__,
				(size_t)ctrl, (size_t)vdd);
		return false;
	}

	pinfo = &(ctrl->panel_data.panel_info);

	if (IS_ERR_OR_NULL(pinfo)) {
		pr_err("%s: Invalid data pinfo : 0x%zx\n", __func__,  (size_t)pinfo);
		return false;
	}

	if (pinfo->esd_check_enabled) {
		vdd->esd_recovery.esd_irq_enable(false, true, (void *)vdd);
		vdd->esd_recovery.is_enabled_esd_recovery = false;
	}

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_panel_off_post))
		vdd->panel_func.samsung_panel_off_post(ctrl);

	return ret;
}

/*************************************************************
*
*		EXTRA POWER RELATED FUNCTION BELOW.
*
**************************************************************/
static int mdss_dsi_extra_power_request_gpios(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0, i;
	/*
	 * gpio_name[] named as gpio_name + num(recomend as 0)
	 * because of the num will increase depend on number of gpio
	 */
	static const char gpio_name[] = "panel_extra_power";
	static u8 gpio_request_status = -EINVAL;
	struct samsung_display_driver_data *vdd = NULL;

	if (!gpio_request_status)
		goto end;

	if (!IS_ERR_OR_NULL(ctrl))
		vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data pinfo : 0x%zx\n", __func__,  (size_t)vdd);
		goto end;
	}

	for (i = 0; i < MAX_EXTRA_POWER_GPIO; i++) {
		if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i]) && mdss_panel_attach_get(ctrl)) {
			rc = gpio_request(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i],
							gpio_name);
			if (rc) {
				pr_err("request %s failed, rc=%d\n", gpio_name, rc);
				goto extra_power_gpio_err;
			}
		}
	}

	gpio_request_status = rc;
end:
	return rc;
extra_power_gpio_err:
	if (i) {
		do {
			if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i]))
				gpio_free(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i--]);
			pr_err("%s : i = %d\n", __func__, i);
		} while (i > 0);
	}

	return rc;
}

int mdss_samsung_panel_extra_power(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	int ret = 0, i = 0, add_value = 1;
	struct samsung_display_driver_data *vdd = NULL;

	if (IS_ERR_OR_NULL(pdata)) {
		pr_err("%s: Invalid pdata : 0x%zx\n", __func__, (size_t)pdata);
		ret = -EINVAL;
		goto end;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
						panel_data);

	if (!IS_ERR_OR_NULL(ctrl))
		vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data pinfo : 0x%zx\n", __func__,  (size_t)vdd);
		goto end;
	}

	pr_info("%s: ++ enable(%d) ndx(%d)\n",
			__func__, enable, ctrl->ndx);

	if (ctrl->ndx == DSI_CTRL_1)
		goto end;

	if (mdss_dsi_extra_power_request_gpios(ctrl)) {
		pr_err("%s : fail to request extra power gpios", __func__);
		goto end;
	}

	pr_debug("%s : %s extra power gpios\n", __func__, enable ? "enable" : "disable");

	/*
	 * The order of extra_power_gpio enable/disable
	 * 1. Enable : panel_extra_power_gpio[0], [1], ... [MAX_EXTRA_POWER_GPIO - 1]
	 * 2. Disable : panel_extra_power_gpio[MAX_EXTRA_POWER_GPIO - 1], ... [1], [0]
	 */
	if (!enable) {
		add_value = -1;
		i = MAX_EXTRA_POWER_GPIO - 1;
	}

	do {
		if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i]) && mdss_panel_attach_get(ctrl)) {
			gpio_set_value(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i], enable);
			pr_debug("%s : set extra power gpio[%d] to %s\n",
						 __func__, vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i],
						enable ? "high" : "low");
			usleep_range(1500, 1500);
		}
	} while (((i += add_value) < MAX_EXTRA_POWER_GPIO) && (i >= 0));

end:
	pr_info("%s: --\n", __func__);
	return ret;
}
/*************************************************************
*
*		TFT BACKLIGHT GPIO FUNCTION BELOW.
*
**************************************************************/
int mdss_backlight_tft_request_gpios(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0, i;
	/*
	 * gpio_name[] named as gpio_name + num(recomend as 0)
	 * because of the num will increase depend on number of gpio
	 */
	char gpio_name[17] = "disp_bcklt_gpio0";
	static u8 gpio_request_status = -EINVAL;
	struct samsung_display_driver_data *vdd = NULL;

	if (!gpio_request_status)
		goto end;

	if (!IS_ERR_OR_NULL(ctrl))
		vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data pinfo : 0x%zx\n", __func__,  (size_t)vdd);
		goto end;
	}

	for (i = 0; i < MAX_BACKLIGHT_TFT_GPIO; i++) {
		if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i])) {
			rc = gpio_request(vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i],
							gpio_name);
			if (rc) {
				pr_err("request %s failed, rc=%d\n", gpio_name, rc);
				goto tft_backlight_gpio_err;
			}
		}
	}

	gpio_request_status = rc;
end:
	return rc;
tft_backlight_gpio_err:
	if (i) {
		do {
			if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i]))
				gpio_free(vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i--]);
			pr_err("%s : i = %d\n", __func__, i);
		} while (i > 0);
	}

	return rc;
}
int mdss_backlight_tft_gpio_config(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	int ret = 0, i = 0, add_value = 1;
	struct samsung_display_driver_data *vdd = NULL;

	if (IS_ERR_OR_NULL(pdata)) {
		pr_err("%s: Invalid pdata : 0x%zx\n", __func__, (size_t)pdata);
		ret = -EINVAL;
		goto end;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
						panel_data);

	if (!IS_ERR_OR_NULL(ctrl))
		vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data pinfo : 0x%zx\n", __func__,  (size_t)vdd);
		goto end;
	}

	pr_info("%s: ++ enable(%d) ndx(%d)\n",
			__func__, enable, ctrl->ndx);

	if (ctrl->ndx == DSI_CTRL_1)
		goto end;

	if (mdss_backlight_tft_request_gpios(ctrl)) {
		pr_err("%s : fail to request tft backlight gpios", __func__);
		goto end;
	}

	pr_debug("%s : %s tft backlight gpios\n", __func__, enable ? "enable" : "disable");

	/*
	 * The order of backlight_tft_gpio enable/disable
	 * 1. Enable : backlight_tft_gpio[0], [1], ... [MAX_BACKLIGHT_TFT_GPIO - 1]
	 * 2. Disable : backlight_tft_gpio[MAX_BACKLIGHT_TFT_GPIO - 1], ... [1], [0]
	 */
	if (!enable) {
		add_value = -1;
		i = MAX_BACKLIGHT_TFT_GPIO - 1;
	}

	do {
		if (gpio_is_valid(vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i])) {
			gpio_set_value(vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i], enable);
			pr_debug("%s : set backlight tft gpio[%d] to %s\n",
						 __func__, vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i],
						enable ? "high" : "low");
			usleep_range(500, 500);
		}
	} while (((i += add_value) < MAX_BACKLIGHT_TFT_GPIO) && (i >= 0));

end:
	pr_info("%s: --\n", __func__);
	return ret;
}

/*************************************************************
*
*		ESD RECOVERY RELATED FUNCTION BELOW.
*
**************************************************************/

static int mdss_dsi_esd_irq_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	pr_info("%s: lcd esd recovery\n", __func__);

	return !ctrl->status_value;
}

/*
 * esd_irq_enable() - Enable or disable esd irq.
 *
 * @enable	: flag for enable or disabled
 * @nosync	: flag for disable irq with nosync
 * @data	: point ot struct mdss_panel_info
 */
static void esd_irq_enable(bool enable, bool nosync, void *data)
{
	/* The irq will enabled when do the request_threaded_irq() */
	static bool is_enabled = true;
	int gpio;
	unsigned long flags;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)data;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx\n", __func__, (size_t)vdd);
		return;
	}

	spin_lock_irqsave(&vdd->esd_recovery.irq_lock, flags);
	gpio = vdd->esd_recovery.esd_gpio;

	if (enable == is_enabled) {
		pr_info("%s: ESD irq already %s\n",
				__func__, enable ? "enabled" : "disabled");
		goto error;
	}

	if (enable) {
		is_enabled = true;
		enable_irq(gpio_to_irq(gpio));
	} else {
		if (nosync)
			disable_irq_nosync(gpio_to_irq(gpio));
		else
			disable_irq(gpio_to_irq(gpio));
		is_enabled = false;
	}

	/* TODO: Disable log if the esd function stable */
	pr_debug("%s: ESD irq %s with %s\n",
				__func__,
				enable ? "enabled" : "disabled",
				nosync ? "nosync" : "sync");
error:
	spin_unlock_irqrestore(&vdd->esd_recovery.irq_lock, flags);

}

static irqreturn_t esd_irq_handler(int irq, void *handle)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = NULL;

	if (!handle) {
		pr_info("handle is null\n");
		return IRQ_HANDLED;
	}

	ctrl = (struct mdss_dsi_ctrl_pdata *)handle;

	if (!IS_ERR_OR_NULL(ctrl))
		vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx\n", __func__, (size_t)vdd);
		goto end;
	}

	pr_info("%s++\n", __func__);
	if (!vdd->esd_recovery.is_enabled_esd_recovery) {
		pr_info("%s: esd recovery is not enabled yet", __func__);
		goto end;
	}

	esd_irq_enable(false, true, (void *)vdd);

	vdd->panel_lpm.esd_recovery = true;

	schedule_work(&pstatus_data->check_status.work);
	pr_info("%s--\n", __func__);

end:
	return IRQ_HANDLED;
}

static void mdss_samsung_event_esd_recovery_init(struct mdss_panel_data *pdata, int event, void *arg)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = NULL;
	int ret;
	uint32_t *interval;
	uint32_t interval_ms_for_irq = 500;
	struct mdss_panel_info *pinfo;

	if (IS_ERR_OR_NULL(pdata)) {
		pr_err("%s: Invalid pdata : 0x%zx\n", __func__, (size_t)pdata);
		return;
	}

	vdd = (struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	interval = arg;

	pinfo = &ctrl->panel_data.panel_info;

	if (IS_ERR_OR_NULL(ctrl) ||	IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n",
				__func__, (size_t)ctrl, (size_t)vdd);
		return;
	}
	if (unlikely(!vdd->esd_recovery.esd_recovery_init)) {
		vdd->esd_recovery.esd_recovery_init = true;
		vdd->esd_recovery.esd_irq_enable = esd_irq_enable;
		if (ctrl->status_mode == ESD_REG_IRQ) {
			if (gpio_is_valid(vdd->esd_recovery.esd_gpio)) {
				gpio_request(vdd->esd_recovery.esd_gpio, "esd_recovery");
				ret = request_threaded_irq(
						gpio_to_irq(vdd->esd_recovery.esd_gpio),
						NULL,
						esd_irq_handler,
						vdd->esd_recovery.irqflags,
						"esd_recovery",
						(void *)ctrl);
				if (ret)
					pr_err("%s : Failed to request_irq, ret=%d\n",
							__func__, ret);
				else
					esd_irq_enable(false, true, (void *)vdd);
				*interval = interval_ms_for_irq;
			}
		}
	}
}

/*************************************************************
*
*		BRIGHTNESS RELATED FUNCTION BELOW.
*
**************************************************************/
int get_cmd_index(struct samsung_display_driver_data *vdd, int ndx)
{
	int index = vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].bkl[vdd->bl_level];

	if (vdd->dtsi_data[0].hmt_enabled && vdd->hmt_stat.hmt_on)
		index = vdd->cmd_idx;

	return vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].cmd_idx[index];
}

int get_candela_value(struct samsung_display_driver_data *vdd, int ndx)
{
	int index;

	if (vdd->aid_subdivision_enable)
		index = vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].bkl_subdivision[vdd->bl_level];
	else
		index = vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].bkl[vdd->bl_level];

	if (vdd->dtsi_data[0].hmt_enabled && vdd->hmt_stat.hmt_on)
		index = vdd->cmd_idx;

	return vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].lux_tab[index];
}

int get_scaled_level(struct samsung_display_driver_data *vdd, int ndx)
{
	int index = vdd->dtsi_data[ndx].scaled_level_map_table[vdd->panel_revision].bkl[vdd->bl_level];

	return vdd->dtsi_data[ndx].scaled_level_map_table[vdd->panel_revision].lux_tab[index];
}

static void mdss_samsung_update_brightness_packet(struct dsi_cmd_desc *packet, int *count, struct dsi_panel_cmds *tx_cmd)
{
	int loop = 0;

	if (IS_ERR_OR_NULL(packet)) {
		pr_err("%s %ps packet error", __func__, __builtin_return_address(0));
		return;
	}

	if (IS_ERR_OR_NULL(tx_cmd)) {
		pr_err("%s %ps tx_cmd error\n", __func__, __builtin_return_address(0));
		return;
	}

	if (*count > (BRIGHTNESS_MAX_PACKET - 1))/*cmd_count is index, if cmd_count >13 then panic*/
		panic("over max brightness_packet size(%d).. !!", BRIGHTNESS_MAX_PACKET);

	for (loop = 0; loop < tx_cmd->cmd_cnt; loop++) {
		packet[*count].dchdr.dtype = tx_cmd->cmds[loop].dchdr.dtype;
		packet[*count].dchdr.wait = tx_cmd->cmds[loop].dchdr.wait;
		packet[*count].dchdr.dlen = tx_cmd->cmds[loop].dchdr.dlen;

		packet[*count].payload = tx_cmd->cmds[loop].payload;

		(*count)++;
	}
}

static void update_packet_level_key_enable(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *packet, int *cmd_cnt, int level_key)
{
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)ctrl->panel_data.panel_private;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return;
	}

	if (!level_key)
		return;
	else {
		if ((level_key & PANEL_LEVE1_KEY) && !IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].level1_key_enable_tx_cmds[vdd->panel_revision].cmds))
			mdss_samsung_update_brightness_packet(packet, cmd_cnt, mdss_samsung_cmds_select(ctrl, PANEL_LEVE1_KEY_ENABLE));

		if ((level_key & PANEL_LEVE2_KEY) && !IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].level2_key_enable_tx_cmds[vdd->panel_revision].cmds))
			mdss_samsung_update_brightness_packet(packet, cmd_cnt, mdss_samsung_cmds_select(ctrl, PANEL_LEVE2_KEY_ENABLE));

	}
}

static void update_packet_level_key_disable(struct mdss_dsi_ctrl_pdata *ctrl, struct dsi_cmd_desc *packet, int *cmd_cnt, int level_key)
{
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)ctrl->panel_data.panel_private;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return;
	};

	if (!level_key)
		return;
	else {
		if ((level_key & PANEL_LEVE1_KEY) && !IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].level1_key_disable_tx_cmds[vdd->panel_revision].cmds))
			mdss_samsung_update_brightness_packet(packet, cmd_cnt, mdss_samsung_cmds_select(ctrl, PANEL_LEVE1_KEY_DISABLE));

		if ((level_key & PANEL_LEVE2_KEY) && !IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].level2_key_disable_tx_cmds[vdd->panel_revision].cmds))
			mdss_samsung_update_brightness_packet(packet, cmd_cnt, mdss_samsung_cmds_select(ctrl, PANEL_LEVE2_KEY_DISABLE));

	}
}

int mdss_samsung_hbm_brightenss_packet_set(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ndx;
	int cmd_cnt = 0;
	int level_key = 0;
	struct dsi_cmd_desc *packet = NULL;
	struct dsi_panel_cmds *tx_cmd = NULL;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return 0;
	}

	ndx = display_ndx_check(ctrl);

	if (vdd->display_status_dsi[ndx].hbm_mode && !vdd->auto_brightness_level) {
		pr_err("%s DSI%d: already hbm mode! return ", __func__, ndx);
		return 0;
	}

	/* init packet */
	packet = &vdd->brightness[ndx].brightness_packet_dsi[0];

	/*
	*	HBM doesn't need calculated cmds. So Just use previously fixed data.
	*/
	/* To check supporting HBM mdoe by hbm_gamma_tx_cmds */
	if (vdd->dtsi_data[ndx].hbm_gamma_tx_cmds[vdd->panel_revision].cmd_cnt) {
		/* hbm gamma */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_hbm_gamma)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_hbm_gamma(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	}

	if (vdd->dtsi_data[ndx].hbm_etc_tx_cmds[vdd->panel_revision].cmd_cnt) {
		/* hbm etc */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_hbm_etc)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_hbm_etc(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	}

	return cmd_cnt;
}

int mdss_samsung_normal_brightenss_packet_set(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ndx;
	int cmd_cnt = 0;
	int level_key = 0;
	struct dsi_cmd_desc *packet = NULL;
	struct dsi_panel_cmds *tx_cmd = NULL;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return 0;
	}

	ndx = display_ndx_check(ctrl);

	vdd->candela_level = get_candela_value(vdd, ctrl->ndx);

	packet = &vdd->brightness[ndx].brightness_packet_dsi[0]; /* init packet */

	if (vdd->smart_dimming_loaded_dsi[ndx]) { /* OCTA PANEL */
		/* hbm off */
		if (vdd->display_status_dsi[ndx].hbm_mode) {
			if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_hbm_off)) {
				level_key = false;
				tx_cmd = vdd->panel_func.samsung_brightness_hbm_off(ctrl, &level_key);

				update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
				mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
				update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
			}
		}

		/* aid/aor */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_aid)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_aid(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* ACL on status force setting - It depends on project concept, if there is no ACL setting menu */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_force_acl_on))
			vdd->acl_status = vdd->panel_func.samsung_force_acl_on(ctrl);

		/* acl */
		if (vdd->acl_status || vdd->siop_status) {
			if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_acl_on)) {
				level_key = false;
				tx_cmd = vdd->panel_func.samsung_brightness_acl_on(ctrl, &level_key);

				update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
				mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
				update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
			}

			if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_acl_percent)) {
				if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_pre_acl_percent)) {
					level_key = false;
					tx_cmd = vdd->panel_func.samsung_brightness_pre_acl_percent(ctrl, &level_key);

					update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
					mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
					update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
				}
				level_key = false;
				tx_cmd = vdd->panel_func.samsung_brightness_acl_percent(ctrl, &level_key);

				update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
				mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
				update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
			}
		} else {
			if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_acl_off)) {
				level_key = false;
				tx_cmd = vdd->panel_func.samsung_brightness_acl_off(ctrl, &level_key);

				update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
				mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
				update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
			}
		}

		/* elvss */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_elvss)) {
			if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_pre_elvss)) {
				level_key = false;
				tx_cmd = vdd->panel_func.samsung_brightness_pre_elvss(ctrl, &level_key);

				update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
				mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
				update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
			}
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_elvss(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* temperature elvss */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_elvss_temperature1)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_elvss_temperature1(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_elvss_temperature2)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_elvss_temperature2(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
		/* caps*/
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_caps)) {
			if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_pre_caps)) {
				level_key = false;
				tx_cmd = vdd->panel_func.samsung_brightness_pre_caps(ctrl, &level_key);

				update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
				mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
				update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
			}
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_caps(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
		/* vint */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_vint)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_vint(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* gamma */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_gamma)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_gamma(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	} else { /* TFT PANEL */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_tft_pwm_ldi)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_tft_pwm_ldi(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	}

	return cmd_cnt;
}

static int mdss_samsung_single_transmission_packet(struct samsung_brightenss_data *tx_packet)
{
	int loop;
	struct dsi_cmd_desc *packet = tx_packet->brightness_packet_dsi;
	int packet_cnt = tx_packet->brightness_packet_tx_cmds_dsi.cmd_cnt;

	for (loop = 0; (loop < packet_cnt) && (loop < BRIGHTNESS_MAX_PACKET); loop++) {
		if (packet[loop].dchdr.dtype == DTYPE_DCS_LWRITE ||
			packet[loop].dchdr.dtype == DTYPE_GEN_LWRITE)
			packet[loop].dchdr.last = 0;
		else {
			if (loop > 0)
				packet[loop - 1].dchdr.last = 1; /*To ensure previous single tx packet */

			packet[loop].dchdr.last = 1;
		}
	}

	if (loop == BRIGHTNESS_MAX_PACKET)
		return false;
	else {
		packet[loop - 1].dchdr.last = 1; /* To make last packet flag */
		return true;
	}
}

int mdss_samsung_brightness_dcs(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct samsung_display_driver_data *vdd = NULL;
	int cmd_cnt = 0;
	int ret = 0;
	int ndx;

	vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	vdd->bl_level = level;

	/* check the lcd id for DISPLAY_1 or DISPLAY_2 */
	if (!mdss_panel_attached(ctrl->ndx))
		return false;

	if (vdd->dtsi_data[0].hmt_enabled && vdd->hmt_stat.hmt_on) {
		pr_err("%s : HMT is on. do not set normal brightness..(%d)\n", __func__, level);
		return false;
	}

	if (vdd->mfd_dsi[DISPLAY_1]->panel_info->cont_splash_enabled) {
		pr_err("%s : splash is not done..\n", __func__);
		return 0;
	}

	ndx = display_ndx_check(ctrl);

	if (vdd->auto_brightness >= HBM_MODE && vdd->bl_level == 255 && !vdd->dtsi_data[ndx].tft_common_support) {
		cmd_cnt = mdss_samsung_hbm_brightenss_packet_set(ctrl);
		cmd_cnt > 0 ? vdd->display_status_dsi[ndx].hbm_mode = true : false;
	} else {
		cmd_cnt = mdss_samsung_normal_brightenss_packet_set(ctrl);
		cmd_cnt > 0 ? vdd->display_status_dsi[ndx].hbm_mode = false : false;
	}

	if (cmd_cnt) {
		/* setting tx cmds cmt */
		vdd->brightness[ndx].brightness_packet_tx_cmds_dsi.cmd_cnt = cmd_cnt;

		/* generate single tx packet */
		ret = mdss_samsung_single_transmission_packet(&vdd->brightness[ndx]);

		/* sending tx cmds */
		if (ret) {
			mdss_samsung_send_cmd(ctrl, PANEL_BRIGHT_CTRL);

		if (!IS_ERR_OR_NULL(vdd->dtsi_data[ndx].blic_dimming_cmds[vdd->panel_revision].cmds)) {
			if (vdd->bl_level == 0)
				vdd->dtsi_data[ndx].blic_dimming_cmds->cmds->payload[1] = 0x24;
			else
				vdd->dtsi_data[ndx].blic_dimming_cmds->cmds->payload[1] = 0x2C;

			mdss_samsung_send_cmd(ctrl, PANEL_BLIC_DIMMING);
		}

			pr_info("%s DSI%d level : %d  candela : %dCD hbm : %d\n", __func__,
				ndx, level, vdd->candela_level,
				vdd->display_status_dsi[ndx].hbm_mode);
		} else
			pr_info("%s DSDI%d single_transmission_fail error\n", __func__, ndx);
	} else
		pr_info("%s DSI%d level : %d skip\n", __func__, ndx, vdd->bl_level);

	return cmd_cnt;
}
void mdss_samsung_brightness_tft_pwm(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct samsung_display_driver_data *vdd = NULL;
	struct mdss_panel_info *pinfo;

	vdd = check_valid_ctrl(ctrl);
	if (vdd == NULL) {
		pr_err("%s: no PWM\n", __func__);
		return;
	}

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->blank_state == MDSS_PANEL_BLANK_BLANK)
		return;

	vdd->bl_level = level;

	if (vdd->panel_func.samsung_brightness_tft_pwm)
		vdd->panel_func.samsung_brightness_tft_pwm(ctrl, level);
}
void mdss_tft_autobrightness_cabc_update(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return;
	}
	pr_info("%s:\n", __func__);

	switch (vdd->auto_brightness) {
		case 0:
			mdss_samsung_cabc_update();
			break;
		case 1:
		case 2:
		case 3:
		case 4:
			mdss_samsung_send_cmd(ctrl, PANEL_CABC_ON);
			break;
		case 5:
		case 6:
			mdss_samsung_send_cmd(ctrl, PANEL_CABC_OFF);
			break;
	}
}
/*************************************************************
*
*		OSC TE FITTING RELATED FUNCTION BELOW.
*
**************************************************************/
static void mdss_samsung_event_osc_te_fitting(struct mdss_panel_data *pdata, int event, void *arg)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = NULL;
	struct osc_te_fitting_info *te_info = NULL;
	struct mdss_mdp_ctl *ctl = NULL;
	int ret, i, lut_count;

	if (IS_ERR_OR_NULL(pdata)) {
		pr_err("%s: Invalid pdata : 0x%zx\n", __func__, (size_t)pdata);
		return;
	}

	vdd = (struct samsung_display_driver_data *)pdata->panel_private;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	ctl = arg;

	if (IS_ERR_OR_NULL(ctrl) ||	IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(ctl)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx ctl : 0x%zx\n",
				__func__, (size_t)ctrl, (size_t)vdd, (size_t)ctl);
		return;
	}

	te_info = &vdd->te_fitting_info;

	if (IS_ERR_OR_NULL(te_info)) {
		pr_err("%s: Invalid te data : 0x%zx\n",
				__func__, (size_t)te_info);
		return;
	}

	if (pdata->panel_info.cont_splash_enabled) {
		pr_err("%s: cont splash enabled\n", __func__);
		return;
	}

	if (!ctl->vsync_handler.enabled) {
		pr_debug("%s: vsync handler does not enabled yet\n", __func__);
		return;
	}

	te_info->status |= TE_FITTING_DONE;

	pr_debug("%s:++\n", __func__);

	if (!(te_info->status & TE_FITTING_REQUEST_IRQ)) {
		te_info->status |= TE_FITTING_REQUEST_IRQ;

		ret = request_threaded_irq(
				gpio_to_irq(ctrl->disp_te_gpio),
				samsung_te_check_handler,
				NULL,
				IRQF_TRIGGER_FALLING,
				"VSYNC_GPIO",
				(void *)ctrl);
		if (ret)
			pr_err("%s : Failed to request_irq, ret=%d\n",
					__func__, ret);
		else
			disable_irq(gpio_to_irq(ctrl->disp_te_gpio));
		te_info->te_time =
			kzalloc(sizeof(long long) * te_info->sampling_rate, GFP_KERNEL);
		INIT_WORK(&te_info->work, samsung_te_check_done_work);
	}

	for (lut_count = 0; lut_count < OSC_TE_FITTING_LUT_MAX; lut_count++) {
		init_completion(&te_info->te_check_comp);
		te_info->status |= TE_CHECK_ENABLE;
		te_info->te_duration = 0;

		pr_debug("%s: osc_te_fitting _irq : %d\n",
				__func__, gpio_to_irq(ctrl->disp_te_gpio));

		enable_irq(gpio_to_irq(ctrl->disp_te_gpio));
		ret = wait_for_completion_timeout(
				&te_info->te_check_comp, 1000);

		if (ret <= 0)
			pr_err("%s: timeout\n", __func__);

		for (i = 0; i < te_info->sampling_rate; i++) {
			te_info->te_duration +=
				(i != 0 ? (te_info->te_time[i] - te_info->te_time[i-1]) : 0);
			pr_debug("%s: vsync time : %lld, sum : %lld\n",
					__func__, te_info->te_time[i], te_info->te_duration);
		}
		do_div(te_info->te_duration, te_info->sampling_rate - 1);
		pr_info("%s: ave vsync time : %lld\n",
				__func__, te_info->te_duration);
		te_info->status &= ~TE_CHECK_ENABLE;

		if (vdd->panel_func.samsung_osc_te_fitting)
			ret = vdd->panel_func.samsung_osc_te_fitting(ctrl);

		if (!ret)
			mdss_samsung_send_cmd(ctrl, PANEL_OSC_TE_FITTING);
		else
			break;
	}
	pr_debug("%s:--\n", __func__);
}

static void samsung_te_check_done_work(struct work_struct *work)
{
	struct osc_te_fitting_info *te_info = NULL;

	te_info = container_of(work, struct osc_te_fitting_info, work);

	if (IS_ERR_OR_NULL(te_info)) {
		pr_err("%s: Invalid TE tuning data\n", __func__);
		return;
	}

	complete_all(&te_info->te_check_comp);
}

static irqreturn_t samsung_te_check_handler(int irq, void *handle)
{
	struct samsung_display_driver_data *vdd = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct osc_te_fitting_info *te_info = NULL;
	static bool skip_first_te = true;
	static u8 count;

	if (skip_first_te) {
		skip_first_te = false;
		goto end;
	}

	if (IS_ERR_OR_NULL(handle)) {
		pr_err("handle is null\n");
		goto end;
	}

	ctrl = (struct mdss_dsi_ctrl_pdata *)handle;

	vdd = (struct samsung_display_driver_data *)ctrl->panel_data.panel_private;

	te_info = &vdd->te_fitting_info;

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n",
				__func__, (size_t)ctrl, (size_t)vdd);
		return -EINVAL;
	}

	if (!(te_info->status & TE_CHECK_ENABLE))
		goto end;

	if (count < te_info->sampling_rate) {
		te_info->te_time[count++] =
			ktime_to_us(ktime_get());
	} else {
		disable_irq_nosync(gpio_to_irq(ctrl->disp_te_gpio));
		schedule_work(&te_info->work);
		skip_first_te = true;
		count = 0;
	}

end:
	return IRQ_HANDLED;
}

/*************************************************************
*
*		LDI FPS RELATED FUNCTION BELOW.
*
**************************************************************/
int ldi_fps(unsigned int input_fps)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = samsung_get_vdd();

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx\n", __func__, (size_t)vdd);
		return 0;
	}

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (IS_ERR_OR_NULL(ctrl)) {
		pr_err("%s ctrl is error", __func__);
		return 0;
	}

	if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_change_ldi_fps))
		rc = vdd->panel_func.samsung_change_ldi_fps(ctrl, input_fps);

	if (rc)
		mdss_samsung_send_cmd(ctrl, PANEL_LDI_FPS_CHANGE);

	return rc;
}
EXPORT_SYMBOL(ldi_fps);

/*************************************************************
*
*		HMT RELATED FUNCTION BELOW.
*
**************************************************************/
static int get_candela_value_hmt(struct samsung_display_driver_data *vdd, int ndx)
{
	int index = vdd->dtsi_data[ndx].hmt_candela_map_table[vdd->panel_revision].bkl[vdd->hmt_stat.hmt_bl_level];

	return vdd->dtsi_data[ndx].hmt_candela_map_table[vdd->panel_revision].lux_tab[index];
}

static int get_cmd_idx_hmt(struct samsung_display_driver_data *vdd, int ndx)
{
	int index = vdd->dtsi_data[ndx].hmt_candela_map_table[vdd->panel_revision].bkl[vdd->hmt_stat.hmt_bl_level];

	return vdd->dtsi_data[ndx].hmt_candela_map_table[vdd->panel_revision].cmd_idx[index];
}

int mdss_samsung_hmt_brightenss_packet_set(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int cmd_cnt = 0;
	int level_key = 0;
	struct dsi_cmd_desc *packet = NULL;
	struct dsi_panel_cmds *tx_cmd = NULL;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	pr_info("%s : ++\n", __func__);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return 0;
	}

	/* init packet */
	packet = &vdd->brightness[ctrl->ndx].brightness_packet_dsi[0];

	if (vdd->smart_dimming_hmt_loaded_dsi[ctrl->ndx]) {
		/* aid/aor B2 */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_aid_hmt)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_aid_hmt(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* elvss B5 */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_elvss_hmt)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_elvss_hmt(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* vint F4 */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_vint_hmt)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_vint_hmt(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}

		/* gamma CA */
		if (!IS_ERR_OR_NULL(vdd->panel_func.samsung_brightness_gamma_hmt)) {
			level_key = false;
			tx_cmd = vdd->panel_func.samsung_brightness_gamma_hmt(ctrl, &level_key);

			update_packet_level_key_enable(ctrl, packet, &cmd_cnt, level_key);
			mdss_samsung_update_brightness_packet(packet, &cmd_cnt, tx_cmd);
			update_packet_level_key_disable(ctrl, packet, &cmd_cnt, level_key);
		}
	}

	pr_info("%s : --\n", __func__);

	return cmd_cnt;
}

int mdss_samsung_brightness_dcs_hmt(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct samsung_display_driver_data *vdd = NULL;
	int cmd_cnt;
	int ret = 0;

	vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n", __func__, (size_t)ctrl, (size_t)vdd);
		return false;
	}

	vdd->hmt_stat.hmt_bl_level = level;

	pr_err("[HMT] hmt_bl_level(%d)\n", vdd->hmt_stat.hmt_bl_level);

	if (level < 0 || level > 255) {
		pr_err("[HMT] hmt_bl_level(%d) is out of range!\n", level);
		return 0;
	}

	vdd->hmt_stat.cmd_idx_hmt = get_cmd_idx_hmt(vdd, ctrl->ndx);
	vdd->hmt_stat.candela_level_hmt = get_candela_value_hmt(vdd, ctrl->ndx);

	cmd_cnt = mdss_samsung_hmt_brightenss_packet_set(ctrl);

	/* sending tx cmds */
	if (cmd_cnt) {
		/* setting tx cmds cmt */
		vdd->brightness[ctrl->ndx].brightness_packet_tx_cmds_dsi.cmd_cnt = cmd_cnt;

		/* generate single tx packet */
		ret = mdss_samsung_single_transmission_packet(vdd->brightness);

		if (ret) {
			mdss_samsung_send_cmd(ctrl, PANEL_BRIGHT_CTRL);

			pr_info("%s : DSI(%d) cmd_idx(%d), candela_level(%d), hmt_bl_level(%d)", __func__,
				ctrl->ndx, vdd->hmt_stat.cmd_idx_hmt, vdd->hmt_stat.candela_level_hmt, vdd->hmt_stat.hmt_bl_level);
		} else
			pr_debug("%s DSDI%d single_transmission_fail error\n", __func__, ctrl->ndx);
	} else
		pr_info("%s DSI%d level : %d skip\n", __func__, ctrl->ndx, vdd->bl_level);

	return cmd_cnt;
}

/************************************************************

		PANEL DTSI PARSE FUNCTION.

		--- NEVER DELETE OR CHANGE ORDER ---
		---JUST ADD ITEMS AT LAST---

	"samsung,display_on_tx_cmds_rev%c"
	"samsung,display_off_tx_cmds_rev%c"
	"samsung,level1_key_enable_tx_cmds_rev%c"
	"samsung,level1_key_disable_tx_cmds_rev%c"
	"samsung,hsync_on_tx_cmds_rev%c"
	"samsung,level2_key_enable_tx_cmds_rev%c"
	"samsung,level2_key_disable_tx_cmds_rev%c"
	"samsung,smart_dimming_mtp_rx_cmds_rev%c"
	"samsung,manufacture_read_pre_tx_cmds_rev%c"
	"samsung,manufacture_id0_rx_cmds_rev%c"
	"samsung,manufacture_id1_rx_cmds_rev%c"
	"samsung,manufacture_id2_rx_cmds_rev%c"
	"samsung,manufacture_date_rx_cmds_rev%c"
	"samsung,ddi_id_rx_cmds_rev%c"
	"samsung,rddpm_rx_cmds_rev%c"
	"samsung,mtp_read_sysfs_rx_cmds_rev%c"
	"samsung,vint_tx_cmds_rev%c"
	"samsung,vint_map_table_rev%c"
	"samsung,acl_off_tx_cmds_rev%c"
	"samsung,acl_map_table_rev%c"
	"samsung,candela_map_table_rev%c"
	"samsung,acl_percent_tx_cmds_rev%c"
	"samsung,acl_pre_percent_tx_cmds_rev%c"
	"samsung,acl_on_tx_cmds_rev%c"
	"samsung,gamma_tx_cmds_rev%c"
	"samsung,elvss_rx_cmds_rev%c"
	"samsung,elvss_tx_cmds_rev%c"
	"samsung,elvss_map_table_rev%c"
	"samsung,elvss_pre_tx_cmds_rev%c"
	"samsung,aid_map_table_rev%c"
	"samsung,aid_tx_cmds_rev%c"
	"samsung,hbm_rx_cmds_rev%c"
	"samsung,hbm2_rx_cmds_rev%c"
	"samsung,hbm_gamma_tx_cmds_rev%c"
	"samsung,hbm_etc_tx_cmds_rev%c"
	"samsung,hbm_etc2_tx_cmds_rev%c"
	"samsung,hbm_off_tx_cmds_rev%c"
	"samsung,mdnie_read_rx_cmds_rev%c"
	"samsung,ldi_debug0_rx_cmds_rev%c"
	"samsung,ldi_debug1_rx_cmds_rev%c"
	"samsung,ldi_debug2_rx_cmds_rev%c"
	"samsung,elvss_lowtemp_tx_cmds_rev%c"
	"samsung,elvss_lowtemp2_tx_cmds_rev%c"
	"samsung,smart_acl_elvss_lowtemp_tx_cmds_rev%c"
	"samsung,smart_acl_elvss_tx_cmds_rev%c"
	"samsung,smart_acl_elvss_map_table_rev%c"
	"samsung,partial_display_on_tx_cmds_rev%c"
	"samsung,partial_display_off_tx_cmds_rev%c"
	"samsung,partial_display_column_row_tx_cmds_rev%c"
	"samsung,alpm_on_tx_cmds_rev%c"
	"samsung,alpm_off_tx_cmds_rev%c"
	"samsung,lpm_2nit_tx_cmds_rev%c"
	"samsung,lpm_40nit_tx_cmds_rev%c"
	"samsung,lpm_ctrl_alpm_tx_cmds_rev%c"
	"samsung,lpm_ctrl_hlpm_tx_cmds_rev%c"
	"samsung,lpm_1hz_tx_cmds_rev%c"
	"samsung,lpm_2hz_tx_cmds_rev%c"
	"samsung,lpm_hz_none_tx_cmds_rev%c"
	"samsung,hmt_gamma_tx_cmds_rev%c"
	"samsung,hmt_elvss_tx_cmds_rev%c"
	"samsung,hmt_vint_tx_cmds_rev%c"
	"samsung,hmt_enable_tx_cmds_rev%c"
	"samsung,hmt_disable_tx_cmds_rev%c"
	"samsung,hmt_reverse_enable_tx_cmds_rev%c"
	"samsung,hmt_reverse_disable_tx_cmds_rev%c"
	"samsung,hmt_aid_tx_cmds_rev%c"
	"samsung,hmt_reverse_aid_map_table_rev%c"
	"samsung,hmt_150cd_rx_cmds_rev%c"
	"samsung,hmt_candela_map_table_rev%c"
	"samsung,ldi_fps_change_tx_cmds_rev%c"
	"samsung,ldi_fps_rx_cmds_rev%c"
	"samsung,tft_pwm_tx_cmds_rev%c"
	"samsung,scaled_level_map_table_rev%c"
	"samsung,packet_size_tx_cmds_rev%c"
	"samsung,reg_read_pos_tx_cmds_rev%c"
	"samsung,osc_te_fitting_tx_cmds_rev%c"
	"samsung,panel_ldi_vdd_read_cmds_rev%c"
	"samsung,panel_ldi_vddm_read_cmds_rev%c"
	"samsung,panel_ldi_vdd_offset_write_cmds_rev%c"
	"samsung,panel_ldi_vddm_offset_write_cmds_rev%c"
	"samsung,cabc_on_tx_cmds_rev%c"
	"samsung,cabc_off_tx_cmds_rev%c"
	"samsung,cabc_on_duty_tx_cmds_rev%c"
	"samsung,cabc_off_duty_tx_cmds_rev%c"
	"samsung,pre_caps_setting_tx_cmds_revA%c"
	"samsung,caps_setting_tx_cmds_rev%c"
	"samsung,caps_map_table_rev%c"
*************************************************************/
static void parse_dt_data(struct device_node *np,
		void *data,	char *cmd_string, char panel_rev, void *fnc)
{
	char string[PARSE_STRING];
	int ret = 0;

	if (IS_ERR_OR_NULL(np) || IS_ERR_OR_NULL(data) || IS_ERR_OR_NULL(cmd_string)) {
		pr_err("%s: Invalid data, np : 0x%zx data : 0x%zx cmd_string : 0x%zx\n",
				__func__, (size_t)np, (size_t)data, (size_t)cmd_string);
		return;
	}

	/* Generate string to parsing from DT */
	snprintf(string, PARSE_STRING, "%s%c", cmd_string, 'A' + panel_rev);

	if (fnc == mdss_samsung_parse_panel_table) {
		ret = mdss_samsung_parse_panel_table(np,
				(struct cmd_map *)data, string);

		if (ret && (panel_rev > 0))
			memcpy(data, (struct cmd_map *)data - 1, sizeof(struct cmd_map));
	} else if (fnc == mdss_samsung_parse_candella_lux_mapping_table) {
		ret = mdss_samsung_parse_candella_lux_mapping_table(np,
			(struct candella_lux_map *)data, string);

		if (ret && (panel_rev > 0))
			memcpy(data, (struct candella_lux_map *)data - 1, sizeof(struct candella_lux_map));
	} else if ((fnc == mdss_samsung_parse_dcs_cmds) || (fnc == NULL)) {
		ret = mdss_samsung_parse_dcs_cmds(np,
			(struct dsi_panel_cmds *)data, string, NULL);

		if (ret && (panel_rev > 0))
			memcpy(data, (struct dsi_panel_cmds *)data - 1, sizeof(struct dsi_panel_cmds));
	}
}

void mdss_samsung_panel_parse_dt_cmds(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int panel_rev;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	struct samsung_display_dtsi_data *dtsi_data = NULL;
	/* At this time ctrl->ndx is not set */
	int ndx = ctrl->panel_data.panel_info.pdest;
	static int parse_dt_cmds_cnt = 0;

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx\n",
				__func__, (size_t)ctrl, (size_t)vdd);
		return;
	}

	pr_info("%s DSI%d", __func__, ndx);

	if (vdd->support_hall_ic) {
		if (!parse_dt_cmds_cnt) {
			ndx = DSI_CTRL_0;
			parse_dt_cmds_cnt = 1;
		} else
			ndx = DSI_CTRL_1;
	}

	dtsi_data = &vdd->dtsi_data[ndx];

	for (panel_rev = 0; panel_rev < SUPPORT_PANEL_REVISION; panel_rev++) {
		parse_dt_data(np, &dtsi_data->display_on_tx_cmds[panel_rev],
				"samsung,display_on_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->display_off_tx_cmds[panel_rev],
				"samsung,display_off_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->level1_key_enable_tx_cmds[panel_rev],
				"samsung,level1_key_enable_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->level1_key_disable_tx_cmds[panel_rev],
				"samsung,level1_key_disable_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->hsync_on_tx_cmds[panel_rev],
				"samsung,hsync_on_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->level2_key_enable_tx_cmds[panel_rev],
				"samsung,level2_key_enable_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->level2_key_disable_tx_cmds[panel_rev],
				"samsung,level2_key_disable_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->smart_dimming_mtp_rx_cmds[panel_rev],
				"samsung,smart_dimming_mtp_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->manufacture_read_pre_tx_cmds[panel_rev],
				"samsung,manufacture_read_pre_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->manufacture_id_rx_cmds[panel_rev],
				"samsung,manufacture_id_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->manufacture_id0_rx_cmds[panel_rev],
				"samsung,manufacture_id0_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->manufacture_id1_rx_cmds[panel_rev],
				"samsung,manufacture_id1_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->manufacture_id2_rx_cmds[panel_rev],
				"samsung,manufacture_id2_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->manufacture_date_rx_cmds[panel_rev],
				"samsung,manufacture_date_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->ddi_id_rx_cmds[panel_rev],
				"samsung,ddi_id_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->cell_id_rx_cmds[panel_rev],
				"samsung,cell_id_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->rddpm_rx_cmds[panel_rev],
				"samsung,rddpm_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->mtp_read_sysfs_rx_cmds[panel_rev],
				"samsung,mtp_read_sysfs_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->vint_tx_cmds[panel_rev],
				"samsung,vint_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->vint_map_table[panel_rev],
				"samsung,vint_map_table_rev", panel_rev,
				mdss_samsung_parse_panel_table); /* VINT TABLE */

		parse_dt_data(np, &dtsi_data->acl_off_tx_cmds[panel_rev],
				"samsung,acl_off_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->acl_map_table[panel_rev],
				"samsung,acl_map_table_rev", panel_rev,
				mdss_samsung_parse_panel_table); /* ACL TABLE */

		parse_dt_data(np, &dtsi_data->candela_map_table[panel_rev],
				"samsung,candela_map_table_rev", panel_rev,
				mdss_samsung_parse_candella_lux_mapping_table);

		parse_dt_data(np, &dtsi_data->acl_percent_tx_cmds[panel_rev],
				"samsung,acl_percent_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->acl_on_tx_cmds[panel_rev],
				"samsung,acl_on_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->acl_pre_percent_tx_cmds[panel_rev],
				"samsung,acl_pre_percent_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->gamma_tx_cmds[panel_rev],
				"samsung,gamma_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->elvss_rx_cmds[panel_rev],
				"samsung,elvss_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->elvss_tx_cmds[panel_rev],
				"samsung,elvss_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->elvss_map_table[panel_rev],
				"samsung,elvss_map_table_rev", panel_rev,
				mdss_samsung_parse_panel_table); /* ELVSS TABLE */

		parse_dt_data(np, &dtsi_data->elvss_pre_tx_cmds[panel_rev],
				"samsung,elvss_pre_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->aid_map_table[panel_rev],
				"samsung,aid_map_table_rev", panel_rev,
				mdss_samsung_parse_panel_table); /* AID TABLE */

		parse_dt_data(np, &dtsi_data->aid_tx_cmds[panel_rev],
				"samsung,aid_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->aid_subdivision_tx_cmds[panel_rev],
				"samsung,aid_subdivision_tx_cmds_rev", panel_rev, NULL);

		/* CONFIG_HBM_RE */
		parse_dt_data(np, &dtsi_data->hbm_rx_cmds[panel_rev],
				"samsung,hbm_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->hbm2_rx_cmds[panel_rev],
				"samsung,hbm2_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->hbm_gamma_tx_cmds[panel_rev],
				"samsung,hbm_gamma_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->hbm_etc_tx_cmds[panel_rev],
				"samsung,hbm_etc_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->hbm_off_tx_cmds[panel_rev],
				"samsung,hbm_off_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->hbm_etc2_tx_cmds[panel_rev],
				"samsung,hbm_etc2_tx_cmds_rev", panel_rev, NULL);

		/* CCB */
		parse_dt_data(np, &dtsi_data->ccb_on_tx_cmds[panel_rev],
				"samsung,ccb_on_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->ccb_off_tx_cmds[panel_rev],
				"samsung,ccb_off_tx_cmds_rev", panel_rev, NULL);

		/* CONFIG_TCON_MDNIE_LITE */
		parse_dt_data(np, &dtsi_data->mdnie_read_rx_cmds[panel_rev],
				"samsung,mdnie_read_rx_cmds_rev", panel_rev, NULL);

		/* CONFIG_DEBUG_LDI_STATUS */
		parse_dt_data(np, &dtsi_data->ldi_debug0_rx_cmds[panel_rev],
				"samsung,ldi_debug0_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->ldi_debug1_rx_cmds[panel_rev],
				"samsung,ldi_debug1_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->ldi_debug2_rx_cmds[panel_rev],
				"samsung,ldi_debug2_rx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->ldi_loading_det_rx_cmds[panel_rev],
				"samsung,ldi_loading_det_rx_cmds_rev", panel_rev, NULL);

		/* CONFIG_TEMPERATURE_ELVSS */
		parse_dt_data(np, &dtsi_data->elvss_lowtemp_tx_cmds[panel_rev],
				"samsung,elvss_lowtemp_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->elvss_lowtemp2_tx_cmds[panel_rev],
				"samsung,elvss_lowtemp2_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->smart_acl_elvss_lowtemp_tx_cmds[panel_rev],
				"samsung,smart_acl_elvss_lowtemp_tx_cmds_rev", panel_rev, NULL);

		/* CONFIG_SMART_ACL */
		parse_dt_data(np, &dtsi_data->smart_acl_elvss_tx_cmds[panel_rev],
				"samsung,smart_acl_elvss_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->smart_acl_elvss_map_table[panel_rev],
				"samsung,smart_acl_elvss_map_table_rev", panel_rev,
				mdss_samsung_parse_panel_table); /* TABLE */

		/* CONFIG_CAPS*/
		parse_dt_data(np, &dtsi_data->pre_caps_setting_tx_cmds[panel_rev],
				"samsung,pre_caps_setting_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->caps_setting_tx_cmds[panel_rev],
				"samsung,caps_setting_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->caps_map_table[panel_rev],
				"samsung,caps_map_table_rev", panel_rev,
				mdss_samsung_parse_panel_table); /* TABLE */

		/* CONFIG_PARTIAL_UPDATE */
		parse_dt_data(np, &dtsi_data->partial_display_on_tx_cmds[panel_rev],
				"samsung,partial_display_on_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->partial_display_off_tx_cmds[panel_rev],
				"samsung,partial_display_off_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->partial_display_column_row_tx_cmds[panel_rev],
				"samsung,partial_display_column_row_tx_cmds_rev", panel_rev, NULL);

		/* CONFIG_ALPM_MODE */
		parse_dt_data(np, &dtsi_data->alpm_on_tx_cmds[panel_rev],
				"samsung,alpm_on_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->alpm_off_tx_cmds[panel_rev],
				"samsung,alpm_off_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->lpm_2nit_tx_cmds[panel_rev],
				"samsung,lpm_2nit_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->lpm_40nit_tx_cmds[panel_rev],
				"samsung,lpm_40nit_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->lpm_ctrl_alpm_tx_cmds[panel_rev],
				"samsung,lpm_ctrl_alpm_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->lpm_ctrl_hlpm_tx_cmds[panel_rev],
				"samsung,lpm_ctrl_hlpm_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->lpm_1hz_tx_cmds[panel_rev],
				"samsung,lpm_1hz_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->lpm_2hz_tx_cmds[panel_rev],
				"samsung,lpm_2hz_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->lpm_hz_none_tx_cmds[panel_rev],
				"samsung,lpm_hz_none_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->lpm_aod_on_tx_cmds[panel_rev],
				"samsung,lpm_ctrl_alpm_aod_on_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->lpm_aod_off_tx_cmds[panel_rev],
				"samsung,lpm_ctrl_alpm_aod_off_tx_cmds_rev", panel_rev, NULL);

		/* HMT */
		parse_dt_data(np, &dtsi_data->hmt_gamma_tx_cmds[panel_rev],
				"samsung,hmt_gamma_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->hmt_elvss_tx_cmds[panel_rev],
				"samsung,hmt_elvss_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->hmt_vint_tx_cmds[panel_rev],
				"samsung,hmt_vint_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->hmt_enable_tx_cmds[panel_rev],
				"samsung,hmt_enable_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->hmt_disable_tx_cmds[panel_rev],
				"samsung,hmt_disable_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->hmt_reverse_tx_cmds[panel_rev],
				"samsung,hmt_reverse_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->hmt_forward_tx_cmds[panel_rev],
				"samsung,hmt_forward_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->hmt_aid_tx_cmds[panel_rev],
				"samsung,hmt_aid_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->hmt_reverse_aid_map_table[panel_rev],
				"samsung,hmt_reverse_aid_map_table_rev", panel_rev,
				mdss_samsung_parse_panel_table); /* TABLE */

		parse_dt_data(np, &dtsi_data->hmt_candela_map_table[panel_rev],
				"samsung,hmt_candela_map_table_rev", panel_rev,
				mdss_samsung_parse_candella_lux_mapping_table);

		/* CONFIG_FPS_CHANGE */
		parse_dt_data(np, &dtsi_data->ldi_fps_change_tx_cmds[panel_rev],
				"samsung,ldi_fps_change_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->ldi_fps_rx_cmds[panel_rev],
				"samsung,ldi_fps_rx_cmds_rev", panel_rev, NULL);

		/* TFT PWM CONTROL */
		parse_dt_data(np, &dtsi_data->tft_pwm_tx_cmds[panel_rev],
				"samsung,tft_pwm_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->blic_dimming_cmds[panel_rev],
				"samsung,blic_dimming_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->scaled_level_map_table[panel_rev],
				"samsung,scaled_level_map_table_rev", panel_rev,
				mdss_samsung_parse_candella_lux_mapping_table);

		/* Additional Command */
		parse_dt_data(np, &dtsi_data->packet_size_tx_cmds[panel_rev],
				"samsung,packet_size_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->reg_read_pos_tx_cmds[panel_rev],
				"samsung,reg_read_pos_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->osc_te_fitting_tx_cmds[panel_rev],
				"samsung,osc_te_fitting_tx_cmds_rev", panel_rev, NULL);

		/* samsung,avc_on */
		parse_dt_data(np, &dtsi_data->avc_on_tx_cmds[panel_rev],
				"samsung,avc_on_rev", panel_rev, NULL);

		/* VDDM OFFSET */
		parse_dt_data(np, &dtsi_data->read_vdd_ref_cmds[panel_rev],
				"samsung,panel_ldi_vdd_read_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->read_vddm_ref_cmds[panel_rev],
				"samsung,panel_ldi_vddm_read_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->write_vdd_offset_cmds[panel_rev],
				"samsung,panel_ldi_vdd_offset_write_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->write_vddm_offset_cmds[panel_rev],
				"samsung,panel_ldi_vddm_offset_write_cmds_rev", panel_rev, NULL);

		/* TFT CABC CONTROL */
		parse_dt_data(np, &dtsi_data->cabc_on_tx_cmds[panel_rev],
				"samsung,cabc_on_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->cabc_off_tx_cmds[panel_rev],
				"samsung,cabc_off_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->cabc_on_duty_tx_cmds[panel_rev],
				"samsung,cabc_on_duty_tx_cmds_rev", panel_rev, NULL);

		parse_dt_data(np, &dtsi_data->cabc_off_duty_tx_cmds[panel_rev],
				"samsung,cabc_off_duty_tx_cmds_rev", panel_rev, NULL);

		/* SPI ENABLE */
		parse_dt_data(np, &dtsi_data->spi_enable_tx_cmds[panel_rev],
				"samsung,spi_enable_tx_cmds_rev", panel_rev, NULL);
	}
}

void mdss_samsung_check_hw_config(struct platform_device *pdev)
{
	struct mdss_dsi_data *dsi_res = platform_get_drvdata(pdev);
	struct dsi_shared_data *sdata;
	struct samsung_display_driver_data *vdd;

	if (!dsi_res)
		return;

	vdd = check_valid_ctrl(dsi_res->ctrl_pdata[DSI_CTRL_0]);
	sdata = dsi_res->shared_data;

	if (!vdd || !sdata)
		return;

	sdata->hw_config = vdd->samsung_hw_config;
	pr_info("%s: DSI h/w configuration is %d\n", __func__,
			sdata->hw_config);
}

void mdss_samsung_panel_pbaboot_config(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_debug_data *mdd =
		(struct mdss_debug_data *)((mdss_mdp_get_mdata())->debug_inf.debug_data);
	struct samsung_display_driver_data *vdd = NULL;
	bool need_to_force_vidoe_mode = false;

	if (IS_ERR_OR_NULL(ctrl) || IS_ERR_OR_NULL(mdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx, mdd : 9x%zx\n", __func__,
				(size_t)ctrl, (size_t)mdd);
		return;
	}

	pinfo = &ctrl->panel_data.panel_info;
	vdd = check_valid_ctrl(ctrl);

	if (vdd->support_hall_ic) {
		/* check the lcd id for DISPLAY_1 and DISPLAY_2 */
		if (!mdss_panel_attached(DISPLAY_1) && !mdss_panel_attached(DISPLAY_2))
			need_to_force_vidoe_mode = true;
	} else {
		/* check the lcd id for DISPLAY_1 */
		if (!mdss_panel_attached(DISPLAY_1))
			need_to_force_vidoe_mode = true;
	}

	/* Support PBA boot without lcd */
	if (need_to_force_vidoe_mode &&
			!IS_ERR_OR_NULL(pinfo) &&
			!IS_ERR_OR_NULL(vdd) &&
			(pinfo->mipi.mode == DSI_CMD_MODE)) {
		pr_err("%s force VIDEO_MODE : %d\n", __func__, ctrl->ndx);
		pinfo->type = MIPI_VIDEO_PANEL;
		pinfo->mipi.mode = DSI_VIDEO_MODE;
		pinfo->mipi.traffic_mode = DSI_BURST_MODE;
		pinfo->mipi.bllp_power_stop = true;
		pinfo->mipi.te_sel = 0;
		pinfo->mipi.vsync_enable = 0;
		pinfo->mipi.hw_vsync_mode = 0;
		pinfo->mipi.force_clk_lane_hs = true;
		pinfo->mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;

		pinfo->cont_splash_enabled = false;
		pinfo->mipi.lp11_init = false;

		vdd->support_mdnie_lite = false;
		if (vdd->other_line_panel_support) {
			vdd->other_line_panel_support = false;
			destroy_workqueue(vdd_data.other_line_panel_support_workq);
		}

		pinfo->esd_check_enabled = false;
		ctrl->on_cmds.link_state = DSI_LP_MODE;
		ctrl->off_cmds.link_state = DSI_LP_MODE;

		/* To avoid underrun panic*/
		mdd->logd.xlog_enable = 0;
		vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting = false;
	}
}

void mdss_samsung_panel_parse_dt_esd(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0;
	const char *data;
	struct samsung_display_driver_data *vdd = NULL;

	if (!ctrl) {
		pr_err("%s: ctrl is null\n", __func__);
		return;
	}

	vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data vdd : 0x%zx\n", __func__, (size_t)vdd);
		return;
	}

	vdd->esd_recovery.esd_gpio = of_get_named_gpio(np, "qcom,esd_irq_gpio", 0);

	if (gpio_is_valid(vdd->esd_recovery.esd_gpio)) {
		pr_info("%s: esd gpio : %d, irq : %d\n",
				__func__,
				vdd->esd_recovery.esd_gpio,
				gpio_to_irq(vdd->esd_recovery.esd_gpio));
	}

	rc = of_property_read_string(np, "qcom,mdss-dsi-panel-status-check-mode", &data);
	if (!rc) {
		if (!strcmp(data, "reg_read_irq")) {
			ctrl->status_mode = ESD_REG_IRQ;
			ctrl->status_cmds_rlen = 0;
			ctrl->check_read_status = mdss_dsi_esd_irq_status;
		}
	}

	rc = of_property_read_string(np, "qcom,mdss-dsi-panel-status-irq-trigger", &data);
	if (!rc) {
		vdd->esd_recovery.irqflags = IRQF_ONESHOT;

		if (!strcmp(data, "rising"))
			vdd->esd_recovery.irqflags |= IRQF_TRIGGER_RISING;
		else if (!strcmp(data, "falling"))
			vdd->esd_recovery.irqflags |= IRQF_TRIGGER_FALLING;
		else if (!strcmp(data, "high"))
			vdd->esd_recovery.irqflags |= IRQF_TRIGGER_HIGH;
		else if (!strcmp(data, "low"))
			vdd->esd_recovery.irqflags |= IRQF_TRIGGER_LOW;
	}
}

void mdss_samsung_panel_parse_dt(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc, i;
	u32 tmp[2];
	char panel_extra_power_gpio[] = "samsung,panel-extra-power-gpio1";
	char backlight_tft_gpio[] = "samsung,panel-backlight-tft-gpio1";
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);
	const char *data;

	rc = of_property_read_u32(np, "samsung,support_panel_max", tmp);
	vdd->support_panel_max = !rc ? tmp[0] : 1;


	/* Set LP11 init flag */
	vdd->dtsi_data[ctrl->ndx].samsung_lp11_init = of_property_read_bool(np, "samsung,dsi-lp11-init");

	pr_err("%s: LP11 init %s\n", __func__,
		vdd->dtsi_data[ctrl->ndx].samsung_lp11_init ? "enabled" : "disabled");

	rc = of_property_read_u32(np, "samsung,mdss-power-on-reset-delay-us", tmp);
	vdd->dtsi_data[ctrl->ndx].samsung_power_on_reset_delay = (!rc ? tmp[0] : 0);

	rc = of_property_read_u32(np, "samsung,mdss-power-on-delay-us", tmp);
	vdd->dtsi_data[ctrl->ndx].samsung_power_on_delay = (!rc ? tmp[0] : 0);

	rc = of_property_read_u32(np, "samsung,mdss-power-off-delay-us", tmp);
	vdd->dtsi_data[ctrl->ndx].samsung_power_off_delay = (!rc ? tmp[0] : 0);

	rc = of_property_read_u32(np, "samsung,mdss-dsi-off-reset-delay-us", tmp);
	vdd->dtsi_data[ctrl->ndx].samsung_dsi_off_reset_delay = (!rc ? tmp[0] : 0);

	/* Set esc clk 128M */
	vdd->dtsi_data[ctrl->ndx].samsung_esc_clk_128M = of_property_read_bool(np, "samsung,esc-clk-128M");
	pr_err("%s: ESC CLK 128M %s\n", __func__,
		vdd->dtsi_data[ctrl->ndx].samsung_esc_clk_128M ? "enabled" : "disabled");

	vdd->dtsi_data[ctrl->ndx].panel_lpm_enable = of_property_read_bool(np, "samsung,panel-lpm-enable");
	pr_err("%s: alpm enable %s\n", __func__,
		vdd->dtsi_data[ctrl->ndx].panel_lpm_enable ? "enabled" : "disabled");

	/* Set HALL IC */
	vdd->support_hall_ic  = of_property_read_bool(np, "samsung,mdss_dsi_hall_ic_enable");
	pr_err("%s: hall_ic %s\n", __func__, vdd->support_hall_ic ? "enabled" : "disabled");

	/*Set OSC TE fitting flag */
	vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting =
		of_property_read_bool(np, "samsung,osc-te-fitting-enable");

	if (vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting) {
		rc = of_property_read_u32_array(np, "samsung,osc-te-fitting-cmd-index", tmp, 2);

		if (!rc) {
			vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting_cmd_index[0] =
				tmp[0];
			vdd->dtsi_data[ctrl->ndx].samsung_osc_te_fitting_cmd_index[1] =
				tmp[1];
		}

		rc = of_property_read_u32(np, "samsung,osc-te-fitting-sampling-rate", tmp);

		vdd->te_fitting_info.sampling_rate = !rc ? tmp[0] : 2;

	}

	pr_info("%s: OSC TE fitting %s\n", __func__,
		vdd->dtsi_data[0].samsung_osc_te_fitting ? "enabled" : "disabled");

	/* Set HMT flag */
	vdd->dtsi_data[0].hmt_enabled = of_property_read_bool(np, "samsung,hmt_enabled");
	if (vdd->dtsi_data[0].hmt_enabled)
		for (i = 1; i < vdd->support_panel_max; i++)
			vdd->dtsi_data[i].hmt_enabled = true;

	pr_info("%s: hmt %s\n", __func__,
		vdd->dtsi_data[0].hmt_enabled ? "enabled" : "disabled");

	/* TCON Clk On Support */
	vdd->dtsi_data[ctrl->ndx].samsung_tcon_clk_on_support = of_property_read_bool(np,
		 "samsung,tcon-clk-on-support");
	pr_info("%s: Tcon Clk On Suppor %s\n", __func__,
		vdd->dtsi_data[ctrl->ndx].samsung_tcon_clk_on_support ? "enabled" : "disabled");

	/* Set TFT flag */
	vdd->mdnie_tuning_enable_tft = of_property_read_bool(np,
				"samsung,mdnie-tuning-enable-tft");
	vdd->dtsi_data[ctrl->ndx].tft_common_support  = of_property_read_bool(np,
		"samsung,tft-common-support");

	pr_info("%s: tft_common_support %s\n", __func__,
	vdd->dtsi_data[ctrl->ndx].tft_common_support ? "enabled" : "disabled");

	vdd->dtsi_data[ctrl->ndx].tft_module_name = of_get_property(np,
		"samsung,tft-module-name", NULL);  /* for tft tablet */

	vdd->dtsi_data[ctrl->ndx].panel_vendor = of_get_property(np,
		"samsung,panel-vendor", NULL);

	vdd->dtsi_data[ctrl->ndx].backlight_gpio_config = of_property_read_bool(np,
		"samsung,backlight-gpio-config");

	pr_info("%s: backlight_gpio_config %s\n", __func__,
	vdd->dtsi_data[ctrl->ndx].backlight_gpio_config ? "enabled" : "disabled");

	/* Factory Panel Swap*/
	vdd->dtsi_data[ctrl->ndx].samsung_support_factory_panel_swap = of_property_read_bool(np,
		"samsung,support_factory_panel_swap");

	/* Set extra power gpio */
	for (i = 0; i < MAX_EXTRA_POWER_GPIO; i++) {
		panel_extra_power_gpio[strlen(panel_extra_power_gpio) - 1] = '1' + i;
		vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i] =
				 of_get_named_gpio(np,
						panel_extra_power_gpio, 0);
		if (!gpio_is_valid(vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i]))
			pr_err("%s:%d, panel_extra_power gpio%d not specified\n",
							__func__, __LINE__, i+1);
		else
			pr_err("extra gpio num : %d\n", vdd->dtsi_data[ctrl->ndx].panel_extra_power_gpio[i]);
	}

	/* Set tft backlight gpio */
	for (i = 0; i < MAX_BACKLIGHT_TFT_GPIO; i++) {
		backlight_tft_gpio[strlen(backlight_tft_gpio) - 1] = '1' + i;
		vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i] =
				 of_get_named_gpio(np,
						backlight_tft_gpio, 0);
		if (!gpio_is_valid(vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i]))
			pr_err("%s:%d, backlight_tft_gpio gpio%d not specified\n",
							__func__, __LINE__, i+1);
		else
			pr_err("tft gpio num : %d\n", vdd->dtsi_data[ctrl->ndx].backlight_tft_gpio[i]);
	}

	/* Set Mdnie lite HBM_CE_TEXT_MDNIE mode used */
	vdd->dtsi_data[ctrl->ndx].hbm_ce_text_mode_support  = of_property_read_bool(np, "samsung,hbm_ce_text_mode_support");

	/* Set Backlight IC discharge time */
	rc = of_property_read_u32(np, "samsung,blic-discharging-delay-us", tmp);
	vdd->dtsi_data[ctrl->ndx].blic_discharging_delay_tft = (!rc ? tmp[0] : 6);

	/* Set cabc delay time */
	rc = of_property_read_u32(np, "samsung,cabc-delay-us", tmp);
	vdd->dtsi_data[ctrl->ndx].cabc_delay = (!rc ? tmp[0] : 6);

	/* Change hw_config to support single, dual and split dsi on runtime */
	data = of_get_property(np, "samsung,hw-config", NULL);
	if (data) {
		if (!strcmp(data, "dual_dsi"))
			vdd->samsung_hw_config = DUAL_DSI;
		else if (!strcmp(data, "split_dsi"))
			vdd->samsung_hw_config = SPLIT_DSI;
		else if (!strcmp(data, "single_dsi"))
			vdd->samsung_hw_config = SINGLE_DSI;
	}

	mdss_samsung_panel_parse_dt_cmds(np, ctrl);
	if (vdd->support_hall_ic) {
		vdd->hall_ic_notifier_display.priority = 0; /* Tsp is 1, Touch key is 2 */
		vdd->hall_ic_notifier_display.notifier_call = samsung_display_hall_ic_status;
		/* hall_ic_register_notify(&vdd->hall_ic_notifier_display); */

		mdss_samsung_panel_parse_dt_cmds(np, ctrl);
		vdd->mdnie_tune_state_dsi[DISPLAY_2] = init_dsi_tcon_mdnie_class(DISPLAY_2, vdd);
	}

	mdss_samsung_panel_parse_dt_esd(np, ctrl);
	mdss_samsung_panel_pbaboot_config(np, ctrl);
}


/************************************************************
*
*		SYSFS RELATED FUNCTION
*
**************************************************************/
#if defined(CONFIG_LCD_CLASS_DEVICE)
static char char_to_dec(char data1, char data2)
{
	char dec;

	dec = 0;

	if (data1 >= 'a') {
		data1 -= 'a';
		data1 += 10;
	} else if (data1 >= 'A') {
		data1 -= 'A';
		data1 += 10;
	} else
		data1 -= '0';

	dec = data1 << 4;

	if (data2 >= 'a') {
		data2 -= 'a';
		data2 += 10;
	} else if (data2 >= 'A') {
		data2 -= 'A';
		data2 += 10;
	} else
		data2 -= '0';

	dec |= data2;

	return dec;
}

static void sending_tune_cmd(struct device *dev, char *src, int len)
{
	int data_pos;
	int cmd_step;
	int cmd_pos;

	char *mdnie_tuning1;
	char *mdnie_tuning2;
	char *mdnie_tuning3;
	char *mdnie_tuning4;
	char *mdnie_tuning5;
	char *mdnie_tuning6;

	struct dsi_cmd_desc *mdnie_tune_cmd;

	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error\n", __func__);
		return;
	}

	if (!vdd->mdnie_tune_size1 || !vdd->mdnie_tune_size2 || !vdd->mdnie_tune_size3) {
		pr_err("%s : mdnie_tune_size is zero 1(%d) 2(%d) 3(%d)\n",
			__func__, vdd->mdnie_tune_size1, vdd->mdnie_tune_size2, vdd->mdnie_tune_size3);
		return;
	}

	if (vdd->mdnie_tuning_enable_tft) {
		mdnie_tune_cmd = kzalloc(7 * sizeof(struct dsi_cmd_desc), GFP_KERNEL);
		mdnie_tuning1 = kzalloc(sizeof(char) * vdd->mdnie_tune_size1, GFP_KERNEL);
		mdnie_tuning2 = kzalloc(sizeof(char) * vdd->mdnie_tune_size2, GFP_KERNEL);
		mdnie_tuning3 = kzalloc(sizeof(char) * vdd->mdnie_tune_size3, GFP_KERNEL);
		mdnie_tuning4 = kzalloc(sizeof(char) * vdd->mdnie_tune_size4, GFP_KERNEL);
		mdnie_tuning5 = kzalloc(sizeof(char) * vdd->mdnie_tune_size5, GFP_KERNEL);
		mdnie_tuning6 = kzalloc(sizeof(char) * vdd->mdnie_tune_size6, GFP_KERNEL);

	} else {
		mdnie_tune_cmd = kzalloc(3 * sizeof(struct dsi_cmd_desc), GFP_KERNEL);
		mdnie_tuning1 = kzalloc(sizeof(char) * vdd->mdnie_tune_size1, GFP_KERNEL);
		mdnie_tuning2 = kzalloc(sizeof(char) * vdd->mdnie_tune_size2, GFP_KERNEL);
		mdnie_tuning3 = kzalloc(sizeof(char) * vdd->mdnie_tune_size3, GFP_KERNEL);
	}

	cmd_step = 0;
	cmd_pos = 0;

	for (data_pos = 0; data_pos < len;) {
		if (*(src + data_pos) == '0') {
			if (*(src + data_pos + 1) == 'x') {
				if (!cmd_step)
					mdnie_tuning1[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else if (cmd_step == 1)
					mdnie_tuning2[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else if (cmd_step == 2 && vdd->mdnie_tuning_enable_tft)
					mdnie_tuning3[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else if (cmd_step == 3 && vdd->mdnie_tuning_enable_tft)
					mdnie_tuning4[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else if (cmd_step == 4 && vdd->mdnie_tuning_enable_tft)
					mdnie_tuning5[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else if (cmd_step == 5 && vdd->mdnie_tuning_enable_tft)
					mdnie_tuning6[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));
				else
					mdnie_tuning3[cmd_pos] = char_to_dec(*(src + data_pos + 2), *(src + data_pos + 3));

				data_pos += 3;
				cmd_pos++;

				if (cmd_pos == vdd->mdnie_tune_size1 && !cmd_step) {
					cmd_pos = 0;
					cmd_step = 1;
				} else if ((cmd_pos == vdd->mdnie_tune_size2) && (cmd_step == 1)) {
					cmd_pos = 0;
					cmd_step = 2;
				} else if ((cmd_pos == vdd->mdnie_tune_size3) && (cmd_step == 2) && vdd->mdnie_tuning_enable_tft) {
					cmd_pos = 0;
					cmd_step = 3;
				} else if ((cmd_pos == vdd->mdnie_tune_size4) && (cmd_step == 3) && vdd->mdnie_tuning_enable_tft) {
					cmd_pos = 0;
					cmd_step = 4;
				} else if ((cmd_pos == vdd->mdnie_tune_size5) && (cmd_step == 4) && vdd->mdnie_tuning_enable_tft) {
					cmd_pos = 0;
					cmd_step = 5;
				}
			} else
				data_pos++;
		} else {
			data_pos++;
		}
	}

	mdnie_tune_cmd[0].dchdr.dtype = DTYPE_DCS_LWRITE;
	mdnie_tune_cmd[0].dchdr.last = 1;
	mdnie_tune_cmd[0].dchdr.dlen = vdd->mdnie_tune_size1;
	mdnie_tune_cmd[0].payload = mdnie_tuning1;

	mdnie_tune_cmd[1].dchdr.dtype = DTYPE_DCS_LWRITE;
	mdnie_tune_cmd[1].dchdr.last = 1;
	mdnie_tune_cmd[1].dchdr.dlen = vdd->mdnie_tune_size2;
	mdnie_tune_cmd[1].payload = mdnie_tuning2;

	mdnie_tune_cmd[2].dchdr.dtype = DTYPE_DCS_LWRITE;
	mdnie_tune_cmd[2].dchdr.last = 1;
	mdnie_tune_cmd[2].dchdr.dlen = vdd->mdnie_tune_size3;
	mdnie_tune_cmd[2].payload = mdnie_tuning3;

	printk(KERN_ERR "mdnie_tuning1 (%d)\n", vdd->mdnie_tune_size1);
	for (data_pos = 0; data_pos < vdd->mdnie_tune_size1 ; data_pos++)
		printk(KERN_ERR "0x%x \n", mdnie_tuning1[data_pos]);
	printk(KERN_ERR "mdnie_tuning2 (%d)\n", vdd->mdnie_tune_size2);
	for (data_pos = 0; data_pos < vdd->mdnie_tune_size2 ; data_pos++)
		printk(KERN_ERR "0x%x \n", mdnie_tuning2[data_pos]);
	printk(KERN_ERR "mdnie_tuning3 (%d)\n", vdd->mdnie_tune_size3);
	for (data_pos = 0; data_pos < vdd->mdnie_tune_size3 ; data_pos++)
		printk(KERN_ERR "0x%x \n", mdnie_tuning3[data_pos]);

	if (vdd->mdnie_tuning_enable_tft) {
		mdnie_tune_cmd[3].dchdr.dtype = DTYPE_DCS_LWRITE;
		mdnie_tune_cmd[3].dchdr.last = 1;
		mdnie_tune_cmd[3].dchdr.dlen = vdd->mdnie_tune_size4;
		mdnie_tune_cmd[3].payload = mdnie_tuning4;

		mdnie_tune_cmd[4].dchdr.dtype = DTYPE_DCS_LWRITE;
		mdnie_tune_cmd[4].dchdr.last = 1;
		mdnie_tune_cmd[4].dchdr.dlen = vdd->mdnie_tune_size5;
		mdnie_tune_cmd[4].payload = mdnie_tuning5;

		mdnie_tune_cmd[5].dchdr.dtype = DTYPE_DCS_LWRITE;
		mdnie_tune_cmd[5].dchdr.last = 1;
		mdnie_tune_cmd[5].dchdr.dlen = vdd->mdnie_tune_size6;
		mdnie_tune_cmd[5].payload = mdnie_tuning6;

		printk(KERN_ERR "\n");
		for (data_pos = 0; data_pos < vdd->mdnie_tune_size3 ; data_pos++)
			printk(KERN_ERR "0x%x ", mdnie_tuning3[data_pos]);
		printk(KERN_ERR "\n");
		for (data_pos = 0; data_pos < vdd->mdnie_tune_size4 ; data_pos++)
			printk(KERN_ERR "0x%x ", mdnie_tuning4[data_pos]);
		printk(KERN_ERR "\n");
		for (data_pos = 0; data_pos < vdd->mdnie_tune_size5 ; data_pos++)
			printk(KERN_ERR "0x%x ", mdnie_tuning5[data_pos]);
		printk(KERN_ERR "\n");
		for (data_pos = 0; data_pos < vdd->mdnie_tune_size6 ; data_pos++)
			printk(KERN_ERR "0x%x ", mdnie_tuning6[data_pos]);
		printk(KERN_ERR "\n");
	}

	if (IS_ERR_OR_NULL(vdd))
		pr_err("%s vdd is error", __func__);
	else {
		if ((vdd->ctrl_dsi[DSI_CTRL_0]->cmd_sync_wait_broadcast)
			&& (vdd->ctrl_dsi[DSI_CTRL_1]->cmd_sync_wait_trigger)) { /* Dual DSI & dsi 1 trigger */
			if (!IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].level1_key_enable_tx_cmds[vdd->panel_revision].cmds))
				mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_1], PANEL_LEVE1_KEY_ENABLE);
			if (!IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].level2_key_enable_tx_cmds[vdd->panel_revision].cmds))
				mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_1], PANEL_LEVE2_KEY_ENABLE);

			/* Set default link_stats as DSI_HS_MODE for mdnie tune data */
			vdd->mdnie_tune_data[DSI_CTRL_1].mdnie_tune_packet_tx_cmds_dsi.link_state = DSI_HS_MODE;
			vdd->mdnie_tune_data[DSI_CTRL_1].mdnie_tune_packet_tx_cmds_dsi.cmds = mdnie_tune_cmd;
			vdd->mdnie_tune_data[DSI_CTRL_1].mdnie_tune_packet_tx_cmds_dsi.cmd_cnt = 3;
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_1], PANEL_MDNIE_TUNE);

			if (!IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].level1_key_disable_tx_cmds[vdd->panel_revision].cmds))
				mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_1], PANEL_LEVE1_KEY_DISABLE);
			if (!IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].level2_key_disable_tx_cmds[vdd->panel_revision].cmds))
				mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_1], PANEL_LEVE2_KEY_DISABLE);
		} else { /* Single DSI, dsi 0 trigger */
			if (!IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].level1_key_enable_tx_cmds[vdd->panel_revision].cmds))
				mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_0], PANEL_LEVE1_KEY_ENABLE);

			/* Set default link_stats as DSI_HS_MODE for mdnie tune data */
			vdd->mdnie_tune_data[DSI_CTRL_0].mdnie_tune_packet_tx_cmds_dsi.link_state = DSI_HS_MODE;
			vdd->mdnie_tune_data[DSI_CTRL_0].mdnie_tune_packet_tx_cmds_dsi.cmds = mdnie_tune_cmd;
			vdd->mdnie_tune_data[DSI_CTRL_0].mdnie_tune_packet_tx_cmds_dsi.cmd_cnt = vdd->mdnie_tuning_enable_tft ? 6 : 3;
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_0], PANEL_MDNIE_TUNE);

			if (!IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].level1_key_disable_tx_cmds[vdd->panel_revision].cmds))
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DSI_CTRL_0], PANEL_LEVE1_KEY_DISABLE);
		}
	}

	if (vdd->mdnie_tuning_enable_tft) {
		kfree(mdnie_tune_cmd);
		kfree(mdnie_tuning1);
		kfree(mdnie_tuning2);
		kfree(mdnie_tuning3);
		kfree(mdnie_tuning4);
		kfree(mdnie_tuning5);
		kfree(mdnie_tuning6);

	} else {
		kfree(mdnie_tune_cmd);
		kfree(mdnie_tuning1);
		kfree(mdnie_tuning2);
		kfree(mdnie_tuning3);
	}
}

static void load_tuning_file(struct device *dev, char *filename)
{
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int ret;
	mm_segment_t fs;

	pr_info("%s called loading file name : [%s]\n", __func__,
	       filename);

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk(KERN_ERR "%s File open failed\n", __func__);
		return;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	pr_info("%s Loading File Size : %ld(bytes)", __func__, l);

	dp = kmalloc(l + 10, GFP_KERNEL);
	if (dp == NULL) {
		pr_info("Can't not alloc memory for tuning file load\n");
		filp_close(filp, current->files);
		return;
	}
	pos = 0;
	memset(dp, 0, l);

	pr_info("%s before vfs_read()\n", __func__);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	pr_info("%s after vfs_read()\n", __func__);

	if (ret != l) {
		pr_info("vfs_read() filed ret : %d\n", ret);
		kfree(dp);
		filp_close(filp, current->files);
		return;
	}

	filp_close(filp, current->files);

	set_fs(fs);

	sending_tune_cmd(dev, dp, l);

	kfree(dp);
}

static ssize_t tuning_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret = snprintf(buf, MAX_FILE_NAME, "Tunned File Name : %s\n", tuning_file);
	return ret;
}

static ssize_t tuning_store(struct device *dev,
			    struct device_attribute *attr, const char *buf,
			    size_t size)
{
	char *pt;

	memset(tuning_file, 0, sizeof(tuning_file));
	snprintf(tuning_file, MAX_FILE_NAME, "%s%s", TUNING_FILE_PATH, buf);

	pt = tuning_file;

	while (*pt) {
		if (*pt == '\r' || *pt == '\n') {
			*pt = 0;
			break;
		}
		pt++;
	}

	pr_info("%s:%s\n", __func__, tuning_file);

	load_tuning_file(dev, tuning_file);

	return size;
}

static DEVICE_ATTR(tuning, 0664, tuning_show, tuning_store);
#endif

/* #define OTHER_LINE_PANEL_SUPPORT_DEBUG */
#define A3_PANEL_FILE "/sdcard/test.dat"

#define MAX_BUF_SIZE 256
#define BASIC_DATA_ELEMENT_CNT 7
#define REF_SHIFT_ELEMENT_CNT 10
#define COLOR_SHIFT_ELEMENT_CNT 28
#define ELVSS_LOW_TEMP_ELEMENT_CNT 6
#define VINT_ELEMENT_CNT 12
#define AOR_INTERPOLATION_ELEMENT_CNT 4

enum {
	BASIC_DATA = 0,
	REF_SHIFT,
	COLOR_SHIFT,
	ELVSS_LOW_TEMP,
	VINT,
	AOR_INTERPOLATION,
};

int parse_basic_data(struct samsung_display_driver_data *vdd, char *buf)
{
	int ret;
	int idx, br, base_br, gamma;
	int aid[3];

	pr_debug("%s ++\n", __func__);

	ret = sscanf(buf, "%d, %d, %d, %d, %x, %x, %x",
						&idx, &br, &base_br, &gamma,
						&aid[0], &aid[1], &aid[2]);

	if (ret != BASIC_DATA_ELEMENT_CNT) {
		pr_err("%s : invalid arg.. ret(%d)\n", __func__, ret);
		return -EINVAL;
	}

#ifdef OTHER_LINE_PANEL_SUPPORT_DEBUG
	pr_err("%s : idx(%d), br(%d), base_br(%d), gamma(%d), aid[0](%x), aid[1](%x), aid[2](%x)\n", __func__,
				idx, br, base_br, gamma, aid[0], aid[1], aid[2]);
#endif

	pr_debug("%s --\n", __func__);

	return ret;
}

int parse_ref_shift(struct samsung_display_driver_data *vdd, char *buf)
{
	int ret;
	int idx;
	int shift[9];

	pr_debug("%s ++\n", __func__);

	ret = sscanf(buf, "%d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
						&idx, &shift[0], &shift[1], &shift[2], &shift[3], &shift[4],
						&shift[5], &shift[6], &shift[7], &shift[8]);

	if (ret != REF_SHIFT_ELEMENT_CNT) {
		pr_err("%s : invalid arg.. ret(%d)\n", __func__, ret);
		return -EINVAL;
	}

#ifdef OTHER_LINE_PANEL_SUPPORT_DEBUG
	pr_err("%s : idx(%d), %d, %d, %d, %d, %d, %d, %d, %d, %d\n", __func__,
				idx, shift[0], shift[1], shift[2], shift[3], shift[4],
						shift[5], shift[6], shift[7], shift[8]);
#endif

	pr_debug("%s --\n", __func__);

	return ret;
}

int parse_color_shift(struct samsung_display_driver_data *vdd, char *buf)
{
	int ret;
	int idx;
	int shift[27];

	pr_debug("%s ++\n", __func__);

	ret = sscanf(buf, "%d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\
							%d, %d, %d, %d, %d, %d, %d, %d, %d,\
							%d, %d, %d, %d, %d, %d, %d, %d, %d",
						&idx, &shift[0], &shift[1], &shift[2], &shift[3], &shift[4],
							  &shift[5], &shift[6], &shift[7], &shift[8], &shift[9],
							  &shift[10], &shift[11], &shift[12], &shift[13], &shift[14],
							  &shift[15], &shift[16], &shift[17], &shift[18], &shift[19],
							  &shift[20], &shift[21], &shift[22], &shift[23], &shift[24],
							  &shift[25], &shift[26]);

	if (ret != COLOR_SHIFT_ELEMENT_CNT) {
		pr_err("%s : invalid arg.. ret(%d)\n", __func__, ret);
		return -EINVAL;
	}

#ifdef OTHER_LINE_PANEL_SUPPORT_DEBUG
	pr_err("%s : idx(%d), %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n", __func__,
				idx, shift[0], shift[1], shift[2], shift[3], shift[4],
					  shift[5], shift[6], shift[7], shift[8], shift[9],
					  shift[10], shift[11], shift[12], shift[13], shift[14],
					  shift[15], shift[16], shift[17], shift[18], shift[19],
					  shift[20], shift[21], shift[22], shift[23], shift[24],
					  shift[25], shift[26]);
#endif

	pr_debug("%s --\n", __func__);

	return ret;
}

int parse_elvss_low_temp(struct samsung_display_driver_data *vdd, char *buf)
{
	int ret;
	int idx;
	int elvss[5];

	pr_err("%s ++\n", __func__);

	ret = sscanf(buf, "%d, %d, %d, %d, %d, %d",
						&idx, &elvss[0], &elvss[1], &elvss[2], &elvss[3], &elvss[4]);

	if (ret != ELVSS_LOW_TEMP_ELEMENT_CNT) {
		pr_err("%s : invalid arg.. ret(%d)\n", __func__, ret);
		return -EINVAL;
	}

#ifdef OTHER_LINE_PANEL_SUPPORT_DEBUG
	pr_err("%s : idx(%d), %d, %d, %d, %d, %d\n", __func__,
				idx, elvss[0], elvss[1], elvss[2], elvss[3], elvss[4]);

	pr_err("%s --\n", __func__);
#endif

	return ret;
}

int parse_vint_tbl(struct samsung_display_driver_data *vdd, char *buf)
{
	int ret;
	int idx;
	int vint[11];

	pr_debug("%s ++\n", __func__);

	ret = sscanf(buf, "%d, %x, %x, %x, %x, %x,\
							%x, %x, %x, %x, %x, %x",
						&idx, &vint[0], &vint[1], &vint[2], &vint[3], &vint[4],
								&vint[5], &vint[6], &vint[7], &vint[8], &vint[9], &vint[10]);

	if (ret != VINT_ELEMENT_CNT) {
		pr_err("%s : invalid arg.. ret(%d)\n", __func__, ret);
		return -EINVAL;
	}

#ifdef OTHER_LINE_PANEL_SUPPORT_DEBUG
	pr_err("%s : idx(%d),  %x, %x, %x, %x, %x, %x, %x, %x, %x, %x, %x", __func__,
						idx, vint[0], vint[1], vint[2], vint[3], vint[4],
								vint[5], vint[6], vint[7], vint[8], vint[9], vint[10]);

	pr_debug("%s --\n", __func__);
#endif

	return ret;
}

int parse_aor_interpolation(struct samsung_display_driver_data *vdd, char *buf)
{
	int ret;
	int idx;
	int aor[4];

	pr_debug("%s ++\n", __func__);

	ret = sscanf(buf, "%d, %d, %x, %x",
						&idx, &aor[0], &aor[1], &aor[2]);

	if (ret != AOR_INTERPOLATION_ELEMENT_CNT) {
		pr_err("%s : invalid arg.. ret(%d)\n", __func__, ret);
		return -EINVAL;
	}

#ifdef OTHER_LINE_PANEL_SUPPORT_DEBUG
	pr_err("%s : idx(%d),  %d, %x, %x", __func__,
						idx, aor[0], aor[1], aor[2]);
#endif

	pr_debug("%s --\n", __func__);

	return ret;
}

int read_line(char *src, char *buf, int *pos, int len)
{
	int idx = 0;

	pr_debug("%s (%d) ++\n", __func__, *pos);

	while (*(src + *pos) != 10 && *(src + *pos) != 13) {
		buf[idx] = *(src + *pos);

#ifdef OTHER_LINE_PANEL_SUPPORT_DEBUG
		pr_err("%s : %c (%3d) idx(%d)\n", __func__, buf[idx], buf[idx], idx);
#endif
		idx++;
		(*pos)++;

		if (idx > MAX_BUF_SIZE) {
			pr_err("%s : overflow!\n", __func__);
			return idx;
		}

		if (*pos >= len) {
			pr_err("%s : End of File (%d) / (%d)\n", __func__, *pos, len);
			return idx;
		}
	}

	while (*(src + *pos) == 10 || *(src + *pos) == 13)
		(*pos)++;

	pr_debug("%s --\n", __func__);

	return idx;
}

int parsing_other_line_panel_data(struct file *f, struct samsung_display_driver_data *vdd,
		char *src, int len)
{
	int pos = 0;
	int read_size;
	int fn_idx = 0;
	char read_buf[MAX_BUF_SIZE];
	int ret = 0;

	int (*parse_fn[6])(struct samsung_display_driver_data *, char *) = {
		parse_basic_data,
		parse_ref_shift,
		parse_color_shift,
		parse_elvss_low_temp,
		parse_vint_tbl,
		parse_aor_interpolation,
	};

	pr_err("%s : ++\n", __func__);

	while (pos < len) {
		memset(read_buf, 0, MAX_BUF_SIZE);

		read_size = read_line(src, read_buf, &pos, len);

		read_buf[MAX_BUF_SIZE-1] = 0;

		pr_err("%s : read_buf (%s) cnt (%d) pos(%d)\n", __func__, read_buf, read_size, pos);

		if (!strncmp(read_buf, "basic data", read_size)) {
			pr_err("basic data!\n");
			fn_idx = BASIC_DATA;
		} else if (!strncmp(read_buf, "reference shift data", read_size)) {
			pr_err("reference shift data!\n");
			fn_idx = REF_SHIFT;
		} else if (!strncmp(read_buf, "color shift data", read_size)) {
			pr_err("color shift data!\n");
			fn_idx = COLOR_SHIFT;
		} else if (!strncmp(read_buf, "elvss low temperature", read_size)) {
			pr_err("elvss low temperature!\n");
			fn_idx = ELVSS_LOW_TEMP;
		} else if (!strncmp(read_buf, "vint data", read_size)) {
			pr_err("vint data!\n");
			fn_idx = VINT;
		} else if (!strncmp(read_buf, "aor interpolation", read_size)) {
			pr_err("aor interpolation!\n");
			fn_idx = AOR_INTERPOLATION;
		} else
			ret = parse_fn[fn_idx](vdd, read_buf);

		if (pos >= len) {
			pr_err("%s : End of File (%d) / (%d)\n", __func__, pos, len);
			goto end;
		}
	}

end:
	pr_err("%s : --\n", __func__);

	return ret;
}

int mdss_samsung_read_a3_panel_data(struct samsung_display_driver_data *vdd)
{
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int ret = 0;
	mm_segment_t fs;

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(A3_PANEL_FILE, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk(KERN_ERR "%s File open failed\n", __func__);
		return -ENOENT;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	pr_info("%s Loading File Size : %ld(bytes)", __func__, l);

	dp = kmalloc(l + 10, GFP_KERNEL);
	if (dp == NULL) {
		pr_info("Can't not alloc memory for tuning file load\n");
		filp_close(filp, current->files);
		return -1;
	}
	pos = 0;
	memset(dp, 0, l);

	pr_info("%s before vfs_read()\n", __func__);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	pr_info("%s after vfs_read()\n", __func__);

	if (ret != l) {
		pr_info("vfs_read() filed ret : %d\n", ret);
		kfree(dp);
		filp_close(filp, current->files);
		return -1;
	}

	ret = parsing_other_line_panel_data(filp, vdd, dp, l);

	filp_close(filp, current->files);

	set_fs(fs);

	kfree(dp);

	return ret;
}

void other_read_panel_data_work_fn(struct delayed_work *work)
{
	int ret;

	pr_err("%s : ++\n", __func__);

	ret = mdss_samsung_read_a3_panel_data(&vdd_data);

	if (ret && vdd_data.other_line_panel_work_cnt) {
		queue_delayed_work(vdd_data.other_line_panel_support_workq,
						&vdd_data.other_line_panel_support_work, msecs_to_jiffies(500));
		vdd_data.other_line_panel_work_cnt--;
	} else
		destroy_workqueue(vdd_data.other_line_panel_support_workq);

	pr_err("%s : --\n", __func__);
}

static ssize_t mdss_samsung_disp_cell_id_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	static int string_size = 50;
	char temp[string_size];
	int *cell_id;
	int ndx;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return strnlen(buf, string_size);
	}

	ndx = display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0]);

	cell_id = &vdd->cell_id_dsi[ndx][0];

	/*
	*	STANDARD FORMAT (Total is 11Byte)
	*	MAX_CELL_ID : 11Byte
	*	7byte(cell_id) + 2byte(Mdnie x_postion) + 2byte(Mdnie y_postion)
	*/

	snprintf((char *)temp, sizeof(temp),
			"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
		cell_id[0], cell_id[1], cell_id[2], cell_id[3], cell_id[4],
		cell_id[5], cell_id[6],
		(vdd->mdnie_x[ndx] & 0xFF00) >> 8,
		vdd->mdnie_x[ndx] & 0xFF,
		(vdd->mdnie_y[ndx] & 0xFF00) >> 8,
		vdd->mdnie_y[ndx] & 0xFF);

	strlcat(buf, temp, string_size);

	pr_info("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		cell_id[0], cell_id[1], cell_id[2], cell_id[3], cell_id[4],
		cell_id[5], cell_id[6],
		(vdd->mdnie_x[ndx] & 0xFF00) >> 8,
		vdd->mdnie_x[ndx] & 0xFF,
		(vdd->mdnie_y[ndx] & 0xFF00) >> 8,
		vdd->mdnie_y[ndx] & 0xFF);

	return strnlen(buf, string_size);
}

static ssize_t mdss_samsung_disp_lcdtype_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	static int string_size = 100;
	char temp[string_size];
	int ndx;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return strnlen(buf, string_size);
	}

	ndx = display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0]);

	if (vdd->dtsi_data[ndx].tft_common_support && vdd->dtsi_data[ndx].tft_module_name) {
		if (vdd->dtsi_data[ndx].panel_vendor)
			snprintf(temp, 20, "%s_%s\n", vdd->dtsi_data[ndx].panel_vendor, vdd->dtsi_data[ndx].tft_module_name);
		else
			snprintf(temp, 20, "SDC_%s\n", vdd->dtsi_data[ndx].tft_module_name);
	} else if (mdss_panel_attached(ndx)) {
		if (vdd->dtsi_data[ndx].panel_vendor)
			snprintf(temp, 20, "%s_%06x\n", vdd->dtsi_data[ndx].panel_vendor, vdd->manufacture_id_dsi[ndx]);
		else
			snprintf(temp, 20, "SDC_%06x\n", vdd->manufacture_id_dsi[ndx]);
	} else {
		LCD_INFO("no manufacture id\n");
		snprintf(temp, 20, "SDC_000000\n");
	}

	strlcat(buf, temp, string_size);

	return strnlen(buf, string_size);
}

static ssize_t mdss_samsung_disp_windowtype_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static int string_size = 15;
	char temp[string_size];
	int id, id1, id2, id3;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return strnlen(buf, string_size);
	}

	id = vdd->manufacture_id_dsi[display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0])];

	id1 = (id & 0x00FF0000) >> 16;
	id2 = (id & 0x0000FF00) >> 8;
	id3 = id & 0xFF;

	pr_info("%s ndx : %d %02x %02x %02x\n", __func__,
		display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0]), id1, id2, id3);

	snprintf(temp, sizeof(temp), "%02x %02x %02x\n", id1, id2, id3);

	strlcat(buf, temp, string_size);

	return strnlen(buf, string_size);
}

static ssize_t mdss_samsung_disp_manufacture_date_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static int string_size = 30;
	char temp[string_size];
	int date;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return strnlen(buf, string_size);
	}

	date = vdd->manufacture_date_dsi[display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0])];
	snprintf((char *)temp, sizeof(temp), "manufacture date : %d\n", date);

	strlcat(buf, temp, string_size);

	pr_info("manufacture date : %d\n", date);

	return strnlen(buf, string_size);
}

static ssize_t mdss_samsung_disp_manufacture_code_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static int string_size = 30;
	char temp[string_size];
	int *ddi_id;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return strnlen(buf, string_size);
	}

	ddi_id = &vdd->ddi_id_dsi[display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0])][0];

	snprintf((char *)temp, sizeof(temp), "%02x%02x%02x%02x%02x\n",
		ddi_id[0], ddi_id[1], ddi_id[2], ddi_id[3], ddi_id[4]);

	strlcat(buf, temp, string_size);

	pr_info("%s : %02x %02x %02x %02x %02x\n", __func__,
		ddi_id[0], ddi_id[1], ddi_id[2], ddi_id[3], ddi_id[4]);

	return strnlen(buf, string_size);
}

static ssize_t mdss_samsung_disp_acl_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return rc;
	}

	rc = snprintf((char *)buf, sizeof(vdd->acl_status), "%d\n", vdd->acl_status);

	pr_info("acl status: %d\n", *buf);

	return rc;
}

static ssize_t mdss_samsung_disp_acl_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int acl_set = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		return size;
	}

	if (sysfs_streq(buf, "1"))
		acl_set = true;
	else if (sysfs_streq(buf, "0"))
		acl_set = false;
	else
		pr_info("%s: Invalid argument!!", __func__);

	pr_info("%s (%d)\n", __func__, acl_set);

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;
	mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	if (acl_set && !vdd->acl_status) {
		vdd->acl_status = acl_set;
		pdata->set_backlight(pdata, vdd->bl_level);
	} else if (!acl_set && vdd->acl_status) {
		vdd->acl_status = acl_set;
		pdata->set_backlight(pdata, vdd->bl_level);
	} else {
		vdd->acl_status = acl_set;
		pr_info("%s: skip acl update!! acl %d", __func__, vdd->acl_status);
	}

	mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	return size;
}

static ssize_t mdss_samsung_disp_siop_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return rc;
	}

	rc = snprintf((char *)buf, sizeof(vdd->siop_status), "%d\n", vdd->siop_status);

	pr_info("siop status: %d\n", *buf);

	return rc;
}

static ssize_t mdss_samsung_disp_siop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int siop_set = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;


	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		return size;
	}

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;

	if (sysfs_streq(buf, "1"))
		siop_set = true;
	else if (sysfs_streq(buf, "0"))
		siop_set = false;
	else
		pr_info("%s: Invalid argument!!", __func__);

	pr_info("%s (%d)\n", __func__, siop_set);

	mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	if (siop_set && !vdd->siop_status) {
		vdd->siop_status = siop_set;
		pdata->set_backlight(pdata, vdd->bl_level);
	} else if (!siop_set && vdd->siop_status) {
		vdd->siop_status = siop_set;
		pdata->set_backlight(pdata, vdd->bl_level);
	} else {
		vdd->siop_status = siop_set;
		pr_info("%s: skip siop update!! acl %d", __func__, vdd->acl_status);
	}

	mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	return size;
}

static ssize_t mdss_samsung_aid_log_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int loop = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return rc;
	}

	for (loop = 0; loop < vdd->support_panel_max; loop++) {
		if (vdd->smart_dimming_dsi[loop] && vdd->smart_dimming_dsi[loop]->print_aid_log)
			vdd->smart_dimming_dsi[loop]->print_aid_log(vdd->smart_dimming_dsi[loop]);
		else
			pr_err("%s DSI%d smart dimming is not loaded\n", __func__, loop);
	}

	if (vdd->dtsi_data[0].hmt_enabled) {
		for (loop = 0; loop < vdd->support_panel_max; loop++) {
			if (vdd->smart_dimming_dsi_hmt[loop] && vdd->smart_dimming_dsi_hmt[loop]->print_aid_log)
				vdd->smart_dimming_dsi_hmt[loop]->print_aid_log(vdd->smart_dimming_dsi_hmt[loop]);
			else
				pr_err("%s DSI%d smart dimming hmt is not loaded\n", __func__, loop);
		}
	}

	return rc;
}

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
static ssize_t mdss_samsung_auto_brightness_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return rc;
	}

	rc = snprintf((char *)buf, sizeof(vdd->auto_brightness), "%d\n", vdd->auto_brightness);

	pr_info("%s:auto_brightness: %d\n", __func__, *buf);

	return rc;
}

static ssize_t mdss_samsung_auto_brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;
	int input;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		goto end;
	}

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;

	sscanf(buf, "%d" , &input);

	/* vdd->auto_brightness 6 ~ 13 read to hbm_mode 1*/
	vdd->auto_brightness = input;

	if (input > vdd->auto_brightness_level) {
		pr_info("%s: Invalid argument!!", __func__);
		return size;
	}

	pr_info("%s (%d)\n", __func__, vdd->auto_brightness);

	if (!vdd->dtsi_data[DISPLAY_1].tft_common_support) {
		mutex_lock(&vdd_data.vdd_bl_level_lock);
		mdss_samsung_brightness_dcs(vdd->ctrl_dsi[DISPLAY_1], vdd->bl_level);
		mutex_unlock(&vdd_data.vdd_bl_level_lock);
	}
	if (vdd->mdss_panel_tft_outdoormode_update)
		vdd->mdss_panel_tft_outdoormode_update(vdd->ctrl_dsi[DISPLAY_1]);
	else if (vdd->support_cabc)
		mdss_tft_autobrightness_cabc_update(vdd->ctrl_dsi[DISPLAY_1]);

	if (vdd->support_mdnie_lite)
		update_dsi_tcon_mdnie_register(vdd);

end:
	return size;
}

static ssize_t mdss_samsung_auto_brightness_level(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return rc;
	}

	rc = snprintf((char *)buf, 10, "%d\n", vdd->auto_brightness_level);

	pr_info("%s auto_brightness_step : %d", __func__, vdd->auto_brightness_level);

	return rc;
}

static ssize_t mdss_samsung_disp_brightness_step(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int ndx;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return rc;
	}

	ndx = display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0]);

	rc = snprintf((char *)buf, 20, "%d\n", vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].lux_tab_size);

	pr_info("%s brightness_step : %d", __func__, vdd->dtsi_data[ndx].candela_map_table[vdd->panel_revision].lux_tab_size);

	return rc;
}

static ssize_t mdss_samsung_disp_color_weakness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int ndx;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return rc;
	}

	ndx = display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0]);

	rc = snprintf((char *)buf, 20, "%d %d\n", vdd->color_weakness_mode, vdd->color_weakness_level);

	pr_info("%s color weakness : %d %d", __func__, vdd->color_weakness_mode, vdd->color_weakness_level);

	return rc;
}

static ssize_t mdss_samsung_disp_color_weakness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;
	unsigned int mode, level, value = 0;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		goto end;
	}

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;

	sscanf(buf, "%x %x" , &mode, &level);

	if (mode >= 0 && mode <= 9)
		vdd->color_weakness_mode = mode;
	else
		pr_err("%s : mode (%x) is not correct !\n", __func__, mode);

	if (level >= 0 && level <= 9)
		vdd->color_weakness_level = level;
	else
		pr_err("%s : level (%x) is not correct !\n", __func__, level);

	value = level << 4 | mode;

	pr_err("%s : level (%x) mode (%x) value (%x)\n", __func__, level, mode, value);

	vdd->dtsi_data[DISPLAY_1].ccb_on_tx_cmds[vdd->panel_revision].cmds[1].payload[1] = value;

	if (mode)
		mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_COLOR_WEAKNESS_ENABLE);
	else
		mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_COLOR_WEAKNESS_DISABLE);

end:
	return size;
}

static ssize_t mdss_samsung_weakness_hbm_comp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int ndx;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return rc;
	}

	ndx = display_ndx_check(vdd->ctrl_dsi[DSI_CTRL_0]);

	rc = snprintf((char *)buf, 20, "%d\n", vdd->weakness_hbm_comp);

	pr_info("%s color weakness : %d", __func__, vdd->weakness_hbm_comp);

	return rc;
}

static ssize_t mdss_samsung_weakness_hbm_comp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		return size;
 	}

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;

	if (sysfs_streq(buf, "0"))
		vdd->weakness_hbm_comp = 0;
	else if (sysfs_streq(buf, "1"))
		vdd->weakness_hbm_comp = 1;
	else if (sysfs_streq(buf, "2"))
		vdd->weakness_hbm_comp = 2;
	else if (sysfs_streq(buf, "3"))
	    vdd->weakness_hbm_comp = 3;
	else
		pr_info("%s: Invalid argument!!", __func__);

	pr_info("%s (%d) \n", __func__, vdd->weakness_hbm_comp);

	/* AMOLED Only */
	if (!vdd->dtsi_data[DISPLAY_1].tft_common_support) {
		mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
		pdata->set_backlight(pdata, vdd->bl_level);
		mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
	}

	return size;
}

#endif

static ssize_t mdss_samsung_read_mtp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int addr, len, start;
	char *read_buf = NULL;
	char read_size, read_startoffset;
	struct dsi_panel_cmds *rx_cmds;
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		return size;
	}
	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (IS_ERR_OR_NULL(ctrl)) {
		pr_err("%s ctrl is error", __func__);
		return size;
	}

	sscanf(buf, "%x %d %x" , &addr, &len, &start);

	read_buf = kmalloc(len * sizeof(char), GFP_KERNEL);

	pr_info("%x %d %x\n", addr, len, start);

	rx_cmds = &(vdd->dtsi_data[display_ndx_check(ctrl)].mtp_read_sysfs_rx_cmds[vdd->panel_revision]);

	rx_cmds->cmds[0].payload[0] =  addr;
	rx_cmds->cmds[0].payload[1] =  len;
	rx_cmds->cmds[0].payload[2] =  start;

	read_size = len;
	read_startoffset = start;

	rx_cmds->read_size =  &read_size;
	rx_cmds->read_startoffset =  &read_startoffset;


	pr_info("%x %x %x %x %x %x %x %x %x\n",
		rx_cmds->cmds[0].dchdr.dtype,
		rx_cmds->cmds[0].dchdr.last,
		rx_cmds->cmds[0].dchdr.vc,
		rx_cmds->cmds[0].dchdr.ack,
		rx_cmds->cmds[0].dchdr.wait,
		rx_cmds->cmds[0].dchdr.dlen,
		rx_cmds->cmds[0].payload[0],
		rx_cmds->cmds[0].payload[1],
		rx_cmds->cmds[0].payload[2]);

	mdss_samsung_panel_data_read(ctrl, rx_cmds, read_buf, PANEL_LEVE1_KEY | PANEL_LEVE2_KEY);

	kfree(read_buf);

	return size;
}

static ssize_t mdss_samsung_temperature_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return rc;
	}

	rc = snprintf((char *)buf, 40, "-20, -19, 0, 1, 30, 40\n");

	pr_info("%s temperature : %d", __func__, vdd->temperature);

	return rc;
}

static ssize_t mdss_samsung_temperature_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;
	int pre_temp = 0;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		return size;
	}

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;
	pre_temp = vdd->temperature;

	sscanf(buf, "%d" , &vdd->temperature);

	/* When temperature changed, hbm_mode must setted 0 for EA8061 hbm setting. */
	if (pre_temp != vdd->temperature && vdd->display_status_dsi[DISPLAY_1].hbm_mode == 1)
		vdd->display_status_dsi[DISPLAY_1].hbm_mode = 0;

	mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
	pdata->set_backlight(pdata, vdd->bl_level);
	mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);

	pr_info("%s temperature : %d\n", __func__, vdd->temperature);

	return size;
}

static ssize_t mdss_samsung_read_copr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static int string_size = 70;
	char temp[string_size];
	char copr[10] = {0};

	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return strnlen(buf, string_size);
	}

	mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_SPI_ENABLE);

	if (vdd->panel_func.samsung_spi_read_reg) {
		mutex_lock(&vdd->vdd_lock);
		memcpy(copr, vdd->panel_func.samsung_spi_read_reg(), sizeof(copr));
		mutex_unlock(&vdd->vdd_lock);
	}

	pr_info("%s %x %x %x %x %x %x %x %x %x %x\n", __func__,
			copr[0], copr[1], copr[2], copr[3], copr[4],
			copr[5], copr[6], copr[7], copr[8], copr[9]);

	snprintf((char *)temp, sizeof(temp), "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		copr[0], copr[1], copr[2], copr[3], copr[4], copr[5], copr[6], copr[7], copr[8], copr[9]);

	strlcat(buf, temp, string_size);

	return strnlen(buf, string_size);

}

static ssize_t mdss_samsung_disp_partial_disp_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	pr_debug("TDB");

	return 0;
}

static ssize_t mdss_samsung_disp_partial_disp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	pr_debug("TDB");

	return size;
}

static ssize_t mdss_samsung_panel_lpm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct samsung_display_driver_data *vdd =
	(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct panel_func *pfunc;
	u8 current_status = 0;

	pfunc = &vdd->panel_func;

	if (IS_ERR_OR_NULL(pfunc)) {
		pr_err("%s pfunc is error", __func__);
		return rc;
	}

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (vdd->dtsi_data[ctrl->ndx].panel_lpm_enable)
		current_status = vdd->panel_lpm.mode;

	rc = snprintf((char *)buf, 30, "%d\n", current_status);
	LCD_INFO("[Panel LPM] Current status : %d\n", (int)current_status);

	return rc;
}

void mdss_samsung_panel_low_power_config(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = NULL;

	if (IS_ERR_OR_NULL(pdata)) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	vdd = check_valid_ctrl(ctrl);

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (!vdd->dtsi_data[ctrl->ndx].panel_lpm_enable) {
		LCD_INFO("[Panel LPM] LPM(ALPM/HLPM) is not supported\n");
		return;
	}

	mdss_samsung_panel_lpm_ctrl(pdata, MDSS_SAMSUNG_EVENT_LP);
}

static void mdss_samsung_panel_lpm_hz_ctrl(struct mdss_panel_data *pdata, int aod_ctrl)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct samsung_display_driver_data *vdd = NULL;
	struct mdss_panel_info *pinfo = NULL;

	if (IS_ERR_OR_NULL(pdata)) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	vdd = check_valid_ctrl(ctrl);

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (!vdd->dtsi_data[ctrl->ndx].panel_lpm_enable) {
		LCD_INFO("[Panel LPM] LPM(ALPM/HLPM) is not supported\n");
		return;
	}

	if (pinfo->blank_state == MDSS_PANEL_BLANK_BLANK) {
		LCD_INFO("[Panel LPM] Do not change Hz\n");
		/*return;*/
	}

	switch (aod_ctrl) {
		case PANEL_LPM_HZ_NONE:
			mdss_samsung_send_cmd(ctrl, vdd->panel_lpm.hz);
			break;
		case PANEL_LPM_AOD_ON:
		case PANEL_LPM_AOD_OFF:
			mdss_samsung_send_cmd(ctrl, aod_ctrl);
			break;
		default:
			break;
	}

	LCD_INFO("[Panel LPM] Update Hz: %s, aod_ctrl : %s\n",
			vdd->panel_lpm.hz == PANEL_LPM_HZ_NONE ? "NONE" :
			vdd->panel_lpm.hz == PANEL_LPM_1HZ ? "1HZ" :
			vdd->panel_lpm.hz == PANEL_LPM_2HZ ? "2HZ" :
			vdd->panel_lpm.hz == PANEL_LPM_30HZ ? "30HZ" : "Default",
			aod_ctrl == PANEL_LPM_HZ_NONE ? "NONE" :
			aod_ctrl == PANEL_LPM_AOD_ON ? "AOD_ON" :
			aod_ctrl == PANEL_LPM_AOD_OFF ? "AOD_OFF" : "UNKNOWN");
}

static void mdss_samsung_panel_lpm_ctrl(struct mdss_panel_data *pdata, int event)
{
	struct mdss_dsi_ctrl_pdata *ctrl= NULL;
	struct samsung_display_driver_data *vdd = NULL;
	struct mdss_panel_info *pinfo = NULL;
	static u8 prev_blank_state = MDSS_PANEL_BLANK_UNBLANK;

	if (IS_ERR_OR_NULL(pdata)) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	return;
	LCD_DEBUG("[Panel LPM] ++\n");
	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	vdd = check_valid_ctrl(ctrl);

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (!vdd->dtsi_data[ctrl->ndx].panel_lpm_enable) {
		LCD_INFO("[Panel LPM] LPM(ALPM/HLPM) is not supported\n");
		return;
	}

	if (pinfo->blank_state == MDSS_PANEL_BLANK_BLANK) {
		LCD_INFO("[Panel LPM] Do not change mode\n");
		goto end;
	}

	mutex_lock(&vdd->vdd_panel_lpm_lock);


	LCD_DEBUG("[Panel LPM] blank_state %d, prev_blank_state : %d, param_changed %s\n",
			pinfo->blank_state, prev_blank_state, vdd->panel_lpm.param_changed ? "true" : "false");
	if ((pinfo->blank_state == MDSS_PANEL_BLANK_LOW_POWER) &&
			vdd->panel_lpm.param_changed) {
		mdss_samsung_send_cmd(ctrl, PANEL_DISPLAY_OFF);
		LCD_DEBUG("[Panel LPM] Send panel DISPLAY_OFF cmds\n");

		mdss_samsung_send_cmd(ctrl, PANEL_LPM_ON);
		LCD_INFO("[Panel LPM] Send panel LPM cmds\n");

		mdss_samsung_panel_lpm_hz_ctrl(pdata, PANEL_LPM_HZ_NONE);
		vdd->panel_lpm.param_changed = false;

		vdd->display_status_dsi[ctrl->ndx].wait_disp_on = true;
		LCD_DEBUG("[Panel LPM] Set wait_disp_on to true\n");
	} else if ((prev_blank_state == MDSS_PANEL_BLANK_LOW_POWER) &&
			(pinfo->blank_state == MDSS_PANEL_BLANK_UNBLANK)) {
		/* Turn Off ALPM Mode */
		mdss_samsung_send_cmd(ctrl, PANEL_LPM_OFF);
		LCD_DEBUG("[Panel LPM] Send panel LPM off cmds\n");

		vdd->panel_lpm.param_changed = true;
		LCD_INFO("[Panel LPM] Restore brightness level\n");
		mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
		pdata->set_backlight(pdata, vdd->mfd_dsi[DISPLAY_1]->bl_level);
		mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
		vdd->display_status_dsi[ctrl->ndx].wait_disp_on = true;
	} else
		LCD_DEBUG("[Panel LPM] Do not change mode, line : %d\n", __LINE__);

	/*
	 * AOD ON/OFF
	 * ON	: AOD will be on if the blank_state is MDSS_SAMSUNG_EVENT_ULP
	 * OFF	: AOD will be off if the blank_state is MDSS_SAMSUNG_EVENT_LP
	 */
	if ((vdd->panel_lpm.hz == PANEL_LPM_2HZ) ||
			(vdd->panel_lpm.hz == PANEL_LPM_1HZ)) {
		if (event == MDSS_SAMSUNG_EVENT_ULP)
			mdss_samsung_panel_lpm_hz_ctrl(pdata, PANEL_LPM_AOD_ON);
		else if (event == MDSS_SAMSUNG_EVENT_LP)
			mdss_samsung_panel_lpm_hz_ctrl(pdata, PANEL_LPM_AOD_OFF);
	}

end:
	prev_blank_state = pinfo->blank_state;
	LCD_DEBUG("[Panel LPM] --\n");
	mutex_unlock(&vdd->vdd_panel_lpm_lock);
}

static void mdss_samsung_panel_lpm_mode_store(struct mdss_panel_data *pdata, u8 mode)
{
    struct mdss_dsi_ctrl_pdata *ctrl= NULL;
	struct samsung_display_driver_data *vdd = NULL;
	struct mdss_panel_info *pinfo = NULL;
	u8 req_mode = mode;

    if (IS_ERR_OR_NULL(pdata)) {
        pr_err("%s: Invalid input data\n", __func__);
        return;
    }

	LCD_DEBUG("[Panel LPM] ++\n");
	pinfo = &pdata->panel_info;
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	vdd = check_valid_ctrl(ctrl);

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	if (!vdd->dtsi_data[ctrl->ndx].panel_lpm_enable) {
		LCD_INFO("[Panel LPM] LPM(ALPM/HLPM) is not supported\n");
		return;
	}

	mutex_lock(&vdd->vdd_panel_lpm_lock);

	/*
	 * Get Panel LPM mode and update brightness and Hz setting
	 */
	vdd->panel_func.samsung_get_panel_lpm_mode(ctrl, &req_mode);

	if ((req_mode >= MAX_LPM_MODE)) {
		LCD_INFO("[Panel LPM] Set req_mode to ALPM_MODE_ON\n");
		req_mode = ALPM_MODE_ON;
	}

	vdd->panel_lpm.mode = req_mode;

	mutex_unlock(&vdd->vdd_panel_lpm_lock);

	LCD_DEBUG("[Panel LPM] --\n");
}

static ssize_t mdss_samsung_panel_lpm_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int mode = 0;
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);
	struct mdss_panel_data *pdata;
	struct mdss_panel_info *pinfo;

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;
	pinfo = &pdata->panel_info;

	if (IS_ERR_OR_NULL(vdd) || IS_ERR_OR_NULL(vdd->mfd_dsi[DISPLAY_1])) {
		pr_err("%s vdd is error", __func__);
		return size;
	}

	sscanf(buf, "%d" , &mode);
	LCD_INFO("[Panel LPM] Mode : %d\n", mode);

	mdss_samsung_panel_lpm_mode_store(pdata, mode + MAX_LPM_MODE);

	return size;
}

int hmt_bright_update(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_data *pdata;
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	pdata = &vdd->ctrl_dsi[DISPLAY_1]->panel_data;

	msleep(20);

	if (vdd->hmt_stat.hmt_on) {
		mdss_samsung_brightness_dcs_hmt(ctrl, vdd->hmt_stat.hmt_bl_level);
	} else {
		mutex_lock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
		pdata->set_backlight(pdata, vdd->bl_level);
		mutex_unlock(&vdd->mfd_dsi[DISPLAY_1]->bl_lock);
		pr_info("%s : hmt off state!\n", __func__);
	}

	return 0;
}

int hmt_enable(struct mdss_dsi_ctrl_pdata *ctrl, int enable)
{
	pr_info("[HMT] HMT %s\n", enable ? "ON" : "OFF");

	if (enable)
		mdss_samsung_send_cmd(ctrl, PANEL_HMT_ENABLE);
	else
		mdss_samsung_send_cmd(ctrl, PANEL_HMT_DISABLE);

	return 0;
}

int hmt_reverse_update(struct mdss_dsi_ctrl_pdata *ctrl, int enable)
{
	pr_info("[HMT] HMT %s\n", enable ? "REVERSE" : "FORWARD");

	if (enable)
		mdss_samsung_send_cmd(ctrl, PANEL_HMT_REVERSE);
	else
		mdss_samsung_send_cmd(ctrl, PANEL_HMT_FORWARD);

	return 0;
}

static ssize_t mipi_samsung_hmt_bright_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		pr_err("%s : hmt is not supported..\n", __func__);
		return -ENODEV;
	}

	rc = snprintf((char *)buf, 30, "%d\n", vdd->hmt_stat.hmt_bl_level);
	pr_info("[HMT] hmt bright : %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_hmt_bright_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int input;
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl;

	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		pr_err("%s : hmt is not supported..\n", __func__);
		return -ENODEV;
	}

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	pinfo = &ctrl->panel_data.panel_info;

	sscanf(buf, "%d" , &input);
	pr_info("[HMT] %s: input (%d) ++\n", __func__, input);

	if (!vdd->hmt_stat.hmt_on) {
		pr_info("[HMT] hmt is off!\n");
		return size;
	}

	if (!pinfo->blank_state) {
		pr_err("[HMT] panel is off!\n");
		vdd->hmt_stat.hmt_bl_level = input;
		return size;
	}

	if (vdd->hmt_stat.hmt_bl_level == input) {
		pr_err("[HMT] hmt bright already %d!\n", vdd->hmt_stat.hmt_bl_level);
		return size;
	}

	vdd->hmt_stat.hmt_bl_level = input;
	hmt_bright_update(ctrl);

	pr_info("[HMT] %s: input (%d) --\n", __func__, input);

	return size;
}

static ssize_t mipi_samsung_hmt_on_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		pr_err("%s : hmt is not supported..\n", __func__);
		return -ENODEV;
	}

	rc = snprintf((char *)buf, 30, "%d\n", vdd->hmt_stat.hmt_on);
	pr_info("[HMT] hmt on input : %d\n", *buf);

	return rc;
}

static ssize_t mipi_samsung_hmt_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int input;
	struct mdss_panel_info *pinfo = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl;

	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return -ENODEV;
	}

	if (!vdd->dtsi_data[0].hmt_enabled) {
		pr_err("%s : hmt is not supported..\n", __func__);
		return -ENODEV;
	}

	if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_broadcast) {
		if (vdd->ctrl_dsi[DISPLAY_1]->cmd_sync_wait_trigger)
			ctrl = vdd->ctrl_dsi[DISPLAY_1];
		else
			ctrl = vdd->ctrl_dsi[DISPLAY_2];
	} else
		ctrl = vdd->ctrl_dsi[DISPLAY_1];

	pinfo = &ctrl->panel_data.panel_info;

	sscanf(buf, "%d" , &input);
	pr_info("[HMT] %s: input (%d) ++\n", __func__, input);

	if (!pinfo->blank_state) {
		pr_err("[HMT] panel is off!\n");
		vdd->hmt_stat.hmt_on = input;
		return size;
	}

	if (vdd->hmt_stat.hmt_on == input) {
		pr_info("[HMT] hmt already %s !\n", vdd->hmt_stat.hmt_on?"ON":"OFF");
		return size;
	}

	vdd->hmt_stat.hmt_on = input;

	hmt_enable(ctrl, vdd->hmt_stat.hmt_on);
	hmt_reverse_update(ctrl, vdd->hmt_stat.hmt_on);

	hmt_bright_update(ctrl);

	pr_info("[HMT] %s: input hmt (%d) --\n",
		__func__, vdd->hmt_stat.hmt_on);

	return size;
}

void mdss_samsung_cabc_update(void)
{
	struct samsung_display_driver_data *vdd = samsung_get_vdd();

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return;
	}

	if (vdd->auto_brightness) {
		pr_info("%s auto brightness is on , cabc cmds are already sent--\n",
			__func__);
		return;
	}

	if (vdd->siop_status) {
		if (vdd->panel_func.samsung_lvds_write_reg)
			vdd->panel_func.samsung_brightness_tft_pwm(vdd->ctrl_dsi[DISPLAY_1], vdd->bl_level);
		else {
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_CABC_OFF_DUTY);
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_CABC_ON);
			if (vdd->dtsi_data[0].cabc_delay && !vdd->display_status_dsi[0].disp_on_pre)
				usleep_range(vdd->dtsi_data[0].cabc_delay, vdd->dtsi_data[0].cabc_delay);
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_CABC_ON_DUTY);
		}
	} else {
		if (vdd->panel_func.samsung_lvds_write_reg)
			vdd->panel_func.samsung_brightness_tft_pwm(vdd->ctrl_dsi[DISPLAY_1], vdd->bl_level);
		else {
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_CABC_OFF_DUTY);
			mdss_samsung_send_cmd(vdd->ctrl_dsi[DISPLAY_1], PANEL_CABC_OFF);
		}
	}
}

int config_cabc(int value)
{
	struct samsung_display_driver_data *vdd = samsung_get_vdd();

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s vdd is error", __func__);
		return value;
	}

	if (vdd->siop_status == value) {
		pr_info("%s No change in cabc state, update not needed--\n", __func__);
		return value;
	}

	vdd->siop_status = value;
	mdss_samsung_cabc_update();
	return 0;
}

static DEVICE_ATTR(lcd_type, S_IRUGO, mdss_samsung_disp_lcdtype_show, NULL);
static DEVICE_ATTR(cell_id, S_IRUGO, mdss_samsung_disp_cell_id_show, NULL);
static DEVICE_ATTR(window_type, S_IRUGO, mdss_samsung_disp_windowtype_show, NULL);
static DEVICE_ATTR(manufacture_date, S_IRUGO, mdss_samsung_disp_manufacture_date_show, NULL);
static DEVICE_ATTR(manufacture_code, S_IRUGO, mdss_samsung_disp_manufacture_code_show, NULL);
static DEVICE_ATTR(power_reduce, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_disp_acl_show, mdss_samsung_disp_acl_store);
static DEVICE_ATTR(siop_enable, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_disp_siop_show, mdss_samsung_disp_siop_store);
static DEVICE_ATTR(read_mtp, S_IRUGO | S_IWUSR | S_IWGRP, NULL, mdss_samsung_read_mtp_store);
static DEVICE_ATTR(temperature, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_temperature_show, mdss_samsung_temperature_store);
static DEVICE_ATTR(read_copr, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_read_copr_show, NULL);
static DEVICE_ATTR(aid_log, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_aid_log_show, NULL);
static DEVICE_ATTR(partial_disp, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_disp_partial_disp_show, mdss_samsung_disp_partial_disp_store);
static DEVICE_ATTR(alpm, S_IRUGO | S_IWUSR | S_IWGRP, mdss_samsung_panel_lpm_show, mdss_samsung_panel_lpm_store);
static DEVICE_ATTR(hmt_bright, S_IRUGO | S_IWUSR | S_IWGRP, mipi_samsung_hmt_bright_show, mipi_samsung_hmt_bright_store);
static DEVICE_ATTR(hmt_on, S_IRUGO | S_IWUSR | S_IWGRP,	mipi_samsung_hmt_on_show, mipi_samsung_hmt_on_store);

static struct attribute *panel_sysfs_attributes[] = {
	&dev_attr_lcd_type.attr,
	&dev_attr_cell_id.attr,
	&dev_attr_window_type.attr,
	&dev_attr_manufacture_date.attr,
	&dev_attr_manufacture_code.attr,
	&dev_attr_power_reduce.attr,
	&dev_attr_siop_enable.attr,
	&dev_attr_aid_log.attr,
	&dev_attr_read_mtp.attr,
	&dev_attr_read_copr.attr,
	&dev_attr_temperature.attr,
	&dev_attr_partial_disp.attr,
	&dev_attr_alpm.attr,
	&dev_attr_hmt_bright.attr,
	&dev_attr_hmt_on.attr,
	NULL
};
static const struct attribute_group panel_sysfs_group = {
	.attrs = panel_sysfs_attributes,
};

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
static DEVICE_ATTR(auto_brightness, S_IRUGO | S_IWUSR | S_IWGRP,
			mdss_samsung_auto_brightness_show,
			mdss_samsung_auto_brightness_store);

static DEVICE_ATTR(auto_brightness_level, S_IRUGO | S_IWUSR | S_IWGRP,
			mdss_samsung_auto_brightness_level,
			NULL);

static DEVICE_ATTR(brightness_step, S_IRUGO | S_IWUSR | S_IWGRP,
			mdss_samsung_disp_brightness_step,
			NULL);

static DEVICE_ATTR(color_weakness, S_IRUGO | S_IWUSR | S_IWGRP,
			mdss_samsung_disp_color_weakness_show,
			mdss_samsung_disp_color_weakness_store);
static DEVICE_ATTR(weakness_hbm_comp, S_IRUGO | S_IWUSR | S_IWGRP,
			mdss_samsung_weakness_hbm_comp_show,
			mdss_samsung_weakness_hbm_comp_store);

static struct attribute *bl_sysfs_attributes[] = {
	&dev_attr_auto_brightness.attr,
	&dev_attr_auto_brightness_level.attr,
	&dev_attr_brightness_step.attr,
	&dev_attr_color_weakness.attr,
	&dev_attr_weakness_hbm_comp.attr,
	NULL
};

static const struct attribute_group bl_sysfs_group = {
	.attrs = bl_sysfs_attributes,
};
#endif /* END CONFIG_LCD_CLASS_DEVICE*/

static ssize_t csc_read_cfg(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", csc_update);
	return ret;
}

static ssize_t csc_write_cfg(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret = strnlen(buf, PAGE_SIZE);
	int err;
	int mode;

	err =  kstrtoint(buf, 0, &mode);
	if (err)
		return ret;

	csc_update = (u8)mode;
	csc_change = 1;
	pr_info(" csc ctrl set to csc_update(%d)\n", csc_update);

	return ret;
}

static DEVICE_ATTR(csc_cfg, S_IRUGO | S_IWUSR, csc_read_cfg, csc_write_cfg);

int mdss_samsung_create_sysfs(void *data)
{
	static int sysfs_enable = 0;
	int rc = 0;
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct lcd_device *lcd_device;
#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	struct backlight_device *bd = NULL;
#endif
#endif
	struct device *csc_dev = vdd_data.mfd_dsi[0]->fbi->dev;

	/* sysfs creat func should be called one time in dual dsi mode */
	if (sysfs_enable)
		return 0;

#if defined(CONFIG_LCD_CLASS_DEVICE)
	lcd_device = lcd_device_register("panel", NULL, data, NULL);

	if (IS_ERR_OR_NULL(lcd_device)) {
		rc = PTR_ERR(lcd_device);
		pr_err("Failed to register lcd device..\n");
		return rc;
	}

	rc = sysfs_create_group(&lcd_device->dev.kobj, &panel_sysfs_group);
	if (rc) {
		pr_err("Failed to create panel sysfs group..\n");
		sysfs_remove_group(&lcd_device->dev.kobj, &panel_sysfs_group);
		return rc;
	}

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	bd = backlight_device_register("panel", &lcd_device->dev,
						data, NULL, NULL);
	if (IS_ERR(bd)) {
		rc = PTR_ERR(bd);
		pr_err("backlight : failed to register device\n");
		return rc;
	}

	rc = sysfs_create_group(&bd->dev.kobj, &bl_sysfs_group);
	if (rc) {
		pr_err("Failed to create backlight sysfs group..\n");
		sysfs_remove_group(&bd->dev.kobj, &bl_sysfs_group);
		return rc;
	}
#endif

	rc = sysfs_create_file(&lcd_device->dev.kobj, &dev_attr_tuning.attr);
	if (rc) {
		pr_err("sysfs create fail-%s\n", dev_attr_tuning.attr.name);
		return rc;
	}
#endif

	rc = sysfs_create_file(&csc_dev->kobj, &dev_attr_csc_cfg.attr);
	if (rc) {
		pr_err("sysfs create fail-%s\n", dev_attr_csc_cfg.attr.name);
		return rc;
	}

	sysfs_enable = 1;

	pr_info("%s: done!!\n", __func__);

	return rc;
}

struct samsung_display_driver_data *samsung_get_vdd(void)
{
	return &vdd_data;
}

int display_ndx_check(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct samsung_display_driver_data *vdd = check_valid_ctrl(ctrl);

	if (IS_ERR_OR_NULL(vdd)) {
		pr_err("%s: Invalid data ctrl : 0x%zx vdd : 0x%zx", __func__, (size_t)ctrl, (size_t)vdd);
		return DSI_CTRL_0;
	}

	if (vdd->support_hall_ic) {
		if (vdd->display_status_dsi[DISPLAY_1].hall_ic_status == HALL_IC_OPEN)
			return DSI_CTRL_0; /*OPEN : Internal PANEL */
		else
			return DSI_CTRL_1; /*CLOSE : External PANEL */
	} else
		return ctrl->ndx;
}

int samsung_display_hall_ic_status(struct notifier_block *nb,
			unsigned long hall_ic, void *data)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = vdd_data.ctrl_dsi[DISPLAY_1];

	/*
		previou panel off -> current panel on

		foder open : 0, close : 1
	*/

	if (!vdd_data.support_hall_ic)
		return 0;

	mutex_lock(&vdd_data.vdd_hall_ic_blank_unblank_lock); /*HALL IC blank mode change */
	mutex_lock(&vdd_data.vdd_hall_ic_lock); /* HALL IC switching */

	pr_err("mdss %s hall_ic : %s, blank_status: %d start\n", __func__, hall_ic ? "CLOSE" : "OPEN", vdd_data.vdd_blank_mode[DISPLAY_1]);

	/* check the lcd id for DISPLAY_1 and DISPLAY_2 */
	if (mdss_panel_attached(DISPLAY_1) && mdss_panel_attached(DISPLAY_2)) {
		/* To check current blank mode */
		if (vdd_data.vdd_blank_mode[DISPLAY_1] == FB_BLANK_UNBLANK &&
			ctrl_pdata->panel_data.panel_info.panel_state &&
			vdd_data.display_status_dsi[DISPLAY_1].hall_ic_status != hall_ic) {

			/* set flag */
			vdd_data.display_status_dsi[DISPLAY_1].hall_ic_mode_change_trigger = true;

			/* panel off */
			ctrl_pdata->off(&ctrl_pdata->panel_data);

			/* set status */
			vdd_data.display_status_dsi[DISPLAY_1].hall_ic_status = hall_ic;

			/* panel on */
			ctrl_pdata->on(&ctrl_pdata->panel_data);

			/* clear flag */
			vdd_data.display_status_dsi[DISPLAY_1].hall_ic_mode_change_trigger = false;

			/* Brightness setting */
			if (vdd_data.ctrl_dsi[DISPLAY_1]->bklt_ctrl == BL_DCS_CMD) {
				mutex_lock(&vdd_data.vdd_bl_level_lock);
				mdss_samsung_brightness_dcs(ctrl_pdata, vdd_data.bl_level);
				mutex_unlock(&vdd_data.vdd_bl_level_lock);
			}

			/* display on */
			mdss_samsung_send_cmd(ctrl_pdata, PANEL_DISPLAY_ON);
		} else {
			vdd_data.display_status_dsi[DISPLAY_1].hall_ic_status = hall_ic;
			pr_err("mdss %s skip display changing\n", __func__);
		}
	} else {
		/* check the lcd id for DISPLAY_1 */
		if (mdss_panel_attached(DISPLAY_1))
			vdd_data.display_status_dsi[DISPLAY_1].hall_ic_status = HALL_IC_OPEN;

		/* check the lcd id for DISPLAY_2 */
		if (mdss_panel_attached(DISPLAY_2))
			vdd_data.display_status_dsi[DISPLAY_1].hall_ic_status = HALL_IC_CLOSE;
	}

	mutex_unlock(&vdd_data.vdd_hall_ic_lock); /* HALL IC switching */
	mutex_unlock(&vdd_data.vdd_hall_ic_blank_unblank_lock); /*HALL IC blank mode change */

	pr_err("mdss %s hall_ic : %s, blank_status: %d end\n", __func__, hall_ic ? "CLOSE" : "OPEN", vdd_data.vdd_blank_mode[DISPLAY_1]);

	return 0;
}

/************************************************************
*
*		MDSS & DSI REGISTER DUMP FUNCTION
*
**************************************************************/
size_t kvaddr_to_paddr(unsigned long vaddr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	size_t paddr;

	pgd = pgd_offset_k(vaddr);
	if (unlikely(pgd_none(*pgd) || pgd_bad(*pgd)))
		return 0;

	pud = pud_offset(pgd, vaddr);
	if (unlikely(pud_none(*pud) || pud_bad(*pud)))
		return 0;

	pmd = pmd_offset(pud, vaddr);
	if (unlikely(pmd_none(*pmd) || pmd_bad(*pmd)))
		return 0;

	pte = pte_offset_kernel(pmd, vaddr);
	if (!pte_present(*pte))
		return 0;

	paddr = (unsigned long)pte_pfn(*pte) << PAGE_SHIFT;
	paddr += (vaddr & (PAGE_SIZE - 1));

	return paddr;
}

static void dump_reg(char *addr, int len)
{
	if (IS_ERR_OR_NULL(addr))
		return;
#if defined(CONFIG_ARCH_MSM8976) || defined(CONFIG_ARCH_MSM8952)
	mdss_dump_reg(MDSS_REG_DUMP_IN_LOG, addr, len, NULL);
#else
	mdss_dump_reg(addr, len);
#endif
}

void mdss_samsung_dump_regs(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_ctl *ctl;
	char name[32];
	int loop;

	if (IS_ERR_OR_NULL(mdata))
		return;
	else
		 ctl = mdata->ctl_off;

	snprintf(name, sizeof(name), "MDP BASE");
	pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->mdss_io.base));
	dump_reg(mdata->mdss_io.base, 0x100);

	snprintf(name, sizeof(name), "MDP REG");
	pr_err("=============%s 0x%08zx ==============\n", name,
		kvaddr_to_paddr((unsigned long)mdata->mdp_base));
	dump_reg(mdata->mdp_base, 0x500);

	for (loop = 0; loop < mdata->nctl ; loop++) {
		snprintf(name, sizeof(name), "CTRL%d", loop);
		pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->ctl_off[loop].base));
		dump_reg(mdata->ctl_off[loop].base, 0x100);
	}

	for (loop = 0; loop < mdata->nvig_pipes ; loop++) {
		snprintf(name, sizeof(name), "VG%d", loop);
		pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->vig_pipes[loop].base));
		/* dump_reg(mdata->vig_pipes[loop].base, 0x100); */
		dump_reg(mdata->vig_pipes[loop].base, 0x400);
	}

	for (loop = 0; loop < mdata->nrgb_pipes ; loop++) {
		snprintf(name, sizeof(name), "RGB%d", loop);
		pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->rgb_pipes[loop].base));
		/* dump_reg(mdata->rgb_pipes[loop].base, 0x100); */
		dump_reg(mdata->rgb_pipes[loop].base, 0x400);
	}

	for (loop = 0; loop < mdata->ndma_pipes ; loop++) {
		snprintf(name, sizeof(name), "DMA%d", loop);
		pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->dma_pipes[loop].base));
		/* dump_reg(mdata->dma_pipes[loop].base, 0x100); */
		dump_reg(mdata->dma_pipes[loop].base, 0x400);
	}

	for (loop = 0; loop < mdata->nmixers_intf ; loop++) {
		snprintf(name, sizeof(name), "MIXER_INTF_%d", loop);
		pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->mixer_intf[loop].base));
		dump_reg(mdata->mixer_intf[loop].base, 0x100);
	}

	for (loop = 0; loop < mdata->nmixers_wb ; loop++) {
		snprintf(name, sizeof(name), "MIXER_WB_%d", loop);
		pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->mixer_wb[loop].base));
		dump_reg(mdata->mixer_wb[loop].base, 0x100);
	}

	if (ctl->is_video_mode) {
		for (loop = 0; loop < mdata->nintf; loop++) {
			snprintf(name, sizeof(name), "VIDEO_INTF%d", loop);
			pr_err("=============%s 0x%08zx ==============\n", name,
				kvaddr_to_paddr((unsigned long)mdss_mdp_get_intf_base_addr(mdata, loop)));
			dump_reg(mdss_mdp_get_intf_base_addr(mdata, loop), 0x40);
		}
	}

	for (loop = 0; loop < mdata->nmixers_intf ; loop++) {
		snprintf(name, sizeof(name), "PING_PONG%d", loop);
		pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->mixer_intf[loop].pingpong_base));
		/* dump_reg(mdata->mixer_intf[loop].pingpong_base, 0x40); */
		dump_reg(mdata->mixer_intf[loop].pingpong_base, 0x100);
	}
#if 0
	for (loop = 0; loop < mdata->nwb; loop++) {
		snprintf(name, sizeof(name), "WRITE BACK%d", loop);
		pr_err("=============%s 0x%08zx ==============\n", name,
				kvaddr_to_paddr((unsigned long)mdata->mdss_io.base + mdata->wb_offsets[loop]));
		dump_reg((mdata->mdss_io.base + mdata->wb_offsets[loop]), 0x2BC);
	}
#endif

	if (ctl->mfd->panel_info->compression_mode == COMPRESSION_DSC) {
		for (loop = 0; loop < mdata->ndsc ; loop++) {
			snprintf(name, sizeof(name), "DSC%d", loop);
			pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->dsc_off[loop].base));
			dump_reg(mdata->dsc_off[loop].base, 0x200);
		}
	}

#if defined(CONFIG_ARCH_MSM8992)
	/* To dump ping-pong slave register for ping-pong split supporting chipset */
	snprintf(name, sizeof(name), "PING_PONG SLAVE");
		pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->slave_pingpong_base));
		dump_reg(mdata->slave_pingpong_base, 0x40);
#endif

	snprintf(name, sizeof(name), "VBIF");
	pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->vbif_io.base));
	dump_reg(mdata->vbif_io.base, 0x270);

	snprintf(name, sizeof(name), "VBIF NRT");
	pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)mdata->vbif_nrt_io.base));
	dump_reg(mdata->vbif_nrt_io.base, 0x270);

}

void mdss_samsung_dsi_dump_regs(int dsi_num)
{
	struct mdss_dsi_ctrl_pdata *dsi_ctrl = mdss_dsi_get_ctrl(dsi_num);
	char name[32];

	if (!dsi_ctrl) {
		pr_err("dsi_ctrl is null.. (%d)\n", dsi_num);
		return;
	}

	if (vdd_data.panel_attach_status & BIT(dsi_num)) {
		snprintf(name, sizeof(name), "DSI%d CTL", dsi_num);
		pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)dsi_ctrl->ctrl_io.base));
		dump_reg((char *)dsi_ctrl->ctrl_io.base, dsi_ctrl->ctrl_io.len);

		snprintf(name, sizeof(name), "DSI%d PHY", dsi_num);
		pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)dsi_ctrl->phy_io.base));
		dump_reg((char *)dsi_ctrl->phy_io.base, (size_t)dsi_ctrl->phy_io.len);

		snprintf(name, sizeof(name), "DSI%d PLL", dsi_num);
		pr_err("=============%s 0x%08zx ==============\n", name,
			kvaddr_to_paddr((unsigned long)vdd_data.dump_info[dsi_num].dsi_pll.virtual_addr));
		dump_reg((char *)vdd_data.dump_info[dsi_num].dsi_pll.virtual_addr, 0x200);

#if defined(CONFIG_ARCH_MSM8992) || defined(CONFIG_ARCH_MSM8994)
		if (dsi_ctrl->shared_ctrl_data->phy_regulator_io.base) {
			snprintf(name, sizeof(name), "DSI%d REGULATOR", dsi_num);
			pr_err("=============%s 0x%08zx ==============\n", name,
				kvaddr_to_paddr((unsigned long)dsi_ctrl->shared_ctrl_data->phy_regulator_io.base));
			dump_reg((char *)dsi_ctrl->shared_ctrl_data->phy_regulator_io.base, (size_t)dsi_ctrl->shared_ctrl_data->phy_regulator_io.len);
		}
#endif
	}
}

int mdss_samsung_read_rddpm(void)
{
	struct mdss_dsi_ctrl_pdata *dsi_ctrl = mdss_dsi_get_ctrl(0);
	struct samsung_display_driver_data *vdd = samsung_get_vdd();
	char rddpm_reg = 0;

	MDSS_XLOG(dsi_ctrl->ndx, 0x0A);

	if (!IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].ldi_debug0_rx_cmds[vdd->panel_revision].cmds)) {
		mdss_samsung_read_nv_mem(&dsi_ctrl[DISPLAY_1], &vdd->dtsi_data[DISPLAY_1].ldi_debug0_rx_cmds[vdd->panel_revision],
			&rddpm_reg, PANEL_LEVE1_KEY);

		pr_err("%s : rddpm 0x(%x)\n", __func__, rddpm_reg);

		if (rddpm_reg == 0x08) {
			pr_err("%s : ddi reset status (%x)\n", __func__, rddpm_reg);
			schedule_work(&pstatus_data->check_status.work);
			return 1;
		}
	} else
		pr_err("%s : no rddpm read cmds..\n", __func__);

	return 0;
}

int loading_det = 0;
int mdss_samsung_read_loading_detection(void)
{
	struct mdss_dsi_ctrl_pdata *dsi_ctrl = mdss_dsi_get_ctrl(0);
	struct samsung_display_driver_data *vdd = samsung_get_vdd();
	char readbuf = 0;

	if (!IS_ERR_OR_NULL(vdd->dtsi_data[DISPLAY_1].ldi_loading_det_rx_cmds[vdd->panel_revision].cmds)) {
		mdss_samsung_read_nv_mem(&dsi_ctrl[DISPLAY_1], &vdd->dtsi_data[DISPLAY_1].ldi_loading_det_rx_cmds[vdd->panel_revision], &readbuf, 0);

		if (readbuf == 0x80 || readbuf == 0xC0) {
			pr_err("%s : ddi loading OK (%x) (%d)\n", __func__, readbuf, loading_det);
			return 1;
		} else {
			pr_err("%s : ddi loading not OK (%x) (%d)\n", __func__, readbuf, loading_det);
			if (!loading_det)
				loading_det = 1;
			return 0;
		}

	} else
		pr_err("%s : no ddi loading detection cmds..\n", __func__);

	return 0;
}

int mdss_samsung_dsi_te_check(void)
{
	struct mdss_dsi_ctrl_pdata *dsi_0_ctrl = mdss_dsi_get_ctrl(0);
	struct mdss_dsi_ctrl_pdata *dsi_1_ctrl = mdss_dsi_get_ctrl(1);
	int rc, te_count = 0;
	int te_max = 20000; /*sampling 200ms */

	if (dsi_0_ctrl[DISPLAY_1].panel_mode == DSI_VIDEO_MODE || dsi_1_ctrl[DISPLAY_1].panel_mode == DSI_VIDEO_MODE)
		return 0;

	if (gpio_is_valid(dsi_0_ctrl[DISPLAY_1].disp_te_gpio) || gpio_is_valid(dsi_1_ctrl[DISPLAY_1].disp_te_gpio)) {
		pr_err(" ============ start waiting for TE ============\n");

		for (te_count = 0;  te_count < te_max; te_count++) {
			if(gpio_is_valid(dsi_0_ctrl[DISPLAY_1].disp_te_gpio))
				rc = gpio_get_value(dsi_0_ctrl[DISPLAY_1].disp_te_gpio);
			else
				rc = gpio_get_value(dsi_1_ctrl[DISPLAY_1].disp_te_gpio);
			if (rc == 1) {
				pr_err("%s: gpio_get_value(disp_te_gpio) = %d ",
									__func__, rc);
				pr_err("te_count = %d\n", te_count);
				break;
			}
			/* usleep suspends the calling thread whereas udelay is a
			 * busy wait. Here the value of te_gpio is checked in a loop of
			 * max count = 250. If this loop has to iterate multiple
			 * times before the te_gpio is 1, the calling thread will end
			 * up in suspend/wakeup sequence multiple times if usleep is
			 * used, which is an overhead. So use udelay instead of usleep.
			 */
			udelay(10);
		}

		if (te_count == te_max) {
			pr_err("LDI doesn't generate TE, ddi recovery start.");
			schedule_work(&pstatus_data->check_status.work);
			return 1;
		} else
			pr_err("LDI generate TE\n");

		pr_err(" ============ finish waiting for TE ============\n");
	} else
		pr_err("disp_te_gpio is not valid\n");

	return 0;
}

void mdss_mdp_underrun_dump_info(void)
{
	struct mdss_mdp_pipe *pipe;
	/* struct mdss_data_type *mdss_res = mdss_mdp_get_mdata(); */
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(vdd_data.mfd_dsi[0]);
	int pcount = mdp5_data->mdata->nrgb_pipes + mdp5_data->mdata->nvig_pipes + mdp5_data->mdata->ndma_pipes;

	pr_err(" ============ %s start ===========\n", __func__);
	list_for_each_entry(pipe, &mdp5_data->pipes_used, list) {
		if (pipe && pipe->src_fmt)
			pr_err(" [%4d, %4d, %4d, %4d] -> [%4d, %4d, %4d, %4d]"
				"|flags = %8d|src_format = %2d|bpp = %2d|ndx = %3d|\n",
				pipe->src.x, pipe->src.y, pipe->src.w, pipe->src.h,
				pipe->dst.x, pipe->dst.y, pipe->dst.w, pipe->dst.h,
				pipe->flags, pipe->src_fmt->format, pipe->src_fmt->bpp,
				pipe->ndx);
		pr_err("pipe addr : %p\n", pipe);
		pcount--;
		if (!pcount)
			break;
	}
/*
	pr_err("mdp_clk = %ld, bus_ab = %llu, bus_ib = %llu\n", mdss_mdp_get_clk_rate(MDSS_CLK_MDP_SRC),
		mdss_res->bus_scale_table->usecase[mdss_res->curr_bw_uc_idx].vectors[0].ab,
		mdss_res->bus_scale_table->usecase[mdss_res->curr_bw_uc_idx].vectors[0].ib);
*/
	pr_err(" ============ %s end ===========\n", __func__);
}

#if 0
DEFINE_MUTEX(FENCE_LOCK);
static const char *sync_status_str(int status)
{
	if (status > 0)
		return "signaled";
	else if (status == 0)
		return "active";
	else
		return "error";
}

static void sync_pt_log(struct sync_pt *pt, bool pt_callback)
{
	int status = pt->status;

	pr_cont("[mdss DEBUG_FENCE]  %s_pt %s",
		   pt->parent->name,
		   sync_status_str(status));

	if (pt->status) {
		struct timeval tv = ktime_to_timeval(pt->timestamp);

		pr_cont("@%ld.%06ld", tv.tv_sec, tv.tv_usec);
	}

	if (pt->parent->ops->timeline_value_str &&
	    pt->parent->ops->pt_value_str) {
		char value[64];

		pt->parent->ops->pt_value_str(pt, value, sizeof(value));
		pr_cont(": %s", value);
		pt->parent->ops->timeline_value_str(pt->parent, value,
					    sizeof(value));
		pr_cont(" / %s", value);
	}

	pr_cont("\n");

	/* Show additional details for active fences */
	if (pt->status == 0 && pt->parent->ops->pt_log && pt_callback)
		pt->parent->ops->pt_log(pt);
}

void mdss_samsung_fence_dump(char *intf, struct sync_fence *fence)
{
	struct sync_pt *pt;
	struct list_head *pos;

	mutex_lock(&FENCE_LOCK);

	pr_err("[mdss DEBUG_FENCE] %s : %s start\n", intf, fence->name);

	list_for_each(pos, &fence->pt_list_head) {
		pt = container_of(pos, struct sync_pt, pt_list);
		sync_pt_log(pt, true);
	}

	pr_err("[mdss DEBUG_FENCE] %s : %s end\n", intf, fence->name);

	mutex_unlock(&FENCE_LOCK);
}
#endif

#include "../../../../staging/android/ion/ion_priv.h"
#include "../../../../staging/android/ion/ion.h"

struct ion_handle {
	struct kref ref;
	struct ion_client *client;
	struct ion_buffer *buffer;
	struct rb_node node;
	unsigned int kmap_cnt;
	int id;
};

static int WIDTH_COMPRESS_RATIO = 1;
static int HEIGHT_COMPRESS_RATIO = 1;

static inline char samsung_drain_image_888(struct mdss_mdp_pipe *pipe, char *drain_pos, int drain_x, int drain_y, int RGB)
{
	char (*drain_pos_RGB888)[pipe->img_width][3] = (void *)drain_pos;

	return drain_pos_RGB888[drain_y][drain_x][RGB];
}

static inline char samsung_drain_image_8888(struct mdss_mdp_pipe *pipe, char *drain_pos, int drain_x, int drain_y, int RGB)
{
	char (*drain_pos_RGB8888)[pipe->img_width][4] = (void *)drain_pos;

	return drain_pos_RGB8888[drain_y][drain_x][RGB];
}

void samsung_image_dump(void)
{
	struct mdss_panel_info *panel_info = &vdd_data.ctrl_dsi[DISPLAY_1]->panel_data.panel_info;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(vdd_data.mfd_dsi[0]);
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_data *buf;

	int z_order;

	int x, y;
	int dst_x, dst_y;
	int drain_x, drain_y, drain_max_x, drain_max_y;
	int panel_x_res, panel_y_res;

	char *dst_fb_pos = NULL;
	char *drain_pos = NULL;

	char (*dst_fb_pos_none_split)[panel_info->xres / WIDTH_COMPRESS_RATIO][3];
	char (*dst_fb_pos_split)[(panel_info->xres * 2) / WIDTH_COMPRESS_RATIO][3];

	struct ion_buffer *ion_buffers;

	int split_mode, acquire_lock;
	/*
		cont_splash_mem: cont_splash_mem@0 {
			linux,reserve-contiguous-region;
			linux,reserve-region;
			reg = <0 0x83000000 0 0x01C00000>;
			label = "cont_splash_mem";
		};

		QC reserves tripple buffer for WQHD size at kernel. 0x2200000 is 34Mbyte.
		So we uses second buffer to image dump.
	*/
	unsigned int buffer_offset, buffer_size;
	struct BITMAPFILEHEADER bitmap;
	int bitmap_y_reverse;

	pr_err("%s start\n", __func__);

	if (vdd_data.mfd_dsi[0]->split_mode == MDP_DUAL_LM_DUAL_DISPLAY ||
		vdd_data.mfd_dsi[0]->split_mode == MDP_PINGPONG_SPLIT)  {
		split_mode = true;
		buffer_offset = ((panel_info->xres * 2) * panel_info->yres * 3);
		panel_x_res = (panel_info->xres * 2) / WIDTH_COMPRESS_RATIO;
		panel_y_res = panel_info->yres / HEIGHT_COMPRESS_RATIO;
		buffer_size = panel_x_res * panel_y_res * 3;
	} else {
		split_mode = false;
		buffer_offset = (panel_info->xres * panel_info->yres * 3);
		panel_x_res = panel_info->xres / WIDTH_COMPRESS_RATIO;
		panel_y_res = panel_info->yres / HEIGHT_COMPRESS_RATIO;
		buffer_size = panel_x_res * panel_y_res * 3;
	}

	acquire_lock = mutex_trylock(&mdp5_data->list_lock);

	/*
		We use 2 frame to dump image.

		First, dump image to First fb frame address

		Second, copy to reversed data to Second fb Frame address from Frist fb frame address.
		Because BMP format use upside-down & flipped format
	*/

	/* BMP HEADER ADDRESS */
	dst_fb_pos = phys_to_virt(mdp5_data->splash_mem_addr + buffer_offset);

	if (IS_ERR_OR_NULL(dst_fb_pos))
		return;

	/* Generate BMP format */
	bitmap.bfType = BF_TYPE;
	bitmap.bfSize = buffer_size + sizeof(struct BITMAPFILEHEADER);
	bitmap.bfReserved1 = 0x00;
	bitmap.bfReserved2 = 0x00;
	bitmap.bfOffBits = sizeof(struct BITMAPFILEHEADER);
	bitmap.biSize = 0x28;
	bitmap.biWidth = panel_x_res;
	bitmap.biHeight = panel_y_res;
	bitmap.biPlanes = 0x01;
	bitmap.biBitCount = 0x18;
	bitmap.biCompression = 0x00;
	bitmap.biSizeImage = buffer_size;
	bitmap.biXPelsPerMeter = (1000 * panel_info->xres) / panel_info->physical_width;
	bitmap.biYPelsPerMeter = (1000 * panel_info->yres) / panel_info->physical_height;
	bitmap.biClrUsed = 0x00;
	bitmap.biClrImportant = 0x00;

	memcpy(dst_fb_pos, &bitmap, sizeof(struct BITMAPFILEHEADER));

	/* fb frameaddress */
	dst_fb_pos = phys_to_virt(mdp5_data->splash_mem_addr + buffer_offset + sizeof(struct BITMAPFILEHEADER));

	if (IS_ERR_OR_NULL(dst_fb_pos))
		return;

	dst_fb_pos_none_split = (void *)dst_fb_pos;
	dst_fb_pos_split = (void *)dst_fb_pos;
	memset(dst_fb_pos, 0x00, buffer_size);

	pr_err("%s dst_pos : 0x%p split_mode : %s xres :%d yres :%d\n", __func__, dst_fb_pos,
		vdd_data.mfd_dsi[0]->split_mode == 0 ? "MDP_SPLIT_MODE_NONE" :
		vdd_data.mfd_dsi[0]->split_mode == 1 ? "MDP_DUAL_LM_SINGLE_DISPLAY" :
		vdd_data.mfd_dsi[0]->split_mode == 2 ? "MDP_DUAL_LM_DUAL_DISPLAY" : "MDP_SPLIT_MODE_NONE",
		panel_info->xres, panel_info->yres);

	for (z_order = MDSS_MDP_STAGE_0; z_order < MDSS_MDP_MAX_STAGE; z_order++) {
		list_for_each_entry(pipe, &mdp5_data->pipes_used, list) {
			/* Check z-order */
			if (pipe->mixer_stage != z_order)
				continue;

			pr_err("L_mixer:%d R_mixer:%d %s z:%d format:%d %s flag:0x%x src.x:%d y:%d w:%d h:%d "
				"des_rect.x:%d y:%d w:%d h:%d\n",
				pipe->mixer_left ? pipe->mixer_left->num : -1,
				pipe->mixer_right ? pipe->mixer_right->num : -1,
				pipe->ndx == BIT(0) ? "VG0" : pipe->ndx == BIT(1) ? "VG1" :
				pipe->ndx == BIT(2) ? "VG2" : pipe->ndx == BIT(3) ? "RGB0" :
				pipe->ndx == BIT(4) ? "RGB1" : pipe->ndx == BIT(5) ? "RGB2" :
				pipe->ndx == BIT(6) ? "DMA0" : pipe->ndx == BIT(7) ? "DMA1" :
				pipe->ndx == BIT(8) ? "VG3" : pipe->ndx == BIT(9) ? "RGB3" :
				pipe->ndx == BIT(10) ? "CURSOR0" : pipe->ndx == BIT(11) ? "CURSOR1" : "MAX_SSPP",
				pipe->mixer_stage - MDSS_MDP_STAGE_0,
				pipe->src_fmt->format,
				pipe->src_fmt->format == MDP_RGB_888 ? "MDP_RGB_888" :
				pipe->src_fmt->format == MDP_RGBX_8888 ? "MDP_RGBX_8888" :
				pipe->src_fmt->format == MDP_Y_CRCB_H2V2 ? "MDP_Y_CRCB_H2V2" :
				pipe->src_fmt->format == MDP_RGBA_8888 ? "MDP_RGBA_8888" :
				pipe->src_fmt->format == MDP_Y_CBCR_H2V2_VENUS ? "MDP_Y_CBCR_H2V2_VENUS" : "NONE",
				pipe->flags,
				pipe->src.x, pipe->src.y, pipe->src.w, pipe->src.h,
				pipe->dst.x, pipe->dst.y, pipe->dst.w, pipe->dst.h);

			/* Check format */
			if (pipe->src_fmt->format != MDP_RGB_888 &&
					pipe->src_fmt->format != MDP_RGBA_8888 &&
					pipe->src_fmt->format != MDP_RGBX_8888)
				continue;

			buf = list_first_entry_or_null(&pipe->buf_queue,
					struct mdss_mdp_data, pipe_list);

			if (IS_ERR_OR_NULL(buf))
				continue;

#if defined(CONFIG_ARCH_MSM8996)
			ion_buffers = (struct ion_buffer *)(buf->p[0].srcp_dma_buf->priv);
#else
			ion_buffers = buf->p[0].srcp_ihdl->buffer;
#endif
			if (!IS_ERR_OR_NULL(ion_buffers)) {
				/* To avoid buffer free */
				kref_get(&ion_buffers->ref);

				drain_pos = ion_heap_map_kernel(NULL, ion_buffers);

				if (!IS_ERR_OR_NULL(drain_pos)) {
					dst_x = pipe->dst.x / WIDTH_COMPRESS_RATIO;
					dst_y = pipe->dst.y / HEIGHT_COMPRESS_RATIO;

					drain_x = pipe->src.x;
					drain_y = pipe->src.y;

					drain_max_x = dst_x + (pipe->dst.w / WIDTH_COMPRESS_RATIO);
					drain_max_y = dst_y + (pipe->dst.h / HEIGHT_COMPRESS_RATIO);

					/*
						Because of BMP format, we save RGB to BGR & Flip(up -> donw) image
					*/
					if (pipe->src_fmt->format == MDP_RGB_888) {
						if (split_mode) {
							for (y = dst_y; y < drain_max_y; y++, drain_y += HEIGHT_COMPRESS_RATIO) {
								bitmap_y_reverse = panel_y_res -  y;
								drain_x = pipe->src.x;
								for (x = dst_x; x < drain_max_x; x++, drain_x += WIDTH_COMPRESS_RATIO) {
									dst_fb_pos_split[bitmap_y_reverse][x][0] |= samsung_drain_image_888(pipe, drain_pos, drain_x, drain_y, 2); /* B */
									dst_fb_pos_split[bitmap_y_reverse][x][1] |= samsung_drain_image_888(pipe, drain_pos, drain_x, drain_y, 1); /* G */
									dst_fb_pos_split[bitmap_y_reverse][x][2] |= samsung_drain_image_888(pipe, drain_pos, drain_x, drain_y, 0); /* R */
								}
							}

						} else {
							for (y = dst_y; y < drain_max_y; y++, drain_y += HEIGHT_COMPRESS_RATIO) {
								bitmap_y_reverse = panel_y_res -  y;
								drain_x = pipe->src.x;
								for (x = dst_x; x < drain_max_x; x++, drain_x += WIDTH_COMPRESS_RATIO) {
									dst_fb_pos_none_split[bitmap_y_reverse][x][0] |= samsung_drain_image_888(pipe, drain_pos, drain_x, drain_y, 2); /* B */
									dst_fb_pos_none_split[bitmap_y_reverse][x][1] |= samsung_drain_image_888(pipe, drain_pos, drain_x, drain_y, 1); /* G */
									dst_fb_pos_none_split[bitmap_y_reverse][x][2] |= samsung_drain_image_888(pipe, drain_pos, drain_x, drain_y, 0); /* R */
								}
							}
						}
					} else {
						if (split_mode) {
							for (y = dst_y; y < drain_max_y; y++, drain_y += HEIGHT_COMPRESS_RATIO) {
								bitmap_y_reverse = panel_y_res -  y;
								drain_x = pipe->src.x;
								for (x = dst_x; x < drain_max_x; x++, drain_x += WIDTH_COMPRESS_RATIO) {
									dst_fb_pos_split[bitmap_y_reverse][x][0] |= samsung_drain_image_8888(pipe, drain_pos, drain_x, drain_y, 2); /* B */
									dst_fb_pos_split[bitmap_y_reverse][x][1] |= samsung_drain_image_8888(pipe, drain_pos, drain_x, drain_y, 1); /* G */
									dst_fb_pos_split[bitmap_y_reverse][x][2] |= samsung_drain_image_8888(pipe, drain_pos, drain_x, drain_y, 0); /* R */
								}
							}
						} else {
							for (y = dst_y; y < drain_max_y; y++, drain_y += HEIGHT_COMPRESS_RATIO) {
								bitmap_y_reverse = panel_y_res -  y;
								drain_x = pipe->src.x;
								for (x = dst_x; x < drain_max_x; x++, drain_x += WIDTH_COMPRESS_RATIO) {
									dst_fb_pos_none_split[bitmap_y_reverse][x][0] |= samsung_drain_image_8888(pipe, drain_pos, drain_x, drain_y, 2); /* B */
									dst_fb_pos_none_split[bitmap_y_reverse][x][1] |= samsung_drain_image_8888(pipe, drain_pos, drain_x, drain_y, 1); /* G */
									dst_fb_pos_none_split[bitmap_y_reverse][x][2] |= samsung_drain_image_8888(pipe, drain_pos, drain_x, drain_y, 0); /* R */
								}
							}
						}
					}

					ion_heap_unmap_kernel(NULL, ion_buffers);
				} else
					pr_err("%s vmap fail", __func__);
			} else
				pr_err("%s ion_buffer is NULL\n", __func__);
		}
	}

	if (acquire_lock)
		mutex_unlock(&mdp5_data->list_lock);

	pr_err("%s end\n", __func__);
}

void samsung_image_dump_worker(struct work_struct *work)
{
	struct mdss_panel_info *panel_info = &vdd_data.ctrl_dsi[DISPLAY_1]->panel_data.panel_info;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(vdd_data.mfd_dsi[0]);
	unsigned int image_size;

	/* update compress ratio */
	if (vdd_data.mfd_dsi[0]->split_mode == MDP_DUAL_LM_DUAL_DISPLAY ||
		vdd_data.mfd_dsi[0]->split_mode == MDP_PINGPONG_SPLIT)
		image_size = (panel_info->xres * 2) * panel_info->yres * 3;
	else
		image_size = panel_info->xres * panel_info->yres * 3;

	if (image_size <= SZ_2M) {
		WIDTH_COMPRESS_RATIO = 1;
		HEIGHT_COMPRESS_RATIO = 1;
	} else if (image_size <= SZ_8M) {
		WIDTH_COMPRESS_RATIO = 2;
		HEIGHT_COMPRESS_RATIO = 2;
	} else {
		WIDTH_COMPRESS_RATIO = 4;
		HEIGHT_COMPRESS_RATIO = 4;
	}

	/* Clear image dump BMP format */
	memset(phys_to_virt(mdp5_data->splash_mem_addr + image_size), 0x00, sizeof(struct BITMAPFILEHEADER));

	if (in_interrupt()) {
		pr_err("%s in_interrupt()\n", __func__);
		return;
	}

	/* real dump */
	samsung_image_dump();
}

void samsung_mdss_image_dump(void)
{
	static int dump_done = false;

#if defined(CONFIG_SEC_DEBUG)
	if (!sec_debug_is_enabled())
		return;
#endif

	if (dump_done)
		return;
	else
		dump_done = true;

	if (in_interrupt()) {
		if (!IS_ERR_OR_NULL(vdd_data.image_dump_workqueue)) {
			queue_work(vdd_data.image_dump_workqueue, &vdd_data.image_dump_work);
			pr_err("%s dump_work queued\n", __func__);
		}
	} else
		samsung_image_dump_worker(NULL);
}

