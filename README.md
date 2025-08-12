# C++ Game Engine & Vulkan Renderer

This is a from-scratch game engine built in modern **C++20**. This project was undertaken as a personal, learning-focused deep dive into the fundamental principles of low-level game engine architecture, graphics programming, and modern C++ practices.

---

## Core Technical Features & Learning Goals

*   **Language:** Modern **C++20**, utilizing features like modules, concepts, and ranges where appropriate.
*   **Graphics API: A Deep Dive into Vulkan**
    The engine features a low-level rendering backend built directly on **Vulkan** to gain a mastery of modern, explicit graphics APIs. The implementation includes:

    *   **Core Rendering Pipeline:**
        *   Management of the full Vulkan lifecycle: instance, device, queues, and swapchain.
        *   A robust system for command buffer recording, submission, and synchronization using fences and semaphores.
        *   A flexible material and shader system that sends data to the GPU via a combination of **descriptor sets** for resource binding and **push constants** for low-overhead, dynamic data.

    *   **Advanced Rendering Techniques:**
        *   **Dynamic Shadow Mapping:** Implemented shadow maps to provide real-time, dynamic shadows for objects in the scene.
        *   **Post-Processing Effects:** A basic post-processing pipeline, used to implement effects like selectable object **outlines**.

    *   **Performance and Optimization:**
        *   **Frustum Culling:** Implemented geometric frustum culling on the CPU to efficiently determine which objects are visible and avoid submitting unnecessary draw calls.
        *   **Efficient GPU Memory Management:** Leveraged the **Vulkan Memory Allocator (VMA)** library for robust and efficient GPU memory management, a best practice for modern Vulkan applications, avoiding naive `vkAllocateMemory` calls.
*   **Windowing & Input:** Cross-platform window and input handling managed by **SDL3**.
*   **Entity Component System (ECS):** Game object architecture is built around the excellent **`EnTT`** library, a staple of modern C++ game development. This provides a clean, data-oriented foundation for game logic.
*   **Build System:** A clean, modern build process managed by **CMake**.

