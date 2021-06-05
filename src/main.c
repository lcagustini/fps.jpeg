#include "raylib.h"
#define RAYMATH_HEADER_ONLY
#include "raymath.h"
#include "math.h"
#include "stdio.h"
#include "assert.h"
#include "time.h"
#include <sys/time.h>

#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include "types.h"

Shader shader;
int localPlayerID = -1;

void DrawProjectiles(Projectiles *projectiles) {
    for (int i = 0; i < projectiles->count; i++) {
        // TODO: maybe use DrawSphereEx to draw a sphere with less slices if performance becomes a concern
        DrawSphere(projectiles->position[i], projectiles->radius[i], YELLOW);
    }
}

void SetupGun(Gun *gun) {
    for (int i = 0; i < gun->model.materialCount; i++) {
        gun->model.materials[i].shader = shader;
    }
}

void SetupWorld(World *world) {
    world->map.materials[0].shader = shader;
}

void SetupPlayer(Player *player)
{
    for (int i = 0; i < player->model.materialCount; i++) {
        player->model.materials[i].shader = shader;
    }

    player->cameraFPS.camera.position = player->position;
    player->cameraFPS.camera.target = Vector3Add(player->position, (Vector3){0.0f, 0.0f, 1.0f});
    player->cameraFPS.camera.up = WORLD_UP_VECTOR;
    player->cameraFPS.camera.fovy = 75.0f;
    player->cameraFPS.camera.projection = CAMERA_PERSPECTIVE;

    SetCameraMode(player->cameraFPS.camera, CAMERA_CUSTOM);

    Vector3 v1 = player->cameraFPS.camera.position;
    Vector3 v2 = player->cameraFPS.camera.target;

    float dx = v2.x - v1.x;
    float dy = v2.y - v1.y;
    float dz = v2.z - v1.z;

    player->cameraFPS.targetDistance = sqrtf(dx*dx + dy*dy + dz*dz);   // Distance to target

    // Camera angle calculation
    player->cameraFPS.angle.x = atan2f(dx, dz);                        // Camera angle in plane XZ (0 aligned with Z, move positive CCW)
    player->cameraFPS.angle.y = atan2f(dy, sqrtf(dx*dx + dz*dz));      // Camera angle in plane XY (0 aligned with X, move positive CW)

    // Lock cursor for first person and third person cameras
    DisableCursor();
}

void UpdateLights(LightSystem lights) {
    SetShaderValue(shader, lights.lightsLenLoc, &lights.lightsLen, SHADER_UNIFORM_INT);
    SetShaderValueV(shader, lights.lightsPositionLoc, &lights.lightsPosition, SHADER_UNIFORM_VEC3, lights.lightsLen);
    SetShaderValueV(shader, lights.lightsColorLoc, &lights.lightsColor, SHADER_UNIFORM_VEC3, lights.lightsLen);
}

