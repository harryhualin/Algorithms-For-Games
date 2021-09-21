#include <windows.h>
#include <Windowsx.h>
#include <d2d1.h>


#include <list>
#include <memory>
using namespace std;

#pragma comment(lib, "d2d1")

#include "basewin.h"
#include "resource.h"

template <class T> void SafeRelease(T **ppT)
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
    static void Initialize(ID2D1Factory *pFactory)
    {
        FLOAT dpiX, dpiY;
        pFactory->GetDesktopDpi(&dpiX, &dpiY);
        scaleX = dpiX/96.0f;
        scaleY = dpiY/96.0f;
    }

    template <typename T>
    static float PixelsToDipsX(T x)
    {
        return static_cast<float>(x) / scaleX;
    }

    template <typename T>
    static float PixelsToDipsY(T y)
    {
        return static_cast<float>(y) / scaleY;
    }
};

float DPIScale::scaleX = 1.0f;
float DPIScale::scaleY = 1.0f;

struct MyEllipse
{
    D2D1_ELLIPSE    ellipse;
    D2D1_COLOR_F    color;
    int             group;
           

    void Draw(ID2D1RenderTarget *pRT, ID2D1SolidColorBrush *pBrush)
    {
        pBrush->SetColor(color);
        pRT->FillEllipse(ellipse, pBrush);
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
        pRT->DrawEllipse(ellipse, pBrush, 1.0f);
    }

    void ChangeColor(D2D1_COLOR_F newColor) {
        color = newColor;
    }

    BOOL HitTest(float x, float y)
    {
        const float a = ellipse.radiusX;
        const float b = ellipse.radiusY;
        const float x1 = x - ellipse.point.x;
        const float y1 = y - ellipse.point.y;
        const float d = ((x1 * x1) / (a * a)) + ((y1 * y1) / (b * b));
        return d <= 1.0f;
    }
};

// Global Variables
shared_ptr<MyEllipse> innerPoint; // Point inside convex hull


class MainWindow : public BaseWindow<MainWindow>
{
    enum Mode
    {
        SelectMode,
        DragMode
    };

    enum Screen
    {
        MinkowskiSum,
        MinkowskiDifference,
        GJK,
        QuickHull,
        PointConvexHull
    };

    HCURSOR                 hCursor;

    ID2D1Factory            *pFactory;
    ID2D1HwndRenderTarget   *pRenderTarget;
    ID2D1SolidColorBrush    *pBrush;
    D2D1_POINT_2F           ptMouse;

    Mode                    mode;
    Screen                  screen;
    size_t                  nextColor;

    list<shared_ptr<MyEllipse>>             ellipses;
    list<shared_ptr<MyEllipse>>::iterator   selection;

    list<shared_ptr<MyEllipse>>             hull1;
    list<shared_ptr<MyEllipse>>             hull2;
    list<shared_ptr<MyEllipse>>             group1;
    list<shared_ptr<MyEllipse>>             group2;
    int                                     group;

    float                                   centerX;
    float                                   centerY;

     
    shared_ptr<MyEllipse> Selection() 
    { 
        if (selection == ellipses.end()) 
        { 
            return nullptr;
        }
        else
        {
            return (*selection);
        }
    }

    void    ClearSelection() { selection = ellipses.end(); }
    HRESULT InsertEllipse(float x, float y, float radius, D2D1::ColorF color, int group);

    BOOL    HitTest(float x, float y);
    void    SetMode(Mode m);
    void    MoveSelection(float x, float y);
    HRESULT CreateGraphicsResources();
    void    DiscardGraphicsResources();
    void    OnPaint();
    void    Resize();
    void    OnLButtonDown(int pixelX, int pixelY, DWORD flags);
    void    OnLButtonUp();
    void    OnMouseMove(int pixelX, int pixelY, DWORD flags);
    void    OnKeyDown(UINT vkey);
    void    CreateButtons();
    void    QuickHullButton();
    void    MinkowskiSumButton();
    void    MinkowskiDifferenceButton();
    void    PointConvexHullButton();
    void    GJKButton();
    void    QuickHullAlgorithm(list<shared_ptr<MyEllipse>> ellipses, int n, list<shared_ptr<MyEllipse>> *hull);
    void    MinkowskiSumAlgorithm(list<shared_ptr<MyEllipse>> group1, list<shared_ptr<MyEllipse>> group2, list<shared_ptr<MyEllipse>>* hull);
    void    MinkowskiDifferenceAlgorithm(list<shared_ptr<MyEllipse>> group1, list<shared_ptr<MyEllipse>> group2, list<shared_ptr<MyEllipse>>* hull);
    void    PointConvexHullAlgorithm(list<shared_ptr<MyEllipse>> ellipses);
    void    GJKAlgorithm(list<shared_ptr<MyEllipse>> ellipses);
    void    QuickHullDraw();
    void    MinkowskiSumDraw();
    void    MinkowskiDifferenceDraw();
    void    PointConvexHullDraw();
    void    GJKDraw();


public:

