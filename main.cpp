
#include <Windows.h>
#include "resource.h"
#include <Gdiplus.h>
#pragma comment(lib,"gdiplus.lib")
using namespace Gdiplus;

#include <Shellapi.h>
#pragma warning(disable:4996)

BYTE* GrabFile(WCHAR* szFileName,DWORD* pdwSize)
{
	DWORD dwIO;
	if (pdwSize==NULL)
		pdwSize=&dwIO;
	*pdwSize=0;
	HANDLE hFileR=CreateFile(szFileName,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if (hFileR==INVALID_HANDLE_VALUE)
		return NULL;
	DWORD dwSizeR=GetFileSize(hFileR,NULL);
	if (dwSizeR==INVALID_FILE_SIZE || dwSizeR==0)
	{
		CloseHandle(hFileR);
		return NULL;
	}
	BYTE* pOut=new BYTE[dwSizeR];
	SetFilePointer(hFileR,0,NULL,FILE_BEGIN);
	ReadFile(hFileR,pOut,dwSizeR,pdwSize,NULL);
	CloseHandle(hFileR);
	return pOut;
}

BOOL DumpFile(WCHAR* szFileName,BYTE* pData,DWORD dwDataLen)
{
	DWORD dwIO;
	if (pData==NULL)
		return FALSE;
	if (dwDataLen==0)
		return FALSE;

	HANDLE hFileW=CreateFile(szFileName,GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	if (hFileW==INVALID_HANDLE_VALUE)
		return FALSE;
	SetFilePointer(hFileW,0,NULL,FILE_BEGIN);
	WriteFile(hFileW,pData,dwDataLen,&dwIO,NULL);
	SetEndOfFile(hFileW);
	CloseHandle(hFileW);

	return TRUE;
}

LRESULT CALLBACK NewEditProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam);
HWND hWnd;
WNDPROC OldEditProc;

void OnInitDialog(HWND hWndDlg)
{
	hWnd=hWndDlg;
	OldEditProc=(WNDPROC)SetWindowLong(GetDlgItem(hWndDlg,IDC_EDIT1),GWL_WNDPROC,(LONG)NewEditProc);
	SetDlgItemText(hWndDlg,IDC_EDIT2,L"拖拽文件夹至此");
	SetDlgItemText(hWndDlg,IDC_EDIT2,L".c|.cpp|.h|.hpp|.def");
}

void OnClose(HWND hWndDlg)
{
	SetWindowLong(hWndDlg,GWL_WNDPROC,(LONG)OldEditProc);
}

BOOL TailMatch(WCHAR* str,WCHAR* tail)
{
	int len1=wcslen(str);
	int len2=wcslen(tail);

	if (len2>len1)
		return FALSE;

	str+=len1-len2;

	if (_wcsicmp(str,tail)==0)
		return TRUE;
	return FALSE;
}

int SplitString(WCHAR* str,WCHAR token,WCHAR*** splitout)
{
	WCHAR* splitbuffer=wcsdup(str);
	//开始只有一段，遇到一个token多一段
	int num=1;
	while (*str!=0)
	{
		if (*str==token)
			num++;
		str++;
	}

	WCHAR** splitarray=new WCHAR*[num];
	splitarray[0]=splitbuffer;
	int index=1;
	while (index<num)
	{
		//遇到token，结束上一段，开始新段
		if (*splitbuffer==token)
		{
			*splitbuffer=0;
			splitarray[index]=splitbuffer+1;
			index++;
		}
		*splitbuffer++;
	}

	*splitout=splitarray;
	return num;
}

void SplitFree(WCHAR** splitarray)
{
	free(splitarray[0]);
	delete[] splitarray;
}

void OnCommand(HWND hWndDlg,int nCtlID,int nNotify)
{
	//nNotify==1,accelerator
	//nNotify==0,menu
	if (nCtlID==IDC_BUTTON1)
	{
		int filterlen=GetWindowTextLength(GetDlgItem(hWndDlg,IDC_EDIT2));
		WCHAR* filtertext=new WCHAR[filterlen+1];
		GetDlgItemText(hWndDlg,IDC_EDIT2,filtertext,filterlen+1);
		WCHAR** filterarray;
		int filternum=SplitString(filtertext,L'|',&filterarray);

		WCHAR TargetPath[MAX_PATH];
		WCHAR SavePath[MAX_PATH];
		WCHAR TempPath[MAX_PATH];

		//C:\123
		GetDlgItemText(hWndDlg,IDC_EDIT1,TargetPath,MAX_PATH);
		if (GetFileAttributes(TargetPath)!=0xFFFFFFFF)
		{
			//C:\123_new
			wcscpy(SavePath,TargetPath);
			wcscat(SavePath,L"_new");
			//C:\123\*.*
			wcscpy(TempPath,TargetPath);
			wcscat(TempPath,L"\\*.*");
			
			//目录不存在则创建
			if (GetFileAttributes(SavePath)==0xFFFFFFFF)
				CreateDirectory(SavePath,NULL);

			WIN32_FIND_DATA wfd;
			HANDLE hFind=FindFirstFile(TempPath,&wfd);
			do 
			{
				for (int i=0;i<filternum;i++)
				{
					//只允许一层目录，不进行递归
					if (wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
						continue;		
					if (TailMatch(wfd.cFileName,filterarray[i]))
					{
						//C:\123\main.cpp
						wcscpy(TempPath,TargetPath);
						wcscat(TempPath,L"\\");
						wcscat(TempPath,wfd.cFileName);

						//字母1字节，转Unicode 2字节，转UTF-8 1字节
						//汉字2字节，转Unicode 2字节，转UTF-8 3字节
						//Unicode缓冲区2倍足够，UTF-8缓冲区2倍也足够
						DWORD dwSize;
						BYTE* pAnsi=GrabFile(TempPath,&dwSize);
						//检测bom，ucs2-le编码的不转换
						if (dwSize>2 && pAnsi[0]==0xFF && pAnsi[1]==0xFE)
						{
							delete[] pAnsi;
							continue;
						}

						BYTE* pUcs2=new BYTE[dwSize*2];
						BYTE* pUtf8=new BYTE[dwSize*2];
						int UniNum=MultiByteToWideChar(CP_ACP,0,(char*)pAnsi,dwSize,(WCHAR*)pUcs2,dwSize);
						dwSize=WideCharToMultiByte(CP_UTF8,0,(WCHAR*)pUcs2,UniNum,(char*)pUtf8,dwSize*2,NULL,NULL);

						//C:\123_new\main.cpp
						wcscpy(TempPath,SavePath);
						wcscat(TempPath,L"\\");
						wcscat(TempPath,wfd.cFileName);
						DumpFile(TempPath,pUtf8,dwSize);

						delete[] pUtf8;
						delete[] pUcs2;
						delete[] pAnsi;
						break;
					}
				}
			} while (FindNextFile(hFind,&wfd));
			FindClose(hFind);

			SplitFree(filterarray);
			delete[] filtertext;
			MessageBox(hWndDlg,SavePath,L"转换完成",MB_OK);
		}
	}
}

void OnEditDropFile(HDROP hDrop)
{
	DWORD dwFileNum=DragQueryFile(hDrop,0xFFFFFFFF,NULL,0);
	if (dwFileNum==1)
	{
		int nNameLen=DragQueryFile(hDrop,0,NULL,0);
		WCHAR* Name=new WCHAR[nNameLen+1];
		DragQueryFile(hDrop,0,Name,nNameLen+1);
		SetDlgItemText(hWnd,IDC_EDIT1,Name);
		delete[] Name;
	}
	DragFinish(hDrop);
}

LRESULT CALLBACK NewEditProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	if (uMsg==WM_DROPFILES)
	{
		OnEditDropFile((HDROP)wParam);
		return 0;
	}
	return OldEditProc(hwnd,uMsg,wParam,lParam);
}

void OnPaint(HDC hDC)
{
	//Graphics g(hDC);
}

void OnTimer(HWND hWndDlg,int nTmrID)
{

}

INT_PTR CALLBACK DialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		OnInitDialog(hwndDlg);
		return TRUE;
	case WM_CLOSE:
		OnClose(hwndDlg);
		EndDialog(hwndDlg,0);
		return TRUE;
	case WM_ERASEBKGND:
		return FALSE;	//nonzero if erased background
	case WM_PAINT:
		PAINTSTRUCT ps;
		BeginPaint(hwndDlg,&ps);
		OnPaint(ps.hdc);
		EndPaint(hwndDlg,&ps);
		return TRUE;
	case WM_COMMAND:
		OnCommand(hwndDlg,LOWORD(wParam),HIWORD(wParam));
		return TRUE;
	case WM_TIMER:
		OnTimer(hwndDlg,wParam);
		return TRUE;
	}
	return FALSE;
}

int APIENTRY WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nShowCmd)
{
	ULONG_PTR Token;
	GdiplusStartupInput GdipInput;
	GdiplusStartup(&Token,&GdipInput,NULL);
	DialogBoxParam(hInstance,MAKEINTRESOURCE(IDD_DIALOG1),NULL,DialogProc,0);
	GdiplusShutdown(Token);
	return 0;
}
