#include "raylib.h"
#define RAYMATH_HEADER_ONLY
#include "raymath.h"
#include "math.h"
#include "stdio.h"

#define CAMERA_FREE_PANNING_DIVIDER                     5.1f

#define CAMERA_FIRST_PERSON_FOCUS_DISTANCE              25.0f
#define CAMERA_FIRST_PERSON_MIN_CLAMP                   89.0f
#define CAMERA_FIRST_PERSON_MAX_CLAMP                  -89.0f

#define CAMERA_FIRST_PERSON_STEP_TRIGONOMETRIC_DIVIDER  8.0f
#define CAMERA_FIRST_PERSON_STEP_DIVIDER                30.0f
#define CAMERA_FIRST_PERSON_WAVING_DIVIDER              200.0f

#define PLAYER_MOVEMENT_SENSITIVITY                     0.25f

#define CAMERA_MOUSE_MOVE_SENSITIVITY                   0.003f
#define CAMERA_MOUSE_SCROLL_SENSITIVITY                 1.5f

#define PLAYER_RADIUS 0.4f

#define MAX_HEALTH 10.0f

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define WORLD_UP_VECTOR (Vector3) {0.0f, 1.0f, 0.0f}

#include "physics.h"

typedef enum {
    MOVE_FRONT = 0,
    MOVE_BACK,
    MOVE_RIGHT,
    MOVE_LEFT,
    MOVE_JUMP,
    SHOOT,

    INPUT_ALL,
} InputAction;

typedef struct {
    Camera camera;
    Vector2 angle;
    float targetDistance;
} CameraFPS;

typedef struct {
    Model model;
    float damage;
} Gun;

typedef struct {
    Model model;

    Vector3 position;
    Vector3 target;

    Gun currentGun;

    float velocity;
    float health;
    bool grounded;

    CameraFPS cameraFPS;
    char inputBindings[INPUT_ALL];
} Player;

typedef struct {
    int lightsLenLoc;
    int lightsPositionLoc;
    int lightsColorLoc;

    Vector3 lightsPosition[10];
    Vector3 lightsColor[10];
    int lightsLen;
} LightSystem;

Shader shader;

void SetupGun(Gun *gun) {
    for (int i = 0; i < gun->model.materialCount; i++) {
        gun->model.materials[i].shader = shader;
    }
}

