/*
 * Copyright 2004-2009 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef SCC_TEST_DRIVER_H
#define SCC_TEST_DRIVER_H

#ifdef __KERNEL__

#include <portable_os.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include <asm/uaccess.h>
#include <asm/memory.h>

#include <linux/mm.h>           /* for io_remap_page_range() */

#endif /* kernel */

#include <linux/mxc_scc_driver.h>

#ifdef __KERNEL__

static int scc_test_init(void);
static void scc_test_cleanup(void);
OS_DEV_IOCTL(scc_test_ioctl);
OS_DEV_OPEN(scc_test_open);
OS_DEV_CLOSE(scc_test_release);
OS_DEV_MMAP(scc_test_mmap);

static int scc_test_get_configuration(unsigned long scc_data);
static int scc_test_read_register(unsigned long scc_data);
static int scc_test_write_register(unsigned long scc_data);
static int scc_test_cipher(uint32_t cmd, unsigned long scc_data);
static int scc_test_zeroize(unsigned long scc_data);
static int setup_user_driver_interaction(void);
static void scc_test_report_failure(void);

static int scc_test_alloc_slot(unsigned long cmd, unsigned long scc_data);
static int scc_test_dealloc_slot(unsigned long cmd, unsigned long scc_data);
static int scc_test_load_slot(unsigned long cmd, unsigned long scc_data);
static int scc_test_encrypt_slot(unsigned long cmd, unsigned long scc_data);
static int scc_test_get_slot_info(unsigned long cmd, unsigned long scc_data);
extern int scc_test_major_node;


#ifndef SCC_TEST_DRIVER_NAME
/** /dev/xxx name for this device */
#define SCC_TEST_DRIVER_NAME "scc_test"
#endif


#ifndef SCC_TEST_MAJOR_NODE
/** Linux major node value for the device special file (/dev/scc_test) */
#define SCC_TEST_MAJOR_NODE  240
#endif

#endif /* kernel */


/* User/Driver interface definitions */

/**
 * @defgroup ioctlreturns Ioctl Error Codes.
 *
 * These are the values returned by #scc_ioctl and placed into @c
 * errno by @c ioctl. Porting opportunity.  These values were chosen to
 * match standard Linux values.
 */
/** @addtogroup ioctlreturns */
/** @{ */
#define IOCTL_SCC_OK             0      /**< @c ioctl completed successfully */
#define IOCTL_SCC_INVALID_CMD    ENOTTY /**< Invalid command passed. */
#define IOCTL_SCC_INVALID_MODE   EINVAL /**< Invalid cipher mode. */
#define IOCTL_SCC_SCM_BUSY       EBUSY  /**< SCM is busy */
#define IOCTL_SCC_IMPROPER_STATE EBADFD /**< Improper state for operation */
#define IOCTL_SCC_IMPROPER_ADDR  EFAULT /**< Improper address/offset passed */
#define IOCTL_SCC_NO_MEMORY      ENOMEM /**< Insufficient memory to process */
#define IOCTL_SCC_FAILURE        ESPIPE /**< Generic 'SCC error' error */
/** @} */

/*
 * Interface definitions between user and driver
 */

/* This is a porting opportunity.  It identifies the 'unique' byte
   value inserted into the IOCTL number.  It really only has to be
   unique within the software used on a given device. */
#ifndef SCC_TEST_DRIVER_IOCTL_IDENTIFIER
#define SCC_TEST_DRIVER_IOCTL_IDENTIFIER 's'
#endif

/* Define SCC Driver Commands (Argument 2 of ioctl) */

/** @defgroup ioctlcmds ioctl command (argument 2) values */
/** @addtogroup ioctlcmds */
/** @{ */
/** ioctl cmd to return version and configuration information on driver and
 *  SCC */

#define SCC_TEST_GET_CONFIGURATION _IOW (SCC_TEST_DRIVER_IOCTL_IDENTIFIER, 0, \
         scc_configuration_access)
/** ioctl cmd to test the scc_read_register() function of the SCC driver */
#define SCC_TEST_READ_REG         _IOWR(SCC_TEST_DRIVER_IOCTL_IDENTIFIER, 1, \
         scc_reg_access)
/** ioctl cmd to test the scc_write_register() function of the SCC driver */
#define SCC_TEST_WRITE_REG        _IOWR(SCC_TEST_DRIVER_IOCTL_IDENTIFIER, 2, \
         scc_reg_access)
/** ioctl cmd to test the scc_crypt() function of the SCC driver */
#define SCC_TEST_ENCRYPT          _IOWR(SCC_TEST_DRIVER_IOCTL_IDENTIFIER, 3, \
         scc_encrypt_decrypt)
/** ioctl cmd to test the scc_crypt() function of the SCC driver */
#define SCC_TEST_DECRYPT          _IOWR(SCC_TEST_DRIVER_IOCTL_IDENTIFIER, 4, \
         scc_encrypt_decrypt)
/** ioctl cmd to test the scc_set_sw_alarm() function of the SCC driver */
#define SCC_TEST_SET_ALARM        _IO  (SCC_TEST_DRIVER_IOCTL_IDENTIFIER, 5)
/** ioctl cmd to test the scc_zeroize_memories() function of the SCC driver */
#define SCC_TEST_ZEROIZE          _IOWR(SCC_TEST_DRIVER_IOCTL_IDENTIFIER, 6, \
         scc_return_t)
/** ioctl cmd to test the scc_slot_alloc() function of the SCC driver */
#define SCC_TEST_ALLOC_SLOT       _IOWR(SCC_TEST_DRIVER_IOCTL_IDENTIFIER, 7, \
        scc_alloc_slot_access)
