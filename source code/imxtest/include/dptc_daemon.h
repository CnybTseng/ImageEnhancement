/*
 * Copyright 2004-2009 Freescale Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @file include/dptc_daemon.h
 *
 * @brief DPTC daemon program header file.
 *
 * @ingroup DPTC
 */

#ifndef __DAEMON_H__
#define __DAEMON_H__

/*!
 * Defines a 32 bit unsigened integer
 */
typedef unsigned int u32;

/*!
 * This structure holds the parameters passed to a new communication
 * thread.
 */
typedef struct
{
	/*!
	 * File descriptor number of the socket used for communication.
	 */
	int socket;

	/*!
	 * File descriptor number of DPTC driver.
	 */
	int dptc_fd;

	/*!
	 * File descriptor number of DPTC proc file system file
	 * used for reading the DPTC log buffer.
	 */
	int log_fd;
} connection_param_s;

/*!
 * This enum defines the type of command/responce messages possible
 * in the communication between the daemon and the human interface program.
 */
typedef enum
{
	/*!
	 * Enable DPTC operation command message.
	 */
	ENABLE_DPTC	= 0,

	/*!
	 * Disable DPTC operation command message.
	 */
	DISABLE_DPTC,

	/*!
	 * Update DPTC translation table command message.
	 */
	WRITE_TABLE,

	/*!
	 * Read DPTC translation table from driver command message.
	 */
	READ_TABLE,

	/*!
	 * Set frequency index used by DPTC driver command message.
	 */
	SET_FREQ,

	/*!
	 * Get frequency index used by DPTC driver command message.
	 */
	GET_FREQ,

	/*!
	 * Get current DPTC module state (Enabled/Disabled) command message.
	 */
	GET_STATE,

	/*!
	 * Get DPTC driver log entries command message.
	 */
	GET_LOG,

	/*!
	 * End communication command message.
	 */
	QUIT,

	/*!
	 * Acknowledge message.
	 */
	ACK		= 98,

	/*!
	 * Not Acknowledge message.
	 */
	NACK		= 99
} cmd_type_e;

/*!
 * This structure defines the command/responce message structure.
 */
typedef struct
{
	/*!
	 * Command message type.
	 */
	cmd_type_e	cmd;

	/*!
	 * Message parameter.
	 */
	int		param;

	/*!
	 * Size of data message that follows the command message.
	 * If size is 0 no data message is sent.
	 */
	int		size;
} command_msg_s;

/*!
 * This structure is used in the daemon functions to indicate to the
 * do_connection function (the communication thread main function) what
 * return message to send to the human interface and what is the data
 * associated with the responce message.
 */
typedef struct
{
	/*!
	 * Responce message type (should be ACK or NACK).
	 */
	command_msg_s	ret_msg;

	/*!
	 * Pointer to the data to be sent after the command message.
	 * The data will be sent in the following message if ret_msg.size
	 * is not equal to 0.
	 */
	void		*data;
} return_values_s;


/* #define IN_BUFFER_SIZE	1024 */
/*!
 * Default port number used for daemon, human interface communication.
 */
#define DEFAULT_PORT_NUMBER 3532

/*!
 * Number of message backlog allowed in the socket communication.
 */
#define BACKLOG 10

/*!
 * Maximum number of frequencies allowd in the DPTC translation table.
 */
#define MAX_NUM_OF_FREQS	50

/*!
 * Maximum number of workin points allowd in the DPTC translation table.
 */
#define MAX_NUM_OF_WP		50


/* Error values returened from the daemon */

/*!
 * Daemon has received an invalid value.
 */
#define INVALED_VALUE_ERR	-1

/*!
 * Error while accessing the DPTC driver.
 */
#define DRIVER_ACCESS_ERR	-2

/*!
 * Invalid values received from the DPTC driver.
 */
#define INVLID_DRIVER_VALS_ERR	-3

/*!
 * Erorr Allocating memory.
 */
#define MEM_ALLOC_ERR		-4


/*
 * if not defined define TREU and FALSE values.
 */
#ifndef TRUE
	#define TRUE  1
	#define FALSE 0
#endif /* TRUE */

#endif /* __DAEMON_H__ */
