// Independent IR:USER client for the Circle Pad Pro (MIT).
// Protocol references and implementation notes are in ../README.md.
#include "cpp.h"
#include "cpp_packet.h"
#include <stdlib.h>
#include <string.h>

enum { SHARED_SIZE = 0x1000, RX_SIZE = 0x800, RX_SLOTS = 32,
       TX_SIZE = 0x400, TX_SLOTS = 8, RX_DATA = RX_SIZE - RX_SLOTS * 8 };

static Handle service, memoryBlock;
static volatile u8 *shared;
static bool session, suspended;
static u64 lastAttempt, lastRequest, lastInput;
static CppInput state;

static Result transact(void)
{
    Result result = svcSendSyncRequest(service);
    return R_FAILED(result) ? result : (Result)getThreadCommandBuffer()[1];
}

static Result command(u16 id)
{
    getThreadCommandBuffer()[0] = IPC_MakeHeader(id, 0, 0);
    return transact();
}

static Result commandArg(u16 id, u32 value)
{
    u32 *ipc = getThreadCommandBuffer();
    ipc[0] = IPC_MakeHeader(id, 1, 0);
    ipc[1] = value;
    return transact();
}

static void disconnect(void)
{
    if (session) {
        command(0x09); // Disconnect
        command(0x02); // FinalizeIrnop
        session = false;
    }
    state.connected = false;
    state.keys = 0;
    state.position = (circlePosition){0};
    state.battery = 0;
    lastInput = lastRequest = 0;
}

static Result startSession(void)
{
    u32 *ipc = getThreadCommandBuffer();
    ipc[0] = IPC_MakeHeader(0x18, 6, 2); // InitializeIrnopShared
    ipc[1] = SHARED_SIZE;
    ipc[2] = RX_SIZE;
    ipc[3] = RX_SLOTS;
    ipc[4] = TX_SIZE;
    ipc[5] = TX_SLOTS;
    ipc[6] = 4; // CPP IR bitrate selector
    ipc[7] = IPC_Desc_SharedHandles(1);
    ipc[8] = memoryBlock;
    Result result = transact();
    if (R_FAILED(result)) return result;
    session = true;
    result = commandArg(0x06, 1); // RequireConnection: Circle Pad Pro
    if (R_FAILED(result)) disconnect();
    return result;
}

static Result requestInput(void)
{
    // Request a report every 32 ms. This command does not change calibration.
    const u8 request[] = {1, 32, 0x87};
    u32 *ipc = getThreadCommandBuffer();
    ipc[0] = IPC_MakeHeader(0x0D, 1, 2); // SendIrnop
    ipc[1] = sizeof(request);
    ipc[2] = IPC_Desc_StaticBuffer(sizeof(request), 0);
    ipc[3] = (u32)request;
    return transact();
}

Result cppInit(void)
{
    memset(&state, 0, sizeof(state));
    Result result = srvGetServiceHandle(&service, "ir:USER");
    if (R_FAILED(result)) goto failed;
    shared = aligned_alloc(SHARED_SIZE, SHARED_SIZE);
    if (!shared) { result = -1; goto failed; }
    memset((void *)shared, 0, SHARED_SIZE);
    result = svcCreateMemoryBlock(&memoryBlock, (u32)shared, SHARED_SIZE,
                                  MEMPERM_READ, MEMPERM_READWRITE);
    if (R_FAILED(result)) goto failed;
    lastAttempt = 0;
    suspended = false;
    return 0;
failed:
    cppExit();
    state.error = result;
    return result;
}

void cppPoll(CppInput *input)
{
    if (!service || suspended) { *input = state; return; }
    u64 now = osGetTime();
    if (!session) {
        if (lastAttempt && now - lastAttempt < 1000) { *input = state; return; }
        lastAttempt = now;
        state.error = startSession();
        *input = state;
        return;
    }
    __sync_synchronize();
    if (shared[8] != 2) {
        // Allow up to one second for a handshake, then reconnect automatically.
        if (state.connected || now - lastAttempt >= 1000) disconnect();
        *input = state;
        return;
    }

    // Drain the bounded receive queue. Release each packet only after decoding.
    const volatile u32 *words = (const volatile u32 *)shared;
    for (unsigned int n = 0; n < RX_SLOTS && words[6]; ++n) {
        __sync_synchronize();
        u32 slot = words[4];
        if (slot >= RX_SLOTS || words[6] > RX_SLOTS) {
            state.error = -1;
            disconnect();
            break;
        }
        u32 offset = words[8 + slot * 2];
        u32 size = words[9 + slot * 2];
        CppSample sample;
        if (cppDecodePacket(shared + 0x20 + RX_SLOTS * 8, RX_DATA,
                            offset, size, &sample)) {
            state.position.dx = sample.x;
            state.position.dy = sample.y;
            state.battery = sample.battery;
            state.keys = (sample.buttons & CPP_R ? KEY_R : 0)
                       | (sample.buttons & CPP_ZL ? KEY_ZL : 0)
                       | (sample.buttons & CPP_ZR ? KEY_ZR : 0);
            state.connected = true;
            lastInput = now;
        }
        state.error = commandArg(0x19, 1); // ReleaseReceivedData
        if (R_FAILED(state.error)) { disconnect(); break; }
    }
    // Expire stale stick/button values promptly if infrared communication stops.
    if (lastInput && now - lastInput > 250) disconnect();
    if (session && (!lastRequest || now - lastRequest >= 32)) {
        state.error = requestInput();
        lastRequest = now;
        if (R_FAILED(state.error)) disconnect();
    }
    if (session && !lastInput && now - lastAttempt > 2000) disconnect();
    *input = state;
}

void cppSuspend(bool value)
{
    suspended = value;
    if (value) disconnect();
    else lastAttempt = 0;
}

void cppExit(void)
{
    if (service) disconnect();
    if (memoryBlock) svcCloseHandle(memoryBlock);
    if (service) svcCloseHandle(service);
    free((void *)shared);
    shared = NULL;
    memoryBlock = service = 0;
}
