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
#include <string>
#include <vector>

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static GLuint createTexture(int size, const void* data)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    return tex;
}

static void updateTexture(GLuint tex, int size, const void* data)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

static void setPerspective(float fovY, float aspect, float zNear, float zFar)
{
    float rad = fovY * 3.14159265358979323846f / 180.0f;
    float tang = tanf(rad / 2.0f);
    float h = zNear * tang, w = h * aspect;
    glFrustum(-w, w, -h, h, zNear, zFar);
}

static void drawTexturedSphere(GLuint tex, float radius, int lonSeg, int latSeg,
                                float yaw, float pitch)
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor3f(1, 1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -3);
    glRotatef(pitch, 1, 0, 0);
    glRotatef(yaw,   0, 1, 0);

    constexpr float PI = 3.14159265358979323846f;
    for (int i = 0; i < latSeg; ++i)
    {
        float lat0 = PI * (-0.5f + float(i)   / latSeg);
        float lat1 = PI * (-0.5f + float(i+1) / latSeg);
        float y0 = std::sin(lat0), y1 = std::sin(lat1);
        float r0 = std::cos(lat0), r1 = std::cos(lat1);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= lonSeg; ++j)
        {
            float lon = 2.f * PI * float(j) / lonSeg;
            float x = std::cos(lon), z = std::sin(lon);
            float s = float(j) / lonSeg;
            glTexCoord2f(s, float(i)   / latSeg); glVertex3f(radius*r0*x, radius*y0, radius*r0*z);
            glTexCoord2f(s, float(i+1) / latSeg); glVertex3f(radius*r1*x, radius*y1, radius*r1*z);
        }
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
}

