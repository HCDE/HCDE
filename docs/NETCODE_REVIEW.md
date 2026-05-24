# HCDE Netcode Review Document

**Review Date:** 2026-05-24  
**Author:** Zed Agent  
**Scope:** Network layer implementation (`i_net.cpp`, `hcde_servermode.cpp`, `hcde_servermode.h`, netcode stress tests)

---

## Executive Summary

After reviewing the HCDE netcode implementation, I identified several issues. Some earlier notes were overly broad assumptions; this revision distinguishes fixed issues from real risks.

---

## 1. CRITICAL ISSUES (Priority: Fix Immediately)

### 1.1 Race Condition in `HandleIncomingConnection()`

**Location:** `i_net.cpp`, Lines ~2348-2454

```cpp
void HandleIncomingConnection()
{
    if (!I_IsLocalHCDEServiceAuthority() || RemoteClient == -1)
        return;
    
    auto& con = Connected[RemoteClient];  // <-- Shared state access
    
    if (NetBuffer[1] == PRE_USER_INFO && con.Status == CSTAT_CONNECTING)
    {
        // ... accesses con.Status, NetBuffer ...
        // No mutex or synchronization
    }
}
```

**Issue:** Prior review assumed concurrent access from another thread. The current looped net processing path is single-threaded, so this race is not currently reproducible.

**Potential Impact:** 
- Concurrent access to `con.Status`, `NetBuffer`, and other connection state could cause data races
- Packet processing order is not guaranteed

**Status:** not reproduced in the current architecture.

---

### 1.2 Memory Safety in `GetPacket()`

**Location:** `i_net.cpp`, Lines ~1700-1825

```cpp
static void GetPacket(sockaddr_in* const from = nullptr)
{
    sockaddr_in fromAddress;
    socklen_t fromSize = sizeof(fromAddress);
    
    int msgSize = recvfrom(MySocket, (char *)TransmitBuffer, MaxTransmitSize, 0,
                        (sockaddr *)&fromAddress, &fromSize);
    
    // ... later ...
    else if (msgSize > 0)
    {
        // ... decompression code ...
        else
        {
            msgSize -= 4;
            memcpy(NetBuffer + 1, dataStart + 1, msgSize - 1);  // <-- Potential overflow
        }
    }
}
```

**Issue:** Prior code relied on implicit assumptions around payload length and could underflow on malformed input.

**Potential Impact:** 
- Memory corruption under specific network conditions
- Potential buffer overflow leading to crashes

**Status:** fixed in `i_net.cpp` with explicit checks for `msgSize < 5`, payload-size validation, and bounded uncompressed-copy length.

---

### 1.3 State Error in `DriveRuntimeSetupStateForClient()`

**Location:** `i_net.cpp`, Lines ~2161-2346

```cpp
static void DriveRuntimeSetupStateForClient(int client, int connectedPlayers)
{
    if (client <= 0 || client >= MaxClients)
        return;
    
    auto& con = Connected[client];
    if (con.Status == CSTAT_NONE)
        return;
    
    if (con.Status == CSTAT_CONNECTING)
    {
        // ... sends packets ...
        if (con.bHCDEConnect)
        {
            if (BeginReliableHCDEPregameService(HPS_CONSOLE_PLAYER, con, uint8_t(client)))
            {
                // ...
            }
        }
    }
}
```

**Issue:** Queue saturation can block setup progression when reliable service enqueuing is denied.

**Potential Impact:** 
- Stuck connection states if reliable service queue fills up
- Clients may never reach `CSTAT_READY` state
- Connection hang

**Status:** enqueue return codes are checked at call sites, but we still need stronger saturation telemetry and backoff behavior.

---

### 1.4 Deadlock Potential in `Host_CheckForConnections()`

**Location:** `i_net.cpp`, Lines ~2483-2858

```cpp
static bool Host_CheckForConnections(void* connected)
{
    const bool hasPassword = strlen(net_password) > 0;
    int* connectedPlayers = (int*)connected;
    
    // ... loops ...
    
    // Inside loops:
    I_GetKickClients(toBoot);
    for (auto client : toBoot)
    {
        RemoveClientConnection(client, "kicked during setup");  // <-- Modifies state
        // ... more state changes ...
    }
}
```

**Issue:** This remains a speculative concern around callback re-entry; no concrete lock inversion is visible with current single-threaded networking loops.

**Potential Impact:** 
- Deadlock under load
- UI freezes during high-pressure scenarios

**Status:** monitor during future multi-threaded refactors.

---

## 2. HIGH PRIORITY ISSUES (Priority: Fix Before Release)

### 2.1 Missing Comments in Critical Code Sections

Multiple functions lack documentation comments:

```cpp
// Line ~1588
static void SendPacket()
{
    uint8_t* dataStart = &TransmitBuffer[4];  // What's here?
    // ...
}
```

**Issue:** Several critical functions needed more context and were partially undocumented in earlier review.

