// Dear ImGui: standalone example application for DirectX 11

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <math.h>
#include <string>
#include <vector>
#include <memory>
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "ImGuiFileDialog.h"
#include <GL/gl.h>
//#include <GL/glew.h>
//#include <GLFW/glfw3.h>



using namespace ImGui;
using namespace std;

#define CLAMP(x, min, max) ((x < min) ? min : (x > max) ? max : x)

// Data
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


// Get the next available Pin ID
int GetNextPinId() {
    static int currentId = 1000;
    return currentId++;
}

struct Pin {
    int id;
    string label;
    ImVec2 Pos;
    bool isInput;
    string ParentNodeId;
    float radius = 6.2f;

    bool IsMouseOver(const ImVec2& mousePos) {
        double distance = sqrt(pow(mousePos.x - Pos.x, 2) + pow(mousePos.y - Pos.y, 2));
        return distance <= radius ;
    }
};

struct Link {
    int id;
    int fromPinId;  // Using Pin ID instead of Pin pointer
    int toPinId;    // Using Pin ID instead of Pin pointer
};


// Variables for Linking
bool isDraggingLink = false;
ImVec2 dragStartPos = ImVec2(0, 0);
ImVec2 currentMousePos = ImVec2(0, 0);
Link* activeLink = nullptr;
vector<Link> links;
vector<Pin> Pins;

// Get Pin by ID
Pin* GetPinById(int pinId) {
    for (auto& pin : Pins) {
        if (pin.id == pinId) {
            return &pin;
        }
    }
    return nullptr;  // Return nullptr if pin with the given ID is not found
}

Pin* GetPinUnderMouse(vector<Pin>& pins) {
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    for (auto& pin : pins) {
        if (pin.IsMouseOver(mousePos)) {
            return &pin;
        }
    }
    return nullptr;
}

void DrawBezierCurve(ImDrawList* drawList, const ImVec2& start, const ImVec2& end, const ImVec2& control, ImU32 color, float thickness = 1.5f) {
    const int segments = 30;
    for (int i = 0; i < segments; ++i) {
        float t0 = (float)i / (float)(segments - 1);
        float t1 = (float)(i + 1) / (float)(segments - 1);

        ImVec2 p0 = ImVec2(
            start.x * (1 - t0) * (1 - t0) + control.x * 2 * (1 - t0) * t0 + end.x * t0 * t0,
            start.y * (1 - t0) * (1 - t0) + control.y * 2 * (1 - t0) * t0 + end.y * t0 * t0
        );

        ImVec2 p1 = ImVec2(
            start.x * (1 - t1) * (1 - t1) + control.x * 2 * (1 - t1) * t1 + end.x * t1 * t1,
            start.y * (1 - t1) * (1 - t1) + control.y * 2 * (1 - t1) * t1 + end.y * t1 * t1
        );

        drawList->AddLine(p0, p1, color, thickness);
    }
}

void UpdateDragLink() {
    ImVec2 mousePos = ImGui::GetMousePos();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Handle the case when dragging has started
    if (isDraggingLink) {
        // Draw the link as the user drags
        ImVec2 start = dragStartPos;
        ImVec2 end = mousePos; // The end follows the mouse position
        ImVec2 control = ImVec2((start.x + end.x) / 2, (start.y + end.y) / 2 - 50);  // Modify this control point as necessary

        // Draw the Bezier curve from the start to the current mouse position
        DrawBezierCurve(drawList, start, end, control, IM_COL32(255, 255, 255, 255));
    }
    else if (activeLink) {
        // Finalize the link when the mouse is released
        Pin* fromPin = GetPinById(activeLink->fromPinId);
        Pin* toPin = GetPinById(activeLink->toPinId);

        if (fromPin && toPin) {
            ImVec2 start = fromPin->Pos;
            ImVec2 end = toPin->Pos;
            ImVec2 control = ImVec2((start.x + end.x) / 2, (start.y + end.y) / 2 - 50);  // Modify this control point as necessary
            DrawBezierCurve(drawList, start, end, control, IM_COL32(255, 255, 255, 255));
        }
    }
}

// Start dragging the link (when a pin is clicked)
void StartDragLink(Pin* fromPin) {
    isDraggingLink = true;
    dragStartPos = fromPin->Pos;
    activeLink = new Link(); // Create a new link object
    activeLink->fromPinId = fromPin->id; // Set the from pin using Pin ID
}

