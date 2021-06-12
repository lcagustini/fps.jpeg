#include "raylib.h"

#define RAYMATH_HEADER_ONLY
#include "raymath.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

#include <math.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__

#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <arpa/inet.h>
typedef int SOCKET;
#include <pthread.h>

#else

#include "windows_hacks.h"

#endif

#include "physics.h"
#include "common.h"

#include "server.h"

Shader shader;
int localPlayerID = -1;
GameScreen currentScreen;

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

void DrawProjectiles(NetworkProjectile *projectiles, int len) {
    for (int i = 0; i < len; i++) {
        // TODO: maybe use DrawSphereEx to draw a sphere with less slices if performance becomes a concern
        DrawSphere(projectiles[i].position, projectiles[i].radius, YELLOW);
    }
}

Gun SetupGun(GunType type) {
    Gun gun = {
        .model = LoadModel("assets/machinegun.obj"),
        .type = type,
    };

    for (int i = 0; i < gun.model.materialCount; i++) {
        gun.model.materials[i].shader = shader;
    }

    return gun;
}

void SetupWorld(World *world) {
    world->map.materials[0].shader = shader;
}

void SetupPlayer(Player *player) {
    player->model = LoadModel("assets/human.obj");
    player->position = (Vector3) { 4.0f, 1.0f, 4.0f };
    player->size = (Vector3) { 0.15f, 0.75f, 0.15f };
    char bindings[INPUT_ALL] = { 'W', 'S', 'D', 'A', ' ', 'E' };
    memcpy(player->inputBindings, bindings, sizeof(bindings));
    player->health = MAX_HEALTH;
    player->currentGun = SetupGun(GUN_GRENADE);

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

void UpdatePlayer(int index, Player *players, Model mapModel) {
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
}

#include "screen_lobby.h"
#include "screen_game.h"

int main(void) {
    socketInit();

    InitWindow(1280, 720, "fps.jpeg");
    //InitWindow(GetMonitorWidth(0), GetMonitorHeight(0), "fps.jpeg");
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    //SetConfigFlags(FLAG_FULLSCREEN_MODE);
    SetTargetFPS(GetMonitorRefreshRate(0));

    shader = LoadShader("shaders/lighting.vs", "shaders/lighting.fs");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");

    currentScreen = SCREEN_LOBBY;

    while (true) {
        switch (currentScreen) {
            case SCREEN_LOBBY:
                currentScreen = lobbyMain();
                break;
            case SCREEN_GAME:
                currentScreen = gameMain();
                break;
            default:
                goto close_game;
        }
    }
close_game:

    CloseWindow();

    return 0;
}
