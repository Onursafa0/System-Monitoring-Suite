#include <iostream>
#include <string>
#include <limits>  
#include <sqlite3.h>
#include <vector>
#include <algorithm>
#include <map>

// List of critical applications
const std::vector<std::wstring> criticalApplications = {
    L"SecurityHealth", L"svchost.exe", L"MsMpEng.exe", L"winlogon.exe", L"csrss.exe",
    L"smss.exe", L"lsass.exe", L"services.exe", L"spoolsv.exe", L"explorer.exe",
    L"dwm.exe", L"taskhostw.exe", L"SearchIndexer.exe", L"ctfmon.exe", L"conhost.exe",
    L"audiodg.exe", L"wuauserv.exe", L"wininit.exe", L"System", L"Idle", L"msiexec.exe",
    L"lsm.exe", L"mmc.exe", L"Realtek", L"RTHDVCPL", L"RtkAudUService", L"igfxtray.exe",
    L"igfxpers.exe", L"hkcmd.exe", L"SynTPEnh.exe", L"SYNTP", L"SYNTPHelper",
    L"rundll32.exe", L"cmd.exe", L"powershell.exe", L"powershell_ise.exe", L"regedit.exe",
    L"taskmgr.exe", L"calc.exe", L"mstsc.exe", L"sihost.exe", L"LocationNotificationWindows.exe",
    L"SearchHost.exe", L"StartMenuExperienceHost.exe", L"RuntimeBroker.exe", L"DllHost.exe",
    L"TextInputHost.exe", L"SecurityHealthSystray.exe", L"ArcControlAssist.exe",
    L"HostAppServiceUpdater.exe", L"ApplicationFrameHost.exe", L"ShellExperienceHost.exe",
    L"SystemSettingsBroker.exe", L"smartscreen.exe", L"backgroundTaskHost.exe"
};

// Function to check if a process is a standard application
bool IsStandardApplication(const std::wstring& processName) {
    // If the process name is not in the list of critical applications, it is considered standard
    return std::find(criticalApplications.begin(), criticalApplications.end(), processName) == criticalApplications.end();
}

// Function to calculate average performance data for a given process
void CalculateProcessAverages(const std::string& processName, sqlite3* db) {
    std::string sql = "SELECT AVG(cpu_usage), AVG(ram_usage), AVG(read_count), AVG(write_count) FROM ProcessData WHERE name = ?;";
    sqlite3_stmt* stmt;

    // Prepare the SQL statement
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, processName.c_str(), -1, SQLITE_STATIC);

        // Execute the statement and read the results
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            double avgCpuUsage = sqlite3_column_double(stmt, 0);
            double avgRamUsage = sqlite3_column_double(stmt, 1);
            double avgReadCount = sqlite3_column_double(stmt, 2);
            double avgWriteCount = sqlite3_column_double(stmt, 3);

            std::cout << "Process Name: " << processName << std::endl;
            std::cout << "Average CPU Usage: " << avgCpuUsage << "%" << std::endl;
            std::cout << "Average RAM Usage: " << avgRamUsage << " MB" << std::endl;
            std::cout << "Average Read Count: " << avgReadCount << " MB" << std::endl;
            std::cout << "Average Write Count: " << avgWriteCount << " MB" << std::endl;
        } else {
            std::cout << "No data found for process: " << processName << std::endl;
        }
    } else {
        std::cerr << "Error preparing SQL statement: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);
}

// Function to calculate average network data
void CalculateNetworkAverages(sqlite3* db) {
    std::string sql = "SELECT AVG(download), AVG(upload) FROM NetworkData;";
    sqlite3_stmt* stmt;

    // Prepare the SQL statement
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        // Execute the statement and read the results
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            double avgDownload = sqlite3_column_double(stmt, 0);
            double avgUpload = sqlite3_column_double(stmt, 1);

            std::cout << "Average Network Usage:" << std::endl;
            std::cout << "Average Download: " << avgDownload << " MB" << std::endl;
            std::cout << "Average Upload: " << avgUpload << " MB" << std::endl;
        } else {
            std::cout << "No network data available." << std::endl;
        }
    } else {
        std::cerr << "Error preparing SQL statement: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);
}

// Function to list and select standard applications
void ListAndSelectStandardApplications(sqlite3* db) {
    std::string sql = "SELECT DISTINCT name FROM ProcessData;";
    sqlite3_stmt* stmt;
    std::map<int, std::string> applications;
    int index = 1;

    // Prepare the SQL statement
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        std::cout << "Standard Applications:" << std::endl;

        // Execute the statement and collect standard applications
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string processName(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
            std::wstring wProcessName(processName.begin(), processName.end());
            if (IsStandardApplication(wProcessName)) {
                applications[index++] = processName;
                std::cout << index - 1 << ". " << processName << std::endl;
            }
        }
    } else {
        std::cerr << "Error preparing SQL statement: " << sqlite3_errmsg(db) << std::endl;
    }
    sqlite3_finalize(stmt);

    if (applications.empty()) {
        std::cout << "No standard applications found." << std::endl;
        return;
    }

    int choice;
    std::cout << "Enter the number of the application to calculate averages: ";
    std::cin >> choice;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // Validate the user's choice and calculate averages for the selected application
    if (applications.find(choice) != applications.end()) {
        CalculateProcessAverages(applications[choice], db);
    } else {
        std::cerr << "Invalid choice." << std::endl;
    }
}

int main() {
    sqlite3* db;

    // Open the SQLite database
    if (sqlite3_open("system_monitor.db", &db)) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << "\n";
        return 1;
    }

    int choice;
    do {
        std::cout << "Select an option:\n";
        std::cout << "1. List and select standard applications\n";
        std::cout << "2. Calculate network averages\n";
        std::cout << "3. Exit\n";
        std::cout << "Enter your choice: ";
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        // Handle user choices
        if (choice == 1) {
            ListAndSelectStandardApplications(db);
        } else if (choice == 2) {
            CalculateNetworkAverages(db);
        } else if (choice == 3) {
            std::cout << "Exiting application." << std::endl;
        } else {
            std::cerr << "Invalid choice." << std::endl;
        }
    } while (choice != 3);

    // Close the database connection
    sqlite3_close(db);

    return 0;
}
