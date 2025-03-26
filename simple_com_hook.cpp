#include <Windows.h>
#include <detours.h>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <vector>
#include <algorithm>

#pragma comment(lib, "detours.lib")

// Function prototypes for the Windows API functions we want to hook
static HANDLE(WINAPI* Real_CreateFileW)(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) = CreateFileW;

static BOOL(WINAPI* Real_ReadFile)(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
) = ReadFile;

static BOOL(WINAPI* Real_WriteFile)(
    HANDLE hFile,
    LPCVOID lpBuffer,
    DWORD nNumberOfBytesToWrite,
    LPDWORD lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped
) = WriteFile;

static BOOL(WINAPI* Real_CloseHandle)(
    HANDLE hObject
) = CloseHandle;

static HMODULE(WINAPI* Real_LoadLibraryW)(
    LPCWSTR lpLibFileName
) = LoadLibraryW;

static HMODULE(WINAPI* Real_LoadLibraryExW)(
    LPCWSTR lpLibFileName,
    HANDLE hFile,
    DWORD dwFlags
) = LoadLibraryExW;

// Global data structures
std::unordered_map<HANDLE, std::wstring> g_comPorts;
std::mutex g_mutex;
std::ofstream g_logFile;
std::filesystem::path g_logDir;
std::filesystem::path g_captureDir;
bool g_initialized = false;
std::vector<std::wstring> g_loadedDlls;
HMODULE g_tech2DllModule = NULL;

// Utility functions
void InitializeLogger() {
    if (g_initialized) return;
    
    // Create directories if they don't exist
    g_logDir = std::filesystem::current_path() / "logs";
    g_captureDir = std::filesystem::current_path() / "captured_data";
    
    try {
        if (!std::filesystem::exists(g_logDir)) {
            std::filesystem::create_directory(g_logDir);
        }
        
        if (!std::filesystem::exists(g_captureDir)) {
            std::filesystem::create_directory(g_captureDir);
        }
    }
    catch (const std::exception& e) {
        // If we can't create directories in current path, try using temp directory
        auto tempPath = std::filesystem::temp_directory_path();
        g_logDir = tempPath / "tech2_hook" / "logs";
        g_captureDir = tempPath / "tech2_hook" / "captured_data";
        
        std::filesystem::create_directories(g_logDir);
        std::filesystem::create_directories(g_captureDir);
    }
    
    // Create log file with timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time);
    
    std::ostringstream logFileName;
    logFileName << g_logDir.string() << "\\com_hook_" 
                << std::put_time(&tm, "%Y%m%d_%H%M%S") 
                << ".log";
    
    g_logFile.open(logFileName.str());
    
    if (g_logFile.is_open()) {
        g_logFile << "COM Port Hook DLL - Started at " 
                  << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") 
                  << std::endl;
        g_logFile << "Process ID: " << GetCurrentProcessId() << std::endl;
        g_logFile << "Process Name: ";

        // Get process name
        wchar_t processPath[MAX_PATH] = { 0 };
        if (GetModuleFileNameW(NULL, processPath, MAX_PATH)) {
            std::wstring path = processPath;
            size_t lastBackslash = path.find_last_of(L'\\');
            if (lastBackslash != std::wstring::npos) {
                std::wstring processName = path.substr(lastBackslash + 1);
                
                // Convert to narrow string for logging
                int size = WideCharToMultiByte(CP_UTF8, 0, processName.c_str(), -1, NULL, 0, NULL, NULL);
                if (size > 0) {
                    std::string narrowName(size, 0);
                    WideCharToMultiByte(CP_UTF8, 0, processName.c_str(), -1, &narrowName[0], size, NULL, NULL);
                    g_logFile << narrowName;
                }
            }
        }
        
        g_logFile << std::endl;
        g_logFile << "------------------------------------" << std::endl;
        g_logFile.flush();
        
        g_initialized = true;
    }
}

void Log(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_mutex);
    
    if (!g_initialized) {
        InitializeLogger();
    }
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time);
    
    if (g_logFile.is_open()) {
        g_logFile << "[" << std::put_time(&tm, "%H:%M:%S") << "] " 
                  << message << std::endl;
        g_logFile.flush();
    }
    
    // Debug output
    OutputDebugStringA(("[COM Hook] " + message).c_str());
}

