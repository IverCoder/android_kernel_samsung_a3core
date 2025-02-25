/*
 * Copyright (C) 2012--2015 Spreadtrum Communications Inc.
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
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/dma-buf.h>
#include <linux/platform_device.h>
#include <linux/sprd_iommu.h>
#include <uapi/video/sprd_jpg.h>
#include "ion.h"
#include "sprd_jpg_common.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-jpg: " fmt

struct jpg_iommu_map_entry {
	struct list_head list;

	int fd;
	unsigned long iova_addr;
	size_t iova_size;

	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *table;
};

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-jpg: " fmt

#if IS_ENABLED(CONFIG_SPRD_JPG_CALL_VSP_PW_DOMAIN)
#include <uapi/video/sprd_vsp_pw_domain.h>

int sprd_jpg_pw_on(void)
{
	int ret = 0;

	ret = vsp_pw_on(VSP_PW_DOMAIN_VSP_JPG);
	return ret;
}

int sprd_jpg_pw_off(void)
{
	int ret = 0;

	ret = vsp_pw_off(VSP_PW_DOMAIN_VSP_JPG);
	return ret;
}

int sprd_jpg_domain_eb(void) { return 0; }
int sprd_jpg_domain_disable(void) { return 0; }

#elif (IS_ENABLED(CONFIG_SPRD_CAM_PW_DOMAIN_R4P0) || \
	IS_ENABLED(CONFIG_SPRD_MM_PW_DOMAIN_R6P0) || \
	IS_ENABLED(CONFIG_SPRD_CAM_PW_DOMAIN_R5P1) || \
	IS_ENABLED(CONFIG_SPRD_CAM_PW_DOMAIN_R7P0) || \
	IS_ENABLED(CONFIG_SPRD_CAM_PW_DOMAIN_R8P0) || \
	IS_ENABLED(CONFIG_SPRD_CAM_PW_DOMAIN_QOGIRN6PRO))

int sprd_jpg_pw_on(void)
{
	int ret = 0;

	ret = sprd_cam_pw_on();
	return ret;
}

int sprd_jpg_pw_off(void)
{
	int ret = 0;

	ret = sprd_cam_pw_off();
	return ret;
}

int sprd_jpg_domain_eb(void)
{
	return sprd_cam_domain_eb();
}

int sprd_jpg_domain_disable(void)
{
	return sprd_cam_domain_disable();
}
#else
int sprd_jpg_pw_on(void)
{
	int ret = 0;
	return ret;
}

int sprd_jpg_pw_off(void)
{
	int ret = 0;
	return ret;
}

int sprd_jpg_domain_eb(void) { return 0; }
int sprd_jpg_domain_disable(void) { return 0; }

#endif

struct clk *jpg_get_clk_src_name(struct clock_name_map_t clock_name_map[],
				unsigned int freq_level,
				unsigned int max_freq_level)
{
	if (freq_level >= max_freq_level) {
		pr_info("set freq_level to 0\n");
		freq_level = 0;
	}

	pr_debug(" freq_level %d %s\n", freq_level,
		 clock_name_map[freq_level].name);
	return clock_name_map[freq_level].clk_parent;
}

int find_jpg_freq_level(struct clock_name_map_t clock_name_map[],
			unsigned long freq,
			unsigned int max_freq_level)
{
	int level = 0;
	int i;

	for (i = 0; i < max_freq_level; i++) {
		if (clock_name_map[i].freq == freq) {
			level = i;
			break;
		}
	}
	return level;
}

int jpg_get_mm_clk(struct jpg_dev_t *jpg_hw_dev)
{
	int ret = 0;
	struct clk *jpg_domain_eb;
	struct clk *clk_aon_jpg_emc_eb;
	struct clk *jpg_dev_eb;
	struct clk *jpg_ckg_eb;
	struct clk *clk_vsp_mq_ahb_eb;
	struct clk *clk_ahb_vsp;
	struct clk *clk_emc_vsp;
	struct clk *jpg_clk;
	struct clk *clk_parent;

	jpg_domain_eb = devm_clk_get(jpg_hw_dev->jpg_dev, "jpg_domain_eb");

	if (IS_ERR(jpg_domain_eb)) {
		pr_err("Failed : Can't get clock [%s]!\n", "jpg_domain_eb");
		pr_err("jpg_domain_eb =  %p\n", jpg_domain_eb);
		jpg_hw_dev->jpg_domain_eb = NULL;
		ret = PTR_ERR(jpg_domain_eb);
	} else {
		jpg_hw_dev->jpg_domain_eb = jpg_domain_eb;
	}

	if (jpg_hw_dev->version == SHARKL3) {
		clk_aon_jpg_emc_eb = devm_clk_get(jpg_hw_dev->jpg_dev,
					"clk_aon_jpg_emc_eb");
		if (IS_ERR(clk_aon_jpg_emc_eb)) {
			pr_err("Can't get clock [%s]!\n", "clk_aon_jpg_emc_eb");
			pr_err("clk_aon_jpg_emc_eb = %p\n", clk_aon_jpg_emc_eb);
			jpg_hw_dev->clk_aon_jpg_emc_eb = NULL;
			ret = PTR_ERR(clk_aon_jpg_emc_eb);
		} else {
			jpg_hw_dev->clk_aon_jpg_emc_eb = clk_aon_jpg_emc_eb;
		}
	}

	jpg_dev_eb =
	    devm_clk_get(jpg_hw_dev->jpg_dev, "jpg_dev_eb");
	if (IS_ERR(jpg_dev_eb)) {
		pr_err("Failed : Can't get clock [%s]!\n", "jpg_dev_eb");
		pr_err("jpg_dev_eb =  %p\n", jpg_dev_eb);
		jpg_hw_dev->jpg_dev_eb = NULL;
		ret = PTR_ERR(jpg_dev_eb);
	} else {
		jpg_hw_dev->jpg_dev_eb = jpg_dev_eb;
	}

	jpg_ckg_eb =
	    devm_clk_get(jpg_hw_dev->jpg_dev, "jpg_ckg_eb");

	if (IS_ERR(jpg_ckg_eb)) {
		pr_err("Failed : Can't get clock [%s]!\n",
		       "jpg_ckg_eb");
		pr_err("jpg_ckg_eb =  %p\n", jpg_ckg_eb);
		jpg_hw_dev->jpg_ckg_eb = NULL;
		ret = PTR_ERR(jpg_ckg_eb);
	} else {
		jpg_hw_dev->jpg_ckg_eb = jpg_ckg_eb;
	}

	if (jpg_hw_dev->version == PIKE2) {
		clk_vsp_mq_ahb_eb =
		    devm_clk_get(jpg_hw_dev->jpg_dev, "clk_vsp_mq_ahb_eb");

		if (IS_ERR(clk_vsp_mq_ahb_eb)) {
			pr_err("Failed: Can't get clock [%s]! %p\n",
			       "clk_vsp_mq_ahb_eb", clk_vsp_mq_ahb_eb);
			jpg_hw_dev->clk_vsp_mq_ahb_eb = NULL;
			ret = PTR_ERR(clk_vsp_mq_ahb_eb);
		} else {
			jpg_hw_dev->clk_vsp_mq_ahb_eb = clk_vsp_mq_ahb_eb;
		}
	}

	if (jpg_hw_dev->version == SHARKL3) {
		clk_ahb_vsp =
		    devm_clk_get(jpg_hw_dev->jpg_dev, "clk_ahb_vsp");

		if (IS_ERR(clk_ahb_vsp)) {
			pr_err("Failed: Can't get clock [%s]! %p\n",
			       "clk_ahb_vsp", clk_ahb_vsp);
			jpg_hw_dev->clk_ahb_vsp = NULL;
			ret = PTR_ERR(clk_ahb_vsp);
		} else
			jpg_hw_dev->clk_ahb_vsp = clk_ahb_vsp;

		clk_parent = devm_clk_get(jpg_hw_dev->jpg_dev,
				"clk_ahb_vsp_parent");
		if (IS_ERR(clk_parent)) {
			pr_err("clock[%s]: failed to get parent in probe!\n",
				 "clk_ahb_vsp_parent");
			ret = PTR_ERR(clk_parent);
		} else
			jpg_hw_dev->ahb_parent_clk = clk_parent;

		clk_emc_vsp =
			devm_clk_get(jpg_hw_dev->jpg_dev, "clk_emc_vsp");

		if (IS_ERR(clk_emc_vsp)) {
			pr_err("Failed: Can't get clock [%s]! %p\n",
				   "clk_emc_vsp", clk_emc_vsp);
			jpg_hw_dev->clk_emc_vsp = NULL;
			ret = PTR_ERR(clk_emc_vsp);
		} else
			jpg_hw_dev->clk_emc_vsp = clk_emc_vsp;

		clk_parent = devm_clk_get(jpg_hw_dev->jpg_dev,
				"clk_emc_vsp_parent");
		if (IS_ERR(clk_parent)) {
			pr_err("clock[%s]: failed to get parent in probe!\n",
				 "clk_emc_vsp_parent");
			ret = PTR_ERR(clk_parent);
		} else
			jpg_hw_dev->emc_parent_clk = clk_parent;
	}

	jpg_clk = devm_clk_get(jpg_hw_dev->jpg_dev, "jpg_clk");

	if (IS_ERR(jpg_clk)) {
		pr_err("Failed : Can't get clock [%s}!\n", "jpg_clk");
		pr_err("jpg_clk =  %p\n", jpg_clk);
		jpg_hw_dev->jpg_clk = NULL;
		ret = PTR_ERR(jpg_clk);
	} else {
		jpg_hw_dev->jpg_clk = jpg_clk;
	}

	clk_parent = jpg_get_clk_src_name(jpg_hw_dev->clock_name_map, 0,
			jpg_hw_dev->max_freq_level);
	jpg_hw_dev->jpg_parent_clk_df = clk_parent;

	return ret;
}

#ifdef CONFIG_COMPAT
int compat_get_mmu_map_data(struct compat_jpg_iommu_map_data __user *
				   data32,
				   struct jpg_iommu_map_data __user *data)
{
	compat_int_t i;
	compat_size_t s;
	compat_ulong_t ul;
	int err;

	err = get_user(i, &data32->fd);
	err |= put_user(i, &data->fd);
	err |= get_user(s, &data32->size);
	err |= put_user(s, &data->size);
	err |= get_user(ul, &data32->iova_addr);
	err |= put_user(ul, &data->iova_addr);

	return err;
};

int compat_put_mmu_map_data(struct compat_jpg_iommu_map_data __user *
				   data32,
				   struct jpg_iommu_map_data __user *data)
{
	compat_int_t i;
	compat_size_t s;
	compat_ulong_t ul;
	int err;

	err = get_user(i, &data->fd);
	err |= put_user(i, &data32->fd);
	err |= get_user(s, &data->size);
	err |= put_user(s, &data32->size);
	err |= get_user(ul, &data->iova_addr);
	err |= put_user(ul, &data32->iova_addr);

	return err;
};

long compat_jpg_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	long ret = 0;
	struct jpg_fh *jpg_fp = filp->private_data;

	if (!filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	if (jpg_fp == NULL) {
		pr_err("%s, jpg_ioctl error occurred, jpg_fp == NULL\n",
		       __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case COMPAT_JPG_GET_IOVA:
		{
			struct compat_jpg_iommu_map_data __user *data32;
			struct jpg_iommu_map_data __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL) {
				pr_err("%s %d, compat_alloc_user_space failed",
				       __func__, __LINE__);
				return -EFAULT;
			}

			err = compat_get_mmu_map_data(data32, data);
			if (err) {
				pr_err("%s %d, compat_get_mmu_map_data failed",
				       __func__, __LINE__);
				return err;
			}
			ret = filp->f_op->unlocked_ioctl(filp, JPG_GET_IOVA,
							 (unsigned long)data);
			err = compat_put_mmu_map_data(data32, data);
			return ret ? ret : err;
		}
	case COMPAT_JPG_FREE_IOVA:
		{
			struct compat_jpg_iommu_map_data __user *data32;
			struct jpg_iommu_map_data __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL) {
				pr_err("%s %d, compat_alloc_user_space failed",
				       __func__, __LINE__);
				return -EFAULT;
			}

			err = compat_get_mmu_map_data(data32, data);
			if (err) {
				pr_err("%s %d, compat_get_mmu_map_data failed",
				       __func__, __LINE__);
				return err;
			}
			ret = filp->f_op->unlocked_ioctl(filp, JPG_FREE_IOVA,
							 (unsigned long)data);
			err = compat_put_mmu_map_data(data32, data);
			return ret ? ret : err;
		}
	default:
		return filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)
						  compat_ptr(arg));
	}

	return ret;
}
#endif
int jpg_get_iova(struct jpg_dev_t *jpg_hw_dev,
		 struct jpg_iommu_map_data *mapdata, void __user *arg)
{
	int ret = 0;
	/*struct jpg_iommu_map_data mapdata; */
	struct sprd_iommu_map_data iommu_map_data;
	struct sprd_iommu_unmap_data iommu_ummap_data;
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *table;
	struct jpg_iommu_map_entry *entry;

	if (sprd_iommu_attach_device(jpg_hw_dev->jpg_dev) == 0) {
		dmabuf = dma_buf_get(mapdata->fd);
		if (IS_ERR_OR_NULL(dmabuf)) {
			pr_err("get dmabuf failed\n");
			ret = PTR_ERR(dmabuf);
			goto err_get_dmabuf;
		}

		attachment = dma_buf_attach(dmabuf, jpg_hw_dev->jpg_dev);
		if (IS_ERR_OR_NULL(attachment)) {
			pr_err("Failed to attach dmabuf=%p\n", dmabuf);
			ret = PTR_ERR(attachment);
			goto err_attach;
		}

		table = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
		if (IS_ERR_OR_NULL(table)) {
			pr_err("Failed to map attachment=%p\n", attachment);
			ret = PTR_ERR(table);
			goto err_map_attachment;
		}

		iommu_map_data.buf = dmabuf->priv;
		iommu_map_data.iova_size = ((struct ion_buffer *)(dmabuf->priv))->size;
		iommu_map_data.ch_type = SPRD_IOMMU_FM_CH_RW;
		ret = sprd_iommu_map(jpg_hw_dev->jpg_dev, &iommu_map_data);
		if (!ret) {
			mutex_lock(&jpg_hw_dev->map_lock);
			entry = kzalloc(sizeof(*entry), GFP_KERNEL);
			if (!entry) {
				mutex_unlock(&jpg_hw_dev->map_lock);
				pr_err("fatal error! kzalloc fail!\n");
				iommu_ummap_data.iova_addr = iommu_map_data.iova_addr;
				iommu_ummap_data.iova_size = iommu_map_data.iova_size;
				iommu_ummap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
				iommu_ummap_data.buf = NULL;
				ret = -ENOMEM;
				goto err_kzalloc;
			}
			entry->fd = mapdata->fd;
			entry->iova_addr = iommu_map_data.iova_addr;
			entry->iova_size = iommu_map_data.iova_size;
			entry->dmabuf = dmabuf;
			entry->attachment = attachment;
			entry->table = table;
			list_add(&entry->list, &jpg_hw_dev->map_list);
			mutex_unlock(&jpg_hw_dev->map_lock);

			mapdata->iova_addr = iommu_map_data.iova_addr;
			mapdata->size = iommu_map_data.iova_size;
			pr_debug("jpg iommu map success iova addr=%llu size=%llu\n",
				mapdata->iova_addr, mapdata->size);
			ret =
				copy_to_user((void __user *)arg,
					 (void *)mapdata,
					 sizeof(struct jpg_iommu_map_data));
			if (ret) {
				pr_err("fatal error! copy_to_user failed, ret=%d\n", ret);
				goto err_copy_to_user;
			}
			pr_debug("suceess to add map_node(iova_addr=%llu, size=%llu)\n",
				mapdata->iova_addr, mapdata->size);
		} else {
			pr_err("jpg iommu map failed, ret=%d, map_size=%zu\n",
				ret, iommu_map_data.iova_size);
			goto err_iommu_map;
			}
	} else {
		ret =
		    copy_to_user((void __user *)arg,
				 (void *)mapdata,
				 sizeof(struct jpg_iommu_map_data));
		if (ret) {
			pr_err("copy_to_user failed, ret %d\n", ret);
			return -EFAULT;
		}
	}
	return ret;
