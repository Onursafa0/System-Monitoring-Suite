# Windows System Performance Monitor

A lightweight C++ application for **monitoring Windows system performance**, including:

- Per-process CPU usage
- Per-process RAM usage
- Per-process I/O read/write counts
- Overall network upload and download usage

The program logs this data both to a text file (`process_log.txt`) and to a SQLite database (`system_monitor.db`) for further analysis.

---

## Features

- Retrieves process information (name, PID)
- Calculates CPU usage per process
- Retrieves RAM usage per process
- Tracks I/O read and write transfer counts per process
- Aggregates network interface upload/download data
- Logs data to a local SQLite database and a text log file
- Updates every 15 seconds, clearing the console for fresh output

---

## Prerequisites

- Windows OS (tested on Windows 10+)
- Visual Studio with C++ development tools
- SQLite3 library and headers (`sqlite3.lib`, `sqlite3.h`)

---

## Building the Project

### Using Visual Studio

1. **Install Visual Studio** with the **Desktop development with C++** workload.

2. **Download and install SQLite3 development files**:
   - Download precompiled binaries from [SQLite Download Page](https://www.sqlite.org/download.html).
   - Ensure `sqlite3.lib` and `sqlite3.h` are available and referenced by your project.

3. **Clone or download this repository**.

4. **Open Visual Studio** and create a new Win32 Console Application project.

5. **Add the source file (`.cpp`)** to the project.

6. **Link required libraries**:
   - Add `Psapi.lib`, `Iphlpapi.lib`, and `sqlite3.lib` to the linker input.

7. **Configure include directories** to point to SQLite headers.

8. **Build the project** in Release or Debug mode.

---

### Using MinGW (GCC)

1. **Install MinGW-w64** with C++ support and ensure `gcc` and `g++` are in your system PATH.

2. **Download SQLite3 development files**:
   - Obtain `sqlite3.dll`, `sqlite3.lib` (or `libsqlite3.a`), and `sqlite3.h`.
   - Place them in appropriate directories, for example:
     - Headers in `C:\mingw\include\`
     - Libraries in `C:\mingw\lib\`

3. **Clone or download this repository** and navigate to its directory in the terminal.

4. **Compile the program using g++** with the following command:

   ```bash
   g++ -o system_monitor.exe system_monitor.cpp -lpsapi -liphlpapi -lsqlite3 -static-libgcc -static-libstdc++ -mwindows
**Notes:**

- `-lpsapi` and `-liphlpapi` link Windows-specific libraries.

- `-lsqlite3` links the SQLite3 library.

- `-static-libgcc` and `-static-libstdc++` statically link runtime libraries to avoid DLL issues.

- `-mwindows` suppresses console window (optional; remove if you want console output).

5. **Run the executable** `system_monitor.exe` from the command line.
## Usage

- Run the compiled executable.
- The program will wait 15 seconds before the first data collection to initialize baseline measurements.
- Every 15 seconds, the console will update with current process CPU, RAM, and I/O stats, plus network usage.
- Data is simultaneously logged to:
  - `process_log.txt` (text file)
  - `system_monitor.db` (SQLite database)

---

## Database Schema

### ProcessData Table
| Column     | Type    | Description                     |
|------------|---------|---------------------------------|
| id         | INTEGER | Primary key (auto-increment)   |
| name       | TEXT    | Process name                   |
| pid        | INTEGER | Process ID                    |
| cpu_usage  | REAL    | CPU usage percentage          |
| ram_usage  | REAL    | RAM usage in megabytes        |
| read_count | REAL    | I/O Read bytes in megabytes   |
| write_count| REAL    | I/O Write bytes in megabytes  |
| timestamp  | DATETIME| Time of data recording        |

### NetworkData Table
| Column     | Type    | Description                      |
|------------|---------|---------------------------------|
| id         | INTEGER | Primary key (auto-increment)    |
| download   | REAL    | Download bytes in megabytes     |
| upload     | REAL    | Upload bytes in megabytes       |
| timestamp  | DATETIME| Time of data recording           |

---

## Notes

- The program must be run with sufficient privileges to query process information.
- Network statistics are aggregated from all interfaces.
- The program clears the console screen every update, so logs should be checked in `process_log.txt` or `system_monitor.db`.
- The timing interval is fixed at 15 seconds but can be adjusted in the source (`updateInterval`).

---