    MainWindow() : pFactory(NULL), pRenderTarget(NULL), pBrush(NULL), 
        ptMouse(D2D1::Point2F()), nextColor(0), selection(ellipses.end())
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

        if (SUCCEEDED(hr))
        {
            const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 1.0f, 0);
            hr = pRenderTarget->CreateSolidColorBrush(color, &pBrush);
        }
    }
    return hr;
}

void MainWindow::DiscardGraphicsResources()
{
    SafeRelease(&pRenderTarget);
    SafeRelease(&pBrush);
}

bool AngleComparison(shared_ptr<MyEllipse> p1, shared_ptr<MyEllipse> p2) {
    double p1x = p1->ellipse.point.x;
    double p2x = p2->ellipse.point.x;
    double p1y = p1->ellipse.point.y;
    double p2y = p2->ellipse.point.y;
    double angle1 = atan2(p1y - innerPoint->ellipse.point.y, p1x - innerPoint->ellipse.point.x);
    double angle2 = atan2(p2y - innerPoint->ellipse.point.y, p2x - innerPoint->ellipse.point.x);
    return (angle1 < angle2);
}

void MainWindow::OnPaint()
{
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);
     
        pRenderTarget->BeginDraw();

        pRenderTarget->Clear( D2D1::ColorF(D2D1::ColorF::SkyBlue));

        // Draw a grid background.
        RECT rect;
        GetWindowRect(m_hwnd, &rect);
        int width = static_cast<int>(rect.right - rect.left);
        int height = static_cast<int>(rect.bottom - rect.top);

        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::DarkGray));
        for (int x = 220; x < width; x += 20)
        {
            if (x == (width + 220)/2 - (((width + 220)/2)%20)) {
                pRenderTarget->DrawLine(
                    D2D1::Point2F(static_cast<FLOAT>(x), 0.0f),
                    D2D1::Point2F(static_cast<FLOAT>(x), rect.bottom),
                    pBrush,
                    5.0f
                );
                centerX = x;
            }
            else {
                pRenderTarget->DrawLine(
                    D2D1::Point2F(static_cast<FLOAT>(x), 0.0f),
                    D2D1::Point2F(static_cast<FLOAT>(x), rect.bottom),
                    pBrush,
                    0.5f
                );
            }
        }

        for (int y = 0; y < height; y += 20)
        {
            if (y == height/2 - ((height/2)%20)) {
                pRenderTarget->DrawLine(
                    D2D1::Point2F(220.0f, static_cast<FLOAT>(y)),
                    D2D1::Point2F(rect.right, static_cast<FLOAT>(y)),
                    pBrush,
                    5.0f
                );
                centerY = y;
            }
            else {
                pRenderTarget->DrawLine(
                    D2D1::Point2F(220.0f, static_cast<FLOAT>(y)),
                    D2D1::Point2F(rect.right, static_cast<FLOAT>(y)),
                    pBrush,
                    0.5f
                );
            }
        }

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
        {
            // Redraw Circles
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
            if ((*i)->group == 1)
                (*i)->ChangeColor(D2D1::ColorF(D2D1::ColorF::Red));
            else if ((*i)->group == 2)
                (*i)->ChangeColor(D2D1::ColorF(D2D1::ColorF::Blue));
            (*i)->Draw(pRenderTarget, pBrush);
        }

        // Determine algorithm to use to represent on screen
        switch (screen) {
        case QuickHull:
            QuickHullDraw();
            break;

        case MinkowskiSum:
            MinkowskiSumDraw();
            break;

        case MinkowskiDifference:
            MinkowskiDifferenceDraw();
            break;

        case PointConvexHull:
            PointConvexHullDraw();
            break;

        case GJK:
            GJKDraw();
            break;

        }

        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);

    }
}

void MainWindow::QuickHullDraw() {
    hull1.clear();
    shared_ptr<MyEllipse> prev;
    QuickHullAlgorithm(ellipses, ellipses.size(), &hull1);
    pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
    pRenderTarget->DrawLine(
        hull1.front()->ellipse.point,
        hull1.back()->ellipse.point,
        pBrush,
        3.0f
    );
    prev = hull1.front();
    for (auto i = hull1.begin(); i != hull1.end(); ++i)
    {
        // Redraw Convex hull circles
        (*i)->ChangeColor(D2D1::ColorF(D2D1::ColorF::Blue));
        (*i)->Draw(pRenderTarget, pBrush);

        // Redraw lines of convex hull
        if (i != hull1.begin()) {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            pRenderTarget->DrawLine(
                prev->ellipse.point,
                (*i)->ellipse.point,
                pBrush,
                3.0f
            );
        }
        prev = *i;
    }
}