std::string HexDump(const void* data, size_t size) {
    if (!data || size == 0) return "";
    
    const unsigned char* buffer = static_cast<const unsigned char*>(data);
    std::ostringstream ss;
    
    for (size_t i = 0; i < size; i += 16) {
        ss << std::setw(8) << std::setfill('0') << std::hex << i << ": ";
        
        // Hex values
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                ss << std::setw(2) << std::setfill('0') << std::hex 
                   << static_cast<int>(buffer[i + j]) << " ";
            } else {
                ss << "   ";
            }
        }
        
        ss << " | ";
        
        // ASCII representation
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                unsigned char c = buffer[i + j];
                if (c >= 32 && c <= 126) {
                    ss << c;
                } else {
                    ss << '.';
                }
            } else {
                ss << " ";
            }
        }
        
        ss << std::endl;
    }
    
    return ss.str();
}

void SaveCapturedData(const void* data, size_t size, const std::string& direction, HANDLE hFile) {
    if (!data || size == 0) return;
    
    std::lock_guard<std::mutex> lock(g_mutex);
    
    if (!g_initialized) {
        InitializeLogger();
    }
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm;
    localtime_s(&tm, &time);
    
    std::ostringstream fileName;
    fileName << g_captureDir.string() << "\\" << direction << "_" 
             << std::put_time(&tm, "%Y%m%d_%H%M%S") 
             << "_" << std::setfill('0') << std::setw(3) << ms.count()
             << "_handle_" << hFile << ".bin";
    
    try {
        std::ofstream outFile(fileName.str(), std::ios::binary);
        if (outFile.is_open()) {
            outFile.write(static_cast<const char*>(data), size);
            outFile.close();
            
            Log("Saved " + std::to_string(size) + " bytes to " + fileName.str());
        }
    }
    catch (const std::exception& e) {
        Log("Error saving data: " + std::string(e.what()));
    }
}

// Utility function to convert wide string to narrow string
std::string WideToNarrow(const std::wstring& wide) {
    if (wide.empty()) return "";
    
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, NULL, 0, NULL, NULL);
    if (size <= 0) return "";
    
    std::string narrow(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &narrow[0], size, NULL, NULL);
    
    // Remove null terminator if any
    if (!narrow.empty() && narrow.back() == '\0') {
        narrow.pop_back();
    }
    
    return narrow;
}

// Function to get module information
void LogModuleInfo(HMODULE hModule, const std::wstring& moduleName) {
    if (!hModule) return;
    
    std::wstring fullPath;
    wchar_t path[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(hModule, path, MAX_PATH)) {
        fullPath = path;
    }
    
    Log("Loaded DLL: " + WideToNarrow(moduleName));
    Log("Full path: " + WideToNarrow(fullPath));
    
    // Store module if it's tech2.dll
    std::wstring lowerName = moduleName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
    
    if (lowerName.find(L"tech2.dll") != std::wstring::npos) {
        g_tech2DllModule = hModule;
        Log("*** TECH2.DLL DETECTED ***");
        Log("tech2.dll loaded at address: " + std::to_string((uintptr_t)hModule));
    }
}

// Hooked LoadLibraryW function
HMODULE WINAPI Hooked_LoadLibraryW(LPCWSTR lpLibFileName) {
    if (lpLibFileName) {
        std::wstring libName = lpLibFileName;
        size_t lastBackslash = libName.find_last_of(L'\\');
        std::wstring fileName = (lastBackslash != std::wstring::npos) ? 
                               libName.substr(lastBackslash + 1) : libName;
        
        std::wstring lowerName = fileName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
        
        if (lowerName.find(L"tech2") != std::wstring::npos || 
            lowerName.find(L"rs232") != std::wstring::npos ||
            lowerName.find(L"serial") != std::wstring::npos) {
            Log("LoadLibraryW called for: " + WideToNarrow(libName));
        }
    }
    
    // Call the original function
    HMODULE hModule = Real_LoadLibraryW(lpLibFileName);
    
    // If successfully loaded and it's a DLL we're interested in
    if (hModule && lpLibFileName) {
        std::wstring libName = lpLibFileName;
        std::wstring lowerName = libName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
        
        if (lowerName.find(L"tech2") != std::wstring::npos || 
            lowerName.find(L"rs232") != std::wstring::npos ||
            lowerName.find(L"serial") != std::wstring::npos) {
            
            size_t lastBackslash = libName.find_last_of(L'\\');
            std::wstring fileName = (lastBackslash != std::wstring::npos) ? 
                                  libName.substr(lastBackslash + 1) : libName;
            
            // Log module information
            LogModuleInfo(hModule, fileName);
            
            // Store in loaded DLLs list
            g_loadedDlls.push_back(libName);
        }
    }
    
    return hModule;
}

