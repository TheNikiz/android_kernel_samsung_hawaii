/*
 * SAMSUNG NFC Controller
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Woonki Lee <woonki84.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/wait.h>
#include <linux/delay.h>

#ifdef CONFIG_SEC_NFC_I2C_SB
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#endif

#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif
#ifdef CONFIG_KONA_PI_MGR_FORCE_DISABLE
#include <plat/pi_mgr.h>
#endif

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/nfc/sec_nfc_sb.h>

#ifdef CONFIG_SEC_NFC_I2C_SB
enum sec_nfc_irq {
	SEC_NFC_NONE,
	SEC_NFC_INT,
	SEC_NFC_TRY_AGAIN,
};
#endif

// CONFIG_KONA_PI_MGR_FORCE_DISABLE will be removed - jaehyuk7.lee
#ifdef CONFIG_HAS_WAKELOCK
struct wake_lock nfc_wake_lock;
#endif
#ifdef CONFIG_KONA_PI_MGR_FORCE_DISABLE
static struct pi_mgr_qos_node qos_node;
#endif


struct sec_nfc_info {
	struct miscdevice miscdev;
	struct mutex mutex;
	enum sec_nfc_state state;
	struct device *dev;
	struct sec_nfc_platform_data *pdata;

#ifdef CONFIG_SEC_NFC_I2C_SB
	struct i2c_client *i2c_dev;
	struct mutex read_mutex;
	enum sec_nfc_irq read_irq;
	wait_queue_head_t read_wait;
	size_t buflen;
	u8 *buf;
#endif
#ifdef CONFIG_HAS_WAKELOCK
	unsigned int read_count;
#endif
};

static struct sec_nfc_info debug_info;

#define LOG_OUT 0
#ifdef CONFIG_HAS_WAKELOCK
static void sec_nfc_wakelock_init(void)
{
	wake_lock_init(&nfc_wake_lock, WAKE_LOCK_SUSPEND, "NFCWAKE");
#ifdef CONFIG_KONA_PI_MGR_FORCE_DISABLE
	pi_mgr_qos_add_request(&qos_node, "sec_nfc_qos",
		PI_MGR_PI_ID_ARM_SUB_SYSTEM, PI_MGR_QOS_DEFAULT_VALUE);
#endif
}
static void sec_nfc_wakelock(bool is_lock)
{
	if(is_lock){
			if (!wake_lock_active(&nfc_wake_lock)){
				wake_lock(&nfc_wake_lock);
#if LOG_OUT
				pr_info("[NFC] %s : lock!\n", __func__);
#endif
			}
#ifdef CONFIG_KONA_PI_MGR_FORCE_DISABLE
			pi_mgr_qos_request_update(&qos_node, 0);
#endif
	}
	else{
	
			if (wake_lock_active(&nfc_wake_lock)){
				wake_unlock(&nfc_wake_lock);
#if LOG_OUT
				pr_info("[NFC] %s : unlock!\n", __func__);
#endif				

			}
#ifdef CONFIG_KONA_PI_MGR_FORCE_DISABLE
			pi_mgr_qos_request_update(&qos_node, PI_MGR_QOS_DEFAULT_VALUE);
#endif	
	}
	
}
#endif


#ifdef CONFIG_SEC_NFC_I2C_SB
static irqreturn_t sec_nfc_irq_thread_fn(int irq, void *dev_id)
{
	struct sec_nfc_info *info = dev_id;
	
	if (info->state == SEC_NFC_ST_OFF)
		return IRQ_HANDLED;

	mutex_lock(&info->read_mutex);
#if LOG_OUT
	pr_info("[NFC] %s : I2C Interrupt is occurred!\n", __func__);
#endif
	info->read_irq = SEC_NFC_INT;
#ifdef CONFIG_HAS_WAKELOCK
	if (info->state == SEC_NFC_ST_FIRM)
		info->read_count += 1;
	else
		info->read_count += 2;

	sec_nfc_wakelock(true);
#endif
	mutex_unlock(&info->read_mutex);
	wake_up_interruptible(&info->read_wait);

	return IRQ_HANDLED;
}

#endif

int sec_nfc_get_state(void)
{
	int ret = 0;
	pr_err("[NFC] %s: SCL=%d,SDA=%d,irq=%d,ven=%d,firm=%d\n", __func__,
		gpio_get_value(16),
		gpio_get_value(17),
		gpio_get_value(90),
		gpio_get_value(25),
		gpio_get_value(26) );

	return ret;
}

static int sec_nfc_set_state(struct sec_nfc_info *info,
					enum sec_nfc_state state)
{
	struct sec_nfc_platform_data *pdata = info->pdata;

	/* intfo lock is aleady getten before calling this function */
	info->state = state;

	gpio_set_value(pdata->ven, 0);
	gpio_set_value(pdata->firm, 0);
	msleep(SEC_NFC_VEN_WAIT_TIME);