err_copy_to_user:
	mutex_lock(&jpg_hw_dev->map_lock);
	list_del(&entry->list);
	kfree(entry);
	mutex_unlock(&jpg_hw_dev->map_lock);
err_kzalloc:
	ret = sprd_iommu_unmap(jpg_hw_dev->jpg_dev, &iommu_ummap_data);
	if (ret) {
		pr_err("sprd_iommu_unmap failed, ret=%d, addr&size: 0x%lx 0x%zx\n",
			ret, iommu_ummap_data.iova_addr, iommu_ummap_data.iova_size);
	}
err_iommu_map:
	dma_buf_unmap_attachment(attachment, table, DMA_BIDIRECTIONAL);
err_map_attachment:
	dma_buf_detach(dmabuf, attachment);
err_attach:
	dma_buf_put(dmabuf);
err_get_dmabuf:

	return ret;

}

int jpg_free_iova(struct jpg_dev_t *jpg_hw_dev,
		  struct jpg_iommu_map_data *ummapdata)
{

	int ret = 0;
	struct jpg_iommu_map_entry *entry = NULL;
	struct sprd_iommu_unmap_data iommu_ummap_data;
	int b_find = 0;

	if (sprd_iommu_attach_device(jpg_hw_dev->jpg_dev) == 0) {
		mutex_lock(&jpg_hw_dev->map_lock);
		list_for_each_entry(entry, &jpg_hw_dev->map_list, list) {
			if (entry->iova_addr == ummapdata->iova_addr) {
				b_find = 1;
				break;
			}
		}
		if (b_find) {
			iommu_ummap_data.iova_addr = entry->iova_addr;
			iommu_ummap_data.iova_size = entry->iova_size;
			iommu_ummap_data.ch_type = SPRD_IOMMU_FM_CH_RW;
			iommu_ummap_data.buf = NULL;
			list_del(&entry->list);
			pr_debug("success to find node(iova_addr=%llu, size=%llu)\n",
				ummapdata->iova_addr, ummapdata->size);
		} else {
			pr_err("fatal error! not find node(iova_addr=%llu, size=%llu)\n",
				ummapdata->iova_addr, ummapdata->size);
			mutex_unlock(&jpg_hw_dev->map_lock);
			return -EFAULT;
		}
		mutex_unlock(&jpg_hw_dev->map_lock);

		ret =
		    sprd_iommu_unmap(jpg_hw_dev->jpg_dev,
					&iommu_ummap_data);

		if (ret) {
			pr_err("sprd_iommu_unmap failed: ret=%d, iova_addr=%llu, size=%llu\n",
				ret, ummapdata->iova_addr, ummapdata->size);
			return ret;
		}
		pr_debug("sprd_iommu_unmap success: iova_addr=%llu size=%llu\n",
			ummapdata->iova_addr, ummapdata->size);
		dma_buf_unmap_attachment(entry->attachment, entry->table, DMA_BIDIRECTIONAL);
		dma_buf_detach(entry->dmabuf, entry->attachment);
		dma_buf_put(entry->dmabuf);
		kfree(entry);
	}