void MainWindow::MinkowskiSumDraw() {
    
    // Divide ellipses into two groups
    group1.clear();
    group2.clear();
    hull1.clear();
    hull2.clear();
    list<shared_ptr<MyEllipse>> hull3;
    list<shared_ptr<MyEllipse>> hull4;
    shared_ptr<MyEllipse> prev;

    for (auto i = ellipses.begin(); i != ellipses.end(); ++i) {
        if ((*i)->group == 1) {
            group1.push_back(*i);
        }
        else if ((*i)->group == 2) {
            group2.push_back(*i);
        }
    }

    // Draw first set of convex hulls
    QuickHullAlgorithm(group1, group1.size(), &hull1);
    pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
    pRenderTarget->DrawLine(
        hull1.front()->ellipse.point,
        hull1.back()->ellipse.point,
        pBrush,
        3.0f
    );
    prev = hull1.front();
    for (auto i = hull1.begin(); i != hull1.end(); ++i)
    {
        // Redraw lines of convex hull
        if (i != hull1.begin()) {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            pRenderTarget->DrawLine(
                prev->ellipse.point,
                (*i)->ellipse.point,
                pBrush,
                3.0f
            );
        }
        prev = *i;
    }

    // Draw second set of convex hulls
    QuickHullAlgorithm(group2, group2.size(), &hull2);
    pRenderTarget->DrawLine(
        hull2.front()->ellipse.point,
        hull2.back()->ellipse.point,
        pBrush,
        3.0f
    );
    prev = hull2.front();
    for (auto i = hull2.begin(); i != hull2.end(); ++i)
    {
        // Redraw lines of convex hull
        if (i != hull2.begin()) {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            pRenderTarget->DrawLine(
                prev->ellipse.point,
                (*i)->ellipse.point,
                pBrush,
                3.0f
            );
        }
        prev = *i;
    }

    MinkowskiSumAlgorithm(hull1, hull2, &hull3);
    QuickHullAlgorithm(hull3, hull3.size(), &hull4);
    pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));
    pRenderTarget->DrawLine(
        hull4.front()->ellipse.point,
        hull4.back()->ellipse.point,
        pBrush,
        3.0f
    );
    prev = hull4.front();
    for (auto i = hull4.begin(); i != hull4.end(); ++i)
    {
        // Redraw lines of convex hull
        if (i != hull4.begin()) {
            pRenderTarget->DrawLine(
                prev->ellipse.point,
                (*i)->ellipse.point,
                pBrush,
                3.0f
            );
        }
        prev = *i;
    }
}

void MainWindow::MinkowskiDifferenceDraw() {
    // Divide ellipses into two groups
    group1.clear();
    group2.clear();
    hull1.clear();
    hull2.clear();
    list<shared_ptr<MyEllipse>> hull3;
    list<shared_ptr<MyEllipse>> hull4;
    shared_ptr<MyEllipse> prev;

    for (auto i = ellipses.begin(); i != ellipses.end(); ++i) {
        if ((*i)->group == 1) {
            group1.push_back(*i);
        }
        else if ((*i)->group == 2) {
            group2.push_back(*i);
        }
    }

    // Draw first set of convex hulls
    QuickHullAlgorithm(group1, group1.size(), &hull1);
    pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
    pRenderTarget->DrawLine(
        hull1.front()->ellipse.point,
        hull1.back()->ellipse.point,
        pBrush,
        3.0f
    );
    prev = hull1.front();
    for (auto i = hull1.begin(); i != hull1.end(); ++i)
    {
        // Redraw lines of convex hull
        if (i != hull1.begin()) {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            pRenderTarget->DrawLine(
                prev->ellipse.point,
                (*i)->ellipse.point,
                pBrush,
                3.0f
            );
        }
        prev = *i;
    }

    // Draw second set of convex hulls
    QuickHullAlgorithm(group2, group2.size(), &hull2);
    pRenderTarget->DrawLine(
        hull2.front()->ellipse.point,
        hull2.back()->ellipse.point,
        pBrush,
        3.0f
    );
    prev = hull2.front();
    for (auto i = hull2.begin(); i != hull2.end(); ++i)
    {
        // Redraw lines of convex hull
        if (i != hull2.begin()) {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            pRenderTarget->DrawLine(
                prev->ellipse.point,
                (*i)->ellipse.point,
                pBrush,
                3.0f
            );
        }
        prev = *i;
    }

    MinkowskiDifferenceAlgorithm(hull1, hull2, &hull3);
    QuickHullAlgorithm(hull3, hull3.size(), &hull4);
    pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));
    pRenderTarget->DrawLine(
        hull4.front()->ellipse.point,
        hull4.back()->ellipse.point,
        pBrush,
        3.0f
    );
    prev = hull4.front();
    for (auto i = hull4.begin(); i != hull4.end(); ++i)
    {
        // Redraw lines of convex hull
        if (i != hull4.begin()) {
            pRenderTarget->DrawLine(
                prev->ellipse.point,
                (*i)->ellipse.point,
                pBrush,
                3.0f
            );
        }
        prev = *i;
    }
}

