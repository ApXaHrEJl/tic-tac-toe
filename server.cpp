#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fstream>
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
#include <utility>
#include <vector>

constexpr int BUFLEN = 1024;

struct Params {
    int game_id;
    int sockMain;
    std::vector<std::pair<int, std::string>>* db;
    int p1_id;
    int p2_id;
};

struct ControlParams {
    int sockMain;
    std::vector<std::pair<int, std::string>>* db;
};

void sendmsg(int sock, std::string message, struct sockaddr_in& client)
{
    if ((sendto(sock,
                message.c_str(),
                message.length() + 1,
                0,
                (struct sockaddr*)&client,
                sizeof(client)))
        < 0) {
        perror("SERVER: sendto error\n");
        exit(1);
    }
}

void recvmsg(int sock, char* message, struct sockaddr_in& client)
{
    bzero(message, BUFLEN);
    unsigned int length = sizeof(client);
    recvfrom(sock, message, BUFLEN, 0, (struct sockaddr*)&client, &length);
}

int server_init(struct sockaddr_in& servAddr, bool show_port)
{
    int sockMain;
    if ((sockMain = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Cannot open UDP socket.");
        exit(1);
    }
    unsigned int length = sizeof(servAddr);
    bzero((char*)&servAddr, length);
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = 0;
    if (bind(sockMain, (struct sockaddr*)&servAddr, length)) {
        perror("Bad binding.");
        exit(1);
    }
    if (getsockname(sockMain, (struct sockaddr*)&servAddr, &length)) {
        perror("Bad getsockname.");
        exit(1);
    }
    if (show_port) {
        std::cout << "SERVER: Port: " << ntohs(servAddr.sin_port) << "\n";
    }
    return sockMain;
}

std::string genmsg(char code, char* field)
{
    std::string res;
    res += code;
    for (int i = 0; i < 9; i++) {
        res += field[i];
    }
    return res;
}

int check_game(char* field)
{
    if ((field[0] == '1' && field[1] == '1' && field[2] == '1')
        || (field[3] == '1' && field[4] == '1' && field[5] == '1')
        || (field[6] == '1' && field[7] == '1' && field[8] == '1')
        || (field[0] == '1' && field[3] == '1' && field[6] == '1')
        || (field[1] == '1' && field[4] == '1' && field[7] == '1')
        || (field[2] == '1' && field[5] == '1' && field[8] == '1')
        || (field[0] == '1' && field[4] == '1' && field[8] == '1')
        || (field[2] == '1' && field[4] == '1' && field[6] == '1'))
        return 1;

    if ((field[0] == '2' && field[1] == '2' && field[2] == '2')
        || (field[3] == '2' && field[4] == '2' && field[5] == '2')
        || (field[6] == '2' && field[7] == '2' && field[8] == '2')
        || (field[0] == '2' && field[3] == '2' && field[6] == '2')
        || (field[1] == '2' && field[4] == '2' && field[7] == '2')
        || (field[2] == '2' && field[5] == '2' && field[8] == '2')
        || (field[0] == '2' && field[4] == '2' && field[8] == '2')
        || (field[2] == '2' && field[4] == '2' && field[6] == '2'))
        return 2;

    if ((field[0] != '0') && (field[1] != '0') && (field[2] != '0')
        && (field[3] != '0') && (field[4] != '0') && (field[5] != '0')
        && (field[6] != '0') && (field[7] != '0') && (field[8] != '0'))
        return 3;
    return 0;
}

int turn1(
        int id,
        int sock,
        struct sockaddr_in& client,
        struct sockaddr_in& p1,
        struct sockaddr_in& p2,
        char* buf,
        char* field)
{
    if (client.sin_port == p1.sin_port) {
        if ((buf[0] >= '1') && (buf[0] <= '9')) {
            if (field[buf[0] - '1'] == '0') {
                field[buf[0] - '1'] = '1';
                std::cout << "GAME " << id
                          << " SERVER: Player 1 made his move\n";
                if (check_game(field) == 0) {
                    sendmsg(sock, genmsg('b', field), p1);
                    sendmsg(sock, genmsg('a', field), p2);
                    return 2;
                } else if (check_game(field) == 1) {
                    sendmsg(sock, genmsg('d', field), p1);
                    sendmsg(sock, genmsg('e', field), p2);
                    std::cout << "GAME " << id
                              << " SERVER: Game over - Player 1 won\n";
                    return 3;
                } else if (check_game(field) == 3) {
                    sendmsg(sock, genmsg('f', field), p1);
                    sendmsg(sock, genmsg('f', field), p2);
                    std::cout << "GAME " << id << " SERVER: Game over - Draw\n";
                    return 5;
                }
            } else {
                sendmsg(sock, genmsg('c', field), p1);
                std::cout << "GAME " << id
                          << " SERVER: Player 1 made the wrong move\n";
            }
        } else {
            std::cout << "GAME " << id << " SERVER: Player 1 wrote a message \""
                      << buf << "\" to chat\n";
            sendmsg(sock, genmsg('a', field) + '1' + "You: " + buf, p1);
            sendmsg(sock, genmsg('b', field) + '1' + "Opponent: " + buf, p2);
        }
    } else if (client.sin_port == p2.sin_port) {
        std::cout << "GAME " << id << " SERVER: Player 2 wrote a message \""
                  << buf << "\" to chat\n";
        sendmsg(sock, genmsg('a', field) + '1' + "Opponent: " + buf, p1);
        sendmsg(sock, genmsg('b', field) + '1' + "You: " + buf, p2);
    }
    return 1;
}

int turn2(
        int id,
        int sock,
        struct sockaddr_in& client,
        struct sockaddr_in& p1,
        struct sockaddr_in& p2,
        char* buf,
        char* field)
{
    if (client.sin_port == p2.sin_port) {
        if ((buf[0] >= '1') && (buf[0] <= '9')) {
            if (field[buf[0] - '1'] == '0') {
                field[buf[0] - '1'] = '2';
                std::cout << "GAME " << id
                          << " SERVER: Player 2 made his move\n";
                if (check_game(field) == 0) {
                    sendmsg(sock, genmsg('a', field), p1);
                    sendmsg(sock, genmsg('b', field), p2);
                    return 1;
                } else if (check_game(field) == 2) {
                    sendmsg(sock, genmsg('e', field), p1);
                    sendmsg(sock, genmsg('d', field), p2);
                    std::cout << "GAME " << id
                              << " SERVER: Game over - Player 2 won\n";
                    return 4;
                } else if (check_game(field) == 3) {
                    sendmsg(sock, genmsg('f', field), p1);
                    sendmsg(sock, genmsg('f', field), p2);
                    std::cout << "GAME " << id << " SERVER: Game over - Draw\n";
                    return 5;
                }
            } else {
                sendmsg(sock, genmsg('c', field), p2);
                std::cout << "GAME " << id
                          << " SERVER: Player 2 made the wrong move\n";
            }
        } else {
            sendmsg(sock, genmsg('b', field) + '1' + "Opponent: " + buf, p1);
            sendmsg(sock, genmsg('a', field) + '1' + "You: " + buf, p2);
            std::cout << "GAME " << id << " SERVER: Player 2 wrote a message \""
                      << buf << "\" to chat\n";
        }
    } else if (client.sin_port == p1.sin_port) {
        sendmsg(sock, genmsg('b', field) + '1' + "You: " + buf, p1);
        sendmsg(sock, genmsg('a', field) + '1' + "Opponent: " + buf, p2);
        std::cout << "GAME " << id << " SERVER: Player 1 wrote a message \""
                  << buf << "\" to chat\n";
    }
    return 2;
}

void clear_field(char* field)
{
    for (int i = 0; i < 9; i++) {
        field[i] = '0';
    }
}

void* game(void* arg)
{
    struct Params p = *((Params*)arg);
    int game_id = p.game_id;
    int sockMain = p.sockMain;
    std::vector<std::pair<int, std::string>>& db = *(p.db);
    int p1_id = p.p1_id;
    int p2_id = p.p2_id;
    delete (Params*)arg;

    char buf[BUFLEN];
    char field[10] = "000000000";
    int game = 1;
    struct sockaddr_in client, p1, p2;
    recvmsg(sockMain, buf, client);
    if (std::stoi(buf) == p1_id) {
        p1 = client;
    } else {
        p2 = client;
    }
    recvmsg(sockMain, buf, client);
    if (std::stoi(buf) == p2_id) {
        p2 = client;
    } else {
        p1 = client;
    }
    sendmsg(sockMain, genmsg('a', field), p1);
    sendmsg(sockMain, genmsg('b', field), p2);

    while (true) {
        if (game < 3) {
            recvmsg(sockMain, buf, client);
        }
        int cur_game = game;
        switch (cur_game) {
        case 1:
            game = turn1(game_id, sockMain, client, p1, p2, buf, field);
            break;
        case 2:
            game = turn2(game_id, sockMain, client, p1, p2, buf, field);
            break;
        case 3:

            db[p1_id].first += 1;
            db[p2_id].first -= 1;

            close(sockMain);
            return nullptr;
        case 4:

            db[p1_id].first -= 1;
            db[p2_id].first += 1;

            close(sockMain);
            return nullptr;
        case 5:
            close(sockMain);
            return nullptr;
        }
    }
    close(sockMain);
    return nullptr;
}

void* control(void* arg)
{
    struct ControlParams cp = *((ControlParams*)arg);
    int sockMain = cp.sockMain;
    std::vector<std::pair<int, std::string>>& db = *(cp.db);
    std::string command;
    while (true) {
        std::cin >> command;
        if (command == "save") {
            std::ofstream out("database.db");
            out << db.size() << " ";
            for (auto it : db) {
                out << it.first << " " << it.second << " ";
            }
            std::cout << "SERVER: Database saved in database.db\n";
            continue;
        }
        if (command == "exit") {
            close(sockMain);
            return nullptr;
        }
    }
    return nullptr;
}

int main()
{
    std::vector<std::pair<int, std::string>> db;
    std::ifstream in("database.db");
    if (!in) {
        std::cout << "SERVER: Database not found\n";
    }
    long unsigned i = 0, db_size = 0;
    in >> db_size;
    for (i = 0; i < db_size; i++) {
        int raiting;
        std::string name;
        in >> raiting >> name;
        db.push_back({raiting, name});
    }
    if (i > 0) {
        std::cout << "SERVER: Succesfully loaded " << i
                  << " users from database\n";
    }
    std::vector<std::pair<int, struct sockaddr_in>> lobby;
    char buf[BUFLEN];
    int games = 0;
    struct sockaddr_in servAddr, client;
    int sockMain = server_init(servAddr, true);

    pthread_t c_th;
    pthread_attr_t c_ta;
    pthread_attr_init(&c_ta);
    pthread_attr_setdetachstate(&c_ta, PTHREAD_CREATE_DETACHED);

    ControlParams* cp = new ControlParams[sizeof(ControlParams)];
    cp->sockMain = sockMain;
    cp->db = &db;
    if (pthread_create(&c_th, &c_ta, control, (void*)cp) < 0) {
        perror("SERVER: Cannot launch control thread\n");
        exit(1);
    }
    while (true) {
        recvmsg(sockMain, buf, client);
        if (!strcmp(buf, "exit")) {
            return 0;
        }
        if (!strcmp(buf, "show_users")) {
            sendmsg(sockMain, std::to_string(db.size()), client);
            for (auto it : db) {
                sendmsg(sockMain,
                        it.second + ": " + std::to_string(it.first),
                        client);
            }
            std::cout << "SERVER: Sent database to user from port "
                      << client.sin_port << "\n";
            continue;
        }
        std::cout << "SERVER: " << buf << " connected\n";
        for (i = 0; i < db.size(); i++) {
            if (!strcmp(buf, db[i].second.c_str())) {
                std::cout << buf << "'s raiting: " << db[i].first << "\n";
                break;
            }
        }
        if (i == db.size()) {
            db.push_back({0, buf});
            std::cout << buf << "'s raiting: " << db[i].first << "\n";
        }
        bool flag = false;
        for (long unsigned j = 0; j < lobby.size(); j++) {
            if (abs(db[i].first - db[lobby[j].first].first) < 5) {
                flag = true;
                games++;

                struct sockaddr_in gameAddr;
                int sockGame = server_init(gameAddr, false);
                sendmsg(sockMain, std::to_string(i), client);
                sendmsg(sockMain, std::to_string(gameAddr.sin_port), client);
                sendmsg(sockMain,
                        std::to_string(lobby[j].first),
                        lobby[j].second);
                sendmsg(sockMain,
                        std::to_string(gameAddr.sin_port),
                        lobby[j].second);

                pthread_t th;
                pthread_attr_t ta;
                pthread_attr_init(&ta);
                pthread_attr_setdetachstate(&ta, PTHREAD_CREATE_DETACHED);

                Params* pp = new Params[sizeof(Params)];
                pp->game_id = games;
                pp->sockMain = sockGame;
                pp->db = &db;
                pp->p1_id = i;
                pp->p2_id = lobby[j].first;
                if (pthread_create(&th, &ta, game, (void*)pp) < 0) {
                    perror("SERVER: Thread not started.\n");
                    exit(1);
                }

                if (j != lobby.size() - 1) {
                    lobby[j] = lobby.back();
                }
                lobby.resize(lobby.size() - 1);
                break;
            }
        }
        if (!flag) {
            lobby.push_back({i, client});
            sendmsg(sockMain, "Connected. Looking for opponent", client);
        }
    }
    return 0;
}
