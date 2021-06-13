GameScreen lobbyMain() {
    GuiLoadStyleDefault();

    char ipInput[20] = {0}, portInput[20] = {0};
    bool ipInputEdit = false, portInputEdit = false;
    while (!WindowShouldClose()) {
        BeginDrawing();

        ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));

        GuiGrid((Rectangle) { 0, 0, GetScreenWidth(), GetScreenHeight() }, 20.0f, 2); // draw a fancy grid

        if (GuiButton((Rectangle) { 10, 10, 215, 20 }, "Host")) {
	    startServerThread();
            return SCREEN_GAME;
        }

        if (GuiTextBox((Rectangle) { 10, 40, 100, 20 }, ipInput, 20, ipInputEdit)) ipInputEdit = !ipInputEdit;
        if (GuiTextBox((Rectangle) { 115, 40, 50, 20 }, portInput, 20, portInputEdit)) portInputEdit = !portInputEdit;
        if (GuiButton((Rectangle) { 175, 40, 50, 20 }, "Join")) {
            printf("Joining %s:%s\n", ipInput, portInput);
            strcpy(serverAddress, ipInput);
            serverPort = strtol(portInput, NULL, 10);
            return SCREEN_GAME;
        }

        EndDrawing();
    }

    return SCREEN_CLOSE;
}