// Returns the side of point p with respect to line joining p1 and p2
int findSide(shared_ptr<MyEllipse> p1, shared_ptr<MyEllipse> p2, shared_ptr<MyEllipse> p) {
    int val = (p->ellipse.point.y - p1->ellipse.point.y) * (p2->ellipse.point.x - p1->ellipse.point.x) -
        (p2->ellipse.point.y - p1->ellipse.point.y) * (p->ellipse.point.x - p1->ellipse.point.x);

    if (val > 0)
        return 1;
    if (val < 0)
        return -1;
    return 0;
}

// Retruns whether or not a point is within a convex hull
bool convexHullContains(list<shared_ptr<MyEllipse>> hull, float x, float y) {
    if (hull.empty()) {
        return false;
    }
    shared_ptr<MyEllipse> prev = hull.front();
    shared_ptr<MyEllipse> ellipse = shared_ptr<MyEllipse>(new MyEllipse());
    ellipse->ellipse.point = D2D1::Point2F(x, y);
    for (auto i = ++(hull.begin()); i != hull.end(); ++i) {
        if (findSide(prev, *i, ellipse) <= 0) {
            return false;
        }
        prev = *i;
    }

    if (findSide(hull.back(), hull.front(), ellipse) <= 0)
        return false;

    return true;
}

void MainWindow::PointConvexHullDraw() {
    hull1.clear();
    shared_ptr<MyEllipse> prev;
    QuickHullAlgorithm(ellipses, ellipses.size() - 1, &hull1);
    pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
    pRenderTarget->DrawLine(
        hull1.front()->ellipse.point,
        hull1.back()->ellipse.point,
        pBrush,
        3.0f
    );
    prev = hull1.front();
    for (auto i = hull1.begin(); i != hull1.end(); ++i)
    {
        // Redraw Convex hull circles
        (*i)->ChangeColor(D2D1::ColorF(D2D1::ColorF::Blue));
        (*i)->Draw(pRenderTarget, pBrush);

        // Redraw lines of convex hull
        if (i != hull1.begin()) {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            pRenderTarget->DrawLine(
                prev->ellipse.point,
                (*i)->ellipse.point,
                pBrush,
                3.0f
            );
        }
        prev = *i;
    }
    if (convexHullContains(hull1, ellipses.back()->ellipse.point.x, ellipses.back()->ellipse.point.y))
        ellipses.back()->ChangeColor(D2D1::ColorF(D2D1::ColorF::Red));
    else
        ellipses.back()->ChangeColor(D2D1::ColorF(D2D1::ColorF::Blue));
    ellipses.back()->Draw(pRenderTarget, pBrush);
}

void MainWindow::GJKDraw() {
    // Divide ellipses into two groups
    group1.clear();
    group2.clear();
    hull1.clear();
    hull2.clear();
    list<shared_ptr<MyEllipse>> hull3;
    list<shared_ptr<MyEllipse>> hull4;
    shared_ptr<MyEllipse> prev;

    for (auto i = ellipses.begin(); i != ellipses.end(); ++i) {
        if ((*i)->group == 1) {
            group1.push_back(*i);
        }
        else if ((*i)->group == 2) {
            group2.push_back(*i);
        }
    }

    // Draw first set of convex hulls
    QuickHullAlgorithm(group1, group1.size(), &hull1);
    pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
    pRenderTarget->DrawLine(
        hull1.front()->ellipse.point,
        hull1.back()->ellipse.point,
        pBrush,
        3.0f
    );
    prev = hull1.front();
    for (auto i = hull1.begin(); i != hull1.end(); ++i)
    {
        // Redraw lines of convex hull
        if (i != hull1.begin()) {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            pRenderTarget->DrawLine(
                prev->ellipse.point,
                (*i)->ellipse.point,
                pBrush,
                3.0f
            );
        }
        prev = *i;
    }

    // Draw second set of convex hulls
    QuickHullAlgorithm(group2, group2.size(), &hull2);
    pRenderTarget->DrawLine(
        hull2.front()->ellipse.point,
        hull2.back()->ellipse.point,
        pBrush,
        3.0f
    );
    prev = hull2.front();
    for (auto i = hull2.begin(); i != hull2.end(); ++i)
    {
        // Redraw lines of convex hull
        if (i != hull2.begin()) {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            pRenderTarget->DrawLine(
                prev->ellipse.point,
                (*i)->ellipse.point,
                pBrush,
                3.0f
            );
        }
        prev = *i;
    }

    MinkowskiDifferenceAlgorithm(hull1, hull2, &hull3);
    QuickHullAlgorithm(hull3, hull3.size(), &hull4);

    if (convexHullContains(hull4, centerX, centerY))
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Green));
    else
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));

    pRenderTarget->DrawLine(
        hull4.front()->ellipse.point,
        hull4.back()->ellipse.point,
        pBrush,
        3.0f
    );
    prev = hull4.front();
    for (auto i = hull4.begin(); i != hull4.end(); ++i)
    {
        // Redraw lines of convex hull
        if (i != hull4.begin()) {
            pRenderTarget->DrawLine(
                prev->ellipse.point,
                (*i)->ellipse.point,
                pBrush,
                3.0f
            );
        }
        prev = *i;
    }
    
}

