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

	bool VideoWindow::Create(const WChar* title, const DWORD dwExStyle, const DWORD dwStyle, const int x, const int y,
							 const int width, const int height, const bool visible, const HWND parent) noexcept
	{
		WNDCLASSEX wc{ 0 };
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BACKGROUND);
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.lpfnWndProc = VideoWindow::WndProc;
		wc.lpszClassName = TEXT("VideoWindowClass");
		wc.style = CS_VREDRAW | CS_HREDRAW;

		RegisterClassEx(&wc);
		m_WndHandle = CreateWindowEx(dwExStyle, wc.lpszClassName, title,
									 dwStyle, x, y, width, height, parent, nullptr, nullptr, this);
		if (m_WndHandle)
		{
			if (InitializeD2DRenderTarget(m_WndHandle, width, height))
			{
				if (visible)
				{
					ShowWindow(m_WndHandle, SW_SHOW);
					UpdateWindow(m_WndHandle);
				}

				ResizeRenderTarget();

				return true;
			}
		}

		return false;
	}

	void VideoWindow::Close() noexcept
	{
		DeinitializeD2DRenderTarget();
		
		m_VideoResampler.Close();

		SafeRelease(&m_OutputSample);

		if (m_WndHandle != nullptr)
		{
			if (DestroyWindow(m_WndHandle))
			{
				m_WndHandle = nullptr;
			}
			else LogErr(L"Failed to destroy video window; GetLastError() returned %d", GetLastError());
		}
	}

	bool VideoWindow::IsVisible() const noexcept
	{
		return ::IsWindowVisible(m_WndHandle);
	}

	void VideoWindow::SetWindowVisible(const bool visible) noexcept
	{
		ShowWindow(m_WndHandle, visible ? SW_SHOW : SW_HIDE);
		UpdateWindow(m_WndHandle);
	}

	void VideoWindow::ProcessMessages() noexcept
	{
		MSG msg{ 0 };

		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			if (m_WndHandle != nullptr && IsDialogMessage(m_WndHandle, &msg))
			{
				continue;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	void VideoWindow::Redraw() noexcept
	{
		RedrawWindow(m_WndHandle, nullptr, nullptr, RDW_ERASE | RDW_UPDATENOW | RDW_INVALIDATE);
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

	bool VideoWindow::InitializeD2DRenderTarget(const HWND hwnd, const UInt width, const UInt height) noexcept
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
				// No DPI scaling for MFC so use default (100%)
				m_D2D1RenderTarget->SetDpi(96.f, 96.f);

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
				if (rh > wnd_h&& rh > 0.0f)
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

	bool VideoWindow::SetInputFormat(const VideoFormat& fmt) noexcept
	{
		if (m_VideoResampler.IsOpen()) m_VideoResampler.Close();

		if (m_VideoResampler.Create(fmt.Width, fmt.Height,
									CaptureDevices::GetMFVideoFormat(fmt.Format),
									MFVideoFormat_RGB24))
		{
			auto result = CaptureDevices::CreateMediaSample(CaptureDevices::GetImageSize(m_VideoResampler.GetOutputFormat()));
			if (result.Succeeded())
			{
				SafeRelease(&m_OutputSample);

				m_OutputSample = result.GetValue();

				return true;
			}
		}

		return false;
	}

	void VideoWindow::Render(IMFSample* in_sample) noexcept
	{
		if (m_VideoResampler.Resample(in_sample, m_OutputSample))
		{
			Render(m_OutputSample, m_VideoResampler.GetOutputFormat());
		}
	}

	void VideoWindow::Render(const UInt64 in_timestamp, const BufferView pixels) noexcept
	{
		if (m_VideoResampler.Resample(in_timestamp, pixels, m_OutputSample))
		{
			Render(m_OutputSample, m_VideoResampler.GetOutputFormat());
		}
	}

	void VideoWindow::Render(IMFSample* in_sample, const VideoFormat& format) noexcept
	{
		assert(in_sample != nullptr);
		assert(format.Format != VideoFormat::PixelFormat::Unknown);

		IMFMediaBuffer* media_buffer{ nullptr };

		// Get the buffer from the sample
		auto hr = in_sample->GetBufferByIndex(0, &media_buffer);
		if (SUCCEEDED(hr))
		{
			// Release buffer when we exit this scope
			const auto sg = MakeScopeGuard([&]() noexcept { QuantumGate::AVExtender::SafeRelease(&media_buffer); });

			BYTE* in_data{ nullptr };
			DWORD in_data_len{ 0 };

			hr = media_buffer->Lock(&in_data, nullptr, &in_data_len);
			if (SUCCEEDED(hr))
			{
				Render(BufferView(reinterpret_cast<Byte*>(in_data), in_data_len), format);

				media_buffer->Unlock();
			}
		}
	}

	void VideoWindow::Render(const BufferView pixels, const VideoFormat& format) noexcept
	{
		assert(pixels.GetBytes() != nullptr && m_D2D1Bitmap != nullptr && m_D2D1RenderTarget != nullptr);

		// Number of bytes should match expected frame size
		if (pixels.GetSize() != CaptureDevices::GetImageSize(format))
		{
			assert(false);
			return;
		}

		const auto bmsize = m_D2D1Bitmap->GetSize();
		if (bmsize.width != static_cast<float>(format.Width) || bmsize.height != static_cast<float>(format.Height))
		{
			SafeRelease(&m_D2D1Bitmap);

			const auto hr = m_D2D1RenderTarget->CreateBitmap(D2D1::SizeU(format.Width, format.Height),
															 D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)),
															 &m_D2D1Bitmap);
			if (FAILED(hr)) return;

			ResizeDrawRect();
		}

		switch (format.Format)
		{
			case VideoFormat::PixelFormat::RGB24:
			{
				m_ResampleBuffer.Resize(static_cast<Size>(format.Width)*
										static_cast<Size>(format.Height) * sizeof(BGRAPixel));

				RGB24ToBGRA32(reinterpret_cast<BGRAPixel*>(m_ResampleBuffer.GetBytes()),
							  reinterpret_cast<const BGRPixel*>(pixels.GetBytes()),
							  format.Width, format.Height);

				m_D2D1Bitmap->CopyFromMemory(nullptr, m_ResampleBuffer.GetBytes(), format.Width * 4);
				break;
			}
			case VideoFormat::PixelFormat::RGB32:
			{
				m_ResampleBuffer.Resize(static_cast<Size>(format.Width)*
										static_cast<Size>(format.Height) * sizeof(BGRAPixel));

				ARGB32ToBGRA32(reinterpret_cast<BGRAPixel*>(m_ResampleBuffer.GetBytes()),
							   reinterpret_cast<const BGRAPixel*>(pixels.GetBytes()),
							   format.Width, format.Height);

				m_D2D1Bitmap->CopyFromMemory(nullptr, m_ResampleBuffer.GetBytes(), format.Width * 4);
				break;
			}
			default:
				// Unsupported format
				assert(false);
				return;
		}

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