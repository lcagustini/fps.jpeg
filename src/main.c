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

#define PLAYER_MOVEMENT_SENSITIVITY                     0.4f

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

Vector3 CollideWithMap(Model mapModel, Vector3 curPos, Vector3 nextPos) {
    int reboundLen = 0;
    Vector3 rebounds[100][5];

    for (int i = 0; i < mapModel.meshCount; i++) {
        for (int j = 0; j < 3 * mapModel.meshes[i].vertexCount; j += 9) {
            Vector3 vertex1 = { mapModel.meshes[i].vertices[j], mapModel.meshes[i].vertices[j+1], mapModel.meshes[i].vertices[j+2]};
            Vector3 vertex2 = { mapModel.meshes[i].vertices[j+3], mapModel.meshes[i].vertices[j+4], mapModel.meshes[i].vertices[j+5]};
            Vector3 vertex3 = { mapModel.meshes[i].vertices[j+6], mapModel.meshes[i].vertices[j+7], mapModel.meshes[i].vertices[j+8]};

            Vector3 normal1 = { mapModel.meshes[i].normals[j], mapModel.meshes[i].normals[j+1], mapModel.meshes[i].normals[j+2]};
            Vector3 normal2 = { mapModel.meshes[i].normals[j+3], mapModel.meshes[i].normals[j+4], mapModel.meshes[i].normals[j+5]};
            Vector3 normal3 = { mapModel.meshes[i].normals[j+6], mapModel.meshes[i].normals[j+7], mapModel.meshes[i].normals[j+8]};
            Vector3 normal = Vector3Normalize(Vector3Add(normal1, Vector3Add(normal2, normal3)));

            // TODO: might cause problems when falling close to "boxes"
            if (Vector3DotProduct(normal, WORLD_UP_VECTOR) > 0.1f) continue;

            Vector3 aabb_min = Vector3Subtract(nextPos, (Vector3){PLAYER_RADIUS, PLAYER_RADIUS, PLAYER_RADIUS});
            Vector3 aabb_max = Vector3Add(nextPos, (Vector3){PLAYER_RADIUS, PLAYER_RADIUS, PLAYER_RADIUS});

            if (triangleAABBIntersects(aabb_min, aabb_max, vertex1, vertex2, vertex3)) {
#if 0
                // collision response
                Vector3 dir = Vector3Subtract(nextPos, curPos);
                float comp1 = Vector3DotProduct(dir, normal);
                Vector3 perp = Vector3Normalize(Vector3CrossProduct(normal, WORLD_UP_VECTOR));
                float comp2 = Vector3DotProduct(dir, perp);

                Vector3 rebound = Vector3Add(curPos, Vector3Add(Vector3Scale(perp, comp2), Vector3Scale(rebounds[i][4], MAX(comp1, 0.0f))));

                //printf("collided, size: %f", Vector3Length(Vector3Subtract(nextPos, curPos)));
                //printf("collided, size: %f", Vector3Length(penetration));

                nextPos = rebound;
            }
        }
    }
#else
                float projection = Vector3DotProduct(Vector3Subtract(nextPos, vertex1), normal);

                rebounds[reboundLen][0] = Vector3Scale(normal, PLAYER_RADIUS - projection);
                rebounds[reboundLen][1] = vertex1;
                rebounds[reboundLen][2] = vertex2;
                rebounds[reboundLen][3] = vertex3;
                rebounds[reboundLen][4] = normal;
                reboundLen++;
            }
        }
    }

    // sort from least penetration to most penetration
    // might not matter at all
    for (int i = 1; i < reboundLen; i++) {
        for (int j = 0; j < reboundLen - i; j++) {
            if (Vector3LengthSqr(rebounds[j][0]) > Vector3LengthSqr(rebounds[j + 1][0])) {
                Vector3 aux[5];

                aux[0] = rebounds[j][0];
                aux[1] = rebounds[j][1];
                aux[2] = rebounds[j][2];
                aux[3] = rebounds[j][3];
                aux[4] = rebounds[j][4];

                rebounds[j][0] = rebounds[j + 1][0];
                rebounds[j][1] = rebounds[j + 1][1];
                rebounds[j][2] = rebounds[j + 1][2];
                rebounds[j][3] = rebounds[j + 1][3];
                rebounds[j][4] = rebounds[j + 1][4];

                rebounds[j + 1][0] = aux[0];
                rebounds[j + 1][1] = aux[1];
                rebounds[j + 1][2] = aux[2];
                rebounds[j + 1][3] = aux[3];
                rebounds[j + 1][4] = aux[4];
            }
        }
    }

    //printf("reboundLen = %d\n", reboundLen);
    for (int i = 0; i < reboundLen; i++) {
        Vector3 aabb_min = Vector3Subtract(nextPos, (Vector3){PLAYER_RADIUS, PLAYER_RADIUS, PLAYER_RADIUS});
        Vector3 aabb_max = Vector3Add(nextPos, (Vector3){PLAYER_RADIUS, PLAYER_RADIUS, PLAYER_RADIUS});

        // collision response
        if (triangleAABBIntersects(aabb_min, aabb_max, rebounds[i][1], rebounds[i][2], rebounds[i][3])) {
            Vector3 dir = Vector3Subtract(nextPos, curPos);
            float comp1 = Vector3DotProduct(dir, rebounds[i][4]);
            Vector3 perp = Vector3Normalize(Vector3CrossProduct(rebounds[i][4], (Vector3){0.0f, 1.0f, 0.0f}));
            float comp2 = Vector3DotProduct(dir, perp);

            Vector3 rebound = Vector3Add(curPos, Vector3Add(Vector3Scale(perp, comp2), Vector3Scale(rebounds[i][4], MAX(comp1, 0.0f))));

            //printf("collided, size: %f", Vector3Length(Vector3Subtract(nextPos, curPos)));
            //printf("collided, size: %f", Vector3Length(penetration));

            nextPos = rebound;
        } else {
            //printf("no collision, size: %f", Vector3Length(rebounds[i][0]));
        }
        //printf("\n");
    }
