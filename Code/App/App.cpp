#include "App.h"

#include <Sim/World.h>

#include <Core/Containers/UniquePtr.h>

#include <imgui.h>

class AppState
{
public:
    enum class Mode
    {
        kTitle,
        kPlay
    };

    Mode mMode = Mode::kTitle;
    UniquePtr<World> mWorld;
};

AppState* gAppState = nullptr;

void AppInit()
{
    gAppState = new AppState;
}

void AppUpdate()
{

}

void AppRenderUI()
{
    if (gAppState->mMode == AppState::Mode::kTitle)
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("World"))
            {
                if (ImGui::MenuItem("New"))
                {
                    gAppState->mWorld = new World;
                }
                if (ImGui::MenuItem("Load"))
                {

                }
                if (ImGui::MenuItem("Save"))
                {

                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }
}

void AppDone()
{
    delete gAppState;
}