void UpdatePlayer(int index, Projectiles *projectiles, Player *players, int playersLen, Model mapModel) {
    bool isLocalPlayer = index == localPlayerID;

    players[index].model.transform = MatrixScale(1.0f, 1.0f, 1.0f);
    players[index].model.transform = MatrixMultiply(players[index].model.transform, MatrixRotateXYZ((Vector3) { 0, PI - players[index].cameraFPS.angle.x, 0 }));
    players[index].model.transform = MatrixMultiply(players[index].model.transform, MatrixTranslate(players[index].position.x, players[index].position.y, players[index].position.z));

    Vector3 cameraOffset = { 0, 0.9f * players[index].size.y, 0 };
    players[index].cameraFPS.camera.position = Vector3Add(players[index].position, cameraOffset);
    players[index].cameraFPS.camera.target = Vector3Add(players[index].cameraFPS.camera.target, cameraOffset);
    Vector3 tmp = Vector3Subtract(players[index].cameraFPS.camera.target, players[index].cameraFPS.camera.position);

    players[index].currentGun.model.transform = MatrixScale(5.0f, 5.0f, 5.0f);
    if (isLocalPlayer) players[index].currentGun.model.transform = MatrixMultiply(players[index].currentGun.model.transform, MatrixTranslate(-players[index].size.x, 0.0f, players[index].size.z));
    else players[index].currentGun.model.transform = MatrixMultiply(players[index].currentGun.model.transform, MatrixTranslate(-1.5f * players[index].size.x, 0.0f, players[index].size.z));
    Matrix rot = MatrixRotateXYZ((Vector3) { players[index].cameraFPS.angle.y, PI - players[index].cameraFPS.angle.x, 0 });
    players[index].currentGun.model.transform = MatrixMultiply(players[index].currentGun.model.transform, rot);
    if (isLocalPlayer) players[index].currentGun.model.transform = MatrixMultiply(players[index].currentGun.model.transform, MatrixTranslate(0.0f, 0.85f * players[index].size.y, 0.0f));
    else players[index].currentGun.model.transform = MatrixMultiply(players[index].currentGun.model.transform, MatrixTranslate(0.0f, 0.0f, 0.0f));
    players[index].currentGun.model.transform = MatrixMultiply(players[index].currentGun.model.transform, MatrixTranslate(players[index].position.x, players[index].position.y, players[index].position.z));

    /*
    if (isLocalPlayer && IsKeyPressed(players[index].inputBindings[SHOOT])) {
        Vector3 dir = Vector3Subtract(players[index].cameraFPS.camera.target, players[index].cameraFPS.camera.position);
        switch (players[index].currentGun.type) {
            case GUN_GRENADE: {
                float projSpeed = 12.0f;
                float radius = 0.1f;
                AddProjectile(projectiles, players[index].cameraFPS.camera.position, Vector3Scale(dir, projSpeed), radius);
                //printf("dir: %f,%f,%f\n", dir.x, dir.y, dir.z);
                break;
            }
            case GUN_BULLET: {
                Ray shootRay = { .position = players[index].cameraFPS.camera.position, .direction = dir };

                for (int i = 0; i < playersLen; i++) {
                    if (i == index) continue;

                    RayHitInfo playerHitInfo = GetCollisionRayModel(shootRay, players[i].model);
                    if (playerHitInfo.hit) {
                        RayHitInfo mapHitInfo = GetCollisionRayModel(shootRay, mapModel);

                        if (mapHitInfo.hit && mapHitInfo.distance < playerHitInfo.distance) break;

                        players[i].health -= players[index].currentGun.damage;

                        puts("hit");
                    }
                }
                break;
            }
        }
    }
    */
}