// Hooked LoadLibraryExW function
HMODULE WINAPI Hooked_LoadLibraryExW(
    LPCWSTR lpLibFileName,
    HANDLE hFile,
    DWORD dwFlags
) {
    if (lpLibFileName) {
        std::wstring libName = lpLibFileName;
        std::wstring lowerName = libName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
        
        if (lowerName.find(L"tech2") != std::wstring::npos || 
            lowerName.find(L"rs232") != std::wstring::npos ||
            lowerName.find(L"serial") != std::wstring::npos) {
            Log("LoadLibraryExW called for: " + WideToNarrow(libName));
        }
    }
    
    // Call the original function
    HMODULE hModule = Real_LoadLibraryExW(lpLibFileName, hFile, dwFlags);
    
    // If successfully loaded and it's a DLL we're interested in
    if (hModule && lpLibFileName) {
        std::wstring libName = lpLibFileName;
        std::wstring lowerName = libName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
        
        if (lowerName.find(L"tech2") != std::wstring::npos || 
            lowerName.find(L"rs232") != std::wstring::npos ||
            lowerName.find(L"serial") != std::wstring::npos) {
            
            size_t lastBackslash = libName.find_last_of(L'\\');
            std::wstring fileName = (lastBackslash != std::wstring::npos) ? 
                                  libName.substr(lastBackslash + 1) : libName;
            
            // Log module information
            LogModuleInfo(hModule, fileName);
            
            // Store in loaded DLLs list
            g_loadedDlls.push_back(libName);
        }
    }
    
    return hModule;
}

// Function to identify calling module for trace info
std::string GetCallingModuleName() {
    HMODULE hModule = NULL;
    DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | 
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
                
    // Get module handle from return address
    void* returnAddress = _ReturnAddress();
    if (GetModuleHandleExW(flags, (LPCWSTR)returnAddress, &hModule)) {
        wchar_t modulePath[MAX_PATH] = { 0 };
        if (GetModuleFileNameW(hModule, modulePath, MAX_PATH)) {
            std::wstring path = modulePath;
            size_t lastBackslash = path.find_last_of(L'\\');
            if (lastBackslash != std::wstring::npos) {
                return WideToNarrow(path.substr(lastBackslash + 1));
            }
            return WideToNarrow(path);
        }
    }
    
    // If tech2.dll is loaded and this address is within its range, report it
    if (g_tech2DllModule) {
        MODULEINFO modInfo = { 0 };
        if (GetModuleInformation(GetCurrentProcess(), g_tech2DllModule, &modInfo, sizeof(modInfo))) {
            uintptr_t moduleStart = (uintptr_t)modInfo.lpBaseOfDll;
            uintptr_t moduleEnd = moduleStart + modInfo.SizeOfImage;
            uintptr_t returnAddr = (uintptr_t)returnAddress;
            
            if (returnAddr >= moduleStart && returnAddr < moduleEnd) {
                return "tech2.dll";
            }
        }
    }
    
    return "unknown";
}

// Hooked function implementations
HANDLE WINAPI Hooked_CreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    // Call the original function
    HANDLE hFile = Real_CreateFileW(
        lpFileName,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile
    );
    
    // Check if it's a COM port
    if (lpFileName) {
        std::wstring fileName = lpFileName;
        
        if ((fileName.find(L"COM") != std::wstring::npos || 
             fileName.find(L"\\\\.\\") != std::wstring::npos) && 
            hFile != INVALID_HANDLE_VALUE) {
            
            std::lock_guard<std::mutex> lock(g_mutex);
            g_comPorts[hFile] = fileName;
            
            // Get calling module for better tracing
            std::string callingModule = GetCallingModuleName();
            
            Log("COM port opened: " + WideToNarrow(fileName) + 
                ", handle: " + std::to_string((uintptr_t)hFile) + 
                ", called by: " + callingModule);
        }
    }
    
    return hFile;
}

