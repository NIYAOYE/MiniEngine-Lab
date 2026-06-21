#include <cstdio> // std::snprintf(Hierarchy 面板实体标签)
#include <memory>
#include <vector>

#include "me/platform/Window.h"
#include "me/platform/Input.h"
#include "me/assets/ImageData.h"
#include "me/assets/TiledMapLoader.h"
#include "me/assets/TileLayout.h"
#include "me/core/Log.h"
#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"
#include "me/core/Vector4.h"
#include "me/core/Rect.h"

#include "me/rhi/GpuDevice.h"
#include "me/rhi/SwapChain.h"
#include "me/rhi/Fence.h"
#include "me/rhi/CommandContext.h"
#include "me/rhi/DescriptorHeap.h"
#include "me/rhi/GpuTexture.h"
#include "me/render/SpriteBatch.h"
#include "me/render/SpriteDesc.h"
#include "me/render/OrthographicCamera.h"
#include "me/render/Tileset.h"
#include "me/render/TileMapRenderer.h"

#include "me/scene/Scene.h"
#include "me/scene/Components.h"
#include "me/scene/RenderSystem.h"
#include "me/scene/TransformSystem.h"
#include "me/scene/CameraView.h"

#include <d3d12.h>

// M7: ToolAPI + Editor stack
#include "me/command/CommandStack.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/tools/BuiltinTools.h"
#include "me/editor/EditorController.h"

// M7: Dear ImGui (DX12 backend + Win32 backend)
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include <windows.h>

using namespace me;

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr float kCameraSpeed = 6.0f;     // 每帧 player 平移步长(像素)
constexpr float kZoomStep = 0.02f;       // 每帧缩放步长
constexpr float kMinZoom = 0.25f;
constexpr float kMaxZoom = 4.0f;
constexpr float kSpriteSize = 16.0f;     // prop/player 精灵世界像素尺寸

void Transition(ID3D12GraphicsCommandList* cmd, ID3D12Resource* res,
                D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = res;
    b.Transition.StateBefore = from;
    b.Transition.StateAfter = to;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd->ResourceBarrier(1, &b);
}

float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ImGui 提供的 Win32 消息处理器(由其后端导出)。
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

/** @brief 适配 platform::Window::WndProcHook 裸类型签名到 ImGui Win32 处理器。
 *         返回 true 表示该消息已被 ImGui 消费,WndProc 不再继续默认处理。 */
bool ImGuiWndProcHook(void* hwnd, unsigned int msg,
                      unsigned long long wparam, long long lparam) {
    return ImGui_ImplWin32_WndProcHandler(reinterpret_cast<HWND>(hwnd), msg,
                                          static_cast<WPARAM>(wparam),
                                          static_cast<LPARAM>(lparam)) != 0;
}

/// M7 ImGui 帧并发数(与交换链双缓冲对齐,每帧录制时 ImGui 需知道最大并发帧数)。
constexpr int kImGuiFramesInFlight = 2;

} // namespace

