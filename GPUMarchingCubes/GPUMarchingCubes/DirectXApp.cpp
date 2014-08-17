#include "DirectXApp.h"

//Certain windows unique identifier
HINSTANCE g_hInst = NULL;

//Handle to a window
HWND g_hWnd = NULL;


/*
Globals. These will probably be transferred to private parts of the dx app class
*/
D3D_DRIVER_TYPE g_DriverType = D3D_DRIVER_TYPE_NULL;

IDXGISwapChain* g_SwapChain = NULL;
ID3D11Device* g_d3dDevice = NULL;
ID3D11DeviceContext* g_ImmediateContext = NULL;
ID3D11RenderTargetView* g_RenderTargetView = NULL;

ID3D11VertexShader* g_VertexShader = NULL;
ID3D11PixelShader* g_PixelShader = NULL;
ID3D11GeometryShader* g_GeometryShader = NULL;

ID3D11InputLayout* g_VertexLayout = NULL;

ID3D11Buffer* g_VertexBuffer = NULL;
ID3D11Buffer* g_IndexBuffer = NULL;
ID3D11Buffer* g_ConstantBuffer = NULL;

ID3D11Texture2D* g_DepthStencil = NULL;
ID3D11DepthStencilView* g_DepthStencilView = NULL;



//Matrices used for 3D transformations
XMMATRIX g_World;
XMMATRIX g_View;
XMMATRIX g_Projection;


/*
Constant buffer description
*/
struct ConstantBuffer
{
	XMMATRIX m_World;
	XMMATRIX m_View;
	XMMATRIX m_Projection;
};

/*
Vertex description
*/
struct SimpleVertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

/*
Constructor
*/
DirectXApp::DirectXApp()
{
}

/*
Destructor
*/
DirectXApp::~DirectXApp()
{
}

/*
Tries to initialize the whole DirectX app to the point where it's ready to start the rendering loop
*/
bool DirectXApp::Init(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
  	if(FAILED(createWindow(hInstance, nCmdShow)))
		return false;

    if (FAILED(initDX()))
        return false;

	if (FAILED(createDepthStencil()))
		return false;

	if (FAILED(setupVertexAndIndexAndSOBuffer()))
		return false;

	if (FAILED(compileAndEnableShaders()))
		return false;


	if (FAILED(setupConstantBuffer()))
		return false;

    return true;
}

/*
Initializes a depth stencil that is needed to render objects that overlap each other correctly
*/
HRESULT DirectXApp::createDepthStencil()
{
	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = m_width;
	descDepth.Height = m_height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	HRESULT hr = g_d3dDevice->CreateTexture2D(&descDepth, NULL, &g_DepthStencil);
	if (FAILED(hr))
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = g_d3dDevice->CreateDepthStencilView(g_DepthStencil, &descDSV, &g_DepthStencilView);

	g_ImmediateContext->OMSetRenderTargets(1, &g_RenderTargetView, g_DepthStencilView);


	if (FAILED(hr))
		return hr;
}

/*
Set up the constant buffer that is used to transfer data from cpu to gpu once per frame (render).
*/
HRESULT DirectXApp::setupConstantBuffer()
{
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ConstantBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	HRESULT hr = g_d3dDevice->CreateBuffer(&bd, NULL, &g_ConstantBuffer);
	if (FAILED(hr))
		return hr;

	g_World = XMMatrixIdentity();

	XMVECTOR eye = XMVectorSet(0.0f, 1.0f, -5.0f, 0.0f);
	XMVECTOR at = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	g_View = XMMatrixLookAtLH(eye, at, up);

	//XM_PIDIV2 is pi / 2 aka 90 degrees field of view
	//0.01f is near clipping plane, 
	//100.0f is far clipping plane.
	g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, m_width / (FLOAT)m_height, 0.01f, 100.0f);

	return S_OK;
}


