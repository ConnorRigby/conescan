#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "uds_request_download.h"
#include "console.h"
#include "util.h"

#if defined(_WIN32) || defined(WIN32) || defined (_WIN64) || defined (WIN64)
DWORD WINAPI MyThreadFunction(LPVOID lpParam);
#else
#endif

struct ThreadData {
    RX8* ecu;
    UDSRequestDownload* request;
    uint8_t* seed;
    uint8_t* key;
    ConeScan::Console* console;
} _thread_data;

void uds_request_start_download(RX8* ecu, struct UDSRequestDownload* request, ConeScan::Console* console)
{
    if (_thread_data.ecu && _thread_data.request) return;
    if (request->transferChunkSize == 0) {
        console->AddLog("[UDS] Invalid Transfer params: chunkSize must be > 0");
        return;
    }
    if (request->transferSize == 0) {
        console->AddLog("[UDS] Invalid Transfer params: transferSize must be > 0");
        return;
    }
    memset(&_thread_data, 0, sizeof(struct ThreadData));
    _thread_data.ecu = ecu;
    _thread_data.request = request;
    _thread_data.console = console;
    _thread_data.request->downloadinProgress = 1;
#if defined(_WIN32) || defined(WIN32) || defined (_WIN64) || defined (WIN64)
	request->threadHandle = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        MyThreadFunction,       // thread function name
        &_thread_data,           // argument to thread function 
        0,                      // use default creation flags 
        &request->threadID      // thread param
    );
#else
#endif
}
#if defined(_WIN32) || defined(WIN32) || defined (_WIN64) || defined (WIN64)
DWORD WINAPI MyThreadFunction(LPVOID lpParam) {
    struct ThreadData* thread_data = (struct ThreadData*)lpParam;
#else
int MyThreadFunction(void* param) {
    struct ThreadData* thread_data = (struct ThreadData*) param;
#endif
    char* transferBuffer = NULL; // pointer to the real transfer buffer for pointer math, do not free

    thread_data->console->AddLog("[UDS] Starting download");
    if (!thread_data->ecu->initDiagSession(0x85)) {
        thread_data->console->AddLog("[UDS] Download failed: could not get diag session");
        goto cleanup;
    }
    thread_data->console->AddLog("[UDS] Diag Session 85 initialized");

    if (thread_data->ecu->getSeed(&thread_data->seed)) {
        thread_data->console->AddLog("[UDS] Download failed: could not get seed");
        goto cleanup;
    }
    thread_data->console->AddLog("[UDS] Got Key Seed: 0x%02X, 0x%02X, 0x%02X", thread_data->seed[0], thread_data->seed[1], thread_data->seed[2]);

    if (thread_data->ecu->calculateKey(thread_data->seed, &thread_data->key)) {
        thread_data->console->AddLog("[UDS] Download failed: could not calculate key");
        goto cleanup;
    }
    thread_data->console->AddLog("[UDS] Calculated Key 0x%02X, 0x%02X, 0x%02X", thread_data->key[0], thread_data->key[1], thread_data->key[2]);

    if (!thread_data->ecu->unlock(thread_data->key)) {
        thread_data->console->AddLog("[UDS] Download failed: could not exchange key");
    }
    // malloc this *after* exchange so it doesn't need to be freed
    thread_data->request->payload = (char*)malloc(sizeof(char) * thread_data->request->transferSize);
    assert(thread_data->request->payload);
    memset(thread_data->request->payload, 0, (sizeof(char) * thread_data->request->transferSize));

    thread_data->request->endAddress = thread_data->request->startAddress + thread_data->request->transferSize;
    thread_data->request->address = thread_data->request->startAddress;

    thread_data->console->AddLog("[UDS] Starting transfer\n TransferSize=0x%04X bytes\n ChunkSize=0x%04X bytes\n StartAddress=0x%04X\n EndAddress=0x%04X\n ", 
        thread_data->request->transferSize,
        thread_data->request->transferChunkSize,
        thread_data->request->startAddress,
        thread_data->request->endAddress);

    transferBuffer = thread_data->request->payload;

    for (thread_data->request->transferBytes = 0; 
         thread_data->request->address < thread_data->request->endAddress; 
         thread_data->request->address += thread_data->request->transferChunkSize, thread_data->request->transferBytes += thread_data->request->transferChunkSize, transferBuffer += thread_data->request->transferChunkSize) 
    {
        if (thread_data->request->downloadinProgress == false) break;
        assert(thread_data->request->endAddress > thread_data->request->address);
        if (thread_data->ecu->readMem(thread_data->request->address, thread_data->request->transferChunkSize, transferBuffer)) {
            thread_data->console->AddLog("[UDS] Transfer failed!");
            break;
        }
        thread_data->request->transferProgress = (float)((float)thread_data->request->transferBytes / (float)thread_data->request->transferSize);
    }
    hexdump(thread_data->request->payload, thread_data->request->transferChunkSize);
    thread_data->request->downloadinProgress = false;
    
cleanup:
    thread_data->console->AddLog("[UDS] Download complete");
    if (thread_data->key) {
        free(thread_data->key);
        thread_data->key = NULL;
    }
    if (thread_data->seed) {
        free(thread_data->seed);
        thread_data->seed = NULL;
    }

    return 0;
}

void uds_request_complete(struct UDSRequestDownload* request)
{
    request->downloadinProgress = false;
#if defined(_WIN32) || defined(WIN32) || defined (_WIN64) || defined (WIN64)
    if (request->threadHandle) {
        WaitForMultipleObjects(1, &request->threadHandle, TRUE, INFINITE);
        CloseHandle(request->threadHandle);
        request->threadHandle = NULL;
        request->threadID = NULL;
    }
#else
#endif
    if (request->payload) {
        free(request->payload);
        request->payload = NULL;
    }

    _thread_data.ecu = NULL;
    _thread_data.request = NULL;
    _thread_data.console = NULL;   
}