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

Shader shader;

Vector3 closestPointOnLineSegment(Vector3 A, Vector3 B, Vector3 Point) {
    Vector3 AB = Vector3Subtract(B, A);
    float t = Vector3DotProduct(Vector3Subtract(Point, A), AB) / Vector3DotProduct(AB, AB);
    return Vector3Add(A, Vector3Scale(AB, MIN(MAX(t, 0), 1)));
}

// Adapted from https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
bool raySphereIntersection(Vector3 orig, Vector3 dir, Vector3 center, float radius, float *t0, float *t1) {
#if 1
    // geometric solution
    Vector3 L = Vector3Subtract(center, orig);
    float tca = Vector3DotProduct(L, dir);
    // if (tca < 0) return false;
    float d2 = Vector3DotProduct(L, L) - tca * tca;
    if (d2 > radius * radius) return false;
    float thc = sqrt(radius * radius - d2);
    *t0 = tca - thc;
    *t1 = tca + thc;
#else
    // analytic solution
    Vec3f L = orig - center;
    float a = dir.dotProduct(dir);
    float b = 2 * dir.dotProduct(L);
    float c = L.dotProduct(L) - radius2;
    if (!solveQuadratic(a, b, c, t0, t1)) return false;
#endif

    if (*t0 > *t1) {
        float tmp = *t0;
        *t0 = *t1;
        *t1 = tmp;
    }

    if (*t0 < 0) {
        *t0 = *t1; // if t0 is negative, let's use t1 instead
        if (*t0 < 0) return false; // both t0 and t1 are negative
    }

    return true;
}

