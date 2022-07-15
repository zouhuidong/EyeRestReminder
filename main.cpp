#include <Windows.h>
#include <WinUser.h>
#include <easyx.h>
#include "EasyWin32.h"
#include "ini.hpp"
#include "resource.h"

#pragma comment(lib,"winmm.lib")

#define APP_NAME L"EyeRestReminder"
#define INI_PATH L"./settings.ini"

#define IDC_MENU_ABOUT 101

// �洢������Ļ�Ĵ�С��Ϣ������ʾ����
struct ScreenSize
{
	int left;	// ����ʾ�������Ͻ� x ����
	int top;	// ����ʾ�������Ͻ� y ����
	int w;		// ����ʾ�����ܺͿ��
	int h;		// ����ʾ�����ܺ͸߶�
};

// ��ȡ����ʾ����С��Ϣ
ScreenSize GetScreenSize()
{
	int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
	int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
	int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	return { left,top,w,h };
}

ScreenSize g_sizeScreen;
RECT g_rctWnd;

int g_nWorkMinutes;		// ����ʱ��
int g_nRestMinutes;		// ��Ϣʱ��
int g_nLeaveMinutes;	// �뿪�ж�ʱ��

IMAGE g_imgIcon;
IMAGE g_imgBackground;

/*
 *    �ο��� http://tieba.baidu.com/p/5218523817
 *    ����˵����pImg ��ԭͼָ�룬width1 �� height1 ��Ŀ��ͼƬ�ĳߴ硣
 *    �������ܣ���ͼƬ�������ţ�����Ŀ��ͼƬ�������Զ����ߣ�Ҳ����ֻ����ȣ������Զ�����߶�
 *    ����Ŀ��ͼƬ
*/
IMAGE zoomImage(IMAGE* pImg, int newWidth, int newHeight = 0)
{
	// ��ֹԽ��
	if (newWidth < 0 || newHeight < 0) {
		newWidth = pImg->getwidth();
		newHeight = pImg->getheight();
	}

	// ������ֻ��һ��ʱ�������Զ�����
	if (newHeight == 0) {
		// �˴���Ҫע����*��/����Ȼ��Ŀ��ͼƬС��ԭͼʱ�����
		newHeight = newWidth * pImg->getheight() / pImg->getwidth();
	}

	// ��ȡ��Ҫ�������ŵ�ͼƬ
	IMAGE newImg(newWidth, newHeight);

	// �ֱ��ԭͼ���Ŀ��ͼ���ȡָ��
	DWORD* oldDr = GetImageBuffer(pImg);
	DWORD* newDr = GetImageBuffer(&newImg);

	// ��ֵ ʹ��˫���Բ�ֵ�㷨
	for (int i = 0; i < newHeight - 1; i++) {
		for (int j = 0; j < newWidth - 1; j++) {
			int t = i * newWidth + j;
			int xt = j * pImg->getwidth() / newWidth;
			int yt = i * pImg->getheight() / newHeight;
			newDr[i * newWidth + j] = oldDr[xt + yt * pImg->getwidth()];
			// ʵ�����м���ͼƬ
			byte r = (GetRValue(oldDr[xt + yt * pImg->getwidth()]) +
				GetRValue(oldDr[xt + yt * pImg->getwidth() + 1]) +
				GetRValue(oldDr[xt + (yt + 1) * pImg->getwidth()]) +
				GetRValue(oldDr[xt + (yt + 1) * pImg->getwidth() + 1])) / 4;
			byte g = (GetGValue(oldDr[xt + yt * pImg->getwidth()]) +
				GetGValue(oldDr[xt + yt * pImg->getwidth()] + 1) +
				GetGValue(oldDr[xt + (yt + 1) * pImg->getwidth()]) +
				GetGValue(oldDr[xt + (yt + 1) * pImg->getwidth()]) + 1) / 4;
			byte b = (GetBValue(oldDr[xt + yt * pImg->getwidth()]) +
				GetBValue(oldDr[xt + yt * pImg->getwidth()] + 1) +
				GetBValue(oldDr[xt + (yt + 1) * pImg->getwidth()]) +
				GetBValue(oldDr[xt + (yt + 1) * pImg->getwidth() + 1])) / 4;
			newDr[i * newWidth + j] = RGB(r, g, b);
		}
	}

	return newImg;
}

// ��ʼ��
void Init()
{
	g_nWorkMinutes = GetIniFileInfoInt(INI_PATH, L"settings", L"work_min", 40);
	g_nRestMinutes = GetIniFileInfoInt(INI_PATH, L"settings", L"rest_min", 3);
	g_nLeaveMinutes = GetIniFileInfoInt(INI_PATH, L"settings", L"leave_min", 3);

	loadimage(&g_imgIcon, L"MYICON", MAKEINTRESOURCE(IDB_ICON1));
	loadimage(&g_imgBackground, L"./bk.jpg");
}

