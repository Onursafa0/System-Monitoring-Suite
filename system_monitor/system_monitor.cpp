#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <iphlpapi.h>
#include <sqlite3.h> 

#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Iphlpapi.lib")

// Global variables to store previous system times and network byte counts
ULARGE_INTEGER prevTotalKernelTime = { 0 };
ULARGE_INTEGER prevTotalUserTime = { 0 };
ULONGLONG prevRxBytes = 0;
ULONGLONG prevTxBytes = 0;

// Converts bytes to megabytes
double BytesToMB(LONGLONG bytes)
{
    return static_cast<double>(bytes) / 1048576.0; 
}

// Retrieves the process name using the process ID
std::string GetProcessName(DWORD processID)
{
    CHAR szProcessName[MAX_PATH] = "<unknown>";
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

    if (hProcess)
    {
        HMODULE hMod;
        DWORD cbNeeded;

        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
        {
            GetModuleBaseNameA(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(CHAR));
        }
        CloseHandle(hProcess);
    }
    return std::string(szProcessName);
}

// Retrieves the memory usage of a process in megabytes
double GetProcessMemoryUsageMB(HANDLE hProcess)
{
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
    {
        return static_cast<double>(pmc.WorkingSetSize) / (1024 * 1024); // MB
    }
    return 0.0;
}

// Retrieves network statistics for all interfaces
void GetNetworkStats(ULONGLONG& rxBytes, ULONGLONG& txBytes)
{
    MIB_IFTABLE* pIfTable = NULL;
    DWORD dwSize = 0;

    if (GetIfTable(pIfTable, &dwSize, TRUE) == ERROR_INSUFFICIENT_BUFFER)
    {
        pIfTable = (MIB_IFTABLE*)malloc(dwSize);
        if (pIfTable == NULL)
        {
            std::cerr << "Memory allocation failed.\n";
            return;
        }
    }

    if (GetIfTable(pIfTable, &dwSize, TRUE) == NO_ERROR)
    {
        for (DWORD i = 0; i < pIfTable->dwNumEntries; ++i)
        {
            rxBytes += pIfTable->table[i].dwInOctets;
            txBytes += pIfTable->table[i].dwOutOctets;
        }
    }
    else
    {
        std::cerr << "GetIfTable failed.\n";
    }

    if (pIfTable)
    {
        free(pIfTable);
    }
}

// Executes an SQL statement
void ExecuteSQL(sqlite3* db, const std::string& sql)
{
    char* errMsg = nullptr;
    if (sqlite3_exec(db, sql.c_str(), 0, 0, &errMsg) != SQLITE_OK)
    {
        std::cerr << "SQL error: " << errMsg << "\n";
        sqlite3_free(errMsg);
    }
}

// Creates tables for storing process and network data in the database
void CreateTables(sqlite3* db)
{
    const std::string createProcessTable = R"(
        CREATE TABLE IF NOT EXISTS ProcessData (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT,
            pid INTEGER,
            cpu_usage REAL,
            ram_usage REAL,
            read_count REAL,
            write_count REAL,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";
    ExecuteSQL(db, createProcessTable);

    const std::string createNetworkTable = R"(
        CREATE TABLE IF NOT EXISTS NetworkData (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            download REAL,
            upload REAL,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )";
    ExecuteSQL(db, createNetworkTable);
}

// Inserts a new row into the ProcessData table
void InsertProcessData(sqlite3* db, const std::string& name, DWORD pid, double cpuUsage, double ramUsage, double readCount, double writeCount)
{
    std::string sql = "INSERT INTO ProcessData (name, pid, cpu_usage, ram_usage, read_count, write_count) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, pid);
        sqlite3_bind_double(stmt, 3, cpuUsage);
        sqlite3_bind_double(stmt, 4, ramUsage);
        sqlite3_bind_double(stmt, 5, readCount);
        sqlite3_bind_double(stmt, 6, writeCount);

        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            std::cerr << "Error inserting process data\n";
        }
    }
    else
    {
        std::cerr << "Error preparing SQL statement\n";
    }
    sqlite3_finalize(stmt);
}

