/*
 * Copyright (C) 2004-2009, 2011 Freescale Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*! @file scc2_test_driver.h
 *
 *  This header file provides definitions needed by the SCC2 test driver.
 *
 */


#ifndef SCC2_TEST_DRIVER_H
#define SCC2_TEST_DRIVER_H

#ifdef __KERNEL__

#include <portable_os.h>
#include <linux/mutex.h>

#endif /* kernel */


#include <linux/mxc_scc2_driver.h>


#ifndef SCC2_TEST_DRIVER_NAME

/** /dev/xxx name for this device */
#define SCC2_TEST_DRIVER_NAME "scc2_test"


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
#define IOCTL_SCC2_OK             0      /**< @c ioctl completed successfully */
#define IOCTL_SCC2_INVALID_CMD    ENOTTY /**< Invalid command passed. */
#define IOCTL_SCC2_INVALID_MODE   EINVAL /**< Invalid cipher mode. */
#define IOCTL_SCC2_SCM_BUSY       EBUSY  /**< SCM is busy */
#define IOCTL_SCC2_IMPROPER_STATE EBADFD /**< Improper state for operation */
#define IOCTL_SCC2_IMPROPER_ADDR  EFAULT /**< Improper address/offset passed */
#define IOCTL_SCC2_NO_MEMORY      ENOMEM /**< Insufficient memory to process */
#define IOCTL_SCC2_FAILURE        ESPIPE /**< Generic 'SCC2 error' error */
/** @} */

/*
 * Interface definitions between user and driver
 */

/* This is a porting opportunity.  It identifies the 'unique' byte
   value inserted into the IOCTL number.  It really only has to be
   unique within the software used on a given device. */
#ifndef SCC2_TEST_DRIVER_IOCTL_IDENTIFIER
#define SCC2_TEST_DRIVER_IOCTL_IDENTIFIER 's'
#endif

/* Define SCC2 Driver Commands (Argument 2 of ioctl) */

/** @defgroup ioctlcmds ioctl command (argument 2) values */
/** @addtogroup ioctlcmds */
/** @{ */
/** ioctl cmd to return version and configuration information on driver and
 *  SCC2 */

#define SCC2_TEST_GET_CONFIGURATION _IOW (SCC2_TEST_DRIVER_IOCTL_IDENTIFIER, 0, \
         scc_configuration_access)
/** ioctl cmd to test the scc_read_register() function of the SCC2 driver */
#define SCC2_TEST_READ_REG         _IOWR(SCC2_TEST_DRIVER_IOCTL_IDENTIFIER, 1, \
         scc_reg_access)
/** ioctl cmd to test the scc_write_register() function of the SCC2 driver */
#define SCC2_TEST_WRITE_REG        _IOWR(SCC2_TEST_DRIVER_IOCTL_IDENTIFIER, 2, \
         scc_reg_access)
/** ioctl cmd to test the scc_crypt() function of the SCC2 driver */
#define SCC2_TEST_ENCRYPT          _IOWR(SCC2_TEST_DRIVER_IOCTL_IDENTIFIER, 3, \
         scc_encrypt_decrypt)
/** ioctl cmd to test the scc_crypt() function of the SCC2 driver */
#define SCC2_TEST_DECRYPT          _IOWR(SCC2_TEST_DRIVER_IOCTL_IDENTIFIER, 4, \
         scc_encrypt_decrypt)
/** ioctl cmd to test the scc_set_sw_alarm() function of the SCC2 driver */
#define SCC2_TEST_SET_ALARM        _IO  (SCC2_TEST_DRIVER_IOCTL_IDENTIFIER, 5)
/** ioctl cmd to test the scc_zeroize_memories() function of the SCC2 driver */
#define SCC2_TEST_ZEROIZE          _IOWR(SCC2_TEST_DRIVER_IOCTL_IDENTIFIER, 6, \
         scc_return_t)
/** ioctl cmd to test partition allocation function of the SCC2 driver */
#define SCC2_TEST_ALLOC_PART       _IOWR(SCC2_TEST_DRIVER_IOCTL_IDENTIFIER, 12, \
        scc_partition_access)
