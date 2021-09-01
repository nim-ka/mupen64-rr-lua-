#include "../resource.h"
#include "../main/win/CrashHandler.h"
#include "../main/win/main_win.h"
#include "GameDebugger.h"
#include <stdio.h>
#include <Psapi.h>
#include "vcr.h"
#include <Windows.h>
#include "../../r4300/r4300.h"
#include <LuaConsole.h>
#include "../../memory/memory.h"

// shitty crappy n64 debugger* by aurumaker 


typedef struct _DebuggedData DebuggerData;
struct _DebuggedData
{
    unsigned long Address;
    unsigned long Source;


    char* DecodedInstructionText;
};
    
DebuggerData* debuggerData = NULL;
int debugger_cpuAllowed = 1;
int debugger_step = 0;
HWND hwndd;
extern unsigned long op;
BOOL diecdmode;

DWORD(__cdecl* ORIGINAL_doRspCycles)(DWORD Cycles) = NULL;
static DWORD __cdecl fake_doRspCycles(DWORD Cycles) { return Cycles; };


int _gaddr() {
    return Config.guiDynacore ? PC->addr : interp_addr;
}
int _gsrc() {
    return Config.guiDynacore ? PC->src : op;
}



void DebuggerSet(int debuggerFlag) {

    if (!Config.guiDynacore) {
        N64DEBUG_MBOX(N64DEBUG_NAME " might not work with (pure) interpreter cpu core.");
    }

    debugger_cpuAllowed = debuggerFlag;

    if (debuggerFlag == N64DEBUG_RESUME) debugger_step = 0;

    if (debuggerFlag == N64DEBUG_PAUSE) {
        
        char* instrstr = (char*)calloc(100, sizeof(char));
        char* precomp_addr = (char*)calloc(100, sizeof(char)); // fuck you pure interpreter
        char* precomp_op = (char*)calloc(100, sizeof(char));


        diecdmode = IsDlgButtonChecked(hwndd, IDC_DEBUGGER_INSTDECODEMODE);

        if(diecdmode)
            instrStr1(_gaddr(), _gsrc(), instrstr);
        else
            instrStr2(_gaddr(), _gsrc(), instrstr);
        
        
        sprintf(precomp_addr, "0x%lx", PC->addr);
        sprintf(precomp_op, "0x%lx", PC->ops); // idk how to represent this exactly hm

        SetWindowText(GetDlgItem(hwndd, IDC_DEBUGGER_INSTRUCTION), instrstr);

        SetWindowText(GetDlgItem(hwndd, IDC_DEBUGGER_PRECOMPADDR), precomp_addr);

        SetWindowText(GetDlgItem(hwndd, IDC_DEBUGGER_PRECOMPOP), precomp_op);


        //if(Config.guiDynacore)
        //PC->s_ops();
    }

}

BOOL CALLBACK DebuggerDialogProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
    switch (Message) {
    case WM_INITDIALOG: {

        hwndd = hwnd;
            
        //SendMessage(GetDlgItem(hwnd, IDC_DEBUGGER_PAUSE),
        //    (UINT)BM_SETIMAGE,
        //    (WPARAM)IMAGE_BITMAP,
        //    (LPARAM)LoadBitmap(GetModuleHandle(0), MAKEINTRESOURCE(IDB_PAUSE)));
        //
        //SendMessage(GetDlgItem(hwnd, IDC_DEBUGGER_RESUME),
        //    (UINT)BM_SETIMAGE,
        //    (WPARAM)IMAGE_BITMAP,
        //    (LPARAM)LoadBitmap(GetModuleHandle(0), MAKEINTRESOURCE(IDB_RESUME)));
        //

        SetWindowText(hwnd, N64DEBUG_NAME " - " MUPEN_VERSION);

        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_DEBUGGER_DUMPRDRAM:
            N64DEBUG_MBOX("You are about to write 32 megabytes to a file.");
            FILE* f;
            f = fopen("rdram.bin", "wb");
            fwrite(&rdram, sizeof(unsigned long), sizeof(rdram), f);
            fflush(f); fclose(f);
            break;
        case IDC_DEBUGGER_RSP_TOGGLE:
            printf("dame tu %d\n", IsDlgButtonChecked(hwnd, IDC_DEBUGGER_RSP_TOGGLE));
            if (!ORIGINAL_doRspCycles) {
                ORIGINAL_doRspCycles = doRspCycles;
            }
            if (IsDlgButtonChecked(hwnd, IDC_DEBUGGER_RSP_TOGGLE))
                doRspCycles = ORIGINAL_doRspCycles;
            else
                doRspCycles = fake_doRspCycles;

            break;
        case IDC_DEBUGGER_STEP:
            if (!debugger_cpuAllowed) {
                debugger_step = 1;
            }
            break;
        case IDC_DEBUGGER_INSTDECODEMODE:
            if (!debugger_cpuAllowed) DebuggerSet(0);
            break;
        case IDC_DEBUGGER_PAUSE:
            DebuggerSet(N64DEBUG_PAUSE);

            
            break;
        case IDC_DEBUGGER_RESUME:
            DebuggerSet(N64DEBUG_RESUME);

            
            break;
        case IDOK:
            if (!debugger_cpuAllowed) {
                N64DEBUG_MBOX("The debugger paused the r4300. Unpause it before quitting the Debugger.");
                break;
            }
            EndDialog(hwnd,0);
            break;
        }
    default:
        return FALSE;

        return DebuggerDialogProc(hwnd, Message, wParam, lParam);
    }
}

DWORD WINAPI DebuggerThread(LPVOID lpParam) {
    DialogBox(GetModuleHandle(0), MAKEINTRESOURCE(IDD_GAMEDEBUGGERDIALOG), 0, DebuggerDialogProc);
    ExitThread(0);
}
void DebuggerDialog() {

    DWORD troll;
    CreateThread(NULL, 0, DebuggerThread, NULL, 0, &troll);

    

}