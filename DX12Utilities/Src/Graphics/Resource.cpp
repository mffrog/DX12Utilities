#include "Resource.h"
namespace Resource {
	//------------------------------------------------------------------------------------------
	bool DescriptorList::Init(Graphics* graphics, Type type, size_t maxCount) {
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		switch (type) {
		case DescriptorList::RTV:
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			break;
		case DescriptorList::DSV:
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			break;
		case DescriptorList::Resource:
			desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		default:
			break;
		}
		desc.NumDescriptors = maxCount;
		device = graphics->GetComPtrDevice();
		if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)))) {
			return false;
		}
		perDescriptorSize = device->GetDescriptorHandleIncrementSize(desc.Type);
		cpuHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
		gpuHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
		this->maxCount = maxCount;
		index = 0;
		return true;
	}

	//------------------------------------------------------------------------------------------
	int ResourceDescriptorList::AddConstantDescriptor(D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress, size_t bufferSize) {
		if (index >= maxCount) {
			return -1;
		}
		D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
		desc.BufferLocation = gpuVirtualAddress;
		bufferSize += 0xff;
		bufferSize = bufferSize & (~0xff);
		desc.SizeInBytes = bufferSize;
		device->CreateConstantBufferView(&desc, GetCpuHandle(index));
		size_t offset = index;
		++index;
		return offset;
	}

	int ResourceDescriptorList::AddShaderResourceDescriptor(Microsoft::WRL::ComPtr<ID3D12Resource> resource, DXGI_FORMAT format) {
		if (index >= maxCount) {
			return -1;
		}
		D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.Format = format;
		desc.Texture2D.MipLevels = 1;
		desc.Texture2D.MostDetailedMip = 0;
		device->CreateShaderResourceView(resource.Get(), &desc, GetCpuHandle(index));
		size_t offset = index;
		++index;
		return offset;
	}

	//------------------------------------------------------------------------------------------
	int DepthStencilDescriptorList::CreateDepthStencilView(Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilBuffer, DXGI_FORMAT format) {
		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
		depthStencilDesc.Format = format;
		depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		device->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilDesc, CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuHandle, index, perDescriptorSize));
		int offset = static_cast<int>(index);
		++index;
		return offset;
	}

	//------------------------------------------------------------------------------------------
	bool DepthStencilFactory::CreateDefault(DepthStencilDescriptorList* heap, int width, int height, DepthStencilBuffer* buffer) {
		Microsoft::WRL::ComPtr<ID3D12Device> device = heap->GetDevice();
		D3D12_CLEAR_VALUE dsClearValue = {};
		dsClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		dsClearValue.DepthStencil.Depth = 1.0f;
		dsClearValue.DepthStencil.Stencil = 0;
		if (FAILED(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(
				DXGI_FORMAT_D32_FLOAT,
				width, height,
				1, 0, 1, 0,
				D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&dsClearValue,
			IID_PPV_ARGS(&buffer->resource)
		))) {
			return false;
		}
		int id = heap->CreateDepthStencilView(buffer->resource, DXGI_FORMAT_D32_FLOAT);
		if (id < 0) {
			return false;
		}
		buffer->cpuHandle = heap->GetCpuHandle(id);
		buffer->format = DXGI_FORMAT_D32_FLOAT;
		buffer->depthClearValue = 1.0f;
		buffer->stencilClearValue = 0;
		return true;
	}

	bool ConstantBuffer::Init(Graphics* graphics, size_t dataCount) {
		if (FAILED(graphics->GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(dataCount * 0x100),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&resource)
		))) {
			return false;
		}
		maxCount = dataCount;
		D3D12_RANGE range = { 0,0 };
		resource->Map(0, &range, (void**)&pResourcePtr);
		gpuVirtualAddress = resource->GetGPUVirtualAddress();
		return true;
	}

	int ConstantBuffer::AddData(size_t size, void* pData) {
		size_t alignmented = size + 0xff;
		size_t count = alignmented / 0x100;
		if ((count + index) > maxCount) {
			return -1;
		}
		size_t offset = index;
		index += count;
		if (pData) {
			memcpy(pResourcePtr + offset * 0x100, pData, size);
		}
		return offset;
	}
	bool RenderTarget::Init(Graphics * graphics, int width, int height, DXGI_FORMAT format, const float* clearColor) {
		D3D12_CLEAR_VALUE clear = {};
		clear.Format = format;
		for (int i = 0; i < 4; ++i) {
			this->clearColor[i] = clearColor[i];
			clear.Color[i] = clearColor[i];
		}
		if (FAILED(graphics->GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(
				format,
				width, height,
				1, 0, 1, 0,
				D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
			),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			&clear,
			IID_PPV_ARGS(&resource)
		))) {
			return false;
		}
		this->format = format;
		return true;
	}
	int RenderTargetDescriptorList::AddRenderTargetView(RenderTarget* renderTarget) {
		D3D12_RENDER_TARGET_VIEW_DESC desc = {};
		desc.Format = renderTarget->GetFormat();
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += perDescriptorSize * index;
		device->CreateRenderTargetView(renderTarget->GetResource().Get(), &desc, handle);
		int offset = index;
		++index;
		return offset;
	}
} // namespace Resource