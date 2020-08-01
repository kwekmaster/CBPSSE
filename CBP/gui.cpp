#include "pch.h"

namespace CBP
{
    DUI DUI::m_Instance;

    DUI::DUI() :
        m_isRunning(false)
    {
    }

    bool DUI::Initialize()
    {
        if (!Hook::Call5(m_Instance.CreateD3D11,
            reinterpret_cast<uintptr_t>(CreateD3D11_Hook),
            m_Instance.CreateD3D11_O))
        {
            m_Instance.Error("CreateD3D11 hook failed");
            return false;
        }

        if (!Hook::Call5(m_Instance.UnkPresent,
            reinterpret_cast<uintptr_t>(Present_Pre),
            m_Instance.UnkPresent_O))
        {
            m_Instance.Error("Present_Pre hook failed");
            return false;
        }

        //Hook::Call5(RelocAddr<uintptr_t>(0x5B15B6), uintptr_t(tt), tt_o);

        DInput::RegisterForKeyEvents(&m_Instance.inputEventHandler);


        return true;
    }

    void DUI::Present_Pre(uint32_t p1)
    {
        m_Instance.UnkPresent_O(p1);
        m_Instance.Present_Pre_Impl();
    }

    void DUI::Present_Pre_Impl()
    {
        if (m_drawCallbacks.size() == 0) {
            m_isRunning = false;
            return;
        }

        auto mm = MenuManager::GetSingleton();
        if (mm && mm->InPausedMenu()) {
            m_isRunning = false;
            return;
        }

        m_isRunning = true;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        ImGui::NewFrame();

        auto it = m_drawCallbacks.begin();
        while (it != m_drawCallbacks.end())
        {
            ImGui::PushID(static_cast<const void*>(it->second));
            bool res = it->second();
            ImGui::PopID();

            if (!res) {
                it = m_drawCallbacks.erase(it);
            }
            else {
                ++it;
            }
        }

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        m_keyEvents.ProcessTasks();

        if (m_drawCallbacks.size() == 0) {
            ResetImGuiIO();
            m_isRunning = false;
        }
    }

    void DUI::CreateD3D11_Hook()
    {
        m_Instance.CreateD3D11_O();

        auto rm = BSRenderManager::GetSingleton();
        if (!rm) {
            m_Instance.Error("Couldn't get BSRenderManager");
            return;
        }

        if (!rm->swapChain)
            return;

        DXGI_SWAP_CHAIN_DESC sd;
        if (rm->swapChain->GetDesc(&sd) != S_OK) {
            m_Instance.Error("IDXGISwapChain::GetDesc failed");
            return;
        }

        m_Instance.info.bufferSize.width = static_cast<float>(sd.BufferDesc.Width);
        m_Instance.info.bufferSize.height = static_cast<float>(sd.BufferDesc.Height);
        m_Instance.g_WindowHandle = sd.OutputWindow;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::LoadIniSettingsFromDisk(PLUGIN_IMGUI_INI_FILE);

        ImGuiIO& io = ImGui::GetIO();
        io.MouseDrawCursor = true;
        io.WantSetMousePos = true;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(m_Instance.info.bufferSize.width, m_Instance.info.bufferSize.height);
        io.MousePos.x = io.DisplaySize.x / 2.0f;
        io.MousePos.y = io.DisplaySize.y / 2.0f;

        ImGui::StyleColorsDark();

        ImGui_ImplWin32_Init(sd.OutputWindow);
        ImGui_ImplDX11_Init(rm->forwarder, rm->context);
    }

    void DUI::KeyPressHandler::ReceiveEvent(KeyEvent ev, UInt32 keyCode)
    {
        if (!m_Instance.m_isRunning) {
            return;
        }

        switch (keyCode)
        {
        case InputMap::kMacro_MouseButtonOffset:
            m_Instance.m_keyEvents.AddTask(new KeyEventTask(ev, KeyEventTask::kMouseButton, 0U));
            break;
        case InputMap::kMacro_MouseButtonOffset + 1:
            m_Instance.m_keyEvents.AddTask(new KeyEventTask(ev, KeyEventTask::kMouseButton, 1U));
            break;
        case InputMap::kMacro_MouseButtonOffset + 2:
            m_Instance.m_keyEvents.AddTask(new KeyEventTask(ev, KeyEventTask::kMouseButton, 2U));
            break;
        case InputMap::kMacro_MouseWheelOffset:
            m_Instance.m_keyEvents.AddTask(new KeyEventTask(ev, KeyEventTask::kMouseWheel, 1.0f));
            return;
        case InputMap::kMacro_MouseWheelOffset + 1:
            m_Instance.m_keyEvents.AddTask(new KeyEventTask(ev, KeyEventTask::kMouseWheel, -1.0f));
            return;
        default:
            if (keyCode < 256)
            {
                UINT vkCode;

                switch (keyCode)
                {
                case DIK_LEFT:
                    vkCode = VK_LEFT;
                    break;
                case DIK_RIGHT:
                    vkCode = VK_RIGHT;
                    break;
                case DIK_UP:
                    vkCode = VK_UP;
                    break;
                case DIK_DOWN:
                    vkCode = VK_DOWN;
                    break;
                default:
                    vkCode = MapVirtualKeyW(keyCode, MAPVK_VSC_TO_VK);
                    if (vkCode == 0) {
                        return;
                    }
                }

                WORD Char = 0;

                if (ev == KeyEvent::KeyDown) {
                    if (GetKeyboardState(keyState)) {
                        if (!ToAscii(vkCode, keyCode, keyState, &Char, 0) != 0) {
                            Char = 0;
                        }
                    }
                }

                m_Instance.m_keyEvents.AddTask(
                    new KeyEventTask(
                        ev,
                        KeyEventTask::kKeyboard,
                        vkCode,
                        Char
                    )
                );
            }
        }
    }