// Function to handle releasing the drag (finalizing the link)
void EndDragLink(Pin* toPin) {
    if (isDraggingLink && activeLink) {
        Pin* fromPin = GetPinById(activeLink->fromPinId);
        if (fromPin && toPin) {
            // Only allow connecting Output (fromPin) -> Input (toPin)
            if (!fromPin->isInput && toPin->isInput) {
                activeLink->toPinId = toPin->id;
                links.push_back(*activeLink);
            }
        }
        isDraggingLink = false;
        delete activeLink;
        activeLink = nullptr;
    }
}

// Function to draw links and handle dragging
void DrawLinksAndHandleDrag(vector<Pin>& pins) {
    if (!isDraggingLink && ImGui::IsMouseDown(0)) {
        Pin* fromPin = GetPinUnderMouse(pins); // Get the pin that was clicked
        if (fromPin) {
            StartDragLink(fromPin);
        }
    }

    // Check for release (when mouse button is released)
    if (ImGui::IsMouseReleased(0)) {
        Pin* toPin = GetPinUnderMouse(pins); // Get the pin under the mouse after drag
        if (toPin && activeLink && toPin!=GetPinById(activeLink->fromPinId)) {
            EndDragLink(toPin);
        }
        else if (activeLink) {
            isDraggingLink = false;
            activeLink = nullptr;
        }
    }

    // Draw all existing links
    for (const Link& link : links) {
        Pin* fromPin = GetPinById(link.fromPinId);
        Pin* toPin = GetPinById(link.toPinId);

        if (fromPin && toPin) {
            ImVec2 start = fromPin->Pos;
            ImVec2 end = toPin->Pos;
            ImVec2 control = ImVec2((start.x + end.x) / 2, (start.y + end.y) / 2 - 50);
            DrawBezierCurve(ImGui::GetForegroundDrawList(), start, end, control, IM_COL32(255, 255, 255, 255));
        }
    }

    // Draw the dragging link (if any)
    UpdateDragLink();
}

GLuint imageTexture = 0;
int imageWidth = 0, imageHeight = 0;
bool imageLoaded = false;

