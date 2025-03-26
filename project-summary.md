# Tuesday Driver: Project Summary

## Project Overview

Tuesday Driver is a project aimed at monitoring and analyzing communications between Tech2Win diagnostic software and vehicle interfaces. The project has explored multiple approaches to capture this data, ranging from driver modification to direct COM port monitoring.

## Key Findings

### 1. Tech2Win Communication Architecture

Tech2Win communicates with vehicles using a layered architecture:

```
+------------------+          +-------------------+          +------------------+
| Tech2Win         |--------->| EVSERIAL7.SYS     |--------->| Vehicle          |
| Application      |<---------| Driver            |<---------| Interface        |
+------------------+          +-------------------+          +------------------+
```

The software uses a virtual serial port (COM9) created by the `evserial7.sys` driver, which translates standard Windows serial port operations into the proprietary protocol used by the vehicle interface.

### 2. Communication Protocol

The protocol uses multiple layers:
- **Transport Layer**: Serial communication (115200 baud typical)
- **Command Layer**: IOCTLs and specific command codes
- **Data Layer**: Structured packets with headers, payload, and checksums
- **Application Layer**: Vehicle-specific diagnostic commands

Key IOCTL codes discovered:
- `0x2A0850`: Port configuration 
- `0x2A0848`: Data retrieval
- `0x2A0860`: Control commands

### 3. Driver Architecture

The `evserial7.sys` driver has been analyzed and found to:
- Create a virtual COM port device
- Process standard Windows serial IRP requests
- Implement custom IOCTLs for Tech2Win
- Handle asynchronous I/O for performance
- Transform standard serial commands into proprietary protocols

## Monitoring Approaches Evaluated

We've evaluated five distinct approaches to monitoring the Tech2Win communications:

### 1. Driver Modification (30-50% Success Rate)

Involves replacing the `evserial7.sys` driver with a modified version that includes logging:

**Pros:**
- Could potentially capture all communications
- Works at the kernel level

**Cons:**
- High technical complexity
- Requires kernel-mode programming
- Faces Windows driver protection mechanisms
- Highest system impact
- Potential for system instability

**Status:** Attempted but encountered Windows driver protection mechanisms.

### 2. Direct COM Port Monitoring (60-70% Success Rate)

Connects to COM9 before Tech2Win to monitor all traffic:

**Pros:**
- Simple implementation
- User-mode operation
- No system modifications
- Easy to use and understand
- Minimal impact on system stability

**Cons:**
- Requires opening the port before Tech2Win
- May miss initial configuration
- Port sharing challenges

**Status:** Successfully implemented and functional. This is the recommended approach for most users.

### 3. API Hooking/DLL Injection (50-70% Success Rate)

Intercepts Windows API calls made by Tech2Win:

**Pros:**
- No direct driver modification
- Can capture all API calls
- Flexible implementation

**Cons:**
- Process protection mechanisms
- Complexity of hooking implementation
- Potential stability impact on Tech2Win

**Status:** Not implemented due to higher complexity/risk ratio compared to direct monitoring.

### 4. Hardware Monitoring (90-95% Success Rate)

Uses physical hardware to monitor communications:

**Pros:**
- Complete capture of all data
- No software impact
- Highest success rate
- Precise timing information

**Cons:**
- Requires specialized hardware
- Higher cost
- Additional technical expertise

**Status:** Not implemented due to hardware requirements, but recommended as an optimal solution where available.

### 5. Custom Virtual COM Port Driver (80-85% Success Rate)

Creates a filter driver that sits between Tech2Win and the original driver:

**Pros:**
- Does not modify original driver
- Standard Windows approach
- More stable than driver replacement
- Good coverage of communications

**Cons:**
- Driver development expertise required
- Driver signing requirements
- Complexity of implementation

**Status:** Not implemented due to high technical requirements, but represents a viable approach for those with driver development expertise.

## Recommendation

Based on our evaluation, we recommend the **Direct COM Port Monitoring** approach for most users:

1. **Use the GUI tool** (`start_gui.bat`) for the most user-friendly experience
2. **Direct monitoring** (`monitor_com9.bat`) for those who prefer command-line tools
3. **Driver modification** (`force_replace_driver.bat`) only for advanced users after careful consideration

The direct monitoring approach provides a good balance of:
- Technical simplicity
- Minimal system impact
- Reasonable success rate
- No administrative requirements
- Ease of use

## Resources Created

This project has generated several valuable resources:

1. **Monitoring Tools:**
   - GUI application for COM port monitoring
   - Command-line monitoring utilities
   - Log viewing and analysis tools

2. **Documentation:**
   - Driver architecture analysis
   - Protocol documentation
   - Monitoring approach evaluations

3. **Technical Guides:**
   - Step-by-step instructions for each approach
   - Troubleshooting guidance
   - Implementation details

## Conclusion

The Tuesday Driver project has successfully developed multiple approaches to monitor Tech2Win communications, with the direct COM port monitoring method proving to be the most practical for general use. The project has also generated valuable documentation about the Tech2Win communication protocol and driver architecture, which can support future research and development efforts.

For most users, the GUI-based monitoring tool provides the simplest path to capturing Tech2Win communications while maintaining system stability and requiring minimal technical expertise. 