# Tech2Win Communication Protocol Analysis

This document provides analysis of the communication protocol used between Tech2Win and the virtual serial port driver based on observed behavior and decompiled code fragments.

## Protocol Overview

Tech2Win communicates with vehicle interfaces through a virtualized serial communication channel provided by the `evserial7.sys` driver. The protocol appears to be a layered structure:

1. **Transport Layer**: Standard serial communication over COM port
2. **Command Layer**: IOCTL-based control commands
3. **Data Layer**: Custom data packaging format
4. **Application Layer**: Vehicle-specific diagnostic commands

## Communication Sequence

Based on our analysis, the typical communication sequence follows this pattern:

```
+----------------+                      +----------------+                      +----------------+
| Tech2Win       |                      | evserial7.sys  |                      | Vehicle        |
| Application    |                      | Driver         |                      | Interface      |
+----------------+                      +----------------+                      +----------------+
        |                                       |                                       |
        | 1. Open COM Port                      |                                       |
        |-------------------------------------->|                                       |
        |                                       |                                       |
        | 2. Configure (IOCTL 0x2A0850)         |                                       |
        |-------------------------------------->|                                       |
        |                                       |                                       |
        | 3. Set Parameters (Standard IOCTLs)   |                                       |
        |-------------------------------------->|                                       |
        |                                       |                                       |
        | 4. Control Command (IOCTL 0x2A0860)   |                                       |
        |-------------------------------------->|                                       |
        |                                       | 5. Initialize Communication           |
        |                                       |-------------------------------------->|
        |                                       |                                       |
        | 6. Query Status (IOCTL 0x2A0848)      |                                       |
        |-------------------------------------->|                                       |
        |                                       | 7. Request Vehicle Information        |
        |                                       |-------------------------------------->|
        |                                       |                                       |
        |                                       | 8. Vehicle Information Response       |
        |                                       |<--------------------------------------|
        |                                       |                                       |
        | 9. Read Data (ReadFile)               |                                       |
        |-------------------------------------->|                                       |
        |                                       |                                       |
        | 10. Vehicle Data Response             |                                       |
        |<--------------------------------------|                                       |
        |                                       |                                       |
        | 11. Send Command (WriteFile)          |                                       |
        |-------------------------------------->|                                       |
        |                                       | 12. Forward Command                   |
        |                                       |-------------------------------------->|
        |                                       |                                       |
        | 13. Close COM Port                    |                                       |
        |-------------------------------------->|                                       |
        |                                       |                                       |
```

## Control Commands (IOCTLs)

Tech2Win uses both standard Windows serial port IOCTLs and custom IOCTLs specific to the virtual serial port driver:

### Standard Serial Port IOCTLs

| IOCTL | Purpose |
|-------|---------|
| `IOCTL_SERIAL_SET_BAUD_RATE` | Configure communication speed |
| `IOCTL_SERIAL_SET_LINE_CONTROL` | Set data bits, parity, stop bits |
| `IOCTL_SERIAL_SET_TIMEOUTS` | Configure read/write timeouts |
| `IOCTL_SERIAL_GET_MODEMSTATUS` | Read modem control lines |
| `IOCTL_SERIAL_SET_DTR` | Set Data Terminal Ready line |
| `IOCTL_SERIAL_CLR_DTR` | Clear Data Terminal Ready line |
| `IOCTL_SERIAL_SET_RTS` | Set Request To Send line |
| `IOCTL_SERIAL_CLR_RTS` | Clear Request To Send line |

### Custom IOCTLs

| IOCTL | Purpose | Buffer Format |
|-------|---------|---------------|
| `0x2A0850` | Configuration/Initialization | See below |
| `0x2A0848` | Data Retrieval/Status | See below |
| `0x2A0860` | Control Commands | See below |

#### IOCTL 0x2A0850 Configuration Buffer Format

This IOCTL configures the virtual serial port for Tech2Win's specific needs. The input buffer is typically 16 bytes with this structure:

```
+------+------+------+------+------+------+------+------+
| 0x00 | 0x01 | 0x02 | 0x03 | 0x04 | 0x05 | 0x06 | 0x07 |
+------+------+------+------+------+------+------+------+
| Mode |Config|Flags | Rsvd | Timeout Value | Buffer Sz |
+------+------+------+------+------+------+------+------+

+------+------+------+------+------+------+------+------+
| 0x08 | 0x09 | 0x0A | 0x0B | 0x0C | 0x0D | 0x0E | 0x0F |
+------+------+------+------+------+------+------+------+
| Protocol Version   | Device Type    | Features       |
+------+------+------+------+------+------+------+------+
```

- **Mode (0x00)**: Operating mode (0x01 = normal, 0x02 = bypass)
- **Config (0x01)**: Configuration flags
- **Flags (0x02)**: Feature flags
- **Reserved (0x03)**: Unused, typically 0
- **Timeout Value (0x04-0x05)**: Milliseconds for operations
- **Buffer Size (0x06-0x07)**: Internal buffer size
- **Protocol Version (0x08-0x09)**: Version of the communication protocol
- **Device Type (0x0A-0x0B)**: Type of vehicle interface
- **Features (0x0C-0x0F)**: Supported features bitmap