    DUI::KeyEventTask::KeyEventTask(
        KeyEvent a_event, KeyEventType a_eventType, UINT a_val, WCHAR a_k) :
        m_event(a_event),
        m_eventType(a_eventType),
        m_k(a_k)
    {
        b.m_uval = a_val;
    }

    DUI::KeyEventTask::KeyEventTask(
        KeyEvent a_event, KeyEventType a_eventType, float a_val) :
        m_event(a_event),
        m_eventType(a_eventType)
    {
        b.m_fval = a_val;
    }

    void DUI::KeyEventTask::Run()
    {
        auto& io = ImGui::GetIO();

        switch (m_eventType) {
        case kMouseButton:
            io.MouseDown[b.m_uval] = (m_event == KeyEvent::KeyDown ? true : false);
            /*if (!ImGui::IsAnyMouseDown() && ::GetCapture() == m_Instance.g_WindowHandle)
                ::ReleaseCapture();*/
            break;
        case kMouseWheel:
            io.MouseWheel += b.m_fval;
            break;
        case kKeyboard:
            if (m_event == KeyEvent::KeyDown) {
                io.KeysDown[b.m_uval] = true;
                if (m_k != 0) {
                    io.AddInputCharacterUTF16(m_k);
                }
            }
            else {
                io.KeysDown[b.m_uval] = false;
            }
            break;
        }
    }

    void DUI::AddCallback(uint32_t id, uiDrawCallback_t f) {
        m_Instance.m_drawCallbacks.emplace(id, f);
    }

    void DUI::RemoveCallback(uint32_t id) {
        m_Instance.m_drawCallbacks.erase(id);
        if (m_Instance.m_drawCallbacks.size() == 0) {
            m_Instance.ResetImGuiIO();
        }
    }

    bool DUI::HasCallback(uint32_t id) {
        return m_Instance.m_drawCallbacks.contains(id);
    }

    void DUI::ResetImGuiIO()
    {
        auto& io = ImGui::GetIO();

        memset(&io.KeysDown, 0x0, sizeof(io.KeysDown));
        memset(&io.MouseDown, 0x0, sizeof(io.MouseDown));

        io.MouseWheel = 0.0f;
        io.KeyCtrl = false;
        io.KeyShift = false;
        io.KeyAlt = false;

        m_keyEvents.ClearTasks();
    }

    namespace UICommon
    {
        template<typename... Args>
        bool ConfirmDialog(const char* name, const char* text, Args... args)
        {
            bool ret = false;
            auto& io = ImGui::GetIO();

            ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

            if (ImGui::BeginPopupModal(name, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 20.0f);
                ImGui::Text(text, args...);
                ImGui::PopTextWrapPos();
                ImGui::Separator();

                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                    ret = true;
                }

                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            return ret;
        }

        template bool ConfirmDialog<>(const char*, const char*);
        template bool ConfirmDialog<const char*>(const char*, const char*, const char*);

        template<typename... Args>
        void MessageDialog(const char* name, const char* text, Args... args)
        {
            auto& io = ImGui::GetIO();

            ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

            if (ImGui::BeginPopupModal(name, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
                ImGui::Text(text, args...);
                ImGui::PopTextWrapPos();
                ImGui::Separator();

                ImGui::SetItemDefaultFocus();
                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }

        template void MessageDialog<>(const char*, const char*);
        template void MessageDialog<const char*>(const char*, const char*, const char*);
        template void MessageDialog<const char*, const char*>(const char*, const char*, const char*, const char*);
        template void MessageDialog<const char*, const char*, const char*>(const char*, const char*, const char*, const char*, const char*);

        template<typename... Args>
        bool TextInputDialog(const char* a_name, const char* a_text, char* a_buf, size_t a_size, Args... args)
        {
            bool ret = false;

            auto& io = ImGui::GetIO();

            ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

            if (ImGui::BeginPopupModal(a_name, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 20.0f);
                ImGui::Text(a_text, args...);
                ImGui::PopTextWrapPos();

                ImGui::Spacing();

                if (!ImGui::IsAnyItemActive())
                    ImGui::SetKeyboardFocusHere();

                if (ImGui::InputText("", a_buf, a_size, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    ImGui::CloseCurrentPopup();
                    ret = true;
                }

                ImGui::Separator();

                if (ImGui::Button("OK", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                    ret = true;
                }

                ImGui::SetItemDefaultFocus();
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

            return ret;
        }

        template bool TextInputDialog<>(const char*, const char*, char*, size_t);

        void HelpMarker(const char* desc)
        {
            ImGui::TextDisabled("[?]");
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted(desc);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }
    }

}