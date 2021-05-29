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

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

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
} CameraFPS;

typedef struct {
    Vector3 position;
    Vector3 target;
    CameraFPS cameraFPS;
    char moveControl[6];
} Player;

Vector3 closestPointOnLineSegment(Vector3 A, Vector3 B, Vector3 Point) {
  Vector3 AB = Vector3Subtract(B, A);
  float t = Vector3DotProduct(Vector3Subtract(Point, A), AB) / Vector3DotProduct(AB, AB);
  return Vector3Add(A, Vector3Scale(AB, MIN(MAX(t, 0), 1)));
}

// https://wickedengine.net/2020/04/26/capsule-collision-detection/
bool sphereCollidesTriangleEx(Vector3 center, float radius, Vector3 p0, Vector3 p1, Vector3 p2, Vector3 *penetrationVec) {
    Vector3 N = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(p1, p0), Vector3Subtract(p2, p0))); // plane normal
    float dist = Vector3DotProduct(Vector3Subtract(center, p0), N); // signed distance between sphere and plane
    //if(!mesh.is_double_sided() && dist > 0)
        //return false; // can pass through back side of triangle (optional)
    if (dist < -radius || dist > radius)
        return false; // no intersection

    Vector3 point0 = Vector3Subtract(center, Vector3Scale(N, dist)); // projected sphere center on triangle plane

    // Now determine whether point0 is inside all triangle edges:
    Vector3 c0 = Vector3CrossProduct(Vector3Subtract(point0, p0), Vector3Subtract(p1, p0));
    Vector3 c1 = Vector3CrossProduct(Vector3Subtract(point0, p1), Vector3Subtract(p2, p1));
    Vector3 c2 = Vector3CrossProduct(Vector3Subtract(point0, p2), Vector3Subtract(p0, p2));
    bool inside = Vector3DotProduct(c0, N) <= 0 && Vector3DotProduct(c1, N) <= 0 && Vector3DotProduct(c2, N) <= 0;

    float radiussq = radius * radius; // sphere radius squared

    // Edge 1:
    Vector3 point1 = closestPointOnLineSegment(p0, p1, center);
    Vector3 v1 = Vector3Subtract(center, point1);
    float distsq1 = Vector3DotProduct(v1, v1);
    bool intersects = distsq1 < radiussq;

    // Edge 2:
    Vector3 point2 = closestPointOnLineSegment(p1, p2, center);
    Vector3 v2 = Vector3Subtract(center, point2);
    float distsq2 = Vector3DotProduct(v2, v2);
    intersects |= distsq2 < radiussq;

    // Edge 3:
    Vector3 point3 = closestPointOnLineSegment(p2, p0, center);
    Vector3 v3 = Vector3Subtract(center, point3);
    float distsq3 = Vector3DotProduct(v3, v3);
    intersects |= distsq3 < radiussq;

    if (inside || intersects) {
        //printf("inside:%d ... intersects:%d\n", inside, intersects);
        Vector3 best_point = point0;
        Vector3 intersection_vec;

        if (inside) {
            intersection_vec = Vector3Subtract(center, point0);
        } else {
            Vector3 d = Vector3Subtract(center, point1);
            float best_distsq = Vector3DotProduct(d, d);
            best_point = point1;
            intersection_vec = d;

            d = Vector3Subtract(center, point2);
            float distsq = Vector3DotProduct(d, d);
            if (distsq < best_distsq) {
                distsq = best_distsq;
                best_point = point2;
                intersection_vec = d;
            }

            d = Vector3Subtract(center, point3);
            distsq = Vector3DotProduct(d, d);
            if (distsq < best_distsq) {
                distsq = best_distsq;
                best_point = point3;
                intersection_vec = d;
            }
        }

        float len = Vector3Length(intersection_vec);
        if (penetrationVec) *penetrationVec = Vector3Scale(Vector3Normalize(intersection_vec), radius - len);  // normalize

        return true; // intersection success
    }

    return false;
}

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

