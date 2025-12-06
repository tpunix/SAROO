
#define WINVER 0x0500

#include <windows.h>
#include <wingdi.h>

#include <stdio.h>
#include <unistd.h>


/*************************************************************/

static int width;
static int height;
unsigned char *gdifbuf;
static volatile int gdifb_ready;

static HINSTANCE hInst;
static HBITMAP hbmp;
static HDC hmemdc;

static HANDLE h_gdifb;
static HANDLE h_window;
static DWORD gdifb_id;
static HWND gdifb;

static ATOM MyRegisterClass(void);
static BOOL InitInstance(void);
static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

static volatile int vkey;

/*************************************************************/

DWORD WINAPI gdifb_main(LPVOID lpParam)
{
	MSG msg;

	hInst = GetModuleHandle(0);
	MyRegisterClass();
	if (!InitInstance ()) {
		return FALSE;
	}

	gdifb_ready = 1;

	// Main message loop:
	while(GetMessage(&msg, NULL, 0, 0)){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

ATOM MyRegisterClass()
{
	WNDCLASS	wc;

	wc.style         = CS_HREDRAW|CS_VREDRAW;
	wc.lpfnWndProc   = (WNDPROC)WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInst;
	wc.hIcon         = NULL;
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = "GDI_FB";

	return RegisterClass(&wc);
}


void memdc_create(HWND hwnd)
{
	BITMAPINFOHEADER bh;
	HDC hdc;

	memset(&bh, 0, sizeof(bh));
	bh.biSize = sizeof(bh);
	bh.biHeight = -(height+256);
	//bh.biWidth = width;
	bh.biWidth = 2048;
	bh.biPlanes = 1;
	bh.biBitCount = 32;
	bh.biCompression = BI_RGB;

	hdc = GetDC(hwnd);
	hbmp = CreateDIBSection (hdc, (LPBITMAPINFO)&bh, DIB_RGB_COLORS, (void**)&gdifbuf, NULL, 0) ;
	hmemdc = CreateCompatibleDC(hdc);
	SelectObject(hmemdc, hbmp);
	ReleaseDC(hwnd, hdc);
}


BOOL InitInstance()
{
	DWORD style = WS_OVERLAPPEDWINDOW&~WS_SIZEBOX&~WS_MAXIMIZEBOX;
	RECT rect;
       
	rect.left = 0;
	rect.top = 0;
	rect.right = width;
	rect.bottom = height;
	AdjustWindowRect(&rect, style, FALSE);

	h_window = CreateWindow("GDI_FB", "GDI_FB", style, CW_USEDEFAULT, 0,
		rect.right-rect.left, rect.bottom-rect.top,
		0, 0, hInst, 0);

	if (!h_window){
		return FALSE;
	}

	memdc_create(h_window);

	ShowWindow(h_window, SW_SHOWDEFAULT);
	UpdateWindow(h_window);

	return TRUE;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;

	switch (message) 
	{
		case WM_PAINT:
			hdc = BeginPaint(hWnd, &ps);
			BitBlt(hdc, 0, 0, width, height, hmemdc, 0, 0, SRCCOPY);
			EndPaint(hWnd, &ps);
			break;
		case WM_KEYDOWN:
			vkey = wParam;
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}


/*************************************************************/

void gdifb_textout(int x, int y, char *text)
{
	TextOut(hmemdc, x, y, text, strlen(text));
}


void gdifb_pixel(int x, int y, int color)
{
	*(DWORD*)(gdifbuf + (y*2048 + x)*4) = color;
}


void gdifb_line(int x1, int y1, int x2, int y2, int color)
{
	HPEN pen, old_pen;

	pen = CreatePen(PS_SOLID, 1, color);
	old_pen = SelectObject(hmemdc, pen);

	MoveToEx(hmemdc, x1, y1, NULL);
	LineTo(hmemdc, x2, y2);
	SetPixel(hmemdc, x2, y2, color);

	SelectObject(hmemdc, old_pen);
}


void gdifb_rect(int x1, int y1, int x2, int y2, int color)
{
	HPEN pen, old_pen;

	pen = CreatePen(PS_SOLID, 1, color);
	old_pen = SelectObject(hmemdc, pen);

	MoveToEx(hmemdc, x1, y1, NULL);
	LineTo(hmemdc, x2, y1);
	LineTo(hmemdc, x2, y2);
	LineTo(hmemdc, x1, y2);
	LineTo(hmemdc, x1, y1);

	SelectObject(hmemdc, old_pen);
}


void gdifb_box(int x1, int y1, int x2, int y2, int color)
{
	HPEN pen, old_pen;
	HBRUSH brush;
	RECT rect;

	pen = CreatePen(PS_SOLID, 1, color);
	old_pen = SelectObject(hmemdc, pen);
	brush = CreateSolidBrush(color);

	rect.left = x1;
	rect.right = x2+1;
	rect.top = y1;
	rect.bottom = y2+1;
	FillRect(hmemdc, &rect, brush);

	SelectObject(hmemdc, old_pen);
}


void gdifb_flush(void)
{
	InvalidateRect(gdifb, NULL, TRUE);
}


int gdifb_waitkey(void)
{
	vkey = 0;
	while(vkey==0);
	return vkey;
}

/*************************************************************/


int gdifb_init(int w, int h)
{
	width = w;
	height = h;
	gdifb_ready = 0;

	h_gdifb = CreateThread(NULL, 0, gdifb_main, NULL, 0, &gdifb_id);
	while(1){
		gdifb = FindWindow(NULL, "GDI_FB");
		if(gdifb)
			break;
	}

	while(gdifb_ready==0);
	return 0;
}


int gdifb_exit(int force)
{
	if(force){
		SendMessage(h_window, WM_CLOSE, 0, 0);
	}

	WaitForSingleObject(h_gdifb,INFINITE);
	CloseHandle(h_gdifb);

	return 0;
}

/*************************************************************/

#ifdef TEST_GDIFB

int main(int argc, char *argv[])
{
	gdifb_init(320, 240);

	gdifb_textout(48, 48, "gdifb_test!");

	int i;
	for(i=0; i<100; i++){
		gdifb_pixel(20+i, 200, 0xff00ff);
	}

	gdifb_line(10, 10, 100, 100, 0x00FF0000);
	gdifb_rect(5, 5, 300, 235, 0x000000FF);
	gdifb_box(120, 120, 140, 140, 0x0000FF00);

	gdifb_flush();

	gdifb_exit();
	return 0;
}

#endif

/*************************************************************/

