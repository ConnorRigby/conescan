#pragma once

#include <stdint.h>

#include "librx8.h"
#include "console.h"

#if defined(_WIN32) || defined(WIN32) || defined (_WIN64) || defined (WIN64)
#include <Windows.h>
#else
#include <pthread.h>
#endif

struct UDSRequestDownload {
	unsigned long address;
	unsigned long startAddress;
	unsigned long endAddress;
	unsigned long transferSize;
	uint16_t      transferChunkSize;

	float         transferProgress;
	uint32_t      transferBytes;
	char*         payload;
	bool          downloadinProgress;

#if defined(_WIN32) || defined(WIN32) || defined (_WIN64) || defined (WIN64)
	DWORD         threadID;
	HANDLE        threadHandle;
#else

#endif

};

void uds_request_start_download(RX8* ecu, struct UDSRequestDownload* request, ConeScan::Console* console);
void uds_request_complete(struct UDSRequestDownload* request);
