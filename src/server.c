#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "assert.h"
#include "string.h"
#include <unistd.h>
#include <sys/time.h>
#include "stdlib.h"

#include "raylib.h"
#define RAYMATH_HEADER_ONLY
#include "raymath.h"
#include "math.h"
#include "stdio.h"

#include "physics.h"

typedef struct {
    bool isActive;

    struct sockaddr_in client_address;

    Vector3 position;
    Vector2 angle;
    Vector3 size;

    GunType currentGun;

    float health;
} ServerPlayer;

Model mapModel;
Model playerModel;
float tickTime;

float GetGunTypeDamage(GunType type) {
    switch (type) {
        case GUN_BULLET:
            return 0.3f;
        case GUN_GRENADE:
            return 0.3f;
    }
}

void AddProjectile(Projectiles *projectiles, Vector3 pos, Vector3 vel, float radius, ProjectileType type, int owner) {
    projectiles->position[projectiles->count] = pos;
    projectiles->velocity[projectiles->count] = vel;
    projectiles->radius[projectiles->count] = radius;
    projectiles->lifetime[projectiles->count] = 0.0f;
    projectiles->type[projectiles->count] = type;
    projectiles->owners[projectiles->count] = owner;
    projectiles->count++;
}

void ShootProjectile(Projectiles *projectiles, ServerPlayer players[MAX_PLAYERS], int ownerID) {
    //Vector3 dir = Vector3Subtract(players[index].cameraFPS.camera.target, players[index].cameraFPS.camera.position);
    Vector3 dir = (Vector3) {0.0f, 0.0f, 1.0f};
    Matrix rot = MatrixRotateXYZ((Vector3) { players[ownerID].angle.y, PI - players[ownerID].angle.x, 0 });
    dir = Vector3Transform(dir, rot);

    Vector3 cameraOffset = { 0, 0.9f * players[ownerID].size.y, 0 };
    Vector3 eyePosition = Vector3Add(players[ownerID].position, cameraOffset);

    switch (players[ownerID].currentGun) {
        case GUN_GRENADE:
            {
                float projSpeed = 12.0f;
                float radius = 0.1f;

                AddProjectile(projectiles, eyePosition, Vector3Scale(dir, projSpeed), radius, PROJECTILE_GRENADE, ownerID);
                //printf("dir: %f,%f,%f\n", dir.x, dir.y, dir.z);
            }
            break;
        case GUN_BULLET:
            {
                Ray shootRay = { .position = eyePosition, .direction = dir };

                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (i == ownerID || !players[i].isActive) continue;

                    RayHitInfo playerHitInfo = GetCollisionRayModel(shootRay, playerModel);
                    if (playerHitInfo.hit) {
                        RayHitInfo mapHitInfo = GetCollisionRayModel(shootRay, mapModel);

                        if (mapHitInfo.hit && mapHitInfo.distance < playerHitInfo.distance) break;

                        players[i].health -= GetGunTypeDamage(GUN_BULLET);
                    }
                }
            }
            break;
    }
}

void DeleteProjectile(Projectiles *projectiles, int index) {
    projectiles->count--;
    for (int i = index; i < projectiles->count; i++) {
        projectiles->position[i] = projectiles->position[i+1];
        projectiles->velocity[i] = projectiles->velocity[i+1];
        projectiles->radius[i] = projectiles->radius[i+1];
        projectiles->lifetime[i] = projectiles->lifetime[i+1];
        projectiles->type[i] = projectiles->type[i+1];
    }
}

void UpdateProjectiles(Model mapModel, Projectiles *projectiles) {
    for (int i = 0; i < projectiles->count; i++) {
        projectiles->lifetime[i] += tickTime;

        // FIXME: dirty hack, we should fix how we add Y to position
        float tmpVelY = projectiles->velocity[i].y;
        projectiles->velocity[i].y = 0.0f;
        Vector3 nextPos = Vector3Add(projectiles->position[i], Vector3Scale(projectiles->velocity[i], tickTime));
        projectiles->velocity[i].y = tmpVelY;

        projectiles->position[i] = CollideWithMap(mapModel, projectiles->position[i], nextPos, HITBOX_SPHERE, projectiles->radius[i], COLLIDE_AND_BOUNCE, &projectiles->velocity[i]);

        bool delete = false;
        bool grounded = false;

        if (projectiles->type[i] == PROJECTILE_GRENADE) {
            ApplyGravity(mapModel, &projectiles->position[i], projectiles->radius[i], &projectiles->velocity[i], &grounded);
        }

        if (grounded) {
            assert(projectiles->type[i] != PROJECTILE_EXPLOSION);
            AddProjectile(projectiles, projectiles->position[i], Vector3Zero(), 2.0f, PROJECTILE_EXPLOSION, projectiles->owners[i]);
            delete = true;
        }

        if (projectiles->type[i] == PROJECTILE_EXPLOSION && projectiles->lifetime[i] > 1.0f) {
            delete = true;
        }

        if (delete) {
            DeleteProjectile(projectiles, i--);
        }
    }
}

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

    InitWindow(200, 200, "server.jpeg");
    mapModel = LoadModel("assets/map2.obj");
    playerModel = LoadModel("assets/human.obj");

    ServerPlayer players[MAX_PLAYERS] = {0};
    Projectiles projectiles = {0};

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
                    players[inputPacket.playerID].size = inputPacket.size;
                    players[inputPacket.playerID].currentGun = inputPacket.currentGun;
                    if (inputPacket.shoot) ShootProjectile(&projectiles, players, inputPacket.playerID);
                    break;
                default:
                    break;
            }
        }
        else {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            double currentTimestamp = (double)(1000000l * tv.tv_sec + tv.tv_usec) / 1000000.0f;
            static double previousTimestamp = 0.0f;

            tickTime = currentTimestamp - previousTimestamp;
            previousTimestamp = currentTimestamp;

            printf("%f\n", tickTime);

            //TODO: rethink this sleep
            usleep(1000);

            UpdateProjectiles(mapModel, &projectiles);

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

            int projectilesPacketSize = sizeof(ProjectilesPacket) + projectiles.count * sizeof(NetworkProjectile);
            ProjectilesPacket *projectilesPacket = malloc(projectilesPacketSize);
            projectilesPacket->type = PACKET_PROJECTILES;
            projectilesPacket->len = projectiles.count;
            for (int i = 0; i < projectiles.count; i++) {
                projectilesPacket->projectiles[i].position = projectiles.position[i];
                projectilesPacket->projectiles[i].radius = projectiles.radius[i];
                projectilesPacket->projectiles[i].type = projectiles.type[i];
            }
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (players[i].isActive) {
                    sendto(socket_fd, projectilesPacket, projectilesPacketSize, 0, (struct sockaddr *)&players[i].client_address, len);
                }
            }
            free(projectilesPacket);
        }
    }
}
