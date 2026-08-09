// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2012 Henrik Nordstrom <henrik@henriknordstrom.net>
 *
 * Based on earlier arch/arm/cpu/armv7/sunxi/gpio.c:
 *
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 */

#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <malloc.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <dt-bindings/gpio/gpio.h>
#include <sunxi_gpio.h>

/*
 * =======================================================================
 * Low level GPIO/pin controller access functions, to be shared by non-DM
 * SPL code and the DM pinctrl/GPIO drivers.
 * The functions ending in "bank" take a base pointer to a GPIO bank, and
 * the pin offset is relative to that bank.
 * The functions without "bank" in their name take a linear GPIO number,
 * covering all ports, and starting at 0 for PortA.
 * =======================================================================
 */

#define GPIO_BANK(pin)		((pin) >> 5)
#define GPIO_NUM(pin)		((pin) & 0x1f)

#define GPIO_CFG_REG_OFFSET	0x00
#define GPIO_CFG_INDEX(pin)	(((pin) & 0x1f) >> 3)
#define GPIO_CFG_OFFSET(pin)	((((pin) & 0x1f) & 0x7) << 2)

#define GPIO_DAT_REG_OFFSET	0x10

#define GPIO_DRV_REG_OFFSET	0x14

/*		Newer SoCs use a slightly different register layout */
#ifdef CONFIG_SUNXI_NEW_PINCTRL
/* pin drive strength: 4 bits per pin */
#define GPIO_DRV_INDEX(pin)	((pin) / 8)
#define GPIO_DRV_OFFSET(pin)	(((pin) % 8) * 4)

#define GPIO_PULL_REG_OFFSET	0x24

#else /* older generation pin controllers */
/* pin drive strength: 2 bits per pin */
#define GPIO_DRV_INDEX(pin)	((pin) / 16)
#define GPIO_DRV_OFFSET(pin)	(((pin) % 16) * 2)

#define GPIO_PULL_REG_OFFSET	0x1c
#endif

#define GPIO_PULL_INDEX(pin)	(((pin) & 0x1f) >> 4)
#define GPIO_PULL_OFFSET(pin)	((((pin) & 0x1f) & 0xf) << 1)

#define A733_MAIN_BANK_SIZE	0x80
#define A733_MAIN_DAT_OFFSET	0x10
#define A733_MAIN_DRV_OFFSET	0x20
#define A733_MAIN_PULL_OFFSET	0x30

#define A733_R_BANK_SIZE		0x30
#define A733_R_DAT_OFFSET	0x10
#define A733_R_DRV_OFFSET	0x14
#define A733_R_PULL_OFFSET	0x24

static enum sunxi_gpio_layout sunxi_gpio_bank_layout(int bank)
{
	if (!IS_ENABLED(CONFIG_MACH_SUN60I_A733))
		return SUNXI_GPIO_LAYOUT_DEFAULT;

	return bank < SUNXI_GPIO_L ? SUNXI_GPIO_LAYOUT_A733_MAIN :
		SUNXI_GPIO_LAYOUT_A733_R;
}

static void *BANK_TO_GPIO(int bank)
{
	void *pio_base;

	if (IS_ENABLED(CONFIG_MACH_SUN60I_A733)) {
		if (bank < SUNXI_GPIO_L)
			return (void *)(uintptr_t)(SUNXI_PIO_BASE +
				(bank + 1) * A733_MAIN_BANK_SIZE);

		return (void *)(uintptr_t)(SUNXI_R_PIO_BASE +
			(bank - SUNXI_GPIO_L) * A733_R_BANK_SIZE);
	}

	if (bank < SUNXI_GPIO_L) {
		pio_base = (void *)(uintptr_t)SUNXI_PIO_BASE;
	} else {
		pio_base = (void *)(uintptr_t)SUNXI_R_PIO_BASE;
		bank -= SUNXI_GPIO_L;
	}

	return pio_base + bank * SUNXI_PINCTRL_BANK_SIZE;
}