int main(void) {
    InitWindow(1280, 720, "fps.jpeg");
    //InitWindow(GetMonitorWidth(0), GetMonitorHeight(0), "fps.jpeg");
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    //SetConfigFlags(FLAG_FULLSCREEN_MODE);
    SetTargetFPS(GetMonitorRefreshRate(0));

    shader = LoadShader("shaders/lighting.vs", "shaders/lighting.fs");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");

    World world = {
        .map = LoadModel("assets/map2.obj"),
        .players = {
            {
                .model = LoadModel("assets/human.obj"),
                .position = (Vector3){ 4.0f, 1.0f, 4.0f },
                .size = (Vector3) { 0.15f, 0.75f, 0.15f },
                .inputBindings = { 'W', 'S', 'D', 'A', ' ', 'E' },
                .health = MAX_HEALTH,
                .currentGun = {
                    .model = LoadModel("assets/machinegun.obj"),
                    .type = GUN_GRENADE,
                    .damage = 0.3f,
                }
            },
            {
                .model = LoadModel("assets/human.obj"),
                .position = (Vector3){ 4.0f, 1.0f, 4.0f },
                .size = (Vector3) { 0.15f, 0.75f, 0.15f },
                .inputBindings = { 'W', 'S', 'D', 'A', ' ', 'E' },
                .health = MAX_HEALTH,
                .currentGun = {
                    .model = LoadModel("assets/machinegun.obj"),
                    .type = GUN_GRENADE,
                    .damage = 0.3f,
                }
            }
        },
        .playersLen = 2,
        .lights = {
            .lightsLenLoc = GetShaderLocation(shader, "lightsLen"),
            .lightsPositionLoc = GetShaderLocation(shader, "lightsPosition"),
            .lightsColorLoc = GetShaderLocation(shader, "lightsColor"),

            .lightsPosition = { (Vector3) { 2.0, 3.0, 2.0 }, (Vector3) { -2.0, 4.0, -3.0 } },
            .lightsColor = { (Vector3) { 0.6, 0.5, 0.4 }, (Vector3) { 0.5, 0.5, 0.5 } },
            .lightsLen = 2,
        }
    };

    SetupWorld(&world);
    for (int i = 0; i < world.playersLen; i++) {
        SetupPlayer(&world.players[i]);
        SetupGun(&world.players[i].currentGun);
    }

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    fcntl(socket_fd, F_SETFL, O_NONBLOCK);
    struct sockaddr_in server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(20586);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    {
        JoinPacket joinPacket = { PACKET_JOIN };
        sendto(socket_fd, &joinPacket, sizeof(joinPacket), 0, (struct sockaddr *)&server_address, sizeof(server_address));
    }

    while (!WindowShouldClose()) {
        PacketType type = 0;
        recv(socket_fd, &type, sizeof(type), MSG_PEEK);

        switch (type) {
            case PACKET_PLAYER_LIST:
                {
                    PlayerListPacket playerListPacket = {0};
                    recv(socket_fd, &playerListPacket, sizeof(playerListPacket), 0);
                    localPlayerID = playerListPacket.clientId;

                    printf("Got id %d\n", localPlayerID);
                    for (int i = 0; i < playerListPacket.allIdsLen; i++) {
                        printf("Id %d is online\n", playerListPacket.allIds[i]);
                    }
                }
                break;
            default:
                if (type != 0) printf("got %d\n", type);
                break;
        }

        if (localPlayerID == -1) continue;

        for (int i = 0; i < world.playersLen; i++) {
            //if (i == localPlayerID) MovePlayer(world.map, &world.players[i]);
            //else ApplyGravity(world.map, &world.players[i].position, world.players[i].size.y, &world.players[i].velocity, &world.players[i].grounded);

            UpdatePlayer(i, &world.projectiles, world.players, world.playersLen, world.map);
        }

        if (IsKeyPressed(world.players[0].inputBindings[SHOOT])) {
            ShootPacket shoot = { PACKET_SHOOT };
            sendto(socket_fd, &shoot, sizeof(shoot), 0, (struct sockaddr *)&server_address, sizeof(server_address));
        }

        //UpdateProjectiles(world.map, &world.projectiles);

        UpdateLights(world.lights);

        BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode3D(world.players[localPlayerID].cameraFPS.camera);

        int timeLoc = GetShaderLocation(shader, "time");
        struct timeval tv;
        gettimeofday(&tv, NULL);
        float timestamp = (float) tv.tv_usec / 100000; // calculate milliseconds
        SetShaderValue(shader, timeLoc, &timestamp, SHADER_UNIFORM_FLOAT);

        // section UV[0..0.5][0..0.5] is a procedurally generated wall texture
        int isMapLoc = GetShaderLocation(shader, "isMap");
        int isMap = 1;

        SetShaderValue(shader, isMapLoc, &isMap, SHADER_UNIFORM_INT);
        DrawModel(world.map, (Vector3) {0}, 1.0f, WHITE);
        isMap = 0;
        SetShaderValue(shader, isMapLoc, &isMap, SHADER_UNIFORM_INT);

        for (int i = 0; i < world.playersLen; i++) {
            DrawModel(world.players[i].model, Vector3Zero(), 1.0f, WHITE);
            DrawCubeWires(world.players[i].position, 2 * world.players[i].size.x, 2 * world.players[i].size.y, 2 * world.players[i].size.z, BLUE);
            DrawModel(world.players[i].currentGun.model, Vector3Zero(), 1.0f, WHITE);
        }

        DrawProjectiles(&world.projectiles);

        EndMode3D();

        for (int i = 0; i < world.playersLen; i++) {
            DrawRectangleGradientH(10, i * 25 + 10, 20 * world.players[i].health, 20, RED, GREEN);
        }

        DrawLine(GetScreenWidth() / 2 - 6, GetScreenHeight() / 2, GetScreenWidth() / 2 + 5, GetScreenHeight() / 2, MAGENTA);
        DrawLine(GetScreenWidth() / 2, GetScreenHeight() / 2 - 6, GetScreenWidth() / 2, GetScreenHeight() / 2 + 5, MAGENTA);

        EndDrawing();
    }

    UnloadModel(world.map);
    for (int i = 0; i < world.playersLen; i++) {
        UnloadModel(world.players[i].model);
        UnloadModel(world.players[i].currentGun.model);
    }

    CloseWindow();

    return 0;
}
