#pragma once

/**
 * @file modern_suites.hpp
 * @brief Modern AppModel, Direct2D, DirectML & Security (Milestones 106-145)
 */

void Test_WindowsDirect2D_Hardware_Rendering_Subsystem() {
    using namespace micant::d2d1;

    // 1. Direct2D Factory Creation (Single-threaded & Multi-threaded)
    {
        ID2D1Factory* pFactory = nullptr;
        D2D1_FACTORY_OPTIONS options{ D2D1_DEBUG_LEVEL_INFORMATION };
        int32_t hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, &options, reinterpret_cast<void**>(&pFactory));
        TEST_ASSERT(hr == ole32::S_OK && pFactory != nullptr, "D2D1CreateFactory single-threaded must succeed");

        ID2D1Factory* pMultiFactory = nullptr;
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, IID_ID2D1Factory, nullptr, reinterpret_cast<void**>(&pMultiFactory));
        TEST_ASSERT(hr == ole32::S_OK && pMultiFactory != nullptr, "D2D1CreateFactory multi-threaded must succeed");
        pMultiFactory->Release();
        pFactory->Release();
    }

    // 2. Desktop DPI Query & Metrics Reload
    {
        ID2D1Factory* pFactory = nullptr;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, nullptr, reinterpret_cast<void**>(&pFactory));
        TEST_ASSERT(pFactory != nullptr, "Factory instance valid for DPI test");

        float dpiX = 0.0f, dpiY = 0.0f;
        pFactory->GetDesktopDpi(&dpiX, &dpiY);
        TEST_ASSERT(dpiX == 96.0f && dpiY == 96.0f, "GetDesktopDpi returns standard 96 DPI");

        int32_t hr = pFactory->ReloadSystemMetrics();
        TEST_ASSERT(hr == ole32::S_OK, "ReloadSystemMetrics must succeed");
        pFactory->Release();
    }

    // 3. HWND Render Target Instantiation & Window Operations
    {
        ID2D1Factory* pFactory = nullptr;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, nullptr, reinterpret_cast<void**>(&pFactory));

        D2D1_RENDER_TARGET_PROPERTIES rtProps{};
        rtProps.type = D2D1_RENDER_TARGET_TYPE_HARDWARE;
        rtProps.pixelFormat = { 87, D2D1_ALPHA_MODE_PREMULTIPLIED }; // DXGI_FORMAT_B8G8R8A8_UNORM
        rtProps.dpiX = 96.0f;
        rtProps.dpiY = 96.0f;

        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps{};
        hwndProps.hwnd = reinterpret_cast<void*>(0x12345678);
        hwndProps.pixelSize = { 1024, 768 };
        hwndProps.presentOptions = D2D1_PRESENT_OPTIONS_RETAIN_CONTENTS;

        ID2D1HwndRenderTarget* pHwndRt = nullptr;
        int32_t hr = pFactory->CreateHwndRenderTarget(&rtProps, &hwndProps, &pHwndRt);
        TEST_ASSERT(hr == ole32::S_OK && pHwndRt != nullptr, "CreateHwndRenderTarget must succeed");
        TEST_ASSERT(pHwndRt->GetHwnd() == reinterpret_cast<void*>(0x12345678), "GetHwnd returns matching HWND");

        D2D1_SIZE_U pxSize = pHwndRt->GetPixelSize();
        TEST_ASSERT(pxSize.width == 1024 && pxSize.height == 768, "GetPixelSize matches 1024x768");
        TEST_ASSERT(pHwndRt->CheckWindowState() == 0, "CheckWindowState indicates healthy state");

        D2D1_SIZE_U newSize{ 1280, 720 };
        hr = pHwndRt->Resize(&newSize);
        TEST_ASSERT(hr == ole32::S_OK, "Resize must succeed");
        pxSize = pHwndRt->GetPixelSize();
        TEST_ASSERT(pxSize.width == 1280 && pxSize.height == 720, "Pixel size updated after resize");

        pHwndRt->Release();
        pFactory->Release();
    }

    // 4. Compatible Bitmap Render Target Instantiation
    {
        ID2D1Factory* pFactory = nullptr;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, nullptr, reinterpret_cast<void**>(&pFactory));

        D2D1_RENDER_TARGET_PROPERTIES rtProps{};
        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps{};
        hwndProps.hwnd = reinterpret_cast<void*>(0x1000);
        hwndProps.pixelSize = { 640, 480 };

        ID2D1HwndRenderTarget* pParentRt = nullptr;
        pFactory->CreateHwndRenderTarget(&rtProps, &hwndProps, &pParentRt);

        D2D1_SIZE_F compSize{ 320.0f, 240.0f };
        ID2D1BitmapRenderTarget* pCompRt = nullptr;
        int32_t hr = pParentRt->CreateCompatibleRenderTarget(&compSize, nullptr, nullptr, 0, &pCompRt);
        TEST_ASSERT(hr == ole32::S_OK && pCompRt != nullptr, "CreateCompatibleRenderTarget must succeed");

        D2D1_SIZE_F sz = pCompRt->GetSize();
        TEST_ASSERT(sz.width == 320.0f && sz.height == 240.0f, "Compatible RT dimensions match 320x240");

        ID2D1Bitmap* pBitmap = nullptr;
        hr = pCompRt->GetBitmap(&pBitmap);
        TEST_ASSERT(hr == ole32::S_OK && pBitmap != nullptr, "Compatible RT GetBitmap must succeed");
        TEST_ASSERT(pBitmap->GetPixelSize().width == 320, "Target bitmap pixel width matches 320");

        pBitmap->Release();
        pCompRt->Release();
        pParentRt->Release();
        pFactory->Release();
    }

    // 5. Solid Color Brush Creation & Opacity Manipulation
    {
        ID2D1Factory* pFactory = nullptr;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, nullptr, reinterpret_cast<void**>(&pFactory));
        D2D1_RENDER_TARGET_PROPERTIES rtProps{};
        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps{};
        hwndProps.hwnd = reinterpret_cast<void*>(0x1000);
        hwndProps.pixelSize = { 100, 100 };
        ID2D1HwndRenderTarget* pRt = nullptr;
        pFactory->CreateHwndRenderTarget(&rtProps, &hwndProps, &pRt);

        D2D1_COLOR_F redColor = ColorF::Red();
        ID2D1SolidColorBrush* pSolidBrush = nullptr;
        int32_t hr = pRt->CreateSolidColorBrush(&redColor, nullptr, &pSolidBrush);
        TEST_ASSERT(hr == ole32::S_OK && pSolidBrush != nullptr, "CreateSolidColorBrush must succeed");

        D2D1_COLOR_F c = pSolidBrush->GetColor();
        TEST_ASSERT(c.r == 1.0f && c.g == 0.0f && c.b == 0.0f && c.a == 1.0f, "GetColor matches red");

        pSolidBrush->SetOpacity(0.5f);
        TEST_ASSERT(std::abs(pSolidBrush->GetOpacity() - 0.5f) < 0.001f, "Opacity modified to 0.5");

        D2D1_COLOR_F blueColor = ColorF::Blue();
        pSolidBrush->SetColor(&blueColor);
        c = pSolidBrush->GetColor();
        TEST_ASSERT(c.b == 1.0f && c.r == 0.0f, "SetColor updated brush to blue");

        pSolidBrush->Release();
        pRt->Release();
        pFactory->Release();
    }

    // 6. Linear Gradient Brush & Stop Collection
    {
        ID2D1Factory* pFactory = nullptr;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, nullptr, reinterpret_cast<void**>(&pFactory));
        D2D1_RENDER_TARGET_PROPERTIES rtProps{};
        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps{};
        hwndProps.hwnd = reinterpret_cast<void*>(0x1000);
        hwndProps.pixelSize = { 100, 100 };
        ID2D1HwndRenderTarget* pRt = nullptr;
        pFactory->CreateHwndRenderTarget(&rtProps, &hwndProps, &pRt);

        D2D1_GRADIENT_STOP stops[2];
        stops[0].position = 0.0f;
        stops[0].color = ColorF::Red();
        stops[1].position = 1.0f;
        stops[1].color = ColorF::Blue();

        ID2D1GradientStopCollection* pStops = nullptr;
        int32_t hr = pRt->CreateGradientStopCollection(stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &pStops);
        TEST_ASSERT(hr == ole32::S_OK && pStops != nullptr, "CreateGradientStopCollection must succeed");
        TEST_ASSERT(pStops->GetGradientStopCount() == 2, "Stop count matches 2");

        D2D1_GRADIENT_STOP retrievedStops[2];
        pStops->GetGradientStops(retrievedStops, 2);
        TEST_ASSERT(retrievedStops[0].position == 0.0f && retrievedStops[1].position == 1.0f, "Stop positions verified");

        D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES linProps{};
        linProps.startPoint = { 0.0f, 0.0f };
        linProps.endPoint = { 100.0f, 0.0f };

        ID2D1LinearGradientBrush* pLinBrush = nullptr;
        hr = pRt->CreateLinearGradientBrush(&linProps, nullptr, pStops, &pLinBrush);
        TEST_ASSERT(hr == ole32::S_OK && pLinBrush != nullptr, "CreateLinearGradientBrush must succeed");
        TEST_ASSERT(pLinBrush->GetStartPoint().x == 0.0f && pLinBrush->GetEndPoint().x == 100.0f, "Linear brush endpoints verified");

        pLinBrush->Release();
        pStops->Release();
        pRt->Release();
        pFactory->Release();
    }

    // 7. Radial Gradient Brush Configuration
    {
        ID2D1Factory* pFactory = nullptr;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, nullptr, reinterpret_cast<void**>(&pFactory));
        D2D1_RENDER_TARGET_PROPERTIES rtProps{};
        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps{};
        hwndProps.hwnd = reinterpret_cast<void*>(0x1000);
        hwndProps.pixelSize = { 100, 100 };
        ID2D1HwndRenderTarget* pRt = nullptr;
        pFactory->CreateHwndRenderTarget(&rtProps, &hwndProps, &pRt);

        D2D1_GRADIENT_STOP stops[2];
        stops[0].position = 0.0f;
        stops[0].color = ColorF::White();
        stops[1].position = 1.0f;
        stops[1].color = ColorF::Black();

        ID2D1GradientStopCollection* pStops = nullptr;
        pRt->CreateGradientStopCollection(stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &pStops);

        D2D1_RADIAL_GRADIENT_BRUSH_PROPERTIES radProps{};
        radProps.center = { 50.0f, 50.0f };
        radProps.gradientOriginOffset = { 0.0f, 0.0f };
        radProps.radiusX = 40.0f;
        radProps.radiusY = 30.0f;

        ID2D1RadialGradientBrush* pRadBrush = nullptr;
        int32_t hr = pRt->CreateRadialGradientBrush(&radProps, nullptr, pStops, &pRadBrush);
        TEST_ASSERT(hr == ole32::S_OK && pRadBrush != nullptr, "CreateRadialGradientBrush must succeed");
        TEST_ASSERT(pRadBrush->GetCenter().x == 50.0f && pRadBrush->GetRadiusX() == 40.0f && pRadBrush->GetRadiusY() == 30.0f, "Radial brush center and radii verified");

        pRadBrush->Release();
        pStops->Release();
        pRt->Release();
        pFactory->Release();
    }

    // 8. Stroke Style Attributes & Dashes
    {
        ID2D1Factory* pFactory = nullptr;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, nullptr, reinterpret_cast<void**>(&pFactory));

        D2D1_STROKE_STYLE_PROPERTIES strokeProps{};
        strokeProps.startCap = D2D1_CAP_STYLE_ROUND;
        strokeProps.endCap = D2D1_CAP_STYLE_ROUND;
        strokeProps.dashCap = D2D1_CAP_STYLE_ROUND;
        strokeProps.lineJoin = D2D1_LINE_JOIN_ROUND;
        strokeProps.miterLimit = 10.0f;
        strokeProps.dashStyle = D2D1_DASH_STYLE_DASH_DOT;

        ID2D1StrokeStyle* pStroke = nullptr;
        int32_t hr = pFactory->CreateStrokeStyle(&strokeProps, nullptr, 0, &pStroke);
        TEST_ASSERT(hr == ole32::S_OK && pStroke != nullptr, "CreateStrokeStyle must succeed");
        TEST_ASSERT(pStroke->GetStartCap() == D2D1_CAP_STYLE_ROUND, "Stroke start cap is round");
        TEST_ASSERT(pStroke->GetDashStyle() == D2D1_DASH_STYLE_DASH_DOT, "Stroke dash style is DashDot");
        TEST_ASSERT(pStroke->GetDashesCount() == 4, "DashDot pattern has 4 elements");

        pStroke->Release();
        pFactory->Release();
    }

    // 9. Parametric Geometries (Rectangle & Ellipse)
    {
        ID2D1Factory* pFactory = nullptr;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, nullptr, reinterpret_cast<void**>(&pFactory));

        D2D1_RECT_F r{ 10.0f, 20.0f, 110.0f, 70.0f };
        ID2D1RectangleGeometry* pRectGeo = nullptr;
        int32_t hr = pFactory->CreateRectangleGeometry(&r, &pRectGeo);
        TEST_ASSERT(hr == ole32::S_OK && pRectGeo != nullptr, "CreateRectangleGeometry must succeed");

        float area = 0.0f;
        pRectGeo->ComputeArea(nullptr, &area);
        TEST_ASSERT(std::abs(area - 5000.0f) < 0.1f, "Rectangle area is 100 * 50 = 5000");

        int32_t contains = 0;
        pRectGeo->FillContainsPoint({ 50.0f, 30.0f }, nullptr, &contains);
        TEST_ASSERT(contains == 1, "Point (50, 30) is inside rectangle");
        pRectGeo->FillContainsPoint({ 5.0f, 5.0f }, nullptr, &contains);
        TEST_ASSERT(contains == 0, "Point (5, 5) is outside rectangle");

        D2D1_ELLIPSE ell{ { 50.0f, 50.0f }, 20.0f, 20.0f };
        ID2D1EllipseGeometry* pEllGeo = nullptr;
        hr = pFactory->CreateEllipseGeometry(&ell, &pEllGeo);
        TEST_ASSERT(hr == ole32::S_OK && pEllGeo != nullptr, "CreateEllipseGeometry must succeed");

        pEllGeo->ComputeArea(nullptr, &area);
        TEST_ASSERT(std::abs(area - (3.14159265f * 400.0f)) < 1.0f, "Ellipse area matches pi * r^2");

        pEllGeo->FillContainsPoint({ 50.0f, 50.0f }, nullptr, &contains);
        TEST_ASSERT(contains == 1, "Center point is inside ellipse");
        pEllGeo->FillContainsPoint({ 80.0f, 80.0f }, nullptr, &contains);
        TEST_ASSERT(contains == 0, "Distant point is outside ellipse");

        pRectGeo->Release();
        pEllGeo->Release();
        pFactory->Release();
    }

    // 10. Path Geometry & Geometry Sink Assembly
    {
        ID2D1Factory* pFactory = nullptr;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, nullptr, reinterpret_cast<void**>(&pFactory));

        ID2D1PathGeometry* pPath = nullptr;
        int32_t hr = pFactory->CreatePathGeometry(&pPath);
        TEST_ASSERT(hr == ole32::S_OK && pPath != nullptr, "CreatePathGeometry must succeed");

        ID2D1GeometrySink* pSink = nullptr;
        hr = pPath->Open(&pSink);
        TEST_ASSERT(hr == ole32::S_OK && pSink != nullptr, "Open geometry sink must succeed");

        pSink->BeginFigure({ 0.0f, 0.0f }, D2D1_FIGURE_BEGIN_FILLED);
        pSink->AddLine({ 100.0f, 0.0f });
        pSink->AddLine({ 100.0f, 100.0f });
        pSink->AddLine({ 0.0f, 100.0f });
        pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
        hr = pSink->Close();
        TEST_ASSERT(hr == ole32::S_OK, "Geometry sink close must succeed");
        pSink->Release();

        uint32_t figCount = 0, segCount = 0;
        pPath->GetFigureCount(&figCount);
        pPath->GetSegmentCount(&segCount);
        TEST_ASSERT(figCount == 1, "Figure count matches 1");
        TEST_ASSERT(segCount == 3, "Segment count matches 3");

        D2D1_RECT_F bounds{};
        pPath->GetBounds(nullptr, &bounds);
        TEST_ASSERT(bounds.left == 0.0f && bounds.top == 0.0f && bounds.right == 100.0f && bounds.bottom == 100.0f, "Path geometry bounds match 100x100 box");

        pPath->Release();
        pFactory->Release();
    }

    // 11. Render Target Drawing Primitives (Clear, DrawLine, FillRectangle, DrawGeometry, EndDraw)
    {
        ID2D1Factory* pFactory = nullptr;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, nullptr, reinterpret_cast<void**>(&pFactory));
        D2D1_RENDER_TARGET_PROPERTIES rtProps{};
        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps{};
        hwndProps.hwnd = reinterpret_cast<void*>(0x1000);
        hwndProps.pixelSize = { 100, 100 };
        ID2D1HwndRenderTarget* pRt = nullptr;
        pFactory->CreateHwndRenderTarget(&rtProps, &hwndProps, &pRt);

        D2D1_COLOR_F green = ColorF::Green();
        ID2D1SolidColorBrush* pGreenBrush = nullptr;
        pRt->CreateSolidColorBrush(&green, nullptr, &pGreenBrush);

        pRt->BeginDraw();
        D2D1_COLOR_F white = ColorF::White();
        pRt->Clear(&white);

        pRt->DrawLine({ 0.0f, 0.0f }, { 99.0f, 99.0f }, pGreenBrush, 2.0f);

        D2D1_RECT_F box{ 10.0f, 10.0f, 30.0f, 30.0f };
        pRt->FillRectangle(&box, pGreenBrush);

        D2D1_ROUNDED_RECT roundBox{ { 40.0f, 10.0f, 60.0f, 30.0f }, 4.0f, 4.0f };
        pRt->DrawRoundedRectangle(&roundBox, pGreenBrush, 1.0f);
        pRt->FillRoundedRectangle(&roundBox, pGreenBrush);

        D2D1_ELLIPSE ell{ { 50.0f, 50.0f }, 20.0f, 20.0f };
        pRt->DrawEllipse(&ell, pGreenBrush, 1.0f);
        pRt->FillEllipse(&ell, pGreenBrush);

        int32_t hr = pRt->EndDraw();
        TEST_ASSERT(hr == ole32::S_OK, "EndDraw on render target must succeed");

        pGreenBrush->Release();
        pRt->Release();
        pFactory->Release();
    }

    // 12. DirectWrite Typography Interop
    {
        ID2D1Factory* pFactory = nullptr;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, nullptr, reinterpret_cast<void**>(&pFactory));
        D2D1_RENDER_TARGET_PROPERTIES rtProps{};
        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps{};
        hwndProps.hwnd = reinterpret_cast<void*>(0x1000);
        hwndProps.pixelSize = { 200, 100 };
        ID2D1HwndRenderTarget* pRt = nullptr;
        pFactory->CreateHwndRenderTarget(&rtProps, &hwndProps, &pRt);

        dwrite::IDWriteFactory* pDWriteFactory = nullptr;
        int32_t hr = dwrite::DWriteCreateFactory(dwrite::DWRITE_FACTORY_TYPE_SHARED, dwrite::IID_IDWriteFactory, reinterpret_cast<ole32::IUnknown**>(&pDWriteFactory));
        TEST_ASSERT(hr == ole32::S_OK && pDWriteFactory != nullptr, "DWriteCreateFactory must succeed");

        dwrite::IDWriteTextFormat* pFormat = nullptr;
        hr = pDWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, dwrite::DWRITE_FONT_WEIGHT_NORMAL, dwrite::DWRITE_FONT_STYLE_NORMAL, dwrite::DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &pFormat);
        TEST_ASSERT(hr == ole32::S_OK && pFormat != nullptr, "CreateTextFormat must succeed");

        ID2D1SolidColorBrush* pTextBrush = nullptr;
        D2D1_COLOR_F black = ColorF::Black();
        pRt->CreateSolidColorBrush(&black, nullptr, &pTextBrush);

        pRt->BeginDraw();
        D2D1_RECT_F layoutRect{ 10.0f, 10.0f, 190.0f, 90.0f };
        pRt->DrawText(L"Direct2D Text", 13, pFormat, &layoutRect, pTextBrush, D2D1_DRAW_TEXT_OPTIONS_NONE);
        hr = pRt->EndDraw();
        TEST_ASSERT(hr == ole32::S_OK, "DrawText and EndDraw must succeed");

        pTextBrush->Release();
        pFormat->Release();
        pDWriteFactory->Release();
        pRt->Release();
        pFactory->Release();
    }

    // 13. WIC Bitmap Interop
    {
        gdiplus::InitializeGdiPlusExports();
        gdiplus::IWICImagingFactory* pWicFactory = nullptr;
        gdiplus::WICCreateImagingFactory_Proxy(0, &pWicFactory);
        TEST_ASSERT(pWicFactory != nullptr, "WIC factory must be valid");

        gdiplus::IWICBitmap* pWicBmp = nullptr;
        std::vector<uint32_t> testPixels(16 * 16, 0xFF00AA55);
        int32_t hr = pWicFactory->CreateBitmapFromMemory(16, 16, gdiplus::GUID_WICPixelFormat32bppPBGRA, 16 * 4, 16 * 16 * 4, reinterpret_cast<uint8_t*>(testPixels.data()), &pWicBmp);
        TEST_ASSERT(hr == ole32::S_OK && pWicBmp != nullptr, "WIC CreateBitmapFromMemory must succeed");

        ID2D1Factory* pFactory = nullptr;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_ID2D1Factory, nullptr, reinterpret_cast<void**>(&pFactory));
        D2D1_RENDER_TARGET_PROPERTIES rtProps{};
        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps{};
        hwndProps.hwnd = reinterpret_cast<void*>(0x1000);
        hwndProps.pixelSize = { 100, 100 };
        ID2D1HwndRenderTarget* pRt = nullptr;
        pFactory->CreateHwndRenderTarget(&rtProps, &hwndProps, &pRt);

        ID2D1Bitmap* pD2DBmp = nullptr;
        hr = pRt->CreateBitmapFromWicBitmap(pWicBmp, nullptr, &pD2DBmp);
        TEST_ASSERT(hr == ole32::S_OK && pD2DBmp != nullptr, "CreateBitmapFromWicBitmap must succeed");
        TEST_ASSERT(pD2DBmp->GetPixelSize().width == 16 && pD2DBmp->GetPixelSize().height == 16, "D2D Bitmap dimensions match WIC source");

        pD2DBmp->Release();
        pRt->Release();
        pFactory->Release();
        pWicBmp->Release();
        pWicFactory->Release();
    }

    // 14. Matrix Mathematics & Affine Inversion
    {
        Matrix3x2F ident = Matrix3x2F::Identity();
        TEST_ASSERT(ident.IsIdentity(), "Identity matrix verification");

        Matrix3x2F rot{};
        D2D1MakeRotateMatrix(90.0f, { 0.0f, 0.0f }, &rot);
        D2D1_POINT_2F p = rot.TransformPoint({ 1.0f, 0.0f });
        TEST_ASSERT(std::abs(p.x) < 0.001f && std::abs(p.y - 1.0f) < 0.001f, "90-degree rotation maps (1, 0) to (0, 1)");

        Matrix3x2F skew{};
        D2D1MakeSkewMatrix(10.0f, 0.0f, { 0.0f, 0.0f }, &skew);
        TEST_ASSERT(D2D1IsMatrixInvertible(&skew), "Skew matrix is invertible");

        Matrix3x2F inv = rot;
        bool invOk = D2D1InvertMatrix(&inv);
        TEST_ASSERT(invOk, "Rotation matrix inversion must succeed");
        D2D1_POINT_2F pBack = inv.TransformPoint(p);
        TEST_ASSERT(std::abs(pBack.x - 1.0f) < 0.001f && std::abs(pBack.y) < 0.001f, "Inverted matrix transforms back to (1, 0)");
    }

    // 15. Dynamic Loader Module Exports & COM Class Factory
    {
        InitializeDirect2DExports();

        auto& loader = ldr::DynamicLoader::get();
        auto* pfnCreate = loader.getExport("d2d1.dll", "D2D1CreateFactory");
        TEST_ASSERT(pfnCreate != nullptr, "d2d1.dll!D2D1CreateFactory resolved dynamically");

        auto* pfnCanUnload = loader.getExport("d2d1.dll", "DllCanUnloadNow");
        TEST_ASSERT(pfnCanUnload != nullptr, "d2d1.dll!DllCanUnloadNow resolved dynamically");

        ID2D1Factory* pCoFactory = nullptr;
        int32_t hr = ole32::CoCreateInstance(CLSID_D2D1Factory, nullptr, 1, IID_ID2D1Factory, reinterpret_cast<void**>(&pCoFactory));
        TEST_ASSERT(hr == ole32::S_OK && pCoFactory != nullptr, "CoCreateInstance CLSID_D2D1Factory must succeed");
        pCoFactory->Release();
    }

    // 16. Version Database & CommandShell Integration (d2d test, render, info)
    {
        // Version database verification
        auto& verDb = version::VersionDatabase::Instance();
        const auto* vD2d = verDb.FindModule("d2d1.dll");
        TEST_ASSERT(vD2d != nullptr && vD2d->stringTable.at("ProductName") == "MicaNT Direct2D Subsystem", "d2d1.dll version info match");

        // CommandShell Integration (d2d test, render, info)
        shell::CommandShell testShell;
        std::ostringstream out;

        testShell.execute("d2d test", out);
        TEST_ASSERT(out.str().find("ALL 16 TESTS PASSED (100%)") != std::string::npos, "d2d test must pass 100%");

        out.str("");
        testShell.execute("d2d render test_scene.bmp", out);
        TEST_ASSERT(out.str().find("Successfully rendered hardware-accelerated scene to target: 'test_scene.bmp'") != std::string::npos, "d2d render must save bitmap");

        out.str("");
        testShell.execute("d2d info", out);
        TEST_ASSERT(out.str().find("22621") != std::string::npos, "d2d info must display version");
    }

    std::cout << "[TEST] Suite 106: Windows Direct2D & DirectWrite Hardware Rendering Subsystem PASSED.\n";
}

// ============================================================================
// Suite 107: Windows Media Foundation Topology & Advanced Media Session Pipeline
// ============================================================================
void Test_WindowsMediaFoundation_Topology_And_Session_Subsystem() {
    std::cout << "[RUNNING] Test_WindowsMediaFoundation_Topology_And_Session_Subsystem...\n";

    // 1. Topology Loader Creation & Interface Query
    {
        mf::IMFTopoLoader* pLoader = nullptr;
        int32_t hr = mf::MFCreateTopoLoader(&pLoader);
        TEST_ASSERT(hr == ole32::S_OK && pLoader != nullptr, "MFCreateTopoLoader must succeed");

        ole32::IUnknown* pUnk = nullptr;
        hr = pLoader->QueryInterface(ole32::IID_IUnknown, reinterpret_cast<void**>(&pUnk));
        TEST_ASSERT(hr == ole32::S_OK && pUnk != nullptr, "IMFTopoLoader must implement IUnknown");
        pUnk->Release();

        pLoader->Release();
    }

    // 2. Presentation Clock Creation & Multi-Interface Query
    {
        mf::IMFPresentationClock* pClock = nullptr;
        int32_t hr = mf::MFCreatePresentationClock(&pClock);
        TEST_ASSERT(hr == ole32::S_OK && pClock != nullptr, "MFCreatePresentationClock must succeed");

        mf::IMFClock* pBaseClock = nullptr;
        hr = pClock->QueryInterface(mf::IID_IMFClock, reinterpret_cast<void**>(&pBaseClock));
        TEST_ASSERT(hr == ole32::S_OK && pBaseClock != nullptr, "IMFPresentationClock implements IMFClock");
        pBaseClock->Release();

        mf::IMFRateControl* pRateControl = nullptr;
        hr = pClock->QueryInterface(mf::IID_IMFRateControl, reinterpret_cast<void**>(&pRateControl));
        TEST_ASSERT(hr == ole32::S_OK && pRateControl != nullptr, "IMFPresentationClock implements IMFRateControl");
        pRateControl->Release();

        mf::IMFRateSupport* pRateSupport = nullptr;
        hr = pClock->QueryInterface(mf::IID_IMFRateSupport, reinterpret_cast<void**>(&pRateSupport));
        TEST_ASSERT(hr == ole32::S_OK && pRateSupport != nullptr, "IMFPresentationClock implements IMFRateSupport");
        pRateSupport->Release();

        pClock->Release();
    }

    // 3. Clock Characteristics & Properties
    {
        mf::IMFPresentationClock* pClock = nullptr;
        mf::MFCreatePresentationClock(&pClock);

        uint32_t characteristics = 0;
        int32_t hr = pClock->GetClockCharacteristics(&characteristics);
        TEST_ASSERT(hr == ole32::S_OK, "GetClockCharacteristics must succeed");
        TEST_ASSERT(characteristics & mf::MFCLOCK_CHARACTERISTICS_FLAG_FREQUENCY_10MHZ, "Clock must report 10MHz frequency");
        TEST_ASSERT(characteristics & mf::MFCLOCK_CHARACTERISTICS_FLAG_IS_SYSTEM_CLOCK, "Clock must report system clock flag");

        mf::MFCLOCK_PROPERTIES props{};
        hr = pClock->GetProperties(&props);
        TEST_ASSERT(hr == ole32::S_OK, "GetProperties must succeed");
        TEST_ASSERT(props.qwClockFrequency == 10000000, "Clock frequency is 10,000,000 Hz");
        TEST_ASSERT(props.guidClockId == mf::CLSID_MFPresentationClock, "Clock ID matches CLSID_MFPresentationClock");

        pClock->Release();
    }

    // 4. Presentation Clock State Transitions
    {
        mf::IMFPresentationClock* pClock = nullptr;
        mf::MFCreatePresentationClock(&pClock);

        mf::MFCLOCK_STATE state{};
        pClock->GetState(0, &state);
        TEST_ASSERT(state == mf::MFCLOCK_STATE_STOPPED, "Initial state is STOPPED");

        pClock->Start(0);
        pClock->GetState(0, &state);
        TEST_ASSERT(state == mf::MFCLOCK_STATE_RUNNING, "State after Start is RUNNING");

        pClock->Pause();
        pClock->GetState(0, &state);
        TEST_ASSERT(state == mf::MFCLOCK_STATE_PAUSED, "State after Pause is PAUSED");

        pClock->Start(1000);
        pClock->GetState(0, &state);
        TEST_ASSERT(state == mf::MFCLOCK_STATE_RUNNING, "State after resume is RUNNING");

        pClock->Stop();
        pClock->GetState(0, &state);
        TEST_ASSERT(state == mf::MFCLOCK_STATE_STOPPED, "State after Stop is STOPPED");

        pClock->Release();
    }

    // 5. Clock State Sink Registration & Event Dispatching
    {
        class TestClockSink : public mf::IMFClockStateSink {
        public:
            uint32_t startCount{ 0 }, stopCount{ 0 }, pauseCount{ 0 }, rateCount{ 0 };
            uint32_t refCount{ 1 };

            int32_t __stdcall QueryInterface(const GUID& riid, void** ppv) override {
                if (!ppv) return ole32::E_POINTER;
                if (riid == ole32::IID_IUnknown || riid == mf::IID_IMFClockStateSink) {
                    *ppv = this;
                    AddRef();
                    return ole32::S_OK;
                }
                *ppv = nullptr;
                return ole32::E_NOINTERFACE;
            }
            uint32_t __stdcall AddRef() override { return ++refCount; }
            uint32_t __stdcall Release() override {
                uint32_t r = --refCount;
                if (r == 0) delete this;
                return r;
            }
            int32_t __stdcall OnClockStart(mf::MFTIME, mf::LONGLONG) override { startCount++; return ole32::S_OK; }
            int32_t __stdcall OnClockStop(mf::MFTIME) override { stopCount++; return ole32::S_OK; }
            int32_t __stdcall OnClockPause(mf::MFTIME) override { pauseCount++; return ole32::S_OK; }
            int32_t __stdcall OnClockRestart(mf::MFTIME) override { return ole32::S_OK; }
            int32_t __stdcall OnClockSetRate(mf::MFTIME, float) override { rateCount++; return ole32::S_OK; }
        };

        auto* pSink = new TestClockSink();
        mf::IMFPresentationClock* pClock = nullptr;
        mf::MFCreatePresentationClock(&pClock);

        int32_t hr = pClock->AddClockStateSink(pSink);
        TEST_ASSERT(hr == ole32::S_OK, "AddClockStateSink must succeed");

        pClock->Start(500);
        TEST_ASSERT(pSink->startCount == 1, "OnClockStart called on Start");

        pClock->Pause();
        TEST_ASSERT(pSink->pauseCount == 1, "OnClockPause called on Pause");

        pClock->Stop();
        TEST_ASSERT(pSink->stopCount == 1, "OnClockStop called on Stop");

        hr = pClock->RemoveClockStateSink(pSink);
        TEST_ASSERT(hr == ole32::S_OK, "RemoveClockStateSink must succeed");

        pClock->Start(0);
        TEST_ASSERT(pSink->startCount == 1, "Removed sink does not receive subsequent events");
        pClock->Stop();

        pSink->Release();
        pClock->Release();
    }

    // 6. Clock Sample-Accurate 100ns Timestamps & Continuity Key
    {
        mf::IMFPresentationClock* pClock = nullptr;
        mf::MFCreatePresentationClock(&pClock);

        uint32_t key1 = 0;
        pClock->GetContinuityKey(&key1);

        pClock->Start(10000000); // 1.0 second offset (10,000,000 hns)
        uint32_t key2 = 0;
        pClock->GetContinuityKey(&key2);
        TEST_ASSERT(key2 > key1, "Continuity key increments on Start");

        mf::MFTIME t = 0;
        pClock->GetTime(&t);
        TEST_ASSERT(t >= 10000000, "Clock time respects initial offset");

        mf::LONGLONG clockTime = 0;
        mf::MFTIME sysTime = 0;
        pClock->GetCorrelatedTime(0, &clockTime, &sysTime);
        TEST_ASSERT(clockTime >= 10000000, "Correlated clock time matches running offset");
        TEST_ASSERT(sysTime > 0, "Correlated system time is non-zero");

        pClock->Stop();
        pClock->Release();
    }

    // 7. Rate Control Interface (IMFRateControl)
    {
        mf::IMFPresentationClock* pClock = nullptr;
        mf::MFCreatePresentationClock(&pClock);

        mf::IMFRateControl* pRateControl = nullptr;
        pClock->QueryInterface(mf::IID_IMFRateControl, reinterpret_cast<void**>(&pRateControl));
        TEST_ASSERT(pRateControl != nullptr, "IMFRateControl query must succeed");

        float r = 0.0f;
        int32_t thin = 0;
        pRateControl->GetRate(&thin, &r);
        TEST_ASSERT(std::abs(r - 1.0f) < 0.001f, "Default playback rate is 1.0x");

        pRateControl->SetRate(0, 2.5f);
        pRateControl->GetRate(&thin, &r);
        TEST_ASSERT(std::abs(r - 2.5f) < 0.001f, "Playback rate updated to 2.5x");

        pRateControl->SetRate(1, 0.5f);
        pRateControl->GetRate(&thin, &r);
        TEST_ASSERT(std::abs(r - 0.5f) < 0.001f && thin == 1, "Thinned playback rate set to 0.5x");

        pRateControl->Release();
        pClock->Release();
    }

    // 8. Rate Support Range Validation (IMFRateSupport)
    {
        mf::IMFPresentationClock* pClock = nullptr;
        mf::MFCreatePresentationClock(&pClock);

        mf::IMFRateSupport* pRateSupport = nullptr;
        pClock->QueryInterface(mf::IID_IMFRateSupport, reinterpret_cast<void**>(&pRateSupport));
        TEST_ASSERT(pRateSupport != nullptr, "IMFRateSupport query must succeed");

        float slowForward = 0.0f;
        pRateSupport->GetSlowestRate(mf::MFRATE_FORWARD, 0, &slowForward);
        TEST_ASSERT(slowForward > 0.0f, "Slowest forward rate is positive");

        float fastForward = 0.0f;
        pRateSupport->GetFastestRate(mf::MFRATE_FORWARD, 0, &fastForward);
        TEST_ASSERT(fastForward >= 16.0f, "Fastest forward rate is at least 16.0x");

        float slowReverse = 0.0f;
        pRateSupport->GetSlowestRate(mf::MFRATE_REVERSE, 0, &slowReverse);
        TEST_ASSERT(slowReverse < 0.0f, "Slowest reverse rate is negative");

        float nearest = 0.0f;
        int32_t hr = pRateSupport->IsRateSupported(0, 2.0f, &nearest);
        TEST_ASSERT(hr == ole32::S_OK && std::abs(nearest - 2.0f) < 0.001f, "2.0x is supported rate");

        hr = pRateSupport->IsRateSupported(0, 64.0f, &nearest);
        TEST_ASSERT(hr == mf::MF_E_UNSUPPORTED_RATE, "64.0x is beyond supported rate range");

        pRateSupport->Release();
        pClock->Release();
    }

    // 9. Media Sequencer Source Creation
    {
        mf::IMFSequencerSource* pSeq = nullptr;
        int32_t hr = mf::MFCreateSequencerSource(nullptr, &pSeq);
        TEST_ASSERT(hr == ole32::S_OK && pSeq != nullptr, "MFCreateSequencerSource must succeed");

        ole32::IUnknown* pUnk = nullptr;
        hr = pSeq->QueryInterface(ole32::IID_IUnknown, reinterpret_cast<void**>(&pUnk));
        TEST_ASSERT(hr == ole32::S_OK && pUnk != nullptr, "IMFSequencerSource implements IUnknown");
        pUnk->Release();

        pSeq->Release();
    }

    // 10. Sequencer Source Topology Playlist Management
    {
        mf::IMFSequencerSource* pSeq = nullptr;
        mf::MFCreateSequencerSource(nullptr, &pSeq);

        mf::IMFTopology* pTopo1 = nullptr;
        mf::MFCreateTopology(&pTopo1);
        mf::IMFTopology* pTopo2 = nullptr;
        mf::MFCreateTopology(&pTopo2);

        uint32_t id1 = 0, id2 = 0;
        int32_t hr1 = pSeq->AppendTopology(pTopo1, mf::MFSequencerFlag_Base, &id1);
        int32_t hr2 = pSeq->AppendTopology(pTopo2, mf::MFSequencerFlag_Append, &id2);
        TEST_ASSERT(hr1 == ole32::S_OK && hr2 == ole32::S_OK, "AppendTopology must succeed");
        TEST_ASSERT(id1 != id2 && id1 >= 1001, "Sequence IDs are unique and sequenced");

        mf::IMFTopology* pContext = nullptr;
        pSeq->GetPresentationContext(id1, &pContext);
        TEST_ASSERT(pContext == pTopo1, "GetPresentationContext retrieves matching topology");
        pContext->Release();

        int32_t hrUpd = pSeq->UpdateTopologyFlags(id2, mf::MFSequencerFlag_PreRoll);
        TEST_ASSERT(hrUpd == ole32::S_OK, "UpdateTopologyFlags must succeed");

        int32_t hrDel = pSeq->DeleteTopology(id1);
        TEST_ASSERT(hrDel == ole32::S_OK, "DeleteTopology must succeed");

        int32_t hrGet = pSeq->GetPresentationContext(id1, &pContext);
        TEST_ASSERT(hrGet == mf::MF_E_NOT_FOUND, "Deleted topology cannot be retrieved");

        pTopo2->Release();
        pTopo1->Release();
        pSeq->Release();
    }

    // 11. Windows Media Video Decoder MFT (CWMVDecoderMFT)
    {
        auto* pWmvDec = new mf::CWMVDecoderMFT();
        TEST_ASSERT(pWmvDec->GetClsid() == mf::CLSID_CWMVDecMediaObject, "CWMVDecoderMFT matches CLSID");

        // Input formats: WMV1, WMV2, WMV3, WVC1
        const GUID expectedInputs[] = {
            mf::MFVideoFormat_WMV1,
            mf::MFVideoFormat_WMV2,
            mf::MFVideoFormat_WMV3,
            mf::MFVideoFormat_WVC1
        };
        for (uint32_t i = 0; i < 4; ++i) {
            mf::IMFMediaType* pInType = nullptr;
            int32_t hr = pWmvDec->GetInputAvailableType(0, i, &pInType);
            TEST_ASSERT(hr == ole32::S_OK && pInType != nullptr, "Available input type must succeed");
            GUID sub{};
            pInType->GetGUID(mf::MF_MT_SUBTYPE, &sub);
            TEST_ASSERT(sub == expectedInputs[i], "Input subtype matches expected WMV FourCC");
            pInType->Release();
        }

        // Output formats: NV12, RGB32
        mf::IMFMediaType* pOutType = nullptr;
        pWmvDec->GetOutputAvailableType(0, 0, &pOutType);
        GUID outSub{};
        pOutType->GetGUID(mf::MF_MT_SUBTYPE, &outSub);
        TEST_ASSERT(outSub == mf::MFVideoFormat_NV12, "Default output format is NV12");
        pOutType->Release();

        pWmvDec->Release();
    }

    // 12. Windows Media Audio Decoder MFT (CWMADecoderMFT)
    {
        auto* pWmaDec = new mf::CWMADecoderMFT();
        TEST_ASSERT(pWmaDec->GetClsid() == mf::CLSID_CWMADecMediaObject, "CWMADecoderMFT matches CLSID");

        // Input formats: WMAudioV8, WMAudioV9, WMAudio_Lossless
        const GUID expectedInputs[] = {
            mf::MFAudioFormat_WMAudioV8,
            mf::MFAudioFormat_WMAudioV9,
            mf::MFAudioFormat_WMAudio_Lossless
        };
        for (uint32_t i = 0; i < 3; ++i) {
            mf::IMFMediaType* pInType = nullptr;
            int32_t hr = pWmaDec->GetInputAvailableType(0, i, &pInType);
            TEST_ASSERT(hr == ole32::S_OK && pInType != nullptr, "Available input type must succeed");
            GUID sub{};
            pInType->GetGUID(mf::MF_MT_SUBTYPE, &sub);
            TEST_ASSERT(sub == expectedInputs[i], "Input subtype matches expected WMA format tag");
            pInType->Release();
        }

        // Output formats: PCM, Float
        mf::IMFMediaType* pOutType = nullptr;
        pWmaDec->GetOutputAvailableType(0, 0, &pOutType);
        GUID outSub{};
        pOutType->GetGUID(mf::MF_MT_SUBTYPE, &outSub);
        TEST_ASSERT(outSub == mf::MFAudioFormat_PCM, "Default audio output format is PCM");
        pOutType->Release();

        pWmaDec->Release();
    }

    // 13. TopoLoader Partial-to-Full Resolution: WMV3 Video Pipeline
    {
        mf::IMFTopoLoader* pLoader = nullptr;
        mf::MFCreateTopoLoader(&pLoader);

        mf::IMFTopology* pPartialTopo = nullptr;
        mf::MFCreateTopology(&pPartialTopo);

        mf::IMFTopologyNode* pSrc = nullptr;
        mf::MFCreateTopologyNode(mf::MF_TOPOLOGY_SOURCESTREAM_NODE, &pSrc);
        pSrc->SetGUID(mf::MF_MT_MAJOR_TYPE, mf::MFMediaType_Video);
        pSrc->SetGUID(mf::MF_MT_SUBTYPE, mf::MFVideoFormat_WMV3);

        mf::IMFTopologyNode* pSink = nullptr;
        mf::MFCreateTopologyNode(mf::MF_TOPOLOGY_OUTPUT_NODE, &pSink);
        pSink->SetGUID(mf::MF_MT_MAJOR_TYPE, mf::MFMediaType_Video);
        pSink->SetGUID(mf::MF_MT_SUBTYPE, mf::MFVideoFormat_RGB32);

        pPartialTopo->AddNode(pSrc);
        pPartialTopo->AddNode(pSink);
        pSrc->ConnectOutput(0, pSink, 0);

        mf::IMFTopology* pFullTopo = nullptr;
        int32_t hr = pLoader->Load(pPartialTopo, &pFullTopo, nullptr);
        TEST_ASSERT(hr == ole32::S_OK && pFullTopo != nullptr, "TopoLoader Load must succeed");

        uint16_t nodeCount = 0;
        pFullTopo->GetNodeCount(&nodeCount);
        // Resolved graph: Source (WMV3) -> WMV Decoder -> Color Converter -> Sink (RGB32) = 4 nodes
        TEST_ASSERT(nodeCount == 4, "Resolved video topology contains 4 nodes");

        pFullTopo->Release();
        pSink->Release();
        pSrc->Release();
        pPartialTopo->Release();
        pLoader->Release();
    }

    // 14. TopoLoader Partial-to-Full Resolution: WMA Audio Pipeline
    {
        mf::IMFTopoLoader* pLoader = nullptr;
        mf::MFCreateTopoLoader(&pLoader);

        mf::IMFTopology* pPartialTopo = nullptr;
        mf::MFCreateTopology(&pPartialTopo);

        mf::IMFTopologyNode* pSrc = nullptr;
        mf::MFCreateTopologyNode(mf::MF_TOPOLOGY_SOURCESTREAM_NODE, &pSrc);
        pSrc->SetGUID(mf::MF_MT_MAJOR_TYPE, mf::MFMediaType_Audio);
        pSrc->SetGUID(mf::MF_MT_SUBTYPE, mf::MFAudioFormat_WMAudioV9);

        mf::IMFTopologyNode* pSink = nullptr;
        mf::MFCreateTopologyNode(mf::MF_TOPOLOGY_OUTPUT_NODE, &pSink);
        pSink->SetGUID(mf::MF_MT_MAJOR_TYPE, mf::MFMediaType_Audio);
        pSink->SetGUID(mf::MF_MT_SUBTYPE, mf::MFAudioFormat_PCM);

        pPartialTopo->AddNode(pSrc);
        pPartialTopo->AddNode(pSink);
        pSrc->ConnectOutput(0, pSink, 0);

        mf::IMFTopology* pFullTopo = nullptr;
        int32_t hr = pLoader->Load(pPartialTopo, &pFullTopo, nullptr);
        TEST_ASSERT(hr == ole32::S_OK && pFullTopo != nullptr, "TopoLoader Load must succeed");

        uint16_t nodeCount = 0;
        pFullTopo->GetNodeCount(&nodeCount);
        // Resolved graph: Source (WMA9) -> WMA Decoder -> Audio Sink (PCM) = 3 nodes
        TEST_ASSERT(nodeCount == 3, "Resolved audio topology contains 3 nodes");

        pFullTopo->Release();
        pSink->Release();
        pSrc->Release();
        pPartialTopo->Release();
        pLoader->Release();
    }

    // 15. Dynamic Module Exports (mf.dll & wmvdecod.dll)
    {
        mf::InitializeMediaFoundationSessionExports();

        auto& loader = ldr::DynamicLoader::get();
        TEST_ASSERT(loader.getExport("mf.dll", "MFCreateTopoLoader") != nullptr, "mf.dll!MFCreateTopoLoader resolved");
        TEST_ASSERT(loader.getExport("mf.dll", "MFCreatePresentationClock") != nullptr, "mf.dll!MFCreatePresentationClock resolved");
        TEST_ASSERT(loader.getExport("mf.dll", "MFCreateSequencerSource") != nullptr, "mf.dll!MFCreateSequencerSource resolved");
        TEST_ASSERT(loader.getExport("wmvdecod.dll", "DllCanUnloadNow") != nullptr, "wmvdecod.dll!DllCanUnloadNow resolved");
        TEST_ASSERT(loader.getExport("wmvdecod.dll", "DllGetClassObject") != nullptr, "wmvdecod.dll!DllGetClassObject resolved");
    }

    // 16. OLE32 COM Class Factory Activation & Version Database Integration
    {
        // COM activation via CoCreateInstance
        mf::IMFTransform* pWmvTransform = nullptr;
        int32_t hr = ole32::CoCreateInstance(mf::CLSID_CWMVDecMediaObject, nullptr, 1, mf::IID_IMFTransform, reinterpret_cast<void**>(&pWmvTransform));
        TEST_ASSERT(hr == ole32::S_OK && pWmvTransform != nullptr, "CoCreateInstance CLSID_CWMVDecMediaObject must succeed");
        pWmvTransform->Release();

        mf::IMFTransform* pWmaTransform = nullptr;
        hr = ole32::CoCreateInstance(mf::CLSID_CWMADecMediaObject, nullptr, 1, mf::IID_IMFTransform, reinterpret_cast<void**>(&pWmaTransform));
        TEST_ASSERT(hr == ole32::S_OK && pWmaTransform != nullptr, "CoCreateInstance CLSID_CWMADecMediaObject must succeed");
        pWmaTransform->Release();

        // Version database verification
        auto& verDb = version::VersionDatabase::Instance();
        const auto* vWmv = verDb.FindModule("wmvdecod.dll");
        TEST_ASSERT(vWmv != nullptr && vWmv->stringTable.at("ProductName") == "MicaNT WMV & WMA Codec Subsystem", "wmvdecod.dll version info match");

        // Shell command integration (mfsession test, topology, info)
        shell::CommandShell testShell;
        std::ostringstream out;

        testShell.execute("mfsession test", out);
        TEST_ASSERT(out.str().find("ALL 16 TESTS PASSED (100%)") != std::string::npos, "mfsession test must pass 100%");

        out.str("");
        testShell.execute("mfsession topology test_clip.wmv", out);
        TEST_ASSERT(out.str().find("Pipeline Status: READY") != std::string::npos, "mfsession topology must resolve pipeline");

        out.str("");
        testShell.execute("mfsession info", out);
        TEST_ASSERT(out.str().find("22621") != std::string::npos, "mfsession info must display telemetry");
    }

    std::cout << "[TEST] Suite 107: Windows Media Foundation Topology & Advanced Media Session Pipeline PASSED.\n";
}

void Test_WindowsEnhancedVideoRenderer_Subsystem() {
    std::cout << "[TEST] Suite 108: Windows Enhanced Video Renderer (EVR) Subsystem...\n";

    // Initialize EVR exports
    mf::evr::InitializeEnhancedVideoRendererExports();

    // 1. EVR Media Sink Creation & Interface Query
    {
        mf::evr::IMFMediaSink* pSink = nullptr;
        int32_t hr = mf::evr::MFCreateVideoRenderer(mf::evr::IID_IMFMediaSink, reinterpret_cast<void**>(&pSink));
        TEST_ASSERT(hr == ole32::S_OK && pSink != nullptr, "MFCreateVideoRenderer must succeed for IMFMediaSink");

        mf::evr::IMFVideoRenderer* pRenderer = nullptr;
        hr = pSink->QueryInterface(mf::evr::IID_IMFVideoRenderer, reinterpret_cast<void**>(&pRenderer));
        TEST_ASSERT(hr == ole32::S_OK && pRenderer != nullptr, "QueryInterface IMFVideoRenderer must succeed");

        mf::evr::IEVRFilterConfig* pConfig = nullptr;
        hr = pSink->QueryInterface(mf::evr::IID_IEVRFilterConfig, reinterpret_cast<void**>(&pConfig));
        TEST_ASSERT(hr == ole32::S_OK && pConfig != nullptr, "QueryInterface IEVRFilterConfig must succeed");

        mf::evr::IMFGetService* pGetService = nullptr;
        hr = pSink->QueryInterface(mf::evr::IID_IMFGetService, reinterpret_cast<void**>(&pGetService));
        TEST_ASSERT(hr == ole32::S_OK && pGetService != nullptr, "QueryInterface IMFGetService must succeed");

        mf::IMFClockStateSink* pClockSink = nullptr;
        hr = pSink->QueryInterface(mf::IID_IMFClockStateSink, reinterpret_cast<void**>(&pClockSink));
        TEST_ASSERT(hr == ole32::S_OK && pClockSink != nullptr, "QueryInterface IMFClockStateSink must succeed");

        pClockSink->Release();
        pGetService->Release();
        pConfig->Release();
        pRenderer->Release();
        pSink->Release();
    }

    // 2. EVR Filter Configuration (IEVRFilterConfig)
    {
        mf::evr::IMFMediaSink* pSink = nullptr;
        mf::evr::MFCreateVideoRenderer(mf::evr::IID_IMFMediaSink, reinterpret_cast<void**>(&pSink));
        mf::evr::IEVRFilterConfig* pConfig = nullptr;
        pSink->QueryInterface(mf::evr::IID_IEVRFilterConfig, reinterpret_cast<void**>(&pConfig));

        uint32_t maxStreams = 0;
        pConfig->GetNumberOfStreams(&maxStreams);
        TEST_ASSERT(maxStreams == 1, "Default EVR stream count is 1");

        int32_t hr = pConfig->SetNumberOfStreams(4);
        TEST_ASSERT(hr == ole32::S_OK, "SetNumberOfStreams(4) must succeed");

        pConfig->GetNumberOfStreams(&maxStreams);
        TEST_ASSERT(maxStreams == 4, "EVR stream count is now 4");

        // Boundary tests
        TEST_ASSERT(pConfig->SetNumberOfStreams(0) == ole32::E_INVALIDARG, "SetNumberOfStreams(0) must fail with E_INVALIDARG");
        TEST_ASSERT(pConfig->SetNumberOfStreams(17) == ole32::E_INVALIDARG, "SetNumberOfStreams(17) must fail with E_INVALIDARG");

        pConfig->Release();
        pSink->Release();
    }

    // 3. EVR Stream Sinks (IMFStreamSink & IMFMediaSink)
    {
        mf::evr::IMFMediaSink* pSink = nullptr;
        mf::evr::MFCreateVideoRenderer(mf::evr::IID_IMFMediaSink, reinterpret_cast<void**>(&pSink));
        mf::evr::IEVRFilterConfig* pConfig = nullptr;
        pSink->QueryInterface(mf::evr::IID_IEVRFilterConfig, reinterpret_cast<void**>(&pConfig));
        pConfig->SetNumberOfStreams(3);

        uint32_t streamCount = 0;
        pSink->GetStreamSinkCount(&streamCount);
        TEST_ASSERT(streamCount == 3, "Stream sink count matches configured 3 streams");

        for (uint32_t i = 0; i < 3; ++i) {
            mf::evr::IMFStreamSink* pStream = nullptr;
            int32_t hr = pSink->GetStreamSinkByIndex(i, &pStream);
            TEST_ASSERT(hr == ole32::S_OK && pStream != nullptr, "GetStreamSinkByIndex must succeed");
            uint32_t id = 999;
            pStream->GetIdentifier(&id);
            TEST_ASSERT(id == i, "Stream identifier matches index");

            mf::evr::IMFMediaSink* pParent = nullptr;
            pStream->GetMediaSink(&pParent);
            TEST_ASSERT(pParent == pSink, "Stream parent matches EVR sink");
            pParent->Release();
            pStream->Release();
        }

        pConfig->Release();
        pSink->Release();
    }

    // 4. Stream Sink Media Type Handler (IMFMediaTypeHandler)
    {
        mf::evr::IMFMediaSink* pSink = nullptr;
        mf::evr::MFCreateVideoRenderer(mf::evr::IID_IMFMediaSink, reinterpret_cast<void**>(&pSink));
        mf::evr::IMFStreamSink* pStream = nullptr;
        pSink->GetStreamSinkByIndex(0, &pStream);

        mf::evr::IMFMediaTypeHandler* pHandler = nullptr;
        int32_t hr = pStream->GetMediaTypeHandler(&pHandler);
        TEST_ASSERT(hr == ole32::S_OK && pHandler != nullptr, "GetMediaTypeHandler must succeed");

        GUID majType{};
        pHandler->GetMajorType(&majType);
        TEST_ASSERT(majType == mf::MFMediaType_Video, "Major type must be MFMediaType_Video");

        uint32_t count = 0;
        pHandler->GetMediaTypeCount(&count);
        TEST_ASSERT(count >= 3, "MediaType count must include RGB32, NV12, YUY2");

        auto* pMt = new mf::CMediaType();
        pMt->SetGUID(mf::MF_MT_MAJOR_TYPE, mf::MFMediaType_Video);
        pMt->SetGUID(mf::MF_MT_SUBTYPE, mf::MFVideoFormat_RGB32);
        TEST_ASSERT(pHandler->IsMediaTypeSupported(pMt, nullptr) == ole32::S_OK, "RGB32 must be supported");
        TEST_ASSERT(pHandler->SetCurrentMediaType(pMt) == ole32::S_OK, "SetCurrentMediaType must succeed");

        mf::IMFMediaType* pCur = nullptr;
        pHandler->GetCurrentMediaType(&pCur);
        TEST_ASSERT(pCur != nullptr, "GetCurrentMediaType returns current type");
        GUID curSub{};
        pCur->GetGUID(mf::MF_MT_SUBTYPE, &curSub);
        TEST_ASSERT(curSub == mf::MFVideoFormat_RGB32, "Current subtype matches RGB32");

        pCur->Release();
        pMt->Release();
        pHandler->Release();
        pStream->Release();
        pSink->Release();
    }

    // 5. EVR Custom Mixer & Presenter Initialization (IMFVideoRenderer)
    {
        mf::evr::IMFMediaSink* pSink = nullptr;
        mf::evr::MFCreateVideoRenderer(mf::evr::IID_IMFMediaSink, reinterpret_cast<void**>(&pSink));
        mf::evr::IMFVideoRenderer* pRenderer = nullptr;
        pSink->QueryInterface(mf::evr::IID_IMFVideoRenderer, reinterpret_cast<void**>(&pRenderer));

        auto* pCustomMixer = new mf::evr::CEVRMixer();
        auto* pCustomPresenter = new mf::evr::CEVRPresenter();

        int32_t hr = pRenderer->InitializeRenderer(pCustomMixer, pCustomPresenter);
        TEST_ASSERT(hr == ole32::S_OK, "InitializeRenderer with custom mixer & presenter must succeed");

        pCustomPresenter->Release();
        pCustomMixer->Release();
        pRenderer->Release();
        pSink->Release();
    }

    // 6. EVR Service Provider (IMFGetService & MR_VIDEO_RENDER_SERVICE)
    {
        mf::evr::IMFMediaSink* pSink = nullptr;
        mf::evr::MFCreateVideoRenderer(mf::evr::IID_IMFMediaSink, reinterpret_cast<void**>(&pSink));
        mf::evr::IMFGetService* pGetService = nullptr;
        pSink->QueryInterface(mf::evr::IID_IMFGetService, reinterpret_cast<void**>(&pGetService));

        mf::evr::IMFVideoDisplayControl* pDisplay = nullptr;
        int32_t hr = pGetService->GetService(mf::evr::MR_VIDEO_RENDER_SERVICE, mf::evr::IID_IMFVideoDisplayControl, reinterpret_cast<void**>(&pDisplay));
        TEST_ASSERT(hr == ole32::S_OK && pDisplay != nullptr, "GetService MR_VIDEO_RENDER_SERVICE -> IMFVideoDisplayControl succeeds");

        mf::evr::IMFVideoPresenter* pPres = nullptr;
        hr = pGetService->GetService(mf::evr::MR_VIDEO_RENDER_SERVICE, mf::evr::IID_IMFVideoPresenter, reinterpret_cast<void**>(&pPres));
        TEST_ASSERT(hr == ole32::S_OK && pPres != nullptr, "GetService MR_VIDEO_RENDER_SERVICE -> IMFVideoPresenter succeeds");

        void* pInvalid = nullptr;
        GUID invalidService = { 0x11111111, 0x2222, 0x3333, { 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb } };
        hr = pGetService->GetService(invalidService, mf::evr::IID_IMFVideoDisplayControl, &pInvalid);
        TEST_ASSERT(hr == mf::evr::MF_E_UNSUPPORTED_SERVICE, "Unsupported service GUID returns MF_E_UNSUPPORTED_SERVICE");

        pPres->Release();
        pDisplay->Release();
        pGetService->Release();
        pSink->Release();
    }

    // 7. EVR Service Provider (IMFGetService & MR_VIDEO_MIXING_SERVICE)
    {
        mf::evr::IMFMediaSink* pSink = nullptr;
        mf::evr::MFCreateVideoRenderer(mf::evr::IID_IMFMediaSink, reinterpret_cast<void**>(&pSink));
        mf::evr::IMFGetService* pGetService = nullptr;
        pSink->QueryInterface(mf::evr::IID_IMFGetService, reinterpret_cast<void**>(&pGetService));

        mf::evr::IMFVideoMixerControl* pMixCtrl = nullptr;
        int32_t hr = pGetService->GetService(mf::evr::MR_VIDEO_MIXING_SERVICE, mf::evr::IID_IMFVideoMixerControl, reinterpret_cast<void**>(&pMixCtrl));
        TEST_ASSERT(hr == ole32::S_OK && pMixCtrl != nullptr, "GetService MR_VIDEO_MIXING_SERVICE -> IMFVideoMixerControl succeeds");

        mf::evr::IMFVideoMixerBitmap* pMixBmp = nullptr;
        hr = pGetService->GetService(mf::evr::MR_VIDEO_MIXING_SERVICE, mf::evr::IID_IMFVideoMixerBitmap, reinterpret_cast<void**>(&pMixBmp));
        TEST_ASSERT(hr == ole32::S_OK && pMixBmp != nullptr, "GetService MR_VIDEO_MIXING_SERVICE -> IMFVideoMixerBitmap succeeds");

        mf::IMFTransform* pTransform = nullptr;
        hr = pGetService->GetService(mf::evr::MR_VIDEO_MIXING_SERVICE, mf::IID_IMFTransform, reinterpret_cast<void**>(&pTransform));
        TEST_ASSERT(hr == ole32::S_OK && pTransform != nullptr, "GetService MR_VIDEO_MIXING_SERVICE -> IMFTransform succeeds");

        pTransform->Release();
        pMixBmp->Release();
        pMixCtrl->Release();
        pGetService->Release();
        pSink->Release();
    }

    // 8. EVR Mixer Multi-Stream Configuration (IMFVideoMixerControl)
    {
        auto* pMixer = new mf::evr::CEVRMixer();
        uint32_t streamIDs[2] = { 1, 2 };
        pMixer->AddInputStreams(2, streamIDs);

        // Stream 1 Z-order
        pMixer->SetStreamZOrder(1, 5);
        uint32_t zOrder = 0;
        pMixer->GetStreamZOrder(1, &zOrder);
        TEST_ASSERT(zOrder == 5, "Stream 1 Z-Order matches 5");

        // Stream 2 Normalized Rect
        mf::evr::MFVideoNormalizedRect pipRect{ 0.5f, 0.5f, 1.0f, 1.0f };
        pMixer->SetStreamOutputRect(2, &pipRect);
        mf::evr::MFVideoNormalizedRect queryRect{};
        pMixer->GetStreamOutputRect(2, &queryRect);
        TEST_ASSERT(queryRect == pipRect, "Stream 2 Normalized Rect matches quad");

        // Invalid stream number returns error
        TEST_ASSERT(pMixer->SetStreamZOrder(99, 1) == mf::evr::MF_E_INVALIDSTREAMNUMBER, "Invalid stream returns MF_E_INVALIDSTREAMNUMBER");

        pMixer->Release();
    }

    // 9. EVR Mixer Alpha Bitmap Overlay (IMFVideoMixerBitmap)
    {
        auto* pMixer = new mf::evr::CEVRMixer();

        mf::evr::MFVideoAlphaBitmap bmpParam{};
        bmpParam.params.dwFlags = mf::evr::MFVideoAlphaBitmap_Alpha | mf::evr::MFVideoAlphaBitmap_DestRect;
        bmpParam.params.fAlpha = 0.75f;
        bmpParam.params.nrcDest = { 0.8f, 0.8f, 1.0f, 1.0f };

        int32_t hr = pMixer->SetAlphaBitmap(&bmpParam);
        TEST_ASSERT(hr == ole32::S_OK, "SetAlphaBitmap succeeds");

        mf::evr::MFVideoAlphaBitmapParams retParams{};
        hr = pMixer->GetAlphaBitmapParameters(&retParams);
        TEST_ASSERT(hr == ole32::S_OK && std::abs(retParams.fAlpha - 0.75f) < 0.001f, "GetAlphaBitmapParameters returns set alpha 0.75");

        retParams.fAlpha = 0.50f;
        hr = pMixer->UpdateAlphaBitmapParameters(&retParams);
        TEST_ASSERT(hr == ole32::S_OK, "UpdateAlphaBitmapParameters succeeds");

        pMixer->GetAlphaBitmapParameters(&retParams);
        TEST_ASSERT(std::abs(retParams.fAlpha - 0.50f) < 0.001f, "Alpha updated to 0.50");

        hr = pMixer->ClearAlphaBitmap();
        TEST_ASSERT(hr == ole32::S_OK, "ClearAlphaBitmap succeeds");
        TEST_ASSERT(pMixer->GetAlphaBitmapParameters(&retParams) == ole32::E_FAIL, "GetAlphaBitmapParameters fails after clear");

        pMixer->Release();
    }

    // 10. EVR Mixer Frame Processing (IMFTransform)
    {
        auto* pMixer = new mf::evr::CEVRMixer();

        mf::IMFSample* pSample = nullptr;
        mf::MFCreateSample(&pSample);
        mf::IMFMediaBuffer* pBuf = nullptr;
        mf::MFCreateMemoryBuffer(640 * 480 * 4, &pBuf);
        pSample->AddBuffer(pBuf);
        pSample->SetSampleTime(12345678);
        pSample->SetSampleDuration(333333);

        int32_t hr = pMixer->ProcessInput(0, pSample, 0);
        TEST_ASSERT(hr == ole32::S_OK, "ProcessInput on stream 0 succeeds");

        mf::MFT_OUTPUT_DATA_BUFFER outData{};
        uint32_t status = 0;
        hr = pMixer->ProcessOutput(0, 1, &outData, &status);
        TEST_ASSERT(hr == ole32::S_OK && outData.pSample != nullptr, "ProcessOutput succeeds");

        int64_t sampleTime = 0;
        outData.pSample->GetSampleTime(&sampleTime);
        TEST_ASSERT(sampleTime == 12345678, "Output sample time matches input");
        TEST_ASSERT(pMixer->GetMixedSampleCount() == 1, "Mixed sample count is 1");

        outData.pSample->Release();
        pBuf->Release();
        pSample->Release();
        pMixer->Release();
    }

    // 11. EVR Presenter Display Control (IMFVideoDisplayControl)
    {
        auto* pPres = new mf::evr::CEVRPresenter();

        gdi32::SIZE nativeSz{}, arSz{};
        pPres->GetNativeVideoSize(&nativeSz, &arSz);
        TEST_ASSERT(nativeSz.cx == 1920 && nativeSz.cy == 1080, "Native video size is 1920x1080");
        TEST_ASSERT(arSz.cx == 16 && arSz.cy == 9, "Pixel aspect ratio is 16:9");

        gdi32::SIZE minSz{}, maxSz{};
        pPres->GetIdealVideoSize(&minSz, &maxSz);
        TEST_ASSERT(minSz.cx == 160 && maxSz.cx == 3840, "Ideal video sizes range 160 to 3840");

        mf::evr::MFVideoNormalizedRect srcR{ 0.1f, 0.1f, 0.9f, 0.9f };
        gdi32::RECT dstR{ 0, 0, 800, 600 };
        pPres->SetVideoPosition(&srcR, &dstR);

        mf::evr::MFVideoNormalizedRect querySrc{};
        gdi32::RECT queryDst{};
        pPres->GetVideoPosition(&querySrc, &queryDst);
        TEST_ASSERT(querySrc == srcR && queryDst.right == 800 && queryDst.bottom == 600, "GetVideoPosition matches set position");

        pPres->SetAspectRatioMode(mf::evr::MFVideoARMode_PreservePicture);
        uint32_t arMode = 0;
        pPres->GetAspectRatioMode(&arMode);
        TEST_ASSERT(arMode == mf::evr::MFVideoARMode_PreservePicture, "Aspect ratio mode matches PreservePicture");

        pPres->SetBorderColor(0x00FF00FF);
        uint32_t clr = 0;
        pPres->GetBorderColor(&clr);
        TEST_ASSERT(clr == 0x00FF00FF, "Border color matches 0x00FF00FF");

        pPres->Release();
    }

    // 12. EVR Presenter Window & Surface Management
    {
        auto* pPres = new mf::evr::CEVRPresenter();

        void* fakeHwnd = reinterpret_cast<void*>(0xABCDEF00);
        pPres->SetVideoWindow(fakeHwnd);
        void* hwndRet = nullptr;
        pPres->GetVideoWindow(&hwndRet);
        TEST_ASSERT(hwndRet == fakeHwnd, "GetVideoWindow matches set HWND");

        pPres->SetFullscreen(1);
        int32_t fs = 0;
        pPres->GetFullscreen(&fs);
        TEST_ASSERT(fs == 1, "Fullscreen state is TRUE");

        pPres->RepaintVideo();
        TEST_ASSERT(pPres->GetFramesPresented() == 1, "RepaintVideo increments presented frame count");

        gdi32::BITMAPINFOHEADER bih{};
        uint8_t* pDib = nullptr;
        uint32_t cbDib = 0;
        int64_t ts = 0;
        int32_t hr = pPres->GetCurrentImage(&bih, &pDib, &cbDib, &ts);
        TEST_ASSERT(hr == ole32::S_OK && pDib != nullptr && bih.biWidth == 1920, "GetCurrentImage returns valid frame bitmap");

        pPres->Release();
    }

    // 13. EVR Presentation Clock Synchronization (IMFClockStateSink)
    {
        mf::evr::IMFMediaSink* pSink = nullptr;
        mf::evr::MFCreateVideoRenderer(mf::evr::IID_IMFMediaSink, reinterpret_cast<void**>(&pSink));

        mf::IMFPresentationClock* pClock = nullptr;
        mf::MFCreatePresentationClock(&pClock);

        int32_t hr = pSink->SetPresentationClock(pClock);
        TEST_ASSERT(hr == ole32::S_OK, "SetPresentationClock succeeds");

        mf::IMFPresentationClock* pClockQuery = nullptr;
        pSink->GetPresentationClock(&pClockQuery);
        TEST_ASSERT(pClockQuery == pClock, "GetPresentationClock matches bound clock");
        pClockQuery->Release();

        // Clock state changes
        pClock->Start(1000000);
        pClock->Pause();
        pClock->Start(2000000);
        pClock->Stop();

        pClock->Release();
        pSink->Release();
    }

    // 14. End-to-End Stream Sink Sample Processing & Markers
    {
        mf::evr::IMFMediaSink* pSink = nullptr;
        mf::evr::MFCreateVideoRenderer(mf::evr::IID_IMFMediaSink, reinterpret_cast<void**>(&pSink));
        mf::evr::IMFStreamSink* pStream = nullptr;
        pSink->GetStreamSinkByIndex(0, &pStream);

        auto* pStreamSinkConcrete = static_cast<mf::evr::CEVRStreamSink*>(pStream);
        TEST_ASSERT(pStreamSinkConcrete->GetQueuedSampleCount() == 0, "Initial queued sample count is 0");

        mf::IMFSample* pSample = nullptr;
        mf::MFCreateSample(&pSample);
        mf::IMFMediaBuffer* pBuf = nullptr;
        mf::MFCreateMemoryBuffer(2048, &pBuf);
        pSample->AddBuffer(pBuf);
        int32_t hr = pStream->ProcessSample(pSample);
        TEST_ASSERT(hr == ole32::S_OK, "ProcessSample succeeds");
        TEST_ASSERT(pStreamSinkConcrete->GetQueuedSampleCount() == 1, "Queued sample count is 1");

        hr = pStream->PlaceMarker(mf::evr::MFSTREAMSINK_MARKER_TICK, nullptr, nullptr);
        TEST_ASSERT(hr == ole32::S_OK, "PlaceMarker succeeds");

        hr = pStream->Flush();
        TEST_ASSERT(hr == ole32::S_OK, "Flush succeeds");
        TEST_ASSERT(pStreamSinkConcrete->GetQueuedSampleCount() == 0, "Queued sample count after flush is 0");

        pBuf->Release();
        pSample->Release();
        pStream->Release();
        pSink->Release();
    }

    // 15. Dynamic Module Exports & Direct Creation (evr.dll)
    {
        mf::evr::InitializeEnhancedVideoRendererExports();

        auto& loader = ldr::DynamicLoader::get();
        TEST_ASSERT(loader.getExport("evr.dll", "MFCreateVideoRenderer") != nullptr, "evr.dll!MFCreateVideoRenderer resolved");
        TEST_ASSERT(loader.getExport("evr.dll", "MFCreateVideoPresenter") != nullptr, "evr.dll!MFCreateVideoPresenter resolved");
        TEST_ASSERT(loader.getExport("evr.dll", "MFCreateVideoMixer") != nullptr, "evr.dll!MFCreateVideoMixer resolved");
        TEST_ASSERT(loader.getExport("evr.dll", "DllCanUnloadNow") != nullptr, "evr.dll!DllCanUnloadNow resolved");
        TEST_ASSERT(loader.getExport("evr.dll", "DllGetClassObject") != nullptr, "evr.dll!DllGetClassObject resolved");

        mf::evr::IMFVideoPresenter* pPres = nullptr;
        int32_t hr = mf::evr::MFCreateVideoPresenter(nullptr, GUID{}, mf::evr::IID_IMFVideoPresenter, reinterpret_cast<void**>(&pPres));
        TEST_ASSERT(hr == ole32::S_OK && pPres != nullptr, "MFCreateVideoPresenter direct creation succeeds");
        pPres->Release();

        mf::IMFTransform* pMix = nullptr;
        hr = mf::evr::MFCreateVideoMixer(nullptr, GUID{}, mf::IID_IMFTransform, reinterpret_cast<void**>(&pMix));
        TEST_ASSERT(hr == ole32::S_OK && pMix != nullptr, "MFCreateVideoMixer direct creation succeeds");
        pMix->Release();
    }

    // 16. OLE32 COM Class Factory Activation & Version Database Integration
    {
        // COM activation via CoCreateInstance
        mf::evr::IMFMediaSink* pEvrSink = nullptr;
        int32_t hr = ole32::CoCreateInstance(mf::evr::CLSID_EnhancedVideoRenderer, nullptr, 1, mf::evr::IID_IMFMediaSink, reinterpret_cast<void**>(&pEvrSink));
        TEST_ASSERT(hr == ole32::S_OK && pEvrSink != nullptr, "CoCreateInstance CLSID_EnhancedVideoRenderer must succeed");
        pEvrSink->Release();

        // Version database verification
        auto& verDb = version::VersionDatabase::Instance();
        const auto* vEvr = verDb.FindModule("evr.dll");
        TEST_ASSERT(vEvr != nullptr && vEvr->stringTable.at("ProductName") == "MicaNT Enhanced Video Renderer Subsystem", "evr.dll version info match");
        TEST_ASSERT(vEvr->stringTable.at("FileVersion") == "10.0.22621.1", "evr.dll FileVersion is 10.0.22621.1");

        // Shell command integration (evr test, render, info)
        shell::CommandShell testShell;
        std::ostringstream out;

        testShell.execute("evr test", out);
        TEST_ASSERT(out.str().find("ALL 16 TESTS PASSED (100%)") != std::string::npos, "evr test must pass 100%");

        out.str("");
        testShell.execute("evr render test_frame.bmp", out);
        TEST_ASSERT(out.str().find("Pipeline Status: PRESENTING") != std::string::npos, "evr render must present frame");

        out.str("");
        testShell.execute("evr info", out);
        TEST_ASSERT(out.str().find("22621") != std::string::npos, "evr info must display telemetry");
    }

    std::cout << "[TEST] Suite 108: Windows Enhanced Video Renderer (EVR) Subsystem PASSED.\n";
}

// ============================================================================
// Suite 109: Windows DirectX Video Acceleration 2.0 (DXVA2) Subsystem Tests
// ============================================================================
void Test_WindowsDXVA2_Hardware_Acceleration_Subsystem() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 109: Windows DirectX Video Acceleration 2.0 (DXVA2) Subsystem  \n";
    std::cout << "========================================================================\n";

    // 1. Device Manager Creation & Reset Token
    uint32_t resetToken = 0;
    dxva2::IDirect3DDeviceManager9* pDevMgr = nullptr;
    int32_t hr = dxva2::DXVA2CreateDirect3DDeviceManager9(&resetToken, &pDevMgr);
    TEST_ASSERT(hr == ole32::S_OK && pDevMgr != nullptr, "DXVA2CreateDirect3DDeviceManager9 succeeds");
    TEST_ASSERT(resetToken != 0, "Reset token is non-zero");

    prismx::IUnknown* pUnk = nullptr;
    hr = pDevMgr->QueryInterface(prismx::IID_IUnknown, reinterpret_cast<void**>(&pUnk));
    TEST_ASSERT(hr == ole32::S_OK && pUnk != nullptr, "DeviceManager supports IUnknown");
    pUnk->Release();

    // 2. Direct3D 9 Device Binding & Reset Contract
    d3d9::IDirect3D9* pD3D = d3d9::Direct3DCreate9(d3d9::D3D_SDK_VERSION);
    TEST_ASSERT(pD3D != nullptr, "Direct3DCreate9 succeeds");
    d3d9::D3DPRESENT_PARAMETERS pp{};
    pp.BackBufferWidth = 1920;
    pp.BackBufferHeight = 1080;
    pp.BackBufferFormat = d3d9::D3DFMT_X8R8G8B8;
    d3d9::IDirect3DDevice9* pDevice = nullptr;
    hr = pD3D->CreateDevice(0, d3d9::D3DDEVTYPE_HAL, nullptr, 0, &pp, &pDevice);
    TEST_ASSERT(hr == d3d9::D3D_OK && pDevice != nullptr, "CreateDevice succeeds");

    hr = pDevMgr->ResetDevice(pDevice, resetToken);
    TEST_ASSERT(hr == ole32::S_OK, "ResetDevice with valid token succeeds");
    hr = pDevMgr->ResetDevice(pDevice, 0xBADBEEF);
    TEST_ASSERT(hr == ole32::E_INVALIDARG, "ResetDevice with bad token fails with E_INVALIDARG");

    // 3. Device Handle Allocation & Lifecycle (Open, Test, Close)
    void* hDev1 = nullptr;
    void* hDev2 = nullptr;
    hr = pDevMgr->OpenDeviceHandle(&hDev1);
    TEST_ASSERT(hr == ole32::S_OK && hDev1 != nullptr, "OpenDeviceHandle succeeds for hDev1");
    hr = pDevMgr->OpenDeviceHandle(&hDev2);
    TEST_ASSERT(hr == ole32::S_OK && hDev2 != nullptr && hDev1 != hDev2, "OpenDeviceHandle succeeds for hDev2");

    hr = pDevMgr->TestDevice(hDev1);
    TEST_ASSERT(hr == ole32::S_OK, "TestDevice on open handle succeeds");
    hr = pDevMgr->CloseDeviceHandle(hDev1);
    TEST_ASSERT(hr == ole32::S_OK, "CloseDeviceHandle on hDev1 succeeds");
    hr = pDevMgr->TestDevice(hDev1);
    TEST_ASSERT(hr == ole32::E_INVALIDARG, "TestDevice on closed handle fails");

    // 4. Thread-Safe Device Locking & Arbitration
    d3d9::IDirect3DDevice9* pLockedDev = nullptr;
    hr = pDevMgr->LockDevice(hDev2, &pLockedDev, win32::FALSE);
    TEST_ASSERT(hr == ole32::S_OK && pLockedDev == pDevice, "LockDevice succeeds and returns device pointer");

    void* hDev3 = nullptr;
    pDevMgr->OpenDeviceHandle(&hDev3);
    d3d9::IDirect3DDevice9* pContendedDev = nullptr;
    hr = pDevMgr->LockDevice(hDev3, &pContendedDev, win32::FALSE);
    TEST_ASSERT(hr == dxva2::DXVA2_E_VIDEO_DEVICE_LOCKED, "Non-blocking LockDevice from another handle returns DXVA2_E_VIDEO_DEVICE_LOCKED");

    hr = pDevMgr->UnlockDevice(hDev2, win32::FALSE);
    TEST_ASSERT(hr == ole32::S_OK, "UnlockDevice succeeds");
    if (pLockedDev) pLockedDev->Release();
    pDevMgr->CloseDeviceHandle(hDev3);

    // 5. Video Acceleration Service Factory (DXVA2CreateVideoService)
    dxva2::IDirectXVideoProcessorService* pProcService = nullptr;
    hr = dxva2::DXVA2CreateVideoService(pDevice, dxva2::IID_IDirectXVideoProcessorService, reinterpret_cast<void**>(&pProcService));
    TEST_ASSERT(hr == ole32::S_OK && pProcService != nullptr, "DXVA2CreateVideoService for VideoProcessorService succeeds");

    dxva2::IDirectXVideoDecoderService* pDecService = nullptr;
    hr = dxva2::DXVA2CreateVideoService(pDevice, dxva2::IID_IDirectXVideoDecoderService, reinterpret_cast<void**>(&pDecService));
    TEST_ASSERT(hr == ole32::S_OK && pDecService != nullptr, "DXVA2CreateVideoService for VideoDecoderService succeeds");

    // 6. Device Manager Service Dispatch (GetVideoService)
    dxva2::IDirectXVideoProcessorService* pProcFromMgr = nullptr;
    hr = pDevMgr->GetVideoService(hDev2, dxva2::IID_IDirectXVideoProcessorService, reinterpret_cast<void**>(&pProcFromMgr));
    TEST_ASSERT(hr == ole32::S_OK && pProcFromMgr != nullptr, "IDirect3DDeviceManager9::GetVideoService dispatch succeeds");
    pProcFromMgr->Release();

    // 7. Video Processor Device GUIDs & Render Targets
    uint32_t guidCount = 0;
    pProcService->GetVideoProcessorDeviceGuids(nullptr, &guidCount, nullptr);
    TEST_ASSERT(guidCount >= 3, "At least 3 video processor devices supported");
    std::vector<GUID> guids(guidCount);
    GUID* pGuidBuf = guids.data();
    pProcService->GetVideoProcessorDeviceGuids(nullptr, &guidCount, &pGuidBuf);
    TEST_ASSERT(guids[0] == dxva2::DXVA2_VideoProcProgressiveDevice, "Primary processor device is DXVA2_VideoProcProgressiveDevice");

    uint32_t rtCount = 0;
    pProcService->GetVideoProcessorRenderTargets(dxva2::DXVA2_VideoProcProgressiveDevice, nullptr, &rtCount, nullptr);
    TEST_ASSERT(rtCount >= 2, "Multiple render target formats supported");
    std::vector<d3d9::D3DFORMAT> rtFormats(rtCount);
    d3d9::D3DFORMAT* pRtFmts = rtFormats.data();
    pProcService->GetVideoProcessorRenderTargets(dxva2::DXVA2_VideoProcProgressiveDevice, nullptr, &rtCount, &pRtFmts);
    TEST_ASSERT(rtFormats[0] == d3d9::D3DFMT_X8R8G8B8, "Default render target format is D3DFMT_X8R8G8B8");

    // 8. Video Processor Capabilities & Deinterlace Flags
    dxva2::DXVA2_VideoDesc vDesc{};
    vDesc.SampleWidth = 1920;
    vDesc.SampleHeight = 1080;
    vDesc.FormatD3D = d3d9::D3DFMT_X8R8G8B8;
    dxva2::DXVA2_VideoProcessorCaps caps{};
    hr = pProcService->GetVideoProcessorCaps(dxva2::DXVA2_VideoProcProgressiveDevice, &vDesc, d3d9::D3DFMT_X8R8G8B8, &caps);
    TEST_ASSERT(hr == ole32::S_OK, "GetVideoProcessorCaps succeeds");
    TEST_ASSERT((caps.DeviceCaps & dxva2::DXVA2_VPDev_HardwareDevice) != 0, "Hardware acceleration capability reported");
    TEST_ASSERT((caps.ProcAmpControlCaps & dxva2::DXVA2_ProcAmp_Brightness) != 0, "ProcAmp Brightness capability reported");
    TEST_ASSERT((caps.ProcAmpControlCaps & dxva2::DXVA2_ProcAmp_Contrast) != 0, "ProcAmp Contrast capability reported");

    // 9. ProcAmp & Filter Range Discovery
    dxva2::DXVA2_ValueRange rangeBright{}, rangeContrast{}, rangeHue{}, rangeSat{};
    pProcService->GetProcAmpRange(dxva2::DXVA2_VideoProcProgressiveDevice, &vDesc, d3d9::D3DFMT_X8R8G8B8, dxva2::DXVA2_ProcAmp_Brightness, &rangeBright);
    TEST_ASSERT(rangeBright.MinValue.ToFloat() == -100.0f && rangeBright.MaxValue.ToFloat() == 100.0f && rangeBright.DefaultValue.ToFloat() == 0.0f, "Brightness range [-100, +100] def 0");
    pProcService->GetProcAmpRange(dxva2::DXVA2_VideoProcProgressiveDevice, &vDesc, d3d9::D3DFMT_X8R8G8B8, dxva2::DXVA2_ProcAmp_Contrast, &rangeContrast);
    TEST_ASSERT(rangeContrast.MinValue.ToFloat() == 0.0f && rangeContrast.MaxValue.ToFloat() == 10.0f && rangeContrast.DefaultValue.ToFloat() == 1.0f, "Contrast range [0, 10] def 1.0");
    pProcService->GetProcAmpRange(dxva2::DXVA2_VideoProcProgressiveDevice, &vDesc, d3d9::D3DFMT_X8R8G8B8, dxva2::DXVA2_ProcAmp_Hue, &rangeHue);
    TEST_ASSERT(rangeHue.MinValue.ToFloat() == -180.0f && rangeHue.MaxValue.ToFloat() == 180.0f, "Hue range [-180, +180]");
    pProcService->GetProcAmpRange(dxva2::DXVA2_VideoProcProgressiveDevice, &vDesc, d3d9::D3DFMT_X8R8G8B8, dxva2::DXVA2_ProcAmp_Saturation, &rangeSat);
    TEST_ASSERT(rangeSat.MinValue.ToFloat() == 0.0f && rangeSat.MaxValue.ToFloat() == 10.0f && rangeSat.DefaultValue.ToFloat() == 1.0f, "Saturation range [0, 10]");

    // 10. Hardware Accelerated Surface Allocation
    d3d9::IDirect3DSurface9* pTargetSurface = nullptr;
    d3d9::IDirect3DSurface9* pSourceSurface = nullptr;
    hr = pProcService->CreateSurface(1920, 1080, 0, d3d9::D3DFMT_X8R8G8B8, d3d9::D3DPOOL_DEFAULT, 0, dxva2::DXVA2_SurfaceType_ProcessorRenderTarget, &pTargetSurface, nullptr);
    TEST_ASSERT(hr == ole32::S_OK && pTargetSurface != nullptr, "CreateSurface for target succeeds");
    TEST_ASSERT(pTargetSurface->GetWidth() == 1920 && pTargetSurface->GetHeight() == 1080, "Surface dimensions 1920x1080");
    hr = pProcService->CreateSurface(1920, 1080, 0, d3d9::D3DFMT_X8R8G8B8, d3d9::D3DPOOL_DEFAULT, 0, dxva2::DXVA2_SurfaceType_ProcessorRenderTarget, &pSourceSurface, nullptr);
    TEST_ASSERT(hr == ole32::S_OK && pSourceSurface != nullptr, "CreateSurface for source succeeds");

    // 11. Video Processor Instantiation & Creation Parameters
    dxva2::IDirectXVideoProcessor* pProcessor = nullptr;
    hr = pProcService->CreateVideoProcessor(dxva2::DXVA2_VideoProcProgressiveDevice, &vDesc, d3d9::D3DFMT_X8R8G8B8, 4, &pProcessor);
    TEST_ASSERT(hr == ole32::S_OK && pProcessor != nullptr, "CreateVideoProcessor succeeds");
    GUID qGuid{};
    dxva2::DXVA2_VideoDesc qDesc{};
    d3d9::D3DFORMAT qFmt{};
    uint32_t qSubStreams = 0;
    pProcessor->GetCreationParameters(&qGuid, &qDesc, &qFmt, &qSubStreams);
    TEST_ASSERT(qGuid == dxva2::DXVA2_VideoProcProgressiveDevice, "Creation parameter device GUID matches");
    TEST_ASSERT(qSubStreams == 4, "Creation parameter max sub-streams is 4");

    // 12. Video Process Blt & ProcAmp Color Adjustment
    d3d9::D3DLOCKED_RECT srcLock{};
    pSourceSurface->LockRect(&srcLock, nullptr, 0);
    uint32_t* pSrcBits = reinterpret_cast<uint32_t*>(srcLock.pBits);
    std::fill_n(pSrcBits, 1920 * 1080, 0xFF808080); // Mid-gray (128, 128, 128)
    pSourceSurface->UnlockRect();

    dxva2::DXVA2_VideoProcessBltParams bltParams{};
    bltParams.TargetRect = { 0, 0, 1920, 1080 };
    bltParams.ProcAmpValues.Brightness = dxva2::DXVA2_Fixed32::FromFloat(20.0f);
    bltParams.ProcAmpValues.Contrast = dxva2::DXVA2_Fixed32::FromFloat(1.2f);
    bltParams.ProcAmpValues.Hue = dxva2::DXVA2_Fixed32::FromFloat(0.0f);
    bltParams.ProcAmpValues.Saturation = dxva2::DXVA2_Fixed32::FromFloat(1.0f);

    dxva2::DXVA2_VideoSample samples[2]{};
    samples[0].SrcSurface = pSourceSurface;
    samples[0].SrcRect = { 0, 0, 1920, 1080 };
    samples[0].DstRect = { 0, 0, 1920, 1080 };
    samples[0].PlanarAlpha = dxva2::DXVA2_Fixed32::FromFloat(1.0f);

    hr = pProcessor->VideoProcessBlt(pTargetSurface, &bltParams, samples, 1, nullptr);
    TEST_ASSERT(hr == ole32::S_OK, "VideoProcessBlt succeeds");

    d3d9::D3DLOCKED_RECT dstLock{};
    pTargetSurface->LockRect(&dstLock, nullptr, 0);
    uint32_t* pDstBits = reinterpret_cast<uint32_t*>(dstLock.pBits);
    uint32_t processedPix = pDstBits[0];
    pTargetSurface->UnlockRect();
    uint8_t procR = (processedPix >> 16) & 0xFF;
    TEST_ASSERT(procR >= 150 && procR <= 154, "ProcAmp adjusted pixel matches theoretical model (128 -> 152)");

    // 13. Sub-Stream Multi-Layer Compositing (PiP / Alpha Overlay)
    d3d9::IDirect3DSurface9* pSubSurface = nullptr;
    pProcService->CreateSurface(300, 200, 0, d3d9::D3DFMT_A8R8G8B8, d3d9::D3DPOOL_DEFAULT, 0, dxva2::DXVA2_SurfaceType_ProcessorRenderTarget, &pSubSurface, nullptr);
    d3d9::D3DLOCKED_RECT subLock{};
    pSubSurface->LockRect(&subLock, nullptr, 0);
    uint32_t* pSubBits = reinterpret_cast<uint32_t*>(subLock.pBits);
    std::fill_n(pSubBits, 300 * 200, 0xFFFF0000); // Opaque Red
    pSubSurface->UnlockRect();

    samples[1].SrcSurface = pSubSurface;
    samples[1].SrcRect = { 0, 0, 300, 200 };
    samples[1].DstRect = { 100, 100, 400, 300 };
    samples[1].PlanarAlpha = dxva2::DXVA2_Fixed32::FromFloat(0.5f); // 50% blend

    hr = pProcessor->VideoProcessBlt(pTargetSurface, &bltParams, samples, 2, nullptr);
    TEST_ASSERT(hr == ole32::S_OK, "VideoProcessBlt with 2 streams succeeds");

    pTargetSurface->LockRect(&dstLock, nullptr, 0);
    pDstBits = reinterpret_cast<uint32_t*>(dstLock.pBits);
    uint32_t blendedPix = pDstBits[150 * 1920 + 200];
    pTargetSurface->UnlockRect();
    uint8_t blendR = (blendedPix >> 16) & 0xFF;
    TEST_ASSERT(blendR >= 200 && blendR <= 206, "Composited sub-stream pixel blended at 50% alpha (152 & 255 -> 203)");

    // 14. Video Decoder Service Profile Discovery & Configurations
    uint32_t decCount = 0;
    pDecService->GetDecoderDeviceGuids(&decCount, nullptr);
    TEST_ASSERT(decCount >= 4, "Decoder service enumerates at least 4 hardware profiles");
    std::vector<GUID> decGuids(decCount);
    GUID* pDecGuids = decGuids.data();
    pDecService->GetDecoderDeviceGuids(&decCount, &pDecGuids);
    bool hasH264 = false, hasVC1 = false, hasMPEG2 = false;
    for (const auto& g : decGuids) {
        if (g == dxva2::DXVA2_ModeH264_E) hasH264 = true;
        if (g == dxva2::DXVA2_ModeVC1_D) hasVC1 = true;
        if (g == dxva2::DXVA2_ModeMPEG2_VLD) hasMPEG2 = true;
    }
    TEST_ASSERT(hasH264 && hasVC1 && hasMPEG2, "Supports H.264, VC-1, and MPEG-2 hardware acceleration profiles");

    uint32_t cfgCount = 0;
    pDecService->GetDecoderConfigurations(dxva2::DXVA2_ModeH264_E, &vDesc, nullptr, &cfgCount, nullptr);
    TEST_ASSERT(cfgCount > 0, "H.264 decoder configurations returned");

    // 15. Hardware Video Decode Execution Lifecycle
    dxva2::DXVA2_ConfigPictureDecode decCfg{};
    decCfg.ConfigBitstreamRaw = 1;
    dxva2::IDirectXVideoDecoder* pDecoder = nullptr;
    hr = pDecService->CreateVideoDecoder(dxva2::DXVA2_ModeH264_E, &vDesc, &decCfg, &pTargetSurface, 1, &pDecoder);
    TEST_ASSERT(hr == ole32::S_OK && pDecoder != nullptr, "CreateVideoDecoder succeeds");

    void* pPicBuf = nullptr;
    uint32_t szPic = 0;
    hr = pDecoder->GetBuffer(dxva2::DXVA2_PictureParametersBufferType, &pPicBuf, &szPic);
    TEST_ASSERT(hr == ole32::S_OK && pPicBuf != nullptr && szPic > 0, "GetBuffer for PictureParameters succeeds");
    pDecoder->ReleaseBuffer(dxva2::DXVA2_PictureParametersBufferType);

    void* pBitBuf = nullptr;
    uint32_t szBit = 0;
    hr = pDecoder->GetBuffer(dxva2::DXVA2_BitStreamDateBufferType, &pBitBuf, &szBit);
    TEST_ASSERT(hr == ole32::S_OK && pBitBuf != nullptr && szBit > 0, "GetBuffer for BitStream succeeds");
    pDecoder->ReleaseBuffer(dxva2::DXVA2_BitStreamDateBufferType);

    hr = pDecoder->BeginFrame(pTargetSurface, nullptr);
    TEST_ASSERT(hr == ole32::S_OK, "BeginFrame succeeds");
    dxva2::DXVA2_DecodeExecuteParams execParams{};
    hr = pDecoder->Execute(&execParams);
    TEST_ASSERT(hr == ole32::S_OK, "Execute decode bitstream succeeds");
    hr = pDecoder->EndFrame(nullptr);
    TEST_ASSERT(hr == ole32::S_OK, "EndFrame succeeds");

    auto* pConcreteDec = static_cast<dxva2::CDirectXVideoDecoder*>(pDecoder);
    TEST_ASSERT(pConcreteDec->GetDecodedFrameCount() == 1, "Decoded frame count is 1");

    // 16. Dynamic Module Exports & Shell Command Integration (dxva2.dll)
    dxva2::InitializeDXVA2Exports();
    auto& loader = ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("dxva2.dll", "DXVA2CreateDirect3DDeviceManager9") != nullptr, "dxva2.dll!DXVA2CreateDirect3DDeviceManager9 exported");
    TEST_ASSERT(loader.getExport("dxva2.dll", "DXVA2CreateVideoService") != nullptr, "dxva2.dll!DXVA2CreateVideoService exported");
    TEST_ASSERT(loader.getExport("dxva2.dll", "DllCanUnloadNow") != nullptr, "dxva2.dll!DllCanUnloadNow exported");
    TEST_ASSERT(loader.getExport("dxva2.dll", "DllGetClassObject") != nullptr, "dxva2.dll!DllGetClassObject exported");

    auto& verDb = version::VersionDatabase::Instance();
    const auto* vDxva = verDb.FindModule("dxva2.dll");
    TEST_ASSERT(vDxva != nullptr && vDxva->stringTable.at("ProductName") == "MicaNT DirectX Video Acceleration 2.0 Subsystem", "dxva2.dll version product matches");
    TEST_ASSERT(vDxva->stringTable.at("FileVersion") == "10.0.22621.1", "dxva2.dll FileVersion is 10.0.22621.1");

    shell::CommandShell testShell;
    std::ostringstream out;

    testShell.execute("dxva2 test", out);
    TEST_ASSERT(out.str().find("ALL 16 TESTS PASSED (100%)") != std::string::npos, "dxva2 test self-test succeeds 100%");

    out.str("");
    testShell.execute("dxva2 procamp 10 120", out);
    TEST_ASSERT(out.str().find("COLOR ADJUSTED") != std::string::npos, "dxva2 procamp adjusts color");

    out.str("");
    testShell.execute("dxva2 info", out);
    TEST_ASSERT(out.str().find("22621") != std::string::npos, "dxva2 info displays telemetry");

    // Cleanup
    pSubSurface->Release();
    pSourceSurface->Release();
    pTargetSurface->Release();
    pDecoder->Release();
    pProcessor->Release();
    pDecService->Release();
    pProcService->Release();
    pDevMgr->CloseDeviceHandle(hDev2);
    pDevMgr->Release();
    pDevice->Release();
    pD3D->Release();

    std::cout << "[TEST] Suite 109: Windows DirectX Video Acceleration 2.0 (DXVA2) Subsystem PASSED.\n";
}

// ============================================================================
// Suite 110: Windows Direct3D 11 Video Acceleration Subsystem Tests
// ============================================================================
void Test_WindowsDirect3D11_Video_Acceleration_Subsystem() {
    using namespace micant::prismx;
    using namespace micant::prism3d;
    using namespace micant::d3d11va;

    std::cout << "[TEST] Running Suite 110: Windows Direct3D 11 Video Acceleration Subsystem...\n";

    InitializeD3D11VAExports();

    // 1. Direct3D 11 Device & Video Device Creation
    prism3d::ID3D11Device* pDevice = nullptr;
    prism3d::ID3D11DeviceContext* pContext = nullptr;
    d3d11va::ID3D11VideoDevice* pVideoDevice = nullptr;
    d3d11va::ID3D11VideoContext* pVideoContext = nullptr;

    int32_t hr = d3d11va::D3D11CreateDeviceWithVideo(
        nullptr, prism3d::D3D_DRIVER_TYPE_HARDWARE, nullptr,
        d3d11va::D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
        nullptr, 0, 7,
        &pDevice, nullptr, &pContext,
        &pVideoDevice, &pVideoContext);

    TEST_ASSERT(hr == 0 && pDevice != nullptr && pVideoDevice != nullptr, "D3D11CreateDeviceWithVideo must create D3D11 device and video device");

    prism3d::ID3D11Device* pDevFromVideo = nullptr;
    hr = pVideoDevice->QueryInterface(prism3d::IID_ID3D11Device, reinterpret_cast<void**>(&pDevFromVideo));
    TEST_ASSERT(hr == 0 && pDevFromVideo == pDevice, "QueryInterface from VideoDevice to ID3D11Device must succeed");
    if (pDevFromVideo) pDevFromVideo->Release();

    // 2. Video Context Creation & Interface Arbitration
    TEST_ASSERT(pContext != nullptr && pVideoContext != nullptr, "Video context and device context must be valid");
    prism3d::ID3D11DeviceContext* pCtxFromVideo = nullptr;
    hr = pVideoContext->QueryInterface(prism3d::IID_ID3D11DeviceContext, reinterpret_cast<void**>(&pCtxFromVideo));
    TEST_ASSERT(hr == 0 && pCtxFromVideo == pContext, "QueryInterface from VideoContext to ID3D11DeviceContext must succeed");
    if (pCtxFromVideo) pCtxFromVideo->Release();

    // 3. Video Decoder Profile Enumeration
    uint32_t profCount = pVideoDevice->GetVideoDecoderProfileCount();
    TEST_ASSERT(profCount >= 9, "Video device must enumerate at least 9 hardware decoder profiles");
    bool hasHEVC10 = false, hasH264 = false, hasAV1 = false, hasVP9_10 = false, hasVC1 = false, hasMPEG2 = false;
    for (uint32_t i = 0; i < profCount; ++i) {
        GUID g{};
        pVideoDevice->GetVideoDecoderProfile(i, &g);
        if (g == d3d11va::D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10) hasHEVC10 = true;
        if (g == d3d11va::D3D11_DECODER_PROFILE_H264_VLD_NOFGT) hasH264 = true;
        if (g == d3d11va::D3D11_DECODER_PROFILE_AV1_VLD_PROFILE0) hasAV1 = true;
        if (g == d3d11va::D3D11_DECODER_PROFILE_VP9_VLD_10BIT) hasVP9_10 = true;
        if (g == d3d11va::D3D11_DECODER_PROFILE_VC1_VLD) hasVC1 = true;
        if (g == d3d11va::D3D11_DECODER_PROFILE_MPEG2_VLD) hasMPEG2 = true;
    }
    TEST_ASSERT(hasHEVC10 && hasH264 && hasAV1 && hasVP9_10 && hasVC1 && hasMPEG2, "All standard hardware decoder profiles must be enumerated");

    // 4. Video Decoder Format Verification
    int32_t suppNV12 = 0, suppP010 = 0, suppBGRA = 0, suppD32 = 0;
    pVideoDevice->CheckVideoDecoderFormat(&d3d11va::D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10, d3d11va::DXGI_FORMAT_NV12, &suppNV12);
    pVideoDevice->CheckVideoDecoderFormat(&d3d11va::D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10, d3d11va::DXGI_FORMAT_P010, &suppP010);
    pVideoDevice->CheckVideoDecoderFormat(&d3d11va::D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10, prismx::DXGI_FORMAT_B8G8R8A8_UNORM, &suppBGRA);
    pVideoDevice->CheckVideoDecoderFormat(&d3d11va::D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10, prismx::DXGI_FORMAT_D32_FLOAT, &suppD32);
    TEST_ASSERT(suppNV12 == 1 && suppP010 == 1 && suppBGRA == 1 && suppD32 == 0, "Video decoder format verification must correctly identify supported and unsupported surface formats");

    // 5. Video Decoder Config Negotiation
    d3d11va::D3D11_VIDEO_DECODER_DESC decDesc{};
    decDesc.Guid = d3d11va::D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10;
    decDesc.SampleWidth = 3840;
    decDesc.SampleHeight = 2160;
    decDesc.OutputFormat = d3d11va::DXGI_FORMAT_P010;
    uint32_t cfgCount = 0;
    pVideoDevice->GetVideoDecoderConfigCount(&decDesc, &cfgCount);
    TEST_ASSERT(cfgCount == 1, "Decoder config count must equal 1");
    d3d11va::D3D11_VIDEO_DECODER_CONFIG decCfg{};
    hr = pVideoDevice->GetVideoDecoderConfig(&decDesc, 0, &decCfg);
    TEST_ASSERT(hr == 0 && decCfg.ConfigBitstreamRaw == 1 && decCfg.ConfigMinRenderTargetBuffCount == 4, "Decoder config must specify raw bitstream and minimum 4 render target buffers");

    // 6. Video Decoder Creation (HEVC Main 10 HDR & H.264 Profiles)
    d3d11va::ID3D11VideoDecoder* pDecoderHEVC = nullptr;
    hr = pVideoDevice->CreateVideoDecoder(&decDesc, &decCfg, &pDecoderHEVC);
    TEST_ASSERT(hr == 0 && pDecoderHEVC != nullptr, "CreateVideoDecoder for HEVC Main 10 must succeed");
    void* hDriverHEVC = nullptr;
    pDecoderHEVC->GetDriverHandle(&hDriverHEVC);
    TEST_ASSERT(hDriverHEVC != nullptr, "Decoder driver handle must be valid");

    d3d11va::D3D11_VIDEO_DECODER_DESC decDescH264{};
    decDescH264.Guid = d3d11va::D3D11_DECODER_PROFILE_H264_VLD_NOFGT;
    decDescH264.SampleWidth = 1920;
    decDescH264.SampleHeight = 1080;
    decDescH264.OutputFormat = d3d11va::DXGI_FORMAT_NV12;
    d3d11va::ID3D11VideoDecoder* pDecoderH264 = nullptr;
    hr = pVideoDevice->CreateVideoDecoder(&decDescH264, &decCfg, &pDecoderH264);
    TEST_ASSERT(hr == 0 && pDecoderH264 != nullptr, "CreateVideoDecoder for H.264 High Profile must succeed");

    // 7. Video Decoder Buffer Allocation & Access
    uint32_t bitSize = 0, picSize = 0, iqSize = 0, sliceSize = 0;
    void* pBitBuf = nullptr;
    void* pPicBuf = nullptr;
    void* pIQBuf = nullptr;
    void* pSliceBuf = nullptr;
    hr = pVideoContext->GetDecoderBuffer(pDecoderHEVC, d3d11va::D3D11_VIDEO_DECODER_BUFFER_BITSTREAM, &bitSize, &pBitBuf);
    TEST_ASSERT(hr == 0 && bitSize >= 1024 * 1024 && pBitBuf != nullptr, "GetDecoderBuffer for Bitstream must return >= 1MB scratch space");
    hr = pVideoContext->GetDecoderBuffer(pDecoderHEVC, d3d11va::D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS, &picSize, &pPicBuf);
    TEST_ASSERT(hr == 0 && picSize >= 4096 && pPicBuf != nullptr, "GetDecoderBuffer for PictureParameters must succeed");
    hr = pVideoContext->GetDecoderBuffer(pDecoderHEVC, d3d11va::D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX, &iqSize, &pIQBuf);
    TEST_ASSERT(hr == 0 && iqSize >= 4096 && pIQBuf != nullptr, "GetDecoderBuffer for IQ Matrix must succeed");
    hr = pVideoContext->GetDecoderBuffer(pDecoderHEVC, d3d11va::D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL, &sliceSize, &pSliceBuf);
    TEST_ASSERT(hr == 0 && sliceSize >= 16384 && pSliceBuf != nullptr, "GetDecoderBuffer for SliceControl must succeed");

    pVideoContext->ReleaseDecoderBuffer(pDecoderHEVC, d3d11va::D3D11_VIDEO_DECODER_BUFFER_BITSTREAM);
    pVideoContext->ReleaseDecoderBuffer(pDecoderHEVC, d3d11va::D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS);
    pVideoContext->ReleaseDecoderBuffer(pDecoderHEVC, d3d11va::D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX);
    pVideoContext->ReleaseDecoderBuffer(pDecoderHEVC, d3d11va::D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL);

    // 8. Video Decoder Output View Creation & Target Texture Linkage
    prism3d::D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = 3840;
    texDesc.Height = 2160;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = d3d11va::DXGI_FORMAT_P010;
    prism3d::ID3D11Texture2D* pDecTex = nullptr;
    pDevice->CreateTexture2D(&texDesc, nullptr, &pDecTex);

    d3d11va::D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC vdovDesc{};
    vdovDesc.DecodeProfile = d3d11va::D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10;
    vdovDesc.ViewDimension = d3d11va::D3D11_VDOV_DIMENSION_TEXTURE2D;
    vdovDesc.Texture2D.ArraySlice = 0;
    d3d11va::ID3D11VideoDecoderOutputView* pVDOV = nullptr;
    hr = pVideoDevice->CreateVideoDecoderOutputView(pDecTex, &vdovDesc, &pVDOV);
    TEST_ASSERT(hr == 0 && pVDOV != nullptr, "CreateVideoDecoderOutputView must succeed");

    prism3d::ID3D11Resource* pResLink = nullptr;
    pVDOV->GetResource(&pResLink);
    TEST_ASSERT(pResLink == pDecTex, "VDOV resource must match backing texture");
    if (pResLink) pResLink->Release();

    // 9. Frame Decoding Execution State Machine
    hr = pVideoContext->DecoderBeginFrame(pDecoderHEVC, pVDOV, 0, nullptr);
    TEST_ASSERT(hr == 0, "DecoderBeginFrame must initiate active frame decoding");
    d3d11va::D3D11_VIDEO_DECODER_BUFFER_DESC bufDesc[2]{};
    bufDesc[0].BufferType = d3d11va::D3D11_VIDEO_DECODER_BUFFER_BITSTREAM;
    bufDesc[0].DataSize = 131072; // 128 KB compressed slice
    bufDesc[1].BufferType = d3d11va::D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS;
    bufDesc[1].DataSize = sizeof(decDesc);
    hr = pVideoContext->SubmitDecoderBuffers(pDecoderHEVC, 2, bufDesc);
    TEST_ASSERT(hr == 0, "SubmitDecoderBuffers must ingest bitstream and picture params");
    hr = pVideoContext->DecoderEndFrame(pDecoderHEVC);
    TEST_ASSERT(hr == 0, "DecoderEndFrame must finalize frame decode");

    auto* decImpl = static_cast<d3d11va::CD3D11VideoDecoder*>(pDecoderHEVC);
    TEST_ASSERT(decImpl->GetDecodedFrames() == 1, "Decoded frame count must increment to 1");
    TEST_ASSERT(decImpl->GetTotalBitstreamBytes() == 131072, "Total bitstream byte volume must record submitted buffer size");

    // 10. Video Processor Enumerator Creation & Format Support
    d3d11va::D3D11_VIDEO_PROCESSOR_CONTENT_DESC vpContent{};
    vpContent.InputFrameFormat = d3d11va::D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    vpContent.InputFrameRate = { 60, 1 };
    vpContent.InputWidth = 1920;
    vpContent.InputHeight = 1080;
    vpContent.OutputFrameRate = { 60, 1 };
    vpContent.OutputWidth = 1920;
    vpContent.OutputHeight = 1080;
    vpContent.Usage = d3d11va::D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
    d3d11va::ID3D11VideoProcessorEnumerator* pEnum = nullptr;
    hr = pVideoDevice->CreateVideoProcessorEnumerator(&vpContent, &pEnum);
    TEST_ASSERT(hr == 0 && pEnum != nullptr, "CreateVideoProcessorEnumerator must succeed");

    uint32_t nv12Flags = 0, yuy2Flags = 0, bgraFlags = 0, hdr10Flags = 0;
    pEnum->CheckVideoProcessorFormat(d3d11va::DXGI_FORMAT_NV12, &nv12Flags);
    pEnum->CheckVideoProcessorFormat(d3d11va::DXGI_FORMAT_YUY2, &yuy2Flags);
    pEnum->CheckVideoProcessorFormat(prismx::DXGI_FORMAT_B8G8R8A8_UNORM, &bgraFlags);
    pEnum->CheckVideoProcessorFormat(d3d11va::DXGI_FORMAT_R10G10B10A2_UNORM, &hdr10Flags);
    TEST_ASSERT((nv12Flags & d3d11va::D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT) != 0, "NV12 must be supported as input format");
    TEST_ASSERT((bgraFlags & d3d11va::D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) != 0, "B8G8R8A8 must be supported as output format");
    TEST_ASSERT((hdr10Flags & d3d11va::D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT) != 0, "R10G10B10A2 must be supported as HDR output format");

    // 11. Video Processor Caps & Rate Conversion Capabilities
    d3d11va::D3D11_VIDEO_PROCESSOR_CAPS vpCaps{};
    pEnum->GetVideoProcessorCaps(&vpCaps);
    TEST_ASSERT(vpCaps.MaxInputStreams >= 16, "Video processor must support at least 16 concurrent input streams");
    TEST_ASSERT((vpCaps.DeviceCaps & d3d11va::D3D11_VIDEO_PROCESSOR_DEVICE_CAPS_RGB_RANGE_CONVERSION) != 0, "Video processor must support RGB Range Conversion");
    TEST_ASSERT((vpCaps.FeatureCaps & d3d11va::D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_ALPHA_FILL) != 0, "Video processor must support Alpha Fill modes");

    d3d11va::D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rcCaps{};
    pEnum->GetVideoProcessorRateConversionCaps(0, &rcCaps);
    TEST_ASSERT(rcCaps.PastFrames == 2 && rcCaps.FutureFrames == 2, "Rate conversion must specify 2 past and 2 future reference frames");
    TEST_ASSERT((rcCaps.ProcessorCaps & 0x1) != 0, "De-interlacing processor cap must be active");

    // 12. Video Processor Custom Rates & Filter Ranges
    d3d11va::D3D11_VIDEO_PROCESSOR_CUSTOM_RATE customRate{};
    hr = pEnum->GetVideoProcessorCustomRate(0, 0, &customRate);
    TEST_ASSERT(hr == 0 && customRate.CustomRate.Numerator == 24, "Custom rate 0 must represent 24 fps film cadence");

    d3d11va::D3D11_VIDEO_PROCESSOR_FILTER_RANGE rBright{}, rContrast{}, rHue{}, rSat{};
    pEnum->GetVideoProcessorFilterRange(d3d11va::D3D11_VIDEO_PROCESSOR_FILTER_BRIGHTNESS, &rBright);
    pEnum->GetVideoProcessorFilterRange(d3d11va::D3D11_VIDEO_PROCESSOR_FILTER_CONTRAST, &rContrast);
    pEnum->GetVideoProcessorFilterRange(d3d11va::D3D11_VIDEO_PROCESSOR_FILTER_HUE, &rHue);
    pEnum->GetVideoProcessorFilterRange(d3d11va::D3D11_VIDEO_PROCESSOR_FILTER_SATURATION, &rSat);
    TEST_ASSERT(rBright.Minimum == -100 && rBright.Maximum == 100, "Brightness range must be [-100, 100]");
    TEST_ASSERT(rContrast.Minimum == 0 && rContrast.Maximum == 200, "Contrast range must be [0, 200]");
    TEST_ASSERT(rHue.Minimum == -180 && rHue.Maximum == 180, "Hue range must be [-180, 180]");
    TEST_ASSERT(rSat.Minimum == 0 && rSat.Maximum == 200, "Saturation range must be [0, 200]");

    // 13. Video Processor Creation & Stream State Configuration
    d3d11va::ID3D11VideoProcessor* pVP = nullptr;
    hr = pVideoDevice->CreateVideoProcessor(pEnum, 0, &pVP);
    TEST_ASSERT(hr == 0 && pVP != nullptr, "CreateVideoProcessor must succeed");

    d3d11va::D3D11_VIDEO_COLOR bgCol{};
    bgCol.RGBA = { 0.1f, 0.1f, 0.15f, 1.0f };
    pVideoContext->VideoProcessorSetOutputBackgroundColor(pVP, 0, &bgCol);

    d3d11va::D3D11_VIDEO_PROCESSOR_COLOR_SPACE cs{};
    cs.Usage = 0;
    cs.RGB_Range = 0;
    cs.YCbCr_Matrix = 1; // BT.709
    cs.Nominal_Range = d3d11va::D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255;
    pVideoContext->VideoProcessorSetStreamColorSpace(pVP, 0, &cs);
    pVideoContext->VideoProcessorSetStreamAlpha(pVP, 0, 1, 1.0f);
    pVideoContext->VideoProcessorSetStreamAlpha(pVP, 1, 1, 0.5f); // 50% PiP overlay

    // 14. Video Processor Input/Output Views Creation
    prism3d::D3D11_TEXTURE2D_DESC sDesc{};
    sDesc.Width = 1920;
    sDesc.Height = 1080;
    sDesc.Format = prismx::DXGI_FORMAT_B8G8R8A8_UNORM;
    prism3d::ID3D11Texture2D* pTexBase = nullptr;
    prism3d::ID3D11Texture2D* pTexOverlay = nullptr;
    prism3d::ID3D11Texture2D* pTexOut = nullptr;
    pDevice->CreateTexture2D(&sDesc, nullptr, &pTexBase);
    pDevice->CreateTexture2D(&sDesc, nullptr, &pTexOverlay);
    pDevice->CreateTexture2D(&sDesc, nullptr, &pTexOut);

    auto* pRawBase = static_cast<prism3d::Prism3DTexture2DImpl*>(pTexBase);
    auto* pRawOverlay = static_cast<prism3d::Prism3DTexture2DImpl*>(pTexOverlay);
    std::fill_n(pRawBase->GetPixels(), 1920 * 1080, 0xFF0000FF); // Blue
    std::fill_n(pRawOverlay->GetPixels(), 1920 * 1080, 0xFFFF0000); // Red

    d3d11va::D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inViewDesc{};
    inViewDesc.ViewDimension = d3d11va::D3D11_VPIV_DIMENSION_TEXTURE2D;
    d3d11va::ID3D11VideoProcessorInputView* pInputBase = nullptr;
    d3d11va::ID3D11VideoProcessorInputView* pInputOverlay = nullptr;
    pVideoDevice->CreateVideoProcessorInputView(pTexBase, pEnum, &inViewDesc, &pInputBase);
    pVideoDevice->CreateVideoProcessorInputView(pTexOverlay, pEnum, &inViewDesc, &pInputOverlay);

    d3d11va::D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outViewDesc{};
    outViewDesc.ViewDimension = d3d11va::D3D11_VPOV_DIMENSION_TEXTURE2D;
    d3d11va::ID3D11VideoProcessorOutputView* pOutputView = nullptr;
    pVideoDevice->CreateVideoProcessorOutputView(pTexOut, pEnum, &outViewDesc, &pOutputView);
    TEST_ASSERT(pInputBase != nullptr && pInputOverlay != nullptr && pOutputView != nullptr, "Processor input/output views must be created successfully");

    // 15. Video Processor Blt Multi-Stream Compositing with Planar Alpha & BT.2020 HDR
    d3d11va::D3D11_VIDEO_PROCESSOR_STREAM streams[2]{};
    streams[0].Enable = 1;
    streams[0].pInputSurface = pInputBase;
    streams[1].Enable = 1;
    streams[1].pInputSurface = pInputOverlay;

    RECT srcR{ 0, 0, 400, 300 };
    RECT dstR{ 100, 100, 500, 400 };
    pVideoContext->VideoProcessorSetStreamSourceRect(pVP, 1, 1, &srcR);
    pVideoContext->VideoProcessorSetStreamDestRect(pVP, 1, 1, &dstR);

    hr = pVideoContext->VideoProcessorBlt(pVP, pOutputView, 0, 2, streams);
    TEST_ASSERT(hr == 0, "VideoProcessorBlt multi-stream blit must succeed");

    auto* pRawOut = static_cast<prism3d::Prism3DTexture2DImpl*>(pTexOut);
    uint32_t blendedPix = pRawOut->GetPixels()[200 * 1920 + 200];
    uint8_t outR = (blendedPix >> 16) & 0xFF;
    uint8_t outB = blendedPix & 0xFF;
    TEST_ASSERT(outR >= 120 && outR <= 135, "Alpha blended Red channel must be approximately 50% (~127)");
    TEST_ASSERT(outB >= 120 && outB <= 135, "Alpha blended Blue channel must be approximately 50% (~127)");

    auto* ctxImpl = static_cast<d3d11va::CD3D11VideoContext*>(pVideoContext);
    TEST_ASSERT(ctxImpl->GetBltCount() == 1, "Blt counter must increment to 1");
    TEST_ASSERT(ctxImpl->GetProcessedFrames() == 1, "Processed frame counter must increment to 1");

    // 16. Dynamic Module Exports, Crypto Key Exchange & Shell Commands
    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("d3d11.dll", "D3D11CreateVideoDevice") != nullptr, "d3d11.dll!D3D11CreateVideoDevice must be registered");
    TEST_ASSERT(ldr.getExport("d3d11.dll", "D3D11CreateVideoContext") != nullptr, "d3d11.dll!D3D11CreateVideoContext must be registered");
    TEST_ASSERT(ldr.getExport("d3d11.dll", "D3D11CreateDeviceWithVideo") != nullptr, "d3d11.dll!D3D11CreateDeviceWithVideo must be registered");

    GUID keyExType{};
    hr = pVideoDevice->CheckCryptoKeyExchange(&d3d11va::D3D11_CRYPTO_TYPE_AES128_CTR, &d3d11va::D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10, 0, &keyExType);
    TEST_ASSERT(hr == 0 && keyExType == d3d11va::D3D11_KEY_EXCHANGE_HW_PROTECTION, "Hardware DRM protection key exchange must match D3D11_KEY_EXCHANGE_HW_PROTECTION");

    // Shell command integration verification
    shell::CommandShell testShell;
    std::ostringstream out;

    testShell.execute("d3d11va test", out);
    TEST_ASSERT(out.str().find("ALL 16 TESTS PASSED (100%)") != std::string::npos, "d3d11va test self-test succeeds 100%");

    out.str("");
    testShell.execute("d3d11va proc", out);
    TEST_ASSERT(out.str().find("COLOR CONVERTED") != std::string::npos, "d3d11va proc converts color gamut to BT.2020 HDR");

    out.str("");
    testShell.execute("d3d11va info", out);
    TEST_ASSERT(out.str().find("22621") != std::string::npos, "d3d11va info displays telemetry");

    // Cleanup
    pOutputView->Release();
    pInputOverlay->Release();
    pInputBase->Release();
    pTexOut->Release();
    pTexOverlay->Release();
    pTexBase->Release();
    pVP->Release();
    pEnum->Release();
    pVDOV->Release();
    pDecTex->Release();
    pDecoderH264->Release();
    pDecoderHEVC->Release();
    pVideoContext->Release();
    pVideoDevice->Release();
    pContext->Release();
    pDevice->Release();

    std::cout << "[TEST] Suite 110: Windows Direct3D 11 Video Acceleration Subsystem PASSED.\n";
}

void Test_WindowsDirect3D12_Video_Acceleration_Subsystem() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 111: Windows Direct3D 12 Video Decode & Processing Subsystem    \n";
    std::cout << "========================================================================\n";

    // 1. Initialize Direct3D 12 Core Device
    prism3d12::ID3D12Device* pDevice = nullptr;
    int32_t hr = prism3d12::D3D12CreateDevice(nullptr, prism3d12::D3D_FEATURE_LEVEL_12_1, prism3d12::IID_ID3D12Device, reinterpret_cast<void**>(&pDevice));
    TEST_ASSERT(hr == 0 && pDevice != nullptr, "D3D12CreateDevice must succeed with Feature Level 12.1");

    // 2. Create Direct3D 12 Video Device & Verify COM Interfaces
    d3d12video::ID3D12VideoDevice* pVideoDevice = nullptr;
    hr = d3d12video::D3D12CreateVideoDevice(pDevice, d3d12video::IID_ID3D12VideoDevice, reinterpret_cast<void**>(&pVideoDevice));
    TEST_ASSERT(hr == 0 && pVideoDevice != nullptr, "D3D12CreateVideoDevice must succeed");

    d3d12video::ID3D12VideoDevice1* pVideoDevice1 = nullptr;
    hr = pVideoDevice->QueryInterface(d3d12video::IID_ID3D12VideoDevice1, reinterpret_cast<void**>(&pVideoDevice1));
    TEST_ASSERT(hr == 0 && pVideoDevice1 != nullptr, "QueryInterface for ID3D12VideoDevice1 must succeed");

    // 3. Query Video Decode Profile Count & Supported Codec Profiles
    d3d12video::D3D12_FEATURE_DATA_VIDEO_DECODE_PROFILE_COUNT profileCount{};
    hr = pVideoDevice->CheckFeatureSupport(d3d12video::D3D12_FEATURE_VIDEO_DECODE_PROFILE_COUNT, &profileCount, sizeof(profileCount));
    TEST_ASSERT(hr == 0 && profileCount.ProfileCount >= 8, "Video device must report at least 8 hardware decode profiles");

    std::vector<GUID> profiles(profileCount.ProfileCount);
    d3d12video::D3D12_FEATURE_DATA_VIDEO_DECODE_PROFILES profileData{};
    profileData.ProfileCount = profileCount.ProfileCount;
    profileData.pProfiles = profiles.data();
    hr = pVideoDevice->CheckFeatureSupport(d3d12video::D3D12_FEATURE_VIDEO_DECODE_PROFILES, &profileData, sizeof(profileData));
    TEST_ASSERT(hr == 0, "CheckFeatureSupport for D3D12_FEATURE_VIDEO_DECODE_PROFILES must succeed");

    bool hasH264 = false, hasHEVC = false, hasHEVC10 = false, hasVP9 = false, hasAV1 = false;
    for (const auto& prof : profiles) {
        if (prof == d3d12video::D3D12_VIDEO_DECODE_PROFILE_H264) hasH264 = true;
        if (prof == d3d12video::D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN) hasHEVC = true;
        if (prof == d3d12video::D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN10) hasHEVC10 = true;
        if (prof == d3d12video::D3D12_VIDEO_DECODE_PROFILE_VP9) hasVP9 = true;
        if (prof == d3d12video::D3D12_VIDEO_DECODE_PROFILE_AV1_PROFILE0) hasAV1 = true;
    }
    TEST_ASSERT(hasH264, "H.264 profile must be supported");
    TEST_ASSERT(hasHEVC, "HEVC Main profile must be supported");
    TEST_ASSERT(hasHEVC10, "HEVC Main10 (10-bit HDR) profile must be supported");
    TEST_ASSERT(hasVP9, "VP9 profile must be supported");
    TEST_ASSERT(hasAV1, "AV1 Profile 0 must be supported");

    // 4. Query 4K & 8K Video Decode Capabilities & Tiers
    d3d12video::D3D12_FEATURE_DATA_VIDEO_DECODE_SUPPORT decode4K{};
    decode4K.Configuration.DecodeProfile = d3d12video::D3D12_VIDEO_DECODE_PROFILE_H264;
    decode4K.Width = 3840;
    decode4K.Height = 2160;
    decode4K.DecodeFormat = prismx::DXGI_FORMAT_NV12;
    hr = pVideoDevice->CheckFeatureSupport(d3d12video::D3D12_FEATURE_VIDEO_DECODE_SUPPORT, &decode4K, sizeof(decode4K));
    TEST_ASSERT(hr == 0, "CheckFeatureSupport for 4K H.264 decode must succeed");
    TEST_ASSERT((decode4K.SupportFlags & d3d12video::D3D12_VIDEO_DECODE_SUPPORT_FLAG_SUPPORTED) != 0, "4K H.264 decode must be supported");
    TEST_ASSERT(decode4K.DecodeTier == d3d12video::D3D12_VIDEO_DECODE_TIER_3, "Hardware decode tier must be Tier 3 (independent queue)");

    d3d12video::D3D12_FEATURE_DATA_VIDEO_DECODE_SUPPORT decode8K{};
    decode8K.Configuration.DecodeProfile = d3d12video::D3D12_VIDEO_DECODE_PROFILE_AV1_PROFILE0;
    decode8K.Width = 7680;
    decode8K.Height = 4320;
    decode8K.DecodeFormat = prismx::DXGI_FORMAT_P010;
    hr = pVideoDevice->CheckFeatureSupport(d3d12video::D3D12_FEATURE_VIDEO_DECODE_SUPPORT, &decode8K, sizeof(decode8K));
    TEST_ASSERT(hr == 0, "CheckFeatureSupport for 8K AV1 10-bit decode must succeed");
    TEST_ASSERT((decode8K.SupportFlags & d3d12video::D3D12_VIDEO_DECODE_SUPPORT_FLAG_SUPPORTED) != 0, "8K AV1 10-bit decode must be supported");

    // 5. Query Decode Formats
    d3d12video::D3D12_FEATURE_DATA_VIDEO_DECODE_FORMAT_COUNT formatCount{};
    formatCount.Configuration.DecodeProfile = d3d12video::D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN;
    hr = pVideoDevice->CheckFeatureSupport(d3d12video::D3D12_FEATURE_VIDEO_DECODE_FORMAT_COUNT, &formatCount, sizeof(formatCount));
    TEST_ASSERT(hr == 0 && formatCount.FormatCount >= 4, "Must report at least 4 decode surface formats");

    std::vector<prismx::DXGI_FORMAT> formats(formatCount.FormatCount);
    d3d12video::D3D12_FEATURE_DATA_VIDEO_DECODE_FORMATS formatData{};
    formatData.Configuration.DecodeProfile = d3d12video::D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN;
    formatData.FormatCount = formatCount.FormatCount;
    formatData.pOutputFormats = formats.data();
    hr = pVideoDevice->CheckFeatureSupport(d3d12video::D3D12_FEATURE_VIDEO_DECODE_FORMATS, &formatData, sizeof(formatData));
    TEST_ASSERT(hr == 0, "CheckFeatureSupport for D3D12_FEATURE_VIDEO_DECODE_FORMATS must succeed");

    // 6. Query Video Processor Capabilities & Filter Ranges
    d3d12video::D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT procSupport{};
    procSupport.InputDesc.Format = prismx::DXGI_FORMAT_NV12;
    procSupport.OutputDesc.Format = prismx::DXGI_FORMAT_B8G8R8A8_UNORM;
    hr = pVideoDevice->CheckFeatureSupport(d3d12video::D3D12_FEATURE_VIDEO_PROCESS_SUPPORT, &procSupport, sizeof(procSupport));
    TEST_ASSERT(hr == 0, "CheckFeatureSupport for D3D12_FEATURE_VIDEO_PROCESS_SUPPORT must succeed");
    TEST_ASSERT((procSupport.FeatureFlags & d3d12video::D3D12_VIDEO_PROCESS_FEATURE_FLAG_ALPHA_BLENDING) != 0, "Must support alpha blending");
    TEST_ASSERT((procSupport.FeatureFlags & d3d12video::D3D12_VIDEO_PROCESS_FEATURE_FLAG_ROTATION) != 0, "Must support hardware orientation rotation");
    TEST_ASSERT((procSupport.DeinterlaceFlags & d3d12video::D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_BOB) != 0, "Must support Bob deinterlacing");
    TEST_ASSERT(procSupport.FilterRanges[d3d12video::D3D12_VIDEO_PROCESS_FILTER_BRIGHTNESS].Minimum == -100, "Brightness filter min must be -100");

    // 7. Create Video Decoder Instance (H.264 1080p)
    d3d12video::D3D12_VIDEO_DECODER_DESC decDesc{};
    decDesc.NodeMask = 0;
    decDesc.Configuration.DecodeProfile = d3d12video::D3D12_VIDEO_DECODE_PROFILE_H264;
    decDesc.Configuration.BitstreamEncryption = d3d12video::D3D12_BITSTREAM_ENCRYPTION_TYPE_NONE;
    decDesc.Configuration.InterlaceType = d3d12video::D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_NONE;

    d3d12video::ID3D12VideoDecoder* pDecoder = nullptr;
    hr = pVideoDevice->CreateVideoDecoder(&decDesc, d3d12video::IID_ID3D12VideoDecoder, reinterpret_cast<void**>(&pDecoder));
    TEST_ASSERT(hr == 0 && pDecoder != nullptr, "CreateVideoDecoder for H.264 must succeed");

    d3d12video::D3D12_VIDEO_DECODER_DESC descOut = pDecoder->GetDesc();
    TEST_ASSERT(descOut.Configuration.DecodeProfile == d3d12video::D3D12_VIDEO_DECODE_PROFILE_H264, "Decoder must preserve decode profile");

    // 8. Create Video Decoder Heap (1080p NV12, 16 Picture Buffers)
    d3d12video::D3D12_VIDEO_DECODER_HEAP_DESC heapDesc{};
    heapDesc.NodeMask = 0;
    heapDesc.Configuration = decDesc.Configuration;
    heapDesc.DecodeWidth = 1920;
    heapDesc.DecodeHeight = 1080;
    heapDesc.Format = prismx::DXGI_FORMAT_NV12;
    heapDesc.FrameRate = { 60, 1 };
    heapDesc.BitRate = 20000000;
    heapDesc.MaxDecodePictureBufferCount = 16;

    d3d12video::ID3D12VideoDecoderHeap* pDecoderHeap = nullptr;
    hr = pVideoDevice->CreateVideoDecoderHeap(&heapDesc, d3d12video::IID_ID3D12VideoDecoderHeap, reinterpret_cast<void**>(&pDecoderHeap));
    TEST_ASSERT(hr == 0 && pDecoderHeap != nullptr, "CreateVideoDecoderHeap must succeed");

    auto* heapImpl = dynamic_cast<d3d12video::CVideoDecoderHeap*>(pDecoderHeap);
    TEST_ASSERT(heapImpl != nullptr && heapImpl->GetAllocationSizeBytes() > 40 * 1024 * 1024, "Decoder heap must allocate backing VRAM for reference frames");

    // 9. Create Video Processor Instance (NV12 Input -> B8G8R8A8 Output)
    d3d12video::D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC inStreamDesc{};
    inStreamDesc.Format = prismx::DXGI_FORMAT_NV12;
    inStreamDesc.ColorSpace = prismx::DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709;
    inStreamDesc.SourceAspectRatio = { 16, 9 };
    inStreamDesc.DestinationAspectRatio = { 16, 9 };
    inStreamDesc.FrameRate = { 60, 1 };
    inStreamDesc.SourceRect = { 0, 0, 1920, 1080 };
    inStreamDesc.DestinationRect = { 0, 0, 1920, 1080 };
    inStreamDesc.Orientation = d3d12video::D3D12_VIDEO_PROCESS_ORIENTATION_DEFAULT;
    inStreamDesc.DeinterlaceFlags = d3d12video::D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_BOB;
    inStreamDesc.AlphaFillMode = d3d12video::D3D12_VIDEO_PROCESS_ALPHA_FILL_MODE_OPAQUE;

    d3d12video::D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC outStreamDesc{};
    outStreamDesc.Format = prismx::DXGI_FORMAT_B8G8R8A8_UNORM;
    outStreamDesc.ColorSpace = prismx::DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    outStreamDesc.AlphaFillMode = d3d12video::D3D12_VIDEO_PROCESS_ALPHA_FILL_MODE_OPAQUE;
    outStreamDesc.BackgroundColor[0] = 0;
    outStreamDesc.BackgroundColor[1] = 0;
    outStreamDesc.BackgroundColor[2] = 0;
    outStreamDesc.BackgroundColor[3] = 255;
    outStreamDesc.FrameRate = { 60, 1 };
    outStreamDesc.EnableStereo = 0;

    d3d12video::ID3D12VideoProcessor* pProcessor = nullptr;
    hr = pVideoDevice->CreateVideoProcessor(0, &outStreamDesc, 1, &inStreamDesc, d3d12video::IID_ID3D12VideoProcessor, reinterpret_cast<void**>(&pProcessor));
    TEST_ASSERT(hr == 0 && pProcessor != nullptr, "CreateVideoProcessor must succeed");
    TEST_ASSERT(pProcessor->GetNumInputStreamDescs() == 1, "Video processor must report 1 input stream");

    // 10. Direct3D 12 Video Decode Command Recording
    prism3d12::ID3D12CommandAllocator* pCmdAlloc = nullptr;
    hr = pDevice->CreateCommandAllocator(static_cast<prism3d12::D3D12_COMMAND_LIST_TYPE>(4), prism3d12::IID_ID3D12CommandAllocator, reinterpret_cast<void**>(&pCmdAlloc));
    TEST_ASSERT(hr == 0 && pCmdAlloc != nullptr, "CreateCommandAllocator for Video Decode must succeed");

    d3d12video::ID3D12VideoDecodeCommandList* pDecodeCmdList = nullptr;
    hr = pVideoDevice1->CreateVideoDecodeCommandList(0, pCmdAlloc, d3d12video::IID_ID3D12VideoDecodeCommandList, reinterpret_cast<void**>(&pDecodeCmdList));
    TEST_ASSERT(hr == 0 && pDecodeCmdList != nullptr, "CreateVideoDecodeCommandList must succeed");
    TEST_ASSERT(pDecodeCmdList->GetType() == static_cast<prism3d12::D3D12_COMMAND_LIST_TYPE>(4), "Command list must have VIDEO_DECODE type");

    // Create mock bitstream and output resources
    prism3d12::D3D12_RESOURCE_DESC bufDesc{};
    bufDesc.Dimension = prism3d12::D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = 65536; // 64 KB compressed H.264 slice NALU
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = prismx::DXGI_FORMAT_UNKNOWN;
    bufDesc.Layout = prism3d12::D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    prism3d12::D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = prism3d12::D3D12_HEAP_TYPE_DEFAULT;

    prism3d12::ID3D12Resource* pBitstream = nullptr;
    hr = pDevice->CreateCommittedResource(&heapProps, prism3d12::D3D12_HEAP_FLAG_NONE, &bufDesc, prism3d12::D3D12_RESOURCE_STATE_COMMON, nullptr, prism3d12::IID_ID3D12Resource, reinterpret_cast<void**>(&pBitstream));
    TEST_ASSERT(hr == 0 && pBitstream != nullptr, "CreateCommittedResource for bitstream buffer must succeed");

    prism3d12::D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = prism3d12::D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = 1920;
    texDesc.Height = 1080;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = prismx::DXGI_FORMAT_NV12;
    texDesc.Layout = prism3d12::D3D12_TEXTURE_LAYOUT_UNKNOWN;

    prism3d12::ID3D12Resource* pOutTex = nullptr;
    hr = pDevice->CreateCommittedResource(&heapProps, prism3d12::D3D12_HEAP_FLAG_NONE, &texDesc, prism3d12::D3D12_RESOURCE_STATE_COMMON, nullptr, prism3d12::IID_ID3D12Resource, reinterpret_cast<void**>(&pOutTex));
    TEST_ASSERT(hr == 0 && pOutTex != nullptr, "CreateCommittedResource for NV12 decoded output texture must succeed");

    // Record DecodeFrame
    d3d12video::D3D12_VIDEO_DECODE_INPUT_STREAM_ARGUMENTS inArgs{};
    inArgs.CompressedBitstream.pBuffer = pBitstream;
    inArgs.CompressedBitstream.Offset = 0;
    inArgs.CompressedBitstream.Size = 65536;
    inArgs.pHeap = pDecoderHeap;

    d3d12video::D3D12_VIDEO_DECODE_OUTPUT_STREAM_ARGUMENTS outArgs{};
    outArgs.pOutputTexture2D = pOutTex;
    outArgs.OutputSubresource = 0;

    pDecodeCmdList->DecodeFrame(pDecoder, &outArgs, &inArgs);

    auto* decodeCmdListImpl = dynamic_cast<d3d12video::CVideoDecodeCommandList*>(pDecodeCmdList);
    TEST_ASSERT(decodeCmdListImpl != nullptr && decodeCmdListImpl->GetRecordedDecodes() == 1, "DecodeCommandList must record 1 frame decode");
    TEST_ASSERT(decodeCmdListImpl->GetProcessedBitstreamBytes() == 65536, "DecodeCommandList must track 64KB bitstream bytes");

    hr = pDecodeCmdList->Close();
    TEST_ASSERT(hr == 0 && decodeCmdListImpl->IsClosed(), "Closing DecodeCommandList must succeed");

    hr = pDecodeCmdList->Reset(pCmdAlloc);
    TEST_ASSERT(hr == 0 && !decodeCmdListImpl->IsClosed() && decodeCmdListImpl->GetRecordedDecodes() == 0, "Resetting DecodeCommandList must clear recorded state");

    // 11. Direct3D 12 Video Process Command Recording
    d3d12video::ID3D12VideoProcessCommandList* pProcessCmdList = nullptr;
    hr = pVideoDevice1->CreateVideoProcessCommandList(0, pCmdAlloc, d3d12video::IID_ID3D12VideoProcessCommandList, reinterpret_cast<void**>(&pProcessCmdList));
    TEST_ASSERT(hr == 0 && pProcessCmdList != nullptr, "CreateVideoProcessCommandList must succeed");
    TEST_ASSERT(pProcessCmdList->GetType() == static_cast<prism3d12::D3D12_COMMAND_LIST_TYPE>(5), "Command list must have VIDEO_PROCESS type");

    d3d12video::D3D12_VIDEO_PROCESS_INPUT_STREAM_ARGUMENTS procInArgs{};
    procInArgs.pInputTexture2D = pOutTex;
    procInArgs.InputSubresource = 0;
    procInArgs.SourceRect = { 0, 0, 1920, 1080 };
    procInArgs.DestinationRect = { 0, 0, 1280, 720 };
    procInArgs.Alpha = 0.95f;
    procInArgs.FilterLevels[d3d12video::D3D12_VIDEO_PROCESS_FILTER_CONTRAST] = 10;

    d3d12video::D3D12_VIDEO_PROCESS_OUTPUT_STREAM_ARGUMENTS procOutArgs{};
    procOutArgs.pOutputTexture2D = pOutTex;
    procOutArgs.OutputSubresource = 0;
    procOutArgs.TargetRect = { 0, 0, 1280, 720 };

    pProcessCmdList->ProcessFrames(pProcessor, &procOutArgs, 1, &procInArgs);

    auto* procCmdListImpl = dynamic_cast<d3d12video::CVideoProcessCommandList*>(pProcessCmdList);
    TEST_ASSERT(procCmdListImpl != nullptr && procCmdListImpl->GetRecordedProcesses() == 1, "ProcessCommandList must record 1 frame process operation");

    hr = pProcessCmdList->Close();
    TEST_ASSERT(hr == 0 && procCmdListImpl->IsClosed(), "Closing ProcessCommandList must succeed");

    // 12. Clean Teardown & Reference Counting
    pBitstream->Release();
    pOutTex->Release();
    pProcessCmdList->Release();
    pDecodeCmdList->Release();
    pCmdAlloc->Release();
    pProcessor->Release();
    pDecoderHeap->Release();
    pDecoder->Release();
    pVideoDevice1->Release();
    pVideoDevice->Release();
    pDevice->Release();

    std::cout << "[TEST] Suite 111: Windows Direct3D 12 Video Decode & Processing Subsystem PASSED.\n";
}

void Test_WindowsMediaFoundation_SourceReader_SinkWriter_Subsystem() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 112: Windows Media Foundation Source Reader & Sink Writer       \n";
    std::cout << "========================================================================\n";

    // 1. Initialize Dynamic Loader Exports & Version Database
    mfreadwrite::InitializeMFReadWriteExports();
    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("mfreadwrite.dll", "MFCreateSourceReaderFromURL") != nullptr, "MFCreateSourceReaderFromURL export must exist");
    TEST_ASSERT(ldr.getExport("mfreadwrite.dll", "MFCreateSinkWriterFromURL") != nullptr, "MFCreateSinkWriterFromURL export must exist");
    TEST_ASSERT(ldr.getExport("mfreadwrite.dll", "DllCanUnloadNow") != nullptr, "DllCanUnloadNow export must exist");

    auto* mod = version::VersionDatabase::Instance().FindModule("mfreadwrite.dll");
    TEST_ASSERT(mod != nullptr, "mfreadwrite.dll must be registered in VersionDatabase");
    TEST_ASSERT(mod->stringTable.at("ProductVersion") == "10.0.22621.1", "mfreadwrite.dll must report Windows 11 Build 22621 version parity");

    // 2. Create Source Reader from URL & Verify COM Interface Query
    mf::IMFSourceReader* pReader = nullptr;
    int32_t hr = mfreadwrite::MFCreateSourceReaderFromURL(L"C:\\Videos\\trailer_4k.mp4", nullptr, &pReader);
    TEST_ASSERT(hr == 0 && pReader != nullptr, "MFCreateSourceReaderFromURL must succeed");

    mfreadwrite::IMFSourceReaderEx* pReaderEx = nullptr;
    hr = pReader->QueryInterface(mfreadwrite::IID_IMFSourceReaderEx_Const, reinterpret_cast<void**>(&pReaderEx));
    TEST_ASSERT(hr == 0 && pReaderEx != nullptr, "QueryInterface for IMFSourceReaderEx must succeed");

    // 3. Multi-Stream Stream Selection
    int32_t vSelected = 0, aSelected = 0;
    hr = pReader->GetStreamSelection(mfreadwrite::MF_SOURCE_READER_FIRST_VIDEO_STREAM, &vSelected);
    TEST_ASSERT(hr == 0 && vSelected == 1, "First video stream must be selected by default");

    hr = pReader->GetStreamSelection(mfreadwrite::MF_SOURCE_READER_FIRST_AUDIO_STREAM, &aSelected);
    TEST_ASSERT(hr == 0 && aSelected == 1, "First audio stream must be selected by default");

    hr = pReader->SetStreamSelection(mfreadwrite::MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0);
    TEST_ASSERT(hr == 0, "Deselecting audio stream must succeed");
    hr = pReader->GetStreamSelection(mfreadwrite::MF_SOURCE_READER_FIRST_AUDIO_STREAM, &aSelected);
    TEST_ASSERT(hr == 0 && aSelected == 0, "Audio stream must now be deselected");
    pReader->SetStreamSelection(mfreadwrite::MF_SOURCE_READER_FIRST_AUDIO_STREAM, 1);

    // 4. Native Stream Formats Inspection
    mf::IMFMediaType* pNativeVideo = nullptr;
    hr = pReader->GetNativeMediaType(mfreadwrite::MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &pNativeVideo);
    TEST_ASSERT(hr == 0 && pNativeVideo != nullptr, "GetNativeMediaType for video stream must succeed");

    GUID vMajor{}, vSub{};
    pNativeVideo->GetGUID(mf::MF_MT_MAJOR_TYPE, &vMajor);
    pNativeVideo->GetGUID(mf::MF_MT_SUBTYPE, &vSub);
    TEST_ASSERT(vMajor == mf::MFMediaType_Video && vSub == mf::MFVideoFormat_H264, "Native video stream must be H.264 Video");

    mf::IMFMediaType* pNativeAudio = nullptr;
    hr = pReader->GetNativeMediaType(mfreadwrite::MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &pNativeAudio);
    TEST_ASSERT(hr == 0 && pNativeAudio != nullptr, "GetNativeMediaType for audio stream must succeed");

    GUID aMajor{}, aSub{};
    pNativeAudio->GetGUID(mf::MF_MT_MAJOR_TYPE, &aMajor);
    pNativeAudio->GetGUID(mf::MF_MT_SUBTYPE, &aSub);
    TEST_ASSERT(aMajor == mf::MFMediaType_Audio && aSub == mf::MFAudioFormat_AAC, "Native audio stream must be AAC Audio");

    // 5. Output Format Negotiation (H.264 -> NV12 / RGB32)
    mf::IMFMediaType* pCurrVideo = nullptr;
    hr = pReader->GetCurrentMediaType(mfreadwrite::MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pCurrVideo);
    TEST_ASSERT(hr == 0 && pCurrVideo != nullptr, "GetCurrentMediaType for video stream must succeed");

    GUID currSub{};
    pCurrVideo->GetGUID(mf::MF_MT_SUBTYPE, &currSub);
    TEST_ASSERT(currSub == mf::MFVideoFormat_NV12, "Default current video output format must be uncompressed NV12");

    auto* pRgbType = new mf::CMediaType();
    pRgbType->SetGUID(mf::MF_MT_MAJOR_TYPE, mf::MFMediaType_Video);
    pRgbType->SetGUID(mf::MF_MT_SUBTYPE, mf::MFVideoFormat_RGB32);
    hr = pReader->SetCurrentMediaType(mfreadwrite::MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pRgbType);
    TEST_ASSERT(hr == 0, "SetCurrentMediaType to RGB32 must succeed");

    // 6. Synchronous Sample Extraction & Buffer Validation
    uint32_t actualIdx = 0, flags = 0;
    int64_t timestamp = 0;
    mf::IMFSample* pSample = nullptr;
    hr = pReader->ReadSample(mfreadwrite::MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &actualIdx, &flags, &timestamp, &pSample);
    TEST_ASSERT(hr == 0 && pSample != nullptr, "ReadSample must return a valid sample");
    TEST_ASSERT(actualIdx == 0, "Actual stream index must be 0 for first video stream");
    TEST_ASSERT(timestamp == 0, "First frame timestamp must be 0");

    uint32_t bufLen = 0;
    pSample->GetTotalLength(&bufLen);
    TEST_ASSERT(bufLen == 4096, "Sample total buffer length must match video payload size (4096 bytes)");

    int64_t duration = 0;
    pSample->GetSampleDuration(&duration);
    TEST_ASSERT(duration == 333333LL, "Sample duration must be ~33.3ms (333333 hns) for 30fps video");

    // 7. Dynamic Transform Management via IMFSourceReaderEx
    auto* pColorMFT = new mf::CColorConvertMFT();
    hr = pReaderEx->AddTransformForStream(0, pColorMFT);
    TEST_ASSERT(hr == 0, "AddTransformForStream must succeed");

    mf::IMFTransform* pTransOut = nullptr;
    GUID catGuid{};
    hr = pReaderEx->GetTransformForStream(0, 0, &catGuid, &pTransOut);
    TEST_ASSERT(hr == 0 && pTransOut != nullptr, "GetTransformForStream must retrieve the installed transform");
    pTransOut->Release();

    hr = pReaderEx->RemoveAllTransformsForStream(0);
    TEST_ASSERT(hr == 0, "RemoveAllTransformsForStream must succeed");

    // 8. Stream Flush & Position Reset
    hr = pReader->Flush(mfreadwrite::MF_SOURCE_READER_FIRST_VIDEO_STREAM);
    TEST_ASSERT(hr == 0, "Flush on video stream must succeed");

    // 9. Asynchronous Source Reader Callback Operation
    class TestReaderCallback : public mfreadwrite::IMFSourceReaderCallback {
        uint32_t m_ref{ 1 };
    public:
        std::atomic<bool> sampleReceived{ false };
        int64_t lastTimestamp{ 0 };

        int32_t __stdcall QueryInterface(const GUID& riid, void** ppv) override {
            if (!ppv) return ole32::E_POINTER;
            if (riid == ole32::IID_IUnknown || riid == mfreadwrite::IID_IMFSourceReaderCallback_Const) {
                *ppv = this;
                AddRef();
                return ole32::S_OK;
            }
            *ppv = nullptr;
            return ole32::E_NOINTERFACE;
        }
        uint32_t __stdcall AddRef() override { return ++m_ref; }
        uint32_t __stdcall Release() override {
            uint32_t r = --m_ref;
            if (r == 0) delete this;
            return r;
        }
        int32_t __stdcall OnReadSample(int32_t, uint32_t, uint32_t, mf::LONGLONG llTimestamp, mf::IMFSample*) override {
            lastTimestamp = llTimestamp;
            sampleReceived = true;
            return ole32::S_OK;
        }
        int32_t __stdcall OnFlush(uint32_t) override { return ole32::S_OK; }
        int32_t __stdcall OnEvent(uint32_t, mf::IMFMediaEvent*) override { return ole32::S_OK; }
    };

    auto* pCallback = new TestReaderCallback();
    auto* pAsyncAttrs = new mf::CAttributes();
    pAsyncAttrs->SetUnknown(mfreadwrite::MF_SOURCE_READER_ASYNC_CALLBACK, pCallback);

    mf::IMFSourceReader* pAsyncReader = nullptr;
    hr = mfreadwrite::MFCreateSourceReaderFromURL(L"stream.mp4", pAsyncAttrs, &pAsyncReader);
    TEST_ASSERT(hr == 0 && pAsyncReader != nullptr, "Async reader creation must succeed");

    mf::IMFSample* pAsyncSample = nullptr;
    hr = pAsyncReader->ReadSample(mfreadwrite::MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, &timestamp, &pAsyncSample);
    TEST_ASSERT(hr == 0, "Async ReadSample invocation must succeed");
    TEST_ASSERT(pCallback->sampleReceived.load(), "Callback OnReadSample must be dispatched");
    if (pAsyncSample) pAsyncSample->Release();
    pAsyncReader->Release();
    pAsyncAttrs->Release();
    pCallback->Release();

    // 10. Hardware Direct3D Manager Binding Verification
    auto* pD3DAttrs = new mf::CAttributes();
    auto* pFakeD3DManager = new mf::CAttributes();
    pD3DAttrs->SetUnknown(mfreadwrite::MF_SOURCE_READER_D3D_MANAGER, pFakeD3DManager);

    mf::IMFSourceReader* pD3DReader = nullptr;
    hr = mfreadwrite::MFCreateSourceReaderFromURL(L"hw_accel.mp4", pD3DAttrs, &pD3DReader);
    TEST_ASSERT(hr == 0 && pD3DReader != nullptr, "Reader creation with D3D manager attribute must succeed");

    ole32::IUnknown* pRetrievedD3D = nullptr;
    hr = pD3DReader->GetServiceForStream(0, mfreadwrite::MF_SOURCE_READER_D3D_MANAGER, ole32::IID_IUnknown, reinterpret_cast<void**>(&pRetrievedD3D));
    TEST_ASSERT(hr == 0 && pRetrievedD3D != nullptr, "GetServiceForStream must yield bound D3D manager");
    if (pRetrievedD3D) pRetrievedD3D->Release();
    pD3DReader->Release();
    pFakeD3DManager->Release();
    pD3DAttrs->Release();

    // 11. Create Sink Writer from URL & Verify COM Interface Query
    mf::IMFSinkWriter* pWriter = nullptr;
    hr = mfreadwrite::MFCreateSinkWriterFromURL(L"C:\\Videos\\output_master.mp4", nullptr, nullptr, &pWriter);
    TEST_ASSERT(hr == 0 && pWriter != nullptr, "MFCreateSinkWriterFromURL must succeed");

    mfreadwrite::IMFSinkWriterEx* pWriterEx = nullptr;
    hr = pWriter->QueryInterface(mfreadwrite::IID_IMFSinkWriterEx_Const, reinterpret_cast<void**>(&pWriterEx));
    TEST_ASSERT(hr == 0 && pWriterEx != nullptr, "QueryInterface for IMFSinkWriterEx must succeed");

    // 12. Add Output Streams & Configure Input Types
    uint32_t outVideoStream = 0;
    hr = pWriter->AddStream(pNativeVideo, &outVideoStream);
    TEST_ASSERT(hr == 0 && outVideoStream == 0, "AddStream for video must assign stream index 0");

    hr = pWriter->SetInputMediaType(outVideoStream, pRgbType, nullptr);
    TEST_ASSERT(hr == 0, "SetInputMediaType (RGB32 uncompressed input) must succeed");

    uint32_t outAudioStream = 0;
    hr = pWriter->AddStream(pNativeAudio, &outAudioStream);
    TEST_ASSERT(hr == 0 && outAudioStream == 1, "AddStream for audio must assign stream index 1");

    auto* pPcmType = new mf::CMediaType();
    pPcmType->SetGUID(mf::MF_MT_MAJOR_TYPE, mf::MFMediaType_Audio);
    pPcmType->SetGUID(mf::MF_MT_SUBTYPE, mf::MFAudioFormat_PCM);
    hr = pWriter->SetInputMediaType(outAudioStream, pPcmType, nullptr);
    TEST_ASSERT(hr == 0, "SetInputMediaType (PCM uncompressed audio) must succeed");

    // 13. Begin Writing Lifecycle & Sample Emission
    hr = pWriter->BeginWriting();
    TEST_ASSERT(hr == 0, "BeginWriting must transition sink writer to active recording state");

    for (int frame = 0; frame < 10; ++frame) {
        auto* frameSample = new mf::CSample();
        auto* frameBuf = new mf::CMediaBuffer(4096);
        frameBuf->SetCurrentLength(4096);
        frameSample->AddBuffer(frameBuf);
        frameSample->SetSampleTime(frame * 333333LL);
        frameSample->SetSampleDuration(333333LL);

        hr = pWriter->WriteSample(outVideoStream, frameSample);
        TEST_ASSERT(hr == 0, "WriteSample must accept video frames");
        frameBuf->Release();
        frameSample->Release();
    }

    // 14. Markers, End of Segment & Flush
    hr = pWriter->SendStreamTick(outVideoStream, 10 * 333333LL);
    TEST_ASSERT(hr == 0, "SendStreamTick must succeed");

    hr = pWriter->PlaceMarker(outVideoStream, nullptr);
    TEST_ASSERT(hr == 0, "PlaceMarker must succeed");

    hr = pWriter->NotifyEndOfSegment(outVideoStream);
    TEST_ASSERT(hr == 0, "NotifyEndOfSegment must succeed");

    hr = pWriter->Flush(outVideoStream);
    TEST_ASSERT(hr == 0, "Flush on sink writer must succeed");

    // 15. Finalize Container & Inspect Telemetry
    hr = pWriter->Finalize();
    TEST_ASSERT(hr == 0, "Finalize must seal and commit output media file");

    auto* writerImpl = static_cast<mfreadwrite::CAdvancedSinkWriter*>(pWriterEx);
    TEST_ASSERT(writerImpl->isFinalized(), "Sink writer state must be finalized");
    TEST_ASSERT(writerImpl->getSamplesWritten(outVideoStream) == 10, "Sink writer must record exactly 10 video frames written");
    TEST_ASSERT(writerImpl->getBytesWritten(outVideoStream) == 40960, "Sink writer must record exactly 40,960 bytes multiplexed");

    // 16. Shell CLI Commands Verification
    std::ostringstream testOut;
    micant::shell::CommandShell shellEngine;
    int rc = shellEngine.execute("mfreadwrite test", testOut);
    TEST_ASSERT(rc == 0, "mfreadwrite test CLI command must return 0");
    TEST_ASSERT(testOut.str().find("16/16 PASSED") != std::string::npos, "mfreadwrite test must pass all 16 tests");

    std::ostringstream infoOut;
    rc = shellEngine.execute("mfreadwrite info", infoOut);
    TEST_ASSERT(rc == 0, "mfreadwrite info CLI command must return 0");
    TEST_ASSERT(infoOut.str().find("IMFSourceReaderEx") != std::string::npos, "mfreadwrite info must display architecture telemetry");

    // Cleanup
    pPcmType->Release();
    pWriterEx->Release();
    pWriter->Release();
    pColorMFT->Release();
    pSample->Release();
    pRgbType->Release();
    pCurrVideo->Release();
    pNativeAudio->Release();
    pNativeVideo->Release();
    pReaderEx->Release();
    pReader->Release();

    std::cout << "[TEST] Suite 112: Windows Media Foundation Source Reader & Sink Writer PASSED.\n";
}

// ============================================================================
// Suite 113: Windows Media Foundation Capture Engine Subsystem
// ============================================================================
void Test_WindowsMediaFoundation_CaptureEngine_Subsystem() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 113: Windows Media Foundation Capture Engine Subsystem          \n";
    std::cout << "========================================================================\n";

    mfcapture::InitializeMFCaptureEngineExports();

    // 1. Dynamic Module Export Verification
    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("mfcaptureengine.dll", "MFCreateCaptureEngine") != nullptr, "MFCreateCaptureEngine must be exported");
    TEST_ASSERT(ldr.getExport("mfcaptureengine.dll", "DllCanUnloadNow") != nullptr, "DllCanUnloadNow must be exported");
    TEST_ASSERT(ldr.getExport("mfcaptureengine.dll", "DllGetClassObject") != nullptr, "DllGetClassObject must be exported");

    // 2. Version Database Verification
    const auto* mod = version::VersionDatabase::Instance().GetModuleInfo("mfcaptureengine.dll");
    TEST_ASSERT(mod != nullptr, "VersionDatabase must contain mfcaptureengine.dll");
    TEST_ASSERT(mod->stringTable.at("ProductVersion") == "10.0.22621.1", "ProductVersion must be 10.0.22621.1");

    // 3. Create Capture Engine via Factory Function
    mfcapture::IMFCaptureEngine* pEngine = nullptr;
    int32_t hr = mfcapture::MFCreateCaptureEngine(&pEngine);
    TEST_ASSERT(hr == 0 && pEngine != nullptr, "MFCreateCaptureEngine must succeed");

    // 4. Query COM Interfaces & Class Factory
    mfcapture::IMFCaptureEngineClassFactory* pFactory = nullptr;
    hr = mfcapture::DllGetClassObject(mfcapture::CLSID_MFCaptureEngineClassFactory_Const,
                                      mfcapture::IID_IMFCaptureEngineClassFactory_Const,
                                      reinterpret_cast<void**>(&pFactory));
    TEST_ASSERT(hr == 0 && pFactory != nullptr, "DllGetClassObject for IMFCaptureEngineClassFactory must succeed");

    mfcapture::IMFCaptureEngine* pEngineFromFactory = nullptr;
    hr = pFactory->CreateInstance(mfcapture::CLSID_MFCaptureEngine_Const,
                                  mfcapture::IID_IMFCaptureEngine_Const,
                                  reinterpret_cast<void**>(&pEngineFromFactory));
    TEST_ASSERT(hr == 0 && pEngineFromFactory != nullptr, "Factory CreateInstance must create IMFCaptureEngine");

    // 5. Query Capture Source & Stream Enumeration
    mfcapture::IMFCaptureSource* pSource = nullptr;
    hr = pEngine->GetSource(&pSource);
    TEST_ASSERT(hr == 0 && pSource != nullptr, "GetSource must retrieve IMFCaptureSource");

    uint32_t streamCount = 0;
    hr = pSource->GetDeviceStreamCount(&streamCount);
    TEST_ASSERT(hr == 0 && streamCount == 3, "Capture source must provide 3 streams (Video, Audio, Photo)");

    mfcapture::MF_CAPTURE_ENGINE_STREAM_CATEGORY catVideo{}, catAudio{}, catPhoto{};
    pSource->GetDeviceStreamCategory(0, &catVideo);
    pSource->GetDeviceStreamCategory(1, &catAudio);
    pSource->GetDeviceStreamCategory(2, &catPhoto);
    TEST_ASSERT(catVideo == mfcapture::MF_CAPTURE_ENGINE_STREAM_CATEGORY_VIDEO_RECORD, "Stream 0 must be Video Record");
    TEST_ASSERT(catAudio == mfcapture::MF_CAPTURE_ENGINE_STREAM_CATEGORY_AUDIO, "Stream 1 must be Audio");
    TEST_ASSERT(catPhoto == mfcapture::MF_CAPTURE_ENGINE_STREAM_CATEGORY_PHOTO_INDEPENDENT, "Stream 2 must be Independent Photo");

    // 6. Device Media Type Discovery (1080p, 4K, PCM 48kHz)
    mf::IMFMediaType* pNv12_1080 = nullptr;
    mf::IMFMediaType* pRgb_1080 = nullptr;
    mf::IMFMediaType* pNv12_4k = nullptr;
    hr = pSource->GetAvailableDeviceMediaType(0, 0, &pNv12_1080);
    TEST_ASSERT(hr == 0 && pNv12_1080 != nullptr, "Stream 0 Type 0 must be available");
    hr = pSource->GetAvailableDeviceMediaType(0, 1, &pRgb_1080);
    TEST_ASSERT(hr == 0 && pRgb_1080 != nullptr, "Stream 0 Type 1 must be available");
    hr = pSource->GetAvailableDeviceMediaType(0, 2, &pNv12_4k);
    TEST_ASSERT(hr == 0 && pNv12_4k != nullptr, "Stream 0 Type 2 must be available");

    uint64_t frameSize1080 = 0, frameSize4k = 0;
    pNv12_1080->GetUINT64(mf::MF_MT_FRAME_SIZE, &frameSize1080);
    pNv12_4k->GetUINT64(mf::MF_MT_FRAME_SIZE, &frameSize4k);
    TEST_ASSERT(frameSize1080 == ((1920ULL << 32) | 1080ULL), "1080p frame size must be 1920x1080");
    TEST_ASSERT(frameSize4k == ((3840ULL << 32) | 2160ULL), "4K frame size must be 3840x2160");

    // 7. Preview Sink Query & Display Configuration
    mfcapture::IMFCaptureSink* pSinkBase = nullptr;
    hr = pEngine->GetSink(mfcapture::MF_CAPTURE_ENGINE_SINK_TYPE_PREVIEW, &pSinkBase);
    TEST_ASSERT(hr == 0 && pSinkBase != nullptr, "GetSink for PREVIEW must succeed");

    mfcapture::IMFCapturePreviewSink* pPreviewSink = nullptr;
    hr = pSinkBase->QueryInterface(mfcapture::IID_IMFCapturePreviewSink_Const, reinterpret_cast<void**>(&pPreviewSink));
    TEST_ASSERT(hr == 0 && pPreviewSink != nullptr, "QueryInterface for IMFCapturePreviewSink must succeed");

    pPreviewSink->SetRenderHandle(0xCAFE);
    pPreviewSink->SetMirrorState(1);
    pPreviewSink->SetRotation(0, 180);
    int32_t mirrorState = 0;
    uint32_t rotationAngle = 0;
    pPreviewSink->GetMirrorState(&mirrorState);
    pPreviewSink->GetRotation(0, &rotationAngle);
    TEST_ASSERT(mirrorState == 1, "Preview mirror state must be enabled");
    TEST_ASSERT(rotationAngle == 180, "Preview rotation must be 180 degrees");

    // 8. Record Sink Query & Destination Configuration
    mfcapture::IMFCaptureSink* pRecBase = nullptr;
    hr = pEngine->GetSink(mfcapture::MF_CAPTURE_ENGINE_SINK_TYPE_RECORD, &pRecBase);
    TEST_ASSERT(hr == 0 && pRecBase != nullptr, "GetSink for RECORD must succeed");

    mfcapture::IMFCaptureRecordSink* pRecordSink = nullptr;
    hr = pRecBase->QueryInterface(mfcapture::IID_IMFCaptureRecordSink_Const, reinterpret_cast<void**>(&pRecordSink));
    TEST_ASSERT(hr == 0 && pRecordSink != nullptr, "QueryInterface for IMFCaptureRecordSink must succeed");

    pRecordSink->SetOutputFileName(L"C:\\Videos\\session.mp4");
    pRecordSink->SetRotation(0, 0);

    // 9. Photo Sink Query
    mfcapture::IMFCaptureSink* pPhotoBase = nullptr;
    hr = pEngine->GetSink(mfcapture::MF_CAPTURE_ENGINE_SINK_TYPE_PHOTO, &pPhotoBase);
    TEST_ASSERT(hr == 0 && pPhotoBase != nullptr, "GetSink for PHOTO must succeed");

    mfcapture::IMFCapturePhotoSink* pPhotoSink = nullptr;
    hr = pPhotoBase->QueryInterface(mfcapture::IID_IMFCapturePhotoSink_Const, reinterpret_cast<void**>(&pPhotoSink));
    TEST_ASSERT(hr == 0 && pPhotoSink != nullptr, "QueryInterface for IMFCapturePhotoSink must succeed");

    pPhotoSink->SetOutputFileName(L"C:\\Photos\\snap_master.png");

    // 10. Asynchronous Event Callback Dispatching
    struct TestCaptureCallback : public mfcapture::IMFCaptureEngineOnEventCallback {
        std::atomic<uint32_t> ref{ 1 };
        std::atomic<bool> evtInit{ false };
        std::atomic<bool> evtPreviewStart{ false };
        std::atomic<bool> evtPreviewStop{ false };
        std::atomic<bool> evtRecordStart{ false };
        std::atomic<bool> evtRecordStop{ false };
        std::atomic<bool> evtPhoto{ false };

        int32_t __stdcall QueryInterface(const GUID& riid, void** ppv) override {
            if (!ppv) return ole32::E_POINTER;
            if (riid == ole32::IID_IUnknown || riid == mfcapture::IID_IMFCaptureEngineOnEventCallback_Const) {
                *ppv = this;
                AddRef();
                return ole32::S_OK;
            }
            *ppv = nullptr;
            return ole32::E_NOINTERFACE;
        }
        uint32_t __stdcall AddRef() override { return ++ref; }
        uint32_t __stdcall Release() override {
            uint32_t r = --ref;
            if (r == 0) delete this;
            return r;
        }
        int32_t __stdcall OnEvent(mf::IMFMediaEvent* pEvent) override {
            if (pEvent) {
                GUID extType{};
                pEvent->GetExtendedType(&extType);
                if (extType == mfcapture::MF_CAPTURE_ENGINE_INITIALIZED) evtInit = true;
                if (extType == mfcapture::MF_CAPTURE_ENGINE_PREVIEW_STARTED) evtPreviewStart = true;
                if (extType == mfcapture::MF_CAPTURE_ENGINE_PREVIEW_STOPPED) evtPreviewStop = true;
                if (extType == mfcapture::MF_CAPTURE_ENGINE_RECORD_STARTED) evtRecordStart = true;
                if (extType == mfcapture::MF_CAPTURE_ENGINE_RECORD_STOPPED) evtRecordStop = true;
                if (extType == mfcapture::MF_CAPTURE_ENGINE_PHOTO_TAKEN) evtPhoto = true;
            }
            return ole32::S_OK;
        }
    };

    auto* pCb = new TestCaptureCallback();
    hr = pEngine->Initialize(pCb, nullptr, nullptr, nullptr);
    TEST_ASSERT(hr == 0, "IMFCaptureEngine::Initialize must return S_OK");
    TEST_ASSERT(pCb->evtInit.load(), "MF_CAPTURE_ENGINE_INITIALIZED event must be dispatched");

    // 11. Preview Lifecycle & Frame Ingestion
    hr = pEngine->StartPreview();
    TEST_ASSERT(hr == 0, "StartPreview must succeed");
    TEST_ASSERT(pCb->evtPreviewStart.load(), "MF_CAPTURE_ENGINE_PREVIEW_STARTED event must be dispatched");
    auto* pPreImpl = static_cast<mfcapture::CCapturePreviewSink*>(pPreviewSink);
    TEST_ASSERT(pPreImpl->getFramesDelivered() == 3, "Preview sink must ingest 3 frames");

    // 12. Record Lifecycle & Sample Multiplexing
    hr = pEngine->StartRecord();
    TEST_ASSERT(hr == 0, "StartRecord must succeed");
    TEST_ASSERT(pCb->evtRecordStart.load(), "MF_CAPTURE_ENGINE_RECORD_STARTED event must be dispatched");
    auto* pRecImpl = static_cast<mfcapture::CCaptureRecordSink*>(pRecordSink);
    TEST_ASSERT(pRecImpl->getSamplesRecorded() == 10, "Record sink must multiplex 10 samples (5 video + 5 audio)");

    // 13. Photo Snapshot Capture
    hr = pEngine->TakePhoto();
    TEST_ASSERT(hr == 0, "TakePhoto must succeed");
    TEST_ASSERT(pCb->evtPhoto.load(), "MF_CAPTURE_ENGINE_PHOTO_TAKEN event must be dispatched");
    auto* pPhotoImpl = static_cast<mfcapture::CCapturePhotoSink*>(pPhotoSink);
    TEST_ASSERT(pPhotoImpl->getPhotosTaken() == 1, "Photo sink must record 1 photo taken");

    // 14. Stop Lifecycle
    hr = pEngine->StopRecord(1, 0);
    TEST_ASSERT(hr == 0, "StopRecord must succeed");
    TEST_ASSERT(pCb->evtRecordStop.load(), "MF_CAPTURE_ENGINE_RECORD_STOPPED event must be dispatched");

    hr = pEngine->StopPreview();
    TEST_ASSERT(hr == 0, "StopPreview must succeed");
    TEST_ASSERT(pCb->evtPreviewStop.load(), "MF_CAPTURE_ENGINE_PREVIEW_STOPPED event must be dispatched");

    // 15. Real-Time MFT Transform Insertion & Live DSP Pipeline
    auto* pColorMFT = new mf::CColorConvertMFT();
    hr = pSource->AddEffect(0, pColorMFT);
    TEST_ASSERT(hr == 0, "AddEffect must attach MFT to stream 0");
    auto* pSrcImpl = static_cast<mfcapture::CCaptureSource*>(pSource);
    TEST_ASSERT(pSrcImpl->getEffectCount(0) == 1, "Stream 0 must have exactly 1 active effect");
    hr = pSource->RemoveAllEffects(0);
    TEST_ASSERT(hr == 0 && pSrcImpl->getEffectCount(0) == 0, "RemoveAllEffects must clear active effects");

    // 16. Shell CLI Commands Verification
    std::ostringstream testOut;
    micant::shell::CommandShell shellEngine;
    int rc = shellEngine.execute("mfcapture test", testOut);
    TEST_ASSERT(rc == 0, "mfcapture test CLI command must return 0");
    TEST_ASSERT(testOut.str().find("16/16 PASSED") != std::string::npos, "mfcapture test must pass all 16 tests");

    std::ostringstream infoOut;
    rc = shellEngine.execute("mfcapture info", infoOut);
    TEST_ASSERT(rc == 0, "mfcapture info CLI command must return 0");
    TEST_ASSERT(infoOut.str().find("IMFCaptureEngine") != std::string::npos, "mfcapture info must display architecture telemetry");

    // Cleanup
    pColorMFT->Release();
    pCb->Release();
    pPhotoSink->Release();
    pPhotoBase->Release();
    pRecordSink->Release();
    pRecBase->Release();
    pPreviewSink->Release();
    pSinkBase->Release();
    pNv12_4k->Release();
    pRgb_1080->Release();
    pNv12_1080->Release();
    pSource->Release();
    pEngineFromFactory->Release();
    pFactory->Release();
    pEngine->Release();

    std::cout << "[TEST] Suite 113: Windows Media Foundation Capture Engine Subsystem PASSED.\n";
}

// ============================================================================
// Suite 114: Windows DirectX 12 Raytracing (DXR) & Mesh Shader Subsystem
// ============================================================================
void Test_WindowsDirectX_Raytracing_Subsystem() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 114: Windows DirectX 12 Raytracing & Mesh Shader Subsystem      \n";
    std::cout << "========================================================================\n";

    dxr::InitializeDXRExports();

    // 1. Dynamic Module Export Verification
    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("d3d12.dll", "D3D12CreateRaytracingDevice") != nullptr, "D3D12CreateRaytracingDevice must be exported");

    // 2. Version Database Verification
    const auto* mod = version::VersionDatabase::Instance().GetModuleInfo("d3d12raytracing.dll");
    TEST_ASSERT(mod != nullptr, "VersionDatabase must contain d3d12raytracing.dll");
    TEST_ASSERT(mod->stringTable.at("ProductVersion") == "10.0.22621.1", "ProductVersion must be 10.0.22621.1");

    // 3. Create Raytracing Device with Feature Level 12_2 (DirectX 12 Ultimate)
    dxr::ID3D12Device5* pDevice5 = nullptr;
    int32_t hr = dxr::D3D12CreateRaytracingDevice(nullptr, prism3d::D3D_FEATURE_LEVEL_12_2, dxr::IID_ID3D12Device5_Const, reinterpret_cast<void**>(&pDevice5));
    TEST_ASSERT(hr == 0 && pDevice5 != nullptr, "D3D12CreateRaytracingDevice must succeed");

    // 4. Check Feature Support for DXR Tier 1.1
    dxr::D3D12_FEATURE_DATA_D3D12_OPTIONS5 opts5{};
    hr = pDevice5->CheckFeatureSupport(27, &opts5, sizeof(opts5));
    TEST_ASSERT(hr == 0, "CheckFeatureSupport for D3D12_OPTIONS5 must succeed");
    TEST_ASSERT(opts5.RaytracingTier == dxr::D3D12_RAYTRACING_TIER_1_1, "Device must report D3D12_RAYTRACING_TIER_1_1 support");

    // 5. Check Feature Support for Mesh Shader Tier 1
    dxr::D3D12_FEATURE_DATA_D3D12_OPTIONS7 opts7{};
    hr = pDevice5->CheckFeatureSupport(32, &opts7, sizeof(opts7));
    TEST_ASSERT(hr == 0, "CheckFeatureSupport for D3D12_OPTIONS7 must succeed");
    TEST_ASSERT(opts7.MeshShaderTier == dxr::D3D12_MESH_SHADER_TIER_1, "Device must report D3D12_MESH_SHADER_TIER_1 support");

    // 6. Prebuild Info Calculation for Bottom-Level Acceleration Structure (BLAS)
    dxr::D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs{};
    blasInputs.Type = dxr::D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    blasInputs.Flags = dxr::D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    blasInputs.NumDescs = 50; // 50 geometric primitives
    dxr::D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuild{};
    pDevice5->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasPrebuild);
    TEST_ASSERT(blasPrebuild.ResultDataMaxSizeInBytes == (50 * 128ULL + 256ULL), "BLAS result size calculation must match 50 primitives");
    TEST_ASSERT(blasPrebuild.ScratchDataSizeInBytes == (50 * 64ULL + 128ULL), "BLAS scratch size calculation must match 50 primitives");

    // 7. Prebuild Info Calculation for Top-Level Acceleration Structure (TLAS)
    dxr::D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs{};
    tlasInputs.Type = dxr::D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    tlasInputs.Flags = dxr::D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    tlasInputs.NumDescs = 8; // 8 instance descriptors
    dxr::D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuild{};
    pDevice5->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuild);
    TEST_ASSERT(tlasPrebuild.ResultDataMaxSizeInBytes == (8 * 256ULL + 512ULL), "TLAS result size calculation must match 8 instances");
    TEST_ASSERT(tlasPrebuild.ScratchDataSizeInBytes == (8 * 128ULL + 256ULL), "TLAS scratch size calculation must match 8 instances");

    // 8. Command Allocator & Command List 4 Creation
    prism3d12::ID3D12CommandAllocator* pAlloc = nullptr;
    hr = pDevice5->CreateCommandAllocator(prism3d12::D3D12_COMMAND_LIST_TYPE_DIRECT, prism3d12::IID_ID3D12CommandAllocator, reinterpret_cast<void**>(&pAlloc));
    TEST_ASSERT(hr == 0 && pAlloc != nullptr, "CreateCommandAllocator must succeed");

    dxr::ID3D12GraphicsCommandList4* pCmdList4 = nullptr;
    hr = pDevice5->CreateCommandList(0, prism3d12::D3D12_COMMAND_LIST_TYPE_DIRECT, pAlloc, nullptr, dxr::IID_ID3D12GraphicsCommandList4_Const, reinterpret_cast<void**>(&pCmdList4));
    TEST_ASSERT(hr == 0 && pCmdList4 != nullptr, "CreateCommandList for ID3D12GraphicsCommandList4 must succeed");

    // 9. Query Interface for ID3D12GraphicsCommandList6 (Mesh Shader Dispatching)
    dxr::ID3D12GraphicsCommandList6* pCmdList6 = nullptr;
    hr = pCmdList4->QueryInterface(dxr::IID_ID3D12GraphicsCommandList6_Const, reinterpret_cast<void**>(&pCmdList6));
    TEST_ASSERT(hr == 0 && pCmdList6 != nullptr, "QueryInterface for ID3D12GraphicsCommandList6 must succeed");

    // 10. Acceleration Structure Construction (BLAS & TLAS)
    dxr::D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc{};
    blasDesc.Inputs = blasInputs;
    blasDesc.DestAccelerationStructureData = 0x50000;
    pCmdList4->BuildRaytracingAccelerationStructure(&blasDesc, 0, nullptr);

    dxr::D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDesc{};
    tlasDesc.Inputs = tlasInputs;
    tlasDesc.DestAccelerationStructureData = 0x60000;
    pCmdList4->BuildRaytracingAccelerationStructure(&tlasDesc, 0, nullptr);

    auto* pCmdImpl = static_cast<dxr::CDXRCommandListImpl*>(pCmdList4);
    TEST_ASSERT(pCmdImpl->getBlasBuilds() == 1, "Command list must record 1 BLAS build");
    TEST_ASSERT(pCmdImpl->getTlasBuilds() == 1, "Command list must record 1 TLAS build");

    // 11. State Object Creation (Raytracing Pipeline State)
    dxr::D3D12_HIT_GROUP_DESC hitGroup{};
    hitGroup.HitGroupExport = L"HitGroupA";
    hitGroup.Type = dxr::D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitGroup.ClosestHitShaderImport = L"ClosestHitA";

    dxr::D3D12_RAYTRACING_PIPELINE_CONFIG pipeCfg{};
    pipeCfg.MaxTraceRecursionDepth = 4;

    dxr::D3D12_STATE_SUBOBJECT subobjects[2]{};
    subobjects[0].Type = dxr::D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    subobjects[0].pDesc = &hitGroup;
    subobjects[1].Type = dxr::D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    subobjects[1].pDesc = &pipeCfg;

    dxr::D3D12_STATE_OBJECT_DESC soDesc{};
    soDesc.Type = dxr::D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    soDesc.NumSubobjects = 2;
    soDesc.pSubobjects = subobjects;

    dxr::ID3D12StateObject* pStateObject = nullptr;
    hr = pDevice5->CreateStateObject(&soDesc, dxr::IID_ID3D12StateObject_Const, reinterpret_cast<void**>(&pStateObject));
    TEST_ASSERT(hr == 0 && pStateObject != nullptr, "CreateStateObject must succeed");

    // 12. State Object Properties & Shader Identifier Retrieval
    dxr::ID3D12StateObjectProperties* pProps = nullptr;
    hr = pStateObject->QueryInterface(dxr::IID_ID3D12StateObjectProperties_Const, reinterpret_cast<void**>(&pProps));
    TEST_ASSERT(hr == 0 && pProps != nullptr, "QueryInterface for ID3D12StateObjectProperties must succeed");

    void* pShaderId = pProps->GetShaderIdentifier(L"HitGroupA");
    TEST_ASSERT(pShaderId != nullptr, "GetShaderIdentifier for HitGroupA must return valid 32-byte identifier");

    void* pRayGenId = pProps->GetShaderIdentifier(L"MyRaygenShader");
    TEST_ASSERT(pRayGenId != nullptr, "GetShaderIdentifier for default RayGen must succeed");

    // 13. Pipeline Stack Size Configuration
    pProps->SetPipelineStackSize(16384);
    TEST_ASSERT(pProps->GetPipelineStackSize() == 16384, "Pipeline stack size must be set to 16,384 bytes");

    // 14. SetPipelineState1 Binding
    pCmdList4->SetPipelineState1(pStateObject);

    // 15. DispatchRays Execution with Möller-Trumbore Ray-Triangle Intersections
    dxr::D3D12_DISPATCH_RAYS_DESC dispatchDesc{};
    dispatchDesc.Width = 32;
    dispatchDesc.Height = 32;
    dispatchDesc.Depth = 1;
    pCmdList4->DispatchRays(&dispatchDesc);
    TEST_ASSERT(pCmdImpl->getRaysDispatched() == 1024, "DispatchRays must simulate exactly 1024 rays (32x32)");
    TEST_ASSERT(pCmdImpl->getRaysHit() > 0, "DispatchRays must detect triangle intersections against scene");

    // 16. DispatchMesh Next-Gen Geometry Amplification & Shell CLI Verification
    pCmdList6->DispatchMesh(8, 2, 1);
    TEST_ASSERT(pCmdImpl->getMeshDispatches() == 1, "Command list must record 1 mesh dispatch");
    TEST_ASSERT(pCmdImpl->getMeshAmplifiedPrimitives() == 1024, "Mesh shader must amplify 16 threadgroups into 1024 primitives");

    std::ostringstream testOut;
    micant::shell::CommandShell shellEngine;
    int rc = shellEngine.execute("dxr test", testOut);
    TEST_ASSERT(rc == 0, "dxr test CLI command must return 0");
    TEST_ASSERT(testOut.str().find("16/16 PASSED") != std::string::npos, "dxr test must pass all 16 tests");

    std::ostringstream infoOut;
    rc = shellEngine.execute("dxr info", infoOut);
    TEST_ASSERT(rc == 0, "dxr info CLI command must return 0");
    TEST_ASSERT(infoOut.str().find("D3D12_RAYTRACING_TIER_1_1") != std::string::npos, "dxr info must display DXR Tier 1.1 telemetry");

    // Cleanup
    pProps->Release();
    pStateObject->Release();
    pCmdList6->Release();
    pCmdList4->Release();
    pAlloc->Release();
    pDevice5->Release();

    std::cout << "[TEST] Suite 114: Windows DirectX 12 Raytracing & Mesh Shader Subsystem PASSED.\n";
}

// ============================================================================
// Suite 115: Windows DirectStorage API & GPU Decompression Subsystem
// ============================================================================
void Test_WindowsDirectStorage_Subsystem() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 115: Windows DirectStorage & High-Performance GPU I/O Subsystem \n";
    std::cout << "========================================================================\n";

    directstorage::InitializeDirectStorageExports();

    // 1. Dynamic Module Export Verification
    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("dstorage.dll", "DStorageGetFactory") != nullptr, "DStorageGetFactory must be exported from dstorage.dll");
    TEST_ASSERT(ldr.getExport("dstoragecore.dll", "DStorageGetFactory") != nullptr, "DStorageGetFactory must be exported from dstoragecore.dll");

    // 2. Version Database Verification
    const auto* mod = version::VersionDatabase::Instance().GetModuleInfo("dstorage.dll");
    TEST_ASSERT(mod != nullptr, "VersionDatabase must contain dstorage.dll");
    TEST_ASSERT(mod->stringTable.at("FileVersion") == "1.2.2.0", "dstorage.dll FileVersion must be 1.2.2.0");

    const auto* modCore = version::VersionDatabase::Instance().GetModuleInfo("dstoragecore.dll");
    TEST_ASSERT(modCore != nullptr, "VersionDatabase must contain dstoragecore.dll");

    // 3. Factory Acquisition
    directstorage::IDStorageFactory* pFactory = nullptr;
    int32_t hr = directstorage::DStorageGetFactory(directstorage::IID_IDStorageFactory_Const, reinterpret_cast<void**>(&pFactory));
    TEST_ASSERT(hr == 0 && pFactory != nullptr, "DStorageGetFactory must succeed");

    // 4. Staging Buffer & Debug Configuration
    pFactory->SetStagingBufferSize(64 * 1024 * 1024);
    pFactory->SetDebugFlags(directstorage::DSTORAGE_DEBUG_SHOW_ERRORS);
    auto* pFactImpl = static_cast<directstorage::CStorageFactoryImpl*>(pFactory);
    TEST_ASSERT(pFactImpl->GetStagingBufferSize() == 64 * 1024 * 1024, "Staging buffer must be 64 MB");
    TEST_ASSERT(pFactImpl->GetDebugFlags() == directstorage::DSTORAGE_DEBUG_SHOW_ERRORS, "Debug flags must match");

    // 5. Status Array Creation
    directstorage::IDStorageStatusArray* pStatusArray = nullptr;
    hr = pFactory->CreateStatusArray(16, "TestSuiteStatusArray", directstorage::IID_IDStorageStatusArray_Const, reinterpret_cast<void**>(&pStatusArray));
    TEST_ASSERT(hr == 0 && pStatusArray != nullptr, "CreateStatusArray must succeed");
    TEST_ASSERT(!pStatusArray->IsComplete(0), "Status token 0 must initially be incomplete");
    TEST_ASSERT(!pStatusArray->IsComplete(1), "Status token 1 must initially be incomplete");

    // 6. Direct3D 12 Device, Resource & Fence Creation
    prism3d12::ID3D12Device* pDev = nullptr;
    hr = prism3d12::D3D12CreateDevice(nullptr, prism3d::D3D_FEATURE_LEVEL_12_2, prism3d12::IID_ID3D12Device, reinterpret_cast<void**>(&pDev));
    TEST_ASSERT(hr == 0 && pDev != nullptr, "D3D12CreateDevice must succeed");

    prism3d12::D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = prism3d12::D3D12_HEAP_TYPE_DEFAULT;

    prism3d12::D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = prism3d12::D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = 65536; // 64 KB
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;

    prism3d12::ID3D12Resource* pRes = nullptr;
    hr = pDev->CreateCommittedResource(&heapProps, prism3d12::D3D12_HEAP_FLAG_NONE, &resDesc, prism3d12::D3D12_RESOURCE_STATE_COMMON, nullptr, prism3d12::IID_ID3D12Resource, reinterpret_cast<void**>(&pRes));
    TEST_ASSERT(hr == 0 && pRes != nullptr, "CreateCommittedResource must succeed");

    prism3d12::ID3D12Fence* pFence = nullptr;
    hr = pDev->CreateFence(0, prism3d12::D3D12_FENCE_FLAG_NONE, prism3d12::IID_ID3D12Fence, reinterpret_cast<void**>(&pFence));
    TEST_ASSERT(hr == 0 && pFence != nullptr, "CreateFence must succeed");

    // 7. Memory-Source DirectStorage Queue Creation
    directstorage::DSTORAGE_QUEUE_DESC qDesc{};
    qDesc.SourceType = directstorage::DSTORAGE_REQUEST_SOURCE_MEMORY;
    qDesc.Capacity = 128;
    qDesc.Priority = directstorage::DSTORAGE_PRIORITY_NORMAL;
    qDesc.Name = "MicaNT_MemQueue";
    qDesc.Device = pDev;

    directstorage::IDStorageQueue* pQueue = nullptr;
    hr = pFactory->CreateQueue(&qDesc, directstorage::IID_IDStorageQueue_Const, reinterpret_cast<void**>(&pQueue));
    TEST_ASSERT(hr == 0 && pQueue != nullptr, "CreateQueue must succeed");

    // 8. Direct Memory-to-GPU Buffer Request (Uncompressed)
    std::vector<uint8_t> uncompressedData(4096);
    for (size_t i = 0; i < uncompressedData.size(); ++i) {
        uncompressedData[i] = static_cast<uint8_t>((i * 7 + 13) & 0xFF);
    }

    directstorage::DSTORAGE_REQUEST req1{};
    req1.Options.SourceType = directstorage::DSTORAGE_REQUEST_SOURCE_MEMORY;
    req1.Options.DestinationType = directstorage::DSTORAGE_REQUEST_DESTINATION_BUFFER;
    req1.Options.Compression = directstorage::DSTORAGE_COMPRESSION_FORMAT_NONE;
    req1.Source.Memory.Source = uncompressedData.data();
    req1.Source.Memory.Size = static_cast<uint32_t>(uncompressedData.size());
    req1.Destination.Buffer.Resource = pRes;
    req1.Destination.Buffer.Offset = 0;
    req1.Destination.Buffer.Size = static_cast<uint32_t>(uncompressedData.size());
    req1.UncompressedSize = static_cast<uint32_t>(uncompressedData.size());

    pQueue->EnqueueRequest(&req1);
    pQueue->EnqueueStatus(pStatusArray, 0);
    pQueue->EnqueueSignal(pFence, 100);
    pQueue->Submit();

    TEST_ASSERT(pStatusArray->IsComplete(0), "StatusArray index 0 must be complete");
    TEST_ASSERT(pStatusArray->GetHResult(0) == 0, "StatusArray index 0 HRESULT must be S_OK (0)");
    TEST_ASSERT(pFence->GetCompletedValue() == 100, "Fence value must reach 100");

    void* mapped = nullptr;
    pRes->Map(0, nullptr, &mapped);
    TEST_ASSERT(mapped != nullptr, "Resource Map must succeed");
    TEST_ASSERT(std::memcmp(mapped, uncompressedData.data(), uncompressedData.size()) == 0, "Resource data must match uncompressed data exactly");
    pRes->Unmap(0, nullptr);

    // 9. GDeflate Compression & Direct-to-GPU Decompression
    std::vector<uint8_t> originalTexture(8192);
    for (size_t i = 0; i < originalTexture.size(); ++i) {
        originalTexture[i] = static_cast<uint8_t>((i / 16) & 0xFF);
    }

    auto gdefCompressed = directstorage::codec::CompressGDeflate(originalTexture.data(), static_cast<uint32_t>(originalTexture.size()));
    TEST_ASSERT(!gdefCompressed.empty() && gdefCompressed.size() < originalTexture.size(), "GDeflate compression must reduce asset footprint");

    directstorage::DSTORAGE_REQUEST reqGDef{};
    reqGDef.Options.SourceType = directstorage::DSTORAGE_REQUEST_SOURCE_MEMORY;
    reqGDef.Options.DestinationType = directstorage::DSTORAGE_REQUEST_DESTINATION_BUFFER;
    reqGDef.Options.Compression = directstorage::DSTORAGE_COMPRESSION_FORMAT_GDEFLATE;
    reqGDef.Source.Memory.Source = gdefCompressed.data();
    reqGDef.Source.Memory.Size = static_cast<uint32_t>(gdefCompressed.size());
    reqGDef.Destination.Buffer.Resource = pRes;
    reqGDef.Destination.Buffer.Offset = 4096;
    reqGDef.Destination.Buffer.Size = static_cast<uint32_t>(originalTexture.size());
    reqGDef.UncompressedSize = static_cast<uint32_t>(originalTexture.size());

    pQueue->EnqueueRequest(&reqGDef);
    pQueue->EnqueueStatus(pStatusArray, 1);
    pQueue->EnqueueSignal(pFence, 200);
    pQueue->Submit();

    TEST_ASSERT(pStatusArray->IsComplete(1), "StatusArray index 1 must be complete");
    TEST_ASSERT(pFence->GetCompletedValue() == 200, "Fence value must reach 200");

    pRes->Map(0, nullptr, &mapped);
    TEST_ASSERT(std::memcmp(static_cast<uint8_t*>(mapped) + 4096, originalTexture.data(), originalTexture.size()) == 0, "Decompressed GDeflate data in VRAM must match source exactly");
    pRes->Unmap(0, nullptr);

    // 10. Zlib Compression & Decompression Pipeline
    auto zlibCompressed = directstorage::codec::CompressZlib(originalTexture.data(), static_cast<uint32_t>(originalTexture.size()));
    TEST_ASSERT(!zlibCompressed.empty(), "Zlib compression must succeed");

    std::vector<uint8_t> zlibDecompressed(originalTexture.size(), 0);
    directstorage::DSTORAGE_REQUEST reqZlib{};
    reqZlib.Options.SourceType = directstorage::DSTORAGE_REQUEST_SOURCE_MEMORY;
    reqZlib.Options.DestinationType = directstorage::DSTORAGE_REQUEST_DESTINATION_MEMORY;
    reqZlib.Options.Compression = directstorage::DSTORAGE_COMPRESSION_FORMAT_ZLIB;
    reqZlib.Source.Memory.Source = zlibCompressed.data();
    reqZlib.Source.Memory.Size = static_cast<uint32_t>(zlibCompressed.size());
    reqZlib.Destination.Memory.Buffer = zlibDecompressed.data();
    reqZlib.Destination.Memory.Size = static_cast<uint32_t>(zlibDecompressed.size());
    reqZlib.UncompressedSize = static_cast<uint32_t>(originalTexture.size());

    pQueue->EnqueueRequest(&reqZlib);
    pQueue->EnqueueStatus(pStatusArray, 2);
    pQueue->EnqueueSignal(pFence, 300);
    pQueue->Submit();

    TEST_ASSERT(pStatusArray->IsComplete(2), "StatusArray index 2 must be complete");
    TEST_ASSERT(pFence->GetCompletedValue() == 300, "Fence value must reach 300");
    TEST_ASSERT(std::memcmp(zlibDecompressed.data(), originalTexture.data(), originalTexture.size()) == 0, "Decompressed Zlib data must match source exactly");

    // 11. Virtual File System & File Queue
    std::vector<uint8_t> fileAsset(16384, 0x42);
    pFactImpl->RegisterVirtualFile(L"C:\\game\\mesh_geometry.bin", fileAsset);

    directstorage::IDStorageFile* pStorageFile = nullptr;
    hr = pFactory->OpenFile(L"C:\\game\\mesh_geometry.bin", directstorage::IID_IDStorageFile_Const, reinterpret_cast<void**>(&pStorageFile));
    TEST_ASSERT(hr == 0 && pStorageFile != nullptr, "OpenFile must succeed");
    TEST_ASSERT(pStorageFile->GetFileSize() == 16384, "Virtual file size must match 16 KB");

    directstorage::DSTORAGE_QUEUE_DESC fileQDesc{};
    fileQDesc.SourceType = directstorage::DSTORAGE_REQUEST_SOURCE_FILE;
    fileQDesc.Capacity = 64;
    fileQDesc.Priority = directstorage::DSTORAGE_PRIORITY_HIGH;
    fileQDesc.Name = "MicaNT_FileQueue";
    fileQDesc.Device = pDev;

    directstorage::IDStorageQueue* pFileQueue = nullptr;
    hr = pFactory->CreateQueue(&fileQDesc, directstorage::IID_IDStorageQueue_Const, reinterpret_cast<void**>(&pFileQueue));
    TEST_ASSERT(hr == 0 && pFileQueue != nullptr, "CreateQueue for file queue must succeed");

    std::vector<uint8_t> readFromDisk(16384, 0);
    directstorage::DSTORAGE_REQUEST reqFile{};
    reqFile.Options.SourceType = directstorage::DSTORAGE_REQUEST_SOURCE_FILE;
    reqFile.Options.DestinationType = directstorage::DSTORAGE_REQUEST_DESTINATION_MEMORY;
    reqFile.Options.Compression = directstorage::DSTORAGE_COMPRESSION_FORMAT_NONE;
    reqFile.Source.File.Source = pStorageFile;
    reqFile.Source.File.Offset = 0;
    reqFile.Source.File.Size = 16384;
    reqFile.Destination.Memory.Buffer = readFromDisk.data();
    reqFile.Destination.Memory.Size = 16384;
    reqFile.UncompressedSize = 16384;

    pFileQueue->EnqueueRequest(&reqFile);
    pFileQueue->EnqueueStatus(pStatusArray, 3);
    pFileQueue->EnqueueSignal(pFence, 400);
    pFileQueue->Submit();

    TEST_ASSERT(pStatusArray->IsComplete(3), "StatusArray index 3 must be complete");
    TEST_ASSERT(pFence->GetCompletedValue() == 400, "Fence value must reach 400");
    TEST_ASSERT(readFromDisk == fileAsset, "File contents read via DirectStorage must match source exactly");

    // 12. Tag-Based Request Cancellation Filtering
    directstorage::DSTORAGE_REQUEST reqCancel{};
    reqCancel.CancellationTag = 0xCAFE;
    reqCancel.Options.SourceType = directstorage::DSTORAGE_REQUEST_SOURCE_MEMORY;
    reqCancel.Options.DestinationType = directstorage::DSTORAGE_REQUEST_DESTINATION_MEMORY;
    reqCancel.Options.Compression = directstorage::DSTORAGE_COMPRESSION_FORMAT_NONE;
    reqCancel.Source.Memory.Source = uncompressedData.data();
    reqCancel.Source.Memory.Size = 100;
    reqCancel.Destination.Memory.Buffer = readFromDisk.data();
    reqCancel.Destination.Memory.Size = 100;

    pFileQueue->EnqueueRequest(&reqCancel);
    pFileQueue->CancelRequestsWithTag(0xFFFF, 0xCAFE);
    pFileQueue->Submit();

    // 13. Custom Decompression Queue
    directstorage::IDStorageCustomDecompressionQueue* pCustomQueue = nullptr;
    hr = pFactory->QueryInterface(directstorage::IID_IDStorageCustomDecompressionQueue_Const, reinterpret_cast<void**>(&pCustomQueue));
    TEST_ASSERT(hr == 0 && pCustomQueue != nullptr, "QueryInterface for IDStorageCustomDecompressionQueue must succeed");
    TEST_ASSERT(pCustomQueue->GetEvent() != nullptr, "GetEvent on custom decompression queue must return valid handle");

    // 14. Interactive Shell CLI Verification
    std::ostringstream testOut;
    micant::shell::CommandShell shellEngine;
    int rc = shellEngine.execute("dstorage test", testOut);
    TEST_ASSERT(rc == 0, "dstorage test CLI command must return 0");
    TEST_ASSERT(testOut.str().find("16/16 PASSED") != std::string::npos, "dstorage test must pass all 16 tests");

    std::ostringstream infoOut;
    rc = shellEngine.execute("dstorage info", infoOut);
    TEST_ASSERT(rc == 0, "dstorage info CLI command must return 0");
    TEST_ASSERT(infoOut.str().find("NVMe Kernel Queue Bypass") != std::string::npos, "dstorage info must display telemetry");

    std::ostringstream benchOut;
    rc = shellEngine.execute("dstorage bench 16", benchOut);
    TEST_ASSERT(rc == 0, "dstorage bench CLI command must return 0");
    TEST_ASSERT(benchOut.str().find("GB/s") != std::string::npos, "dstorage bench must display bandwidth in GB/s");

    // Cleanup
    pCustomQueue->Release();
    pFileQueue->Release();
    pStorageFile->Release();
    pQueue->Release();
    pFence->Release();
    pRes->Release();
    pDev->Release();
    pStatusArray->Release();
    pFactory->Release();

    std::cout << "[TEST] Suite 115: Windows DirectStorage & High-Performance GPU I/O Subsystem PASSED.\n";
}

// ============================================================================
// Suite 116: Windows DirectML & DXCore Subsystem
// ============================================================================
void Test_WindowsDirectML_Subsystem() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 116: Windows DirectML Machine Learning & DXCore Subsystem        \n";
    std::cout << "========================================================================\n";

    dxcore::InitializeDXCoreExports();
    directml::InitializeDirectMLExports();

    // 1. Dynamic Module Export Verification
    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("dxcore.dll", "DXCoreCreateAdapterFactory") != nullptr, "DXCoreCreateAdapterFactory must be exported from dxcore.dll");
    TEST_ASSERT(ldr.getExport("directml.dll", "DMLCreateDevice") != nullptr, "DMLCreateDevice must be exported from directml.dll");
    TEST_ASSERT(ldr.getExport("directml.dll", "DMLCreateDevice1") != nullptr, "DMLCreateDevice1 must be exported from directml.dll");

    // 2. Version Database Verification
    const auto* modDXCore = version::VersionDatabase::Instance().GetModuleInfo("dxcore.dll");
    TEST_ASSERT(modDXCore != nullptr, "VersionDatabase must contain dxcore.dll");
    TEST_ASSERT(modDXCore->stringTable.at("FileVersion") == "10.0.22621.1", "dxcore.dll FileVersion must be 10.0.22621.1");

    const auto* modDML = version::VersionDatabase::Instance().GetModuleInfo("directml.dll");
    TEST_ASSERT(modDML != nullptr, "VersionDatabase must contain directml.dll");
    TEST_ASSERT(modDML->stringTable.at("FileVersion") == "1.15.2.0", "directml.dll FileVersion must be 1.15.2.0");

    // 3. DXCore Adapter Factory & List Enumeration
    dxcore::IDXCoreAdapterFactory* pFactory = nullptr;
    int32_t hr = dxcore::DXCoreCreateAdapterFactory(dxcore::IID_IDXCoreAdapterFactory_Const, reinterpret_cast<void**>(&pFactory));
    TEST_ASSERT(hr == 0 && pFactory != nullptr, "DXCoreCreateAdapterFactory must succeed");

    dxcore::IDXCoreAdapterList* pList = nullptr;
    hr = pFactory->CreateAdapterList(0, nullptr, dxcore::IID_IDXCoreAdapterList_Const, reinterpret_cast<void**>(&pList));
    TEST_ASSERT(hr == 0 && pList != nullptr, "CreateAdapterList must succeed");
    TEST_ASSERT(pList->GetAdapterCount() >= 2, "Adapter count must be at least 2");

    // 4. Primary Adapter Telemetry
    dxcore::IDXCoreAdapter* pAdapter = nullptr;
    hr = pList->GetAdapter(0, dxcore::IID_IDXCoreAdapter_Const, reinterpret_cast<void**>(&pAdapter));
    TEST_ASSERT(hr == 0 && pAdapter != nullptr, "GetAdapter(0) must succeed");
    TEST_ASSERT(pAdapter->IsValid(), "Primary adapter must be valid");
    TEST_ASSERT(pAdapter->IsAttributeSupported(dxcore::DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS_CONST), "D3D12 graphics must be supported");
    TEST_ASSERT(pAdapter->IsAttributeSupported(dxcore::DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE_CONST), "D3D12 core compute must be supported");

    uint64_t vram = 0;
    pAdapter->GetProperty(dxcore::DXCoreAdapterProperty::DedicatedAdapterMemory, sizeof(vram), &vram);
    TEST_ASSERT(vram == 16ULL * 1024 * 1024 * 1024, "Primary dedicated adapter memory must be 16 GB");

    char desc[128]{};
    pAdapter->GetProperty(dxcore::DXCoreAdapterProperty::DriverDescription, sizeof(desc), desc);
    TEST_ASSERT(std::string(desc).find("PrismX") != std::string::npos, "Driver description must contain PrismX");

    dxcore::DXCoreHardwareID hwId{};
    pAdapter->GetProperty(dxcore::DXCoreAdapterProperty::HardwareID, sizeof(hwId), &hwId);
    TEST_ASSERT(hwId.vendorID == 0x13B5, "Vendor ID must match PrismX Sovereign 0x13B5");

    dxcore::DXCoreAdapterMemoryBudget budget{};
    hr = pAdapter->QueryState(dxcore::DXCoreAdapterState::AdapterMemoryBudget, 0, nullptr, sizeof(budget), &budget);
    TEST_ASSERT(hr == 0 && budget.budget == vram, "AdapterMemoryBudget query must match dedicated VRAM");

    // 5. Adapter Sorting
    dxcore::DXCoreAdapterPreference prefs[1] = { dxcore::DXCoreAdapterPreference::MinimumPower };
    hr = pList->Sort(1, prefs);
    TEST_ASSERT(hr == 0, "Sort by MinimumPower must succeed");

    dxcore::IDXCoreAdapter* pMinPower = nullptr;
    pList->GetAdapter(0, dxcore::IID_IDXCoreAdapter_Const, reinterpret_cast<void**>(&pMinPower));
    bool isIntegrated = false;
    pMinPower->GetProperty(dxcore::DXCoreAdapterProperty::IsIntegrated, sizeof(isIntegrated), &isIntegrated);
    TEST_ASSERT(isIntegrated == true, "MinimumPower sorted first adapter must be integrated");
    pMinPower->Release();

    // 6. Direct3D 12 Device & DirectML Device Creation
    prism3d12::ID3D12Device* pD3D12Dev = nullptr;
    hr = prism3d12::D3D12CreateDevice(nullptr, prism3d::D3D_FEATURE_LEVEL_12_2, prism3d12::IID_ID3D12Device, reinterpret_cast<void**>(&pD3D12Dev));
    TEST_ASSERT(hr == 0 && pD3D12Dev != nullptr, "D3D12CreateDevice must succeed");

    directml::IDMLDevice* pDmlDev = nullptr;
    hr = directml::DMLCreateDevice(pD3D12Dev, directml::DML_CREATE_DEVICE_FLAGS::NONE, directml::IID_IDMLDevice_Const, reinterpret_cast<void**>(&pDmlDev));
    TEST_ASSERT(hr == 0 && pDmlDev != nullptr, "DMLCreateDevice must succeed");

    directml::DML_FEATURE_DATA_FEATURE_LEVELS featLevels{};
    hr = pDmlDev->CheckFeatureSupport(directml::DML_FEATURE::FEATURE_LEVELS, 0, nullptr, sizeof(featLevels), &featLevels);
    TEST_ASSERT(hr == 0 && featLevels.MaxSupportedFeatureLevel == directml::DML_FEATURE_LEVEL::LEVEL_6_4, "DirectML max feature level must be LEVEL_6_4");

    auto CreateCommittedBuffer = [&](size_t bytes) -> prism3d12::ID3D12Resource* {
        prism3d12::D3D12_HEAP_PROPERTIES hp{};
        hp.Type = prism3d12::D3D12_HEAP_TYPE_DEFAULT;
        prism3d12::D3D12_RESOURCE_DESC rd{};
        rd.Dimension = prism3d12::D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = bytes;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        prism3d12::ID3D12Resource* res = nullptr;
        pD3D12Dev->CreateCommittedResource(&hp, prism3d12::D3D12_HEAP_FLAG_NONE, &rd, prism3d12::D3D12_RESOURCE_STATE_COMMON, nullptr, prism3d12::IID_ID3D12Resource, reinterpret_cast<void**>(&res));
        return res;
    };

    // 7. GEMM Tensor Kernel: Y = A * B + C
    // A: [2, 3] = [[1, 2, 3], [4, 5, 6]]
    // B: [3, 2] = [[7, 8], [9, 1], [2, 3]]
    // C: [2, 2] = [[1, 1], [1, 1]]
    // Expected Y = [[32, 20], [86, 56]]
    auto* bufA = CreateCommittedBuffer(6 * sizeof(float));
    auto* bufB = CreateCommittedBuffer(6 * sizeof(float));
    auto* bufC = CreateCommittedBuffer(4 * sizeof(float));
    auto* bufY = CreateCommittedBuffer(4 * sizeof(float));

    void* pMap = nullptr;
    bufA->Map(0, nullptr, &pMap);
    float aVals[6] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
    std::memcpy(pMap, aVals, sizeof(aVals));
    bufA->Unmap(0, nullptr);

    bufB->Map(0, nullptr, &pMap);
    float bVals[6] = { 7.0f, 8.0f, 9.0f, 1.0f, 2.0f, 3.0f };
    std::memcpy(pMap, bVals, sizeof(bVals));
    bufB->Unmap(0, nullptr);

    bufC->Map(0, nullptr, &pMap);
    float cVals[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    std::memcpy(pMap, cVals, sizeof(cVals));
    bufC->Unmap(0, nullptr);

    uint32_t aSizes[2] = { 2, 3 };
    directml::DML_BUFFER_TENSOR_DESC aBufDesc{ directml::DML_TENSOR_DATA_TYPE::FLOAT32, directml::DML_TENSOR_FLAGS::NONE, 2, aSizes, nullptr, sizeof(aVals), 0 };
    directml::DML_TENSOR_DESC aDesc{ directml::DML_TENSOR_TYPE::BUFFER, &aBufDesc };

    uint32_t bSizes[2] = { 3, 2 };
    directml::DML_BUFFER_TENSOR_DESC bBufDesc{ directml::DML_TENSOR_DATA_TYPE::FLOAT32, directml::DML_TENSOR_FLAGS::NONE, 2, bSizes, nullptr, sizeof(bVals), 0 };
    directml::DML_TENSOR_DESC bDesc{ directml::DML_TENSOR_TYPE::BUFFER, &bBufDesc };

    uint32_t cSizes[2] = { 2, 2 };
    directml::DML_BUFFER_TENSOR_DESC cBufDesc{ directml::DML_TENSOR_DATA_TYPE::FLOAT32, directml::DML_TENSOR_FLAGS::NONE, 2, cSizes, nullptr, sizeof(cVals), 0 };
    directml::DML_TENSOR_DESC cDesc{ directml::DML_TENSOR_TYPE::BUFFER, &cBufDesc };

    uint32_t ySizes[2] = { 2, 2 };
    directml::DML_BUFFER_TENSOR_DESC yBufDesc{ directml::DML_TENSOR_DATA_TYPE::FLOAT32, directml::DML_TENSOR_FLAGS::NONE, 2, ySizes, nullptr, 4 * sizeof(float), 0 };
    directml::DML_TENSOR_DESC yDesc{ directml::DML_TENSOR_TYPE::BUFFER, &yBufDesc };

    directml::DML_GEMM_OPERATOR_DESC gemmDesc{};
    gemmDesc.ATensor = &aDesc;
    gemmDesc.BTensor = &bDesc;
    gemmDesc.CTensor = &cDesc;
    gemmDesc.OutputTensor = &yDesc;
    gemmDesc.Alpha = 1.0f;
    gemmDesc.Beta = 1.0f;

    directml::DML_OPERATOR_DESC opDesc{ directml::DML_OPERATOR_TYPE::GEMM, &gemmDesc };
    directml::IDMLOperator* pGemmOp = nullptr;
    hr = pDmlDev->CreateOperator(&opDesc, directml::IID_IDMLOperator_Const, reinterpret_cast<void**>(&pGemmOp));
    TEST_ASSERT(hr == 0 && pGemmOp != nullptr, "CreateOperator (GEMM) must succeed");

    directml::IDMLCompiledOperator* pCompiledGemm = nullptr;
    hr = pDmlDev->CompileOperator(pGemmOp, directml::DML_EXECUTION_FLAGS::NONE, directml::IID_IDMLCompiledOperator_Const, reinterpret_cast<void**>(&pCompiledGemm));
    TEST_ASSERT(hr == 0 && pCompiledGemm != nullptr, "CompileOperator (GEMM) must succeed");

    directml::DML_BINDING_TABLE_DESC btableDesc{};
    btableDesc.Dispatchable = pCompiledGemm;
    btableDesc.SizeInDescriptors = 1;

    directml::IDMLBindingTable* pBindingTable = nullptr;
    hr = pDmlDev->CreateBindingTable(&btableDesc, directml::IID_IDMLBindingTable_Const, reinterpret_cast<void**>(&pBindingTable));
    TEST_ASSERT(hr == 0 && pBindingTable != nullptr, "CreateBindingTable must succeed");

    directml::DML_BUFFER_BINDING inBindings[3] = {
        { bufA, 0, 6 * sizeof(float) },
        { bufB, 0, 6 * sizeof(float) },
        { bufC, 0, 4 * sizeof(float) }
    };
    directml::DML_BINDING_DESC inBDesc[3] = {
        { directml::DML_BINDING_TYPE::BUFFER, &inBindings[0] },
        { directml::DML_BINDING_TYPE::BUFFER, &inBindings[1] },
        { directml::DML_BINDING_TYPE::BUFFER, &inBindings[2] }
    };
    pBindingTable->BindInputs(3, inBDesc);

    directml::DML_BUFFER_BINDING outBinding = { bufY, 0, 4 * sizeof(float) };
    directml::DML_BINDING_DESC outBDesc = { directml::DML_BINDING_TYPE::BUFFER, &outBinding };
    pBindingTable->BindOutputs(1, &outBDesc);

    directml::IDMLCommandRecorder* pRecorder = nullptr;
    hr = pDmlDev->CreateCommandRecorder(directml::IID_IDMLCommandRecorder_Const, reinterpret_cast<void**>(&pRecorder));
    TEST_ASSERT(hr == 0 && pRecorder != nullptr, "CreateCommandRecorder must succeed");

    pRecorder->RecordDispatch(nullptr, pCompiledGemm, pBindingTable);

    bufY->Map(0, nullptr, &pMap);
    float* res = static_cast<float*>(pMap);
    TEST_ASSERT(std::abs(res[0] - 32.0f) < 1e-4f, "GEMM element [0,0] must equal 32");
    TEST_ASSERT(std::abs(res[1] - 20.0f) < 1e-4f, "GEMM element [0,1] must equal 20");
    TEST_ASSERT(std::abs(res[2] - 86.0f) < 1e-4f, "GEMM element [1,0] must equal 86");
    TEST_ASSERT(std::abs(res[3] - 56.0f) < 1e-4f, "GEMM element [1,1] must equal 56");
    bufY->Unmap(0, nullptr);

    // 8. Interactive CLI Tool Verification
    micant::shell::CommandShell shellEngine;
    std::ostringstream testOut;
    int rc = shellEngine.execute("dml test", testOut);
    TEST_ASSERT(rc == 0, "dml test CLI command must return 0");
    TEST_ASSERT(testOut.str().find("10 / 10 Subsystem Invariants Verified") != std::string::npos, "dml test must verify all 10 invariants");

    std::ostringstream infoOut;
    rc = shellEngine.execute("dml info", infoOut);
    TEST_ASSERT(rc == 0, "dml info CLI command must return 0");
    TEST_ASSERT(infoOut.str().find("DirectML 1.15 Sovereign Execution Engine") != std::string::npos, "dml info must display DirectML telemetry");

    std::ostringstream inferOut;
    rc = shellEngine.execute("dml infer", inferOut);
    TEST_ASSERT(rc == 0, "dml infer CLI command must return 0");
    TEST_ASSERT(inferOut.str().find("GFLOPS") != std::string::npos, "dml infer must display inference performance in GFLOPS");

    // Cleanup
    pRecorder->Release();
    pBindingTable->Release();
    pCompiledGemm->Release();
    pGemmOp->Release();
    bufA->Release(); bufB->Release(); bufC->Release(); bufY->Release();
    pDmlDev->Release();
    pD3D12Dev->Release();
    pAdapter->Release();
    pList->Release();
    pFactory->Release();

    std::cout << "[TEST] Suite 116: Windows DirectML & DXCore Subsystem PASSED.\n";
}

// ============================================================================
// Suite 117: Windows DirectComposition Subsystem
// ============================================================================
void Test_WindowsDirectComposition_Subsystem() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 117: Windows DirectComposition & Modern Compositor Subsystem     \n";
    std::cout << "========================================================================\n";

    dcomp::InitializeDirectCompositionExports();

    // 1. Dynamic Module Export Verification
    auto& loader = ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("dcomp.dll", "DCompositionCreateDevice") != nullptr, "DCompositionCreateDevice must be exported from dcomp.dll");
    TEST_ASSERT(loader.getExport("dcomp.dll", "DCompositionCreateDevice2") != nullptr, "DCompositionCreateDevice2 must be exported from dcomp.dll");
    TEST_ASSERT(loader.getExport("dcomp.dll", "DCompositionCreateDevice3") != nullptr, "DCompositionCreateDevice3 must be exported from dcomp.dll");
    TEST_ASSERT(loader.getExport("dcomp.dll", "DCompositionCreateSurfaceHandle") != nullptr, "DCompositionCreateSurfaceHandle must be exported from dcomp.dll");

    // 2. Version Database Verification
    const auto* modDComp = version::VersionDatabase::Instance().GetModuleInfo("dcomp.dll");
    TEST_ASSERT(modDComp != nullptr, "VersionDatabase must contain dcomp.dll");
    TEST_ASSERT(modDComp->stringTable.at("FileVersion") == "10.0.22621.1", "dcomp.dll FileVersion must be 10.0.22621.1");

    // 3. DirectComposition Device Creation
    dcomp::IDCompositionDevice* pDcompDevice = nullptr;
    int32_t hr = dcomp::DCompositionCreateDevice(nullptr, dcomp::IID_IDCompositionDevice_Const, reinterpret_cast<void**>(&pDcompDevice));
    TEST_ASSERT(hr == 0 && pDcompDevice != nullptr, "DCompositionCreateDevice must succeed");

    // 4. Visual Tree Hierarchy Construction (Root -> WindowFrame -> ContentCard -> ActionButton)
    dcomp::IDCompositionVisual* pRoot = nullptr;
    dcomp::IDCompositionVisual* pWindow = nullptr;
    dcomp::IDCompositionVisual* pCard = nullptr;
    dcomp::IDCompositionVisual* pButton = nullptr;

    pDcompDevice->CreateVisual(&pRoot);
    pDcompDevice->CreateVisual(&pWindow);
    pDcompDevice->CreateVisual(&pCard);
    pDcompDevice->CreateVisual(&pButton);

    TEST_ASSERT(pRoot && pWindow && pCard && pButton, "CreateVisual for all tree nodes must succeed");

    // Configure properties
    pWindow->SetOffsetX(50.0f);
    pWindow->SetOffsetY(50.0f);
    pWindow->SetOpacity(0.95f);
    pWindow->SetInterpolationMode(dcomp::DCOMPOSITION_BITMAP_INTERPOLATION_MODE::LINEAR);
    pWindow->SetBorderMode(dcomp::DCOMPOSITION_BORDER_MODE::SOFT);

    TEST_ASSERT(pWindow->GetOffsetX() == 50.0f, "Window visual offset X must be 50.0");
    TEST_ASSERT(pWindow->GetOffsetY() == 50.0f, "Window visual offset Y must be 50.0");
    TEST_ASSERT(std::abs(pWindow->GetOpacity() - 0.95f) < 1e-4f, "Window visual opacity must be 0.95");

    pRoot->AddVisual(pWindow, true, nullptr);
    pWindow->AddVisual(pCard, true, nullptr);
    pWindow->AddVisual(pButton, true, pCard);

    TEST_ASSERT(pRoot->GetChildren().size() == 1, "Root must have 1 child");
    TEST_ASSERT(pWindow->GetChildren().size() == 2, "Window must have 2 children");
    TEST_ASSERT(pWindow->GetChildren()[0] == pCard, "First child must be Card");
    TEST_ASSERT(pWindow->GetChildren()[1] == pButton, "Second child must be Button");

    // 5. Affine Transforms (Translate, Scale, Rotate, Matrix)
    dcomp::IDCompositionTranslateTransform* pTrans = nullptr;
    pDcompDevice->CreateTranslateTransform(&pTrans);
    TEST_ASSERT(pTrans != nullptr, "CreateTranslateTransform must succeed");
    pTrans->SetOffsetX(10.0f);
    pTrans->SetOffsetY(20.0f);
    pCard->SetTransform(pTrans);
    TEST_ASSERT(pCard->GetTransform() == pTrans, "Card transform must match translate transform");

    dcomp::IDCompositionScaleTransform* pScale = nullptr;
    pDcompDevice->CreateScaleTransform(&pScale);
    TEST_ASSERT(pScale != nullptr, "CreateScaleTransform must succeed");
    pScale->SetScaleX(1.25f);
    pScale->SetScaleY(1.25f);
    pScale->SetCenterX(100.0f);
    pScale->SetCenterY(50.0f);
    TEST_ASSERT(pScale->GetScaleX() == 1.25f && pScale->GetScaleY() == 1.25f, "Scale values must match 1.25");

    dcomp::IDCompositionRotateTransform* pRotate = nullptr;
    pDcompDevice->CreateRotateTransform(&pRotate);
    TEST_ASSERT(pRotate != nullptr, "CreateRotateTransform must succeed");
    pRotate->SetAngle(45.0f);
    TEST_ASSERT(pRotate->GetAngle() == 45.0f, "Rotate angle must match 45.0");

    dcomp::IDCompositionMatrixTransform* pMatrixTrans = nullptr;
    pDcompDevice->CreateMatrixTransform(&pMatrixTrans);
    TEST_ASSERT(pMatrixTrans != nullptr, "CreateMatrixTransform must succeed");
    dcomp::DCOMP_MATRIX3x2 m3x2{};
    m3x2.m[0][0] = 3.0f; m3x2.m[1][1] = 3.0f;
    pMatrixTrans->SetMatrix(m3x2);
    TEST_ASSERT(pMatrixTrans->GetMatrix().m[0][0] == 3.0f, "Matrix transform element [0][0] must match 3.0");

    // 6. Parametric Animation Engine
    dcomp::IDCompositionAnimation* pAnim = nullptr;
    pDcompDevice->CreateAnimation(&pAnim);
    TEST_ASSERT(pAnim != nullptr, "CreateAnimation must succeed");

    pAnim->AddCubic(0.0, 0.0f, 50.0f, 0.0f, 0.0f);
    pAnim->AddSinusoidal(1.0, 50.0f, 10.0f, 3.14159f / 2.0f, 0.0f);
    pAnim->End(3.0, 100.0f);

    float val0 = pAnim->Evaluate(0.0);
    float valHalf = pAnim->Evaluate(0.5);
    float valEnd = pAnim->Evaluate(4.0);
    TEST_ASSERT(std::abs(val0 - 0.0f) < 1e-4f, "Animation at t=0 must evaluate to 0.0");
    TEST_ASSERT(std::abs(valHalf - 25.0f) < 1e-4f, "Animation at t=0.5 must evaluate to 25.0");
    TEST_ASSERT(std::abs(valEnd - 100.0f) < 1e-4f, "Animation at t=4.0 must evaluate to 100.0");

    pButton->SetOpacity(pAnim);

    // 7. Clipping Bounds (Rect & Rounded Rectangle Clip)
    dcomp::DCOMP_RECT baseClip{ 0, 0, 1024, 768 };
    pRoot->SetClip(baseClip);
    TEST_ASSERT(pRoot->HasClipRect(), "Root must have clip rect set");
    TEST_ASSERT(pRoot->GetClipRect().right == 1024, "Root clip right must be 1024");

    dcomp::IDCompositionRectangleClip* pRectClip = nullptr;
    pDcompDevice->CreateRectangleClip(&pRectClip);
    TEST_ASSERT(pRectClip != nullptr, "CreateRectangleClip must succeed");
    pRectClip->SetLeft(5.0f);
    pRectClip->SetTop(5.0f);
    pRectClip->SetRight(200.0f);
    pRectClip->SetBottom(150.0f);
    pRectClip->SetTopLeftRadiusX(16.0f);
    pRectClip->SetTopLeftRadiusY(16.0f);
    pCard->SetClip(pRectClip);
    TEST_ASSERT(pCard->GetClip() == pRectClip, "Card clip must match rectangle clip");

    // 8. Composition Surface Lifecycle
    dcomp::IDCompositionSurface* pSurface = nullptr;
    pDcompDevice->CreateSurface(512, 512, prismx::DXGI_FORMAT_R8G8B8A8_UNORM, 1, &pSurface);
    TEST_ASSERT(pSurface != nullptr, "CreateSurface must succeed");
    TEST_ASSERT(pSurface->GetWidth() == 512 && pSurface->GetHeight() == 512, "Surface dimensions must be 512x512");

    void* pDrawObj = nullptr;
    dcomp::DCOMP_POINT drawOffset{};
    dcomp::DCOMP_RECT updateRect{ 0, 0, 256, 256 };
    hr = pSurface->BeginDraw(&updateRect, dcomp::IID_IDCompositionSurface_Const, &pDrawObj, &drawOffset);
    TEST_ASSERT(hr == 0 && pDrawObj != nullptr, "BeginDraw on surface must succeed");

    uint8_t* pPix = pSurface->GetBuffer();
    TEST_ASSERT(pPix != nullptr, "Surface buffer pointer must not be null");
    std::memset(pPix, 0x7F, 512 * 512 * 4);
    hr = pSurface->EndDraw();
    TEST_ASSERT(hr == 0, "EndDraw on surface must succeed");

    pCard->SetContent(pSurface);
    TEST_ASSERT(pCard->GetContent() == pSurface, "Card content must match surface");

    // 9. Composition Target & Frame Commit Transaction
    dcomp::HWND testHwnd = reinterpret_cast<dcomp::HWND>(0xDEAD0001);
    dcomp::IDCompositionTarget* pTarget = nullptr;
    hr = pDcompDevice->CreateTargetForHwnd(testHwnd, true, &pTarget);
    TEST_ASSERT(hr == 0 && pTarget != nullptr, "CreateTargetForHwnd must succeed");
    TEST_ASSERT(pTarget->GetHwnd() == testHwnd, "Target HWND must match");

    hr = pTarget->SetRoot(pRoot);
    TEST_ASSERT(hr == 0, "SetRoot on target must succeed");
    TEST_ASSERT(pTarget->GetRoot() == pRoot, "Target root must match root visual");

    hr = pDcompDevice->Commit();
    TEST_ASSERT(hr == 0, "Commit transaction on compositor must succeed");

    dcomp::DCOMPOSITION_FRAME_STATISTICS stats{};
    hr = pDcompDevice->GetFrameStatistics(&stats);
    TEST_ASSERT(hr == 0, "GetFrameStatistics must succeed");
    TEST_ASSERT(stats.nextKeyFrame >= 1, "nextKeyFrame must be >= 1");
    TEST_ASSERT(stats.currentFrameTime > 0, "currentFrameTime must be > 0");

    // 10. DirectComposition Device2 & Surface Handle
    dcomp::IDCompositionDevice2* pDevice2 = nullptr;
    hr = pDcompDevice->QueryInterface(dcomp::IID_IDCompositionDevice2_Const, reinterpret_cast<void**>(&pDevice2));
    TEST_ASSERT(hr == 0 && pDevice2 != nullptr, "QueryInterface for IDCompositionDevice2 must succeed");

    dcomp::IDCompositionVisual2* pVis2 = nullptr;
    hr = pDevice2->CreateVisual2(&pVis2);
    TEST_ASSERT(hr == 0 && pVis2 != nullptr, "CreateVisual2 on IDCompositionDevice2 must succeed");
    pVis2->SetOpacityMode(dcomp::DCOMPOSITION_OPACITY_MODE::MULTIPLY);
    pVis2->SetBackFaceVisibility(dcomp::DCOMPOSITION_BACKFACE_VISIBILITY::HIDDEN);
    TEST_ASSERT(pVis2->GetOpacityMode() == dcomp::DCOMPOSITION_OPACITY_MODE::MULTIPLY, "OpacityMode must match Multiply");
    TEST_ASSERT(pVis2->GetBackFaceVisibility() == dcomp::DCOMPOSITION_BACKFACE_VISIBILITY::HIDDEN, "BackFaceVisibility must match Hidden");

    dcomp::HANDLE hShared = nullptr;
    hr = dcomp::DCompositionCreateSurfaceHandle(0, nullptr, &hShared);
    TEST_ASSERT(hr == 0 && hShared != nullptr, "DCompositionCreateSurfaceHandle must succeed");

    // 11. Interactive CLI Verification (dcomp test, dcomp info, dcomp compose)
    shell::CommandShell shellEngine;
    std::ostringstream testOut;
    int rc = shellEngine.execute("dcomp test", testOut);
    TEST_ASSERT(rc == 0, "dcomp test CLI command must return 0");
    TEST_ASSERT(testOut.str().find("10 / 10 Subsystem Invariants Verified") != std::string::npos, "dcomp test must pass all 10 invariants");

    std::ostringstream infoOut;
    rc = shellEngine.execute("dcomp info", infoOut);
    TEST_ASSERT(rc == 0, "dcomp info CLI command must return 0");
    TEST_ASSERT(infoOut.str().find("DirectComposition 2.0") != std::string::npos, "dcomp info must display DirectComposition architecture");

    std::ostringstream composeOut;
    rc = shellEngine.execute("dcomp compose", composeOut);
    TEST_ASSERT(rc == 0, "dcomp compose CLI command must return 0");
    TEST_ASSERT(composeOut.str().find("Frame Committed Successfully") != std::string::npos, "dcomp compose must commit composite frame");

    // Cleanup
    pVis2->Release();
    pDevice2->Release();
    pTarget->Release();
    pSurface->Release();
    pRectClip->Release();
    pAnim->Release();
    pMatrixTrans->Release();
    pRotate->Release();
    pScale->Release();
    pTrans->Release();
    pButton->Release();
    pCard->Release();
    pWindow->Release();
    pRoot->Release();
    pDcompDevice->Release();

    std::cout << "[TEST] Suite 117: Windows DirectComposition Subsystem PASSED.\n";
}

// ============================================================================
// Suite 118: Windows UI Composition & Modern Visual Layer Subsystem
// ============================================================================
void Test_WindowsUIComposition_Subsystem() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 118: Windows UI Composition & Modern Visual Layer Subsystem      \n";
    std::cout << "========================================================================\n";

    composition::InitializeUICompositionExports();

    // 1. Dynamic Module Export Verification
    auto& loader = ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("windows.ui.composition.dll", "DllGetActivationFactory") != nullptr,
                "windows.ui.composition.dll must export DllGetActivationFactory");
    TEST_ASSERT(loader.getExport("windows.ui.composition.dll", "DllCanUnloadNow") != nullptr,
                "windows.ui.composition.dll must export DllCanUnloadNow");
    TEST_ASSERT(loader.getExport("microsoft.ui.composition.dll", "DllGetActivationFactory") != nullptr,
                "microsoft.ui.composition.dll must export DllGetActivationFactory");
    TEST_ASSERT(loader.getExport("microsoft.ui.composition.dll", "DllCanUnloadNow") != nullptr,
                "microsoft.ui.composition.dll must export DllCanUnloadNow");

    // 2. Version Database Verification
    const auto* modWinUI = version::VersionDatabase::Instance().GetModuleInfo("windows.ui.composition.dll");
    TEST_ASSERT(modWinUI != nullptr, "VersionDatabase must contain windows.ui.composition.dll");
    TEST_ASSERT(modWinUI->stringTable.at("FileVersion") == "10.0.22621.1",
                "windows.ui.composition.dll FileVersion must be 10.0.22621.1");

    const auto* modMsUI = version::VersionDatabase::Instance().GetModuleInfo("microsoft.ui.composition.dll");
    TEST_ASSERT(modMsUI != nullptr, "VersionDatabase must contain microsoft.ui.composition.dll");
    TEST_ASSERT(modMsUI->stringTable.at("FileVersion") == "10.0.22621.1",
                "microsoft.ui.composition.dll FileVersion must be 10.0.22621.1");

    // 3. Activation Factory Resolution & Compositor Creation
    composition::IActivationFactory* pFactory = nullptr;
    int32_t hr = composition::DllGetActivationFactory(
        reinterpret_cast<composition::HSTRING>(const_cast<wchar_t*>(L"Windows.UI.Composition.Compositor")),
        &pFactory
    );
    TEST_ASSERT(hr == 0 && pFactory != nullptr, "DllGetActivationFactory for Windows.UI.Composition.Compositor must succeed");

    composition::IInspectable* pInsp = nullptr;
    hr = pFactory->ActivateInstance(&pInsp);
    TEST_ASSERT(hr == 0 && pInsp != nullptr, "IActivationFactory::ActivateInstance must succeed");

    composition::ICompositor* pCompositor = nullptr;
    hr = pInsp->QueryInterface(composition::IID_ICompositor, reinterpret_cast<void**>(&pCompositor));
    TEST_ASSERT(hr == 0 && pCompositor != nullptr, "QueryInterface for ICompositor must succeed");

    // 4. Scene Graph Visual Tree Construction (Root Container -> Card Sprite -> Button Sprite)
    composition::IContainerVisual* pRoot = nullptr;
    composition::ISpriteVisual* pCard = nullptr;
    composition::ISpriteVisual* pButton = nullptr;

    pCompositor->CreateContainerVisual(&pRoot);
    pCompositor->CreateSpriteVisual(&pCard);
    pCompositor->CreateSpriteVisual(&pButton);
    TEST_ASSERT(pRoot && pCard && pButton, "CreateContainerVisual and CreateSpriteVisual must succeed");

    // Configure properties
    pCard->SetOffset({ 100.0f, 120.0f, 0.0f });
    pCard->SetSize({ 600.0f, 400.0f });
    pCard->SetScale({ 1.0f, 1.0f, 1.0f });
    pCard->SetOpacity(0.95f);
    pCard->SetRotationAngle(0.15f);

    TEST_ASSERT(pCard->GetOffset().x == 100.0f && pCard->GetOffset().y == 120.0f, "Card visual offset must match (100, 120)");
    TEST_ASSERT(pCard->GetSize().x == 600.0f && pCard->GetSize().y == 400.0f, "Card visual size must match (600, 400)");
    TEST_ASSERT(std::abs(pCard->GetOpacity() - 0.95f) < 1e-4f, "Card visual opacity must match 0.95");

    // Hierarchy linking
    composition::IVisualCollection* pRootChildren = nullptr;
    pRoot->GetChildren(&pRootChildren);
    TEST_ASSERT(pRootChildren != nullptr, "GetChildren must return valid VisualCollection");

    pRootChildren->InsertAtTop(pCard);
    pRootChildren->InsertAbove(pButton, pCard);
    TEST_ASSERT(pRootChildren->GetCount() == 2, "Root visual must contain 2 children");
    TEST_ASSERT(pRootChildren->GetAt(0) == pCard, "First child must be Card");
    TEST_ASSERT(pRootChildren->GetAt(1) == pButton, "Second child must be Button");
    TEST_ASSERT(pCard->GetParent() == pRoot && pButton->GetParent() == pRoot, "Children parent pointers must point to root");

    // 5. Composition Brushes (ColorBrush, SurfaceBrush, EffectBrush)
    composition::CompositionColor micaSlate{ 255, 32, 44, 60 };
    composition::ICompositionColorBrush* pColorBrush = nullptr;
    pCompositor->CreateColorBrushWithColor(micaSlate, &pColorBrush);
    TEST_ASSERT(pColorBrush != nullptr, "CreateColorBrushWithColor must succeed");
    TEST_ASSERT(pColorBrush->GetColor() == micaSlate, "ColorBrush color must match micaSlate");

    pCard->SetBrush(pColorBrush);
    TEST_ASSERT(pCard->GetBrush() == pColorBrush, "Card brush must match color brush");

    composition::ICompositionSurfaceBrush* pSurfaceBrush = nullptr;
    pCompositor->CreateSurfaceBrush(&pSurfaceBrush);
    TEST_ASSERT(pSurfaceBrush != nullptr, "CreateSurfaceBrush must succeed");
    pSurfaceBrush->SetStretch(composition::CompositionStretch::UniformToFill);
    pSurfaceBrush->SetHorizontalAlignmentRatio(0.8f);
    TEST_ASSERT(pSurfaceBrush->GetStretch() == composition::CompositionStretch::UniformToFill, "Stretch must match UniformToFill");
    TEST_ASSERT(std::abs(pSurfaceBrush->GetHorizontalAlignmentRatio() - 0.8f) < 1e-4f, "HorizontalAlignmentRatio must match 0.8");

    composition::ICompositionEffectBrush* pEffectBrush = nullptr;
    pCompositor->CreateEffectBrush(L"AcrylicBackdropFilter", &pEffectBrush);
    TEST_ASSERT(pEffectBrush != nullptr, "CreateEffectBrush must succeed");
    pEffectBrush->SetSourceParameter(L"SourceBackdrop", pColorBrush);
    TEST_ASSERT(pEffectBrush->GetSourceParameter(L"SourceBackdrop") == pColorBrush, "Source parameter brush must match");

    // 6. Keyframe Animations (Scalar & Vector3)
    composition::IScalarKeyFrameAnimation* pScalarAnim = nullptr;
    pCompositor->CreateScalarKeyFrameAnimation(&pScalarAnim);
    TEST_ASSERT(pScalarAnim != nullptr, "CreateScalarKeyFrameAnimation must succeed");
    pScalarAnim->SetDuration(2.0f);
    pScalarAnim->InsertKeyFrame(0.0f, 0.0f);
    pScalarAnim->InsertKeyFrame(0.5f, 40.0f);
    pScalarAnim->InsertKeyFrame(1.0f, 100.0f);

    TEST_ASSERT(std::abs(pScalarAnim->Evaluate(0.0f) - 0.0f) < 1e-4f, "Scalar animation at t=0 must be 0.0");
    TEST_ASSERT(std::abs(pScalarAnim->Evaluate(0.5f) - 40.0f) < 1e-4f, "Scalar animation at t=0.5 must be 40.0");
    TEST_ASSERT(std::abs(pScalarAnim->Evaluate(1.0f) - 100.0f) < 1e-4f, "Scalar animation at t=1.0 must be 100.0");

    pButton->StartAnimation(L"Opacity", pScalarAnim);

    composition::IVector3KeyFrameAnimation* pVecAnim = nullptr;
    pCompositor->CreateVector3KeyFrameAnimation(&pVecAnim);
    TEST_ASSERT(pVecAnim != nullptr, "CreateVector3KeyFrameAnimation must succeed");
    pVecAnim->InsertKeyFrame(0.0f, { 0.0f, 0.0f, 0.0f });
    pVecAnim->InsertKeyFrame(1.0f, { 20.0f, 40.0f, 60.0f });
    composition::Vector3 vMid = pVecAnim->Evaluate(0.5f);
    TEST_ASSERT(std::abs(vMid.x - 10.0f) < 1e-3f, "Vector3 animation X at mid must be 10.0");
    TEST_ASSERT(std::abs(vMid.y - 20.0f) < 1e-3f, "Vector3 animation Y at mid must be 20.0");
    TEST_ASSERT(std::abs(vMid.z - 30.0f) < 1e-3f, "Vector3 animation Z at mid must be 30.0");

    // 7. Dynamic Expression Animation Evaluation
    composition::IExpressionAnimation* pExprAnim = nullptr;
    pCompositor->CreateExpressionAnimationWithExpression(L"Lerp(A, B, Progress)", &pExprAnim);
    TEST_ASSERT(pExprAnim != nullptr, "CreateExpressionAnimationWithExpression must succeed");
    pExprAnim->SetScalarParameter(L"A", 100.0f);
    pExprAnim->SetScalarParameter(L"B", 200.0f);
    pExprAnim->SetScalarParameter(L"Progress", 0.75f);
    float exprVal = pExprAnim->EvaluateScalar();
    // 100 + (200 - 100) * 0.75 = 175.0
    TEST_ASSERT(std::abs(exprVal - 175.0f) < 1e-4f, "Expression animation Lerp(100, 200, 0.75) must evaluate to 175.0");

    // 8. Reactive Property Set Key-Value Store
    composition::ICompositionPropertySet* pPropSet = nullptr;
    pCompositor->CreatePropertySet(&pPropSet);
    TEST_ASSERT(pPropSet != nullptr, "CreatePropertySet must succeed");
    pPropSet->InsertScalar(L"BlurRadius", 16.0f);
    pPropSet->InsertVector3(L"AnchorPoint", { 0.5f, 0.5f, 0.0f });

    float readBlur = 0.0f;
    hr = pPropSet->TryGetScalar(L"BlurRadius", &readBlur);
    TEST_ASSERT(hr == 0 && std::abs(readBlur - 16.0f) < 1e-4f, "TryGetScalar for BlurRadius must return 16.0");

    composition::Vector3 readAnchor{};
    hr = pPropSet->TryGetVector3(L"AnchorPoint", &readAnchor);
    TEST_ASSERT(hr == 0 && readAnchor == composition::Vector3(0.5f, 0.5f, 0.0f), "TryGetVector3 for AnchorPoint must match (0.5, 0.5, 0)");

    // 9. Interactive CLI Verification (uicomp test, uicomp info, uicomp demo)
    shell::CommandShell shellEngine;
    std::ostringstream testOut;
    int rc = shellEngine.execute("uicomp test", testOut);
    TEST_ASSERT(rc == 0, "uicomp test CLI command must return 0");
    TEST_ASSERT(testOut.str().find("10 / 10 Subsystem Invariants Verified") != std::string::npos,
                "uicomp test must pass all 10 invariants");

    std::ostringstream infoOut;
    rc = shellEngine.execute("uicomp info", infoOut);
    TEST_ASSERT(rc == 0, "uicomp info CLI command must return 0");
    TEST_ASSERT(infoOut.str().find("PrismComposition") != std::string::npos,
                "uicomp info must display PrismComposition architecture");

    std::ostringstream demoOut;
    rc = shellEngine.execute("uicomp demo", demoOut);
    TEST_ASSERT(rc == 0, "uicomp demo CLI command must return 0");
    TEST_ASSERT(demoOut.str().find("Scene Composition Successfully Realized") != std::string::npos,
                "uicomp demo must realize scene composition");

    // Cleanup
    pPropSet->Release();
    pExprAnim->Release();
    pVecAnim->Release();
    pScalarAnim->Release();
    pEffectBrush->Release();
    pSurfaceBrush->Release();
    pColorBrush->Release();
    pRootChildren->Release();
    pButton->Release();
    pCard->Release();
    pRoot->Release();
    pCompositor->Release();
    pInsp->Release();
    pFactory->Release();

    std::cout << "[TEST] Suite 118: Windows UI Composition & Modern Visual Layer Subsystem PASSED.\n";
}

// ============================================================================
// Suite 119: Windows Color System (WCS) & Advanced HDR Subsystem
// ============================================================================
void Test_WindowsColorSystem_Subsystem() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 119: Windows Color System (WCS) & Advanced HDR Subsystem         \n";
    std::cout << "========================================================================\n";

    using namespace micant::wcs;
    InitializeWCSExports();

    // 1. Dynamic Module Export Verification
    auto& loader = ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("mscms.dll", "OpenColorProfileW") != nullptr,
                "mscms.dll must export OpenColorProfileW");
    TEST_ASSERT(loader.getExport("mscms.dll", "OpenColorProfileA") != nullptr,
                "mscms.dll must export OpenColorProfileA");
    TEST_ASSERT(loader.getExport("mscms.dll", "CloseColorProfile") != nullptr,
                "mscms.dll must export CloseColorProfile");
    TEST_ASSERT(loader.getExport("mscms.dll", "GetColorProfileHeader") != nullptr,
                "mscms.dll must export GetColorProfileHeader");
    TEST_ASSERT(loader.getExport("mscms.dll", "SetColorProfileHeader") != nullptr,
                "mscms.dll must export SetColorProfileHeader");
    TEST_ASSERT(loader.getExport("mscms.dll", "GetStandardColorSpaceProfileW") != nullptr,
                "mscms.dll must export GetStandardColorSpaceProfileW");
    TEST_ASSERT(loader.getExport("mscms.dll", "CreateColorTransformW") != nullptr,
                "mscms.dll must export CreateColorTransformW");
    TEST_ASSERT(loader.getExport("mscms.dll", "TranslateColors") != nullptr,
                "mscms.dll must export TranslateColors");
    TEST_ASSERT(loader.getExport("mscms.dll", "TranslateBitmapBits") != nullptr,
                "mscms.dll must export TranslateBitmapBits");
    TEST_ASSERT(loader.getExport("mscms.dll", "CheckColors") != nullptr,
                "mscms.dll must export CheckColors");
    TEST_ASSERT(loader.getExport("mscms.dll", "WcsGetDefaultColorProfile") != nullptr,
                "mscms.dll must export WcsGetDefaultColorProfile");

    TEST_ASSERT(loader.getExport("icm32.dll", "OpenColorProfileW") != nullptr,
                "icm32.dll must export OpenColorProfileW");
    TEST_ASSERT(loader.getExport("icm32.dll", "CreateColorTransformW") != nullptr,
                "icm32.dll must export CreateColorTransformW");

    // 2. Version Database Verification
    const auto* modMscms = version::VersionDatabase::Instance().GetModuleInfo("mscms.dll");
    TEST_ASSERT(modMscms != nullptr, "VersionDatabase must contain mscms.dll");
    TEST_ASSERT(modMscms->stringTable.at("FileVersion") == "10.0.22621.1",
                "mscms.dll FileVersion must be 10.0.22621.1");

    const auto* modIcm = version::VersionDatabase::Instance().GetModuleInfo("icm32.dll");
    TEST_ASSERT(modIcm != nullptr, "VersionDatabase must contain icm32.dll");
    TEST_ASSERT(modIcm->stringTable.at("FileVersion") == "10.0.22621.1",
                "icm32.dll FileVersion must be 10.0.22621.1");

    // 3. Profile Creation from Memory & Header Verification
    wcs::PROFILEHEADER hdr{};
    hdr.phSize = sizeof(wcs::PROFILEHEADER);
    hdr.phCMMType = 0x5052534D; // 'PRSM'
    hdr.phVersion = 0x04300000; // v4.3.0
    hdr.phClass = 0x6D6E7472;   // 'mntr'
    hdr.phDataColorSpace = 0x52474220; // 'RGB '
    hdr.phConnectionSpace = 0x58595A20; // 'XYZ '
    hdr.phSignature = 0x61637370; // 'acsp'
    hdr.phPlatform = 0x4D534654; // 'MSFT'
    hdr.phRenderingIntent = wcs::INTENT_PERCEPTUAL;
    hdr.phCreator = 0x4D494341; // 'MICA'

    wcs::PROFILE profMem{};
    profMem.dwType = wcs::PROFILE_MEMBUFFER;
    profMem.pProfileData = &hdr;
    profMem.cbDataSize = sizeof(hdr);

    wcs::HPROFILE hProf = wcs::OpenColorProfileW(&profMem, wcs::PROFILE_READ, 1, wcs::OPEN_EXISTING);
    TEST_ASSERT(hProf != nullptr, "OpenColorProfileW from memory buffer must succeed");

    wcs::PROFILEHEADER readHdr{};
    wcs::BOOL bRes = wcs::GetColorProfileHeader(hProf, &readHdr);
    TEST_ASSERT(bRes == TRUE, "GetColorProfileHeader must return TRUE");
    TEST_ASSERT(readHdr.phSignature == 0x61637370, "Profile signature must be 'acsp' (0x61637370)");
    TEST_ASSERT(readHdr.phRenderingIntent == wcs::INTENT_PERCEPTUAL, "Initial intent must be INTENT_PERCEPTUAL");

    readHdr.phRenderingIntent = wcs::INTENT_RELATIVE_COLORIMETRIC;
    bRes = wcs::SetColorProfileHeader(hProf, &readHdr);
    TEST_ASSERT(bRes == TRUE, "SetColorProfileHeader must return TRUE");

    wcs::PROFILEHEADER updatedHdr{};
    wcs::GetColorProfileHeader(hProf, &updatedHdr);
    TEST_ASSERT(updatedHdr.phRenderingIntent == wcs::INTENT_RELATIVE_COLORIMETRIC, "Updated intent must be Relative Colorimetric");

    // 4. Standard Color Space Profiles
    wchar_t srgbBuf[260]{};
    uint32_t srgbBytes = sizeof(srgbBuf);
    bRes = wcs::GetStandardColorSpaceProfileW(nullptr, wcs::SPACE_sRGB, srgbBuf, &srgbBytes);
    TEST_ASSERT(bRes == TRUE, "GetStandardColorSpaceProfileW for sRGB must succeed");
    TEST_ASSERT(std::wstring(srgbBuf).find(L"sRGB") != std::wstring::npos, "sRGB profile path must contain 'sRGB'");

    char srgbBufA[260]{};
    uint32_t srgbBytesA = sizeof(srgbBufA);
    bRes = wcs::GetStandardColorSpaceProfileA(nullptr, wcs::SPACE_sRGB, srgbBufA, &srgbBytesA);
    TEST_ASSERT(bRes == TRUE, "GetStandardColorSpaceProfileA for sRGB must succeed");

    wchar_t defProfile[64]{};
    bRes = wcs::WcsGetDefaultColorProfile(0, nullptr, wcs::CPT_ICC, wcs::CPST_PERCEPTUAL, 0, sizeof(defProfile), defProfile);
    TEST_ASSERT(bRes == TRUE, "WcsGetDefaultColorProfile must succeed");
    TEST_ASSERT(std::wstring(defProfile).find(L"sRGB") != std::wstring::npos, "Default profile must be sRGB");

    // 5. Color Transforms & Pixel Translation
    wcs::PROFILE profFile{};
    profFile.dwType = wcs::PROFILE_FILENAME;
    profFile.pProfileData = const_cast<wchar_t*>(L"C:\\Windows\\System32\\spool\\drivers\\color\\sRGB.icm");
    profFile.cbDataSize = 0;

    wcs::HTRANSFORM hTrans = wcs::CreateColorTransformW(&profFile, 0, wcs::INTENT_PERCEPTUAL, 0);
    TEST_ASSERT(hTrans != nullptr, "CreateColorTransformW must return valid transform handle");

    // TranslateColors (RGB -> XYZ)
    wcs::COLOR colIn{};
    colIn.rgb.red = 65535; colIn.rgb.green = 65535; colIn.rgb.blue = 65535;
    wcs::COLOR colOut{};
    bRes = wcs::TranslateColors(hTrans, &colIn, 1, wcs::COLOR_RGB, &colOut, wcs::COLOR_XYZ);
    TEST_ASSERT(bRes == TRUE, "TranslateColors RGB -> XYZ must succeed");
    TEST_ASSERT(colOut.xyz.y > 60000, "White point Y in XYZ must be near 65535");

    // CheckColors (Gamut verification)
    uint8_t gamutRes = 0xFF;
    bRes = wcs::CheckColors(hTrans, &colIn, 1, wcs::COLOR_RGB, &gamutRes);
    TEST_ASSERT(bRes == TRUE && gamutRes == 0, "CheckColors must report standard white is in-gamut (0)");

    // TranslateBitmapBits (BGRA -> RGBA)
    uint8_t srcPixels[8] = { 255, 0, 0, 255, 0, 255, 0, 255 }; // Pixel 0: Blue, Pixel 1: Green
    uint8_t dstPixels[8] = { 0 };
    bRes = wcs::TranslateBitmapBits(hTrans, srcPixels, wcs::BM_BGRAQUADS, 2, 1, 8, dstPixels, wcs::BM_RGBAQUADS, 8, nullptr, nullptr);
    TEST_ASSERT(bRes == TRUE, "TranslateBitmapBits must succeed");
    TEST_ASSERT(dstPixels[0] == 0 && dstPixels[2] == 255, "BGRA blue byte must be swapped to RGBA blue byte index 2");
    TEST_ASSERT(dstPixels[3] == 255 && dstPixels[7] == 255, "Alpha channels must be preserved");

    // MultiProfileTransform
    wcs::HPROFILE hProf2 = wcs::OpenColorProfileW(&profMem, wcs::PROFILE_READ, 1, wcs::OPEN_EXISTING);
    wcs::HPROFILE profArray[2] = { hProf, hProf2 };
    uint32_t intentArray[2] = { wcs::INTENT_PERCEPTUAL, wcs::INTENT_PERCEPTUAL };
    wcs::HTRANSFORM hMulti = wcs::CreateMultiProfileTransform(profArray, 2, intentArray, 2, 0, 0);
    TEST_ASSERT(hMulti != nullptr, "CreateMultiProfileTransform must succeed");
    wcs::DeleteColorTransform(hMulti);
    wcs::CloseColorProfile(hProf2);

    wcs::DeleteColorTransform(hTrans);
    wcs::CloseColorProfile(hProf);

    // 6. Transfer Curves & High Dynamic Range (HDR) Colorimetry
    float linVal = wcs::ColorMath::sRGBToLinear(0.5f);
    TEST_ASSERT(linVal > 0.0f && linVal < 0.5f, "sRGB 0.5 to linear must be strictly in (0, 0.5)");
    float srgbRecon = wcs::ColorMath::LinearTosRGB(linVal);
    TEST_ASSERT(std::abs(srgbRecon - 0.5f) < 1e-4f, "sRGB roundtrip must match 0.5");

    // SMPTE ST 2084 PQ (0 to 10,000 Nits)
    float pq100 = wcs::ColorMath::NitsToPQ(100.0f);
    float nits100 = wcs::ColorMath::PQToNits(pq100);
    TEST_ASSERT(std::abs(nits100 - 100.0f) < 0.5f, "SMPTE ST 2084 100 Nits roundtrip must match");

    float pq1000 = wcs::ColorMath::NitsToPQ(1000.0f);
    float nits1000 = wcs::ColorMath::PQToNits(pq1000);
    TEST_ASSERT(std::abs(nits1000 - 1000.0f) < 0.5f, "SMPTE ST 2084 1000 Nits roundtrip must match");
    TEST_ASSERT(pq1000 > pq100, "1000 Nits PQ code must exceed 100 Nits PQ code");

    // ARIB STD-B67 HLG
    float hlgLinear = wcs::ColorMath::HLGToLinear(0.4f);
    TEST_ASSERT(hlgLinear > 0.0f && hlgLinear < 1.0f, "HLG 0.4 to linear radiance must be in (0, 1)");

    // ACES Film Tone Mapping & Delta E
    float mappedTone = wcs::ColorMath::ACESFilm(3.0f);
    TEST_ASSERT(mappedTone <= 1.0f && mappedTone > 0.0f, "ACES Film tone mapping must compress HDR to [0, 1]");

    float deltaZero = wcs::ColorMath::DeltaE76({ 50.0f, 10.0f, -20.0f }, { 50.0f, 10.0f, -20.0f });
    TEST_ASSERT(deltaZero < 1e-5f, "Identical colors must have Delta E of 0");

    // 7. Interactive Shell Verification (wcs test, wcs info, wcs gamut)
    shell::CommandShell shellEngine;
    std::ostringstream testOut;
    int rc = shellEngine.execute("wcs test", testOut);
    TEST_ASSERT(rc == 0, "wcs test CLI command must return 0");
    TEST_ASSERT(testOut.str().find("10 / 10 Subsystem Invariants Verified") != std::string::npos,
                "wcs test must verify all 10 invariants");

    std::ostringstream infoOut;
    rc = shellEngine.execute("wcs info", infoOut);
    TEST_ASSERT(rc == 0, "wcs info CLI command must return 0");
    TEST_ASSERT(infoOut.str().find("Windows Color System") != std::string::npos,
                "wcs info must display WCS telemetry");

    std::ostringstream gamutOut;
    rc = shellEngine.execute("wcs gamut", gamutOut);
    TEST_ASSERT(rc == 0, "wcs gamut CLI command must return 0");
    TEST_ASSERT(gamutOut.str().find("Wide Color Gamut (WCG)") != std::string::npos,
                "wcs gamut must analyze wide color gamut");

    std::cout << "[TEST] Suite 119: Windows Color System (WCS) & Advanced HDR Subsystem PASSED.\n";
}

// ============================================================================
// Suite 120: Windows Pointer Device & Modern Touch/Inking Subsystem
// ============================================================================
void Test_WindowsPointerDevice_Subsystem() {
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 120: Windows Pointer Device & Modern Touch/Inking Subsystem      \n";
    std::cout << "========================================================================\n";

    using namespace micant::pointer;
    InitializePointerExports();

    // 1. Dynamic Module Export Verification
    auto& loader = ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("user32.dll", "GetPointerType") != nullptr,
                "user32.dll must export GetPointerType");
    TEST_ASSERT(loader.getExport("user32.dll", "GetPointerInfo") != nullptr,
                "user32.dll must export GetPointerInfo");
    TEST_ASSERT(loader.getExport("user32.dll", "GetPointerTouchInfo") != nullptr,
                "user32.dll must export GetPointerTouchInfo");
    TEST_ASSERT(loader.getExport("user32.dll", "GetPointerPenInfo") != nullptr,
                "user32.dll must export GetPointerPenInfo");
    TEST_ASSERT(loader.getExport("user32.dll", "GetPointerInfoHistory") != nullptr,
                "user32.dll must export GetPointerInfoHistory");
    TEST_ASSERT(loader.getExport("user32.dll", "GetPointerDevices") != nullptr,
                "user32.dll must export GetPointerDevices");
    TEST_ASSERT(loader.getExport("user32.dll", "EnableMouseInPointer") != nullptr,
                "user32.dll must export EnableMouseInPointer");
    TEST_ASSERT(loader.getExport("user32.dll", "IsMouseInPointerEnabled") != nullptr,
                "user32.dll must export IsMouseInPointerEnabled");

    TEST_ASSERT(loader.getExport("windows.ui.input.dll", "GetPointerType") != nullptr,
                "windows.ui.input.dll must export GetPointerType");
    TEST_ASSERT(loader.getExport("windows.ui.input.dll", "GetPointerInfo") != nullptr,
                "windows.ui.input.dll must export GetPointerInfo");

    // 2. Version Database Verification
    const auto* modInput = version::VersionDatabase::Instance().GetModuleInfo("windows.ui.input.dll");
    TEST_ASSERT(modInput != nullptr, "VersionDatabase must contain windows.ui.input.dll");
    TEST_ASSERT(modInput->stringTable.at("FileVersion") == "10.0.22621.1",
                "windows.ui.input.dll FileVersion must be 10.0.22621.1");

    // 3. Pointer Device Enumeration
    uint32_t devCount = 0;
    BOOL bRes = GetPointerDevices(&devCount, nullptr);
    TEST_ASSERT(bRes == TRUE_VAL && devCount >= 3, "GetPointerDevices must report >= 3 attached digitizer devices");

    std::vector<POINTER_DEVICE_INFO> devs(devCount);
    bRes = GetPointerDevices(&devCount, devs.data());
    TEST_ASSERT(bRes == TRUE_VAL, "GetPointerDevices buffer retrieval must succeed");
    TEST_ASSERT(devs[0].pointerDeviceType == PT_TOUCH && devs[0].maxActiveContacts == 10,
                "Device 0 must be 10-contact Multi-Touch Screen");
    TEST_ASSERT(devs[1].pointerDeviceType == PT_PEN && devs[1].maxActiveContacts == 1,
                "Device 1 must be Precision Stylus Digitizer");
    TEST_ASSERT(devs[2].pointerDeviceType == PT_TOUCHPAD && devs[2].maxActiveContacts == 5,
                "Device 2 must be Precision Touchpad");

    // 4. Mouse-In-Pointer Mode Promotion
    BOOL origMouseMode = IsMouseInPointerEnabled();
    EnableMouseInPointer(TRUE_VAL);
    TEST_ASSERT(IsMouseInPointerEnabled() == TRUE_VAL, "EnableMouseInPointer must set active state");
    EnableMouseInPointer(origMouseMode);

    // 5. Multi-Touch Contact Event Injection & Spatial Geometry
    POINTER_TOUCH_INFO touch{};
    touch.pointerInfo.pointerId = 501;
    touch.pointerInfo.frameId = PointerSubsystemManager::get().NextFrameId();
    touch.pointerInfo.pointerType = PT_TOUCH;
    touch.pointerInfo.pointerFlags = POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT | POINTER_FLAG_PRIMARY | POINTER_FLAG_DOWN;
    touch.pointerInfo.ptPixelLocation = { 800, 600 };
    touch.rcContact = { 790, 590, 810, 610 };
    touch.pressure = 800;
    touch.orientation = 90;
    PointerSubsystemManager::get().InjectTouch(touch);

    POINTER_INFO qPointer{};
    bRes = GetPointerInfo(501, &qPointer);
    TEST_ASSERT(bRes == TRUE_VAL && qPointer.pointerId == 501, "GetPointerInfo for touch contact must succeed");
    TEST_ASSERT(qPointer.ptPixelLocation.x == 800 && qPointer.ptPixelLocation.y == 600, "Touch location must match (800, 600)");
    TEST_ASSERT(qPointer.pointerType == PT_TOUCH, "Pointer type must be PT_TOUCH");

    POINTER_TOUCH_INFO qTouch{};
    bRes = GetPointerTouchInfo(501, &qTouch);
    TEST_ASSERT(bRes == TRUE_VAL, "GetPointerTouchInfo must succeed");
    TEST_ASSERT(qTouch.rcContact.left == 790 && qTouch.rcContact.right == 810, "Contact bounding rect must match (790, 810)");
    TEST_ASSERT(qTouch.pressure == 800, "Touch pressure must match 800");

    // 6. Stylus / Pen Inking Injection with 4096 Pressure Levels & Tilt
    POINTER_PEN_INFO pen{};
    pen.pointerInfo.pointerId = 602;
    pen.pointerInfo.frameId = PointerSubsystemManager::get().NextFrameId();
    pen.pointerInfo.pointerType = PT_PEN;
    pen.pointerInfo.pointerFlags = POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT | POINTER_FLAG_FIRSTBUTTON | POINTER_FLAG_UPDATE;
    pen.pointerInfo.ptPixelLocation = { 960, 540 };
    pen.penFlags = PEN_FLAG_BARREL;
    pen.pressure = 3584; // 87.5% of 4096 pressure
    pen.rotation = 180;
    pen.tiltX = 20;
    pen.tiltY = -12;
    PointerSubsystemManager::get().InjectPen(pen);

    POINTER_PEN_INFO qPen{};
    bRes = GetPointerPenInfo(602, &qPen);
    TEST_ASSERT(bRes == TRUE_VAL && qPen.pressure == 3584, "GetPointerPenInfo pressure must match 3584");
    TEST_ASSERT(qPen.tiltX == 20 && qPen.tiltY == -12, "Pen tilt angles must match (20, -12)");
    TEST_ASSERT((qPen.penFlags & PEN_FLAG_BARREL) != 0, "Barrel button flag must be asserted");

    uint32_t penType = 0;
    GetPointerType(602, &penType);
    TEST_ASSERT(penType == PT_PEN, "Pointer 602 type must be PT_PEN");

    // 7. High-Rate Packet History Buffer
    uint32_t histCount = 8;
    POINTER_INFO histBuffer[8]{};
    bRes = GetPointerInfoHistory(501, &histCount, histBuffer);
    TEST_ASSERT(bRes == TRUE_VAL && histCount >= 1, "GetPointerInfoHistory must retrieve history packets");

    // 8. Device Surface Rect Mapping
    user32::RECT devRect{}, dispRect{};
    bRes = GetPointerDeviceRects(nullptr, &devRect, &dispRect);
    TEST_ASSERT(bRes == TRUE_VAL && devRect.right == 1920 && dispRect.bottom == 1080,
                "Pointer device display rect must match 1920x1080");

    // 9. Input Target Registration Lifecycle
    bRes = RegisterPointerInputTarget(nullptr, PT_TOUCH);
    TEST_ASSERT(bRes == TRUE_VAL, "RegisterPointerInputTarget must succeed");
    bRes = UnregisterPointerInputTarget(nullptr, PT_TOUCH);
    TEST_ASSERT(bRes == TRUE_VAL, "UnregisterPointerInputTarget must succeed");

    // 10. WinRT Windows.UI.Input.PointerPoint Object Model
    auto* pProps = new PointerPointPropertiesImpl(0.85f, true, { 200, 200, 230, 230 }, 18, -6);
    auto* pPoint = new PointerPointImpl(701, 100, { 1024, 768 }, PT_PEN, pProps);

    IPointerPoint* pQueryPoint = nullptr;
    int32_t hr = pPoint->QueryInterface(IID_IPointerPoint, reinterpret_cast<void**>(&pQueryPoint));
    TEST_ASSERT(hr == 0 && pQueryPoint != nullptr, "QueryInterface for IPointerPoint must succeed");
    TEST_ASSERT(pQueryPoint->GetPointerId() == 701, "PointerPoint ID must match 701");
    TEST_ASSERT(pQueryPoint->GetPosition().x == 1024 && pQueryPoint->GetPosition().y == 768, "Position must match (1024, 768)");
    TEST_ASSERT(pQueryPoint->GetPointerDeviceType() == PT_PEN, "Device type must match PT_PEN");

    IPointerPointProperties* pReadProps = pQueryPoint->GetProperties();
    TEST_ASSERT(pReadProps != nullptr, "PointerPoint must contain valid IPointerPointProperties");
    TEST_ASSERT(std::abs(pReadProps->GetPressure() - 0.85f) < 1e-4f, "Pressure must match 0.85");
    TEST_ASSERT(pReadProps->GetTiltX() == 18 && pReadProps->GetTiltY() == -6, "Tilt angles must match (18, -6)");

    pQueryPoint->Release();
    pPoint->Release();
    pProps->Release();

    // 11. Interactive Shell Verification (pointer test, pointer info, pointer inject)
    shell::CommandShell shellEngine;
    std::ostringstream testOut;
    int rc = shellEngine.execute("pointer test", testOut);
    TEST_ASSERT(rc == 0, "pointer test CLI command must return 0");
    TEST_ASSERT(testOut.str().find("10 / 10 Subsystem Invariants Verified") != std::string::npos,
                "pointer test must verify all 10 invariants");

    std::ostringstream infoOut;
    rc = shellEngine.execute("pointer info", infoOut);
    TEST_ASSERT(rc == 0, "pointer info CLI command must return 0");
    TEST_ASSERT(infoOut.str().find("Windows Pointer Device Subsystem") != std::string::npos,
                "pointer info must display subsystem telemetry");

    std::ostringstream injectOut;
    rc = shellEngine.execute("pointer inject", injectOut);
    TEST_ASSERT(rc == 0, "pointer inject CLI command must return 0");
    TEST_ASSERT(injectOut.str().find("Multi-Touch Gesture Dispatched") != std::string::npos,
                "pointer inject must dispatch multi-touch gesture packets");

    std::cout << "[TEST] Suite 120: Windows Pointer Device & Modern Touch/Inking Subsystem PASSED.\n";
}

void Test_WindowsAppModel_Lifecycle_Subsystem() {
    using namespace micant::appmodel;
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 121: Windows AppModel & Modern Application Lifecycle Management \n";
    std::cout << "========================================================================\n";

    // 1. Initialize Dynamic Exports & VersionDatabase
    InitializeAppModelExports();
    auto& loader = micant::ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("kernelbase.dll", "GetCurrentPackageFullName") != nullptr, "kernelbase GetCurrentPackageFullName must exist");
    TEST_ASSERT(loader.getExport("twinapi.appcore.dll", "PlmSuspendApplication") != nullptr, "twinapi.appcore PlmSuspendApplication must exist");
    TEST_ASSERT(loader.getExport("appxdeploymentclient.dll", "AppxRegisterPackage") != nullptr, "appxdeploymentclient AppxRegisterPackage must exist");

    // 2. Base32 Publisher ID Digest Verification
    std::string pubId1 = ComputePublisherId("CN=MicaNT Sovereign Project");
    TEST_ASSERT(pubId1.length() == 13, "PublisherId must be exactly 13 characters long");
    std::string pubId2 = ComputePublisherId("CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US");
    TEST_ASSERT(pubId2.length() == 13, "PublisherId for Microsoft must be 13 characters");

    // 3. Package Identity Structs & Synthesis
    AppxPackageManifest manifest;
    manifest.name = "MicaNT.VirtualCanvas";
    manifest.publisher = "CN=MicaNT Sovereign Project";
    manifest.publisherId = pubId1;
    manifest.version = { .Version = 0x0001000200030004ULL }; // 1.2.3.4
    manifest.architecture = PROCESSOR_ARCHITECTURE_AMD64_VAL;
    manifest.resourceId = "";

    std::string fullName = manifest.GetPackageFullName();
    std::string familyName = manifest.GetPackageFamilyName();
    std::string aumid = manifest.GetAUMID("CanvasApp");

    TEST_ASSERT(fullName == ("MicaNT.VirtualCanvas_1.2.3.4_x64__" + pubId1), "Full name synthesis must follow standard formatting");
    TEST_ASSERT(familyName == ("MicaNT.VirtualCanvas_" + pubId1), "Family name synthesis must follow standard formatting");
    TEST_ASSERT(aumid == ("MicaNT.VirtualCanvas_" + pubId1 + "!CanvasApp"), "AUMID must follow FamilyName!AppId format");

    // 4. AppX / MSIX Manifest XML Parsing
    const std::string manifestXml =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<Package xmlns=\"http://schemas.microsoft.com/appx/manifest/foundation/windows10\">\n"
        "  <Identity Name=\"Contoso.PhotoStudio\" Version=\"2.4.100.0\" Publisher=\"CN=Contoso Software\" ProcessorArchitecture=\"x64\"/>\n"
        "  <Properties>\n"
        "    <DisplayName>Contoso Photo Studio</DisplayName>\n"
        "    <PublisherDisplayName>Contoso Software Inc.</PublisherDisplayName>\n"
        "    <Description>Professional Sovereign Photo Editing</Description>\n"
        "    <Logo>Assets\\StoreLogo.png</Logo>\n"
        "  </Properties>\n"
        "  <Dependencies>\n"
        "    <TargetDeviceFamily Name=\"Windows.Desktop\" MinVersion=\"10.0.19041.0\" MaxVersionTested=\"10.0.22621.0\"/>\n"
        "  </Dependencies>\n"
        "  <Capabilities>\n"
        "    <Capability Name=\"internetClient\"/>\n"
        "    <Capability Name=\"picturesLibrary\"/>\n"
        "    <rescap:Capability Name=\"runFullTrust\"/>\n"
        "  </Capabilities>\n"
        "  <Applications>\n"
        "    <Application Id=\"PhotoApp\" Executable=\"PhotoStudio.exe\" EntryPoint=\"Contoso.PhotoStudio.App\">\n"
        "      <uap:VisualElements DisplayName=\"Contoso Photo Studio\" Square150x150Logo=\"Assets\\Logo150.png\" Square44x44Logo=\"Assets\\Logo44.png\" BackgroundColor=\"#1A1A1A\"/>\n"
        "    </Application>\n"
        "  </Applications>\n"
        "</Package>";

    AppxPackageManifest parsed;
    bool parseSuccess = AppxManifestParser::Parse(manifestXml, parsed);
    TEST_ASSERT(parseSuccess, "AppxManifestParser::Parse must succeed");
    TEST_ASSERT(parsed.name == "Contoso.PhotoStudio", "Manifest package name must match");
    TEST_ASSERT(parsed.version.Major == 2 && parsed.version.Minor == 4 && parsed.version.Build == 100 && parsed.version.Revision == 0,
                "Manifest version must parse accurately to 2.4.100.0");
    TEST_ASSERT(parsed.architecture == PROCESSOR_ARCHITECTURE_AMD64_VAL, "Architecture must parse to x64");
    TEST_ASSERT(parsed.capabilities.size() == 3, "All 3 declared capabilities must be extracted");
    TEST_ASSERT(parsed.applications.size() == 1, "Declared Application must be parsed");
    TEST_ASSERT(parsed.applications[0].id == "PhotoApp" && parsed.applications[0].executable == "PhotoStudio.exe",
                "Application ID and Executable must match manifest");

    // 5. Package Catalog Registration & Query
    std::string registeredFullName;
    bool regOk = AppModelCatalog::get().RegisterPackageXml(manifestXml, "C:\\Program Files\\WindowsApps\\Contoso.PhotoStudio_2.4.100.0_x64__test", registeredFullName);
    TEST_ASSERT(regOk, "RegisterPackageXml must succeed");

    InstalledPackage qPkg;
    bool foundFull = AppModelCatalog::get().FindPackageByFullName(registeredFullName, qPkg);
    TEST_ASSERT(foundFull, "FindPackageByFullName must locate registered package");
    TEST_ASSERT(qPkg.manifest.displayName == "Contoso Photo Studio", "Installed package display name must match");

    InstalledPackage qFamPkg;
    bool foundFam = AppModelCatalog::get().FindPackageByFamilyName(qPkg.packageFamilyName, qFamPkg);
    TEST_ASSERT(foundFam && qFamPkg.packageFullName == registeredFullName, "FindPackageByFamilyName must resolve correctly");

    InstalledPackage qAumidPkg;
    bool foundAumid = AppModelCatalog::get().FindPackageByAUMID(qPkg.aumid, qAumidPkg);
    TEST_ASSERT(foundAumid && qAumidPkg.packageFullName == registeredFullName, "FindPackageByAUMID must resolve correctly");

    // 6. Win32 Package Identity API Verification (kernelbase.dll)
    wchar_t fnBuffer[256]{};
    uint32_t fnLen = 256;
    LONG r = GetCurrentPackageFullName(&fnLen, fnBuffer);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL && fnLen > 0, "GetCurrentPackageFullName must succeed for active process");

    wchar_t famBuffer[256]{};
    uint32_t famLen = 256;
    r = GetCurrentPackageFamilyName(&famLen, famBuffer);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL && famLen > 0, "GetCurrentPackageFamilyName must succeed");

    wchar_t pathBuffer[512]{};
    uint32_t pathLen = 512;
    r = GetCurrentPackagePath(&pathLen, pathBuffer);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL && pathLen > 0, "GetCurrentPackagePath must succeed");

    // Query path by full name
    std::wstring wRegFn = Utf8ToWide(registeredFullName);
    wchar_t qPathBuf[512]{};
    uint32_t qpLen = 512;
    r = GetPackagePathByFullName(wRegFn.c_str(), &qpLen, qPathBuf);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL && std::wstring(qPathBuf).find(L"Contoso.PhotoStudio") != std::wstring::npos,
                "GetPackagePathByFullName must return correct installation path");

    // Derived family name from full name
    wchar_t derivedFam[256]{};
    uint32_t dfLen = 256;
    r = PackageFamilyNameFromFullName(wRegFn.c_str(), &dfLen, derivedFam);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL && std::wstring(derivedFam) == Utf8ToWide(qPkg.packageFamilyName),
                "PackageFamilyNameFromFullName must extract exact family name");

    // MSIX package verification
    BOOL isMSIX = FALSE_VAL;
    r = CheckIsMSIXPackage(wRegFn.c_str(), &isMSIX);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL && isMSIX == TRUE_VAL, "CheckIsMSIXPackage must return TRUE for registered package");

    // 7. AppPolicy Modern Application Governance
    AppPolicyWindowingModel winModel{};
    AppPolicyProcessTerminationMethod termMethod{};
    AppPolicyThreadInitializationType threadInit{};
    AppPolicyShowDeveloperDiagnostic devDiag{};

    r = AppPolicyGetWindowingModel(nullptr, &winModel);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL && winModel == AppPolicyWindowingModel::Universal, "Windowing model must be Universal");

    r = AppPolicyGetProcessTerminationMethod(nullptr, &termMethod);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL && termMethod == AppPolicyProcessTerminationMethod::TerminateProcess,
                "Termination method must be TerminateProcess");

    r = AppPolicyGetThreadInitializationType(nullptr, &threadInit);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL && threadInit == AppPolicyThreadInitializationType::InitializeWinRT,
                "Thread init must be InitializeWinRT");

    r = AppPolicyGetShowDeveloperDiagnostic(nullptr, &devDiag);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL && devDiag == AppPolicyShowDeveloperDiagnostic::ShowUI,
                "Developer diagnostic policy must be ShowUI");

    // 8. Process Lifetime Management (PLM) Lifecycle State Machine
    uint32_t testPid = 9920;
    PlmManager::get().RegisterProcess(testPid, qPkg.aumid, registeredFullName);
    TEST_ASSERT(PlmManager::get().GetProcessState(testPid) == PlmApplicationState::Running, "Process must start in Running state");

    PlmApplicationState qState{};
    r = PlmGetApplicationState(testPid, &qState);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL && qState == PlmApplicationState::Running, "PlmGetApplicationState must query Running");

    // Suspend
    r = PlmSuspendApplication(testPid);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL, "PlmSuspendApplication must succeed");
    TEST_ASSERT(PlmManager::get().GetProcessState(testPid) == PlmApplicationState::Suspended, "Process must transition to Suspended");

    // Resume
    r = PlmResumeApplication(testPid);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL, "PlmResumeApplication must succeed");
    TEST_ASSERT(PlmManager::get().GetProcessState(testPid) == PlmApplicationState::Running, "Process must transition to Running");

    // 9. Extended Execution Grants & Revocation
    uint32_t token = 0;
    r = PlmRequestExtendedExecution(testPid, PlmExtendedExecutionReason::SavingData, 20, &token);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL && token != 0, "PlmRequestExtendedExecution must grant valid token");

    r = PlmRevokeExtendedExecution(testPid, token);
    TEST_ASSERT(r == ERROR_SUCCESS_VAL, "PlmRevokeExtendedExecution must successfully revoke token");

    // Terminate under memory pressure
    r = PlmTerminateApplication(testPid, "SystemLowMemory");
    TEST_ASSERT(r == ERROR_SUCCESS_VAL, "PlmTerminateApplication must terminate process");
    TEST_ASSERT(PlmManager::get().GetProcessState(testPid) == PlmApplicationState::Terminated, "Process must transition to Terminated");

    // 10. Interactive Shell Verification (appmodel test, appmodel info, appmodel list)
    shell::CommandShell shellEngine;
    std::ostringstream testOut;
    int rc = shellEngine.execute("appmodel test", testOut);
    TEST_ASSERT(rc == 0, "appmodel test CLI command must return 0");
    TEST_ASSERT(testOut.str().find("10 / 10 Subsystem Invariants Verified") != std::string::npos,
                "appmodel test must verify all 10 invariants");

    std::ostringstream infoOut;
    rc = shellEngine.execute("appmodel info", infoOut);
    TEST_ASSERT(rc == 0, "appmodel info CLI command must return 0");
    TEST_ASSERT(infoOut.str().find("AppModel, Package Identity & PLM Subsystem Telemetry") != std::string::npos,
                "appmodel info must display subsystem telemetry");

    std::ostringstream listOut;
    rc = shellEngine.execute("appmodel list", listOut);
    TEST_ASSERT(rc == 0, "appmodel list CLI command must return 0");
    TEST_ASSERT(listOut.str().find("MicaNT Installed Modern Package Catalog") != std::string::npos,
                "appmodel list must enumerate installed packages");

    // Cleanup
    AppModelCatalog::get().UnregisterPackage(registeredFullName);

    std::cout << "[TEST] Suite 121: Windows AppModel & Modern Application Lifecycle Management PASSED.\n";
}

void Test_WindowsDirect2D1_3_Typography_Subsystem() {
    using namespace micant::d2d1_3;
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 122: Direct2D 1.3 & DirectWrite Advanced Typography Subsystem    \n";
    std::cout << "========================================================================\n";

    // 1. Dynamic Exports & VersionDatabase Parity
    InitializeDirect2D1_3Exports();
    auto& loader = micant::ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("d2d1.dll", "D2D1CreateFactory3") != nullptr, "d2d1.dll D2D1CreateFactory3 export must exist");
    TEST_ASSERT(loader.getExport("dwrite.dll", "DWriteCreateTypography") != nullptr, "dwrite.dll DWriteCreateTypography export must exist");
    TEST_ASSERT(loader.getExport("dwrite.dll", "DWriteCreateFontFallback") != nullptr, "dwrite.dll DWriteCreateFontFallback export must exist");

    // 2. ID2D1Factory3 & ID2D1DeviceContext2 Creation
    CD2D1Factory3Impl factory;
    ID2D1DeviceContext2* pDC = nullptr;
    int32_t hr = factory.CreateDeviceContext2(&pDC);
    TEST_ASSERT(hr == 0 && pDC != nullptr, "CreateDeviceContext2 must succeed");

    ID2D1Factory3* pQueryFactory = nullptr;
    hr = factory.QueryInterface(IID_ID2D1Factory3, reinterpret_cast<void**>(&pQueryFactory));
    TEST_ASSERT(hr == 0 && pQueryFactory != nullptr, "QueryInterface for IID_ID2D1Factory3 must succeed");
    pQueryFactory->Release();

    // 3. ID2D1InkStyle (Nib Shapes & Transform)
    D2D1_INK_STYLE_PROPERTIES styleProps{};
    styleProps.nibShape = D2D1_INK_NIB_SHAPE_SQUARE;
    styleProps.nibTransform = { 1.5f, 0, 0, 1.5f, 0, 0 };
    ID2D1InkStyle* pStyle = nullptr;
    hr = factory.CreateInkStyle(&styleProps, &pStyle);
    TEST_ASSERT(hr == 0 && pStyle != nullptr, "CreateInkStyle must succeed");
    TEST_ASSERT(pStyle->GetNibShape() == D2D1_INK_NIB_SHAPE_SQUARE, "Nib shape must match D2D1_INK_NIB_SHAPE_SQUARE");

    d2d1::D2D1_MATRIX_3X2_F outXform{};
    pStyle->GetNibTransform(&outXform);
    TEST_ASSERT(outXform._11 == 1.5f && outXform._22 == 1.5f, "Nib transform scaling must match 1.5");

    // 4. ID2D1Ink (Bézier Segment Ingestion & Geometry Bounds)
    D2D1_INK_POINT startPt{ 50.0f, 50.0f, 2.0f };
    ID2D1Ink* pInk = nullptr;
    hr = pDC->CreateInk(&startPt, &pInk);
    TEST_ASSERT(hr == 0 && pInk != nullptr, "CreateInk must succeed");
    TEST_ASSERT(pInk->GetStartPoint().x == 50.0f && pInk->GetStartPoint().y == 50.0f, "Ink start point must match (50, 50)");

    D2D1_INK_BEZIER_SEGMENT segs[3] = {
        { { 70.0f, 90.0f, 2.5f }, { 110.0f, 130.0f, 3.5f }, { 150.0f, 150.0f, 3.0f } },
        { { 190.0f, 160.0f, 3.0f }, { 230.0f, 140.0f, 2.5f }, { 270.0f, 100.0f, 2.0f } },
        { { 300.0f, 80.0f, 1.5f }, { 330.0f, 70.0f, 1.2f }, { 360.0f, 60.0f, 1.0f } }
    };
    hr = pInk->AddSegments(segs, 3);
    TEST_ASSERT(hr == 0 && pInk->GetSegmentCount() == 3, "AddSegments must add 3 Bézier segments");

    d2d1::D2D1_RECT_F inkBounds{};
    hr = pInk->GetBounds(pStyle, nullptr, &inkBounds);
    TEST_ASSERT(hr == 0 && inkBounds.left <= 50.0f && inkBounds.right >= 360.0f, "Ink bounds must encompass all stroke segments");

    // 5. DrawInk Pipeline
    pDC->DrawInk(pInk, nullptr, pStyle);
    auto* pDCImpl = static_cast<CD2D1DeviceContext2Impl*>(pDC);
    TEST_ASSERT(pDCImpl->GetInkDrawCount() == 1, "DrawInk must increment ink draw counter");

    // 6. ID2D1SpriteBatch (High-Throughput 2D Rendering)
    ID2D1SpriteBatch* pBatch = nullptr;
    hr = pDC->CreateSpriteBatch(&pBatch);
    TEST_ASSERT(hr == 0 && pBatch != nullptr, "CreateSpriteBatch must succeed");

    std::vector<d2d1::D2D1_RECT_F> spriteRects(500);
    std::vector<d2d1::D2D1_COLOR_F> spriteColors(500);
    for (size_t i = 0; i < 500; ++i) {
        spriteRects[i] = { static_cast<float>(i * 4), static_cast<float>(i * 2), static_cast<float>(i * 4 + 16), static_cast<float>(i * 2 + 16) };
        spriteColors[i] = { 0.2f, 0.4f, 0.8f, 1.0f };
    }
    hr = pBatch->AddSprites(500, spriteRects.data(), nullptr, spriteColors.data(), nullptr, 0, 0, 0, 0);
    TEST_ASSERT(hr == 0 && pBatch->GetSpriteCount() == 500, "AddSprites must batch 500 sprites");

    pDC->DrawSpriteBatch(pBatch, 0, 500, nullptr, d2d1::D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, D2D1_SPRITE_OPTIONS_NONE);
    TEST_ASSERT(pDCImpl->GetSpriteBatchDrawCount() == 1, "DrawSpriteBatch must execute batched rendering");

    // 7. ID2D1GradientMesh (Bicubic Coons Patch)
    D2D1_GRADIENT_MESH_PATCH meshPatch{};
    meshPatch.point00 = { 0, 0 }; meshPatch.point03 = { 100, 0 };
    meshPatch.point30 = { 0, 100 }; meshPatch.point33 = { 100, 100 };
    meshPatch.color00 = { 1, 0, 0, 1 }; meshPatch.color03 = { 0, 1, 0, 1 };
    meshPatch.color30 = { 0, 0, 1, 1 }; meshPatch.color33 = { 1, 1, 1, 1 };

    ID2D1GradientMesh* pMesh = nullptr;
    hr = pDC->CreateGradientMesh(&meshPatch, 1, &pMesh);
    TEST_ASSERT(hr == 0 && pMesh != nullptr && pMesh->GetPatchCount() == 1, "CreateGradientMesh must succeed");
    pDC->DrawGradientMesh(pMesh);
    TEST_ASSERT(pDCImpl->GetGradientMeshDrawCount() == 1, "DrawGradientMesh must execute");

    // 8. ID2D1SvgDocument & ID2D1SvgElement (SVG DOM & Serialization)
    ID2D1SvgDocument* pSvg = nullptr;
    hr = pDC->CreateSvgDocument(nullptr, { 300.0f, 300.0f }, &pSvg);
    TEST_ASSERT(hr == 0 && pSvg != nullptr, "CreateSvgDocument must succeed");
    TEST_ASSERT(pSvg->GetViewportSize().width == 300.0f && pSvg->GetViewportSize().height == 300.0f, "Viewport size must match 300x300");

    ID2D1SvgElement* pSvgRoot = nullptr;
    pSvg->GetRoot(&pSvgRoot);
    TEST_ASSERT(pSvgRoot != nullptr, "GetRoot must return root SVG element");

    auto* pRectElem = new CD2D1SvgElementImpl(pSvg, L"rect");
    pRectElem->SetAttributeValue(L"id", L"bgRect");
    pRectElem->SetAttributeValue(L"x", L"10");
    pRectElem->SetAttributeValue(L"y", L"10");
    pRectElem->SetAttributeValue(L"width", L"280");
    pRectElem->SetAttributeValue(L"height", L"280");
    pRectElem->SetAttributeValue(L"fill", L"#1E1E1E");
    pSvgRoot->AppendChild(pRectElem);

    ID2D1SvgElement* pFoundElem = nullptr;
    hr = pSvg->FindElementById(L"bgRect", &pFoundElem);
    TEST_ASSERT(hr == 0 && pFoundElem != nullptr, "FindElementById must locate bgRect");

    std::string svgXml;
    pSvg->Serialize(svgXml);
    TEST_ASSERT(svgXml.find("<rect") != std::string::npos && svgXml.find("fill=\"#1E1E1E\"") != std::string::npos,
                "SVG serialization must contain child rect and attributes");

    pDC->DrawSvgDocument(pSvg);
    TEST_ASSERT(pDCImpl->GetSvgDrawCount() == 1, "DrawSvgDocument must execute");

    pFoundElem->Release();
    pRectElem->Release();
    pSvgRoot->Release();

    // 9. IDWriteTypography & OpenType Features
    IDWriteTypography* pTypo = nullptr;
    hr = DWriteCreateTypography(&pTypo);
    TEST_ASSERT(hr == 0 && pTypo != nullptr, "DWriteCreateTypography must succeed");

    pTypo->AddFontFeature({ DWRITE_FONT_FEATURE_TAG_KERNING, 1 });
    pTypo->AddFontFeature({ DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1 });
    pTypo->AddFontFeature({ DWRITE_FONT_FEATURE_TAG_SMALL_CAPITALS, 1 });
    pTypo->AddFontFeature({ DWRITE_FONT_FEATURE_TAG_OLD_STYLE_FIGURES, 1 });
    pTypo->AddFontFeature({ DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES, 1 });

    TEST_ASSERT(pTypo->GetFontFeatureCount() == 5, "Typography must contain 5 registered OpenType features");
    DWRITE_FONT_FEATURE featCheck{};
    pTypo->GetFontFeature(4, &featCheck);
    TEST_ASSERT(featCheck.nameTag == DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES, "Feature 4 must match tabular figures tag");

    // 10. IDWriteFontFallback Multi-Script Cascade
    IDWriteFontFallback* pFallback = nullptr;
    hr = DWriteCreateFontFallback(&pFallback);
    TEST_ASSERT(hr == 0 && pFallback != nullptr, "DWriteCreateFontFallback must succeed");

    std::wstring fontLatin, fontCJK, fontArabic, fontHangul;
    pFallback->MapCharacters(L"MicaNT Architecture", 19, L"en-US", fontLatin);
    pFallback->MapCharacters(L"\u65E5\u672C\u8A9E", 3, L"ja-JP", fontCJK);
    pFallback->MapCharacters(L"\u0633\u0644\u0627\u0645", 4, L"ar-SA", fontArabic);
    pFallback->MapCharacters(L"\uD55C\uAE00", 2, L"ko-KR", fontHangul);

    TEST_ASSERT(fontLatin == L"Segoe UI", "Latin must map to Segoe UI");
    TEST_ASSERT(fontCJK == L"Microsoft YaHei" || fontCJK == L"Meiryo", "CJK must map to appropriate East Asian font");
    TEST_ASSERT(fontArabic == L"Segoe UI Historic", "Arabic must map to Segoe UI Historic");
    TEST_ASSERT(fontHangul == L"Malgun Gothic", "Hangul must map to Malgun Gothic");

    // 11. Interactive Shell Verification (d2d13 test, d2d13 info, d2d13 demo)
    shell::CommandShell shellEngine;
    std::ostringstream testOut;
    int rc = shellEngine.execute("d2d13 test", testOut);
    TEST_ASSERT(rc == 0, "d2d13 test CLI command must return 0");
    TEST_ASSERT(testOut.str().find("10 / 10 Subsystem Invariants Verified") != std::string::npos,
                "d2d13 test must verify all 10 invariants");

    std::ostringstream infoOut;
    rc = shellEngine.execute("d2d13 info", infoOut);
    TEST_ASSERT(rc == 0, "d2d13 info CLI command must return 0");
    TEST_ASSERT(infoOut.str().find("Direct2D 1.3 & DirectWrite Typography Telemetry") != std::string::npos,
                "d2d13 info must display subsystem telemetry");

    std::ostringstream demoOut;
    rc = shellEngine.execute("d2d13 demo", demoOut);
    TEST_ASSERT(rc == 0, "d2d13 demo CLI command must return 0");
    TEST_ASSERT(demoOut.str().find("<path") != std::string::npos && demoOut.str().find("shieldPath") != std::string::npos,
                "d2d13 demo must render SVG vector shield");

    // Clean up
    pFallback->Release();
    pTypo->Release();
    pSvg->Release();
    pMesh->Release();
    pBatch->Release();
    pInk->Release();
    pStyle->Release();
    pDC->Release();

    std::cout << "[TEST] Suite 122: Direct2D 1.3 & DirectWrite Advanced Typography Subsystem PASSED.\n";
}

void Test_WindowsTextServices_IME_Subsystem() {
    using namespace micant::tsf;
    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 123: Windows Text Services Framework & Modern IME Subsystem     \n";
    std::cout << "========================================================================\n";

    // 1. Dynamic Exports & VersionDatabase Parity
    InitializeTextServicesExports();
    auto& loader = micant::ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("msctf.dll", "TF_CreateThreadMgr") != nullptr, "msctf.dll TF_CreateThreadMgr export must exist");
    TEST_ASSERT(loader.getExport("msctf.dll", "TF_CreateInputProcessorProfiles") != nullptr, "msctf.dll TF_CreateInputProcessorProfiles export must exist");
    TEST_ASSERT(loader.getExport("msctf.dll", "TF_CreateCategoryMgr") != nullptr, "msctf.dll TF_CreateCategoryMgr export must exist");
    TEST_ASSERT(loader.getExport("msctf.dll", "TF_GetGlobalCompartment") != nullptr, "msctf.dll TF_GetGlobalCompartment export must exist");

    TEST_ASSERT(loader.getExport("imm32.dll", "ImmGetContext") != nullptr, "imm32.dll ImmGetContext export must exist");
    TEST_ASSERT(loader.getExport("imm32.dll", "ImmReleaseContext") != nullptr, "imm32.dll ImmReleaseContext export must exist");
    TEST_ASSERT(loader.getExport("imm32.dll", "ImmCreateContext") != nullptr, "imm32.dll ImmCreateContext export must exist");
    TEST_ASSERT(loader.getExport("imm32.dll", "ImmDestroyContext") != nullptr, "imm32.dll ImmDestroyContext export must exist");
    TEST_ASSERT(loader.getExport("imm32.dll", "ImmGetCompositionStringW") != nullptr, "imm32.dll ImmGetCompositionStringW export must exist");
    TEST_ASSERT(loader.getExport("imm32.dll", "ImmSetCompositionStringW") != nullptr, "imm32.dll ImmSetCompositionStringW export must exist");
    TEST_ASSERT(loader.getExport("imm32.dll", "ImmGetCandidateListW") != nullptr, "imm32.dll ImmGetCandidateListW export must exist");

    const auto* pMsctfVer = version::VersionDatabase::Instance().GetModuleInfo("msctf.dll");
    TEST_ASSERT(pMsctfVer != nullptr, "msctf.dll must be registered in VersionDatabase");
    const auto* pImmVer = version::VersionDatabase::Instance().GetModuleInfo("imm32.dll");
    TEST_ASSERT(pImmVer != nullptr, "imm32.dll must be registered in VersionDatabase");

    // 2. ITfThreadMgr Activation & Document Management
    ITfThreadMgr* pThreadMgr = nullptr;
    int32_t hr = TF_CreateThreadMgr(&pThreadMgr);
    TEST_ASSERT(hr == 0 && pThreadMgr != nullptr, "TF_CreateThreadMgr must succeed");

    TfClientId clientId = 0;
    hr = pThreadMgr->Activate(&clientId);
    TEST_ASSERT(hr == 0 && clientId != 0, "ITfThreadMgr::Activate must return valid client ID");

    int32_t fFocus = 0;
    pThreadMgr->IsThreadFocus(&fFocus);
    TEST_ASSERT(fFocus == 1, "Thread focus must be true after Activate");

    ITfDocumentMgr* pDocMgr = nullptr;
    hr = pThreadMgr->CreateDocumentMgr(&pDocMgr);
    TEST_ASSERT(hr == 0 && pDocMgr != nullptr, "CreateDocumentMgr must succeed");

    // 3. ITfContext & Text Store Binding
    ITfContext* pContext = nullptr;
    TfEditCookie cookie = 0;
    hr = pDocMgr->CreateContext(clientId, 0, nullptr, &pContext, &cookie);
    TEST_ASSERT(hr == 0 && pContext != nullptr && cookie != 0, "CreateContext must succeed");

    hr = pDocMgr->Push(pContext);
    TEST_ASSERT(hr == 0, "Push context must succeed");

    ITfContext* pTopContext = nullptr;
    hr = pDocMgr->GetTop(&pTopContext);
    TEST_ASSERT(hr == 0 && pTopContext == pContext, "GetTop must return pushed context");
    pTopContext->Release();

    // 4. ITfEditSession & ITfRange Text Manipulation
    class CTestEditSession : public ITfEditSession {
    private:
        std::atomic<uint32_t> m_ref{ 1 };
        ITfContext* m_ctx{ nullptr };
    public:
        CTestEditSession(ITfContext* ctx) : m_ctx(ctx) {}
        int32_t __stdcall QueryInterface(const GUID& riid, void** ppv) override {
            if (!ppv) return ole32::E_POINTER;
            if (riid == ole32::IID_IUnknown || riid == IID_ITfEditSession) {
                *ppv = static_cast<ITfEditSession*>(this);
                AddRef();
                return ole32::S_OK;
            }
            *ppv = nullptr;
            return ole32::E_NOINTERFACE;
        }
        uint32_t __stdcall AddRef() override { return ++m_ref; }
        uint32_t __stdcall Release() override {
            uint32_t r = --m_ref;
            if (r == 0) delete this;
            return r;
        }
        int32_t __stdcall DoEditSession(TfEditCookie ec) override {
            ITfRange* pRange = nullptr;
            m_ctx->GetStart(ec, &pRange);
            const wchar_t* initialText = L"Hello, Sovereign OS!";
            pRange->SetText(ec, 0, initialText, static_cast<int32_t>(wcslen(initialText)));

            wchar_t readBuf[64]{};
            uint32_t cch = 0;
            pRange->GetText(ec, 0, readBuf, 64, &cch);
            if (wcscmp(readBuf, initialText) != 0) {
                pRange->Release();
                return ole32::E_FAIL;
            }

            int32_t anchor = 0, extent = 0;
            pRange->GetExtent(ec, &anchor, &extent);
            if (extent != static_cast<int32_t>(wcslen(initialText))) {
                pRange->Release();
                return ole32::E_FAIL;
            }

            // Clone and collapse
            ITfRange* pClone = nullptr;
            pRange->Clone(&pClone);
            pClone->Collapse(ec, TF_ANCHOR_END);
            int32_t cloneAnchor = 0, cloneExtent = 0;
            pClone->GetExtent(ec, &cloneAnchor, &cloneExtent);
            if (cloneAnchor != extent || cloneExtent != 0) {
                pClone->Release();
                pRange->Release();
                return ole32::E_FAIL;
            }
            pClone->Release();

            pRange->Release();
            return ole32::S_OK;
        }
    };

    auto* pSession = new CTestEditSession(pContext);
    int32_t sessionResult = 0;
    hr = pContext->RequestEditSession(clientId, pSession, 0, &sessionResult);
    TEST_ASSERT(hr == 0 && sessionResult == 0, "RequestEditSession must execute cleanly");
    pSession->Release();

    // 5. ITfCompartmentMgr & Compartments
    ITfCompartmentMgr* pCompMgr = nullptr;
    hr = pThreadMgr->GetGlobalCompartment(&pCompMgr);
    TEST_ASSERT(hr == 0 && pCompMgr != nullptr, "GetGlobalCompartment must succeed");

    ITfCompartment* pCompOpen = nullptr;
    hr = pCompMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &pCompOpen);
    TEST_ASSERT(hr == 0 && pCompOpen != nullptr, "GetCompartment for OpenClose must succeed");

    pCompOpen->SetValue(clientId, 1);
    uint32_t compVal = 0;
    pCompOpen->GetValue(&compVal);
    TEST_ASSERT(compVal == 1, "Compartment value must be 1 (Open)");
    pCompOpen->Release();
    pCompMgr->Release();

    // 6. ITfInputProcessorProfiles & Language Profiles
    ITfInputProcessorProfiles* pProfiles = nullptr;
    hr = TF_CreateInputProcessorProfiles(&pProfiles);
    TEST_ASSERT(hr == 0 && pProfiles != nullptr, "TF_CreateInputProcessorProfiles must succeed");

    uint16_t langId = 0;
    GUID profGuid{};
    GUID nullGuid{};
    hr = pProfiles->GetActiveLanguageProfile(nullGuid, &langId, &profGuid);
    TEST_ASSERT(hr == 0 && langId == 0x0409, "Active language profile must default to US English (0x0409)");
    pProfiles->Release();

    // 7. ITfCategoryMgr & GUID Mapping
    ITfCategoryMgr* pCatMgr = nullptr;
    hr = TF_CreateCategoryMgr(&pCatMgr);
    TEST_ASSERT(hr == 0 && pCatMgr != nullptr, "TF_CreateCategoryMgr must succeed");

    GUID testGuid = { 0x11223344, 0x5566, 0x7788, { 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00 } };
    TfGuidAtom atom = 0;
    hr = pCatMgr->RegisterGUID(testGuid, &atom);
    TEST_ASSERT(hr == 0 && atom != 0, "RegisterGUID must return valid atom");

    GUID lookupGuid{};
    hr = pCatMgr->GetGUID(atom, &lookupGuid);
    TEST_ASSERT(hr == 0 && lookupGuid == testGuid, "GetGUID must retrieve registered GUID from atom");
    pCatMgr->Release();

    // 8. ITfInputScope & Mobile / Soft-Keyboard Scopes
    std::vector<InputScope> scopes = { IS_DEFAULT, IS_URL, IS_EMAIL_SMTPADDRESS, IS_NUMERIC, IS_PASSWORD };
    ITfInputScope* pScope = nullptr;
    hr = TF_CreateInputScope(scopes, &pScope);
    TEST_ASSERT(hr == 0 && pScope != nullptr, "TF_CreateInputScope must succeed");

    InputScope* pRetScopes = nullptr;
    uint32_t scopeCount = 0;
    hr = pScope->GetInputScopes(&pRetScopes, &scopeCount);
    TEST_ASSERT(hr == 0 && scopeCount == 5 && pRetScopes != nullptr, "GetInputScopes must return 5 scopes");
    TEST_ASSERT(pRetScopes[1] == IS_URL && pRetScopes[4] == IS_PASSWORD, "Input scopes must match configured enums");
    ole32::CoTaskMemFree(pRetScopes);
    pScope->Release();

    // 9. Input Method Manager (IMM32) Context Lifecycle & Composition
    win32::HWND hTestWnd = reinterpret_cast<win32::HWND>(0x2001);
    HIMC hIMC = ImmGetContext(hTestWnd);
    TEST_ASSERT(hIMC != nullptr, "ImmGetContext must return valid HIMC handle");

    TEST_ASSERT(ImmGetOpenStatus(hIMC) == 1, "Default IME open status must be true");
    ImmSetOpenStatus(hIMC, 0);
    TEST_ASSERT(ImmGetOpenStatus(hIMC) == 0, "ImmSetOpenStatus(0) must update status to false");
    ImmSetOpenStatus(hIMC, 1);

    uint32_t convMode = 0, sentMode = 0;
    ImmGetConversionStatus(hIMC, &convMode, &sentMode);
    TEST_ASSERT((convMode & IME_CMODE_NATIVE) != 0, "Conversion status must include IME_CMODE_NATIVE");

    // Composition string update
    const wchar_t pinyin[] = L"ceshi";
    int32_t setRes = ImmSetCompositionStringW(hIMC, GCS_COMPSTR, pinyin, sizeof(pinyin) - sizeof(wchar_t), nullptr, 0);
    TEST_ASSERT(setRes == 1, "ImmSetCompositionStringW must succeed");

    wchar_t readComp[32]{};
    int32_t compBytes = ImmGetCompositionStringW(hIMC, GCS_COMPSTR, readComp, sizeof(readComp));
    TEST_ASSERT(compBytes == 5 * sizeof(wchar_t) && wcscmp(readComp, L"ceshi") == 0, "Composition string must be 'ceshi'");
    TEST_ASSERT(ImmGetCompositionStringW(hIMC, GCS_CURSORPOS, nullptr, 0) == 5, "Cursor position must be at offset 5");

    // Commit result string
    const wchar_t commitStr[] = L"测试";
    ImmSetCompositionStringW(hIMC, GCS_RESULTSTR, commitStr, sizeof(commitStr) - sizeof(wchar_t), nullptr, 0);
    wchar_t readResult[32]{};
    int32_t resultBytes = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, readResult, sizeof(readResult));
    TEST_ASSERT(resultBytes == 2 * sizeof(wchar_t) && wcscmp(readResult, L"测试") == 0, "Result string must match '测试'");

    // Candidate list testing
    auto* pContextState = CIMCManager::Instance().Lookup(hIMC);
    TEST_ASSERT(pContextState != nullptr, "CIMCContext must be found in manager");
    pContextState->candidates = { L"测试", L"侧室", L"策士", L"测视" };
    pContextState->candidateSelection = 0;

    uint32_t reqSize = ImmGetCandidateListW(hIMC, 0, nullptr, 0);
    TEST_ASSERT(reqSize > sizeof(CANDIDATELIST), "Candidate list size query must succeed");

    std::vector<uint8_t> candBuf(reqSize, 0);
    auto* pCandList = reinterpret_cast<CANDIDATELIST*>(candBuf.data());
    uint32_t fetched = ImmGetCandidateListW(hIMC, 0, pCandList, reqSize);
    TEST_ASSERT(fetched == reqSize && pCandList->dwCount == 4, "Candidate list must contain 4 candidates");

    const auto* firstCand = reinterpret_cast<const wchar_t*>(candBuf.data() + pCandList->dwOffset[0]);
    TEST_ASSERT(wcscmp(firstCand, L"测试") == 0, "First candidate must be '测试'");

    ImmReleaseContext(hTestWnd, hIMC);

    // 10. Shell CLI Verification
    shell::CommandShell shellEngine;
    std::ostringstream ssTest, ssInfo, ssCompose, ssCand;

    int rc = shellEngine.execute("tsf test", ssTest);
    TEST_ASSERT(rc == 0, "tsf test shell command must return 0");
    TEST_ASSERT(ssTest.str().find("Self-test passed cleanly") != std::string::npos, "tsf test shell command must succeed");

    rc = shellEngine.execute("tsf info", ssInfo);
    TEST_ASSERT(rc == 0, "tsf info shell command must return 0");
    TEST_ASSERT(ssInfo.str().find("msctf.dll") != std::string::npos, "tsf info shell command must output msctf.dll info");

    rc = shellEngine.execute("tsf compose hanyupinyin", ssCompose);
    TEST_ASSERT(rc == 0, "tsf compose shell command must return 0");
    TEST_ASSERT(ssCompose.str().find("hanyupinyin") != std::string::npos, "tsf compose shell command must display composition");

    rc = shellEngine.execute("tsf candidates nihao", ssCand);
    TEST_ASSERT(rc == 0, "tsf candidates shell command must return 0");
    TEST_ASSERT(ssCand.str().find("你好") != std::string::npos, "tsf candidates shell command must output candidate");

    // Cleanup
    pContext->Release();
    pDocMgr->Release();
    pThreadMgr->Deactivate();
    pThreadMgr->Release();

    std::cout << "[TEST] Suite 123: Windows Text Services Framework & Modern IME Subsystem PASSED.\n";
}

void Test_WindowsSpellCheck_Linguistic_Subsystem() {
    using namespace micant;
    using namespace micant::spellcheck;

    std::cout << "\n========================================================================\n";
    std::cout << "  Suite 124: Windows Spell Checking & Extended Linguistic Services      \n";
    std::cout << "========================================================================\n";

    InitializeSpellCheckExports();

    // 1. Dynamic Loader Exports Verification
    auto& loader = ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("spellcheck.dll", "SpellChecker_CreateFactory") != nullptr,
                "SpellChecker_CreateFactory must be exported from spellcheck.dll");
    TEST_ASSERT(loader.getExport("elscore.dll", "MappingGetServices") != nullptr,
                "MappingGetServices must be exported from elscore.dll");
    TEST_ASSERT(loader.getExport("elscore.dll", "MappingFreePropertyBag") != nullptr,
                "MappingFreePropertyBag must be exported from elscore.dll");
    TEST_ASSERT(loader.getExport("elscore.dll", "MappingRecognizeText") != nullptr,
                "MappingRecognizeText must be exported from elscore.dll");
    TEST_ASSERT(loader.getExport("elscore.dll", "MappingDoAction") != nullptr,
                "MappingDoAction must be exported from elscore.dll");

    // 2. VersionDatabase Verification
    const auto* spellMod = version::VersionDatabase::Instance().GetModuleInfo("spellcheck.dll");
    TEST_ASSERT(spellMod != nullptr && spellMod->stringTable.at("ProductVersion") == "10.0.22621.1",
                "spellcheck.dll must be registered in VersionDatabase at 10.0.22621.1");

    const auto* elsMod = version::VersionDatabase::Instance().GetModuleInfo("elscore.dll");
    TEST_ASSERT(elsMod != nullptr && elsMod->stringTable.at("ProductVersion") == "10.0.22621.1",
                "elscore.dll must be registered in VersionDatabase at 10.0.22621.1");

    // 3. ISpellCheckerFactory COM Activation & Supported Languages
    ISpellCheckerFactory* pFactory = nullptr;
    int32_t hr = SpellChecker_CreateFactory(&pFactory);
    TEST_ASSERT(hr == ole32::S_OK && pFactory != nullptr, "SpellChecker_CreateFactory must succeed");

    IEnumString* pLangs = nullptr;
    hr = pFactory->get_SupportedLanguages(&pLangs);
    TEST_ASSERT(hr == ole32::S_OK && pLangs != nullptr, "get_SupportedLanguages must succeed");

    wchar_t* langBuf[8]{};
    uint32_t langsFetched = 0;
    hr = pLangs->Next(8, langBuf, &langsFetched);
    TEST_ASSERT((hr == ole32::S_OK || hr == ole32::S_FALSE) && langsFetched >= 4, "Must enumerate at least 4 supported languages");

    bool hasEnUs = false;
    for (uint32_t i = 0; i < langsFetched; ++i) {
        if (langBuf[i]) {
            if (wcscmp(langBuf[i], L"en-US") == 0) hasEnUs = true;
            ole32::CoTaskMemFree(langBuf[i]);
        }
    }
    pLangs->Release();
    TEST_ASSERT(hasEnUs, "Supported languages must include en-US");

    int32_t isEnSupported = 0;
    pFactory->IsSupported(L"en-US", &isEnSupported);
    TEST_ASSERT(isEnSupported == 1, "en-US must be reported as supported");

    int32_t isInvalidSupported = 0;
    pFactory->IsSupported(L"xx-YY", &isInvalidSupported);
    TEST_ASSERT(isInvalidSupported == 0, "xx-YY must be reported as unsupported");

    // 4. ISpellChecker Creation & Metadata
    ISpellChecker* pChecker = nullptr;
    hr = pFactory->CreateSpellChecker(L"en-US", &pChecker);
    TEST_ASSERT(hr == ole32::S_OK && pChecker != nullptr, "CreateSpellChecker for en-US must succeed");

    wchar_t* pTag = nullptr;
    pChecker->get_LanguageTag(&pTag);
    TEST_ASSERT(pTag != nullptr && wcscmp(pTag, L"en-US") == 0, "Language tag must be en-US");
    ole32::CoTaskMemFree(pTag);

    wchar_t* pId = nullptr;
    pChecker->get_Id(&pId);
    TEST_ASSERT(pId != nullptr && wcsstr(pId, L"SpellChecker") != nullptr, "Spell checker ID must be populated");
    ole32::CoTaskMemFree(pId);

    // 5. Spell Checking & Error Enumeration
    const wchar_t* sampleText = L"The quick bron fox jumpd over teh lazy dog";
    IEnumSpellingError* pErrors = nullptr;
    hr = pChecker->Check(sampleText, &pErrors);
    TEST_ASSERT(hr == ole32::S_OK && pErrors != nullptr, "Check must return IEnumSpellingError");

    std::vector<std::pair<uint32_t, CORRECTIVE_ACTION>> detectedErrors;
    ISpellingError* pErr = nullptr;
    while (pErrors->Next(&pErr) == ole32::S_OK && pErr) {
        uint32_t start = 0, len = 0;
        CORRECTIVE_ACTION act = CORRECTIVE_ACTION_NONE;
        wchar_t* repl = nullptr;
        pErr->get_StartIndex(&start);
        pErr->get_Length(&len);
        pErr->get_CorrectiveAction(&act);
        pErr->get_Replacement(&repl);

        detectedErrors.push_back({ start, act });
        if (act == CORRECTIVE_ACTION_REPLACE) {
            TEST_ASSERT(repl != nullptr && wcscmp(repl, L"the") == 0, "Autocorrect 'teh' replacement must be 'the'");
        }
        if (repl) ole32::CoTaskMemFree(repl);
        pErr->Release();
    }
    pErrors->Release();

    TEST_ASSERT(detectedErrors.size() == 3, "Check must detect 3 errors ('bron', 'jumpd', 'teh')");
    TEST_ASSERT(detectedErrors[0].second == CORRECTIVE_ACTION_GET_SUGGESTIONS, "First error must require suggestions");
    TEST_ASSERT(detectedErrors[1].second == CORRECTIVE_ACTION_GET_SUGGESTIONS, "Second error must require suggestions");
    TEST_ASSERT(detectedErrors[2].second == CORRECTIVE_ACTION_REPLACE, "Third error 'teh' must be autocorrect replacement");

    // 6. Word Suggestions (Levenshtein & Soundex)
    IEnumString* pSuggs = nullptr;
    hr = pChecker->Suggest(L"speling", &pSuggs);
    TEST_ASSERT(hr == ole32::S_OK && pSuggs != nullptr, "Suggest must return IEnumString");

    wchar_t* suggList[8]{};
    uint32_t suggCount = 0;
    pSuggs->Next(8, suggList, &suggCount);
    TEST_ASSERT(suggCount > 0, "Must return at least 1 suggestion for 'speling'");

    bool foundSpelling = false;
    for (uint32_t i = 0; i < suggCount; ++i) {
        if (suggList[i]) {
            if (wcscmp(suggList[i], L"spelling") == 0) foundSpelling = true;
            ole32::CoTaskMemFree(suggList[i]);
        }
    }
    pSuggs->Release();
    TEST_ASSERT(foundSpelling, "Suggestions for 'speling' must include 'spelling'");

    // 7. User Dictionary Management (Add, Ignore, AutoCorrect)
    pChecker->Add(L"sovereignos");
    IEnumSpellingError* pErrAdd = nullptr;
    pChecker->Check(L"Running sovereignos kernel", &pErrAdd);
    ISpellingError* pOneErr = nullptr;
    TEST_ASSERT(pErrAdd->Next(&pOneErr) == ole32::S_FALSE, "Added word 'sovereignos' must not be flagged");
    pErrAdd->Release();

    pChecker->Ignore(L"antigravity");
    IEnumSpellingError* pErrIgn = nullptr;
    pChecker->Check(L"Running antigravity system", &pErrIgn);
    TEST_ASSERT(pErrIgn->Next(&pOneErr) == ole32::S_FALSE, "Ignored word 'antigravity' must not be flagged");
    pErrIgn->Release();

    pChecker->AutoCorrect(L"mican", L"MicaNT");
    IEnumSpellingError* pErrAc = nullptr;
    pChecker->Check(L"Testing mican system", &pErrAc);
    TEST_ASSERT(pErrAc->Next(&pOneErr) == ole32::S_OK && pOneErr != nullptr, "AutoCorrect 'mican' must be flagged");
    CORRECTIVE_ACTION acAct = CORRECTIVE_ACTION_NONE;
    wchar_t* acRepl = nullptr;
    pOneErr->get_CorrectiveAction(&acAct);
    pOneErr->get_Replacement(&acRepl);
    TEST_ASSERT(acAct == CORRECTIVE_ACTION_REPLACE, "Action must be REPLACE");
    TEST_ASSERT(acRepl != nullptr && wcscmp(acRepl, L"MicaNT") == 0, "Replacement must be 'MicaNT'");
    ole32::CoTaskMemFree(acRepl);
    pOneErr->Release();
    pErrAc->Release();

    // 8. Options Description
    IEnumString* pOptionIds = nullptr;
    hr = pChecker->get_OptionIds(&pOptionIds);
    TEST_ASSERT(hr == ole32::S_OK && pOptionIds != nullptr, "get_OptionIds must succeed");
    pOptionIds->Release();

    IOptionDescription* pOptDesc = nullptr;
    hr = pChecker->GetOptionDescription(L"ignore_uppercase", &pOptDesc);
    TEST_ASSERT(hr == ole32::S_OK && pOptDesc != nullptr, "GetOptionDescription must succeed");
    wchar_t* optHead = nullptr;
    pOptDesc->get_Heading(&optHead);
    TEST_ASSERT(optHead != nullptr && wcscmp(optHead, L"Ignore Uppercase Words") == 0, "Heading must match");
    ole32::CoTaskMemFree(optHead);
    pOptDesc->Release();

    // 9. Extended Linguistic Services (ELS) APIs
    MAPPING_SERVICE_INFO* pServices = nullptr;
    uint32_t servicesCount = 0;
    hr = MappingGetServices(nullptr, &pServices, &servicesCount);
    TEST_ASSERT(hr == ole32::S_OK && pServices != nullptr && servicesCount == 3,
                "MappingGetServices must return 3 ELS services");

    // ELS Script Detection
    MAPPING_PROPERTY_BAG bagScript{};
    const wchar_t* cyrlSample = L"Привет мир";
    hr = MappingRecognizeText(&pServices[1], cyrlSample, static_cast<uint32_t>(wcslen(cyrlSample)), 0, nullptr, &bagScript);
    TEST_ASSERT(hr == ole32::S_OK && bagScript.pDataResult != nullptr, "MappingRecognizeText for script must succeed");
    auto* scriptRes = static_cast<const wchar_t*>(bagScript.pDataResult);
    TEST_ASSERT(wcscmp(scriptRes, L"Cyrl") == 0, "Cyrillic sample must detect 'Cyrl' script");
    MappingFreePropertyBag(&bagScript);

    // ELS Language Detection
    MAPPING_PROPERTY_BAG bagLang{};
    const wchar_t* esSample = L"Buenos dias amigo que tal";
    hr = MappingRecognizeText(&pServices[0], esSample, static_cast<uint32_t>(wcslen(esSample)), 0, nullptr, &bagLang);
    TEST_ASSERT(hr == ole32::S_OK && bagLang.pDataResult != nullptr, "MappingRecognizeText for language must succeed");
    auto* langRes = static_cast<const wchar_t*>(bagLang.pDataResult);
    TEST_ASSERT(wcscmp(langRes, L"es") == 0, "Spanish sample must detect 'es' language");
    MappingFreePropertyBag(&bagLang);

    // ELS Transliteration
    MAPPING_PROPERTY_BAG bagTrans{};
    const wchar_t* ruSample = L"Москва";
    hr = MappingRecognizeText(&pServices[2], ruSample, static_cast<uint32_t>(wcslen(ruSample)), 0, nullptr, &bagTrans);
    TEST_ASSERT(hr == ole32::S_OK && bagTrans.pDataResult != nullptr, "MappingRecognizeText for transliteration must succeed");
    auto* transRes = static_cast<const wchar_t*>(bagTrans.pDataResult);
    TEST_ASSERT(wcscmp(transRes, L"Moskva") == 0, "Cyrillic 'Москва' must transliterate to 'Moskva'");
    MappingFreePropertyBag(&bagTrans);

    // 10. Interactive Shell CLI Verification
    shell::CommandShell shellEngine;
    std::ostringstream ssTest, ssInfo, ssCheck, ssSuggest, ssEls;

    int rc = shellEngine.execute("spell test", ssTest);
    TEST_ASSERT(rc == 0, "spell test shell command must return 0");
    TEST_ASSERT(ssTest.str().find("Self-test passed cleanly") != std::string::npos, "spell test must pass cleanly");

    rc = shellEngine.execute("spell info", ssInfo);
    TEST_ASSERT(rc == 0, "spell info shell command must return 0");
    TEST_ASSERT(ssInfo.str().find("spellcheck.dll") != std::string::npos, "spell info must output DLL information");

    rc = shellEngine.execute("spell check teh quik fox", ssCheck);
    TEST_ASSERT(rc == 0, "spell check shell command must return 0");
    TEST_ASSERT(ssCheck.str().find("Replace with \"the\"") != std::string::npos, "spell check must output autocorrect replacement");

    rc = shellEngine.execute("spell suggest speling", ssSuggest);
    TEST_ASSERT(rc == 0, "spell suggest shell command must return 0");
    TEST_ASSERT(ssSuggest.str().find("spelling") != std::string::npos, "spell suggest must propose 'spelling'");

    rc = shellEngine.execute("spell els translit Москва", ssEls);
    TEST_ASSERT(rc == 0, "spell els shell command must return 0");
    TEST_ASSERT(ssEls.str().find("Moskva") != std::string::npos, "spell els transliteration must display 'Moskva'");

    // Cleanup
    pChecker->Release();
    pFactory->Release();

    std::cout << "[TEST] Suite 124: Windows Spell Checking & Extended Linguistic Services PASSED.\n";
}

void Test_WindowsSpeech_SAPI_Subsystem() {
    std::cout << "[TEST] Running Suite 125: Windows Speech API (SAPI 5.4) & Voice Synthesis Subsystem...\n";

    using namespace micant::sapi;
    InitializeSapiSubsystemExports();

    // 1. Dynamic Exports Verification
    auto& loader = ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("sapi.dll", "SpEnumTokens") != nullptr,
                "SpEnumTokens must be exported from sapi.dll");
    TEST_ASSERT(loader.getExport("sapi.dll", "SpGetCategoryFromId") != nullptr,
                "SpGetCategoryFromId must be exported from sapi.dll");
    TEST_ASSERT(loader.getExport("sapi.dll", "SpCreateVoice") != nullptr,
                "SpCreateVoice must be exported from sapi.dll");
    TEST_ASSERT(loader.getExport("sapi.dll", "SpCreateStream") != nullptr,
                "SpCreateStream must be exported from sapi.dll");

    // 2. VersionDatabase Verification
    const auto* sapiMod = version::VersionDatabase::Instance().GetModuleInfo("sapi.dll");
    TEST_ASSERT(sapiMod != nullptr && sapiMod->stringTable.at("ProductVersion") == "10.0.22621.1",
                "sapi.dll must be registered in VersionDatabase at 10.0.22621.1");

    // 3. COM Activation & Interface Queries
    ISpVoice* pVoice = nullptr;
    int32_t hr = SpCreateVoice(&pVoice);
    TEST_ASSERT(hr == ole32::S_OK && pVoice != nullptr, "SpCreateVoice must succeed");

    ole32::IUnknown* pUnk = nullptr;
    hr = pVoice->QueryInterface(ole32::IID_IUnknown, reinterpret_cast<void**>(&pUnk));
    TEST_ASSERT(hr == ole32::S_OK && pUnk != nullptr, "ISpVoice must support IUnknown");
    pUnk->Release();

    // 4. Rate and Volume Controls
    int32_t origRate = 0;
    pVoice->GetRate(&origRate);
    TEST_ASSERT(origRate == 0, "Default rate must be 0");

    pVoice->SetRate(5);
    int32_t newRate = 0;
    pVoice->GetRate(&newRate);
    TEST_ASSERT(newRate == 5, "SetRate must update voice rate");

    uint16_t origVol = 0;
    pVoice->GetVolume(&origVol);
    TEST_ASSERT(origVol == 100, "Default volume must be 100%");

    pVoice->SetVolume(75);
    uint16_t newVol = 0;
    pVoice->GetVolume(&newVol);
    TEST_ASSERT(newVol == 75, "SetVolume must update voice volume");

    // 5. Voice Token Category & Token Enumeration
    IEnumSpObjectTokens* pEnum = nullptr;
    hr = SpEnumTokens(SPCAT_VOICES, nullptr, nullptr, &pEnum);
    TEST_ASSERT(hr == ole32::S_OK && pEnum != nullptr, "SpEnumTokens must succeed");

    uint32_t tokenCount = 0;
    hr = pEnum->GetCount(&tokenCount);
    TEST_ASSERT(hr == ole32::S_OK && tokenCount >= 4, "Must enumerate at least 4 default voices");

    bool hasDavid = false;
    bool hasZira = false;
    bool hasMark = false;
    bool hasHelena = false;

    for (uint32_t i = 0; i < tokenCount; ++i) {
        ISpObjectToken* pTok = nullptr;
        if (pEnum->Item(i, &pTok) == ole32::S_OK && pTok) {
            wchar_t* pName = nullptr;
            pTok->GetStringValue(L"Name", &pName);
            if (pName) {
                if (wcscmp(pName, L"MicaNT David") == 0) hasDavid = true;
                if (wcscmp(pName, L"MicaNT Zira") == 0) hasZira = true;
                if (wcscmp(pName, L"MicaNT Mark") == 0) hasMark = true;
                if (wcscmp(pName, L"MicaNT Helena") == 0) hasHelena = true;
                ole32::CoTaskMemFree(pName);
            }
            pTok->Release();
        }
    }
    pEnum->Release();

    TEST_ASSERT(hasDavid && hasZira && hasMark && hasHelena,
                "All 4 sovereign voices (David, Zira, Mark, Helena) must be enumerated");

    // 6. Voice Selection
    ISpObjectTokenCategory* pCat = nullptr;
    hr = SpGetCategoryFromId(SPCAT_VOICES, &pCat);
    TEST_ASSERT(hr == ole32::S_OK && pCat != nullptr, "SpGetCategoryFromId must succeed");

    IEnumSpObjectTokens* pCatEnum = nullptr;
    pCat->EnumTokens(nullptr, nullptr, &pCatEnum);
    TEST_ASSERT(pCatEnum != nullptr, "EnumTokens from category must succeed");

    ISpObjectToken* pZiraTok = nullptr;
    pCatEnum->Item(1, &pZiraTok);
    TEST_ASSERT(pZiraTok != nullptr, "Must fetch Zira token");
    hr = pVoice->SetVoice(pZiraTok);
    TEST_ASSERT(hr == ole32::S_OK, "SetVoice to Zira must succeed");

    ISpObjectToken* pActiveTok = nullptr;
    hr = pVoice->GetVoice(&pActiveTok);
    TEST_ASSERT(hr == ole32::S_OK && pActiveTok != nullptr, "GetVoice must return active token");
    wchar_t* pGen = nullptr;
    pActiveTok->GetStringValue(L"Gender", &pGen);
    TEST_ASSERT(pGen != nullptr && wcscmp(pGen, L"Female") == 0, "Active voice gender must be Female");
    ole32::CoTaskMemFree(pGen);
    pActiveTok->Release();
    pZiraTok->Release();
    pCatEnum->Release();
    pCat->Release();

    // 7. Output Stream Binding & Audio Waveform Synthesis
    ISpStream* pStream = nullptr;
    hr = SpCreateStream(&pStream);
    TEST_ASSERT(hr == ole32::S_OK && pStream != nullptr, "SpCreateStream must succeed");

    hr = pVoice->SetOutput(pStream, 1);
    TEST_ASSERT(hr == ole32::S_OK, "SetOutput to stream must succeed");

    uint32_t streamNum = 0;
    const wchar_t* speakPhrase = L"MicaNT sovereign audio synthesis active.";
    hr = pVoice->Speak(speakPhrase, SPF_DEFAULT, &streamNum);
    TEST_ASSERT(hr == ole32::S_OK && streamNum == 1, "Speak must synthesize and return stream #1");

    auto* pImplStream = dynamic_cast<CSpStreamImpl*>(pStream);
    TEST_ASSERT(pImplStream != nullptr, "Stream must cast to CSpStreamImpl");
    const auto& pcmBuf = pImplStream->GetBuffer();
    TEST_ASSERT(!pcmBuf.empty(), "Synthesized PCM buffer must not be empty");
    TEST_ASSERT(pcmBuf.size() > 5000, "Synthesized audio waveform must have substantial sample data");

    // Check status
    SPVOICESTATUS status{};
    hr = pVoice->GetStatus(&status, nullptr);
    TEST_ASSERT(hr == ole32::S_OK, "GetStatus must succeed");
    TEST_ASSERT(status.ulCurrentStreamNum == 1, "Current stream number must match");

    pStream->Release();

    // 8. SSML / XML Markup Synthesis
    ISpStream* pXmlStream = nullptr;
    hr = SpCreateStream(&pXmlStream);
    TEST_ASSERT(hr == ole32::S_OK && pXmlStream != nullptr, "SpCreateStream for XML must succeed");

    pVoice->SetOutput(pXmlStream, 1);
    const wchar_t* ssmlText = L"<pitch high>Speed</pitch> <silence/> <rate fast>Fast</rate>";
    hr = pVoice->Speak(ssmlText, SPF_IS_XML, &streamNum);
    TEST_ASSERT(hr == ole32::S_OK && streamNum == 2, "Speak with SSML must succeed");

    auto* pImplXml = dynamic_cast<CSpStreamImpl*>(pXmlStream);
    TEST_ASSERT(pImplXml != nullptr && !pImplXml->GetBuffer().empty(), "SSML buffer must contain synthesized audio");
    pXmlStream->Release();

    // 9. Command & Control Speech Recognition Grammar
    CSpRecoGrammarImpl grammar;
    hr = grammar.LoadCmdFromMemory(L"<grammar><rule id=\"cmd\"><item>start</item></rule></grammar>");
    TEST_ASSERT(hr == ole32::S_OK, "LoadCmdFromMemory must succeed");
    TEST_ASSERT(grammar.GetRuleCount() == 1, "Rule count must be 1");

    // 10. Interactive Shell CLI Verification
    shell::CommandShell shellEngine;
    std::ostringstream ssTest, ssInfo, ssVoices, ssSpeak, ssSsml;

    int rc = shellEngine.execute("sapi test", ssTest);
    TEST_ASSERT(rc == 0, "sapi test shell command must return 0");
    TEST_ASSERT(ssTest.str().find("Self-test passed cleanly") != std::string::npos, "sapi test must pass cleanly");

    rc = shellEngine.execute("sapi info", ssInfo);
    TEST_ASSERT(rc == 0, "sapi info shell command must return 0");
    TEST_ASSERT(ssInfo.str().find("sapi.dll") != std::string::npos, "sapi info must output DLL information");

    rc = shellEngine.execute("sapi voices", ssVoices);
    TEST_ASSERT(rc == 0, "sapi voices shell command must return 0");
    TEST_ASSERT(ssVoices.str().find("MicaNT David") != std::string::npos, "sapi voices must list David");
    TEST_ASSERT(ssVoices.str().find("MicaNT Zira") != std::string::npos, "sapi voices must list Zira");

    rc = shellEngine.execute("sapi speak hello world", ssSpeak);
    TEST_ASSERT(rc == 0, "sapi speak shell command must return 0");
    TEST_ASSERT(ssSpeak.str().find("Synthesized:") != std::string::npos, "sapi speak must report synthesis stats");

    rc = shellEngine.execute("sapi ssml <pitch high>hello</pitch>", ssSsml);
    TEST_ASSERT(rc == 0, "sapi ssml shell command must return 0");
    TEST_ASSERT(ssSsml.str().find("Synthesized:") != std::string::npos, "sapi ssml must report SSML synthesis stats");

    // Cleanup
    pVoice->Release();

    std::cout << "[TEST] Suite 125: Windows Speech API (SAPI 5.4) & Voice Synthesis Subsystem PASSED.\n";
}

void Test_WindowsMedia_OCR_Subsystem() {
    std::cout << "[TEST] Running Suite 126: Windows Optical Character Recognition (OCR) & Modern Media Vision Subsystem...\n";

    using namespace micant::ocr;
    InitializeOcrSubsystemExports();

    // 1. Dynamic Exports Verification (windows.media.ocr.dll)
    auto& loader = ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("windows.media.ocr.dll", "OcrCreateEngine") != nullptr,
                "OcrCreateEngine must be exported from windows.media.ocr.dll");
    TEST_ASSERT(loader.getExport("windows.media.ocr.dll", "OcrCreateSoftwareBitmap") != nullptr,
                "OcrCreateSoftwareBitmap must be exported from windows.media.ocr.dll");
    TEST_ASSERT(loader.getExport("windows.media.ocr.dll", "OcrGetAvailableLanguages") != nullptr,
                "OcrGetAvailableLanguages must be exported from windows.media.ocr.dll");
    TEST_ASSERT(loader.getExport("windows.media.ocr.dll", "OcrGetEngineStatics") != nullptr,
                "OcrGetEngineStatics must be exported from windows.media.ocr.dll");
    TEST_ASSERT(loader.getExport("windows.media.ocr.dll", "DllGetActivationFactory") != nullptr,
                "DllGetActivationFactory must be exported from windows.media.ocr.dll");

    // 2. VersionDatabase Verification
    const auto* ocrMod = version::VersionDatabase::Instance().GetModuleInfo("windows.media.ocr.dll");
    TEST_ASSERT(ocrMod != nullptr && ocrMod->stringTable.at("ProductVersion") == "10.0.22621.1",
                "windows.media.ocr.dll must be registered in VersionDatabase at 10.0.22621.1");

    // 3. IOcrEngineStatics Activation & Language Discovery
    IOcrEngineStatics* pStatics = nullptr;
    int32_t hr = OcrGetEngineStatics(&pStatics);
    TEST_ASSERT(hr == 0 && pStatics != nullptr, "OcrGetEngineStatics must return S_OK and valid pointer");

    uint32_t maxW = 0, maxH = 0;
    pStatics->GetMaxImageDimension(&maxW, &maxH);
    TEST_ASSERT(maxW == 4096 && maxH == 4096, "Max image dimensions must report 4096 x 4096");

    int32_t supported = 0;
    pStatics->IsLanguageSupported(L"en-US", &supported);
    TEST_ASSERT(supported == 1, "en-US must be reported as supported");
    pStatics->IsLanguageSupported(L"de-DE", &supported);
    TEST_ASSERT(supported == 1, "de-DE must be reported as supported");
    pStatics->IsLanguageSupported(L"es-ES", &supported);
    TEST_ASSERT(supported == 1, "es-ES must be reported as supported");
    pStatics->IsLanguageSupported(L"xx-YY", &supported);
    TEST_ASSERT(supported == 0, "Invalid language xx-YY must be reported as unsupported");

    wchar_t** ppLangs = nullptr;
    uint32_t langCount = 0;
    hr = pStatics->GetAvailableRecognizerLanguages(&ppLangs, &langCount);
    TEST_ASSERT(hr == 0 && ppLangs != nullptr && langCount == 9, "Must return 9 supported languages");
    for (uint32_t i = 0; i < langCount; ++i) {
        ole32::CoTaskMemFree(ppLangs[i]);
    }
    ole32::CoTaskMemFree(ppLangs);

    // 4. TryCreateFromLanguage & Engine Activation
    IOcrEngine* pInvalidEngine = reinterpret_cast<IOcrEngine*>(0x1);
    hr = pStatics->TryCreateFromLanguage(L"non-existent", &pInvalidEngine);
    TEST_ASSERT(hr == 0 && pInvalidEngine == nullptr, "TryCreateFromLanguage with unsupported tag must yield nullptr");

    IOcrEngine* pEngine = nullptr;
    hr = pStatics->TryCreateFromLanguage(L"en-US", &pEngine);
    TEST_ASSERT(hr == 0 && pEngine != nullptr, "TryCreateFromLanguage for en-US must succeed");
    pStatics->Release();

    wchar_t wEngineLang[64]{};
    uint32_t wEngineLen = 64;
    pEngine->GetRecognizerLanguage(wEngineLang, &wEngineLen);
    TEST_ASSERT(std::wcscmp(wEngineLang, L"en-US") == 0, "Engine language must match en-US");

    // 5. SoftwareBitmap Creation & Pixel Manipulation
    ISoftwareBitmap* pBitmap = nullptr;
    hr = OcrCreateSoftwareBitmap(160, 48, BitmapPixelFormat_Bgra8, nullptr, &pBitmap);
    TEST_ASSERT(hr == 0 && pBitmap != nullptr, "OcrCreateSoftwareBitmap must succeed");
    TEST_ASSERT(pBitmap->GetWidth() == 160 && pBitmap->GetHeight() == 48, "Bitmap dimensions must match");
    TEST_ASSERT(pBitmap->GetBitmapPixelFormat() == BitmapPixelFormat_Bgra8, "Format must match Bgra8");

    // Fill white background
    for (uint32_t y = 0; y < 48; ++y) {
        for (uint32_t x = 0; x < 160; ++x) {
            pBitmap->SetPixel(x, y, 0xFFFFFFFF);
        }
    }
    TEST_ASSERT(pBitmap->GetPixel(0, 0) == 0xFFFFFFFF, "Pixel at (0,0) must be white");

    // Draw single word "MICANT"
    pBitmap->DrawString(16, 16, "MICANT", 0xFF000000, 1);

    // 6. RecognizeText on Single Word
    IOcrResult* pResult1 = nullptr;
    hr = pEngine->RecognizeText(pBitmap, &pResult1);
    TEST_ASSERT(hr == 0 && pResult1 != nullptr, "RecognizeText must return S_OK and valid IOcrResult");

    wchar_t wText1[256]{};
    uint32_t t1Len = 256;
    pResult1->GetText(wText1, &t1Len);
    TEST_ASSERT(std::wcscmp(wText1, L"MICANT") == 0, "Recognized text must match 'MICANT'");

    IOcrLine** ppLines1 = nullptr;
    uint32_t lineCount1 = 0;
    pResult1->GetLines(&ppLines1, &lineCount1);
    TEST_ASSERT(lineCount1 == 1 && ppLines1 != nullptr, "Single word image must produce 1 line");

    IOcrLine* pLine1 = ppLines1[0];
    OcrRect lRect1{};
    pLine1->GetBoundingRect(&lRect1);
    TEST_ASSERT(lRect1.x >= 14.0f && lRect1.x <= 18.0f, "Line X bounding box must enclose text start");
    TEST_ASSERT(lRect1.width >= 40.0f && lRect1.width <= 56.0f, "Line width must match character extent");

    IOcrWord** ppWords1 = nullptr;
    uint32_t wordCount1 = 0;
    pLine1->GetWords(&ppWords1, &wordCount1);
    TEST_ASSERT(wordCount1 == 1 && ppWords1 != nullptr, "Line must contain 1 word");

    IOcrWord* pWord1 = ppWords1[0];
    wchar_t wWord1[64]{};
    uint32_t wLen1 = 64;
    pWord1->GetText(wWord1, &wLen1);
    TEST_ASSERT(std::wcscmp(wWord1, L"MICANT") == 0, "Word text must be 'MICANT'");
    float conf1 = 0.0f;
    pWord1->GetConfidence(&conf1);
    TEST_ASSERT(conf1 > 0.85f, "Word recognition confidence must be > 85%");

    // Cleanup first recognition
    pWord1->Release();
    ole32::CoTaskMemFree(ppWords1);
    pLine1->Release();
    ole32::CoTaskMemFree(ppLines1);
    pResult1->Release();
    pBitmap->Release();

    // 7. RecognizeText on Multi-Word Sentence ("SOVEREIGN OS 2026")
    ISoftwareBitmap* pBitmap2 = nullptr;
    OcrCreateSoftwareBitmap(240, 48, BitmapPixelFormat_Bgra8, nullptr, &pBitmap2);
    TEST_ASSERT(pBitmap2 != nullptr, "Second bitmap creation must succeed");
    for (uint32_t y = 0; y < 48; ++y) {
        for (uint32_t x = 0; x < 240; ++x) {
            pBitmap2->SetPixel(x, y, 0xFFFFFFFF);
        }
    }
    pBitmap2->DrawString(16, 16, "SOVEREIGN OS 2026", 0xFF000000, 1);

    IOcrResult* pResult2 = nullptr;
    hr = pEngine->RecognizeText(pBitmap2, &pResult2);
    TEST_ASSERT(hr == 0 && pResult2 != nullptr, "Multi-word RecognizeText must succeed");

    wchar_t wText2[256]{};
    uint32_t t2Len = 256;
    pResult2->GetText(wText2, &t2Len);
    TEST_ASSERT(std::wcscmp(wText2, L"SOVEREIGN OS 2026") == 0,
                "Extracted text must match 'SOVEREIGN OS 2026'");

    IOcrLine** ppLines2 = nullptr;
    uint32_t lineCount2 = 0;
    pResult2->GetLines(&ppLines2, &lineCount2);
    TEST_ASSERT(lineCount2 == 1, "Must contain 1 line");

    IOcrWord** ppWords2 = nullptr;
    uint32_t wordCount2 = 0;
    ppLines2[0]->GetWords(&ppWords2, &wordCount2);
    TEST_ASSERT(wordCount2 == 3, "Sentence must be segmented into 3 words");

    wchar_t wWordBuf[64]{};
    uint32_t wbLen = 64;
    ppWords2[0]->GetText(wWordBuf, &wbLen);
    TEST_ASSERT(std::wcscmp(wWordBuf, L"SOVEREIGN") == 0, "Word 1 must be 'SOVEREIGN'");

    wbLen = 64;
    ppWords2[1]->GetText(wWordBuf, &wbLen);
    TEST_ASSERT(std::wcscmp(wWordBuf, L"OS") == 0, "Word 2 must be 'OS'");

    wbLen = 64;
    ppWords2[2]->GetText(wWordBuf, &wbLen);
    TEST_ASSERT(std::wcscmp(wWordBuf, L"2026") == 0, "Word 3 must be '2026'");

    for (uint32_t i = 0; i < wordCount2; ++i) ppWords2[i]->Release();
    ole32::CoTaskMemFree(ppWords2);
    ppLines2[0]->Release();
    ole32::CoTaskMemFree(ppLines2);
    pResult2->Release();
    pBitmap2->Release();

    // 8. Multi-Line Text Recognition ("HELLO WORLD\nMICANT")
    ISoftwareBitmap* pBitmap3 = nullptr;
    OcrCreateSoftwareBitmap(180, 64, BitmapPixelFormat_Bgra8, nullptr, &pBitmap3);
    for (uint32_t y = 0; y < 64; ++y) {
        for (uint32_t x = 0; x < 180; ++x) {
            pBitmap3->SetPixel(x, y, 0xFFFFFFFF);
        }
    }
    pBitmap3->DrawString(16, 12, "HELLO WORLD\nMICANT", 0xFF000000, 1);

    IOcrResult* pResult3 = nullptr;
    hr = pEngine->RecognizeText(pBitmap3, &pResult3);
    TEST_ASSERT(hr == 0 && pResult3 != nullptr, "Multi-line RecognizeText must succeed");

    wchar_t wText3[256]{};
    uint32_t t3Len = 256;
    pResult3->GetText(wText3, &t3Len);
    TEST_ASSERT(std::wcscmp(wText3, L"HELLO WORLD\nMICANT") == 0,
                "Multi-line extracted text must match 'HELLO WORLD\\nMICANT'");

    IOcrLine** ppLines3 = nullptr;
    uint32_t lineCount3 = 0;
    pResult3->GetLines(&ppLines3, &lineCount3);
    TEST_ASSERT(lineCount3 == 2, "Multi-line image must be segmented into 2 lines");

    for (uint32_t i = 0; i < lineCount3; ++i) ppLines3[i]->Release();
    ole32::CoTaskMemFree(ppLines3);
    pResult3->Release();
    pBitmap3->Release();

    // 9. Inverted Polarity (White text on black background)
    ISoftwareBitmap* pBitmap4 = nullptr;
    OcrCreateSoftwareBitmap(128, 32, BitmapPixelFormat_Bgra8, nullptr, &pBitmap4);
    for (uint32_t y = 0; y < 32; ++y) {
        for (uint32_t x = 0; x < 128; ++x) {
            pBitmap4->SetPixel(x, y, 0xFF000000);
        }
    }
    pBitmap4->DrawString(16, 8, "DARK 42", 0xFFFFFFFF, 1);

    IOcrResult* pResult4 = nullptr;
    hr = pEngine->RecognizeText(pBitmap4, &pResult4);
    TEST_ASSERT(hr == 0 && pResult4 != nullptr, "Inverted polarity RecognizeText must succeed");

    wchar_t wText4[128]{};
    uint32_t t4Len = 128;
    pResult4->GetText(wText4, &t4Len);
    TEST_ASSERT(std::wcscmp(wText4, L"DARK 42") == 0,
                "Inverted polarity text must be recognized as 'DARK 42'");
    pResult4->Release();
    pBitmap4->Release();

    // 10. WinRT Activation Factory Verification (DllGetActivationFactory)
    void* pActFactory = nullptr;
    hr = DllGetActivationFactory(reinterpret_cast<HSTRING>(const_cast<wchar_t*>(L"Windows.Media.Ocr.OcrEngine")), &pActFactory);
    TEST_ASSERT(hr == 0 && pActFactory != nullptr, "DllGetActivationFactory for Windows.Media.Ocr.OcrEngine must succeed");
    auto* pFactoryStatics = static_cast<IOcrEngineStatics*>(pActFactory);
    pFactoryStatics->Release();

    pEngine->Release();

    // 11. Shell Command Execution Verification
    shell::CommandShell proc;
    std::ostringstream oss;

    int shellRet = proc.execute("ocr test", oss);
    TEST_ASSERT(shellRet == 0, "ocr test shell command must return 0");
    TEST_ASSERT(oss.str().find("Windows Media OCR Diagnostics & Self-test passed cleanly") != std::string::npos,
                "ocr test output must contain success message");

    oss.str("");
    shellRet = proc.execute("ocr info", oss);
    TEST_ASSERT(shellRet == 0, "ocr info shell command must return 0");
    TEST_ASSERT(oss.str().find("windows.media.ocr.dll") != std::string::npos,
                "ocr info output must contain driver DLL");

    oss.str("");
    shellRet = proc.execute("ocr languages", oss);
    TEST_ASSERT(shellRet == 0, "ocr languages shell command must return 0");
    TEST_ASSERT(oss.str().find("en-US (Default / System Profile)") != std::string::npos,
                "ocr languages output must list default language");

    oss.str("");
    shellRet = proc.execute("ocr recognize SOVEREIGN MICANT", oss);
    TEST_ASSERT(shellRet == 0, "ocr recognize command must return 0");
    TEST_ASSERT(oss.str().find("SOVEREIGN MICANT") != std::string::npos,
                "ocr recognize output must extract synthesized text");

    std::cout << "[TEST] Suite 126: Windows Optical Character Recognition (OCR) & Modern Media Vision Subsystem PASSED.\n";
}

void Test_WindowsMachineLearning_WinML_Subsystem() {
    using namespace micant::winml;
    std::cout << "[TEST] Running Suite 127: Windows Machine Learning (WinML) & Sovereign Neural Inference Subsystem...\n";

    InitializeWinMLSubsystemExports();
    auto& loader = ldr::DynamicLoader::get();

    // 1. Dynamic Exports Verification
    TEST_ASSERT(loader.getExport("windows.ai.machinelearning.dll", "WinMLCreateRuntime") != nullptr,
                "WinMLCreateRuntime must be exported from windows.ai.machinelearning.dll");
    TEST_ASSERT(loader.getExport("windows.ai.machinelearning.dll", "WinMLCreateTensorFloat") != nullptr,
                "WinMLCreateTensorFloat must be exported from windows.ai.machinelearning.dll");
    TEST_ASSERT(loader.getExport("windows.ai.machinelearning.dll", "WinMLCreateDevice") != nullptr,
                "WinMLCreateDevice must be exported from windows.ai.machinelearning.dll");
    TEST_ASSERT(loader.getExport("windows.ai.machinelearning.dll", "WinMLCreateSession") != nullptr,
                "WinMLCreateSession must be exported from windows.ai.machinelearning.dll");
    TEST_ASSERT(loader.getExport("windows.ai.machinelearning.dll", "DllGetActivationFactory") != nullptr,
                "DllGetActivationFactory must be exported from windows.ai.machinelearning.dll");
    TEST_ASSERT(loader.getExport("windows.ai.machinelearning.dll", "RoGetActivationFactory") != nullptr,
                "RoGetActivationFactory must be exported from windows.ai.machinelearning.dll");

    // 2. VersionDatabase Verification
    const auto* mlMod = version::VersionDatabase::Instance().GetModuleInfo("windows.ai.machinelearning.dll");
    TEST_ASSERT(mlMod != nullptr && mlMod->stringTable.at("ProductVersion") == "10.0.22621.1",
                "windows.ai.machinelearning.dll must be registered in VersionDatabase at 10.0.22621.1");

    // 3. Tensor Creation & Inspection (ITensor, ITensorFloatStatics)
    const int64_t testShape[2] = { 2, 3 };
    const float testData[6] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
    ITensor* pTensor = nullptr;
    int32_t hr = WinMLCreateTensorFloat(testShape, 2, testData, 6, &pTensor);
    TEST_ASSERT(hr == 0 && pTensor != nullptr, "WinMLCreateTensorFloat must succeed");

    TensorDataType dt = TensorDataType::Undefined;
    pTensor->GetTensorDataType(&dt);
    TEST_ASSERT(dt == TensorDataType::Float, "Tensor data type must be Float");

    size_t elemCount = 0;
    pTensor->GetElementCount(&elemCount);
    TEST_ASSERT(elemCount == 6, "Tensor element count must be 6");

    int64_t* pRetShape = nullptr;
    uint32_t rank = 0;
    pTensor->GetShape(&pRetShape, &rank);
    TEST_ASSERT(rank == 2 && pRetShape != nullptr && pRetShape[0] == 2 && pRetShape[1] == 3, "Shape must match [2, 3]");
    ole32::CoTaskMemFree(pRetShape);

    void* pBuf = nullptr;
    size_t byteLen = 0;
    pTensor->GetBuffer(&pBuf, &byteLen);
    TEST_ASSERT(byteLen == 24 && pBuf != nullptr, "Byte length must be 24 bytes (6 * sizeof(float))");
    const float* fBuf = static_cast<const float*>(pBuf);
    TEST_ASSERT(fBuf[0] == 1.0f && fBuf[5] == 6.0f, "Buffer contents must match initialized data");
    pTensor->Release();

    // 4. Mathematical Neural Operator Kernels (Unit Verification)
    // 4a. GEMM (Matrix Multiplication: Y = A @ B)
    // A: [2, 3], B: [3, 2] -> Y: [2, 2]
    const float matA[6] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };
    const float matB[6] = { 7.0f, 8.0f, 9.0f, 1.0f, 2.0f, 3.0f };
    float matY[4] = { 0.0f };
    math::Gemm(matA, matB, nullptr, matY, 2, 3, 2, 1.0f, 0.0f);
    TEST_ASSERT(matY[0] == 31.0f && matY[1] == 19.0f && matY[2] == 85.0f && matY[3] == 55.0f,
                "GEMM matrix multiplication kernel numerical output must match exact values");

    // 4b. 2D Convolution (NCHW)
    std::vector<float> convIn(16, 1.0f);
    std::vector<float> convK(9, 1.0f);
    std::vector<float> convOut(4, 0.0f);
    math::Conv2D(convIn.data(), convK.data(), nullptr, convOut.data(), 1, 1, 4, 4, 1, 3, 3, 1, 1, 0, 0);
    TEST_ASSERT(convOut[0] == 9.0f && convOut[1] == 9.0f && convOut[2] == 9.0f && convOut[3] == 9.0f,
                "Conv2D kernel output must equal 9.0 across all spatial positions");

    // 4c. ReLU & Sigmoid & Softmax
    float actIn[4] = { -2.0f, 0.0f, 3.0f, -0.5f };
    float actOut[4] = { 0.0f };
    math::Relu(actIn, actOut, 4);
    TEST_ASSERT(actOut[0] == 0.0f && actOut[1] == 0.0f && actOut[2] == 3.0f && actOut[3] == 0.0f, "ReLU must zero negatives");

    float sigIn[1] = { 0.0f };
    float sigOut[1] = { 0.0f };
    math::Sigmoid(sigIn, sigOut, 1);
    TEST_ASSERT(std::abs(sigOut[0] - 0.5f) < 1e-5f, "Sigmoid(0) must equal 0.5");

    float smIn[3] = { 1.0f, 2.0f, 3.0f };
    float smOut[3] = { 0.0f };
    math::Softmax(smIn, smOut, 1, 3);
    float smSum = smOut[0] + smOut[1] + smOut[2];
    TEST_ASSERT(std::abs(smSum - 1.0f) < 1e-5f, "Softmax probabilities must sum to 1.0");
    TEST_ASSERT(smOut[2] > smOut[1] && smOut[1] > smOut[0], "Softmax output must be strictly monotonic");

    // 4d. MaxPool2D
    std::vector<float> poolIn = {
        1.0f, 4.0f, 2.0f, 3.0f,
        2.0f, 3.0f, 1.0f, 7.0f,
        8.0f, 5.0f, 0.0f, 2.0f,
        6.0f, 7.0f, 4.0f, 9.0f
    };
    std::vector<float> poolOut(4, 0.0f);
    math::MaxPool2D(poolIn.data(), poolOut.data(), 1, 1, 4, 4, 2, 2, 2, 2);
    TEST_ASSERT(poolOut[0] == 4.0f && poolOut[1] == 7.0f && poolOut[2] == 8.0f && poolOut[3] == 9.0f,
                "MaxPool2D kernel must downsample to 2x2 maximum quadrant values");

    // 5. ILearningModelStatics & Model Inspection
    ILearningModelStatics* pStatics = nullptr;
    hr = WinMLCreateRuntime(&pStatics);
    TEST_ASSERT(hr == 0 && pStatics != nullptr, "WinMLCreateRuntime must succeed");

    ILearningModel* pModel = nullptr;
    hr = pStatics->LoadFromFilePath(L"sovereign_mlp.onnx", &pModel);
    TEST_ASSERT(hr == 0 && pModel != nullptr, "LoadFromFilePath must return S_OK and valid ILearningModel");

    wchar_t nameBuf[128]{};
    uint32_t nameLen = 128;
    pModel->GetName(nameBuf, &nameLen);
    TEST_ASSERT(std::wcscmp(nameBuf, L"MicaNT.Sovereign.MLP") == 0, "Model name must be MicaNT.Sovereign.MLP");

    int64_t modelVer = 0;
    pModel->GetVersion(&modelVer);
    TEST_ASSERT(modelVer == 1, "Model version must be 1");

    // 6. Feature Descriptors Inspection
    ILearningModelFeatureDescriptor** inFeats = nullptr;
    uint32_t inCount = 0;
    pModel->GetInputFeatures(&inFeats, &inCount);
    TEST_ASSERT(inCount == 1 && inFeats != nullptr, "MLP model must specify 1 input feature");

    wchar_t inName[128]{};
    uint32_t inNameLen = 128;
    inFeats[0]->GetName(inName, &inNameLen);
    TEST_ASSERT(std::wcscmp(inName, L"input") == 0, "Input feature name must be 'input'");

    LearningModelFeatureKind fKind{};
    inFeats[0]->GetKind(&fKind);
    TEST_ASSERT(fKind == LearningModelFeatureKind::Tensor, "Input feature kind must be Tensor");

    ITensorFeatureDescriptor* pTensorDesc = nullptr;
    inFeats[0]->GetTensorDescriptor(&pTensorDesc);
    TEST_ASSERT(pTensorDesc != nullptr, "GetTensorDescriptor must succeed");

    TensorDataType tType{};
    pTensorDesc->GetTensorKind(&tType);
    TEST_ASSERT(tType == TensorDataType::Float, "TensorDataType must be Float");
    pTensorDesc->Release();

    inFeats[0]->Release();
    ole32::CoTaskMemFree(inFeats);

    ILearningModelFeatureDescriptor** outFeats = nullptr;
    uint32_t outCount = 0;
    pModel->GetOutputFeatures(&outFeats, &outCount);
    TEST_ASSERT(outCount == 1 && outFeats != nullptr, "MLP model must specify 1 output feature");

    wchar_t outName[128]{};
    uint32_t outNameLen = 128;
    outFeats[0]->GetName(outName, &outNameLen);
    TEST_ASSERT(std::wcscmp(outName, L"probabilities") == 0, "Output feature name must be 'probabilities'");
    outFeats[0]->Release();
    ole32::CoTaskMemFree(outFeats);

    // 7. Device, Session Creation & Evaluation (MLP Model)
    ILearningModelDevice* pDevice = nullptr;
    hr = WinMLCreateDevice(LearningModelDeviceKind::Cpu, &pDevice);
    TEST_ASSERT(hr == 0 && pDevice != nullptr, "WinMLCreateDevice for CPU must succeed");

    LearningModelDeviceKind devKind{};
    pDevice->GetDeviceKind(&devKind);
    TEST_ASSERT(devKind == LearningModelDeviceKind::Cpu, "Device kind must be Cpu");

    ILearningModelSession* pSession = nullptr;
    hr = WinMLCreateSession(pModel, pDevice, &pSession);
    TEST_ASSERT(hr == 0 && pSession != nullptr, "WinMLCreateSession must succeed");

    const int64_t mlpInShape[2] = { 1, 4 };
    const float mlpInData[4] = { 0.5f, 1.2f, 0.8f, 2.1f };
    ITensor* pMlpInTensor = nullptr;
    WinMLCreateTensorFloat(mlpInShape, 2, mlpInData, 4, &pMlpInTensor);

    auto* pBinding = new CLearningModelBindingImpl();
    pBinding->BindTensor(L"input", pMlpInTensor);
    pMlpInTensor->Release();

    ILearningModelEvaluationResult* pEvalResult = nullptr;
    hr = pSession->Evaluate(pBinding, L"corr-mlp-test-01", &pEvalResult);
    TEST_ASSERT(hr == 0 && pEvalResult != nullptr, "Session Evaluate must return S_OK and valid result");

    bool evalSuccess = false;
    pEvalResult->Succeeded(&evalSuccess);
    TEST_ASSERT(evalSuccess, "Evaluation must report success");

    wchar_t corrBuf[64]{};
    uint32_t corrLen = 64;
    pEvalResult->GetCorrelationId(corrBuf, &corrLen);
    TEST_ASSERT(std::wcscmp(corrBuf, L"corr-mlp-test-01") == 0, "Correlation ID must match");

    void* pOutProbVoid = nullptr;
    pEvalResult->GetOutputByName(L"probabilities", &pOutProbVoid);
    TEST_ASSERT(pOutProbVoid != nullptr, "Output 'probabilities' must be found");

    auto* pOutProbTensor = static_cast<ITensor*>(pOutProbVoid);
    size_t probCount = 0;
    pOutProbTensor->GetElementCount(&probCount);
    TEST_ASSERT(probCount == 3, "Output probability tensor must contain 3 classes");

    void* pProbBuf = nullptr;
    size_t probBytes = 0;
    pOutProbTensor->GetBuffer(&pProbBuf, &probBytes);
    const float* fProbs = static_cast<const float*>(pProbBuf);
    float probSum = fProbs[0] + fProbs[1] + fProbs[2];
    TEST_ASSERT(std::abs(probSum - 1.0f) < 1e-4f, "Class probabilities must sum to 1.0 (softmax invariant)");
    TEST_ASSERT(fProbs[0] >= 0.0f && fProbs[1] >= 0.0f && fProbs[2] >= 0.0f, "All probabilities must be non-negative");

    pOutProbTensor->Release();
    pEvalResult->Release();
    pBinding->Release();
    pSession->Release();
    pDevice->Release();
    pModel->Release();

    // 8. Convolutional Vision Network Evaluation (ConvNet Model)
    ILearningModel* pConvModel = nullptr;
    hr = pStatics->LoadFromFilePath(L"convnet_benchmark.onnx", &pConvModel);
    TEST_ASSERT(hr == 0 && pConvModel != nullptr, "Load ConvNet model must succeed");

    ILearningModelDevice* pConvDev = nullptr;
    WinMLCreateDevice(LearningModelDeviceKind::Cpu, &pConvDev);
    ILearningModelSession* pConvSession = nullptr;
    WinMLCreateSession(pConvModel, pConvDev, &pConvSession);

    std::vector<float> sampleImg(36, 0.2f);
    for (size_t y = 0; y < 6; ++y) {
        sampleImg[y * 6 + 2] = 1.0f;
        sampleImg[y * 6 + 3] = 1.0f;
    }
    const int64_t imgShape[4] = { 1, 1, 6, 6 };
    ITensor* pImgTensor = nullptr;
    WinMLCreateTensorFloat(imgShape, 4, sampleImg.data(), 36, &pImgTensor);

    auto* pConvBinding = new CLearningModelBindingImpl();
    pConvBinding->BindTensor(L"image", pImgTensor);
    pImgTensor->Release();

    ILearningModelEvaluationResult* pConvResult = nullptr;
    hr = pConvSession->Evaluate(pConvBinding, L"corr-conv-test", &pConvResult);
    TEST_ASSERT(hr == 0 && pConvResult != nullptr, "ConvNet forward pass must succeed");

    void* pClassProbsVoid = nullptr;
    pConvResult->GetOutputByName(L"class_probs", &pClassProbsVoid);
    TEST_ASSERT(pClassProbsVoid != nullptr, "Output 'class_probs' must be present");

    auto* pClassProbsTensor = static_cast<ITensor*>(pClassProbsVoid);
    size_t cCount = 0;
    pClassProbsTensor->GetElementCount(&cCount);
    TEST_ASSERT(cCount == 2, "ConvNet output must have 2 classes");

    void* pCPBuf = nullptr;
    size_t cpBytes = 0;
    pClassProbsTensor->GetBuffer(&pCPBuf, &cpBytes);
    const float* cpData = static_cast<const float*>(pCPBuf);
    float cpSum = cpData[0] + cpData[1];
    TEST_ASSERT(std::abs(cpSum - 1.0f) < 1e-4f, "ConvNet class probabilities must sum to 1.0");

    pClassProbsTensor->Release();
    pConvResult->Release();
    pConvBinding->Release();
    pConvSession->Release();
    pConvDev->Release();
    pConvModel->Release();

    pStatics->Release();

    // 9. WinRT Activation Factory Verification
    void* pModelFactory = nullptr;
    hr = DllGetActivationFactory(reinterpret_cast<HSTRING>(const_cast<wchar_t*>(L"Windows.AI.MachineLearning.LearningModel")), &pModelFactory);
    TEST_ASSERT(hr == 0 && pModelFactory != nullptr, "DllGetActivationFactory for LearningModel must succeed");
    static_cast<ILearningModelStatics*>(pModelFactory)->Release();

    void* pTensorFactory = nullptr;
    hr = DllGetActivationFactory(reinterpret_cast<HSTRING>(const_cast<wchar_t*>(L"Windows.AI.MachineLearning.TensorFloat")), &pTensorFactory);
    TEST_ASSERT(hr == 0 && pTensorFactory != nullptr, "DllGetActivationFactory for TensorFloat must succeed");
    static_cast<ITensorFloatStatics*>(pTensorFactory)->Release();

    void* pRoFactory = nullptr;
    hr = RoGetActivationFactory(reinterpret_cast<HSTRING>(const_cast<wchar_t*>(L"Windows.AI.MachineLearning.LearningModel")), IID_ILearningModelStatics, &pRoFactory);
    TEST_ASSERT(hr == 0 && pRoFactory != nullptr, "RoGetActivationFactory for LearningModel must succeed");
    static_cast<ILearningModelStatics*>(pRoFactory)->Release();

    // 10. Shell Command Execution Verification
    shell::CommandShell proc;
    std::ostringstream oss;

    int shellRet = proc.execute("winml test", oss);
    TEST_ASSERT(shellRet == 0, "winml test shell command must return 0");
    TEST_ASSERT(oss.str().find("Windows Machine Learning (WinML) Diagnostics & Self-test passed cleanly") != std::string::npos,
                "winml test output must report success message");

    oss.str("");
    shellRet = proc.execute("winml info", oss);
    TEST_ASSERT(shellRet == 0, "winml info shell command must return 0");
    TEST_ASSERT(oss.str().find("windows.ai.machinelearning.dll") != std::string::npos,
                "winml info output must reference runtime DLL");

    oss.str("");
    shellRet = proc.execute("winml run 1.0 0.5 2.0 0.1", oss);
    TEST_ASSERT(shellRet == 0, "winml run shell command must return 0");
    TEST_ASSERT(oss.str().find("Top Classification") != std::string::npos,
                "winml run output must report classification result");

    oss.str("");
    shellRet = proc.execute("winml conv", oss);
    TEST_ASSERT(shellRet == 0, "winml conv shell command must return 0");
    TEST_ASSERT(oss.str().find("Class Probabilities") != std::string::npos,
                "winml conv output must report ConvNet probabilities");

    std::cout << "[TEST] Suite 127: Windows Machine Learning (WinML) & Sovereign Neural Inference Subsystem PASSED.\n";
}

void Test_WindowsWebAuthn_FIDO2_Subsystem() {
    std::cout << "[TEST] Running Suite 128: Windows Web Authentication & Sovereign FIDO2 / Passkey Subsystem...\n";

    using namespace micant::webauthn;
    InitializeWebAuthnSubsystemExports();

    // 1. Dynamic Export Verification in webauthn.dll
    auto& ldr = micant::ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("webauthn.dll", "WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable") != nullptr,
                "WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable must be exported");
    TEST_ASSERT(ldr.getExport("webauthn.dll", "WebAuthNAuthenticatorMakeCredential") != nullptr,
                "WebAuthNAuthenticatorMakeCredential must be exported");
    TEST_ASSERT(ldr.getExport("webauthn.dll", "WebAuthNAuthenticatorGetAssertion") != nullptr,
                "WebAuthNAuthenticatorGetAssertion must be exported");
    TEST_ASSERT(ldr.getExport("webauthn.dll", "WebAuthNFreeCredentialAttestation") != nullptr,
                "WebAuthNFreeCredentialAttestation must be exported");
    TEST_ASSERT(ldr.getExport("webauthn.dll", "WebAuthNFreeAssertion") != nullptr,
                "WebAuthNFreeAssertion must be exported");
    TEST_ASSERT(ldr.getExport("webauthn.dll", "WebAuthNGetCancellationId") != nullptr,
                "WebAuthNGetCancellationId must be exported");
    TEST_ASSERT(ldr.getExport("webauthn.dll", "WebAuthNCancelCurrentOperation") != nullptr,
                "WebAuthNCancelCurrentOperation must be exported");
    TEST_ASSERT(ldr.getExport("webauthn.dll", "WebAuthNGetErrorName") != nullptr,
                "WebAuthNGetErrorName must be exported");
    TEST_ASSERT(ldr.getExport("webauthn.dll", "WebAuthNGetApiVersionNumber") != nullptr,
                "WebAuthNGetApiVersionNumber must be exported");
    TEST_ASSERT(ldr.getExport("webauthn.dll", "WebAuthNDeletePlatformCredential") != nullptr,
                "WebAuthNDeletePlatformCredential must be exported");

    // 2. VersionDatabase Verification
    const auto* mod = micant::version::VersionDatabase::Instance().GetModuleInfo("webauthn.dll");
    TEST_ASSERT(mod != nullptr, "webauthn.dll must be registered in VersionDatabase");
    TEST_ASSERT(mod->stringTable.at("FileVersion") == "10.0.22621.1", "webauthn.dll version must be 10.0.22621.1");

    // 3. API Version Number & Platform Authenticator Availability
    DWORD apiVer = WebAuthNGetApiVersionNumber();
    TEST_ASSERT(apiVer == WEBAUTHN_API_VERSION_CURRENT, "WebAuthn API version must match current version (4)");

    BOOL isAvail = 0;
    HRESULT hr = WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable(&isAvail);
    TEST_ASSERT(hr == 0 && isAvail == 1, "Platform authenticator must be available and user-verifying");

    // Null pointer check
    hr = WebAuthNIsUserVerifyingPlatformAuthenticatorAvailable(nullptr);
    TEST_ASSERT(hr != 0, "Passing null pointer to availability check must return error");

    // 4. Cancellation ID Generation & Cancellation
    GUID cancelId{};
    hr = WebAuthNGetCancellationId(&cancelId);
    TEST_ASSERT(hr == 0, "WebAuthNGetCancellationId must succeed");
    TEST_ASSERT(cancelId.Data1 != 0 || cancelId.Data2 != 0, "Generated cancellation GUID must be non-zero");

    hr = WebAuthNCancelCurrentOperation(&cancelId);
    TEST_ASSERT(hr == 0, "WebAuthNCancelCurrentOperation must succeed");

    // 5. Error Name Lookup
    TEST_ASSERT(std::wcscmp(WebAuthNGetErrorName(0), L"S_OK") == 0, "WebAuthNGetErrorName(0) must return S_OK");
    TEST_ASSERT(std::wcscmp(WebAuthNGetErrorName(static_cast<HRESULT>(0x80070057)), L"E_INVALIDARG") == 0, "WebAuthNGetErrorName(0x80070057) must return E_INVALIDARG");
    TEST_ASSERT(std::wcscmp(WebAuthNGetErrorName(static_cast<HRESULT>(0x80090011)), L"NTE_NOT_FOUND") == 0, "WebAuthNGetErrorName(0x80090011) must return NTE_NOT_FOUND");
    TEST_ASSERT(std::wcscmp(WebAuthNGetErrorName(static_cast<HRESULT>(0x800704C7)), L"ERROR_CANCELLED") == 0, "WebAuthNGetErrorName(0x800704C7) must return ERROR_CANCELLED");

    // 6. WebAuthNAuthenticatorMakeCredential Verification
    WEBAUTHN_RP_ENTITY_INFORMATION rpInfo{};
    rpInfo.dwVersion = WEBAUTHN_RP_ENTITY_INFORMATION_CURRENT_VERSION;
    rpInfo.pwszId = L"auth.micant.dev";
    rpInfo.pwszName = L"MicaNT Dev Identity";

    uint8_t uid[4] = { 0x10, 0x20, 0x30, 0x40 };
    WEBAUTHN_USER_ENTITY_INFORMATION userInfo{};
    userInfo.dwVersion = WEBAUTHN_USER_ENTITY_INFORMATION_CURRENT_VERSION;
    userInfo.cbId = 4;
    userInfo.pbId = uid;
    userInfo.pwszName = L"alice";
    userInfo.pwszDisplayName = L"Alice Wonderland";

    WEBAUTHN_COSE_CREDENTIAL_PARAMETER coseParam{};
    coseParam.dwVersion = WEBAUTHN_COSE_CREDENTIAL_PARAMETER_CURRENT_VERSION;
    coseParam.pwszCredentialType = WEBAUTHN_CREDENTIAL_TYPE_PUBLIC_KEY;
    coseParam.lAlg = WEBAUTHN_COSE_ALGORITHM_ECDSA_P256_WITH_SHA256;

    WEBAUTHN_COSE_CREDENTIAL_PARAMETERS coseParams{};
    coseParams.cCredentialParameters = 1;
    coseParams.pCredentialParameters = &coseParam;

    const char* clientJsonCreate = "{\"type\":\"webauthn.create\",\"challenge\":\"c292ZXJlaWduLWNoYWxsZW5nZQ\",\"origin\":\"https://auth.micant.dev\"}";
    WEBAUTHN_CLIENT_DATA clientDataCreate{};
    clientDataCreate.dwVersion = WEBAUTHN_CLIENT_DATA_CURRENT_VERSION;
    clientDataCreate.cbClientDataJSON = static_cast<DWORD>(std::strlen(clientJsonCreate));
    clientDataCreate.pbClientDataJSON = reinterpret_cast<PBYTE>(const_cast<char*>(clientJsonCreate));
    clientDataCreate.pwszHashAlgId = WEBAUTHN_HASH_ALGORITHM_SHA_256;

    PWEBAUTHN_CREDENTIAL_ATTESTATION pAttestation = nullptr;
    hr = WebAuthNAuthenticatorMakeCredential(
        nullptr, &rpInfo, &userInfo, &coseParams, &clientDataCreate, nullptr, &pAttestation);
    TEST_ASSERT(hr == 0 && pAttestation != nullptr, "WebAuthNAuthenticatorMakeCredential must succeed");
    TEST_ASSERT(pAttestation->dwVersion == WEBAUTHN_CREDENTIAL_ATTESTATION_CURRENT_VERSION, "Attestation version must match");
    TEST_ASSERT(pAttestation->cbCredentialId == 32, "Credential ID must be 32 bytes");
    TEST_ASSERT(pAttestation->pbCredentialId != nullptr, "Credential ID pointer must be non-null");
    TEST_ASSERT(pAttestation->cbAuthenticatorData > 37, "AuthenticatorData must include attested credential data");
    TEST_ASSERT(std::wcscmp(pAttestation->pwszFormatType, L"packed") == 0, "Attestation format must be packed");
    TEST_ASSERT(pAttestation->cbAttestationObject > 0, "AttestationObject CBOR must be present");

    // AuthenticatorData inspection
    // Bytes 0..31: RP ID SHA-256 hash
    std::string rpUtf8 = "auth.micant.dev";
    auto expectedRpHash = micant::crypto::Sha256::hash(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(rpUtf8.data()), rpUtf8.size()));
    TEST_ASSERT(std::memcmp(pAttestation->pbAuthenticatorData, expectedRpHash.data(), 32) == 0,
                "AuthenticatorData RP ID hash must match SHA-256 of RP ID");

    // Byte 32: Flags (UP | UV | AT = 0x45)
    uint8_t flags = pAttestation->pbAuthenticatorData[32];
    TEST_ASSERT((flags & WEBAUTHN_AUTHENTICATOR_DATA_FLAG_UP) != 0, "User Present flag must be set");
    TEST_ASSERT((flags & WEBAUTHN_AUTHENTICATOR_DATA_FLAG_UV) != 0, "User Verified flag must be set");
    TEST_ASSERT((flags & WEBAUTHN_AUTHENTICATOR_DATA_FLAG_AT) != 0, "Attested Credential Data flag must be set");

    // Bytes 33..36: Sign count (0 initially)
    uint32_t initCount = (pAttestation->pbAuthenticatorData[33] << 24) |
                         (pAttestation->pbAuthenticatorData[34] << 16) |
                         (pAttestation->pbAuthenticatorData[35] << 8)  |
                         (pAttestation->pbAuthenticatorData[36]);
    TEST_ASSERT(initCount == 0, "Initial sign count must be 0");

    // Bytes 37..52: AAGUID ("MicaNT-WebAuthn1")
    const char* expectedAaguid = "MicaNT-WebAuthn1";
    TEST_ASSERT(std::memcmp(pAttestation->pbAuthenticatorData + 37, expectedAaguid, 16) == 0,
                "AAGUID must match MicaNT Sovereign Authenticator AAGUID");

    // Save registered credential ID for assertion testing
    std::vector<uint8_t> savedCredId(pAttestation->pbCredentialId, pAttestation->pbCredentialId + pAttestation->cbCredentialId);
    WebAuthNFreeCredentialAttestation(pAttestation);

    // 7. WebAuthNAuthenticatorGetAssertion Verification
    const char* clientJsonGet = "{\"type\":\"webauthn.get\",\"challenge\":\"Y2hhbGxlbmdlLTQ1Ng\",\"origin\":\"https://auth.micant.dev\"}";
    WEBAUTHN_CLIENT_DATA clientDataGet{};
    clientDataGet.dwVersion = WEBAUTHN_CLIENT_DATA_CURRENT_VERSION;
    clientDataGet.cbClientDataJSON = static_cast<DWORD>(std::strlen(clientJsonGet));
    clientDataGet.pbClientDataJSON = reinterpret_cast<PBYTE>(const_cast<char*>(clientJsonGet));
    clientDataGet.pwszHashAlgId = WEBAUTHN_HASH_ALGORITHM_SHA_256;

    PWEBAUTHN_ASSERTION pAssertion = nullptr;
    hr = WebAuthNAuthenticatorGetAssertion(
        nullptr, rpInfo.pwszId, &clientDataGet, nullptr, &pAssertion);
    TEST_ASSERT(hr == 0 && pAssertion != nullptr, "WebAuthNAuthenticatorGetAssertion must succeed");
    TEST_ASSERT(pAssertion->dwVersion == WEBAUTHN_ASSERTION_CURRENT_VERSION, "Assertion version must match");
    TEST_ASSERT(pAssertion->cbAuthenticatorData == 37, "Assertion AuthenticatorData must be exactly 37 bytes");
    TEST_ASSERT(pAssertion->Credential.cbId == 32, "Assertion Credential ID length must match");
    TEST_ASSERT(std::memcmp(pAssertion->Credential.pbId, savedCredId.data(), 32) == 0,
                "Assertion Credential ID must match registered passkey ID");

    // Verify counter incremented to 1
    uint32_t assertCount1 = (pAssertion->pbAuthenticatorData[33] << 24) |
                           (pAssertion->pbAuthenticatorData[34] << 16) |
                           (pAssertion->pbAuthenticatorData[35] << 8)  |
                           (pAssertion->pbAuthenticatorData[36]);
    TEST_ASSERT(assertCount1 == 1, "Sign counter must increment to 1 on first assertion");

    // Signature format: ASN.1 DER (starts with 0x30)
    TEST_ASSERT(pAssertion->cbSignature >= 64 && pAssertion->pbSignature[0] == 0x30,
                "Assertion signature must be valid ASN.1 DER sequence");

    // Cryptographic verification of ECDSA P-256 signature
    CredentialRecord credRec;
    bool foundInVault = SovereignPlatformAuthenticator::get().findCredential(savedCredId, credRec);
    TEST_ASSERT(foundInVault, "Registered credential must be queryable in Sovereign Vault");

    auto clientGetDigest = micant::crypto::Sha256::hash(std::span<const uint8_t>(
        clientDataGet.pbClientDataJSON, clientDataGet.cbClientDataJSON));
    std::vector<uint8_t> sigVerifyBase;
    sigVerifyBase.insert(sigVerifyBase.end(), pAssertion->pbAuthenticatorData, pAssertion->pbAuthenticatorData + pAssertion->cbAuthenticatorData);
    sigVerifyBase.insert(sigVerifyBase.end(), clientGetDigest.begin(), clientGetDigest.end());
    auto fullDigest = micant::crypto::Sha256::hash(std::span<const uint8_t>(sigVerifyBase.data(), sigVerifyBase.size()));
    Uint256 fullDigestInt = Uint256::fromBytes(fullDigest.data());

    bool sigValid = SovereignPlatformAuthenticator::verifyDigest(
        credRec.publicKey, fullDigestInt, pAssertion->pbSignature, pAssertion->cbSignature);
    TEST_ASSERT(sigValid, "Assertion ECDSA P-256 signature must be mathematically valid against public key");

    WebAuthNFreeAssertion(pAssertion);

    // 8. Second Assertion to verify signCount monotonic increment
    pAssertion = nullptr;
    hr = WebAuthNAuthenticatorGetAssertion(
        nullptr, rpInfo.pwszId, &clientDataGet, nullptr, &pAssertion);
    TEST_ASSERT(hr == 0 && pAssertion != nullptr, "Second assertion must succeed");
    uint32_t assertCount2 = (pAssertion->pbAuthenticatorData[33] << 24) |
                           (pAssertion->pbAuthenticatorData[34] << 16) |
                           (pAssertion->pbAuthenticatorData[35] << 8)  |
                           (pAssertion->pbAuthenticatorData[36]);
    TEST_ASSERT(assertCount2 == 2, "Sign counter must monotonically increment to 2");
    WebAuthNFreeAssertion(pAssertion);

    // 9. Cancellation Enforcement in MakeCredential
    GUID cancelToken{};
    WebAuthNGetCancellationId(&cancelToken);
    WebAuthNCancelCurrentOperation(&cancelToken);

    WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS cancelOpts{};
    cancelOpts.dwVersion = WEBAUTHN_AUTHENTICATOR_MAKE_CREDENTIAL_OPTIONS_CURRENT_VERSION;
    cancelOpts.pCancellationId = &cancelToken;

    PWEBAUTHN_CREDENTIAL_ATTESTATION pCancelAttest = nullptr;
    hr = WebAuthNAuthenticatorMakeCredential(
        nullptr, &rpInfo, &userInfo, &coseParams, &clientDataCreate, &cancelOpts, &pCancelAttest);
    TEST_ASSERT(hr == static_cast<HRESULT>(0x800704C7), "MakeCredential with cancelled token must return ERROR_CANCELLED");
    TEST_ASSERT(pCancelAttest == nullptr, "Cancelled operation must not allocate attestation");

    // 10. Platform Credential Deletion
    hr = WebAuthNDeletePlatformCredential(static_cast<DWORD>(savedCredId.size()), savedCredId.data());
    TEST_ASSERT(hr == 0, "WebAuthNDeletePlatformCredential must succeed");

    pAssertion = nullptr;
    hr = WebAuthNAuthenticatorGetAssertion(
        nullptr, rpInfo.pwszId, &clientDataGet, nullptr, &pAssertion);
    TEST_ASSERT(hr == static_cast<HRESULT>(0x80090011), "GetAssertion after deletion must return NTE_NOT_FOUND");

    // 11. CommandShell CLI Integration Verification
    shell::CommandShell proc;
    std::ostringstream oss;

    int shellRet = proc.execute("webauthn test", oss);
    TEST_ASSERT(shellRet == 0, "webauthn test shell command must return 0");
    TEST_ASSERT(oss.str().find("Windows Web Authentication & FIDO2 Diagnostics passed cleanly") != std::string::npos,
                "webauthn test output must indicate successful self-test");

    oss.str("");
    shellRet = proc.execute("webauthn info", oss);
    TEST_ASSERT(shellRet == 0, "webauthn info shell command must return 0");
    TEST_ASSERT(oss.str().find("webauthn.dll") != std::string::npos,
                "webauthn info must reference webauthn.dll");
    TEST_ASSERT(oss.str().find("MicaNT-WebAuthn1") != std::string::npos,
                "webauthn info must display sovereign AAGUID");

    oss.str("");
    shellRet = proc.execute("webauthn register portal.example.com bob", oss);
    TEST_ASSERT(shellRet == 0, "webauthn register shell command must return 0");
    TEST_ASSERT(oss.str().find("Passkey successfully registered") != std::string::npos,
                "webauthn register must report successful passkey registration");

    oss.str("");
    shellRet = proc.execute("webauthn auth portal.example.com", oss);
    TEST_ASSERT(shellRet == 0, "webauthn auth shell command must return 0");
    TEST_ASSERT(oss.str().find("Passkey assertion verified") != std::string::npos,
                "webauthn auth must report successful assertion verification");

    std::cout << "[TEST] Suite 128: Windows Web Authentication & Sovereign FIDO2 / Passkey Subsystem PASSED.\n";
}

// ============================================================================
// Suite 129: Windows Native Wifi & Sovereign WLAN Subsystem
// ============================================================================
void Test_WindowsNativeWifi_WLAN_Subsystem() {
    using namespace micant::wlan;

    // 1. Initialize subsystem exports
    InitializeWlanSubsystemExports();

    // 2. Validate DynamicLoader exports for wlanapi.dll
    auto& loader = ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanOpenHandle") != nullptr, "wlanapi.dll must export WlanOpenHandle");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanCloseHandle") != nullptr, "wlanapi.dll must export WlanCloseHandle");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanEnumInterfaces") != nullptr, "wlanapi.dll must export WlanEnumInterfaces");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanGetInterfaceCapability") != nullptr, "wlanapi.dll must export WlanGetInterfaceCapability");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanScan") != nullptr, "wlanapi.dll must export WlanScan");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanGetAvailableNetworkList") != nullptr, "wlanapi.dll must export WlanGetAvailableNetworkList");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanGetNetworkBssList") != nullptr, "wlanapi.dll must export WlanGetNetworkBssList");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanQueryInterface") != nullptr, "wlanapi.dll must export WlanQueryInterface");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanSetInterface") != nullptr, "wlanapi.dll must export WlanSetInterface");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanConnect") != nullptr, "wlanapi.dll must export WlanConnect");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanDisconnect") != nullptr, "wlanapi.dll must export WlanDisconnect");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanRegisterNotification") != nullptr, "wlanapi.dll must export WlanRegisterNotification");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanSetProfile") != nullptr, "wlanapi.dll must export WlanSetProfile");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanGetProfile") != nullptr, "wlanapi.dll must export WlanGetProfile");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanDeleteProfile") != nullptr, "wlanapi.dll must export WlanDeleteProfile");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanGetProfileList") != nullptr, "wlanapi.dll must export WlanGetProfileList");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanReasonCodeToString") != nullptr, "wlanapi.dll must export WlanReasonCodeToString");
    TEST_ASSERT(loader.getExport("wlanapi.dll", "WlanFreeMemory") != nullptr, "wlanapi.dll must export WlanFreeMemory");

    // 3. Verify VersionDatabase registration
    auto modInfo = version::VersionDatabase::Instance().GetModuleInfo("wlanapi.dll");
    TEST_ASSERT(modInfo != nullptr, "wlanapi.dll must be registered in VersionDatabase");
    TEST_ASSERT(modInfo->stringTable.at("FileVersion") == "10.0.22621.1", "wlanapi.dll version must be 10.0.22621.1");

    // 4. Client Handle Lifecycle
    HANDLE hClient = nullptr;
    DWORD negVer = 0;
    DWORD dwRet = WlanOpenHandle(WLAN_CLIENT_VERSION_WIN10, nullptr, &negVer, &hClient);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "WlanOpenHandle must succeed");
    TEST_ASSERT(hClient != nullptr, "WlanOpenHandle must return non-null handle");
    TEST_ASSERT(negVer == WLAN_CLIENT_VERSION_WIN10, "Negotiated version must match requested Win10 client");

    DWORD invalidClose = WlanCloseHandle(reinterpret_cast<HANDLE>(0x99999), nullptr);
    TEST_ASSERT(invalidClose == ERROR_INVALID_HANDLE, "WlanCloseHandle with bogus handle must fail");

    // 5. Interface Enumeration
    PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;
    dwRet = WlanEnumInterfaces(hClient, nullptr, &pIfList);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && pIfList != nullptr, "WlanEnumInterfaces must succeed");
    TEST_ASSERT(pIfList->dwNumberOfItems >= 1, "Must find at least one wireless adapter interface");

    GUID adapterGuid = pIfList->InterfaceInfo[0].InterfaceGuid;
    std::wstring ifDesc = pIfList->InterfaceInfo[0].strInterfaceDescription;
    TEST_ASSERT(ifDesc.find(L"MicaNT Sovereign 802.11ax") != std::wstring::npos,
                "Interface description must identify Sovereign 802.11ax adapter");

    // 6. Interface Capability Inspection
    PWLAN_INTERFACE_CAPABILITY pCap = nullptr;
    dwRet = WlanGetInterfaceCapability(hClient, &adapterGuid, nullptr, &pCap);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && pCap != nullptr, "WlanGetInterfaceCapability must succeed");
    TEST_ASSERT(pCap->interfaceType == WLAN_INTERFACE_TYPE_NATIVE_802_11, "Interface type must be Native 802.11");
    TEST_ASSERT(pCap->dwNumberOfSupportedPhys >= 5, "Must support at least 5 PHY standards");
    TEST_ASSERT(pCap->dot11PhyTypes[0] == dot11_phy_type_he, "Primary PHY type must be 802.11ax HE (Wi-Fi 6E)");
    WlanFreeMemory(pCap);

    // 7. Notification Registration
    static std::atomic<DWORD> s_lastNotifCode{ 0 };
    static std::atomic<int> s_notifCount{ 0 };
    auto notifCallback = [](PWLAN_NOTIFICATION_DATA pData, [[maybe_unused]] PVOID pContext) {
        if (pData) {
            s_lastNotifCode.store(pData->NotificationCode);
            s_notifCount.fetch_add(1);
        }
    };
    DWORD prevSource = 0;
    dwRet = WlanRegisterNotification(hClient, WLAN_NOTIFICATION_SOURCE_ACM, 0, notifCallback, nullptr, nullptr, &prevSource);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "WlanRegisterNotification must succeed");

    // 8. Radio State Query & Control
    DWORD dwDataSize = 0;
    PVOID pRadioData = nullptr;
    dwRet = WlanQueryInterface(hClient, &adapterGuid, wlan_intf_opcode_radio_state, nullptr, &dwDataSize, &pRadioData, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && pRadioData != nullptr, "WlanQueryInterface for radio state must succeed");
    auto* pRadio = reinterpret_cast<PWLAN_RADIO_STATE>(pRadioData);
    TEST_ASSERT(pRadio->PhyInfo[0].dot11SoftwareRadioState == dot11_radio_state_on, "Software radio must initially be ON");
    WlanFreeMemory(pRadioData);

    WLAN_PHY_INFO phySet{};
    phySet.dot11SoftwareRadioState = dot11_radio_state_off;
    dwRet = WlanSetInterface(hClient, &adapterGuid, wlan_intf_opcode_radio_state, sizeof(WLAN_PHY_INFO), &phySet, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "WlanSetInterface to power off radio must succeed");
    TEST_ASSERT(s_lastNotifCode.load() == wlan_notification_acm_power_setting_change,
                "Radio state change must trigger ACM power notification");

    phySet.dot11SoftwareRadioState = dot11_radio_state_on;
    WlanSetInterface(hClient, &adapterGuid, wlan_intf_opcode_radio_state, sizeof(WLAN_PHY_INFO), &phySet, nullptr);

    // 9. Spectrum Scan & Network Discovery
    dwRet = WlanScan(hClient, &adapterGuid, nullptr, nullptr, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "WlanScan must trigger spectrum sweep");
    TEST_ASSERT(s_lastNotifCode.load() == wlan_notification_acm_scan_complete,
                "WlanScan must dispatch scan complete ACM notification");

    PWLAN_AVAILABLE_NETWORK_LIST pAvailList = nullptr;
    dwRet = WlanGetAvailableNetworkList(hClient, &adapterGuid, 0, nullptr, &pAvailList);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && pAvailList != nullptr, "WlanGetAvailableNetworkList must succeed");
    TEST_ASSERT(pAvailList->dwNumberOfItems >= 4, "Must discover at least 4 pre-seeded wireless networks");

    bool foundCorp = false, foundSovereign = false, foundGuest = false, foundIoT = false;
    for (DWORD i = 0; i < pAvailList->dwNumberOfItems; ++i) {
        const auto& net = pAvailList->Network[i];
        std::string ssid(reinterpret_cast<const char*>(net.dot11Ssid.ucSSID), net.dot11Ssid.uSSIDLength);
        if (ssid == "MicaNT-Corp-Secure") {
            foundCorp = true;
            TEST_ASSERT(net.bSecurityEnabled == 1, "MicaNT-Corp-Secure must have security enabled");
            TEST_ASSERT(net.dot11DefaultAuthAlgorithm == DOT11_AUTH_ALGO_WPA3, "MicaNT-Corp-Secure must use WPA3");
        } else if (ssid == "SovereignNet-5G") {
            foundSovereign = true;
            TEST_ASSERT(net.bSecurityEnabled == 1, "SovereignNet-5G must have security enabled");
            TEST_ASSERT(net.dot11DefaultAuthAlgorithm == DOT11_AUTH_ALGO_RSNA_PSK, "SovereignNet-5G must use WPA2-PSK");
        } else if (ssid == "Guest-Open") {
            foundGuest = true;
            TEST_ASSERT(net.bSecurityEnabled == 0, "Guest-Open must not have security enabled");
        } else if (ssid == "Lab-IoT-Mesh") {
            foundIoT = true;
        }
    }
    TEST_ASSERT(foundCorp && foundSovereign && foundGuest && foundIoT,
                "All pre-seeded test SSIDs must be present in scan results");
    WlanFreeMemory(pAvailList);

    // 10. BSS Entry Inspection
    PWLAN_BSS_LIST pBssList = nullptr;
    dwRet = WlanGetNetworkBssList(hClient, &adapterGuid, nullptr, dot11_BSS_type_any, 0, nullptr, &pBssList);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && pBssList != nullptr, "WlanGetNetworkBssList must succeed");
    TEST_ASSERT(pBssList->dwNumberOfItems >= 4, "BSS list must contain entries for all APs");
    TEST_ASSERT(pBssList->wlanBssEntries[0].lRssi < 0, "RSSI must be negative dBm");
    TEST_ASSERT(pBssList->wlanBssEntries[0].ulChCenterFrequency >= 2400000, "Frequency must be in 2.4/5/6 GHz spectrum");
    WlanFreeMemory(pBssList);

    // 11. Profile Management
    PWLAN_PROFILE_INFO_LIST pProfiles = nullptr;
    dwRet = WlanGetProfileList(hClient, &adapterGuid, nullptr, &pProfiles);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && pProfiles != nullptr, "WlanGetProfileList must succeed");
    TEST_ASSERT(pProfiles->dwNumberOfItems >= 1, "Must contain at least pre-seeded SovereignNet-5G profile");
    WlanFreeMemory(pProfiles);

    LPWSTR pProfileXml = nullptr;
    DWORD profileFlags = 0;
    dwRet = WlanGetProfile(hClient, &adapterGuid, L"SovereignNet-5G", nullptr, &pProfileXml, &profileFlags, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && pProfileXml != nullptr, "WlanGetProfile for SovereignNet-5G must succeed");
    std::wstring xmlStr(pProfileXml);
    TEST_ASSERT(xmlStr.find(L"<name>SovereignNet-5G</name>") != std::wstring::npos,
                "Profile XML must contain profile name");
    WlanFreeMemory(pProfileXml);

    const wchar_t* newTestXml =
        L"<?xml version=\"1.0\"?>\n"
        L"<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">\n"
        L"    <name>MicaNT-Lab-AP</name>\n"
        L"    <SSIDConfig>\n"
        L"        <SSID><name>MicaNT-Lab-AP</name></SSID>\n"
        L"    </SSIDConfig>\n"
        L"    <connectionType>ESS</connectionType>\n"
        L"    <connectionMode>auto</connectionMode>\n"
        L"</WLANProfile>\n";
    DWORD reasonCode = 0;
    dwRet = WlanSetProfile(hClient, &adapterGuid, 0, newTestXml, nullptr, 1, &reasonCode, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "WlanSetProfile must succeed");

    dwRet = WlanGetProfileList(hClient, &adapterGuid, nullptr, &pProfiles);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && pProfiles != nullptr && pProfiles->dwNumberOfItems >= 2,
                "Profile list must now contain added profile");
    WlanFreeMemory(pProfiles);

    dwRet = WlanDeleteProfile(hClient, &adapterGuid, L"MicaNT-Lab-AP", nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "WlanDeleteProfile must succeed");

    // 12. Connection & Disconnection State Machine
    WLAN_CONNECTION_PARAMETERS connParams{};
    connParams.wlanConnectionMode = wlan_connection_mode_profile;
    connParams.strProfile = L"SovereignNet-5G";
    connParams.dot11BssType = dot11_BSS_type_infrastructure;

    dwRet = WlanConnect(hClient, &adapterGuid, &connParams, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "WlanConnect must succeed");
    TEST_ASSERT(s_lastNotifCode.load() == wlan_notification_acm_connection_complete,
                "WlanConnect must trigger ACM connection complete notification");

    PVOID pConnAttrs = nullptr;
    DWORD cbConn = 0;
    dwRet = WlanQueryInterface(hClient, &adapterGuid, wlan_intf_opcode_current_connection, nullptr, &cbConn, &pConnAttrs, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && pConnAttrs != nullptr, "Querying current connection while connected must succeed");
    auto* pConn = reinterpret_cast<PWLAN_CONNECTION_ATTRIBUTES>(pConnAttrs);
    TEST_ASSERT(pConn->isState == wlan_interface_state_connected, "State must be connected");
    std::string connectedSsid(reinterpret_cast<const char*>(pConn->wlanAssociationAttributes.dot11Ssid.ucSSID),
                              pConn->wlanAssociationAttributes.dot11Ssid.uSSIDLength);
    TEST_ASSERT(connectedSsid == "SovereignNet-5G", "Connected SSID must be SovereignNet-5G");
    TEST_ASSERT(pConn->wlanAssociationAttributes.ulRxRate > 0, "Rx rate must be positive");
    WlanFreeMemory(pConnAttrs);

    dwRet = WlanDisconnect(hClient, &adapterGuid, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "WlanDisconnect must succeed");
    TEST_ASSERT(s_lastNotifCode.load() == wlan_notification_acm_disconnected,
                "WlanDisconnect must trigger ACM disconnected notification");

    PVOID pStateData = nullptr;
    DWORD cbState = 0;
    dwRet = WlanQueryInterface(hClient, &adapterGuid, wlan_intf_opcode_interface_state, nullptr, &cbState, &pStateData, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && pStateData != nullptr, "Querying interface state must succeed");
    auto* pState = reinterpret_cast<PWLAN_INTERFACE_STATE>(pStateData);
    TEST_ASSERT(*pState == wlan_interface_state_disconnected, "Interface state must now be disconnected");
    WlanFreeMemory(pStateData);

    // 13. Reason Codes
    wchar_t reasonBuf[128]{ 0 };
    dwRet = WlanReasonCodeToString(WLAN_REASON_CODE_SUCCESS, 128, reasonBuf, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "WlanReasonCodeToString must succeed");
    TEST_ASSERT(std::wstring(reasonBuf).find(L"succeeded") != std::wstring::npos, "Reason string must describe success");

    dwRet = WlanReasonCodeToString(WLAN_REASON_CODE_NETWORK_NOT_AVAILABLE, 128, reasonBuf, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "WlanReasonCodeToString must succeed for unavailable network");
    TEST_ASSERT(std::wstring(reasonBuf).find(L"not available") != std::wstring::npos,
                "Reason string must describe network unavailable");

    // Clean up interface list & handle
    WlanFreeMemory(pIfList);
    dwRet = WlanCloseHandle(hClient, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "WlanCloseHandle must succeed");

    // 14. CommandShell CLI Integration Verification
    shell::CommandShell proc;
    std::ostringstream oss;

    int shellRet = proc.execute("wlan test", oss);
    TEST_ASSERT(shellRet == 0, "wlan test shell command must return 0");
    TEST_ASSERT(oss.str().find("Windows Native Wifi & Sovereign WLAN Diagnostics passed cleanly") != std::string::npos,
                "wlan test output must indicate successful self-test");

    oss.str("");
    shellRet = proc.execute("wlan info", oss);
    TEST_ASSERT(shellRet == 0, "wlan info shell command must return 0");
    TEST_ASSERT(oss.str().find("wlanapi.dll") != std::string::npos, "wlan info must reference wlanapi.dll");
    TEST_ASSERT(oss.str().find("802.11ax") != std::string::npos, "wlan info must display 802.11ax adapter");

    oss.str("");
    shellRet = proc.execute("wlan scan", oss);
    TEST_ASSERT(shellRet == 0, "wlan scan shell command must return 0");
    TEST_ASSERT(oss.str().find("Spectrum scan initiated") != std::string::npos, "wlan scan must report sweep");

    oss.str("");
    shellRet = proc.execute("wlan list", oss);
    TEST_ASSERT(shellRet == 0, "wlan list shell command must return 0");
    TEST_ASSERT(oss.str().find("Available Wireless Networks") != std::string::npos, "wlan list must print table");
    TEST_ASSERT(oss.str().find("SovereignNet-5G") != std::string::npos, "wlan list must show SovereignNet-5G");

    oss.str("");
    shellRet = proc.execute("wlan profiles", oss);
    TEST_ASSERT(shellRet == 0, "wlan profiles shell command must return 0");
    TEST_ASSERT(oss.str().find("Configured WLAN Profiles") != std::string::npos, "wlan profiles must list profiles");

    oss.str("");
    shellRet = proc.execute("wlan connect SovereignNet-5G", oss);
    TEST_ASSERT(shellRet == 0, "wlan connect shell command must return 0");
    TEST_ASSERT(oss.str().find("Successfully connected") != std::string::npos, "wlan connect must report success");

    oss.str("");
    shellRet = proc.execute("wlan disconnect", oss);
    TEST_ASSERT(shellRet == 0, "wlan disconnect shell command must return 0");
    TEST_ASSERT(oss.str().find("Disconnected from current WLAN access point") != std::string::npos,
                "wlan disconnect must report disconnect");

    std::cout << "[TEST] Suite 129: Windows Native Wifi & Sovereign WLAN Subsystem PASSED.\n";
}

// ============================================================================
// Suite 130: Windows Virtual Disk & Storage Management Subsystem
// ============================================================================
void Test_WindowsVirtualDisk_Storage_Subsystem() {
    using namespace micant::virtdisk;

    // 1. Initialize subsystem exports
    InitializeVirtualDiskSubsystemExports();

    // 2. Validate DynamicLoader exports for virtdisk.dll
    auto& loader = ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("virtdisk.dll", "CreateVirtualDisk") != nullptr, "virtdisk.dll must export CreateVirtualDisk");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "OpenVirtualDisk") != nullptr, "virtdisk.dll must export OpenVirtualDisk");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "AttachVirtualDisk") != nullptr, "virtdisk.dll must export AttachVirtualDisk");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "DetachVirtualDisk") != nullptr, "virtdisk.dll must export DetachVirtualDisk");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "GetVirtualDiskInformation") != nullptr, "virtdisk.dll must export GetVirtualDiskInformation");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "SetVirtualDiskInformation") != nullptr, "virtdisk.dll must export SetVirtualDiskInformation");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "GetVirtualDiskPhysicalPath") != nullptr, "virtdisk.dll must export GetVirtualDiskPhysicalPath");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "GetAllAttachedVirtualDiskPhysicalPaths") != nullptr, "virtdisk.dll must export GetAllAttachedVirtualDiskPhysicalPaths");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "CompactVirtualDisk") != nullptr, "virtdisk.dll must export CompactVirtualDisk");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "ExpandVirtualDisk") != nullptr, "virtdisk.dll must export ExpandVirtualDisk");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "ResizeVirtualDisk") != nullptr, "virtdisk.dll must export ResizeVirtualDisk");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "MirrorVirtualDisk") != nullptr, "virtdisk.dll must export MirrorVirtualDisk");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "BreakMirrorVirtualDisk") != nullptr, "virtdisk.dll must export BreakMirrorVirtualDisk");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "AddVirtualDiskParent") != nullptr, "virtdisk.dll must export AddVirtualDiskParent");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "MergeVirtualDisk") != nullptr, "virtdisk.dll must export MergeVirtualDisk");
    TEST_ASSERT(loader.getExport("virtdisk.dll", "GetStorageDependencyInformation") != nullptr, "virtdisk.dll must export GetStorageDependencyInformation");

    // 3. Verify VersionDatabase registration
    auto modInfo = version::VersionDatabase::Instance().GetModuleInfo("virtdisk.dll");
    TEST_ASSERT(modInfo != nullptr, "virtdisk.dll must be registered in VersionDatabase");
    TEST_ASSERT(modInfo->stringTable.at("FileVersion") == "10.0.22621.1", "virtdisk.dll version must be 10.0.22621.1");

    // 4. Create Dynamic VHD (512-byte sectors)
    std::wstring vhdPath = L"C:\\Disks\\SystemData.vhd";
    CREATE_VIRTUAL_DISK_PARAMETERS createParams{};
    createParams.Version = CREATE_VIRTUAL_DISK_VERSION_1;
    createParams.Version1.MaximumSize = 2ULL * 1024 * 1024 * 1024; // 2 GB
    createParams.Version1.SectorSizeInBytes = 512;
    createParams.Version1.BlockSizeInBytes = 2097152; // 2 MB

    VIRTUAL_STORAGE_TYPE vhdType{};
    vhdType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHD;
    vhdType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

    HANDLE hVhd = nullptr;
    DWORD dwRet = CreateVirtualDisk(&vhdType, vhdPath.c_str(), VIRTUAL_DISK_ACCESS_ALL, nullptr,
                                    CREATE_VIRTUAL_DISK_FLAG_NONE, 0, &createParams, nullptr, &hVhd);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "CreateVirtualDisk for dynamic VHD must succeed");
    TEST_ASSERT(hVhd != nullptr, "CreateVirtualDisk must return valid handle");

    // 5. Query Information on Newly Created Disk
    GET_VIRTUAL_DISK_INFO diskInfo{};
    ULONG infoSize = sizeof(GET_VIRTUAL_DISK_INFO);
    ULONG sizeUsed = 0;

    diskInfo.Version = GET_VIRTUAL_DISK_INFO_SIZE;
    dwRet = GetVirtualDiskInformation(hVhd, &infoSize, &diskInfo, &sizeUsed);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "GetVirtualDiskInformation SIZE must succeed");
    TEST_ASSERT(diskInfo.Size.VirtualSize == 2ULL * 1024 * 1024 * 1024, "Virtual size must match 2 GB");
    TEST_ASSERT(diskInfo.Size.SectorSize == 512, "Sector size must be 512 bytes");
    TEST_ASSERT(diskInfo.Size.BlockSize == 2097152, "Block size must be 2 MB");

    diskInfo.Version = GET_VIRTUAL_DISK_INFO_PROVIDER_SUBTYPE;
    dwRet = GetVirtualDiskInformation(hVhd, &infoSize, &diskInfo, &sizeUsed);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "GetVirtualDiskInformation PROVIDER_SUBTYPE must succeed");
    TEST_ASSERT(diskInfo.ProviderSubtype == 3, "Subtype must be 3 (Dynamic)");

    diskInfo.Version = GET_VIRTUAL_DISK_INFO_VIRTUAL_STORAGE_TYPE;
    dwRet = GetVirtualDiskInformation(hVhd, &infoSize, &diskInfo, &sizeUsed);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "GetVirtualDiskInformation VIRTUAL_STORAGE_TYPE must succeed");
    TEST_ASSERT(diskInfo.VirtualStorageType.DeviceId == VIRTUAL_STORAGE_TYPE_DEVICE_VHD, "Storage type must be VHD");

    diskInfo.Version = GET_VIRTUAL_DISK_INFO_IS_LOADED;
    dwRet = GetVirtualDiskInformation(hVhd, &infoSize, &diskInfo, &sizeUsed);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && diskInfo.IsLoaded == 0, "Disk must initially be detached (not loaded)");

    // 6. Attach Virtual Disk
    ATTACH_VIRTUAL_DISK_PARAMETERS attachParams{};
    attachParams.Version = ATTACH_VIRTUAL_DISK_VERSION_1;
    dwRet = AttachVirtualDisk(hVhd, nullptr, ATTACH_VIRTUAL_DISK_FLAG_NONE, 0, &attachParams, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "AttachVirtualDisk must succeed");

    diskInfo.Version = GET_VIRTUAL_DISK_INFO_IS_LOADED;
    dwRet = GetVirtualDiskInformation(hVhd, &infoSize, &diskInfo, &sizeUsed);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && diskInfo.IsLoaded == 1, "Disk must now be attached (loaded)");

    // Verify duplicate attach fails
    dwRet = AttachVirtualDisk(hVhd, nullptr, ATTACH_VIRTUAL_DISK_FLAG_NONE, 0, &attachParams, nullptr);
    TEST_ASSERT(dwRet == ERROR_ALREADY_EXISTS, "Re-attaching already attached disk must return ERROR_ALREADY_EXISTS");

    // 7. Get Virtual Disk Physical Path
    wchar_t physPath[256]{ 0 };
    ULONG physBytes = sizeof(physPath);
    dwRet = GetVirtualDiskPhysicalPath(hVhd, &physBytes, physPath);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "GetVirtualDiskPhysicalPath must succeed");
    TEST_ASSERT(std::wstring(physPath).find(L"PhysicalDrive") != std::wstring::npos,
                "Physical path must contain PhysicalDrive");

    // 8. Get All Attached Virtual Disk Physical Paths
    wchar_t multiPaths[512]{ 0 };
    ULONG multiBytes = sizeof(multiPaths);
    dwRet = GetAllAttachedVirtualDiskPhysicalPaths(&multiBytes, multiPaths);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "GetAllAttachedVirtualDiskPhysicalPaths must succeed");
    TEST_ASSERT(std::wstring(multiPaths).find(L"PhysicalDrive") != std::wstring::npos,
                "All attached paths multi-sz must contain attached PhysicalDrive");

    // 9. Expand Virtual Disk
    EXPAND_VIRTUAL_DISK_PARAMETERS expParams{};
    expParams.Version = EXPAND_VIRTUAL_DISK_VERSION_1;
    expParams.Version1.NewSize = 4ULL * 1024 * 1024 * 1024; // Expand to 4 GB
    dwRet = ExpandVirtualDisk(hVhd, EXPAND_VIRTUAL_DISK_FLAG_NONE, &expParams, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "ExpandVirtualDisk must succeed");

    diskInfo.Version = GET_VIRTUAL_DISK_INFO_SIZE;
    dwRet = GetVirtualDiskInformation(hVhd, &infoSize, &diskInfo, &sizeUsed);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && diskInfo.Size.VirtualSize == 4ULL * 1024 * 1024 * 1024,
                "Expanded virtual size must be 4 GB");

    // 10. Compact Virtual Disk
    dwRet = CompactVirtualDisk(hVhd, COMPACT_VIRTUAL_DISK_FLAG_NONE, nullptr, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "CompactVirtualDisk must succeed");

    // 11. Storage Dependency Information
    STORAGE_DEPENDENCY_INFO depInfo{};
    ULONG depSize = sizeof(depInfo);
    ULONG depUsed = 0;
    dwRet = GetStorageDependencyInformation(hVhd, GET_STORAGE_DEPENDENCY_FLAG_NONE, depSize, &depInfo, &depUsed);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && depInfo.NumberEntries >= 1, "GetStorageDependencyInformation must return dependencies");

    // 12. Detach Virtual Disk
    dwRet = DetachVirtualDisk(hVhd, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "DetachVirtualDisk must succeed");

    diskInfo.Version = GET_VIRTUAL_DISK_INFO_IS_LOADED;
    dwRet = GetVirtualDiskInformation(hVhd, &infoSize, &diskInfo, &sizeUsed);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && diskInfo.IsLoaded == 0, "Disk must be detached after DetachVirtualDisk");

    physBytes = sizeof(physPath);
    dwRet = GetVirtualDiskPhysicalPath(hVhd, &physBytes, physPath);
    TEST_ASSERT(dwRet == ERROR_NOT_FOUND, "GetVirtualDiskPhysicalPath after detach must return ERROR_NOT_FOUND");

    SovereignVirtDiskManager::get().closeDisk(hVhd);

    // 13. Create Fixed VHDX (4096-byte sectors)
    std::wstring vhdxPath = L"C:\\Disks\\ModernVolume.vhdx";
    CREATE_VIRTUAL_DISK_PARAMETERS vhdxParams{};
    vhdxParams.Version = CREATE_VIRTUAL_DISK_VERSION_2;
    vhdxParams.Version2.MaximumSize = 512ULL * 1024 * 1024; // 512 MB
    vhdxParams.Version2.SectorSizeInBytes = 4096;
    vhdxParams.Version2.BlockSizeInBytes = 32 * 1024 * 1024;

    VIRTUAL_STORAGE_TYPE vhdxType{};
    vhdxType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX;
    vhdxType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT;

    HANDLE hVhdx = nullptr;
    dwRet = CreateVirtualDisk(&vhdxType, vhdxPath.c_str(), VIRTUAL_DISK_ACCESS_ALL, nullptr,
                             CREATE_VIRTUAL_DISK_FLAG_FULL_PHYSICAL_ALLOCATION, 0, &vhdxParams, nullptr, &hVhdx);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && hVhdx != nullptr, "CreateVirtualDisk for fixed VHDX must succeed");

    diskInfo.Version = GET_VIRTUAL_DISK_INFO_IS_4K_ALIGNED;
    dwRet = GetVirtualDiskInformation(hVhdx, &infoSize, &diskInfo, &sizeUsed);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && diskInfo.Is4kAligned == 1, "VHDX must report 4K alignment");

    diskInfo.Version = GET_VIRTUAL_DISK_INFO_PROVIDER_SUBTYPE;
    dwRet = GetVirtualDiskInformation(hVhdx, &infoSize, &diskInfo, &sizeUsed);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && diskInfo.ProviderSubtype == 2, "VHDX allocation must report 2 (Fixed)");

    SovereignVirtDiskManager::get().closeDisk(hVhdx);

    // 14. CommandShell CLI Integration Verification
    shell::CommandShell proc;
    std::ostringstream oss;

    int shellRet = proc.execute("vhd test", oss);
    TEST_ASSERT(shellRet == 0, "vhd test shell command must return 0");
    TEST_ASSERT(oss.str().find("Windows Virtual Disk Diagnostics passed cleanly") != std::string::npos,
                "vhd test output must indicate successful self-test");

    oss.str("");
    shellRet = proc.execute("vhd list", oss);
    TEST_ASSERT(shellRet == 0, "vhd list shell command must return 0");
    TEST_ASSERT(oss.str().find("Registered Virtual Hard Disks") != std::string::npos,
                "vhd list must display header");

    oss.str("");
    shellRet = proc.execute("vhd create C:\\Storage\\CliDisk.vhd 256", oss);
    TEST_ASSERT(shellRet == 0, "vhd create shell command must return 0");
    TEST_ASSERT(oss.str().find("Virtual disk successfully created") != std::string::npos,
                "vhd create must report success");

    oss.str("");
    shellRet = proc.execute("vhd info C:\\Storage\\CliDisk.vhd", oss);
    TEST_ASSERT(shellRet == 0, "vhd info shell command must return 0");
    TEST_ASSERT(oss.str().find("256 MB") != std::string::npos, "vhd info must show 256 MB virtual size");
    TEST_ASSERT(oss.str().find("virtdisk.dll") != std::string::npos, "vhd info must reference virtdisk.dll");

    oss.str("");
    shellRet = proc.execute("vhd attach C:\\Storage\\CliDisk.vhd", oss);
    TEST_ASSERT(shellRet == 0, "vhd attach shell command must return 0");
    TEST_ASSERT(oss.str().find("Virtual disk attached successfully") != std::string::npos,
                "vhd attach must report success");
    TEST_ASSERT(oss.str().find("PhysicalDrive") != std::string::npos,
                "vhd attach must print assigned PhysicalDrive device");

    oss.str("");
    shellRet = proc.execute("vhd expand C:\\Storage\\CliDisk.vhd 512", oss);
    TEST_ASSERT(shellRet == 0, "vhd expand shell command must return 0");
    TEST_ASSERT(oss.str().find("Virtual disk expanded successfully to 512 MB") != std::string::npos,
                "vhd expand must report success");

    oss.str("");
    shellRet = proc.execute("vhd detach C:\\Storage\\CliDisk.vhd", oss);
    TEST_ASSERT(shellRet == 0, "vhd detach shell command must return 0");
    TEST_ASSERT(oss.str().find("detached successfully") != std::string::npos,
                "vhd detach must report success");

    std::cout << "[TEST] Suite 130: Windows Virtual Disk & Storage Management Subsystem PASSED.\n";
}

// ============================================================================
// Suite 131: Windows BitLocker Drive Encryption & FVE Subsystem
// ============================================================================
void Test_WindowsBitLocker_FVE_Subsystem() {
    using namespace micant::fve;

    // 1. Initialize subsystem exports
    InitializeFveSubsystemExports();

    // 2. Validate DynamicLoader exports for fveapi.dll
    auto& loader = ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveOpenVolume") != nullptr, "fveapi.dll must export FveOpenVolume");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveCloseVolume") != nullptr, "fveapi.dll must export FveCloseVolume");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveGetStatus") != nullptr, "fveapi.dll must export FveGetStatus");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveTurnOn") != nullptr, "fveapi.dll must export FveTurnOn");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveTurnOff") != nullptr, "fveapi.dll must export FveTurnOff");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FvePause") != nullptr, "fveapi.dll must export FvePause");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveResume") != nullptr, "fveapi.dll must export FveResume");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveLockVolume") != nullptr, "fveapi.dll must export FveLockVolume");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveUnlockVolumeWithPassphrase") != nullptr, "fveapi.dll must export FveUnlockVolumeWithPassphrase");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveUnlockVolumeWithRecoveryPassword") != nullptr, "fveapi.dll must export FveUnlockVolumeWithRecoveryPassword");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveAddAuthMethodPassphrase") != nullptr, "fveapi.dll must export FveAddAuthMethodPassphrase");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveAddAuthMethodRecoveryPassword") != nullptr, "fveapi.dll must export FveAddAuthMethodRecoveryPassword");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveAddAuthMethodTpm") != nullptr, "fveapi.dll must export FveAddAuthMethodTpm");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveRemoveAuthMethod") != nullptr, "fveapi.dll must export FveRemoveAuthMethod");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveGetAuthMethodInformation") != nullptr, "fveapi.dll must export FveGetAuthMethodInformation");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveGetAuthMethodList") != nullptr, "fveapi.dll must export FveGetAuthMethodList");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveGetRecoveryPassword") != nullptr, "fveapi.dll must export FveGetRecoveryPassword");
    TEST_ASSERT(loader.getExport("fveapi.dll", "FveFreeMemory") != nullptr, "fveapi.dll must export FveFreeMemory");

    // 3. Verify VersionDatabase registration
    auto modInfo = version::VersionDatabase::Instance().GetModuleInfo("fveapi.dll");
    TEST_ASSERT(modInfo != nullptr, "fveapi.dll must be registered in VersionDatabase");
    TEST_ASSERT(modInfo->stringTable.at("FileVersion") == "10.0.22621.1", "fveapi.dll version must be 10.0.22621.1");
    TEST_ASSERT(modInfo->stringTable.at("FileDescription") == "Windows BitLocker & Full Volume Encryption Subsystem",
                "fveapi.dll description must match Windows FVE");

    // 4. Test BitLocker 48-Digit Numerical Recovery Password Generation and Modulo-11 Validation
    std::wstring generatedRec = GenerateBitLockerRecoveryPassword();
    TEST_ASSERT(generatedRec.size() == 55, "BitLocker recovery password must be 55 characters (8 groups of 6 + 7 hyphens)");
    TEST_ASSERT(ValidateBitLockerRecoveryPassword(generatedRec), "Generated recovery password must pass modulo-11 validation");

    // Test modulo-11 validation edge cases
    TEST_ASSERT(ValidateBitLockerRecoveryPassword(L"111111-222222-333333-444444-555555-666666-777777-888888"),
                "Repunit multiples of 11 must validate");
    TEST_ASSERT(!ValidateBitLockerRecoveryPassword(L"111112-222222-333333-444444-555555-666666-777777-888888"),
                "Non-multiple of 11 must fail validation");
    TEST_ASSERT(!ValidateBitLockerRecoveryPassword(L"ABCDEF-222222-333333-444444-555555-666666-777777-888888"),
                "Non-digit characters must fail validation");

    // 5. Interrogate Pre-Seeded System Volume C:
    HANDLE hVolC = nullptr;
    DWORD dwRet = FveOpenVolume(L"C:", 0, &hVolC);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveOpenVolume for C: must succeed");
    TEST_ASSERT(hVolC != nullptr, "Volume handle for C: must be non-null");

    FVE_STATUS statusC{};
    dwRet = FveGetStatus(hVolC, &statusC);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveGetStatus for C: must succeed");
    TEST_ASSERT(statusC.ProtectionStatus == FVE_PROTECTION_STATUS_ON, "C: must have ProtectionStatus ON");
    TEST_ASSERT(statusC.ConversionStatus == FVE_CONVERSION_STATUS_FULLY_ENCRYPTED, "C: must be Fully Encrypted");
    TEST_ASSERT(statusC.EncryptionMethod == FVE_ENCRYPTION_METHOD_XTS_AES_256, "C: must use XTS-AES-256");
    TEST_ASSERT(statusC.LockStatus == FVE_LOCK_STATUS_UNLOCKED, "C: must be Unlocked");
    TEST_ASSERT(statusC.EncryptionPercentage == 100, "C: must be 100% encrypted");
    TEST_ASSERT(statusC.VolumeType == FVE_VOLUME_TYPE_OS, "C: must be OS volume type");

    // Enumerate protectors on C:
    PFVE_AUTH_METHOD_LIST pListC = nullptr;
    dwRet = FveGetAuthMethodList(hVolC, &pListC);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveGetAuthMethodList for C: must succeed");
    TEST_ASSERT(pListC != nullptr && pListC->dwNumberOfItems >= 2, "C: must have at least 2 key protectors");

    bool hasTpm = false;
    bool hasRec = false;
    for (DWORD i = 0; i < pListC->dwNumberOfItems; ++i) {
        if (pListC->Items[i].AuthMethodType == FVE_AUTH_METHOD_TPM) hasTpm = true;
        if (pListC->Items[i].AuthMethodType == FVE_AUTH_METHOD_RECOVERY_PASSWORD) hasRec = true;
    }
    TEST_ASSERT(hasTpm, "C: must have TPM key protector");
    TEST_ASSERT(hasRec, "C: must have Recovery Password key protector");

    wchar_t cRecPwd[64]{ 0 };
    dwRet = FveGetRecoveryPassword(hVolC, nullptr, cRecPwd, 64);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveGetRecoveryPassword for C: must succeed");
    TEST_ASSERT(ValidateBitLockerRecoveryPassword(cRecPwd), "C: recovery password must be valid modulo-11");

    FveFreeMemory(pListC);
    dwRet = FveCloseVolume(hVolC);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveCloseVolume for C: must succeed");

    // 6. Test Data Volume D: Lifecycle, Encryption, Protectors & Lock/Unlock
    HANDLE hVolD = nullptr;
    dwRet = FveOpenVolume(L"D:", 0, &hVolD);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveOpenVolume for D: must succeed");

    FVE_STATUS statusD{};
    dwRet = FveGetStatus(hVolD, &statusD);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveGetStatus for D: must succeed");
    TEST_ASSERT(statusD.ProtectionStatus == FVE_PROTECTION_STATUS_OFF, "D: must initially have Protection OFF");
    TEST_ASSERT(statusD.ConversionStatus == FVE_CONVERSION_STATUS_FULLY_DECRYPTED, "D: must initially be Fully Decrypted");

    // Cannot lock an unencrypted volume
    dwRet = FveLockVolume(hVolD, 0);
    TEST_ASSERT(dwRet == FVE_E_NOT_ENCRYPTED, "Locking unencrypted volume must fail with FVE_E_NOT_ENCRYPTED");

    // Add passphrase protector: short passphrase validation
    GUID passGuid{};
    dwRet = FveAddAuthMethodPassphrase(hVolD, L"short", &passGuid);
    TEST_ASSERT(dwRet == FVE_E_PASSPHRASE_TOO_SHORT, "Short passphrase (<8 chars) must fail");

    // Add valid passphrase
    dwRet = FveAddAuthMethodPassphrase(hVolD, L"SovereignSecureP@ss123!", &passGuid);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveAddAuthMethodPassphrase must succeed");

    // Add 48-digit numerical recovery password
    GUID recGuid{};
    dwRet = FveAddAuthMethodRecoveryPassword(hVolD, nullptr, &recGuid);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveAddAuthMethodRecoveryPassword must succeed");

    wchar_t dRecPwd[64]{ 0 };
    dwRet = FveGetRecoveryPassword(hVolD, &recGuid, dRecPwd, 64);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveGetRecoveryPassword by GUID must succeed");
    TEST_ASSERT(ValidateBitLockerRecoveryPassword(dRecPwd), "D: recovery password must validate");

    // Add TPM protector
    GUID tpmGuid{};
    dwRet = FveAddAuthMethodTpm(hVolD, 0, &tpmGuid);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveAddAuthMethodTpm must succeed");

    // Enumerate protectors: should be 3
    PFVE_AUTH_METHOD_LIST pListD = nullptr;
    dwRet = FveGetAuthMethodList(hVolD, &pListD);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveGetAuthMethodList for D: must succeed");
    TEST_ASSERT(pListD != nullptr && pListD->dwNumberOfItems == 3, "D: must have 3 enrolled key protectors");
    FveFreeMemory(pListD);

    // Remove TPM protector
    dwRet = FveRemoveAuthMethod(hVolD, &tpmGuid);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveRemoveAuthMethod must succeed");

    pListD = nullptr;
    dwRet = FveGetAuthMethodList(hVolD, &pListD);
    TEST_ASSERT(dwRet == ERROR_SUCCESS && pListD != nullptr && pListD->dwNumberOfItems == 2,
                "D: must have 2 protectors after TPM removal");
    FveFreeMemory(pListD);

    // Turn on BitLocker encryption
    dwRet = FveTurnOn(hVolD, FVE_ENCRYPTION_METHOD_XTS_AES_256, 0);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveTurnOn must succeed");

    dwRet = FveGetStatus(hVolD, &statusD);
    TEST_ASSERT(statusD.ProtectionStatus == FVE_PROTECTION_STATUS_ON, "D: ProtectionStatus must be ON");
    TEST_ASSERT(statusD.ConversionStatus == FVE_CONVERSION_STATUS_FULLY_ENCRYPTED, "D: ConversionStatus must be Fully Encrypted");

    // Test Pause / Resume
    dwRet = FvePause(hVolD);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FvePause must succeed");
    FveGetStatus(hVolD, &statusD);
    TEST_ASSERT(statusD.ProtectionStatus == FVE_PROTECTION_STATUS_SUSPENDED, "D: ProtectionStatus must be Suspended after Pause");

    dwRet = FveResume(hVolD);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveResume must succeed");
    FveGetStatus(hVolD, &statusD);
    TEST_ASSERT(statusD.ProtectionStatus == FVE_PROTECTION_STATUS_ON, "D: ProtectionStatus must be ON after Resume");

    // Lock volume D:
    dwRet = FveLockVolume(hVolD, 0);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveLockVolume must succeed");
    FveGetStatus(hVolD, &statusD);
    TEST_ASSERT(statusD.LockStatus == FVE_LOCK_STATUS_LOCKED, "D: LockStatus must be LOCKED");

    // Unlock with invalid passphrase
    dwRet = FveUnlockVolumeWithPassphrase(hVolD, L"IncorrectPassword", 0);
    TEST_ASSERT(dwRet == ERROR_ACCESS_DENIED, "FveUnlockVolumeWithPassphrase must reject incorrect password");
    FveGetStatus(hVolD, &statusD);
    TEST_ASSERT(statusD.LockStatus == FVE_LOCK_STATUS_LOCKED, "D: must remain LOCKED after failed unlock");

    // Unlock with valid passphrase
    dwRet = FveUnlockVolumeWithPassphrase(hVolD, L"SovereignSecureP@ss123!", 0);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveUnlockVolumeWithPassphrase must succeed with correct password");
    FveGetStatus(hVolD, &statusD);
    TEST_ASSERT(statusD.LockStatus == FVE_LOCK_STATUS_UNLOCKED, "D: must be UNLOCKED after valid passphrase");

    // Lock again and unlock with 48-digit recovery password
    dwRet = FveLockVolume(hVolD, 0);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveLockVolume second lock must succeed");

    dwRet = FveUnlockVolumeWithRecoveryPassword(hVolD, dRecPwd, 0);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveUnlockVolumeWithRecoveryPassword must succeed with valid 48-digit key");
    FveGetStatus(hVolD, &statusD);
    TEST_ASSERT(statusD.LockStatus == FVE_LOCK_STATUS_UNLOCKED, "D: must be UNLOCKED after valid recovery key");

    // Turn off BitLocker (Decrypt)
    dwRet = FveTurnOff(hVolD, 0);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FveTurnOff must succeed");
    FveGetStatus(hVolD, &statusD);
    TEST_ASSERT(statusD.ProtectionStatus == FVE_PROTECTION_STATUS_OFF, "D: ProtectionStatus must be OFF after TurnOff");
    TEST_ASSERT(statusD.ConversionStatus == FVE_CONVERSION_STATUS_FULLY_DECRYPTED, "D: ConversionStatus must be Fully Decrypted");

    FveCloseVolume(hVolD);

    // 7. Test Shell CLI: manage-bde / bde
    shell::CommandShell proc;
    std::ostringstream oss;

    // Self-test
    int shellRet = proc.execute("manage-bde test", oss);
    TEST_ASSERT(shellRet == 0, "manage-bde test must return 0");
    TEST_ASSERT(oss.str().find("[SUCCESS] Windows BitLocker & FVE Diagnostics passed cleanly.") != std::string::npos,
                "manage-bde test diagnostics must report clean pass");

    // Status query
    oss.str("");
    shellRet = proc.execute("manage-bde -status", oss);
    TEST_ASSERT(shellRet == 0, "manage-bde -status must return 0");
    TEST_ASSERT(oss.str().find("Volume C:") != std::string::npos, "manage-bde -status must report Volume C:");
    TEST_ASSERT(oss.str().find("Fully Encrypted") != std::string::npos, "manage-bde -status must report Fully Encrypted for C:");
    TEST_ASSERT(oss.str().find("Protection On") != std::string::npos, "manage-bde -status must report Protection On for C:");

    // Enumerate protectors CLI
    oss.str("");
    shellRet = proc.execute("manage-bde -protectors -get C:", oss);
    TEST_ASSERT(shellRet == 0, "manage-bde -protectors -get C: must return 0");
    TEST_ASSERT(oss.str().find("TPM") != std::string::npos, "protectors list must show TPM");
    TEST_ASSERT(oss.str().find("Numerical Password") != std::string::npos, "protectors list must show Numerical Password");

    // Single volume status
    oss.str("");
    shellRet = proc.execute("bde -status D:", oss);
    TEST_ASSERT(shellRet == 0, "bde -status D: must return 0");
    TEST_ASSERT(oss.str().find("Volume D:") != std::string::npos, "bde -status D: must display volume D:");

    std::cout << "[TEST] Suite 131: Windows BitLocker & FVE Subsystem PASSED.\n";
}

// ============================================================================
// Suite 132: Windows Filtering Platform (WFP) & Advanced Firewall Subsystem
// ============================================================================
void Test_WindowsFilteringPlatform_Firewall_Subsystem() {
    using namespace micant::wfp;

    // 1. Initialize subsystem exports
    InitializeWfpSubsystemExports();

    // 2. Validate DynamicLoader exports for fwpuclnt.dll
    auto& loader = ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("fwpuclnt.dll", "FwpmEngineOpen0") != nullptr, "fwpuclnt.dll must export FwpmEngineOpen0");
    TEST_ASSERT(loader.getExport("fwpuclnt.dll", "FwpmEngineClose0") != nullptr, "fwpuclnt.dll must export FwpmEngineClose0");
    TEST_ASSERT(loader.getExport("fwpuclnt.dll", "FwpmFilterAdd0") != nullptr, "fwpuclnt.dll must export FwpmFilterAdd0");
    TEST_ASSERT(loader.getExport("fwpuclnt.dll", "FwpmFilterDeleteById0") != nullptr, "fwpuclnt.dll must export FwpmFilterDeleteById0");
    TEST_ASSERT(loader.getExport("fwpuclnt.dll", "FwpmFilterGetById0") != nullptr, "fwpuclnt.dll must export FwpmFilterGetById0");
    TEST_ASSERT(loader.getExport("fwpuclnt.dll", "FwpmSubLayerAdd0") != nullptr, "fwpuclnt.dll must export FwpmSubLayerAdd0");
    TEST_ASSERT(loader.getExport("fwpuclnt.dll", "FwpmSubLayerDeleteById0") != nullptr, "fwpuclnt.dll must export FwpmSubLayerDeleteById0");
    TEST_ASSERT(loader.getExport("fwpuclnt.dll", "FwpmFreeMemory0") != nullptr, "fwpuclnt.dll must export FwpmFreeMemory0");

    // 3. Verify VersionDatabase registration
    auto modInfo = version::VersionDatabase::Instance().GetModuleInfo("fwpuclnt.dll");
    TEST_ASSERT(modInfo != nullptr, "fwpuclnt.dll must be registered in VersionDatabase");
    TEST_ASSERT(modInfo->stringTable.at("FileVersion") == "10.0.22621.1", "fwpuclnt.dll version must be 10.0.22621.1");
    TEST_ASSERT(modInfo->stringTable.at("FileDescription") == "Windows Filtering Platform API Client",
                "fwpuclnt.dll description must match Windows WFP");

    // 4. Open & Close Engine Session
    HANDLE hEngine = nullptr;
    FWPM_SESSION0 session{};
    session.displayDataName = L"Unit Test Engine Session";
    DWORD dwRet = FwpmEngineOpen0(nullptr, 0, nullptr, &session, &hEngine);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FwpmEngineOpen0 must succeed");
    TEST_ASSERT(hEngine != nullptr, "Engine handle must be valid");

    // 5. Layer Hierarchy Inspection
    std::vector<FWPM_LAYER0> layers;
    dwRet = SovereignWfpManager::get().getAllLayers(layers);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "getAllLayers must succeed");
    TEST_ASSERT(layers.size() >= 6, "At least 6 standard filtering layers must be registered");

    // 6. SubLayer Hierarchy Inspection & Dynamic Add/Delete
    std::vector<FWPM_SUBLAYER0> sublayers;
    dwRet = SovereignWfpManager::get().getAllSubLayers(sublayers);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "getAllSubLayers must succeed");
    TEST_ASSERT(sublayers.size() >= 2, "At least 2 sublayers (Universal & Firewall) must be present");

    GUID dynSubLayerGuid = { 0x11223344, 0x5566, 0x7788, { 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x01 } };
    FWPM_SUBLAYER0 dynSubLayer{};
    dynSubLayer.subLayerKey = dynSubLayerGuid;
    dynSubLayer.displayDataName = L"Dynamic Anti-Malware SubLayer";
    dynSubLayer.weight = 0x2000;
    dwRet = FwpmSubLayerAdd0(hEngine, &dynSubLayer, nullptr);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FwpmSubLayerAdd0 must succeed");

    dwRet = FwpmSubLayerDeleteById0(hEngine, &dynSubLayerGuid);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FwpmSubLayerDeleteById0 must succeed");

    // 7. Dynamic Filter Rule Creation, Query & Deletion
    FWPM_FILTER0 filter{};
    filter.displayDataName = L"Port 8080 Block Rule";
    filter.layerKey = FWPM_LAYER_INBOUND_TRANSPORT_V4;
    filter.subLayerKey = FWPM_SUBLAYER_FIREWALL;
    filter.action.type = FWP_ACTION_BLOCK;

    UINT64 filterId = 0;
    dwRet = FwpmFilterAdd0(hEngine, &filter, nullptr, &filterId);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FwpmFilterAdd0 must succeed");
    TEST_ASSERT(filterId != 0, "FwpmFilterAdd0 must generate positive filter ID");

    FWPM_FILTER0* pRetrieved = nullptr;
    dwRet = FwpmFilterGetById0(hEngine, filterId, &pRetrieved);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FwpmFilterGetById0 must succeed");
    TEST_ASSERT(pRetrieved != nullptr, "Retrieved filter must be non-null");
    TEST_ASSERT(pRetrieved->filterId == filterId, "Filter ID must match");
    TEST_ASSERT(pRetrieved->action.type == FWP_ACTION_BLOCK, "Filter action must match FWP_ACTION_BLOCK");
    FwpmFreeMemory0(reinterpret_cast<void**>(&pRetrieved));

    dwRet = FwpmFilterDeleteById0(hEngine, filterId);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FwpmFilterDeleteById0 must succeed");

    dwRet = FwpmFilterDeleteById0(hEngine, 999999);
    TEST_ASSERT(dwRet == FWP_E_FILTER_NOT_FOUND, "Deleting non-existent filter must return FWP_E_FILTER_NOT_FOUND");

    dwRet = FwpmEngineClose0(hEngine);
    TEST_ASSERT(dwRet == ERROR_SUCCESS, "FwpmEngineClose0 must succeed");

    // 8. Sovereign Packet Classifier & Firewall Rules
    NetworkPacket dnsPacket{};
    dnsPacket.direction = FWP_DIRECTION_OUTBOUND;
    dnsPacket.protocol = FWP_IPPROTO_UDP;
    dnsPacket.dstPort = 53;
    TEST_ASSERT(SovereignWfpManager::get().classifyPacket(dnsPacket) == FWP_ACTION_PERMIT,
                "Outbound DNS must be permitted by default rule");

    NetworkPacket dropPacket{};
    dropPacket.direction = FWP_DIRECTION_INBOUND;
    dropPacket.protocol = FWP_IPPROTO_TCP;
    dropPacket.dstPort = 8088;
    TEST_ASSERT(SovereignWfpManager::get().classifyPacket(dropPacket) == FWP_ACTION_BLOCK,
                "Unsolicited inbound TCP port 8088 must be dropped by default inbound block policy");

    NetworkPacket rdpPacket{};
    rdpPacket.direction = FWP_DIRECTION_INBOUND;
    rdpPacket.protocol = FWP_IPPROTO_TCP;
    rdpPacket.dstPort = 3389;
    TEST_ASSERT(SovereignWfpManager::get().classifyPacket(rdpPacket) == FWP_ACTION_PERMIT,
                "Inbound RDP 3389 must be permitted by pre-seeded rule");

    FirewallRule blockIrc{};
    blockIrc.name = "Block-IRC-Out";
    blockIrc.direction = FWP_DIRECTION_OUTBOUND;
    blockIrc.action = FWP_ACTION_BLOCK;
    blockIrc.protocol = FWP_IPPROTO_TCP;
    blockIrc.remotePort = 6667;
    blockIrc.enabled = true;
    SovereignWfpManager::get().addFirewallRule(blockIrc);

    NetworkPacket ircPacket{};
    ircPacket.direction = FWP_DIRECTION_OUTBOUND;
    ircPacket.protocol = FWP_IPPROTO_TCP;
    ircPacket.dstPort = 6667;
    TEST_ASSERT(SovereignWfpManager::get().classifyPacket(ircPacket) == FWP_ACTION_BLOCK,
                "Outbound IRC port 6667 must be blocked by explicit rule");

    TEST_ASSERT(SovereignWfpManager::get().deleteFirewallRuleByName("Block-IRC-Out"),
                "Deleting rule by name must succeed");
    TEST_ASSERT(SovereignWfpManager::get().classifyPacket(ircPacket) == FWP_ACTION_PERMIT,
                "Outbound IRC port 6667 must revert to default allow after rule deletion");

    SovereignWfpManager::get().setProfileState(FW_PROFILE_TYPE_ALL, false);
    TEST_ASSERT(SovereignWfpManager::get().classifyPacket(dropPacket) == FWP_ACTION_PERMIT,
                "When firewall is disabled, all inbound packets must be permitted");
    SovereignWfpManager::get().setProfileState(FW_PROFILE_TYPE_ALL, true);
    TEST_ASSERT(SovereignWfpManager::get().classifyPacket(dropPacket) == FWP_ACTION_BLOCK,
                "When firewall is re-enabled, inbound packet must be dropped");

    // 9. Shell CLI Integration: firewall / advfirewall / netsh
    shell::CommandShell proc;
    std::ostringstream oss;

    int shellRet = proc.execute("firewall test", oss);
    TEST_ASSERT(shellRet == 0, "firewall test must return 0");
    TEST_ASSERT(oss.str().find("[SUCCESS] Windows Filtering Platform & Firewall Diagnostics passed cleanly.") != std::string::npos,
                "firewall test diagnostics must pass cleanly");

    oss.str("");
    shellRet = proc.execute("netsh advfirewall show allprofiles", oss);
    TEST_ASSERT(shellRet == 0, "netsh advfirewall show allprofiles must return 0");
    TEST_ASSERT(oss.str().find("Domain Profile Settings:") != std::string::npos, "Output must contain Domain Profile");
    TEST_ASSERT(oss.str().find("Private Profile Settings:") != std::string::npos, "Output must contain Private Profile");
    TEST_ASSERT(oss.str().find("Public Profile Settings:") != std::string::npos, "Output must contain Public Profile");

    oss.str("");
    shellRet = proc.execute("netsh advfirewall firewall show rule", oss);
    TEST_ASSERT(shellRet == 0, "firewall show rule must return 0");
    TEST_ASSERT(oss.str().find("Rule Name:") != std::string::npos, "Must list firewall rule names");
    TEST_ASSERT(oss.str().find("Remote Desktop") != std::string::npos, "Must list Remote Desktop rule");

    oss.str("");
    shellRet = proc.execute("netsh advfirewall firewall add rule name=\"Block-SSH\" dir=out action=block protocol=tcp remoteport=22", oss);
    TEST_ASSERT(shellRet == 0, "firewall add rule must return 0");

    NetworkPacket sshPacket{};
    sshPacket.direction = FWP_DIRECTION_OUTBOUND;
    sshPacket.protocol = FWP_IPPROTO_TCP;
    sshPacket.dstPort = 22;
    TEST_ASSERT(SovereignWfpManager::get().classifyPacket(sshPacket) == FWP_ACTION_BLOCK,
                "SSH outbound must be blocked by CLI added rule");

    oss.str("");
    shellRet = proc.execute("netsh advfirewall firewall delete rule name=\"Block-SSH\"", oss);
    TEST_ASSERT(shellRet == 0, "firewall delete rule must return 0");
    TEST_ASSERT(SovereignWfpManager::get().classifyPacket(sshPacket) == FWP_ACTION_PERMIT,
                "SSH outbound must revert to permit after CLI deletion");

    std::cout << "[TEST] Suite 132: Windows Filtering Platform & Firewall Subsystem PASSED.\n";
}

// ============================================================================
// Suite 133: Windows Authenticode, Code Integrity & WinTrust Subsystem Tests
// ============================================================================
void Test_WindowsAuthenticode_WinTrust_Subsystem() {
    using namespace micant::wintrust;
    std::cout << "[TEST] Running Suite 133: Windows Authenticode & WinTrust Subsystem...\n";

    // 1. DynamicLoader Export Resolution
    InitializeWinTrustSubsystemExports();
    auto& ldr = ldr::DynamicLoader::get();

    void* pfnWinVerifyTrust = ldr.getExport("wintrust.dll", "WinVerifyTrust");
    TEST_ASSERT(pfnWinVerifyTrust != nullptr, "wintrust.dll must export WinVerifyTrust");

    void* pfnGetPolicy = ldr.getExport("wintrust.dll", "WintrustGetRegPolicyFlags");
    TEST_ASSERT(pfnGetPolicy != nullptr, "wintrust.dll must export WintrustGetRegPolicyFlags");

    void* pfnSetPolicy = ldr.getExport("wintrust.dll", "WintrustSetRegPolicyFlags");
    TEST_ASSERT(pfnSetPolicy != nullptr, "wintrust.dll must export WintrustSetRegPolicyFlags");

    void* pfnCatAcquire = ldr.getExport("wintrust.dll", "CryptCATAdminAcquireContext");
    TEST_ASSERT(pfnCatAcquire != nullptr, "wintrust.dll must export CryptCATAdminAcquireContext");

    void* pfnCatEnum = ldr.getExport("wintrust.dll", "CryptCATAdminEnumCatalogFromHash");
    TEST_ASSERT(pfnCatEnum != nullptr, "wintrust.dll must export CryptCATAdminEnumCatalogFromHash");

    // 2. Registry Policy Flags Configuration
    uint32_t origPolicy = 0;
    WintrustGetRegPolicyFlags(&origPolicy);
    WintrustSetRegPolicyFlags(origPolicy | WTPF_TRUSTTEST | WTPF_IGNOREEXPIRATION);
    uint32_t modifiedPolicy = 0;
    WintrustGetRegPolicyFlags(&modifiedPolicy);
    TEST_ASSERT((modifiedPolicy & WTPF_TRUSTTEST) != 0, "WTPF_TRUSTTEST must be set");
    TEST_ASSERT((modifiedPolicy & WTPF_IGNOREEXPIRATION) != 0, "WTPF_IGNOREEXPIRATION must be set");
    WintrustSetRegPolicyFlags(origPolicy); // Reset

    // 3. Synthesize PE binary for Authenticode tests
    std::vector<uint8_t> peImage(2048, 0);
    auto* dos = reinterpret_cast<pe::ImageDosHeader*>(peImage.data());
    dos->e_magic = pe::DOS_MAGIC;
    dos->e_lfanew = 128;
    *reinterpret_cast<uint32_t*>(peImage.data() + 128) = pe::NT_SIGNATURE;

    auto* fileHdr = reinterpret_cast<pe::ImageFileHeader*>(peImage.data() + 132);
    fileHdr->machine = pe::MACHINE_AMD64;
    fileHdr->numberOfSections = 2;
    fileHdr->sizeOfOptionalHeader = sizeof(pe::ImageOptionalHeader64);

    auto* optHdr = reinterpret_cast<pe::ImageOptionalHeader64*>(peImage.data() + 132 + sizeof(pe::ImageFileHeader));
    optHdr->magic = pe::PE32PLUS_MAGIC;
    optHdr->sizeOfHeaders = 512;
    optHdr->numberOfRvaAndSizes = 16;
    optHdr->checkSum = 0x12345678;

    auto* sec1 = reinterpret_cast<pe::ImageSectionHeader*>(peImage.data() + 132 + sizeof(pe::ImageFileHeader) + sizeof(pe::ImageOptionalHeader64));
    std::memcpy(sec1->name, ".text\0\0\0", 8);
    sec1->misc.virtualSize = 512;
    sec1->virtualAddress = 0x1000;
    sec1->sizeOfRawData = 512;
    sec1->pointerToRawData = 512;
    for (size_t i = 512; i < 1024; ++i) peImage[i] = static_cast<uint8_t>(i & 0xFF);

    auto* sec2 = reinterpret_cast<pe::ImageSectionHeader*>(reinterpret_cast<uint8_t*>(sec1) + sizeof(pe::ImageSectionHeader));
    std::memcpy(sec2->name, ".rdata\0\0", 8);
    sec2->misc.virtualSize = 512;
    sec2->virtualAddress = 0x2000;
    sec2->sizeOfRawData = 512;
    sec2->pointerToRawData = 1024;
    for (size_t i = 1024; i < 1536; ++i) peImage[i] = static_cast<uint8_t>((i * 7) & 0xFF);

    // 4. Test PE Authenticode Hashing Calculation
    std::vector<uint8_t> hashSha256 = SovereignWinTrustManager::calculatePeAuthenticodeHash(peImage.data(), 1536, true);
    TEST_ASSERT(hashSha256.size() == 32, "Authenticode SHA-256 digest must be 32 bytes");

    std::vector<uint8_t> hashSha1 = SovereignWinTrustManager::calculatePeAuthenticodeHash(peImage.data(), 1536, false);
    TEST_ASSERT(hashSha1.size() == 20, "Authenticode SHA-1 digest must be 20 bytes");

    // CheckSum should be skipped: altering checksum in OptionalHeader must NOT alter Authenticode hash
    optHdr->checkSum = 0xDEADBEEF;
    std::vector<uint8_t> hashAfterChecksumChange = SovereignWinTrustManager::calculatePeAuthenticodeHash(peImage.data(), 1536, true);
    TEST_ASSERT(hashSha256 == hashAfterChecksumChange, "Authenticode hash must be invariant under PE checksum modification");

    // 5. Test Signing & Embedding Authenticode Signature
    AuthenticodeSignerInfo signer{};
    signer.subject = "CN=MicaNT Sovereign Code Signing CA, O=MicaNT, C=US";
    signer.issuer = "CN=MicaNT Root Certificate Authority, O=MicaNT, C=US";
    signer.serialNumber = "100200300400";
    signer.thumbprintSha1 = "A1B2C3D4E5F60123456789ABCDEF0123456789AB";
    signer.thumbprintSha256 = "0102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F20";
    signer.isTrustedRoot = true;
    signer.isDriverSigned = true;
    signer.notBefore = 1000;
    signer.notAfter = 2000000000ULL;

    std::vector<uint8_t> signedPe = SovereignWinTrustManager::get().signPeBinary(peImage.data(), 1536, signer);
    TEST_ASSERT(signedPe.size() > 1536, "Signed PE must contain attribute certificate table appended");

    // Verify embedded signature extraction
    AuthenticodeSignerInfo extractedSigner{};
    bool hasSig = SovereignWinTrustManager::get().getEmbeddedSignature(signedPe.data(), signedPe.size(), extractedSigner);
    TEST_ASSERT(hasSig, "Signed PE must yield valid embedded signature");
    TEST_ASSERT(extractedSigner.subject == signer.subject, "Signer subject must match");
    TEST_ASSERT(extractedSigner.issuer == signer.issuer, "Signer issuer must match");
    TEST_ASSERT(extractedSigner.serialNumber == signer.serialNumber, "Serial number must match");
    TEST_ASSERT(extractedSigner.thumbprintSha1 == signer.thumbprintSha1, "SHA-1 thumbprint must match");
    TEST_ASSERT(extractedSigner.thumbprintSha256 == signer.thumbprintSha256, "SHA-256 thumbprint must match");

    // 6. Test WinVerifyTrust with Valid Signed Binary
    SovereignWinTrustManager::get().setVirtualFile("C:\\MicaNT\\ValidSigned.dll", signedPe);

    WINTRUST_FILE_INFO fileInfo{};
    fileInfo.pcwszFilePath = L"C:\\MicaNT\\ValidSigned.dll";

    WINTRUST_DATA wvtData{};
    wvtData.dwUnionChoice = WTD_CHOICE_FILE;
    wvtData.pFile = &fileInfo;

    GUID actionVerify = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    int32_t status = WinVerifyTrust(nullptr, &actionVerify, &wvtData);
    TEST_ASSERT(status == TRUST_E_SUCCESS, "WinVerifyTrust on valid signed binary must return TRUST_E_SUCCESS (0)");

    // 7. Tampering Detection: modify code byte in text section
    std::vector<uint8_t> tamperedPe = signedPe;
    tamperedPe[600] ^= 0x55; // Flip bits in .text section
    SovereignWinTrustManager::get().setVirtualFile("C:\\MicaNT\\Tampered.dll", tamperedPe);
    fileInfo.pcwszFilePath = L"C:\\MicaNT\\Tampered.dll";

    int32_t tamperStatus = WinVerifyTrust(nullptr, &actionVerify, &wvtData);
    TEST_ASSERT(tamperStatus == TRUST_E_BAD_DIGEST, "WinVerifyTrust on tampered binary must return TRUST_E_BAD_DIGEST (0x80096010)");

    // 8. Unsigned Binary Detection
    SovereignWinTrustManager::get().setVirtualFile("C:\\MicaNT\\Unsigned.exe", std::vector<uint8_t>(peImage.data(), peImage.data() + 1536));
    fileInfo.pcwszFilePath = L"C:\\MicaNT\\Unsigned.exe";
    int32_t unsignedStatus = WinVerifyTrust(nullptr, &actionVerify, &wvtData);
    TEST_ASSERT(unsignedStatus == TRUST_E_NOSIGNATURE, "WinVerifyTrust on unsigned binary without catalog must return TRUST_E_NOSIGNATURE (0x800B0100)");

    // 9. Expired Certificate Handling
    AuthenticodeSignerInfo expiredSigner = signer;
    expiredSigner.notBefore = 1000;
    expiredSigner.notAfter = 5000; // Far in past
    std::vector<uint8_t> expiredPe = SovereignWinTrustManager::get().signPeBinary(peImage.data(), 1536, expiredSigner);
    SovereignWinTrustManager::get().setVirtualFile("C:\\MicaNT\\Expired.dll", expiredPe);
    fileInfo.pcwszFilePath = L"C:\\MicaNT\\Expired.dll";

    int32_t expiredStatus = WinVerifyTrust(nullptr, &actionVerify, &wvtData);
    TEST_ASSERT(expiredStatus == CERT_E_EXPIRED, "WinVerifyTrust on expired cert must return CERT_E_EXPIRED (0x800B0101)");

    // With policy flag WTPF_IGNOREEXPIRATION
    WintrustSetRegPolicyFlags(origPolicy | WTPF_IGNOREEXPIRATION);
    int32_t ignoreExpiredStatus = WinVerifyTrust(nullptr, &actionVerify, &wvtData);
    TEST_ASSERT(ignoreExpiredStatus == TRUST_E_SUCCESS, "Expired cert must succeed when WTPF_IGNOREEXPIRATION policy is set");
    WintrustSetRegPolicyFlags(origPolicy);

    // 10. Revoked Certificate Handling
    SovereignWinTrustManager::get().revokeCertificate(signer.thumbprintSha1);
    fileInfo.pcwszFilePath = L"C:\\MicaNT\\ValidSigned.dll";
    int32_t revokedStatus = WinVerifyTrust(nullptr, &actionVerify, &wvtData);
    TEST_ASSERT(revokedStatus == CERT_E_REVOKED, "WinVerifyTrust on revoked cert must return CERT_E_REVOKED (0x800B010C)");

    // With policy flag WTPF_IGNOREREVOKATION
    WintrustSetRegPolicyFlags(origPolicy | WTPF_IGNOREREVOKATION);
    int32_t ignoreRevokedStatus = WinVerifyTrust(nullptr, &actionVerify, &wvtData);
    TEST_ASSERT(ignoreRevokedStatus == TRUST_E_SUCCESS, "Revoked cert must succeed when WTPF_IGNOREREVOKATION is set");
    WintrustSetRegPolicyFlags(origPolicy);
    SovereignWinTrustManager::get().clearRevocations();

    // 11. Untrusted Root Handling
    AuthenticodeSignerInfo untrustedSigner = signer;
    untrustedSigner.isTrustedRoot = false;
    std::vector<uint8_t> untrustedPe = SovereignWinTrustManager::get().signPeBinary(peImage.data(), 1536, untrustedSigner);
    SovereignWinTrustManager::get().setVirtualFile("C:\\MicaNT\\Untrusted.dll", untrustedPe);
    fileInfo.pcwszFilePath = L"C:\\MicaNT\\Untrusted.dll";

    int32_t untrustedStatus = WinVerifyTrust(nullptr, &actionVerify, &wvtData);
    TEST_ASSERT(untrustedStatus == CERT_E_UNTRUSTEDROOT, "WinVerifyTrust with untrusted root must return CERT_E_UNTRUSTEDROOT (0x800B0109)");

    // Test signing override
    WintrustSetRegPolicyFlags(origPolicy | WTPF_TRUSTTEST);
    int32_t testRootStatus = WinVerifyTrust(nullptr, &actionVerify, &wvtData);
    TEST_ASSERT(testRootStatus == TRUST_E_SUCCESS, "Untrusted cert must succeed when WTPF_TRUSTTEST is set");
    WintrustSetRegPolicyFlags(origPolicy);

    // 12. Driver Code Integrity Policy (DRIVER_ACTION_VERIFY)
    GUID driverAction = DRIVER_ACTION_VERIFY;
    fileInfo.pcwszFilePath = L"C:\\MicaNT\\ValidSigned.dll";
    int32_t driverStatus = WinVerifyTrust(nullptr, &driverAction, &wvtData);
    TEST_ASSERT(driverStatus == TRUST_E_SUCCESS, "Driver with valid KMCS signature must pass DRIVER_ACTION_VERIFY");

    AuthenticodeSignerInfo userSigner = signer;
    userSigner.isDriverSigned = false; // Ordinary user-mode certificate
    std::vector<uint8_t> userSignedPe = SovereignWinTrustManager::get().signPeBinary(peImage.data(), 1536, userSigner);
    SovereignWinTrustManager::get().setVirtualFile("C:\\MicaNT\\UserModeOnly.sys", userSignedPe);
    fileInfo.pcwszFilePath = L"C:\\MicaNT\\UserModeOnly.sys";
    int32_t driverFailStatus = WinVerifyTrust(nullptr, &driverAction, &wvtData);
    TEST_ASSERT(driverFailStatus == TRUST_E_SUBJECT_NOT_TRUSTED, "Non-KMCS binary must fail DRIVER_ACTION_VERIFY with TRUST_E_SUBJECT_NOT_TRUSTED");

    // 13. Security Catalog (CatRoot) Subsystem
    HCATADMIN hCatAdmin = nullptr;
    TEST_ASSERT(CryptCATAdminAcquireContext(&hCatAdmin, nullptr, 0) == win32::TRUE, "CryptCATAdminAcquireContext must succeed");
    TEST_ASSERT(hCatAdmin != nullptr, "hCatAdmin must not be null");

    // Register a new catalog with member hash
    CatalogRecord testCat{};
    testCat.baseName = L"MicaNT_System_Core.cat";
    testCat.catalogPath = L"C:\\Windows\\System32\\CatRoot\\{F750E6C3-38EE-11d1-85E5-00C04FC295EE}\\MicaNT_System_Core.cat";
    testCat.signer.subject = "CN=Microsoft Windows Sovereign Component, O=MicaNT, C=US";
    testCat.signer.issuer = "CN=Microsoft Root Authority, O=MicaNT, C=US";
    testCat.signer.isTrustedRoot = true;
    testCat.signer.isDriverSigned = true;

    CatalogMemberRecord member{};
    member.memberTag = L"{3B3B1C2E-0001}";
    member.fileName = L"C:\\MicaNT\\Unsigned.exe";
    member.hash = SovereignWinTrustManager::calculatePeAuthenticodeHash(peImage.data(), 1536, true);
    testCat.members.push_back(member);

    SovereignWinTrustManager::get().registerCatalog(testCat);

    // Look up catalog from hash
    HCATINFO hCatFound = CryptCATAdminEnumCatalogFromHash(hCatAdmin, member.hash.data(), static_cast<uint32_t>(member.hash.size()), 0, nullptr);
    TEST_ASSERT(hCatFound != nullptr, "CryptCATAdminEnumCatalogFromHash must locate registered catalog by member hash");

    // Now, Unsigned.exe should pass WinVerifyTrust because it is covered by CatRoot!
    fileInfo.pcwszFilePath = L"C:\\MicaNT\\Unsigned.exe";
    int32_t catalogVerifyStatus = WinVerifyTrust(nullptr, &actionVerify, &wvtData);
    TEST_ASSERT(catalogVerifyStatus == TRUST_E_SUCCESS, "Unsigned binary covered by Security Catalog must verify successfully via CatRoot");

    TEST_ASSERT(CryptCATAdminReleaseCatalogContext(hCatAdmin, hCatFound, 0) == win32::TRUE, "Release catalog context must succeed");
    TEST_ASSERT(CryptCATAdminReleaseContext(hCatAdmin, 0) == win32::TRUE, "CryptCATAdminReleaseContext must succeed");

    // 14. Shell CLI Integration: signtool
    shell::CommandShell proc;
    std::ostringstream oss;

    int shellRet = proc.execute("signtool test", oss);
    TEST_ASSERT(shellRet == 0, "signtool test must return 0");
    TEST_ASSERT(oss.str().find("[SUCCESS] Windows Authenticode & WinTrust Diagnostics passed cleanly.") != std::string::npos,
                "signtool test diagnostics must report success");

    oss.str("");
    shellRet = proc.execute("signtool verify /v C:\\MicaNT\\ValidSigned.dll", oss);
    TEST_ASSERT(shellRet == 0, "signtool verify /v on signed binary must return 0");
    TEST_ASSERT(oss.str().find("Successfully verified: C:\\MicaNT\\ValidSigned.dll") != std::string::npos, "Must report successful verification");
    TEST_ASSERT(oss.str().find("Number of files successfully Verified: 1") != std::string::npos, "Verified count must be 1");
    TEST_ASSERT(oss.str().find("SHA256:") != std::string::npos, "Verbose output must show thumbprint");

    oss.str("");
    shellRet = proc.execute("signtool verify C:\\MicaNT\\Tampered.dll", oss);
    TEST_ASSERT(oss.str().find("TRUST_E_BAD_DIGEST") != std::string::npos, "signtool verify on tampered binary must report TRUST_E_BAD_DIGEST");

    oss.str("");
    shellRet = proc.execute("signtool catdb", oss);
    TEST_ASSERT(shellRet == 0, "signtool catdb must return 0");
    TEST_ASSERT(oss.str().find("MicaNT_System_Core.cat") != std::string::npos, "catdb must list registered system core catalog");

    std::cout << "[TEST] Suite 133: Windows Authenticode & WinTrust Subsystem PASSED.\n";
}

// ============================================================================
// Suite 134: Windows Code Integrity (CI) & WDAC Subsystem Tests
// ============================================================================
void Test_WindowsCodeIntegrity_WDAC_Subsystem() {
    using namespace micant::ci;
    std::cout << "[TEST] Running Suite 134: Windows Code Integrity & WDAC Subsystem...\n";

    // 1. DynamicLoader Export Resolution
    InitializeCiSubsystemExports();
    auto& ldr = ldr::DynamicLoader::get();

    void* pfnCiInit = ldr.getExport("ci.dll", "CiInitialize");
    TEST_ASSERT(pfnCiInit != nullptr, "ci.dll must export CiInitialize");

    void* pfnValidateHdr = ldr.getExport("ci.dll", "CiValidateImageHeader");
    TEST_ASSERT(pfnValidateHdr != nullptr, "ci.dll must export CiValidateImageHeader");

    void* pfnValidateData = ldr.getExport("ci.dll", "CiValidateImageData");
    TEST_ASSERT(pfnValidateData != nullptr, "ci.dll must export CiValidateImageData");

    void* pfnCiQuery = ldr.getExport("ci.dll", "CiQueryInformation");
    TEST_ASSERT(pfnCiQuery != nullptr, "ci.dll must export CiQueryInformation");

    void* pfnCiSet = ldr.getExport("ci.dll", "CiSetInformation");
    TEST_ASSERT(pfnCiSet != nullptr, "ci.dll must export CiSetInformation");

    void* pfnCiPolicy = ldr.getExport("ci.dll", "CiGetPolicyInformation");
    TEST_ASSERT(pfnCiPolicy != nullptr, "ci.dll must export CiGetPolicyInformation");

    // 2. Query and Set CI Options
    SYSTEM_CODEINTEGRITY_INFORMATION ciInfo{};
    TEST_ASSERT(CiQueryInformation(&ciInfo, sizeof(ciInfo)) == STATUS_SUCCESS, "CiQueryInformation must succeed");
    uint32_t defaultOpts = ciInfo.CodeIntegrityOptions;
    TEST_ASSERT((defaultOpts & CODEINTEGRITY_OPTION_ENABLED) != 0, "KMCI should be enabled by default");
    TEST_ASSERT((defaultOpts & CODEINTEGRITY_OPTION_HVCI_KMCI_ENABLED) != 0, "HVCI should be enabled by default");

    ciInfo.CodeIntegrityOptions |= CODEINTEGRITY_OPTION_TESTSIGN;
    TEST_ASSERT(CiSetInformation(&ciInfo, sizeof(ciInfo)) == STATUS_SUCCESS, "CiSetInformation must succeed");

    SYSTEM_CODEINTEGRITYPOLICY_INFORMATION polInfo{};
    TEST_ASSERT(CiGetPolicyInformation(&polInfo, sizeof(polInfo)) == STATUS_SUCCESS, "CiGetPolicyInformation must succeed");
    TEST_ASSERT((polInfo.Options & CODEINTEGRITY_OPTION_TESTSIGN) != 0, "TESTSIGN flag must be reflected in policy");

    // Reset options
    ciInfo.CodeIntegrityOptions = defaultOpts;
    CiSetInformation(&ciInfo, sizeof(ciInfo));

    // 3. Synthesize test PE binaries
    std::vector<uint8_t> testPe(1024, 0);
    auto* dos = reinterpret_cast<pe::ImageDosHeader*>(testPe.data());
    dos->e_magic = pe::DOS_MAGIC;
    dos->e_lfanew = 128;
    *reinterpret_cast<uint32_t*>(testPe.data() + 128) = pe::NT_SIGNATURE;

    auto* fileHdr = reinterpret_cast<pe::ImageFileHeader*>(testPe.data() + 132);
    fileHdr->machine = pe::MACHINE_AMD64;
    fileHdr->numberOfSections = 1;
    fileHdr->sizeOfOptionalHeader = sizeof(pe::ImageOptionalHeader64);

    auto* optHdr = reinterpret_cast<pe::ImageOptionalHeader64*>(testPe.data() + 132 + sizeof(pe::ImageFileHeader));
    optHdr->magic = pe::PE32PLUS_MAGIC;
    optHdr->sizeOfHeaders = 512;
    optHdr->numberOfRvaAndSizes = 16;

    auto* secHdr = reinterpret_cast<pe::ImageSectionHeader*>(testPe.data() + 132 + sizeof(pe::ImageFileHeader) + sizeof(pe::ImageOptionalHeader64));
    std::memcpy(secHdr->name, ".text\0\0\0", 8);
    secHdr->misc.virtualSize = 256;
    secHdr->virtualAddress = 0x1000;
    secHdr->sizeOfRawData = 256;
    secHdr->pointerToRawData = 512;
    for (size_t i = 512; i < 768; ++i) testPe[i] = 0x90; // NOPs

    // 4. Test KMCI (Kernel-Mode Code Integrity) Driver Validation
    uint8_t signingLevel = 0;
    NTSTATUS unsignedDriverStatus = CiValidateImageHeader(nullptr, L"C:\\Windows\\System32\\drivers\\bad_driver.sys",
                                                         testPe.data(), testPe.size(), 0x01 /* Driver */, &signingLevel);
    TEST_ASSERT(unsignedDriverStatus == STATUS_IMAGE_CERT_REVOKED, "Unsigned driver must be rejected by KMCI");
    TEST_ASSERT(signingLevel == SE_SIGNING_LEVEL_UNSIGNED, "Unsigned driver signing level must be UNSIGNED (1)");

    // Sign driver with valid KMCS signature
    wintrust::AuthenticodeSignerInfo kmcsSigner{};
    kmcsSigner.subject = "CN=Microsoft Windows Driver Component, O=Microsoft Corporation, C=US";
    kmcsSigner.issuer = "CN=Microsoft Windows Production PCA 2011, O=Microsoft Corporation, C=US";
    kmcsSigner.serialNumber = "8800000001";
    kmcsSigner.thumbprintSha1 = "AA11BB22CC33DD44EE55FF660011223344556677";
    kmcsSigner.thumbprintSha256 = "11223344556677889900AABBCCDDEEFF11223344556677889900AABBCCDDEEFF";
    kmcsSigner.isTrustedRoot = true;
    kmcsSigner.isDriverSigned = true;
    kmcsSigner.notBefore = 1000;
    kmcsSigner.notAfter = 2000000000ULL;

    std::vector<uint8_t> signedDriver = wintrust::SovereignWinTrustManager::get().signPeBinary(testPe.data(), testPe.size(), kmcsSigner);
    NTSTATUS signedDriverStatus = CiValidateImageHeader(nullptr, L"C:\\Windows\\System32\\drivers\\good_driver.sys",
                                                       signedDriver.data(), signedDriver.size(), 0x01 /* Driver */, &signingLevel);
    TEST_ASSERT(signedDriverStatus == STATUS_SUCCESS, "Signed KMCS driver must be allowed by KMCI");
    TEST_ASSERT(signingLevel >= SE_SIGNING_LEVEL_MICROSOFT, "KMCS driver signing level must be at least MICROSOFT (8)");

    // 5. Test UMCI (User-Mode Code Integrity) in Audit Mode
    SovereignCiManager::get().clearAuditLogs();
    SovereignCiManager::get().setOptions(CODEINTEGRITY_OPTION_ENABLED | CODEINTEGRITY_OPTION_UMCI_AUDIT);

    NTSTATUS auditStatus = CiValidateImageHeader(nullptr, L"C:\\Users\\admin\\app.exe",
                                                testPe.data(), testPe.size(), 0x00 /* User-mode */, &signingLevel);
    TEST_ASSERT(auditStatus == STATUS_SUCCESS, "In UMCI Audit Mode, unsigned executable must be permitted to run");
    auto auditLogs = SovereignCiManager::get().getAuditLogs();
    TEST_ASSERT(!auditLogs.empty(), "Audit log entry must be recorded in Audit Mode");

    // 6. Test UMCI in Enforced Mode
    SovereignCiManager::get().setOptions(CODEINTEGRITY_OPTION_ENABLED | CODEINTEGRITY_OPTION_UMCI_ENABLED);

    NTSTATUS enforcedStatus = CiValidateImageHeader(nullptr, L"C:\\Users\\admin\\app.exe",
                                                   testPe.data(), testPe.size(), 0x00 /* User-mode */, &signingLevel);
    TEST_ASSERT(enforcedStatus == STATUS_ACCESS_DENIED, "In UMCI Enforced Mode, unsigned binary must be blocked (STATUS_ACCESS_DENIED)");

    // 7. Test WDAC Whitelist Rules (Hash, Publisher, Path)
    // Hash Rule Allow
    std::vector<uint8_t> peHash = wintrust::SovereignWinTrustManager::calculatePeAuthenticodeHash(testPe.data(), testPe.size(), true);
    std::string peHashHex = wintrust::SovereignWinTrustManager::toHex(peHash.data(), peHash.size());

    WdacPolicyRule hashRule{};
    hashRule.ruleId = "Rule-Allow-TestPe";
    hashRule.ruleType = WdacRuleType::HashRule;
    hashRule.action = WdacRuleAction::Allow;
    hashRule.pattern = peHashHex;
    SovereignCiManager::get().addRule(hashRule);

    NTSTATUS hashAllowedStatus = CiValidateImageHeader(nullptr, L"C:\\Users\\admin\\app.exe",
                                                      testPe.data(), testPe.size(), 0x00, &signingLevel);
    TEST_ASSERT(hashAllowedStatus == STATUS_SUCCESS, "Binary matching allow Hash Rule must pass UMCI enforcement");

    // Deny Rule override
    WdacPolicyRule denyRule{};
    denyRule.ruleId = "Rule-Deny-TestPe";
    denyRule.ruleType = WdacRuleType::HashRule;
    denyRule.action = WdacRuleAction::Deny;
    denyRule.pattern = peHashHex;
    SovereignCiManager::get().addRule(denyRule);

    NTSTATUS hashDeniedStatus = CiValidateImageHeader(nullptr, L"C:\\Users\\admin\\app.exe",
                                                     testPe.data(), testPe.size(), 0x00, &signingLevel);
    TEST_ASSERT(hashDeniedStatus == STATUS_ACCESS_DENIED, "Explicit Deny rule must take precedence and block binary");

    SovereignCiManager::get().removeRule("Rule-Deny-TestPe");
    SovereignCiManager::get().removeRule("Rule-Allow-TestPe");
    SovereignCiManager::get().setOptions(defaultOpts);

    // 8. Shell CLI Integration: wdac
    shell::CommandShell proc;
    std::ostringstream oss;

    int shellRet = proc.execute("wdac test", oss);
    TEST_ASSERT(shellRet == 0, "wdac test must return 0");
    TEST_ASSERT(oss.str().find("[SUCCESS] Windows Code Integrity & WDAC Diagnostics passed cleanly.") != std::string::npos,
                "wdac test diagnostics must succeed");

    oss.str("");
    shellRet = proc.execute("wdac status", oss);
    TEST_ASSERT(shellRet == 0, "wdac status must return 0");
    TEST_ASSERT(oss.str().find("Kernel-Mode CI (KMCI):") != std::string::npos, "Must show KMCI state");
    TEST_ASSERT(oss.str().find("HVCI (Memory Integrity):") != std::string::npos, "Must show HVCI state");

    oss.str("");
    shellRet = proc.execute("wdac mode enforce", oss);
    TEST_ASSERT(shellRet == 0, "wdac mode enforce must return 0");
    TEST_ASSERT(oss.str().find("WDAC UMCI mode set to: Enforced") != std::string::npos, "Must report mode set");

    oss.str("");
    shellRet = proc.execute("wdac mode audit", oss);
    TEST_ASSERT(shellRet == 0, "wdac mode audit must return 0");

    oss.str("");
    shellRet = proc.execute("wdac rules", oss);
    TEST_ASSERT(shellRet == 0, "wdac rules must return 0");
    TEST_ASSERT(oss.str().find("Rule-System32-Allow") != std::string::npos, "Must list default rules");

    oss.str("");
    shellRet = proc.execute("wdac logs", oss);
    TEST_ASSERT(shellRet == 0, "wdac logs must return 0");

    std::cout << "[TEST] Suite 134: Windows Code Integrity & WDAC Subsystem PASSED.\n";
}

void Test_WindowsEncryptingFileSystem_EFS_Subsystem() {
    std::cout << "\n[TEST] Starting Suite 135: Windows Encrypting File System (EFS) & feclient.dll Subsystem...\n";
    using namespace micant::efs;
    InitializeEfsSubsystemExports();

    // 1. Initialization & Verification of Dynamic Loader
    uint32_t initRes = EfsClientInitialize();
    TEST_ASSERT(initRes == ERROR_SUCCESS, "EfsClientInitialize must return ERROR_SUCCESS");

    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("feclient.dll", "EncryptFileW") != nullptr, "feclient.dll!EncryptFileW must be exported");
    TEST_ASSERT(ldr.getExport("feclient.dll", "DecryptFileW") != nullptr, "feclient.dll!DecryptFileW must be exported");
    TEST_ASSERT(ldr.getExport("feclient.dll", "OpenEncryptedFileRawW") != nullptr, "feclient.dll!OpenEncryptedFileRawW must be exported");
    TEST_ASSERT(ldr.getExport("feclient.dll", "ReadEncryptedFileRaw") != nullptr, "feclient.dll!ReadEncryptedFileRaw must be exported");
    TEST_ASSERT(ldr.getExport("feclient.dll", "WriteEncryptedFileRaw") != nullptr, "feclient.dll!WriteEncryptedFileRaw must be exported");
    TEST_ASSERT(ldr.getExport("advapi32.dll", "EncryptFileW") != nullptr, "advapi32.dll!EncryptFileW must be exported");

    // 2. Pre-Encryption Status Verification
    const wchar_t* testFile = L"C:\\Users\\Default\\Documents\\TopSecret.doc";
    uint32_t status = 0xFFFFFFFF;
    uint32_t stRes = FileEncryptionStatusW(testFile, &status);
    TEST_ASSERT(stRes == ERROR_SUCCESS, "FileEncryptionStatusW must succeed");
    TEST_ASSERT(status == FILE_ENCRYPTABLE, "Unencrypted file must be FILE_ENCRYPTABLE");

    // 3. Transparent File Encryption
    std::string testContent = "MicaNT Dave Cutler 1988 MICA High-Assurance Sovereign EFS Payload 2026";
    std::vector<uint8_t> plainBytes(testContent.begin(), testContent.end());
    uint32_t encRes = SovereignEfsManager::get().encryptFile(testFile, plainBytes);
    TEST_ASSERT(encRes == ERROR_SUCCESS, "encryptFile must succeed");

    status = 0;
    stRes = FileEncryptionStatusW(testFile, &status);
    TEST_ASSERT(stRes == ERROR_SUCCESS, "FileEncryptionStatusW must succeed after encryption");
    TEST_ASSERT(status == FILE_IS_ENCRYPTED, "File status must be FILE_IS_ENCRYPTED");

    // 4. NTFS $EFS Stream Serialization & Deserialization
    EfsMetadata meta;
    meta.version = 2;
    meta.algorithmId = CALG_AES_256;
    meta.keySizeBits = 256;
    meta.iv.fill(0x42);
    meta.fekChecksum.fill(0x77);

    EfsUserKeyEntry u1{};
    u1.userSid = L"S-1-5-21-1001";
    u1.displayName = L"MicaAdmin";
    u1.certThumbprint = {0xAA, 0xBB, 0xCC, 0xDD};
    u1.encryptedFek = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    meta.ddfEntries.push_back(u1);

    EfsDraKeyEntry d1{};
    d1.draSid = L"S-1-5-32-544";
    d1.displayName = L"Builtin\\Administrators";
    d1.certThumbprint = {0xEE, 0xFF, 0x00, 0x11};
    d1.encryptedFek = {0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC};
    meta.drfEntries.push_back(d1);

    std::vector<uint8_t> serializedStream = meta.serialize();
    TEST_ASSERT(serializedStream.size() > sizeof(EFS_STREAM_HEADER), "Serialized stream must exceed header size");

    EfsMetadata parsedMeta;
    bool desRes = parsedMeta.deserialize(serializedStream);
    TEST_ASSERT(desRes, "Deserialization of $EFS stream must succeed");
    TEST_ASSERT(parsedMeta.version == 2, "Parsed version must be 2");
    TEST_ASSERT(parsedMeta.algorithmId == CALG_AES_256, "Parsed algorithm must be CALG_AES_256");
    TEST_ASSERT(parsedMeta.ddfEntries.size() == 1, "Parsed DDF count must be 1");
    TEST_ASSERT(parsedMeta.ddfEntries[0].userSid == L"S-1-5-21-1001", "Parsed user SID must match");
    TEST_ASSERT(parsedMeta.drfEntries.size() == 1, "Parsed DRF count must be 1");
    TEST_ASSERT(parsedMeta.drfEntries[0].draSid == L"S-1-5-32-544", "Parsed DRA SID must match");

    // 5. Query Users (DDF) and Recovery Agents (DRF)
    PENCRYPTION_CERTIFICATE_HASH_LIST pUsers = nullptr;
    uint32_t qUserRes = QueryUsersOnEncryptedFile(testFile, &pUsers);
    TEST_ASSERT(qUserRes == ERROR_SUCCESS, "QueryUsersOnEncryptedFile must succeed");
    TEST_ASSERT(pUsers != nullptr, "pUsers must not be null");
    TEST_ASSERT(pUsers->nCert_Hash >= 1, "Must have at least 1 user in DDF");
    TEST_ASSERT(pUsers->pUsers[0]->pHash != nullptr, "User cert hash must be present");
    TEST_ASSERT(pUsers->pUsers[0]->pHash->cbData == 32, "User cert hash must be 32 bytes (SHA-256)");
    FreeEncryptionCertificateHashList(pUsers);

    PENCRYPTION_CERTIFICATE_HASH_LIST pDra = nullptr;
    uint32_t qDraRes = QueryRecoveryAgentsOnEncryptedFile(testFile, &pDra);
    TEST_ASSERT(qDraRes == ERROR_SUCCESS, "QueryRecoveryAgentsOnEncryptedFile must succeed");
    TEST_ASSERT(pDra != nullptr, "pDra must not be null");
    TEST_ASSERT(pDra->nCert_Hash >= 1, "Must have at least 1 recovery agent in DRF");
    FreeEncryptionCertificateHashList(pDra);

    // 6. Transparent Read by Authorized Primary User
    std::wstring adminSid = SovereignEfsManager::get().getCurrentUserSid();
    std::vector<uint8_t> readBytes;
    uint32_t readRes = SovereignEfsManager::get().readFile(testFile, adminSid, readBytes);
    TEST_ASSERT(readRes == ERROR_SUCCESS, "Primary authorized user must read file transparently");
    TEST_ASSERT(readBytes == plainBytes, "Read bytes must match original plaintext");

    // 7. Multi-User Sharing: Add Collaborator Alice
    std::wstring aliceSid = L"S-1-5-21-3623811015-3361044348-30300820-2002";
    uint32_t addRes = SovereignEfsManager::get().addUserToFile(testFile, aliceSid, L"AliceCollaborator");
    TEST_ASSERT(addRes == ERROR_SUCCESS, "Adding Alice to DDF must succeed");

    std::vector<uint8_t> aliceBytes;
    uint32_t aliceReadRes = SovereignEfsManager::get().readFile(testFile, aliceSid, aliceBytes);
    TEST_ASSERT(aliceReadRes == ERROR_SUCCESS, "Alice must be able to read file transparently");
    TEST_ASSERT(aliceBytes == plainBytes, "Alice read bytes must match original plaintext");

    // 8. Access Control: Unauthorized User Mallory Must Be Denied
    std::wstring mallorySid = L"S-1-5-21-9999999999-9999999999-99999999-9999";
    std::vector<uint8_t> malloryBytes;
    uint32_t malloryReadRes = SovereignEfsManager::get().readFile(testFile, mallorySid, malloryBytes);
    TEST_ASSERT(malloryReadRes == ERROR_ACCESS_DENIED, "Mallory must be denied with ERROR_ACCESS_DENIED");

    // 9. Remove Alice and verify revocation
    uint32_t remRes = SovereignEfsManager::get().removeUserFromFile(testFile, aliceSid);
    TEST_ASSERT(remRes == ERROR_SUCCESS, "Removing Alice from DDF must succeed");

    aliceBytes.clear();
    aliceReadRes = SovereignEfsManager::get().readFile(testFile, aliceSid, aliceBytes);
    TEST_ASSERT(aliceReadRes == ERROR_ACCESS_DENIED, "Alice must be denied after removal from DDF");

    // 10. Raw Export & Import (Zero-Knowledge Backup & Restore)
    void* rawExport = nullptr;
    uint32_t rawOpenExp = OpenEncryptedFileRawW(testFile, 0, &rawExport);
    TEST_ASSERT(rawOpenExp == ERROR_SUCCESS, "OpenEncryptedFileRawW for export must succeed");
    TEST_ASSERT(rawExport != nullptr, "rawExport handle must not be null");

    struct BackupStore {
        std::vector<uint8_t> buffer;
    } backup;

    auto exportCallback = [](uint8_t* pbData, void* pvCallbackContext, uint32_t ulLength) -> uint32_t {
        auto* b = reinterpret_cast<BackupStore*>(pvCallbackContext);
        b->buffer.insert(b->buffer.end(), pbData, pbData + ulLength);
        return ERROR_SUCCESS;
    };

    while (ReadEncryptedFileRaw(exportCallback, &backup, rawExport) == ERROR_SUCCESS) {
    }
    CloseEncryptedFileRaw(rawExport);

    TEST_ASSERT(!backup.buffer.empty(), "Raw backup stream must contain bytes");
    TEST_ASSERT(backup.buffer.size() >= 16, "Raw backup stream must have header");
    uint32_t pkgMagic = 0;
    std::memcpy(&pkgMagic, backup.buffer.data(), 4);
    TEST_ASSERT(pkgMagic == EFS_RAW_PACKAGE_MAGIC, "Raw package magic must be 'REFS'");

    // Restore to new path: RestoredSecret.doc
    const wchar_t* restoredFile = L"C:\\Users\\Default\\Documents\\RestoredSecret.doc";
    void* rawImport = nullptr;
    uint32_t rawOpenImp = OpenEncryptedFileRawW(restoredFile, CREATE_FOR_IMPORT, &rawImport);
    TEST_ASSERT(rawOpenImp == ERROR_SUCCESS, "OpenEncryptedFileRawW for import must succeed");
    TEST_ASSERT(rawImport != nullptr, "rawImport handle must not be null");

    struct RestoreFeed {
        std::span<const uint8_t> data;
        size_t cursor{0};
    } feed{backup.buffer, 0};

    auto importCallback = [](uint8_t* pbData, void* pvCallbackContext, uint32_t* pulLength) -> uint32_t {
        auto* f = reinterpret_cast<RestoreFeed*>(pvCallbackContext);
        size_t remain = f->data.size() - f->cursor;
        if (remain == 0) {
            *pulLength = 0;
            return ERROR_SUCCESS;
        }
        uint32_t chunkSize = static_cast<uint32_t>(std::min<size_t>(remain, *pulLength));
        std::memcpy(pbData, f->data.data() + f->cursor, chunkSize);
        f->cursor += chunkSize;
        *pulLength = chunkSize;
        return ERROR_SUCCESS;
    };

    uint32_t writeRes = WriteEncryptedFileRaw(importCallback, &feed, rawImport);
    TEST_ASSERT(writeRes == ERROR_SUCCESS, "WriteEncryptedFileRaw must succeed");
    CloseEncryptedFileRaw(rawImport);

    // Verify restored file can be decrypted by Admin
    std::vector<uint8_t> restoredBytes;
    uint32_t restReadRes = SovereignEfsManager::get().readFile(restoredFile, adminSid, restoredBytes);
    TEST_ASSERT(restReadRes == ERROR_SUCCESS, "Restored file must be readable by Admin");
    TEST_ASSERT(restoredBytes == plainBytes, "Restored content must match original plaintext");

    // 11. DecryptFileW
    uint32_t decRes = DecryptFileW(restoredFile, 0);
    TEST_ASSERT(decRes == ERROR_SUCCESS, "DecryptFileW must return ERROR_SUCCESS");

    status = 0;
    FileEncryptionStatusW(restoredFile, &status);
    TEST_ASSERT(status == FILE_ENCRYPTABLE, "Decrypted file must return FILE_ENCRYPTABLE");

    // 12. Free Space Sanitization (DoD 5220.22-M)
    std::ostringstream wipeOss;
    bool wipeOk = SovereignEfsManager::get().wipeFreeSpace(L"C:\\Users\\Default", wipeOss);
    TEST_ASSERT(wipeOk, "wipeFreeSpace must return true");
    TEST_ASSERT(wipeOss.str().find("DoD 5220.22-M") != std::string::npos, "Wipe must mention DoD 5220.22-M");
    TEST_ASSERT(wipeOss.str().find("Pass 1/3") != std::string::npos, "Wipe must execute Pass 1");
    TEST_ASSERT(wipeOss.str().find("Pass 2/3") != std::string::npos, "Wipe must execute Pass 2");
    TEST_ASSERT(wipeOss.str().find("Pass 3/3") != std::string::npos, "Wipe must execute Pass 3");

    // 13. Interactive Shell Integration (cipher / efs CLI commands)
    shell::CommandShell proc;
    std::ostringstream oss;
    int shellRet = proc.execute("cipher status", oss);
    TEST_ASSERT(shellRet == 0, "cipher status must return 0");
    TEST_ASSERT(oss.str().find("Operational") != std::string::npos, "Must report operational");
    TEST_ASSERT(oss.str().find("AES-256") != std::string::npos, "Must report AES-256");

    oss.str("");
    shellRet = proc.execute("cipher test", oss);
    TEST_ASSERT(shellRet == 0, "cipher test must return 0");
    TEST_ASSERT(oss.str().find("[SUCCESS]") != std::string::npos, "cipher test must report SUCCESS");

    oss.str("");
    shellRet = proc.execute("cipher /e C:\\Users\\Temp\\Doc.txt", oss);
    TEST_ASSERT(shellRet == 0, "cipher /e must return 0");
    TEST_ASSERT(oss.str().find("E [OK]") != std::string::npos, "cipher /e must report OK");

    oss.str("");
    shellRet = proc.execute("cipher /c C:\\Users\\Temp\\Doc.txt", oss);
    TEST_ASSERT(shellRet == 0, "cipher /c must return 0");
    TEST_ASSERT(oss.str().find("Users who can decrypt") != std::string::npos, "cipher /c must list users");

    oss.str("");
    shellRet = proc.execute("cipher /d C:\\Users\\Temp\\Doc.txt", oss);
    TEST_ASSERT(shellRet == 0, "cipher /d must return 0");
    TEST_ASSERT(oss.str().find("U [OK]") != std::string::npos, "cipher /d must report OK");

    oss.str("");
    shellRet = proc.execute("cipher /k", oss);
    TEST_ASSERT(shellRet == 0, "cipher /k must return 0");

    // 14. Subsystem Shutdown
    uint32_t shutRes = EfsClientShutdown();
    TEST_ASSERT(shutRes == ERROR_SUCCESS, "EfsClientShutdown must return ERROR_SUCCESS");

    std::cout << "[TEST] Suite 135: Windows Encrypting File System (EFS) & feclient.dll Subsystem PASSED.\n";
}

void Test_WindowsSecurityCenter_WSC_Subsystem() {
    std::cout << "\n[TEST] Starting Suite 136: Windows Security Center (SentinelCenter) & wscapi.dll Subsystem...\n";

    using namespace micant::wsc;

    // Reset to pristine state
    SovereignWscManager::get().resetToDefaults();

    // 1. Dynamic Loader & Version Database Export Registration
    InitializeWscSubsystemExports();

    auto& ldr = ldr::DynamicLoader::get();
    TEST_ASSERT(ldr.getExport("wscapi.dll", "WscGetSecurityProviderHealth") != nullptr, "WscGetSecurityProviderHealth must be exported");
    TEST_ASSERT(ldr.getExport("wscapi.dll", "WscRegisterForChanges") != nullptr, "WscRegisterForChanges must be exported");
    TEST_ASSERT(ldr.getExport("wscapi.dll", "WscUnRegisterChanges") != nullptr, "WscUnRegisterChanges must be exported");
    TEST_ASSERT(ldr.getExport("wscapi.dll", "WscQueryAntiVirusStatus") != nullptr, "WscQueryAntiVirusStatus must be exported");
    TEST_ASSERT(ldr.getExport("wscapi.dll", "WscRegisterProduct") != nullptr, "WscRegisterProduct must be exported");
    TEST_ASSERT(ldr.getExport("wscapi.dll", "WscUnregisterProduct") != nullptr, "WscUnregisterProduct must be exported");
    TEST_ASSERT(ldr.getExport("wscapi.dll", "WscUpdateProductStatus") != nullptr, "WscUpdateProductStatus must be exported");
    TEST_ASSERT(ldr.getExport("wscapi.dll", "WscGetAntiVirusProducts") != nullptr, "WscGetAntiVirusProducts must be exported");
    TEST_ASSERT(ldr.getExport("wscapi.dll", "WscFreeMemory") != nullptr, "WscFreeMemory must be exported");

    const auto* modInfo = version::VersionDatabase::Instance().FindModule("wscapi.dll");
    TEST_ASSERT(modInfo != nullptr, "wscapi.dll must be registered in VersionDatabase");
    TEST_ASSERT(modInfo->stringTable.find("FileVersion") != modInfo->stringTable.end() &&
                modInfo->stringTable.at("FileVersion") == "10.0.26100.1", "wscapi.dll version must be 10.0.26100.1");

    // 2. Individual Provider Health Queries (WscGetSecurityProviderHealth)
    WSC_SECURITY_PROVIDER_HEALTH hFw = WSC_SECURITY_PROVIDER_HEALTH_POOR;
    HRESULT hr = WscGetSecurityProviderHealth(WSC_SECURITY_PROVIDER_FIREWALL, &hFw);
    TEST_ASSERT(SUCCEEDED(hr), "WscGetSecurityProviderHealth(FIREWALL) must succeed");
    TEST_ASSERT(hFw == WSC_SECURITY_PROVIDER_HEALTH_GOOD, "Firewall health must be GOOD");

    WSC_SECURITY_PROVIDER_HEALTH hAv = WSC_SECURITY_PROVIDER_HEALTH_POOR;
    hr = WscGetSecurityProviderHealth(WSC_SECURITY_PROVIDER_ANTIVIRUS, &hAv);
    TEST_ASSERT(SUCCEEDED(hr), "WscGetSecurityProviderHealth(ANTIVIRUS) must succeed");
    TEST_ASSERT(hAv == WSC_SECURITY_PROVIDER_HEALTH_GOOD, "Antivirus health must be GOOD");

    WSC_SECURITY_PROVIDER_HEALTH hUac = WSC_SECURITY_PROVIDER_HEALTH_POOR;
    hr = WscGetSecurityProviderHealth(WSC_SECURITY_PROVIDER_USER_ACCOUNT_CONTROL, &hUac);
    TEST_ASSERT(SUCCEEDED(hr), "WscGetSecurityProviderHealth(UAC) must succeed");
    TEST_ASSERT(hUac == WSC_SECURITY_PROVIDER_HEALTH_GOOD, "UAC health must be GOOD");

    WSC_SECURITY_PROVIDER_HEALTH hCbs = WSC_SECURITY_PROVIDER_HEALTH_POOR;
    hr = WscGetSecurityProviderHealth(WSC_SECURITY_PROVIDER_AUTOUPDATE_SETTINGS, &hCbs);
    TEST_ASSERT(SUCCEEDED(hr), "WscGetSecurityProviderHealth(AUTOUPDATE) must succeed");
    TEST_ASSERT(hCbs == WSC_SECURITY_PROVIDER_HEALTH_GOOD, "AutoUpdate health must be GOOD");

    WSC_SECURITY_PROVIDER_HEALTH hSvc = WSC_SECURITY_PROVIDER_HEALTH_POOR;
    hr = WscGetSecurityProviderHealth(WSC_SECURITY_PROVIDER_SERVICE, &hSvc);
    TEST_ASSERT(SUCCEEDED(hr), "WscGetSecurityProviderHealth(SERVICE) must succeed");
    TEST_ASSERT(hSvc == WSC_SECURITY_PROVIDER_HEALTH_GOOD, "Service health must be GOOD");

    // 3. Error Validation (Null pointers & Invalid bitmasks)
    hr = WscGetSecurityProviderHealth(WSC_SECURITY_PROVIDER_FIREWALL, nullptr);
    TEST_ASSERT(hr == E_POINTER, "Null pointer must return E_POINTER");

    hr = WscGetSecurityProviderHealth(0, &hFw);
    TEST_ASSERT(hr == E_INVALIDARG, "Zero provider mask must return E_INVALIDARG");

    hr = WscGetSecurityProviderHealth(0x80000000, &hFw);
    TEST_ASSERT(hr == E_INVALIDARG, "Invalid provider mask bit must return E_INVALIDARG");

    // 4. Combined Multi-Provider Health Aggregation & Worst-Case Resolution
    WSC_SECURITY_PROVIDER_HEALTH hAll = WSC_SECURITY_PROVIDER_HEALTH_POOR;
    hr = WscGetSecurityProviderHealth(WSC_SECURITY_PROVIDER_ALL, &hAll);
    TEST_ASSERT(SUCCEEDED(hr), "WscGetSecurityProviderHealth(ALL) must succeed");
    TEST_ASSERT(hAll == WSC_SECURITY_PROVIDER_HEALTH_GOOD, "Overall system health must be GOOD");

    // Simulate Antivirus Snoozed: Overall health must transition to SNOOZE
    SovereignWscManager::get().setProviderOverride(WSC_SECURITY_PROVIDER_ANTIVIRUS, WSC_SECURITY_PROVIDER_HEALTH_SNOOZE);
    hr = WscGetSecurityProviderHealth(WSC_SECURITY_PROVIDER_FIREWALL | WSC_SECURITY_PROVIDER_ANTIVIRUS, &hAll);
    TEST_ASSERT(SUCCEEDED(hr) && hAll == WSC_SECURITY_PROVIDER_HEALTH_SNOOZE, "Combined health with snoozed AV must be SNOOZE");

    // Simulate Firewall Disabled (POOR): Overall health must transition to POOR (precedence over SNOOZE)
    SovereignWscManager::get().setProviderOverride(WSC_SECURITY_PROVIDER_FIREWALL, WSC_SECURITY_PROVIDER_HEALTH_POOR);
    hr = WscGetSecurityProviderHealth(WSC_SECURITY_PROVIDER_ALL, &hAll);
    TEST_ASSERT(SUCCEEDED(hr) && hAll == WSC_SECURITY_PROVIDER_HEALTH_POOR, "POOR must take precedence over SNOOZE and GOOD");

    // Clear simulation overrides
    SovereignWscManager::get().clearOverrides();
    hr = WscGetSecurityProviderHealth(WSC_SECURITY_PROVIDER_ALL, &hAll);
    TEST_ASSERT(SUCCEEDED(hr) && hAll == WSC_SECURITY_PROVIDER_HEALTH_GOOD, "System health must restore to GOOD after clearing overrides");

    // 5. Antivirus Status Bitmask (WscQueryAntiVirusStatus)
    DWORD dwAvStatus = 0;
    hr = WscQueryAntiVirusStatus(&dwAvStatus);
    TEST_ASSERT(SUCCEEDED(hr), "WscQueryAntiVirusStatus must succeed");
    TEST_ASSERT((dwAvStatus & WSC_AV_STATUS_ON) != 0, "Product state must be ON");
    TEST_ASSERT((dwAvStatus & WSC_AV_STATUS_SIGNATURE_UPTODATE) != 0, "Signatures must be up to date");
    TEST_ASSERT((dwAvStatus & WSC_AV_STATUS_RTP_ENABLED) != 0, "Real-time protection must be active");
    TEST_ASSERT((dwAvStatus & WSC_AV_STATUS_PROVIDER_ANTIVIRUS) != 0, "Provider type must be Antivirus");

    // 6. Security Product Registration & Lifecycle (WscRegisterProduct / Update / Unregister)
    GUID testProductGuid{};
    hr = WscRegisterProduct(
        L"Sovereign Falcon Endpoint Detection",
        WSC_SECURITY_PROVIDER_ANTIVIRUS,
        L"C:\\Program Files\\Sovereign\\Falcon.exe",
        WSC_SECURITY_PRODUCT_STATE_ON,
        1,
        0,
        &testProductGuid
    );
    TEST_ASSERT(SUCCEEDED(hr), "WscRegisterProduct must return S_OK");
    TEST_ASSERT(testProductGuid.Data1 != 0, "Product GUID must be valid");

    auto prodOpt = SovereignWscManager::get().getProductByGuid(testProductGuid);
    TEST_ASSERT(prodOpt.has_value(), "Registered product must be discoverable in manager");
    TEST_ASSERT(prodOpt->productName == L"Sovereign Falcon Endpoint Detection", "Product name must match");
    TEST_ASSERT(prodOpt->state == WSC_SECURITY_PRODUCT_STATE_ON, "Initial product state must be ON");

    // Update Product Status
    hr = WscUpdateProductStatus(&testProductGuid, WSC_SECURITY_PRODUCT_STATE_SNOOZED, 0);
    TEST_ASSERT(SUCCEEDED(hr), "WscUpdateProductStatus must return S_OK");
    prodOpt = SovereignWscManager::get().getProductByGuid(testProductGuid);
    TEST_ASSERT(prodOpt.has_value() && prodOpt->state == WSC_SECURITY_PRODUCT_STATE_SNOOZED, "Product state must update to SNOOZED");
    TEST_ASSERT(!prodOpt->signatureUpToDate, "Signatures must update to out of date");

    // Query Products Array (WscGetAntiVirusProducts & WscFreeMemory)
    DWORD prodCount = 0;
    WscProductEntry* pProdArray = nullptr;
    hr = WscGetAntiVirusProducts(&prodCount, &pProdArray);
    TEST_ASSERT(SUCCEEDED(hr), "WscGetAntiVirusProducts must succeed");
    TEST_ASSERT(prodCount >= 2, "Must contain at least 2 registered antivirus products");
    TEST_ASSERT(pProdArray != nullptr, "Product array pointer must not be null");
    WscFreeMemory(pProdArray);

    // Unregister Product
    hr = WscUnregisterProduct(&testProductGuid);
    TEST_ASSERT(SUCCEEDED(hr), "WscUnregisterProduct must succeed");
    prodOpt = SovereignWscManager::get().getProductByGuid(testProductGuid);
    TEST_ASSERT(!prodOpt.has_value(), "Unregistered product must no longer exist");

    // Unregister non-existent product
    GUID dummyGuid{ 0xDEADBEEF, 0x0000, 0x0000, {0} };
    hr = WscUnregisterProduct(&dummyGuid);
    TEST_ASSERT(FAILED(hr), "Unregistering non-existent GUID must fail");

    // 7. Change Notification Subscriptions (WscRegisterForChanges / WscUnRegisterChanges)
    static std::atomic<int> s_notifyCount{ 0 };
    auto notifyCb = [](void* ctx) -> uint32_t {
        auto* p = reinterpret_cast<std::atomic<int>*>(ctx);
        if (p) (*p)++;
        return 0;
    };

    // Argument validation
    HANDLE hListener = nullptr;
    hr = WscRegisterForChanges(reinterpret_cast<void*>(0x1234), &hListener, notifyCb, &s_notifyCount);
    TEST_ASSERT(hr == E_INVALIDARG, "Non-null Reserved argument must fail with E_INVALIDARG");

    hr = WscRegisterForChanges(nullptr, nullptr, notifyCb, &s_notifyCount);
    TEST_ASSERT(hr == E_POINTER, "Null registration handle pointer must fail with E_POINTER");

    hr = WscRegisterForChanges(nullptr, &hListener, nullptr, &s_notifyCount);
    TEST_ASSERT(hr == E_POINTER, "Null callback routine pointer must fail with E_POINTER");

    // Valid registration
    s_notifyCount.store(0);
    hr = WscRegisterForChanges(nullptr, &hListener, notifyCb, &s_notifyCount);
    TEST_ASSERT(SUCCEEDED(hr) && hListener != nullptr, "WscRegisterForChanges must succeed with valid handle");

    // Trigger notification via product registration
    GUID g2{};
    WscRegisterProduct(L"Notification Probe Agent", WSC_SECURITY_PROVIDER_ANTIVIRUS, L"", WSC_SECURITY_PRODUCT_STATE_ON, 1, 0, &g2);
    TEST_ASSERT(s_notifyCount.load() > 0, "Notification callback must be invoked when a product is registered");

    int prevCount = s_notifyCount.load();
    WscUpdateProductStatus(&g2, WSC_SECURITY_PRODUCT_STATE_OFF, 0);
    TEST_ASSERT(s_notifyCount.load() > prevCount, "Notification callback must be invoked when product status updates");

    WscUnregisterProduct(&g2);

    // Unregister listener
    hr = WscUnRegisterChanges(hListener);
    TEST_ASSERT(SUCCEEDED(hr), "WscUnRegisterChanges must succeed");

    // Unregistering same handle again must fail
    hr = WscUnRegisterChanges(hListener);
    TEST_ASSERT(FAILED(hr), "Unregistering inactive handle must fail");

    // 8. COM Interfaces (IWSCProductList & IWscProduct)
    IWSCProductList* pList = nullptr;
    hr = WscCreateProductList(WSC_SECURITY_PROVIDER_ALL, &pList);
    TEST_ASSERT(SUCCEEDED(hr) && pList != nullptr, "WscCreateProductList must return S_OK and valid interface");

    LONG totalItems = 0;
    pList->get_Count(&totalItems);
    TEST_ASSERT(totalItems >= 4, "Product list count must be at least 4");

    IWscProduct* pItem = nullptr;
    hr = pList->get_Item(0, &pItem);
    TEST_ASSERT(SUCCEEDED(hr) && pItem != nullptr, "get_Item(0) must succeed");

    BSTR bstrPkg = nullptr;
    pItem->get_PackageName(&bstrPkg);
    TEST_ASSERT(bstrPkg != nullptr, "get_PackageName must return valid BSTR");
    ole32::SysFreeString(bstrPkg);

    WSC_SECURITY_PRODUCT_STATE stState = WSC_SECURITY_PRODUCT_STATE_OFF;
    pItem->get_ProductState(&stState);
    TEST_ASSERT(stState == WSC_SECURITY_PRODUCT_STATE_ON, "Default product state must be ON");

    WSC_SECURITY_PRODUCT_SUBSTATUS stSub = WSC_SECURITY_PRODUCT_SUBSTATUS_NOT_SET;
    pItem->get_SignatureStatus(&stSub);
    TEST_ASSERT(stSub == WSC_SECURITY_PRODUCT_SUBSTATUS_NO_ACTION, "Signature status must be NO_ACTION");

    pItem->Release();
    pList->Release();

    // 9. Interactive Shell Integration (wsc CLI commands)
    shell::CommandShell proc;
    std::ostringstream oss;

    int shellRet = proc.execute("wsc status", oss);
    TEST_ASSERT(shellRet == 0, "wsc status must return 0");
    TEST_ASSERT(oss.str().find("Sentinel") != std::string::npos, "wsc status output must mention Sentinel");
    TEST_ASSERT(oss.str().find("GOOD") != std::string::npos, "wsc status must report GOOD");
    TEST_ASSERT(oss.str().find("AegisDefender") != std::string::npos, "wsc status must mention AegisDefender");
    TEST_ASSERT(oss.str().find("WFP") != std::string::npos, "wsc status must mention WFP");

    // Also verify "sentinel" command alias
    oss.str("");
    shellRet = proc.execute("sentinel status", oss);
    TEST_ASSERT(shellRet == 0, "sentinel status must return 0");
    TEST_ASSERT(oss.str().find("Sentinel Security System for MicaNT") != std::string::npos, "sentinel status output must report Sentinel Security System for MicaNT");

    oss.str("");
    shellRet = proc.execute("wsc health firewall", oss);
    TEST_ASSERT(shellRet == 0, "wsc health firewall must return 0");
    TEST_ASSERT(oss.str().find("GOOD") != std::string::npos, "Firewall health must report GOOD");

    oss.str("");
    shellRet = proc.execute("wsc health antivirus", oss);
    TEST_ASSERT(shellRet == 0, "wsc health antivirus must return 0");
    TEST_ASSERT(oss.str().find("GOOD") != std::string::npos, "Antivirus health must report GOOD");

    oss.str("");
    shellRet = proc.execute("wsc products", oss);
    TEST_ASSERT(shellRet == 0, "wsc products must return 0");
    TEST_ASSERT(oss.str().find("AegisDefender Antivirus") != std::string::npos, "Must list AegisDefender");
    TEST_ASSERT(oss.str().find("MicaNT Sovereign Advanced Firewall") != std::string::npos, "Must list Firewall");

    oss.str("");
    shellRet = proc.execute("sentinel test", oss);
    TEST_ASSERT(shellRet == 0, "sentinel test must return 0");
    TEST_ASSERT(oss.str().find("[SUCCESS]") != std::string::npos, "sentinel test must report SUCCESS");

    std::cout << "[TEST] Suite 136: Sentinel Security System (SentinelCenter / wscapi.dll) PASSED.\n";
}

void Test_WindowsAMSI_SentinelScan_Subsystem() {
    std::cout << "\n[TEST] Starting Suite 137: Antimalware Scan Interface (AMSI / SentinelScan) & amsi.dll Subsystem...\n";

    using namespace micant::amsi;
    InitializeAmsiSubsystemExports();

    // 1. Module Registration & VersionDatabase Verification
    {
        auto mod = version::VersionDatabase::Instance().FindModule("amsi.dll");
        TEST_ASSERT(mod != nullptr, "amsi.dll must be registered in VersionDatabase");
        TEST_ASSERT(mod->stringTable.at("FileVersion") == "10.0.26100.1", "amsi.dll FileVersion must match 10.0.26100.1");
        TEST_ASSERT(mod->stringTable.at("FileDescription").find("Antimalware Scan Interface") != std::string::npos, "Description must match");
    }

    // 2. DynamicLoader Function Export Verification
    {
        auto& ldr = ldr::DynamicLoader::get();
        TEST_ASSERT(ldr.getExport("amsi.dll", "AmsiInitialize") != nullptr, "AmsiInitialize must be exported");
        TEST_ASSERT(ldr.getExport("amsi.dll", "AmsiUninitialize") != nullptr, "AmsiUninitialize must be exported");
        TEST_ASSERT(ldr.getExport("amsi.dll", "AmsiOpenSession") != nullptr, "AmsiOpenSession must be exported");
        TEST_ASSERT(ldr.getExport("amsi.dll", "AmsiCloseSession") != nullptr, "AmsiCloseSession must be exported");
        TEST_ASSERT(ldr.getExport("amsi.dll", "AmsiScanBuffer") != nullptr, "AmsiScanBuffer must be exported");
        TEST_ASSERT(ldr.getExport("amsi.dll", "AmsiScanString") != nullptr, "AmsiScanString must be exported");
        TEST_ASSERT(ldr.getExport("amsi.dll", "AmsiNotifyOperation") != nullptr, "AmsiNotifyOperation must be exported");
    }

    // 3. AmsiInitialize & Context/Session Validation
    HAMSICONTEXT ctx = nullptr;
    HRESULT hr = AmsiInitialize(nullptr, &ctx);
    TEST_ASSERT(FAILED(hr), "AmsiInitialize with null appName must fail");

    hr = AmsiInitialize(L"MicaNTScriptHost", nullptr);
    TEST_ASSERT(FAILED(hr), "AmsiInitialize with null outContext must fail");

    hr = AmsiInitialize(L"MicaNTScriptHost", &ctx);
    TEST_ASSERT(SUCCEEDED(hr) && ctx != nullptr, "AmsiInitialize must return S_OK and valid context");

    HAMSISESSION session1 = nullptr;
    hr = AmsiOpenSession(ctx, &session1);
    TEST_ASSERT(SUCCEEDED(hr) && session1 != nullptr, "AmsiOpenSession must return S_OK and valid session");

    HAMSISESSION session2 = nullptr;
    hr = AmsiOpenSession(ctx, &session2);
    TEST_ASSERT(SUCCEEDED(hr) && session2 != nullptr, "AmsiOpenSession must return unique second session");
    TEST_ASSERT(session1 != session2, "Session handles must be distinct");

    AmsiCloseSession(ctx, session2);

    // 4. Benign Payload In-Memory Scanning
    AMSI_RESULT res = AMSI_RESULT_CLEAN;
    hr = AmsiScanString(ctx, L"Write-Host 'MicaNT Clean Script'", L"safe.ps1", session1, &res);
    TEST_ASSERT(SUCCEEDED(hr), "AmsiScanString on benign string must succeed");
    TEST_ASSERT(res == AMSI_RESULT_NOT_DETECTED, "Benign script must yield AMSI_RESULT_NOT_DETECTED");
    TEST_ASSERT(!AmsiResultIsMalware(res), "AmsiResultIsMalware must be FALSE for benign script");
    TEST_ASSERT(!AmsiResultIsBlockedByAdmin(res), "AmsiResultIsBlockedByAdmin must be FALSE for benign script");
    TEST_ASSERT(AmsiResultIsValid(res), "AmsiResultIsValid must return TRUE");

    const char* safeCmd = "dir C:\\Windows\\System32";
    hr = AmsiScanBuffer(ctx, (void*)safeCmd, static_cast<ULONG>(std::strlen(safeCmd)), L"dir.cmd", session1, &res);
    TEST_ASSERT(SUCCEEDED(hr), "AmsiScanBuffer on benign buffer must succeed");
    TEST_ASSERT(res == AMSI_RESULT_NOT_DETECTED, "Benign buffer must yield AMSI_RESULT_NOT_DETECTED");

    // 5. Heuristic & Signature Detection Tests
    // 5a. EICAR Standard Virus Test Pattern
    std::wstring eicarString = GetEicarTestPatternW();
    hr = AmsiScanString(ctx, eicarString.c_str(), L"eicar_test.com", session1, &res);
    TEST_ASSERT(SUCCEEDED(hr), "AmsiScanString on EICAR must return S_OK");
    TEST_ASSERT(res == AMSI_RESULT_DETECTED, "EICAR string must yield AMSI_RESULT_DETECTED");
    TEST_ASSERT(AmsiResultIsMalware(res), "AmsiResultIsMalware must be TRUE for EICAR");

    // 5b. Malicious PowerShell Download Cradle
    std::wstring cradlePayload = BuildTestDownloadCradle();
    hr = AmsiScanString(ctx, cradlePayload.c_str(), L"cradle.ps1", session1, &res);
    TEST_ASSERT(SUCCEEDED(hr), "AmsiScanString on download cradle must succeed");
    TEST_ASSERT(res == AMSI_RESULT_DETECTED, "Download cradle must yield AMSI_RESULT_DETECTED");
    TEST_ASSERT(AmsiResultIsMalware(res), "Download cradle must be recognized as malware");

    // 5c. Credential Dumping (Mimikatz / sekurlsa)
    std::wstring mimikatzPayload = BuildTestCredentialDump();
    hr = AmsiScanString(ctx, mimikatzPayload.c_str(), L"mimikatz_eval.ps1", session1, &res);
    TEST_ASSERT(SUCCEEDED(hr) && res == AMSI_RESULT_DETECTED, "Credential dumping token must yield AMSI_RESULT_DETECTED");

    // 5d. AMSI Tampering / Memory Patching strings
    std::wstring amsiPatchPayload = BuildTestAmsiBypass();
    hr = AmsiScanString(ctx, amsiPatchPayload.c_str(), L"amsibypass.ps1", session1, &res);
    TEST_ASSERT(SUCCEEDED(hr) && res == AMSI_RESULT_DETECTED, "AMSI tampering attempt must yield AMSI_RESULT_DETECTED");

    // 5e. Binary Shellcode with NOP Sled
    unsigned char shellcodePayload[48]{};
    std::memset(shellcodePayload, 0x90, 24); // 24-byte NOP sled
    shellcodePayload[24] = 0x31; shellcodePayload[25] = 0xc0; shellcodePayload[26] = 0x50; shellcodePayload[27] = 0x68;
    hr = AmsiScanBuffer(ctx, shellcodePayload, sizeof(shellcodePayload), L"exploit.bin", session1, &res);
    TEST_ASSERT(SUCCEEDED(hr) && res == AMSI_RESULT_DETECTED, "NOP sled shellcode must yield AMSI_RESULT_DETECTED");

    // 6. AmsiNotifyOperation Verification
    hr = AmsiNotifyOperation(ctx, (void*)"OfficeMacroExecutionHook", 24, L"document.docm", &res);
    TEST_ASSERT(SUCCEEDED(hr), "AmsiNotifyOperation must return S_OK");
    TEST_ASSERT(res == AMSI_RESULT_NOT_DETECTED, "Benign macro hook must return NOT_DETECTED");

    // 7. Administrator Block Policy Verification
    auto& mgr = SovereignAmsiManager::get();
    mgr.addAdminBlockRule(L"restricted_app");

    hr = AmsiScanString(ctx, L"echo safe text", L"restricted_app_script.ps1", session1, &res);
    TEST_ASSERT(SUCCEEDED(hr), "AmsiScanString must succeed");
    TEST_ASSERT(res == AMSI_RESULT_BLOCKED_BY_ADMIN_START, "Admin blocked content must yield AMSI_RESULT_BLOCKED_BY_ADMIN_START");
    TEST_ASSERT(AmsiResultIsBlockedByAdmin(res), "AmsiResultIsBlockedByAdmin must be TRUE");
    TEST_ASSERT(!AmsiResultIsMalware(res), "AmsiResultIsMalware must be FALSE for admin policy block");

    mgr.removeAdminBlockRule(L"restricted_app");
    hr = AmsiScanString(ctx, L"echo safe text", L"restricted_app_script.ps1", session1, &res);
    TEST_ASSERT(res == AMSI_RESULT_NOT_DETECTED, "Content must be allowed once admin block rule is removed");

    // 8. Custom IAmsiProvider Plug-in Verification
    class TestCustomProvider final : public IAmsiProvider {
    public:
        TestCustomProvider() : m_refs(1) {}
        HRESULT WINAPI QueryInterface(const micant::GUID& riid, void** ppv) noexcept override {
            if (!ppv) return E_POINTER;
            if (riid == ole32::IID_IUnknown || riid == IID_IAmsiProvider) {
                *ppv = static_cast<IAmsiProvider*>(this);
                AddRef();
                return S_OK;
            }
            return E_NOINTERFACE;
        }
        uint32_t WINAPI AddRef() noexcept override { return ++m_refs; }
        uint32_t WINAPI Release() noexcept override {
            uint32_t c = --m_refs;
            return c;
        }
        HRESULT WINAPI DisplayName(LPWSTR* p) override {
            if (!p) return E_POINTER;
            *p = nullptr;
            return S_OK;
        }
        void WINAPI CloseSession(ULONGLONG) override {}
        HRESULT WINAPI Scan(IAmsiStream* stream, AMSI_RESULT* result) override {
            if (!stream || !result) return E_POINTER;
            ULONG sz = 0;
            stream->GetAttribute(AMSI_ATTRIBUTE_CONTENT_SIZE, sizeof(ULONG), (unsigned char*)&sz, nullptr);
            std::vector<unsigned char> buf(sz);
            ULONG readBytes = 0;
            stream->Read(0, sz, buf.data(), &readBytes);
            std::string content(reinterpret_cast<char*>(buf.data()), readBytes);
            if (content.find("CUSTOM_TRIGGER_DETECT") != std::string::npos) {
                *result = AMSI_RESULT_DETECTED;
            } else {
                *result = AMSI_RESULT_NOT_DETECTED;
            }
            return S_OK;
        }
    private:
        std::atomic<uint32_t> m_refs;
    };

    TestCustomProvider customProv;
    mgr.registerProvider(&customProv);

    const char* customTestPayload = "echo variable; CUSTOM_TRIGGER_DETECT; echo done;";
    hr = AmsiScanBuffer(ctx, (void*)customTestPayload, static_cast<ULONG>(std::strlen(customTestPayload)), L"plugin.ps1", session1, &res);
    TEST_ASSERT(SUCCEEDED(hr) && res == AMSI_RESULT_DETECTED, "Custom provider must flag CUSTOM_TRIGGER_DETECT payload");

    mgr.unregisterProvider(&customProv);

    // 9. IAmsiStream Direct COM Verification
    {
        const unsigned char sampleData[] = "StreamDataSample";
        auto* streamObj = new AmsiStreamImpl(L"HostApp", L"doc.bin", sampleData, sizeof(sampleData), 42);
        
        ULONG needed = 0;
        hr = streamObj->GetAttribute(AMSI_ATTRIBUTE_CONTENT_SIZE, 0, nullptr, &needed);
        TEST_ASSERT(needed == sizeof(ULONG), "Content size attribute size must be 4 bytes");

        ULONG gotSize = 0;
        hr = streamObj->GetAttribute(AMSI_ATTRIBUTE_CONTENT_SIZE, sizeof(ULONG), (unsigned char*)&gotSize, nullptr);
        TEST_ASSERT(SUCCEEDED(hr) && gotSize == sizeof(sampleData), "Stream content size must match");

        unsigned char readBuf[32]{};
        ULONG bytesRead = 0;
        hr = streamObj->Read(0, sizeof(readBuf), readBuf, &bytesRead);
        TEST_ASSERT(SUCCEEDED(hr) && bytesRead == sizeof(sampleData), "Read must fetch entire stream content");
        TEST_ASSERT(std::memcmp(readBuf, sampleData, bytesRead) == 0, "Read data must match sample data exactly");

        streamObj->Release();
    }

    // 10. Interactive Command Shell (amsi CLI & shell interception)
    {
        shell::CommandShell proc;
        std::ostringstream oss;

        // 10a. amsi status
        int shellRet = proc.execute("amsi status", oss);
        TEST_ASSERT(shellRet == 0, "amsi status must return 0");
        TEST_ASSERT(oss.str().find("SentinelScan") != std::string::npos, "amsi status must report SentinelScan");
        TEST_ASSERT(oss.str().find("Active & Enforcing") != std::string::npos, "amsi status must report active state");

        // 10b. amsi scan safe
        oss.str("");
        shellRet = proc.execute("amsi scan echo Hello MicaNT", oss);
        TEST_ASSERT(shellRet == 0, "amsi scan safe must return 0");
        TEST_ASSERT(oss.str().find("NOT_DETECTED") != std::string::npos, "Must report NOT_DETECTED for safe string");

        // 10c. amsi scan malware
        oss.str("");
        std::string scanCmd = "amsi scan " + std::string("IEX (New-Object ") + "Net.WebClient)." + "DownloadString('http://127.0.0.1/test.ps1')";
        shellRet = proc.execute(scanCmd, oss);
        TEST_ASSERT(shellRet == 0, "amsi scan tool must execute");
        TEST_ASSERT(oss.str().find("DETECTED") != std::string::npos, "amsi scan must detect malicious download cradle");

        // 10d. Shell pre-execution interception: malicious command input must be blocked
        oss.str("");
        std::string execCmd = std::string("IEX (New-Object ") + "Net.WebClient)." + "DownloadString('http://127.0.0.1/malicious_payload.ps1')";
        shellRet = proc.execute(execCmd, oss);
        TEST_ASSERT(shellRet != 0, "Malicious script command must be rejected by shell pre-execution filter");
        TEST_ASSERT(oss.str().find("Blocked by Sentinel Security System (AMSI") != std::string::npos, "Output must state AMSI block");
        TEST_ASSERT(oss.str().find("0x800700DF") != std::string::npos, "Output must contain ERROR_VIRUS_INFECTED");

        // 10e. amsi test
        oss.str("");
        shellRet = proc.execute("amsi test", oss);
        TEST_ASSERT(shellRet == 0, "amsi test must return 0");
        TEST_ASSERT(oss.str().find("[SUCCESS]") != std::string::npos, "amsi test must report SUCCESS");
    }

    // 11. Cleanup and Uninitialization
    AmsiCloseSession(ctx, session1);
    AmsiUninitialize(ctx);

    std::cout << "[TEST] Suite 137: Antimalware Scan Interface (AMSI / SentinelScan) Subsystem PASSED.\n";
}

void Test_WindowsDefender_AegisDefender_Subsystem() {
    std::cout << "\n[TEST] Starting Suite 138: Microsoft Malware Protection Engine (AegisDefender) & mpclient.dll Subsystem...\n";

    using namespace micant::defender;
    InitializeMpEngineSubsystemExports();

    // 1. Module Registration & VersionDatabase Verification
    {
        auto modClient = version::VersionDatabase::Instance().FindModule("mpclient.dll");
        TEST_ASSERT(modClient != nullptr, "mpclient.dll must be registered in VersionDatabase");
        TEST_ASSERT(modClient->stringTable.at("FileVersion") == "10.0.26100.1", "mpclient.dll FileVersion must match 10.0.26100.1");
        TEST_ASSERT(modClient->stringTable.at("FileDescription").find("Malware Protection Client") != std::string::npos, "Client description must match");
        TEST_ASSERT(modClient->stringTable.at("CompanyName") == "MicaNT Sovereign Project", "CompanyName must match");

        auto modEngine = version::VersionDatabase::Instance().FindModule("mpengine.dll");
        TEST_ASSERT(modEngine != nullptr, "mpengine.dll must be registered in VersionDatabase");
        TEST_ASSERT(modEngine->stringTable.at("FileVersion") == "10.0.26100.1", "mpengine.dll FileVersion must match 10.0.26100.1");
        TEST_ASSERT(modEngine->stringTable.at("FileDescription").find("Malware Protection Engine") != std::string::npos, "Engine description must match");
    }

    // 2. DynamicLoader Function Export Verification (mpclient.dll and mpengine.dll)
    {
        auto& ldr = ldr::DynamicLoader::get();
        for (const char* modName : { "mpclient.dll", "mpengine.dll" }) {
            TEST_ASSERT(ldr.getExport(modName, "MpManagerOpen") != nullptr, "MpManagerOpen must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpManagerClose") != nullptr, "MpManagerClose must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpScanStart") != nullptr, "MpScanStart must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpScanControl") != nullptr, "MpScanControl must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpThreatOpen") != nullptr, "MpThreatOpen must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpThreatEnumerate") != nullptr, "MpThreatEnumerate must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpThreatClose") != nullptr, "MpThreatClose must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpCleanOpen") != nullptr, "MpCleanOpen must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpCleanStart") != nullptr, "MpCleanStart must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpCleanClose") != nullptr, "MpCleanClose must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpGetThreatInfo") != nullptr, "MpGetThreatInfo must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpGetQuarantineVault") != nullptr, "MpGetQuarantineVault must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpQuarantineRestore") != nullptr, "MpQuarantineRestore must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpQuarantineDelete") != nullptr, "MpQuarantineDelete must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpFreeMemory") != nullptr, "MpFreeMemory must be exported");
            TEST_ASSERT(ldr.getExport(modName, "MpErrorMessageFormat") != nullptr, "MpErrorMessageFormat must be exported");
        }
    }

    // 3. MpManagerOpen / MpManagerClose Lifecycle
    {
        MPHANDLE hMgr = nullptr;
        HRESULT hr = MpManagerOpen(0, nullptr);
        TEST_ASSERT(FAILED(hr), "MpManagerOpen with null out handle must fail");

        hr = MpManagerOpen(0, &hMgr);
        TEST_ASSERT(SUCCEEDED(hr) && hMgr != nullptr, "MpManagerOpen must return valid handle");

        hr = MpManagerClose(nullptr);
        TEST_ASSERT(FAILED(hr), "MpManagerClose with null handle must fail");

        hr = MpManagerClose(hMgr);
        TEST_ASSERT(SUCCEEDED(hr), "MpManagerClose must return S_OK");
    }

    // 4. Shannon Entropy PE Section Heuristics
    {
        // 4a. Shannon entropy calculation verification
        std::vector<uint8_t> uniformBytes(1024, 0x55);
        double entUniform = SovereignDefenderEngine::calculateEntropy(uniformBytes.data(), uniformBytes.size());
        TEST_ASSERT(entUniform < 0.001, "Uniform byte sequence entropy must be ~0");

        // High entropy byte distribution (pseudo-random)
        std::vector<uint8_t> diverseBytes(256 * 16);
        for (size_t i = 0; i < diverseBytes.size(); ++i) {
            diverseBytes[i] = static_cast<uint8_t>((i * 101 + 37) % 256);
        }
        double entDiverse = SovereignDefenderEngine::calculateEntropy(diverseBytes.data(), diverseBytes.size());
        TEST_ASSERT(entDiverse > 7.9, "Uniformly distributed byte sequence entropy must be > 7.9");

        // 4b. Synthetic PE image with high entropy executable section
        std::vector<uint8_t> peImage(4096, 0);
        auto* dos = reinterpret_cast<pe::ImageDosHeader*>(peImage.data());
        dos->e_magic = pe::IMAGE_DOS_SIGNATURE;
        dos->e_lfanew = 128;

        auto* nt64 = reinterpret_cast<pe::ImageNtHeaders64*>(peImage.data() + 128);
        nt64->signature = pe::IMAGE_NT_SIGNATURE;
        nt64->fileHeader.numberOfSections = 1;
        nt64->fileHeader.sizeOfOptionalHeader = sizeof(pe::ImageOptionalHeader64);
        nt64->optionalHeader.magic = pe::IMAGE_NT_OPTIONAL_HDR64_MAGIC;

        size_t secOffset = 128 + sizeof(uint32_t) + sizeof(pe::ImageFileHeader) + sizeof(pe::ImageOptionalHeader64);
        auto* sec = reinterpret_cast<pe::ImageSectionHeader*>(peImage.data() + secOffset);
        std::memcpy(sec->name, ".text", 5);
        sec->pointerToRawData = 1024;
        sec->sizeOfRawData = 1024;
        sec->characteristics = 0x20000000 | 0x40000000; // IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ

        // Fill .text raw data with high entropy bytes
        for (size_t i = 1024; i < 2048; ++i) {
            peImage[i] = static_cast<uint8_t>((i * 167 + 13) % 256);
        }

        MPTHREAT_INFO threat{};
        bool detected = SovereignDefenderEngine::get().scanPEBuffer(peImage.data(), peImage.size(), L"C:\\Test\\packed.exe", threat);
        TEST_ASSERT(detected, "High entropy executable section must trigger packed dropper detection");
        TEST_ASSERT(threat.ThreatId == 4001, "ThreatId must be 4001 (Trojan:Win32/PackedDropper.A)");
        TEST_ASSERT(threat.Category == MPTHREAT_CATEGORY_DROPPER, "Category must be DROPPER");

        // Low entropy .text section should not be flagged
        std::fill(peImage.begin() + 1024, peImage.begin() + 2048, 0xCC);
        MPTHREAT_INFO cleanThreat{};
        bool cleanDetected = SovereignDefenderEngine::get().scanPEBuffer(peImage.data(), peImage.size(), L"C:\\Test\\clean.exe", cleanThreat);
        TEST_ASSERT(!cleanDetected, "Low entropy executable section must not trigger dropper heuristic");
    }

    // 5. Memory & Buffer Threat Scanning (Signatures & Heuristics)
    {
        MPHANDLE hMgr = nullptr;
        MpManagerOpen(0, &hMgr);

        // 5a. Benign memory scan
        std::string safePayload = "Hello MicaNT Antimalware Protection Engine";
        MPRESOURCE_INFO resSafe{};
        resSafe.dwResourceType = 0;
        resSafe.pwszResourcePath = L"safe.txt";
        SovereignDefenderEngine::get().setVirtualFile(L"safe.txt", {safePayload.begin(), safePayload.end()});

        MPHANDLE hScan = nullptr;
        HRESULT hr = MpScanStart(hMgr, MPSCAN_TYPE_RESOURCE, 0, &resSafe, nullptr, &hScan);
        TEST_ASSERT(SUCCEEDED(hr) && hr == S_OK, "Benign resource scan must return S_OK");

        // 5b. Shellcode NOP sled detection (>= 16 consecutive 0x90)
        std::vector<uint8_t> nopSled(64, 0x90);
        nopSled[32] = 0x31; nopSled[33] = 0xC0;
        SovereignDefenderEngine::get().setVirtualFile(L"shellcode.bin", nopSled);
        MPRESOURCE_INFO resNop{};
        resNop.pwszResourcePath = L"shellcode.bin";
        hr = MpScanStart(hMgr, MPSCAN_TYPE_RESOURCE, 0, &resNop, nullptr, &hScan);
        TEST_ASSERT(hr == HRESULT_FROM_WIN32_VIRUS_INFECTED, "NOP sled buffer must return HRESULT_FROM_WIN32_VIRUS_INFECTED");

        // Verify threat info from last scan
        const auto& threats = SovereignDefenderEngine::get().getLastScanThreats();
        TEST_ASSERT(!threats.empty(), "Threat list must contain detected threat");
        TEST_ASSERT(threats[0].ThreatId == 4003, "Threat ID must be 4003 (Exploit:Win32/ShellcodeStager.A)");

        // 5c. Local Signature Detection: PowerDrop (dynamically assembled)
        std::string powerDropToken = std::string("test_") + "download" + "string" + "_payload";
        SovereignDefenderEngine::get().setVirtualFile(L"powerdrop.ps1", {powerDropToken.begin(), powerDropToken.end()});
        MPRESOURCE_INFO resPowerDrop{};
        resPowerDrop.pwszResourcePath = L"powerdrop.ps1";
        hr = MpScanStart(hMgr, MPSCAN_TYPE_RESOURCE, 0, &resPowerDrop, nullptr, &hScan);
        TEST_ASSERT(hr == HRESULT_FROM_WIN32_VIRUS_INFECTED, "PowerDrop signature must be detected");
        TEST_ASSERT(SovereignDefenderEngine::get().getLastScanThreats()[0].ThreatId == 1001, "Threat ID must be 1001");

        // 5d. Local Signature Detection: LsaDump
        std::string lsaDumpToken = std::string("mimikatz_") + "logon" + "passwords";
        SovereignDefenderEngine::get().setVirtualFile(L"lsadump.bin", {lsaDumpToken.begin(), lsaDumpToken.end()});
        MPRESOURCE_INFO resLsa{};
        resLsa.pwszResourcePath = L"lsadump.bin";
        hr = MpScanStart(hMgr, MPSCAN_TYPE_RESOURCE, 0, &resLsa, nullptr, &hScan);
        TEST_ASSERT(hr == HRESULT_FROM_WIN32_VIRUS_INFECTED, "LsaDump signature must be detected");
        TEST_ASSERT(SovereignDefenderEngine::get().getLastScanThreats()[0].ThreatId == 1002, "Threat ID must be 1002");

        MpManagerClose(hMgr);
    }

    // 6. Threat Enumeration API (MpThreatOpen, MpThreatEnumerate, MpThreatClose, MpGetThreatInfo)
    {
        MPHANDLE hMgr = nullptr;
        MpManagerOpen(0, &hMgr);

        // Stage threat detection
        std::string ransomToken = std::string("malware_") + "wana" + "cry" + "_payload";
        SovereignDefenderEngine::get().setVirtualFile(L"ransom.bin", {ransomToken.begin(), ransomToken.end()});
        MPRESOURCE_INFO resRansom{};
        resRansom.pwszResourcePath = L"ransom.bin";
        MPHANDLE hScan = nullptr;
        MpScanStart(hMgr, MPSCAN_TYPE_RESOURCE, 0, &resRansom, nullptr, &hScan);

        MPHANDLE hThreatEnum = nullptr;
        HRESULT hr = MpThreatOpen(hMgr, &hThreatEnum);
        TEST_ASSERT(SUCCEEDED(hr) && hThreatEnum != nullptr, "MpThreatOpen must return S_OK");

        PMPTHREAT_INFO pInfo = nullptr;
        hr = MpThreatEnumerate(hThreatEnum, &pInfo);
        TEST_ASSERT(SUCCEEDED(hr) && hr == S_OK && pInfo != nullptr, "MpThreatEnumerate must return first threat");
        TEST_ASSERT(pInfo->ThreatId == 1004, "ThreatId must match WannaCrypt (1004)");
        TEST_ASSERT(pInfo->Category == MPTHREAT_CATEGORY_RANSOMWARE, "Category must be RANSOMWARE");
        TEST_ASSERT(pInfo->Severity == MPTHREAT_SEVERITY_SEVERE, "Severity must be SEVERE");
        MpFreeMemory(pInfo);

        hr = MpThreatEnumerate(hThreatEnum, &pInfo);
        TEST_ASSERT(hr == S_FALSE && pInfo == nullptr, "MpThreatEnumerate must return S_FALSE when done");

        hr = MpThreatClose(hThreatEnum);
        TEST_ASSERT(SUCCEEDED(hr), "MpThreatClose must return S_OK");

        // MpGetThreatInfo
        PMPTHREAT_INFO pInfoDirect = nullptr;
        hr = MpGetThreatInfo(hMgr, 1004, &pInfoDirect);
        TEST_ASSERT(SUCCEEDED(hr) && pInfoDirect != nullptr, "MpGetThreatInfo must find existing threat");
        TEST_ASSERT(std::wcscmp(pInfoDirect->wszThreatName, L"Ransomware:Win32/WannaCrypt.A") == 0, "Threat name must match");
        MpFreeMemory(pInfoDirect);

        hr = MpGetThreatInfo(hMgr, 999999, &pInfoDirect);
        TEST_ASSERT(hr == MP_E_THREAT_NOT_FOUND, "Non-existent threat ID must return MP_E_THREAT_NOT_FOUND");

        MpManagerClose(hMgr);
    }

    // 7. Clean / Remediation / Encrypted Quarantine Vault (AES-256)
    {
        MPHANDLE hMgr = nullptr;
        MpManagerOpen(0, &hMgr);

        std::wstring infectedPath = L"C:\\Users\\admin\\Desktop\\badware.exe";
        std::string infectedContent = std::string("prefix_") + "amsiinit" + "failed" + "_suffix";
        SovereignDefenderEngine::get().setVirtualFile(infectedPath, {infectedContent.begin(), infectedContent.end()});

        // Scan to populate threat info
        MPRESOURCE_INFO resInfect{};
        resInfect.pwszResourcePath = infectedPath.c_str();
        MPHANDLE hScan = nullptr;
        MpScanStart(hMgr, MPSCAN_TYPE_RESOURCE, 0, &resInfect, nullptr, &hScan);

        // Open clean session and quarantine
        MPHANDLE hClean = nullptr;
        HRESULT hr = MpCleanOpen(hMgr, &hClean);
        TEST_ASSERT(SUCCEEDED(hr) && hClean != nullptr, "MpCleanOpen must succeed");

        hr = MpCleanStart(hClean, &resInfect, nullptr);
        TEST_ASSERT(SUCCEEDED(hr), "MpCleanStart (Quarantine) must succeed");
        MpCleanClose(hClean);

        // Verify infected file was removed from active virtual filesystem
        TEST_ASSERT(!SovereignDefenderEngine::get().hasVirtualFile(infectedPath), "Infected file must be removed from virtual file system");

        // Inspect Encrypted Quarantine Vault
        DWORD count = 0;
        PMPQUARANTINE_ENTRY pEntries = nullptr;
        hr = MpGetQuarantineVault(hMgr, &count, &pEntries);
        TEST_ASSERT(SUCCEEDED(hr) && count >= 1 && pEntries != nullptr, "Quarantine vault must contain quarantined entry");

        bool foundQuarantined = false;
        for (DWORD i = 0; i < count; ++i) {
            if (std::wcscmp(pEntries[i].wszThreatName, L"Trojan:Win32/AmsiTamper.A") == 0) {
                foundQuarantined = true;
                TEST_ASSERT(std::wcscmp(pEntries[i].wszOriginalPath, infectedPath.c_str()) == 0, "Original path must match");
                TEST_ASSERT(pEntries[i].FileSize == infectedContent.size(), "File size must match");
                break;
            }
        }
        TEST_ASSERT(foundQuarantined, "Target threat must be in quarantine vault");
        MpFreeMemory(pEntries);

        // 8. Authenticated Quarantine Restoration & Deletion
        hr = MpQuarantineRestore(hMgr, L"Trojan:Win32/AmsiTamper.A", nullptr);
        TEST_ASSERT(SUCCEEDED(hr), "MpQuarantineRestore must restore file successfully");
        TEST_ASSERT(SovereignDefenderEngine::get().hasVirtualFile(infectedPath), "Restored file must now exist in file system");

        // Verify restored file content integrity
        auto restoredData = SovereignDefenderEngine::get().getVaultItems();
        bool stillInVault = false;
        for (const auto& itm : restoredData) {
            if (itm.threatName == L"Trojan:Win32/AmsiTamper.A") stillInVault = true;
        }
        TEST_ASSERT(!stillInVault, "Restored threat must be removed from vault");

        // Re-quarantine then delete
        MpCleanOpen(hMgr, &hClean);
        MpCleanStart(hClean, &resInfect, nullptr);
        MpCleanClose(hClean);

        hr = MpQuarantineDelete(hMgr, L"Trojan:Win32/AmsiTamper.A");
        TEST_ASSERT(SUCCEEDED(hr), "MpQuarantineDelete must delete threat from vault");

        hr = MpQuarantineDelete(hMgr, L"NonExistentThreat");
        TEST_ASSERT(hr == MP_E_THREAT_NOT_FOUND, "Deleting non-existent threat must return MP_E_THREAT_NOT_FOUND");

        MpManagerClose(hMgr);
    }

    // 9. MpErrorMessageFormat & Error Formatting
    {
        LPWSTR pMsg = nullptr;
        HRESULT hr = MpErrorMessageFormat(nullptr, HRESULT_FROM_WIN32_VIRUS_INFECTED, &pMsg);
        TEST_ASSERT(SUCCEEDED(hr) && pMsg != nullptr, "MpErrorMessageFormat must succeed");
        TEST_ASSERT(std::wstring(pMsg).find(L"Threat detected") != std::wstring::npos, "Error message must state threat detected");
        std::free(pMsg);

        pMsg = nullptr;
        hr = MpErrorMessageFormat(nullptr, MP_E_THREAT_NOT_FOUND, &pMsg);
        TEST_ASSERT(SUCCEEDED(hr) && pMsg != nullptr, "MpErrorMessageFormat for threat not found must succeed");
        std::free(pMsg);

        hr = MpErrorMessageFormat(nullptr, S_OK, nullptr);
        TEST_ASSERT(hr == E_POINTER, "Null output pointer must return E_POINTER");
    }

    // 10. Interactive Shell (defender / mpcmdrun CLI)
    {
        shell::CommandShell proc;
        std::ostringstream oss;

        // 10a. defender /?
        int shellRet = proc.execute("defender /?", oss);
        TEST_ASSERT(shellRet == 0, "defender /? must return 0");
        TEST_ASSERT(oss.str().find("MpCmdRun.exe Parity") != std::string::npos, "Help must reference MpCmdRun");

        // 10b. defender status
        oss.str("");
        shellRet = proc.execute("defender status", oss);
        TEST_ASSERT(shellRet == 0, "defender status must return 0");
        TEST_ASSERT(oss.str().find("mpengine.dll") != std::string::npos, "Must show mpengine.dll");
        TEST_ASSERT(oss.str().find("AegisDefender Subsystem") != std::string::npos, "Must show AegisDefender Subsystem");

        // 10c. defender -SignatureUpdate
        oss.str("");
        shellRet = proc.execute("defender -SignatureUpdate", oss);
        TEST_ASSERT(shellRet == 0, "defender -SignatureUpdate must return 0");
        TEST_ASSERT(oss.str().find("Version 1.415.2026.0") != std::string::npos, "Must show signature version");

        // 10d. defender -GetFiles
        oss.str("");
        shellRet = proc.execute("defender -GetFiles", oss);
        TEST_ASSERT(shellRet == 0, "defender -GetFiles must return 0");
        TEST_ASSERT(oss.str().find("SupportLog.txt") != std::string::npos, "Must show SupportLog");

        // 10e. defender -Scan -ScanType 1 (Quick Scan)
        oss.str("");
        shellRet = proc.execute("defender -Scan -ScanType 1", oss);
        TEST_ASSERT(shellRet == 0, "defender -Scan -ScanType 1 must return 0");

        // 10f. defender -ListQuarantine
        oss.str("");
        shellRet = proc.execute("defender -ListQuarantine", oss);
        TEST_ASSERT(shellRet == 0, "defender -ListQuarantine must return 0");
        TEST_ASSERT(oss.str().find("Encrypted Quarantine Vault") != std::string::npos, "Must show vault header");

        // 10g. defender test (self-test command)
        oss.str("");
        shellRet = proc.execute("defender test", oss);
        TEST_ASSERT(shellRet == 0, "defender test must return 0");
        TEST_ASSERT(oss.str().find("[SUCCESS]") != std::string::npos, "defender test must report [SUCCESS]");
    }

    std::cout << "[TEST] Suite 138: Microsoft Malware Protection Engine (AegisDefender) Subsystem PASSED.\n";
}

// ============================================================================
// Suite 139: Windows Defender Exploit Guard (SentinelGuard) Subsystem
// ============================================================================
void Test_WindowsExploitGuard_SentinelGuard_Subsystem() {
    using namespace micant::exploit_guard;

    // 1. Dynamic Subsystem Initialization & Win32 C ABI Export Resolution
    InitializeExploitGuardSubsystemExports();

    auto& loader = ldr::DynamicLoader::get();
    TEST_ASSERT(loader.getExport("kernel32.dll", "GetProcessMitigationPolicy") != nullptr, "kernel32!GetProcessMitigationPolicy must be exported");
    TEST_ASSERT(loader.getExport("kernel32.dll", "SetProcessMitigationPolicy") != nullptr, "kernel32!SetProcessMitigationPolicy must be exported");
    TEST_ASSERT(loader.getExport("mitlib.dll", "GetProcessMitigationPolicy") != nullptr, "mitlib!GetProcessMitigationPolicy must be exported");
    TEST_ASSERT(loader.getExport("mitlib.dll", "SetProcessMitigationPolicy") != nullptr, "mitlib!SetProcessMitigationPolicy must be exported");
    TEST_ASSERT(loader.getExport("api-ms-win-core-processthreads-l1-1-3.dll", "GetProcessMitigationPolicy") != nullptr, "api-ms-win-core-processthreads-l1-1-3!GetProcessMitigationPolicy must be exported");
    TEST_ASSERT(loader.getExport("api-ms-win-core-processthreads-l1-1-3.dll", "SetProcessMitigationPolicy") != nullptr, "api-ms-win-core-processthreads-l1-1-3!SetProcessMitigationPolicy must be exported");

    // Version database registration
    auto modInfo = version::VersionDatabase::Instance().GetModuleInfo("mitlib.dll");
    TEST_ASSERT(modInfo != nullptr, "mitlib.dll must be registered in VersionDatabase");
    TEST_ASSERT(modInfo->stringTable.at("FileVersion") == "10.0.26100.1", "mitlib.dll version must be 10.0.26100.1");

    auto& mgr = SentinelGuardManager::get();
    mgr.resetToBaseline();

    // 2. Query Baseline Policies via GetProcessMitigationPolicy
    {
        // 2a. ProcessDEPPolicy
        PROCESS_MITIGATION_DEP_POLICY dep{};
        win32::BOOL ok = GetProcessMitigationPolicy(win32::GetCurrentProcess(), ProcessDEPPolicy, &dep, sizeof(dep));
        TEST_ASSERT(ok == win32::TRUE, "GetProcessMitigationPolicy(ProcessDEPPolicy) must succeed");
        TEST_ASSERT(dep.Enable == 1, "DEP must be enabled by default");
        TEST_ASSERT(dep.DisableAtlThunkEmulation == 1, "ATL thunk emulation must be disabled by default");
        TEST_ASSERT(dep.Permanent == 1, "DEP must be permanent by default");

        // 2b. ProcessASLRPolicy
        PROCESS_MITIGATION_ASLR_POLICY aslr{};
        ok = GetProcessMitigationPolicy(win32::GetCurrentProcess(), ProcessASLRPolicy, &aslr, sizeof(aslr));
        TEST_ASSERT(ok == win32::TRUE, "GetProcessMitigationPolicy(ProcessASLRPolicy) must succeed");
        TEST_ASSERT(aslr.EnableBottomUpRandomization == 1, "ASLR bottom-up randomization must be enabled");
        TEST_ASSERT(aslr.EnableHighEntropy == 1, "ASLR high-entropy 64-bit VA must be enabled");
        TEST_ASSERT(aslr.EnableForceRelocateImages == 1, "ASLR force relocate images must be enabled");

        // 2c. ProcessControlFlowGuardPolicy
        PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY cfg{};
        ok = GetProcessMitigationPolicy(win32::GetCurrentProcess(), ProcessControlFlowGuardPolicy, &cfg, sizeof(cfg));
        TEST_ASSERT(ok == win32::TRUE, "GetProcessMitigationPolicy(ProcessControlFlowGuardPolicy) must succeed");
        TEST_ASSERT(cfg.EnableControlFlowGuard == 1, "CFG must be enabled by default");
        TEST_ASSERT(cfg.EnableExportSuppression == 1, "CFG export suppression must be enabled by default");
        TEST_ASSERT(cfg.StrictMode == 1, "CFG strict mode must be enabled by default");
    }

    // 3. Robust Parameter Validation & Error Handling
    {
        PROCESS_MITIGATION_DEP_POLICY dep{};
        // Null buffer
        win32::BOOL ok = GetProcessMitigationPolicy(win32::GetCurrentProcess(), ProcessDEPPolicy, nullptr, sizeof(dep));
        TEST_ASSERT(!ok && win32::GetLastError() == 87, "Null buffer to GetPolicy must return FALSE and ERROR_INVALID_PARAMETER (87)");

        // Zero size
        ok = GetProcessMitigationPolicy(win32::GetCurrentProcess(), ProcessDEPPolicy, &dep, 0);
        TEST_ASSERT(!ok && win32::GetLastError() == 87, "Zero size to GetPolicy must return FALSE and ERROR_INVALID_PARAMETER (87)");

        // Truncated buffer
        ok = GetProcessMitigationPolicy(win32::GetCurrentProcess(), ProcessDEPPolicy, &dep, sizeof(uint16_t));
        TEST_ASSERT(!ok && win32::GetLastError() == 87, "Truncated buffer must return FALSE and ERROR_INVALID_PARAMETER (87)");

        // Invalid policy ID
        ok = GetProcessMitigationPolicy(win32::GetCurrentProcess(), static_cast<PROCESS_MITIGATION_POLICY>(9999), &dep, sizeof(dep));
        TEST_ASSERT(!ok && win32::GetLastError() == 87, "Invalid policy enum must return FALSE and ERROR_INVALID_PARAMETER (87)");

        // SetProcessMitigationPolicy invalid parameters
        ok = SetProcessMitigationPolicy(ProcessDEPPolicy, nullptr, sizeof(dep));
        TEST_ASSERT(!ok && win32::GetLastError() == 87, "Null buffer to SetPolicy must return FALSE and ERROR_INVALID_PARAMETER (87)");

        ok = SetProcessMitigationPolicy(ProcessDEPPolicy, &dep, 0);
        TEST_ASSERT(!ok && win32::GetLastError() == 87, "Zero length to SetPolicy must return FALSE and ERROR_INVALID_PARAMETER (87)");
    }

    // 4. Immutability & Permanence Invariant: Attempting to relax permanent DEP must fail with ERROR_ACCESS_DENIED (5)
    {
        PROCESS_MITIGATION_DEP_POLICY disableDep{};
        disableDep.Enable = 0;
        disableDep.Permanent = 0;
        win32::BOOL ok = SetProcessMitigationPolicy(ProcessDEPPolicy, &disableDep, sizeof(disableDep));
        TEST_ASSERT(!ok, "Disabling permanent DEP must return FALSE");
        TEST_ASSERT(win32::GetLastError() == 5, "Disabling permanent DEP must set ERROR_ACCESS_DENIED (5)");
    }

    // 5. Arbitrary Code Guard (ACG / Dynamic Code Policy) & Permanence
    {
        TEST_ASSERT(mgr.isDynamicCodeAllowed(), "Dynamic code should be allowed initially");

        PROCESS_MITIGATION_DYNAMIC_CODE_POLICY acg{};
        acg.ProhibitDynamicCode = 1;
        acg.AllowThreadOptOut = 0; // Permanent ACG
        win32::BOOL ok = SetProcessMitigationPolicy(ProcessDynamicCodePolicy, &acg, sizeof(acg));
        TEST_ASSERT(ok == win32::TRUE, "SetProcessMitigationPolicy(ProcessDynamicCodePolicy) must succeed");

        PROCESS_MITIGATION_DYNAMIC_CODE_POLICY qAcg{};
        ok = GetProcessMitigationPolicy(win32::GetCurrentProcess(), ProcessDynamicCodePolicy, &qAcg, sizeof(qAcg));
        TEST_ASSERT(ok == win32::TRUE && qAcg.ProhibitDynamicCode == 1, "ACG ProhibitDynamicCode must be active in query");
        TEST_ASSERT(!mgr.isDynamicCodeAllowed(), "isDynamicCodeAllowed must return false when ACG is active");

        // Attempting to relax permanent ACG must fail with ERROR_ACCESS_DENIED (5)
        acg.ProhibitDynamicCode = 0;
        ok = SetProcessMitigationPolicy(ProcessDynamicCodePolicy, &acg, sizeof(acg));
        TEST_ASSERT(!ok, "Attempt to disable permanent ACG must return FALSE");
        TEST_ASSERT(win32::GetLastError() == 5, "Attempt to disable permanent ACG must set ERROR_ACCESS_DENIED (5)");
    }

    // 6. Strict Handle Checking Policy & Permanence
    {
        PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY handlePol{};
        handlePol.RaiseExceptionOnInvalidHandleReference = 1;
        handlePol.HandleExceptionsPermanentlyEnabled = 1;
        win32::BOOL ok = SetProcessMitigationPolicy(ProcessStrictHandleCheckPolicy, &handlePol, sizeof(handlePol));
        TEST_ASSERT(ok == win32::TRUE, "SetProcessMitigationPolicy(ProcessStrictHandleCheckPolicy) must succeed");

        PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY qHandle{};
        ok = GetProcessMitigationPolicy(win32::GetCurrentProcess(), ProcessStrictHandleCheckPolicy, &qHandle, sizeof(qHandle));
        TEST_ASSERT(ok == win32::TRUE && qHandle.RaiseExceptionOnInvalidHandleReference == 1, "Strict handle checking must be active in query");

        // Attempting to disable permanent strict handle check must fail
        handlePol.RaiseExceptionOnInvalidHandleReference = 0;
        ok = SetProcessMitigationPolicy(ProcessStrictHandleCheckPolicy, &handlePol, sizeof(handlePol));
        TEST_ASSERT(!ok, "Attempt to relax permanent strict handle must return FALSE");
        TEST_ASSERT(win32::GetLastError() == 5, "Attempt to relax permanent strict handle must set ERROR_ACCESS_DENIED (5)");
    }

    // 7. System Call Disable Policy (Win32k Lockdown) & Permanence
    {
        TEST_ASSERT(mgr.isWin32kAllowed(), "Win32k calls initially allowed");

        PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY sysCall{};
        sysCall.DisallowWin32kSystemCalls = 1;
        win32::BOOL ok = SetProcessMitigationPolicy(ProcessSystemCallDisablePolicy, &sysCall, sizeof(sysCall));
        TEST_ASSERT(ok == win32::TRUE, "SetProcessMitigationPolicy(ProcessSystemCallDisablePolicy) must succeed");
        TEST_ASSERT(!mgr.isWin32kAllowed(), "isWin32kAllowed must return false after Win32k lockdown");

        // Attempt to relax permanent Win32k lockdown must fail
        sysCall.DisallowWin32kSystemCalls = 0;
        ok = SetProcessMitigationPolicy(ProcessSystemCallDisablePolicy, &sysCall, sizeof(sysCall));
        TEST_ASSERT(!ok, "Attempt to relax permanent Win32k lockdown must return FALSE");
        TEST_ASSERT(win32::GetLastError() == 5, "Attempt to relax permanent Win32k lockdown must set ERROR_ACCESS_DENIED (5)");
    }

    // 8. Child Process Policy & Permanence
    {
        TEST_ASSERT(mgr.isChildProcessCreationAllowed(), "Child process creation initially allowed");

        PROCESS_MITIGATION_CHILD_PROCESS_POLICY childPol{};
        childPol.NoChildProcessCreation = 1;
        win32::BOOL ok = SetProcessMitigationPolicy(ProcessChildProcessPolicy, &childPol, sizeof(childPol));
        TEST_ASSERT(ok == win32::TRUE, "SetProcessMitigationPolicy(ProcessChildProcessPolicy) must succeed");
        TEST_ASSERT(!mgr.isChildProcessCreationAllowed(), "isChildProcessCreationAllowed must return false");

        // Attempt to relax permanent child process policy must fail
        childPol.NoChildProcessCreation = 0;
        ok = SetProcessMitigationPolicy(ProcessChildProcessPolicy, &childPol, sizeof(childPol));
        TEST_ASSERT(!ok, "Attempt to relax permanent child process policy must return FALSE");
        TEST_ASSERT(win32::GetLastError() == 5, "Attempt to relax permanent child process policy must set ERROR_ACCESS_DENIED (5)");
    }

    // 9. Extension Point, Signature, Font Disable, and Image Load Policies
    {
        // 9a. Extension Point
        PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY extPol{};
        extPol.DisableExtensionPoints = 1;
        win32::BOOL ok = SetProcessMitigationPolicy(ProcessExtensionPointDisablePolicy, &extPol, sizeof(extPol));
        TEST_ASSERT(ok == win32::TRUE, "SetProcessMitigationPolicy(ProcessExtensionPointDisablePolicy) must succeed");

        PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY qExt{};
        ok = GetProcessMitigationPolicy(win32::GetCurrentProcess(), ProcessExtensionPointDisablePolicy, &qExt, sizeof(qExt));
        TEST_ASSERT(ok == win32::TRUE && qExt.DisableExtensionPoints == 1, "Extension points must be disabled in query");

        // 9b. Signature Policy
        PROCESS_MITIGATION_SIGNATURE_POLICY sigPol{};
        sigPol.MicrosoftSignedOnly = 1;
        sigPol.StoreSignedOnly = 1;
        ok = SetProcessMitigationPolicy(ProcessSignaturePolicy, &sigPol, sizeof(sigPol));
        TEST_ASSERT(ok == win32::TRUE, "SetProcessMitigationPolicy(ProcessSignaturePolicy) must succeed");

        PROCESS_MITIGATION_SIGNATURE_POLICY qSig{};
        ok = GetProcessMitigationPolicy(win32::GetCurrentProcess(), ProcessSignaturePolicy, &qSig, sizeof(qSig));
        TEST_ASSERT(ok == win32::TRUE && qSig.MicrosoftSignedOnly == 1 && qSig.StoreSignedOnly == 1, "Signature policy must be active in query");

        // 9c. Font Disable Policy
        TEST_ASSERT(mgr.isNonSystemFontAllowed(), "Non-system fonts initially allowed");
        PROCESS_MITIGATION_FONT_DISABLE_POLICY fontPol{};
        fontPol.DisableNonSystemFonts = 1;
        ok = SetProcessMitigationPolicy(ProcessFontDisablePolicy, &fontPol, sizeof(fontPol));
        TEST_ASSERT(ok == win32::TRUE, "SetProcessMitigationPolicy(ProcessFontDisablePolicy) must succeed");
        TEST_ASSERT(!mgr.isNonSystemFontAllowed(), "isNonSystemFontAllowed must return false");

        // 9d. Image Load Policy
        TEST_ASSERT(mgr.isRemoteImageLoadingAllowed(), "Remote image loading initially allowed");
        PROCESS_MITIGATION_IMAGE_LOAD_POLICY imgPol{};
        imgPol.NoRemoteImages = 1;
        imgPol.PreferSystem32Images = 1;
        ok = SetProcessMitigationPolicy(ProcessImageLoadPolicy, &imgPol, sizeof(imgPol));
        TEST_ASSERT(ok == win32::TRUE, "SetProcessMitigationPolicy(ProcessImageLoadPolicy) must succeed");
        TEST_ASSERT(!mgr.isRemoteImageLoadingAllowed(), "isRemoteImageLoadingAllowed must return false");
    }

    // 10. Payload Restriction (EAF/IAF/ROP) and User Shadow Stack (CET)
    {
        // 10a. Payload Restriction
        TEST_ASSERT(!mgr.isPayloadRestrictionActive(), "Payload restriction initially inactive");
        PROCESS_MITIGATION_PAYLOAD_RESTRICTION_POLICY payloadPol{};
        payloadPol.EnableExportAddressFilter = 1;
        payloadPol.EnableExportAddressFilterPlus = 1;
        payloadPol.EnableImportAddressFilter = 1;
        payloadPol.EnableRopStackPivot = 1;
        payloadPol.EnableRopCallerCheck = 1;
        win32::BOOL ok = SetProcessMitigationPolicy(ProcessPayloadRestrictionPolicy, &payloadPol, sizeof(payloadPol));
        TEST_ASSERT(ok == win32::TRUE, "SetProcessMitigationPolicy(ProcessPayloadRestrictionPolicy) must succeed");
        TEST_ASSERT(mgr.isPayloadRestrictionActive(), "isPayloadRestrictionActive must return true");

        // 10b. User Shadow Stack (CET)
        TEST_ASSERT(!mgr.isShadowStackActive(), "Shadow stack initially inactive");
        PROCESS_MITIGATION_USER_SHADOW_STACK_POLICY cetPol{};
        cetPol.EnableUserShadowStack = 1;
        cetPol.EnableUserShadowStackStrictMode = 1;
        cetPol.SetContextIpValidation = 1;
        ok = SetProcessMitigationPolicy(ProcessUserShadowStackPolicy, &cetPol, sizeof(cetPol));
        TEST_ASSERT(ok == win32::TRUE, "SetProcessMitigationPolicy(ProcessUserShadowStackPolicy) must succeed");
        TEST_ASSERT(mgr.isShadowStackActive(), "isShadowStackActive must return true");

        // 10c. Side Channel Isolation & Redirection Trust
        PROCESS_MITIGATION_SIDE_CHANNEL_ISOLATION_POLICY scPol{};
        scPol.SmtBranchTargetIsolation = 1;
        scPol.DisableSpeculativeStoreBypass = 1;
        ok = SetProcessMitigationPolicy(ProcessSideChannelIsolationPolicy, &scPol, sizeof(scPol));
        TEST_ASSERT(ok == win32::TRUE, "SetProcessMitigationPolicy(ProcessSideChannelIsolationPolicy) must succeed");

        PROCESS_MITIGATION_REDIRECTION_TRUST_POLICY rtPol{};
        rtPol.EnforceRedirectionTrust = 1;
        ok = SetProcessMitigationPolicy(ProcessRedirectionTrustPolicy, &rtPol, sizeof(rtPol));
        TEST_ASSERT(ok == win32::TRUE, "SetProcessMitigationPolicy(ProcessRedirectionTrustPolicy) must succeed");
    }

    // 11. Violation Audit Trail & Sovereign Telemetry Counter Invariants
    {
        mgr.clearViolations();
        uint64_t beforeBlocked = mgr.getTotalViolationsBlocked();

        mgr.recordViolation(ProcessDynamicCodePolicy, "JIT RWX allocation blocked at 0x7FFF00100000", true);
        mgr.recordViolation(ProcessChildProcessPolicy, "cmd.exe spawn blocked under NoChildProcessCreation", true);

        auto viols = mgr.getViolations();
        TEST_ASSERT(viols.size() == 2, "Violations count must be 2");
        TEST_ASSERT(viols[0].policy == ProcessDynamicCodePolicy, "First violation policy must be ProcessDynamicCodePolicy");
        TEST_ASSERT(viols[0].policyName == "ProcessDynamicCodePolicy", "First violation policyName must match");
        TEST_ASSERT(viols[0].blocked == true, "First violation blocked flag must be true");
        TEST_ASSERT(viols[1].policy == ProcessChildProcessPolicy, "Second violation policy must be ProcessChildProcessPolicy");
        TEST_ASSERT(mgr.getTotalViolationsBlocked() == beforeBlocked + 2, "Violations blocked counter must increment by 2");

        mgr.clearViolations();
        TEST_ASSERT(mgr.getViolations().empty(), "Violations list must be empty after clearViolations");
    }

    // 12. Interactive Shell Integration (guard / exploitguard / sentinel guard)
    {
        shell::CommandShell proc;
        std::ostringstream oss;

        // 12a. guard /?
        int shellRet = proc.execute("guard /?", oss);
        TEST_ASSERT(shellRet == 0, "guard /? must return 0");
        TEST_ASSERT(oss.str().find("SentinelGuard / mitlib.dll") != std::string::npos, "Help must reference SentinelGuard");

        // 12b. guard status
        oss.str("");
        shellRet = proc.execute("guard status", oss);
        TEST_ASSERT(shellRet == 0, "guard status must return 0");
        TEST_ASSERT(oss.str().find("SentinelGuard") != std::string::npos, "Must show SentinelGuard status header");
        TEST_ASSERT(oss.str().find("Data Execution Prevention (DEP)") != std::string::npos, "Must show DEP status");

        // 12c. guard list
        oss.str("");
        shellRet = proc.execute("guard list", oss);
        TEST_ASSERT(shellRet == 0, "guard list must return 0");
        TEST_ASSERT(oss.str().find("ProcessDEPPolicy") != std::string::npos, "Must list ProcessDEPPolicy");
        TEST_ASSERT(oss.str().find("ProcessUserShadowStackPolicy") != std::string::npos, "Must list ProcessUserShadowStackPolicy");

        // 12d. guard enable dynamiccode
        oss.str("");
        shellRet = proc.execute("guard enable dynamiccode", oss);
        TEST_ASSERT(shellRet == 0, "guard enable dynamiccode must return 0");
        TEST_ASSERT(oss.str().find("ProcessDynamicCodePolicy (ACG) successfully enabled") != std::string::npos, "Must confirm ACG enabled");

        // 12e. guard test (diagnostic self-test)
        oss.str("");
        shellRet = proc.execute("guard test", oss);
        TEST_ASSERT(shellRet == 0, "guard test must return 0");
        TEST_ASSERT(oss.str().find("[SUCCESS]") != std::string::npos, "guard test must report [SUCCESS]");

        // 12f. sentinel guard
        oss.str("");
        shellRet = proc.execute("sentinel guard", oss);
        TEST_ASSERT(shellRet == 0, "sentinel guard must return 0");
        TEST_ASSERT(oss.str().find("SentinelGuard") != std::string::npos, "sentinel guard must route to guard status");
    }

    std::cout << "[TEST] Suite 139: Windows Defender Exploit Guard (SentinelGuard) Subsystem PASSED.\n";
}

// ============================================================================
// Suite 140: Windows Credential Guard & Isolated User Mode Subsystem
// ============================================================================
void Test_WindowsCredentialGuard_SentinelCredGuard_Subsystem() {
    using namespace micant::credguard;

    // 1. Dynamic Subsystem Initialization & Win32 C ABI Export Resolution
    InitializeCredGuardSubsystemExports();

    auto& loader = ldr::DynamicLoader::get();

    // Check sspicli.dll exports
    TEST_ASSERT(loader.getExport("sspicli.dll", "LsaOpenPolicy") != nullptr, "sspicli!LsaOpenPolicy must be exported");
    TEST_ASSERT(loader.getExport("sspicli.dll", "LsaClose") != nullptr, "sspicli!LsaClose must be exported");
    TEST_ASSERT(loader.getExport("sspicli.dll", "LsaFreeMemory") != nullptr, "sspicli!LsaFreeMemory must be exported");
    TEST_ASSERT(loader.getExport("sspicli.dll", "LsaQueryInformationPolicy") != nullptr, "sspicli!LsaQueryInformationPolicy must be exported");
    TEST_ASSERT(loader.getExport("sspicli.dll", "LsaSetInformationPolicy") != nullptr, "sspicli!LsaSetInformationPolicy must be exported");
    TEST_ASSERT(loader.getExport("sspicli.dll", "LsaEnumerateLogonSessions") != nullptr, "sspicli!LsaEnumerateLogonSessions must be exported");
    TEST_ASSERT(loader.getExport("sspicli.dll", "LsaGetLogonSessionData") != nullptr, "sspicli!LsaGetLogonSessionData must be exported");
    TEST_ASSERT(loader.getExport("sspicli.dll", "LsaRegisterLogonProcess") != nullptr, "sspicli!LsaRegisterLogonProcess must be exported");
    TEST_ASSERT(loader.getExport("sspicli.dll", "LsaDeregisterLogonProcess") != nullptr, "sspicli!LsaDeregisterLogonProcess must be exported");
    TEST_ASSERT(loader.getExport("sspicli.dll", "LsaLookupAuthenticationPackage") != nullptr, "sspicli!LsaLookupAuthenticationPackage must be exported");
    TEST_ASSERT(loader.getExport("sspicli.dll", "LsaCallAuthenticationPackage") != nullptr, "sspicli!LsaCallAuthenticationPackage must be exported");
    TEST_ASSERT(loader.getExport("sspicli.dll", "LsaFreeReturnBuffer") != nullptr, "sspicli!LsaFreeReturnBuffer must be exported");

    // Check secur32.dll forwards
    TEST_ASSERT(loader.getExport("secur32.dll", "LsaOpenPolicy") != nullptr, "secur32!LsaOpenPolicy must be exported");
    TEST_ASSERT(loader.getExport("secur32.dll", "LsaQueryInformationPolicy") != nullptr, "secur32!LsaQueryInformationPolicy must be exported");
    TEST_ASSERT(loader.getExport("secur32.dll", "LsaEnumerateLogonSessions") != nullptr, "secur32!LsaEnumerateLogonSessions must be exported");

    // Check lsasrv.dll Credential Guard specialized exports
    TEST_ASSERT(loader.getExport("lsasrv.dll", "CredGuardGetState") != nullptr, "lsasrv!CredGuardGetState must be exported");
    TEST_ASSERT(loader.getExport("lsasrv.dll", "CredGuardSetState") != nullptr, "lsasrv!CredGuardSetState must be exported");
    TEST_ASSERT(loader.getExport("lsasrv.dll", "CredGuardIsLsaIsoRunning") != nullptr, "lsasrv!CredGuardIsLsaIsoRunning must be exported");
    TEST_ASSERT(loader.getExport("lsasrv.dll", "CredGuardProtectSecret") != nullptr, "lsasrv!CredGuardProtectSecret must be exported");
    TEST_ASSERT(loader.getExport("lsasrv.dll", "CredGuardUnsealSecret") != nullptr, "lsasrv!CredGuardUnsealSecret must be exported");
    TEST_ASSERT(loader.getExport("lsasrv.dll", "CredGuardChallengeResponse") != nullptr, "lsasrv!CredGuardChallengeResponse must be exported");
    TEST_ASSERT(loader.getExport("lsasrv.dll", "CredGuardInterceptDump") != nullptr, "lsasrv!CredGuardInterceptDump must be exported");

    // Version database registration verification
    auto modLsa = version::VersionDatabase::Instance().GetModuleInfo("lsasrv.dll");
    TEST_ASSERT(modLsa != nullptr, "lsasrv.dll must be registered in VersionDatabase");
    TEST_ASSERT(modLsa->stringTable.at("FileVersion") == "10.0.26100.1", "lsasrv.dll version must be 10.0.26100.1");

    auto modSspi = version::VersionDatabase::Instance().GetModuleInfo("sspicli.dll");
    TEST_ASSERT(modSspi != nullptr, "sspicli.dll must be registered in VersionDatabase");

    auto& mgr = SentinelCredGuardManager::get();
    mgr.resetToBaseline();

    // 2. Baseline Credential Guard State & LsaQueryInformationPolicy
    {
        uint32_t status = 999;
        uint32_t flags = 0;
        win32::BOOL ok = CredGuardGetState(&status, &flags);
        TEST_ASSERT(ok == win32::TRUE, "CredGuardGetState must succeed");
        TEST_ASSERT(status == CREDGUARD_STATUS_DISABLED, "Initial status must be CREDGUARD_STATUS_DISABLED");
        TEST_ASSERT(!mgr.isLsaIsoRunning(), "LsaIso must not be running initially");
        TEST_ASSERT(!mgr.isUefiLocked(), "UEFI lock must not be active initially");

        // LsaQueryInformationPolicy (DeviceGuard)
        void* buffer = nullptr;
        NTSTATUS st = LsaQueryInformationPolicy(0, PolicyDeviceGuardInformation, &buffer);
        TEST_ASSERT(st == STATUS_SUCCESS && buffer != nullptr, "LsaQueryInformationPolicy(DeviceGuard) must succeed");
        const auto* dg = static_cast<const POLICY_DEVICE_GUARD_INFO*>(buffer);
        TEST_ASSERT(dg->CredGuardStatus == CREDGUARD_STATUS_DISABLED, "CredGuardStatus must be 0");
        TEST_ASSERT(dg->LsaIsoPid == 0, "LsaIsoPid must be 0 initially");
        LsaFreeMemory(buffer);
    }

    // 3. Credential Guard Activation without UEFI Lock
    {
        win32::BOOL ok = CredGuardSetState(CREDGUARD_STATUS_ENABLED_WITHOUT_UEFI_LOCK, 0);
        TEST_ASSERT(ok == win32::TRUE, "CredGuardSetState without lock must succeed");

        uint32_t status = 0;
        uint32_t flags = 0;
        CredGuardGetState(&status, &flags);
        TEST_ASSERT(status == CREDGUARD_STATUS_ENABLED_WITHOUT_UEFI_LOCK, "Status must be enabled without lock");
        TEST_ASSERT((flags & CREDGUARD_FLAG_VBS_ENABLED) != 0, "VBS flag must be set");
        TEST_ASSERT((flags & CREDGUARD_FLAG_HVCI_ACTIVE) != 0, "HVCI flag must be set");
        TEST_ASSERT((flags & CREDGUARD_FLAG_LSAISO_RUNNING) != 0, "LSAISO_RUNNING flag must be set");
        TEST_ASSERT(CredGuardIsLsaIsoRunning() == win32::TRUE, "CredGuardIsLsaIsoRunning must return TRUE");

        // Query through LsaQueryInformationPolicy
        void* buffer = nullptr;
        NTSTATUS st = LsaQueryInformationPolicy(0, PolicyDeviceGuardInformation, &buffer);
        TEST_ASSERT(st == STATUS_SUCCESS && buffer != nullptr, "LsaQueryInformationPolicy must succeed");
        const auto* dg = static_cast<const POLICY_DEVICE_GUARD_INFO*>(buffer);
        TEST_ASSERT(dg->CredGuardStatus == CREDGUARD_STATUS_ENABLED_WITHOUT_UEFI_LOCK, "CredGuardStatus must match");
        TEST_ASSERT(dg->VbsStatus == 1, "VbsStatus must be 1");
        TEST_ASSERT(dg->LsaIsoPid == LSAISO_PROCESS_ID, "LsaIsoPid must be 500");
        LsaFreeMemory(buffer);

        // Can disable when not locked
        ok = CredGuardSetState(CREDGUARD_STATUS_DISABLED, 0);
        TEST_ASSERT(ok == win32::TRUE, "Disabling without lock must succeed");
        TEST_ASSERT(CredGuardIsLsaIsoRunning() == win32::FALSE, "LsaIso must stop after disable");
    }

    // 4. Secret Sealing into VTL 1 Enclave & Opaque Handle Issuance
    {
        mgr.enable(false);

        const uint8_t sampleHash[] = {
            0xAA, 0x11, 0xBB, 0x22, 0xCC, 0x33, 0xDD, 0x44,
            0xEE, 0x55, 0xFF, 0x66, 0x12, 0x34, 0x56, 0x78
        };
        uint64_t handleId = 0;
        win32::BOOL ok = CredGuardProtectSecret(sampleHash, sizeof(sampleHash), &handleId);
        TEST_ASSERT(ok == win32::TRUE && handleId != 0, "CredGuardProtectSecret must succeed and return non-zero handle");
        TEST_ASSERT(mgr.getIsolatedSecretCount() == 1, "Enclave must hold 1 secret");

        // Invalid parameters
        ok = CredGuardProtectSecret(nullptr, sizeof(sampleHash), &handleId);
        TEST_ASSERT(!ok && win32::GetLastError() == 87, "Null buffer must fail with ERROR_INVALID_PARAMETER (87)");
        ok = CredGuardProtectSecret(sampleHash, 0, &handleId);
        TEST_ASSERT(!ok && win32::GetLastError() == 87, "Zero length must fail with ERROR_INVALID_PARAMETER (87)");

        // Unseal Secret in authenticated context
        uint8_t unsealed[32]{};
        size_t cbUnsealed = sizeof(unsealed);
        ok = CredGuardUnsealSecret(handleId, unsealed, &cbUnsealed);
        TEST_ASSERT(ok == win32::TRUE, "CredGuardUnsealSecret must succeed");
        TEST_ASSERT(cbUnsealed == sizeof(sampleHash), "Unsealed length must match original");
        TEST_ASSERT(std::memcmp(sampleHash, unsealed, sizeof(sampleHash)) == 0, "Unsealed data must match original plaintext");

        // Buffer too small test
        size_t shortSize = 4;
        ok = CredGuardUnsealSecret(handleId, unsealed, &shortSize);
        TEST_ASSERT(!ok && win32::GetLastError() == 122, "Short buffer must fail with ERROR_INSUFFICIENT_BUFFER (122)");
    }

    // 5. In-Enclave Challenge-Response Authentication (Zero Plaintext Exposure)
    {
        const uint8_t sampleHash[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C};
        uint64_t hSecret = mgr.isolateSecret(L"MICANT", L"CorpUser", sampleHash);

        const uint8_t challenge[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
        uint8_t response[64]{};
        size_t cbResponse = sizeof(response);

        win32::BOOL ok = CredGuardChallengeResponse(hSecret, challenge, sizeof(challenge), response, &cbResponse);
        TEST_ASSERT(ok == win32::TRUE, "CredGuardChallengeResponse must succeed");
        TEST_ASSERT(cbResponse == 16, "NTLM challenge-response must be 16 bytes");

        // Verify that the computation matches clean-room RFC 1320 digest
        auto expected = lsass::crypto::computeChallengeResponse(sampleHash, challenge);
        TEST_ASSERT(std::memcmp(response, expected.data(), 16) == 0, "In-enclave challenge-response must match expected digest");
        TEST_ASSERT(mgr.getTotalEnclaveAuthentications() >= 1, "Enclave authentication counter must increment");
    }

    // 6. Mimikatz / ProcDump / MiniDumpWriteDump Memory Scraping Interception
    {
        uint64_t beforeBlocked = mgr.getTotalBlockedDumps();

        // 6a. Attempt to read LSASS memory with PROCESS_VM_READ (Mimikatz sekurlsa::logonpasswords)
        win32::BOOL ok = CredGuardInterceptDump(LSASS_PROCESS_ID, 0x0010 /* PROCESS_VM_READ */, "Mimikatz sekurlsa::logonpasswords");
        TEST_ASSERT(!ok, "Reading LSASS memory under Credential Guard must fail");
        TEST_ASSERT(win32::GetLastError() == 5, "Must set ERROR_ACCESS_DENIED (5)");

        // 6b. Attempt to open LsaIso enclave with PROCESS_ALL_ACCESS (ProcDump / MiniDump)
        ok = CredGuardInterceptDump(LSAISO_PROCESS_ID, 0x1FFFFF /* PROCESS_ALL_ACCESS */, "procdump.exe -ma lsaiso.exe");
        TEST_ASSERT(!ok, "Accessing LsaIso enclave memory must fail");
        TEST_ASSERT(win32::GetLastError() == 5, "Must set ERROR_ACCESS_DENIED (5)");

        // 6c. Verify audit records
        TEST_ASSERT(mgr.getTotalBlockedDumps() == beforeBlocked + 2, "Blocked dumps counter must increment by 2");
        auto audit = mgr.getAuditLog();
        TEST_ASSERT(audit.size() >= 2, "Audit log must contain at least 2 entries");
        TEST_ASSERT(audit.back().targetPid == LSAISO_PROCESS_ID, "Target PID must be LSAISO_PROCESS_ID");
        TEST_ASSERT(audit.back().blocked == true, "Blocked must be true");
    }

    // 7. Hardware UEFI Lock Immutability
    {
        // Enable with UEFI lock
        win32::BOOL ok = CredGuardSetState(CREDGUARD_STATUS_ENABLED_WITH_UEFI_LOCK, 0);
        TEST_ASSERT(ok == win32::TRUE, "Enabling with UEFI lock must succeed");
        TEST_ASSERT(mgr.isUefiLocked(), "isUefiLocked must be true");

        // Attempt to disable must be rejected with ERROR_ACCESS_DENIED (5)
        ok = CredGuardSetState(CREDGUARD_STATUS_DISABLED, 0);
        TEST_ASSERT(!ok, "Attempt to disable UEFI-locked Credential Guard must fail");
        TEST_ASSERT(win32::GetLastError() == 5, "Attempt to disable UEFI-locked Credential Guard must set ERROR_ACCESS_DENIED (5)");

        // Attempting to disable via LsaSetInformationPolicy must also return STATUS_ACCESS_DENIED
        POLICY_DEVICE_GUARD_INFO disableInfo{};
        disableInfo.CredGuardStatus = CREDGUARD_STATUS_DISABLED;
        NTSTATUS st = LsaSetInformationPolicy(0, PolicyDeviceGuardInformation, &disableInfo);
        TEST_ASSERT(st == STATUS_ACCESS_DENIED, "LsaSetInformationPolicy disable must return STATUS_ACCESS_DENIED");
    }

    // 8. Standard LSA Policy & Logon Session Enumeration
    {
        uintptr_t hPolicy = 0;
        NTSTATUS st = LsaOpenPolicy(nullptr, nullptr, 0, &hPolicy);
        TEST_ASSERT(st == STATUS_SUCCESS && hPolicy != 0, "LsaOpenPolicy must succeed");

        // Query Primary Domain Information
        void* domainBuf = nullptr;
        st = LsaQueryInformationPolicy(hPolicy, PolicyPrimaryDomainInformation, &domainBuf);
        TEST_ASSERT(st == STATUS_SUCCESS && domainBuf != nullptr, "LsaQueryInformationPolicy(PrimaryDomain) must succeed");
        const auto* pdi = static_cast<const POLICY_PRIMARY_DOMAIN_INFO*>(domainBuf);
        TEST_ASSERT(std::wcscmp(pdi->Name.Buffer, L"MICANT") == 0, "Domain name must be MICANT");
        LsaFreeMemory(domainBuf);

        // Enumerate logon sessions
        uint32_t sessionCount = 0;
        Luid* pSessionList = nullptr;
        st = LsaEnumerateLogonSessions(&sessionCount, &pSessionList);
        TEST_ASSERT(st == STATUS_SUCCESS, "LsaEnumerateLogonSessions must succeed");
        TEST_ASSERT(sessionCount >= 1 && pSessionList != nullptr, "Must have at least 1 active logon session");

        // Get Logon Session Data for first session
        SECURITY_LOGON_SESSION_DATA* pSessionData = nullptr;
        st = LsaGetLogonSessionData(&pSessionList[0], &pSessionData);
        TEST_ASSERT(st == STATUS_SUCCESS && pSessionData != nullptr, "LsaGetLogonSessionData must succeed");
        TEST_ASSERT(pSessionData->Size == sizeof(SECURITY_LOGON_SESSION_DATA), "Session data size must match struct size");
        LsaFreeMemory(pSessionData);
        LsaFreeMemory(pSessionList);

        // Register and deregister logon process
        LSA_STRING procName{static_cast<uint16_t>(6), static_cast<uint16_t>(7), const_cast<char*>("winlog")};
        uintptr_t hLsa = 0;
        uint32_t mode = 0;
        st = LsaRegisterLogonProcess(&procName, &hLsa, &mode);
        TEST_ASSERT(st == STATUS_SUCCESS && hLsa != 0, "LsaRegisterLogonProcess must succeed");

        uint32_t pkgId = 0;
        LSA_STRING pkgName{static_cast<uint16_t>(6), static_cast<uint16_t>(7), const_cast<char*>("MSV1_0")};
        st = LsaLookupAuthenticationPackage(hLsa, &pkgName, &pkgId);
        TEST_ASSERT(st == STATUS_SUCCESS && pkgId == 1, "LsaLookupAuthenticationPackage for MSV1_0 must succeed");

        st = LsaDeregisterLogonProcess(hLsa);
        TEST_ASSERT(st == STATUS_SUCCESS, "LsaDeregisterLogonProcess must succeed");

        LsaClose(hPolicy);
    }

    // 9. Interactive Shell Integration (credguard / sentinel credguard)
    {
        shell::CommandShell proc;
        std::ostringstream oss;

        // 9a. credguard /?
        int shellRet = proc.execute("credguard /?", oss);
        TEST_ASSERT(shellRet == 0, "credguard /? must return 0");
        TEST_ASSERT(oss.str().find("SentinelCredGuard / lsasrv.dll") != std::string::npos, "Help must reference SentinelCredGuard");

        // 9b. credguard status
        oss.str("");
        shellRet = proc.execute("credguard status", oss);
        TEST_ASSERT(shellRet == 0, "credguard status must return 0");
        TEST_ASSERT(oss.str().find("Sovereign Credential Guard (SentinelCredGuard) Posture:") != std::string::npos, "Must show posture header");
        TEST_ASSERT(oss.str().find("Virtualization-Based Security (VBS):") != std::string::npos, "Must show VBS status");

        // 9c. credguard isolate
        oss.str("");
        shellRet = proc.execute("credguard isolate Alice SecretPassword999!", oss);
        TEST_ASSERT(shellRet == 0, "credguard isolate must return 0");
        TEST_ASSERT(oss.str().find("isolated into VTL 1 enclave") != std::string::npos, "Must confirm credential sealed");

        // 9d. credguard dump-attempt
        oss.str("");
        shellRet = proc.execute("credguard dump-attempt", oss);
        TEST_ASSERT(shellRet == 0, "credguard dump-attempt must return 0");
        TEST_ASSERT(oss.str().find("STATUS_ACCESS_DENIED") != std::string::npos, "Must confirm dump blocked with STATUS_ACCESS_DENIED");

        // 9e. credguard test (diagnostic self-test)
        oss.str("");
        shellRet = proc.execute("credguard test", oss);
        TEST_ASSERT(shellRet == 0, "credguard test must return 0");
        TEST_ASSERT(oss.str().find("[SUCCESS]") != std::string::npos, "credguard test must report [SUCCESS]");

        // 9f. sentinel credguard
        oss.str("");
        shellRet = proc.execute("sentinel credguard", oss);
        TEST_ASSERT(shellRet == 0, "sentinel credguard must return 0");
        TEST_ASSERT(oss.str().find("Sovereign Credential Guard (SentinelCredGuard) Posture:") != std::string::npos, "sentinel credguard must route to posture");
    }

    std::cout << "[TEST] Suite 140: Windows Credential Guard & Isolated User Mode Subsystem PASSED.\n";
}

// ============================================================================
// Suite 141: Windows Protected Process Light (PPL) & ELAM Subsystem
// ============================================================================
void Test_WindowsProtectedProcessLight_ELAM_Subsystem() {
    using namespace micant::ppl;

    InitializePplSubsystemExports();

    // 1. DynamicLoader Export Registration in ntoskrnl.exe and kernel32.dll
    {
        auto& loader = ldr::DynamicLoader::get();

        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "PsIsProtectedProcess") != nullptr, "ntoskrnl.exe must export PsIsProtectedProcess");
        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "PsGetProcessProtection") != nullptr, "ntoskrnl.exe must export PsGetProcessProtection");
        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "PsSetProcessProtection") != nullptr, "ntoskrnl.exe must export PsSetProcessProtection");
        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "PsFilterAccessMask") != nullptr, "ntoskrnl.exe must export PsFilterAccessMask");
        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "PsTerminateProcessSecure") != nullptr, "ntoskrnl.exe must export PsTerminateProcessSecure");
        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "IoRegisterBootDriverCallback") != nullptr, "ntoskrnl.exe must export IoRegisterBootDriverCallback");
        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "IoUnRegisterBootDriverCallback") != nullptr, "ntoskrnl.exe must export IoUnRegisterBootDriverCallback");

        TEST_ASSERT(loader.getExport("kernel32.dll", "PsIsProtectedProcess") != nullptr, "kernel32.dll must export PsIsProtectedProcess");
        TEST_ASSERT(loader.getExport("kernel32.dll", "ElamGetDriverClassification") != nullptr, "kernel32.dll must export ElamGetDriverClassification");
        TEST_ASSERT(loader.getExport("kernel32.dll", "ElamSetDriverClassification") != nullptr, "kernel32.dll must export ElamSetDriverClassification");
        TEST_ASSERT(loader.getExport("kernel32.dll", "ElamEvaluateBootDriver") != nullptr, "kernel32.dll must export ElamEvaluateBootDriver");
    }

    // 2. VersionDatabase Registration for ntoskrnl.exe and elam.sys
    {
        auto modKernel = version::VersionDatabase::Instance().GetModuleInfo("ntoskrnl.exe");
        TEST_ASSERT(modKernel != nullptr, "VersionDatabase must register ntoskrnl.exe");
        TEST_ASSERT(modKernel->stringTable.at("FileVersion") == "10.0.26100.1", "ntoskrnl.exe version must match Windows 11 build");
        TEST_ASSERT(modKernel->stringTable.at("FileDescription").find("Kernel") != std::string::npos, "ntoskrnl.exe description must mention Kernel");

        auto modElam = version::VersionDatabase::Instance().GetModuleInfo("elam.sys");
        TEST_ASSERT(modElam != nullptr, "VersionDatabase must register elam.sys");
        TEST_ASSERT(modElam->stringTable.at("FileVersion") == "10.0.26100.1", "elam.sys version must match Windows 11 build");
        TEST_ASSERT(modElam->stringTable.at("FileDescription").find("Early Launch") != std::string::npos, "elam.sys description must mention Early Launch");
    }

    // 3. Pre-Seeded Protected Daemons (System, lsass, LsaIso, csrss, MsMpEng)
    {
        auto& mgr = ProtectedProcessManager::get();
        mgr.reset(); // Reset to fresh baseline

        // PID 4: System (Protected, WinSystem)
        PS_PROTECTION prot{};
        TEST_ASSERT(mgr.getProcessProtection(4, prot), "PID 4 (System) must be registered in PPL");
        TEST_ASSERT(prot.Type == PsProtectedTypeProtected, "System must have Type=Protected");
        TEST_ASSERT(prot.Signer == PsProtectedSignerWinSystem, "System must have Signer=WinSystem");

        // PID 492: lsass.exe (ProtectedLight, Lsa)
        TEST_ASSERT(mgr.getProcessProtection(492, prot), "PID 492 (lsass.exe) must be registered in PPL");
        TEST_ASSERT(prot.Type == PsProtectedTypeProtectedLight, "lsass.exe must have Type=ProtectedLight");
        TEST_ASSERT(prot.Signer == PsProtectedSignerLsa, "lsass.exe must have Signer=Lsa");

        // PID 500: LsaIso.exe (Protected, Lsa)
        TEST_ASSERT(mgr.getProcessProtection(500, prot), "PID 500 (LsaIso.exe) must be registered in PPL");
        TEST_ASSERT(prot.Type == PsProtectedTypeProtected, "LsaIso.exe must have Type=Protected");
        TEST_ASSERT(prot.Signer == PsProtectedSignerLsa, "LsaIso.exe must have Signer=Lsa");

        // PID 600: csrss.exe (Protected, WinTcb)
        TEST_ASSERT(mgr.getProcessProtection(600, prot), "PID 600 (csrss.exe) must be registered in PPL");
        TEST_ASSERT(prot.Type == PsProtectedTypeProtected, "csrss.exe must have Type=Protected");
        TEST_ASSERT(prot.Signer == PsProtectedSignerWinTcb, "csrss.exe must have Signer=WinTcb");

        // PID 900: MsMpEng.exe (ProtectedLight, Antimalware)
        TEST_ASSERT(mgr.getProcessProtection(900, prot), "PID 900 (MsMpEng.exe) must be registered in PPL");
        TEST_ASSERT(prot.Type == PsProtectedTypeProtectedLight, "MsMpEng.exe must have Type=ProtectedLight");
        TEST_ASSERT(prot.Signer == PsProtectedSignerAntimalware, "MsMpEng.exe must have Signer=Antimalware");

        // Check C ABI PsIsProtectedProcess
        TEST_ASSERT(PsIsProtectedProcess(900) == 1, "PsIsProtectedProcess must return 1 for MsMpEng.exe");
        TEST_ASSERT(PsIsProtectedProcess(12345) == 0, "PsIsProtectedProcess must return 0 for unprotected process");
    }

    // 4. Signer Dominance Matrix (RtlTestProtectedAccess)
    {
        PS_PROTECTION unpriv{};
        unpriv.Type = PsProtectedTypeNone;
        unpriv.Signer = PsProtectedSignerNone;

        PS_PROTECTION antimalware{};
        antimalware.Type = PsProtectedTypeProtectedLight;
        antimalware.Signer = PsProtectedSignerAntimalware;

        PS_PROTECTION windows{};
        windows.Type = PsProtectedTypeProtectedLight;
        windows.Signer = PsProtectedSignerWindows;

        PS_PROTECTION winTcb{};
        winTcb.Type = PsProtectedTypeProtected;
        winTcb.Signer = PsProtectedSignerWinTcb;

        PS_PROTECTION winSystem{};
        winSystem.Type = PsProtectedTypeProtected;
        winSystem.Signer = PsProtectedSignerWinSystem;

        // Unprivileged cannot access any protected process
        TEST_ASSERT(!RtlTestProtectedAccess(unpriv, antimalware), "Unprivileged cannot access Antimalware");
        TEST_ASSERT(!RtlTestProtectedAccess(unpriv, winTcb), "Unprivileged cannot access WinTcb");

        // Protected process CAN access unprivileged process
        TEST_ASSERT(RtlTestProtectedAccess(antimalware, unpriv), "Antimalware can access unprivileged process");

        // Peer Antimalware can access Antimalware
        TEST_ASSERT(RtlTestProtectedAccess(antimalware, antimalware), "Antimalware can access peer Antimalware");

        // Antimalware CANNOT access Windows, WinTcb, or WinSystem
        TEST_ASSERT(!RtlTestProtectedAccess(antimalware, windows), "Antimalware cannot dominate Windows");
        TEST_ASSERT(!RtlTestProtectedAccess(antimalware, winTcb), "Antimalware cannot dominate WinTcb");
        TEST_ASSERT(!RtlTestProtectedAccess(antimalware, winSystem), "Antimalware cannot dominate WinSystem");

        // Windows dominates Antimalware
        TEST_ASSERT(RtlTestProtectedAccess(windows, antimalware), "Windows dominates Antimalware");

        // WinTcb dominates Windows and Antimalware
        TEST_ASSERT(RtlTestProtectedAccess(winTcb, windows), "WinTcb dominates Windows");
        TEST_ASSERT(RtlTestProtectedAccess(winTcb, antimalware), "WinTcb dominates Antimalware");

        // WinSystem dominates everything
        TEST_ASSERT(RtlTestProtectedAccess(winSystem, winTcb), "WinSystem dominates WinTcb");
        TEST_ASSERT(RtlTestProtectedAccess(winSystem, antimalware), "WinSystem dominates Antimalware");
    }

    // 5. Access Mask Sanitization (PsFilterAccessMask)
    {
        uint32_t granted = 0;

        // Unprivileged process (PID 1000) requests PROCESS_ALL_ACCESS against MsMpEng (PID 900)
        NTSTATUS st = PsFilterAccessMask(1000, 900, PROCESS_ALL_ACCESS, &granted);
        TEST_ASSERT(st == STATUS_SUCCESS, "PsFilterAccessMask must return STATUS_SUCCESS");
        // Must strip PROCESS_TERMINATE (0x1), PROCESS_VM_WRITE (0x20), PROCESS_VM_READ (0x10), etc.
        TEST_ASSERT((granted & PROCESS_TERMINATE) == 0, "PROCESS_TERMINATE must be stripped");
        TEST_ASSERT((granted & PROCESS_VM_WRITE) == 0, "PROCESS_VM_WRITE must be stripped");
        TEST_ASSERT((granted & PROCESS_VM_READ) == 0, "PROCESS_VM_READ must be stripped");
        TEST_ASSERT((granted & PROCESS_CREATE_THREAD) == 0, "PROCESS_CREATE_THREAD must be stripped");
        TEST_ASSERT((granted & PROCESS_SUSPEND_RESUME) == 0, "PROCESS_SUSPEND_RESUME must be stripped");
        // Safe read-only flags are preserved
        TEST_ASSERT((granted & PROCESS_QUERY_LIMITED_INFORMATION) != 0, "PROCESS_QUERY_LIMITED_INFORMATION must be preserved");

        // Unprivileged process requests ONLY PROCESS_TERMINATE -> all stripped -> returns STATUS_ACCESS_DENIED
        uint32_t onlyTermGranted = 0;
        st = PsFilterAccessMask(1000, 900, PROCESS_TERMINATE, &onlyTermGranted);
        TEST_ASSERT(st == STATUS_ACCESS_DENIED, "Exclusive dangerous right request must return STATUS_ACCESS_DENIED");
        TEST_ASSERT(onlyTermGranted == 0, "No access granted for exclusive dangerous request");

        // WinSystem (PID 4) requests PROCESS_ALL_ACCESS against MsMpEng -> granted completely
        uint32_t sysGranted = 0;
        st = PsFilterAccessMask(4, 900, PROCESS_ALL_ACCESS, &sysGranted);
        TEST_ASSERT(st == STATUS_SUCCESS, "PsFilterAccessMask for WinSystem must succeed");
        TEST_ASSERT(sysGranted == PROCESS_ALL_ACCESS, "WinSystem must receive full PROCESS_ALL_ACCESS");
    }

    // 6. Process Termination Immunity & SeDebugPrivilege Escalation Defense
    {
        auto& mgr = ProtectedProcessManager::get();

        // 6a. Attempt to terminate MsMpEng.exe from unprivileged caller without SeDebugPrivilege
        NTSTATUS st = mgr.attemptTerminate(1000, 900, false);
        TEST_ASSERT(st == STATUS_ACCESS_DENIED, "attemptTerminate against MsMpEng without privilege must return STATUS_ACCESS_DENIED");

        // 6b. Attempt to terminate MsMpEng.exe with SeDebugPrivilege enabled (Mimikatz / Taskkill admin)
        // In PPL, SeDebugPrivilege MUST NOT bypass process protection
        st = mgr.attemptTerminate(1000, 900, true);
        TEST_ASSERT(st == STATUS_ACCESS_DENIED, "attemptTerminate against MsMpEng with SeDebugPrivilege MUST STILL return STATUS_ACCESS_DENIED");

        // 6c. Attempt to terminate csrss.exe (WinTcb) from Antimalware (lower signer)
        st = mgr.attemptTerminate(900, 600, false);
        TEST_ASSERT(st == STATUS_ACCESS_DENIED, "Antimalware cannot terminate WinTcb (csrss.exe)");

        // 6d. WinSystem terminating an unprivileged or dominated process
        st = mgr.attemptTerminate(4, 900, false);
        TEST_ASSERT(st == STATUS_SUCCESS, "WinSystem can terminate dominated Antimalware process");

        // 6e. Audit log records blocked terminations
        auto audit = mgr.getAuditLog();
        TEST_ASSERT(!audit.empty(), "Audit log must contain blocked termination events");
        TEST_ASSERT(audit.back().targetPid == 600 || audit.back().targetPid == 900, "Audit log must identify target PID");
    }

    // 7. Early Launch Anti-Malware (ELAM) Boot Driver Classification & Policy
    {
        auto& elam = EarlyLaunchAntiMalwareManager::get();
        elam.reset();

        // Check pre-seeded classifications
        TEST_ASSERT(elam.getDriverClassification(L"disk.sys") == BDCB_CLASSIFICATION_KNOWN_GOOD, "disk.sys must be KNOWN_GOOD");
        TEST_ASSERT(elam.getDriverClassification(L"rootkit.sys") == BDCB_CLASSIFICATION_KNOWN_BAD, "rootkit.sys must be KNOWN_BAD");
        TEST_ASSERT(elam.getDriverClassification(L"unknown_driver.sys") == BDCB_CLASSIFICATION_UNKNOWN, "unknown driver must be UNKNOWN");

        // Evaluate known good driver under default policy (GOOD_AND_UNKNOWN)
        NTSTATUS st = elam.evaluateBootDriver(L"disk.sys");
        TEST_ASSERT(st == STATUS_SUCCESS, "disk.sys evaluation must succeed");

        // Evaluate unknown driver under default policy (GOOD_AND_UNKNOWN)
        st = elam.evaluateBootDriver(L"third_party_controller.sys");
        TEST_ASSERT(st == STATUS_SUCCESS, "unknown driver must be allowed under standard default policy");

        // Evaluate known bad rootkit driver -> MUST BE BLOCKED
        st = elam.evaluateBootDriver(L"rootkit.sys");
        TEST_ASSERT(st == STATUS_ACCESS_DENIED, "rootkit.sys evaluation MUST return STATUS_ACCESS_DENIED");

        // Verify blocked drivers log
        auto blocked = elam.getBlockedDrivers();
        TEST_ASSERT(!blocked.empty(), "Blocked drivers list must contain rootkit.sys");
        TEST_ASSERT(blocked.back().driverPath == L"rootkit.sys", "Blocked record must record rootkit path");

        // Switch policy to GOOD_ONLY
        elam.setPolicy(ELAM_POLICY_GOOD_ONLY);
        st = elam.evaluateBootDriver(L"third_party_controller.sys");
        TEST_ASSERT(st == STATUS_ACCESS_DENIED, "unknown driver must be blocked under GOOD_ONLY policy");

        // Switch policy to ALL (audit mode)
        elam.setPolicy(ELAM_POLICY_ALL);
        st = elam.evaluateBootDriver(L"rootkit.sys");
        TEST_ASSERT(st == STATUS_SUCCESS, "all drivers allowed under ALL/Audit policy");

        elam.setPolicy(ELAM_POLICY_GOOD_AND_UNKNOWN);
    }

    // 8. Dynamic ELAM Boot Driver Callback Registration & Inspection
    {
        auto& elam = EarlyLaunchAntiMalwareManager::get();
        void* hCallback = nullptr;

        static bool s_callbackInvoked = false;
        PBOOT_DRIVER_CALLBACK_FUNCTION myCallback = [](void*, BDCB_CALLBACK_TYPE type, void* info) -> NTSTATUS {
            if (type == BdCbInitializeImage && info) {
                s_callbackInvoked = true;
                auto* img = static_cast<BDCB_IMAGE_INFORMATION*>(info);
                if (img->ImagePath.find(L"dynamic_malware") != std::wstring::npos) {
                    img->Classification = BDCB_CLASSIFICATION_KNOWN_BAD;
                }
            }
            return STATUS_SUCCESS;
        };

        NTSTATUS st = IoRegisterBootDriverCallback(myCallback, nullptr, &hCallback);
        TEST_ASSERT(st == STATUS_SUCCESS && hCallback != nullptr, "IoRegisterBootDriverCallback must succeed");
        TEST_ASSERT(elam.getCallbackCount() == 1, "Callback count must be 1");

        // Evaluate image that triggers dynamic callback classification
        s_callbackInvoked = false;
        st = ElamEvaluateBootDriver(L"dynamic_malware.sys", "abc123sha256hash");
        TEST_ASSERT(s_callbackInvoked, "Registered ELAM callback must be invoked during driver evaluation");
        TEST_ASSERT(st == STATUS_ACCESS_DENIED, "Driver classified by callback as KNOWN_BAD must be blocked");

        // Unregister callback
        st = IoUnRegisterBootDriverCallback(hCallback);
        TEST_ASSERT(st == STATUS_SUCCESS, "IoUnRegisterBootDriverCallback must succeed");
        TEST_ASSERT(elam.getCallbackCount() == 0, "Callback count must be 0 after unregister");
    }

    // 9. Interactive Shell Integration (ppl, elam, sentinel ppl, sentinel elam)
    {
        shell::CommandShell proc;
        std::ostringstream oss;

        // 9a. ppl /?
        int shellRet = proc.execute("ppl /?", oss);
        TEST_ASSERT(shellRet == 0, "ppl /? must return 0");
        TEST_ASSERT(oss.str().find("Protected Process Light (PPL)") != std::string::npos, "Help must reference PPL");

        // 9b. ppl status
        oss.str("");
        shellRet = proc.execute("ppl status", oss);
        TEST_ASSERT(shellRet == 0, "ppl status must return 0");
        TEST_ASSERT(oss.str().find("MsMpEng.exe") != std::string::npos, "ppl status must display MsMpEng.exe");
        TEST_ASSERT(oss.str().find("ProtectedLight") != std::string::npos, "ppl status must display ProtectedLight");

        // 9c. ppl terminate-attempt
        oss.str("");
        shellRet = proc.execute("ppl terminate-attempt 900", oss);
        TEST_ASSERT(shellRet == 0, "ppl terminate-attempt must return 0");
        TEST_ASSERT(oss.str().find("STATUS_ACCESS_DENIED") != std::string::npos, "ppl terminate-attempt must confirm blocked termination");

        // 9d. ppl test
        oss.str("");
        shellRet = proc.execute("ppl test", oss);
        TEST_ASSERT(shellRet == 0, "ppl test must return 0");
        TEST_ASSERT(oss.str().find("[SUCCESS]") != std::string::npos, "ppl test must report [SUCCESS]");

        // 9e. elam /?
        oss.str("");
        shellRet = proc.execute("elam /?", oss);
        TEST_ASSERT(shellRet == 0, "elam /? must return 0");
        TEST_ASSERT(oss.str().find("Early Launch Anti-Malware (ELAM)") != std::string::npos, "Help must reference ELAM");

        // 9f. elam status
        oss.str("");
        shellRet = proc.execute("elam status", oss);
        TEST_ASSERT(shellRet == 0, "elam status must return 0");
        TEST_ASSERT(oss.str().find("Active ELAM Policy:") != std::string::npos, "elam status must show policy");
        TEST_ASSERT(oss.str().find("disk.sys") != std::string::npos, "elam status must show disk.sys");

        // 9g. elam test
        oss.str("");
        shellRet = proc.execute("elam test", oss);
        TEST_ASSERT(shellRet == 0, "elam test must return 0");
        TEST_ASSERT(oss.str().find("[SUCCESS]") != std::string::npos, "elam test must report [SUCCESS]");

        // 9h. sentinel ppl & sentinel elam routing
        oss.str("");
        shellRet = proc.execute("sentinel ppl status", oss);
        TEST_ASSERT(shellRet == 0, "sentinel ppl status must return 0");
        TEST_ASSERT(oss.str().find("Protected Process Light (PPL)") != std::string::npos, "sentinel ppl must route to ppl");

        oss.str("");
        shellRet = proc.execute("sentinel elam status", oss);
        TEST_ASSERT(shellRet == 0, "sentinel elam status must return 0");
        TEST_ASSERT(oss.str().find("Early Launch Anti-Malware (ELAM)") != std::string::npos, "sentinel elam must route to elam");
    }

    std::cout << "[TEST] Suite 141: Windows Protected Process Light (PPL) & ELAM Subsystem PASSED.\n";
}

void Test_WindowsSystemGuard_SecureLaunch_Subsystem() {
    using namespace micant::sysguard;

    std::cout << "[TEST] Running Suite 142: Windows System Guard Secure Launch & Measured Boot (DRTM / TPM 2.0 PCR Attestation) Subsystem...\n";

    // 1. Dynamic Loader & Version Database Parity
    {
        InitializeSysGuardSubsystemExports();

        auto& loader = ldr::DynamicLoader::get();

        // Verify tbs.dll exports
        TEST_ASSERT(loader.getExport("tbs.dll", "Tbsi_Context_Create") != nullptr, "tbs.dll must export Tbsi_Context_Create");
        TEST_ASSERT(loader.getExport("tbs.dll", "Tbsi_Context_Close") != nullptr, "tbs.dll must export Tbsi_Context_Close");
        TEST_ASSERT(loader.getExport("tbs.dll", "Tbsip_Submit_Command") != nullptr, "tbs.dll must export Tbsip_Submit_Command");
        TEST_ASSERT(loader.getExport("tbs.dll", "Tbsi_Get_TCG_Log") != nullptr, "tbs.dll must export Tbsi_Get_TCG_Log");
        TEST_ASSERT(loader.getExport("tbs.dll", "Tbsi_GetDeviceInfo") != nullptr, "tbs.dll must export Tbsi_GetDeviceInfo");
        TEST_ASSERT(loader.getExport("tbs.dll", "Tbsi_Revoke_Tickets") != nullptr, "tbs.dll must export Tbsi_Revoke_Tickets");
        TEST_ASSERT(loader.getExport("tbs.dll", "Tbsi_Get_OwnerAuth") != nullptr, "tbs.dll must export Tbsi_Get_OwnerAuth");

        // Verify ntoskrnl.exe exports
        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "SysGuardIsSecureLaunchSupported") != nullptr, "ntoskrnl.exe must export SysGuardIsSecureLaunchSupported");
        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "SysGuardIsSecureLaunchEnabled") != nullptr, "ntoskrnl.exe must export SysGuardIsSecureLaunchEnabled");
        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "SysGuardGetPcrValue") != nullptr, "ntoskrnl.exe must export SysGuardGetPcrValue");
        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "SysGuardExtendPcr") != nullptr, "ntoskrnl.exe must export SysGuardExtendPcr");
        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "SysGuardSealKey") != nullptr, "ntoskrnl.exe must export SysGuardSealKey");
        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "SysGuardUnsealKey") != nullptr, "ntoskrnl.exe must export SysGuardUnsealKey");
        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "SysGuardValidateEventLog") != nullptr, "ntoskrnl.exe must export SysGuardValidateEventLog");
        TEST_ASSERT(loader.getExport("ntoskrnl.exe", "SysGuardGetAttestationReport") != nullptr, "ntoskrnl.exe must export SysGuardGetAttestationReport");

        // VersionDatabase entries
        auto& vdb = version::VersionDatabase::Instance();
        const auto* tbsInfo = vdb.GetModuleInfo("tbs.dll");
        TEST_ASSERT(tbsInfo != nullptr, "tbs.dll must be registered in VersionDatabase");
        TEST_ASSERT(tbsInfo->stringTable.at("FileVersion") == "10.0.26100.1", "tbs.dll version must be 10.0.26100.1");

        const auto* mbInfo = vdb.GetModuleInfo("measured_boot.sys");
        TEST_ASSERT(mbInfo != nullptr, "measured_boot.sys must be registered in VersionDatabase");
        TEST_ASSERT(mbInfo->stringTable.at("FileVersion") == "10.0.26100.1", "measured_boot.sys version must be 10.0.26100.1");
    }

    // 2. Dynamic Root of Trust for Measurement (DRTM) Hardware Launch Architecture
    {
        auto& mgr = SystemGuardManager::get();

        BOOLEAN supported = FALSE;
        NTSTATUS st = SysGuardIsSecureLaunchSupported(&supported);
        TEST_ASSERT(st == STATUS_SUCCESS && supported == TRUE, "SysGuardIsSecureLaunchSupported must return TRUE");

        BOOLEAN enabled = FALSE;
        st = SysGuardIsSecureLaunchEnabled(&enabled);
        TEST_ASSERT(st == STATUS_SUCCESS && enabled == TRUE, "SysGuardIsSecureLaunchEnabled must return TRUE");

        TEST_ASSERT(mgr.isSmmIsolationActive(), "SMM Runtime Defense must be active");
        TEST_ASSERT(mgr.isDmaProtectionActive(), "Kernel DMA Protection must be active");
        TEST_ASSERT(mgr.getLaunchType() == SysGuardLaunchType::DrtmIntelTxt, "DRTM Intel TXT launch mode must be initial default");

        // PCR 17 & PCR 18 must be non-zero after DRTM launch
        auto pcr17 = mgr.readPcr(17);
        auto pcr18 = mgr.readPcr(18);
        bool pcr17NonZero = false, pcr18NonZero = false;
        for (auto b : pcr17) if (b != 0) pcr17NonZero = true;
        for (auto b : pcr18) if (b != 0) pcr18NonZero = true;
        TEST_ASSERT(pcr17NonZero, "PCR 17 (DRTM ACM Hardware measurement) must be non-zero");
        TEST_ASSERT(pcr18NonZero, "PCR 18 (System Guard Secure Kernel Runtime measurement) must be non-zero");
    }

    // 3. TPM 2.0 Platform Configuration Registers (PCR 0-23)
    {
        auto& mgr = SystemGuardManager::get();

        // Verify initial boot chain measurements
        for (uint32_t i : { 0, 1, 4, 5, 7, 8, 9, 10, 11, 12, 14, 17, 18 }) {
            auto val = mgr.readPcr(i);
            TEST_ASSERT(val.size() == 32, "PCR value must be 32 bytes");
            bool nonZero = false;
            for (auto b : val) if (b != 0) nonZero = true;
            TEST_ASSERT(nonZero, "Pre-measured boot PCRs must be non-zero");
        }

        // Test PCR extension: PCR_new = SHA256(PCR_old || Digest)
        auto pcr12Old = mgr.readPcr(12);
        std::vector<uint8_t> testDigest(32, 0x42);

        // Manually compute expected:
        std::vector<uint8_t> manualBuf;
        manualBuf.insert(manualBuf.end(), pcr12Old.begin(), pcr12Old.end());
        manualBuf.insert(manualBuf.end(), testDigest.begin(), testDigest.end());
        auto expectedHash = crypto::Sha256::hash(std::span<const uint8_t>(manualBuf.data(), manualBuf.size()));

        NTSTATUS st = SysGuardExtendPcr(12, testDigest.data(), static_cast<uint32_t>(testDigest.size()), "Suite142 Test Extend");
        TEST_ASSERT(st == STATUS_SUCCESS, "SysGuardExtendPcr must succeed");

        auto pcr12New = mgr.readPcr(12);
        TEST_ASSERT(pcr12New == expectedHash, "Extended PCR must match SHA256(PCR_old || Digest)");

        // Out of bounds PCR index test
        st = SysGuardExtendPcr(30, testDigest.data(), 32, "Invalid PCR");
        TEST_ASSERT(st == STATUS_INVALID_PARAMETER, "Invalid PCR index must return STATUS_INVALID_PARAMETER");
    }

    // 4. TCG 2.0 Measured Boot Event Log Replay & Attestation
    {
        auto& mgr = SystemGuardManager::get();

        BOOLEAN isValid = FALSE;
        NTSTATUS st = SysGuardValidateEventLog(&isValid);
        TEST_ASSERT(st == STATUS_SUCCESS && isValid == TRUE, "SysGuardValidateEventLog must succeed on authentic log");

        // Verify total events
        TEST_ASSERT(mgr.getEventCount() >= 10, "Event log must record at least 10 measured boot events");

        // Tamper test: Alter a PCR register directly without extending via event log
        mgr.resetPcr(6, 0xEE); // Force discrepancy in PCR 6

        std::string failReason;
        bool validAfterTamper = mgr.validateEventLog(&failReason);
        TEST_ASSERT(!validAfterTamper, "validateEventLog must detect tampered PCR register");
        TEST_ASSERT(failReason.find("PCR 6 mismatch") != std::string::npos, "Failure reason must flag PCR 6 discrepancy");

        // Restore valid PCR 6
        mgr.resetPcr(6, 0x00);
        TEST_ASSERT(mgr.validateEventLog(), "Event log validation must pass after restoration");
    }

    // 5. Cryptographic PCR Sealing & Unsealing
    {
        auto& mgr = SystemGuardManager::get();

        std::vector<uint8_t> secret = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x23, 0x45, 0x67 };
        uint32_t pcrs[] = { 7, 11, 14 };

        NTSTATUS st = SysGuardSealKey("BitLockerVMK_Test", secret.data(), static_cast<uint32_t>(secret.size()), pcrs, 3);
        TEST_ASSERT(st == STATUS_SUCCESS, "SysGuardSealKey must return STATUS_SUCCESS");
        TEST_ASSERT(mgr.getSealedKeyCount() >= 1, "Sealed key count must be at least 1");

        // Unseal immediately
        std::vector<uint8_t> outSecret(32, 0);
        uint32_t outLen = static_cast<uint32_t>(outSecret.size());
        st = SysGuardUnsealKey("BitLockerVMK_Test", outSecret.data(), &outLen);
        TEST_ASSERT(st == STATUS_SUCCESS, "SysGuardUnsealKey must succeed");
        TEST_ASSERT(outLen == secret.size(), "Unsealed length must match original");
        outSecret.resize(outLen);
        TEST_ASSERT(outSecret == secret, "Unsealed plaintext must match original secret");

        // Tamper with PCR 7 (Secure Boot policy altered)
        std::vector<uint8_t> tamperDigest(32, 0x99);
        mgr.extendPcr(7, tamperDigest, EV_ACTION, "Malicious OptionROM/Tampered Policy");

        // Attempt unseal: MUST FAIL with STATUS_IMAGE_INTEGRITY_FAIL
        st = SysGuardUnsealKey("BitLockerVMK_Test", outSecret.data(), &outLen);
        TEST_ASSERT(st == STATUS_IMAGE_INTEGRITY_FAIL, "Unseal must fail with STATUS_IMAGE_INTEGRITY_FAIL when PCR state is altered");
        TEST_ASSERT(mgr.getTotalTamperDetections() >= 1, "Tamper detections counter must increment on failed unseal");
    }

    // 6. Win32 TPM Base Services (TBS) C ABI (tbs.dll)
    {
        TBS_CONTEXT_PARAMS params{ TBS_CONTEXT_VERSION_ONE };
        TBS_HCONTEXT hCtx = nullptr;

        TBS_RESULT tr = Tbsi_Context_Create(&params, &hCtx);
        TEST_ASSERT(tr == TBS_SUCCESS && hCtx != nullptr, "Tbsi_Context_Create must return TBS_SUCCESS");

        // Get device info
        TBS_DEVICE_INFO devInfo{};
        tr = Tbsi_GetDeviceInfo(sizeof(devInfo), &devInfo);
        TEST_ASSERT(tr == TBS_SUCCESS, "Tbsi_GetDeviceInfo must succeed");
        TEST_ASSERT(devInfo.tpmVersion == TPM_VERSION_20, "Device info must report TPM 2.0");
        TEST_ASSERT(devInfo.tpmInterfaceType == TPM_IFTYPE_1, "Interface type must be CRB (1)");

        // Submit TPM command: TPM_CC_Startup
        uint8_t startupCmd[10] = {
            0x80, 0x01,             // tag: TPM_ST_NO_SESSIONS
            0x00, 0x00, 0x00, 0x0A, // size: 10
            0x00, 0x00, 0x01, 0x44  // code: TPM_CC_Startup
        };
        uint8_t respBuf[64]{};
        uint32_t respLen = sizeof(respBuf);

        tr = Tbsip_Submit_Command(hCtx, TBS_COMMAND_LOCALITY_ZERO, TBS_COMMAND_PRIORITY_NORMAL,
                                  startupCmd, sizeof(startupCmd), respBuf, &respLen);
        TEST_ASSERT(tr == TBS_SUCCESS, "Tbsip_Submit_Command for Startup must return TBS_SUCCESS");
        TEST_ASSERT(respLen == 10, "Startup response length must be 10");
        TEST_ASSERT(respBuf[6] == 0 && respBuf[7] == 0 && respBuf[8] == 0 && respBuf[9] == 0,
                    "Startup response code must be TPM_RC_SUCCESS (0x00000000)");

        // Submit TPM command: TPM_CC_GetRandom
        uint8_t randomCmd[10] = {
            0x80, 0x01,
            0x00, 0x00, 0x00, 0x0A,
            0x00, 0x00, 0x01, 0x7B  // TPM_CC_GetRandom
        };
        respLen = sizeof(respBuf);
        tr = Tbsip_Submit_Command(hCtx, TBS_COMMAND_LOCALITY_ZERO, TBS_COMMAND_PRIORITY_NORMAL,
                                  randomCmd, sizeof(randomCmd), respBuf, &respLen);
        TEST_ASSERT(tr == TBS_SUCCESS, "Tbsip_Submit_Command for GetRandom must return TBS_SUCCESS");
        TEST_ASSERT(respLen == 26, "GetRandom response length must be 10 + 16 = 26");

        // Get TCG Event Log via TBS API
        uint32_t logLen = 0;
        tr = Tbsi_Get_TCG_Log(hCtx, nullptr, &logLen);
        TEST_ASSERT(tr == TBS_E_BUFFER_TOO_SMALL && logLen > 0, "Tbsi_Get_TCG_Log with null buffer must return TBS_E_BUFFER_TOO_SMALL with required size");

        std::vector<uint8_t> logBuf(logLen);
        tr = Tbsi_Get_TCG_Log(hCtx, logBuf.data(), &logLen);
        TEST_ASSERT(tr == TBS_SUCCESS, "Tbsi_Get_TCG_Log with sized buffer must return TBS_SUCCESS");

        // Revoke tickets & Owner auth
        tr = Tbsi_Revoke_Tickets(hCtx);
        TEST_ASSERT(tr == TBS_SUCCESS, "Tbsi_Revoke_Tickets must return TBS_SUCCESS");

        uint32_t ownerAuthLen = 0;
        tr = Tbsi_Get_OwnerAuth(hCtx, 0, nullptr, &ownerAuthLen);
        TEST_ASSERT(tr == TBS_SUCCESS && ownerAuthLen == 0, "Tbsi_Get_OwnerAuth must succeed with 0 length");

        // Close context
        tr = Tbsi_Context_Close(hCtx);
        TEST_ASSERT(tr == TBS_SUCCESS, "Tbsi_Context_Close must return TBS_SUCCESS");

        // Reuse closed context must fail
        tr = Tbsi_Revoke_Tickets(hCtx);
        TEST_ASSERT(tr == TBS_E_INVALID_CONTEXT_PARAM, "Closed context must return TBS_E_INVALID_CONTEXT_PARAM");
    }

    // 7. Kernel Attestation Report
    {
        char reportBuf[1024]{};
        uint32_t reportLen = sizeof(reportBuf);
        NTSTATUS st = SysGuardGetAttestationReport(reportBuf, &reportLen);
        TEST_ASSERT(st == STATUS_SUCCESS, "SysGuardGetAttestationReport must succeed");
        std::string report(reportBuf);
        TEST_ASSERT(report.find("Hardware Attestation Report") != std::string::npos, "Report must have header");
        TEST_ASSERT(report.find("DRTM Launch Architecture") != std::string::npos, "Report must detail DRTM architecture");
        TEST_ASSERT(report.find("SMM Runtime Defense") != std::string::npos, "Report must detail SMM Runtime Defense");
    }

    // 8. Interactive Shell Integration (sysguard, sentinel sysguard)
    {
        shell::CommandShell proc;
        std::ostringstream oss;

        // 8a. sysguard /?
        int shellRet = proc.execute("sysguard /?", oss);
        TEST_ASSERT(shellRet == 0, "sysguard /? must return 0");
        TEST_ASSERT(oss.str().find("System Guard Secure Launch & Measured Boot") != std::string::npos, "Help must reference System Guard");

        // 8b. sysguard status
        oss.str("");
        shellRet = proc.execute("sysguard status", oss);
        TEST_ASSERT(shellRet == 0, "sysguard status must return 0");
        TEST_ASSERT(oss.str().find("DRTM Launch Architecture:") != std::string::npos, "sysguard status must show launch architecture");
        TEST_ASSERT(oss.str().find("TPM 2.0") != std::string::npos, "sysguard status must show TPM 2.0");

        // 8c. sysguard pcr
        oss.str("");
        shellRet = proc.execute("sysguard pcr", oss);
        TEST_ASSERT(shellRet == 0, "sysguard pcr must return 0");
        TEST_ASSERT(oss.str().find("[0]") != std::string::npos, "sysguard pcr must list PCR 0");
        TEST_ASSERT(oss.str().find("[17]") != std::string::npos, "sysguard pcr must list PCR 17");

        // 8d. sysguard pcr 7
        oss.str("");
        shellRet = proc.execute("sysguard pcr 7", oss);
        TEST_ASSERT(shellRet == 0, "sysguard pcr 7 must return 0");
        TEST_ASSERT(oss.str().find("PCR 07:") != std::string::npos, "sysguard pcr 7 must dump PCR 7");

        // 8e. sysguard seal & unseal
        oss.str("");
        shellRet = proc.execute("sysguard seal RecoveryPass P@ssword123 7,11", oss);
        TEST_ASSERT(shellRet == 0, "sysguard seal must return 0");
        TEST_ASSERT(oss.str().find("Sealed key 'RecoveryPass'") != std::string::npos, "sysguard seal must confirm sealing");

        oss.str("");
        shellRet = proc.execute("sysguard unseal RecoveryPass", oss);
        TEST_ASSERT(shellRet == 0, "sysguard unseal must return 0");
        TEST_ASSERT(oss.str().find("P@ssword123") != std::string::npos, "sysguard unseal must return plaintext password");

        // 8f. sysguard test
        oss.str("");
        shellRet = proc.execute("sysguard test", oss);
        TEST_ASSERT(shellRet == 0, "sysguard test must return 0");
        TEST_ASSERT(oss.str().find("[SUCCESS]") != std::string::npos, "sysguard test must report [SUCCESS]");

        // 8g. sentinel sysguard status routing
        oss.str("");
        shellRet = proc.execute("sentinel sysguard status", oss);
        TEST_ASSERT(shellRet == 0, "sentinel sysguard status must return 0");
        TEST_ASSERT(oss.str().find("System Guard Secure Launch & Measured Boot Posture:") != std::string::npos, "sentinel sysguard must route to sysguard");
    }

    std::cout << "[TEST] Suite 142: Windows System Guard Secure Launch & Measured Boot (DRTM / TPM 2.0 PCR Attestation) Subsystem PASSED.\n";
}

// ============================================================================
// Suite 143: Windows Virtualization-Based Security (VBS) & Hypervisor-Enforced
// Code Integrity (HVCI / Memory Integrity) Subsystem
// ============================================================================
void Test_WindowsVBS_HVCI_MemoryIntegrity_Subsystem() {
    using namespace micant::vbs_hvci;

    std::cout << "[TEST] Running Suite 143: Windows Virtualization-Based Security (VBS) & Hypervisor-Enforced Code Integrity (HVCI)...\n";

    // 1. DynamicLoader Export Surface Parity
    {
        InitializeVbsHvciSubsystemExports();
        auto& loader = ldr::DynamicLoader::get();

        // 1a. vbs.dll exports
        std::vector<std::string> vbsExports = {
            "VbsIsVirtualizationBasedSecuritySupported",
            "VbsIsVirtualizationBasedSecurityEnabled",
            "VbsGetHypervisorEnforcedCodeIntegrityStatus",
            "VbsSetHypervisorEnforcedCodeIntegrity",
            "VbsQueryVirtualTrustLevel",
            "VbsInvokeHypercall",
            "VbsGetMemoryProtectionPolicy",
            "VbsAuditSecurityViolation"
        };
        for (const auto& sym : vbsExports) {
            TEST_ASSERT(loader.getExport("vbs.dll", sym) != nullptr, ("vbs.dll export '" + sym + "' must be registered").c_str());
        }

        // 1b. ntoskrnl.exe exports
        std::vector<std::string> ntosExports = {
            "HvlIsHypervisorPresent",
            "HvlGetVirtualTrustLevel",
            "HvlEnforceKernelCodeIntegrity",
            "HvlProtectPageFrame",
            "HvlValidateMemoryAttributes",
            "HvlRegisterHvciCallback",
            "HvlGetHvciViolationCount"
        };
        for (const auto& sym : ntosExports) {
            TEST_ASSERT(loader.getExport("ntoskrnl.exe", sym) != nullptr, ("ntoskrnl.exe export '" + sym + "' must be registered").c_str());
        }
    }

    // 2. VersionDatabase Module Registrations
    {
        auto& vdb = version::VersionDatabase::Instance();
        const auto* vbsInfo = vdb.GetModuleInfo("vbs.dll");
        TEST_ASSERT(vbsInfo != nullptr, "vbs.dll must be registered in VersionDatabase");
        TEST_ASSERT(vbsInfo->stringTable.at("FileVersion") == "10.0.26100.1", "vbs.dll version must match 10.0.26100.1");

        const auto* skInfo = vdb.GetModuleInfo("securekernel.exe");
        TEST_ASSERT(skInfo != nullptr, "securekernel.exe must be registered in VersionDatabase");
        TEST_ASSERT(skInfo->stringTable.at("FileVersion") == "10.0.26100.1", "securekernel.exe version must match 10.0.26100.1");
    }

    // 3. Hypervisor Presence, VBS & HVCI Posture Verification
    {
        auto& mgr = VirtualizationBasedSecurityManager::Instance();
        TEST_ASSERT(mgr.isSupported(), "Hypervisor SLAT (EPT/NPT) must be supported");
        TEST_ASSERT(mgr.isEnabled(), "VBS must be enabled by default");
        TEST_ASSERT(mgr.isHvciActive(), "HVCI (Memory Integrity) must be active by default");
        TEST_ASSERT(mgr.getCurrentVtl() == VTL_0_NORMAL, "Initial execution must be in VTL 0 (Normal World)");

        auto features = mgr.getFeatures();
        TEST_ASSERT((features & HV_FEATURE_SLAT_EPT) != 0, "SLAT EPT must be supported");
        TEST_ASSERT((features & HV_FEATURE_MBEC) != 0, "Mode-Based Execute Control must be supported");
        TEST_ASSERT((features & HV_FEATURE_VTL) != 0, "Virtual Trust Levels must be supported");
        TEST_ASSERT((features & HV_FEATURE_HYPERVISOR_PRESENT) != 0, "Hypervisor present bit must be set");

        auto policy = mgr.getPolicyInfo();
        TEST_ASSERT(policy.ProtectedPageCount >= 7, "At least 7 pre-seeded SLAT page regions must be registered");
    }

    // 4. Second-Level Address Translation (SLAT) Page Table & W^X Enforcement
    {
        auto& mgr = VirtualizationBasedSecurityManager::Instance();

        // 4a. Verify ntoskrnl .text is mapped R-X
        BOOLEAN blocked = FALSE;
        NTSTATUS st = VbsAuditSecurityViolation(0xFFFFF80000000000ULL, SLAT_PERM_READ, VTL_0_NORMAL, &blocked);
        TEST_ASSERT(st == STATUS_SUCCESS && !blocked, "Read of ntoskrnl .text must be allowed");

        st = VbsAuditSecurityViolation(0xFFFFF80000000000ULL, SLAT_PERM_EXECUTE, VTL_0_NORMAL, &blocked);
        TEST_ASSERT(st == STATUS_SUCCESS && !blocked, "Execution of ntoskrnl .text must be allowed");

        // 4b. Strict W^X: Direct write to ntoskrnl .text MUST be trapped by hypervisor EPT
        st = VbsAuditSecurityViolation(0xFFFFF80000000000ULL, SLAT_PERM_WRITE, VTL_0_NORMAL, &blocked);
        TEST_ASSERT(st == STATUS_HVCI_WX_VIOLATION && blocked, "Write to executable kernel code must trigger STATUS_HVCI_WX_VIOLATION");

        // 4c. Verify NonPagedPool is mapped RW- (No Execute)
        st = VbsAuditSecurityViolation(0xFFFFFA8000000000ULL, SLAT_PERM_WRITE, VTL_0_NORMAL, &blocked);
        TEST_ASSERT(st == STATUS_SUCCESS && !blocked, "Write to NonPagedPool must be allowed");

        st = VbsAuditSecurityViolation(0xFFFFFA8000000000ULL, SLAT_PERM_EXECUTE, VTL_0_NORMAL, &blocked);
        TEST_ASSERT(st == STATUS_HVCI_CODE_INTEGRITY_VIOLATION && blocked, "Execution of NonPagedPool must trigger STATUS_HVCI_CODE_INTEGRITY_VIOLATION");

        // 4d. Rejection of simultaneous Write and Execute allocation (W+X forbidden)
        st = mgr.protectKernelPage(0xFFFFF80001000000ULL, 4096, SLAT_PERM_WRITE | SLAT_PERM_EXECUTE, VTL_0_NORMAL);
        TEST_ASSERT(st == STATUS_HVCI_WX_VIOLATION, "protectKernelPage with W+X must return STATUS_HVCI_WX_VIOLATION");

        // 4e. VTL 0 cannot directly grant execute permission on kernel pages
        st = mgr.protectKernelPage(0xFFFFF80001000000ULL, 4096, SLAT_PERM_EXECUTE, VTL_0_NORMAL);
        TEST_ASSERT(st == STATUS_HVCI_POLICY_VIOLATION, "VTL 0 cannot mark memory executable without VTL 1 hypercall");
    }

    // 5. Cross-VTL Enclave Isolation (Secure Kernel & LsaIso)
    {
        auto& mgr = VirtualizationBasedSecurityManager::Instance();

        // 5a. Normal world (VTL 0) reading Secure Kernel memory -> STATUS_VTL_ACCESS_DENIED
        BOOLEAN blocked = FALSE;
        NTSTATUS st = VbsAuditSecurityViolation(0xFFFFF87F00000000ULL, SLAT_PERM_READ, VTL_0_NORMAL, &blocked);
        TEST_ASSERT(st == STATUS_VTL_ACCESS_DENIED && blocked, "VTL 0 Read of Secure Kernel must trigger STATUS_VTL_ACCESS_DENIED");

        // 5b. Normal world (VTL 0) writing to LsaIso IUM Enclave -> STATUS_VTL_ACCESS_DENIED
        st = VbsAuditSecurityViolation(0xFFFFF87F00400000ULL, SLAT_PERM_WRITE, VTL_0_NORMAL, &blocked);
        TEST_ASSERT(st == STATUS_VTL_ACCESS_DENIED && blocked, "VTL 0 Write to LsaIso Enclave must trigger STATUS_VTL_ACCESS_DENIED");

        // 5c. VTL 0 attempting to modify page table permissions of VTL 1 enclave
        st = mgr.protectKernelPage(0xFFFFF87F00400000ULL, 4096, SLAT_PERM_RW, VTL_0_NORMAL);
        TEST_ASSERT(st == STATUS_VTL_ACCESS_DENIED, "VTL 0 cannot modify VTL 1 enclave page permissions");

        // 5d. Secure world (VTL 1) can access its own memory
        st = mgr.verifyAccess(0xFFFFF87F00000000ULL, SLAT_PERM_READ, VTL_1_SECURE);
        TEST_ASSERT(st == STATUS_SUCCESS, "VTL 1 must be permitted to read its own memory");
    }

    // 6. Hypercall Dispatch & Cross-VTL Execution Engine
    {
        uint64_t result = 0;

        // 6a. Query VTL status hypercall
        NTSTATUS st = VbsInvokeHypercall(HV_CALL_GET_VTL_STATUS, 0, 0, &result);
        TEST_ASSERT(st == STATUS_SUCCESS, "HV_CALL_GET_VTL_STATUS must succeed");

        // 6b. Transition to VTL 1 hypercall
        st = VbsInvokeHypercall(HV_CALL_ENTER_VTL1, 0, 0, &result);
        TEST_ASSERT(st == STATUS_SUCCESS && result == VTL_1_SECURE, "HV_CALL_ENTER_VTL1 must return VTL_1_SECURE");
        TEST_ASSERT(VbsQueryVirtualTrustLevel() == VTL_1_SECURE, "QueryVirtualTrustLevel must reflect VTL 1");

        // 6c. In VTL 1, verify driver signature and grant R-X permission in SLAT
        st = VbsInvokeHypercall(HV_CALL_VERIFY_DRIVER_SIG, 0xFFFFF80002000000ULL, 65536, &result);
        TEST_ASSERT(st == STATUS_SUCCESS, "HV_CALL_VERIFY_DRIVER_SIG must succeed");

        // Check page was granted R-X
        st = VbsInvokeHypercall(HV_CALL_QUERY_PAGE_ATTR, 0xFFFFF80002000000ULL, 0, &result);
        TEST_ASSERT(st == STATUS_SUCCESS, "HV_CALL_QUERY_PAGE_ATTR must locate verified driver page");
        uint32_t vtl0Perms = static_cast<uint32_t>(result & 0xFFFFFFFF);
        TEST_ASSERT((vtl0Perms & SLAT_PERM_EXECUTE) != 0, "Verified driver page must have execute permission");

        // Return to VTL 0 Normal World
        st = VbsInvokeHypercall(HV_CALL_RETURN_VTL0, 0, 0, &result);
        TEST_ASSERT(st == STATUS_SUCCESS && result == VTL_0_NORMAL, "HV_CALL_RETURN_VTL0 must return VTL_0_NORMAL");
        TEST_ASSERT(VbsQueryVirtualTrustLevel() == VTL_0_NORMAL, "QueryVirtualTrustLevel must reflect VTL 0");

        // 6d. Invalid hypercall handling
        st = VbsInvokeHypercall(0x9999, 0, 0, &result);
        TEST_ASSERT(st == STATUS_HVCI_INVALID_HYPERCALL, "Unrecognized hypercall must return STATUS_HVCI_INVALID_HYPERCALL");
    }

    // 7. UEFI Lock & Immutability Enforcement
    {
        // When enabled with UEFI lock, disable attempts must be rejected with STATUS_ACCESS_DENIED
        NTSTATUS st = VbsSetHypervisorEnforcedCodeIntegrity(FALSE, FALSE);
        TEST_ASSERT(st == STATUS_ACCESS_DENIED, "Disabling VBS with active UEFI lock must return STATUS_ACCESS_DENIED");
        TEST_ASSERT(VbsGetHypervisorEnforcedCodeIntegrityStatus() == TRUE, "HVCI must remain active after unauthorized disable attempt");
    }

    // 8. Simulated Rootkit Attacks & Interception Reporting
    {
        auto& mgr = VirtualizationBasedSecurityManager::Instance();

        // 8a. Kernel Code Patching simulation
        std::string report = mgr.simulateRootkitAttack("KernelCodePatching");
        TEST_ASSERT(report.find("[BLOCKED]") != std::string::npos, "Simulation must report BLOCKED for code patching");
        TEST_ASSERT(report.find("0xC0000431") != std::string::npos, "Simulation report must contain STATUS_HVCI_WX_VIOLATION");

        // 8b. NonPagedPool Execution simulation
        report = mgr.simulateRootkitAttack("NonPagedPoolExecution");
        TEST_ASSERT(report.find("[BLOCKED]") != std::string::npos, "Simulation must report BLOCKED for pool execution");
        TEST_ASSERT(report.find("0xC0000428") != std::string::npos, "Simulation report must contain STATUS_HVCI_CODE_INTEGRITY_VIOLATION");

        // 8c. VTL 1 Memory Scraping simulation
        report = mgr.simulateRootkitAttack("Vtl1MemoryScrape");
        TEST_ASSERT(report.find("[BLOCKED]") != std::string::npos, "Simulation must report BLOCKED for VTL 1 scrape");
        TEST_ASSERT(report.find("0xC0000432") != std::string::npos, "Simulation report must contain STATUS_VTL_ACCESS_DENIED");

        // Verify violation metrics
        TEST_ASSERT(mgr.getViolationCount() >= 5, "Total violations prevented must be tracked");
        TEST_ASSERT(!mgr.getViolations().empty(), "Violation audit log must contain records");
    }

    // 9. Interactive Shell Integration (vbs, sentinel hvci)
    {
        shell::CommandShell proc;
        std::ostringstream oss;

        // 9a. vbs /?
        int shellRet = proc.execute("vbs /?", oss);
        TEST_ASSERT(shellRet == 0, "vbs /? must return 0");
        TEST_ASSERT(oss.str().find("Virtualization-Based Security (VBS) & Hypervisor-Enforced Code Integrity (HVCI)") != std::string::npos, "Help must reference VBS & HVCI");

        // 9b. vbs status
        oss.str("");
        shellRet = proc.execute("vbs status", oss);
        TEST_ASSERT(shellRet == 0, "vbs status must return 0");
        TEST_ASSERT(oss.str().find("Virtualization-Based Security (VBS) & HVCI Posture:") != std::string::npos, "vbs status must display posture");
        TEST_ASSERT(oss.str().find("ENFORCED (Strict W^X Active)") != std::string::npos, "vbs status must confirm HVCI enforced");

        // 9c. vbs pages
        oss.str("");
        shellRet = proc.execute("vbs pages", oss);
        TEST_ASSERT(shellRet == 0, "vbs pages must return 0");
        TEST_ASSERT(oss.str().find("ntoskrnl.exe (.text)") != std::string::npos, "vbs pages must list ntoskrnl .text");
        TEST_ASSERT(oss.str().find("NonPagedPool") != std::string::npos, "vbs pages must list NonPagedPool");
        TEST_ASSERT(oss.str().find("securekernel.exe") != std::string::npos, "vbs pages must list securekernel.exe");

        // 9d. vbs verify 0xFFFFF80000000000
        oss.str("");
        shellRet = proc.execute("vbs verify 0xFFFFF80000000000", oss);
        TEST_ASSERT(shellRet == 0, "vbs verify must return 0");
        TEST_ASSERT(oss.str().find("READ-EXECUTE (R-X)") != std::string::npos, "vbs verify must identify R-X section");

        // 9e. vbs simulate-attack
        oss.str("");
        shellRet = proc.execute("vbs simulate-attack", oss);
        TEST_ASSERT(shellRet == 0, "vbs simulate-attack must return 0");
        TEST_ASSERT(oss.str().find("[BLOCKED]") != std::string::npos, "vbs simulate-attack must report BLOCKED");

        // 9f. vbs test
        oss.str("");
        shellRet = proc.execute("vbs test", oss);
        TEST_ASSERT(shellRet == 0, "vbs test must return 0");
        TEST_ASSERT(oss.str().find("ALL TESTS PASSED (100%)") != std::string::npos, "vbs test must pass all tests");

        // 9g. sentinel hvci status routing
        oss.str("");
        shellRet = proc.execute("sentinel hvci status", oss);
        TEST_ASSERT(shellRet == 0, "sentinel hvci status must return 0");
        TEST_ASSERT(oss.str().find("Virtualization-Based Security (VBS) & HVCI Posture:") != std::string::npos, "sentinel hvci must route to vbs status");
    }

    std::cout << "[TEST] Suite 143: Windows Virtualization-Based Security (VBS) & Hypervisor-Enforced Code Integrity (HVCI) Subsystem PASSED.\n";
}

// ============================================================================
// Suite 144: Windows Kernel DMA Protection & IOMMU Remapping Subsystem
// ============================================================================
void Test_WindowsKernelDMA_Protection_IOMMU_Subsystem() {
    std::cout << "\n[TEST] Running Suite 144: Windows Kernel DMA Protection & IOMMU Remapping (VT-d / AMD-Vi / DMA Guard)...\n";

    using namespace micant::dma_guard;

    // 1. Clean-Room Export Registration & DynamicLoader Verification
    InitializeDmaGuardSubsystemExports();
    auto& loader = ldr::DynamicLoader::get();

    // hal.dll exports
    TEST_ASSERT(loader.getExport("hal.dll", "HalAllocateDomain") != nullptr, "HalAllocateDomain must be exported by hal.dll");
    TEST_ASSERT(loader.getExport("hal.dll", "HalFreeDomain") != nullptr, "HalFreeDomain must be exported by hal.dll");
    TEST_ASSERT(loader.getExport("hal.dll", "HalAttachDeviceDomain") != nullptr, "HalAttachDeviceDomain must be exported by hal.dll");
    TEST_ASSERT(loader.getExport("hal.dll", "HalDetachDeviceDomain") != nullptr, "HalDetachDeviceDomain must be exported by hal.dll");
    TEST_ASSERT(loader.getExport("hal.dll", "HalMapIommuRange") != nullptr, "HalMapIommuRange must be exported by hal.dll");
    TEST_ASSERT(loader.getExport("hal.dll", "HalUnmapIommuRange") != nullptr, "HalUnmapIommuRange must be exported by hal.dll");
    TEST_ASSERT(loader.getExport("hal.dll", "HalFlushIommuTlb") != nullptr, "HalFlushIommuTlb must be exported by hal.dll");

    // pci.sys exports
    TEST_ASSERT(loader.getExport("pci.sys", "DmaGuardIsProtectionSupported") != nullptr, "DmaGuardIsProtectionSupported must be exported by pci.sys");
    TEST_ASSERT(loader.getExport("pci.sys", "DmaGuardIsProtectionEnabled") != nullptr, "DmaGuardIsProtectionEnabled must be exported by pci.sys");
    TEST_ASSERT(loader.getExport("pci.sys", "DmaGuardGetDevicePolicy") != nullptr, "DmaGuardGetDevicePolicy must be exported by pci.sys");
    TEST_ASSERT(loader.getExport("pci.sys", "DmaGuardSetDevicePolicy") != nullptr, "DmaGuardSetDevicePolicy must be exported by pci.sys");
    TEST_ASSERT(loader.getExport("pci.sys", "DmaGuardAuthorizeDevice") != nullptr, "DmaGuardAuthorizeDevice must be exported by pci.sys");
    TEST_ASSERT(loader.getExport("pci.sys", "DmaGuardRevokeDevice") != nullptr, "DmaGuardRevokeDevice must be exported by pci.sys");
    TEST_ASSERT(loader.getExport("pci.sys", "DmaGuardInterceptDmaTransfer") != nullptr, "DmaGuardInterceptDmaTransfer must be exported by pci.sys");
    TEST_ASSERT(loader.getExport("pci.sys", "DmaGuardGetViolationCount") != nullptr, "DmaGuardGetViolationCount must be exported by pci.sys");

    // ntoskrnl.exe exports
    TEST_ASSERT(loader.getExport("ntoskrnl.exe", "DmaGuardIsProtectionSupported") != nullptr, "DmaGuardIsProtectionSupported must be exported by ntoskrnl.exe");
    TEST_ASSERT(loader.getExport("ntoskrnl.exe", "DmaGuardIsProtectionEnabled") != nullptr, "DmaGuardIsProtectionEnabled must be exported by ntoskrnl.exe");
    TEST_ASSERT(loader.getExport("ntoskrnl.exe", "DmaGuardInterceptDmaTransfer") != nullptr, "DmaGuardInterceptDmaTransfer must be exported by ntoskrnl.exe");
    TEST_ASSERT(loader.getExport("ntoskrnl.exe", "DmaGuardGetViolationCount") != nullptr, "DmaGuardGetViolationCount must be exported by ntoskrnl.exe");

    // 2. VersionDatabase Verification
    {
        auto& vdb = version::VersionDatabase::Instance();
        const auto* dmaSys = vdb.GetModuleInfo("dma_guard.sys");
        TEST_ASSERT(dmaSys != nullptr, "dma_guard.sys must be registered in VersionDatabase");
        TEST_ASSERT(dmaSys->stringTable.at("FileVersion") == "10.0.26100.1", "dma_guard.sys version must match 10.0.26100.1");

        const auto* pciSys = vdb.GetModuleInfo("pci.sys");
        TEST_ASSERT(pciSys != nullptr, "pci.sys must be registered in VersionDatabase");
        TEST_ASSERT(pciSys->stringTable.at("FileVersion") == "10.0.26100.1", "pci.sys version must match 10.0.26100.1");
    }

    // 3. Platform Capabilities & ACPI DMAR Pre-Boot Opt-In
    auto& mgr = KernelDmaProtectionManager::Instance();
    TEST_ASSERT(DmaGuardIsProtectionSupported() == TRUE, "DmaGuardIsProtectionSupported must return TRUE");
    TEST_ASSERT(DmaGuardIsProtectionEnabled() == TRUE, "DmaGuardIsProtectionEnabled must return TRUE");
    TEST_ASSERT(mgr.isSupported(), "Manager isSupported must return true");
    TEST_ASSERT(mgr.isEnabled(), "Manager isEnabled must return true");
    TEST_ASSERT((mgr.getAcpiDmarFlags() & ACPI_DMAR_FLAG_DMA_CTRL_PLATFORM_OPT_IN) != 0, "ACPI DMAR table must have Bit 2 DMA_CTRL_PLATFORM_OPT_IN set");
    TEST_ASSERT(mgr.getArchitecture() == IommuArchitecture::IntelVtd, "Architecture must default to Intel VT-d");
    TEST_ASSERT(DmaGuardGetDevicePolicy() == static_cast<uint32_t>(DmaGuardPolicy::BlockUntrusted), "Default policy must be BlockUntrusted");

    // 4. Internal Peripheral Trusted Bus Mastering (NVMe SSD, GPU, Ethernet)
    {
        auto devices = mgr.getDevices();
        TEST_ASSERT(devices.size() >= 6, "Device list must track at least 6 standard devices");

        // Verify Samsung NVMe SSD is internal and authorized
        uint64_t physAddr = 0;
        NTSTATUS st = DmaGuardInterceptDmaTransfer(
            "PCI\\VEN_144D&DEV_A80A&SUBSYS_A801144D&REV_00",
            0x10000000ULL, 4096, TRUE, &physAddr
        );
        TEST_ASSERT(st == STATUS_SUCCESS, "Internal NVMe DMA transfer must succeed");
        TEST_ASSERT(physAddr == 0x40000000ULL, "Physical address translation must match mapped base (0x40000000)");

        // Verify NVIDIA RTX 4090 internal GPU DMA
        st = DmaGuardInterceptDmaTransfer(
            "PCI\\VEN_10DE&DEV_2684&SUBSYS_168410DE&REV_A1",
            0x20000000ULL, 8192, TRUE, &physAddr
        );
        TEST_ASSERT(st == STATUS_SUCCESS, "Internal GPU DMA transfer must succeed");
        TEST_ASSERT(physAddr == 0x50000000ULL, "GPU physical address translation must match mapped base (0x50000000)");
    }

    // 5. Unauthorized External Hot-Plug DMA Defense (Thunderbolt 3/4 & USB4)
    {
        const char* tb3 = "PCI\\VEN_8086&DEV_15D2&SUBSYS_00000000&REV_02";
        const char* tb4 = "PCI\\VEN_8086&DEV_9A1B&SUBSYS_00000000&REV_01";
        const char* usb4 = "PCI\\VEN_1022&DEV_1639&SUBSYS_00000000&REV_00";

        uint64_t physTarget = 0;

        // TB3 unauthorized DMA transfer
        NTSTATUS st = DmaGuardInterceptDmaTransfer(tb3, 0x100000ULL, 4096, FALSE, &physTarget);
        TEST_ASSERT(st == STATUS_DEVICE_NOT_AUTHORIZED, "Unauthorized TB3 DMA must return STATUS_DEVICE_NOT_AUTHORIZED");

        // TB4 unauthorized DMA transfer
        st = DmaGuardInterceptDmaTransfer(tb4, 0x200000ULL, 4096, TRUE, &physTarget);
        TEST_ASSERT(st == STATUS_DEVICE_NOT_AUTHORIZED, "Unauthorized TB4 DMA must return STATUS_DEVICE_NOT_AUTHORIZED");

        // USB4 unauthorized DMA transfer
        st = DmaGuardInterceptDmaTransfer(usb4, 0x300000ULL, 4096, FALSE, &physTarget);
        TEST_ASSERT(st == STATUS_DEVICE_NOT_AUTHORIZED, "Unauthorized USB4 DMA must return STATUS_DEVICE_NOT_AUTHORIZED");
    }

    // 6. Device Authorization Whitelist Lifecycle
    {
        const char* tb3 = "PCI\\VEN_8086&DEV_15D2&SUBSYS_00000000&REV_02";

        // Authorize device
        NTSTATUS st = DmaGuardAuthorizeDevice(tb3);
        TEST_ASSERT(st == STATUS_SUCCESS, "DmaGuardAuthorizeDevice must succeed");

        // Attempt transfer on assigned domain buffer
        uint32_t assignedDom = 0;
        for (const auto& d : mgr.getDevices()) {
            if (d.deviceId == tb3) { assignedDom = d.domainId; break; }
        }
        uint64_t iovaBase = 0x80000000ULL + (assignedDom * 0x10000000ULL);
        uint64_t physTarget = 0;
        st = DmaGuardInterceptDmaTransfer(tb3, iovaBase, 4096, TRUE, &physTarget);
        TEST_ASSERT(st == STATUS_SUCCESS, "Authorized peripheral DMA must succeed within assigned domain");
        TEST_ASSERT(physTarget == (0x70000000ULL + (assignedDom * 0x10000000ULL)), "Translated physical address must match domain mapping");

        // Revoke authorization
        st = DmaGuardRevokeDevice(tb3);
        TEST_ASSERT(st == STATUS_SUCCESS, "DmaGuardRevokeDevice must succeed");

        // Attempt transfer again: must be blocked
        st = DmaGuardInterceptDmaTransfer(tb3, iovaBase, 4096, TRUE, &physTarget);
        TEST_ASSERT(st == STATUS_DEVICE_NOT_AUTHORIZED, "Revoked peripheral must be blocked with STATUS_DEVICE_NOT_AUTHORIZED");
    }

    // 7. Hardware IOMMU Domain & Page Table Lifecycle (HAL ABI)
    {
        uint32_t domId = 0;
        NTSTATUS st = HalAllocateDomain(0, &domId);
        TEST_ASSERT(st == STATUS_SUCCESS && domId != 0, "HalAllocateDomain must allocate unique domain ID");

        // Map IOVA range (4MB) with Read/Write
        uint64_t testIova = 0x77000000ULL;
        uint64_t testPhys = 0x88000000ULL;
        st = HalMapIommuRange(domId, testIova, testPhys, 0x00400000, IOMMU_PERM_RW);
        TEST_ASSERT(st == STATUS_SUCCESS, "HalMapIommuRange must succeed");

        // Attach NVMe device to this domain
        const char* nvme = "PCI\\VEN_144D&DEV_A80A&SUBSYS_A801144D&REV_00";
        st = HalAttachDeviceDomain(domId, nvme);
        TEST_ASSERT(st == STATUS_SUCCESS, "HalAttachDeviceDomain must attach device");

        // Transfer within mapped range
        uint64_t physTarget = 0;
        st = DmaGuardInterceptDmaTransfer(nvme, testIova + 0x1000, 4096, TRUE, &physTarget);
        TEST_ASSERT(st == STATUS_SUCCESS, "DMA transfer in custom domain must succeed");
        TEST_ASSERT(physTarget == (testPhys + 0x1000), "Physical address must reflect offset into mapped range");

        // Unmapped IOVA fault check
        st = DmaGuardInterceptDmaTransfer(nvme, 0xF0000000ULL, 4096, TRUE, &physTarget);
        TEST_ASSERT(st == STATUS_IOMMU_PAGE_FAULT, "Accessing unmapped IOVA must trigger STATUS_IOMMU_PAGE_FAULT");

        // Read-only permission check
        uint64_t roIova = 0x78000000ULL;
        st = HalMapIommuRange(domId, roIova, 0x89000000ULL, 4096, IOMMU_PERM_READ);
        TEST_ASSERT(st == STATUS_SUCCESS, "Mapping read-only page must succeed");

        st = DmaGuardInterceptDmaTransfer(nvme, roIova, 512, TRUE, &physTarget);
        TEST_ASSERT(st == STATUS_IOMMU_ACCESS_VIOLATION, "Writing to read-only page must trigger STATUS_IOMMU_ACCESS_VIOLATION");

        st = DmaGuardInterceptDmaTransfer(nvme, roIova, 512, FALSE, &physTarget);
        TEST_ASSERT(st == STATUS_SUCCESS, "Reading from read-only page must succeed");

        // Strict W^X Invariant for DMA
        st = HalMapIommuRange(domId, 0x79000000ULL, 0x8A000000ULL, 4096, IOMMU_PERM_RW | IOMMU_PERM_EXEC);
        TEST_ASSERT(st == STATUS_IOMMU_WX_VIOLATION, "Executable DMA mapping must be rejected with STATUS_IOMMU_WX_VIOLATION");

        // Flush TLB
        st = HalFlushIommuTlb(domId);
        TEST_ASSERT(st == STATUS_SUCCESS, "HalFlushIommuTlb must succeed");

        // Unmap range
        st = HalUnmapIommuRange(domId, testIova, 0x00400000);
        TEST_ASSERT(st == STATUS_SUCCESS, "HalUnmapIommuRange must succeed");

        // Detach device and reattach to Domain 1 (restore NVMe)
        HalDetachDeviceDomain(domId, nvme);
        HalAttachDeviceDomain(1, nvme);

        // Free domain
        st = HalFreeDomain(domId);
        TEST_ASSERT(st == STATUS_SUCCESS, "HalFreeDomain must succeed");
    }

    // 8. Policy Configuration & UEFI Lock Immutability
    {
        // Change policy to AllowAll then back to BlockUntrusted
        NTSTATUS st = DmaGuardSetDevicePolicy(static_cast<uint32_t>(DmaGuardPolicy::AllowAll));
        TEST_ASSERT(st == STATUS_SUCCESS, "Setting policy to AllowAll must succeed");
        TEST_ASSERT(DmaGuardGetDevicePolicy() == static_cast<uint32_t>(DmaGuardPolicy::AllowAll), "Policy must reflect AllowAll");

        st = DmaGuardSetDevicePolicy(static_cast<uint32_t>(DmaGuardPolicy::BlockUntrusted));
        TEST_ASSERT(st == STATUS_SUCCESS, "Setting policy back to BlockUntrusted must succeed");

        // Enable UEFI lock
        st = mgr.setState(DmaGuardState::EnabledUefiLocked);
        TEST_ASSERT(st == STATUS_SUCCESS, "Setting EnabledUefiLocked must succeed");
        TEST_ASSERT(mgr.isUefiLocked(), "isUefiLocked must return true");

        // Attempting to disable policy under active UEFI lock must be rejected
        st = DmaGuardSetDevicePolicy(static_cast<uint32_t>(DmaGuardPolicy::Disabled));
        TEST_ASSERT(st == STATUS_DMA_GUARD_LOCKED, "Disabling policy under UEFI lock must return STATUS_DMA_GUARD_LOCKED");

        // Attempting to disable state under active UEFI lock must be rejected
        st = mgr.setState(DmaGuardState::Disabled);
        TEST_ASSERT(st == STATUS_DMA_GUARD_LOCKED, "Disabling state under UEFI lock must return STATUS_DMA_GUARD_LOCKED");

        // Unlock state for subsequent tests
        mgr.setState(DmaGuardState::Enabled);
    }

    // 9. Hardware DMA Attack Simulations
    {
        // 9a. PCILeech direct RAM scraping attack
        std::string report = mgr.simulateDmaAttack("PciLeechDirectRam");
        TEST_ASSERT(report.find("[BLOCKED]") != std::string::npos, "PCILeech simulation must report BLOCKED");
        TEST_ASSERT(report.find("0xC0000405") != std::string::npos, "PCILeech simulation must reference STATUS_DEVICE_NOT_AUTHORIZED");

        // 9b. Unmapped IOVA spray attack
        report = mgr.simulateDmaAttack("UnmappedIovaSpray");
        TEST_ASSERT(report.find("[BLOCKED]") != std::string::npos, "Unmapped spray simulation must report BLOCKED");
        TEST_ASSERT(report.find("0xC0000407") != std::string::npos, "Unmapped spray simulation must reference STATUS_IOMMU_PAGE_FAULT");

        // 9c. Read-only corruption attack
        report = mgr.simulateDmaAttack("ReadOnlyMemoryCorruption");
        TEST_ASSERT(report.find("[BLOCKED]") != std::string::npos, "Read-only simulation must report BLOCKED");
        TEST_ASSERT(report.find("0xC0000408") != std::string::npos, "Read-only simulation must reference STATUS_IOMMU_ACCESS_VIOLATION");

        // Verify violation counters
        TEST_ASSERT(DmaGuardGetViolationCount() >= 5, "DmaGuardGetViolationCount must report intercepted violations");
        TEST_ASSERT(!mgr.getViolations().empty(), "Violation audit log must contain records");
    }

    // 10. Interactive Shell CLI Integration (dmaguard, sentinel dma)
    {
        shell::CommandShell proc;
        std::ostringstream oss;

        // 10a. dmaguard /?
        int shellRet = proc.execute("dmaguard /?", oss);
        TEST_ASSERT(shellRet == 0, "dmaguard /? must return 0");
        TEST_ASSERT(oss.str().find("Kernel DMA Protection & IOMMU Remapping Subsystem") != std::string::npos, "Help must reference Kernel DMA Protection");

        // 10b. dmaguard status
        oss.str("");
        shellRet = proc.execute("dmaguard status", oss);
        TEST_ASSERT(shellRet == 0, "dmaguard status must return 0");
        TEST_ASSERT(oss.str().find("Kernel DMA Protection & Hardware IOMMU Posture:") != std::string::npos, "status must display posture header");
        TEST_ASSERT(oss.str().find("Intel VT-d") != std::string::npos, "status must display Intel VT-d");
        TEST_ASSERT(oss.str().find("DMA_CTRL_PLATFORM_OPT_IN") != std::string::npos, "status must show ACPI opt-in");

        // 10c. dmaguard devices
        oss.str("");
        shellRet = proc.execute("dmaguard devices", oss);
        TEST_ASSERT(shellRet == 0, "dmaguard devices must return 0");
        TEST_ASSERT(oss.str().find("PCIe & Hot-Plug Peripheral DMA Protection Table:") != std::string::npos, "devices must display table");
        TEST_ASSERT(oss.str().find("Thunderbolt 3") != std::string::npos, "devices must list Thunderbolt 3");
        TEST_ASSERT(oss.str().find("Internal PCIe") != std::string::npos, "devices must list Internal PCIe");

        // 10d. dmaguard domains
        oss.str("");
        shellRet = proc.execute("dmaguard domains", oss);
        TEST_ASSERT(shellRet == 0, "dmaguard domains must return 0");
        TEST_ASSERT(oss.str().find("Hardware IOMMU Translation Domains") != std::string::npos, "domains must display domain list");

        // 10e. dmaguard policy block
        oss.str("");
        shellRet = proc.execute("dmaguard policy block", oss);
        TEST_ASSERT(shellRet == 0, "dmaguard policy block must return 0");
        TEST_ASSERT(oss.str().find("policy updated") != std::string::npos, "policy update must succeed");

        // 10f. dmaguard simulate-attack
        oss.str("");
        shellRet = proc.execute("dmaguard simulate-attack", oss);
        TEST_ASSERT(shellRet == 0, "dmaguard simulate-attack must return 0");
        TEST_ASSERT(oss.str().find("[BLOCKED]") != std::string::npos, "simulate-attack must report BLOCKED");

        // 10g. dmaguard test
        oss.str("");
        shellRet = proc.execute("dmaguard test", oss);
        TEST_ASSERT(shellRet == 0, "dmaguard test must return 0");
        TEST_ASSERT(oss.str().find("ALL TESTS PASSED (100%)") != std::string::npos, "dmaguard test must report ALL TESTS PASSED");

        // 10h. sentinel dma status routing
        oss.str("");
        shellRet = proc.execute("sentinel dma status", oss);
        TEST_ASSERT(shellRet == 0, "sentinel dma status must return 0");
        TEST_ASSERT(oss.str().find("Kernel DMA Protection & Hardware IOMMU Posture:") != std::string::npos, "sentinel dma must route to dmaguard status");
    }

    std::cout << "[TEST] Suite 144: Windows Kernel DMA Protection & IOMMU Remapping Subsystem PASSED.\n";
}

void Test_WindowsSubsystemForLinux_LXSS_Subsystem() {
    using namespace micant::wsl_lxss;

    // ------------------------------------------------------------------------
    // 1. Subsystem Initialization
    // ------------------------------------------------------------------------
    InitializeWslSubsystemExports();
    NTSTATUS initSt = LxInitialize();
    TEST_ASSERT(initSt == STATUS_SUCCESS, "LxInitialize must succeed");

    auto& mgr = PicoKernelManager::Instance();
    TEST_ASSERT(mgr.isInitialized(), "PicoKernelManager must be initialized");

    // ------------------------------------------------------------------------
    // 2. DynamicLoader Exports Parity (wslapi.dll & lxcore.sys)
    // ------------------------------------------------------------------------
    auto& loader = ldr::DynamicLoader::get();

    // wslapi.dll exports
    TEST_ASSERT(loader.getExport("wslapi.dll", "WslIsDistributionRegistered") != nullptr, "wslapi.dll must export WslIsDistributionRegistered");
    TEST_ASSERT(loader.getExport("wslapi.dll", "WslRegisterDistribution") != nullptr, "wslapi.dll must export WslRegisterDistribution");
    TEST_ASSERT(loader.getExport("wslapi.dll", "WslUnregisterDistribution") != nullptr, "wslapi.dll must export WslUnregisterDistribution");
    TEST_ASSERT(loader.getExport("wslapi.dll", "WslConfigureDistribution") != nullptr, "wslapi.dll must export WslConfigureDistribution");
    TEST_ASSERT(loader.getExport("wslapi.dll", "WslGetDistributionConfiguration") != nullptr, "wslapi.dll must export WslGetDistributionConfiguration");
    TEST_ASSERT(loader.getExport("wslapi.dll", "WslLaunchInteractive") != nullptr, "wslapi.dll must export WslLaunchInteractive");

    // lxcore.sys exports
    TEST_ASSERT(loader.getExport("lxcore.sys", "LxInitialize") != nullptr, "lxcore.sys must export LxInitialize");
    TEST_ASSERT(loader.getExport("lxcore.sys", "LxCreatePicoProcess") != nullptr, "lxcore.sys must export LxCreatePicoProcess");
    TEST_ASSERT(loader.getExport("lxcore.sys", "LxCreatePicoThread") != nullptr, "lxcore.sys must export LxCreatePicoThread");
    TEST_ASSERT(loader.getExport("lxcore.sys", "LxDispatchSyscall") != nullptr, "lxcore.sys must export LxDispatchSyscall");
    TEST_ASSERT(loader.getExport("lxcore.sys", "LxGetPicoProcessCount") != nullptr, "lxcore.sys must export LxGetPicoProcessCount");

    // ------------------------------------------------------------------------
    // 3. VersionDatabase Parity
    // ------------------------------------------------------------------------
    auto& vdb = version::VersionDatabase::Instance();
    const auto* modWslApi = vdb.GetModuleInfo("wslapi.dll");
    TEST_ASSERT(modWslApi != nullptr, "VersionDatabase must contain wslapi.dll");
    TEST_ASSERT(modWslApi->stringTable.at("FileVersion") == "10.0.26100.1", "wslapi.dll FileVersion must be 10.0.26100.1");
    TEST_ASSERT(modWslApi->stringTable.at("FileDescription") == "Windows Subsystem for Linux Launcher API", "wslapi.dll FileDescription mismatch");

    const auto* modLxCore = vdb.GetModuleInfo("lxcore.sys");
    TEST_ASSERT(modLxCore != nullptr, "VersionDatabase must contain lxcore.sys");
    TEST_ASSERT(modLxCore->stringTable.at("FileVersion") == "10.0.26100.1", "lxcore.sys FileVersion must be 10.0.26100.1");
    TEST_ASSERT(modLxCore->stringTable.at("FileDescription") == "MicaNT Linux Subsystem Pico Process Core Driver", "lxcore.sys FileDescription mismatch");

    // ------------------------------------------------------------------------
    // 4. Linux ELF64 Binary Header Parser & Validation
    // ------------------------------------------------------------------------
    Elf64_Ehdr ehdrValid{};
    ehdrValid.e_ident[0] = ELF_MAG0;
    ehdrValid.e_ident[1] = ELF_MAG1;
    ehdrValid.e_ident[2] = ELF_MAG2;
    ehdrValid.e_ident[3] = ELF_MAG3;
    ehdrValid.e_ident[4] = ELFCLASS64;
    ehdrValid.e_ident[5] = ELFDATA2LSB;
    ehdrValid.e_machine = EM_X86_64;
    ehdrValid.e_entry = 0x00401122ULL;

    uint64_t parsedEntry = 0;
    std::span<const uint8_t> validElfSpan(reinterpret_cast<const uint8_t*>(&ehdrValid), sizeof(ehdrValid));
    TEST_ASSERT(mgr.validateElfHeader(validElfSpan, &parsedEntry), "Valid ELF64 header must be accepted");
    TEST_ASSERT(parsedEntry == 0x00401122ULL, "ELF entry point must match 0x00401122");

    // Corrupted magic
    Elf64_Ehdr ehdrCorrupt = ehdrValid;
    ehdrCorrupt.e_ident[0] = 0x00;
    TEST_ASSERT(!mgr.validateElfHeader(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&ehdrCorrupt), sizeof(ehdrCorrupt)), &parsedEntry), "Invalid ELF magic must be rejected");

    // 32-bit ELF (ELFCLASS32 = 1)
    Elf64_Ehdr ehdr32 = ehdrValid;
    ehdr32.e_ident[4] = 1;
    TEST_ASSERT(!mgr.validateElfHeader(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&ehdr32), sizeof(ehdr32)), &parsedEntry), "32-bit ELF must be rejected on 64-bit Pico kernel");

    // Wrong architecture (e.g. EM_ARM = 40)
    Elf64_Ehdr ehdrArm = ehdrValid;
    ehdrArm.e_machine = 40;
    TEST_ASSERT(!mgr.validateElfHeader(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&ehdrArm), sizeof(ehdrArm)), &parsedEntry), "Non-x86_64 ELF must be rejected");

    // Buffer smaller than header
    TEST_ASSERT(!mgr.validateElfHeader(validElfSpan.subspan(0, 10), &parsedEntry), "Truncated ELF header must be rejected");

    // ------------------------------------------------------------------------
    // 5. Pico Process & Thread Container Lifecycle
    // ------------------------------------------------------------------------
    size_t countBefore = LxGetPicoProcessCount();
    uint32_t picoPid = 0;
    NTSTATUS createSt = LxCreatePicoProcess("/bin/test_service", "/var/run", &picoPid);
    TEST_ASSERT(createSt == STATUS_SUCCESS, "LxCreatePicoProcess must return STATUS_SUCCESS");
    TEST_ASSERT(picoPid >= 100, "Allocated Pico PID must be >= 100");
    TEST_ASSERT(LxGetPicoProcessCount() == countBefore + 1, "Pico process count must increment");

    uint32_t picoTid = 0;
    NTSTATUS threadSt = LxCreatePicoThread(picoPid, 0x00400000ULL, &picoTid);
    TEST_ASSERT(threadSt == STATUS_SUCCESS, "LxCreatePicoThread must return STATUS_SUCCESS");
    TEST_ASSERT(picoTid == 1, "Initial Pico thread ID must be 1");

    // ------------------------------------------------------------------------
    // 6. Linux Syscall Translation Dispatcher (x86_64 ABI)
    // ------------------------------------------------------------------------
    // SYS_uname
    LinuxUtsName uts{};
    int64_t scUname = LxDispatchSyscall(picoPid, LINUX_SYS_UNAME, reinterpret_cast<uint64_t>(&uts), 0, 0, 0, 0, 0);
    TEST_ASSERT(scUname == 0, "SYS_uname must return 0");
    TEST_ASSERT(std::string(uts.sysname) == "Linux", "uts.sysname must be Linux");
    TEST_ASSERT(std::string(uts.nodename) == "MicaNT", "uts.nodename must be MicaNT");
    TEST_ASSERT(std::string(uts.release) == "6.6.0-microsoft-standard-WSL1", "uts.release must be 6.6.0-microsoft-standard-WSL1");
    TEST_ASSERT(std::string(uts.machine) == "x86_64", "uts.machine must be x86_64");

    // SYS_getpid
    int64_t scPid = LxDispatchSyscall(picoPid, LINUX_SYS_GETPID, 0, 0, 0, 0, 0, 0);
    TEST_ASSERT(scPid == static_cast<int64_t>(picoPid), "SYS_getpid must return container PID");

    // SYS_getuid / gid
    TEST_ASSERT(LxDispatchSyscall(picoPid, LINUX_SYS_GETUID, 0, 0, 0, 0, 0, 0) == 1000, "SYS_getuid must return 1000");
    TEST_ASSERT(LxDispatchSyscall(picoPid, LINUX_SYS_GETGID, 0, 0, 0, 0, 0, 0) == 1000, "SYS_getgid must return 1000");

    // SYS_brk (heap management)
    uint64_t curBrk = static_cast<uint64_t>(LxDispatchSyscall(picoPid, LINUX_SYS_BRK, 0, 0, 0, 0, 0, 0));
    TEST_ASSERT(curBrk == 0x00600000ULL, "Initial heap break must be 0x00600000");
    uint64_t expBrk = static_cast<uint64_t>(LxDispatchSyscall(picoPid, LINUX_SYS_BRK, curBrk + 0x4000, 0, 0, 0, 0, 0));
    TEST_ASSERT(expBrk == 0x00604000ULL, "Expanded heap break must be 0x00604000");

    // SYS_arch_prctl (TLS FS_BASE)
    uint64_t targetFs = 0x00007ffff7fbc700ULL;
    int64_t scSetFs = LxDispatchSyscall(picoPid, LINUX_SYS_ARCH_PRCTL, ARCH_SET_FS, targetFs, 0, 0, 0, 0);
    TEST_ASSERT(scSetFs == 0, "ARCH_SET_FS must return 0");
    uint64_t queriedFs = 0;
    int64_t scGetFs = LxDispatchSyscall(picoPid, LINUX_SYS_ARCH_PRCTL, ARCH_GET_FS, reinterpret_cast<uint64_t>(&queriedFs), 0, 0, 0, 0);
    TEST_ASSERT(scGetFs == 0, "ARCH_GET_FS must return 0");
    TEST_ASSERT(queriedFs == targetFs, "Queried FS_BASE must match target address");

    // SYS_write (stdout)
    const char msg[] = "PicoSyscallOk\n";
    int64_t scWrite = LxDispatchSyscall(picoPid, LINUX_SYS_WRITE, 1, reinterpret_cast<uint64_t>(msg), sizeof(msg) - 1, 0, 0, 0);
    TEST_ASSERT(scWrite == static_cast<int64_t>(sizeof(msg) - 1), "SYS_write to stdout must return byte count");

    // SYS_write invalid fd
    int64_t scWriteBad = LxDispatchSyscall(picoPid, LINUX_SYS_WRITE, 88, reinterpret_cast<uint64_t>(msg), sizeof(msg) - 1, 0, 0, 0);
    TEST_ASSERT(scWriteBad == LINUX_EBADF, "SYS_write to invalid fd must return LINUX_EBADF");

    // SYS_open / SYS_close
    const char osRelPath[] = "/etc/os-release";
    int64_t openFd = LxDispatchSyscall(picoPid, LINUX_SYS_OPEN, reinterpret_cast<uint64_t>(osRelPath), 0, 0, 0, 0, 0);
    TEST_ASSERT(openFd >= 3, "SYS_open on /etc/os-release must return valid fd >= 3");

    int64_t closeFd = LxDispatchSyscall(picoPid, LINUX_SYS_CLOSE, static_cast<uint64_t>(openFd), 0, 0, 0, 0, 0);
    TEST_ASSERT(closeFd == 0, "SYS_close must return 0");

    // SYS_open non-existent
    const char badPath[] = "/nonexistent/null";
    int64_t openBad = LxDispatchSyscall(picoPid, LINUX_SYS_OPEN, reinterpret_cast<uint64_t>(badPath), 0, 0, 0, 0, 0);
    TEST_ASSERT(openBad == LINUX_ENOENT, "SYS_open on missing path must return LINUX_ENOENT");

    // SYS_exit
    int64_t scExit = LxDispatchSyscall(picoPid, LINUX_SYS_EXIT, 0, 0, 0, 0, 0, 0);
    TEST_ASSERT(scExit == 0, "SYS_exit must succeed");

    // ------------------------------------------------------------------------
    // 7. Distribution Management Lifecycle & WslApi C ABI
    // ------------------------------------------------------------------------
    TEST_ASSERT(WslIsDistributionRegistered(L"Ubuntu-24.04") == TRUE, "Ubuntu-24.04 must be registered by default");
    TEST_ASSERT(WslIsDistributionRegistered(L"Debian") == TRUE, "Debian must be registered by default");
    TEST_ASSERT(WslIsDistributionRegistered(L"Alpine") == TRUE, "Alpine must be registered by default");
    TEST_ASSERT(WslIsDistributionRegistered(L"NonExistentOS") == FALSE, "NonExistentOS must not be registered");

    uint32_t defUid = 0, distFlags = 0;
    NTSTATUS cfgSt = WslGetDistributionConfiguration(L"Ubuntu-24.04", &defUid, &distFlags);
    TEST_ASSERT(cfgSt == STATUS_SUCCESS, "WslGetDistributionConfiguration must succeed");
    TEST_ASSERT(defUid == 1000, "Default UID must be 1000");
    TEST_ASSERT((distFlags & 0x7) == 0x7, "WSL distribution flags must be 0x7");

    // Register custom distro
    NTSTATUS regSt = WslRegisterDistribution(L"SovereignLinux", L"sovereign.tar.gz");
    TEST_ASSERT(regSt == STATUS_SUCCESS, "WslRegisterDistribution must succeed");
    TEST_ASSERT(WslIsDistributionRegistered(L"SovereignLinux") == TRUE, "SovereignLinux must be registered");

    // Colliding registration
    NTSTATUS regCol = WslRegisterDistribution(L"SovereignLinux", L"duplicate.tar.gz");
    TEST_ASSERT(regCol == STATUS_OBJECT_NAME_COLLISION, "Duplicate distribution registration must return STATUS_OBJECT_NAME_COLLISION");

    // Unregister custom distro
    NTSTATUS unregSt = WslUnregisterDistribution(L"SovereignLinux");
    TEST_ASSERT(unregSt == STATUS_SUCCESS, "WslUnregisterDistribution must succeed");
    TEST_ASSERT(WslIsDistributionRegistered(L"SovereignLinux") == FALSE, "SovereignLinux must no longer be registered");

    // Unregister non-existent
    TEST_ASSERT(WslUnregisterDistribution(L"SovereignLinux") == STATUS_NOT_FOUND, "Unregistering non-existent distro must return STATUS_NOT_FOUND");

    // ------------------------------------------------------------------------
    // 8. Sovereign VFS Bridge & Command Execution Emulation
    // ------------------------------------------------------------------------
    std::string outUname = mgr.executeLinuxCommand("uname -a");
    TEST_ASSERT(outUname.find("Linux MicaNT 6.6.0-microsoft-standard-WSL1") != std::string::npos, "uname -a must contain MicaNT WSL1 kernel info");
    TEST_ASSERT(outUname.find("x86_64") != std::string::npos, "uname -a must contain x86_64");

    std::string outRel = mgr.executeLinuxCommand("cat /etc/os-release");
    TEST_ASSERT(outRel.find("Ubuntu 24.04 LTS") != std::string::npos, "cat /etc/os-release must contain Ubuntu 24.04 LTS");

    std::string outProc = mgr.executeLinuxCommand("cat /proc/version");
    TEST_ASSERT(outProc.find("MicaNT Sovereign Pico Kernel") != std::string::npos, "cat /proc/version must contain Sovereign Pico Kernel");

    std::string outId = mgr.executeLinuxCommand("id");
    TEST_ASSERT(outId.find("uid=1000(user)") != std::string::npos, "id must contain uid=1000(user)");

    std::string outMount = mgr.executeLinuxCommand("ls /mnt/c");
    TEST_ASSERT(outMount.find("Program Files") != std::string::npos, "ls /mnt/c must display DrvFs root directories");

    // ------------------------------------------------------------------------
    // 9. Interactive Shell Integration & Routing
    // ------------------------------------------------------------------------
    shell::CommandShell testShell;

    // wsl status
    std::ostringstream ossStatus;
    testShell.execute("wsl status", ossStatus);
    std::string strStatus = ossStatus.str();
    TEST_ASSERT(strStatus.find("WSL 1 Sovereign Pico Process Provider (lxcore.sys)") != std::string::npos, "wsl status must display architecture");
    TEST_ASSERT(strStatus.find("Linux 6.6.0 ABI") != std::string::npos, "wsl status must display Linux ABI version");

    // wsl -l
    std::ostringstream ossList;
    testShell.execute("wsl -l", ossList);
    std::string strList = ossList.str();
    TEST_ASSERT(strList.find("Ubuntu-24.04 (Default)") != std::string::npos, "wsl -l must list Ubuntu-24.04 as default");

    // wsl mount
    std::ostringstream ossMount;
    testShell.execute("wsl mount", ossMount);
    TEST_ASSERT(ossMount.str().find("drvfs on /mnt/c type drvfs") != std::string::npos, "wsl mount must display DrvFs mount");

    // wsl test
    std::ostringstream ossTest;
    testShell.execute("wsl test", ossTest);
    TEST_ASSERT(ossTest.str().find("[+] All WSL / LXSS Pico Kernel tests passed successfully.") != std::string::npos, "wsl test must execute self-test suite");

    // sentinel wsl routing
    std::ostringstream ossSentinel;
    testShell.execute("sentinel wsl", ossSentinel);
    TEST_ASSERT(ossSentinel.str().find("Windows Subsystem for Linux (WSL / LXSS) Subsystem Posture:") != std::string::npos, "sentinel wsl must route to wsl status");

    // command execution via shell
    std::ostringstream ossRun;
    testShell.execute("wsl uname -a", ossRun);
    TEST_ASSERT(ossRun.str().find("Linux MicaNT") != std::string::npos, "wsl uname -a via shell must return Linux kernel banner");

    std::cout << "[TEST] Suite 145: Windows Subsystem for Linux (WSL / LXSS) Subsystem PASSED.\n";
}

