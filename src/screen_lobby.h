GameScreen lobbyMain() {
    GuiLoadStyleDefault();

    char ipInput[40] = {0}, portInput[20] = {0};
    bool ipInputEdit = false, portInputEdit = false;
    pthread_t thread_id;
    while (!WindowShouldClose()) {
        BeginDrawing();

        ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));

        GuiGrid((Rectangle) { 0, 0, GetScreenWidth(), GetScreenHeight() }, 20.0f, 2); // draw a fancy grid

        if (GuiButton((Rectangle) { 10, 10, 205, 20 }, "Host")) {
            pthread_create(&thread_id, NULL, serverMain, NULL);
            break;
        }

        if (GuiTextBox((Rectangle) { 10, 40, 100, 20}, ipInput, 20, ipInputEdit)) ipInputEdit = !ipInputEdit;
        if (GuiTextBox((Rectangle) { 115, 40, 40, 20}, portInput, 20, portInputEdit)) portInputEdit = !portInputEdit;
        if (GuiButton((Rectangle) { 165, 40, 50, 20 }, "Join")) {
            printf("%s:%s\n", ipInput, portInput);
        }

        EndDrawing();
    }

    return SCREEN_GAME;
}
