#include "me/rhi/CommandContext.h"

namespace me::rhi {

std::unique_ptr<CommandContext> CommandContext::Create(ID3D12Device* device) {
    auto self = std::unique_ptr<CommandContext>(new CommandContext());
    for (auto& alloc : self->m_allocators) {
        if (FAILED(device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc)))) {
            return nullptr;
        }
    }
    if (FAILED(device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            self->m_allocators[0].Get(), nullptr, IID_PPV_ARGS(&self->m_list)))) {
        return nullptr;
    }
    self->m_list->Close(); // 创建即处于录制态,先关闭以便 Begin 统一 Reset
    return self;
}

ID3D12GraphicsCommandList* CommandContext::Begin() {
    auto& alloc = m_allocators[m_ring.Current()];
    ME_HR_CHECK(alloc->Reset());
    ME_HR_CHECK(m_list->Reset(alloc.Get(), nullptr));
    return m_list.Get();
}

void CommandContext::End() {
    ME_HR_CHECK(m_list->Close());
}

void CommandContext::Execute(ID3D12CommandQueue* queue) {
    ID3D12CommandList* lists[] = {m_list.Get()};
    queue->ExecuteCommandLists(1, lists);
}

} // namespace me::rhi
