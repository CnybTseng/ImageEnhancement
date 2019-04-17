/*
 * Copyright 2005-2008 Freescale Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*! @file rng_test_driver.h
 *
 *  This header file provides definitions needed by the RNG test driver.
 *
 */

#ifndef RNG_TEST_DRIVER_H
#define RNG_TEST_DRIVER_H

#ifndef __KERNEL__
#include <inttypes.h>
#include <stdlib.h>
#include <memory.h>
#else
#include "portable_os.h"
#endif

#define RNG_REGISTER_PEEK_POKE

#include "shw_driver.h"
#include "rng_driver.h"

#ifdef __KERNEL__

static os_error_code rng_test_get_random(fsl_shw_uco_t* user_ctx,
                                          unsigned long rng_data);
static os_error_code rng_test_add_entropy(fsl_shw_uco_t* user_ctx,
                                          unsigned long rng_data);
static os_error_code rng_test_read_register(unsigned long rng_data);
static os_error_code rng_test_write_register(unsigned long rng_data);
static os_error_code rng_test_setup_user_driver_interaction(void);
static void rng_test_cleanup(void);

extern int rng_major_node;


#ifndef RNG_TEST_MAJOR_NODE
/** Linux major node value for the device special file (/dev/rng_test) */
#define RNG_TEST_MAJOR_NODE  0
#endif

#endif /* kernel */


#ifndef RNG_TEST_DRIVER_NAME
/** /dev/xxx name for this device */
#define RNG_TEST_DRIVER_NAME "rng_test"
#endif

/* User/Driver interface definitions */

/*
 * Interface definitions between user and driver
 */

/* This is a porting opportunity.  It identifies the 'unique' byte
   value inserted into the IOCTL number.  It really only has to be
   unique within the software used on a given device. */
#ifndef RNG_TEST_DRIVER_IOCTL_IDENTIFIER
#define RNG_TEST_DRIVER_IOCTL_IDENTIFIER 'r'
#endif

/* Define RNG Driver Commands (Argument 2 of ioctl) */

/** @defgroup ioctlcmds ioctl command (argument 2) values */
/** @addtogroup ioctlcmds */
/** @{ */
/** ioctl cmd to test the rng_read_register() function of the RNG driver */
#define RNG_TEST_READ_REG        _IOWR(RNG_TEST_DRIVER_IOCTL_IDENTIFIER, 1, \
         rng_test_reg_access_t)
/** ioctl cmd to test the rng_write_register() function of the RNG driver */
#define RNG_TEST_WRITE_REG       _IOWR(RNG_TEST_DRIVER_IOCTL_IDENTIFIER, 2, \
         rng_test_reg_access_t)
/** ioctl cmd to test the rng_add_entropy() function of the RNG driver */
#define RNG_TEST_ADD_ENTROPY     _IOWR(RNG_TEST_DRIVER_IOCTL_IDENTIFIER, 3, \
         rng_test_add_entropy_t)
/** ioctl cmd to test the rng_get_random() function of the RNG driver */
#define RNG_TEST_GET_RANDOM     _IOWR(RNG_TEST_DRIVER_IOCTL_IDENTIFIER, 4, \
         rng_test_get_random_t)
/** @} */


/** @defgroup ioctlStructs Special structs for argument 3 of ioctl
 *
 *
 */
/** @addtogroup ioctlStructs */
/** @{ */

/**
 * ioctl structure for add entropy to the RNG, through the driver, used
 * with #RNG_ADD_ENTROPY
 */
typedef struct {
    rng_return_t function_return_code; /**< Straight from RNG driver. */
    uint32_t  count_bytes;      /**< Number of bytes at entropy.  */
    uint8_t*  entropy;          /**< Location of entropy add to RNG.  */
} rng_test_add_entropy_t;


/**
 * ioctl structure for retrieving entropy from the RNG driver, used
 * with #RNG_GET_RANDOM
 */
typedef struct {
    int count_bytes;            /**< Amount of entropy to retrieve */
    uint8_t* random;           /**< Place to copy the random data  */
    int isr_flag;               /**< Request entropy using isr mode */
    rng_return_t function_return_code; /**< Straight from RNG driver */
} rng_test_get_random_t;


/**
 * ioctl structure for accessing RNG registers, used with
 * #RNG_TEST_READ_REG and #RNG_TEST_WRITE_REG.
 */
typedef struct {
    uint32_t reg_offset;        /**< The register address from Memory Map */
    uint32_t reg_data;          /**< Data to/from the register */
    rng_return_t      function_return_code; /**< Straight from RNG driver */
} rng_test_reg_access_t;


/*! @} */

#endif				/* rng_test_driver.h */
