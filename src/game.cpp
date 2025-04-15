#include "game.h"
#include <imgui_impl_sdl3.h>

Uint64 TIMESTEP_MS = 1000 / 60;
float TIMESTEP_S = (float)TIMESTEP_MS / 1000;

const std::unordered_map<UnitType, UnitData> UnitData::registry = {
    {UnitType::kCube, {"Cube", 5.f}},
};

Game::Game() : _camera(_input) {}

void Game::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "could not initialize sdl3: %s\n", SDL_GetError());
        abort();
    }

    _window = SDL_CreateWindow("hello_sdl3", SCREEN_WIDTH, SCREEN_HEIGHT,
                               SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (_window == NULL) {
        fprintf(stderr, "could not create window: %s\n", SDL_GetError());
        abort();
    }

    _camera.set_screen_dimensions((float)SCREEN_WIDTH, (float)SCREEN_HEIGHT);

    _renderer.init(_window);
    _renderer.set_camera_view(_camera.get_view_matrix());
    _renderer.set_camera_projection(_camera.get_projection_matrix());

    void *memory = malloc(GAME_MEMORY);
    arena_init(&_arena, GAME_MEMORY, (uint8_t *)memory);

    _world = generate_world(&_arena);
    _renderer.update_tile_draw_commands(create_tile_map_mesh(_world->tile_map));

    auto meshFile = loadGltf(&_renderer, "assets/meshes/basicmesh.glb");
    assert(meshFile.has_value());
    _assets = *meshFile;

    init_test_entities();
}

void Game::deinit() {
    _renderer.deinit();
    SDL_DestroyWindow(_window);
    SDL_Quit();
}

void Game::run() {
    Uint64 nextGameStep = SDL_GetTicks();
    Uint64 last = nextGameStep;
    Uint64 now = last;
    SDL_Event e;
    _isRunning = true;
    while (_isRunning) {
        last = now;
        now = SDL_GetTicks();
        if (nextGameStep >= now) {
            SDL_Delay(nextGameStep - now);
            continue;
        }

        while (nextGameStep <= now) {
            while (SDL_PollEvent(&e)) {
                ImGui_ImplSDL3_ProcessEvent(&e);
                if (e.type == SDL_EVENT_QUIT) {
                    _isRunning = false;
                } else if (e.type == SDL_EVENT_WINDOW_RESIZED) {
                    int width, height;
                    SDL_GetWindowSizeInPixels(_window, &width, &height);
                    if (width > 0 && height > 0) {
                        _camera.set_screen_dimensions((float)width,
                                                      (float)height);
                    }
                    _renderer.set_camera_projection(
                        _camera.get_projection_matrix());
                } else {
                    _input.process_event(e);
                }
            }

            nextGameStep += TIMESTEP_MS;
        }

        _input.update();
        _camera.update(TIMESTEP_S);
        if (_camera._isDirty) {
            _renderer.set_camera_projection(_camera.get_projection_matrix());
            _camera._isDirty = false;
        }

        if (_input.hasPendingPickRequest()) {
            handle_pick_request();
        }

        if (_input.hasRightClickRequest()) {
            handle_move_request();
        }

        update_positions();

        _input.reset();

        auto cmd = _renderer.begin_frame();
        render_entities();
        _renderer.draw(cmd);
        _renderer.end_frame(cmd, now - last);
    }
}

void Game::handle_pick_request() {
    Ray clickRay = screen_point_to_ray(_input.lastLeftClickPos());

    entt::entity selectedEntity = entt::null;

    auto view = _registry.view<UnitType, PositionComponent>();
    view.each([this, clickRay, &selectedEntity](const auto entity, auto &t,
                                                auto &p) {
        UnitData unitData = UnitData::registry.at(t);
        // TODO: This sucks
        Bounds bounds = _assets->meshes.at(unitData.name)->surfaces[0].bounds;
        glm::mat4 worldTransform = p.value.to_world_transform().as_matrix();
        glm::mat4 invWorldTransform = glm::inverse(worldTransform);

        glm::vec3 localRayOrigin =
            glm::vec3(invWorldTransform * glm::vec4(clickRay.origin, 1.0f));
        glm::vec3 localRayDir =
            glm::vec3(invWorldTransform * glm::vec4(clickRay.direction, 0.0f));
        localRayDir = glm::normalize(localRayDir);
        Ray localRay = Ray{localRayOrigin, localRayDir};

        glm::vec3 localAABBMin = bounds.origin - bounds.extents;
        glm::vec3 localAABBMax = bounds.origin + bounds.extents;

        if (math::intersect_ray_aabb(localRay, localAABBMin, localAABBMax)) {
            selectedEntity = entity;
        }
    });

    if (selectedEntity == entt::null) {
        return;
    }

    _registry.view<Selected>()->clear();
    _registry.emplace<Selected>(selectedEntity);
}

