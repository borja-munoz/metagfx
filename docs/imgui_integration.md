# ImGui Integration

## Overview

MetaGFX integrates [Dear ImGui](https://github.com/ocornut/imgui) to provide an immediate-mode GUI for runtime controls and debugging. ImGui is a bloat-free graphical user interface library for C++ that outputs optimized vertex buffers for efficient rendering.

The integration uses the official Vulkan and SDL3 backends to render UI elements directly on top of the 3D scene.

## Architecture

### Integration Points

ImGui is integrated at three key points in the application lifecycle:

1. **Initialization** ([Application.cpp:915-1030](../src/app/Application.cpp#L915-L1030)) - `InitImGui()`
2. **Per-Frame Rendering** ([Application.cpp:1065-1111](../src/app/Application.cpp#L1065-L1111)) - `RenderImGui()`
3. **Shutdown** ([Application.cpp:1032-1063](../src/app/Application.cpp#L1032-L1063)) - `ShutdownImGui()`

### Build Integration

ImGui source files are compiled directly into the application target rather than as a separate library:

**[src/app/CMakeLists.txt:7-14](../src/app/CMakeLists.txt#L7-L14)**:
```cmake
# ImGui sources
${CMAKE_SOURCE_DIR}/external/imgui/imgui.cpp
${CMAKE_SOURCE_DIR}/external/imgui/imgui_demo.cpp
${CMAKE_SOURCE_DIR}/external/imgui/imgui_draw.cpp
${CMAKE_SOURCE_DIR}/external/imgui/imgui_tables.cpp
${CMAKE_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_sdl3.cpp
${CMAKE_SOURCE_DIR}/external/imgui/backends/imgui_impl_vulkan.cpp
```

This approach avoids CMake complexity while keeping compilation units manageable.

## Initialization

### Descriptor Pool Creation

ImGui requires a dedicated Vulkan descriptor pool for managing its internal resources:

```cpp
VkDescriptorPoolSize poolSizes[] = {
    { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
};

VkDescriptorPoolCreateInfo poolInfo{};
poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
poolInfo.maxSets = 1000;
poolInfo.poolSizeCount = 11;
poolInfo.pPoolSizes = poolSizes;

vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_ImGuiDescriptorPool);
```

**Key Design Decisions**:
- `VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT` allows individual descriptor set freeing
- Generous pool sizes (1000 per type) to avoid exhaustion during complex UI
- Separate pool from main rendering to avoid interference

### ImGui Context Setup

```cpp
IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGuiIO& io = ImGui::GetIO();
io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

ImGui::StyleColorsDark();  // Dark theme
```

**Configuration**:
- `ImGuiConfigFlags_NavEnableKeyboard` enables keyboard navigation
- `StyleColorsDark()` applies the dark theme (alternatives: `StyleColorsLight()`, `StyleColorsClassic()`)

### Render Pass Creation

ImGui requires its own render pass to overlay UI on top of the 3D scene:

```cpp
VkAttachmentDescription attachment{};
attachment.format = VK_FORMAT_B8G8R8A8_SRGB;
attachment.samples = VK_SAMPLE_COUNT_1_BIT;
attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;     // Load existing content
attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
```

**Critical Settings**:
- `loadOp = VK_ATTACHMENT_LOAD_OP_LOAD` preserves the 3D scene rendered in the previous pass
- `initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` matches the output layout of the main render pass
- `finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` keeps the image ready for presentation

### Backend Initialization

```cpp
// SDL3 backend
ImGui_ImplSDL3_InitForVulkan(m_Window);

// Vulkan backend
ImGui_ImplVulkan_InitInfo initInfo{};
initInfo.Instance = context.instance;
initInfo.PhysicalDevice = context.physicalDevice;
initInfo.Device = context.device;
initInfo.QueueFamily = context.graphicsQueueFamily;
initInfo.Queue = context.graphicsQueue;
initInfo.DescriptorPool = m_ImGuiDescriptorPool;
initInfo.RenderPass = m_ImGuiRenderPass;
initInfo.MinImageCount = 2;
initInfo.ImageCount = 2;
initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

ImGui_ImplVulkan_Init(&initInfo);
```

**Double-Buffering**:
- `MinImageCount = 2` and `ImageCount = 2` match the swap chain's double-buffering
- Ensures ImGui resources are synchronized with frame pacing

### Font Texture Upload

```cpp
ImGui_ImplVulkan_CreateFontsTexture();
```

This function:
1. Creates internal command buffers
2. Uploads the default font atlas to GPU
3. Submits and waits for upload completion
4. Automatically cleans up staging resources

## Framebuffer Management

### Persistent Framebuffers

ImGui framebuffers are created **lazily** on first use and **reused** across frames:

```cpp
// During initialization
m_ImGuiFramebuffers.resize(imageCount, VK_NULL_HANDLE);

// During rendering (on-demand creation)
if (m_ImGuiFramebuffers[imageIndex] == VK_NULL_HANDLE) {
    VkImageView attachments[] = { vkTexture->GetImageView() };

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_ImGuiRenderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = swapChain->GetWidth();
    framebufferInfo.height = swapChain->GetHeight();
    framebufferInfo.layers = 1;

    vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_ImGuiFramebuffers[imageIndex]);
}
```

**Why Lazy Creation?**:
- Swap chain images may not be available during `InitImGui()`
- Image views are retrieved from the swap chain at render time
- Avoids coupling initialization order with swap chain creation

### Framebuffer Destruction

Framebuffers are destroyed:
1. **On window resize** - Swap chain recreation invalidates old framebuffers
2. **On shutdown** - Full cleanup during `ShutdownImGui()`

**Window Resize Handler** ([Application.cpp:580-599](../src/app/Application.cpp#L580-L599)):
```cpp
case SDL_EVENT_WINDOW_RESIZED:
    // Destroy old ImGui framebuffers before resizing swap chain
    for (auto framebuffer : m_ImGuiFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(context.device, framebuffer, nullptr);
        }
    }
    m_ImGuiFramebuffers.clear();

    m_Device->GetSwapChain()->Resize(event.window.data1, event.window.data2);
    break;
```

## Per-Frame Rendering

### Frame Flow

The rendering pipeline consists of two sequential passes:

1. **Main 3D Scene Rendering** (using dynamic rendering)
2. **ImGui Overlay Rendering** (using traditional render pass)

### ImGui Frame Setup

```cpp
void Application::RenderImGui() {
    // Start the ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Define UI here...
}
```

**Call Order is Critical**:
1. `ImGui_ImplVulkan_NewFrame()` - Prepares Vulkan backend for new frame
2. `ImGui_ImplSDL3_NewFrame()` - Updates input state from SDL events
3. `ImGui::NewFrame()` - Begins ImGui frame recording

### UI Definition

MetaGFX provides a control panel with model selection and rendering parameters:

```cpp
ImGui::Begin("MetaGFX Controls");

// Model selection combo box
const char* modelNames[] = {
    "Damaged Helmet (glTF)",
    "Metal Rough Spheres (glTF)",
    "Bunny with Textures (OBJ)",
    "Stanford Bunny (OBJ)"
};

if (ImGui::Combo("Model", &m_CurrentModelIndex, modelNames, IM_ARRAYSIZE(modelNames))) {
    LoadModel(m_AvailableModels[m_CurrentModelIndex]);
}

// Exposure slider
if (ImGui::SliderFloat("Exposure", &m_Exposure, 0.1f, 5.0f)) {
    METAGFX_INFO << "Exposure changed to: " << m_Exposure;
}

// Demo window toggle
ImGui::Checkbox("Show ImGui Demo", &m_ShowDemoWindow);

ImGui::End();

// Show demo window if enabled
if (m_ShowDemoWindow) {
    ImGui::ShowDemoWindow(&m_ShowDemoWindow);
}
```

**Widget Types Used**:
- `ImGui::Begin()/End()` - Create window
- `ImGui::Text()` - Display text
- `ImGui::Separator()` - Horizontal line
- `ImGui::Combo()` - Dropdown selection
- `ImGui::SliderFloat()` - Floating-point slider
- `ImGui::Checkbox()` - Boolean toggle
- `ImGui::Spacing()` - Vertical spacing

### Rendering the UI

After defining UI elements, convert to draw commands and render:

```cpp
// In Render() after main scene rendering
RenderImGui();
ImGui::Render();
ImDrawData* drawData = ImGui::GetDrawData();

if (drawData && drawData->TotalVtxCount > 0) {
    // Begin ImGui render pass
    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = m_ImGuiRenderPass;
    renderPassBeginInfo.framebuffer = m_ImGuiFramebuffers[imageIndex];
    renderPassBeginInfo.renderArea.offset = { 0, 0 };
    renderPassBeginInfo.renderArea.extent = { width, height };
    renderPassBeginInfo.clearValueCount = 0;  // Don't clear - load existing content

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
    vkCmdEndRenderPass(commandBuffer);
}
```

**Key Points**:
- Check `drawData->TotalVtxCount > 0` to skip rendering when UI is empty
- `clearValueCount = 0` ensures the 3D scene isn't overwritten
- `VK_SUBPASS_CONTENTS_INLINE` means commands are recorded directly (no secondary command buffers)

## Event Handling

SDL events are forwarded to ImGui before application processing:

```cpp
void Application::ProcessEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Let ImGui process the event first
        ImGui_ImplSDL3_ProcessEvent(&event);

        // Then handle application events
        switch (event.type) {
            // ...
        }
    }
}
```

This allows ImGui to:
- Capture mouse clicks on UI elements
- Handle keyboard input in text fields
- Block events from reaching the application when interacting with UI

**Checking if ImGui Wants Input**:
```cpp
ImGuiIO& io = ImGui::GetIO();
if (io.WantCaptureMouse) {
    // ImGui is using the mouse, don't process camera controls
}
if (io.WantCaptureKeyboard) {
    // ImGui is using the keyboard, don't process shortcuts
}
```

## Shutdown and Cleanup

Proper shutdown order is critical to avoid Vulkan validation errors:

```cpp
void Application::ShutdownImGui() {
    // 1. Shutdown ImGui backends (also destroys ImGui resources on GPU)
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    // 2. Destroy framebuffers
    for (auto framebuffer : m_ImGuiFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
    }
    m_ImGuiFramebuffers.clear();

    // 3. Destroy render pass
    if (m_ImGuiRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_ImGuiRenderPass, nullptr);
    }

    // 4. Destroy descriptor pool
    if (m_ImGuiDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_ImGuiDescriptorPool, nullptr);
    }
}
```

**Order Matters**:
- ImGui backends must shut down before destroying Vulkan resources they reference
- Framebuffers must be destroyed before the render pass
- Descriptor pool can be destroyed last (automatically frees all allocated sets)

## Common Patterns

### Adding a New Control

To add a new UI control:

1. **Add member variable** to store state ([Application.h:112-113](../src/app/Application.h#L112-L113)):
   ```cpp
   float m_MyParameter = 1.0f;
   ```

2. **Add widget** in `RenderImGui()`:
   ```cpp
   if (ImGui::SliderFloat("My Parameter", &m_MyParameter, 0.0f, 10.0f)) {
       // Optional: React to change
       METAGFX_INFO << "Parameter changed to: " << m_MyParameter;
   }
   ```

3. **Use the parameter** in rendering code (e.g., pass via push constants, uniform buffers)

### Creating a New Window

```cpp
ImGui::Begin("My Custom Window", &m_ShowMyWindow);  // &m_ShowMyWindow adds close button

ImGui::Text("Window content here");

if (ImGui::Button("Action Button")) {
    // Handle button click
}

ImGui::End();
```

### Conditional Rendering

```cpp
if (m_Model) {
    ImGui::Text("Model: %s", m_Model->GetName().c_str());
    ImGui::Text("Meshes: %d", m_Model->GetMeshCount());
} else {
    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No model loaded");
}
```

### Layout and Styling

```cpp
// Columns
ImGui::Columns(2, "split");
ImGui::Text("Left column");
ImGui::NextColumn();
ImGui::Text("Right column");
ImGui::Columns(1);  // Reset to single column

// Colored text
ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Warning");

// Tooltips
ImGui::Text("Hover me");
if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("This is a tooltip");
}

// Tree nodes (collapsible sections)
if (ImGui::TreeNode("Advanced Settings")) {
    ImGui::SliderFloat("Detail", &m_Detail, 0.0f, 1.0f);
    ImGui::TreePop();
}
```

## Integration with Rendering

### Passing UI Parameters to Shaders

Parameters from ImGui (like `m_Exposure`) are passed to shaders via push constants:

**Application.cpp**:
```cpp
float exposure = m_Exposure;  // Value from ImGui slider
vkCmd->PushConstants(pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
                     20, sizeof(float), &exposure);
```

**Shader** ([model.frag:38-43](../src/app/model.frag#L38-L43)):
```glsl
layout(push_constant) uniform PushConstants {
    vec4 cameraPosition;
    uint materialFlags;
    float exposure;  // Controlled by ImGui
    uint padding[2];
} pushConstants;

// Later in shader:
color = color * pushConstants.exposure;
```

### Reacting to UI Changes

Some UI changes require immediate action:

```cpp
if (ImGui::Combo("Model", &m_CurrentModelIndex, modelNames, count)) {
    // Triggers model loading with deferred deletion
    LoadModel(m_AvailableModels[m_CurrentModelIndex]);
}

if (ImGui::Checkbox("Enable Wireframe", &m_Wireframe)) {
    // Recreate pipeline with different rasterization state
    CreatePipeline();
}
```

## Performance Considerations

### Minimizing Overhead

- **Check vertex count** before rendering: `if (drawData->TotalVtxCount > 0)`
- **Reuse framebuffers** across frames
- **Separate descriptor pool** prevents fragmentation of main rendering pool
- **Single render pass** for all UI elements

### Validation Layers

Enable Vulkan validation layers during development to catch:
- Framebuffer/render pass mismatches
- Descriptor pool exhaustion
- Image layout transitions
- Synchronization issues

### Profiling

ImGui provides built-in metrics:

```cpp
ImGui::Begin("Performance");
ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
            1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);
ImGui::End();
```

## Troubleshooting

### Common Issues

**1. Black screen after adding ImGui**
- Check that `loadOp = VK_ATTACHMENT_LOAD_OP_LOAD` (not CLEAR)
- Verify `initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`

**2. Validation errors about image layouts**
- Ensure main render pass ends with `PRESENT_SRC_KHR` layout
- ImGui render pass must start and end with same layout

**3. Framebuffer creation fails**
- Check image view validity
- Verify swap chain dimensions match framebuffer dimensions
- Ensure framebuffers are recreated after window resize

**4. UI elements don't respond to input**
- Verify `ImGui_ImplSDL3_ProcessEvent()` is called before application event handling
- Check that SDL event forwarding is working

**5. Crash on window resize**
- Destroy ImGui framebuffers before swap chain resize
- Recreate framebuffers lazily on next render

## Future Enhancements

Potential improvements to the ImGui integration:

1. **Docking and Multi-Viewport**: Enable `ImGuiConfigFlags_DockingEnable` and `ImGuiConfigFlags_ViewportsEnable`
2. **Custom Fonts**: Load custom TTF fonts for better aesthetics
3. **Themes**: Create custom color schemes matching application branding
4. **Render Stats**: Display detailed GPU timings, memory usage, draw call counts
5. **Scene Graph Inspector**: Hierarchical view of scene objects
6. **Material Editor**: Visual material property editing with live preview
7. **Console Log**: Integrated log viewer for debug messages
8. **Profiler Integration**: Real-time performance graphs

## Resources

- [Dear ImGui GitHub](https://github.com/ocornut/imgui)
- [ImGui Demo](https://github.com/ocornut/imgui/blob/master/imgui_demo.cpp) - Comprehensive widget showcase
- [Vulkan Backend Documentation](https://github.com/ocornut/imgui/blob/master/backends/imgui_impl_vulkan.h)
- [SDL3 Backend Documentation](https://github.com/ocornut/imgui/blob/master/backends/imgui_impl_sdl3.h)
- [ImGui Wiki](https://github.com/ocornut/imgui/wiki)