static void drawFlatMap(GLuint tex, float aspect)
{
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor3f(1, 1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float w = 1.6f, h = w / aspect;
    glBegin(GL_QUADS);
    glTexCoord2f(0,1); glVertex3f(-w,-h,-3);
    glTexCoord2f(1,1); glVertex3f( w,-h,-3);
    glTexCoord2f(1,0); glVertex3f( w, h,-3);
    glTexCoord2f(0,0); glVertex3f(-w, h,-3);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

// ── Per-snapshot GPU textures ─────────────────────────────────────────────────

constexpr int LC = static_cast<int>(MapLayer::Count);

struct SnapTextures
{
    GLuint globe   [LC] = {};
    GLuint mercator[LC] = {};
};

static void uploadSnapshot(SnapTextures& st, const MapSnapshot& snap, int size)
{
    for (int L = 0; L < LC; ++L)
    {
        if (st.globe[L] == 0)
        {
            st.globe   [L] = createTexture(size, snap.layerPixels   [L].data());
            st.mercator[L] = createTexture(size, snap.layerMercator [L].data());
        }
        else
        {
            updateTexture(st.globe   [L], size, snap.layerPixels   [L].data());
            updateTexture(st.mercator[L], size, snap.layerMercator [L].data());
        }
    }
}

static void deleteSnapTextures(SnapTextures& st)
{
    for (int L = 0; L < LC; ++L)
    {
        if (st.globe   [L]) { glDeleteTextures(1, &st.globe   [L]); st.globe   [L]=0; }
        if (st.mercator[L]) { glDeleteTextures(1, &st.mercator[L]); st.mercator[L]=0; }
    }
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

    // Upload all snapshot textures
    std::vector<SnapTextures> snapTextures(map.snapshots.size());
    for (int s = 0; s < int(map.snapshots.size()); ++s)
        uploadSnapshot(snapTextures[s], map.snapshots[s], map.size);

    int activeLayer    = static_cast<int>(MapLayer::TectonicPlates);
    int activeSnapshot = int(map.snapshots.size()) - 1;  // default = final

    enum ViewMode { GlobeView = 0, Map2DView = 1 };
    ViewMode viewMode = GlobeView;
    float globeYaw = 0.f, globePitch = 15.f;
    bool mouseDragging = false;
    double lastMouseX = 0, lastMouseY = 0;
    bool rotateGlobe = true;
    float rotationSpeed = 18.f;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        double now = glfwGetTime();
        float dt = float(now - lastTime);
        lastTime = now;

        ImGuiIO& io = ImGui::GetIO();
        double mx = 0, my = 0;
        glfwGetCursorPos(window, &mx, &my);
        int lb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);

        if (lb == GLFW_PRESS && !io.WantCaptureMouse && viewMode == GlobeView)
        {
            if (!mouseDragging) { mouseDragging=true; lastMouseX=mx; lastMouseY=my; }
            else
            {
                globeYaw   += float(mx-lastMouseX)*0.4f;
                globePitch  = std::clamp(globePitch+float(my-lastMouseY)*0.3f,-89.f,89.f);
                lastMouseX=mx; lastMouseY=my;
            }
            rotateGlobe = false;
        }
        else
        {
            mouseDragging = false;
            if (rotateGlobe && viewMode == GlobeView)
                globeYaw = std::fmod(globeYaw + rotationSpeed*dt, 360.f);
        }

        glfwPollEvents();
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int dw, dh;
        glfwGetFramebufferSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.10f, 0.11f, 0.13f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = (dw>0&&dh>0) ? float(dw)/float(dh) : 1.f;
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        // Clamp activeSnapshot in case map was just regenerated with fewer snapshots
        activeSnapshot = std::clamp(activeSnapshot, 0, int(snapTextures.size())-1);
        const SnapTextures& st = snapTextures[activeSnapshot];

        if (viewMode == GlobeView)
        {
            setPerspective(45.f, aspect, 0.1f, 10.f);
            drawTexturedSphere(st.globe[activeLayer], 1.f, 64, 32, globeYaw, globePitch);
        }
        else
        {
            glOrtho(-aspect, aspect, -1.f, 1.f, 0.1f, 10.f);
            drawFlatMap(st.mercator[activeLayer], aspect);
        }

        // ── UI panel ─────────────────────────────────────────────────────────
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Always);
        ImGui::Begin("Map Controls", nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

        ImGui::TextWrapped("Generate tectonic plates and choose the display mode.");

        ImGui::RadioButton("Globe",   reinterpret_cast<int*>(&viewMode), GlobeView);
        ImGui::SameLine();
        ImGui::RadioButton("2-D Map", reinterpret_cast<int*>(&viewMode), Map2DView);

        ImGui::Separator();

        // ── Layer selector ────────────────────────────────────────────────────
        ImGui::Text("Map Layer:");
        ImGui::RadioButton("Tectonic Plates",   &activeLayer, int(MapLayer::TectonicPlates));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Each plate in its own colour.\nFractal boundaries visible as colour transitions.");
        ImGui::RadioButton("Boundary Types",    &activeLayer, int(MapLayer::BoundaryTypes));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Plate colours + coloured fault lines:\nRed=Convergent  Green=Divergent  Gold=Transform");
        ImGui::RadioButton("Collision Effects", &activeLayer, int(MapLayer::CollisionEffects));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Gradient terrain halos:\nMountain belts, rift valleys, transform fault scars.");
        ImGui::RadioButton("Elevation Heatmap", &activeLayer, int(MapLayer::Elevation));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Topographic heatmap (with erosion applied).\nNavy=deep ocean  Teal=coast  Green=lowland\nBrown=highland  White=peak");
        ImGui::RadioButton("Climate / Wind",    &activeLayer, int(MapLayer::Climate));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Moisture & biome map with wind arrows.\nSandy=desert  Yellow-green=savanna  Green=temperate\nDark green=rainforest  Blue=ocean\nWhite arrows show prevailing wind direction.\nErosion from rainfall softens mountain peaks.");
        ImGui::RadioButton("Hydrology",         &activeLayer, int(MapLayer::Hydrology));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Rivers, lakes & drainage basins.\nBlue lines = rivers (width proportional to flow)\nBlue-grey fill = lakes / endorheic basins\nGreen tint near rivers = riparian vegetation\nRivers dry up in arid regions (evaporation)");

        ImGui::Separator();

        // ── Snapshot timeline ─────────────────────────────────────────────────
        {
            int snapCount = int(map.snapshots.size());
            ImGui::Text("Timeline  (%d snapshots):", snapCount);

            // Slider — 0 = earliest, snapCount-1 = final
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderInt("##snap", &activeSnapshot, 0, snapCount-1))
                activeSnapshot = std::clamp(activeSnapshot, 0, snapCount-1);

            // Label showing elapsed time and plate count for the active snapshot
            const MapSnapshot& cur = map.snapshots[activeSnapshot];
            bool isFinal = (activeSnapshot == snapCount-1);
            ImGui::Text("  t = %.2f s   |   plates = %d%s",
                        cur.simulationTime, cur.plateCount,
                        isFinal ? "  [final]" : "");

            // Quick-jump buttons: one per snapshot
            ImGui::Text("Jump to:");
            for (int s = 0; s < snapCount; ++s)
            {
                if (s > 0) ImGui::SameLine();
                char lbl[16];
                std::snprintf(lbl, sizeof(lbl), "%d", s+1);
                bool active = (s == activeSnapshot);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f,0.6f,0.9f,1.f));
                if (ImGui::SmallButton(lbl)) activeSnapshot = s;
                if (active) ImGui::PopStyleColor();
            }
        }

        ImGui::Separator();

        // ── Generation settings ───────────────────────────────────────────────
        ImGui::SliderInt("Map Size",          &settings.mapSize,          128, 2048);
        ImGui::SliderInt("Plate Count",       &settings.plateCount,         2,   64);
        ImGui::SliderInt("Fault Thickness",   &settings.faultIntensity,     1,    6);
        ImGui::SliderFloat("Sim Time",        &settings.simulationTime,   0.f, 120.f, "%.1f s");
        ImGui::SliderInt("Sim Steps",         &settings.simulationSteps,    1,   32);
        ImGui::SliderInt("Snapshots",         &settings.snapshotCount,      1,   20);
        ImGui::SliderFloat("Boundary Roughness", &settings.boundaryRoughness, 0.f, 1.f, "%.2f");
        ImGui::SliderFloat("Angular Velocity",   &settings.angularVelocity,  0.1f, 3.f, "%.2f");
        ImGui::Checkbox("Plate Fragmentation", &settings.fragmentation);
        ImGui::InputScalar("Seed", ImGuiDataType_U32, &settings.seed);

        if (ImGui::Button("Generate", ImVec2(-1, 0)))
        {
            map = generateMap(settings);

            // Delete old textures
            for (auto& st2 : snapTextures) deleteSnapTextures(st2);
            snapTextures.clear();

            // Upload new ones
            snapTextures.resize(map.snapshots.size());
            for (int s = 0; s < int(map.snapshots.size()); ++s)
                uploadSnapshot(snapTextures[s], map.snapshots[s], map.size);

            // Jump to final snapshot
            activeSnapshot = int(map.snapshots.size()) - 1;
        }

        ImGui::Separator();
        ImGui::Checkbox("Auto Rotate", &rotateGlobe);
        ImGui::SliderFloat("Rotation Speed", &rotationSpeed, 0.f, 90.f, "%.1f deg/s");

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

    for (auto& st2 : snapTextures) deleteSnapTextures(st2);
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
