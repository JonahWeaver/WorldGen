#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>

#include "MapGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static GLuint createTexture(int size, const void* data)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return texture;
}

static void updateTexture(GLuint texture, int size, const void* data)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

static void setPerspective(float fovY, float aspect, float zNear, float zFar)
{
    float rad = fovY * 3.14159265358979323846f / 180.0f;
    float tangent = tanf(rad / 2.0f);
    float height = zNear * tangent;
    float width = height * aspect;
    glFrustum(-width, width, -height, height, zNear, zFar);
}

static void drawTexturedSphere(GLuint texture, float radius, int lonSegments, int latSegments, float yawDegrees, float pitchDegrees)
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor3f(1.0f, 1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -3.0f);
    glRotatef(pitchDegrees, 1.0f, 0.0f, 0.0f);
    glRotatef(yawDegrees, 0.0f, 1.0f, 0.0f);

    constexpr float PI = 3.14159265358979323846f;
    for (int i = 0; i < latSegments; ++i)
    {
        float lat0 = PI * (-0.5f + static_cast<float>(i) / latSegments);
        float lat1 = PI * (-0.5f + static_cast<float>(i + 1) / latSegments);
        float y0 = std::sin(lat0), y1 = std::sin(lat1);
        float r0 = std::cos(lat0), r1 = std::cos(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= lonSegments; ++j)
        {
            float lon = 2.0f * PI * static_cast<float>(j) / lonSegments;
            float x = std::cos(lon), z = std::sin(lon);
            float s = static_cast<float>(j) / lonSegments;
            glTexCoord2f(s, static_cast<float>(i) / latSegments);
            glVertex3f(radius * r0 * x, radius * y0, radius * r0 * z);
            glTexCoord2f(s, static_cast<float>(i + 1) / latSegments);
            glVertex3f(radius * r1 * x, radius * y1, radius * r1 * z);
        }
        glEnd();
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
}

