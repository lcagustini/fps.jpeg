GameScreen gameMain() {
    World world = {
        .map = LoadModel("assets/map2.obj"),
        .lights = {
            .lightsLenLoc = GetShaderLocation(shader, "lightsLen"),
            .lightsPositionLoc = GetShaderLocation(shader, "lightsPosition"),
            .lightsColorLoc = GetShaderLocation(shader, "lightsColor"),

            .lightsPosition = { (Vector3) { 2.0, 3.0, 2.0 }, (Vector3) { -2.0, 4.0, -3.0 } },
            .lightsColor = { (Vector3) { 0.6, 0.5, 0.4 }, (Vector3) { 0.5, 0.5, 0.5 } },
            .lightsLen = 2,
        }
    };

    int timeLoc = GetShaderLocation(shader, "time");
    int isMapLoc = GetShaderLocation(shader, "isMap");

    SOCKET socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    validateSocket(socket_fd);
    setupSocket(socket_fd);

    struct sockaddr_in inbound_addr = { 0 };
    unsigned int inbound_addr_len = sizeof(inbound_addr);

    struct sockaddr_in server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(20586);
    inet_pton(AF_INET, SERVER_ADDR, &server_address.sin_addr.s_addr);

    SetupWorld(&world);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        SetupPlayer(&world.players[i]);
    }

    while (!WindowShouldClose()) {
        while (true) {
            PacketType type;
            int ret = peekPacket(socket_fd, &server_address, &type, NULL);
            if (ret <= 0) break;

            checkClientState();

            switch (type) {
                case PACKET_PLAYER_LIST:
                    {
                        PlayerListPacket playerListPacket = {0};
                        recvfrom(socket_fd, &playerListPacket, sizeof(playerListPacket), 0, (struct sockaddr *)&server_address, &inbound_addr_len);

                        localPlayerID = playerListPacket.clientId;

                        for (int i = 0; i < MAX_PLAYERS; i++) {
                            world.players[i].isActive = false;
                        }

                        printf("Got id %d\n", localPlayerID);
                        for (int i = 0; i < playerListPacket.allIdsLen; i++) {
                            world.players[playerListPacket.allIds[i]].isActive = true;
                            printf("Id %d is online\n", playerListPacket.allIds[i]);
                        }
                    }
                    break;
                case PACKET_STATE:
                    {
                        StatePacket statePacket = {0};
                        recvfrom(socket_fd, &statePacket, sizeof(statePacket), 0, (struct sockaddr *)&server_address, &inbound_addr_len);

                        for (int i = 0; i < MAX_PLAYERS; i++) {
                            if (i != localPlayerID) {
                                world.players[i].position = statePacket.playersPositions[i];
                                world.players[i].cameraFPS.angle = statePacket.playersAngles[i];

                                if (world.players[i].currentGun.type != statePacket.playersGuns[i]) {
                                    UnloadModel(world.players[i].currentGun.model);
                                    world.players[i].currentGun = SetupGun(statePacket.playersGuns[i]);
                                }
                            }
                            world.players[i].health = statePacket.playersHealth[i];
                        }
                    }
                    break;
                case PACKET_PROJECTILES:
                    {
                        int len;
                        peekPacket(socket_fd, &server_address, NULL, &len);

                        int projectilesPacketSize = sizeof(ProjectilesPacket) + len * sizeof(NetworkProjectile);

                        ProjectilesPacket *projectilesPacket = malloc(projectilesPacketSize);
                        recvfrom(socket_fd, projectilesPacket, projectilesPacketSize, 0, (struct sockaddr *)&server_address, &inbound_addr_len);

                        memcpy(&world.projectiles[0], &projectilesPacket->projectiles[0], len * sizeof(NetworkProjectile));
                        world.projectilesLen = len;

                        free(projectilesPacket);
                    }
                    break;
                default:
                    //if (type != 0) printf("got %d\n", type);
                    break;
            }
        }

        if (localPlayerID == -1) {
            JoinPacket joinPacket = { PACKET_JOIN };
            sendto(socket_fd, &joinPacket, sizeof(joinPacket), 0, (struct sockaddr *)&server_address, sizeof(server_address));

            usleep(100);

            continue;
        }

        MovePlayer(world.map, &world.players[localPlayerID]);

        //TODO: limit send rate
        InputPacket inputPacket = {
            .type = PACKET_INPUT,
            .playerID = localPlayerID,
            .position = world.players[localPlayerID].position,
            .angle = world.players[localPlayerID].cameraFPS.angle,
            .size = world.players[localPlayerID].size,
            .shoot = IsKeyPressed(world.players[localPlayerID].inputBindings[SHOOT]),
            .currentGun = world.players[localPlayerID].currentGun.type,
        };
        sendto(socket_fd, &inputPacket, sizeof(inputPacket), 0, (struct sockaddr *)&server_address, sizeof(server_address));

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!world.players[i].isActive) continue;

            UpdatePlayer(i, world.players, world.map);
        }

        UpdateLights(world.lights);

        BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode3D(world.players[localPlayerID].cameraFPS.camera);

        float timestamp = gettimestamp();
        double integral;
        timestamp = modf(timestamp, &integral) * 10;
        //printf("timestamp = %lf\n", timestamp);

        SetShaderValue(shader, timeLoc, &timestamp, SHADER_UNIFORM_FLOAT);

        // section UV[0..0.5][0..0.5] is a procedurally generated wall texture
        int isMap = 1;

        SetShaderValue(shader, isMapLoc, &isMap, SHADER_UNIFORM_INT);
        DrawModel(world.map, (Vector3) {0}, 1.0f, WHITE);
        isMap = 0;
        SetShaderValue(shader, isMapLoc, &isMap, SHADER_UNIFORM_INT);

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!world.players[i].isActive) continue;

            DrawModel(world.players[i].model, Vector3Zero(), 1.0f, WHITE);
            DrawCubeWires(world.players[i].position, 2 * world.players[i].size.x, 2 * world.players[i].size.y, 2 * world.players[i].size.z, BLUE);
            DrawModel(world.players[i].currentGun.model, Vector3Zero(), 1.0f, WHITE);
        }

        DrawProjectiles(world.projectiles, world.projectilesLen);

        EndMode3D();

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!world.players[i].isActive) continue;

            DrawRectangleGradientH(10, i * 25 + 10, 20 * world.players[i].health, 20, RED, GREEN);
        }

        DrawLine(GetScreenWidth() / 2 - 6, GetScreenHeight() / 2, GetScreenWidth() / 2 + 5, GetScreenHeight() / 2, MAGENTA);
        DrawLine(GetScreenWidth() / 2, GetScreenHeight() / 2 - 6, GetScreenWidth() / 2, GetScreenHeight() / 2 + 5, MAGENTA);

        EndDrawing();
    }

    UnloadModel(world.map);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        UnloadModel(world.players[i].model);
        UnloadModel(world.players[i].currentGun.model);
    }

    socketClose(socket_fd);

    return SCREEN_CLOSE;
}