	return ret;
}

int poll_mbio_vlc_done(struct jpg_dev_t *jpg_hw_dev, int cmd0)
{
	int ret = 0;

	pr_debug("jpg_poll_begin\n");
	if (cmd0 == INTS_MBIO) {
		/* JPG_ACQUAIRE_MBIO_DONE */
		ret = wait_event_interruptible_timeout(
				jpg_hw_dev->wait_queue_work_MBIO,
				jpg_hw_dev->condition_work_MBIO,
				msecs_to_jiffies(JPG_TIMEOUT_MS));

		if (ret == -ERESTARTSYS) {
			pr_err("jpg error start -ERESTARTSYS\n");
			ret = -EINVAL;
		} else if (ret == 0) {
			pr_err("jpg error start  timeout\n");
			ret = readl_relaxed(
				(void __iomem *)(jpg_hw_dev->sprd_jpg_virt +
				GLB_INT_STS_OFFSET));
			pr_err("jpg_int_status %x", ret);
			ret = -ETIMEDOUT;
		} else {
			ret = 0;
		}

		if (ret) {
			/* timeout, clear jpg int */
			writel_relaxed((1 << 3) | (1 << 2) | (1 << 1) |
				(1 << 0),
				(void __iomem *)(jpg_hw_dev->sprd_jpg_virt +
				GLB_INT_CLR_OFFSET));
			ret = 1;
		} else {
			/* poll successful */
			ret = 0;
		}

		jpg_hw_dev->jpg_int_status &= (~0x8);
		jpg_hw_dev->condition_work_MBIO = 0;
	} else if (cmd0 == INTS_VLC) {
		/* JPG_ACQUAIRE_VLC_DONE */
		ret = wait_event_interruptible_timeout
			    (jpg_hw_dev->wait_queue_work_VLC,
			     jpg_hw_dev->condition_work_VLC,
			     msecs_to_jiffies(JPG_TIMEOUT_MS));

		if (ret == -ERESTARTSYS) {
			pr_err("jpg error start -ERESTARTSYS\n");
			ret = -EINVAL;
		} else if (ret == 0) {
			pr_err("jpg error start  timeout\n");
			ret = readl_relaxed(
				(void __iomem *)(jpg_hw_dev->sprd_jpg_virt +
							   GLB_INT_STS_OFFSET));
			pr_err("jpg_int_status %x", ret);
			ret = -ETIMEDOUT;
		} else {
			ret = 0;
		}

		if (ret) {
			/* timeout, clear jpg int */
			writel_relaxed((1 << 3) | (1 << 2) | (1 << 1) |
				(1 << 0),
				(void __iomem *)(jpg_hw_dev->sprd_jpg_virt +
				GLB_INT_CLR_OFFSET));
			ret = 1;
		} else {
			/* poll successful */
			ret = 4;
		}

		jpg_hw_dev->jpg_int_status &= (~0x2);
		jpg_hw_dev->condition_work_VLC = 0;
	} else {
		pr_err("JPG_ACQUAIRE_MBIO_DONE error arg");
		ret = -1;
	}
	pr_debug("jpg_poll_end\n");
	return ret;
}

