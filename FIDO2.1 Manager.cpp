#include <windows.h>
#include <commctrl.h> // For ListView and related controls
#include <tchar.h>
#include <string>
#include <vector>
#include <sstream>
#include <regex>
#include "resource.h"
#pragma comment(lib, "Comctl32.lib") // Link the Common Controls library

#define ID_COMBOBOX 101
#define ID_LISTVIEW 102
#define ID_REFRESH_BUTTON 103
 
// Global string variable
std::wstring globalPin = L"111";
std::wstring deviceNumber = L"";
    
HINSTANCE hInst;
HWND hComboBox, hListView, hRefreshButton, hMainWindow, hwnd;

struct PasskeyInfo {
    std::wstring credentialId;
    std::wstring user;
};


// Function to execute a command and capture its output
std::vector<std::wstring> RunCommandAndGetOutput(const std::wstring& command) {
    std::vector<std::wstring> lines;
    HANDLE hPipeRead, hPipeWrite;

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hPipeRead, &hPipeWrite, &sa, 0)) {
        return lines;
    }

    STARTUPINFO si = { sizeof(STARTUPINFO) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.hStdOutput = hPipeWrite;
    si.hStdError = hPipeWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };

    if (CreateProcess(
        NULL,
        const_cast<LPWSTR>(command.c_str()),
        NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hPipeWrite);

        char buffer[1024];
        DWORD bytesRead;
        while (ReadFile(hPipeRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';

            int wideSize = MultiByteToWideChar(CP_OEMCP, 0, buffer, -1, NULL, 0);
            if (wideSize > 0) {
                std::wstring wideBuffer(wideSize, L'\0');
                MultiByteToWideChar(CP_OEMCP, 0, buffer, -1, &wideBuffer[0], wideSize);
                std::wstringstream ss(wideBuffer);
                std::wstring line;
                while (std::getline(ss, line)) {
                    lines.push_back(line);
                }
            }
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    CloseHandle(hPipeRead);
    return lines;
}


std::vector<PasskeyInfo> ParseResidentKeys(const std::vector<std::wstring>& output) {
    std::vector<PasskeyInfo> passkeys;

    for (const auto& line : output) {
        size_t credPos = line.find(L"Credential ID:");
        size_t userPos = line.find(L"User:");

        if (credPos != std::wstring::npos && userPos != std::wstring::npos) {
            PasskeyInfo info;
            info.credentialId = line.substr(credPos + 14, userPos - (credPos + 14));
            info.credentialId = info.credentialId.substr(info.credentialId.find_first_not_of(L" "));
            info.user = line.substr(userPos + 5);
            info.user = info.user.substr(info.user.find_first_not_of(L" "));
            passkeys.push_back(info);
        }
    }

    return passkeys;
}

std::vector<std::wstring> GetDomains(const std::wstring& deviceNumber, const std::wstring& globalPin) {
    // Command for the first run
    std::wstring command = L".\\fido2-manage.exe -residentkeys -device " + deviceNumber + L" -pin " + globalPin;

    // Debug: Show the command being executed
    //MessageBox(NULL, command.c_str(), L"Debug: Command Being Executed", MB_OK);

    std::vector<std::wstring> output = RunCommandAndGetOutput(command);

    // Debug: Check the raw output
    for (const auto& line : output) {
      //  MessageBox(NULL, line.c_str(), L"Debug: First Run Output", MB_OK);
    }

    // Extract domains (Users) from the output
    std::vector<std::wstring> domains;
    for (const auto& line : output) {
        size_t userPos = line.find(L"User:");
        if (userPos != std::wstring::npos) {
            std::wstring domain = line.substr(userPos + 5);
            domain = domain.substr(domain.find_first_not_of(L" "));
            domains.push_back(domain);
        }
    }

    return domains;
}

std::vector<PasskeyInfo> GetResidentKeysWithDomains(const std::wstring& deviceNumber, const std::wstring& globalPin, const std::vector<std::wstring>& domains) {
    std::vector<PasskeyInfo> passkeys;

    for (const auto& domain : domains) {
        // Command for the second run
        std::wstring command = L".\\fido2-manage.exe -residentkeys -device " + deviceNumber +
            L" -domain " + domain + L" -pin " + globalPin;
        std::vector<std::wstring> output = RunCommandAndGetOutput(command);

        // Debug: Check the raw output
        for (const auto& line : output) {
        //    MessageBox(NULL, line.c_str(), L"Debug: Second Run Output", MB_OK);
        }

        // Parse the results and add them to the list
        std::vector<PasskeyInfo> parsedPasskeys = ParseResidentKeys(output);
        passkeys.insert(passkeys.end(), parsedPasskeys.begin(), parsedPasskeys.end());
    }

    return passkeys;
}

void DisableAllButtons(HWND hwnd) {
    // Disable Passkeys button
    HWND hPasskeysButton = GetDlgItem(hwnd, ID_BTN_PASSKEYS);
    if (hPasskeysButton) {
        EnableWindow(hPasskeysButton, FALSE);
        RedrawWindow(hPasskeysButton, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    }

    // Disable Fingerprints button
    HWND hFingerprintButton = GetDlgItem(hwnd, ID_BTN_FINGERPRINT);
    if (hFingerprintButton) {
        EnableWindow(hFingerprintButton, FALSE);
        RedrawWindow(hFingerprintButton, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    }

    // Disable Change PIN button
    HWND hChangePinButton = GetDlgItem(hwnd, ID_BTN_CHANGEPIN);
    if (hChangePinButton) {
        EnableWindow(hChangePinButton, FALSE);
        RedrawWindow(hChangePinButton, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    }

    // Disable Enforce UV button
    HWND hEnforceUVButton = GetDlgItem(hwnd, ID_BTN_ENFORCEUV);
    if (hEnforceUVButton) {
        EnableWindow(hEnforceUVButton, FALSE);
        RedrawWindow(hEnforceUVButton, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    }

    // Disable Reset button
    HWND hResetButton = GetDlgItem(hwnd, ID_BTN_RESET);
    if (hResetButton) {
        EnableWindow(hResetButton, FALSE);
        RedrawWindow(hResetButton, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    }
}

void PopulatePasskeysList(HWND hListView, const std::vector<PasskeyInfo>& passkeys) {
    for (size_t i = 0; i < passkeys.size(); ++i) {
        const PasskeyInfo& pk = passkeys[i];

        LVITEM lvItem = { 0 };
        lvItem.mask = LVIF_TEXT;
        lvItem.iItem = static_cast<int>(i);

        // Add Credential ID
        lvItem.pszText = const_cast<LPWSTR>(pk.credentialId.c_str());
        SendMessage(hListView, LVM_INSERTITEM, 0, (LPARAM)&lvItem);

        // Add User
        lvItem.iSubItem = 1;
        lvItem.pszText = const_cast<LPWSTR>(pk.user.c_str());
        SendMessage(hListView, LVM_SETITEM, 0, (LPARAM)&lvItem);
    }
}
 

INT_PTR CALLBACK PasskeysDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static HWND hListView;
    static std::vector<PasskeyInfo>* passkeys = nullptr;

    switch (message) {
    case WM_INITDIALOG: {
        hListView = GetDlgItem(hDlg, IDC_LISTVIEW);

        // Initialize ListView columns
        LVCOLUMN lvColumn = { 0 };
        lvColumn.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

        lvColumn.cx = 150;
        lvColumn.pszText = const_cast<LPWSTR>(L"Credential ID");
        SendMessage(hListView, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

        lvColumn.cx = 300;
        lvColumn.pszText = const_cast<LPWSTR>(L"User");
        SendMessage(hListView, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);

        // Store the initial passkeys pointer
        passkeys = reinterpret_cast<std::vector<PasskeyInfo>*>(lParam);

        // Populate the ListView with initial data
        PopulatePasskeysList(hListView, *passkeys);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_REFRESH: {

            // Set the wait cursor
            HCURSOR hWaitCursor = LoadCursor(NULL, IDC_WAIT);
            SetCursor(hWaitCursor);
            // Process pending messages to update the cursor
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            // Clear the ListView
            SendMessage(hListView, LVM_DELETEALLITEMS, 0, 0);

          

            // Step 1: First Run - Fetch domains
            std::wstring firstCommand = L".\\fido2-manage.exe -residentkeys -device " + deviceNumber + L" -pin " + globalPin;
            std::vector<std::wstring> firstOutput = RunCommandAndGetOutput(firstCommand);

            // Parse the output to get domains (users)
            std::vector<std::wstring> domains;
            for (const auto& line : firstOutput) {
                size_t userPos = line.find(L"User:");
                if (userPos != std::wstring::npos) {
                    std::wstring domain = line.substr(userPos + 5);
                    domain = domain.substr(domain.find_first_not_of(L" ")); // Trim leading spaces
                    domains.push_back(domain);
                }
            }

            // Debug: Display the parsed domains
            for (const auto& domain : domains) {
             //   MessageBox(hDlg, domain.c_str(), L"Debug: Parsed Domain", MB_OK);
            }

            // Step 2: Second Run - Fetch resident keys for each domain
            std::vector<PasskeyInfo> refreshedPasskeys;
            for (const auto& domain : domains) {
                std::wstring secondCommand = L".\\fido2-manage.exe -residentkeys -device " + deviceNumber +
                    L" -domain " + domain + L" -pin " + globalPin;
                std::vector<std::wstring> secondOutput = RunCommandAndGetOutput(secondCommand);

                // Parse the second run output
                std::vector<PasskeyInfo> parsedPasskeys = ParseResidentKeys(secondOutput);
                refreshedPasskeys.insert(refreshedPasskeys.end(), parsedPasskeys.begin(), parsedPasskeys.end());
            }

            // Debug: Display parsed passkeys
            for (const auto& pk : refreshedPasskeys) {
              //  MessageBox(hDlg, (L"Credential ID: " + pk.credentialId + L"\nUser: " + pk.user).c_str(), L"Debug: Parsed Passkey", MB_OK);
            }

            // Step 3: Populate the ListView with the complete data
            PopulatePasskeysList(hListView, refreshedPasskeys);

            // Update the global passkeys pointer
            *passkeys = refreshedPasskeys;

            // Restore the default cursor
            HCURSOR hArrowCursor = LoadCursor(NULL, IDC_ARROW);
            SetCursor(hArrowCursor);

            return TRUE;
        }

        case IDC_BTN_SHOW_SELECTED: {
            // Existing logic to handle the selected row
            int selectedRow = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
            if (selectedRow == -1) {
                MessageBox(hDlg, L"No row selected.", L"Error", MB_OK | MB_ICONERROR);
                return TRUE;
            }

            wchar_t buffer[256] = { 0 };
            LVITEM lvItem = { 0 };
            lvItem.iItem = selectedRow;
            lvItem.iSubItem = 0; // First column
            lvItem.pszText = buffer;
            lvItem.cchTextMax = 256;

            if (SendMessage(hListView, LVM_GETITEMTEXT, selectedRow, (LPARAM)&lvItem) > 0) {
                std::wstring command = L".\\fido2-manage.exe -delete -device " + deviceNumber + L" -credential " + buffer;
                ShellExecute(NULL, L"open", L"cmd.exe", (L"/k " + command).c_str(), NULL, SW_SHOW);
            }
            else {
                MessageBox(hDlg, L"Failed to retrieve row content.", L"Error", MB_OK | MB_ICONERROR);
            }
            return TRUE;
        }

        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }
        break;
    }

    return FALSE;
}




 

std::wstring ShowInputBox(HWND hWnd, const std::wstring& title, const std::wstring& prompt) {
    static wchar_t buffer[256] = { 0 };
   
    // Show the input dialog
    DialogBoxParam(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(101), // ID of the dialog resource
        hWnd,
        [](HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) -> INT_PTR {
            switch (message) {
            case WM_INITDIALOG: {
                // Set the prompt text
                SetDlgItemText(hDlg, 1001, reinterpret_cast<LPCWSTR>(lParam));

                // Center the dialog on the screen
                RECT rcDlg, rcScreen;
                GetWindowRect(hDlg, &rcDlg);
                SystemParametersInfo(SPI_GETWORKAREA, 0, &rcScreen, 0); // Get the working area of the screen
                int x = (rcScreen.right - rcScreen.left - (rcDlg.right - rcDlg.left)) / 2;
                int y = (rcScreen.bottom - rcScreen.top - (rcDlg.bottom - rcDlg.top)) / 2;
                SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
                return TRUE;
            }

            case WM_COMMAND:
                if (LOWORD(wParam) == IDOK) {
                    GetDlgItemText(hDlg, 1002, buffer, 256); // Get input from the edit box
                    EndDialog(hDlg, IDOK);
                    return TRUE;
                }
                if (LOWORD(wParam) == IDCANCEL) {
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
                }
                break;
            }
            return FALSE;
        },
        reinterpret_cast<LPARAM>(prompt.c_str()));

    return buffer; // Return the user input
}

INT_PTR CALLBACK FingerprintDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static HWND hListView;

    switch (message) {
    case WM_INITDIALOG: {
        hListView = GetDlgItem(hDlg, IDC_LISTVIEW_FINGERPRINTS);

        // Initialize ListView columns
        LVCOLUMN lvColumn = { 0 };
        lvColumn.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

        lvColumn.cx = 100;
        lvColumn.pszText = const_cast<LPWSTR>(L"Fingerprint ID");
        SendMessage(hListView, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

        lvColumn.cx = 300;
        lvColumn.pszText = const_cast<LPWSTR>(L"Description");
        SendMessage(hListView, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);

        // Populate the ListView with fingerprints
        std::wstring command = L".\\fido2-manage.exe -fingerprintlist -device " + deviceNumber + L" -pin " + globalPin;
        std::vector<std::wstring> output = RunCommandAndGetOutput(command);

        for (const auto& line : output) {
            // Split the line into parts
            size_t colonPos = line.find(L":");
            size_t spacePos = line.find(L" ", colonPos + 2);

            if (colonPos != std::wstring::npos && spacePos != std::wstring::npos) {
                // Extract Index (first part)
                std::wstring index = line.substr(0, colonPos);
                index = index.substr(index.find_first_not_of(L" ")); // Trim spaces

                // Extract Fingerprint ID and Description (combined into second column)
                std::wstring fingerprintID_And_Description = line.substr(colonPos + 2); // From after ':'

                // Debug: Display the parsed data
                MessageBox(hDlg, (L"Index: " + index + L"\nData: " + fingerprintID_And_Description).c_str(), L"Debug: Parsed Data", MB_OK);

                // Add to the ListView
                LVITEM lvItem = { 0 };
                lvItem.mask = LVIF_TEXT;
                lvItem.iItem = static_cast<int>(SendMessage(hListView, LVM_GETITEMCOUNT, 0, 0));

                // Add Index to the first column
                wchar_t indexBuffer[256];
                wcscpy_s(indexBuffer, index.c_str());
                lvItem.pszText = indexBuffer;
                SendMessage(hListView, LVM_INSERTITEM, 0, (LPARAM)&lvItem);

                // Add Fingerprint ID and Description to the second column
                lvItem.iSubItem = 1;
                wchar_t dataBuffer[256];
                wcscpy_s(dataBuffer, fingerprintID_And_Description.c_str());
                lvItem.pszText = dataBuffer;
                SendMessage(hListView, LVM_SETITEM, 0, (LPARAM)&lvItem);
            }
            else {
                // Debug: Show lines that failed to parse
                MessageBox(hDlg, line.c_str(), L"Debug: Unparsed Line", MB_OK);
            }
        }

        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_ADD_FINGERPRINT: {
            // Implement Add Fingerprint functionality
           // Construct the add fingerprint command
            std::wstring addCommand = L".\\fido2-manage.exe -fingerprint -device " + deviceNumber;

            // Execute the command in a new console window
            ShellExecute(NULL, L"open", L"cmd.exe", (L"/k " + addCommand).c_str(), NULL, SW_SHOW);
            return TRUE;
        }

        case IDC_BTN_DELETE_FINGERPRINT: {
            // Get the selected row
            int selectedRow = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
            if (selectedRow == -1) {
                MessageBox(hDlg, L"No fingerprint selected.", L"Error", MB_OK | MB_ICONERROR);
                return TRUE;
            }

            // Get the content of the first column (Fingerprint ID)
            wchar_t buffer[256] = { 0 };
            LVITEM lvItem = { 0 };
            lvItem.iItem = selectedRow;
            lvItem.iSubItem = 0; // First column
            lvItem.pszText = buffer;
            lvItem.cchTextMax = 256;

            if (SendMessage(hListView, LVM_GETITEMTEXT, selectedRow, (LPARAM)&lvItem) > 0) {
                // Build the delete command
                std::wstring deleteCommand = L".\\fido2-manage.exe -deletefingerprint " + std::wstring(buffer) + L" -device " + std::wstring(deviceNumber);


                // Execute the delete command
                ShellExecute(NULL, L"open", L"cmd.exe", (L"/k " + deleteCommand).c_str(), NULL, SW_SHOW);
            }
            else {
                MessageBox(hDlg, L"Failed to retrieve fingerprint ID.", L"Error", MB_OK | MB_ICONERROR);
            }
            return TRUE;
        }
        }
        break;

    case WM_CLOSE:
        EndDialog(hDlg, 0);
        return TRUE;
    }

    return FALSE;
}


 

// Function declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


std::vector<std::wstring> RunCommandAndGetOutput(const std::wstring& command);
void PopulateComboBox();
void PopulateListView(const std::wstring& deviceNumber);
void ResizeControls(HWND hwnd);
void RefreshData();

// Entry point
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;

    // Register the window class
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = _T("DeviceInfoApp");
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    


    HWND hwnd = CreateWindowEx(
        WS_EX_APPWINDOW,            // Extended window styles
        _T("DeviceInfoApp"),        // Window class name
        _T("FIDO2.1 Manager"),      // Window title
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, // Window styles
        CW_USEDEFAULT, CW_USEDEFAULT, // Position
        600, 430,                   // Size
        NULL,                       // Parent window
        NULL,                       // Menu
        hInstance,                  // Instance handle
        NULL                        // Additional application data
    );

    if (!hwnd) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

#include <thread>

// Function to execute a command with a timeout
std::vector<std::wstring> RunCommandAndGetOutputWithTimeout(const std::wstring& command, DWORD timeoutMs) {
    std::vector<std::wstring> lines;
    HANDLE hPipeRead, hPipeWrite;

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hPipeRead, &hPipeWrite, &sa, 0)) {
        return lines;
    }

    STARTUPINFO si = { sizeof(STARTUPINFO) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.hStdOutput = hPipeWrite;
    si.hStdError = hPipeWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };

    if (CreateProcess(
        NULL,
        const_cast<LPWSTR>(command.c_str()),
        NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hPipeWrite);

        // Wait for the process to finish or timeout
        DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
        if (waitResult == WAIT_TIMEOUT) {
            // Timeout: Kill the process
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hPipeRead);
            return lines; // Return empty result on timeout
        }

        // Process completed within timeout, read output
        char buffer[1024];
        DWORD bytesRead;
        while (ReadFile(hPipeRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';

            int wideSize = MultiByteToWideChar(CP_OEMCP, 0, buffer, -1, NULL, 0);
            if (wideSize > 0) {
                std::wstring wideBuffer(wideSize, L'\0');
                MultiByteToWideChar(CP_OEMCP, 0, buffer, -1, &wideBuffer[0], wideSize);
                std::wstringstream ss(wideBuffer);
                std::wstring line;
                while (std::getline(ss, line)) {
                    lines.push_back(line);
                }
            }
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    CloseHandle(hPipeRead);
    return lines;
}



// Populate the ComboBox with output from `fido2-manage.exe -list`
void PopulateComboBox() {
    std::vector<std::wstring> devices = RunCommandAndGetOutput(L".\\fido2-manage.exe -list");

    for (const auto& device : devices) {
        SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)device.c_str());
    }
}

// Populate the ListView with output from `fido2-manage.exe -info`
void PopulateListView(HWND hwnd, const std::wstring& deviceNumber) {
    // Clear existing items
    SendMessage(hListView, LVM_DELETEALLITEMS, 0, 0);

    DisableAllButtons(hwnd);
    // Show the input box and store the result in globalPIN
    globalPin = ShowInputBox(NULL, L"Enter PIN", L"Please enter your PIN:");

    const size_t MIN_PIN_LENGTH = 4;

    if (globalPin.length() < MIN_PIN_LENGTH) {
        // Display an error message
        MessageBoxW(NULL, L"Error: PIN must be at least 4 characters long.", L"Invalid PIN", MB_OK | MB_ICONERROR);

        // Terminate the application gracefully
        RefreshData();
        return;

    }


    // Construct the command
    std::wstring command = L".\\fido2-manage.exe  -storage -device " + deviceNumber + L" -pin " + globalPin;  

    // Execute the command
    std::vector<std::wstring> output = RunCommandAndGetOutput(command);

    // Check if the output contains "FIDO_ERR_PIN_REQUIRED"
    for (const auto& line : output) {
        if (line.find(L"FIDO_ERR_PIN_REQUIRED") != std::wstring::npos) {
            MessageBox(NULL, _T("This device does not have a PIN set. Please set a PIN before proceeding."), _T("PIN Required"), MB_ICONWARNING);
            std::wstring setPinCommand = L".\\fido2-manage.exe -setPIN -device " + deviceNumber;
            ShellExecute(NULL, L"open", L"cmd.exe", (L"/k " + setPinCommand).c_str(), NULL, SW_SHOW);
        
            RefreshData();

            return; // Exit the function early if PIN is required
        }

        if (line.find(L"FIDO_ERR_PIN_INVALID") != std::wstring::npos) {
            MessageBox(NULL, _T("Wrong PIN provided."), _T("PIN Required"), MB_ICONWARNING);
          
            RefreshData();

            return; // Exit the function early if PIN is required
        }

    }
    // Construct the second command
    std::wstring command2 = L".\\fido2-manage.exe -info -device " + deviceNumber;

    // Execute the second command
    std::vector<std::wstring> additionalOutput = RunCommandAndGetOutput(command2);

    // Append the second command's output to the existing output vector
    output.insert(output.end(), additionalOutput.begin(), additionalOutput.end());

    // Enable Change PIN button unconditionally
    HWND hChangePinButton = GetDlgItem(hwnd, ID_BTN_CHANGEPIN);
    if (hChangePinButton) {
        EnableWindow(hChangePinButton, TRUE);
        RedrawWindow(hChangePinButton, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    }

    // Enable Reset button unconditionally
    HWND hResetButton = GetDlgItem(hwnd, ID_BTN_RESET);
    if (hResetButton) {
        EnableWindow(hResetButton, TRUE);
        RedrawWindow(hResetButton, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    }

    // Add items to the ListView
    for (size_t i = 0; i < output.size(); ++i) {
        std::wstring line = output[i];
        size_t pos = line.find(L':');
        std::wstring name = (pos != std::wstring::npos) ? line.substr(0, pos) : line;
        std::wstring value = (pos != std::wstring::npos) ? line.substr(pos + 1) : L"";
        // Replace text in names
        if (name.find(L"existing rk(s)") != std::wstring::npos) {
            name = L"existing passkeys";
        }
        if (name.find(L"remaining rk(s)") != std::wstring::npos) {
            name = L"remaining passkeys";
        }
        if (name == L"existing passkeys") {
           // MessageBox(hwnd, (L"Name: " + name + L", Value: " + value).c_str(), L"Debug: Existing Passkeys", MB_OK);

            try {
                int count = std::stoi(value);

             //   MessageBox(hwnd, (L"Count: " + std::to_wstring(count)).c_str(), L"Debug: Conversion Success", MB_OK);

                if (count > 0) {
                   // MessageBox(hwnd, L"Enabling Passkeys Button", L"Debug: Button Enabled", MB_OK);

                    HWND hButton = GetDlgItem(hwnd, ID_BTN_PASSKEYS);
                    if (hButton) {
                        EnableWindow(hButton, TRUE); // Enable the button

                        // Force a redraw to ensure the UI updates
                        RedrawWindow(hButton, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
                    }
                    else {
                    //    MessageBox(hwnd, L"Button handle is null!", L"Debug: Button Error", MB_OK);
                    }
                }
            }
            catch (...) {
               // MessageBox(hwnd, L"Exception during conversion!", L"Debug: Conversion Failed", MB_OK);
            }
        }

        if (name == L"version strings") {
            // Debug: Show the version strings value
           // MessageBox(hwnd, (L"Version Strings: " + value).c_str(), L"Debug: Version Strings", MB_OK);

            // Search for "FIDO_2_1"
            size_t posFIDO_2_1 = value.find(L"FIDO_2_1");

            // Check if "FIDO_2_1" exists and is not followed by "_"
            if (posFIDO_2_1 != std::wstring::npos &&
                (posFIDO_2_1 + 8 >= value.length() || value[posFIDO_2_1 + 8] != L'_')) {
               // MessageBox(hwnd, L"Enabling Enforce UV Button", L"Debug: Button Enabled", MB_OK);

                HWND hButton = GetDlgItem(hwnd, ID_BTN_ENFORCEUV);
                if (hButton) {
                    EnableWindow(hButton, TRUE); // Enable the button
                    RedrawWindow(hButton, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
                }
                else {
                 //   MessageBox(hwnd, L"Enforce UV Button handle is null!", L"Debug: Button Error", MB_OK);
                }
            }
            else {
               // MessageBox(hwnd, L"Conditions not met: Button will not be enabled", L"Debug: Button Not Enabled", MB_OK);
            }
        }

        if (name == L"options") {
            // Debug: Show the options value
           // MessageBox(hwnd, (L"Options: " + value).c_str(), L"Debug: Options", MB_OK);

            // Check if "bioEnroll" exists in the options
            if (value.find(L"bioEnroll") != std::wstring::npos) {
              //  MessageBox(hwnd, L"Enabling Fingerprints Button", L"Debug: Button Enabled", MB_OK);

                HWND hButton = GetDlgItem(hwnd, ID_BTN_FINGERPRINT);
                if (hButton) {
                    EnableWindow(hButton, TRUE); // Enable the Fingerprints button
                    RedrawWindow(hButton, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
                }
                else {
                 //   MessageBox(hwnd, L"Fingerprints Button handle is null!", L"Debug: Button Error", MB_OK);
                }
            }
            else {
              //  MessageBox(hwnd, L"bioEnroll not found in options", L"Debug: Button Not Enabled", MB_OK);
            }
        }



        LVITEM lvItem = { 0 };
        lvItem.mask = LVIF_TEXT;
        lvItem.iItem = static_cast<int>(i);

        // Add Name
        wchar_t nameBuffer[256];
        wcscpy_s(nameBuffer, name.c_str());
        lvItem.pszText = nameBuffer;
        SendMessage(hListView, LVM_INSERTITEM, 0, (LPARAM)&lvItem);

        // Add Value
        wchar_t valueBuffer[256];
        wcscpy_s(valueBuffer, value.c_str());
        lvItem.iSubItem = 1;
        lvItem.pszText = valueBuffer;
        SendMessage(hListView, LVM_SETITEM, 0, (LPARAM)&lvItem);
    }


}

// Resize controls when the window is resized
void ResizeControls(HWND hwnd) {
    RECT rect;
    GetClientRect(hwnd, &rect);

    // Resize ComboBox and Refresh Button
    int buttonWidth = 100;
    SetWindowPos(hComboBox, NULL, 10, 10, rect.right - buttonWidth - 30, 200, SWP_NOZORDER);
    SetWindowPos(hRefreshButton, NULL, rect.right - buttonWidth - 10, 10, buttonWidth, 25, SWP_NOZORDER);

    // Resize ListView
    SetWindowPos(hListView, NULL, 10, 50, rect.right - 20, rect.bottom - 100, SWP_NOZORDER);

    // Calculate horizontal spacing for buttons
    int buttonHeight = 30;
    int buttonSpacing = 10; // Spacing between buttons
    int totalButtonWidth = 5 * (100 + buttonSpacing) - buttonSpacing; // 5 buttons of width 100
    int startX = (rect.right - totalButtonWidth) / 2; // Center align buttons

    // Adjust positions for new buttons
    SetWindowPos(GetDlgItem(hwnd, ID_BTN_PASSKEYS), NULL, startX, rect.bottom - buttonHeight - 10, 100, buttonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, ID_BTN_FINGERPRINT), NULL, startX + 110, rect.bottom - buttonHeight - 10, 100, buttonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, ID_BTN_CHANGEPIN), NULL, startX + 220, rect.bottom - buttonHeight - 10, 100, buttonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, ID_BTN_ENFORCEUV), NULL, startX + 330, rect.bottom - buttonHeight - 10, 100, buttonHeight, SWP_NOZORDER);
    SetWindowPos(GetDlgItem(hwnd, ID_BTN_RESET), NULL, startX + 440, rect.bottom - buttonHeight - 10, 100, buttonHeight, SWP_NOZORDER);

    // Force redraw of the entire client area to prevent garbled visuals
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}



// Clear and repopulate data
void RefreshData() {

    DisableAllButtons(hwnd);

    // Clear ComboBox
    SendMessage(hComboBox, CB_RESETCONTENT, 0, 0);

    // Clear ListView
    SendMessage(hListView, LVM_DELETEALLITEMS, 0, 0);

    // Repopulate ComboBox
    PopulateComboBox();
}

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_GETMINMAXINFO : {
            MINMAXINFO* mmi = (MINMAXINFO*)lParam;

            // Set minimum window size
            mmi->ptMinTrackSize.x = 600; // Minimum width
            mmi->ptMinTrackSize.y = 430; // Minimum height
            break;
        }
    case WM_CREATE: {
        // Initialize common controls
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icex);

        // Create ComboBox
        hComboBox = CreateWindow(
            _T("COMBOBOX"),
            NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_HASSTRINGS | WS_VSCROLL,
            10, 10, 200, 200,
            hwnd,
            (HMENU)ID_COMBOBOX,
            hInst,
            NULL);

        // Create Refresh Button
        hRefreshButton = CreateWindow(
            _T("BUTTON"),
            _T("Refresh"),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            220, 10, 100, 25,
            hwnd,
            (HMENU)ID_REFRESH_BUTTON,
            hInst,
            NULL);

        // Create ListView
        hListView = CreateWindow(
            WC_LISTVIEW,
            NULL,
            WS_CHILD | WS_VISIBLE | LVS_REPORT,
            10, 50, 460, 300,
            hwnd,
            (HMENU)ID_LISTVIEW,
            hInst,
            NULL);

        // Add columns to the ListView
        LVCOLUMN lvColumn = { 0 };
        lvColumn.mask = LVCF_TEXT | LVCF_WIDTH;

        // Add "Name" column
        lvColumn.cx = 200;
        wchar_t columnName[] = L"Name";
        lvColumn.pszText = columnName;
        SendMessage(hListView, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

        // Add "Value" column
        lvColumn.cx = 260;
        wchar_t columnValue[] = L"Value";
        lvColumn.pszText = columnValue;
        SendMessage(hListView, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);

        // Create buttons at the bottom (initially disabled)
        HWND hPasskeysButton = CreateWindow(
            L"BUTTON", L"Passkeys",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_DISABLED | BS_DEFPUSHBUTTON,
            10, 360, 100, 30, // Position and size (x, y, width, height)
            hwnd, (HMENU)ID_BTN_PASSKEYS, hInst, NULL);

        HWND hFingerprintButton = CreateWindow(
            L"BUTTON", L"Fingerprints",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_DISABLED | BS_DEFPUSHBUTTON,
            120, 360, 100, 30,
            hwnd, (HMENU)ID_BTN_FINGERPRINT, hInst, NULL);

        HWND hChangePinButton = CreateWindow(
            L"BUTTON", L"Change PIN",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_DISABLED | BS_DEFPUSHBUTTON,
            230, 360, 100, 30,
            hwnd, (HMENU)ID_BTN_CHANGEPIN, hInst, NULL);

        HWND hEnforceUVButton = CreateWindow(
            L"BUTTON", L"Enforce UV",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_DISABLED | BS_DEFPUSHBUTTON,
            340, 360, 100, 30,
            hwnd, (HMENU)ID_BTN_ENFORCEUV, hInst, NULL);

        HWND hResetButton = CreateWindow(
            L"BUTTON", L"Reset",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_DISABLED | BS_DEFPUSHBUTTON,
            450, 360, 100, 30,
            hwnd, (HMENU)ID_BTN_RESET, hInst, NULL);

        

        // Populate the ComboBox
        PopulateComboBox();
        break;
    }
    case WM_COMMAND:

        switch (LOWORD(wParam)) {
            // Example: Enable the "Passkeys" button when another action is taken
        case ID_BTN_PASSKEYS: {
            // Set the wait cursor
            HCURSOR hWaitCursor = LoadCursor(NULL, IDC_WAIT);
            SetCursor(hWaitCursor);
            // Process pending messages to update the cursor
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            // Step 1: Get domains from the first run
            std::vector<std::wstring> domains = GetDomains(deviceNumber, globalPin);

            // Debug: Check the extracted domains
            for (const auto& domain : domains) {
                //MessageBox(hwnd, domain.c_str(), L"Debug: Extracted Domain", MB_OK);
            }

            // Step 2: Get resident keys with domains
            std::vector<PasskeyInfo> passkeys = GetResidentKeysWithDomains(deviceNumber, globalPin, domains);

            // Debug: Check the parsed results from the second run
            for (const auto& pk : passkeys) {
              //  MessageBox(hwnd, (L"Credential ID: " + pk.credentialId + L"\nUser: " + pk.user).c_str(), L"Debug: Parsed Passkey", MB_OK);
            }

            // Restore the default cursor
            HCURSOR hArrowCursor = LoadCursor(NULL, IDC_ARROW);
            SetCursor(hArrowCursor);
            
            // Step 3: Pass the results to the dialog
            DialogBoxParam(
                hInst,
                MAKEINTRESOURCE(IDD_PASSKEYS_DIALOG),
                hwnd,
                PasskeysDialogProc,
                reinterpret_cast<LPARAM>(&passkeys)
            );


            break;
        }



        case ID_BTN_FINGERPRINT:
             
            DialogBox(
                hInst,
                MAKEINTRESOURCE(IDD_FINGERPRINT_DIALOG),
                hwnd,
                FingerprintDialogProc
            );
            break;
           

        case ID_BTN_CHANGEPIN:
        {
            // Construct the change PIN command
            std::wstring chPINCommand = L".\\fido2-manage.exe -changepin -device " + deviceNumber;

            // Execute the command in a new console window
            ShellExecute(NULL, L"open", L"cmd.exe", (L"/k \"" + chPINCommand + L"\"").c_str(), NULL, SW_SHOW);
            break;
        }
        case ID_BTN_ENFORCEUV:
        {
            // Construct the enforce UV command
            std::wstring UVSCommand = L".\\fido2-manage.exe -uvs -device " + deviceNumber;

            // Execute the command in a new console window
            ShellExecute(NULL, L"open", L"cmd.exe", (L"/k \"" + UVSCommand + L"\"").c_str(), NULL, SW_SHOW);
            break;
        }


        case ID_BTN_RESET:
        {
            // Construct the reset command
            std::wstring resetCommand = L".\\fido2-manage.exe -reset -device " + deviceNumber;

            // Execute the command in a new console window
            ShellExecute(NULL, L"open", L"cmd.exe", (L"/k \"" + resetCommand + L"\"").c_str(), NULL, SW_SHOW);
            break;
        }

        }
        if (LOWORD(wParam) == ID_COMBOBOX && HIWORD(wParam) == CBN_SELCHANGE) {
            // Get selected item from the ComboBox
            int selIndex = static_cast<int>(SendMessage(hComboBox, CB_GETCURSEL, 0, 0));
            if (selIndex != CB_ERR) {
                wchar_t buffer[256];
                SendMessage(hComboBox, CB_GETLBTEXT, selIndex, (LPARAM)buffer);

                // Extract the device number using regex
                std::wregex regex(L"\\[(\\d+)\\]");
                std::wsmatch match;
                std::wstring itemText(buffer);
                if (std::regex_search(itemText, match, regex)) {
                     deviceNumber = match[1];
                    PopulateListView(hwnd, deviceNumber); // Pass hwnd from WindowProc
                }
            }
        }
        else if (LOWORD(wParam) == ID_REFRESH_BUTTON) {
            RefreshData();
        }
        break;

    case WM_SIZE:
        ResizeControls(hwnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}
