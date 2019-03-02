#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "ModelApp.h"

const int WINDOW_WIDTH = 640;
const int WINDOW_HEIGHT = 480;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
  PAINTSTRUCT ps;
  HDC hdc;
  switch (msg)
  {
  case WM_PAINT:
    hdc = BeginPaint(hWnd, &ps);
    EndPaint(hWnd, &ps);
    break;

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hWnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
  ModelApp theApp{};
  CoInitializeEx(NULL, COINIT_MULTITHREADED);

  WNDCLASSEX wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = L"HelloDirectX12";
  RegisterClassEx(&wc);

  DWORD dwStyle = WS_OVERLAPPEDWINDOW & ~WS_SIZEBOX;
  RECT rect = { 0,0, WINDOW_WIDTH, WINDOW_HEIGHT };
  AdjustWindowRect(&rect, dwStyle, FALSE);

  auto hwnd = CreateWindow(wc.lpszClassName, L"DrawModel",
    dwStyle,
    CW_USEDEFAULT, CW_USEDEFAULT,
    rect.right - rect.left, rect.bottom - rect.top,
    nullptr,
    nullptr,
    hInstance,
    &theApp
  );
  try
  {
    theApp.Initialize(hwnd);

    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&theApp));
    ShowWindow(hwnd, nCmdShow);

    MSG msg{};
    while (msg.message != WM_QUIT)
    {
      if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
      theApp.Render();
    }

    theApp.Terminate();
    return msg.wParam;
  }
  catch (std::runtime_error e)
  {
    OutputDebugStringA(e.what());
    OutputDebugStringA("\n");
    DebugBreak();
  }
  return 0;
}
