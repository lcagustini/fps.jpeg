#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "assert.h"
#include "string.h"
#include <unistd.h>

#include "raylib.h"
#define RAYMATH_HEADER_ONLY
#include "raymath.h"
#include "math.h"
#include "stdio.h"

#include "physics.h"

void AddProjectile(Projectiles *projectiles, Vector3 pos, Vector3 vel, float radius) {
    projectiles->position[projectiles->count] = pos;
    projectiles->velocity[projectiles->count] = vel;
    projectiles->radius[projectiles->count] = radius;
    projectiles->count++;
}

void DeleteProjectile(Projectiles *projectiles, int index) {
    projectiles->count--;
    for (int i = index; i < projectiles->count; i++) {
        projectiles->position[i] = projectiles->position[i+1];
        projectiles->velocity[i] = projectiles->velocity[i+1];
    }
}

void UpdateProjectiles(Model mapModel, Projectiles *projectiles) {
    for (int i = 0; i < projectiles->count; i++) {
        // FIXME: dirty hack, we should fix how we add Y to position
        float tmpVelY = projectiles->velocity[i].y;
        projectiles->velocity[i].y = 0.0f;
        Vector3 nextPos = Vector3Add(projectiles->position[i], Vector3Scale(projectiles->velocity[i], GetFrameTime()));
        projectiles->velocity[i].y = tmpVelY;

        projectiles->position[i] = CollideWithMap(mapModel, projectiles->position[i], nextPos, HITBOX_SPHERE, projectiles->radius[i], COLLIDE_AND_BOUNCE, &projectiles->velocity[i]);

        bool grounded = false;
        ApplyGravity(mapModel, &projectiles->position[i], projectiles->radius[i], &projectiles->velocity[i], &grounded);

        if (grounded) {
            DeleteProjectile(projectiles, i--);
        }
    }
}

typedef struct {
    bool isActive;

    struct sockaddr_in client_address;

    Vector3 position;
    Vector2 angle;

    GunType currentGun;

    float health;
} ServerPlayer;

void AddPlayer(ServerPlayer players[MAX_PLAYERS], struct sockaddr_in client) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].isActive) continue;

        memset(&players[i], 0, sizeof(ServerPlayer));

        players[i].isActive = true;
        players[i].client_address = client;
        players[i].health = MAX_HEALTH;

        return;
    }
}

void main() {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    fcntl(socket_fd, F_SETFL, O_NONBLOCK);
    struct sockaddr_in server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(20586);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        fprintf(stderr, "ERROR: Could not bind file descriptor to socket.\n");
    }
    else {
        fprintf(stderr, "Could bind file descriptor to socket.\n");
    }

    ServerPlayer players[MAX_PLAYERS] = {0};

    while (true) {
        struct sockaddr_in client_address;
        socklen_t len = sizeof(client_address);

        PacketType type = PACKET_ERROR;
        int bytesRead = recvfrom(socket_fd, &type, sizeof(type), MSG_PEEK, (struct sockaddr *)&client_address, &len);
        if (bytesRead > 0) {
            switch (type) {
                case PACKET_JOIN:
                    JoinPacket joinPacket = {0};
                    recv(socket_fd, &joinPacket, sizeof(joinPacket), 0);

                    AddPlayer(players, client_address);

                    PlayerListPacket playerListPacket;
                    playerListPacket.type = PACKET_PLAYER_LIST;
                    playerListPacket.allIdsLen = 0;
                    for (int i = 0; i < MAX_PLAYERS; i++) {
                        if (players[i].isActive) {
                            playerListPacket.allIds[playerListPacket.allIdsLen++] = i;
                        }
                    }

                    for (int i = 0; i < MAX_PLAYERS; i++) {
                        if (players[i].isActive) {
                            playerListPacket.clientId = i;
                            sendto(socket_fd, &playerListPacket, sizeof(playerListPacket), 0, (struct sockaddr *)&players[i].client_address, len);
                        }
                    }
                    break;
                case PACKET_INPUT:
                    InputPacket inputPacket = {0};
                    recv(socket_fd, &inputPacket, sizeof(inputPacket), 0);
                    //TODO: check if player id and client address are as expected
                    players[inputPacket.playerID].position = inputPacket.position;
                    players[inputPacket.playerID].angle = inputPacket.angle;
                    players[inputPacket.playerID].currentGun = inputPacket.currentGun;
                    if (inputPacket.shoot) puts("pew");
                    break;
                default:
                    break;
            }
        }
        else {
            //TODO: rethink this sleep
            usleep(1000);

            //TODO: simulate
            StatePacket statePacket = { PACKET_STATE };
            for (int i = 0; i < MAX_PLAYERS; i++) {
                statePacket.playersPositions[i] = players[i].position;
                statePacket.playersAngles[i] = players[i].angle;
                statePacket.playersGuns[i] = players[i].currentGun;
                statePacket.playersHealth[i] = players[i].health;
            }
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (players[i].isActive) {
                    sendto(socket_fd, &statePacket, sizeof(statePacket), 0, (struct sockaddr *)&players[i].client_address, len);
                }
            }
        }
    }
}