/** ioctl cmd to test the scc_slot_dealloc() function of the SCC driver */
#define SCC_TEST_DEALLOC_SLOT     _IOWR(SCC_TEST_DRIVER_IOCTL_IDENTIFIER, 8, \
        scc_dealloc_slot_access)
/** ioctl cmd to test the scc_slot_load_slot() or scc_decrypt_slot() functions
 * of the SCC driver */
#define SCC_TEST_LOAD_SLOT        _IOWR(SCC_TEST_DRIVER_IOCTL_IDENTIFIER, 9, \
        scc_load_slot_access)
/** ioctl cmd to test the scc_get_slot_info() function of the SCC driver */
#define SCC_TEST_GET_SLOT_INFO    _IOWR(SCC_TEST_DRIVER_IOCTL_IDENTIFIER, 10, \
        scc_get_slot_info_access)
/** ioctl cmd to test the scc_encrypt_slot() function of the SCC driver */
#define SCC_TEST_ENCRYPT_SLOT     _IOWR(SCC_TEST_DRIVER_IOCTL_IDENTIFIER, 11, \
        scc_encrypt_slot_access)
/** @} */


/** @defgroup ioctlStructs Special structs for argument 3 of ioctl
 *
 *
 */
/** @addtogroup ioctlStructs */
/** @{ */

/**
 * ioctl structure for retrieving driver & SCC version ids, used
 * with #SCC_TEST_GET_CONFIGURATION.
 */
typedef struct {
    int driver_major_version;   /**< Major version of the SCC driver code  */
    int driver_minor_version;   /**< Minor version of the SCC driver code  */
    int scm_version;            /**< from Configuration register */
    int smn_version;            /**< from SMN Status register */
    int block_size;             /**< bytes in a block */
    int black_ram_size;         /**< number of blocks of Black RAM */
    int red_ram_size;           /**< number of blocks of Red RAM */
} scc_configuration_access;


/**
 * ioctl structure for accessing SCC registers, used with
 * #SCC_TEST_READ_REG and #SCC_TEST_WRITE_REG.
 */
typedef struct {
    uint32_t reg_offset;        /**< The register address from Memory Map */
    uint32_t reg_data;          /**< Data to/from the register */
    scc_return_t      function_return_code; /**< Straight from SCC driver */
} scc_reg_access;


/**
 * ioctl structure for SCC encryption and decryption, used with
 * #SCC_TEST_ENCRYPT and #SCC_TEST_DECRYPT.
 */
typedef struct {
    uint8_t*          data_in;           /**< Starting text for cipher */
    unsigned long     data_in_length;    /**< Number of bytes in data_in */
    uint8_t*          data_out;          /**< Resulting text of cipher */
    unsigned long     data_out_length;   /**< Number of bytes in data_out  */
    /** inform driver which type of cipher mode to use */
    scc_crypto_mode_t crypto_mode;
    scc_verify_t      check_mode;        /**< none/padded or CRC/pad */
    uint32_t          init_vector[2]; /**< Initial value of cipher function */
    /** Inform driver whether to poll or sleep while waiting for
        cipher completion */
    enum scc_encrypt_wait {
        SCC_ENCRYPT_SLEEP,      /**< Put process to sleep during operation */
        SCC_ENCRYPT_POLL        /**< Make CPU monitor status for completion */
    } wait_mode;
    scc_return_t      function_return_code; /**< Straight from SCC driver */
    scc_verify_t      verify_mode;   /**< Whether to add padding & CRC  */
} scc_encrypt_decrypt;

/**
 *
 */
typedef struct {
    uint64_t value_size_bytes;  /**< Key size being queried */
    uint32_t black_size_bytes;  /**< Associated black size; 0 for unsupported */
} scc_get_black_length_access;


/**
 * ioctl structure for allocating RED KEY slot.
 */
typedef struct {
    uint32_t value_size_bytes;  /**< Datum size for this slot */
    uint32_t slot;              /**< Returned Slot number */
    uint64_t owner_id;          /**< Access control info for slot */
    scc_return_t scc_status;    /**< Returned status of call to scc driver */
} scc_alloc_slot_access;


/**
 * ioctl structure for deallocating RED KEY slot.
 */
typedef struct {
    uint32_t slot;              /**< Slot being deallocated */
    scc_return_t scc_status;    /**< Returned status of call to scc driver */
    uint64_t owner_id;          /**< Access control info for slot */
} scc_dealloc_slot_access;


/**
 *
 */
typedef struct {
    uint32_t slot;              /**< Slot being queried */
    unsigned key_is_red;        /**< true - load value; else decrypt to slot */
    uint64_t owner_id;          /**< Access control info for slot */
    uint8_t* key_data;          /**< Black (wrapped) key/data to be loaded */
    uint32_t key_data_length;   /**< Length, in bytes, of @c key_data */
    scc_return_t scc_status;    /**< Returned status of call to scc driver */
} scc_load_slot_access;

/**
 *
 */
typedef struct {
    uint32_t slot;              /**< Slot being queried */
    uint8_t* key_data;          /**< Black (wrapped) key/data to be loaded */
    uint64_t owner_id;          /**< Access control info for slot */
    uint32_t key_data_length;   /**< Length, in bytes, of @c key_data */
    scc_return_t scc_status;    /**< Returned status of call to scc driver */
} scc_encrypt_slot_access;


/**
 *
 */
typedef struct {
    uint32_t slot;              /**< Slot being queried */
    uint32_t address;           /**< Returned physical address of slot */
    uint64_t owner_id;          /**< Access control info for slot */
    uint32_t value_size_bytes;  /**< Returned datum size */
    uint32_t slot_size_bytes;   /**< Returned max slot size */
    scc_return_t scc_status;    /**< Returned status of call to scc driver */
} scc_get_slot_info_access;

/** @} */


#endif /* scc_test_driver.h */
