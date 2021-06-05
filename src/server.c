#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include "assert.h"

#include "raylib.h"
#define RAYMATH_HEADER_ONLY
#include "raymath.h"
#include "math.h"
#include "stdio.h"

#include "physics.h"

void ApplyGravity(Model mapModel, Vector3 *position, float radius, Vector3 *velocity, bool *grounded) {
    *velocity = Vector3Subtract(*velocity, (Vector3) {0.0f, 3.0f * GetFrameTime(), 0.0f});
    Vector3 nextPos = *position;
    nextPos.y += velocity->y * GetFrameTime();
    *position = CollideWithMapGravity(mapModel, nextPos, radius, velocity, grounded);
}

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

void MovePlayer(Model mapModel, Player *player) {
    static Vector2 previousMousePosition = { 0.0f, 0.0f };

    Vector2 mousePositionDelta = { 0.0f, 0.0f };
    Vector2 mousePosition = GetMousePosition();
    float mouseWheelMove = GetMouseWheelMove();

    bool inputs[5] = { IsKeyDown(player->inputBindings[MOVE_FRONT]),
        IsKeyDown(player->inputBindings[MOVE_BACK]),
        IsKeyDown(player->inputBindings[MOVE_RIGHT]),
        IsKeyDown(player->inputBindings[MOVE_LEFT]) };

    mousePositionDelta.x = mousePosition.x - previousMousePosition.x;
    mousePositionDelta.y = mousePosition.y - previousMousePosition.y;

    previousMousePosition = mousePosition;

    float deltaX = (sinf(player->cameraFPS.angle.x) * inputs[MOVE_BACK] -
            sinf(player->cameraFPS.angle.x) * inputs[MOVE_FRONT] -
            cosf(player->cameraFPS.angle.x) * inputs[MOVE_LEFT] +
            cosf(player->cameraFPS.angle.x) * inputs[MOVE_RIGHT]) / PLAYER_MOVEMENT_SENSITIVITY;

    float deltaZ = (cosf(player->cameraFPS.angle.x) * inputs[MOVE_BACK] -
            cosf(player->cameraFPS.angle.x) * inputs[MOVE_FRONT] +
            sinf(player->cameraFPS.angle.x) * inputs[MOVE_LEFT] -
            sinf(player->cameraFPS.angle.x) * inputs[MOVE_RIGHT]) / PLAYER_MOVEMENT_SENSITIVITY;

    // TODO: this whole code won't work with ceilings, we need to integrate gravity with CollideWithMap()

    Vector3 frameMovement = {0};
    frameMovement.x = deltaX * GetFrameTime() / PLAYER_MOVEMENT_SENSITIVITY;
    frameMovement.z = deltaZ * GetFrameTime() / PLAYER_MOVEMENT_SENSITIVITY;

    float tmpVelY = player->velocity.y;
    player->velocity.y = 0.0f;
    player->velocity = Vector3Scale(Vector3Normalize(frameMovement), GetFrameTime() / PLAYER_MOVEMENT_SENSITIVITY);
    player->velocity.y = tmpVelY;

    if (player->grounded && IsKeyDown(player->inputBindings[MOVE_JUMP])) {
        player->grounded = false;
        player->velocity.y = 3.0f;
    }

    ApplyGravity(mapModel, &player->position, player->size.y, &player->velocity, &player->grounded);

    tmpVelY = player->velocity.y;
    player->velocity.y = 0.0f;
    Vector3 nextPos = Vector3Add(player->position, player->velocity);
    player->position = CollideWithMap(mapModel, player->position, nextPos, HITBOX_AABB, player->size.x, COLLIDE_AND_SLIDE, NULL);
    player->velocity.y = tmpVelY;

    // Camera orientation calculation
    player->cameraFPS.angle.x += (mousePositionDelta.x * -CAMERA_MOUSE_MOVE_SENSITIVITY);
    player->cameraFPS.angle.y += (mousePositionDelta.y * -CAMERA_MOUSE_MOVE_SENSITIVITY);

    // Angle clamp
    if (player->cameraFPS.angle.y > CAMERA_FIRST_PERSON_MIN_CLAMP * DEG2RAD) player->cameraFPS.angle.y = CAMERA_FIRST_PERSON_MIN_CLAMP * DEG2RAD;
    else if (player->cameraFPS.angle.y < CAMERA_FIRST_PERSON_MAX_CLAMP * DEG2RAD) player->cameraFPS.angle.y = CAMERA_FIRST_PERSON_MAX_CLAMP * DEG2RAD;

    // Recalculate camera target considering translation and rotation
    Matrix translation = MatrixTranslate(0, 0, (player->cameraFPS.targetDistance));
    Matrix rotation = MatrixRotateXYZ((Vector3) { PI*2 - player->cameraFPS.angle.y, PI*2 - player->cameraFPS.angle.x, 0 });
    Matrix transform = MatrixMultiply(translation, rotation);

    // TODO: investigate
    player->cameraFPS.camera.target.x = player->position.x - transform.m12;
    player->cameraFPS.camera.target.y = player->position.y - transform.m13;
    player->cameraFPS.camera.target.z = player->position.z - transform.m14;
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

    while (true) {
        struct sockaddr_in client_address;
        socklen_t len = sizeof(client_address);

        PacketType type = PACKET_ERROR;
        recvfrom(socket_fd, &type, sizeof(type), MSG_PEEK, (struct sockaddr *)&client_address, &len);

        if (type != PACKET_ERROR) printf("%d\n", type);

        switch (type) {
            case PACKET_JOIN:
                JoinPacket joinPacket = {0};
                recv(socket_fd, &joinPacket, sizeof(joinPacket), 0);
                printf("joining\n");

                PlayerListPacket playerListPacket = {
                    .type = PACKET_PLAYER_LIST,
                    .allIds = {
                        0
                    },
                    .allIdsLen = 1,
                    .clientId = 0,
                };
                printf("sending\n");
                sendto(socket_fd, &playerListPacket, sizeof(playerListPacket), 0, (struct sockaddr *)&client_address, len);
                break;
            default:
                break;
        }
    }
}