// �����½�����Ļ�⣬�����ش��ڣ���ԭλ��
void WindowDown(HWND hWnd, ScreenSize size)
{
	RECT rct;
	GetWindowRect(hWnd, &rct);
	for (int i = 0; i < size.top + size.h - rct.top; i++)
	{
		SetWindowPos(hWnd, HWND_TOP, rct.left, rct.top + i, 0, 0, SWP_NOSIZE);
		Sleep(5);
	}
}

void OnMessage(HWND hWnd)
{
	static bool flag = false;

	if (!flag)
	{
		flag = true;
		ShowWindow(hWnd, SW_SHOW);
		SetWindowPos(hWnd, HWND_TOPMOST, g_rctWnd.left, g_rctWnd.top, 0, 0, SWP_NOSIZE);
		Sleep(5000);
		WindowDown(hWnd, g_sizeScreen);
		ShowWindow(hWnd, SW_HIDE);
		SetWindowPos(hWnd, NULL, g_rctWnd.left, g_rctWnd.top, 0, 0, SWP_NOSIZE);
		flag = false;
	}
}

bool WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, HINSTANCE hInstance)
{
	switch (msg)
	{
		// ����������Ϣ
	case WM_TRAY:
		switch (lParam)
		{
			// ���ʱ��ʾ����
		case WM_LBUTTONDOWN:
			std::thread(OnMessage, hWnd).detach();
			break;
		}
		break;

	case WM_CLOSE:
		return false;
		break;
	}
	return true;
}

// �����ײ�����
HWND CreateBottomWindow(int _w, int _h, ScreenSize size)
{
	int w = size.w + size.left;
	int h = size.h + size.top;
	EasyWin32::PreSetWindowPos(w - _w, h - _h);
	EasyWin32::PreSetWindowStyle(WS_POPUP);
	return EasyWin32::initgraph_win32(_w, _h, EW_NORMAL, APP_NAME, WndProc);
}

// ������ʾ��
void Music(bool flagReverse = false)
{
	int music[] = { 79, 83 };
	if (flagReverse)
	{
		music[0] = 83;
		music[1] = 79;
	}
	HMIDIOUT handle;
	midiOutOpen(&handle, 0, 0, 0, CALLBACK_NULL);
	midiOutShortMsg(handle, (0x007f << 16) + (music[0] << 8) + 0x90);
	Sleep(400);
	midiOutShortMsg(handle, (0x007f << 16) + (music[1] << 8) + 0x90);
	Sleep(5000);
	midiOutClose(handle);
}

void TopAlways(HWND hWnd)
{
	while (EasyWin32::isAliveWindow(hWnd))
	{
		SetWindowPos(hWnd, HWND_TOPMOST, g_sizeScreen.left, g_sizeScreen.top, 0, 0, SWP_NOSIZE);
		Sleep(50);
	}
}

void outtextxy_shadow(int x, int y, LPCTSTR lpText, COLORREF cShadow)
{
	COLORREF c = gettextcolor();
	settextcolor(cShadow);
	outtextxy(x, y, lpText);
	settextcolor(c);
	outtextxy(x + 1, y + 1, lpText);
}

