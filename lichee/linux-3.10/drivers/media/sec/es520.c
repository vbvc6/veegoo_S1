#include <linux/module.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/kernel.h> /* pr_err() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <asm/system.h> /* cli(), *_flags */
#include <asm/uaccess.h> /* copy_*_user */
#include <linux/device.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/pinconf-sunxi.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/clk/sunxi.h>
#include <linux/err.h>
//#include <mach/sys_config.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/power/scenelock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/reboot.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <linux/sys_config.h>
#include "include/sunxi_sec.h"

static unsigned int major;
static struct class * ecy_class;
static struct device *ecy_dev;
static struct regulator *regu_dldo1 = NULL;
static void sec_init_events(struct work_struct *work);
static struct workqueue_struct *init_sec_queue;
static DECLARE_WORK(sec_init_work, sec_init_events);
int rst = 0;
EXPORT_SYMBOL(rst);

// Global Variables
extern void es_delay_us(unsigned int nus);
extern void es_delay_ms(unsigned int nus);
int sec_scl = 0;
int sec_sda = 0;
//Define Delay Function
void es_delay_us(unsigned int nus)
{
	udelay(nus);
}

void es_delay_ms(unsigned int nus)
{
	mdelay(nus);
}

void _gpio_set_data(int gpio_num, unsigned int is_on)
{
	gpio_direction_output(gpio_num, is_on);
}

unsigned int _gpio_get_data(int gpio_num)
{
	gpio_direction_input(gpio_num);
	return gpio_get_value(gpio_num);
}

static ssize_t sec_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	unsigned char ret = 0;
#if 0
	ret = cw0881_demo();
	if (ret == 0) {
		return sprintf(buf, "%d\n", ret);
	}
	ret = MTZ_test(0xcc,0x33);
	return sprintf(buf, "%d\n", ret);
#else
	return sprintf(buf, "%d\n", rst);
#endif
}
static void sec_init_events(struct work_struct *work)
{
	int ret = 0;
	regu_dldo1= regulator_get(NULL, "axp221s_dcdc1");
	if (IS_ERR(regu_dldo1)) {
		pr_err("%s: l:%d some error happen, fail to get regulator \n", __func__, __LINE__);
	}
	regulator_set_voltage(regu_dldo1, 3300000, 3300000);
	ret = regulator_enable(regu_dldo1);
	usleep_range(50000,50000);
#if 0
	gpio_request(15, "PA15");
	gpio_direction_output(15, 0);
	msleep(20);
	gpio_request(16, "PA16");
	gpio_direction_output(16, 0);
	gpio_request(17, "PA17");
	gpio_direction_output(17, 0);
	sec_scl = 16;
	sec_sda = 17;
#endif
	ret = MTZ_test(0xcc,0x33);
	if (ret == 0) {
		pr_err("connect fail: %s,l:%d,ret:%d\n", __func__, __LINE__, ret);
		machine_restart(NULL);
	}
	ret = cw0881_demo();
	if (ret == 0) {
		machine_restart(NULL);
	}

	/*
	 * ret: 0 failed
	 * 		1 ok
	 */
	if (ret == 1) {
		rst = 1;
		pr_err("ecy module init success\n");
	} else {
		rst = 0;
		pr_err("ecy module init fail\n");
	}
}

static DEVICE_ATTR(sec_input, 0644, sec_show, NULL);

static struct attribute *sec_attrs[] = {
	&dev_attr_sec_input.attr,
	NULL
};

static const struct attribute_group sec_attr_group = {
	.attrs = sec_attrs,
};

static int ecy_open(struct inode *inode, struct file *file)
{
	return 0 ;
}