static u32 sunxi_gpio_data_offset(enum sunxi_gpio_layout layout)
{
	if (layout == SUNXI_GPIO_LAYOUT_A733_MAIN)
		return A733_MAIN_DAT_OFFSET;
	if (layout == SUNXI_GPIO_LAYOUT_A733_R)
		return A733_R_DAT_OFFSET;

	return GPIO_DAT_REG_OFFSET;
}

static u32 sunxi_gpio_drv_reg_offset(enum sunxi_gpio_layout layout,
				     u32 pin)
{
	if (layout == SUNXI_GPIO_LAYOUT_A733_MAIN)
		return A733_MAIN_DRV_OFFSET + (pin / 8) * 4;
	if (layout == SUNXI_GPIO_LAYOUT_A733_R)
		return A733_R_DRV_OFFSET + (pin / 8) * 4;

	return GPIO_DRV_REG_OFFSET + GPIO_DRV_INDEX(pin) * 4;
}

static u32 sunxi_gpio_drv_bit_offset(enum sunxi_gpio_layout layout, u32 pin)
{
	if (layout == SUNXI_GPIO_LAYOUT_A733_MAIN ||
	    layout == SUNXI_GPIO_LAYOUT_A733_R)
		return (pin % 8) * 4;

	return GPIO_DRV_OFFSET(pin);
}

static u32 sunxi_gpio_pull_reg_offset(enum sunxi_gpio_layout layout, u32 pin)
{
	u32 offset = GPIO_PULL_REG_OFFSET;

	if (layout == SUNXI_GPIO_LAYOUT_A733_MAIN)
		offset = A733_MAIN_PULL_OFFSET;
	else if (layout == SUNXI_GPIO_LAYOUT_A733_R)
		offset = A733_R_PULL_OFFSET;

	return offset + GPIO_PULL_INDEX(pin) * 4;
}

void sunxi_gpio_set_cfgbank(void *bank_base, int pin_offset, u32 val)
{
	u32 index = GPIO_CFG_INDEX(pin_offset);
	u32 offset = GPIO_CFG_OFFSET(pin_offset);

	/* bank_base always points at the first configuration register. */
	clrsetbits_le32(bank_base + GPIO_CFG_REG_OFFSET + index * 4,
			0xfU << offset, val << offset);
}

void sunxi_gpio_set_cfgpin(u32 pin, u32 val)
{
	u32 bank = GPIO_BANK(pin);
	void *pio = BANK_TO_GPIO(bank);

	sunxi_gpio_set_cfgbank(pio, GPIO_NUM(pin), val);
}

int sunxi_gpio_get_cfgbank(void *bank_base, int pin_offset)
{
	u32 index = GPIO_CFG_INDEX(pin_offset);
	u32 offset = GPIO_CFG_OFFSET(pin_offset);
	u32 cfg;

	cfg = readl(bank_base + GPIO_CFG_REG_OFFSET + index * 4);
	cfg >>= offset;

	return cfg & 0xf;
}

int sunxi_gpio_get_cfgpin(u32 pin)
{
	u32 bank = GPIO_BANK(pin);
	void *bank_base = BANK_TO_GPIO(bank);

	return sunxi_gpio_get_cfgbank(bank_base, GPIO_NUM(pin));
}

static void sunxi_gpio_set_value_bank(void *bank_base, int pin, bool set,
				      enum sunxi_gpio_layout layout)
{
	u32 mask = 1U << pin;

	clrsetbits_le32(bank_base + sunxi_gpio_data_offset(layout),
			set ? 0 : mask, set ? mask : 0);
}

static int sunxi_gpio_get_value_bank(void *bank_base, int pin,
				     enum sunxi_gpio_layout layout)
{
	return !!(readl(bank_base + sunxi_gpio_data_offset(layout)) & (1U << pin));
}

