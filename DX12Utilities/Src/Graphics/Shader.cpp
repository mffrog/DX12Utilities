#include "Shader.h"

namespace Shader {
	bool IndexBuffer::Init(Graphics* graphics, size_t maxIndexCount) {
		size_t bufferSize = sizeof(uint32_t) * maxIndexCount;
		if (FAILED(graphics->GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&resource)
		))) {
			return false;
		}

		const D3D12_RANGE readRange = { 0,0 };
		if (FAILED(resource->Map(0, &readRange, (void**)&pResPtr))) {
			return false;
		}

		view.BufferLocation = resource->GetGPUVirtualAddress();
		view.Format = DXGI_FORMAT_R32_UINT;
		view.SizeInBytes = bufferSize;
		return true;
	}

	void IndexBuffer::UpdateData(void* pData, size_t size, size_t offset) {
		memcpy(pResPtr + offset, pData, size);
	}

	void InputLayout::AddElement(const char* semanticName, FormatType fType, size_t count, Classification classification, unsigned short inputSlot) {
		DXGI_FORMAT format;
		UINT elementSize = count;

		switch (fType)
		{
		case InputLayout::Float:
			elementSize *= sizeof(float);
			break;
		case InputLayout::Int:
			elementSize *= sizeof(int);
			break;
		case InputLayout::Uint:
			elementSize *= sizeof(UINT);
			break;
		default:
			break;
		}

		DXGI_FORMAT formats[FormatTypeSize][4] = {
			{ DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT },
		{ DXGI_FORMAT_R32_SINT, DXGI_FORMAT_R32G32_SINT, DXGI_FORMAT_R32G32B32_SINT, DXGI_FORMAT_R32G32B32A32_SINT },
		{ DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32B32_UINT, DXGI_FORMAT_R32G32B32A32_UINT },
		};
		format = formats[fType][count - 1];

		D3D12_INPUT_CLASSIFICATION ic;
		switch (classification)
		{
		case Classification_PerVertex:
			ic = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
			break;
		case Classification_PerInstance:
			ic = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
		default:
			break;
		}
		elements.push_back({ semanticName, 0U, format, 0U, offset, ic, 0U });
		offset += elementSize;
	}

} // namespace Shader