void Game::handle_move_request() {
    Ray ray = screen_point_to_ray(_input.lastRightClickPos());

    // TODO: Only works when ground is 0.f high
    glm::vec3 groundPlaneOrigin = glm::vec3(0.0f, 0.f, 0.0f);
    glm::vec3 groundPlaneNormal = glm::vec3(0.0f, 1.0f, 0.0f);

    auto selectedView = _registry.view<Selected>();
    selectedView.each(
        [this, ray, groundPlaneOrigin, groundPlaneNormal](const auto entity) {
            glm::vec3 intersectionPoint = math::intersect_ray_plane(
                ray, groundPlaneOrigin, groundPlaneNormal);
            _registry.emplace_or_replace<TargetPositionComponent>(
                entity, intersectionPoint);
        });
}

void Game::render_entities() {
    auto view = _registry.view<UnitType, PositionComponent>();
    view.each([this](const auto entity, auto &t, auto &p) {
        UnitData unitData = UnitData::registry.at(t);
        std::shared_ptr<MeshAsset> mesh = _assets->meshes.at(unitData.name);
        glm::mat4 worldTransform = p.value.to_world_transform().as_matrix();
        for (auto s : mesh->surfaces) {
            _renderer.write_draw_command(MeshDrawCommand{
                .indexCount = s.count,
                .firstIndex = s.startIndex,
                .indexBuffer = mesh->meshBuffers.indexBuffer.buffer,
                .material = &s.material->data,
                .bounds = s.bounds,
                .transform = worldTransform,
                .vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress,
                .isOutlined = _registry.storage<Selected>().contains(entity),
            });
        }
    });
}

void Game::add_entity(UnitType &&type, WorldPosition &&pos) {
    auto data = UnitData::registry.at(type);
    const auto entity = _registry.create();
    _registry.emplace<PositionComponent>(entity, PositionComponent{pos});
    _registry.emplace<UnitType>(entity, type);
    _registry.emplace<MovementSpeed>(entity, data.movementSpeed);
}

void Game::update_positions() {}

void Game::init_test_entities() {
    add_entity(UnitType::kCube, {5, 5, 0.f, 0.f});
    add_entity(UnitType::kCube, {10, 10, 0.f, 0.f});
}

Ray Game::screen_point_to_ray(glm::vec2 &&point) {
    VkExtent2D extent = _renderer.swapchainExtent();
    assert(extent.width > 0 && extent.height > 0);

    float ndcX = (2.0f * point.x) / (float)extent.width - 1.0f;
    float ndcY = (2.0f * point.y) / (float)extent.height - 1.0f;
    glm::vec2 ndcCoords = {ndcX, ndcY};

    // TODO: Maybe cache the matrices in the camera class?
    glm::mat4 projMatrix = _camera.get_projection_matrix();
    glm::mat4 viewMatrix = _camera.get_view_matrix();
    glm::mat4 invProj = glm::inverse(projMatrix);
    glm::mat4 invView = glm::inverse(viewMatrix);

    glm::vec4 rayOriginClip =
        invProj * glm::vec4(ndcCoords.x, ndcCoords.y, 0.0f, 1.0f);
    glm::vec4 rayOriginWorldH = invView * rayOriginClip;
    glm::vec3 rayOrigin = glm::vec3(rayOriginWorldH);
    glm::vec3 rayDirection = glm::normalize(
        -glm::vec3(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2]));

    return Ray{.origin = rayOrigin, .direction = rayDirection};
}