// ������Ϣ
void Rest()
{
	// �����������
	EasyWin32::PreSetWindowPos(g_sizeScreen.left, g_sizeScreen.top);
	EasyWin32::PreSetWindowStyle(WS_POPUP | WS_EX_TOPMOST);
	HWND hWnd = EasyWin32::initgraph_win32(
		g_sizeScreen.w, g_sizeScreen.h, EW_NORMAL, L"",
		[](HWND, UINT msg, WPARAM, LPARAM, HINSTANCE)->bool {
			static HHOOK hook;
			switch (msg)
			{
			case WM_CREATE:
				ShowCursor(false);
				hook = SetWindowsHookEx(
					WH_KEYBOARD_LL,
					[](int, WPARAM, LPARAM)->LRESULT {
						return 1;
					},
					GetModuleHandle(0), 0);
				break;
			case WM_DESTROY:	UnhookWindowsHookEx(hook);	break;
			}
			return true;
		});
	RECT rctCursor = { 0,0,1,1 };
	ClipCursor(&rctCursor);
	std::thread(TopAlways, hWnd).detach();

	IMAGE imgBk = zoomImage(&g_imgBackground, g_sizeScreen.w, g_sizeScreen.h);
	setbkmode(TRANSPARENT);

	for (int i = 0; i < 60 * g_nRestMinutes; i++)
	{
		BEGIN_TASK_WND(hWnd);
		{
			setorigin(0, 0);
			putimage(0, 0, &imgBk);
			putimage(g_sizeScreen.w - g_imgIcon.getwidth() - 10, g_sizeScreen.h - g_imgIcon.getheight() - 10, &g_imgIcon);
			setorigin(-g_sizeScreen.left, -g_sizeScreen.top);
			settextcolor(BLACK);
			settextstyle(128, 0, L"΢���ź�");
			outtextxy(100, 100, L"Please take a break.");

			settextcolor(YELLOW);
			settextstyle(36, 0, L"Consolas");
			outtextxy_shadow(100, 220, L"Take a break, for better work.", BLACK);
			outtextxy_shadow(100, 270, L"Please stand up and walk around, and see the scenery outside the window.", BLACK);

			WCHAR buf[32] = { 0 };
			wsprintf(buf, L"%d", g_nRestMinutes - i / 60);

			settextcolor(DARKYELLOW);
			settextstyle(128, 0, L"Consolas");
			outtextxy(200, 360, buf);
			settextcolor(CLASSICGRAY);
			settextstyle(72, 0, L"Consolas");
			outtextxy(280, 400, L"minutes");
			settextcolor(CLASSICGRAY);
			settextstyle(32, 0, L"Consolas");
			outtextxy(200, 480, L"to unlock.");
		}
		END_TASK();
		FLUSH_DRAW();

		Sleep(1000);
	}

	ClipCursor(NULL);
	EasyWin32::closegraph_win32(hWnd);
	Music(true);
}

void OnMenu(UINT uId)
{
	static HWND hWnd = NULL;

	// "����" ��ť
	if (uId == IDC_MENU_ABOUT)
	{
		// ��ǰһ�����ڻ�δ�ر�
		if (hWnd && EasyWin32::isAliveWindow(hWnd))
		{
			SetForegroundWindow(hWnd);
			return;
		}
		else
		{
			hWnd = EasyWin32::initgraph_win32(480, 200, EW_NOMINIMIZE, L"About EyeRestReminder");
			BEGIN_TASK_WND(hWnd);
			{
				setbkcolor(SKYBLUE);
				cleardevice();
				settextcolor(BLACK);
				putimage(20, 20, &g_imgIcon);
				settextstyle(40, 0, L"Arial Black");
				outtextxy(110, 10, L"Eye Rest Reminder");
				settextstyle(18, 0, L"system");
				outtextxy(110, 60, L"Author: huidong <mailhuid@163.com>");
				outtextxy(110, 80, L"Website: huidong.xyz");
				outtextxy(110, 100, L"Graphics Library: EasyX & EasyWin32");
				outtextxy(110, 130, L"2022.07.14  dedicated to my friends.");
				outtextxy(110, 150, L"To be updated...");
			}
			END_TASK();
			FLUSH_DRAW();
		}
	}
}

