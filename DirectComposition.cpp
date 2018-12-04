#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <CommCtrl.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <ShellScalingApi.h>
#include <wrl/client.h>
#include <d2d1_1.h>
#include <d3d11.h>

namespace
{
	using Microsoft::WRL::ComPtr;

	struct Data
	{
		~Data()
		{
			if (CaptionFont)
				DeleteObject(CaptionFont);
		}

		HWND TextBox = nullptr;
		HFONT CaptionFont = nullptr;
		float Scale = 1.0f;
		ComPtr<ID2D1Brush> BackgroundBrush;
		ComPtr<ID2D1Brush> BorderBrush;

		ComPtr<ID3D11Device> D3DDevice;
		ComPtr<IDXGIDevice> DXGIDevice;
		ComPtr<ID2D1Device> D2DDevice;
		ComPtr<IDCompositionDesktopDevice> CompositionDevice;
		ComPtr<IDCompositionTarget> CompositionTarget;
		ComPtr<IDCompositionVisual2> CompositionRootVisual;
		ComPtr<IDCompositionSurface> CompositionSurface;
		ComPtr<IDCompositionVisual2> TextBoxVisual;
		ComPtr<IUnknown> TextBoxSurface;

		bool IsAnimating = false;
		const UINT_PTR AnimationTimerID = 1;
	};

	Data* GetData(HWND hWnd)
	{
		LONG_PTR ptr = GetWindowLongPtrW(hWnd, GWLP_USERDATA);
		return reinterpret_cast<Data*>(ptr);
	}

	LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	HWND Create(HINSTANCE hInstance, const wchar_t *const className)
	{
		HWND hWnd = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP,
									className,
									L"DirectComposition",
									WS_OVERLAPPEDWINDOW | WS_VISIBLE,
									CW_USEDEFAULT, CW_USEDEFAULT,
									800, 800,
									nullptr,
									nullptr,
									hInstance,
									new Data());

		ShowWindow(hWnd, SW_SHOWDEFAULT);