void sunxi_gpio_set_drv(u32 pin, u32 val)
{
	u32 bank = GPIO_BANK(pin);
	void *bank_base = BANK_TO_GPIO(bank);
	enum sunxi_gpio_layout layout = sunxi_gpio_bank_layout(bank);

	sunxi_gpio_set_drv_bank_layout(bank_base, GPIO_NUM(pin), val, layout);
}

void sunxi_gpio_set_drv_bank_layout(void *bank_base, u32 pin_offset,
				    u32 val, enum sunxi_gpio_layout layout)
{
	u32 reg_offset = sunxi_gpio_drv_reg_offset(layout, pin_offset);
	u32 bit_offset = sunxi_gpio_drv_bit_offset(layout, pin_offset);

	clrsetbits_le32(bank_base + reg_offset, 0x3U << bit_offset,
			val << bit_offset);
}

void sunxi_gpio_set_drv_bank(void *bank_base, u32 pin_offset, u32 val)
{
	sunxi_gpio_set_drv_bank_layout(bank_base, pin_offset, val,
				       SUNXI_GPIO_LAYOUT_DEFAULT);
}

void sunxi_gpio_set_pull(u32 pin, u32 val)
{
	u32 bank = GPIO_BANK(pin);
	void *bank_base = BANK_TO_GPIO(bank);
	enum sunxi_gpio_layout layout = sunxi_gpio_bank_layout(bank);

	sunxi_gpio_set_pull_bank_layout(bank_base, GPIO_NUM(pin), val, layout);
}

void sunxi_gpio_set_pull_bank_layout(void *bank_base, int pin_offset,
				     u32 val, enum sunxi_gpio_layout layout)
{
	u32 offset = GPIO_PULL_OFFSET(pin_offset);
	u32 reg_offset = sunxi_gpio_pull_reg_offset(layout, pin_offset);

	clrsetbits_le32(bank_base + reg_offset,
			0x3U << offset, val << offset);
}

void sunxi_gpio_set_pull_bank(void *bank_base, int pin_offset, u32 val)
{
	sunxi_gpio_set_pull_bank_layout(bank_base, pin_offset, val,
					SUNXI_GPIO_LAYOUT_DEFAULT);
}

/* =========== Non-DM code, used by the SPL. ============ */

#if !CONFIG_IS_ENABLED(DM_GPIO)
static void sunxi_gpio_set_value(u32 pin, bool set)
{
	u32 bank = GPIO_BANK(pin);
	void *pio = BANK_TO_GPIO(bank);

	sunxi_gpio_set_value_bank(pio, GPIO_NUM(pin), set,
				  sunxi_gpio_bank_layout(bank));
}

static int sunxi_gpio_get_value(u32 pin)
{
	u32 bank = GPIO_BANK(pin);
	void *pio = BANK_TO_GPIO(bank);

	return sunxi_gpio_get_value_bank(pio, GPIO_NUM(pin),
					 sunxi_gpio_bank_layout(bank));
}

int gpio_request(unsigned gpio, const char *label)
{
	return 0;
}

int gpio_free(unsigned gpio)
{
	return 0;
}

int gpio_direction_input(unsigned gpio)
{
	sunxi_gpio_set_cfgpin(gpio, SUNXI_GPIO_INPUT);

	return 0;
}

int gpio_direction_output(unsigned gpio, int value)
{
	sunxi_gpio_set_cfgpin(gpio, SUNXI_GPIO_OUTPUT);
	sunxi_gpio_set_value(gpio, value);

	return 0;
}

int gpio_get_value(unsigned gpio)
{
	return sunxi_gpio_get_value(gpio);
}

int gpio_set_value(unsigned gpio, int value)
{
	sunxi_gpio_set_value(gpio, value);

	return 0;
}