BOOL WINAPI Hooked_ReadFile(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
) {
    // Check if this is a COM port handle we're tracking
    bool isComPort = false;
    std::string portName;
    
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_comPorts.find(hFile);
        if (it != g_comPorts.end()) {
            isComPort = true;
            portName = WideToNarrow(it->second);
        }
    }
    
    if (isComPort) {
        // Get calling module for better tracing
        std::string callingModule = GetCallingModuleName();
        
        Log("ReadFile from " + portName + 
            ", buffer size: " + std::to_string(nNumberOfBytesToRead) + 
            ", called by: " + callingModule);
    }
    
    // Call the original function
    BOOL result = Real_ReadFile(
        hFile,
        lpBuffer,
        nNumberOfBytesToRead,
        lpNumberOfBytesRead,
        lpOverlapped
    );
    
    // If successful and it's a COM port, log the data
    if (result && isComPort && lpNumberOfBytesRead && *lpNumberOfBytesRead > 0) {
        DWORD bytesRead = *lpNumberOfBytesRead;
        
        std::string hexDump = HexDump(lpBuffer, bytesRead);
        Log("Read " + std::to_string(bytesRead) + " bytes from " + portName + ":\n" + hexDump);
        
        // Save the binary data
        SaveCapturedData(lpBuffer, bytesRead, "read", hFile);
    }
    
    return result;
}

BOOL WINAPI Hooked_WriteFile(
    HANDLE hFile,
    LPCVOID lpBuffer,
    DWORD nNumberOfBytesToWrite,
    LPDWORD lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped
) {
    // Check if this is a COM port handle we're tracking
    bool isComPort = false;
    std::string portName;
    
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_comPorts.find(hFile);
        if (it != g_comPorts.end()) {
            isComPort = true;
            portName = WideToNarrow(it->second);
        }
    }
    
    // If it's a COM port, log the data before writing
    if (isComPort) {
        // Get calling module for better tracing
        std::string callingModule = GetCallingModuleName();
        
        std::string hexDump = HexDump(lpBuffer, nNumberOfBytesToWrite);
        Log("Writing " + std::to_string(nNumberOfBytesToWrite) + 
            " bytes to " + portName + 
            ", called by: " + callingModule + "\n" + hexDump);
        
        // Save the binary data
        SaveCapturedData(lpBuffer, nNumberOfBytesToWrite, "write", hFile);
    }
    
    // Call the original function
    BOOL result = Real_WriteFile(
        hFile,
        lpBuffer,
        nNumberOfBytesToWrite,
        lpNumberOfBytesWritten,
        lpOverlapped
    );
    
    // If successful and it's a COM port, log the result
    if (isComPort) {
        DWORD bytesWritten = lpNumberOfBytesWritten ? *lpNumberOfBytesWritten : 0;
        Log("WriteFile result: " + std::to_string(result) + 
            ", bytes written: " + std::to_string(bytesWritten));
    }
    
    return result;
}

BOOL WINAPI Hooked_CloseHandle(
    HANDLE hObject
) {
    // Check if this is a COM port handle we're tracking
    bool isComPort = false;
    std::string portName;
    
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_comPorts.find(hObject);
        if (it != g_comPorts.end()) {
            isComPort = true;
            portName = WideToNarrow(it->second);
        }
    }
    
    if (isComPort) {
        // Get calling module for better tracing
        std::string callingModule = GetCallingModuleName();
        
        Log("Closing COM port: " + portName + 
            ", handle: " + std::to_string((uintptr_t)hObject) + 
            ", called by: " + callingModule);
    }
    
    // Call the original function
    BOOL result = Real_CloseHandle(hObject);
    
    // If successful and it's a COM port, remove it from our tracking
    if (result && isComPort) {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_comPorts.erase(hObject);
        Log("COM port closed");
    }
    
    return result;
}

