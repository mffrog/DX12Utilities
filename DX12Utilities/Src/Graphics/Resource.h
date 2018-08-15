#pragma once
#include "Graphics.h"
#include <stdint.h>

namespace Resource {
	//------------------------------------------------------------------------------------------
	class DescriptorList {
	public:
		enum Type {
			RTV,
			DSV,
			Resource,
		};
		bool Init(Graphics* graphics, Type type, size_t maxCount);

		ID3D12DescriptorHeap* GetHeap() const {
			return descriptorHeap.Get();
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(size_t index) const {
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, index, perDescriptorSize);
		}
		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(size_t index) const {
			return CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuHandle, index, perDescriptorSize);
		}

		Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() {
			return device;
		}
	protected:
		Microsoft::WRL::ComPtr<ID3D12Device> device;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
		size_t perDescriptorSize;
		size_t index;
		size_t maxCount;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	};

	//------------------------------------------------------------------------------------------
	class ResourceDescriptorList : public DescriptorList {
	public:
		int AddConstantDescriptor(D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress, size_t bufferSize);
		int AddShaderResourceDescriptor(Microsoft::WRL::ComPtr<ID3D12Resource> resource, DXGI_FORMAT format);
	};


	//------------------------------------------------------------------------------------------
	class DepthStencilDescriptorList : public DescriptorList {
	public:
		int CreateDepthStencilView(Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilBuffer, DXGI_FORMAT format);
	};

	//------------------------------------------------------------------------------------------
	class DepthStencilFactory;
	class DepthStencilBuffer {
		friend DepthStencilFactory;
	public:
		bool operator!() {
			return !resource;
		}
		D3D12_CPU_DESCRIPTOR_HANDLE GetHandle() {
			return cpuHandle;
		}
		DXGI_FORMAT GetFormat() const {
			return format;
		}
		float GetDepthClearValue() const { return depthClearValue; }
		UINT GetStencilClearValue() const { return stencilClearValue; }
	private:
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		DXGI_FORMAT format;
		float depthClearValue;
		UINT stencilClearValue;
	};


	//------------------------------------------------------------------------------------------
	class DepthStencilFactory {
	public:
		bool CreateDefault(DepthStencilDescriptorList* heap, int width, int height, DepthStencilBuffer* buffer);
	};


	//------------------------------------------------------------------------------------------
	class ConstantBuffer {
	public:
		bool Init(Graphics* graphics, size_t dataCount);
		int AddData(size_t size, void* pData = nullptr);

		void Update(int index, void* pData, size_t size) {
			memcpy(pResourcePtr + index * 0x100, pData, size);
		}

		D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress(int index) {
			return gpuVirtualAddress + index * 0x100;
		}
	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		size_t bufferSize;
		size_t maxCount;
		size_t index = 0;
		uint8_t* pResourcePtr;
		D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress;
	};

	//------------------------------------------------------------------------------------------
	class RenderTarget {
	public:
		bool Init(Graphics* graphics, int width, int height, DXGI_FORMAT format, const float* clearColor);
		Microsoft::WRL::ComPtr<ID3D12Resource> GetResource() { return resource; }
		const float* GetClearColor() const { return clearColor; }
		DXGI_FORMAT GetFormat() const { return format; }
	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		DXGI_FORMAT format;
		float clearColor[4];
	};
	
	class RenderTargetDescriptorList : public DescriptorList {
	public:
		int AddRenderTargetView(RenderTarget*);
	};


} // namespace Resource
