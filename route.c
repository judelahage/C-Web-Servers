#include "route.h"
#include <winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void route(SOCKET client, char *request, const char* rootdir){

    if(memcmp(request, "GET /", 5) != 0) send(client, "HTTP/1.1 405 Method Not Allowed\r\nAllow: GET\r\nContent-Length: 0\r\n\r\n", 66, 0); return;   // ignore non-GET requests

    // ── 1. EXTRACT PATH FROM REQUEST ─────────────────────────────────────────
    // Request looks like:  GET /styles.css HTTP/1.1
    // We find the / and the space after it to isolate just the path portion.
    char filepath[100] = "";
    char *slash = strchr(request, '/');
    char *space = strchr(slash, ' ');
    strncpy(filepath, slash, space - slash);
    filepath[space - slash] = '\0';

    // ── 2. NORMALIZE PATH ────────────────────────────────────────────────────
    // Map directory-style URLs to their index.html files:
    //   /           →  /index.html
    //   /blog/      →  /blog/index.html
    //   /blog       →  /blog/index.html  (no extension = treat as directory)
    if(strcmp(filepath, "/") == 0){
        strcpy(filepath, "/index.html");
    }
    if(filepath[strlen(filepath) - 1] == '/'){
        strcat(filepath, "index.html");
    }

    char fullpath[300];
    sprintf(fullpath, "%s%s", rootdir, filepath);

    // ── 3. DETECT CONTENT TYPE BY FILE EXTENSION ─────────────────────────────
    // strrchr finds the LAST dot in the path, giving us the file extension.
    // Text formats use "r" (text mode), binary formats use "rb" (binary mode).
    char msg[150] = {0};
    const char *contentType = NULL;
    const char *openType    = "r";

    char* fileExtension = strrchr(fullpath, '.');

    if(fileExtension == NULL){
        // No extension means a directory path like /blog — serve its index.html
        strcat(fullpath, "/index.html");
        contentType = "text/html";
    }
    else if(strcmp(fileExtension, ".html")        == 0){ contentType = "text/html"; }
    else if(strcmp(fileExtension, ".css")         == 0){ contentType = "text/css"; }
    else if(strcmp(fileExtension, ".js")          == 0){ contentType = "application/javascript"; }
    else if(strcmp(fileExtension, ".webmanifest") == 0){ contentType = "application/manifest+json"; }
    else if(strcmp(fileExtension, ".png")         == 0){ contentType = "image/png";       openType = "rb"; }
    else if(strcmp(fileExtension, ".jpg")         == 0 ||
            strcmp(fileExtension, ".jpeg")        == 0){ contentType = "image/jpeg";      openType = "rb"; }
    else if(strcmp(fileExtension, ".ico")         == 0){ contentType = "image/x-icon";    openType = "rb"; }
    else if(strcmp(fileExtension, ".webp")        == 0){ contentType = "image/webp";      openType = "rb"; }
    else if(strcmp(fileExtension, ".svg")         == 0){ contentType = "image/svg+xml";   openType = "rb"; }
    else {
        const char* err = "HTTP/1.1 415 Unsupported Media Type\r\nContent-Length: 0\r\n\r\n";
        send(client, err, (int)strlen(err), 0);
        return;
    }

    // ── 4. OPEN AND SERVE THE FILE ────────────────────────────────────────────
    // fseek/ftell measures the exact file size so we can allocate a perfectly
    // sized buffer and send the correct Content-Length header.
    FILE* f = fopen(fullpath, openType);
    if(f){
        fseek(f, 0, SEEK_END);
        long filesize = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* buffer = malloc(filesize);

        sprintf(msg, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n",
                contentType, filesize);
        send(client, msg, (int)strlen(msg), 0);
        send(client, buffer, fread(buffer, 1, filesize, f), 0);
        fclose(f);
        free(buffer);
    } else {
        const char* err = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client, err, (int)strlen(err), 0);
    }
}
