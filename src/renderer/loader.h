#pragma once
#include "descriptor.h"
#include "scene.h"
#include "types.h"
#include <optional>

class Renderer;

std::optional<std::shared_ptr<Scene>> loadGltf(Renderer *renderer,
                                               std::string_view filePath);
