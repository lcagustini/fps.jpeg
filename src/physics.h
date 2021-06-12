#include "types.h"

Vector3 vectorAbs(Vector3 v) {
  return (Vector3) {fabs(v.x), fabs(v.y), fabs(v.z)};
}

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
#if 0
        //if (penetrationVec) *penetrationVec = Vector3Scale(Vector3Normalize(intersection_vec), radius - len);  // normalize
#else
        if (penetrationVec) *penetrationVec = Vector3Scale(normal, t0);
#endif

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

// TODO: use output velocity/normal also for other ResponseTypes?, currently only working for BOUNCE
Vector3 CollideWithMap(Model mapModel, Vector3 curPos, Vector3 nextPos, HitboxType hitbox, float radius, CollisionResponseType response, Vector3 *velocity, Vector3 *hitNormal) {
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

            bool intersects = false;
            if (hitbox == HITBOX_AABB) {
                Vector3 aabb_radius = (Vector3) {radius, radius, radius};
                Vector3 aabb_min = Vector3Subtract(nextPos, aabb_radius);
                Vector3 aabb_max = Vector3Add(nextPos, aabb_radius);

                intersects = triangleAABBIntersects(aabb_min, aabb_max, vertex1, vertex2, vertex3);
            } else if (hitbox == HITBOX_SPHERE) {
                intersects = sphereCollidesTriangle(nextPos, radius, vertex1, vertex2, vertex3);
            }

            if (intersects) {
                float projection = Vector3DotProduct(Vector3Subtract(nextPos, vertex1), normal);

                rebounds[reboundLen][0] = Vector3Scale(normal, radius - projection);
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

    for (int i = 0; i < reboundLen; i++) {
        bool intersects = false;
        if (hitbox == HITBOX_AABB) {
            Vector3 aabb_radius = (Vector3) {radius, radius, radius};
            Vector3 aabb_min = Vector3Subtract(nextPos, aabb_radius);
            Vector3 aabb_max = Vector3Add(nextPos, aabb_radius);

            intersects = triangleAABBIntersects(aabb_min, aabb_max, rebounds[i][1], rebounds[i][2], rebounds[i][3]);
        } else if (hitbox == HITBOX_SPHERE) {
            intersects = sphereCollidesTriangle(nextPos, radius, rebounds[i][1], rebounds[i][2], rebounds[i][3]);
        }

        Vector3 dir = Vector3Subtract(nextPos, curPos);
        Vector3 normal = rebounds[i][4];
        Vector3 rebound = nextPos;
        if (intersects) {
            // collision response
            //printf("%f %f %f\n", normal.x, normal.y, normal.z);
            if (response == COLLIDE_AND_SLIDE) {
                float comp1 = Vector3DotProduct(dir, normal);
                Vector3 perp = Vector3Normalize(Vector3CrossProduct(normal, WORLD_UP_VECTOR));
                float comp2 = Vector3DotProduct(dir, perp);

                rebound = Vector3Add(curPos, Vector3Add(Vector3Scale(perp, comp2), Vector3Scale(rebounds[i][4], MAX(comp1, 0.0f))));
            } else if (response == COLLIDE_AND_BOUNCE) {
                assert(velocity);
                rebound = Vector3Subtract(*velocity, Vector3Scale(normal, 2 * Vector3DotProduct(*velocity, normal)));

                assert(velocity);
                assert(hitNormal);
                *velocity = rebound;
                *hitNormal = normal;

                rebound = Vector3Add(curPos, Vector3Scale(rebound, GetFrameTime()));
            }
        }
        nextPos = rebound;
    }

    // hack to prevent going out of bounds by shoving head into corners
    if (Vector3Length(Vector3Subtract(nextPos, curPos)) < 0.004f) return curPos;

    return nextPos;
}

Vector3 GetGroundNormal(Model mapModel, Vector3 position) {
    Ray ray = {
        .position = position,
        .direction = (Vector3) { 0.0f, -1.0f, 0.0f }
    };
    RayCollision hit = GetRayCollisionModel(ray, mapModel);
    return hit.normal;
}

Vector3 PlayerCollideWithMapGravity(Model mapModel, Vector3 nextPos, float radius, Vector3 *velocity, bool *grounded) {
    Ray ray = {
        .position = nextPos,
        .direction = (Vector3) { 0.0f, -1.0f, 0.0f }
    };
    RayCollision hit = GetRayCollisionModel(ray, mapModel);
    if (hit.hit && hit.distance < radius) {
        nextPos = Vector3Add(hit.point, Vector3Scale(WORLD_UP_VECTOR, radius));
        *grounded = true;
        velocity->y = 0;
    } else {
        *grounded = false;
    }

    return nextPos;
}