**Status:** added packet-offset and pending-service field comments in `i_net.cpp`. Remaining function-level comments are still recommended.

**Recommendation:** Add comprehensive comments explaining:
- What the function does
- Pre-conditions
- Post-conditions
- Error handling

---

### 2.2 Unhandled Edge Cases in Session Token Validation

```cpp
static bool CheckSessionToken(const FConnection& connection, uint32_t token, const char* context)
{
    if (connection.SessionToken != token)
    {
        DebugTrace::Markf("net", "%s token mismatch expected=%08x got=%08x", context, connection.SessionToken, token);
        return false;
    }
    return true;
}
```

**Issue:** Token mismatch currently results in false return and packet-path rejection in current call chains; behavior is consistent for existing setup/service ingress.

**Recommendation:** Add a retry counter and exponential backoff for token mismatches before hard rejection.

---

### 2.3 Inconsistent Error Handling in Packet Parsing

```cpp
static bool CheckHCDEPregameService(size_t client, size_t minimumSize, const char* context)
{
    if (NetBufferLength < minimumSize)
    {
        DebugTrace::Markf("net", "%s service packet too short len=%zu minimum=%zu", context, NetBufferLength, minimumSize);
        return false;
    }
    
    auto& connection = Connected[client];
    if (!CheckSessionToken(connection, ReadBE32(&NetBuffer[3]), context))
        return false;
    // ...
}
```

**Issue:** Some callers treat `false` as "ignore this packet" while others might treat it as an error condition requiring recovery.

**Recommendation:** Add a parameter to distinguish between "ignore" and "error" cases, or use a more specific error code.

---

### 2.4 Missing Timeout Handling for Client Disconnections

```cpp
static bool DropClientForHCDETimeout(int client, int* connectedPlayers, const char* context)
{
    if (client <= 0 || client >= MaxClients || Connected[client].Status == CSTAT_NONE || !Connected[client].bHCDEConnect)
        return false;
    
    auto* pending = FindTimedOutHCDEReliableService(Connected[client], I_msTime());
    if (pending == nullptr)
        return false;
    // ...
}
```

**Issue:** Timeout checks are periodic by design, sharing maintenance flow with setup/runtime loops.

**Potential Impact:** 
- Stale connections accumulate
- Memory leaks over time (connection structs not freed)

**Status:** adequate under current single-thread architecture; revisit if maintenance is split across threads.

---

### 2.5 Race Condition in Connection State Updates

```cpp
static void AddClientConnection(const sockaddr_in& from, int client, const FHCDEConnectInfo& connectInfo, bool runtimeJoin)
{
    Net_ResetClientState(client);
    Connected[client].Status = CSTAT_CONNECTING;
    Connected[client].Address = from;
    Connected[client].SessionToken = MakeSessionToken(from, client);
    Connected[client].bHCDEConnect = connectInfo.Present;
    // ...
    for (int i = 1; i < MaxClients; ++i)
    {
        if (Connected[i].Status == CSTAT_READY)
        {
            Connected[i].Status = CSTAT_WAITING;  // <-- Concurrency issue
            I_NetClientUpdated(i);
        }
    }
}
```

**Issue:** The loop updates `Connected[i].Status` without synchronization. If multiple clients are processed simultaneously, this could cause state corruption.

**Recommendation:** Use atomic operations or mutex protection for status updates.

---

## 3. MEDIUM PRIORITY ISSUES (Priority: Fix in Next Sprint)

### 3.1 Incomplete Comments

While some functions are commented, many others lack:
- Parameter documentation
- Return value documentation
- Complexity analysis

**Example:**

```cpp
struct FHCDEPendingService
{
    bool Active = false;
    EHCDEPregameService Service = HPS_HEARTBEAT;
    uint8_t Key = 0u;
    uint32_t Sequence = 0u;
    // ...
    
    void Clear()
    {
        Active = false;
        Service = HPS_HEARTBEAT;
        // ... clears all fields ...
    }
};
```

**Issue:** `Clear()` function doesn't explain what it's clearing for, or why it's important.

**Recommendation:** Add documentation explaining the purpose of each field and the `Clear()` method.

---

### 3.2 Magic Numbers Without Explanation

```cpp
constexpr size_t HCDEServiceSequenceOffset = 7u;
constexpr size_t HCDEServiceAckOffset = 11u;
constexpr size_t HCDEServiceHeaderSize = 15u;
```

**Issue:** These constants lacked explicit inline comments.

**Status:** fixed in `i_net.cpp` with packet structure comments.

---

### 3.3 Unnecessary Static Variables

```cpp
static FRandom GameIDGen = {};
static uint8_t GameID[8] = {};
static u_short GamePort = (IPPORT_USERRESERVED + 29);
static SOCKET MySocket = INVALID_SOCKET;
```

**Issue:** Some static variables could cause issues if the module is loaded/unloaded multiple times in a modular build.