// Inserts a new row into the NetworkData table
void InsertNetworkData(sqlite3* db, double download, double upload)
{
    std::string sql = "INSERT INTO NetworkData (download, upload) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_double(stmt, 1, download);
        sqlite3_bind_double(stmt, 2, upload);

        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            std::cerr << "Error inserting network data\n";
        }
    }
    else
    {
        std::cerr << "Error preparing SQL statement\n";
    }
    sqlite3_finalize(stmt);
}

// Calculates CPU usage for all processes and logs the data
void CalculateCPUUsage(ULARGE_INTEGER& prevKernelTime, ULARGE_INTEGER& prevUserTime, std::ofstream& logFile, sqlite3* db)
{
    FILETIME ftIdle, ftKernel, ftUser;
    GetSystemTimes(&ftIdle, &ftKernel, &ftUser);

    ULARGE_INTEGER totalKernelTime, totalUserTime;
    totalKernelTime.LowPart = ftKernel.dwLowDateTime;
    totalKernelTime.HighPart = ftKernel.dwHighDateTime;
    totalUserTime.LowPart = ftUser.dwLowDateTime;
    totalUserTime.HighPart = ftUser.dwHighDateTime;

    // Calculate the difference in system times since the last check
    ULONGLONG kernelDiff = totalKernelTime.QuadPart - prevKernelTime.QuadPart;
    ULONGLONG userDiff = totalUserTime.QuadPart - prevUserTime.QuadPart;

    prevKernelTime = totalKernelTime;
    prevUserTime = totalUserTime;

    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE)
    {
        std::cerr << "CreateToolhelp32Snapshot failed.\n";
        return;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32))
    {
        std::cerr << "Process32First failed.\n";
        CloseHandle(hProcessSnap);
        return;
    }

    ULONGLONG totalSystemTime = (kernelDiff + userDiff) / 10000;
    if (totalSystemTime == 0)
    {
        std::cerr << "Total system time is zero, unable to calculate CPU usage.\n";
        CloseHandle(hProcessSnap);
        return;
    }

    std::wcout << std::left
        << std::setw(45) << L"Process Name"
        << std::setw(10) << L"PID"
        << std::setw(20) << L"CPU Usage (%)"
        << std::setw(20) << L"RAM Usage (MB)"
        << std::setw(20) << L"Read Count (MB)"
        << std::setw(20) << L"Write Count (MB)" << std::endl;

    std::wcout << std::wstring(125, L'=') << std::endl;

    // Iterate over all processes and gather CPU and memory usage
    do
    {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);

        if (hProcess)
        {
            FILETIME creationTime, exitTime, kernelTime, userTime;
            if (GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime))
            {
                ULARGE_INTEGER processKernelTime, processUserTime;
                processKernelTime.LowPart = kernelTime.dwLowDateTime;
                processKernelTime.HighPart = kernelTime.dwHighDateTime;
                processUserTime.LowPart = userTime.dwLowDateTime;
                processUserTime.HighPart = userTime.dwHighDateTime;

                ULONGLONG processTime = (processKernelTime.QuadPart + processUserTime.QuadPart) / 10000; // Convert to milliseconds

                double cpuUsage = (totalSystemTime > 0) ? (static_cast<double>(processTime) / totalSystemTime) * 100.0 : 0.0;
                cpuUsage = (cpuUsage > 100.0) ? 100.0 : cpuUsage;

                double ramUsageMB = GetProcessMemoryUsageMB(hProcess);

                IO_COUNTERS ioCounters;
                if (GetProcessIoCounters(hProcess, &ioCounters))
                {
                    std::string processName = GetProcessName(pe32.th32ProcessID);
                    std::wcout << std::left
                        << std::setw(45) << std::wstring(processName.begin(), processName.end())
                        << std::setw(15) << pe32.th32ProcessID
                        << std::setw(20) << cpuUsage
                        << std::setw(20) << std::fixed << std::setprecision(2) << ramUsageMB
                        << std::setw(20) << BytesToMB(ioCounters.ReadTransferCount)
                        << std::setw(20) << BytesToMB(ioCounters.WriteTransferCount)
                        << std::endl;

                    // Log process information to the file
                    logFile << std::left
                        << std::setw(45) << processName
                        << std::setw(15) << pe32.th32ProcessID
                        << std::setw(20) << cpuUsage
                        << std::setw(20) << std::fixed << std::setprecision(2) << ramUsageMB
                        << std::setw(20) << BytesToMB(ioCounters.ReadTransferCount)
                        << std::setw(20) << BytesToMB(ioCounters.WriteTransferCount)
                        << std::endl;

                    // Insert process data into the database
                    InsertProcessData(db, processName, pe32.th32ProcessID, cpuUsage, ramUsageMB, BytesToMB(ioCounters.ReadTransferCount), BytesToMB(ioCounters.WriteTransferCount));
                }
                else
                {
                    std::cerr << "Warning: Could not retrieve IO counters for process ID " << pe32.th32ProcessID << "." << std::endl;
                }
            }
            else
            {
                std::cerr << "GetProcessTimes failed.\n";
            }
            CloseHandle(hProcess);
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
}

