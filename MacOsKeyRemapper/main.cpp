#include <windows.h>
#include <iostream>
using namespace std;


bool lock = false; // Prevent recursion
bool wasAlt = false;
bool bypassChecks = false;
short int whitelistKeys[] = { VK_TAB, VK_SPACE, VK_UP, VK_DOWN };


void debugOnlyLog(string message) {
#if(_DEBUG)
    std::cout << message << "\n";
#endif
}


LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (bypassChecks) {
        debugOnlyLog("BYPASS CHECK...");
        bypassChecks = false;
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }


    if (nCode == HC_ACTION && !lock) {
        KBDLLHOOKSTRUCT* kbStruct = (KBDLLHOOKSTRUCT*)lParam;

        //DEBUG ONLY!!! CLEAR CONSOLE ON "P" KEY PRESS
#if(_DEBUG)
        if (kbStruct->vkCode == 0x50 && wParam == WM_KEYDOWN) {
            system("cls");
        }
#endif


        if (
            (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
            && (kbStruct->vkCode == VK_LMENU || kbStruct->vkCode == VK_RMENU)
            && wasAlt
            ) {
            wasAlt = false;
            lock = true;
            debugOnlyLog("ALT KEY RELEASED, SENDING CTRL KEY UP EMULATED EVENT");

            // Inject CTRL key release
            INPUT input = { 0 };
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = VK_CONTROL;
            input.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &input, sizeof(INPUT));

            lock = false;
            return 1;
        }
        else if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            if (kbStruct->vkCode == VK_LMENU
                || kbStruct->vkCode == VK_RMENU) {
                debugOnlyLog("ALT PRESSED. WASALT = TRUE");

                //IS ALT
                wasAlt = true;
                return 1;
            }
            //NOT ALT   
            if (!wasAlt) {

                debugOnlyLog("KEY PRESSED (WAS NOT ALT)");

                return CallNextHookEx(NULL, nCode, wParam, lParam);
            }

            //WAS ALT
            //check if is whitelist key

            bool match = false;
            for (int i = 0; i < sizeof(whitelistKeys); i++)
            {
                if (kbStruct->vkCode == whitelistKeys[i]) {
                    match = true;
                    break;
                }
            }

            if (match) {
                debugOnlyLog("TAB OR SPACE PRESSED! SENDING ALT KEYDOWN + CURRENT KEY (SHOULD BYPASS CHECK)");

                //is whitelist
                lock = true;
                wasAlt = false;
                bypassChecks = true;

                //send alt 
                INPUT input = { 0 };
                input.type = INPUT_KEYBOARD;
                input.ki.wVk = VK_LMENU;
                SendInput(1, &input, sizeof(INPUT));

                //send current key
                INPUT input2 = { 0 };
                input2.type = INPUT_KEYBOARD;
                input2.ki.wVk = kbStruct->vkCode;
                SendInput(1, &input2, sizeof(INPUT));

                lock = false;
            }
            else
            {
                debugOnlyLog("SENDING CTRL EMULATED PRESS. (SHOULD LISTEN FOR ALT KEY UP)");

                //Not in whitelist
                lock = true;

                //send ctrl 
                INPUT input = { 0 };
                input.type = INPUT_KEYBOARD;
                input.ki.wVk = VK_CONTROL;
                SendInput(1, &input, sizeof(INPUT));

                //send current key
                INPUT input2 = { 0 };
                input2.type = INPUT_KEYBOARD;
                input2.ki.wVk = kbStruct->vkCode;
                SendInput(1, &input2, sizeof(INPUT));

                lock = false;
            }
            return 1;

        }

    }

    // Pass other events to the next hook in the chain
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Hide console window
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    // Add to Windows startup registry
    HKEY hKey;
    const char* keyPath = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    char executablePath[MAX_PATH];
    GetModuleFileNameA(NULL, executablePath, MAX_PATH);

    if (RegOpenKeyExA(HKEY_CURRENT_USER, keyPath, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, "MacOSKeymapper", 0, REG_SZ, (BYTE*)executablePath, strlen(executablePath) + 1);
        RegCloseKey(hKey);
    }

    // Set the low-level keyboard hook
    HHOOK hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (!hook) {
        return 1;
    }

    // Message loop to keep the application running
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Unhook before exiting
    UnhookWindowsHookEx(hook);
    return 0;
}