/*
Setup vertex, index and stream output buffers
*/
HRESULT DirectXApp::setupVertexAndIndexAndSOBuffer()
{
	SimpleVertex vertices[] =
	{
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) },
	};

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 8;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA initData;
	ZeroMemory(&initData, sizeof(initData));
	initData.pSysMem = vertices;
	HRESULT hr = g_d3dDevice->CreateBuffer(&bd, &initData, &g_VertexBuffer);

	if (FAILED(hr))
		return hr;


	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	g_ImmediateContext->IASetVertexBuffers(0, 1, &g_VertexBuffer, &stride, &offset);

	// Create index buffer
	WORD indices[] =
	{
		3, 1, 0,
		2, 1, 3,

		0, 5, 4,
		1, 5, 0,

		3, 4, 7,
		0, 4, 3,

		1, 6, 5,
		2, 6, 1,

		2, 7, 6,
		3, 7, 2,

		6, 4, 5,
		7, 4, 6,
	};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD) * 36;        // 36 vertices (indices) needed for 12 triangles in a triangle list
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	initData.pSysMem = indices;
	hr = g_d3dDevice->CreateBuffer(&bd, &initData, &g_IndexBuffer);
	if (FAILED(hr))
		return hr;

	// Set index buffer
	g_ImmediateContext->IASetIndexBuffer(g_IndexBuffer, DXGI_FORMAT_R16_UINT, 0);



	//Create SO stage buffer
	ID3D11Buffer* soBuffer;
	int soBufferSize = 1000000;
	D3D11_BUFFER_DESC soBufferDesc =
	{
		soBufferSize,
		D3D11_USAGE_DEFAULT,
		D3D11_BIND_STREAM_OUTPUT,
		0,
		0,
		0
	};
	g_d3dDevice->CreateBuffer(&soBufferDesc, NULL, &soBuffer);
	//put the SO buffer to use
	UINT soffset[1] = { 0 };
	//UINT* offset = 
	g_ImmediateContext->SOSetTargets(1, &soBuffer, soffset);
	
	//g_d3dDevice->SO

	return S_OK;
}

/*
Compile shaders and describe how they talk to each other and the CPU. 
*/
HRESULT DirectXApp::compileAndEnableShaders()
{
	HRESULT hr = S_OK;

	//Vertex shader
	ID3DBlob* vsBlob = NULL;
	hr = compileShaderFromFile(L"VertexShader.hlsl", "main", "vs_4_0", &vsBlob);

	if (FAILED(hr))
	{
		vsBlob->Release();
		return hr;
	}

	hr = g_d3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(),
		vsBlob->GetBufferSize(), NULL, &g_VertexShader);

	if (FAILED(hr))
	{
		vsBlob->Release();
		return hr;
	}

	//Create input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};	
	UINT numElements = ARRAYSIZE(layout);
	hr = g_d3dDevice->CreateInputLayout(layout, numElements, vsBlob->GetBufferPointer(),
										vsBlob->GetBufferSize(), &g_VertexLayout);
	vsBlob->Release();
	if (FAILED(hr))
		return hr;
	//set input layout to use
	g_ImmediateContext->IASetInputLayout(g_VertexLayout);


	//compile geometry shader
	ID3DBlob* gsBlob = NULL;
	hr = compileShaderFromFile(L"GeometryShader.hlsl", "main", "gs_4_0", &gsBlob);
	//stream output stage input signature declaration
	D3D11_SO_DECLARATION_ENTRY decl[] =
	{
		{ 0, "SV_POSITION", 0, 0, 4, 0 },
		{ 0, "COLOR", 0, 0, 4, 0 },
	};

	UINT stream = (UINT)0;
	hr = g_d3dDevice->CreateGeometryShaderWithStreamOutput
	(
		gsBlob->GetBufferPointer(),
		gsBlob->GetBufferSize(),
		decl,
		(UINT)2,
		//sizeof(decl),
		NULL,
		0,
		stream,
		NULL,
		&g_GeometryShader
	);

	gsBlob->Release();
	if (FAILED(hr))
	{
		return hr;
	}

	//compile and create pixel shader
	ID3DBlob* psBlob = NULL;
	hr = compileShaderFromFile(L"PixelShader.hlsl", "main", "ps_4_0", &psBlob);
	if (FAILED(hr))
		return hr;
	hr = g_d3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), NULL, &g_PixelShader);
	psBlob->Release();
	if (FAILED(hr))
	{
		return hr;
	}

	return S_OK;
}

/*
Compiles targeted text file as the specified shader type.
*/
HRESULT DirectXApp::compileShaderFromFile(WCHAR* FileName, LPCSTR EntryPoint, LPCSTR ShaderModel, ID3DBlob** OutBlob)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;

