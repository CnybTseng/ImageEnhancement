#ifndef _DEBUG_H_
#define _DEBUG_H_
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
 
#define DEBUG_ON    	1
#define EDEBUG_ON    	1
#define ASSERT_ON		1
 
/**
 * Print debug message.
 */
#ifdef DEBUG_ON
#define PR_DEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#else
#define PR_DEBUG(fmt, args...) /*do nothing */
#endif
 
/**
 * Print error message.
 */
#ifdef EDEBUG_ON
#define PR_ERR(fmt, args...) fprintf(stderr, "\nError:\nFile:<%s> Fun:[%s] Line:%d\n ", __FILE__, __FUNCTION__, __LINE__, ##args)
#else
#define PR_ERR(fmt, args...) /*do nothing */
#endif
 
/**
 * Condition assert.
 */
#ifdef ASSERT_ON
#define EX_ASSERT(condition, fmt, args...) \
({ \
	if(condition) \
    {   \
		fprintf(stderr, "\nError:\nFile:<%s> Fun:[%s] Line:%d\n ", __FILE__, __FUNCTION__, __LINE__, ##args); \
		abort(); \
    } \
})
#else
#define EX_ASSERT(condition, fmt, args...) NULL
#endif
 
#endif