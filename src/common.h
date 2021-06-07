
double gettimestamp() {
#ifdef _WIN32
    LARGE_INTEGER fq, t;
    QueryPerformanceFrequency(&fq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / fq.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)(tv.tv_sec + tv.tv_usec / 1000000.0);
#endif
}

void socketInit() {
#ifdef _WIN32
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != NO_ERROR) {
        puts("Failed to initialized winsock.");
    }
#endif
}

void socketClose(SOCKET socket_fd) {
#ifdef _WIN32
    int status = shutdown(socket_fd, SD_BOTH);
    if (status == 0) { status = closesocket(socket_fd); }
#endif
}

void checkClientState() {
    // TODO: check error for linux recvfrom as well
#ifdef _WIN32
    int socket_err = WSAGetLastError();
    if (socket_err && socket_err != WSAEWOULDBLOCK) printf("err = %d\n", WSAGetLastError());
#endif
}

void checkServerState() {
    // TODO: check error for linux recvfrom as well
#ifdef _WIN32
    int socket_err = WSAGetLastError();
    if (socket_err && socket_err != WSAEWOULDBLOCK && socket_err != WSAECONNRESET) printf("err = %d\n", WSAGetLastError());
#endif
}

void validateSocket(SOCKET socket_fd) {
#ifdef _WIN32
    if (socket_fd == INVALID_SOCKET) {
        puts("Failed to create socket.");
    }
#endif
}

void setupSocket(SOCKET socket_fd) {
#ifdef __linux__
    fcntl(socket_fd, F_SETFL, O_NONBLOCK);
#else
    u_long iMode = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, &iMode, sizeof(int)) != NO_ERROR) {
        printf("Failed to set socket option.\n");
        return 0;
    }
    int iResult;
    if ((iResult = ioctlsocket(socket_fd, FIONBIO, &iMode)) != NO_ERROR) {
        printf("ioctlsocket failed with error: %ld\n", iResult);
        return 0;
    }
#endif
}

// out_packet_len should be NULL for non-dynamic size packets
#define MAX_UDP_PACKET_SIZE (250 * sizeof(int))
int peekPacket(SOCKET socket_fd, struct sockaddr_in *addr, PacketType *type, int *out_packet_len) {
    socklen_t tmp = sizeof(struct sockaddr_in);

    // dirty hack for this to work on winsock
    int dgram[MAX_UDP_PACKET_SIZE / sizeof(int)] = { PACKET_ERROR };
    int bytesRead = recvfrom(socket_fd, dgram, MAX_UDP_PACKET_SIZE, MSG_PEEK, (struct sockaddr *) addr, &tmp);
    //if (bytesRead != -1) printf("bytesRead = %d\n", bytesRead);

    if (dgram[0] == PACKET_PROJECTILES) {
        memset(dgram, 0, sizeof(dgram));
        recvfrom(socket_fd, &dgram, MAX_UDP_PACKET_SIZE, MSG_PEEK, (struct sockaddr*) addr, &tmp);

        //assert(out_packet_len);
        if (out_packet_len) *out_packet_len = dgram[1];
    }

    checkClientState();

    if (type) *type = dgram[0];

    return bytesRead;
}
