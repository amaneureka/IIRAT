/*
* @Author: amaneureka
* @Date:   2017-04-02 03:33:49
* @Last Modified by:   amaneureka
* @Last Modified time: 2017-04-02 14:15:14
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace std;

int main(int argc, char *argv[])
{
    size_t size;
    int sock, len;
    char *line = NULL;
    char buffer[4096];
    struct sockaddr_in addr;
    fd_set active_fd_set, read_fd_set;

    /* create socket */
    sock = socket(AF_INET , SOCK_STREAM , 0);

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
    FD_SET(0, &active_fd_set);

    while(true)
    {
        read_fd_set = active_fd_set;

        if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
        {
            printf("select error\n");
            exit(0);
        }

        for (int i = 0; i < FD_SETSIZE; ++i)
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