void SetupPlayer(Player *player)
{
    for (int i = 0; i < player->model.materialCount; i++) {
        player->model.materials[i].shader = shader;
    }

    player->cameraFPS.camera.position = player->position;
    player->cameraFPS.camera.target = player->target;
    player->cameraFPS.camera.up = WORLD_UP_VECTOR;
    player->cameraFPS.camera.fovy = 60.0f;
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

Vector3 CollideWithMapGravity(Model mapModel, Player *player, Vector3 nextPos) {
    Ray ray = {
        .position = nextPos,
        .direction = (Vector3) { 0.0f, -1.0f, 0.0f }
    };
    RayHitInfo hit = GetCollisionRayModel(ray, mapModel);
    if (hit.hit && hit.distance < PLAYER_RADIUS) {
        nextPos = Vector3Add(hit.position, Vector3Scale((Vector3) {0.0f, 1.0f, 0.0f}, PLAYER_RADIUS));
        printf("grounded! (%f,%f,%f)\n", nextPos.x, nextPos.y, nextPos.z);
        player->grounded = true;
        player->velocity = 0;
    }

    return nextPos;
}

void ApplyGravity(Model mapModel, Player *player) {
    player->velocity -= 3.0f * GetFrameTime();
    Vector3 nextPos = player->position;
    nextPos.y += player->velocity * GetFrameTime();
    player->position = CollideWithMapGravity(mapModel, player, nextPos);
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

    float deltaZ = (cosf(player->cameraFPS.angle.x)*inputs[MOVE_BACK] -
            cosf(player->cameraFPS.angle.x)*inputs[MOVE_FRONT] +
            sinf(player->cameraFPS.angle.x)*inputs[MOVE_LEFT] -
            sinf(player->cameraFPS.angle.x)*inputs[MOVE_RIGHT]) / PLAYER_MOVEMENT_SENSITIVITY;

    Vector3 nextPos = player->position;
    nextPos.x += deltaX * GetFrameTime();
    nextPos.z += deltaZ * GetFrameTime();

    Vector3 tmpDir = Vector3Scale(Vector3Normalize(Vector3Subtract(nextPos, player->position)), GetFrameTime() / PLAYER_MOVEMENT_SENSITIVITY);
    nextPos = Vector3Add(player->position, tmpDir);

    player->position = CollideWithMap(mapModel, player->position, nextPos, COLLIDE_AND_SLIDE);

    if (player->grounded && IsKeyDown(player->inputBindings[MOVE_JUMP])) {
        player->grounded = false;
        player->velocity = 3.0f;
    }

    ApplyGravity(mapModel, player);

    // Camera orientation calculation
    player->cameraFPS.angle.x += (mousePositionDelta.x * -CAMERA_MOUSE_MOVE_SENSITIVITY);
    player->cameraFPS.angle.y += (mousePositionDelta.y * -CAMERA_MOUSE_MOVE_SENSITIVITY);

    // Angle clamp
    if (player->cameraFPS.angle.y > CAMERA_FIRST_PERSON_MIN_CLAMP * DEG2RAD) player->cameraFPS.angle.y = CAMERA_FIRST_PERSON_MIN_CLAMP * DEG2RAD;
    else if (player->cameraFPS.angle.y < CAMERA_FIRST_PERSON_MAX_CLAMP * DEG2RAD) player->cameraFPS.angle.y = CAMERA_FIRST_PERSON_MAX_CLAMP * DEG2RAD;

    // Recalculate camera target considering translation and rotation
    Matrix translation = MatrixTranslate(0, 0, (player->cameraFPS.targetDistance/CAMERA_FREE_PANNING_DIVIDER));
    Matrix rotation = MatrixRotateXYZ((Vector3) { PI*2 - player->cameraFPS.angle.y, PI*2 - player->cameraFPS.angle.x, 0 });
    Matrix transform = MatrixMultiply(translation, rotation);

    player->target.x = player->position.x - transform.m12;
    player->target.y = player->position.y - transform.m13;
    player->target.z = player->position.z - transform.m14;
}

void UpdateLights(LightSystem lights) {
    SetShaderValue(shader, lights.lightsLenLoc, &lights.lightsLen, SHADER_UNIFORM_INT);
    SetShaderValueV(shader, lights.lightsPositionLoc, &lights.lightsPosition, SHADER_UNIFORM_VEC3, lights.lightsLen);
    SetShaderValueV(shader, lights.lightsColorLoc, &lights.lightsColor, SHADER_UNIFORM_VEC3, lights.lightsLen);
}

void UpdatePlayer(Player *player) {
    player->model.transform = MatrixScale(1.0f, 1.0f, 1.0f);
    player->model.transform = MatrixMultiply(player->model.transform, MatrixRotateXYZ((Vector3) { 0, PI - player->cameraFPS.angle.x, 0 }));
    player->model.transform = MatrixMultiply(player->model.transform, MatrixTranslate(player->position.x, player->position.y, player->position.z));

    player->cameraFPS.camera.position = player->position;
    player->cameraFPS.camera.target = player->target;

    player->currentGun.model.transform = MatrixScale(5.0f, 5.0f, 5.0f);
    player->currentGun.model.transform = MatrixMultiply(player->currentGun.model.transform, MatrixTranslate(-PLAYER_RADIUS, -0.05f, PLAYER_RADIUS));
    player->currentGun.model.transform = MatrixMultiply(player->currentGun.model.transform, MatrixRotateXYZ((Vector3) { player->cameraFPS.angle.y, PI - player->cameraFPS.angle.x, 0 }));
    player->currentGun.model.transform = MatrixMultiply(player->currentGun.model.transform, MatrixTranslate(player->position.x, player->position.y, player->position.z));
}

int main(void) {
    const int screenWidth = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "fps.jpeg");
    SetTargetFPS(60);

    shader = LoadShader("shaders/lighting.vs", "shaders/lighting.fs");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");

    Player players[4];
    int playerLen = 2;
    players[0] = (Player) {
        .model = LoadModel("assets/human.obj"),
        .position = (Vector3){ 4.0f, 1.0f, 4.0f },
        .target = (Vector3){ 0.0f, 1.8f, 0.0f },
        .inputBindings = { 'W', 'S', 'D', 'A', ' ', 'E' },
        .health = MAX_HEALTH,
        .currentGun = {
            .model = LoadModel("assets/machinegun.obj"),
            .damage = 0.1f,
        }
    };
    players[1] = (Player) {
        .model = LoadModel("assets/human.obj"),
        .position = (Vector3){ 4.0f, 1.0f, 4.0f },
        .target = (Vector3){ 0.0f, 1.8f, 0.0f },
        .inputBindings = { 'W', 'S', 'D', 'A', ' ', 'E' },
        .health = MAX_HEALTH,
        .currentGun = {
            .model = LoadModel("assets/machinegun.obj"),
            .damage = 0.1f,
        }
    };

    LightSystem lights = {
        .lightsLenLoc = GetShaderLocation(shader, "lightsLen"),
        .lightsPositionLoc = GetShaderLocation(shader, "lightsPosition"),
        .lightsColorLoc = GetShaderLocation(shader, "lightsColor"),

        .lightsPosition = { (Vector3) { 2.0, 3.0, 2.0 }, (Vector3) { -2.0, 4.0, -3.0 } },
        .lightsColor = { (Vector3) { 0.6, 0.5, 0.4 }, (Vector3) { 0.5, 0.5, 0.5 } },
        .lightsLen = 2,
    };

    for (int i = 0; i < playerLen; i++) {
        SetupPlayer(&players[i]);
        SetupGun(&players[i].currentGun);
    }

    Model mapModel = LoadModel("assets/final_map.obj");
    mapModel.materials[0].shader = shader;

    while (!WindowShouldClose()) {
        for (int i = 0; i < playerLen; i++) {
            if (i == 0) MovePlayer(mapModel, &players[i]);
            else ApplyGravity(mapModel, &players[i]);

            UpdatePlayer(&players[i]);
        }

        for (int i = 0; i < playerLen; i++) {
        }

        UpdateLights(lights);

        BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode3D(players[0].cameraFPS.camera);

        DrawModel(mapModel, (Vector3) {0}, 1.0f, WHITE);

        for (int i = 0; i < playerLen; i++) {
            DrawModel(players[i].model, Vector3Zero(), 1.0f, WHITE);
            DrawModel(players[i].currentGun.model, Vector3Zero(), 1.0f, WHITE);
        }

        EndMode3D();

        DrawRectangleGradientH(10, 10, 20 * players[0].health, 20, RED, GREEN);

        DrawLine(GetScreenWidth() / 2 - 6, GetScreenHeight() / 2, GetScreenWidth() / 2 + 5, GetScreenHeight() / 2, MAGENTA);
        DrawLine(GetScreenWidth() / 2, GetScreenHeight() / 2 - 6, GetScreenWidth() / 2, GetScreenHeight() / 2 + 5, MAGENTA);

        EndDrawing();
    }

    UnloadModel(mapModel);

    CloseWindow();

    return 0;
}
