#include <Windows.h>
#include "Window/Window.h"
#include "Graphics/Graphics.h"
#include "Graphics/Shader.h"
#include "Graphics/Resource.h"
#include "Math/Vector/Vector2.h"
#include "Math/Vector/Vector3.h"
#include "Math/Vector/Vector4.h"
#include "Math/Matrix/Matrix4x4.h"
struct FrameComponent {
	CommandAllocator allocator;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
};

template<int N>
struct FrameComponentManager {
	void SetCurrent(int c) { current = c; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() {
		return components[current].rtvHandle;
	}
	CommandAllocator* GetCommandAllocator() {
		return &components[current].allocator;
	}
	FrameComponent components[N];
	int current = 0;
};

using Mat = mff::Matrix4x4<float>;
using Vec2 = mff::Vector2<float>;
using Vec4 = mff::Vector4<float>;
const int FrameBufferCount = 2;
FrameComponentManager<FrameBufferCount> frameComponent;
//FrameComponent frameComponents[FrameBufferCount];
GraphicsCommand graphicsCommand;
Resource::DepthStencilDescriptorList depthStencilList;
Resource::DepthStencilBuffer dsBuffer;
Resource::ResourceDescriptorList resourceDescriptorList;
Resource::ConstantBuffer constantBuffer;
int constantId[2] = { -1,-1 };
int cDescriptorId[2] = { -1,-1 };
Shader::GraphicsPipeline pipeline;
SyncObject sync;


int windowWidth = 800;
int windowHeight = 600;
const wchar_t* windowTitle = L"title";

struct Vertex {
	mff::Vector3<float> position;
	mff::Vector4<float> color;
};

Vertex vertices[] = {
	{ { 0.0f, 0.5f,0.0f },{ 1.0f,0.0f,0.0f,1.0f }, },
	{ { 0.5f,-0.5f,0.0f },{ 1.0f,1.0f,0.0f,1.0f }, },
	{ {-0.5f,-0.5f,0.0f },{ 1.0f,0.0f,1.0f,1.0f }, },
};

struct PostVertex {
	Vec4 position;
	Vec2 texcoord;
};

PostVertex postVertex[] = {
	{ { -0.8f, 0.8f,0.0f,1.0f }, {0.0f,0.0f} },
	{ {  0.8f, 0.8f,0.0f,1.0f }, {1.0f,0.0f} },
	{ {  0.8f,-0.8f,0.0f,1.0f }, {1.0f,1.0f} },
	{ { -0.8f,-0.8f,0.0f,1.0f }, {0.0f,1.0f} },
};

uint32_t postIndices[] = {
	0,1,2,2,3,0,
};

Shader::VertexBuffer<Vertex> vertexBuffer;
Shader::IndexBuffer indexBuffer;

Shader::VertexBuffer<PostVertex> postVertexBuffer;
Shader::IndexBuffer postIndexBuffer;
Shader::GraphicsPipeline postPipeline;

Resource::RenderTargetDescriptorList rtDescriptorList;
struct RenderTarget {
	Resource::RenderTarget renderTarget;
	int rtDescriptorId;
	int resourceDescriptorId;
};
Resource::RenderTarget preRenderTarget;
int preRenderTargetDescriptorId;
int preRTResourceDescriptorId;
RenderTarget posRenderTarget;


LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
bool InitGraphics(HWND hwnd) {
	Graphics& graphics = Graphics::GetInstance();
	if (!graphics.Initialize(hwnd, windowWidth, windowHeight)) {
		return 1;
	}

	int currentFrame = graphics.GetCurrentBackBufferIndex();
	frameComponent.SetCurrent(currentFrame);
	for (int i = 0; i < FrameBufferCount; ++i) {
		if (!frameComponent.components[i].allocator.Init(&graphics, CommandType::CommandType_Direct)) {
			return false;
		}
		frameComponent.components[i].rtvHandle = graphics.GetRTVHandle(i);
	}

	if (!graphicsCommand.Init(frameComponent.GetCommandAllocator())) {
		return false;
	}

	if (!depthStencilList.Init(&graphics, Resource::DescriptorList::DSV, 1)) {
		return false;
	}

	Resource::DepthStencilFactory dsFactory;
	if (!dsFactory.CreateDefault(&depthStencilList, windowWidth, windowHeight, &dsBuffer)) {
		return false;
	}

	if (!resourceDescriptorList.Init(&graphics, Resource::DescriptorList::Resource, 1024)) {
		return false;
	}
	if (!constantBuffer.Init(&graphics, 1024)) {
		return false;
	}

	for (int i = 0; i < 2; ++i) {
		size_t cSize = sizeof(float) * 16;
		constantId[i] = constantBuffer.AddData(cSize);
		if (constantId[i] < 0) {
			return false;
		}
		cDescriptorId[i] = resourceDescriptorList.AddConstantDescriptor(constantBuffer.GetGpuVirtualAddress(constantId[i]), cSize);
		if (cDescriptorId < 0) {
			return false;
		}
	}
	Mat mat = Mat(
		Vec4(1.0f, 0.0f, 0.0f, 0.5f),
		Vec4(0.0f, 1.0f, 0.0f, 0.5f),
		Vec4(0.0f, 0.0f, 1.0f, 0.0f),
		Vec4(0.0f, 0.0f, 0.0f, 1.0f));
	constantBuffer.Update(constantId[0], &mat, sizeof(Mat));

	Shader::InputLayout layout;
	layout.AddElement("POSITION", Shader::InputLayout::Float, 3);
	layout.AddElement("COLOR", Shader::InputLayout::Float, 4);

	Shader::RootSignatureFactory rsFactory;
	rsFactory.AddParameterBlock(1, 0);
	if (!rsFactory.Create(&graphics, pipeline.rootSignature)) {
		return false;
	}
	Shader::PipelineStateFactory psFactory;
	psFactory.SetLayout(layout);
	psFactory.SetRootSignature(pipeline.rootSignature.Get());
	DXGI_FORMAT format[] = {
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
	};
	psFactory.SetRenderTargets(_countof(format), format);
	psFactory.UseDepthStencil(dsBuffer.GetFormat());
	Shader::Shader vs;
	if (!vs.LoadShader(L"Res/VertexShader.hlsl", "vs_5_0", "main")) {
		return false;
	}
	Shader::Shader ps;
	if (!ps.LoadShader(L"Res/PixelShader.hlsl", "ps_5_0", "main")) {
		return false;
	}

	psFactory.SetVertexShader(&vs);
	psFactory.SetPixelShader(&ps);
	if (!psFactory.Create(&graphics, pipeline.pso)) {
		return false;
	}

	if (!sync.Init(&graphics)) {
		return false;
	}

	if (!vertexBuffer.Init(&graphics, 256)) {
		return false;
	}
	if (!indexBuffer.Init(&graphics, 256 * 3)) {
		return false;
	}

	vertexBuffer.Update(vertices, sizeof(Vertex) * (sizeof(vertices) / sizeof(vertices[0])));

	uint32_t indices[] = { 0,1,2 };
	indexBuffer.UpdateData(indices, sizeof(uint32_t) * (sizeof(indices) / sizeof(indices[0])));

	//ポストプロセス描画用
	{
		if (!rtDescriptorList.Init(&graphics, Resource::DescriptorList::RTV, 2)) {
			return false;
		}
		float clearColor[4] = {0.1f,0.3f,0.5f,1.0f};
		if (!preRenderTarget.Init(&graphics, windowWidth, windowHeight, DXGI_FORMAT_R8G8B8A8_UNORM, clearColor)) {
			return false;
		}
		float posClearColor[4] = {0.0f,0.0f,0.0f,1.0f};
		if (!posRenderTarget.renderTarget.Init(&graphics, windowWidth, windowHeight, DXGI_FORMAT_R32G32B32A32_FLOAT, posClearColor)) {
			return false;
		}

		preRenderTargetDescriptorId = rtDescriptorList.AddRenderTargetView(&preRenderTarget);
		posRenderTarget.rtDescriptorId = rtDescriptorList.AddRenderTargetView(&posRenderTarget.renderTarget);

		preRTResourceDescriptorId = resourceDescriptorList.AddShaderResourceDescriptor(preRenderTarget.GetResource(), preRenderTarget.GetFormat());
		posRenderTarget.resourceDescriptorId = resourceDescriptorList.AddShaderResourceDescriptor(posRenderTarget.renderTarget.GetResource(), posRenderTarget.renderTarget.GetFormat());
		if (preRTResourceDescriptorId < 0) {
			return false;
		}
		if (posRenderTarget.resourceDescriptorId < 0) {
			return false;
		}
		Shader::InputLayout layout;
		layout.AddElement("POSITION", Shader::InputLayout::Float, 4);
		layout.AddElement("TEXCOORD", Shader::InputLayout::Float, 2);
		Shader::Shader vs;
		if (!vs.LoadShader(L"Res/PostVertexShader.hlsl", "vs_5_0", "main")) {
			return false;
		}
		Shader::Shader ps;
		if (!ps.LoadShader(L"Res/PostPixelShader.hlsl", "ps_5_0", "main")) {
			return false;
		}

		Shader::RootSignatureFactory rsFactory;
		rsFactory.AddResourceBlock(2, 0);
		rsFactory.AddStaticSampler(0);
		if (!rsFactory.Create(&graphics, postPipeline.rootSignature)) {
			return false;
		}

		Shader::PipelineStateFactory psFactory;
		psFactory.SetLayout(layout);
		DXGI_FORMAT formats[] = { DXGI_FORMAT_R8G8B8A8_UNORM , };
		psFactory.SetRenderTargets(1, formats);
		psFactory.SetVertexShader(&vs);
		psFactory.SetPixelShader(&ps);
		psFactory.SetRootSignature(postPipeline.rootSignature.Get());
		if (!psFactory.Create(&graphics, postPipeline.pso)) {
			return false;
		}
		if (!postVertexBuffer.Init(&graphics, _countof(postVertex))) {
			return false;
		}
		if (!postIndexBuffer.Init(&graphics, _countof(postIndices))) {
			return false;
		}
		postVertexBuffer.Update(postVertex, sizeof(postVertex));
		postIndexBuffer.UpdateData(postIndices, sizeof(postIndices));
	}
	return true;
}

bool Render() {
	Graphics& graphics = Graphics::GetInstance();

	int currentBackBufferIndex = graphics.GetCurrentBackBufferIndex();
	frameComponent.SetCurrent(currentBackBufferIndex);
	if (!graphicsCommand.Reset(frameComponent.GetCommandAllocator())) {
		return false;
	}
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = graphicsCommand.GetCommandList();
	Microsoft::WRL::ComPtr<ID3D12Resource> renderTarget = graphics.GetRenderTarget(currentBackBufferIndex);
	commandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			renderTarget.Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET

		));
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtDescriptorList.GetCpuHandle(preRenderTargetDescriptorId);
	D3D12_CPU_DESCRIPTOR_HANDLE dsHandle = dsBuffer.GetHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandles[] = {
		handle,
		rtDescriptorList.GetCpuHandle(posRenderTarget.rtDescriptorId),
	};
	commandList->OMSetRenderTargets(_countof(cpuHandles), cpuHandles, FALSE, &dsHandle);
	float clearColor[] = { 0.1f,0.3f,0.5f,1.0f };
	commandList->ClearRenderTargetView(handle, preRenderTarget.GetClearColor(), 0, nullptr);
	commandList->ClearRenderTargetView(rtDescriptorList.GetCpuHandle(posRenderTarget.rtDescriptorId), posRenderTarget.renderTarget.GetClearColor(), 0, nullptr);
	commandList->ClearDepthStencilView(dsHandle, D3D12_CLEAR_FLAG_DEPTH, dsBuffer.GetDepthClearValue(), dsBuffer.GetStencilClearValue(), 0, nullptr);

	D3D12_VIEWPORT viewport;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MaxDepth = 1.0f;
	viewport.MinDepth = 0.0f;
	viewport.Width = windowWidth;
	viewport.Height = windowHeight;
	commandList->RSSetViewports(1, &viewport);
	D3D12_RECT scissor;
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = windowWidth;
	scissor.bottom = windowHeight;
	commandList->RSSetScissorRects(1, &scissor);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
	ID3D12DescriptorHeap* heaps[] = {
		resourceDescriptorList.GetHeap(),
	};
	commandList->SetDescriptorHeaps(_countof(heaps), heaps);
	commandList->SetGraphicsRootDescriptorTable(0, resourceDescriptorList.GetGpuHandle(cDescriptorId[0]));
	commandList->SetPipelineState(pipeline.pso.Get());
	D3D12_VERTEX_BUFFER_VIEW vbViews[] = {
		vertexBuffer.GetView(),
	};
	commandList->IASetVertexBuffers(0, 1, vbViews);
	commandList->DrawInstanced(3, 1, 0, 0);

	handle = frameComponent.GetRTVHandle();
	commandList->OMSetRenderTargets(1, &handle, FALSE, nullptr);
	float clear[] = {0.1f,0.1f,0.1f,1.0f};
	commandList->ClearRenderTargetView(handle, clear, 0, nullptr);
	postPipeline.Use(commandList);
	commandList->SetGraphicsRootDescriptorTable(0, resourceDescriptorList.GetGpuHandle(preRTResourceDescriptorId));
	D3D12_VERTEX_BUFFER_VIEW postVBViews[] = {
		postVertexBuffer.GetView(),
	};
	commandList->IASetVertexBuffers(0, 1, postVBViews);
	commandList->IASetIndexBuffer(&postIndexBuffer.GetView());
	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	commandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			renderTarget.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT
		)
	);
	if (FAILED(commandList->Close())) {
		return false;
	}
	ID3D12CommandList* ppCommands[] = {
		commandList.Get(),
	};
	graphics.GetCommandQueue()->ExecuteCommandLists(_countof(ppCommands), ppCommands);
	if (!graphics.Swap()) {
		return false;
	}
	if (!sync.WaitGpuProcess(graphics.GetCommandQueue())) {
		return false;
	}
	return true;
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	Window& window = Window::Get();
	if (!window.Init(hInstance, WndProc, windowWidth, windowHeight, windowTitle, nCmdShow)) {
		return 1;
	}

	if (!InitGraphics(window.GetWindowHandle())) {
		return false;
	}


	while (!window.WindowDestroyed()) {
		window.UpdateMessage();
		if (!Render()) {
			break;
		}
	}


	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
		if (wparam == VK_ESCAPE) {
			Window::Get().Destroy();
		}
		return 0;
	default:
		break;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}
