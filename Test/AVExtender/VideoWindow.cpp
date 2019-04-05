// This file is part of the QuantumGate project. For copyright and
// licensing information refer to the license file(s) in the project root.

#include "stdafx.h"
#include "VideoWindow.h"

namespace QuantumGate::AVExtender
{
	VideoWindow::VideoWindow() noexcept
	{}

	VideoWindow::~VideoWindow()
	{
		Close();
	}

	const bool VideoWindow::Create(const DWORD dwExStyle, const DWORD dwStyle, const int x, const int y,
								   const int width, const int height, const HWND parent) noexcept
	{
		WNDCLASSEX wc{ 0 };
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BACKGROUND);
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.lpfnWndProc = VideoWindow::WndProc;
		wc.lpszClassName = TEXT("VideoWindowClass");
		wc.style = CS_VREDRAW | CS_HREDRAW;

		RegisterClassEx(&wc);
		m_WndHandle = CreateWindowEx(dwExStyle, wc.lpszClassName, L"VideoWindow",
									 dwStyle, x, y, width, height, parent, nullptr, nullptr, this);
		if (m_WndHandle)
		{
			if (InitializeD2DRenderTarget(m_WndHandle, width, height))
			{
				ShowWindow(m_WndHandle, SW_SHOW);
				UpdateWindow(m_WndHandle);

				ResizeRenderTarget();

				return true;
			}
		}

		return false;
	}

	void VideoWindow::Close() noexcept
	{
		DeinitializeD2DRenderTarget();

		if (DestroyWindow(m_WndHandle))
		{
			m_WndHandle = nullptr;
		}
	}

	LRESULT VideoWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept
	{
		VideoWindow* vwnd{ nullptr };

		if (msg == WM_CREATE)
		{
			vwnd = static_cast<VideoWindow*>(reinterpret_cast<LPCREATESTRUCT>(lparam)->lpCreateParams);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(vwnd));
		}
		else
		{
			vwnd = reinterpret_cast<VideoWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		}

		switch (msg)
		{
			case WM_SIZE:
				vwnd->ResizeRenderTarget();
				vwnd->ResizeDrawRect();
				break;
			case WM_CLOSE:
				vwnd->Close();
				break;
			default:
				break;
		}

		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	const bool VideoWindow::InitializeD2DRenderTarget(const HWND hwnd, const UInt width, const UInt height) noexcept
	{
		auto hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_D2D1Factory);
		if (SUCCEEDED(hr))
		{
			const auto size = D2D1::SizeU(width, height);
			auto rtprops = D2D1::RenderTargetProperties();
			rtprops.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
			rtprops.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

			hr = m_D2D1Factory->CreateHwndRenderTarget(rtprops,
													   D2D1::HwndRenderTargetProperties(hwnd, size),
													   &m_D2D1RenderTarget);
			if (SUCCEEDED(hr))
			{
				m_D2D1RenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE::D2D1_ANTIALIAS_MODE_ALIASED);

				hr = m_D2D1RenderTarget->CreateBitmap(size,
													  D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)),
													  &m_D2D1Bitmap);
				if (SUCCEEDED(hr))
				{
					return true;
				}
			}
		}

		DeinitializeD2DRenderTarget();

		return false;
	}

	void VideoWindow::DeinitializeD2DRenderTarget() noexcept
	{
		SafeRelease(&m_D2D1Factory);
		SafeRelease(&m_D2D1RenderTarget);
		SafeRelease(&m_D2D1Bitmap);
	}

	void VideoWindow::ResizeRenderTarget() noexcept
	{
		if (!m_D2D1RenderTarget) return;

		GetClientRect(m_WndHandle, &m_WndClientRect);
		m_D2D1RenderTarget->Resize(D2D1::SizeU(m_WndClientRect.right - m_WndClientRect.left,
											   m_WndClientRect.bottom - m_WndClientRect.top));
	}

	void VideoWindow::ResizeDrawRect() noexcept
	{
		if (!m_D2D1Bitmap) return;

		const auto bmsize = m_D2D1Bitmap->GetSize();
		if (bmsize.width == 0.0f || bmsize.height == 0.0f)
		{
			// Nothing to draw
			m_DrawRect.left = 0.0f;
			m_DrawRect.top = 0.0f;
			m_DrawRect.right = 0.0f;
			m_DrawRect.bottom = 0.0f;
		}
		else
		{
			// Center drawing rectangle in window rectangle so that the video
			// appears in the middle of the window, whatever its size
			const auto wnd_w = static_cast<float>(m_WndClientRect.right - m_WndClientRect.left);
			const auto wnd_h = static_cast<float>(m_WndClientRect.bottom - m_WndClientRect.top);

			auto rw = bmsize.width;
			auto rh = bmsize.height;

			if (rw > 0.0f)
			{
				rh = rh * (wnd_w / rw);
				rw = wnd_w;
			}

			if (m_RenderSize == RenderSize::Cover)
			{
				if (rh < wnd_h && rh > 0.0f)
				{
					rw = rw * (wnd_h / rh);
					rh = wnd_h;
				}
			}
			else
			{
				if (rh > wnd_h && rh > 0.0f)
				{
					rw = rw * (wnd_h / rh);
					rh = wnd_h;
				}
			}

			m_DrawRect.left = (wnd_w - rw) / 2.0f;
			m_DrawRect.top = (wnd_h - rh) / 2.0f;
			m_DrawRect.right = ((wnd_w - rw) / 2.0f) + rw;
			m_DrawRect.bottom = ((wnd_h - rh) / 2.0f) + rh;
		}
	}

	void VideoWindow::Render(const Byte* pixels, const UInt width, const UInt height) noexcept
	{
		assert(pixels != nullptr && m_D2D1Bitmap != nullptr && m_D2D1RenderTarget != nullptr);

		const auto bmsize = m_D2D1Bitmap->GetSize();
		if (bmsize.width != static_cast<float>(width) || bmsize.height != static_cast<float>(height))
		{
			SafeRelease(&m_D2D1Bitmap);

			const auto hr = m_D2D1RenderTarget->CreateBitmap(D2D1::SizeU(width, height),
															 D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)),
															 &m_D2D1Bitmap);
			if (FAILED(hr)) return;

			ResizeDrawRect();
		}

		m_D2D1Bitmap->CopyFromMemory(nullptr, pixels, width * 4);

		m_D2D1RenderTarget->BeginDraw();
		
		// No need to clear background if we cover entire window with video bitmap later
		if (m_RenderSize != RenderSize::Cover)
		{
			m_D2D1RenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
		}

		m_D2D1RenderTarget->DrawBitmap(m_D2D1Bitmap, &m_DrawRect, 1.0f,
									   D2D1_BITMAP_INTERPOLATION_MODE::D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
		
		m_D2D1RenderTarget->EndDraw();
	}
}