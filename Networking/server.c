#include "string_ops.h"
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CRLF  "\r\n"
#define SP  " "
const int PORT = 8000;
/*Request-Line = Method SP Request-URI SP HTTP-Version CRLF */

typedef enum http_status: uint16_t {
    HTTP_RES_OK = 200,
    HTTP_RES_INTERNAL_SERVER_ERR = 400,
    HTTP_RES_NOT_FOUND = 404,
    HTTP_RES_BAD_REQUEST = 500,
} http_status;

const char* http_status_to_string(http_status status) {
    switch (status) {
        case HTTP_RES_OK:
            return "OK";
        case HTTP_RES_BAD_REQUEST:
            return "Bad Request";
        case HTTP_RES_INTERNAL_SERVER_ERR:
            return "Internal Server Error";
        case HTTP_RES_NOT_FOUND:
            return "Not Found";
            default:
            return "Uknown";
    }
}
typedef struct {
    string method;
    string uri;
    string version;
} http_req_line;

typedef struct {
    const char* version;
    uint16_t status;
} http_resp_status_line;

http_req_line http_req_line_init(void) {
    http_req_line line;
    memset(&line, 0, sizeof(line));
    return line;
}


http_status parse_req_line(http_req_line* req_line, const char* buf, size_t len) {
    if (!buf || !req_line) {
        HTTP_RES_INTERNAL_SERVER_ERR;
    }
    string_splits components = split_string(buf, len, SP);

    if (components.count != 3) {
        printf("Error: invalid request line: expected 3 components, got %zu\n", components.count);
        return HTTP_RES_BAD_REQUEST;
    }
    req_line->method.data = components.splits[0].start;
    req_line->method.len = components.splits[0].len;
    req_line->uri.data = components.splits[1].start;
    req_line->uri.len = components.splits[1].len;
    req_line->version.data = components.splits[2].start;
    req_line->version.len = components.splits[2].len;

    free_splits(&components);
    return HTTP_RES_OK;
}

    string http_response_generate(char* buf, size_t buf_len, http_status status, size_t body_len) {
    memset(buf, 0, buf_len);
    int n = 0;
    string response;
    response.len = 0;

    response.len += sprintf(buf, "%s %d %s" CRLF, "HTTP/1.0", status, http_status_to_string(status));
    response.len += sprintf(buf + response.len, "Content Length: %zu" CRLF, body_len);
    response.len += sprintf(buf+ response.len, CRLF);
    response.data = buf;
    response.data;
    return response;
}

bool http_send_response(int socket, string header, string body) {
    ssize_t n = send(socket, header.data, header.len, 0);
    if (n < 0) {
        perror("send");
        return false;
    }
   if (n == 0) {
       fprintf(stderr, "send() returned 0\n");
       return false;
   }
   n = send(socket, body.data, body.len, 0);
}

int handle_client(int client_socket) {
    ssize_t n = 0;
    char buf[1024];
    string hello_body = string_from_cstr("<h1>Hello, World!<h1>");
    string bye_body = string_from_cstr("<h1>Goodbye, World!<h1>");
    for(;;) {

        memset(buf,0,sizeof(buf));
        n = read(client_socket,buf, sizeof(buf)-1);

        if(n < 0) {
            perror("read(client)");
            return -1;
        }
        if(n == 0) {
            printf("connection closed\n");
            break;
        }
        printf("REQUEST:\n%s", buf);

        string_splits lines = split_string(buf,n,CRLF);

        if (lines.count < 1) {
            printf("Error: empty request\n");
            return -1;
        }

        http_req_line req_line = http_req_line_init();
        http_status result = parse_req_line(&req_line, lines.splits[0].start, lines.splits[0].len);
        if (result != HTTP_RES_OK) {
            printf("Error: invalid request line\n");
            return HTTP_RES_BAD_REQUEST;
        }

        string route_hello = string_from_cstr("/hello");
        string route_bye = string_from_cstr("/bye");

        if (string_equal(&req_line.uri, &route_hello)) {
            http_send_response(client_socket ,http_response_generate(buf, sizeof(buf), HTTP_RES_OK,hello_body.len), hello_body);
        }

       else if (string_equal(&req_line.uri, &route_bye)) {
           http_send_response(client_socket, http_response_generate(buf, sizeof(buf), HTTP_RES_OK, bye_body.len), bye_body);
       }
        else {
            printf("Error: unknown route: \"%.*s\"\n", (int)req_line.uri.len, req_line.uri.data);
            return -1;
        }

        free_splits(&lines);
        close(client_socket);
        break;

    }
    printf("\n--\n");
    return 0;
}
/*TCP information taken from Arch Linux TCP 'man' pages */
int main(void) {
    int rc = 0;
    struct sockaddr_in bind_addr;
    int tcp_socket = 0;
    int ret = 0;
    int client_socket = 0;
    int enabled = true;
    memset(&bind_addr, 0, sizeof(bind_addr));

    tcp_socket = socket(AF_INET,  /*IPv4 */
        SOCK_STREAM, /*TCP */
        0);

    if (tcp_socket < 0) {
        perror("socket");
        return 1;
    }

    printf("socket creation succeeded\n");

    (void)setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

    bind_addr.sin_port = htons(PORT);
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    rc = bind(tcp_socket, (const struct sockaddr*) &bind_addr, sizeof(bind_addr));
    if (rc < 0) {
        perror("bind()");
        ret = 1;
        goto exit;
    }

    printf("bind successful\n");
    rc = listen(tcp_socket, SOMAXCONN);

    if (rc < 0) {
        perror("listen()");
        ret = 1;
        goto exit;
    }

    printf("listen on http://localhost:%d\n", PORT);

    for(;;) {
        ssize_t n = 0;
        printf("waiting for connection..\n");
        client_socket = accept(tcp_socket, NULL, NULL);

        printf("got connection\n");
        rc = handle_client(client_socket);
        /*ignore errors, don't care for now */
    }
exit:
    close(tcp_socket);
    return ret;
}