#if defined( DEBUG ) || defined( _DEBUG )
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	ID3DBlob* errorBlob;
	//DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;


	hr = D3DX11CompileFromFile(FileName, NULL, NULL, EntryPoint, ShaderModel,
		NULL, NULL, NULL, OutBlob, &errorBlob, NULL);

	if (FAILED(hr))
	{
		if (errorBlob != NULL)
		{
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		}
		if (errorBlob)
		{
			errorBlob->Release();
		}
		return hr;
	}

	if (errorBlob) errorBlob->Release();

	return S_OK;

}

/*
Creates the d3d device and initializes swapchain. Creates the backbuffer. Creates a viewport where we'll draw stuff.
*/
bool DirectXApp::initDX()
{
	//Only allow directx 11
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0
	};


	UINT featureLevelCount = ARRAYSIZE(featureLevels);

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = m_width;
	sd.BufferDesc.Height = m_height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//Refresh rate
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = g_hWnd;
	//Multisampling
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	g_DriverType = D3D_DRIVER_TYPE_HARDWARE;


	//Enable debugging of d3d11 stuff
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, g_DriverType, NULL, creationFlags, featureLevels, featureLevelCount,
        D3D11_SDK_VERSION, &sd, &g_SwapChain, &g_d3dDevice, NULL, &g_ImmediateContext);

    if (FAILED(hr))
        return hr;

    ID3D11Texture2D* backBuffer = NULL;

    //this saves the position of swap chain's back buffer to backBuffer
    hr = g_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer);
    if (FAILED(hr))
        return hr;


    hr = g_d3dDevice->CreateRenderTargetView(backBuffer, NULL, &g_RenderTargetView);
    //release memory and close threads used by COM object. Only closes texture object, doesn't destroy the actual back buffer
    backBuffer->Release();
    if (FAILED(hr))
        return hr;


	//Create the viewport
    D3D11_VIEWPORT vp;
    vp.Width = (float)m_width;
    vp.Height = (float)m_height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_ImmediateContext->RSSetViewports(1, &vp);


    return S_OK;
}

/*
Starts running the window message loop which handles messages to the window and when there's no messages renders.
*/
bool DirectXApp::Run()
{
    MSG msg = { 0 };
    while(msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            render();
        }
    }

    return true;
}

/* Renders a frame,  */
void DirectXApp::render()
{


	//Background color
    float clearColor[4] = { 0.0f, 0.125f, 0.2f, 1.0f };

	//Draw background color
    g_ImmediateContext->ClearRenderTargetView(g_RenderTargetView, clearColor);


	//Reset depth buffer to max depth
	g_ImmediateContext->ClearDepthStencilView(g_DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	g_ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//Constant buffer is what we'll transfer from the cpu side to the gpu (eg. for the shader code to use)
	ConstantBuffer cb;
	cb.m_World = XMMatrixTranspose(g_World);
	cb.m_View = XMMatrixTranspose(g_View);
	cb.m_Projection = XMMatrixTranspose(g_Projection);
	g_ImmediateContext->UpdateSubresource(g_ConstantBuffer, 0, NULL, &cb, 0, 0);

	//Put the shaders to use
	g_ImmediateContext->VSSetShader(g_VertexShader, NULL, 0);
	g_ImmediateContext->GSSetShader(g_GeometryShader, NULL, 0);
	g_ImmediateContext->VSSetConstantBuffers(0, 1, &g_ConstantBuffer);
	g_ImmediateContext->PSSetShader(g_PixelShader, NULL, 0);

	//Draws the 36 indices currently bound to the device
	g_ImmediateContext->DrawIndexed(36, 0, 0);

	//Switch back buffer and front buffer to show what we've drawn. 
	//ie. present it.
    g_SwapChain->Present(0, 0);
}


/* Create a basic window using windows api*/
HRESULT DirectXApp::createWindow(HINSTANCE hInstance, int nCmdShow)
{

    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;

	//lpfn == long pointer to a function, WndProc == Windows Procedure 
	//Long Pointer to the Windows Procedure function
    wcex.lpfnWndProc = WndProc;

    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = 0;
    //wcex.hIcon = LoadIcon(0, IDI_WINLOGO);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"TestWindowClass";
    wcex.hIconSm = NULL;
    
    if (!RegisterClassEx(&wcex))
        return E_FAIL;


	g_hInst = hInstance;
    RECT rc = { 0, 0, 640, 480 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	g_hWnd = CreateWindow(L"TestWindowClass", L"Hello world", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
		NULL);

	if(!g_hWnd)
		return E_FAIL;

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}

/* Windows Procedure function used in showing and drawing the window*/
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}