#if LOG_OUT
	pr_info("[NFC] %s : VEN LOW, FIRM LOW(during : %d ms)\n",
		__func__, SEC_NFC_VEN_WAIT_TIME);
#endif
#ifdef CONFIG_HAS_WAKELOCK
	mutex_lock(&info->read_mutex);
	info->read_count = 0;
	info->read_irq = SEC_NFC_NONE;
	mutex_unlock(&info->read_mutex);
#endif
	if (state == SEC_NFC_ST_FIRM) {
		gpio_set_value(pdata->firm, 1);
		msleep(SEC_NFC_VEN_WAIT_TIME);
		gpio_set_value(pdata->ven, 1);
#if LOG_OUT
		pr_info("[NFC] %s : FIRM HIGH, VEN HIGH(during : %d ms)\n",
			__func__, SEC_NFC_VEN_WAIT_TIME);
#endif			
	}
	else if (state == SEC_NFC_ST_NORM)
	{
		gpio_set_value(pdata->ven, 1);
		msleep(SEC_NFC_VEN_WAIT_TIME);
#if LOG_OUT		
		pr_info("[NFC] %s : VEN HIGH(during : %d ms)\n", __func__, SEC_NFC_VEN_WAIT_TIME);
#endif		
	}
	else
	{
		msleep(SEC_NFC_VEN_WAIT_TIME);
#if LOG_OUT		
		pr_info("[NFC] %s : VEN LOW, FIRM LOW (during : %d ms)\n", __func__, SEC_NFC_VEN_WAIT_TIME);
#endif		
	}

	dev_dbg(info->dev, "Power state is : %d\n", state);

	return 0;
}

#ifdef CONFIG_SEC_NFC_I2C_SB
static int sec_nfc_reset(struct sec_nfc_info *info)
{
	dev_err(info->dev, "i2c failed. return resatrt to M/W\n");
#if LOG_OUT		
	pr_info("[NFC] %s : NFC RESET!\n", __func__);
#endif	
	sec_nfc_set_state(info, SEC_NFC_ST_NORM);

	return -ERESTART;
}

static ssize_t sec_nfc_read(struct file *file, char __user *buf,
			   size_t count, loff_t *ppos)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	enum sec_nfc_irq irq;
	int ret = 0;
	int i = 0;
	unsigned char tempBuffer[256];

	/* dev_dbg(info->dev, "%s: info: %p, count: %zu\n",
		__func__, info, count);
	*/
#if LOG_OUT
	pr_info("[NFC] %s : Enter - info: %p, count: %zu\n", __func__, info, count);
