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

// 存储整个屏幕的大小信息（多显示器）
struct ScreenSize
{
	int left;	// 多显示器的左上角 x 坐标
	int top;	// 多显示器的左上角 y 坐标
	int w;		// 多显示器的总和宽度
	int h;		// 多显示器的总和高度
};

// 获取多显示器大小信息
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

int g_nWorkMinutes;		// 工作时长
int g_nRestMinutes;		// 休息时长
int g_nLeaveMinutes;	// 离开判定时长

IMAGE g_imgIcon;
IMAGE g_imgBackground;

/*
 *    参考自 http://tieba.baidu.com/p/5218523817
 *    参数说明：pImg 是原图指针，width1 和 height1 是目标图片的尺寸。
 *    函数功能：将图片进行缩放，返回目标图片。可以自定义宽高；也可以只给宽度，程序自动计算高度
 *    返回目标图片
*/
IMAGE zoomImage(IMAGE* pImg, int newWidth, int newHeight = 0)
{
	// 防止越界
	if (newWidth < 0 || newHeight < 0) {
		newWidth = pImg->getwidth();
		newHeight = pImg->getheight();
	}

	// 当参数只有一个时按比例自动缩放
	if (newHeight == 0) {
		// 此处需要注意先*再/。不然当目标图片小于原图时会出错
		newHeight = newWidth * pImg->getheight() / pImg->getwidth();
	}

	// 获取需要进行缩放的图片
	IMAGE newImg(newWidth, newHeight);

	// 分别对原图像和目标图像获取指针
	DWORD* oldDr = GetImageBuffer(pImg);
	DWORD* newDr = GetImageBuffer(&newImg);

	// 赋值 使用双线性插值算法
	for (int i = 0; i < newHeight - 1; i++) {
		for (int j = 0; j < newWidth - 1; j++) {
			int t = i * newWidth + j;
			int xt = j * pImg->getwidth() / newWidth;
			int yt = i * pImg->getheight() / newHeight;
			newDr[i * newWidth + j] = oldDr[xt + yt * pImg->getwidth()];
			// 实现逐行加载图片
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

// 初始化
void Init()
{
	g_nWorkMinutes = GetIniFileInfoInt(INI_PATH, L"settings", L"work_min", 40);
	g_nRestMinutes = GetIniFileInfoInt(INI_PATH, L"settings", L"rest_min", 3);
	g_nLeaveMinutes = GetIniFileInfoInt(INI_PATH, L"settings", L"leave_min", 3);

	loadimage(&g_imgIcon, L"MYICON", MAKEINTRESOURCE(IDB_ICON1));
	loadimage(&g_imgBackground, L"./bk.jpg");
}

// 窗口下降到屏幕外，再隐藏窗口，复原位置
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
		// 处理托盘消息
	case WM_TRAY:
		switch (lParam)
		{
			// 左键时显示窗口
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

// 创建底部窗口
HWND CreateBottomWindow(int _w, int _h, ScreenSize size)
{
	int w = size.w + size.left;
	int h = size.h + size.top;
	EasyWin32::PreSetWindowPos(w - _w, h - _h);
	EasyWin32::PreSetWindowStyle(WS_POPUP);
	return EasyWin32::initgraph_win32(_w, _h, EW_NORMAL, APP_NAME, WndProc);
}

// 播放提示音
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

// 提醒休息
void Rest()
{
	// 锁死键盘鼠标
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
			settextstyle(128, 0, L"微软雅黑");
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

	// "关于" 按钮
	if (uId == IDC_MENU_ABOUT)
	{
		// 若前一个窗口还未关闭
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

	// 创建托盘，绑定菜单
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
	clock_t cRecord = clock();				// 开始工作的时间
	clock_t cStayRecord = clock();			// 停留的时间
	POINT ptCursorOld = { 0,0 }, ptCursor;	// 记录鼠标位置
	bool flagStay = false;					// 标记鼠标停留
	bool flagWelcome = false;				// 标记刚刚回来
	clock_t cWelcomeRecord;					// 显示欢迎文字的时间
	int nLeaveTime;							// 离开时长
	bool flagPreTip = false;				// 标记进行预提醒
	clock_t cPreTipRecord;					// 显示预提醒文字的时间
	bool flagAlreadyPreTip = false;			// 标记已经预提醒过
	while (true)
	{
		clock_t cNow = clock();

		// 鼠标不动，开始计时，时长超过阈值后可判定为离开
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

		int nWorkTime		// 工作时长
			= (cNow - cRecord) / CLOCKS_PER_SEC / 60;
		int nStayTime		// 鼠标停留时长
			= (cNow - cStayRecord) / CLOCKS_PER_SEC / 60;

		// 工作时间超额，需要休息
		if (nWorkTime >= g_nWorkMinutes)
		{
			std::thread(Music, false).detach();
			Sleep(2000);
			Rest();

			// 休息已结束，等待回来
			while (ptCursor.x == ptCursorOld.x && ptCursor.y == ptCursorOld.y)
			{
				GetCursorPos(&ptCursor);
				Sleep(500);
			}

			// 重新计时，显示欢迎文字
			nLeaveTime = (clock() - cNow) / CLOCKS_PER_SEC / 60 + g_nRestMinutes;
			cRecord = clock();
			flagWelcome = true;
			cWelcomeRecord = clock();
			flagAlreadyPreTip = false;

			std::thread(OnMessage, hWnd).detach();
		}

		// 休息前一分钟，预提示
		else if (nWorkTime >= g_nWorkMinutes - 1 && !flagAlreadyPreTip)
		{
			flagPreTip = true;
			cPreTipRecord = clock();
			std::thread(OnMessage, hWnd).detach();
			flagAlreadyPreTip = true;
		}

		// 鼠标不动超时，说明已经离开
		if (flagStay && nStayTime >= g_nLeaveMinutes)
		{
			// 等待回来
			while (ptCursor.x == ptCursorOld.x && ptCursor.y == ptCursorOld.y)
			{
				GetCursorPos(&ptCursor);
				Sleep(500);
			}

			// 重新计时，显示欢迎文字
			nLeaveTime = (clock() - cStayRecord) / CLOCKS_PER_SEC / 60;
			flagStay = false;
			cRecord = clock();
			flagWelcome = true;
			cWelcomeRecord = clock();

			std::thread(OnMessage, hWnd).detach();
		}

		//** 绘制 **//
		BEGIN_TASK_WND(hWnd);
		{
			int nOffset = 20;
			cleardevice();
			putimage(20, 20, &g_imgIcon);
			settextcolor(PINKWHITE);
			settextstyle(32, 0, L"Consolas");
			if (flagWelcome)	// 欢迎文字
			{
				// 超时关闭
				if ((cNow - cWelcomeRecord) / CLOCKS_PER_SEC >= 7)
				{
					flagWelcome = false;
				}
				outtextxy(100, 10, L"Welcome back!");
				nOffset = 30;
			}
			else if (flagPreTip)	// 预提醒文字
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

