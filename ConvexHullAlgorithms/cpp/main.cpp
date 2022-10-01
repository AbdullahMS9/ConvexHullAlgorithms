#include <windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <list>
#include <memory>
#include <iostream>
#include<algorithm>

using namespace std;

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib") 

#include "basewin.h"

template <class T> void SafeRelease(T** ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

class DPIScale
{
    static float scaleX;
    static float scaleY;

public:
    static void Initialize(ID2D1Factory* pFactory)
    {
        FLOAT dpiX, dpiY;
        pFactory->GetDesktopDpi(&dpiX, &dpiY);
        scaleX = dpiX / 96.0f;
        scaleY = dpiY / 96.0f;
    }

    template <typename T>
    static D2D1_POINT_2F PixelsToDips(T x, T y)
    {
        return D2D1::Point2F(static_cast<float>(x) / scaleX, static_cast<float>(y) / scaleY);
    }
};


static int GetRandomNumber(int min, int max) {
    return min + (std::rand() % (max - min + 1));
}

float DPIScale::scaleX = 1.0f;
float DPIScale::scaleY = 1.0f;


enum AppState { QUICK_HULL, POINT_CONVEX_HULL, GJK, M_SUM, M_DIFF, NONE };
static AppState currentAppState;

struct Shape {

    AppState targetAppState = NONE;

    virtual void Draw(ID2D1RenderTarget* pRenderTarget, ID2D1SolidColorBrush* pBrush) = 0;

    virtual void WriteText(
        ID2D1RenderTarget* pRenderTarget,
        ID2D1SolidColorBrush* pBrush,
        IDWriteTextFormat* pTextFormat) { };

    virtual bool IsMouseOverlapping(float mousePosX, float mousePosY) { return 0; };

    virtual void OnMouseClick(float x, float y) { };

    virtual void OnMouseDrag(float x, float y) { };
};

struct Grid : Shape {

    float lineSpacing;
    float gridLinesStrokeWidth;
    float axisLinesStrokeWidth;
    D2D1_COLOR_F gridLinesColor;
    D2D1_COLOR_F axisLinesColor;

    void Draw(ID2D1RenderTarget* pRenderTarget, ID2D1SolidColorBrush* pBrush) {
        D2D1_SIZE_F rtSize = pRenderTarget->GetSize();
        float centerX = rtSize.width / 2;
        float centerY = rtSize.height / 2;
        pBrush->SetColor(gridLinesColor);
        for (float x = centerX; x < rtSize.width; x += lineSpacing)
        {
            pRenderTarget->DrawLine(
                D2D1::Point2F(x, 0.0f),
                D2D1::Point2F(x, rtSize.height),
                pBrush,
                gridLinesStrokeWidth
            );
        }
        for (float x = centerX; x > 0; x -= lineSpacing)
        {
            pRenderTarget->DrawLine(
                D2D1::Point2F(x, 0.0f),
                D2D1::Point2F(x, rtSize.height),
                pBrush,
                gridLinesStrokeWidth
            );
        }
        for (float y = centerY; y < rtSize.height; y += lineSpacing)
        {
            pRenderTarget->DrawLine(
                D2D1::Point2F(0.0f, y),
                D2D1::Point2F(rtSize.width, y),
                pBrush,
                gridLinesStrokeWidth
            );
        }
        for (float y = centerY; y > 0; y -= lineSpacing)
        {
            pRenderTarget->DrawLine(
                D2D1::Point2F(0.0f, y),
                D2D1::Point2F(rtSize.width, y),
                pBrush,
                gridLinesStrokeWidth
            );
        }
        pBrush->SetColor(axisLinesColor);
        pRenderTarget->DrawLine(
            D2D1::Point2F(centerX, 0),
            D2D1::Point2F(centerX, rtSize.height),
            pBrush,
            axisLinesStrokeWidth
        );
        pRenderTarget->DrawLine(
            D2D1::Point2F(0, centerY),
            D2D1::Point2F(rtSize.width, centerY),
            pBrush,
            axisLinesStrokeWidth
        );

    }

};

struct MoveablePoint : Shape {

    D2D1_POINT_2F center;
    float radius;
    D2D1_COLOR_F fillColor;


    void Draw(ID2D1RenderTarget* pRenderTarget, ID2D1SolidColorBrush* pBrush) {
        D2D1_ELLIPSE ellipse = D2D1::Ellipse(
            center,
            radius,
            radius
        );
        pBrush->SetColor(fillColor);
        pRenderTarget->FillEllipse(ellipse, pBrush);
    }

    void OnMouseClick() {
        fillColor = D2D1::ColorF(D2D1::ColorF::Red);
    }

    void OnMouseDrag(float x, float y) {
        center.x = x;
        center.y = y;
    }

    bool IsMouseOverlapping(float x, float y) {
        float a = (x - center.x) * (x - center.x);
        float b = (y - center.y) * (y - center.y);
        return a * b < radius* radius;
    }

};

struct NPolygon : Shape {

    vector<D2D1_POINT_2F> points;
    D2D1_COLOR_F color;
    float strokeWidth;

    void Draw(ID2D1RenderTarget* pRenderTarget, ID2D1SolidColorBrush* pBrush) {
        pBrush->SetColor(color);
        for (int i = 1; i < points.size(); i++) {
            pRenderTarget->DrawLine(points[i - 1], points[i], pBrush, strokeWidth);
        }
        pRenderTarget->DrawLine(points[0], points[points.size() - 1], pBrush, strokeWidth);
    }

    bool IsMouseOverlapping(float x, float y) {
        return 0;
    }

};

struct Button : Shape {

    D2D1_RECT_F rectangle;
    D2D1_COLOR_F fillColor;
    D2D1_COLOR_F outlineColor;
    D2D1_COLOR_F textColor;
    string text;
    float outlineWidth;
    void (*onClickCallback) ();

    void Draw(ID2D1RenderTarget* pRenderTarget, ID2D1SolidColorBrush* pBrush) {
        pBrush->SetColor(outlineColor);
        pRenderTarget->DrawRectangle(rectangle, pBrush, outlineWidth);
        pBrush->SetColor(fillColor);
        pRenderTarget->FillRectangle(rectangle, pBrush);
    }

    void WriteText(
        ID2D1RenderTarget* pRenderTarget,
        ID2D1SolidColorBrush* pBrush,
        IDWriteTextFormat* pTextFormat) {

        std::wstring widestr = std::wstring(text.begin(), text.end());
        const wchar_t* widecstr = widestr.c_str();

        pBrush->SetColor(textColor);
        pRenderTarget->DrawTextW(
            widecstr,
            text.size(),
            pTextFormat,
            rectangle,
            pBrush
        );
    };

    void OnMouseClick(float x, float y) {
        onClickCallback();
    }

    bool IsMouseOverlapping(float x, float y) {
        return x < rectangle.right&& x > rectangle.left && y > rectangle.top && y < rectangle.bottom;
    }

};

struct QuickHull : Shape {

    vector<MoveablePoint*> points;
    vector<MoveablePoint*> hullPoints;

    void Draw(ID2D1RenderTarget* pRenderTarget, ID2D1SolidColorBrush* pBrush) {
        CalculateHull();

        for (int i = 1; i < hullPoints.size(); i++) {

            MoveablePoint* pointA = hullPoints[i - 1];
            MoveablePoint* pointB = hullPoints[i];
            pBrush->SetColor(pointA->fillColor);
            pRenderTarget->DrawLine(pointA->center, pointB->center, pBrush, 2.0f);
        }
        pRenderTarget->DrawLine(hullPoints[hullPoints.size() - 1]->center, hullPoints[0]->center, pBrush, 2.0f);

    }

    void CalculateHull() {
        hullPoints.clear();
        MoveablePoint* leftMostPoint = points[0];
        MoveablePoint* rightMostPoint = points[0];

        for (int i = 0; i < points.size(); i++) {
            MoveablePoint* pt = points[i];
            if (pt->center.x < leftMostPoint->center.x)
                leftMostPoint = pt;
            if (pt->center.x > rightMostPoint->center.x)
                rightMostPoint = pt;
        }

        hullPoints.push_back(leftMostPoint);
        hullPoints.push_back(rightMostPoint);

        quickHull(leftMostPoint, rightMostPoint, 1);
        quickHull(leftMostPoint, rightMostPoint, -1);

        std::sort(hullPoints.begin(), hullPoints.end(),
            [&](MoveablePoint* a, MoveablePoint* b) -> bool
            {
                float slopeA = (a->center.y - leftMostPoint->center.y) / (a->center.x - leftMostPoint->center.x);
                float slopeB = (b->center.y - leftMostPoint->center.y) / (b->center.x - leftMostPoint->center.x);
                return slopeA < slopeB;
            });

    }

    void quickHull(MoveablePoint* p1, MoveablePoint* p2, int side)
    {
        MoveablePoint* p = nullptr;
        int maxDistance = 0;

        for (int i = 0; i < points.size(); i++)
        {
            int distance = abs(signedLineDist(p1, p2, points[i]));
            if (findSide(p1, p2, points[i]) == side && distance > maxDistance)
            {
                p = points[i];
                maxDistance = distance;
            }
        }

        if (p == nullptr)
        {
            if (std::count(hullPoints.begin(), hullPoints.end(), p1) == 0) {
                hullPoints.push_back(p1);
            }
            if (std::count(hullPoints.begin(), hullPoints.end(), p2) == 0) {
                hullPoints.push_back(p2);
            }
            return;
        }

        quickHull(p, p1, -findSide(p, p1, p2));
        quickHull(p, p2, -findSide(p, p2, p1));
    }

    int findSide(MoveablePoint* p1, MoveablePoint* p2, MoveablePoint* pTest)
    {
        int d = signedLineDist(p1, p2, pTest);

        if (d > 0)
            return 1;
        if (d < 0)
            return -1;
        return 0;
    }

    int signedLineDist(MoveablePoint* p1, MoveablePoint* p2, MoveablePoint* pTest)
    {
        return (pTest->center.y - p1->center.y) * (p2->center.x - p1->center.x) -
            (p2->center.y - p1->center.y) * (pTest->center.x - p1->center.x);
    }

};


struct PointConvexHull : Shape {

    QuickHull hull;
    MoveablePoint* point;
    D2D1_COLOR_F inColor;
    D2D1_COLOR_F outColor;

    void Draw(ID2D1RenderTarget* pRenderTarget, ID2D1SolidColorBrush* pBrush) {
        if (isPointInside()) {
            point->fillColor = inColor;
        }
        else {
            point->fillColor = outColor;
        }
        hull.Draw(pRenderTarget, pBrush);

    }

    bool isPointInside() {

        hull.points.push_back(point);
        hull.CalculateHull();
        bool isPartOfHull = std::count(hull.hullPoints.begin(), hull.hullPoints.end(), point) > 0;

        hull.points.erase(std::remove(hull.points.begin(), hull.points.end(), point), hull.points.end());
        hull.hullPoints.erase(std::remove(hull.hullPoints.begin(), hull.hullPoints.end(), point), hull.hullPoints.end());

        return !isPartOfHull;
    }

};

struct MinkowskiSum : Shape {

    QuickHull* hull1;
    QuickHull* hull2;
    QuickHull* result;
    D2D1_COLOR_F resultColor;

    void Draw(ID2D1RenderTarget* pRenderTarget, ID2D1SolidColorBrush* pBrush) {

        hull1->Draw(pRenderTarget, pBrush);
        hull2->Draw(pRenderTarget, pBrush);
        CalculateSum();
        result->Draw(pRenderTarget, pBrush);
    }

    void CalculateSum() {
        result->hullPoints.clear();
        for (int i = 0; i < hull1->hullPoints.size(); i++) {
            MoveablePoint* point1 = hull1->hullPoints[i];
            for (int k = 0; k < hull2->hullPoints.size(); k++) {
                MoveablePoint* point2 = hull2->hullPoints[k];
                MoveablePoint* newPoint = new MoveablePoint();
                float pointsXAdded = point1->center.x + point2->center.x;
                float pointsYAdded = point1->center.y + point2->center.y;
                newPoint->center = D2D1::Point2F(pointsXAdded, pointsYAdded);
                newPoint->radius = 1;
                newPoint->fillColor = D2D1::ColorF(D2D1::ColorF::Red);
                result->points.push_back(newPoint);
            }
        }
    }

};

struct MinkowskiDifference : Shape {

    QuickHull* hull1;
    QuickHull* hull2;
    QuickHull* result;
    D2D1_COLOR_F resultColor;

    void Draw(ID2D1RenderTarget* pRenderTarget, ID2D1SolidColorBrush* pBrush) {

        hull1->Draw(pRenderTarget, pBrush);
        hull2->Draw(pRenderTarget, pBrush);
        CalculateDiff();
        result->Draw(pRenderTarget, pBrush);
    }

    void CalculateDiff() {
        result->hullPoints.clear();
        for (int i = 0; i < hull1->hullPoints.size(); i++) {
            MoveablePoint* point1 = hull1->hullPoints[i];
            for (int k = 0; k < hull2->hullPoints.size(); k++) {
                MoveablePoint* point2 = hull2->hullPoints[k];
                MoveablePoint* newPoint = new MoveablePoint();
                float pointsXAdded = point1->center.x - point2->center.x;
                float pointsYAdded = point1->center.y - point2->center.y;
                newPoint->center = D2D1::Point2F(pointsXAdded, pointsYAdded);
                newPoint->radius = 1;
                newPoint->fillColor = D2D1::ColorF(D2D1::ColorF::Red);
                result->points.push_back(newPoint);
            }
        }
    }

};


class MainWindow : public BaseWindow<MainWindow>
{
    ID2D1Factory* pFactory;
    ID2D1HwndRenderTarget* pRenderTarget;
    ID2D1SolidColorBrush* pBrush;
    D2D1_POINT_2F           ptMouse;

    IDWriteFactory* pWriteFactory;
    IDWriteFontCollection* fontCollection;
    IDWriteTextFormat* pTextFormat;

    vector<Shape*> shapes;
    Shape* selection = nullptr;

    void    CalculateLayout() { }
    HRESULT CreateGraphicsResources();
    void    DiscardGraphicsResources();
    void    OnPaint();
    void    Resize();
    void    OnLButtonDown(int pixelX, int pixelY, DWORD flags);
    void    OnLButtonUp();
    void    OnMouseMove(int pixelX, int pixelY, DWORD flags);
    void    CreateScene();
    void    UpdateScene();

public:

    MainWindow() : pFactory(NULL), pRenderTarget(NULL), pBrush(NULL),
        ptMouse(D2D1::Point2F())
    {
    }

    PCWSTR  ClassName() const { return L"Circle Window Class"; }
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
};


HRESULT MainWindow::CreateGraphicsResources()
{
    HRESULT hr = S_OK;
    if (pRenderTarget == NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);


        hr = pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &pRenderTarget);


        static const WCHAR fontName[] = L"Verdana";
        static const FLOAT fontSize = 20;

        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
        if (SUCCEEDED(hr))
        {
            hr = DWriteCreateFactory(
                DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&pWriteFactory)
            );
        }
        if (SUCCEEDED(hr))
        {
            hr = pWriteFactory->CreateTextFormat(
                fontName,
                NULL,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                fontSize,
                L"",
                &pTextFormat
            );
        }
        if (SUCCEEDED(hr))
        {
            pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }


        if (SUCCEEDED(hr))
        {
            const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 1.0f, 0);
            hr = pRenderTarget->CreateSolidColorBrush(color, &pBrush);

            if (SUCCEEDED(hr))
            {
                CalculateLayout();
            }
        }

    }
    return hr;
}