// Main code
int main(int, char**)
{
    
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX11 Example", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);


    


    // Pin Class

    //struct Pin {
    //    int id;
    //    string label;
    //    ImVec2 Pos;
    //    bool isInput;
    //    float radius = 5.0f;

    //    bool IsMouseOver(const ImVec2& mousePos) {
    //        float distance = sqrt(pow(mousePos.x - Pos.x, 2) + pow(mousePos.y - Pos.y, 2));
    //        return distance <= radius;
    //    }
    //};



    // Link Class

    //struct Link {
    //    int id;
    //    int fromPinId;
    //    int toPinId;
    //};

    // Node Class

    class BaseNode {
    public:
        ImVec2 position = ImVec2(50, 50);
        ImVec2 size = ImVec2(150, 250);
        bool selected = false;
        bool resized = false;
        string NodeName = "Simple Node";
        string NodeId = NodeName;
        vector<Pin> inputPins;
        vector<Pin> outputPins;
        int NumOfInputPins = 1;
        int NumOfOutputPins = 1;


        virtual void Draw(ImVec2 gridMin, ImVec2 gridMax, string &NodeName) {
            if (!resized) {
                ImGui::SetNextWindowPos(position);
                ImGui::SetNextWindowSize(size);
                resized = true;
            }

            // Style
            ImGui::PushStyleColor(ImGuiCol_Border, selected ? IM_COL32(212, 28, 28, 255) : IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, selected ? 2.0f : 0.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(59, 59, 56, 255));
            GetStyle().WindowRounding = 7.0f;

            ImGui::PushID(this); // Avoid ID conflicts

            if (ImGui::Begin(NodeName.c_str(), nullptr, ImGuiWindowFlags_NoResize)) {
                ImVec2 winPos = ImGui::GetWindowPos();
                ImVec2 winSize = ImGui::GetWindowSize();

                if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0)) {
                    ImVec2 delta = ImGui::GetIO().MouseDelta;
                    winPos.x += delta.x;
                    winPos.y += delta.y;

                    winPos.x = CLAMP(winPos.x, gridMin.x, gridMax.x - winSize.x);
                    winPos.y = CLAMP(winPos.y, gridMin.y, gridMax.y - winSize.y);
                    ImGui::SetWindowPos(winPos);
                    position = winPos;
                }

                selected = ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0);

                DrawContent();
            }
            ImGui::End();

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
            ImGui::PopID();
        }

        virtual void DrawContent() = 0;
        virtual ~BaseNode() = default;
    };

    // Brightness Node
    
    class BrightnessNode : public BaseNode {
    public:

        BrightnessNode() {
            NodeName = "Brightness Node";
            int num = GetNextPinId();
            NodeId = NodeName + "num";
            NumOfInputPins = 1;
            NumOfOutputPins = 1;
            ImVec2 winPos = ImGui::GetWindowPos();
            float InitialLoc = 120.0f;
            
     
            for (int i = 0;i < NumOfInputPins;i++) {
                ImVec2 localPos = ImVec2(0, InitialLoc);
                string PinName1 = "In";
                inputPins.push_back({ GetNextPinId(),PinName1+"##"+to_string(i),ImVec2(winPos.x + localPos.x, winPos.y + localPos.y),true,NodeId});
            }
            for (int j = 0;j < NumOfOutputPins;j++) {
                ImVec2 localPos = ImVec2(0, InitialLoc);
                string PinName2 = "Out";
                outputPins.push_back({ GetNextPinId(),PinName2 + "##" + to_string(j),ImVec2(winPos.x + localPos.x, winPos.y + localPos.y),false,NodeId});
            }
            /*Pins.insert(Pins.end(), outputPins.begin(), outputPins.end());*/
        }
        float brightness = 0.0f;
        float contrast = 1.0f;

        void DrawContent() override {
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            ImVec2 winPos = ImGui::GetWindowPos();
            ImVec2 winSize = ImGui::GetWindowSize();
            float InitialLoc = 120.0f;
            
            // Draw input pins
            for (int i = 0; i < inputPins.size(); ++i) {
                ImVec2 localPos = ImVec2(0, InitialLoc); // Local offset inside the node
                inputPins[i].Pos = ImVec2(winPos.x + localPos.x, winPos.y + localPos.y);
                drawList->AddCircleFilled(inputPins[i].Pos, 5.0f, IM_COL32(255, 255, 255, 255));
            }

            // Draw output pins
            for (int i = 0; i < outputPins.size(); ++i) {
                ImVec2 localPos = ImVec2(size.x, InitialLoc); // Right edge of node
                outputPins[i].Pos = ImVec2(winPos.x + localPos.x,winPos.y+localPos.y);
                drawList->AddCircleFilled(outputPins[i].Pos, 5.0f, IM_COL32(255, 255, 255, 255));
            }
            float controlsWidth = 120.0f;   // Slider width
            float resetButtonWidth = controlsWidth * 0.5f; // Shorter reset buttons
            float controlSpacing = 15.0f;

            float controlsHeight = (ImGui::GetTextLineHeight() + ImGui::GetFrameHeight() * 2 + controlSpacing + 10.0f) * 2;

            ImVec2 center(
                (winSize.x - controlsWidth) * 0.5f,
                (winSize.y - controlsHeight) * 0.5f
            );
            ImGui::SetCursorPos(center);

            ImGui::BeginGroup();

            // Brightness
            ImGui::SetCursorPosX(center.x + (controlsWidth - ImGui::CalcTextSize("Brightness").x) * 0.5f);
            ImGui::Text("Brightness");

            ImGui::SetCursorPosX(center.x);
            ImGui::SetNextItemWidth(controlsWidth);
            ImGui::SliderFloat("##Brightness", &brightness, -100.0f, 100.0f, "%.1f");

            // Center the short Reset button
            ImGui::SetCursorPosX(center.x + (controlsWidth - resetButtonWidth) * 0.5f);
            if (ImGui::Button("Reset##0", ImVec2(resetButtonWidth, 0))) {
                brightness = 0.0f;
            }

            // Gap
            ImGui::Dummy(ImVec2(1.0f, controlSpacing));

            // Contrast
            ImGui::SetCursorPosX(center.x + (controlsWidth - ImGui::CalcTextSize("Contrast").x) * 0.5f);
            ImGui::Text("Contrast");

            ImGui::SetCursorPosX(center.x);
            ImGui::SetNextItemWidth(controlsWidth);
            ImGui::SliderFloat("##Contrast", &contrast, 0.0f, 3.0f, "%.2f");

            ImGui::SetCursorPosX(center.x + (controlsWidth - resetButtonWidth) * 0.5f);
            if (ImGui::Button("Reset##1", ImVec2(resetButtonWidth, 0))) {
                contrast = 1.0f;
            }

            ImGui::EndGroup();
  
            Pins.erase(remove_if(Pins.begin(), Pins.end(), [this](const Pin& p) {return p.ParentNodeId == this->NodeId;}),Pins.end());
            Pins.insert(Pins.end(), inputPins.begin(), inputPins.end());
            Pins.insert(Pins.end(), outputPins.begin(), outputPins.end());
            DrawLinksAndHandleDrag(Pins);
 
            
        }
    };





 

    class InputImageNode : public BaseNode {
    public:
        InputImageNode() {
            NodeName = "Input Image";
            NodeId = NodeName + "num";
            NumOfInputPins = 0;
            NumOfOutputPins = 1;
            

            ImVec2 winPos = ImGui::GetWindowPos();
            float InitialLoc = 120.0f;

            ImVec2 localPos = ImVec2(size.x, InitialLoc);
            string PinName = "Out";
            outputPins.push_back({ GetNextPinId(), PinName + "##0", ImVec2(winPos.x + localPos.x, winPos.y + localPos.y), false, NodeId });
        }

        void DrawContent() override {
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            ImVec2 winPos = ImGui::GetWindowPos();
            ImVec2 winSize = ImGui::GetWindowSize();
            float InitialLoc = 120.0f;

            ImVec2 localPos = ImVec2(size.x, InitialLoc);
            outputPins[0].Pos = ImVec2(winPos.x + localPos.x, winPos.y + localPos.y);
            drawList->AddCircleFilled(outputPins[0].Pos, 5.0f, IM_COL32(255, 255, 255, 255));

            // UI Title
            ImGui::Text("Image Source");

             //Button to open file dialog
            if (ImGui::Button("Load Image")) {
                ImGuiFileDialog::Instance()->OpenDialog("ChooseImage", "Choose Image", ".png,.jpg,.jpeg,.bmp");
            }

            // Handle image selection
            if (ImGuiFileDialog::Instance()->Display("ChooseImage")) {
                if (ImGuiFileDialog::Instance()->IsOk()) {
                    std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();

                    // Load image using stb_image
                    int nChannels;
                    unsigned char* data = stbi_load(filePath.c_str(), &imageWidth, &imageHeight, &nChannels, 4);
                    if (data) {
                        if (imageTexture) {
                           glDeleteTextures(1, &imageTexture);
                        }

                        glGenTextures(1, &imageTexture);
                        glBindTexture(GL_TEXTURE_2D, imageTexture);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                        stbi_image_free(data);
                        imageLoaded = true;
                    }
                }
                ImGuiFileDialog::Instance()->Close();
            }

            // Display image if loaded
            if (imageLoaded) {
                float maxWidth = ImGui::GetContentRegionAvail().x;
                float aspect = (float)imageHeight / (float)imageWidth;
                float imageDisplayWidth = maxWidth;
                float imageDisplayHeight = maxWidth * aspect;

                //ImGui::Image((void*)(intptr_t)imageTexture, ImVec2(imageDisplayWidth, imageDisplayHeight));
            }

            // Update pins
            Pins.erase(remove_if(Pins.begin(), Pins.end(), [this](const Pin& p) { return p.ParentNodeId == this->NodeId; }), Pins.end());
            Pins.insert(Pins.end(), outputPins.begin(), outputPins.end());
            DrawLinksAndHandleDrag(Pins);
        }
    };


    class OutputImageNode : public BaseNode {
    public:
        OutputImageNode() {
            NodeName = "Output Image";
            NodeId = NodeName + "num";
            NumOfInputPins = 1;
            NumOfOutputPins = 0;

            ImVec2 winPos = ImGui::GetWindowPos();
            float InitialLoc = 100.0f;

            ImVec2 localPos = ImVec2(0, InitialLoc);
            string PinName = "In";
            inputPins.push_back({ GetNextPinId(), PinName + "##0", ImVec2(winPos.x + localPos.x, winPos.y + localPos.y), true, NodeId });
        }

        void DrawContent() override {
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            ImVec2 winPos = ImGui::GetWindowPos();
            ImVec2 winSize = ImGui::GetWindowSize();
            float InitialLoc = 120.0f;

            // Draw input pin
            ImVec2 localPos = ImVec2(0, InitialLoc);
            inputPins[0].Pos = ImVec2(winPos.x + localPos.x, winPos.y + localPos.y);
            drawList->AddCircleFilled(inputPins[0].Pos, 5.0f, IM_COL32(255, 255, 255, 255));

            // Visual representation
            ImVec2 center = ImVec2(
                (winSize.x - ImGui::CalcTextSize("Display Output").x) * 0.5f,
                (winSize.y - ImGui::GetTextLineHeight()) * 0.5f
            );
            ImGui::SetCursorPos(center);
            ImGui::Text("Display Output");

            // Update pins
            Pins.erase(remove_if(Pins.begin(), Pins.end(), [this](const Pin& p) { return p.ParentNodeId == this->NodeId; }), Pins.end());
            Pins.insert(Pins.end(), inputPins.begin(), inputPins.end());
            DrawLinksAndHandleDrag(Pins);
        }
    };


    

    





    vector<std::unique_ptr<BaseNode>> nodes;
    // Our state
    //bool show_demo_window = true;
    //bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // SOME VARIABLES
    static float gridZoom = 1.0f; // Starting at 100% zoom
    ImVec2 NodeSpawnPos = ImVec2(50, 50);
    ImVec2 NodesSize = ImVec2(100, 200);
    /*bool ResizeNode = false;
    bool SelectedNodeOutline = false;*/



    // Main loop
    bool done = false;
    while (!done)
    {

        

        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Handle window being minimized or screen locked
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        
        


    //  1. MAKING A GRID [ WITH ZOOM IN/OUT ] AND PROPERTIES PANEL




        // Set up your layout
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize); // Full window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::Begin("Node Graph Editor", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
        
        // Let's define canvas width
        float canvasWidth = 1200.0f;
        ImVec2 fullSize = ImGui::GetContentRegionAvail();

        // Left-side child canvas for grid
        ImGui::BeginChild("GridCanvas", ImVec2(canvasWidth, fullSize.y), true , ImGuiWindowFlags_NoBringToFrontOnFocus);

        // ZOOM IN/OUT
        float wheel = ImGui::GetIO().MouseWheel;
        float ZoomStepSize = 0.1f;
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
            if (wheel != 0.0f) {
                gridZoom += wheel * ZoomStepSize;
                gridZoom = CLAMP(gridZoom, 0.5f, 2.0f);
            }
        }

        // === Draw grid ===
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();  // screen-space top-left
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        ImVec2 origin = canvasPos;

        float gridSpacing = 20.0f*gridZoom;
        ImU32 gridColor = IM_COL32(200, 200, 200, 40);

        for (float x = fmodf(origin.x, gridSpacing); x < canvasPos.x + canvasSize.x; x += gridSpacing)
            drawList->AddLine(ImVec2(x, canvasPos.y), ImVec2(x, canvasPos.y + canvasSize.y), gridColor);

        for (float y = fmodf(origin.y, gridSpacing); y < canvasPos.y + canvasSize.y; y += gridSpacing)
            drawList->AddLine(ImVec2(canvasPos.x, y), ImVec2(canvasPos.x + canvasSize.x, y), gridColor);

        ImVec2 gridMin = ImGui::GetWindowPos();                     // Top-left of grid
        ImVec2 gridSize = ImGui::GetWindowSize();                   // Size of grid
        ImVec2 gridMax(gridMin.x + gridSize.x, gridMin.y + gridSize.y);

        for (int i = 0; i < nodes.size(); ++i) {
            std::string uniqueName = nodes[i]->NodeName + "##" + std::to_string(i);
            nodes[i]->Draw(gridMin, gridMax, uniqueName);
        }

        ImGui::EndChild();


        // Right-side panel
        ImGui::SameLine();
        ImGui::BeginChild("RightPanel", ImVec2(0, fullSize.y), true, ImGuiWindowFlags_NoBringToFrontOnFocus); // 0 for width = take remaining space

        ImGui::Text("This is the right-side panel.");
        ImGui::Text("Use this panel for creating different nodes");

        if (ImGui::Button("Create Brightness Node")) {
            auto node = std::make_unique<BrightnessNode>();
            node->position = NodeSpawnPos;
            nodes.push_back(std::move(node));
        }
        else if (ImGui::Button("Create Input Node")) {
            auto node = std::make_unique<InputImageNode>();
            node->position = NodeSpawnPos;
            nodes.push_back(std::move(node));
        }
        else if (ImGui::Button("Create Output Node")) {
            auto node = std::make_unique<OutputImageNode>();
            node->position = NodeSpawnPos;
            nodes.push_back(std::move(node));
        }

        ImGui::EndChild();

        ImGui::End(); // End main window




 
        
        




        
















        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
