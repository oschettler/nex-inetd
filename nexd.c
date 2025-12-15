#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>

#define BUFFER_SIZE 4096
#define MAX_PATH_LEN 1024

/* Serve a regular file */
static int serve_file(const char* filepath) {
    FILE* fp = fopen(filepath, "rb");
    if (!fp) {
        return 1;
    }
    
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        fwrite(buffer, 1, bytes, stdout);
    }
    
    fclose(fp);
    fflush(stdout);
    return 0;
}

/* Compare function for qsort */
static int compare_strings(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

/* Serve a directory listing */
static int serve_directory(const char* dirpath, const char* request_path) {
    DIR* dir = opendir(dirpath);
    if (!dir) {
        return 1;
    }
    
    /* Collect entries */
    char** entries = NULL;
    size_t entry_count = 0;
    size_t entry_capacity = 0;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip hidden files */
        if (entry->d_name[0] == '.') {
            continue;
        }
        
        if (entry_count >= entry_capacity) {
            entry_capacity = entry_capacity == 0 ? 16 : entry_capacity * 2;
            entries = realloc(entries, entry_capacity * sizeof(char*));
            if (!entries) {
                closedir(dir);
                return 1;
            }
        }
        
        entries[entry_count] = strdup(entry->d_name);
        if (!entries[entry_count]) {
            closedir(dir);
            return 1;
        }
        entry_count++;
    }
    closedir(dir);
    
    /* Sort entries */
    if (entry_count > 0) {
        qsort(entries, entry_count, sizeof(char*), compare_strings);
    }
    
    /* Ensure request path ends with / */
    char base_path[MAX_PATH_LEN];
    if (request_path[strlen(request_path) - 1] == '/') {
        snprintf(base_path, sizeof(base_path), "%s", request_path);
    } else {
        snprintf(base_path, sizeof(base_path), "%s/", request_path);
    }
    
    /* Output directory listing */
    for (size_t i = 0; i < entry_count; i++) {
        /* Build full path to check if it's a directory */
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

/* Normalize path and check for directory traversal */
static int normalize_path(const char* base_dir, const char* request_path, char* result, size_t result_size) {
    char temp_path[MAX_PATH_LEN];
    
    /* Handle empty or root path */
    if (!request_path || !request_path[0] || strcmp(request_path, "/") == 0) {
        snprintf(temp_path, sizeof(temp_path), "%s/", base_dir);
    } else {
        /* Remove leading slash if present */
        const char* path_start = request_path;
        if (path_start[0] == '/') {
            path_start++;
        }
        snprintf(temp_path, sizeof(temp_path), "%s/%s", base_dir, path_start);
    }
    
    /* Resolve to canonical path */
    char* canonical = realpath(temp_path, NULL);
    if (!canonical) {
        return -1;
    }
    
    /* Check that the canonical path is within base_dir */
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

int main(int argc, char* argv[]) {
    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
        return 1;
    }
    
    const char* serve_dir = argv[1];
    
    /* Verify directory exists and is accessible */
    struct stat st;
    if (stat(serve_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a valid directory\n", serve_dir);
        return 1;
    }
    
    /* Read request from stdin */
    char request[MAX_PATH_LEN];
    if (!fgets(request, sizeof(request), stdin)) {
        return 1;
    }
    
    /* Remove \r\n from end */
    size_t len = strlen(request);
    while (len > 0 && (request[len - 1] == '\r' || request[len - 1] == '\n')) {
        request[--len] = '\0';
    }
    
    /* Normalize and validate path */
    char fullpath[MAX_PATH_LEN];
    if (normalize_path(serve_dir, request, fullpath, sizeof(fullpath)) != 0) {
        return 1;
    }
    
    /* Check if path exists */
    if (stat(fullpath, &st) != 0) {
        return 1;
    }
    
    /* Serve file or directory */
    if (S_ISDIR(st.st_mode)) {
        return serve_directory(fullpath, request);
    } else if (S_ISREG(st.st_mode)) {
        return serve_file(fullpath);
    } else {
        return 1;
    }
    
    return 0;
}
