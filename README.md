# FIDO2 Token Management Tool (C++ Version)

> **Note**  
> This tool is a complete rewrite of our PowerShell-based FIDO2 management tool, designed to address reliability and compatibility issues stemming from frequent antivirus false positives associated with PowerShell executables. While the new tool is harder to compile due to its C++ codebase, it offers significant improvements in performance, security, and user experience.  

## Overview

The FIDO2 Token Management Tool (`fido2-manage.exe`) is a lightweight, standalone application for managing FIDO2 authentication tokens. Built with C++, this tool leverages the power and efficiency of a compiled binary while maintaining the user-centric functionality of the original tool.

![image](https://github.com/user-attachments/assets/0150a1b6-3907-4d58-8e22-b07579db7b25)

This new implementation directly integrates with the `libfido2` library, providing seamless access to advanced token management features without the need for external scripting environments.

## Why the Rewrite?

The decision to rewrite the tool in C++ was driven by the following challenges and objectives:

### 1. **Antivirus Compatibility**
- PowerShell-based executables often trigger false positives from antivirus software, leading to unnecessary disruptions for users.
- A compiled C++ binary significantly reduces the likelihood of such false positives, ensuring smoother deployments.

### 2. **Performance and Efficiency**
- C++ provides a faster runtime and lower memory footprint compared to PowerShell.
- This improvement is especially noticeable in environments where multiple tokens are being managed concurrently.

### 3. **Enhanced Security**
- Eliminating dependencies on PowerShell mitigates potential vulnerabilities tied to scripting environments.
- The released exe files are signed with the Token2 Code Signing certificate, further enhancing trust and security.

 

## Key Features

- **Reliable Execution**: Minimizes antivirus-related disruptions.
- **User-Friendly Interface**: Retains human-readable device names and intuitive operation.
- **Efficient PIN Management**: Supports secure PIN input directly via the command line.
- **Standalone Operation**: Requires no additional scripting environments or runtime installations.

## Getting Started

### Prerequisites

- Windows operating system
- FIDO2 authentication device

### Installation

1. Download the latest release package .
2. Place the executable in a directory of your choice.
3. Ensure the tool is launched with administrative privileges for full functionality.

### Usage

Run the executable from the command line with the desired arguments or launch FIDO2.1Manager.exe to launch the GUI