#endif
	mutex_lock(&info->mutex);

	if (info->state == SEC_NFC_ST_OFF) {
		/* dev_err(info->dev, "sec_nfc is not enabled\n"); */
		pr_err("[NFC] sec_nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	mutex_lock(&info->read_mutex);
	irq = info->read_irq;
	mutex_unlock(&info->read_mutex);
	if (irq == SEC_NFC_NONE) {
		if (file->f_flags & O_NONBLOCK) {
			/* dev_err(info->dev, "it is nonblock\n"); */
			pr_err("[NFC] it is nonblock\n");
			ret = -EAGAIN;
			goto out;
		}
	}

	/* i2c recv */
	if (count > info->buflen)
		count = info->buflen;

	if (count < SEC_NFC_MSG_MIN_SIZE || count > SEC_NFC_MSG_MAX_SIZE) {
		/* dev_err(info->dev, "user required wrong size\n"); */
		pr_err("[NFC] %d : user required wrong size -max: %d, size : %d \n",__func__, SEC_NFC_MSG_MAX_SIZE, count);
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&info->read_mutex);
	
#if LOG_OUT
	pr_info("[NFC] %s : BEFORE i2c_master_recv\n", __func__);
#endif
	memset(info->buf, 0, count);
	ret = i2c_master_recv(info->i2c_dev, info->buf, count);

	if (ret == -EREMOTEIO) {
// patch for download failed
		//ret = sec_nfc_reset(info);
// patch for download failed
		goto read_error;
	} else if (ret != count) {
		pr_err("[NFC] %s : READ FAILED : ret : %d,  expected : %d\n",
			__func__, ret, count);
		/* ret = -EREMOTEIO; */
		goto read_error;
	} else if (info->buf[ret-1] > 0xFF) {
		pr_err("[NFC] %s : Read critical error!!!! 0x%02x",
			__func__, info->buf[ret-1]);
		sec_nfc_get_state();

		goto read_error;
	}

	info->read_irq = SEC_NFC_NONE;
#ifdef CONFIG_HAS_WAKELOCK
	if (info->read_count > 0)
		info->read_count--;
	if (info->read_count == 0)
		sec_nfc_wakelock(false);
#if LOG_OUT
	pr_info("[NFC] %s : remain_read_count : %d \n", __func__, info->read_count);
#endif		
#endif

	mutex_unlock(&info->read_mutex);

	if (copy_to_user(buf, info->buf, ret)) {
		/* dev_err(info->dev, "copy failed to user\n"); */
		pr_err("[NFC] copy failed to user\n");
		ret = -EFAULT;
	}

	goto out;

read_error:
	info->read_irq = SEC_NFC_NONE;
	mutex_unlock(&info->read_mutex);
out:
	mutex_unlock(&info->mutex);
#if LOG_OUT
	pr_info("[NFC] %s : Exit - ret : %d \n", __func__, ret);
#endif
	return ret;
}

static ssize_t sec_nfc_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *ppos)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	int ret = 0;

	/* dev_dbg(info->dev, "%s: info: %p, count %zu\n",
		__func__, info, count);
	*/
#if LOG_OUT
	pr_info("[NFC] %s : Enter - info: %p, count %zu\n", __func__, info, count);
#endif
	mutex_lock(&info->mutex);

	if (info->state == SEC_NFC_ST_OFF) {
		/* dev_err(info->dev, "sec_nfc is not enabled\n"); */
		pr_err("[NFC] sec_nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}

	if (count > info->buflen)
		count = info->buflen;

	if (count < SEC_NFC_MSG_MIN_SIZE || count > SEC_NFC_MSG_MAX_SIZE + 4) {
		/* dev_err(info->dev, "user required wrong size\n"); */
		pr_err("[NFC] %d : user required wrong size -max: %d, size : %d \n",__func__, SEC_NFC_MSG_MAX_SIZE, count);
		ret = -EINVAL;
		goto out;
	}

	if (copy_from_user(info->buf, buf, count)) {
		/* dev_err(info->dev, "copy failed from user\n"); */
		pr_err("[NFC] copy failed from user\n");
		ret = -EFAULT;
		goto out;
	}

	//usleep_range(6000, 10000);
	ret = i2c_master_send(info->i2c_dev, info->buf, count);
	dev_dbg(info->dev, "send size : %d\n", ret);
#if LOG_OUT
	pr_info("[NFC] %s : I2C SEND SIZE : %d\n", __func__, ret);
#endif	

	if (ret == -EREMOTEIO) {
// patch for download failed
		//ret = sec_nfc_reset(info);
// patch for download failed
		goto out;
	}

		if (ret != count) {
		/* dev_err(info->dev, "send failed: return: %d count: %d\n",
			ret, count);
		*/
		pr_err("[NFC] %s : SEND FAILED : ret : %d,  expected : %d\n",
				__func__, ret, count);
		sec_nfc_get_state();
		ret = -EREMOTEIO;
	}

out:
	mutex_unlock(&info->mutex);
#if LOG_OUT
	pr_info("[NFC] %s : Exit - ret : %d\n", __func__, ret);
#endif
	return ret;
}
#endif

#ifdef CONFIG_SEC_NFC_I2C_SB
static unsigned int sec_nfc_poll(struct file *file, poll_table *wait)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	enum sec_nfc_irq irq;

	int ret = 0;

	dev_dbg(info->dev, "%s: info: %p\n", __func__, info);
#if LOG_OUT
   	pr_info("[NFC] %s : Enter - info : %p\n", __func__, info);
#endif	
	mutex_lock(&info->mutex);

	if (info->state == SEC_NFC_ST_OFF) {
		dev_err(info->dev, "sec_nfc is not enabled\n");
		ret = -ENODEV;
		goto out;
	}
#if LOG_OUT
    pr_info("[NFC] %s : Start POLL WAIT FOR...\n", __func__);
#endif	
	poll_wait(file, &info->read_wait, wait);
#if LOG_OUT
    pr_info("[NFC] %s : Start POLL WAKE UP...\n", __func__);
#endif	


	mutex_lock(&info->read_mutex);
	irq = info->read_irq;
	if (irq == SEC_NFC_INT)
		ret = (POLLIN | POLLRDNORM);
	mutex_unlock(&info->read_mutex);

out:
	mutex_unlock(&info->mutex);
#if LOG_OUT
    pr_info("[NFC] %s : Exit - ret : %d\n", __func__, ret);
#endif	
	return ret;
}
#endif