int lineDist(shared_ptr<MyEllipse> p1, shared_ptr<MyEllipse> p2, shared_ptr<MyEllipse> p) {
    return abs((p->ellipse.point.y - p1->ellipse.point.y) * (p2->ellipse.point.x - p1->ellipse.point.x) -
        (p2->ellipse.point.y - p1->ellipse.point.y) * (p->ellipse.point.x - p1->ellipse.point.x));
}

shared_ptr<MyEllipse> getValue(list<shared_ptr<MyEllipse>> ellipses, int n) {
    if (ellipses.empty() || n < 0 || n >= ellipses.size()) {
        return nullptr;
    }
    list<shared_ptr<MyEllipse>>::iterator it = ellipses.begin();
    advance(it, n);
    return *it;
}

bool Contains(list<shared_ptr<MyEllipse>> a, shared_ptr<MyEllipse> ellipse) {
    for (auto i = a.begin(); i != a.end(); ++i) {
        if (*i == ellipse) {
            return true;
        }
    }
    return false;
}

void quickHull(list<shared_ptr<MyEllipse>> a, int n, shared_ptr<MyEllipse> p1, shared_ptr<MyEllipse> p2, int side, list<shared_ptr<MyEllipse>> *hull) {
    int ind = -1;
    int max_dist = 0;

    // find point with max distance from line and also with side of line
    for (int i = 0; i < n; i++) {
        int temp = lineDist(p1, p2, getValue(a, i));
        if (findSide(p1, p2, getValue(a, i)) == side && temp > max_dist) {
            ind = i;
            max_dist = temp;
        }
    }

    // If not point is found, add end of line to convex hull
    if (ind == -1) {
        if (!Contains(*hull, p1))
            hull->push_back(p1);
        if (!Contains(*hull, p2))
            hull->push_back(p2);
        return;
    }

    quickHull(a, n, getValue(a, ind), p1, -findSide(getValue(a, ind), p1, p2), hull);
    quickHull(a, n, getValue(a, ind), p2, -findSide(getValue(a, ind), p2, p1), hull);

}

// Algorithm implementations
void MainWindow::QuickHullAlgorithm(list<shared_ptr<MyEllipse>> a, int n, list<shared_ptr<MyEllipse>> *hull) {
    if (n < 3) {
        return;
    }

    // Finding point with min and max x coordinate
    int min_x = 0;
    int max_x = 0;
    for (int i = 1; i < n; i++) {
        if (getValue(a, i)->ellipse.point.x < getValue(a, min_x)->ellipse.point.x) {
            min_x = i;
        }
        if (getValue(a, i)->ellipse.point.x > getValue(a, max_x)->ellipse.point.x) {
            max_x = i;
        }
    }

    // Recursively find convex hull points of both sides of line joining a[min_x] and a[max_x]
    quickHull(a, n, getValue(a, min_x), getValue(a, max_x), 1, hull);
    quickHull(a, n, getValue(a, min_x), getValue(a, max_x), -1, hull);

    innerPoint = hull->front();
    for (auto i = ++(hull->begin()); i != hull->end(); ++i) {
        if ((*i)->ellipse.point.y > innerPoint->ellipse.point.y) {
            innerPoint = *i;
        }
        else if ((*i)->ellipse.point.y == innerPoint->ellipse.point.y) {
            if ((*i)->ellipse.point.x < innerPoint->ellipse.point.x) {
                innerPoint = *i;
            }
        }
    }
    hull->sort(AngleComparison);
}


void MainWindow::MinkowskiSumAlgorithm(list<shared_ptr<MyEllipse>> group1, list<shared_ptr<MyEllipse>> group2, list<shared_ptr<MyEllipse>> *hull) {
    for (auto i = group1.begin(); i != group1.end(); ++i) {
        for (auto j = group2.begin(); j != group2.end(); ++j) {
            shared_ptr<MyEllipse> ellipse = *(hull->insert(
                hull->end(),
                shared_ptr<MyEllipse>(new MyEllipse())));

            ellipse->ellipse.point = D2D1::Point2F((float)((*i)->ellipse.point.x) + (float)((*j)->ellipse.point.x) - centerX, (float)((*i)->ellipse.point.y) + (float)((*j)->ellipse.point.y) - centerY);
            ellipse->ellipse.radiusX = ellipse->ellipse.radiusY = 0.0f;
            ellipse->color = D2D1::ColorF(D2D1::ColorF::Black);
            ellipse->group = 3;
        }
    }
}