int main()
{
    std::ofstream logFile("process_log.txt");

    // Write headers to the log file
    logFile << std::left
        << std::setw(45) << "Process Name"
        << std::setw(15) << "PID"
        << std::setw(20) << "CPU Usage (%)"
        << std::setw(20) << "RAM Usage (MB)"
        << std::setw(20) << "Read Count (MB)"
        << std::setw(20) << "Write Count (MB)" << std::endl;

    logFile << std::string(125, '=') << std::endl;

    // Initialize previous system times
    FILETIME ftIdle, ftKernel, ftUser;
    GetSystemTimes(&ftIdle, &ftKernel, &ftUser);
    prevTotalKernelTime.LowPart = ftKernel.dwLowDateTime;
    prevTotalKernelTime.HighPart = ftKernel.dwHighDateTime;
    prevTotalUserTime.LowPart = ftUser.dwLowDateTime;
    prevTotalUserTime.HighPart = ftUser.dwHighDateTime;

    // Open the SQLite database
    sqlite3* db;
    if (sqlite3_open("system_monitor.db", &db))
    {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << "\n";
        return 1;
    }
    // Create tables if they do not exist
    CreateTables(db);

    std::cout << "Waiting 15 seconds before the first update...\n";
    std::this_thread::sleep_for(std::chrono::seconds(15));

    const int updateInterval = 15000; // 15 seconds

    while (true)
    {
        system("cls"); // Clear console

        // Calculate and log CPU usage
        CalculateCPUUsage(prevTotalKernelTime, prevTotalUserTime, logFile, db);

        ULONGLONG currRxBytes = 0;
        ULONGLONG currTxBytes = 0;
        // Retrieve current network statistics
        GetNetworkStats(currRxBytes, currTxBytes);

        // Calculate network usage since the last check
        ULONGLONG rxBytesDiff = currRxBytes - prevRxBytes;
        ULONGLONG txBytesDiff = currTxBytes - prevTxBytes;

        std::cout << "Network Usage: "
            << "Download: " << rxBytesDiff / (1024 * 1024) << " MB"
            << " | Upload: " << txBytesDiff / (1024 * 1024) << " MB\n";

        logFile << "Network Usage: "
            << "Download: " << rxBytesDiff / (1024 * 1024) << " MB"
            << " | Upload: " << txBytesDiff / (1024 * 1024) << " MB\n";

        // Insert network data into the database
        InsertNetworkData(db, rxBytesDiff / (1024.0 * 1024.0), txBytesDiff / (1024.0 * 1024.0));

        // Update previous network byte counts
        prevRxBytes = currRxBytes;
        prevTxBytes = currTxBytes;

        std::cout << "Waiting 15 seconds before the next update...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(updateInterval));
    }

    logFile.close();
    sqlite3_close(db);
    return 0;
}