static long sec_nfc_ioctl(struct file *file, unsigned int cmd,
							unsigned long arg)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	unsigned int mode = (unsigned int)arg;
	int ret = 0;

	dev_dbg(info->dev, "%s: info: %p, cmd: 0x%x\n",
			__func__, info, cmd);

	mutex_lock(&info->mutex);

	switch (cmd) {
	case SEC_NFC_SET_MODE:
		dev_dbg(info->dev, "%s: SEC_NFC_SET_MODE\n", __func__);

		if (info->state == mode)
			break;

		if (mode >= SEC_NFC_ST_COUNT) {
			dev_err(info->dev, "wrong state (%d)\n", mode);
			ret = -EFAULT;
			break;
		}

		ret = sec_nfc_set_state(info, mode);
		if (ret < 0) {
			dev_err(info->dev, "enable failed\n");
			break;
		}

		break;

	default:
		dev_err(info->dev, "Unknow ioctl 0x%x\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	mutex_unlock(&info->mutex);

	return ret;
}


static void nfc_power_onoff(bool onoff)
{
        struct regulator *nfc_regulator = NULL;
        int ret=0;

	if (onoff) {
                nfc_regulator = regulator_get(NULL, "gpldo3_uc");
                if (IS_ERR(nfc_regulator)){
                    printk(KERN_ERR "%s : Can't get regulator instance \n", __func__);
                } else {
                    ret = regulator_set_voltage(nfc_regulator,3300000,3300000);
                    printk(KERN_INFO "%s : regulator_set_voltage : %d\n", __func__, ret);
                    ret = regulator_enable(nfc_regulator);
                    printk(KERN_INFO "%s : regulator_enable : %d\n", __func__, ret);
                    regulator_put(nfc_regulator);
                    mdelay(5);
                }
	} else {
			nfc_regulator = regulator_get(NULL, "gpldo3_uc");
			ret = regulator_disable(nfc_regulator);
			printk(KERN_INFO "%s : regulator_disable : %d\n",__func__, ret);
			regulator_put(nfc_regulator);
	}
}
static int sec_nfc_open(struct inode *inode, struct file *file)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);
	int ret = 0;

	dev_dbg(info->dev, "%s: info : %p" , __func__, info);
#if LOG_OUT
	pr_info("[NFC] %s : Enter  - info = %p\n", __func__, info);
#endif	

	mutex_lock(&info->mutex);
	if (info->state != SEC_NFC_ST_OFF) {
		dev_err(info->dev, "sec_nfc is busy\n");
		pr_info("sec_nfc is busy\n");
		sec_nfc_get_state();
		ret = -EBUSY;
		goto out;
	}

	ret = sec_nfc_set_state(info, SEC_NFC_ST_NORM);
out:
	mutex_unlock(&info->mutex);
#if LOG_OUT
	pr_info("[NFC] %s : Exit - ret = %d\n", __func__, ret);
#endif	
	return ret;
}

static int sec_nfc_close(struct inode *inode, struct file *file)
{
	struct sec_nfc_info *info = container_of(file->private_data,
						struct sec_nfc_info, miscdev);

	dev_dbg(info->dev, "%s: info : %p" , __func__, info);
#if LOG_OUT
	pr_info("[NFC] %s : Enter  - info = %p\n", __func__, info);
#endif	
	mutex_lock(&info->mutex);
	sec_nfc_set_state(info, SEC_NFC_ST_OFF);
	mutex_unlock(&info->mutex);
#if LOG_OUT
	pr_info("[NFC] %s : Exit\n", __func__);
#endif	
	return 0;
}

