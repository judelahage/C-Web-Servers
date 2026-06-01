#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "route.h"
DWORD WINAPI myThreadFunction(LPVOID arg);

struct ThreadData{
    SOCKET client;
    const char* rootdir;
};

int main(){

    //server setup
    const char* ROOTDIR = "Z:/Documents/Business/Tutoring/Lahage-Tutoring-Website";
    WSADATA wsadata;
    if(WSAStartup(MAKEWORD(2, 2), &wsadata) != 0){
        printf("WSAStartup failed\n");
        return 1;
    }
    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    if(server == INVALID_SOCKET){
        printf("socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);
    printf("Starting server on port 8080...\n");
    if(bind(server, (struct sockaddr*)&addr, sizeof(addr)) != 0){
        printf("bind failed: %d\n", WSAGetLastError());
        closesocket(server);
        WSACleanup();
        return 1;
    }
    if(listen(server, 10) != 0){
        printf("listen failed: %d\n", WSAGetLastError());
        closesocket(server);
        WSACleanup();
        return 1;
    }

    //now the receiving part of the server is setup, we can start listening for connections, so we make a client socket.

    while(1){
        printf("Waiting for connections...\n");

        SOCKET client = accept(server, 0, 0); //create the client socket and make sure its valid.
        if(client == INVALID_SOCKET){
            printf("accept failed: %d\n", WSAGetLastError());
            closesocket(server);
            WSACleanup();
            return 1;
        }

        struct ThreadData* data = malloc(sizeof(struct ThreadData)); //allocate some memory for the curent thread
        //make sure that there is available memory allocated. if there isn't available memory, then malloc would give us NULL
        if(data == NULL){
            closesocket(client);
            continue; //instead of crashing the server we just continue on to the next connection
        }

        data->client = client;
        data->rootdir = ROOTDIR;
        HANDLE H = CreateThread(NULL, 0, myThreadFunction, data, 0, NULL);
        CloseHandle(H);
    }

    closesocket(server);
    WSACleanup();
    return 0;
}

DWORD WINAPI myThreadFunction(LPVOID arg){
    struct ThreadData* data = (struct ThreadData*)arg;

    SOCKET c = data->client;
    const char* root = data->rootdir;

    char* request = malloc(8192);
    if(request == NULL){
        send(c, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n", 35, 0);
        closesocket(c);
        free(request);
        free(data);
        return 0;
    }
    recv(c, request, 8192, 0);
    printf("Request recieved: %s\n", request);
    route(c, request, root);
    closesocket(c);
    free(request);
    free(data); //we free data here, even though it was malloced in main, since we don't know when we are done with it until the thread is done with it.
    return 0;
}