		return hWnd;
	}

	int MessageLoop()
	{
		MSG msg = {};
		while (msg.message != WM_QUIT)
		{
			if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
			else
			{
				// Nothing to do
				Sleep(0);
			}
		}

		return static_cast<int>(msg.wParam);
	}

	bool Register(HINSTANCE hInstance, const wchar_t *const className)
	{
		WNDCLASSEXW wc = {};
		wc.cbSize = sizeof(WNDCLASSEXW);
		wc.lpfnWndProc = WndProc;
		wc.lpszClassName = className;
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		wc.style = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;
		wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
		wc.hInstance = hInstance;

		return !!RegisterClassExW(&wc);
	}

	RECT GetTextBoxRect(HWND hWnd, const RECT &clientRect)
	{
		Data *const data = GetData(hWnd);

		const int width = static_cast<int>(200 * data->Scale);
		const int height = static_cast<int>(21 * data->Scale);

		RECT textBoxRect = {};
		textBoxRect.left = (clientRect.right - width) / 2;
		textBoxRect.right = textBoxRect.left + width;
		textBoxRect.top = (clientRect.bottom - height) / 2;
		textBoxRect.bottom = textBoxRect.top + height;

		return textBoxRect;
	}

	RECT GetTextBoxRect(HWND hWnd)
	{
		RECT clientRect = {};
		GetClientRect(hWnd, &clientRect);
		return GetTextBoxRect(hWnd, clientRect);
	}

	void UpdateDPI(HWND hWnd)
	{
		Data *const data = GetData(hWnd);

		HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
		UINT dpiX = 0, dpiY = 0;
		GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
		data->Scale = dpiX / 96.0f;
	}

	void CreateDeviceResources(HWND hWnd)
	{
		Data *const data = GetData(hWnd);

		// DXGI Resources are tied to a D3D Device, so all resources will need re-creating after a new device is made
		data->BackgroundBrush = nullptr;
		data->BorderBrush = nullptr;



		HRESULT hr = S_OK;

		// Direct3D 11 Device
		UINT d3dDeviceFlags = D3D11_CREATE_DEVICE_SINGLETHREADED | D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		#if !defined(NDEBUG)
		d3dDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
		#endif
		hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, d3dDeviceFlags, nullptr, 0, D3D11_SDK_VERSION, data->D3DDevice.ReleaseAndGetAddressOf(), nullptr, nullptr);

		// DXGI Device
		hr = data->D3DDevice.As(&data->DXGIDevice);

		// Direct2D Device (settings must match those in the D3D device)
		D2D1_CREATION_PROPERTIES d2dProperties = {};
		d2dProperties.threadingMode = D2D1_THREADING_MODE_SINGLE_THREADED;
		#if !defined(NDEBUG)
		d2dProperties.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
		#endif
		hr = D2D1CreateDevice(data->DXGIDevice.Get(), d2dProperties, data->D2DDevice.ReleaseAndGetAddressOf());

		// DirectComposition Device
		hr = DCompositionCreateDevice2(data->D2DDevice.Get(), IID_PPV_ARGS(data->CompositionDevice.ReleaseAndGetAddressOf()));

		// DirectComposition Target
		hr = data->CompositionDevice->CreateTargetForHwnd(hWnd, FALSE, data->CompositionTarget.ReleaseAndGetAddressOf());

		// DirectComposition (root) Visual
		hr = data->CompositionDevice->CreateVisual(data->CompositionRootVisual.ReleaseAndGetAddressOf());

		hr = data->CompositionTarget->SetRoot(data->CompositionRootVisual.Get());

		hr = data->CompositionDevice->CreateVisual(data->TextBoxVisual.ReleaseAndGetAddressOf());
		hr = data->CompositionDevice->CreateSurfaceFromHwnd(data->TextBox, data->TextBoxSurface.ReleaseAndGetAddressOf());

		hr = data->TextBoxVisual->SetContent(data->TextBoxSurface.Get());

		const RECT textBoxRect = GetTextBoxRect(hWnd);
		data->TextBoxVisual->SetOffsetX(static_cast<float>(textBoxRect.left));
		data->TextBoxVisual->SetOffsetY(static_cast<float>(textBoxRect.top));
	}

	void CreateWindowSizeDependentResources(HWND hWnd)
	{
		Data *const data = GetData(hWnd);

		HRESULT hr = S_OK;

		RECT rect = {};
		::GetClientRect(hWnd, &rect);

		const int width = rect.right - rect.left;
		const int height = rect.bottom - rect.top;

		if (width <= 0 || height <= 0)
		{
			data->CompositionSurface = nullptr;
			return;
		}

		// DirectComposition Surface
		hr = data->CompositionDevice->CreateSurface(width, height, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED, data->CompositionSurface.ReleaseAndGetAddressOf());

		hr = data->CompositionRootVisual->SetContent(data->CompositionSurface.Get());
	}

	void OnCreate(HWND hWnd)
	{
		UpdateDPI(hWnd);

		Data *const data = GetData(hWnd);

		const RECT textBoxRect = GetTextBoxRect(hWnd);

		data->TextBox = CreateWindowExW(WS_EX_CLIENTEDGE | WS_EX_LAYERED, WC_EDITW, L"Click window to animate", WS_TABSTOP | WS_CHILD | WS_VISIBLE, textBoxRect.left, textBoxRect.top, textBoxRect.right - textBoxRect.left, textBoxRect.bottom - textBoxRect.top, hWnd, nullptr, nullptr, nullptr);

		::SetLayeredWindowAttributes(data->TextBox, 0, 255, LWA_ALPHA);

		NONCLIENTMETRICSW nonClientMetrics = {};
		nonClientMetrics.cbSize = sizeof(NONCLIENTMETRICSW);
		SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, nonClientMetrics.cbSize, &nonClientMetrics, 0);

		data->CaptionFont = CreateFontIndirectW(&nonClientMetrics.lfCaptionFont);
		SendMessageW(data->TextBox, WM_SETFONT, reinterpret_cast<WPARAM>(data->CaptionFont), TRUE);

		CreateDeviceResources(hWnd);
		//CreateWindowSizeDependentResources(hWnd);
	}

	void OnDPIChanged(HWND hWnd)
	{
		UpdateDPI(hWnd);
		
		CreateWindowSizeDependentResources(hWnd);
	}

	void OnLButtonUp(HWND hWnd)
	{
		Data *const data = GetData(hWnd);

		if (data->IsAnimating)
			return;

		data->IsAnimating = true;

		// Start an animation!

		ComPtr<IDCompositionAnimation> animation;
		HRESULT hr = data->CompositionDevice->CreateAnimation(&animation);

		animation->AddCubic(
			0.0,
			0.0f,
			500.0f,
			0.0f,
			0.0f
		);
		animation->AddCubic(
			1.0,
			500.0f,
			-500.0f,
			0.0f,
			0.0f
		);
		const double duration = 2.0; // Seconds
		animation->End(duration, 0.0f);

		ComPtr<IDCompositionTranslateTransform> transform;
		hr = data->CompositionDevice->CreateTranslateTransform(&transform);

		//transform->SetOffsetX(animation.Get());
		transform->SetOffsetY(animation.Get());

		data->TextBoxVisual->SetTransform(transform.Get());

		hr = data->CompositionRootVisual->AddVisual(data->TextBoxVisual.Get(), TRUE, nullptr);

		data->CompositionDevice->Commit();

		BOOL cloak = TRUE;
		hr = DwmSetWindowAttribute(data->TextBox, DWMWA_CLOAK, &cloak, sizeof(cloak));

		SetTimer(hWnd, data->AnimationTimerID, static_cast<UINT>(duration * 1000), nullptr);
	}

	void Render(Data *const data, ComPtr<ID2D1DeviceContext> deviceContext, const D2D1_RECT_F &area)
	{
		deviceContext->Clear();

		if (!data->BackgroundBrush)
		{
			HRESULT hr = S_OK;

			ComPtr<ID2D1SolidColorBrush> brush;
			hr = deviceContext->CreateSolidColorBrush(D2D1::ColorF(0.75f, 0.25f, 0.25f, 0.5f), &brush);
			data->BackgroundBrush = brush;
		}

		if (!data->BorderBrush)
		{
			HRESULT hr = S_OK;

			ComPtr<ID2D1SolidColorBrush> brush;
			hr = deviceContext->CreateSolidColorBrush(D2D1::ColorF(0.25f, 0.75f, 0.25f, 1.0f), &brush);
			data->BorderBrush = brush;
		}

		deviceContext->FillRectangle(area, data->BackgroundBrush.Get());

		D2D_RECT_F const rect = D2D1::RectF(area.left + 100.0f, area.top + 100.0f, area.right - 100.0f, area.bottom - 100.0f);
		deviceContext->DrawRectangle(rect, data->BorderBrush.Get(), 50.0f);
	}

	void OnPaint(HWND hWnd)
	{
		Data *const data = GetData(hWnd);

		PAINTSTRUCT ps = {};
		BeginPaint(hWnd, &ps);

		if (!data->D3DDevice)
			CreateDeviceResources(hWnd);

		if (data->CompositionSurface)
		{
			HRESULT hr = S_OK;

			ComPtr<ID2D1DeviceContext> deviceContext;
			POINT offset = {};
			hr = data->CompositionSurface->BeginDraw(nullptr, IID_PPV_ARGS(&deviceContext), &offset);

			deviceContext->SetDpi(96.0f * data->Scale, 96.0f * data->Scale);
			deviceContext->SetTransform(D2D1::Matrix3x2F::Translation(offset.x / data->Scale, offset.y / data->Scale));

			RECT clientRect = {};
			GetClientRect(hWnd, &clientRect);

			D2D1_RECT_F area = D2D1::RectF(clientRect.left / data->Scale, clientRect.top / data->Scale, clientRect.right / data->Scale, clientRect.bottom / data->Scale);

			Render(data, deviceContext, area);

			hr = data->CompositionSurface->EndDraw();

			hr = data->CompositionDevice->Commit();
		}

		EndPaint(hWnd, &ps);
	}

	void OnSize(HWND hWnd)
	{
		Data *const data = GetData(hWnd);

		const RECT textBoxRect = GetTextBoxRect(hWnd);

		SetWindowPos(data->TextBox, nullptr, textBoxRect.left, textBoxRect.top, textBoxRect.right - textBoxRect.left, textBoxRect.bottom - textBoxRect.top, SWP_NOZORDER);

		data->TextBoxVisual->SetOffsetX(static_cast<float>(textBoxRect.left));
		data->TextBoxVisual->SetOffsetY(static_cast<float>(textBoxRect.top));

		CreateWindowSizeDependentResources(hWnd);
	}

	void OnTimer(HWND hWnd, UINT_PTR id)
	{
		Data *const data = GetData(hWnd);

		if (id == data->AnimationTimerID)
		{
			KillTimer(hWnd, id);

			HRESULT hr = data->CompositionRootVisual->RemoveVisual(data->TextBoxVisual.Get());

			data->CompositionDevice->Commit();

			BOOL cloak = FALSE;
			hr = DwmSetWindowAttribute(data->TextBox, DWMWA_CLOAK, &cloak, sizeof(cloak));

			data->IsAnimating = false;
		}
	}

	LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_CREATE)
		{
			OnCreate(hWnd);
		}
		else if (msg == WM_DESTROY)
		{
			PostQuitMessage(0);
		}
		else if (msg == WM_DPICHANGED)
		{
			OnDPIChanged(hWnd);
		}
		else if (msg == WM_LBUTTONUP)
		{
			OnLButtonUp(hWnd);
		}
		else if (msg == WM_NCCREATE)
		{
			CREATESTRUCTW *const creationData = reinterpret_cast<CREATESTRUCTW*>(lParam);
			SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creationData->lpCreateParams));
		}
		else if (msg == WM_NCDESTROY)
		{
			delete GetData(hWnd);
		}
		else if (msg == WM_PAINT)
		{
			OnPaint(hWnd);
			return 0;
		}
		else if (msg == WM_SIZE)
		{
			OnSize(hWnd);
		}
		else if (msg == WM_TIMER)
		{
			OnTimer(hWnd, static_cast<UINT_PTR>(wParam));
		}

		return DefWindowProcW(hWnd, msg, wParam, lParam);
	}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
	// To support Windows 8.1 you could change this to SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
	SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	const wchar_t *const className = L"DCompWnd";

	if (!Register(hInstance, className))
		return -1;

	if (!Create(hInstance, className))
		return -1;

	return MessageLoop();
}
