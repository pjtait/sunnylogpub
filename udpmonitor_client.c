// udpmonitor_client.cc

#ifdef __cplusplus
#include <iostream>
#include <sstream>
#else
#include <stdio.h>
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "udpmonitor_client.h"

#ifdef __cplusplus
using namespace std;
#endif

static int init_socket(struct sockaddr **, int *);
static void errno_msg(char *);

/* Copy a message to the UDP monitor server */
void send_udpmonitor(const char *fac, const char *msg)
{
    static struct sockaddr *servaddrp;
    static int servaddrlen;
    static int inited = 0;
    static int sock;
#ifndef __cplusplus
    char msgbuff[1024];
    int msglen;
#endif

    if (!inited)
    {
	if ((sock = init_socket(&servaddrp, &servaddrlen)) < 0)
	    return;
        inited = 1;
    }
#ifdef __cplusplus
    ostringstream msgbuff;
    msgbuff << fac << ": " << msg;
    string msgstr = msgbuff.str();
    if (sendto(sock, msgstr.c_str(), msgstr.length(), 0,
               servaddrp, servaddrlen) < 0)
        errno_msg("sendto");
#else
    if ((msglen = snprintf(msgbuff, sizeof msgbuff, "%s: %s", fac, msg)) < 0)
    {
        fprintf(stderr, "send_udpmonitor: snprintf\n");
        return;
    }
    if (sendto(sock, msgbuff, msglen, 0, servaddrp, servaddrlen) < 0)
        errno_msg("sendto");
#endif
}

// One-time socket initialisation.
// Fills-in addr and length pointers to servaddr, returns socket fd, or -1
// for error.
static int init_socket(struct sockaddr **servaddrp, int *servaddrlenp)
{
    struct addrinfo hints, *addr, *addrsave;
    char *port_str;
    char *addr_str;
    int res;
    int sock;
    const int on = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if ((addr_str = getenv("UDPMONITOR_ADDR")) == NULL)
        addr_str = "fmos02";
    if ((port_str = getenv("UDPMONITOR_PORT")) == NULL)
        port_str = "65001";
    res = getaddrinfo(addr_str, port_str, &hints, &addr);
    if (res != 0)
    {
#ifdef __cplusplus
        cout << "init_socket: getaddrinfo: " << gai_strerror(res) << endl;
        cout.flush();
#else
        fprintf(stderr, "init_socket: getaddrinfo: %s\n", gai_strerror(res));
#endif
        return -1;
    }
    addrsave = addr;
    do {		
        sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock >= 0)
            break;
    } while ((addr = addr->ai_next) != NULL);
    if (addr == NULL)
            errno_msg("socket");
    if ((*servaddrp = (struct sockaddr *)malloc(addr->ai_addrlen)) == NULL)
    {
#ifdef __cplusplus
        cout << "init_socket: malloc failed" << endl;
        cout.flush();
#else
        fprintf(stderr, "init_socket: malloc failed\n");
#endif
        return -1;
    }
    memcpy(*servaddrp, addr->ai_addr, addr->ai_addrlen);
    *servaddrlenp = addr->ai_addrlen;
    freeaddrinfo(addrsave);
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST|SO_REUSEADDR,
                     &on, sizeof on))
    {
#ifdef __cplusplus
        cout << "init_socket: setsockopt failed" << endl;
        cout.flush();
#else
        fprintf(stderr, "init_socket: setsockopt failed\n");
#endif
        return -1;
    } 
    return sock;
}

static void errno_msg(char *msg)
{
#ifdef __cplusplus
    std::cout << "send_udpmonitor: " << msg << " " << strerror(errno);
    std::cout.flush();
#else
        fprintf(stderr, "send_udpmonitor: %s: '%s'\n", msg, strerror(errno));
#endif
}

#ifdef TEST_UDPMONITOR_CLIENT
int main(int argc, char **argv)
{
    int i;
#ifdef __cplusplus
    ostringstream msgbuff;
#else
    char msg[1024];
#endif

    for (i = 0; ; ++i)
    {
#ifdef __cplusplus
        msgbuff << "test message " << " " << i;
        string msgstr = msgbuff.str();
	send_udpmonitor("IRS1", msgstr.c_str());
#else
        snprintf(msg, sizeof msg, "test message %d", i);
	send_udpmonitor("IRS1", msg);
#endif
        usleep(1000);
    }
}
#endif