int jpg_clk_enable(struct jpg_dev_t *jpg_hw_dev)
{
	int ret = 0;

	pr_info("jpg JPG_ENABLE\n");

	ret = clk_prepare_enable(jpg_hw_dev->jpg_domain_eb);
	if (ret) {
		pr_err("jpg jpg_domain_eb clk_prepare_enable failed!\n");
		return ret;
	}
	pr_debug("jpg jpg_domain_eb clk_prepare_enable ok.\n");

	if (jpg_hw_dev->version == SHARKL3 &&
			jpg_hw_dev->clk_aon_jpg_emc_eb) {
		ret = clk_prepare_enable(jpg_hw_dev->clk_aon_jpg_emc_eb);
		if (ret) {
			pr_err("clk_aon_jpg_emc_eb clk_enable failed!\n");
			goto clk_disable_0;
		}
		pr_debug("clk_aon_jpg_emc_eb clk_prepare_enable ok.\n");
	}

	ret = clk_prepare_enable(jpg_hw_dev->jpg_dev_eb);
	if (ret) {
		pr_err("jpg_dev_eb clk_prepare_enable failed!\n");
		goto clk_disable_1;
	}
	pr_debug("jpg_dev_eb clk_prepare_enable ok.\n");

	ret = clk_prepare_enable(jpg_hw_dev->jpg_ckg_eb);
	if (ret) {
		pr_err("jpg_ckg_eb prepare_enable failed!\n");
		goto clk_disable_2;
	}
	pr_debug("jpg_ckg_eb clk_prepare_enable ok.\n");

	if (jpg_hw_dev->version == SHARKL3) {
		ret = clk_set_parent(jpg_hw_dev->clk_ahb_vsp,
				   jpg_hw_dev->ahb_parent_clk);
		if (ret) {
			pr_err("clock[%s]: clk_set_parent() failed!",
			       "ahb_parent_clk");
			goto clk_disable_3;
		}

		ret = clk_prepare_enable(jpg_hw_dev->clk_ahb_vsp);
		if (ret) {
			pr_err("clk_ahb_vsp: clk_prepare_enable failed!\n");
			goto clk_disable_3;
		}
		pr_info("clk_ahb_vsp: clk_prepare_enable ok.\n");

		ret = clk_set_parent(jpg_hw_dev->clk_emc_vsp,
				   jpg_hw_dev->emc_parent_clk);
		if (ret) {
			pr_err("clock[%s]: clk_set_parent() failed!",
				   "emc_parent_clk");
			goto clk_disable_4;
		}

		ret = clk_prepare_enable(jpg_hw_dev->clk_emc_vsp);
		if (ret) {
			pr_err("clk_emc_vsp: clk_prepare_enable failed!\n");
			goto clk_disable_4;
		}
		pr_debug("clk_emc_vsp: clk_prepare_enable ok.\n");
	}

	if (jpg_hw_dev->clk_vsp_mq_ahb_eb) {
		ret = clk_prepare_enable(jpg_hw_dev->clk_vsp_mq_ahb_eb);
		if (ret) {
			pr_err("clk_vsp_mq_ahb_eb: clk_prepare_enable failed!");
			goto clk_disable_5;
		}
		pr_debug("jpg clk_vsp_mq_ahb_eb clk_prepare_enable ok.\n");
	}

	ret = clk_set_parent(jpg_hw_dev->jpg_clk,
		jpg_hw_dev->jpg_parent_clk_df);
	if (ret) {
		pr_err("clock[%s]: clk_set_parent() failed!",
				"clk_jpg");
		goto clk_disable_6;
	}

	ret = clk_set_parent(jpg_hw_dev->jpg_clk,
			jpg_hw_dev->jpg_parent_clk);
	if (ret) {
		pr_err("clock[%s]: clk_set_parent() failed!",
			"clk_jpg");
		goto clk_disable_6;
	}

	ret = clk_prepare_enable(jpg_hw_dev->jpg_clk);
	if (ret) {
		pr_err("jpg_clk clk_prepare_enable failed!\n");
		goto clk_disable_6;
	}

	pr_info("jpg_clk clk_prepare_enable ok.\n");
	return ret;

clk_disable_6:
	if (jpg_hw_dev->version == PIKE2)
		clk_disable_unprepare(jpg_hw_dev->clk_vsp_mq_ahb_eb);
clk_disable_5:
	if (jpg_hw_dev->version == SHARKL3)
		clk_disable_unprepare(jpg_hw_dev->clk_emc_vsp);
clk_disable_4:
	if (jpg_hw_dev->version == SHARKL3)
		clk_disable_unprepare(jpg_hw_dev->clk_ahb_vsp);
clk_disable_3:
	clk_disable_unprepare(jpg_hw_dev->jpg_ckg_eb);
clk_disable_2:
	clk_disable_unprepare(jpg_hw_dev->jpg_dev_eb);
clk_disable_1:
	if (jpg_hw_dev->version == SHARKL3)
		clk_disable_unprepare(jpg_hw_dev->clk_aon_jpg_emc_eb);
clk_disable_0:
	clk_disable_unprepare(jpg_hw_dev->jpg_domain_eb);

	return ret;
}


void jpg_clk_disable(struct jpg_dev_t *jpg_hw_dev)
{
	if (jpg_hw_dev->version == SHARKL3) {
		if (jpg_hw_dev->clk_ahb_vsp)
			clk_disable_unprepare(jpg_hw_dev->clk_ahb_vsp);
		if (jpg_hw_dev->clk_emc_vsp)
			clk_disable_unprepare(jpg_hw_dev->clk_emc_vsp);
	}
	clk_disable_unprepare(jpg_hw_dev->jpg_clk);
	if (jpg_hw_dev->version == SHARKL3) {
		if (jpg_hw_dev->clk_aon_jpg_emc_eb)
			clk_disable_unprepare(jpg_hw_dev->clk_aon_jpg_emc_eb);
	}
	if (jpg_hw_dev->clk_vsp_mq_ahb_eb)
		clk_disable_unprepare(jpg_hw_dev->clk_vsp_mq_ahb_eb);
	clk_disable_unprepare(jpg_hw_dev->jpg_ckg_eb);
	clk_disable_unprepare(jpg_hw_dev->jpg_dev_eb);
	clk_disable_unprepare(jpg_hw_dev->jpg_domain_eb);
}
