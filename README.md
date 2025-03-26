# Tuesday Driver - Tech2Win Communication Monitor

This project provides tools to monitor and analyze communications between Tech2Win diagnostic software and vehicle interfaces. Multiple monitoring approaches are available, each with different technical characteristics and success rates.

## Project Overview

Tuesday Driver helps capture and analyze the communication data exchanged between Tech2Win and vehicle interfaces, enabling:

- Protocol analysis and understanding
- Debugging communication issues
- Validating correct operation
- Capturing data for further analysis

## Available Monitoring Methods

This project includes several monitoring methods with varying complexity and effectiveness:

### 1. Direct COM Port Monitoring (Recommended for Most Users)

The direct monitoring approach connects to COM9 before Tech2Win does, capturing all data without modifying system drivers.

**To use this method:**
```
.\monitor_com9.bat
```

Then start Tech2Win and connect to COM9. All communications will be captured to `tech2win_com9_log.txt`.

### 2. GUI-Based Monitoring Tool

For a more user-friendly experience, we provide a GUI application that supports multiple monitoring methods:

**To launch the GUI:**
```
.\start_gui.bat
```

The GUI provides:
- Direct COM port monitoring
- Log file viewing
- Data analysis tools
- Hex visualization

### 3. Driver Modification Approach

This advanced method replaces the original `evserial7.sys` driver with a modified version that includes logging capabilities.

**⚠️ Warning: This method modifies system drivers and may cause stability issues. Use with caution.**

To attempt driver replacement:
```
.\force_replace_driver.bat
```

To restore the original driver:
```
.\restore_driver.bat
```

## Project Structure

- `direct_monitor_com9.ps1` - PowerShell script for direct COM port monitoring
- `tech2_monitor_gui.py` - GUI application for monitoring and analysis
- `force_driver_replace.ps1` - Driver replacement script
- `monitoring-approaches.md` - Detailed technical analysis of monitoring approaches
- `protocol-notes.md` - Analysis of the Tech2Win communication protocol
- `driver-analysis.md` - Technical analysis of the evserial7.sys driver

## Choosing the Right Approach

| Approach | Success Rate | Technical Difficulty | System Impact | Recommended For |
|----------|--------------|----------------------|---------------|-----------------|
| Direct COM Port | 60-70% | Low | Minimal | Most users |
| GUI Tool | 60-70% | Low | Minimal | All users |
| Driver Modification | 30-50% | High | Significant | Advanced users only |

For most users, we recommend starting with the GUI tool or direct COM port monitoring as these methods are:
1. Non-invasive to the system
2. Easy to use
3. Do not require administrative privileges
4. Have a reasonable success rate

If you experience issues with these methods, please read `monitoring-approaches.md` for a detailed analysis of all possible monitoring techniques.

## Installation Requirements

- Windows 10 or later
- PowerShell 5.1 or higher
- For GUI: Python 3.6+ with tkinter
- Administrative privileges (for driver modification approach only)

## Quick Start

1. Clone or download this repository
2. Open a PowerShell terminal in the project directory
3. Run one of the following commands:
   - For direct monitoring: `.\monitor_com9.bat`
   - For GUI tool: `.\start_gui.bat`

## Troubleshooting

If monitoring is not capturing data:

1. Ensure Tech2Win is actively communicating with a vehicle interface
2. Try different baud rates (default is 115200, alternatives: 9600, 19200, 38400)
3. Ensure COM9 is the port used by Tech2Win
4. Check if Tech2Win is already running when you start monitoring
5. For driver approach: Windows protections may prevent driver replacement

## References

For detailed technical information, refer to:
- `monitoring-approaches.md` - Technical analysis of different monitoring methods
- `protocol-notes.md` - Analysis of the Tech2Win communication protocol
- `driver-analysis.md` - Technical analysis of the evserial7.sys driver
- `DIRECT_MONITOR_README.md` - Specific instructions for direct monitoring