static void drawFlatMap(GLuint texture, float aspect)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor3f(1.0f, 1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float width = 1.6f;
    float height = width / aspect;
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-width, -height, -3.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f( width, -height, -3.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f( width,  height, -3.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-width,  height, -3.0f);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

int main()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 760, "WorldGen Fantasy Map", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();

    GeneratorSettings settings;
    MapResult map = generateMap(settings);

    // One globe texture + one mercator texture per layer
    constexpr int LayerCount = static_cast<int>(MapLayer::Count);
    GLuint globeTextures   [LayerCount] = {};
    GLuint mercatorTextures[LayerCount] = {};
    for (int L = 0; L < LayerCount; ++L)
    {
        globeTextures   [L] = createTexture(map.size, map.layerPixels   [L].data());
        mercatorTextures[L] = createTexture(map.size, map.layerMercator [L].data());
    }

    // Active layer (default = TectonicPlates)
    int activeLayer = static_cast<int>(MapLayer::TectonicPlates);

    enum ViewMode { GlobeView = 0, Map2DView = 1 };
    ViewMode viewMode = GlobeView;
    float globeYaw = 0.0f, globePitch = 15.0f;
    bool mouseDragging = false;
    double lastMouseX = 0.0, lastMouseY = 0.0;
    bool rotateGlobe = true;
    float rotationSpeed = 18.0f;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        ImGuiIO& io = ImGui::GetIO();
        double mouseX = 0.0, mouseY = 0.0;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        int leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);

        if (leftDown == GLFW_PRESS && !io.WantCaptureMouse && viewMode == GlobeView)
        {
            if (!mouseDragging)
            {
                mouseDragging = true;
                lastMouseX = mouseX; lastMouseY = mouseY;
            }
            else
            {
                float dx = static_cast<float>(mouseX - lastMouseX);
                float dy = static_cast<float>(mouseY - lastMouseY);
                globeYaw   += dx * 0.4f;
                globePitch  = std::clamp(globePitch + dy * 0.3f, -89.0f, 89.0f);
                lastMouseX = mouseX; lastMouseY = mouseY;
            }
            rotateGlobe = false;
        }
        else
        {
            mouseDragging = false;
            if (rotateGlobe && viewMode == GlobeView)
                globeYaw = std::fmod(globeYaw + rotationSpeed * deltaTime, 360.0f);
        }

        glfwPollEvents();
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.10f, 0.11f, 0.13f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = (displayW > 0 && displayH > 0)
                     ? static_cast<float>(displayW) / static_cast<float>(displayH)
                     : 1.0f;

        if (viewMode == GlobeView)
        {
            setPerspective(45.0f, aspect, 0.1f, 10.0f);
            drawTexturedSphere(globeTextures[activeLayer], 1.0f, 64, 32, globeYaw, globePitch);
        }
        else
        {
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(-aspect, aspect, -1.0f, 1.0f, 0.1f, 10.0f);
            drawFlatMap(mercatorTextures[activeLayer], aspect);
        }

        // ── UI panel ─────────────────────────────────────────────────────────
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(355, 0), ImGuiCond_Always);
        ImGui::Begin("Map Controls", nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

        ImGui::TextWrapped("Generate tectonic plates and choose the main display mode.");

        // View mode (Globe / 2-D Map)
        ImGui::RadioButton("Globe",  reinterpret_cast<int*>(&viewMode), GlobeView);
        ImGui::SameLine();
        ImGui::RadioButton("2-D Map", reinterpret_cast<int*>(&viewMode), Map2DView);

        ImGui::Separator();

        // Layer selector
        ImGui::Text("Map Layer:");
        ImGui::RadioButton("Tectonic Plates",   &activeLayer, static_cast<int>(MapLayer::TectonicPlates));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Each plate shown in its own colour.\nFractal boundaries visible as colour transitions.");
        ImGui::RadioButton("Boundary Types",    &activeLayer, static_cast<int>(MapLayer::BoundaryTypes));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Plate colours + coloured fault lines:\nRed = Convergent  |  Green = Divergent  |  Gold = Transform");
        ImGui::RadioButton("Collision Effects", &activeLayer, static_cast<int>(MapLayer::CollisionEffects));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Gradient terrain halos along boundaries:\nMountain belts, rift valleys, transform fault scars.");

        ImGui::Separator();

        // Generation settings
        ImGui::SliderInt("Map Size",          &settings.mapSize,          128, 2048);
        ImGui::SliderInt("Plate Count",       &settings.plateCount,         2,   64);
        ImGui::SliderInt("Fault Thickness",   &settings.faultIntensity,     1,    6);
        ImGui::SliderFloat("Sim Time",        &settings.simulationTime,   0.0f, 120.0f, "%.1f s");
        ImGui::SliderInt("Sim Steps",         &settings.simulationSteps,    1,   16);
        ImGui::SliderFloat("Boundary Roughness", &settings.boundaryRoughness, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Angular Velocity",   &settings.angularVelocity,  0.1f, 3.0f, "%.2f");
        ImGui::Checkbox("Plate Fragmentation", &settings.fragmentation);
        ImGui::InputScalar("Seed", ImGuiDataType_U32, &settings.seed);

        if (ImGui::Button("Generate", ImVec2(-1, 0)))
        {
            map = generateMap(settings);
            for (int L = 0; L < LayerCount; ++L)
            {
                updateTexture(globeTextures   [L], map.size, map.layerPixels   [L].data());
                updateTexture(mercatorTextures [L], map.size, map.layerMercator [L].data());
            }
        }

        ImGui::Separator();
        ImGui::Checkbox("Auto Rotate", &rotateGlobe);
        ImGui::SliderFloat("Rotation Speed", &rotationSpeed, 0.0f, 90.0f, "%.1f deg/s");

        ImGui::Separator();
        ImGui::Text("Requested plates : %d", settings.plateCount);
        ImGui::Text("Final plates     : %d", map.finalPlateCount);
        ImGui::Text("Size             : %dx%d", map.size, map.size);
        ImGui::Text("Seed             : %u", settings.seed);
        ImGui::Text("Convergent       : %d", map.convergentCount);
        ImGui::Text("Transform        : %d", map.transformCount);
        ImGui::Text("Divergent        : %d", map.divergentCount);

        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    for (int L = 0; L < LayerCount; ++L)
    {
        glDeleteTextures(1, &globeTextures   [L]);
        glDeleteTextures(1, &mercatorTextures[L]);
    }
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
