#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int client_socket;
    struct sockaddr_in server_address;
    struct hostent *server;
    char buffer[BUFFER_SIZE];

    // Создаем сокет
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Получаем информацию о сервере
    server = gethostbyname("192.168.0.255"); // Замените на IP адрес сервера, если он не на локальной машине
    if (server == NULL) {
        fprintf(stderr, "Error, no such host\n");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    memset((char *)&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    memcpy((char *)server->h_addr, (char *)&server_address.sin_addr.s_addr, server->h_length);
    server_address.sin_port = htons(PORT);

    // Подключаемся к серверу
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connect failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server. Start chatting! (Type 'exit' to quit)\n");

    while (1) {
        // Получаем сообщение от пользователя
        printf("You: ");
        fgets(buffer, BUFFER_SIZE, stdin);

        // Удаляем символ новой строки
        buffer[strcspn(buffer, "\n")] = 0;

        // Отправляем сообщение на сервер
        if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
            perror("Send failed");
            break;
        }

        // Проверяем, не хочет ли пользователь выйти
        if (strcmp(buffer, "exit") == 0) {
            break;
        }
    }

    // Закрываем сокет
    close(client_socket);
    return 0;
}
