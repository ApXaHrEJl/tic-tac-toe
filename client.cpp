#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <string>
#include <strings.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

constexpr int BUFLEN = 1024;

void reaper(int sig)
{
    int status;
    while (wait3(&status, WNOHANG, (struct rusage*)0) >= 0)
        ;
}

void sendmsg(int sock, std::string message, struct sockaddr_in& server)
{
    sendto(sock,
           message.c_str(),
           message.length() + 1,
           0,
           (struct sockaddr*)&server,
           sizeof(server));
}

void recvmsg(int sock, char* message, struct sockaddr_in& server)
{
    bzero(message, BUFLEN);
    unsigned int length = sizeof(server);
    recvfrom(sock, message, BUFLEN, 0, (struct sockaddr*)&server, &length);
}

void sender(int sock, struct sockaddr_in& server)
{
    while (true) {
        std::string sbuf;
        getline(std::cin, sbuf);
        sendmsg(sock, sbuf, server);
    }
}

void encode_servmsg(char code)
{
    switch (code) {
    case 'a':
        std::cout << "Your turn";
        return;
    case 'b':
        std::cout << "Waiting for the opponent";
        return;
    case 'c':
        std::cout << "Wrong turn - Try again";
        return;
    case 'd':
        std::cout << "You won! Raiting: +1";
        return;
    case 'e':
        std::cout << "You lose! Raiting: -1";
        return;
    case 'f':
        std::cout << "Game over - Draw";
        return;
    }
}

void draw_interface(char* buf, std::vector<std::string>& chat)
{
    std::cout << "\E[2J";
    std::cout << "\E[8;18;36;t";
    std::cout << "\E[0;14H"; //перейти на Х и на У
    for (int i = 1; i < 10; i++) {
        if (buf[i] == '1') {
            std::cout << "X";
        } else if (buf[i] == '2') {
            std::cout << "O";
        } else {
            std::cout << " ";
        }
        if ((i == 3) || (i == 6)) {
            std::cout << "\n            ";
            std::cout << "-----------\n";
            std::cout << "             ";
        } else if (i != 9) {
            std::cout << " | ";
        }
    }
    std::cout << "\n____________________________________\n";
    encode_servmsg(buf[0]);
    std::cout << "\n____________________________________\n";
    for (auto it : chat) {
        std::cout << it << "\n";
    }
    std::cout << "\E[16;0H";
    std::cout << "____________________________________\n";
}

void receiver(
        int sock,
        struct sockaddr_in& server,
        char* buf,
        std::vector<std::string>& chat)
{
    draw_interface(buf, chat);
    while (true) {
        recvmsg(sock, buf, server);
        if (buf[10] == '1') {
            if (chat.size() < 8) {
                chat.push_back(buf + 11);
            } else {
                for (int i = 0; i < 7; i++) {
                    chat[i] = chat[i + 1];
                }
                chat[7] = buf + 11;
            }
        }
        draw_interface(buf, chat);
        if ((buf[0] >= 'd') && (buf[0] <= 'f')) {
            return;
        }
    }
}

int main(int argc, char* argv[])
{
    int sock;
    unsigned int length;
    char buf[BUFLEN];
    std::vector<std::string> chat;
    std::string sbuf = argv[3];
    struct sockaddr_in server, clientAddr;
    struct hostent* hp;
    if (argc < 4) {
        std::cout << "Недостаточное количество аргументов\n";
        exit(1);
    }
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cout << "He могу получить socket\n";
        exit(1);
    }
    length = sizeof(server);
    bzero((char*)&server, length);
    server.sin_family = AF_INET;
    hp = gethostbyname(argv[1]);
    bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
    server.sin_port = htons(atoi(argv[2]));
    length = sizeof(clientAddr);
    bzero((char*)&clientAddr, length);
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    clientAddr.sin_port = 0;
    if (bind(sock, (struct sockaddr*)&clientAddr, length)) {
        std::cout << "Клиент не получил порт.\n";
        exit(1);
    }
    length = sizeof(server);
    sendmsg(sock, sbuf, server);
    if (sbuf == "show_users") {
        recvmsg(sock, buf, server);
        unsigned long size = std::stoi(buf);
        std::cout << "Total users: " << size << "\n|USER, RAITING|\n";
        for (unsigned long i = 0; i < size; i++) {
            recvmsg(sock, buf, server);
            std::cout << buf << "\n";
        }
        close(sock);
        return 0;
    }
    
    while (true) {
        recvmsg(sock, buf, server);
        if (isdigit(buf[0])) {
            break;
        }
        std::cout << buf << "\n";
    }
    std::string id = buf;
    recvmsg(sock, buf, server);
    server.sin_port = atoi(buf);
    sendmsg(sock, id, server);

    signal(SIGCHLD, reaper);
    int child = fork();
    if (child != 0) {
        recvmsg(sock, buf, server);
        receiver(sock, server, buf, chat);
        close(sock);
        kill(child, SIGKILL);
    } else {
        sender(sock, server);
    }
    return 0;
}

