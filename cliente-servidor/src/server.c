#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <locale.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "proto.h"

// encerrar servidor
static volatile sig_atomic_t g_running = 1;

// sinalizar pra sair do loop
static void on_sigint(int sig) {
    (void)sig;
    g_running = 0;
}

// registrar handlers de sinais do processo
static void install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
}

// remover \r \n e espaços
void remove_spaces(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r' || isspace((unsigned char)s[n-1]))) n--;
    s[n] = '\0';
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i) memmove(s, s + i, n - i + 1);
}

// garantir ponto decimal
void set_c_locale(void) {
    setlocale(LC_NUMERIC, "C");
}

// imprimir double com até 6 casas e remover 0s e ponto finais
void format_number(double x, char *out, size_t outsz) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.6f", x);
    char *p = buf + strlen(buf) - 1;
    while (p > buf && *p == '0') { *p-- = '\0'; }
    if (p > buf && *p == '.') *p = '\0';
    snprintf(out, outsz, "%s", buf);
}

// enviar len bytes
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

// lê do socket até \n
static int safe_readline(int fd, char *buf, size_t bufsz) {
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

// converte string em double com strtod
static bool parse_double(const char *s, double *out) {
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    if (errno != 0 || end == s) return false;
    while (*end) {
        if (!isspace((unsigned char)*end)) return false;
        end++;
    }
    *out = v;
    return true;
}

// tenta prefixo e depois infixo
parse_result_t parse_request_line(const char *line_in) {
    parse_result_t r = (parse_result_t){0};
    r.ok = false;
    r.err_code = ERR_INV;
    r.err_msg = "entrada_invalida";

    if (!line_in) return r;

    char line[LINE_MAX];
    snprintf(line, sizeof(line), "%s", line_in);
    remove_spaces(line);
    if (line[0] == '\0') return r;

    if (strcasecmp(line, "QUIT") == 0) {
        r.ok = true; r.op = OP_NONE; return r;
    }

    {
        char op[8], sa[128], sb[128];
        op[0] = sa[0] = sb[0] = '\0';
        int n = sscanf(line, "%7s %127s %127s", op, sa, sb);
        if (n == 3) {
            for (char *p = op; *p; ++p) *p = (char)toupper((unsigned char)*p);
            double a, b;
            if (parse_double(sa, &a) && parse_double(sb, &b)) {
                if (strcmp(op, "ADD") == 0) r.op = OP_ADD;
                else if (strcmp(op, "SUB") == 0) r.op = OP_SUB;
                else if (strcmp(op, "MUL") == 0) r.op = OP_MUL;
                else if (strcmp(op, "DIV") == 0) r.op = OP_DIVI;
                else goto try_infix;
                r.a = a; r.b = b; r.ok = true; return r;
            }
        }
    }

try_infix:
    {
        char sa[128], sb[128], sop[8];
        sa[0]=sb[0]=sop[0]='\0';
        int n = sscanf(line, "%127s %7s %127s", sa, sop, sb);
        if (n == 3 && strlen(sop) == 1) {
            double a, b;
            if (parse_double(sa, &a) && parse_double(sb, &b)) {
                char opch = sop[0];
                switch (opch) {
                    case '+': r.op = OP_ADD; break;
                    case '-': r.op = OP_SUB; break;
                    case '*': r.op = OP_MUL; break;
                    case '/': r.op = OP_DIVI; break;
                    default: return r;
                }
                r.a = a; r.b = b; r.ok = true; return r;
            }
        }
    }

    return r;
}

// atender cliente
static void serve_client(int conn_fd) {
    char line[LINE_MAX];
    for (;;) {
        int nr = safe_readline(conn_fd, line, sizeof(line));
        if (nr == 0) {
            fprintf(stderr, "Cliente fechou a conexão.\n");
            break;
        }
        if (nr < 0) {
            if (errno == EINTR && !g_running) break;
            if (errno == EINTR) continue;
            perror("recv");
            break;
        }

        // parsing da linha lida
        parse_result_t pr = parse_request_line(line);

        // erro EINV 
        if (!pr.ok) {
            char resp[128];
            int m = snprintf(resp, sizeof(resp), "ERR %s %s\n", pr.err_code ? pr.err_code : ERR_INV, pr.err_msg  ? pr.err_msg  : "entrada_invalida");
            if (send_all(conn_fd, resp, (size_t)m) < 0) break;
            continue;
        }

        if (pr.op == OP_NONE) break;

        // erro EZDV
        if (pr.op == OP_DIVI && pr.b == 0.0) {
            (void)send_all(conn_fd, "ERR EZDV divisao_por_zero\n", strlen("ERR EZDV divisao_por_zero\n"));
            continue;
        }

        // executar a operação
        double result = 0.0;
        switch (pr.op) {
            case OP_ADD: result = pr.a + pr.b; break;
            case OP_SUB: result = pr.a - pr.b; break;
            case OP_MUL: result = pr.a * pr.b; break;
            case OP_DIVI: result = pr.a / pr.b; break;
            default:
                (void)send_all(conn_fd, "ERR ESRV erro_interno\n", strlen("ERR ESRV erro_interno\n"));
                continue;
        }
        char num[64];
        format_number(result, num, sizeof num);
        char resp[128];
        int m = snprintf(resp, sizeof(resp), "OK %s\n", num);
        if (send_all(conn_fd, resp, (size_t)m) < 0) break;
    }
}

int main(int argc, char **argv) {
    int port = DEFAULT_PORT;
    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Porta inválida.\n");
            return EXIT_FAILURE;
        }
    }

    set_c_locale();

    install_signal_handlers();

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    int yes = 1;
    (void)setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;  
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    if (listen(listen_fd, 8) < 0) {
        perror("listen");
        close(listen_fd);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "Ouvindo em 0.0.0.0:%d (Ctrl+C para encerrar)\n", port);

    // aceita mais de um cliente
    while (g_running) {
        int conn_fd = accept(listen_fd, NULL, NULL);
        if (conn_fd < 0) {
            if (errno == EINTR && !g_running) break;
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }
        fprintf(stderr, "Cliente conectado (fd=%d)\n", conn_fd);

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(conn_fd);
            continue;
        }
        if (pid == 0) {
            close(listen_fd);
            serve_client(conn_fd);
            close(conn_fd);
            fprintf(stderr, "Conexão encerrada.\n");
            _exit(0);
        } else {
            close(conn_fd);
        }
    }

    // fechar socket de escuta
    close(listen_fd);
    fprintf(stderr, "Servidor finalizado.\n");
    return 0;
}