#### IOCTL 0x2A0848 Data Retrieval Buffer Format

This IOCTL queries status or retrieves data from the driver. The input buffer is typically 4 bytes:

```
+------+------+------+------+
| 0x00 | 0x01 | 0x02 | 0x03 |
+------+------+------+------+
| Type | Rsvd | Parameter  |
+------+------+------+------+
```

- **Type (0x00)**: Query type (0x01 = status, 0x02 = pending data, 0x03 = errors)
- **Reserved (0x01)**: Unused, typically 0
- **Parameter (0x02-0x03)**: Additional parameters for the query

The output buffer contains the response data based on the query type.

#### IOCTL 0x2A0860 Control Command Buffer Format

This IOCTL sends control commands to the driver. The input buffer is typically 24 bytes:

```
+------+------+------+------+------+------+------+------+
| 0x00 | 0x01 | 0x02 | 0x03 | 0x04 | 0x05 | 0x06 | 0x07 |
+------+------+------+------+------+------+------+------+
| Cmd  |SubCmd|Flags | Rsvd | Parameter 1    | Param 2  |
+------+------+------+------+------+------+------+------+

+------+------+------+------+------+------+------+------+
| 0x08 | 0x09 | 0x0A | 0x0B | 0x0C | 0x0D | 0x0E | 0x0F |
+------+------+------+------+------+------+------+------+
| Parameter 3         | Parameter 4         | Param 5   |
+------+------+------+------+------+------+------+------+

+------+------+------+------+------+------+------+------+
| 0x10 | 0x11 | 0x12 | 0x13 | 0x14 | 0x15 | 0x16 | 0x17 |
+------+------+------+------+------+------+------+------+
| Parameter 6         | Command Data...                 |
+------+------+------+------+------+------+------+------+
```

- **Command (0x00)**: Main command code
- **SubCommand (0x01)**: Sub-command code
- **Flags (0x02)**: Command flags
- **Reserved (0x03)**: Unused, typically 0
- **Parameters (0x04-0x0F)**: Command-specific parameters
- **Command Data (0x10-0x17)**: Additional command data

## Data Transmission Format

After the initial configuration using IOCTLs, Tech2Win primarily uses standard ReadFile and WriteFile operations for data exchange. The data format appears to be:

```
+------+------+------+------+------+------+------+------+
| 0x00 | 0x01 | 0x02 | 0x03 | 0x04 | 0x05 | .... | 0xNN |
+------+------+------+------+------+------+------+------+
| Len  | Type | SubTy| Seq  | Data Payload...           |
+------+------+------+------+------+------+------+------+
```

- **Length (0x00)**: Total length of the packet
- **Type (0x01)**: Packet type (0x01 = command, 0x02 = response, 0x03 = event)
- **SubType (0x02)**: Packet subtype
- **Sequence (0x03)**: Sequence number for matching requests/responses
- **Data Payload (0x04-0xNN)**: The actual data payload

## Vehicle-Specific Commands

Tech2Win appears to use different command sets depending on the vehicle being diagnosed. Based on observed patterns, these commands follow these general categories:

1. **Vehicle Identification Commands**: Retrieve VIN and ECU information
2. **Diagnostic Trouble Code Commands**: Read and clear DTCs
3. **Parameter Reading Commands**: Read sensor values and parameters
4. **Actuator Test Commands**: Activate components for testing
5. **Programming Commands**: Update ECU firmware or configurations

Each of these command categories uses specific payload structures that vary by vehicle manufacturer and model.

## Handshake Mechanism

The initial handshake between Tech2Win and the vehicle interface appears to follow this sequence:

1. Tech2Win sends a configuration command (IOCTL 0x2A0850)
2. Tech2Win sends several control commands (IOCTL 0x2A0860)
3. The driver acknowledges these commands
4. Tech2Win begins reading status information (IOCTL 0x2A0848)
5. Once the status indicates readiness, Tech2Win begins normal read/write operations

## Timing Characteristics

The protocol shows these timing characteristics:

1. Initial configuration commands are sent in rapid succession
2. There is typically a 50-100ms delay before the first data read attempt
3. Read operations may use timeouts of several seconds to wait for vehicle responses
4. Write operations are followed by immediate read attempts to check for acknowledgments
5. The protocol appears to use a request-response pattern with timeouts for recovery

## Error Handling

Error handling appears to use these mechanisms:

1. Timeout-based retries for failed commands
2. Specific error status codes returned in response packets
3. Status polling to recover from error conditions
4. Connection reset sequences for serious errors

## Security Mechanisms

The protocol appears to implement several security mechanisms:

1. Challenge-response authentication for some vehicle ECUs
2. Checksum validation for data integrity
3. Sequence numbering to prevent replay attacks
4. Timeout mechanisms to prevent indefinite blocking

## Conclusion

Tech2Win uses a multi-layered protocol that combines standard Windows I/O mechanisms with custom control commands and data formats. The protocol is designed to handle the complexities of vehicle diagnostics, including timing constraints, error recovery, and diverse vehicle interfaces.

Understanding this protocol structure is crucial for implementing effective monitoring solutions. The key challenges include capturing the full context of each communication, correctly interpreting the proprietary data formats, and maintaining proper timing relationships. 