/** ioctl cmd to test partition engagement function of the SCC2 driver */
#define SCC2_TEST_ENGAGE_PART      _IOWR(SCC2_TEST_DRIVER_IOCTL_IDENTIFIER, 13, \
        scc_partition_access)
/** ioctl cmd to test partition cipher function of the SCC2 driver */
#define SCC2_TEST_ENCRYPT_PART     _IOWR(SCC2_TEST_DRIVER_IOCTL_IDENTIFIER, 14, \
        scc_part_cipher_access)
#define SCC2_TEST_DECRYPT_PART     _IOWR(SCC2_TEST_DRIVER_IOCTL_IDENTIFIER, 15, \
        scc_part_cipher_access)
/** ioctl cmd to test partition allocation function of the SCC2 driver */
#define SCC2_TEST_RELEASE_PART     _IOWR(SCC2_TEST_DRIVER_IOCTL_IDENTIFIER, 16, \
        scc_partition_access)
/** ioctl cmd to load data into an SCC2 partition */
#define SCC2_TEST_LOAD_PART        _IOWR(SCC2_TEST_DRIVER_IOCTL_IDENTIFIER, 17, \
        scc_part_cipher_access)
/** ioctl cmd to lread data from an SCC2 partition */
#define SCC2_TEST_READ_PART        _IOWR(SCC2_TEST_DRIVER_IOCTL_IDENTIFIER, 18, \
        scc_part_cipher_access)

/** @} */


/** @defgroup ioctlStructs Special structs for argument 3 of ioctl
 *
 *
 */
/** @addtogroup ioctlStructs */
/** @{ */

/**
 * ioctl structure for retrieving driver & SCC2 version ids, used
 * with #SCC2_TEST_GET_CONFIGURATION.
 */
typedef struct {
    int driver_major_version;   /**< Major version of the SCC2 driver code  */
    int driver_minor_version;   /**< Minor version of the SCC2 driver code  */
    int scm_version;            /**< from Configuration register */
    int smn_version;            /**< from SMN Status register */
    int block_size;             /**< bytes in a block */
    int black_ram_size;         /**< number of blocks of Black RAM */
    int red_ram_size;           /**< number of blocks of Red RAM */
} scc_configuration_access;


/**
 * ioctl structure for accessing SCC2 registers, used with
 * #SCC2_TEST_READ_REG and #SCC2_TEST_WRITE_REG.
 */
typedef struct {
    uint32_t reg_offset;        /**< The register address from Memory Map */
    uint32_t reg_data;          /**< Data to/from the register */
    scc_return_t      function_return_code; /**< Straight from SCC2 driver */
} scc_reg_access;


/**
 * ioctl structure for partition setup/teardown functions
 */
typedef struct {
    uint32_t virt_address;      /**< Its virtual address */
    union {
        /* for allocate */
        struct {
            uint32_t smid;      /**< From user  */
            uint32_t part_no;   /**< Which partition was allocated */
            uint32_t phys_address; /**<Its physical address */
        };
        /* for engage */
        struct {
            const uint8_t* umid; /**< 16-byte UMID for partition, or NULL */
            uint32_t permissions; /**< Mode register setting for initial access  */
        };
    };
    scc_return_t scc_status;    /**< Returned status of call to scc2 driver */
} scc_partition_access;

/** @} */

/**
 * ioctl structure for partition encrypt/decrypt functions
 */
typedef struct {
    uint32_t virt_address;      /**< Its virtual address */
    uint32_t red_offset;        /**< Byte offset into partition */
    uint8_t* black_address;     /**< Virtual address of Black data */
    uint32_t size_bytes;        /**< Number of bytes of red/black data */
    uint64_t iv;                /**< first half of IV for CBC mode; second half
                                   is zero */
    scc_return_t scc_status;    /**< Returned status of call to scc2 driver */
} scc_part_cipher_access;

#endif /* scc2_test_driver.h */
