#define CAMERA_FIRST_PERSON_FOCUS_DISTANCE              25.0f
#define CAMERA_FIRST_PERSON_MIN_CLAMP                   89.0f
#define CAMERA_FIRST_PERSON_MAX_CLAMP                  -89.0f

#define CAMERA_FIRST_PERSON_STEP_TRIGONOMETRIC_DIVIDER  8.0f
#define CAMERA_FIRST_PERSON_STEP_DIVIDER                30.0f
#define CAMERA_FIRST_PERSON_WAVING_DIVIDER              200.0f

#define PLAYER_MOVEMENT_SENSITIVITY                     0.25f

#define CAMERA_MOUSE_MOVE_SENSITIVITY                   0.003f
#define CAMERA_MOUSE_SCROLL_SENSITIVITY                 1.5f

#define MAX_HEALTH 10.0f

#define MAX_PROJECTILES 100

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define WORLD_UP_VECTOR (Vector3) {0.0f, 1.0f, 0.0f}

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

typedef enum {
    GUN_GRENADE,
    GUN_BULLET
} GunType;

typedef struct {
    Model model;
    GunType type;
    float damage;
} Gun;

typedef struct {
    Model model;

    Vector3 position;
    Vector3 velocity;

    Vector3 size;

    Gun currentGun;

    float health;
    bool grounded;

    CameraFPS cameraFPS;
    char inputBindings[INPUT_ALL];
} Player;

typedef struct {
    Vector3 position[MAX_PROJECTILES];
    Vector3 velocity[MAX_PROJECTILES];
    float radius[MAX_PROJECTILES];
    int count;
} Projectiles;

typedef struct {
    int lightsLenLoc;
    int lightsPositionLoc;
    int lightsColorLoc;

    Vector3 lightsPosition[10];
    Vector3 lightsColor[10];
    int lightsLen;
} LightSystem;

typedef struct {
    Model map;

    Player players[4];
    int playersLen;

    Projectiles projectiles;

    LightSystem lights;
} World;

typedef enum {
  COLLIDE_AND_SLIDE,
  COLLIDE_AND_BOUNCE,
} CollisionResponseType;

typedef enum {
  HITBOX_AABB,
  HITBOX_SPHERE,
} HitboxType;

typedef enum {
    PACKET_ERROR,
    PACKET_SHOOT,
    PACKET_STATE,
    PACKET_JOIN,
    PACKET_PLAYER_LIST,
} PacketType;

typedef struct {
    PacketType type;
} ShootPacket;

typedef struct {
    PacketType type;
} JoinPacket;

typedef struct {
    PacketType type;

    int allIds[10];
    int allIdsLen;

    int clientId;
} PlayerListPacket;

typedef struct {
    PacketType type;

    Vector3 playersPositions[4];
    Vector3 playersAngles[4];
    int playersLen;
} StatePacket;