// Function to log all currently loaded DLLs
void LogLoadedModules() {
    Log("Scanning for already loaded modules...");
    
    HMODULE hModules[1024];
    DWORD cbNeeded;
    
    if (EnumProcessModules(GetCurrentProcess(), hModules, sizeof(hModules), &cbNeeded)) {
        DWORD numModules = cbNeeded / sizeof(HMODULE);
        
        for (DWORD i = 0; i < numModules; i++) {
            wchar_t szModName[MAX_PATH];
            
            if (GetModuleFileNameExW(GetCurrentProcess(), hModules[i], szModName, sizeof(szModName) / sizeof(wchar_t))) {
                std::wstring modulePath = szModName;
                std::wstring fileName;
                
                size_t lastBackslash = modulePath.find_last_of(L'\\');
                if (lastBackslash != std::wstring::npos) {
                    fileName = modulePath.substr(lastBackslash + 1);
                } else {
                    fileName = modulePath;
                }
                
                std::wstring lowerName = fileName;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::towlower);
                
                if (lowerName.find(L"tech2") != std::wstring::npos || 
                    lowerName.find(L"rs232") != std::wstring::npos ||
                    lowerName.find(L"serial") != std::wstring::npos ||
                    lowerName.find(L"com") != std::wstring::npos) {
                    
                    Log("Found loaded module: " + WideToNarrow(fileName));
                    Log("  Path: " + WideToNarrow(modulePath));
                    
                    // Store tech2.dll module
                    if (lowerName.find(L"tech2.dll") != std::wstring::npos) {
                        g_tech2DllModule = hModules[i];
                        Log("*** TECH2.DLL ALREADY LOADED ***");
                        Log("tech2.dll loaded at address: " + std::to_string((uintptr_t)hModules[i]));
                    }
                }
            }
        }
    }
}

// DLL entry point
BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD ul_reason_for_call,
    LPVOID lpReserved
) {
    if (DetourIsHelperProcess()) {
        return TRUE;
    }
    
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH: {
            // Initialize logger
            InitializeLogger();
            Log("DLL attached to process");
            
            // Log information about already loaded modules
            LogLoadedModules();
            
            // Start transaction for hooking
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            
            // Attach detours to target functions
            DetourAttach(&(PVOID&)Real_CreateFileW, Hooked_CreateFileW);
            DetourAttach(&(PVOID&)Real_ReadFile, Hooked_ReadFile);
            DetourAttach(&(PVOID&)Real_WriteFile, Hooked_WriteFile);
            DetourAttach(&(PVOID&)Real_CloseHandle, Hooked_CloseHandle);
            DetourAttach(&(PVOID&)Real_LoadLibraryW, Hooked_LoadLibraryW);
            DetourAttach(&(PVOID&)Real_LoadLibraryExW, Hooked_LoadLibraryExW);
            
            // Commit the transaction
            LONG error = DetourTransactionCommit();
            if (error != NO_ERROR) {
                Log("Error attaching detours: " + std::to_string(error));
                return FALSE;
            }
            
            Log("Hooks installed successfully");
            
            // Don't need thread attach/detach notifications
            DisableThreadLibraryCalls(hModule);
            break;
        }
            
        case DLL_PROCESS_DETACH: {
            if (lpReserved != nullptr) {
                break;
            }
            
            // Start transaction for unhooking
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            
            // Detach hooks
            DetourDetach(&(PVOID&)Real_CreateFileW, Hooked_CreateFileW);
            DetourDetach(&(PVOID&)Real_ReadFile, Hooked_ReadFile);
            DetourDetach(&(PVOID&)Real_WriteFile, Hooked_WriteFile);
            DetourDetach(&(PVOID&)Real_CloseHandle, Hooked_CloseHandle);
            DetourDetach(&(PVOID&)Real_LoadLibraryW, Hooked_LoadLibraryW);
            DetourDetach(&(PVOID&)Real_LoadLibraryExW, Hooked_LoadLibraryExW);
            
            // Commit the transaction
            LONG error = DetourTransactionCommit();
            if (error != NO_ERROR) {
                Log("Error detaching detours: " + std::to_string(error));
            }
            
            Log("DLL detached from process");
            
            // Close log file
            if (g_logFile.is_open()) {
                g_logFile.close();
            }
            break;
        }
    }
    
    return TRUE;
} 