int sunxi_name_to_gpio(const char *name)
{
	int group = 0;
	int groupsize = 9 * 32;
	long pin;
	char *eptr;

	if (*name == 'P' || *name == 'p')
		name++;
	if (*name >= 'A') {
		group = *name - (*name > 'a' ? 'a' : 'A');
		groupsize = 32;
		name++;
	}

	pin = simple_strtol(name, &eptr, 10);
	if (!*name || *eptr)
		return -1;
	if (pin < 0 || pin > groupsize || group >= 9)
		return -1;
	return group * 32 + pin;
}
#endif /* !DM_GPIO */

/* =========== DM code, used by U-Boot proper. ============ */

#if CONFIG_IS_ENABLED(DM_GPIO)
/* TODO(sjg@chromium.org): Remove this function and use device tree */
int sunxi_name_to_gpio(const char *name)
{
	unsigned int gpio;
	int ret = gpio_lookup_name(name, NULL, NULL, &gpio);

	return ret ? ret : gpio;
}

static int sunxi_gpio_get_value(struct udevice *dev, unsigned offset)
{
	struct sunxi_gpio_plat *plat = dev_get_plat(dev);

	return sunxi_gpio_get_value_bank(plat->regs, offset, plat->layout);
}

static int sunxi_gpio_get_function(struct udevice *dev, unsigned offset)
{
	struct sunxi_gpio_plat *plat = dev_get_plat(dev);
	int func;

	func = sunxi_gpio_get_cfgbank(plat->regs, offset);
	if (func == SUNXI_GPIO_OUTPUT)
		return GPIOF_OUTPUT;
	else if (func == SUNXI_GPIO_INPUT)
		return GPIOF_INPUT;
	else
		return GPIOF_FUNC;
}

static int sunxi_gpio_xlate(struct udevice *dev, struct gpio_desc *desc,
			    struct ofnode_phandle_args *args)
{
	int ret;

	ret = device_get_child(dev, args->args[0], &desc->dev);
	if (ret)
		return ret;
	desc->offset = args->args[1];
	desc->flags = gpio_flags_xlate(args->args[2]);

	return 0;
}

static int sunxi_gpio_set_flags(struct udevice *dev, unsigned int offset,
				ulong flags)
{
	struct sunxi_gpio_plat *plat = dev_get_plat(dev);

	if (flags & GPIOD_IS_OUT) {
		u32 value = !!(flags & GPIOD_IS_OUT_ACTIVE);

		sunxi_gpio_set_value_bank(plat->regs, offset, value, plat->layout);
		sunxi_gpio_set_cfgbank(plat->regs, offset, SUNXI_GPIO_OUTPUT);
	} else if (flags & GPIOD_IS_IN) {
		u32 pull = 0;

		if (flags & GPIOD_PULL_UP)
			pull = 1;
		else if (flags & GPIOD_PULL_DOWN)
			pull = 2;
		sunxi_gpio_set_pull_bank_layout(plat->regs, offset, pull,
						plat->layout);
		sunxi_gpio_set_cfgbank(plat->regs, offset, SUNXI_GPIO_INPUT);
	}

	return 0;
}

static const struct dm_gpio_ops gpio_sunxi_ops = {
	.get_value		= sunxi_gpio_get_value,
	.get_function		= sunxi_gpio_get_function,
	.xlate			= sunxi_gpio_xlate,
	.set_flags		= sunxi_gpio_set_flags,
};

static int gpio_sunxi_probe(struct udevice *dev)
{
	struct sunxi_gpio_plat *plat = dev_get_plat(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	/* Tell the uclass how many GPIOs we have */
	if (plat) {
		uc_priv->gpio_count = SUNXI_GPIOS_PER_BANK;
		uc_priv->bank_name = plat->bank_name;
	}

	return 0;
}

U_BOOT_DRIVER(gpio_sunxi) = {
	.name	= "gpio_sunxi",
	.id	= UCLASS_GPIO,
	.probe	= gpio_sunxi_probe,
	.ops	= &gpio_sunxi_ops,
};
#endif /* DM_GPIO */