int main() {
    platform::WindowDesc wd;
    wd.width = kWindowWidth;
    wd.height = kWindowHeight;
    wd.title = "MiniEngine M4 \xe2\x80\x94 Scene"; // UTF-8: "MiniEngine M4 — Scene"
    auto window = platform::Window::Create(wd);
    if (!window) { ME_LOG_ERROR("窗口创建失败"); return 1; }

    platform::InputState input;
    window->SetInput(&input);

    auto device = rhi::GpuDevice::Create(/*useWarp=*/false);
    if (!device) device = rhi::GpuDevice::Create(/*useWarp=*/true);
    if (!device) { ME_LOG_ERROR("DX12 设备创建失败"); return 1; }

    auto swapChain = rhi::SwapChain::Create(*device, window->NativeHandle(),
                                            kWindowWidth, kWindowHeight);
    auto ctx = rhi::CommandContext::Create(device->Device());
    auto fence = rhi::Fence::Create(device->Device());
    auto srvHeap = rhi::DescriptorHeap::Create(
        device->Device(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8, true);
    if (!swapChain || !ctx || !fence || !srvHeap) {
        ME_LOG_ERROR("RHI 初始化失败"); return 1;
    }

    auto batch = render::SpriteBatch::Create(*device);
    if (!batch) { ME_LOG_ERROR("批渲染器创建失败"); return 1; }

    // 数据驱动:从 Tiled JSON 加载地图 + 其 tileset 贴图。
    auto mapData = assets::LoadTiledMap(std::string(ME_ASSET_DIR) + "/maps/demo.tmj");
    if (!mapData) { ME_LOG_ERROR("加载地图失败"); return 1; }
    auto tileImage = assets::LoadImageRGBA8(mapData->tileset.imagePath);
    if (!tileImage) { ME_LOG_ERROR("加载 tileset 贴图失败"); return 1; }
    auto tileSrv = srvHeap->Allocate();
    auto tileTex = rhi::GpuTexture::Create(
        device->Device(), device->Queue(), *fence,
        uint32_t(tileImage->width), uint32_t(tileImage->height),
        tileImage->pixels.data(), tileSrv);
    if (!tileTex) { ME_LOG_ERROR("创建 tileset 纹理失败"); return 1; }
    render::Tileset tileset(tileTex.get(), mapData->tileset);
    render::TileMapRenderer tileRenderer;

    // textureId → GpuTexture* 解析表(demo 仅一张图集:id 0 = tileset 纹理)。
    constexpr std::uint32_t kTilesetTextureId = 0;
    std::vector<const rhi::GpuTexture*> textureTable;
    textureTable.push_back(tileTex.get()); // index 0

    namespace sc = me::scene;
    sc::Scene scene;

    // 地面:瓦片地图实体(数据驱动,经 TileMapRenderer 绘制)。
    const sc::Entity ground = scene.CreateEntity();
    {
        sc::TileMapComponent tm;
        tm.map = &(*mapData);
        tm.textureId = kTilesetTextureId;
        scene.AddComponent<sc::TileMapComponent>(ground, tm);
    }

    // srcRect 辅助:从 tileset 局部 id 取 UV。
    auto spriteSrc = [&](int localId) {
        return me::assets::SrcRectForLocalId(mapData->tileset, localId);
    };

    // 添加 prop 实体:Sprite 挂在 tileset 图集上,取不同 localId 作为不同物体。
    // localId 范围:4 列 × 3 行 = 0..11。
    auto addProp = [&](float x, float y, int localId, int layer) -> sc::Entity {
        const sc::Entity e = scene.CreateEntity();
        me::Transform2D t;
        t.position = me::Vector2{x, y};
        scene.SetLocalTransform(e, t);
        sc::SpriteComponent sp;
        sp.textureId = kTilesetTextureId;
        sp.srcRect = spriteSrc(localId);
        sp.size = me::Vector2{kSpriteSize, kSpriteSize};
        sp.sortLayer = layer;
        scene.AddComponent<sc::SpriteComponent>(e, sp);
        return e;
    };

    // 同层不同 Y 的两个道具,演示 2.5D Y 排序叠压。
    addProp(80.0f, 96.0f, /*localId*/6, /*layer*/1);
    addProp(96.0f, 64.0f, /*localId*/9, /*layer*/1);
    // player 实体:WASD 控制其局部变换;相机为其子节点自动跟随。
    const sc::Entity player = addProp(96.0f, 80.0f, /*localId*/4, /*layer*/1);

    // 相机实体:挂 CameraComponent,作为 player 的子节点(跟随)。
    const sc::Entity cameraEntity = scene.CreateEntity();
    {
        sc::CameraComponent cc;
        cc.zoom = 1.0f;
        cc.viewportSize = me::Vector2{float(kWindowWidth), float(kWindowHeight)};
        scene.AddComponent<sc::CameraComponent>(cameraEntity, cc);
        scene.SetParent(cameraEntity, player);
        scene.SetActiveCamera(cameraEntity);
    }

    // 当前缩放值,由 Q/E 调整后写入 CameraComponent。
    float zoom = 1.0f;

    // —— M7: ToolAPI + 编辑器栈 ——
    me::toolapi::ToolRegistry registry;
    me::toolapi::RegisterBuiltinTools(registry);
    me::command::CommandStack cmdStack;
    me::toolapi::ToolInvocationLog toolLog;
    me::toolapi::ToolContext toolCtx{scene, cmdStack, toolLog};
    me::editor::EditorController editor(registry, toolCtx);
    editor.RefreshHierarchy();

    // —— ImGui 初始化 ——
    // tileset 纹理已占用 srvHeap 槽 0;ImGui 字体 SRV 顺序取下一槽(bump-allocate)。
    auto fontSrv = srvHeap->Allocate(); // slot 1:CPU+GPU 句柄均有效(shader-visible 堆)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(window->NativeHandle());
    ImGui_ImplDX12_Init(device->Device(), kImGuiFramesInFlight,
                        DXGI_FORMAT_R8G8B8A8_UNORM, srvHeap->Heap(),
                        fontSrv.cpu, fontSrv.gpu);
    // 注册 WndProc 钩子:ImGui 需先于游戏逻辑处理输入消息(鼠标/键盘)。
    window->SetWndProcHook(&ImGuiWndProcHook);
    // 编辑器可见状态(Space 键切换,不与 WASD/QE/Escape 冲突)。
    bool editorVisible = true;

    while (!window->ShouldClose()) {
        input.NewFrame();
        window->PumpMessages();
        if (input.WasPressed(platform::KeyCode::Escape)) break;

        // —— M7: ImGui 新帧 + 编辑器面板(所有变更经 EditorController → Tool API)——
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        // Space 键切换编辑器可见(Escape 已用于退出;WASD/QE 用于移动/缩放)。
        if (input.WasPressed(platform::KeyCode::Space)) editorVisible = !editorVisible;
        if (editorVisible) {
            // ── Hierarchy 面板 ──────────────────────────────────────────────
            ImGui::Begin("Hierarchy");
            if (ImGui::Button("Create")) { editor.CreateEntity(); }
            ImGui::SameLine();
            if (ImGui::Button("Destroy") && editor.HasSelection()) {
                editor.DestroySelected();
            }
            ImGui::Separator();
            for (const auto& row : editor.Hierarchy()) {
                char label[32];
                std::snprintf(label, sizeof(label), "Entity #%llu",
                              static_cast<unsigned long long>(row.id)); // EntityId=uint64
                if (ImGui::Selectable(label, editor.Selected() == row.id)) {
                    editor.Select(row.id);
                    editor.InspectSelected();
                }
            }
            ImGui::End();

            // ── Inspector 面板 ───────────────────────────────────────────────
            ImGui::Begin("Inspector");
            if (editor.HasInspected()) {
                me::Transform2D t = editor.Inspected().localTransform;
                float pos[2] = {t.position.x, t.position.y};
                float scl[2] = {t.scale.x, t.scale.y};
                bool changed = false;
                changed |= ImGui::DragFloat2("position", pos, 1.0f);
                changed |= ImGui::DragFloat("rotation", &t.rotation, 0.01f);
                changed |= ImGui::DragFloat2("scale", scl, 0.01f);
                if (changed) {
                    t.position = me::Vector2{pos[0], pos[1]};
                    t.scale    = me::Vector2{scl[0], scl[1]};
                    editor.ApplyTransform(t);
                }
            } else {
                ImGui::TextUnformatted("(no selection)");
            }
            ImGui::Separator();
            if (ImGui::Button("Undo") && editor.CanUndo()) editor.Undo();
            ImGui::SameLine();
            if (ImGui::Button("Redo") && editor.CanRedo()) editor.Redo();
            ImGui::End();

            // ── Stats 面板(运行时遥测;非 Tool 数据,见 spec §1.2 边界)───────
            ImGui::Begin("Stats");
            ImGui::Text("entities: %d", static_cast<int>(editor.Hierarchy().size()));
            ImGui::Text("frame: %.2f ms", 1000.0f / ImGui::GetIO().Framerate);
            ImGui::End();

            // ── Log 面板 ────────────────────────────────────────────────────
            ImGui::Begin("Log");
            if (ImGui::Button("Refresh")) editor.RefreshLog();
            ImGui::Separator();
            for (const auto& lr : editor.Log()) {
                ImGui::Text("#%llu %s [%s]",
                            static_cast<unsigned long long>(lr.invocationId),
                            lr.toolName.c_str(), lr.code.c_str());
            }
            ImGui::End();

            // ── 错误条(Editor 调用失败时显示;绝不静默)────────────────────
            if (editor.HasError()) {
                ImGui::Begin("Editor Error");
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                                   editor.LastError().c_str());
                if (ImGui::Button("Clear")) editor.ClearError();
                ImGui::End();
            }
        }

        // 1) 运行时逻辑:直调 Scene API 改实体(高频路径,不经 Tool)。
        //    WASD 移动 player 局部变换;相机为其子节点,自动跟随。
        me::Vector2 moveDelta{0.0f, 0.0f};
        if (input.IsDown(platform::KeyCode::A)) moveDelta.x -= kCameraSpeed;
        if (input.IsDown(platform::KeyCode::D)) moveDelta.x += kCameraSpeed;
        if (input.IsDown(platform::KeyCode::W)) moveDelta.y += kCameraSpeed;
        if (input.IsDown(platform::KeyCode::S)) moveDelta.y -= kCameraSpeed;

        me::Transform2D pt = scene.LocalTransform(player);
        pt.position.x += moveDelta.x;
        pt.position.y += moveDelta.y;
        scene.SetLocalTransform(player, pt);

        // Q/E 调整缩放,写入 CameraComponent。
        if (input.IsDown(platform::KeyCode::Q)) zoom -= kZoomStep;
        if (input.IsDown(platform::KeyCode::E)) zoom += kZoomStep;
        zoom = Clamp(zoom, kMinZoom, kMaxZoom);
        if (auto* cc = scene.GetComponent<sc::CameraComponent>(cameraEntity)) {
            cc->zoom = zoom;
        }

        // 2) 系统:解析世界矩阵 + 活动相机 + 产出 RenderView。
        sc::TransformSystem::UpdateWorldTransforms(scene);
        const auto camView = sc::RenderSystem::ResolveActiveCamera(scene);
        render::OrthographicCamera camera;
        if (camView) {
            camera.SetViewportSize(camView->viewportSize.x, camView->viewportSize.y);
            camera.SetPosition(camView->center);
            camera.SetZoom(camView->zoom);
        }
        const sc::RenderView renderView = sc::RenderSystem::BuildRenderView(scene);

        // 3) GPU 渲染骨架(清屏 + 视口 + 描述符堆不变)。
        auto* cmd = ctx->Begin();
        ID3D12Resource* back = swapChain->CurrentBackBuffer();
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapChain->CurrentRtv();
        Transition(cmd, back, D3D12_RESOURCE_STATE_PRESENT,
                   D3D12_RESOURCE_STATE_RENDER_TARGET);

        D3D12_VIEWPORT vp = {0, 0, float(kWindowWidth), float(kWindowHeight), 0.0f, 1.0f};
        D3D12_RECT scissor = {0, 0, kWindowWidth, kWindowHeight};
        cmd->RSSetViewports(1, &vp);
        cmd->RSSetScissorRects(1, &scissor);
        cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        const float clearColor[4] = {0.10f, 0.12f, 0.16f, 1.0f};
        cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

        ID3D12DescriptorHeap* heaps[] = {srvHeap->Heap()};
        cmd->SetDescriptorHeaps(1, heaps);

        // 4) 渲染:地面瓦片 + RenderView 精灵,同 Begin/End 同图集 → 1 drawcall。
        batch->Begin(camera.ViewProj());
        // 地面瓦片(TileMapComponent → TileMapRenderer)。
        if (auto* tm = scene.GetComponent<sc::TileMapComponent>(ground)) {
            tileRenderer.Render(*batch, camera, *tm->map, tileset);
        }
        // Scene 产出的精灵(RenderView → SpriteDesc 桥接:textureId 解析为 GpuTexture*)。
        for (const sc::RenderItem& it : renderView) {
            if (it.textureId >= static_cast<std::uint32_t>(textureTable.size())) {
                ME_LOG_ERROR("RenderItem textureId 越界,跳过该精灵");
                continue;
            }
            render::SpriteDesc d;
            d.texture = textureTable[it.textureId];
            d.srcRect = it.srcRect;
            d.dstRect = it.dstRect;
            d.color = it.color;
            d.rotation = it.rotation;
            batch->Submit(d);
        }
        batch->End(cmd);

        // M7: ImGui 绘制录入同一命令列表(srvHeap 已 SetDescriptorHeaps;
        //     ImGui 字体 SRV 在同一堆内,无需额外 Set)。必须在 PRESENT 转换之前。
        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);

        Transition(cmd, back, D3D12_RESOURCE_STATE_RENDER_TARGET,
                   D3D12_RESOURCE_STATE_PRESENT);
        ctx->End();
        ctx->Execute(device->Queue());
        swapChain->Present();
        fence->Flush(device->Queue()); // M4 仍每帧全同步(帧并行后续里程碑)
        ctx->AdvanceFrame();
    }

    fence->Flush(device->Queue());

    // M7: ImGui 清理(GPU 空闲后、资源销毁前)。
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    return 0;
}