void MainWindow::DiscardGraphicsResources()
{
    SafeRelease(&pRenderTarget);
    SafeRelease(&pBrush);
}

void MainWindow::OnPaint()
{
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);

        pRenderTarget->BeginDraw();

        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

        WCHAR sc_helloWorld[] = L"Hello, World!";

        if (shapes.empty()) {
            CreateScene();
        }
        else {
            UpdateScene();
        }


        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::Resize()
{
    if (pRenderTarget != NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        pRenderTarget->Resize(size);
        CalculateLayout();
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::OnLButtonDown(int pixelX, int pixelY, DWORD flags)
{
    SetCapture(m_hwnd);

    if (selection != nullptr) {
        return;
    }

    for (int i = shapes.size() - 1; i >= 0; i--)
    {
        boolean interactable = shapes[i]->targetAppState == NONE || shapes[i]->targetAppState == currentAppState;
        if (interactable && shapes[i]->IsMouseOverlapping(pixelX, pixelY)) {
            selection = shapes[i];
            shapes[i]->OnMouseClick(pixelX, pixelY);
            break;
        }
    }

    InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::OnMouseMove(int pixelX, int pixelY, DWORD flags)
{
    if (flags & MK_LBUTTON && selection != nullptr)
    {
        boolean interactable = selection->targetAppState == NONE || selection->targetAppState == currentAppState;

        selection->OnMouseDrag(pixelX, pixelY);

        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::OnLButtonUp()
{
    selection = nullptr;
    ReleaseCapture();
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    MainWindow win;

    if (!win.Create(L"Circle", WS_OVERLAPPEDWINDOW))
    {
        return 0;
    }

    ShowWindow(win.Window(), nCmdShow);

    // Run the message loop.

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        if (FAILED(D2D1CreateFactory(
            D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))
        {
            return -1;  // Fail CreateWindowEx.
        }
        DPIScale::Initialize(pFactory);
        OnPaint();
        return 0;

    case WM_DESTROY:
        DiscardGraphicsResources();
        SafeRelease(&pFactory);
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        OnPaint();
        return 0;

    case WM_SIZE:
        Resize();
        return 0;

    case WM_LBUTTONDOWN:
        OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
        return 0;

    case WM_LBUTTONUP:
        OnLButtonUp();
        return 0;

    case WM_MOUSEMOVE:
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
        return 0;
    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}



void MainWindow::CreateScene() {

    currentAppState = NONE;

    //Create polygon
    /*NPolygon *polygon = new NPolygon();
    shapes.push_back(polygon);
    polygon->color = D2D1::ColorF(D2D1::ColorF::Aqua);
    polygon->strokeWidth = 5.0f;
    polygon->points = {
        D2D1::Point2F(0, 0),
        D2D1::Point2F(100, 200),
        D2D1::Point2F(800, 700)
    };*/

    //Create Grid Lines
    Grid* grid = new Grid();
    shapes.push_back(grid);
    grid->lineSpacing = 50.0f;
    grid->gridLinesColor = D2D1::ColorF(D2D1::ColorF::Gray);
    grid->gridLinesStrokeWidth = 1.0f;
    grid->axisLinesColor = D2D1::ColorF(D2D1::ColorF::White);
    grid->axisLinesStrokeWidth = 2.0f;


    //Create Button
    Button* button1 = new Button();
    shapes.push_back(button1);
    button1->fillColor = D2D1::ColorF(D2D1::ColorF::White);
    button1->outlineColor = D2D1::ColorF(D2D1::ColorF::Red);
    button1->textColor = D2D1::ColorF(D2D1::ColorF::Black);
    button1->text = "Quick Hull";
    button1->outlineWidth = 5.0f;
    button1->rectangle = D2D1::RectF(50, 50, 200, 100);
    button1->onClickCallback = []() {
        currentAppState = QUICK_HULL;
    };

    //Create Button
    Button* button2 = new Button();
    shapes.push_back(button2);
    button2->fillColor = D2D1::ColorF(D2D1::ColorF::White);
    button2->outlineColor = D2D1::ColorF(D2D1::ColorF::Red);
    button2->textColor = D2D1::ColorF(D2D1::ColorF::Black);
    button2->text = "Point Convex Hull";
    button2->outlineWidth = 5.0f;
    button2->rectangle = D2D1::RectF(50, 150, 200, 200);
    button2->onClickCallback = []() -> void {
        currentAppState = POINT_CONVEX_HULL;
    };

    //Create Button
    Button* button3 = new Button();
    shapes.push_back(button3);
    button3->fillColor = D2D1::ColorF(D2D1::ColorF::White);
    button3->outlineColor = D2D1::ColorF(D2D1::ColorF::Red);
    button3->textColor = D2D1::ColorF(D2D1::ColorF::Black);
    button3->text = "GJK";
    button3->outlineWidth = 5.0f;
    button3->rectangle = D2D1::RectF(50, 250, 200, 300);
    button3->onClickCallback = []() -> void {
        currentAppState = GJK;
    };

    //Create Button
    Button* button4 = new Button();
    shapes.push_back(button4);
    button4->fillColor = D2D1::ColorF(D2D1::ColorF::White);
    button4->outlineColor = D2D1::ColorF(D2D1::ColorF::Red);
    button4->textColor = D2D1::ColorF(D2D1::ColorF::Black);
    button4->text = "Minkowski Sum";
    button4->outlineWidth = 5.0f;
    button4->rectangle = D2D1::RectF(50, 350, 200, 400);
    button4->onClickCallback = []() -> void {
        currentAppState = M_SUM;
    };

    //Create Button
    Button* button5 = new Button();
    shapes.push_back(button5);
    button5->fillColor = D2D1::ColorF(D2D1::ColorF::White);
    button5->outlineColor = D2D1::ColorF(D2D1::ColorF::Red);
    button5->textColor = D2D1::ColorF(D2D1::ColorF::Black);
    button5->text = "Minkowski Difference";
    button5->outlineWidth = 5.0f;
    button5->rectangle = D2D1::RectF(50, 450, 200, 500);
    button5->onClickCallback = []() -> void {
        currentAppState = M_DIFF;
    };

    //Quick Hull
    QuickHull* quickHull = new QuickHull();
    quickHull->targetAppState = QUICK_HULL;
    for (int i = 0; i < 10; i++) {
        MoveablePoint* point = new MoveablePoint();
        quickHull->points.push_back(point);
        shapes.push_back(point);
        point->targetAppState = QUICK_HULL;
        D2D1_SIZE_F rtSize = pRenderTarget->GetSize();
        float randomX = GetRandomNumber(0, rtSize.width);
        float randomY = GetRandomNumber(0, rtSize.height);
        point->center = D2D1::Point2F(randomX, randomY);
        point->radius = 20.0f;
        point->fillColor = D2D1::ColorF(D2D1::ColorF::Blue);
    }
    shapes.push_back(quickHull);

    //Point Convex Hull
    PointConvexHull* pointConvexHull = new PointConvexHull();
    pointConvexHull->targetAppState = POINT_CONVEX_HULL;
    pointConvexHull->hull = *quickHull;
    pointConvexHull->inColor = D2D1::ColorF(D2D1::ColorF::Red);
    pointConvexHull->outColor = D2D1::ColorF(D2D1::ColorF::Blue);
    MoveablePoint* point = new MoveablePoint();
    point->targetAppState = POINT_CONVEX_HULL;
    pointConvexHull->point = point;
    D2D1_SIZE_F rtSize = pRenderTarget->GetSize();
    point->center = D2D1::Point2F(rtSize.width / 2, rtSize.height / 2);
    point->radius = 20.0f;
    shapes.push_back(pointConvexHull);
    shapes.push_back(point);


    //Minkowski Sum
    MinkowskiSum* mSum = new MinkowskiSum();
    mSum->targetAppState = M_SUM;

    QuickHull* quickHull1 = new QuickHull();
    quickHull1->targetAppState = M_SUM;
    for (int i = 0; i < 5; i++) {
        MoveablePoint* point = new MoveablePoint();
        quickHull1->points.push_back(point);
        point->targetAppState = M_SUM;
        D2D1_SIZE_F rtSize = pRenderTarget->GetSize();
        float randomX = GetRandomNumber(0, rtSize.width);
        float randomY = GetRandomNumber(0, rtSize.height);
        point->center = D2D1::Point2F(randomX, randomY);
        point->radius = 20.0f;
        point->fillColor = D2D1::ColorF(D2D1::ColorF::Blue);
        shapes.push_back(point);
    }
    QuickHull* quickHull2 = new QuickHull();
    quickHull2->targetAppState = M_SUM;
    for (int i = 0; i < 5; i++) {
        MoveablePoint* point = new MoveablePoint();
        quickHull2->points.push_back(point);
        point->targetAppState = M_SUM;
        D2D1_SIZE_F rtSize = pRenderTarget->GetSize();
        float randomX = GetRandomNumber(0, rtSize.width);
        float randomY = GetRandomNumber(0, rtSize.height);
        point->center = D2D1::Point2F(randomX, randomY);
        point->radius = 20.0f;
        point->fillColor = D2D1::ColorF(D2D1::ColorF::Blue);
        shapes.push_back(point);
    }
    QuickHull* resultHull = new QuickHull();
    mSum->hull1 = quickHull1;
    mSum->hull2 = quickHull2;
    mSum->result = resultHull;

    shapes.push_back(mSum);


    //Minkowski Diff
    MinkowskiDifference* mDiff = new MinkowskiDifference();
    mDiff->targetAppState = M_DIFF;
    QuickHull* quickHull3 = new QuickHull();
    quickHull1->targetAppState = M_DIFF;
    for (int i = 0; i < 5; i++) {
        MoveablePoint* point = new MoveablePoint();
        quickHull3->points.push_back(point);
        point->targetAppState = M_DIFF;
        D2D1_SIZE_F rtSize = pRenderTarget->GetSize();
        float randomX = GetRandomNumber(0, rtSize.width);
        float randomY = GetRandomNumber(0, rtSize.height);
        point->center = D2D1::Point2F(randomX, randomY);
        point->radius = 20.0f;
        point->fillColor = D2D1::ColorF(D2D1::ColorF::Blue);
        shapes.push_back(point);
    }
    QuickHull* quickHull4 = new QuickHull();
    quickHull2->targetAppState = M_DIFF;
    for (int i = 0; i < 5; i++) {
        MoveablePoint* point = new MoveablePoint();
        quickHull4->points.push_back(point);
        point->targetAppState = M_DIFF;
        D2D1_SIZE_F rtSize = pRenderTarget->GetSize();
        float randomX = GetRandomNumber(0, rtSize.width);
        float randomY = GetRandomNumber(0, rtSize.height);
        point->center = D2D1::Point2F(randomX, randomY);
        point->radius = 20.0f;
        point->fillColor = D2D1::ColorF(D2D1::ColorF::Blue);
        shapes.push_back(point);
    }
    QuickHull* resultHull2 = new QuickHull();
    mDiff->hull1 = quickHull3;
    mDiff->hull2 = quickHull4;
    mDiff->result = resultHull2;

    shapes.push_back(mDiff);

}

void MainWindow::UpdateScene() {
    for each (Shape * shape in shapes)
    {
        if (shape->targetAppState == NONE || shape->targetAppState == currentAppState) {
            shape->Draw(pRenderTarget, pBrush);
            shape->WriteText(pRenderTarget, pBrush, pTextFormat);
        }
    }
}