#endif

    // hack to prevent going out of bounds by shoving head into corners
    if (Vector3Length(Vector3Subtract(nextPos, curPos)) < 0.004f) return curPos;

    return nextPos;
}

void SetupGun(Gun *gun) {
    for (int i = 0; i < gun->model.materialCount; i++) {
        gun->model.materials[i].shader = shader;
    }
}

void SetupPlayer(Player *player)
{
    player->cameraFPS.camera.position = player->position;
    player->cameraFPS.camera.target = player->target;
    player->cameraFPS.camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
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

void MovePlayer(Model mapModel, Player *player) {
    static Vector2 previousMousePosition = { 0.0f, 0.0f };

    // Mouse movement detection
    Vector2 mousePositionDelta = { 0.0f, 0.0f };
    Vector2 mousePosition = GetMousePosition();
    float mouseWheelMove = GetMouseWheelMove();

    // Keys input detection
    // TODO: Input detection is raylib-dependant, it could be moved outside the module
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

    player->position = CollideWithMap(mapModel, player->position, nextPos);

    if (player->grounded && IsKeyDown(player->inputBindings[MOVE_JUMP])) {
        player->grounded = false;
        player->velocity = 3.0f;
    }
    
      player->velocity -= 3.0f * GetFrameTime();
      nextPos = player->position;
      nextPos.y += player->velocity * GetFrameTime();
      player->position = CollideWithMapGravity(mapModel, player, nextPos);

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

void UpdateCameraFPS(Player *player) {
    player->cameraFPS.camera.position = player->position;
    player->cameraFPS.camera.target = player->target;
}

void UpdateCarriedGun(Gun *gun, Player player) {
    gun->model.transform = MatrixScale(0.35f, 0.35f, 0.35f);
    gun->model.transform = MatrixMultiply(gun->model.transform, MatrixTranslate(-0.015f, -0.005f, 0.013f));
    gun->model.transform = MatrixMultiply(gun->model.transform, MatrixRotateXYZ((Vector3) { player.cameraFPS.angle.y, PI - player.cameraFPS.angle.x, 0 }));
    gun->model.transform = MatrixMultiply(gun->model.transform, MatrixTranslate(player.position.x, player.position.y, player.position.z));
}

void UpdateLights(LightSystem lights) {
    SetShaderValue(shader, lights.lightsLenLoc, &lights.lightsLen, SHADER_UNIFORM_INT);
    SetShaderValueV(shader, lights.lightsPositionLoc, &lights.lightsPosition, SHADER_UNIFORM_VEC3, lights.lightsLen);
    SetShaderValueV(shader, lights.lightsColorLoc, &lights.lightsColor, SHADER_UNIFORM_VEC3, lights.lightsLen);
}

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "fps.jpeg");
    SetTargetFPS(60);

    shader = LoadShader("shaders/lighting.vs", "shaders/lighting.fs");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");

    Player player = {
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

    SetupPlayer(&player);
    SetupGun(&player.currentGun);

    Model mapModel = LoadModel("assets/final_map.obj");
    mapModel.materials[0].shader = shader;

    while (!WindowShouldClose()) {
        MovePlayer(mapModel, &player);
        UpdateCameraFPS(&player);
        UpdateCarriedGun(&player.currentGun, player);
        UpdateLights(lights);

        BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode3D(player.cameraFPS.camera);

        DrawModel(mapModel, (Vector3) {0}, 1.0f, WHITE);
        DrawModel(player.currentGun.model, Vector3Zero(), 1.0f, WHITE);

        EndMode3D();

        DrawRectangleGradientH(10, 10, 20 * player.health, 20, RED, GREEN);

        EndDrawing();
    }

    UnloadModel(mapModel);

    CloseWindow();

    return 0;
}
