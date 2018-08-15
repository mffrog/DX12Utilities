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
};

using Mat = mff::Matrix4x4<float>;
using Vec4 = mff::Vector4<float>;
const int FrameBufferCount = 2;
FrameComponent frameComponents[FrameBufferCount];
GraphicsCommand graphicsCommand;
Resource::DepthStencilDescriptorList depthStencilList;
Resource::DepthStencilBuffer dsBuffer;
Resource::ResourceDescriptorList resourceDescriptorList;
Resource::ConstantBuffer constantBuffer;
int constantId[2] = {-1,-1} ;
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
	{ { 0.0f, 0.5f,0.5f },{ 1.0f,0.0f,0.0f,1.0f }, },
	{ { 0.5f,-0.5f,0.5f },{ 1.0f,1.0f,0.0f,1.0f }, },
	{ {-0.5f,-0.5f,0.5f },{ 1.0f,0.0f,1.0f,1.0f }, },
};

Shader::VertexBuffer<Vertex> vertexBuffer;
Shader::IndexBuffer indexBuffer;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
bool InitGraphics(HWND hwnd) {
	Graphics& graphics = Graphics::GetInstance();
	if (!graphics.Initialize(hwnd, windowWidth, windowHeight)) {
		return 1;
	}

	for (int i = 0; i < FrameBufferCount; ++i) {
		if (!frameComponents[i].allocator.Init(&graphics, CommandType::CommandType_Direct)) {
			return false;
		}
	}

	int currentFrame = graphics.GetCurrentBackBufferIndex();
	if (!graphicsCommand.Init(&frameComponents[currentFrame].allocator)) {
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
		Vec4(1.0f,0.0f,0.0f,0.0f), 
		Vec4(0.0f,1.0f,0.0f,0.0f), 
		Vec4(0.0f,0.0f,1.0f,0.0f), 
		Vec4(0.0f,0.0f,0.0f,1.0f));
	constantBuffer.Update(constantId[0], &mat, sizeof(Mat));

	Shader::InputLayout layout;
	layout.AddElement("POSITION", Shader::InputLayout::Float, 3, Shader::InputLayout::Classification_PerVertex);
	layout.AddElement("COLOR", Shader::InputLayout::Float, 4, Shader::InputLayout::Classification_PerVertex);

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
	};
	psFactory.SetRenderTargets(1, format);
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

	return true;
}

bool Render() {
	Graphics& graphics = Graphics::GetInstance();

	int currentBackBufferIndex = graphics.GetCurrentBackBufferIndex();
	if (!graphicsCommand.Reset(&frameComponents[currentBackBufferIndex].allocator)) {
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
	D3D12_CPU_DESCRIPTOR_HANDLE handle = graphics.GetRTVHandle(currentBackBufferIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE dsHandle = dsBuffer.GetHandle();
	commandList->OMSetRenderTargets(1, &handle, FALSE, &dsHandle);
	float clearColor[] = { 0.1f,0.3f,0.5f,1.0f };
	commandList->ClearRenderTargetView(handle, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(dsHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

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