Vector3 CollideWithMap(Model mapModel, Vector3 curPos, Vector3 nextPos) {
    int reboundLen = 0;
    Vector3 rebounds[100][5];

    for (int i = 0; i < mapModel.meshCount; i++) {
        for (int j = 0; j < 3*mapModel.meshes[i].vertexCount; j += 9) {
            Vector3 normal1 = { mapModel.meshes[i].normals[j], mapModel.meshes[i].normals[j+1], mapModel.meshes[i].normals[j+2]};
            Vector3 normal2 = { mapModel.meshes[i].normals[j+3], mapModel.meshes[i].normals[j+4], mapModel.meshes[i].normals[j+5]};
            Vector3 normal3 = { mapModel.meshes[i].normals[j+6], mapModel.meshes[i].normals[j+7], mapModel.meshes[i].normals[j+8]};

            Vector3 normal = Vector3Normalize(Vector3Add(normal1, Vector3Add(normal2, normal3)));
            Vector3 vertex1 = { mapModel.meshes[i].vertices[j], mapModel.meshes[i].vertices[j+1], mapModel.meshes[i].vertices[j+2]};
            Vector3 vertex2 = { mapModel.meshes[i].vertices[j+3], mapModel.meshes[i].vertices[j+4], mapModel.meshes[i].vertices[j+5]};
            Vector3 vertex3 = { mapModel.meshes[i].vertices[j+6], mapModel.meshes[i].vertices[j+7], mapModel.meshes[i].vertices[j+8]};

            Vector3 penetration;
            if (sphereCollidesTriangleEx(nextPos, PLAYER_RADIUS, vertex1, vertex2, vertex3, &penetration)) {
                rebounds[reboundLen][0] = penetration;
                rebounds[reboundLen][1] = vertex1;
                rebounds[reboundLen][2] = vertex2;
                rebounds[reboundLen][3] = vertex3;
                rebounds[reboundLen][4] = normal;
                reboundLen++;
            }
        }
    }

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
        //printf("trying %d [(%f, %f, %f), (%f, %f, %f), (%f, %f, %f)] ->", i, rebounds[i][1].x, rebounds[i][1].y, rebounds[i][1].z,
                                                                             //rebounds[i][2].x, rebounds[i][2].y, rebounds[i][2].z,
                                                                             //rebounds[i][3].x, rebounds[i][3].y, rebounds[i][3].z);
        Vector3 penetration;
        if (sphereCollidesTriangleEx(nextPos, PLAYER_RADIUS - 0.01f, rebounds[i][1], rebounds[i][2], rebounds[i][3], &penetration)) {
            nextPos = Vector3Add(nextPos, penetration);
            //printf("collided, size: %f", Vector3Length(penetration));
        }
        else {
            //printf("no collision, size: %f", Vector3Length(rebounds[i][0]));
        }
        //printf("\n");
    }

    return nextPos;
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

Vector3 CollideWithMapGravity(Model mapModel, Vector3 nextPos) {
    Ray ray = {
        .position = nextPos,
        .direction = (Vector3) { 0.0f, -1.0f, 0.0f }
    };
    RayHitInfo hit = GetCollisionRayModel(ray, mapModel);
    if (hit.hit && hit.distance < PLAYER_RADIUS) nextPos = Vector3Add(nextPos, Vector3Scale(hit.normal, PLAYER_RADIUS - hit.distance));
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
    bool direction[6] = { IsKeyDown(player->moveControl[MOVE_FRONT]),
        IsKeyDown(player->moveControl[MOVE_BACK]),
        IsKeyDown(player->moveControl[MOVE_RIGHT]),
        IsKeyDown(player->moveControl[MOVE_LEFT]),
        IsKeyDown(player->moveControl[MOVE_UP]),
        IsKeyDown(player->moveControl[MOVE_DOWN]) };

    mousePositionDelta.x = mousePosition.x - previousMousePosition.x;
    mousePositionDelta.y = mousePosition.y - previousMousePosition.y;

    previousMousePosition = mousePosition;

    float deltaX = (sinf(player->cameraFPS.angle.x) * direction[MOVE_BACK] -
            sinf(player->cameraFPS.angle.x) * direction[MOVE_FRONT] -
            cosf(player->cameraFPS.angle.x) * direction[MOVE_LEFT] +
            cosf(player->cameraFPS.angle.x) * direction[MOVE_RIGHT]) / PLAYER_MOVEMENT_SENSITIVITY;

    Vector3 nextPos = player->position;
    nextPos.x += deltaX * GetFrameTime();
    player->position = CollideWithMap(mapModel, player->position, nextPos);

    float deltaZ = (cosf(player->cameraFPS.angle.x)*direction[MOVE_BACK] -
            cosf(player->cameraFPS.angle.x)*direction[MOVE_FRONT] +
            sinf(player->cameraFPS.angle.x)*direction[MOVE_LEFT] -
            sinf(player->cameraFPS.angle.x)*direction[MOVE_RIGHT]) / PLAYER_MOVEMENT_SENSITIVITY;

    nextPos = player->position;
    nextPos.z += deltaZ * GetFrameTime();
    player->position = CollideWithMap(mapModel, player->position, nextPos);

    float deltaY = -0.05f;

    nextPos = player->position;
    nextPos.y += deltaY * GetFrameTime();
    //player->position = CollideWithMapGravity(mapModel, nextPos);

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

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "fps.jpeg");
    SetTargetFPS(60);

    Player player = {
        .position = (Vector3){ 4.0f, 1.0f, 4.0f },
        .target = (Vector3){ 0.0f, 1.8f, 0.0f },
        .moveControl = { 'W', 'S', 'D', 'A', 'E', 'Q' },
    };

    SetupPlayer(&player);

    Vector3 otherPlayer = {0};

    Shader shader = LoadShader("shaders/lighting.vs", "shaders/lighting.fs");
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");

    Model mapModel = LoadModel("assets/final_map.obj");
    mapModel.materials[0].shader = shader;

    while (!WindowShouldClose()) {
        MovePlayer(mapModel, &player);

        otherPlayer.x = -player.position.x;
        otherPlayer.y = player.position.y;
        otherPlayer.z = -player.position.z;

        UpdateCameraFPS(&player);

        BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode3D(player.cameraFPS.camera);

        DrawCube(otherPlayer, 1.0f, 2.0f, 1.0f, BLACK);

        DrawModel(mapModel, (Vector3) {0}, 1.0f, WHITE);

        EndMode3D();

        EndDrawing();
    }

    UnloadModel(mapModel);

    CloseWindow();

    return 0;
}