// Adapted from https://wickedengine.net/2020/04/26/capsule-collision-detection/
bool sphereCollidesTriangleEx(Vector3 center, float radius, Vector3 p0, Vector3 p1, Vector3 p2, Vector3 normal, Vector3 *penetrationVec) {
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

        float t0, t1;
        raySphereIntersection(best_point, Vector3Scale(normal, -1), center, radius, &t0, &t1);
        t0 += 0.01f;
        t1 += 0.01f;

        intersection_vec.y = 0;
        float len = Vector3Length(intersection_vec);
        //if (penetrationVec) *penetrationVec = Vector3Scale(Vector3Normalize(intersection_vec), radius - len);  // normalize
        if (penetrationVec) *penetrationVec = Vector3Scale(normal, t0);

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

Vector3 vectorAbs(Vector3 v) {
  return (Vector3) {fabs(v.x), fabs(v.y), fabs(v.z)};
}

// Adapted from https://github.com/juj/MathGeoLib/blob/master/src/Geometry/Triangle.cpp
bool triangleAABBIntersects(Vector3 aabb_min, Vector3 aabb_max, Vector3 a, Vector3 b, Vector3 c) {
/** The AABB-Triangle test implementation is based on the pseudo-code in
	Christer Ericson's Real-Time Collision Detection, pp. 169-172. It is
	practically a standard SAT test. */
  float tMinX = MIN(a.x, MIN(b.x, c.x)); 
  float tMinY = MIN(a.y, MIN(b.y, c.y)); 
  float tMinZ = MIN(a.z, MIN(b.z, c.z)); 
  float tMaxX = MAX(a.x, MAX(b.x, c.x)); 
  float tMaxY = MAX(a.y, MAX(b.y, c.y)); 
  float tMaxZ = MAX(a.z, MAX(b.z, c.z)); 

	if (tMinX >= aabb_max.x || tMaxX <= aabb_min.x
		|| tMinY >= aabb_max.y || tMaxY <= aabb_min.y
		|| tMinZ >= aabb_max.z || tMaxZ <= aabb_min.z)
		return false;

	Vector3 center = Vector3Scale(Vector3Add(aabb_min, aabb_max), 0.5f);
	Vector3 h = Vector3Subtract(aabb_max, center);

	const Vector3 t[3] = { Vector3Subtract(b, a), Vector3Subtract(c, a), Vector3Subtract(c, b) };

	Vector3 ac = Vector3Subtract(a, center);

	Vector3 n = Vector3CrossProduct(t[0], t[1]);
	float s = Vector3DotProduct(n, ac);
	float r = fabs(Vector3DotProduct(h, (Vector3) {fabs(n.x), fabs(n.y), fabs(n.z)}));
	if (fabs(s) >= r)
		return false;

	const Vector3 at[3] = { vectorAbs(t[0]), vectorAbs(t[1]), vectorAbs(t[2]) };

	Vector3 bc = Vector3Subtract(b, center);
	Vector3 cc = Vector3Subtract(c, center);

	/*
	float d1, d2, a1, a2;
	const vec e[3] = { DIR_VEC(1, 0, 0), DIR_VEC(0, 1, 0), DIR_VEC(0, 0, 1) };
	for(int i = 0; i < 3; ++i)
		for(int j = 0; j < 3; ++j)
		{
			vec axis = Cross(e[i], t[j]);
			ProjectToAxis(axis, d1, d2);
			aabb.ProjectToAxis(axis, a1, a2);
			if (d2 <= a1 || d1 >= a2) return false;
		}
	*/

	// eX <cross> t[0]
	float d1 = t[0].y * ac.z - t[0].z * ac.y;
	float d2 = t[0].y * cc.z - t[0].z * cc.y;
	float tc = (d1 + d2) * 0.5f;
	r = fabs(h.y * at[0].z + h.z * at[0].y);
	if (r + fabs(tc - d1) < fabs(tc))
		return false;

	// eX <cross> t[1]
	d1 = t[1].y * ac.z - t[1].z * ac.y;
	d2 = t[1].y * bc.z - t[1].z * bc.y;
	tc = (d1 + d2) * 0.5f;
	r = fabs(h.y * at[1].z + h.z * at[1].y);
	if (r + fabs(tc - d1) < fabs(tc))
		return false;

	// eX <cross> t[2]
	d1 = t[2].y * ac.z - t[2].z * ac.y;
	d2 = t[2].y * bc.z - t[2].z * bc.y;
	tc = (d1 + d2) * 0.5f;
	r = fabs(h.y * at[2].z + h.z * at[2].y);
	if (r + fabs(tc - d1) < fabs(tc))
		return false;

	// eY <cross> t[0]
	d1 = t[0].z * ac.x - t[0].x * ac.z;
	d2 = t[0].z * cc.x - t[0].x * cc.z;
	tc = (d1 + d2) * 0.5f;
	r = fabs(h.x * at[0].z + h.z * at[0].x);
	if (r + fabs(tc - d1) < fabs(tc))
		return false;

	// eY <cross> t[1]
	d1 = t[1].z * ac.x - t[1].x * ac.z;
	d2 = t[1].z * bc.x - t[1].x * bc.z;
	tc = (d1 + d2) * 0.5f;
	r = fabs(h.x * at[1].z + h.z * at[1].x);
	if (r + fabs(tc - d1) < fabs(tc))
		return false;

	// eY <cross> t[2]
	d1 = t[2].z * ac.x - t[2].x * ac.z;
	d2 = t[2].z * bc.x - t[2].x * bc.z;
	tc = (d1 + d2) * 0.5f;
	r = fabs(h.x * at[2].z + h.z * at[2].x);
	if (r + fabs(tc - d1) < fabs(tc))
		return false;

	// eZ <cross> t[0]
	d1 = t[0].x * ac.y - t[0].y * ac.x;
	d2 = t[0].x * cc.y - t[0].y * cc.x;
	tc = (d1 + d2) * 0.5f;
	r = fabs(h.y * at[0].x + h.x * at[0].y);
	if (r + fabs(tc - d1) < fabs(tc))
		return false;

	// eZ <cross> t[1]
	d1 = t[1].x * ac.y - t[1].y * ac.x;
	d2 = t[1].x * bc.y - t[1].y * bc.x;
	tc = (d1 + d2) * 0.5f;
	r = fabs(h.y * at[1].x + h.x * at[1].y);
	if (r + fabs(tc - d1) < fabs(tc))
		return false;

	// eZ <cross> t[2]
	d1 = t[2].x * ac.y - t[2].y * ac.x;
	d2 = t[2].x * bc.y - t[2].y * bc.x;
	tc = (d1 + d2) * 0.5f;
	r = fabs(h.y * at[2].x + h.x * at[2].y);
	if (r + fabs(tc - d1) < fabs(tc))
		return false;

	// No separating axis exists, the AABB and triangle intersect.
	return true;
}

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

            if (Vector3DotProduct(normal, (Vector3){0.0f, 1.0f, 0.0f}) > 0.1f) continue;

#if 0
            Vector3 penetration;
            if (sphereCollidesTriangleEx(nextPos, PLAYER_RADIUS, vertex1, vertex2, vertex3, normal, &penetration)) {

                Vector3 dir = Vector3Normalize(Vector3Subtract(curPos, nextPos));
                if (Vector3DotProduct(penetration, dir) <= 0) continue;
                
                //rebounds[reboundLen][0] = penetration;
                //rebounds[reboundLen][0].x = Vector3DotProduct(normal, dir);
                rebounds[reboundLen][0].x = Vector3LengthSqr(Vector3Subtract(nextPos, Vector3Add(nextPos, penetration)));
#else
#if 0
            if (sphereCollidesTriangle(nextPos, PLAYER_RADIUS, vertex1, vertex2, vertex3)) {
#else
            Vector3 aabb_min = Vector3Subtract(nextPos, (Vector3){PLAYER_RADIUS, PLAYER_RADIUS, PLAYER_RADIUS});
            Vector3 aabb_max = Vector3Add(nextPos, (Vector3){PLAYER_RADIUS, PLAYER_RADIUS, PLAYER_RADIUS});
            if (triangleAABBIntersects(aabb_min, aabb_max, vertex1, vertex2, vertex3)) {
#endif
                float projection = Vector3DotProduct(Vector3Subtract(nextPos, vertex1), normal);
                rebounds[reboundLen][0] = Vector3Scale(normal, PLAYER_RADIUS - projection);
                //Vector3 dir = Vector3Normalize(Vector3Subtract(curPos, nextPos));
                //rebounds[reboundLen][0].x = Vector3DotProduct(normal, dir);
#endif
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
#if 0
            if (Vector3LengthSqr(rebounds[j][0]) > Vector3LengthSqr(rebounds[j + 1][0])) {
#else
            if (rebounds[j][0].x < rebounds[j + 1][0].x) {
            //if (rebounds[j][0].x > rebounds[j + 1][0].x) {
#endif
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
#if 0
        Vector3 penetration;
        if (sphereCollidesTriangleEx(nextPos, PLAYER_RADIUS, rebounds[i][1], rebounds[i][2], rebounds[i][3], rebounds[i][4], &penetration)) {
            nextPos = Vector3Add(nextPos, penetration);
            printf("collided, size: %f, up dot: %f, front dot: %f, inverse (wall x normal) dot: %f", Vector3Length(penetration), Vector3DotProduct(rebounds[i][4], (Vector3){0.0f,1.0f,0.0f}), Vector3DotProduct(rebounds[i][4], (Vector3){1.0f, 0.0f, 0.0f}), rebounds[i][0].x);
        }
#else
#if 0
        if (sphereCollidesTriangle(nextPos, PLAYER_RADIUS, rebounds[i][1], rebounds[i][2], rebounds[i][3])) {
#else
        Vector3 aabb_min = Vector3Subtract(nextPos, (Vector3){PLAYER_RADIUS, PLAYER_RADIUS, PLAYER_RADIUS});
        Vector3 aabb_max = Vector3Add(nextPos, (Vector3){PLAYER_RADIUS, PLAYER_RADIUS, PLAYER_RADIUS});
        if (triangleAABBIntersects(aabb_min, aabb_max, rebounds[i][1], rebounds[i][2], rebounds[i][3])) {
#endif
            float projection = Vector3DotProduct(Vector3Subtract(nextPos, rebounds[i][1]), rebounds[i][4]);

            Vector3 dir = Vector3Subtract(nextPos, curPos);
            float comp1 = Vector3DotProduct(dir, rebounds[i][4]);
            Vector3 perp = Vector3Normalize(Vector3CrossProduct(rebounds[i][4], (Vector3){0.0f, 1.0f, 0.0f}));
            float comp2 = Vector3DotProduct(dir, perp);

            //Vector3 rebound = Vector3Scale(rebounds[i][4], PLAYER_RADIUS - projection);
            //nextPos = Vector3Add(nextPos, rebound);
            Vector3 rebound = Vector3Add(curPos, Vector3Add(Vector3Scale(perp, comp2), Vector3Scale(rebounds[i][4], MAX(comp1, 0.0f))));

            //if (Vector3Length(Vector3Subtract(nextPos, curPos)) < 0.01f) return curPos;

            //printf("collided, size: %f", Vector3Length(Vector3Subtract(nextPos, curPos)));
            //printf("collided, size: %f", Vector3Length(penetration));

#if 0
            if (comp2 > 0.001f || comp2 < 0.001f)
                nextPos = rebound;
            else
                nextPos = curPos;
#endif
            nextPos = rebound;
        }
#endif
        else {
            //printf("no collision, size: %f", Vector3Length(rebounds[i][0]));
        }
        //printf("\n");
    }

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

    SetupPlayer(&player);
    SetupGun(&player.currentGun);

    Model mapModel = LoadModel("assets/final_map.obj");
    mapModel.materials[0].shader = shader;

    while (!WindowShouldClose()) {
        MovePlayer(mapModel, &player);
        UpdateCameraFPS(&player);
        UpdateCarriedGun(&player.currentGun, player);

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
