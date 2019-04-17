/*
 * Copyright 2004 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef MXC_TEST_H

#define MXC_TEST_H

//#ifndef CONFIG_ARCH_MX3
#if 0
#define HAC_TEST_DEBUG
#endif

#ifdef HAC_TEST_DEBUG
#define HAC_DEBUG(fmt, args...) printk(fmt,## args)
#else
#define HAC_DEBUG(fmt, args...)
#endif
//#endif /* CONFIG_ARCH_MX3 */

#if 0
#define RTIC_TEST_DEBUG
#endif

#ifdef RTIC_TEST_DEBUG
#define RTIC_DEBUG(fmt, args...) printk(fmt,## args)
#else
#define RTIC_DEBUG(fmt, args...)
#endif

enum TEST_MODULE_MAJOR_NUM {
	MXC_TEST_MODULE_MAJOR = 300,
	MXC_WDOG_TM_MAJOR,
};

/*
 * Test IOCTLS
 */
enum MXCTEST_IOCTL {
	MXCTEST_I2C_READ = 0x0001,
	MXCTEST_I2C_WRITE,
	MXCTEST_I2C_CSICLKENB,
	MXCTEST_I2C_CSICLKDIS,
	MXCTEST_WDOG,
	MXCTEST_PM_INTSCALE,
	MXCTEST_PM_PLLSCALE,
	MXCTEST_PM_INT_OR_PLL,
	MXCTEST_PM_CKOH_SEL,
	MXCTEST_PM_LOWPOWER,
	MXCTEST_PM_PMIC_HIGH,
	MXCTEST_PM_PMIC_LOW,
	MXCTEST_PM_PMIC_HILO,
	MXCTEST_GET_PLATFORM,
	MXCTEST_HAC_START_HASH,
	MXCTEST_HAC_STATUS,
	MXCTEST_RTIC_ALGOSELECT,
	MXCTEST_RTIC_CONFIGURE_MEMORY,
	MXCTEST_RTIC_ONETIME_TEST,
	MXCTEST_RTIC_CONFIGURE_RUNTIME_MEMORY_AFTER_WARM_BOOT,
	MXCTEST_RTIC_CONFIGURE_RUNTIME_MEMORY,
	MXCTEST_RTIC_RUNTIME_TEST,
	MXCTEST_RTIC_RUNTIME_ERROR,
	MXCTEST_RTIC_STATUS
};

/*
 * Test Platforms
 */
enum MXCTEST_PLATFORM {
	mxc91221 = 1,
	mxc91231,
	mxc91321,
	mxc91311,
	mxc92323,
	mxc91131,
	mx21,
	mx27,
	mx31,
	mx32
};

/*
 * Data structure passed in to the MXCTEST_I2C_READ and MXCTEST_I2C_WRITE
 * ioctl calls
 */
typedef struct {
	/*
	 * The I2C Bus number that the device is connected to
	 */
	int bus;
	/*
	 * The slave address of the I2C device
	 */
	unsigned int slave_addr;
	/*
	 * The address of the register we want to access
	 */
	char *reg;
	/*
	 * Number of bytes in the register address
	 */
	int reg_size;
	/*
	 * Data buffer to transfer with the register
	 */
	char *buf;
	/*
	 * Number of data bytes to transfer
	 */
	int buf_size;
} mxc_i2c_test;

/*
 * Data structure which holds the required ARM frequency value. This is
 * passed in to the MXCTEST_PM_INTSCALE and MXCTEST_PM_PLLSCALE
 * ioctl calls
 */
typedef struct {
	int armfreq;
	int ahbfreq;
	int ipfreq;
	int lpmd;
	int ckoh;
} mxc_pm_test;

/*
 * Data structure which holds the required ARM frequency value. This is
 * passed in to the MXCTEST_PM_INTSCALE and MXCTEST_PM_PLLSCALE
 * ioctl calls
 */

#endif				/* MXC_TEST_H */
