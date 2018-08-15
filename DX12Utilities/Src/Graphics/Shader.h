#pragma once
#include "Graphics.h"
#include <stdint.h>
#include <vector>

namespace Shader {
	class Shader {
	public:
		bool LoadShader(const wchar_t* filename, const char* target, const char* entryPoint) {
			Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
			if (FAILED(D3DCompileFromFile(filename, nullptr, nullptr, entryPoint, target, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &blob, &errorBlob))) {
				if (errorBlob) {
					OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
				}
				return false;
			}
			return true;
		}
		Microsoft::WRL::ComPtr<ID3DBlob> Get() {
			return blob;
		}
	private:
		Microsoft::WRL::ComPtr<ID3DBlob> blob;
	};
	template<typename T>
	class VertexBuffer {
		using Vertex = T;
	public:
		bool Init(Graphics* graphics, size_t maxVertexCount) {
			size_t bufferSize = maxVertexCount * sizeof(Vertex);
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
			D3D12_RANGE range = { 0,0 };
			if (FAILED(resource->Map(0, &range, (void**)&pResPtr))) {
				return false;
			}

			view.BufferLocation = resource->GetGPUVirtualAddress();
			view.StrideInBytes = sizeof(Vertex);
			view.SizeInBytes = bufferSize;
			return true;
		}

		void Update(void* pData, size_t size, size_t offset = 0) {
			memcpy(pResPtr + offset, pData, size);
		}

		D3D12_VERTEX_BUFFER_VIEW GetView() const {
			return view;
		}
	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_VERTEX_BUFFER_VIEW view;
		uint8_t* pResPtr = nullptr;
	};

	using IndexType = uint32_t;
	class IndexBuffer {
	public:
		bool Init(Graphics* graphics, size_t maxIndexCount);

		void UpdateData(void* pData, size_t size, size_t offset = 0);
		D3D12_INDEX_BUFFER_VIEW GetView() const {
			return view;
		}
	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		D3D12_INDEX_BUFFER_VIEW view;
		uint8_t* pResPtr = nullptr;
	};


	struct InputLayout {
		enum FormatType {
			Float,
			Int,
			Uint,
			FormatTypeSize,
		};
		enum Classification {
			Classification_PerVertex,
			Classification_PerInstance,
		};
		InputLayout() = default;
		InputLayout(int num) {
			elements.reserve(num);
		}
		void AddElement(const char* semanticName, FormatType fType, size_t count, Classification classification, unsigned short inputSlot = 0);

		const D3D12_INPUT_ELEMENT_DESC* GetElements() const {
			return elements.data();
		}

		size_t GetElementsNum() const {
			return elements.size();
		}

		std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
		//インプットスロット毎にオフセット持つべき
		UINT offset = 0;
	};

	class RootSignatureFactory {
	public:
		RootSignatureFactory() {}
		void AddConstants(size_t count, size_t bindIndex) {
			constants.push_back({ count, bindIndex });
		}
		void AddParameterBlock(size_t blockCount, size_t bindIndex) {
			descRanges.push_back(CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, blockCount, bindIndex));
		}
		void AddResourceBlock(size_t blockCount, size_t bindIndex) {
			descRanges.push_back(CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, blockCount, bindIndex));
		}
		void AddStaticSampler(size_t bindIndex) {
			staticSamplers.push_back(CD3DX12_STATIC_SAMPLER_DESC(bindIndex));
		}
		bool Create(Graphics* graphics, Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature) {
			size_t rangeSize = descRanges.size();
			std::vector<CD3DX12_ROOT_PARAMETER> rootParameters(rangeSize + constants.size());
			for (size_t i = 0; i < rangeSize; ++i) {
				rootParameters[i].InitAsDescriptorTable(1, &descRanges[i]);
			}
			for (size_t i = 0; i < constants.size(); ++i) {
				rootParameters[rangeSize + i].InitAsConstants(constants[i].first, constants[i].second);
			}

			D3D12_ROOT_SIGNATURE_DESC rsDesc = {
				rootParameters.size(),
				rootParameters.data(),
				staticSamplers.size(),
				staticSamplers.data(),
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
			};

			Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
			if (FAILED(D3D12SerializeRootSignature(
				&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signatureBlob, nullptr))) {
				return false;
			}
			if (FAILED(graphics->GetDevice()->CreateRootSignature(
				0,
				signatureBlob->GetBufferPointer(),
				signatureBlob->GetBufferSize(),
				IID_PPV_ARGS(&rootSignature)))) {
				return false;
			}
			return true;
		}
	private:
		std::vector<D3D12_DESCRIPTOR_RANGE> descRanges;
		std::vector<CD3DX12_STATIC_SAMPLER_DESC> staticSamplers;
		//first : valueNum, second : register
		std::vector<std::pair<size_t, size_t>> constants;
	};

	class PipelineStateFactory {
	public:
		PipelineStateFactory() { desc = {}; }
		bool Create(Graphics* graphics, Microsoft::WRL::ComPtr<ID3D12PipelineState>& p) {
			desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			desc.SampleMask = 0xffffffff;
			desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			desc.SampleDesc.Count = 1;
			if (graphics->GetWarp()) {
				desc.Flags = D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG;
			}
			if (FAILED(graphics->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&p)))) {
				return false;
			}
			return true;
		}

		void SetRootSignature(ID3D12RootSignature* rootSignature) {
			desc.pRootSignature = rootSignature;
		}
		void SetVertexShader(Shader* vs) {
			desc.VS = {
				vs->Get()->GetBufferPointer(),
				vs->Get()->GetBufferSize()
			};
		}
		void SetPixelShader(Shader* ps) {
			desc.PS = {
				ps->Get()->GetBufferPointer(),
				ps->Get()->GetBufferSize()
			};
		}
		void SetLayout(const InputLayout& layout) {
			desc.InputLayout.pInputElementDescs = layout.GetElements();
			desc.InputLayout.NumElements = layout.GetElementsNum();
		}
		void SetRenderTargets(size_t count, DXGI_FORMAT* formats) {
			desc.NumRenderTargets = count;
			for (size_t i = 0; i < count; ++i) {
				desc.RTVFormats[i] = formats[i];
			}
		}
		void UseDepthStencil(DXGI_FORMAT format) {
			desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			desc.DSVFormat = format;
		}
	private:
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
	};

	struct GraphicsPipeline {
		void Use(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) {
			commandList->SetGraphicsRootSignature(rootSignature.Get());
			commandList->SetPipelineState(pso.Get());
		}

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	};
} // namespace Shader