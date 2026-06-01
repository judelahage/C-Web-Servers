// ── CONFIGURATION ────────────────────────────────────────────────────────────────
#define WIN32_LEAN_AND_MEAN
#define THREAD_COUNT 32     // one worker per CPU core
#define QUEUE_SIZE   256    // max pending connections before main blocks
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// ── SHARED QUEUE STATE ────────────────────────────────────────────────────────────
// Global so every worker thread and main() can all see the same queue.
// queue_head = next slot to read from, queue_tail = next slot to write into.
// Wrapping with % QUEUE_SIZE makes it a circular buffer (head/tail loop around).
SOCKET           queue[QUEUE_SIZE];
int              queue_head = 0;
int              queue_tail = 0;
CRITICAL_SECTION queue_lock;   // mutex: only one thread reads/writes the queue at a time
HANDLE           semaphore;    // counter: incremented by main when work is added,
                               //          decremented by workers when work is taken

// ── FORWARD DECLARATIONS ─────────────────────────────────────────────────────────
#include "route.h"
DWORD WINAPI workerThread(LPVOID arg);


// ════════════════════════════════════════════════════════════════════════════════
//  MAIN  —  sets up the server, launches the thread pool, then loops accepting
//           connections and handing them off to worker threads via the queue.
// ════════════════════════════════════════════════════════════════════════════════
int main(){

    const char* ROOTDIR = "Z:/Documents/Business/Tutoring/Lahage-Tutoring-Website";

    // ── 1. WINSOCK INITIALIZATION ─────────────────────────────────────────────
    // Windows requires WSAStartup before any socket calls can be made.
    WSADATA wsadata;
    if(WSAStartup(MAKEWORD(2, 2), &wsadata) != 0){
        printf("WSAStartup failed\n");
        return 1;
    }

    // ── 2. SERVER SOCKET SETUP ────────────────────────────────────────────────
    // Create a TCP socket, bind it to port 8080 on all interfaces, and start
    // listening. These are one-time setup steps — done before the accept loop.
    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    if(server == INVALID_SOCKET){
        printf("socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(8080);
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

    // ── 3. THREAD POOL INITIALIZATION ────────────────────────────────────────
    // Must one time initialize semaphores and lock BEFORE creating threads, otherwise a
    // thread could try to use them before they exist.
    semaphore = CreateSemaphore(NULL, 0, QUEUE_SIZE, NULL);
    InitializeCriticalSection(&queue_lock);
    HANDLE threads[THREAD_COUNT];
    for(int i = 0; i < THREAD_COUNT; i++){
        threads[i] = CreateThread(NULL, 0, workerThread, (LPVOID)ROOTDIR, 0, NULL);
    }

    // ── 4. ACCEPT LOOP ───────────────────────────────────────────────────────
    // Main thread does nothing but accept connections and put them in the queue.
    // Worker threads do all the actual recv/route/send work.
    while(1){
        printf("Waiting for connections...\n");

        SOCKET client = accept(server, 0, 0);
        if(client == INVALID_SOCKET){
            printf("accept failed: %d\n", WSAGetLastError());
            closesocket(server);
            WSACleanup();
            return 1;
        }

        EnterCriticalSection(&queue_lock); //we enter the critical section because we have found some work to do. 
        queue[queue_tail] = client; //we add the client socket to the end of the filled part of the queue
        queue_tail = (queue_tail + 1) % QUEUE_SIZE; //increment the queue tail by one. modulus is so we can wrap back to zero when we hit the end
        LeaveCriticalSection(&queue_lock);
        ReleaseSemaphore(semaphore, 1, NULL);   // wake one worker
    }

    closesocket(server);
    WSACleanup();
    return 0;
}


// ════════════════════════════════════════════════════════════════════════════════
//  WORKER THREAD  —  sits idle until main signals work is available, then grabs
//                    one connection from the queue and handles it fully before
//                    looping back to wait again.
// ════════════════════════════════════════════════════════════════════════════════
DWORD WINAPI workerThread(LPVOID arg){
    const char* rootdir = (const char*)arg;

    while(1){

        // ── WAIT FOR WORK ─────────────────────────────────────────────────────
        // Blocks here (uses zero CPU) until main calls ReleaseSemaphore.
        WaitForSingleObject(semaphore, INFINITE);

        // ── GRAB SOCKET FROM QUEUE ────────────────────────────────────────────
        // Lock is held only for the two queue lines — released immediately after
        // so other workers can grab their own connections in parallel.
        EnterCriticalSection(&queue_lock);
        SOCKET client = queue[queue_head];
        queue_head = (queue_head + 1) % QUEUE_SIZE;
        LeaveCriticalSection(&queue_lock);

        // ── HANDLE CONNECTION ─────────────────────────────────────────────────
        char* request = calloc(1, 8192);    // calloc zeros memory so strings terminate cleanly
        if(request == NULL){
            const char* err = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
            send(client, err, (int)strlen(err), 0);
            closesocket(client);
            return 0;
        }
        recv(client, request, 8192, 0);
        printf("Request received: %s\n", request);
        route(client, request, rootdir);
        closesocket(client);
        free(request);
    }
    return 0;
}


