// Copyright(c) 2017, Intel Corporation
//
// Redistribution  and  use  in source  and  binary  forms,  with  or  without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of  source code  must retain the  above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name  of Intel Corporation  nor the names of its contributors
//   may be used to  endorse or promote  products derived  from this  software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
// IMPLIED WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT  SHALL THE COPYRIGHT OWNER  OR CONTRIBUTORS BE
// LIABLE  FOR  ANY  DIRECT,  INDIRECT,  INCIDENTAL,  SPECIAL,  EXEMPLARY,  OR
// CONSEQUENTIAL  DAMAGES  (INCLUDING,  BUT  NOT LIMITED  TO,  PROCUREMENT  OF
// SUBSTITUTE GOODS OR SERVICES;  LOSS OF USE,  DATA, OR PROFITS;  OR BUSINESS
// INTERRUPTION)  HOWEVER CAUSED  AND ON ANY THEORY  OF LIABILITY,  WHETHER IN
// CONTRACT,  STRICT LIABILITY,  OR TORT  (INCLUDING NEGLIGENCE  OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,  EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif // HAVE_CONFIG_H

#include "common_int.h"

#include <sys/mman.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

// Buffer Allocation constants
#define KB 1024
#define MB (1024 * KB)
#define GB (1024 * MB)

#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40000
#endif
#ifndef MAP_HUGE_SHIFT
#define MAP_HUGE_SHIFT 26
#endif
#define MAP_1G_HUGEPAGE	(0x1e << MAP_HUGE_SHIFT)

#define PROTECTION (PROT_READ | PROT_WRITE)

#ifdef __ia64__
#define ADDR (void *)(0x8000000000000000UL)
#define FLAGS_4K (MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED)
#define FLAGS_2M (FLAGS_4K | MAP_HUGETLB)
#define FLAGS_1G (FLAGS_2M | MAP_1G_HUGEPAGE)
#else
#define ADDR (void *)(0x0UL)
#define FLAGS_4K (MAP_PRIVATE | MAP_ANONYMOUS)
#define FLAGS_2M (FLAGS_4K | MAP_HUGETLB)
#define FLAGS_1G (FLAGS_2M | MAP_1G_HUGEPAGE)
#endif


/*
 * Check properties object for validity and lock its mutex
 * If prop_check_and_lock() returns FPGA_OK, assume the mutex to be locked.
 */
fpga_result prop_check_and_lock(struct _fpga_properties *prop)
{
	ASSERT_NOT_NULL(prop);

	if (pthread_mutex_lock(&prop->lock)) {
		FPGA_MSG("Failed to lock mutex");
		return FPGA_EXCEPTION;
	}

	if (prop->magic != FPGA_PROPERTY_MAGIC) {
		FPGA_MSG("Invalid properties object");
		pthread_mutex_unlock(&prop->lock);
		return FPGA_INVALID_PARAM;
	}

	return FPGA_OK;
}

/*
 * Check handle object for validity and lock its mutex
 * If handle_check_and_lock() returns FPGA_OK, assume the mutex to be locked.
 */
fpga_result handle_check_and_lock(struct _fpga_handle *handle)
{
	ASSERT_NOT_NULL(handle);

	if (pthread_mutex_lock(&handle->lock)) {
		FPGA_MSG("Failed to lock mutex");
		return FPGA_EXCEPTION;
	}

	if (handle->magic != FPGA_HANDLE_MAGIC) {
		FPGA_MSG("Invalid handle object");
		pthread_mutex_unlock(&handle->lock);
		return FPGA_INVALID_PARAM;
	}

	return FPGA_OK;
}

/* mutex to protect global data structures */
pthread_mutex_t global_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

const char __FPGA_API__ *fpgaErrStr(fpga_result e)
{
	switch (e) {
	case FPGA_OK:
		return "success";
	case FPGA_INVALID_PARAM:
		return "invalid parameter";
	case FPGA_BUSY:
		return "resource busy";
	case FPGA_EXCEPTION:
		return "exception";
	case FPGA_NOT_FOUND:
		return "not found";
	case FPGA_NO_MEMORY:
		return "no memory";
	case FPGA_NOT_SUPPORTED:
		return "not supported";
	case FPGA_NO_DRIVER:
		return "no driver available";
	case FPGA_NO_DAEMON:
		return "no fpga daemon running";
	case FPGA_NO_ACCESS:
		return "insufficient privileges";
	default:
		return "unknown error";
	}
}

/**
 * @brief Generate unique workspace ID number
 *
 * @return id identifier
 */
uint64_t wsid_gen(void)
{
	struct timeval t;
	uint64_t id = 0;
	gettimeofday(&t, NULL);
	id = ((t.tv_sec * 1000 * 1000) + (t.tv_usec * 1000)) << 42;
	id |= ((unsigned long) getpid() % 16777216) << 24;
	return id;
}

/**
 * @brief
 *
 * @param guidh
 * @param guidl
 * @param guid
 */
void aal_guid_to_fpga(uint64_t guidh, uint64_t guidl, uint8_t *guid)
{
	uint32_t i;
	uint32_t s;

	// The API expects the MSB of the GUID at [0] and the LSB at [15].
	s = 64;
	for (i = 0; i < 8; ++i) {
		s -= 8;
		guid[i] = (uint8_t) ((guidh >> s) & 0xff);
	}

	s = 64;
	for (i = 0; i < 8; ++i) {
		s -= 8;
		guid[8 + i] = (uint8_t) ((guidl >> s) & 0xff);
	}
}

