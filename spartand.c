#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <syslog.h>

#define BUFFER_SIZE 4096
#define MAX_PATH_LEN 1024
#define MAX_REQUEST_LINE 4096

static void send_success(const char* mimetype) {
    printf("2 %s\r\n", mimetype);
    fflush(stdout);
}

static void send_redirect(const char* path) __attribute__((unused));
static void send_redirect(const char* path) {
    printf("3 %s\r\n", path);
    fflush(stdout);
}

static void send_client_error(const char* msg) {
    printf("4 %s\r\n", msg);
    fflush(stdout);
}

static void send_server_error(const char* msg) {
    printf("5 %s\r\n", msg);
    fflush(stdout);
}

static const char* get_mimetype(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    ext++;
    if (strcmp(ext, "gmi") == 0 || strcmp(ext, "gemini") == 0)
        return "text/gemini; charset=utf-8";
    if (strcmp(ext, "txt") == 0)
        return "text/plain; charset=utf-8";
    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(ext, "xml") == 0)
        return "text/xml; charset=utf-8";
    if (strcmp(ext, "css") == 0)
        return "text/css; charset=utf-8";
    if (strcmp(ext, "js") == 0)
        return "application/javascript";
    if (strcmp(ext, "json") == 0)
        return "application/json";
    if (strcmp(ext, "png") == 0)
        return "image/png";
    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, "gif") == 0)
        return "image/gif";
    if (strcmp(ext, "svg") == 0)
        return "image/svg+xml";
    if (strcmp(ext, "pdf") == 0)
        return "application/pdf";
    return "application/octet-stream";
}

static int serve_file(const char* filepath) {
    FILE* fp = fopen(filepath, "rb");
    if (!fp) {
        return 1;
    }

    send_success(get_mimetype(filepath));

    unsigned char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        fwrite(buffer, 1, bytes, stdout);
    }

    fclose(fp);
    fflush(stdout);
    return 0;
}

static int execute_cgi(const char* filepath, const char* host, int port,
                       const char* client_ip, long content_length) {
    char port_str[16];
    char content_length_str[32];
    snprintf(port_str, sizeof(port_str), "%d", port);
    snprintf(content_length_str, sizeof(content_length_str), "%ld", content_length);

    pid_t pid = fork();
    if (pid < 0) {
        return 1;
    }

    if (pid == 0) {
        setenv("SERVER_NAME", host, 1);
        setenv("SERVER_PORT", port_str, 1);
        setenv("DATA_LENGTH", content_length_str, 1);
        setenv("REMOTE_ADDR", client_ip, 1);

        execl(filepath, filepath, (char*)NULL);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    fflush(stdout);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return 0;
    }
    return 1;
}

static int serve_or_execute(const char* filepath, const struct stat* st,
                             const char* host, int port,
                             const char* client_ip, long content_length) {
    if (st->st_mode & S_IXUSR) {
        return execute_cgi(filepath, host, port, client_ip, content_length);
    }
    return serve_file(filepath);
}

static int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static char* find_index_file(const char* dirpath) {
    DIR* dir = opendir(dirpath);
    if (!dir) {
        return NULL;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "index.", 6) == 0) {
            char fullpath[MAX_PATH_LEN];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
            struct stat st;
            if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode)) {
                closedir(dir);
                return strdup(fullpath);
            }
        }
    }

    closedir(dir);
    return NULL;
}

static int serve_directory(const char* dirpath, const char* request_path,
                            const char* host, int port,
                            const char* client_ip, long content_length) {
    char* index_path = find_index_file(dirpath);
    if (index_path) {
        struct stat ist;
        int result;
        if (stat(index_path, &ist) == 0) {
            result = serve_or_execute(index_path, &ist, host, port, client_ip, content_length);
        } else {
            result = serve_file(index_path);
        }
        free(index_path);
        return result;
    }

    DIR* dir = opendir(dirpath);
    if (!dir) {
        return 1;
    }

    char** entries = NULL;
    size_t entry_count = 0;
    size_t entry_capacity = 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        if (entry_count >= entry_capacity) {
            entry_capacity = entry_capacity == 0 ? 16 : entry_capacity * 2;
            char** tmp = realloc(entries, entry_capacity * sizeof(char*));
            if (!tmp) {
                closedir(dir);
                for (size_t i = 0; i < entry_count; i++) free(entries[i]);
                free(entries);
                return 1;
            }
            entries = tmp;
        }

        entries[entry_count] = strdup(entry->d_name);
        if (!entries[entry_count]) {
            closedir(dir);
            for (size_t i = 0; i < entry_count; i++) free(entries[i]);
            free(entries);
            return 1;
        }
        entry_count++;
    }
    closedir(dir);

    if (entry_count > 0) {
        qsort(entries, entry_count, sizeof(char*), compare_strings);
    }

    char base_path[MAX_PATH_LEN];
    if (request_path[strlen(request_path) - 1] == '/') {
        snprintf(base_path, sizeof(base_path), "%s", request_path);
    } else {
        snprintf(base_path, sizeof(base_path), "%s/", request_path);
    }

    send_success("text/gemini; charset=utf-8");

    for (size_t i = 0; i < entry_count; i++) {
        char fullpath[MAX_PATH_LEN];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entries[i]);

        struct stat st;
        if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
            printf("=> %s%s/\n", base_path, entries[i]);
        } else {
            printf("=> %s%s\n", base_path, entries[i]);
        }

        free(entries[i]);
    }

    free(entries);
    fflush(stdout);
    return 0;
}

