#include "raylib.h"
#define RAYMATH_HEADER_ONLY
#include "raymath.h"
#include "math.h"
#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"

#define MAX_COLUMNS 20

#define CAMERA_FREE_PANNING_DIVIDER                     5.1f

#define CAMERA_FIRST_PERSON_FOCUS_DISTANCE              25.0f
#define CAMERA_FIRST_PERSON_MIN_CLAMP                   89.0f
#define CAMERA_FIRST_PERSON_MAX_CLAMP                  -89.0f

#define CAMERA_FIRST_PERSON_STEP_TRIGONOMETRIC_DIVIDER  8.0f
#define CAMERA_FIRST_PERSON_STEP_DIVIDER                30.0f
#define CAMERA_FIRST_PERSON_WAVING_DIVIDER              200.0f

#define PLAYER_MOVEMENT_SENSITIVITY                     20.0f

#define CAMERA_MOUSE_MOVE_SENSITIVITY                   0.003f
#define CAMERA_MOUSE_SCROLL_SENSITIVITY                 1.5f

typedef enum {
    MOVE_FRONT = 0,
    MOVE_BACK,
    MOVE_RIGHT,
    MOVE_LEFT,
    MOVE_UP,
    MOVE_DOWN
} CameraMove;

typedef struct {
    Camera camera;
    Vector2 angle;
    float targetDistance;
    float playerEyesPosition;
    char moveControl[6];
} CameraFPS;

Model mapModel;

void SetupCameraFPS(CameraFPS *camera)
{
    SetCameraMode(camera->camera, CAMERA_CUSTOM);

    Vector3 v1 = camera->camera.position;
    Vector3 v2 = camera->camera.target;

    float dx = v2.x - v1.x;
    float dy = v2.y - v1.y;
    float dz = v2.z - v1.z;

    camera->targetDistance = sqrtf(dx*dx + dy*dy + dz*dz);   // Distance to target

    // Camera angle calculation
    camera->angle.x = atan2f(dx, dz);                        // Camera angle in plane XZ (0 aligned with Z, move positive CCW)
    camera->angle.y = atan2f(dy, sqrtf(dx*dx + dz*dz));      // Camera angle in plane XY (0 aligned with X, move positive CW)

    camera->playerEyesPosition = camera->camera.position.y;          // Init player eyes position to camera Y position

    // Lock cursor for first person and third person cameras
    DisableCursor();

    //camera->mode = mode;
}