static const struct file_operations sec_nfc_fops = {
	.owner		= THIS_MODULE,
#ifdef CONFIG_SEC_NFC_I2C_SB
	.read		= sec_nfc_read,
	.write		= sec_nfc_write,
	.poll		= sec_nfc_poll,
#endif
	.open		= sec_nfc_open,
	.release	= sec_nfc_close,
	.unlocked_ioctl	= sec_nfc_ioctl,
};

#ifdef CONFIG_PM
static int sec_nfc_suspend(struct device *dev)
{
#ifdef CONFIG_SEC_NFC_I2C_SB
	struct i2c_client *client = to_i2c_client(dev);
	struct sec_nfc_info *info = i2c_get_clientdata(client);
#else
	struct platform_device *pdev = to_platform_device(dev);
	struct sec_nfc_info *info = platform_get_drvdata(pdev);
#endif
	struct sec_nfc_platform_data *pdata = dev->platform_data;

	int ret = 0;

	mutex_lock(&info->mutex);

	if (info->state == SEC_NFC_ST_FIRM)
		ret = -EPERM;

	mutex_unlock(&info->mutex);

	pdata->cfg_gpio();
	return ret;
}

static int sec_nfc_resume(struct device *dev)
{
	struct sec_nfc_platform_data *pdata = dev->platform_data;
	pdata->cfg_gpio();
	return 0;
}

static SIMPLE_DEV_PM_OPS(sec_nfc_pm_ops, sec_nfc_suspend, sec_nfc_resume);
#endif

#ifdef CONFIG_SEC_NFC_I2C_SB
static int sec_nfc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
#else
static int sec_nfc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
#endif
	struct sec_nfc_info *info;
	struct sec_nfc_platform_data *pdata = dev->platform_data;
	int ret = 0;

	pr_info("[NFC] %s : Enter\n", __func__);

	if (!pdata) {
		dev_err(dev, "No platform data\n");
		ret = -ENOMEM;
		goto err_pdata;
	}

	info = kzalloc(sizeof(struct sec_nfc_info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "failed to allocate memory for sec_nfc_info\n");
		ret = -ENOMEM;
		goto err_info_alloc;
	}
	info->dev = dev;
	info->pdata = pdata;
	info->state = SEC_NFC_ST_OFF;

	mutex_init(&info->mutex);
	dev_set_drvdata(dev, info);

#ifdef CONFIG_SEC_NFC_I2C_SB
	info->buflen = SEC_NFC_MAX_BUFFER_SIZE;
	info->buf = kzalloc(SEC_NFC_MAX_BUFFER_SIZE, GFP_KERNEL);
	if (!info->buf) {
		dev_err(dev,
			"failed to allocate memory for sec_nfc_info->buf\n");
		ret = -ENOMEM;
		goto err_buf_alloc;
	}

	/* pdata->cfg_gpio(); */

	ret = gpio_request(pdata->irq, "nfc_int");
	if (ret) {
		dev_err(dev, "[NFC] GPIO request is failed to register IRQ\n");
		goto err_irq_req;
	}
	gpio_direction_input(pdata->irq);

	info->i2c_dev = client;
	info->read_irq = SEC_NFC_NONE;
#ifdef CONFIG_HAS_WAKELOCK
	info->read_count = 0;
	sec_nfc_wakelock_init();
#endif
	mutex_init(&info->read_mutex);
	init_waitqueue_head(&info->read_wait);
	i2c_set_clientdata(client, info);

	dev_info(dev, "%s : requesting IRQ %d\n", __func__,
		 client->irq);

//	ret = request_threaded_irq(pdata->irq, NULL, sec_nfc_irq_thread_fn,
	ret = request_threaded_irq(client->irq, NULL, sec_nfc_irq_thread_fn,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT, SEC_NFC_DRIVER_NAME,
			info);
	if (ret < 0) {
		dev_err(dev, "failed to register IRQ handler\n");
		goto err_irq_req;
	}