**Recommendation:** Review whether these need to be static or if they can be made thread-local where appropriate.

---

### 3.4 Potential Integer Overflow

```cpp
static int CountConnectedPlayers()
{
    int connected = 0;
    for (int i = 0; i < MaxClients; ++i)
    {
        if (Connected[i].Status != CSTAT_NONE)
            ++connected;
    }
    return connected;
}
```

**Issue:** The return type is `int`, but should be `size_t` or `uint8_t` since it's used with client counts.

**Recommendation:** Use appropriate unsigned types for counts.

---

### 3.5 Inconsistent Logging

```cpp
DebugTrace::Markf("net", "client disconnected client=%d name=%s", client, Net_GetClientName(client, 0u));
Printf("%s:: Client '%s' disconnected.\n", ...);
```

**Issue:** Some uses `DebugTrace::Markf()`, others use `Printf()`. This creates inconsistent logging behavior.

**Recommendation:** Standardize on a logging interface across all functions.

---

## 4. LOW PRIORITY ISSUES (Cosmetic/Documentation)

### 4.1 Missing Error Messages

Some error cases don't have meaningful error messages:

```cpp
if (NetBufferLength < minimumSize)
{
    DebugTrace::Markf("net", "%s service packet too short len=%zu minimum=%zu", context, NetBufferLength, minimumSize);
    return false;
}
```

**Issue:** The error is logged but the function returns `false` without explaining what should happen next.

**Recommendation:** Add more context to error messages or return specific error codes.

---

### 4.2 Unnecessary Function Duplication

The code has significant duplication between:
- `HandleIncomingConnection()` and `HandleIncomingConnectionMaintenance()`
- `Host_CheckForConnections()` and `Host_CheckStartGameAcks()`

**Recommendation:** Extract common logic into helper functions to reduce code size and improve maintainability.

---

## 5. CODE QUALITY CONCERNS

### 5.1 Lack of Unit Tests

The stress test harness exists (`tests/netcode_step12/`), but there are no unit tests for individual network functions.

**Recommendation:** Create unit tests for:
- `CheckSessionToken()`
- `CheckHCDEPregameService()`
- `FindTimedOutHCDEReliableService()`
- Packet parsing edge cases

---

### 5.2 No Fuzzing Tests

The netcode doesn't appear to have fuzzing tests for:
- Malformed packets
- Invalid CRC values
- Buffer overflow attempts
- Protocol version mismatches

**Recommendation:** Add fuzzing tests for packet parsing functions.

---

### 5.3 Missing Memory Leak Detection

While there's no evidence of memory leaks in the review, there's no mechanism in place to:
- Track allocated memory
- Detect leaks on shutdown
- Validate free lists

**Recommendation:** Add memory tracking using a custom allocator or standard tools.

---

## 6. RECOMMENDATIONS SUMMARY

### Immediate Actions (This Sprint)

1. **Improve queue pressure behavior** in HCDE reliable service enqueuing.
2. **Extend comment coverage** for all critical network functions (header + flow comments).
3. **Harden malformed/oversized packet paths** with targeted fuzz coverage.
4. **Standardize log categories** across setup/service paths.

### Short-term Actions (Next Sprint)

1. **Add unit tests** for network functions
2. **Add fuzzing tests** for packet parsing
3. **Review static variables** for thread safety
4. **Fix integer type usage** throughout

### Medium-term Actions (Q3 2026)

1. **Implement memory tracking** for network buffers
2. **Add watchdog threads** for connection cleanup
3. **Refactor duplicate code** into utilities
4. **Add comprehensive documentation** for network protocol

---

## 7. FILES RECOMMENDED FOR COMMENTED REVIEW

All code needs comprehensive commenting. The following functions should be documented:

### Priority 1 (Essential)

- `GetPacket()`
- `SendPacket()`
- `CheckSessionToken()`
- `CheckHCDEPregameService()`
- `HandleIncomingConnection()`
- `HandleIncomingConnectionMaintenance()`
- `Host_CheckForConnections()`
- `DriveRuntimeSetupStateForClient()`
- `DropClientForHCDETimeout()`

### Priority 2 (Important)

- `FlushHCDEReliableServices()`
- `BeginReliableHCDEPregameService()`
- `CommitReliableHCDEPregameService()`
- `AddClientConnection()`
- `RemoveClientConnection()`
- `FindTimedOutHCDEReliableService()`
- `ClearAckedHCDEReliableServices()`

### Priority 3 (Nice to Have)

- All helper functions
- All error handling paths

---

## 8. CONCLUSION

The HCDE netcode is functionally sound but needs attention to:
- **State consistency** under extreme load
- **Error handling** completeness
- **Code documentation** and **comments**
- **Unit test** coverage
- **Memory safety** guarantees for malformed traffic paths

The implementation is complex and already handles many edge cases, but malformed packet handling and queue-pressure telemetry are still the highest remaining risk areas.

---

**End of Netcode Review Document**
