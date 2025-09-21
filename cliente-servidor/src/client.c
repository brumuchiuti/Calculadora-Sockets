#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>      
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h> 
#include "proto.h"      

// garante enviar todos os bytes
static int send_all(int fd, const char *data, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, data + off, len - off, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

// le até \n
static int recv_line(int fd, char *buf, size_t bufsz) {
    size_t i = 0;
    while (i + 1 < bufsz) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n == 0) {
            if (i == 0) return 0;
            break;                
        }
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return (int)i;
}

int main(int argc, char **argv) {
    // verificação de parametros
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <ip-servidor> <porta>\nEx.: %s 127.0.0.1 5050\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Porta inválida.\n");
        return EXIT_FAILURE;
    }

    // criar socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    // preencher endereço do servidor
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        fprintf(stderr, "IP inválido: %s\n", ip);
        close(sock);
        return EXIT_FAILURE;
    }

    // conexão
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "Conectado em %s:%d\n", ip, port);
    fprintf(stderr, "Digite operações (QUIT para sair)\n");

    char line[LINE_MAX];
    char resp[LINE_MAX];

    // le uma linha do stdin e envia, le uma resposta do servidor
    for (;;) {
        if (!fgets(line, sizeof(line), stdin)) {
            const char *quit = "QUIT\n";
            (void)send_all(sock, quit, strlen(quit));
            break;
        }

        if (send_all(sock, line, strlen(line)) < 0) {
            perror("send");
            break;
        }

        if (strncasecmp(line, "QUIT", 4) == 0) {
            break;
        }

        // ler linha de resposta do servidor
        int n = recv_line(sock, resp, sizeof(resp));
        if (n == 0) {
            fprintf(stderr, "Servidor fechou a conexão.\n");
            break;
        }
        if (n < 0) {
            perror("recv");
            break;
        }

        // imprimir resposta
        fputs(resp, stdout);
    }

    // fechar e encerrar socket
    close(sock);
    return 0;
}