void MainWindow::MinkowskiDifferenceAlgorithm(list<shared_ptr<MyEllipse>> group1, list<shared_ptr<MyEllipse>> group2, list<shared_ptr<MyEllipse>>* hull) {
    for (auto i = group1.begin(); i != group1.end(); ++i) {
        for (auto j = group2.begin(); j != group2.end(); ++j) {
            shared_ptr<MyEllipse> ellipse = *(hull->insert(
                hull->end(),
                shared_ptr<MyEllipse>(new MyEllipse())));

            ellipse->ellipse.point = D2D1::Point2F((float)((*j)->ellipse.point.x) - (float)((*i)->ellipse.point.x) + centerX, (float)((*j)->ellipse.point.y) - (float)((*i)->ellipse.point.y) + centerY);
            ellipse->ellipse.radiusX = ellipse->ellipse.radiusY = 0.0f;
            ellipse->color = D2D1::ColorF(D2D1::ColorF::Black);
            ellipse->group = 3;
        }
    }
}

void MainWindow::PointConvexHullAlgorithm(list<shared_ptr<MyEllipse>> ellipses) {
    
}

void MainWindow::GJKAlgorithm(list<shared_ptr<MyEllipse>> ellipses) {

}


void MainWindow::Resize()
{
    if (pRenderTarget != NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        pRenderTarget->Resize(size);

        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::OnLButtonDown(int pixelX, int pixelY, DWORD flags)
{
    const float dipX = DPIScale::PixelsToDipsX(pixelX);
    const float dipY = DPIScale::PixelsToDipsY(pixelY);

    /*
    if (mode == DrawMode)
    {
        POINT pt = { pixelX, pixelY };

        if (DragDetect(m_hwnd, pt))
        {
            SetCapture(m_hwnd);
        
            // Start a new ellipse.
            InsertEllipse(dipX, dipY);
        }
    }
    */
        ClearSelection();

        // Select a ellipse to move
        if (HitTest(dipX, dipY))
        {
            SetCapture(m_hwnd);

            ptMouse = Selection()->ellipse.point;
            ptMouse.x -= dipX;
            ptMouse.y -= dipY;

            SetMode(DragMode);
        }
        else if (convexHullContains(hull1, pixelX, pixelY))
        {
            SetCapture(m_hwnd);
              
            if (screen == QuickHull)
                group = 0;
            else
                group = 1;
            ptMouse = D2D1::Point2F(pixelX, pixelY);

            SetMode(DragMode);
        }
        else if (convexHullContains(hull2, pixelX, pixelY))
        {
            SetCapture(m_hwnd);

            group = 2;
            ptMouse = D2D1::Point2F(pixelX, pixelY);

            SetMode(DragMode);
        }
        else {
            group = -1;
        }

    InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::OnLButtonUp()
{
    /*
    if ((mode == DrawMode) && Selection())
    {
        ClearSelection();
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
    */
    if (mode == DragMode)
    {
        SetMode(SelectMode);
    }
    ReleaseCapture(); 
}


void MainWindow::OnMouseMove(int pixelX, int pixelY, DWORD flags)
{
    const float dipX = DPIScale::PixelsToDipsX(pixelX);
    const float dipY = DPIScale::PixelsToDipsY(pixelY);

    if ((flags & MK_LBUTTON))
    { 
        if (Selection()) {
            /*
            if (mode == DrawMode)
            {
                // Resize the ellipse.
                const float width = (dipX - ptMouse.x) / 2;
                const float height = (dipY - ptMouse.y) / 2;
                const float x1 = ptMouse.x + width;
                const float y1 = ptMouse.y + height;

                Selection()->ellipse = D2D1::Ellipse(D2D1::Point2F(x1, y1), width, height);
            }
            */
            if (mode == DragMode)
            {
                // Move the ellipse.
                Selection()->ellipse.point.x = dipX + ptMouse.x;
                Selection()->ellipse.point.y = dipY + ptMouse.y;
            }
        }
        else if (group == 0) {
            for (auto i = ellipses.begin(); i != ellipses.end(); ++i) {
                (*i)->ellipse.point.x += pixelX - ptMouse.x;
                (*i)->ellipse.point.y += pixelY - ptMouse.y;
            }
            ptMouse = D2D1::Point2F(pixelX, pixelY);
        }
        else if (group == 1) {
            for (auto i = group1.begin(); i != group1.end(); ++i) {
                (*i)->ellipse.point.x += pixelX - ptMouse.x;
                (*i)->ellipse.point.y += pixelY - ptMouse.y;
            }
            ptMouse = D2D1::Point2F(pixelX, pixelY);
        }
        else if (group == 2) {
            for (auto i = group2.begin(); i != group2.end(); ++i) {
                (*i)->ellipse.point.x += pixelX - ptMouse.x;
                (*i)->ellipse.point.y += pixelY - ptMouse.y;
            }
            ptMouse = D2D1::Point2F(pixelX, pixelY);
        }
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}


void MainWindow::OnKeyDown(UINT vkey)
{
    switch (vkey)
    {
    case VK_BACK:
    case VK_DELETE:
        if ((mode == SelectMode) && Selection())
        {
            ellipses.erase(selection);
            ClearSelection();
            SetMode(SelectMode);
            InvalidateRect(m_hwnd, NULL, FALSE);
        };
        break;

    case VK_LEFT:
        MoveSelection(-1, 0);
        break;

    case VK_RIGHT:
        MoveSelection(1, 0);
        break;

    case VK_UP:
        MoveSelection(0, -1);
        break;

    case VK_DOWN:
        MoveSelection(0, 1);
        break;
    }
}

HRESULT MainWindow::InsertEllipse(float x, float y, float radius, D2D1::ColorF color, int group)
{
    try
    {
        selection = ellipses.insert(
            ellipses.end(), 
            shared_ptr<MyEllipse>(new MyEllipse()));

        Selection()->ellipse.point = ptMouse = D2D1::Point2F(x, y);
        Selection()->ellipse.radiusX = Selection()->ellipse.radiusY = radius; 
        Selection()->color = color;
        Selection()->group = group;
    }
    catch (std::bad_alloc)
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}


BOOL MainWindow::HitTest(float x, float y)
{
    for (auto i = ellipses.rbegin(); i != ellipses.rend(); ++i)
    {
        if ((*i)->HitTest(x, y))
        {
            selection = (++i).base();
            return TRUE;
        }
    }
    return FALSE;
}

void MainWindow::MoveSelection(float x, float y)
{
    if ((mode == SelectMode) && Selection())
    {
        Selection()->ellipse.point.x += x;
        Selection()->ellipse.point.y += y;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::SetMode(Mode m)
{
    mode = m;

    LPWSTR cursor;
    switch (mode)
    {
    case SelectMode:
        cursor = IDC_HAND;
        break;

    case DragMode:
        cursor = IDC_SIZEALL;
        break;
    }

    hCursor = LoadCursor(NULL, cursor);
    SetCursor(hCursor);
}

// All button funtions clears all previous circles and creates new circles for given algorithm
// Initializes circles for quick hull
void MainWindow::QuickHullButton() {
    RECT rect;
    GetWindowRect(m_hwnd, &rect);
    screen = QuickHull;
    ellipses.clear();

    for (int i = 0; i < 15; i++) {
        float xcoord = rand() % (rect.right - rect.left - 300) + 250;
        float ycoord = rand() % (rect.bottom - rect.top - 150) + 50;
        InsertEllipse(xcoord, ycoord, 10.0f, D2D1::ColorF(D2D1::ColorF::Red), 1);
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}

// Initializes circles for Minkowski Sum
void MainWindow::MinkowskiSumButton() {
    RECT rect;
    screen = MinkowskiSum;
    GetWindowRect(m_hwnd, &rect);
    ellipses.clear();

    // Convex hull for group 1
    for (int i = 0; i < 6; i++) {
        float xcoord = rand() % ((rect.right - rect.left - 300)/2) + 250;
        float ycoord = rand() % ((rect.bottom - rect.top - 150)/2) + 50;
        InsertEllipse(xcoord, ycoord, 10.0f, D2D1::ColorF(D2D1::ColorF::Red), 1);
    }

    // Convex hull for group 2
    for (int i = 0; i < 6; i++) {
        float xcoord = rand() % ((rect.right - rect.left - 300)/2) + 250 + (rect.right - rect.left - 300) / 2;
        float ycoord = rand() % ((rect.bottom - rect.top - 150)/2) + 50 + (rect.bottom - rect.top - 150) / 2;
        InsertEllipse(xcoord, ycoord, 10.0f, D2D1::ColorF(D2D1::ColorF::Blue), 2);
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}

// Initializes circles for Minkowski Difference
void MainWindow::MinkowskiDifferenceButton() {
    RECT rect;
    screen = MinkowskiDifference;
    GetWindowRect(m_hwnd, &rect);
    ellipses.clear();

    // Convex hull for group 1
    for (int i = 0; i < 6; i++) {
        float xcoord = rand() % ((rect.right - rect.left - 300) / 2) + 250;
        float ycoord = rand() % ((rect.bottom - rect.top - 150) / 2) + 50;
        InsertEllipse(xcoord, ycoord, 10.0f, D2D1::ColorF(D2D1::ColorF::Red), 1);
    }

    // Convex hull for group 2
    for (int i = 0; i < 6; i++) {
        float xcoord = rand() % ((rect.right - rect.left - 300) / 2) + 250 + (rect.right - rect.left - 300) / 2;
        float ycoord = rand() % ((rect.bottom - rect.top - 150) / 2) + 50 + (rect.bottom - rect.top - 150) / 2;
        InsertEllipse(xcoord, ycoord, 10.0f, D2D1::ColorF(D2D1::ColorF::Blue), 2);
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}

// Initializes circles for Point Convex Hull
void MainWindow::PointConvexHullButton() {
    RECT rect;
    screen = PointConvexHull;
    GetWindowRect(m_hwnd, &rect);
    ellipses.clear();

    for (int i = 0; i < 15; i++) {
        float xcoord = rand() % (rect.right - rect.left - 300) + 250;
        float ycoord = rand() % (rect.bottom - rect.top - 150) + 50;
        InsertEllipse(xcoord, ycoord, 0.0f, D2D1::ColorF(D2D1::ColorF::Red), 1);
    }

    InsertEllipse(centerX, centerY, 10.0f, D2D1::ColorF(D2D1::ColorF::Red), 0);

    InvalidateRect(m_hwnd, NULL, FALSE);
}

// Initializes circles for GJK
void MainWindow::GJKButton() {
    RECT rect;
    screen = GJK;
    GetWindowRect(m_hwnd, &rect);
    ellipses.clear();

    // Convex hull for group 1
    for (int i = 0; i < 6; i++) {
        float xcoord = rand() % ((rect.right - rect.left - 300) / 2) + 250;
        float ycoord = rand() % ((rect.bottom - rect.top - 150) / 2) + 50;
        InsertEllipse(xcoord, ycoord, 10.0f, D2D1::ColorF(D2D1::ColorF::Red), 1);
    }

    // Convex hull for group 2
    for (int i = 0; i < 6; i++) {
        float xcoord = rand() % ((rect.right - rect.left - 300) / 2) + 250 + (rect.right - rect.left - 300) / 2;
        float ycoord = rand() % ((rect.bottom - rect.top - 150) / 2) + 50 + (rect.bottom - rect.top - 150) / 2;
        InsertEllipse(xcoord, ycoord, 10.0f, D2D1::ColorF(D2D1::ColorF::Blue), 2);
    }
    InvalidateRect(m_hwnd, NULL, FALSE);
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    MainWindow win;

    if (!win.Create(L"Draw Circles", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN))
    {
        return 0;
    }

    HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCEL1));

    ShowWindow(win.Window(), nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(win.Window(), hAccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
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

        CreateButtons();

        DPIScale::Initialize(pFactory);
        SetMode(SelectMode);
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

    case WM_COMMAND:
        // Change page on associated button press
        if (LOWORD(wParam) == BTN_QUICK_HULL) {
            QuickHullButton();
        }
        else if (LOWORD(wParam) == BTN_MINKOWSKI_SUM) {
            MinkowskiSumButton();
        }
        else if (LOWORD(wParam) == BTN_MINKOWSKI_DIFFERENCE) {
            MinkowskiDifferenceButton();
        }
        else if (LOWORD(wParam) == BTN_POINT_CONVEX_HULL) {
            PointConvexHullButton();
        }
        else if (LOWORD(wParam) == BTN_GJK) {
            GJKButton();
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            SetCursor(hCursor);
            return TRUE;
        }
        break;

    case WM_KEYDOWN:
        OnKeyDown((UINT)wParam);
        return 0;
    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void MainWindow::CreateButtons() {
    // Create a test button
    HWND QuickHullButton = CreateWindow(
        L"BUTTON",          // Predefined class; Unicode assumed 
        L"Quick Hull",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        10,         // x position 
        10,         // y position 
        200,        // Button width
        20,        // Button height
        m_hwnd,     // Parent window
        (HMENU) BTN_QUICK_HULL,       // No menu.
        (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND MinkowskiSumButton = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Minkowski Sum",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        10,         // x position 
        30,         // y position 
        200,        // Button width
        20,        // Button height
        m_hwnd,     // Parent window
        (HMENU) BTN_MINKOWSKI_SUM,       // No menu.
        (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND MinkowskiDifferenceButton = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Minkowski Difference",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        10,         // x position 
        50,         // y position 
        200,        // Button width
        20,        // Button height
        m_hwnd,     // Parent window
        (HMENU) BTN_MINKOWSKI_DIFFERENCE,       // No menu.
        (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND PointConvexHullButton = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"Point Convex Hull",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        10,         // x position 
        70,         // y position 
        200,        // Button width
        20,        // Button height
        m_hwnd,     // Parent window
        (HMENU) BTN_POINT_CONVEX_HULL,       // No menu.
        (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND GJKButton = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"GJK",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        10,         // x position 
        90,         // y position 
        200,        // Button width
        20,        // Button height
        m_hwnd,     // Parent window
        (HMENU) BTN_GJK,       // No menu.
        (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.
}


