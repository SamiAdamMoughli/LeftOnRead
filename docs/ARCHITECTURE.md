# Process Ghosting PoC Architecture

## Project Objective

Process Ghosting is an advanced file-less execution technique that bypasses modern EDR (Endpoint Detection and Response) solutions by exploiting how Windows handles file operations during process creation. This technique circumvents both "on-write" and "on-execution" scanning mechanisms by creating a file, marking it for deletion, writing the malicious payload, creating an image section from the file, and then executing that section. The critical advantage is that the file is deleted before the process actually starts, leaving forensic tools with no file to analyze while the malicious code executes from memory.

This PoC demonstrates how to leverage the Windows process creation mechanism to execute arbitrary code without leaving a persistent file on disk, effectively bypassing traditional file-based security controls and many EDR solutions that monitor file operations.

## The "Ghosting" Workflow

The Process Ghosting attack follows a precise sequence of operations to successfully bypass security controls:

1. **File Creation**
   - Create a new file using `CreateFile` with appropriate permissions
   - The file is initially empty and serves as a container for our payload

2. **Mark for Deletion**
   - Use `SetFileInformationByHandle` with `FileDispositionInfoEx` to mark the file for deletion
   - The file won't be actually deleted until all handles are closed

3. **Payload Writing**
   - Write the malicious PE payload to the file using `WriteFile`
   - Ensure proper PE headers and sections are written correctly

4. **Section Creation**
   - Create an image section from the file using `NtCreateSection`
   - This creates a memory section object backed by the file
   - The file handle must remain open during this operation

5. **Process Creation**
   - Create the process using `NtCreateProcessEx` with the previously created section
   - This creates a process object with the image section as its executable memory

6. **Handle Management**
   - Close the file handle, triggering the actual file deletion
   - The process continues to execute from the memory section despite the file being gone

7. **Thread Creation and Execution**
   - Create the primary thread in the process using `NtCreateThreadEx`
   - Set the context to point to the entry point of the loaded PE
   - Resume thread execution with `NtResumeThread`

The critical timing is maintaining the file handle until the section is created but closing it before process execution begins, ensuring the file is deleted when the process starts.

## Windows API Surface

| API/Native API | Purpose in Attack Chain | Key Parameters |
|---------------|------------------------|----------------|
| `CreateFile` | Creates the initial file that will host the payload | Desired access (GENERIC_WRITE), share mode (FILE_SHARE_READ), creation disposition (CREATE_ALWAYS) |
| `SetFileInformationByHandle` | Marks the file for deletion without actually deleting it | File handle, FileDispositionInfoEx class, delete flag |
| `WriteFile` | Writes the malicious PE payload to the file | File handle, buffer containing PE data, number of bytes to write |
| `NtCreateSection` | Creates a memory section object backed by the file | Section handle, desired access, object attributes, section size, protection flags, file handle |
| `NtCreateProcessEx` | Creates a process object using the section as the executable image | Process handle, desired access, object attributes, section handle, debug port, inherited handles |
| `NtCreateThreadEx` | Creates the primary thread in the new process | Thread handle, desired access, object attributes, process handle, start routine, start context |
| `NtResumeThread` | Resumes thread execution to begin payload execution | Thread handle |

## Handle Management

Proper handle management is critical to the success of Process Ghosting:

1. **File Handle Lifecycle**
   - The file handle must remain open from `CreateFile` until after `NtCreateSection` completes
   - The handle should be closed after `NtCreateProcessEx` but before thread execution begins
   - This timing ensures the file is deleted just as the process starts executing

2. **Section Handle Management**
   - The section handle created by `NtCreateSection` must be kept open until `NtCreateProcessEx` completes
   - The section object maintains a reference to the file even after the file handle is closed
   - The section handle can be closed after process creation is complete

3. **Process and Thread Handle Management**
   - Process and thread handles should be kept open until they're no longer needed
   - These handles don't affect the file deletion timing but are needed for process manipulation

4. **Error Handling**
   - All handle creation operations must include proper error checking
   - If any step fails, all existing handles must be properly closed before exiting

## Module Breakdown

### PayloadManager

- **Responsibility**: Manages the malicious payload and ensures it's properly formatted for execution
- **Key Functions**:
  - LoadPayloadFromBuffer: Loads payload from memory buffer
  - ValidatePEHeaders: Ensures the payload is a valid PE file
  - GetEntryPoint: Retrieves the entry point address from the PE headers

### FileGhoster

- **Responsibility**: Handles the file operations required for the ghosting technique
- **Key Functions**:
  - CreateGhostFile: Creates the file and marks it for deletion
  - WritePayloadToFile: Writes the payload to the ghost file
  - CloseGhostFile: Closes the file handle to trigger deletion

### SectionManager

- **Responsibility**: Manages the creation and manipulation of memory sections
- **Key Functions**:
  - CreateImageSection: Creates a section object from the ghost file
  - ConfigureSectionProtection: Sets appropriate memory protection flags

### ProcessExecutor

- **Responsibility**: Handles process creation and execution using the created section
- **Key Functions**:
  - CreateProcessFromSection: Creates a process from the image section
  - CreateProcessThread: Creates the primary thread in the new process
  - ExecutePayload: Sets up thread context and begins execution

### ErrorHandler

- **Responsibility**: Centralized error handling and cleanup
- **Key Functions**:
  - LogError: Logs errors with context
  - CleanupResources: Ensures proper cleanup of handles and resources

## Detection & Mitigation

### Potential Detection Methods

1. **Section Creation Monitoring**
   - Monitoring `NtCreateSection` calls with file handles that have deletion pending
   - Correlating section creation with subsequent file deletions

2. **Process Creation Anomalies**
   - Detecting processes created from sections with no corresponding file on disk
   - Monitoring for processes with PEB pointing to non-existent files

3. **Handle Analysis**
   - Detecting unusual patterns of file handles being opened, written to, marked for deletion, and then used for section creation

4. **Timing Analysis**
   - Identifying the narrow timing window between section creation and file deletion
   - Monitoring for rapid file creation, deletion, and process creation sequences

### Mitigation Techniques

1. **Kernel-Level Monitoring**
   - Implementing kernel drivers that monitor section creation and process creation at a low level
   - Tracking the relationship between sections and their backing files

2. **Memory Analysis**
   - Scanning process memory for known malicious patterns during execution
   - Analyzing the PEB and process parameters for inconsistencies

3. **Behavioral Analysis**
   - Monitoring process behavior rather than just file operations
   - Detecting suspicious process creation patterns regardless of file presence

4. **Handle Tracking**
   - Implementing comprehensive handle tracking to identify unusual handle usage patterns
   - Correlating handle operations with process creation events

## Build Requirements

### Headers

```cpp
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <tchar.h>
#include <iostream>
```

### Compiler Settings

- **Compiler**: Visual Studio 2019 or later, or compatible C++17 compiler
- **Platform**: x64 (required for modern Windows systems)
- **Runtime Linking**: Dynamic linking for Windows APIs
- **Character Set**: Unicode
- **Optimization**: Release build with optimizations enabled for production use

### Dependencies

- Windows SDK 10.0 or later
- No external dependencies beyond Windows system libraries

### Build Configuration

- Target Architecture: x64
- Configuration: Release
- Use of Native API functions requires proper structure definitions from winternl.h or custom definitions
