# Tech2Win COM Port Monitor

A tool to monitor COM port communications between Java applications and virtual ports.

## How It Works

This tool uses Microsoft Detours to hook Windows API functions related to COM port access:

1. **CreateFileW**: To detect when a COM port is opened
2. **ReadFile**: To capture data read from COM ports
3. **WriteFile**: To capture data written to COM ports
4. **CloseHandle**: To track when COM ports are closed

When injected into the Java process that hosts the Tech2Win application, the hook DLL intercepts these calls, logs the data, and saves binary captures for analysis.

## Building the Monitor

### Prerequisites

- Windows 10 or later
- Visual Studio with C++ support
- Administrator privileges (for DLL injection)

### Build Steps

1. Open a "Developer Command Prompt for Visual Studio"
2. Navigate to the project directory
3. Run `build_simple.bat`

This will:
- Download and build Microsoft Detours
- Compile the hooking DLL (com_hook.dll)
- Compile the DLL injector (dll_injector.exe)

## Usage

1. Start the Tech2Win application
2. Identify the Java process ID:
   - Use Task Manager (Details tab) or Process Explorer
   - Look for `java.exe` or `javaw.exe` processes
   - The correct process will be loading tech2.dll from a temporary file (visible in Process Explorer)

3. Inject the COM hook:
   ```
   dll_injector.exe <PID>
   ```
   where `<PID>` is the process ID of the Java process

4. The hook will:
   - Log all COM port activity to the `logs` directory
   - Save binary data to the `captured_data` directory

## Analyzing the Captured Data

The captured data files follow this naming pattern:
- `read_YYYYMMDD_HHMMSS_xxx_handle_XXXXXXXX.bin`: Data read from the COM port
- `write_YYYYMMDD_HHMMSS_xxx_handle_XXXXXXXX.bin`: Data written to the COM port

You can analyze these files with a hex editor or custom analysis tools.

## Troubleshooting

### Common Issues

1. **"Visual Studio environment not detected"**
   - Make sure to run `build_simple.bat` from a Developer Command Prompt for Visual Studio

2. **"Failed to build Detours library"**
   - Check that you have the necessary build tools (nmake)
   - Visual Studio might be missing C++ components

3. **"Failed to inject DLL"**
   - Verify you have administrator privileges
   - Confirm the target process ID is correct
   - Check that no antivirus is blocking the injection

4. **No data captured**
   - Ensure the Tech2Win application is actually using COM ports
   - The application might not be actively communicating

## Notes

- This tool is designed for educational and research purposes
- Use only on systems you own or have permission to monitor
- Some applications may have anti-debugging measures 