#endif

	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	info->miscdev.name = SEC_NFC_DRIVER_NAME;
	info->miscdev.fops = &sec_nfc_fops;
	info->miscdev.parent = dev;
	ret = misc_register(&info->miscdev);
	if (ret < 0) {
		dev_err(dev, "failed to register Device\n");
		goto err_dev_reg;
	}

	ret = gpio_request(pdata->ven, "nfc_ven");
	if (ret) {
		dev_err(dev, "failed to get gpio ven\n");
		goto err_gpio_ven;
	}
	gpio_direction_output(pdata->ven, 0);

	ret = gpio_request(pdata->firm, "nfc_firm");
	if (ret) {
		dev_err(dev, "failed to get gpio firm\n");
		goto err_gpio_firm;
	}
	gpio_direction_output(pdata->firm, 0);

	gpio_set_value(pdata->ven, 0);
	//gpio_set_value(pdata->irq, 1);

	nfc_power_onoff(1);
#if 0
	if (info->pdata->power_onoff)
		info->pdata->power_onoff(1);
	else
		pr_info("%s : no power control function\n", __func__);
#endif
	dev_dbg(dev, "%s: info: %p, pdata %p\n", __func__, info, pdata);


	pr_info("[NFC] %s : Exit\n", __func__);

	return 0;

err_gpio_firm:
	gpio_free(pdata->firm);
err_gpio_ven:
	gpio_free(pdata->ven);
err_dev_reg:
#ifdef CONFIG_SEC_NFC_I2C_SB
err_irq_req:
	gpio_free(pdata->irq);
err_buf_alloc:
#endif
err_info_alloc:
	kfree(info);
err_pdata:
	return ret;
}

#ifdef CONFIG_SEC_NFC_I2C_SB
static int sec_nfc_remove(struct i2c_client *client)
{
	struct sec_nfc_info *info = i2c_get_clientdata(client);
	struct sec_nfc_platform_data *pdata = client->dev.platform_data;
#else
static int sec_nfc_remove(struct platform_device *pdev)
{
	struct sec_nfc_info *info = dev_get_drvdata(&pdev->dev);
	struct sec_nfc_platform_data *pdata = pdev->dev.platform_data;
#endif

	dev_dbg(info->dev, "%s\n", __func__);

	misc_deregister(&info->miscdev);

	if (info->state != SEC_NFC_ST_OFF) {
		gpio_set_value(pdata->firm, 0);
		gpio_set_value(pdata->ven, 0);
	}

	gpio_free(pdata->firm);
	gpio_free(pdata->ven);

#ifdef CONFIG_SEC_NFC_I2C_SB
	free_irq(pdata->irq, info);
#endif

	kfree(info);

	return 0;
}

#ifdef CONFIG_SEC_NFC_I2C_SB
static struct i2c_device_id sec_nfc_id_table[] = {
#else	/* CONFIG_SEC_NFC_I2C */
static struct platform_device_id sec_nfc_id_table[] = {
#endif
	{ SEC_NFC_DRIVER_NAME, 0 },
	{ }
};

#ifdef CONFIG_SEC_NFC_I2C_SB
MODULE_DEVICE_TABLE(i2c, sec_nfc_id_table);
static struct i2c_driver sec_nfc_driver = {
#else
MODULE_DEVICE_TABLE(platform, sec_nfc_id_table);
static struct platform_driver sec_nfc_driver = {
#endif
	.probe = sec_nfc_probe,
	.id_table = sec_nfc_id_table,
	.remove = sec_nfc_remove,
	.driver = {
		.name = SEC_NFC_DRIVER_NAME,
#ifdef CONFIG_PM
		.pm = &sec_nfc_pm_ops,
#endif
	},
};

static int __init sec_nfc_init(void)
{
#ifdef CONFIG_SEC_NFC_I2C_SB
	return i2c_add_driver(&sec_nfc_driver);
#else
	return platform_driver_register(&sec_nfc_driver);
#endif
}

static void __exit sec_nfc_exit(void)
{
#ifdef CONFIG_SEC_NFC_I2C_SB
	i2c_del_driver(&sec_nfc_driver);
#else
	platform_driver_unregister(&sec_nfc_driver);
#endif
}

module_init(sec_nfc_init);
module_exit(sec_nfc_exit);

MODULE_DESCRIPTION("Samsung sec_nfc driver");
MODULE_LICENSE("GPL");