int main()
{
	Init();
	g_sizeScreen = GetScreenSize();
	EasyWin32::SetCustomIcon(IDI_ICON1, IDI_ICON1);
	int w = 340, h = 140;
	HWND hWnd = CreateBottomWindow(w, h, g_sizeScreen);
	Sleep(100);
	GetWindowRect(hWnd, &g_rctWnd);

	// �������̣��󶨲˵�
	HMENU hMenu = CreatePopupMenu();
	AppendMenu(hMenu, MF_STRING, IDC_MENU_ABOUT, L"About EyeRestReminder...");
	EasyWin32::CreateTray(APP_NAME);
	EasyWin32::SetTrayMenu(hMenu);
	EasyWin32::SetTrayMenuProcFunc(OnMenu);

	EasyWin32::BeginTask();
	setbkcolor(DEEPSKYBLUE);
	setbkmode(TRANSPARENT);
	cleardevice();
	putimage(20, 20, &g_imgIcon);
	settextcolor(PINKWHITE);
	settextstyle(30, 0, L"Consolas");
	outtextxy(100, 25, L"Your eyes are");
	settextcolor(TOMATO);
	settextstyle(52, 0, L"Consolas");
	outtextxy(100, 60, L"protected.");
	EasyWin32::EndTask();
	EasyWin32::EnforceRedraw();

	OnMessage(hWnd);
	int i = 0;
	clock_t cRecord = clock();				// ��ʼ������ʱ��
	clock_t cStayRecord = clock();			// ͣ����ʱ��
	POINT ptCursorOld = { 0,0 }, ptCursor;	// ��¼���λ��
	bool flagStay = false;					// ������ͣ��
	bool flagWelcome = false;				// ��Ǹոջ���
	clock_t cWelcomeRecord;					// ��ʾ��ӭ���ֵ�ʱ��
	int nLeaveTime;							// �뿪ʱ��
	bool flagPreTip = false;				// ��ǽ���Ԥ����
	clock_t cPreTipRecord;					// ��ʾԤ�������ֵ�ʱ��
	bool flagAlreadyPreTip = false;			// ����Ѿ�Ԥ���ѹ�
	while (true)
	{
		clock_t cNow = clock();

		// ��겻������ʼ��ʱ��ʱ��������ֵ����ж�Ϊ�뿪
		GetCursorPos(&ptCursor);
		if (ptCursor.x == ptCursorOld.x && ptCursor.y == ptCursorOld.y)
		{
			if (!flagStay)
			{
				flagStay = true;
				cStayRecord = clock();
			}
		}
		else
		{
			flagStay = false;
			ptCursorOld = ptCursor;
		}

		int nWorkTime		// ����ʱ��
			= (cNow - cRecord) / CLOCKS_PER_SEC / 60;
		int nStayTime		// ���ͣ��ʱ��
			= (cNow - cStayRecord) / CLOCKS_PER_SEC / 60;

		// ����ʱ�䳬���Ҫ��Ϣ
		if (nWorkTime >= g_nWorkMinutes)
		{
			std::thread(Music, false).detach();
			Sleep(2000);
			Rest();

			// ��Ϣ�ѽ������ȴ�����
			while (ptCursor.x == ptCursorOld.x && ptCursor.y == ptCursorOld.y)
			{
				GetCursorPos(&ptCursor);
				Sleep(500);
			}

			// ���¼�ʱ����ʾ��ӭ����
			nLeaveTime = (clock() - cNow) / CLOCKS_PER_SEC / 60 + g_nRestMinutes;
			cRecord = clock();
			flagWelcome = true;
			cWelcomeRecord = clock();
			flagAlreadyPreTip = false;

			std::thread(OnMessage, hWnd).detach();
		}

		// ��Ϣǰһ���ӣ�Ԥ��ʾ
		else if (nWorkTime >= g_nWorkMinutes - 1 && !flagAlreadyPreTip)
		{
			flagPreTip = true;
			cPreTipRecord = clock();
			std::thread(OnMessage, hWnd).detach();
			flagAlreadyPreTip = true;
		}

		// ��겻����ʱ��˵���Ѿ��뿪
		if (flagStay && nStayTime >= g_nLeaveMinutes)
		{
			// �ȴ�����
			while (ptCursor.x == ptCursorOld.x && ptCursor.y == ptCursorOld.y)
			{
				GetCursorPos(&ptCursor);
				Sleep(500);
			}

			// ���¼�ʱ����ʾ��ӭ����
			nLeaveTime = (clock() - cStayRecord) / CLOCKS_PER_SEC / 60;
			flagStay = false;
			cRecord = clock();
			flagWelcome = true;
			cWelcomeRecord = clock();

			std::thread(OnMessage, hWnd).detach();
		}

		//** ���� **//
		BEGIN_TASK_WND(hWnd);
		{
			int nOffset = 20;
			cleardevice();
			putimage(20, 20, &g_imgIcon);
			settextcolor(PINKWHITE);
			settextstyle(32, 0, L"Consolas");
			if (flagWelcome)	// ��ӭ����
			{
				// ��ʱ�ر�
				if ((cNow - cWelcomeRecord) / CLOCKS_PER_SEC >= 7)
				{
					flagWelcome = false;
				}
				outtextxy(100, 10, L"Welcome back!");
				nOffset = 30;
			}
			else if (flagPreTip)	// Ԥ��������
			{
				if ((cNow - cPreTipRecord) / CLOCKS_PER_SEC >= 7)
				{
					flagPreTip = false;
				}
				outtextxy(100, 10, L"About to rest.");
				nOffset = 30;
			}

			WCHAR buf[32] = { 0 };
			wsprintf(buf, L"%d minutes.", flagWelcome ? nLeaveTime : nWorkTime);

			settextcolor(TOMATO);
			settextstyle(22, 0, L"Consolas");
			outtextxy(100, 17 + nOffset, flagWelcome ? L"You have been away for" : L"You have worked for");
			settextcolor(PURPLE);
			settextstyle(30, 0, L"Consolas");
			outtextxy(100, 40 + nOffset, buf);
		}
		END_TASK();
		FLUSH_DRAW();

		Sleep(100);
	}

	EasyWin32::closegraph_win32(hWnd);
	return 0;
}

