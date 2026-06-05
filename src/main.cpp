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
    bool showPanel = true;   // H key or button toggles the panel

    // ── Civilization timeline textures ────────────────────────────────────────
    // Each CivSnapshot gets two textures (globe + mercator).
    struct CivTex { GLuint globe=0, mercator=0; };
    std::vector<CivTex> civTextures;
    auto uploadCivSnaps = [&](){
        for(auto& ct : civTextures){
            if(ct.globe)    glDeleteTextures(1,&ct.globe);
            if(ct.mercator) glDeleteTextures(1,&ct.mercator);
        }
        civTextures.clear();
        civTextures.resize(map.civSnaps.size());
        for(int s=0;s<int(map.civSnaps.size());++s){
            civTextures[s].globe    = createTexture(map.size, map.civSnaps[s].pixels.data());
            civTextures[s].mercator = createTexture(map.size, map.civSnaps[s].mercator.data());
        }
    };
    uploadCivSnaps();

    bool showCivLayer   = false;  // toggle between geology layers and civ view
    int  activeCivSnap  = int(map.civSnaps.size())-1;

    // ── Resource layer textures ───────────────────────────────────────────────
    GLuint resourceGlobeTex = 0, resourceMercTex = 0;
    bool   showResourceLayer = false;
    auto uploadResourceTextures = [&](){
        if(resourceGlobeTex) glDeleteTextures(1,&resourceGlobeTex);
        if(resourceMercTex)  glDeleteTextures(1,&resourceMercTex);
        resourceGlobeTex = resourceMercTex = 0;
        if(!map.resourcePixels.empty())
            resourceGlobeTex = createTexture(map.size, map.resourcePixels.data());
        if(!map.resourceMercator.empty())
            resourceMercTex  = createTexture(map.size, map.resourceMercator.data());
    };
    uploadResourceTextures();

    // ── Event log window ──────────────────────────────────────────────────────
    bool showEventLog = false;

    // ── Civ click-to-inspect ──────────────────────────────────────────────────
    int  inspectCountryId = -1;   // -1 = nothing selected
    bool showCivPopup     = false;

    // Debug: track last computed map UV and cell
    float dbgU = -1.f, dbgV = -1.f;
    int   dbgMapX = -1, dbgMapY = -1, dbgCountryId = -2;
    bool  showCivDebug = false;

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

        // Clamp indices
        activeSnapshot = std::clamp(activeSnapshot, 0, int(snapTextures.size())-1);
        {
            int civMax = civTextures.empty() ? 0 : int(civTextures.size())-1;
            activeCivSnap = std::clamp(activeCivSnap, 0, civMax);
        }

        // Choose which texture to display
        GLuint displayGlobe = 0, displayMerc = 0;
        if (showResourceLayer && resourceGlobeTex)
        {
            displayGlobe = resourceGlobeTex;
            displayMerc  = resourceMercTex;
        }
        else if (showCivLayer && !civTextures.empty())
        {
            displayGlobe = civTextures[activeCivSnap].globe;
            displayMerc  = civTextures[activeCivSnap].mercator;
        }
        else
        {
            const SnapTextures& st = snapTextures[activeSnapshot];
            displayGlobe = st.globe[activeLayer];
            displayMerc  = st.mercator[activeLayer];
        }

        if (viewMode == GlobeView)
        {
            setPerspective(45.f, aspect, 0.1f, 10.f);
            drawTexturedSphere(displayGlobe, 1.f, 64, 32, globeYaw, globePitch);
        }
        else
        {
            glOrtho(-aspect, aspect, -1.f, 1.f, 0.1f, 10.f);
            drawFlatMap(displayMerc, aspect);
        }

        // ── Civ click-to-inspect + debug overlay (2D map only) ───────────────
        if (viewMode == Map2DView)
        {
            // Shared coordinate transform (used for both debug and click)
            float mapW = 1.6f;
            float mapH = mapW / aspect;
            float ndcX = (float(mx) / float(dw)) * 2.f * aspect - aspect;
            float ndcY = 1.f - (float(my) / float(dh)) * 2.f;
            float u = (ndcX + mapW) / (2.f * mapW);
            float v = 1.f - (ndcY + mapH) / (2.f * mapH);

            constexpr float PI2 = 3.14159265358979323846f;
            float mercY = std::clamp(v, 0.001f, 0.999f);
            float lat   = std::atan(std::sinh(PI2 * (1.f - 2.f * mercY)));
            float eqV   = std::clamp(0.5f - lat / PI2, 0.f, 1.f);

            int liveMapX = std::clamp(int(u   * map.size), 0, map.size - 1);
            int liveMapY = std::clamp(int(eqV * map.size), 0, map.size - 1);
            bool onMap   = (u >= 0.f && u <= 1.f && v >= 0.f && v <= 1.f);

            // ── Debug crosshair drawn via ImGui DrawList ──────────────────────
            if (showCivDebug && onMap)
            {
                // Convert the computed map UV back to screen pixels for the crosshair
                // (this shows exactly where the transform thinks the cursor is)
                float crossU = u;
                float crossEqV = eqV;
                // eqV → mercator v (forward Mercator)
                float crossMercV = (1.f - std::log(std::tan(PI2*(0.5f - crossEqV) + PI2/4.f)) / PI2) * 0.5f;
                float crossNdcX  = crossU * 2.f * mapW - mapW;
                float crossNdcY  = 1.f - (1.f - crossMercV) * 2.f * mapH;
                // NDC → screen pixels
                float crossSX = (crossNdcX + aspect) / (2.f * aspect) * float(dw);
                float crossSY = (1.f - crossNdcY) * 0.5f * float(dh);

                ImDrawList* dl = ImGui::GetBackgroundDrawList();
                float cs = 12.f;
                dl->AddLine(ImVec2(crossSX-cs, crossSY), ImVec2(crossSX+cs, crossSY),
                            IM_COL32(255,50,50,220), 2.f);
                dl->AddLine(ImVec2(crossSX, crossSY-cs), ImVec2(crossSX, crossSY+cs),
                            IM_COL32(255,50,50,220), 2.f);
                dl->AddCircle(ImVec2(crossSX, crossSY), 6.f,
                              IM_COL32(255,200,50,220), 12, 1.5f);
            }

            // ── Debug info window ─────────────────────────────────────────────
            if (showCivDebug)
            {
                ImGui::SetNextWindowPos(ImVec2(float(dw)-260.f, 10.f), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_Always);
                ImGui::SetNextWindowBgAlpha(0.85f);
                ImGui::Begin("##civdbg", nullptr,
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
                ImGui::TextColored(ImVec4(1,0.8f,0.2f,1), "Civ Click Debug");
                ImGui::Separator();
                ImGui::Text("Screen  : (%.0f, %.0f)", float(mx), float(my));
                ImGui::Text("NDC     : (%.3f, %.3f)", ndcX, ndcY);
                ImGui::Text("UV      : (%.3f, %.3f)", u, v);
                ImGui::Text("On map  : %s", onMap ? "YES" : "NO");
                ImGui::Text("Lat     : %.2f deg", lat * 180.f / PI2);
                ImGui::Text("EqV     : %.3f", eqV);
                ImGui::Text("MapXY   : (%d, %d)", liveMapX, liveMapY);
                if (onMap && !map.civCells.empty())
                {
                    int idx = liveMapY * map.size + liveMapX;
                    const CivCell& lc = map.civCells[idx];
                    ImGui::Text("Settled : %s", lc.settled ? "YES" : "NO");
                    ImGui::Text("Country : %d", lc.countryId);
                    ImGui::Text("Pop     : %.2f", lc.population);
                    ImGui::Text("Hab     : %.2f", lc.habitability);
                }
                ImGui::Text("Last click country: %d", dbgCountryId);
                ImGui::End();
            }

            // ── Click handler ─────────────────────────────────────────────────
            if (showCivLayer && !map.civCells.empty() &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse)
            {
                dbgU = u; dbgV = v; dbgMapX = liveMapX; dbgMapY = liveMapY;

                if (onMap)
                {
                    int cellIdx = liveMapY * map.size + liveMapX;
                    const CivCell& cell = map.civCells[cellIdx];
                    dbgCountryId = cell.countryId;
                    if (cell.settled && cell.countryId >= 0)
                    {
                        inspectCountryId = cell.countryId;
                        showCivPopup = true;
                    }
                    else
                    {
                        inspectCountryId = -1;
                        showCivPopup = false;
                    }
                }
            }
        }

        // ── Civ info popup ────────────────────────────────────────────────────
        if (showCivPopup && inspectCountryId >= 0)
        {
            // Find the CivInfo for this country
            const CivInfo* info = nullptr;
            for (const auto& ci2 : map.civInfos)
                if (ci2.countryId == inspectCountryId) { info = &ci2; break; }

            if (info)
            {
                ImGui::SetNextWindowPos(ImVec2(float(dw)*0.5f, float(dh)*0.5f),
                                        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(320, 0), ImGuiCond_Appearing);
                ImGui::Begin("Civilization Info", &showCivPopup,
                             ImGuiWindowFlags_AlwaysAutoResize);

                // Colour swatch
                uint32_t col = info->colour;
                float cr2 = (col & 0xFF) / 255.f;
                float cg2 = ((col >> 8) & 0xFF) / 255.f;
                float cb2 = ((col >> 16) & 0xFF) / 255.f;
                ImGui::ColorButton("##civcolour", ImVec4(cr2, cg2, cb2, 1.f),
                                   ImGuiColorEditFlags_NoTooltip, ImVec2(18, 18));
                ImGui::SameLine();
                ImGui::Text("Country #%d  (Culture group %d)",
                            info->countryId, info->cultureId);

                ImGui::Separator();

                // Population & area
                ImGui::Text("Population   : %.1f", info->totalPop);
                ImGui::Text("Territory    : %d cells  (~%.1f%%)",
                            info->cellCount,
                            100.f * float(info->cellCount) / float(map.size * map.size));

                ImGui::Separator();

                // Settlements
                ImGui::Text("Cities       : %d  (pop > 5)", info->cityCount);
                ImGui::Text("Towns        : %d  (pop 2-5)", info->townCount);
                ImGui::Text("Villages     : %d  (pop 0.5-2)", info->villageCount);

                ImGui::Separator();

                // Averages
                ImGui::Text("Avg Fertility     "); ImGui::SameLine();
                ImGui::ProgressBar(info->avgFertility, ImVec2(-1, 0));
                ImGui::Text("Avg Cohesion      "); ImGui::SameLine();
                ImGui::ProgressBar(info->avgCohesion, ImVec2(-1, 0));
                ImGui::Text("Avg Culture       "); ImGui::SameLine();
                ImGui::ProgressBar(info->avgCulture, ImVec2(-1, 0));
                ImGui::Text("Avg Traversability"); ImGui::SameLine();
                ImGui::ProgressBar(info->avgTraversability, ImVec2(-1, 0));

                // Economy
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.f,0.85f,0.3f,1.f), "Economy");
                ImGui::Text("Income/tick  : %.2f", info->income);
                ImGui::Text("Science/tick : %.2f", info->scienceRate);
                ImGui::Text("Trade vol    : %.2f", info->tradeVolume);

                // Resources
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.6f,1.f,0.6f,1.f), "Resource Stockpiles");
                static const char* resNames[RESOURCE_COUNT] = {
                    "Iron","Wood","Niter","ManaStone","Gold","Silver","Copper"};
                for(int r=0;r<RESOURCE_COUNT;++r){
                    if(info->totalResources[r]>0.01f)
                        ImGui::Text("  %-10s : %.1f", resNames[r], info->totalResources[r]);
                }

                // Cultural attributes
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.8f,0.6f,1.f,1.f), "Cultural Attributes");
                static const char* attrNames[CULT_ATTR_COUNT] = {
                    "Militarism","Mercantilism","Piety","Scholarship","Isolationism"};
                for(int a=0;a<CULT_ATTR_COUNT;++a){
                    float v2 = std::clamp(info->culturalAttrs[a]/2.5f, 0.f, 1.f);
                    ImGui::Text("  %-14s", attrNames[a]); ImGui::SameLine();
                    ImGui::ProgressBar(v2, ImVec2(-1, 0));
                }

                // Technologies
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.4f,0.8f,1.f,1.f), "Technologies");
                int techsShown = 0;
                for(int t=0;t<TECH_COUNT;++t){
                    if(info->techs[t]){
                        if(techsShown>0 && techsShown%3!=0) ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.5f,1.f,0.5f,1.f), "[%s]", techName(TechId(t)));
                        ++techsShown;
                    }
                }
                if(techsShown==0) ImGui::TextDisabled("  (none yet)");

                // Military
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.f,0.4f,0.4f,1.f), "Military");
                ImGui::Text("Total Strength : %.1f", info->militaryStr);
                ImGui::Text("  Swordsmen    : %.1f  (Iron + tech)", info->swordsmenStr);
                ImGui::Text("  Gunmen       : %.1f  (Niter + Gunpowder)", info->gunmenStr);
                ImGui::Text("  Arcane       : %.1f  (ManaStone + Arcane)", info->arcaneStr);
                if(info->navyStr > 0.01f)
                    ImGui::Text("Navy Strength  : %.1f  (Copper + Navigation)", info->navyStr);
                else
                    ImGui::TextDisabled("Navy: none (needs Navigation tech)");

                // Great People
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.f,0.6f,1.f,1.f), "Great People");
                ImGui::Text("Total born: %d  (Arcane: %d)",
                            info->greatPersonTotal, info->arcanePersonTotal);
                if(info->greatPeople.empty()){
                    ImGui::TextDisabled("  (none active)");
                } else {
                    for(const GreatPerson& gp : info->greatPeople){
                        bool isArcane = isArcaneGreatPerson(gp.type);
                        ImVec4 gpCol = isArcane
                            ? ImVec4(1.f,0.5f,1.f,1.f)   // arcane: magenta
                            : ImVec4(1.f,0.8f,0.3f,1.f);  // military: gold
                        ImGui::TextColored(gpCol, "  [T%d] %s  (power %d%%)",
                            gp.birthTick,
                            greatPersonName(gp.type),
                            int(gp.power*100));
                    }
                }

                ImGui::Separator();
                if (ImGui::Button("Close", ImVec2(-1, 0))) showCivPopup = false;

                ImGui::End();
            }
            else
            {
                showCivPopup = false;
            }
        }

        // ── Event Log window ──────────────────────────────────────────────────
        if (showEventLog && !map.eventLog.empty())
        {
            ImGui::SetNextWindowPos(ImVec2(float(dw)-420.f, 40.f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(410, 500), ImGuiCond_FirstUseEver);
            ImGui::Begin("Event Log", &showEventLog);

            static const char* evTypeNames[] = {
                "Founded","Conquest","Secession","Merger","Tech","City","Collapse","GreatPerson"};
            static const ImVec4 evTypeColors[] = {
                ImVec4(0.5f,1.f,0.5f,1.f),   // Founded: green
                ImVec4(1.f,0.4f,0.4f,1.f),   // Conquest: red
                ImVec4(1.f,0.8f,0.2f,1.f),   // Secession: yellow
                ImVec4(0.4f,0.8f,1.f,1.f),   // Merger: blue
                ImVec4(0.9f,0.6f,1.f,1.f),   // Tech: purple
                ImVec4(1.f,1.f,0.5f,1.f),    // City: pale yellow
                ImVec4(0.6f,0.6f,0.6f,1.f),  // Collapse: grey
                ImVec4(1.f,0.5f,1.f,1.f),    // GreatPerson: bright magenta
            };

            // Filter controls
            static int filterType = -1; // -1 = all
            ImGui::Text("Filter:");
            ImGui::SameLine();
            if (ImGui::SmallButton("All")) filterType = -1;
            for (int t = 0; t < 8; ++t) {
                ImGui::SameLine();
                if (ImGui::SmallButton(evTypeNames[t])) filterType = t;
            }
            ImGui::Text("Total events: %d", int(map.eventLog.size()));
            ImGui::Separator();

            ImGui::BeginChild("##evlog", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false);
            // Show most recent events first
            for (int i = int(map.eventLog.size())-1; i >= 0; --i) {
                const CivEvent& ev = map.eventLog[i];
                int etype = int(ev.type);
                if (filterType >= 0 && etype != filterType) continue;
                ImGui::TextColored(evTypeColors[etype], "[T%4d] %-9s",
                                   ev.tick, evTypeNames[etype]);
                ImGui::SameLine();
                ImGui::TextUnformatted(ev.note.c_str());
            }
            ImGui::EndChild();
            ImGui::End();
        }

        // ── H key toggles panel visibility ───────────────────────────────────
        if (!io.WantCaptureKeyboard &&
            ImGui::IsKeyPressed(ImGuiKey_H, false))
            showPanel = !showPanel;

        // ── Tiny "Show Panel" button when panel is hidden ─────────────────────
        if (!showPanel)
        {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.75f);
            ImGui::Begin("##showbtn", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
            if (ImGui::Button("Show Panel  [H]")) showPanel = true;
            ImGui::End();
        }

        // ── Main panel ────────────────────────────────────────────────────────
        if (showPanel)
        {
        // First use: place at top-left; after that the user can drag it anywhere.
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(370, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("Map Controls", &showPanel);   // X button also hides it

        ImGui::TextWrapped("Drag title bar to move  |  Click ▼ to collapse  |  H = hide");

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

        // ── Resource layer ────────────────────────────────────────────────────
        ImGui::Checkbox("Show Resource Layer", &showResourceLayer);
        if (showResourceLayer) showCivLayer = false; // mutually exclusive
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Overlays resource deposits on the climate biome map.\nIron=blue  Wood=green  Niter=pale blue  ManaStone=magenta\nGold=teal  Silver=silver  Copper=copper");

        // ── Civilization layer ────────────────────────────────────────────────
        if (ImGui::Checkbox("Show Civilization Layer", &showCivLayer))
            if (showCivLayer) showResourceLayer = false;
        if (showCivLayer && !civTextures.empty())
        {
            int civCount = int(map.civSnaps.size());
            ImGui::Text("Civ Timeline  (%d snapshots):", civCount);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderInt("##civsnap", &activeCivSnap, 0, civCount-1))
            {
                int civMax = civCount-1;
                activeCivSnap = std::clamp(activeCivSnap, 0, civMax);
            }
            const CivSnapshot& cs = map.civSnaps[activeCivSnap];
            ImGui::Text("  Tick %d  |  Pop %.0f  |  Settled %d",
                        cs.tick, cs.totalPop, cs.settledCells);
            ImGui::Text("  Countries %d  |  Cultures %d",
                        cs.countryCount, cs.cultureGroups);

            // Quick-jump buttons
            ImGui::Text("Jump to:");
            for (int s = 0; s < civCount; ++s)
            {
                if (s > 0) ImGui::SameLine();
                char lbl[16];
                std::snprintf(lbl, sizeof(lbl), "%d", s+1);
                bool active = (s == activeCivSnap);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f,0.5f,0.1f,1.f));
                if (ImGui::SmallButton(lbl)) activeCivSnap = s;
                if (active) ImGui::PopStyleColor();
            }
        }
        else if (showCivLayer && civTextures.empty())
        {
            ImGui::TextColored(ImVec4(1,0.5f,0.2f,1), "No civilization data — click Generate.");
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

        if (ImGui::CollapsingHeader("Civilization Settings"))
        {
            ImGui::SliderInt("Civ Steps",        &settings.civSteps,      10, 10000);
            ImGui::SliderInt("Civ Snapshots",    &settings.civSnapshots,   1,   20);
            ImGui::SliderFloat("Growth Rate",    &settings.growthRate,  0.001f, 0.5f, "%.3f");
            ImGui::SliderFloat("Spread Thresh",  &settings.spreadThresh, 0.f,   1.f, "%.2f");

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.9f,0.8f,0.4f,1.f), "Cohesion");
            ImGui::SliderFloat("Coh Strength",   &settings.cohesionStrength, 0.f,  1.f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Scales all neighbour-based cohesion ceilings.\n0 = very fragile (constant fragmentation)\n1 = very stable (large empires persist)");
            ImGui::SliderFloat("Coh Half-Life",  &settings.cohesionHalfLife, 0.05f, 1.f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Distance from capital (as fraction of map size)\nat which cohesion halves.\n0.05 = tiny empires  0.5 = continent-spanning");
            ImGui::SliderFloat("Coh Lerp Rate",  &settings.cohesionLerpRate, 0.01f, 0.3f, "%.3f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("How fast cohesion changes per tick.\nLow = slow, gradual fragmentation\nHigh = rapid, dramatic splits");

            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.f,0.6f,1.f,1.f), "Great People");

            ImGui::TextColored(ImVec4(1.f,0.8f,0.3f,1.f), "Military Great People");
            ImGui::SliderFloat("Mil GP Frequency", &settings.gpMilFrequency, 0.f, 3.f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Multiplier on military great person spawn rate.\n0 = never  1 = default  2 = double rate\nAppear based on pop × militarism × Iron/Niter/Copper");
            ImGui::SliderFloat("Mil GP Power Min", &settings.gpMilPowerMin, 0.f, 1.f, "%.2f");
            ImGui::SliderFloat("Mil GP Power Max", &settings.gpMilPowerMax, 0.f, 1.f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Power range for military great people.\nPower scales their military strength bonus.");

            ImGui::TextColored(ImVec4(1.f,0.5f,1.f,1.f), "Arcane Great People");
            ImGui::SliderFloat("Arc GP Frequency", &settings.gpArcaneFrequency, 0.f, 3.f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Multiplier on arcane great person spawn rate.\nRequires Arcane tech + ManaStone stockpile.\nRarer than military great people by default.");
            ImGui::SliderFloat("Arc GP Power Min", &settings.gpArcanePowerMin, 0.f, 1.f, "%.2f");
            ImGui::SliderFloat("Arc GP Power Max", &settings.gpArcanePowerMax, 0.f, 1.f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Power range for arcane great people.\nArcane Harbinger always gets max power.");
            ImGui::SliderFloat("Harbinger Chance", &settings.gpHarbingerChance, 0.f, 0.5f, "%.3f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Probability that an arcane great person is a Harbinger.\nHarbingers are legendary figures with massive military power.\nDefault: 5% (very rare)");

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.4f,1.f,0.8f,1.f), "Diplomacy / Buyout");

            ImGui::SliderFloat("Iso Buyout Block", &settings.isolationismBuyoutBlock, 0.05f, 2.5f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Isolationism value at or above which Gold buyout is impossible.\n"
                                  "Default 0.5 — most isolationist civs resist buyout.\n"
                                  "Set to 2.5 to allow buying even very isolationist civs.\n"
                                  "Highly isolationist civs can only be taken by conquest.");

            ImGui::SliderFloat("Iso Price Scale", &settings.isolationismPriceScale, 0.f, 5.f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("How much each point of Isolationism inflates the buyout price.\n"
                                  "price *= (1 + Isolationism × scale)\n"
                                  "Default 1.5 — each point adds 150% to the base price.");

            ImGui::SliderFloat("Cult Sim Discount", &settings.culturalSimMaxDiscount, 0.f, 1.f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Maximum Gold discount from cultural similarity [0,1].\n"
                                  "0 = no cultural discount\n"
                                  "0.5 = identical cultures pay 50% less (default)\n"
                                  "1.0 = identical cultures pay nothing");

            ImGui::SliderFloat("Diplomacy Discount", &settings.diplomacyTechDiscount, 0.f, 0.9f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Additional Gold discount when buyer has Diplomacy tech.\n"
                                  "Default 0.30 (30% off).\n"
                                  "Stacks multiplicatively with cultural similarity discount.");

            ImGui::SliderFloat("Buyout Coh Cap", &settings.buyoutCohesionCap, 0.1f, 1.f, "%.2f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Target's average cohesion must be below this to accept buyout.\n"
                                  "Default 0.65 — only fragile/unstable countries can be bought.\n"
                                  "Set higher to allow buying stable countries (at great cost).");
        }

        if (ImGui::Button("Generate", ImVec2(-1, 0)))
        {
            map = generateMap(settings);

            // Delete old geology textures
            for (auto& st2 : snapTextures) deleteSnapTextures(st2);
            snapTextures.clear();
            snapTextures.resize(map.snapshots.size());
            for (int s = 0; s < int(map.snapshots.size()); ++s)
                uploadSnapshot(snapTextures[s], map.snapshots[s], map.size);
            activeSnapshot = int(map.snapshots.size()) - 1;

            // Upload new civilization textures
            uploadCivSnaps();
            activeCivSnap = int(map.civSnaps.size())-1;

            // Upload resource textures
            uploadResourceTextures();
        }

        ImGui::Separator();
        ImGui::Checkbox("Auto Rotate", &rotateGlobe);
        ImGui::SliderFloat("Rotation Speed", &rotationSpeed, 0.f, 90.f, "%.1f deg/s");
        ImGui::Checkbox("Civ Click Debug", &showCivDebug);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Shows crosshair + coordinate debug panel\nwhen in 2D Map view.\nHelps verify click-to-inspect accuracy.");

        ImGui::Separator();
        ImGui::Text("Requested plates : %d", settings.plateCount);
        ImGui::Text("Final plates     : %d", map.finalPlateCount);
        ImGui::Text("Size             : %dx%d", map.size, map.size);
        ImGui::Text("Seed             : %u", settings.seed);
        ImGui::Text("Convergent       : %d", map.convergentCount);
        ImGui::Text("Transform        : %d", map.transformCount);
        ImGui::Text("Divergent        : %d", map.divergentCount);
        if (!map.eventLog.empty()) {
            ImGui::Text("Events logged    : %d", int(map.eventLog.size()));
            ImGui::Text("Countries (final): %d", int(map.countryStates.size()));
            if (ImGui::Button("Event Log", ImVec2(-1, 0)))
                showEventLog = !showEventLog;
        }

        ImGui::End();
        } // end if (showPanel)

        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    for (int si = 0; si < int(snapTextures.size()); ++si) deleteSnapTextures(snapTextures[si]);
    for (int ci = 0; ci < int(civTextures.size()); ++ci){
        if(civTextures[ci].globe)    glDeleteTextures(1,&civTextures[ci].globe);
        if(civTextures[ci].mercator) glDeleteTextures(1,&civTextures[ci].mercator);
    }
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
