#include <windows.h>
#include <shldisp.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <stdlib.h>
#include <objbase.h>

void archivate(char szFrom[],char szTo[])
{
    DWORD strlen = 0;
    HRESULT hResult;
    IShellDispatch *pISD;
    Folder* pToFolder = NULL;
    VARIANT vDir, vFile, vOpt;
    BSTR strptr1, strptr2;

    FILE* f;
    fopen_s(&f, szTo, "wb");
    if (!f)
        return;

    fwrite("\x50\x4B\x05\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 22, 1, f);
    fclose(f);

    CoInitialize(NULL);

    hResult = CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, IID_IShellDispatch, (void**)&pISD);

    if (SUCCEEDED(hResult) && pISD != NULL)
    {
        strlen = MultiByteToWideChar(CP_ACP, 0, szTo, -1, 0, 0);
        strptr1 = SysAllocStringLen(0, strlen);
        MultiByteToWideChar(CP_ACP, 0, szTo, -1, strptr1, strlen);

        VariantInit(&vDir);
        vDir.vt = VT_BSTR;
        vDir.bstrVal = strptr1;
        hResult = pISD->NameSpace(vDir, &pToFolder);

        if (SUCCEEDED(hResult))
        {
            strlen = MultiByteToWideChar(CP_ACP, 0, szFrom, -1, 0, 0);
            strptr2 = SysAllocStringLen(0, strlen);
            MultiByteToWideChar(CP_ACP, 0, szFrom, -1, strptr2, strlen);

            VariantInit(&vFile);
            vFile.vt = VT_BSTR;
            vFile.bstrVal = strptr2;

            VariantInit(&vOpt);
            vOpt.vt = VT_I4;
            vOpt.lVal = 4;          // Do not display a progress dialog box

            hResult = pToFolder->CopyHere(vFile, vOpt); //NOTE: this appears to always return S_OK even on error
            /*
             * 1) Enumerate current threads in the process using Thread32First/Thread32Next
             * 2) Start the operation
             * 3) Enumerate the threads again
             * 4) Wait for any new threads using WaitForMultipleObjects
             *
             * Of course, if the operation creates any new threads that don't exit, then you have a problem.
             */
            if (hResult == S_OK) {
                //NOTE: hard-coded for testing - be sure not to overflow the array if > 5 threads exist
                HANDLE hThrd[5];
                HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);  //TH32CS_SNAPMODULE, 0);
                DWORD NUM_THREADS = 0;
                if (h != INVALID_HANDLE_VALUE) {
                    THREADENTRY32 te;
                    te.dwSize = sizeof(te);
                    if (Thread32First(h, &te)) {
                        do {
                            if (te.dwSize >= (FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID))) {
                                //only enumerate threads that are called by this process and not the main thread
                                if ((te.th32OwnerProcessID == GetCurrentProcessId()) && (te.th32ThreadID != GetCurrentThreadId())) {
                                    hThrd[NUM_THREADS] = OpenThread(SYNCHRONIZE, FALSE, te.th32ThreadID);
                                    NUM_THREADS++;
                                }
                            }
                            te.dwSize = sizeof(te);
                        } while (Thread32Next(h, &te));
                    }
                    CloseHandle(h);

                    //Wait for all threads to exit
                    WaitForMultipleObjects(NUM_THREADS, hThrd, TRUE, 2000);

                    //Close All handles
                    for (DWORD i = 0; i < NUM_THREADS; i++) {
                        CloseHandle(hThrd[i]);
                    }
                } //if invalid handle
            } //if CopyHere() hResult is S_OK

            SysFreeString(strptr2);
            pToFolder->Release();
        }

        SysFreeString(strptr1);
        pISD->Release();
    }
    CoUninitialize();
    return;

}