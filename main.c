#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    StatusSucceeded,
    StatusIncorrectUsage,
    StatusInvalidPid,
    StatusEnumProcessesFailed,
    StatusOpenProcessFailed,
    StatusProcessNotFound,
    StatusCrashProcessFailed,
} Status;

static void PrintUsage();
static BOOL IsPid(const char* argument);
static Status CrashProcessMatchesPid(const char* pid_string);
static Status CrashProcessesMatchName(const char* process_name);
static Status CrashProcessWithPid(DWORD pid, const char* name);
static BOOL CrashProcess(HANDLE process_handle);


int main(int argc, char** argv) {

    if (argc < 2) {
        PrintUsage();
        return StatusIncorrectUsage;
    }

    const char* argument = argv[1];
    if (IsPid(argument)) {
        return CrashProcessMatchesPid(argument);
    }
    else {
        return CrashProcessesMatchName(argument);
    }
}


static void PrintUsage() {

    printf(
        "Usage: \n"
        "    crasher <PID>\n"
        "        Crash the process whose ID matches <PID>.\n"
        "\n"
        "    crasher <ProcessName>\n"
        "        Crash all processes whose names match <ProcessName>.\n"
    );
}


static BOOL IsPid(const char* argument) {

    const char* current = argument;
    while (*current != 0) {

        if ((*current < '0') || ('9' < *current)) {
            return FALSE;
        }
        ++current;
    }
    return TRUE;
}


static Status CrashProcessMatchesPid(const char* pid_string) {

    int converted_pid = atoi(pid_string);
    if (converted_pid <= 0) {
        printf("Invalid PID: %s.\n", pid_string);
        return StatusInvalidPid;
    }

    return CrashProcessWithPid((DWORD)converted_pid, NULL);
}


static Status CrashProcessesMatchName(const char* process_name) {

    HANDLE snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot_handle == INVALID_HANDLE_VALUE) {
        printf("Fail to enumerate processes. Error: %u.\n", GetLastError());
        return StatusEnumProcessesFailed;
    }

    Status status = StatusProcessNotFound;

    PROCESSENTRY32 process_entry = { 0 };
    process_entry.dwSize = sizeof(process_entry);
    if (Process32First(snapshot_handle, &process_entry)) {
        do {

            if (_stricmp(process_entry.szExeFile, process_name) == 0) {

                Status crash_status = CrashProcessWithPid(process_entry.th32ProcessID, process_entry.szExeFile);
                if ((status == StatusSucceeded) || (status == StatusProcessNotFound)) {
                    status = crash_status;
                }
            }
        }
        while (Process32Next(snapshot_handle, &process_entry));
    }
    CloseHandle(snapshot_handle);

    if (status == StatusProcessNotFound) {
        printf("Process %s not found.\n", process_name);
    }

    return status;
}


static Status CrashProcessWithPid(DWORD pid, const char* name) {

    char process_name[MAX_PATH] = { 0 };
    if (name != NULL) {
        strcpy_s(process_name, MAX_PATH, name);
    }

    HANDLE process_handle = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE,
        pid);

    if (process_handle == NULL) {
        printf("Fail to open process %s(%u). Error: %u.\n", process_name, pid, GetLastError());
        return StatusOpenProcessFailed;
    }

    if (name == NULL) {
        HMODULE module_handle = NULL;
        DWORD returned_size = 0;
        BOOL is_succeeded = EnumProcessModules(process_handle, &module_handle, sizeof(module_handle), &returned_size);
        if (is_succeeded) {
            GetModuleBaseName(process_handle, module_handle, process_name, sizeof(process_name));
        }
    }

    printf("Try to crash process %s(%i)... ", process_name, pid);

    BOOL is_succeeded = CrashProcess(process_handle);
    CloseHandle(process_handle);

    if (is_succeeded) {
        printf("Done.\n");
        return StatusSucceeded;
    }
    else {
        printf("Error: %u.\n", GetLastError());
        return StatusCrashProcessFailed;
    }
}


static BOOL CrashProcess(HANDLE process_handle) {
    
    HANDLE thread_handle = CreateRemoteThread(process_handle, NULL, 0, 0, NULL, 0, NULL);
    if (thread_handle == NULL) {
        return FALSE;
    }

    CloseHandle(thread_handle);
    return TRUE;
}