static int ecy_close(struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations ecy_fops = {
    .owner  	=   THIS_MODULE,
    .open   	=   ecy_open,     
	.release	=	ecy_close,
};
#define	DRV_NAME	"sunxi-sec"

struct sec_gpio_{
	u32 gpio;
	bool cfg;
};
struct sec_gpio_ sec0_gpio;
struct sec_gpio_ sec1_gpio;
struct sec_gpio_ sec2_gpio;
static int  sunxi_sec_dev_probe(struct platform_device *pdev)
{
	unsigned char ret = 0;
	struct gpio_config config;
	struct device_node *node = pdev->dev.of_node;
	pr_err("===hx--%s,l:%d=====\n", __func__, __LINE__);

	major = register_chrdev(0, "ecy", &ecy_fops);
	if (major == 0) {
	//	return -1;
	}
	ecy_class = class_create(THIS_MODULE, "ecy");
	ecy_dev = device_create(ecy_class, NULL, MKDEV(major, 0), NULL, "ecy");
	if (IS_ERR(ecy_dev)) {
		pr_err("ecy_dev error\n");
	//	return PTR_ERR(ecy_dev);
	}

	init_sec_queue = create_singlethread_workqueue("sec_init");
	if (init_sec_queue == NULL) {
		pr_err("[sec] try to create workqueue for sec failed!\n");
	//	return -ENOMEM;
	}
	/* register sysfs hooks */
	ret = sysfs_create_group(&ecy_dev->kobj, &sec_attr_group);

	sec0_gpio.gpio = of_get_named_gpio(node, "sec0", 0);
	if (!gpio_is_valid(sec0_gpio.gpio)) {
		pr_err("failed to get gpio-sec0 gpio from dts,sec0_gpio:%d\n", sec0_gpio.gpio);
		sec0_gpio.cfg = 0;
	} else {
		pr_err("%s,l:%d,sec0_gpio.gpio:%d\n", __FUNCTION__, __LINE__, sec0_gpio.gpio);
		ret = devm_gpio_request(&pdev->dev, sec0_gpio.gpio, "sec0");
		if (ret) {
			sec0_gpio.cfg = 0;
			pr_err("failed to request gpio-sec0 gpio\n");
		} else {
			sec0_gpio.cfg = 1;
			gpio_direction_output(sec0_gpio.gpio, 0);
		}
	}
	sec1_gpio.gpio = of_get_named_gpio(node, "sec1", 0);
	if (!gpio_is_valid(sec1_gpio.gpio)) {
		pr_err("failed to get gpio-sec1 gpio from dts,sec1_gpio:%d\n", sec1_gpio.gpio);
		sec1_gpio.cfg = 0;
	} else {
		pr_err("%s,l:%d,sec1_gpio.gpio:%d\n", __FUNCTION__, __LINE__, sec1_gpio.gpio);
		ret = devm_gpio_request(&pdev->dev, sec1_gpio.gpio, "sec1");
		if (ret) {
			sec1_gpio.cfg = 0;
			pr_err("failed to request gpio-sec1 gpio\n");
		} else {
			sec1_gpio.cfg = 1;
			gpio_direction_output(sec1_gpio.gpio, 0);
		}
	}
	sec2_gpio.gpio = of_get_named_gpio(node, "sec2", 0);
	if (!gpio_is_valid(sec2_gpio.gpio)) {
		pr_err("failed to get gpio-sec gpio from dts,sec2_gpio:%d\n", sec2_gpio.gpio);
		sec2_gpio.cfg = 0;
	} else {
		pr_err("%s,l:%d,sec2_gpio.gpio:%d\n", __FUNCTION__, __LINE__, sec2_gpio.gpio);
		ret = devm_gpio_request(&pdev->dev, sec2_gpio.gpio, "sec2");
		if (ret) {
			sec2_gpio.cfg = 0;
			pr_err("failed to request gpio-sec2 gpio\n");
		} else {
			sec2_gpio.cfg = 1;
			gpio_direction_output(sec2_gpio.gpio, 0);
		}
	}

	sec_scl = sec1_gpio.gpio;
	sec_sda = sec2_gpio.gpio;
	queue_work(init_sec_queue, &sec_init_work);
	pr_err("===hx==init_es520Enc====end=\n");
	return 0;
}

static int __exit sunxi_sec_dev_remove(struct platform_device *pdev)
{
	pr_err(KERN_EMERG"module remove now!\r\n");
	/* disable eldo2 if it exist */
	if (regu_dldo1) {
		regulator_disable(regu_dldo1);
		regu_dldo1 = NULL;
	}

	device_destroy(ecy_class, MKDEV(major, 0));

	class_destroy(ecy_class);
	
	unregister_chrdev(major, "ecy");
	
	pr_err("ecy module exit success\n");
	return 0;
}

static const struct of_device_id sunxi_sec_of_match[] = {
	{ .compatible = "allwinner,sunxi-sec", },
	{},
};

static struct platform_driver sunxi_sec_driver = {
	.probe = sunxi_sec_dev_probe,
	.remove = __exit_p(sunxi_sec_dev_remove),
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = sunxi_sec_of_match,
	},
};

module_platform_driver(sunxi_sec_driver);
MODULE_AUTHOR("xin huang <huangxin@vyagoo.com>");
MODULE_DESCRIPTION("SUNXI SEC Interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sunxi-sec");
