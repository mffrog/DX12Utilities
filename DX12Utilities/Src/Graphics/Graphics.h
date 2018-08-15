#pragma once
#include <Windows.h>
#include "../d3dx12.h"
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
 

#include <vector>

class Graphics;

enum CommandType {
	CommandType_Direct,
	CommandType_Bundle,
};

class CommandAllocator {
public:
	bool Init(Graphics* graphics, CommandType type);
	CommandType GetType() const { return type; }
	bool Reset() {
		return SUCCEEDED(commandAllocator->Reset());
	}
	ID3D12CommandAllocator* GetAllocator() {
		return commandAllocator.Get();
	}
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> GetComPtrAllocator() {
		return commandAllocator;
	}
	bool operator!() const {
		return !commandAllocator;
	}
private:
	CommandType type;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
};

class GraphicsCommand {
public:
	bool Init(CommandAllocator* allocator);
	bool Reset(CommandAllocator* allocator);
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetCommandList() const  {
		return commandList;
	}
private:
	CommandType type;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
};

//Directxèâä˙âªÇ∆Ç©GPUÇ∆ÇÃòAåg
class Graphics {
public:
	static Graphics& GetInstance();
	bool Initialize(HWND hwnd,int width, int height);
	ID3D12Device* GetDevice() const {
		return device.Get();
	}
	Microsoft::WRL::ComPtr<ID3D12Device> GetComPtrDevice() {
		return device;
	}
	ID3D12CommandQueue* GetCommandQueue() const {
		return commandQueue.Get();
	}

	UINT GetCurrentBackBufferIndex() const {
		return swapChain->GetCurrentBackBufferIndex();
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> GetRenderTarget(size_t index) {
		return renderTargetList[index];
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(size_t index) const {
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),index, rtvDescriptorSize);
	}
	bool Swap() {
		return SUCCEEDED(swapChain->Present(1, 0));
	}
	void Finalize();
	bool GetWarp()const {
		return warp;
	}
private:
	Graphics() = default;
	Graphics(const Graphics&) = delete;
	Graphics& operator=(const Graphics&) = delete;

	static const int FrameBufferCount = 2;
	Microsoft::WRL::ComPtr<ID3D12Device> device;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;

	Microsoft::WRL::ComPtr<ID3D12Resource> renderTargetList[FrameBufferCount];
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	int rtvDescriptorSize;

	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;

	bool warp;
};

class SyncObject {
public:
	bool Init(Graphics* graphics) {
		ID3D12Device* device = graphics->GetDevice();
		if (FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))) {
			return false;
		}

		fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!fenceEvent) {
			return false;
		}
		masterFenceValue = 1;
		return true;
	}
	~SyncObject() {
		CloseHandle(fenceEvent);
	}

	bool WaitGpuProcess(ID3D12CommandQueue* queue, UINT64 fenceValue) {
		if (FAILED(queue->Signal(fence.Get(), fenceValue))) {
			return false;
		}
		if (fence->GetCompletedValue() < fenceValue) {
			if (FAILED(fence->SetEventOnCompletion(fenceValue, fenceEvent))) {
				return false;
			}
			WaitForSingleObject(fenceEvent, INFINITE);
		}		
		return true;
	}

	bool WaitGpuProcess(ID3D12CommandQueue* queue) {
		fenceValue = masterFenceValue;
		++masterFenceValue;
		return WaitGpuProcess(queue, fenceValue);
	}

private:
	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
	HANDLE fenceEvent;
	UINT64 fenceValue;
	UINT64 masterFenceValue;
};