void UpdateFPSCamera(CameraFPS *camera) {
    static int swingCounter = 0;    // Used for 1st person swinging movement
    static Vector2 previousMousePosition = { 0.0f, 0.0f };

    // TODO: Compute CAMERA.targetDistance and CAMERA.angle here (?)

    // Mouse movement detection
    Vector2 mousePositionDelta = { 0.0f, 0.0f };
    Vector2 mousePosition = GetMousePosition();
    float mouseWheelMove = GetMouseWheelMove();

    // Keys input detection
    // TODO: Input detection is raylib-dependant, it could be moved outside the module
    bool direction[6] = { IsKeyDown(camera->moveControl[MOVE_FRONT]),
        IsKeyDown(camera->moveControl[MOVE_BACK]),
        IsKeyDown(camera->moveControl[MOVE_RIGHT]),
        IsKeyDown(camera->moveControl[MOVE_LEFT]),
        IsKeyDown(camera->moveControl[MOVE_UP]),
        IsKeyDown(camera->moveControl[MOVE_DOWN]) };

    mousePositionDelta.x = mousePosition.x - previousMousePosition.x;
    mousePositionDelta.y = mousePosition.y - previousMousePosition.y;

    previousMousePosition = mousePosition;

    float deltaX = (sinf(camera->angle.x) * direction[MOVE_BACK] -
            sinf(camera->angle.x) * direction[MOVE_FRONT] -
            cosf(camera->angle.x) * direction[MOVE_LEFT] +
            cosf(camera->angle.x) * direction[MOVE_RIGHT]) / PLAYER_MOVEMENT_SENSITIVITY;

    Vector3 nextPos = camera->camera.position;
    nextPos.x += deltaX;

    for (int i = 0; i < mapModel.meshCount; i++) {
        for (int j = 0; j < mapModel.meshes[i].
    }

    camera->camera.position.x += deltaX;

    camera->camera.position.y += (sinf(camera->angle.y)*direction[MOVE_FRONT] -
            sinf(camera->angle.y)*direction[MOVE_BACK] +
            1.0f*direction[MOVE_UP] - 1.0f*direction[MOVE_DOWN])/PLAYER_MOVEMENT_SENSITIVITY;

    camera->camera.position.z += (cosf(camera->angle.x)*direction[MOVE_BACK] -
            cosf(camera->angle.x)*direction[MOVE_FRONT] +
            sinf(camera->angle.x)*direction[MOVE_LEFT] -
            sinf(camera->angle.x)*direction[MOVE_RIGHT])/PLAYER_MOVEMENT_SENSITIVITY;

    // Camera orientation calculation
    camera->angle.x += (mousePositionDelta.x*-CAMERA_MOUSE_MOVE_SENSITIVITY);
    camera->angle.y += (mousePositionDelta.y*-CAMERA_MOUSE_MOVE_SENSITIVITY);

    // Angle clamp
    if (camera->angle.y > CAMERA_FIRST_PERSON_MIN_CLAMP*DEG2RAD) camera->angle.y = CAMERA_FIRST_PERSON_MIN_CLAMP*DEG2RAD;
    else if (camera->angle.y < CAMERA_FIRST_PERSON_MAX_CLAMP*DEG2RAD) camera->angle.y = CAMERA_FIRST_PERSON_MAX_CLAMP*DEG2RAD;

    // Recalculate camera target considering translation and rotation
    Matrix translation = MatrixTranslate(0, 0, (camera->targetDistance/CAMERA_FREE_PANNING_DIVIDER));
    Matrix rotation = MatrixRotateXYZ((Vector3){ PI*2 - camera->angle.y, PI*2 - camera->angle.x, 0 });
    Matrix transform = MatrixMultiply(translation, rotation);

    camera->camera.target.x = camera->camera.position.x - transform.m12;
    camera->camera.target.y = camera->camera.position.y - transform.m13;
    camera->camera.target.z = camera->camera.position.z - transform.m14;

    // If movement detected (some key pressed), increase swinging
    for (int i = 0; i < 6; i++) if (direction[i]) { swingCounter++; break; }

    // Camera position update
    // NOTE: On CAMERA_FIRST_PERSON player Y-movement is limited to player 'eyes position'
    camera->camera.position.y = camera->playerEyesPosition - sinf(swingCounter/CAMERA_FIRST_PERSON_STEP_TRIGONOMETRIC_DIVIDER)/CAMERA_FIRST_PERSON_STEP_DIVIDER;

    camera->camera.up.x = sinf(swingCounter/(CAMERA_FIRST_PERSON_STEP_TRIGONOMETRIC_DIVIDER*2))/CAMERA_FIRST_PERSON_WAVING_DIVIDER;
    camera->camera.up.z = -sinf(swingCounter/(CAMERA_FIRST_PERSON_STEP_TRIGONOMETRIC_DIVIDER*2))/CAMERA_FIRST_PERSON_WAVING_DIVIDER;
}

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "fps.jpeg");

    Camera camera = { 0 };
    camera.position = (Vector3){ 4.0f, 2.0f, 4.0f };
    camera.target = (Vector3){ 0.0f, 1.8f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 60.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float heights[MAX_COLUMNS] = { 0 };
    Vector3 positions[MAX_COLUMNS] = { 0 };
    Color colors[MAX_COLUMNS] = { 0 };

    for (int i = 0; i < MAX_COLUMNS; i++) {
        heights[i] = (float)GetRandomValue(1, 12);
        positions[i] = (Vector3){ (float)GetRandomValue(-15, 15), heights[i]/2.0f, (float)GetRandomValue(-15, 15) };
        colors[i] = (Color){ GetRandomValue(20, 255), GetRandomValue(10, 55), 30, 255 };
    }

    SetTargetFPS(60);

    CameraFPS cameraFPS = {
        .camera = camera,
        .moveControl = { 'W', 'S', 'D', 'A', 'E', 'Q' }
    };
    SetupCameraFPS(&cameraFPS);

    Vector3 otherPlayer = {0};

    Shader shader = LoadShader("lighting.vs", "lighting.fs");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");

    mapModel = LoadModel("final_map.obj");
    mapModel.materials[0].shader = shader;

    while (!WindowShouldClose()) {
        UpdateFPSCamera(&cameraFPS);

        otherPlayer.x = -cameraFPS.camera.position.x;
        otherPlayer.y = cameraFPS.camera.position.y;
        otherPlayer.z = -cameraFPS.camera.position.z;

        BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode3D(cameraFPS.camera);

        DrawPlane((Vector3){ 0.0f, 0.0f, 0.0f }, (Vector2){ 32.0f, 32.0f }, LIGHTGRAY);
        DrawCube((Vector3){ -16.0f, 2.5f, 0.0f }, 1.0f, 5.0f, 32.0f, BLUE);
        DrawCube((Vector3){ 16.0f, 2.5f, 0.0f }, 1.0f, 5.0f, 32.0f, LIME);
        DrawCube((Vector3){ 0.0f, 2.5f, 16.0f }, 32.0f, 5.0f, 1.0f, GOLD);

        for (int i = 0; i < MAX_COLUMNS; i++)
        {
            DrawCube(positions[i], 2.0f, heights[i], 2.0f, colors[i]);
            DrawCubeWires(positions[i], 2.0f, heights[i], 2.0f, MAROON);
        }

        DrawCube(otherPlayer, 1.0f, 2.0f, 1.0f, BLACK);

        DrawModel(mapModel, (Vector3) {0}, 1.0f, WHITE);

        EndMode3D();

        EndDrawing();
    }

    UnloadModel(mapModel);

    CloseWindow();

    return 0;
}
