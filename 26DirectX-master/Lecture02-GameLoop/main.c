/*
          [Start Game]
                |
                v
+ ---------------------------+
|   1. Process Input         | //키보드, 마우스 등 사용자의 개입 확인
+ ---------------------------+
                |
                v
+ ---------------------------+
|   2. Update Game State     | //캐릭터 이동, 충돌 계산, AI 로직 수행
+ ---------------------------+
                |
                v
+ ---------------------------+
|   3. Render Frame          | //계산된 데이터를 화면에 시각화
+ ---------------------------+
                |                                                                      
      [isRunning == true ? ] ------------> [1. Process Input 으로] //반복(Loop)                                                       
                |                                                                      
                v                                                                      
           [End Game] 
*/

#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// 전역 변수
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pImmediateContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;

//추가: 위치 변수
float g_offsetX = 0.0f;

//추가: Constant Buffer
struct ConstantBuffer {
    float offsetX;
    float padding[3]; // 16바이트 정렬
};
ID3D11Buffer* g_pConstantBuffer = nullptr;

// 정점 구조체
struct Vertex {
    float x, y, z;
    float r, g, b, a;
};

//수정된 셰이더
const char* shaderSource = R"(
cbuffer ConstantBuffer : register(b0)
{
    float offsetX;
};

struct VS_INPUT { float3 pos : POSITION; float4 col : COLOR; };
struct PS_INPUT { float4 pos : SV_POSITION; float4 col : COLOR; };

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos.x + offsetX, input.pos.y, input.pos.z, 1.0f);
    output.col = input.col;
    return output;
}

float4 PS(PS_INPUT input) : SV_Target { return input.col; }
)";

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {

    // 윈도우 생성
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpszClassName = L"DX11Win32Class";
    RegisterClassEx(&wcex);

    HWND hWnd = CreateWindow(L"DX11Win32Class", L"DX11 Moving Triangle",
        WS_OVERLAPPEDWINDOW, 100, 100, 800, 600,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hWnd, nCmdShow);

    // DirectX 초기화
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 800;
    sd.BufferDesc.Height = 600;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);

    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    pBackBuffer->Release();

    // 셰이더 컴파일
    ID3DBlob* vsBlob, * psBlob;
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vsBlob, nullptr);
    D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &psBlob, nullptr);

    ID3D11VertexShader* vShader;
    ID3D11PixelShader* pShader;
    g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vShader);
    g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pShader);

    // Input Layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    ID3D11InputLayout* pInputLayout;
    g_pd3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &pInputLayout);

    // Vertex Buffer
    Vertex vertices[] = {
        {  0.0f,  0.5f, 0.5f, 1,0,0,1 },
        {  0.5f, -0.5f, 0.5f, 0,1,0,1 },
        { -0.5f, -0.5f, 0.5f, 0,0,1,1 },

        {  0.5f,  0.25f, 0.5f, 1,0,0,1 },
        {  0.0f, -0.75f, 0.5f, 0,1,0,1 },
        { -0.5f, 0.25f, 0.5f, 0,0,1,1 },
    };

    ID3D11Buffer* pVBuffer;
    D3D11_BUFFER_DESC bd = { sizeof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER };
    D3D11_SUBRESOURCE_DATA initData = { vertices };
    g_pd3dDevice->CreateBuffer(&bd, &initData, &pVBuffer);

    // Constant Buffer 생성
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(ConstantBuffer);
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    g_pd3dDevice->CreateBuffer(&cbd, nullptr, &g_pConstantBuffer);

    // 루프
    MSG msg = {};
    while (WM_QUIT != msg.message) {

        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {

            //입력 처리
            if (GetAsyncKeyState('A') & 0x8000) g_offsetX -= 0.001f;
            if (GetAsyncKeyState('D') & 0x8000) g_offsetX += 0.001f;

            //ConstantBuffer 업데이트
            ConstantBuffer cb = {};
            cb.offsetX = g_offsetX;
            g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, nullptr, &cb, 0, 0);
            g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);

            // 렌더링
            float clearColor[] = { 0.1f, 0.2f, 0.3f, 1.0f };
            g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

            D3D11_VIEWPORT vp = { 0,0,800,600,0,1 };
            g_pImmediateContext->RSSetViewports(1, &vp);

            UINT stride = sizeof(Vertex), offset = 0;
            g_pImmediateContext->IASetInputLayout(pInputLayout);
            g_pImmediateContext->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);
            g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            g_pImmediateContext->VSSetShader(vShader, nullptr, 0);
            g_pImmediateContext->PSSetShader(pShader, nullptr, 0);

            g_pImmediateContext->Draw(6, 0);
            g_pSwapChain->Present(0, 0);
        }
    }

    return 0;
}