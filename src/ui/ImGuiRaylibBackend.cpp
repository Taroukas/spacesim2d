#include "ui/ImGuiRaylibBackend.hpp"

#include <imgui.h>
#include <raylib.h>
#include <rlgl.h>

#include <cstdint>

namespace sim::imgui_backend {

namespace {

struct BackendState {
    Texture2D fontTexture{};
    ImGuiMouseCursor lastCursor = ImGuiMouseCursor_COUNT;
    bool initialized = false;
};

BackendState g_state;

struct KeyMapping {
    int raylibKey;
    ImGuiKey imguiKey;
};

constexpr KeyMapping kKeyMap[] = {
    { KEY_APOSTROPHE,    ImGuiKey_Apostrophe },
    { KEY_COMMA,         ImGuiKey_Comma },
    { KEY_MINUS,         ImGuiKey_Minus },
    { KEY_PERIOD,        ImGuiKey_Period },
    { KEY_SLASH,         ImGuiKey_Slash },
    { KEY_SEMICOLON,     ImGuiKey_Semicolon },
    { KEY_EQUAL,         ImGuiKey_Equal },
    { KEY_LEFT_BRACKET,  ImGuiKey_LeftBracket },
    { KEY_BACKSLASH,     ImGuiKey_Backslash },
    { KEY_RIGHT_BRACKET, ImGuiKey_RightBracket },
    { KEY_GRAVE,         ImGuiKey_GraveAccent },
    { KEY_SPACE,         ImGuiKey_Space },
    { KEY_ESCAPE,        ImGuiKey_Escape },
    { KEY_ENTER,         ImGuiKey_Enter },
    { KEY_TAB,           ImGuiKey_Tab },
    { KEY_BACKSPACE,     ImGuiKey_Backspace },
    { KEY_INSERT,        ImGuiKey_Insert },
    { KEY_DELETE,        ImGuiKey_Delete },
    { KEY_RIGHT,         ImGuiKey_RightArrow },
    { KEY_LEFT,          ImGuiKey_LeftArrow },
    { KEY_DOWN,          ImGuiKey_DownArrow },
    { KEY_UP,            ImGuiKey_UpArrow },
    { KEY_PAGE_UP,       ImGuiKey_PageUp },
    { KEY_PAGE_DOWN,     ImGuiKey_PageDown },
    { KEY_HOME,          ImGuiKey_Home },
    { KEY_END,           ImGuiKey_End },
    { KEY_CAPS_LOCK,     ImGuiKey_CapsLock },
    { KEY_SCROLL_LOCK,   ImGuiKey_ScrollLock },
    { KEY_NUM_LOCK,      ImGuiKey_NumLock },
    { KEY_PRINT_SCREEN,  ImGuiKey_PrintScreen },
    { KEY_PAUSE,         ImGuiKey_Pause },
    { KEY_LEFT_SHIFT,    ImGuiKey_LeftShift },
    { KEY_LEFT_CONTROL,  ImGuiKey_LeftCtrl },
    { KEY_LEFT_ALT,      ImGuiKey_LeftAlt },
    { KEY_LEFT_SUPER,    ImGuiKey_LeftSuper },
    { KEY_RIGHT_SHIFT,   ImGuiKey_RightShift },
    { KEY_RIGHT_CONTROL, ImGuiKey_RightCtrl },
    { KEY_RIGHT_ALT,     ImGuiKey_RightAlt },
    { KEY_RIGHT_SUPER,   ImGuiKey_RightSuper },
    { KEY_KB_MENU,       ImGuiKey_Menu },
    { KEY_KP_DECIMAL,    ImGuiKey_KeypadDecimal },
    { KEY_KP_DIVIDE,     ImGuiKey_KeypadDivide },
    { KEY_KP_MULTIPLY,   ImGuiKey_KeypadMultiply },
    { KEY_KP_SUBTRACT,   ImGuiKey_KeypadSubtract },
    { KEY_KP_ADD,        ImGuiKey_KeypadAdd },
    { KEY_KP_ENTER,      ImGuiKey_KeypadEnter },
    { KEY_KP_EQUAL,      ImGuiKey_KeypadEqual },
};

ImTextureID toTextureId(unsigned int glId) {
    return reinterpret_cast<ImTextureID>(static_cast<std::intptr_t>(glId));
}

unsigned int fromTextureId(ImTextureID id) {
    return static_cast<unsigned int>(reinterpret_cast<std::intptr_t>(id));
}

const char* getClipboard(void* /*userData*/) {
    return GetClipboardText();
}

void setClipboard(void* /*userData*/, const char* text) {
    SetClipboardText(text);
}

void submitKey(ImGuiIO& io, int raylibKey, ImGuiKey imguiKey) {
    if (IsKeyPressed(raylibKey)) {
        io.AddKeyEvent(imguiKey, true);
    } else if (IsKeyReleased(raylibKey)) {
        io.AddKeyEvent(imguiKey, false);
    }
}

void processKeyboard(ImGuiIO& io) {
    io.AddKeyEvent(ImGuiMod_Ctrl,  IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
    io.AddKeyEvent(ImGuiMod_Shift, IsKeyDown(KEY_LEFT_SHIFT)   || IsKeyDown(KEY_RIGHT_SHIFT));
    io.AddKeyEvent(ImGuiMod_Alt,   IsKeyDown(KEY_LEFT_ALT)     || IsKeyDown(KEY_RIGHT_ALT));
    io.AddKeyEvent(ImGuiMod_Super, IsKeyDown(KEY_LEFT_SUPER)   || IsKeyDown(KEY_RIGHT_SUPER));

    for (const KeyMapping& km : kKeyMap) {
        submitKey(io, km.raylibKey, km.imguiKey);
    }

    // Contiguous ranges in both raylib and ImGui key enums
    for (int i = 0; i < 26; ++i) {
        submitKey(io, KEY_A + i, static_cast<ImGuiKey>(ImGuiKey_A + i));
    }
    for (int i = 0; i < 10; ++i) {
        submitKey(io, KEY_ZERO + i, static_cast<ImGuiKey>(ImGuiKey_0 + i));
        submitKey(io, KEY_KP_0 + i, static_cast<ImGuiKey>(ImGuiKey_Keypad0 + i));
    }
    for (int i = 0; i < 12; ++i) {
        submitKey(io, KEY_F1 + i, static_cast<ImGuiKey>(ImGuiKey_F1 + i));
    }

    // Text input (UTF-32 codepoints)
    int codepoint = GetCharPressed();
    while (codepoint != 0) {
        io.AddInputCharacter(static_cast<unsigned int>(codepoint));
        codepoint = GetCharPressed();
    }
}

void processMouse(ImGuiIO& io) {
    Vector2 mouse = GetMousePosition();
    io.AddMousePosEvent(mouse.x, mouse.y);

    const int buttons[3] = { MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE };
    for (int i = 0; i < 3; ++i) {
        if (IsMouseButtonPressed(buttons[i])) {
            io.AddMouseButtonEvent(i, true);
        } else if (IsMouseButtonReleased(buttons[i])) {
            io.AddMouseButtonEvent(i, false);
        }
    }

    Vector2 wheel = GetMouseWheelMoveV();
    if (wheel.x != 0.0f || wheel.y != 0.0f) {
        io.AddMouseWheelEvent(wheel.x, wheel.y);
    }
}

void updateMouseCursor(ImGuiIO& io) {
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) {
        return;
    }

    ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
    if (cursor == g_state.lastCursor) {
        return;
    }
    g_state.lastCursor = cursor;

    if (io.MouseDrawCursor || cursor == ImGuiMouseCursor_None) {
        HideCursor();
        return;
    }

    ShowCursor();
    int rlCursor = MOUSE_CURSOR_DEFAULT;
    switch (cursor) {
        case ImGuiMouseCursor_Arrow:      rlCursor = MOUSE_CURSOR_ARROW;         break;
        case ImGuiMouseCursor_TextInput:  rlCursor = MOUSE_CURSOR_IBEAM;         break;
        case ImGuiMouseCursor_ResizeAll:  rlCursor = MOUSE_CURSOR_RESIZE_ALL;    break;
        case ImGuiMouseCursor_ResizeNS:   rlCursor = MOUSE_CURSOR_RESIZE_NS;     break;
        case ImGuiMouseCursor_ResizeEW:   rlCursor = MOUSE_CURSOR_RESIZE_EW;     break;
        case ImGuiMouseCursor_ResizeNESW: rlCursor = MOUSE_CURSOR_RESIZE_NESW;   break;
        case ImGuiMouseCursor_ResizeNWSE: rlCursor = MOUSE_CURSOR_RESIZE_NWSE;   break;
        case ImGuiMouseCursor_Hand:       rlCursor = MOUSE_CURSOR_POINTING_HAND; break;
        case ImGuiMouseCursor_NotAllowed: rlCursor = MOUSE_CURSOR_NOT_ALLOWED;   break;
        default:                          rlCursor = MOUSE_CURSOR_DEFAULT;       break;
    }
    SetMouseCursor(rlCursor);
}

void emitVertex(const ImDrawVert& v) {
    const ImU32 c = v.col; // IM_COL32 layout: R in low byte, A in high byte
    rlColor4ub(static_cast<unsigned char>(c & 0xFF),
               static_cast<unsigned char>((c >> 8) & 0xFF),
               static_cast<unsigned char>((c >> 16) & 0xFF),
               static_cast<unsigned char>((c >> 24) & 0xFF));
    rlTexCoord2f(v.uv.x, v.uv.y);
    rlVertex2f(v.pos.x, v.pos.y);
}

void renderTriangles(const ImDrawList* cmdList, const ImDrawCmd& cmd) {
    if (cmd.ElemCount < 3) return;

    const unsigned int texId = fromTextureId(cmd.GetTexID());
    const ImDrawIdx* indices = cmdList->IdxBuffer.Data;
    const ImDrawVert* verts = cmdList->VtxBuffer.Data;

    rlBegin(RL_TRIANGLES);
    rlSetTexture(texId);

    for (unsigned int i = 0; i + 2 < cmd.ElemCount; i += 3) {
        // Flush and restart the batch if it would overflow
        if (rlCheckRenderBatchLimit(3)) {
            rlBegin(RL_TRIANGLES);
            rlSetTexture(texId);
        }

        const unsigned int base = cmd.IdxOffset + i;
        emitVertex(verts[indices[base + 0] + cmd.VtxOffset]);
        emitVertex(verts[indices[base + 1] + cmd.VtxOffset]);
        emitVertex(verts[indices[base + 2] + cmd.VtxOffset]);
    }

    rlEnd();
    rlSetTexture(0);
}

void renderDrawData(ImDrawData* drawData) {
    if (drawData == nullptr || drawData->CmdListsCount == 0) return;

    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();

    const ImVec2 clipOff = drawData->DisplayPos;
    const ImVec2 fbScale = drawData->FramebufferScale;
    const float displayH = drawData->DisplaySize.y;

    for (int n = 0; n < drawData->CmdListsCount; ++n) {
        const ImDrawList* cmdList = drawData->CmdLists[n];

        for (int c = 0; c < cmdList->CmdBuffer.Size; ++c) {
            const ImDrawCmd& cmd = cmdList->CmdBuffer[c];

            if (cmd.UserCallback != nullptr) {
                if (cmd.UserCallback != ImDrawCallback_ResetRenderState) {
                    cmd.UserCallback(cmdList, &cmd);
                }
                continue;
            }

            const float clipX = cmd.ClipRect.x - clipOff.x;
            const float clipY = cmd.ClipRect.y - clipOff.y;
            const float clipW = cmd.ClipRect.z - cmd.ClipRect.x;
            const float clipH = cmd.ClipRect.w - cmd.ClipRect.y;
            if (clipW <= 0.0f || clipH <= 0.0f) continue;

            rlDrawRenderBatchActive();
            rlEnableScissorTest();
            rlScissor(static_cast<int>(clipX * fbScale.x),
                      static_cast<int>((displayH - (clipY + clipH)) * fbScale.y),
                      static_cast<int>(clipW * fbScale.x),
                      static_cast<int>(clipH * fbScale.y));

            renderTriangles(cmdList, cmd);
            rlDrawRenderBatchActive();
        }
    }

    rlDisableScissorTest();
    rlEnableBackfaceCulling();
}

void uploadFontAtlas() {
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    Image image{};
    image.data = pixels; // Owned by ImGui; uploaded to GPU, never freed via UnloadImage
    image.width = width;
    image.height = height;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    g_state.fontTexture = LoadTextureFromImage(image);
    io.Fonts->SetTexID(toTextureId(g_state.fontTexture.id));
}

} // namespace

void setup(bool darkTheme) {
    if (g_state.initialized) return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = "imgui_impl_raylib_sim";
    io.BackendRendererName = "imgui_impl_rlgl_sim";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // Fully standalone: no files written next to the executable
    io.SetClipboardTextFn = setClipboard;
    io.GetClipboardTextFn = getClipboard;
    io.ClipboardUserData = nullptr;

    if (darkTheme) {
        ImGui::StyleColorsDark();
    } else {
        ImGui::StyleColorsLight();
    }

    ImFontConfig fontCfg{};
    fontCfg.SizePixels = 16.0f;
    fontCfg.OversampleH = 3;
    fontCfg.OversampleV = 3;
    fontCfg.PixelSnapH = true;
    io.Fonts->AddFontDefault(&fontCfg);
    io.FontGlobalScale = 1.15f;
    uploadFontAtlas();

    g_state.lastCursor = ImGuiMouseCursor_COUNT;
    g_state.initialized = true;
}

void beginFrame() {
    if (!g_state.initialized) return;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    if (IsWindowState(FLAG_WINDOW_HIGHDPI)) {
        Vector2 dpi = GetWindowScaleDPI();
        io.DisplayFramebufferScale = ImVec2(dpi.x, dpi.y);
    }

    const float dt = GetFrameTime();
    io.DeltaTime = (dt > 0.0f) ? dt : (1.0f / 60.0f);

    io.AddFocusEvent(IsWindowFocused());
    processMouse(io);
    processKeyboard(io);
    updateMouseCursor(io);

    ImGui::NewFrame();
}

void endFrame() {
    if (!g_state.initialized) return;

    ImGui::Render();
    renderDrawData(ImGui::GetDrawData());
}

void shutdown() {
    if (!g_state.initialized) return;

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->SetTexID(toTextureId(0));
    if (g_state.fontTexture.id != 0) {
        UnloadTexture(g_state.fontTexture);
        g_state.fontTexture = Texture2D{};
    }

    ImGui::DestroyContext();
    g_state.initialized = false;
}

} // namespace sim::imgui_backend
