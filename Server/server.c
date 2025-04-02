#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

// Структура для передачи данных потоку клиента
typedef struct {
    int client_socket;
    int client_id;
} client_data_t;

// Массив клиентских сокетов
int client_sockets[MAX_CLIENTS] = {0};
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER; // Мьютекс для защиты доступа к client_sockets

// Функция для обработки одного клиента в отдельном потоке
void *handle_client(void *arg) {
    client_data_t *client_data = (client_data_t *)arg;
    int client_socket = client_data->client_socket;
    int client_id = client_data->client_id;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    printf("Client %d connected\n", client_id);

    while ((bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("Received from client %d: %s", client_id, buffer);

        // Отправляем сообщение всем остальным клиентам (broadcast)
        pthread_mutex_lock(&client_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] != 0 && client_sockets[i] != client_socket) {
                char message[BUFFER_SIZE + 20];
                sprintf(message, "Client %d: %s", client_id, buffer);
                send(client_sockets[i], message, strlen(message), 0);
            }
        }
        pthread_mutex_unlock(&client_mutex);
    }

    // Клиент отключился
    printf("Client %d disconnected\n", client_id);

    // Закрываем сокет и освобождаем место в массиве
    close(client_socket);
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == client_socket) {
            client_sockets[i] = 0;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);

    free(client_data);
    pthread_exit(NULL);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len = sizeof(client_address);
    pthread_t thread_id;
    int client_count = 0;

    // Создаем сокет
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT);

    // Привязываем сокет к адресу
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Начинаем прослушивание входящих соединений
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    // Принимаем входящие соединения
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        // Ищем свободное место для нового клиента
        pthread_mutex_lock(&client_mutex);
        int i;
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] == 0) {
                client_sockets[i] = client_socket;
                break;
            }
        }
        pthread_mutex_unlock(&client_mutex);

        if (i == MAX_CLIENTS) {
            printf("Too many clients, connection refused\n");
            close(client_socket);
            continue;
        }

        // Создаем структуру с данными клиента
        client_data_t *client_data = (client_data_t *)malloc(sizeof(client_data_t));
        if (client_data == NULL) {
            perror("Failed to allocate memory for client data");
            close(client_socket);
            pthread_mutex_lock(&client_mutex);
            client_sockets[i] = 0; // Освобождаем место
            pthread_mutex_unlock(&client_mutex);
            continue;
        }

        client_data->client_socket = client_socket;
        client_data->client_id = i + 1; // ID клиента начинается с 1

        // Создаем новый поток для обработки клиента
        if (pthread_create(&thread_id, NULL, handle_client, (void *)client_data) < 0) {
            perror("Could not create thread");
            close(client_socket);
            free(client_data);
            pthread_mutex_lock(&client_mutex);
            client_sockets[i] = 0; // Освобождаем место
            pthread_mutex_unlock(&client_mutex);
            continue;
        }

        pthread_detach(thread_id); // Освобождаем ресурсы потока после завершения
    }

    close(server_socket);
    return 0;
}