static int normalize_path(const char* base_dir, const char* request_path, char* result, size_t result_size) {
    char temp_path[MAX_PATH_LEN];

    if (!request_path || !request_path[0] || strcmp(request_path, "/") == 0) {
        snprintf(temp_path, sizeof(temp_path), "%s/", base_dir);
    } else {
        const char* path_start = request_path;
        if (path_start[0] == '/') {
            path_start++;
        }
        snprintf(temp_path, sizeof(temp_path), "%s/%s", base_dir, path_start);
    }

    char* canonical = realpath(temp_path, NULL);
    if (!canonical) {
        return -1;
    }

    char* base_canonical = realpath(base_dir, NULL);
    if (!base_canonical) {
        free(canonical);
        return -1;
    }

    size_t base_len = strlen(base_canonical);
    int is_safe = (strncmp(canonical, base_canonical, base_len) == 0);

    if (is_safe) {
        snprintf(result, result_size, "%s", canonical);
    }

    free(canonical);
    free(base_canonical);

    return is_safe ? 0 : -1;
}

static void url_decode(const char* src, char* dst, size_t dst_size) {
    size_t di = 0;
    for (size_t si = 0; src[si] && di + 1 < dst_size; si++) {
        if (src[si] == '%' && isxdigit((unsigned char)src[si+1]) && isxdigit((unsigned char)src[si+2])) {
            char hex[3] = { src[si+1], src[si+2], '\0' };
            dst[di++] = (char)strtol(hex, NULL, 16);
            si += 2;
        } else {
            dst[di++] = src[si];
        }
    }
    dst[di] = '\0';
}

static void log_request(const char* ip, const char* host, const char* path, int status) {
    syslog(LOG_INFO, "%s \"%s%s\" %d", ip, host, path, status);
}

int main(int argc, char* argv[]) {
    openlog("spartand", LOG_PID, LOG_DAEMON);

    const char* client_ip = getenv("TCPREMOTEIP");
    if (!client_ip) {
        client_ip = getenv("REMOTE_ADDR");
    }
    if (!client_ip) {
        client_ip = "0.0.0.0";
    }

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <directory> [port]\n", argv[0]);
        closelog();
        return 1;
    }

    const char* serve_dir = argv[1];
    int port = 300;
    if (argc == 3) {
        char* ep;
        long pval = strtol(argv[2], &ep, 10);
        if (*ep != '\0' || pval < 1 || pval > 65535) {
            fprintf(stderr, "Error: invalid port '%s'\n", argv[2]);
            closelog();
            return 1;
        }
        port = (int)pval;
    }

    struct stat st;
    if (stat(serve_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a valid directory\n", serve_dir);
        closelog();
        return 1;
    }

    char request_line[MAX_REQUEST_LINE];
    size_t len = 0;
    while (len < sizeof(request_line) - 1) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0) break;
        if (c == '\n') break;
        if (c != '\r') {
            request_line[len++] = c;
        }
    }
    request_line[len] = '\0';
    if (len == 0) {
        send_client_error("Empty request");
        log_request(client_ip, "", "", 4);
        closelog();
        return 1;
    }

    char* p = request_line;

    char* host = p;
    char* space1 = strchr(p, ' ');
    if (!space1) {
        send_client_error("Bad request: missing path");
        log_request(client_ip, host, "", 4);
        closelog();
        return 1;
    }
    *space1 = '\0';
    p = space1 + 1;

    char* path_raw = p;
    char* space2 = strchr(p, ' ');
    if (!space2) {
        send_client_error("Bad request: missing content-length");
        log_request(client_ip, host, path_raw, 4);
        closelog();
        return 1;
    }
    *space2 = '\0';
    p = space2 + 1;

    char* endptr;
    long content_length = strtol(p, &endptr, 10);
    if (*endptr != '\0' || content_length < 0) {
        send_client_error("Bad request: invalid content-length");
        log_request(client_ip, host, path_raw, 4);
        closelog();
        return 1;
    }

    char path_decoded[MAX_PATH_LEN];
    url_decode(path_raw, path_decoded, sizeof(path_decoded));

    char fullpath[MAX_PATH_LEN];
    if (normalize_path(serve_dir, path_decoded, fullpath, sizeof(fullpath)) != 0) {
        send_client_error("Path not found");
        log_request(client_ip, host, path_decoded, 4);
        closelog();
        return 1;
    }

    if (stat(fullpath, &st) != 0) {
        send_client_error("Not found");
        log_request(client_ip, host, path_decoded, 4);
        closelog();
        return 1;
    }

    int result;
    if (S_ISDIR(st.st_mode)) {
        result = serve_directory(fullpath, path_decoded, host, port, client_ip, content_length);
        if (result != 0) {
            send_server_error("Failed to read directory");
        }
        log_request(client_ip, host, path_decoded, result == 0 ? 2 : 5);
    } else if (S_ISREG(st.st_mode)) {
        result = serve_or_execute(fullpath, &st, host, port, client_ip, content_length);
        if (result != 0) {
            send_server_error("Failed to serve file");
        }
        log_request(client_ip, host, path_decoded, result == 0 ? 2 : 5);
    } else {
        send_server_error("Unsupported resource type");
        log_request(client_ip, host, path_decoded, 5);
        result = 1;
    }

    closelog();
    return result;
}
