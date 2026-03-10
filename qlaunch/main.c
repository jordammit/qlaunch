/*
	Copyright (C) 2004 Cory Nelson

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files (the
	"Software"), to deal in the Software without restriction, including
	without limitation the rights to use, copy, modify, merge, publish,
	distribute, sublicense, and/or sell copies of the Software, and to
	permit persons to whom the Software is furnished to do so, subject to
	the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
	CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <gamestat.h>
#include "resource.h"
#include "conf.h"

#define DEFAULT_SERVERLIST		_T("servers.csv")
#define DEFAULT_ENGINE			_T("fuhquake-gl.exe")
#define DEFAULT_ENGINEARGS		_T("")

typedef struct __password {
	LPTSTR host;
	LPTSTR password;
	struct __password *next;
} PASSWORD;

PASSWORD *passwords=NULL;
PASSWORD *lastpassword=NULL;
CONF *conf=NULL;

static void strtrim(LPTSTR str) {
	TCHAR *start, *end;
	size_t newlen;

	start=str;
	while(_istspace(*start)) start++;

	end=start+_tcslen(start)-1;
	while(end>start && _istspace(*end)) end--;

	newlen=end-start+1;
	memmove(str, start, newlen*sizeof(TCHAR));
	str[newlen]='\0';
}

static void LaunchMultiPlayer(HWND hwnd) {
	HWND servers=GetDlgItem(hwnd, IDC_SERVERS);
	int index=ListView_GetSelectionMark(servers);

	if(index!=-1) {
		TCHAR cmdline[256];
		TCHAR host[128]={0};
		STARTUPINFO si={0};
		PROCESS_INFORMATION pi={0};
		PASSWORD *pass;

		ListView_GetItemText(servers, index, 1, host, 128);

		_stprintf(cmdline, _T("%s %s"), ConfGetOpt(conf, _T("engine")), ConfGetOpt(conf, _T("engineargs")));

		for(pass=passwords; pass!=NULL; pass=pass->next) {
			if(!_tcscmp(pass->host, host)) {
				_stprintf(cmdline+_tcslen(cmdline), _T(" +password %s"), pass->password);
				break;
			}
		}

		_stprintf(cmdline+_tcslen(cmdline), _T(" +connect %s"), host);

		if(CreateProcess(ConfGetOpt(conf, _T("engine")), cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
			WaitForSingleObject(pi.hProcess, INFINITE);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}
}

static void RefreshPlayers(HWND hwnd, NMITEMACTIVATE *nia) {
	if(nia->iItem!=-1) {
		TCHAR host[128]={0}, realhost[128]={0};
#ifdef UNICODE
		char realhost_mbs[128]={0};
#endif
		GS_SERVERINFO *info;
		unsigned short port=0;

		ListView_GetItemText(nia->hdr.hwndFrom, nia->iItem, 1, host, 512);
		_stscanf(host, _T("%[^:]:%hu"), realhost, &port);

#ifdef UNICODE
		wcstombs(realhost_mbs, realhost, sizeof(realhost_mbs));
		info=GSQueryServer(GS_TYPE_QUAKEWORLD, realhost_mbs, port);
#else
		info=GSQueryServer(GS_TYPE_QUAKEWORLD, realhost, port);
#endif
		if(info) {
			HWND players=GetDlgItem(hwnd, IDC_PLAYERS);
			GS_PLAYERINFO *player;

			ListView_DeleteAllItems(players);

			for(player=info->players; player!=NULL; player=player->next) {
				TCHAR buf[128];
				LVITEM lvi={0};

				lvi.mask=LVIF_TEXT;

				if(!player->name) continue;

				lvi.iSubItem=0;
#ifdef _UNICODE
				lvi.pszText=buf;
				mbstowcs(buf, player->name, strlen(player->name)+1);
#else
				lvi.pszText=player->name;
#endif
				lvi.iItem=ListView_InsertItem(players, &lvi);
				
				lvi.iSubItem=1;
				lvi.pszText=buf;
				_stprintf(buf, _T("%d"), player->score);
				ListView_SetItem(players, &lvi);

				lvi.iSubItem=2;
				lvi.pszText=buf;
				_stprintf(buf, _T("%d"), player->time/60);
				ListView_SetItem(players, &lvi);

				lvi.iSubItem=3;
				lvi.pszText=buf;
				_stprintf(buf, _T("%d"), player->ping);
				ListView_SetItem(players, &lvi);
			}

			GSFreeServerInfo(info);
		}
	}
}

static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_NOTIFY: {
			NMHDR *nmh=(NMHDR*)lParam;
			if(nmh->idFrom==IDC_SERVERS) {
				switch(nmh->code) {
					case NM_CLICK:
						RefreshPlayers(hwnd, (NMITEMACTIVATE*)nmh);
						break;
					case NM_DBLCLK:
						LaunchMultiPlayer(hwnd);
						break;
				}
			}
		} break;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDC_CONNECT:
					LaunchMultiPlayer(hwnd);
					break;
				case IDC_SINGLEPLAYER: {
					TCHAR cmdline[256];
					STARTUPINFO si={0};
					PROCESS_INFORMATION pi={0};

					_stprintf(cmdline, _T("%s %s"), ConfGetOpt(conf, _T("engine")), ConfGetOpt(conf, _T("engineargs")));

					if(CreateProcess(ConfGetOpt(conf, _T("engine")), cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
						WaitForSingleObject(pi.hProcess, INFINITE);
						CloseHandle(pi.hProcess);
						CloseHandle(pi.hThread);
					}
				} break;
				case IDC_QUIT:
					EndDialog(hwnd, 0);
					break;
			}
			break;
		case WM_INITDIALOG:
			////////////////////////////////////////////////////////////
			// Init Servers
			{
				TCHAR buf[512];
				LVCOLUMN lvc={0};
				HMODULE mod=GetModuleHandle(NULL);
				HWND servers=GetDlgItem(hwnd, IDC_SERVERS);
				ListView_SetExtendedListViewStyle(servers, LVS_EX_FULLROWSELECT|LVS_EX_LABELTIP);

				conf=ConfCreate();
				if(!ConfLoad(conf, _T("qlaunch.conf"))) {
					ConfSetOpt(conf, _T("serverlist"), DEFAULT_SERVERLIST);
					ConfSetOpt(conf, _T("engine"), DEFAULT_ENGINE);
					ConfSetOpt(conf, _T("engineargs"), DEFAULT_ENGINEARGS);
				}
				else {
					if(!ConfGetOpt(conf, _T("serverlist"))) ConfSetOpt(conf, _T("serverlist"), DEFAULT_SERVERLIST);
					if(!ConfGetOpt(conf, _T("engine"))) ConfSetOpt(conf, _T("engine"), DEFAULT_ENGINE);
					if(!ConfGetOpt(conf, _T("engineargs"))) ConfSetOpt(conf, _T("engineargs"), DEFAULT_ENGINEARGS);
				}
				ConfSave(conf, _T("qlaunch.conf"));

				lvc.pszText=buf;
				lvc.mask=LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM;
				lvc.fmt=LVCFMT_LEFT;

				lvc.iSubItem=0;
				lvc.cx=176;
				LoadString(mod, IDS_SERVER, buf, 512);
				ListView_InsertColumn(servers, 0, &lvc);

				lvc.iSubItem=1;
				lvc.cx=160;
				LoadString(mod, IDS_HOST, buf, 512);
				ListView_InsertColumn(servers, 1, &lvc);
			}

			////////////////////////////////////////////////////////////
			// Load Servers
			{
				HWND servers=GetDlgItem(hwnd, IDC_SERVERS);
				FILE *fp=_tfopen(_T("servers.csv"), _T("r"));

				if(fp) {
					TCHAR buf[512];

					while(_fgetts(buf, 512, fp)) {
						TCHAR name[512]={0}, host[512]={0}, pass[512]={0};
						if(_stscanf(buf, _T("%[^;]; %[^;]; %s[^;]"), name, host, pass)>=2) {
							LVITEM lvi={0};

							strtrim(name); strtrim(host); strtrim(pass);

							lvi.mask=LVIF_TEXT;

							lvi.iSubItem=0;
							lvi.pszText=name;
							lvi.iItem=ListView_InsertItem(servers, &lvi);

							lvi.iSubItem=1;
							lvi.pszText=host;
							ListView_SetItem(servers, &lvi);

							if(_tcslen(pass)) {
								PASSWORD *password=malloc(sizeof(PASSWORD));
								password->host=_tcsdup(host);
								password->password=_tcsdup(pass);
								password->next=NULL;

								if(lastpassword) lastpassword=lastpassword->next=password;
								else lastpassword=passwords=password;
							}
						}
					}

					fclose(fp);
				}
			}

			////////////////////////////////////////////////////////////
			// Init Players
			{
				TCHAR buf[512];
				LVCOLUMN lvc={0};
				HMODULE mod=GetModuleHandle(NULL);
				HWND players=GetDlgItem(hwnd, IDC_PLAYERS);
				ListView_SetExtendedListViewStyle(players, LVS_EX_FULLROWSELECT|LVS_EX_LABELTIP);

				lvc.pszText=buf;
				lvc.mask=LVCF_FMT|LVCF_WIDTH|LVCF_TEXT|LVCF_SUBITEM;
				lvc.fmt=LVCFMT_LEFT;

				lvc.iSubItem=0;
				lvc.cx=192;
				LoadString(mod, IDS_PLAYER, buf, 512);
				ListView_InsertColumn(players, 0, &lvc);

				lvc.iSubItem=1;
				lvc.cx=48;
				LoadString(mod, IDS_FRAGS, buf, 512);
				ListView_InsertColumn(players, 1, &lvc);

				lvc.iSubItem=2;
				lvc.cx=48;
				LoadString(mod, IDS_TIME, buf, 512);
				ListView_InsertColumn(players, 2, &lvc);

				lvc.iSubItem=3;
				lvc.cx=48;
				LoadString(mod, IDS_PING, buf, 512);
				ListView_InsertColumn(players, 3, &lvc);
			}
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd, 0);
			break;
		case WM_DESTROY: {
			PASSWORD *pass, *next;
			for(pass=passwords; pass!=NULL; pass=next) {
				if(pass->host) free(pass->host);
				if(pass->password) free(pass->password);

				next=pass->next;
				free(pass);
			}

			ConfDestroy(conf);
		} break;
	}
	return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	InitCommonControls();
	GSInit(GS_INIT_WINSOCK);

	DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, DlgProc);

	GSCleanup(GS_CLEANUP_WINSOCK);
	return 0;
}
