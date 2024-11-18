#include <windows.h>
#include <iostream>
#include <fstream>

DWORD GetProcessIDByName(const char *processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hSnapshot, &pe32)) {
        return 0;
    }

    do {
        if (strcmp(pe32.szExeFile, processName) == 0) {
            CloseHandle(hSnapshot);
            return pe32.th32ProcessID;
        }
    } while (Process32Next(hSnapshot, &pe32));

    CloseHandle(hSnapshot);
    return 0;
}


// Helper to load a DLL into a process using Manual Mapping
bool ManualMap(HANDLE hProcess, const char* dllPath) {
    // Open DLL file
    std::ifstream file(dllPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open DLL file." << std::endl;
        return false;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    // Allocate memory for DLL
    BYTE* dllBuffer = new BYTE[fileSize];
    if (!dllBuffer) {
        std::cerr << "Memory allocation for DLL failed." << std::endl;
        return false;
    }

    // Read DLL into buffer
    file.read(reinterpret_cast<char*>(dllBuffer), fileSize);
    file.close();

    // Allocate memory in the target process
    void* allocatedMemory = VirtualAllocEx(hProcess, nullptr, fileSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!allocatedMemory) {
        std::cerr << "Memory allocation in target process failed." << std::endl;
        delete[] dllBuffer;
        return false;
    }

    // Write DLL to target process
    if (!WriteProcessMemory(hProcess, allocatedMemory, dllBuffer, fileSize, nullptr)) {
        std::cerr << "Writing to target process memory failed." << std::endl;
        VirtualFreeEx(hProcess, allocatedMemory, 0, MEM_RELEASE);
        delete[] dllBuffer;
        return false;
    }

    // Free local buffer
    delete[] dllBuffer;

    // Create a remote thread to execute the DLL entry point
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)allocatedMemory, nullptr, 0, nullptr);
    if (!hThread) {
        std::cerr << "Failed to create remote thread." << std::endl;
        VirtualFreeEx(hProcess, allocatedMemory, 0, MEM_RELEASE);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    std::cout << "DLL injected successfully." << std::endl;
    return true;
}

int main() {
    DWORD processId;
    char dllPath[MAX_PATH];
    processId = GetProcessIDByName("cs2.exe")
    char dllPath = "inject.dll";

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (!hProcess) {
        std::cerr << "Failed to open target process." << std::endl;
        return 1;
    }

    if (!ManualMap(hProcess, dllPath)) {
        std::cerr << "Manual map injection failed." << std::endl;
        CloseHandle(hProcess);
        return 1;
    }

    CloseHandle(hProcess);
    return 0;
}
