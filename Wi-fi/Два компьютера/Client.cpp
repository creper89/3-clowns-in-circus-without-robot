#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main()
{
    int sock;
    struct sockaddr_in addr;
    char buf[BUFFER_SIZE];
    char user_input[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        perror("socket");
        exit(1);
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("192.168.31.201"); // IP сервера
    addr.sin_port = htons(8025); // используйте тот же порт, что и сервер
    if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        exit(2);
    }

    while(1)
    {
        printf("Введите сообщение (или Stop connect для выхода): ");
        fflush(stdout);
        if(fgets(user_input, BUFFER_SIZE, stdin) == NULL)
            break;

        // Удаляем символ перевода строки
        size_t len = strlen(user_input);
        if(len > 0 && user_input[len-1] == '\n')
            user_input[len-1] = '\0';

        // Проверяем условие выхода
        if(strcmp(user_input, "Stop connect") == 0)
            break;

        // Отправляем сообщение серверу
        send(sock, user_input, strlen(user_input), 0);

        // Получаем ответ от сервера
        int bytes_read = recv(sock, buf, BUFFER_SIZE-1, 0);
        if(bytes_read > 0)
        {
            buf[bytes_read] = '\0';
            printf("Получено от сервера: %s\n", buf);
        }
        else
        {
            printf("Нет ответа от сервера или соединение закрыто\n");
            break;
        }
    }
    close(sock);
    return 0;
}
