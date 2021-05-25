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

#define PLAYER_RADIUS 1.0f

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

bool sphereCollidesTriangle(Vector3 sphere_center, float sphere_radius, Vector3 triangle0, Vector3 triangle1, Vector3 triangle2) {
    Vector3 A = Vector3Subtract(triangle0, sphere_center);
    Vector3 B = Vector3Subtract(triangle1, sphere_center);
    Vector3 C = Vector3Subtract(triangle2, sphere_center);
    float rr = sphere_radius * sphere_radius;
    Vector3 V = Vector3CrossProduct(Vector3Subtract(B, A), Vector3Subtract(C, A));
    float d = Vector3DotProduct(A, V);
    float e = Vector3DotProduct(V, V);

    bool sep1 = d*d > rr*e;

    float aa = Vector3DotProduct(A, A);
    float ab = Vector3DotProduct(A, B);
    float ac = Vector3DotProduct(A, C);
    float bb = Vector3DotProduct(B, B);
    float bc = Vector3DotProduct(B, C);
    float cc = Vector3DotProduct(C, C);

    bool sep2 = (aa > rr) && (ab > aa) && (ac > aa);
    bool sep3 = (bb > rr) && (ab > bb) && (bc > bb);
    bool sep4 = (cc > rr) && (ac > cc) && (bc > cc);

    Vector3 AB = Vector3Subtract(B, A);
    Vector3 BC = Vector3Subtract(C, B);
    Vector3 CA = Vector3Subtract(A, C);

    float d1 = ab - aa;
    float d2 = bc - bb;
    float d3 = ac - cc;

    float e1 = Vector3DotProduct(AB, AB);
    float e2 = Vector3DotProduct(BC, BC);
    float e3 = Vector3DotProduct(CA, CA);

    Vector3 Q1 = Vector3Subtract(Vector3Scale(A, e1), Vector3Scale(AB, d1));
    Vector3 Q2 = Vector3Subtract(Vector3Scale(B, e2), Vector3Scale(BC, d2));
    Vector3 Q3 = Vector3Subtract(Vector3Scale(C, e3), Vector3Scale(CA, d3));
    Vector3 QC = Vector3Subtract(Vector3Scale(C, e1), Q1);
    Vector3 QA = Vector3Subtract(Vector3Scale(A, e2), Q2);
    Vector3 QB = Vector3Subtract(Vector3Scale(B, e3), Q3);

    bool sep5 = (Vector3DotProduct(Q1, Q1) > rr * e1 * e1) && (Vector3DotProduct(Q1, QC) > 0);
    bool sep6 = (Vector3DotProduct(Q2, Q2) > rr * e2 * e2) && (Vector3DotProduct(Q2, QA) > 0);
    bool sep7 = (Vector3DotProduct(Q3, Q3) > rr * e3 * e3) && (Vector3DotProduct(Q3, QB) > 0);

    bool separated = sep1 || sep2 || sep3 || sep4 || sep5 || sep6 || sep7;

    return !separated;
}

Vector3 collideWithMap(Vector3 nextPos) {
    for (int i = 0; i < mapModel.meshCount; i++) {
        for (int j = 0; j < mapModel.meshes[i].vertexCount; j += 9) {
            Vector3 normal1 = { mapModel.meshes[i].normals[j], mapModel.meshes[i].normals[j+1], mapModel.meshes[i].normals[j+2]};
            Vector3 normal2 = { mapModel.meshes[i].normals[j+3], mapModel.meshes[i].normals[j+4], mapModel.meshes[i].normals[j+5]};
            Vector3 normal3 = { mapModel.meshes[i].normals[j+6], mapModel.meshes[i].normals[j+7], mapModel.meshes[i].normals[j+8]};

            Vector3 normal = Vector3Normalize(Vector3Add(normal1, Vector3Add(normal2, normal3)));
            Vector3 vertex1 = { mapModel.meshes[i].vertices[j], mapModel.meshes[i].vertices[j+1], mapModel.meshes[i].vertices[j+2]};
            Vector3 vertex2 = { mapModel.meshes[i].vertices[j+3], mapModel.meshes[i].vertices[j+4], mapModel.meshes[i].vertices[j+5]};
            Vector3 vertex3 = { mapModel.meshes[i].vertices[j+6], mapModel.meshes[i].vertices[j+7], mapModel.meshes[i].vertices[j+8]};

            if (sphereCollidesTriangle(nextPos, PLAYER_RADIUS, vertex1, vertex2, vertex3)) {
                float projection = Vector3DotProduct(Vector3Subtract(nextPos, vertex1), normal);

                Vector3 rebound = Vector3Scale(normal, PLAYER_RADIUS - projection);
                nextPos = Vector3Add(nextPos, rebound);
            }
        }
    }

    return nextPos;
}

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
    camera->camera.position = collideWithMap(nextPos);

    camera->camera.position.y += (sinf(camera->angle.y)*direction[MOVE_FRONT] -
            sinf(camera->angle.y)*direction[MOVE_BACK] +
            1.0f*direction[MOVE_UP] - 1.0f*direction[MOVE_DOWN])/PLAYER_MOVEMENT_SENSITIVITY;

    float deltaZ = (cosf(camera->angle.x)*direction[MOVE_BACK] -
            cosf(camera->angle.x)*direction[MOVE_FRONT] +
            sinf(camera->angle.x)*direction[MOVE_LEFT] -
            sinf(camera->angle.x)*direction[MOVE_RIGHT])/PLAYER_MOVEMENT_SENSITIVITY;

    nextPos = camera->camera.position;
    nextPos.z += deltaZ;
    camera->camera.position = collideWithMap(nextPos);

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
