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
#define MAX_PLAYERS 10

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define WORLD_UP_VECTOR (Vector3) {0.0f, 1.0f, 0.0f}

#define SERVER_ADDR "127.0.0.1"

typedef enum {
    SCREEN_LOBBY,
    SCREEN_GAME,
} GameScreen;

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
} Gun;

typedef struct {
    bool isActive;

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

typedef enum {
    PROJECTILE_GRENADE,
    PROJECTILE_EXPLOSION,
} ProjectileType;

typedef struct {
    Vector3 position[MAX_PROJECTILES];
    Vector3 velocity[MAX_PROJECTILES];
    float radius[MAX_PROJECTILES];
    float lifetime[MAX_PROJECTILES];
    ProjectileType type[MAX_PROJECTILES];
    int owners[MAX_PROJECTILES];
    int count;
} Projectiles;

typedef struct {
    Vector3 position;
    float radius;
    ProjectileType type;
} NetworkProjectile;

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

    Player players[MAX_PLAYERS];
    int playersLen;

    NetworkProjectile projectiles[MAX_PROJECTILES];
    int projectilesLen;

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

    PACKET_INPUT,
    PACKET_STATE,
    PACKET_PROJECTILES,

    PACKET_JOIN,
    PACKET_PLAYER_LIST,
} PacketType;

typedef struct {
    PacketType type;

    int playerID;

    Vector3 position;
    Vector2 angle;
    Vector3 size;

    bool shoot;

    GunType currentGun;
} InputPacket;

typedef struct {
    PacketType type;

    Vector3 playersPositions[MAX_PLAYERS];
    Vector2 playersAngles[MAX_PLAYERS];
    GunType playersGuns[MAX_PLAYERS];
    float playersHealth[MAX_PLAYERS];
} StatePacket;

typedef struct {
    PacketType type;

    int len;
    NetworkProjectile projectiles[];
} ProjectilesPacket;

typedef struct {
    PacketType type;
} JoinPacket;

typedef struct {
    PacketType type;

    int allIds[MAX_PLAYERS];
    int allIdsLen;

    int clientId;
} PlayerListPacket;
