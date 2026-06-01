#ifndef ROUTE_H
#define ROUTE_H
#include <winsock2.h>
void route(SOCKET client, char *request, const char *rootdir);
#endif
