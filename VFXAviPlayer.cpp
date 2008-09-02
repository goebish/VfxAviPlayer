// TODO
// fix leftmost column in picture modes
// fix Reverse on beat strange behavior
// no more need for critsec ?
// correct random, remembering already loaded files in folder
// put MMX routines directly into ::render instead of .h
// webcam input
// gif local palette
// animated gif
// color picker for color keying
// make cursor change while picking color
// transition/fading between 2 files
// add "shuffle" radio to output blend mode on beat
#include <windows.h>
#include <commctrl.h>
#include <vfw.h>
#include "resource.h"
#include "VFXAviPlayer.h"
#include <shlobj.h>
#include <stdio.h>
#include "jpeg\JpegFile.h"
#include "gif\cgif.h"
#include "png\pngfile.h"

#pragma warning( disable : 4996 ) // deprecated warnings

#define MAXMEM 1024*1024*200 // 200MB Max
#define MAXINDEX 1024*100 // 100K bufferized frames max
#define COLORPICKER_TIMER	1

// main class
class C_VFXAVIPLAYER : public C_RBASE 
{
	protected:
	
	public:
		C_VFXAVIPLAYER();  
		virtual ~C_VFXAVIPLAYER();
		virtual int render(char visdata[2][2][576], int isBeat,	int *framebuffer, int *fbout, int w, int h);	 // render routine	
		virtual HWND conf(HINSTANCE hInstance, HWND hwndParent);
		virtual char *get_desc();
		virtual void load_config(unsigned char *data, int len);
		virtual int  save_config(unsigned char *data);
	
		void reload_image();
		void OpenJPEG(LPCSTR szFile);
		void OpenGif(LPCSTR szFile);
		void OpenPng(LPCSTR szFile);
		void OpenAVI(LPCSTR szFile);
		void GrabAVIFrame(int frame);
		void PopulateFileList();
		void setGUI(e_currentMode mode);
		
		HINSTANCE hInst;
		PAVISTREAM pStream;
		PGETFRAME pgf;
		PAVIFILE pAviFile;
		int width,height,colordepth,frame,membeat,pictureWidth,pictureHeight;
		long lastframe; // number of frames in current stream
		long tpf;	// original time per frame (ms)
		DWORD lastframetime;
		//Gif * pGif; // Gif object
		CGIF *pGif; // Gif object
		char *pGifData; // pointer to 24 bits uncompressed frame buffer
		char *pJpegData; // pointer to jpeg uncompressed frame buffer
		char *pData; // pointer to avi uncompressed frame buffer
		char *palData; // pointer to palette for 8 bits streams 
		int pictureChannels; // bytes per pixels for png
		bool isVideoLoaded;
		bool reload;
		int beatcount;	// beat counter for auto change every x beats
		int lastchangetime; // time rememberer for auto change every x seconds & no reverse on beat before 500ms loading
		int loopcount; // counter for auto change every x loops
		int totfiles; // total avi files
		int curfile; // current file index
		char *filelist[4096];
		char filename[4096][MAX_PATH];

		static CRITICAL_SECTION critsec;

		HWND hwndDlg;
		apeconfig config;
		int Rd,Gd,Bd;
		byte palR[256],palG[256],palB[256]; // 8 bits palette
		// avi frames buffer
		bool isIndexable;
		char* framesbuffer;
		bool framesindex[MAXINDEX];
		static bool isLoading;
		e_currentMode currentMode;
		UINT_PTR timerID;
		COLORREF tempcolor; // temp color for color picker
		bool pickingcolor; // we're picking a new color
		static unsigned int instCount; // keep track of instanciated apes (critsec management)
};

// global configuration dialog pointer 
static C_VFXAVIPLAYER *g_ConfigThis; 
static HINSTANCE g_hDllInstance;

CRITICAL_SECTION C_VFXAVIPLAYER::critsec;

bool C_VFXAVIPLAYER::isLoading = false;
unsigned int C_VFXAVIPLAYER::instCount = 0;

BOOL GetFormattedError(LPTSTR dest,int size,int err) // retrieve error string from error number
{
	DWORD dwLastError=err;
	BYTE width=0;
	DWORD flags;
	flags  = FORMAT_MESSAGE_MAX_WIDTH_MASK &width;
	flags |= FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS;
	return 0 != FormatMessage(flags,NULL,dwLastError,MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),	dest,size,NULL);
}

////////////////////////////////////////////////////
// callback pour positionnement du folder initial //
////////////////////////////////////////////////////
int CALLBACK MyBrowseCallbackProc(HWND hwnd, UINT uMsg, WPARAM wParam,LPARAM lParam) 
{
	switch (uMsg)
	{
		case BFFM_INITIALIZED:
			SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)g_ConfigThis->config.avipath);
		break;
	}
	return 0;
}
////////////////////////
// alphasort routines //
////////////////////////
// swap two strings
void sswap( char **str1, char **str2 )
{
	char *tmp = *str1;
	*str1 = *str2;
	*str2 = tmp;
}
// ordering strings alphabetically in ascendant order
void alphasort( char **str, const int &strnum )
{
	for( int i = strnum - 1; i > 0; i-- )
	{
		for( int j = 0; j < i; j++ )
		{
			if( stricmp( str[j], str[j+1] ) > 0 ) // non case sensitive
				sswap( &str[j], &str[j+1] );
		}
	}
}


// this is where we deal with the configuration screen
static BOOL CALLBACK g_DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_TIMER: // todo: check for correct timerID (not really required as this is there is only one timer...)
			if( GetAsyncKeyState(VK_LBUTTON)==0) //  left mouse button released
			{
				// set new chroma key and kill timer;
				KillTimer( hwndDlg, g_ConfigThis->timerID);
				g_ConfigThis->pickingcolor=false;
				g_ConfigThis->Rd = GetRValue(g_ConfigThis->tempcolor);
				g_ConfigThis->Gd = GetGValue(g_ConfigThis->tempcolor);
				g_ConfigThis->Bd = GetBValue(g_ConfigThis->tempcolor);
				g_ConfigThis->config.chromakey = (g_ConfigThis->Rd<<16)&0xff0000 | (g_ConfigThis->Gd<<8)&0xff00 | (g_ConfigThis->Bd&0xff);
				InvalidateRect(GetDlgItem(hwndDlg,IDC_DEFCOL),NULL,TRUE);
				return 0;
			}
			HDC hDC;
			hDC = CreateDC("DISPLAY",0,0,0);
			POINT point;
			GetCursorPos(&point);
			g_ConfigThis->tempcolor = GetPixel(hDC,point.x,point.y);
			DeleteDC(hDC);
			g_ConfigThis->pickingcolor=true;
			InvalidateRect(GetDlgItem(hwndDlg,IDC_DEFCOL),NULL,TRUE);
			return 0;

		case WM_LBUTTONDOWN:
		{
			if( LOWORD(lParam)>=37 && LOWORD(lParam)<=70 &&
				HIWORD(lParam)>=54&& HIWORD(lParam)<=187) // todo: find a better way to find coords
			{	// colorpicker clicked
				g_ConfigThis->timerID = SetTimer(hwndDlg, COLORPICKER_TIMER, 100, NULL);
				return 0;
			}
			return 0;
		}

		case WM_USER+WM_ENABLE: // custom enable/disable control, avoid calling EnableWindow from main class
			EnableWindow( GetDlgItem(hwndDlg, LOWORD(lParam)),LOWORD(wParam)); 
			return 1;
		case WM_COMMAND:
			if (HIWORD(wParam) == CBN_SELCHANGE) { // combo change
				HWND h = (HWND)lParam;
				int p;
				switch (LOWORD(wParam)) {
				case IDC_PICTURE: // new avi file selected
					p = SendMessage(h, CB_GETCURSEL, 0, 0);
					if (p >= 0) {
						g_ConfigThis->curfile=p;
						SendMessage(h, CB_GETLBTEXT, p, (LPARAM)g_ConfigThis->config.image);
						g_ConfigThis->reload=true;
					}
					break;
				case IDC_BLENDMODE: // new blend mode selected
					g_ConfigThis->config.output = SendMessage(h, CB_GETCURSEL, 0, 0);
					break;
				case IDC_BLENDMODE_ONBEAT:
					g_ConfigThis->config.output_onbeat = SendMessage(h, CB_GETCURSEL, 0, 0);
					break;
				}
			} else if (HIWORD(wParam) == BN_CLICKED ) { // button click
				HWND h = (HWND)lParam;
				switch (LOWORD(wParam)) {
				case IDC_DIRECTORY: //button [dir] select directory for avi files
					BROWSEINFO bi;
					ITEMIDLIST *pidl;
					char mydir[MAX_PATH];
					bi.hwndOwner = hwndDlg;
					bi.pidlRoot = 0;
					bi.pszDisplayName = g_ConfigThis->config.avipath;
					bi.lpszTitle = "Select AVIs folder";
					bi.ulFlags = BIF_RETURNONLYFSDIRS;
					bi.lpfn = (BFFCALLBACK) MyBrowseCallbackProc; 
					bi.lParam = 0;
					//Call the directory browse dialog and assign it to a ITEMIDLIST
					pidl=SHBrowseForFolder(&bi);
					//Return the selected path
					SHGetPathFromIDList(pidl, mydir);
					if(strcmp(mydir,"") && strcmp(mydir,g_ConfigThis->config.avipath)) 
					{ 
						strcpy(g_ConfigThis->config.avipath,mydir);
						// populate combobox & filelist
						g_ConfigThis->totfiles=0;
						SendDlgItemMessage(hwndDlg, IDC_PICTURE, CB_RESETCONTENT, 0, 0);
						char buf[MAX_PATH];
						if(!strcmp(g_ConfigThis->config.avipath,"")) GetModuleFileName(g_hDllInstance, buf, MAX_PATH);
						else strcpy(buf,g_ConfigThis->config.avipath);
						g_ConfigThis->curfile=0;
						g_ConfigThis->PopulateFileList();	
						if(g_ConfigThis->totfiles>0) 
						{ 	
							for(int i=0;i<g_ConfigThis->totfiles;i++) 
							{
								SendDlgItemMessage(hwndDlg, IDC_PICTURE, CB_ADDSTRING, 0, (LPARAM)g_ConfigThis->filelist[i]);
							}
							strcpy(g_ConfigThis->config.image,g_ConfigThis->filelist[0]);
							g_ConfigThis->curfile=0;
						}
					}
					break;

				case IDC_ABOUT: //button [?] about
					MessageBox(HWND_DESKTOP,"Version 1.07 (c)2004-2008 by Goebish\n\nThis is an improved AVI renderer for AVS\n- better stability (no more crash when changing files\n   without disabling rendering)\n- avi files folder selection\n- better speed control, with frames skipping\n- pause & reset video buttons\n- luma & chroma keying\n- configurable auto stream change\n\nControls should be self explanary.\nChroma keying is far from being perfect\nbut it works well for what I do with it :)\n\nGet latest version and additional APEs at:\nhttp://vfx.fr.st\nContact: goebish@gmail.com","VFX Avi Player",MB_OK);
					break;

				case IDC_RESET: //button [<<] reset frame counter
					g_ConfigThis->frame=-1;
					break;
				
				case IDC_RESET_SPEED: // button >< reset speed
					g_ConfigThis->config.speed=0;
					SendDlgItemMessage(hwndDlg, IDC_SPEED, TBM_SETPOS, 1, g_ConfigThis->config.speed);
					break;
				
				case IDC_PLAY_PAUSE: // button play/pause
					g_ConfigThis->config.paused=!g_ConfigThis->config.paused;
					HWND hBouton;
					HANDLE hImage;
					hBouton = GetDlgItem(hwndDlg, IDC_PLAY_PAUSE);
					if(!g_ConfigThis->config.paused) {
						hImage = LoadImage(g_ConfigThis->hInst, MAKEINTRESOURCE(IDB_PAUSE), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
					}
					else {
						hImage = LoadImage(g_ConfigThis->hInst, MAKEINTRESOURCE(IDB_PLAY), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
					}
					SendMessage(hBouton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)(HANDLE)hImage);
					break;

				case IDC_DEFCOL: // button chromakey color
					static COLORREF custcolors[16];
					int *a;
					CHOOSECOLOR cs;
					a=&g_ConfigThis->config.chromakey;
					cs.lStructSize = sizeof(cs);
					cs.hwndOwner = hwndDlg;
					cs.hInstance = 0;
					cs.rgbResult=((*a>>16)&0xff)|(*a&0xff00)|((*a<<16)&0xff0000);
					cs.lpCustColors = custcolors;
					cs.Flags = CC_RGBINIT|CC_FULLOPEN;
					// go to windows color selection screen
					if (ChooseColor(&cs))
					{
						*a = ((cs.rgbResult>>16)&0xff)|(cs.rgbResult&0xff00)|((cs.rgbResult<<16)&0xff0000);
						g_ConfigThis->Rd=cs.rgbResult&0xff; // desired R
						g_ConfigThis->Gd=(cs.rgbResult&0xff00)>>8; // desired G
						g_ConfigThis->Bd=(cs.rgbResult&0xff0000)>>16; // desired B
					}
					InvalidateRect(GetDlgItem(hwndDlg,IDC_DEFCOL),NULL,TRUE);   
					break;

				case IDC_ENABLE: // button enable avi stream
					g_ConfigThis->config.enabled = (SendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_PICTURES: // button enable pictures
				case IDC_VIDEOS: // button enable videos				
					g_ConfigThis->config.enable_pictures = (SendMessage( GetDlgItem(hwndDlg, IDC_PICTURES), BM_GETCHECK, 0, 0) == BST_CHECKED);
					g_ConfigThis->config.enable_videos = (SendMessage(GetDlgItem(hwndDlg, IDC_VIDEOS), BM_GETCHECK, 0, 0) == BST_CHECKED);
					SendDlgItemMessage(hwndDlg, IDC_PICTURE, CB_RESETCONTENT, 0, 0);
					g_ConfigThis->PopulateFileList();
					// populate files combobox
					if(g_ConfigThis->totfiles>0)
					{
						for(int i=0;i<g_ConfigThis->totfiles;i++)
						{
							int p = SendDlgItemMessage(hwndDlg, IDC_PICTURE, CB_ADDSTRING, 0, (LPARAM)g_ConfigThis->filelist[i]);
							if (stricmp(g_ConfigThis->filelist[i], g_ConfigThis->config.image) == 0) 
							{
								SendDlgItemMessage(hwndDlg, IDC_PICTURE, CB_SETCURSEL, p, 0);
							}
						}
					}
					break;
				case IDC_LUMA_BLACK: // button luma key on black
					g_ConfigThis->config.lumablack = (SendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_LUMA_WHITE: // button luma key on white
					g_ConfigThis->config.lumawhite = (SendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_CHROMA: // button chroma key
					g_ConfigThis->config.chroma = (SendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_BLEND_ONBEAT: // check OnBeat (blend)
					g_ConfigThis->config.blend_onbeat = (SendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_INV_CHROMA: // check invert chroma
					g_ConfigThis->config.invchroma = (SendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_INV_BLACK: // check invert luma black
					g_ConfigThis->config.invblack = (SendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_INV_WHITE: // check invert luma white
					g_ConfigThis->config.invwhite = (SendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;

				case IDC_REVERSE_ON_BEAT: // check reverse on beat
					g_ConfigThis->config.reverse_onbeat = (SendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_SKIPONBEAT: // check frame skip on beat
					g_ConfigThis->config.skiponbeat = (SendMessage(h, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;
				case IDC_AUTO: // check enable auto stream change
					g_ConfigThis->config.enable_auto = (SendMessage(h, BM_GETCHECK,0,0)== BST_CHECKED);
					g_ConfigThis->beatcount=0;
					g_ConfigThis->lastchangetime=timeGetTime();
					g_ConfigThis->loopcount=0;
					break;
				case IDC_END: // radio every x loops
					g_ConfigThis->config.end_of_stream = true;
					g_ConfigThis->config.x_beats = false;
					g_ConfigThis->config.x_seconds = false;
					g_ConfigThis->loopcount=0;
					break;
				case IDC_BEATS: // radio every x beats
					g_ConfigThis->config.end_of_stream = false;
					g_ConfigThis->config.x_beats = true;
					g_ConfigThis->config.x_seconds = false;
					g_ConfigThis->beatcount=0;
					break;
				case IDC_SECONDS: // radio every x seconds
					g_ConfigThis->config.end_of_stream = false;
					g_ConfigThis->config.x_beats = false;
					g_ConfigThis->config.x_seconds = true;
					g_ConfigThis->lastchangetime = timeGetTime();
					break;
				case IDC_NEXT: // radio next
					g_ConfigThis->config.next = true;
					g_ConfigThis->config.previous = false;
					g_ConfigThis->config.random = false;
					break;
				case IDC_PREVIOUS: // radio previous
					g_ConfigThis->config.next = false;
					g_ConfigThis->config.previous = true;
					g_ConfigThis->config.random = false;
					break;
				case IDC_RANDOM: // radio random
					g_ConfigThis->config.next = false;
					g_ConfigThis->config.previous = false;
					g_ConfigThis->config.random = true;
					break;
				}
			} else if (HIWORD(wParam) == EN_CHANGE) {
				// Placeholder for editboxes
				switch (LOWORD(wParam)) {
				case IDOK:
					break;
				}
			}
			return 1;

		///////////////////////////////////////
		///             SLIDERS             ///
		///////////////////////////////////////
 		case WM_HSCROLL:
		{
			switch(GetDlgCtrlID((HWND)lParam))
			{
				case IDC_BLACK:
				{
					g_ConfigThis->config.black = SendMessage((HWND)lParam,TBM_GETPOS,0,0);
					return 1;
				}
				case IDC_WHITE:
				{
					g_ConfigThis->config.white = SendMessage((HWND)lParam,TBM_GETPOS,0,0);
					return 1;
				}
				case IDC_BEAT_PERSISTANCE: 
				{
					g_ConfigThis->config.beatpersistance = SendMessage((HWND)lParam,TBM_GETPOS,0,0);
					g_ConfigThis->membeat=0;
					return 1;
				}
				case IDC_AJUSTABLE_VALUE:
				{
					g_ConfigThis->config.blend_value = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
					return 1;
				}
				case IDC_AJUSTABLE_VALUE_ONBEAT:
				{
					g_ConfigThis->config .blend_value_onbeat = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
					return 1;
				}
				case IDC_SPEED:
				{
					g_ConfigThis->config.speed = SendMessage((HWND)lParam,TBM_GETPOS,0,0);
					return 1;
				}
				case IDC_FRAMESKIP:
				{
					g_ConfigThis->config.frameskip = SendMessage((HWND)lParam,TBM_GETPOS,0,0);
					return 1;
				}
				case IDC_SLIDER_CHROMA:
				{
					g_ConfigThis->config.chromavalue = SendMessage((HWND)lParam,TBM_GETPOS,0,0);
					return 1;
				}
				case IDC_AUTOVAL:
				{
					g_ConfigThis->config.autovalue = SendMessage((HWND)lParam, TBM_GETPOS,0,0);
					char temp[100];
					sprintf(temp,"%d loops",g_ConfigThis->config.autovalue);
					SetDlgItemText(hwndDlg, IDC_END,temp);
					sprintf(temp,"%d beats",g_ConfigThis->config.autovalue);
					SetDlgItemText(hwndDlg, IDC_BEATS,temp);
					sprintf(temp,"%d seconds",g_ConfigThis->config.autovalue);
					SetDlgItemText(hwndDlg, IDC_SECONDS,temp);
					return 1;
				}
				case IDC_FRAMESKIP_ONBEAT:
				{
					g_ConfigThis->config.frameskip_onbeat = SendMessage((HWND)lParam,TBM_GETPOS,0,0);
					return 1;
				}
				//default:
				//return 1;
			}
		}

		////////////////////////////////////////
		//             INIT DIALOG            //
		////////////////////////////////////////
		case WM_INITDIALOG:
			g_ConfigThis->hwndDlg = hwndDlg;
			// folder icon
			HWND hBouton;
			HANDLE hImage;
			hBouton = GetDlgItem(hwndDlg, IDC_DIRECTORY);
			hImage = LoadImage(g_ConfigThis->hInst, MAKEINTRESOURCE(IDB_FOLDER_BITMAP), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
			SendMessage(hBouton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)(HANDLE)hImage);
			hBouton = GetDlgItem(hwndDlg, IDC_RESET);
			hImage = LoadImage(g_ConfigThis->hInst, MAKEINTRESOURCE(IDB_RESET), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
			SendMessage(hBouton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)(HANDLE)hImage);
			// todo: free hImage ?
			
			// Fill in blendmode lists
			for (int i = 0; i < sizeof(outputs) / sizeof(outputs[0]); ++i) {
				SendDlgItemMessage(hwndDlg, IDC_BLENDMODE, CB_ADDSTRING, 0, (LPARAM)outputs[i]);
				SendDlgItemMessage(hwndDlg, IDC_BLENDMODE_ONBEAT, CB_ADDSTRING, 0, (LPARAM)outputs[i]);			
			}
			// set blend modes
			SendDlgItemMessage(hwndDlg, IDC_BLENDMODE, CB_SETCURSEL, g_ConfigThis->config.output, 0);
			SendDlgItemMessage(hwndDlg, IDC_BLENDMODE_ONBEAT, CB_SETCURSEL, g_ConfigThis->config.output_onbeat , 0);

			// checkboxes
			if (g_ConfigThis->config.enabled) {	// check enabled
				CheckDlgButton(hwndDlg, IDC_ENABLE, BST_CHECKED);
			}
			if (g_ConfigThis->config.enable_pictures) { // check pictures
				CheckDlgButton(hwndDlg, IDC_PICTURES, BST_CHECKED);
			}
			if (g_ConfigThis->config.enable_videos) { // check videos
				CheckDlgButton(hwndDlg, IDC_VIDEOS, BST_CHECKED);
			}
			if (g_ConfigThis->config.lumablack) {	// check luma black
				CheckDlgButton(hwndDlg, IDC_LUMA_BLACK, BST_CHECKED);
			}
			if (g_ConfigThis->config.invblack) {	// check invert luma black
				CheckDlgButton(hwndDlg, IDC_INV_BLACK, BST_CHECKED);
			}
			if (g_ConfigThis->config.lumawhite) {	// check luma white
				CheckDlgButton(hwndDlg, IDC_LUMA_WHITE, BST_CHECKED);
			}
			if (g_ConfigThis->config.invwhite) {	// check invert luma white
				CheckDlgButton(hwndDlg, IDC_INV_WHITE, BST_CHECKED);
			}
			if (g_ConfigThis->config.chroma) { // check chroma
				CheckDlgButton(hwndDlg, IDC_CHROMA, BST_CHECKED);
			}
			if (g_ConfigThis->config.invchroma) {	// check invert chroma
				CheckDlgButton(hwndDlg, IDC_INV_CHROMA, BST_CHECKED);
			}
			hBouton = GetDlgItem(hwndDlg, IDC_PLAY_PAUSE);
			if (!g_ConfigThis->config.paused) { // button play/pause
				hImage = LoadImage(g_ConfigThis->hInst, MAKEINTRESOURCE(IDB_PAUSE), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
			}
			else {
				hImage = LoadImage(g_ConfigThis->hInst, MAKEINTRESOURCE(IDB_PLAY), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
			}
			SendMessage(hBouton, BM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)(HANDLE)hImage);
			if (g_ConfigThis->config.enable_auto) {// check enable auto stream change
				CheckDlgButton(hwndDlg, IDC_AUTO, BST_CHECKED);
			}
			if (g_ConfigThis->config.end_of_stream) { // radio every x loops
				CheckDlgButton(hwndDlg, IDC_END, BST_CHECKED);
			}
			char temp[100];
			sprintf(temp,"%d loops",g_ConfigThis->config.autovalue);
			SetDlgItemText(hwndDlg, IDC_END,temp);
			if (g_ConfigThis->config.x_beats) { // radio every x beats
				CheckDlgButton(hwndDlg, IDC_BEATS, BST_CHECKED);
			}
			sprintf(temp,"%d beats",g_ConfigThis->config.autovalue);
			SetDlgItemText(hwndDlg, IDC_BEATS,temp);
			if (g_ConfigThis->config.x_seconds) { // radio every x seconds
				CheckDlgButton(hwndDlg, IDC_SECONDS, BST_CHECKED);
			}
			sprintf(temp,"%d seconds",g_ConfigThis->config.autovalue);
			SetDlgItemText(hwndDlg,IDC_SECONDS,temp);
			if (g_ConfigThis->config.next) { // radio next
				CheckDlgButton(hwndDlg, IDC_NEXT, BST_CHECKED);
			}
			if (g_ConfigThis->config.previous) { // radio previous
				CheckDlgButton(hwndDlg, IDC_PREVIOUS, BST_CHECKED);
			}
			if (g_ConfigThis->config.random) { // radio random
				CheckDlgButton(hwndDlg, IDC_RANDOM, BST_CHECKED);
			}
			if (g_ConfigThis->config.blend_onbeat) { // radio OnBeat (blend)
				CheckDlgButton(hwndDlg, IDC_BLEND_ONBEAT, BST_CHECKED);
			}
			if (g_ConfigThis->config.reverse_onbeat) { // check reverse on beat
				CheckDlgButton(hwndDlg, IDC_REVERSE_ON_BEAT, BST_CHECKED);
			}
			if (g_ConfigThis->config.skiponbeat) { // check skip on beat
				CheckDlgButton(hwndDlg, IDC_SKIPONBEAT, BST_CHECKED);
			}
			SendDlgItemMessage(hwndDlg, IDC_FRAMESKIP_ONBEAT, TBM_SETRANGE, 1, MAKELONG(1,10));	// init slider frameskip on beat
			SendDlgItemMessage(hwndDlg, IDC_FRAMESKIP_ONBEAT, TBM_SETPOS, 1, g_ConfigThis->config.frameskip_onbeat);
			SendDlgItemMessage(hwndDlg, IDC_BLACK, TBM_SETRANGE, 1, MAKELONG(0, 0x100));	// init slider luma black
			SendDlgItemMessage(hwndDlg, IDC_BLACK, TBM_SETPOS, 1, g_ConfigThis->config.black);
			SendDlgItemMessage(hwndDlg, IDC_WHITE, TBM_SETRANGE, 1, MAKELONG(0, 0x100)); // init slider luma white
			SendDlgItemMessage(hwndDlg, IDC_WHITE, TBM_SETPOS, 1, g_ConfigThis->config.white);
			SendDlgItemMessage(hwndDlg, IDC_BEAT_PERSISTANCE, TBM_SETRANGE, 1, MAKELONG(2,53)); // init slider beat persistance 
			SendDlgItemMessage(hwndDlg, IDC_BEAT_PERSISTANCE, TBM_SETPOS, 1, g_ConfigThis->config.beatpersistance);
			SendDlgItemMessage(hwndDlg, IDC_SPEED, TBM_SETRANGE, 1, MAKELONG(-200,110)); // init slider speed
			SendDlgItemMessage(hwndDlg, IDC_SPEED, TBM_SETPOS, 1, g_ConfigThis->config.speed);
			SendDlgItemMessage(hwndDlg, IDC_FRAMESKIP, TBM_SETRANGE, 1, MAKELONG(-9/*1*/,9)); // init slider frameskip
			SendDlgItemMessage(hwndDlg, IDC_FRAMESKIP, TBM_SETPOS, 1, g_ConfigThis->config.frameskip);
			SendDlgItemMessage(hwndDlg, IDC_SLIDER_CHROMA, TBM_SETRANGE,1, MAKELONG(0,0x180)); // init slider chroma
			SendDlgItemMessage(hwndDlg, IDC_SLIDER_CHROMA, TBM_SETPOS, 1, g_ConfigThis->config.chromavalue);
			SendDlgItemMessage(hwndDlg, IDC_AUTOVAL, TBM_SETRANGE,1, MAKELONG(1,300)); // init slider auto value
			SendDlgItemMessage(hwndDlg, IDC_AUTOVAL, TBM_SETPOS, 1, g_ConfigThis->config.autovalue);
			SendDlgItemMessage(hwndDlg, IDC_AJUSTABLE_VALUE, TBM_SETRANGE, 1, MAKELONG(2,53)); // init slider adjustable blend
			SendDlgItemMessage(hwndDlg, IDC_AJUSTABLE_VALUE, TBM_SETPOS, 1, g_ConfigThis->config.blend_value );
			SendDlgItemMessage(hwndDlg, IDC_AJUSTABLE_VALUE_ONBEAT, TBM_SETRANGE, 1, MAKELONG(2,53)); // init slider adjustable blend
			SendDlgItemMessage(hwndDlg, IDC_AJUSTABLE_VALUE_ONBEAT, TBM_SETPOS, 1, g_ConfigThis->config.blend_value_onbeat );

			// populate files combobox
			if(g_ConfigThis->totfiles>0){
				for(int i=0;i<g_ConfigThis->totfiles;i++)
				{
					int p = SendDlgItemMessage(hwndDlg, IDC_PICTURE, CB_ADDSTRING, 0, (LPARAM)g_ConfigThis->filelist[i]);
					if (stricmp(g_ConfigThis->filelist[i], g_ConfigThis->config.image) == 0) 
					{
						SendDlgItemMessage(hwndDlg, IDC_PICTURE, CB_SETCURSEL, p, 0);
					}
				}
			}
			return 1;

		// draw chromakey color box
		case WM_DRAWITEM:
			DRAWITEMSTRUCT *di;
			di=(DRAWITEMSTRUCT *)lParam;
			if (di->CtlID == IDC_DEFCOL) 
			{
				int w;
				int color;
				w=di->rcItem.right-di->rcItem.left;
				if(g_ConfigThis->pickingcolor)
					color = g_ConfigThis->tempcolor;
				else
				{
					color=g_ConfigThis->config.chromakey;
					color = ((color>>16)&0xff)|(color&0xff00)|((color<<16)&0xff0000);
				}
				// paint nifty color button
				HBRUSH hBrush,hOldBrush;
				LOGBRUSH lb={BS_SOLID,color,0};
				hBrush = CreateBrushIndirect(&lb);
				hOldBrush=(HBRUSH)SelectObject(di->hDC,hBrush);
				Rectangle(di->hDC,di->rcItem.left,di->rcItem.top,di->rcItem.right,di->rcItem.bottom);
				SelectObject(di->hDC,hOldBrush);
				DeleteObject(hBrush);
			}
			return 0;
		
		case WM_DESTROY:
			g_ConfigThis = 0;
			return 1;
	}
	return 0;
}


//////////////////////////////////
//     virtual constructor      //
// set up default configuration //
//       & fill filelist        //
//////////////////////////////////
C_VFXAVIPLAYER::C_VFXAVIPLAYER() 
{
	instCount++;
	// set up critsec so loading and rendering never happen at the same time (would cause crash)
	if(instCount==1)
		InitializeCriticalSection(&critsec);
	memset(&config, 0, sizeof(apeconfig));
	config.enabled=false;
	config.black=0;
	config.white=0;
	config.lumablack=false;
	config.lumawhite=false;
	config.output = OUT_REPLACE; // default blend mode=replace
	config.output_onbeat = OUT_REPLACE;
	config.replace=true;  
	config.additive=false;
	config.blend=false;
	config.blendadditive=false;
	config.blendadjustable=false;
	config.beatpersistance=27;
	config.blend_value=27;
	config.blend_value_onbeat=27;
	config.blend_onbeat = false;
	config.speed=0;
	config.chroma=false;
	config.chromakey=RGB(0xFF,0,0); 
	config.chromavalue=0;
	config.paused=false;
	config.frameskip=1;
	config.invblack=false;
	config.invwhite=false;
	config.invchroma=false;
	config.autovalue=false;
	config.end_of_stream=true;
	config.x_beats=false;
	config.x_seconds=false;
	config.autovalue=30;
	config.next=true;
	config.previous=false;
	config.random=false;
	config.skiponbeat=false;
	config.frameskip_onbeat=1;
	config.reverse_onbeat=false;
	config.enable_videos = true;
	config.enable_pictures = true;
	// avis default dir
	GetModuleFileName(g_hDllInstance, config.avipath, MAX_PATH);
	strcpy(strrchr(config.avipath, '\\') + 1, "");
	// fill filelist
	PopulateFileList();
	// reset internal vars
	frame=0;
	isVideoLoaded=false;
	beatcount=0;
	lastchangetime=0;
	curfile=0;
	loopcount=0;
	Rd=0;
	Gd=0;
	Bd=255; // pure blue
	AVIFileInit(); // init lib AVIFile
	memset( &framesindex,0,MAXINDEX);
	isIndexable=false;
	framesbuffer=NULL;
	pData=NULL;
	pJpegData=NULL;
	pGif=NULL;
	pGifData=NULL;
	hwndDlg=NULL;
	currentMode=modeVideo;
	reload=false;
	pickingcolor=false;
	tempcolor=0;
}


////////////////////////
// virtual destructor //
////////////////////////
C_VFXAVIPLAYER::~C_VFXAVIPLAYER()
{
	if(isVideoLoaded)
	{
		AVIFileRelease(pAviFile);
		AVIStreamGetFrameClose(pgf);
		AVIStreamRelease(pStream);
		AVIFileExit();
	}
	if( framesbuffer!=NULL)
		free(framesbuffer);
	if(pJpegData)
	{
		free(pJpegData);
		pJpegData = NULL;
	}
	if(pGif)
	{
		delete pGif;
		pGif=NULL;
	}
	if(pGifData)
	{
		delete pGifData;
		pGifData=NULL;
	}
	if(instCount==1)
		DeleteCriticalSection(&critsec);
	instCount--;
}

////////////////////////////////////////
// organize GUI for current file type //
////////////////////////////////////////
void C_VFXAVIPLAYER::setGUI(e_currentMode mode)
{
	// todo: sendtimeout messages instead of EnableWindow (cause locks)
	/*switch(mode)
	{
		case modeVideo:
			EnableWindow( GetDlgItem( hwndDlg, IDC_SPEED), TRUE);
			EnableWindow( GetDlgItem( hwndDlg, IDC_RESET_SPEED), TRUE);
			EnableWindow( GetDlgItem( hwndDlg, IDC_REVERSE_ON_BEAT), TRUE);
			EnableWindow( GetDlgItem( hwndDlg, IDC_SKIPONBEAT), TRUE);
			EnableWindow( GetDlgItem( hwndDlg, IDC_FRAMESKIP), TRUE);
			EnableWindow( GetDlgItem( hwndDlg, IDC_FRAMESKIP_ONBEAT), TRUE);
			EnableWindow( GetDlgItem( hwndDlg, IDC_END), TRUE);
			break;
		case modeJpeg:
		case modePng:
		case modeGif:
			EnableWindow( GetDlgItem( hwndDlg, IDC_SPEED), FALSE);
			EnableWindow( GetDlgItem( hwndDlg, IDC_RESET_SPEED), FALSE);
			EnableWindow( GetDlgItem( hwndDlg, IDC_REVERSE_ON_BEAT), FALSE);
			EnableWindow( GetDlgItem( hwndDlg, IDC_SKIPONBEAT), FALSE);
			EnableWindow( GetDlgItem( hwndDlg, IDC_FRAMESKIP), FALSE);
			EnableWindow( GetDlgItem( hwndDlg, IDC_FRAMESKIP_ONBEAT), FALSE);
			EnableWindow( GetDlgItem( hwndDlg, IDC_END), FALSE);
			break;
	}*/
}

/////////////////////////////////////////
// verify file extension, and open avi //
/////////////////////////////////////////
void C_VFXAVIPLAYER::reload_image() {
	if (config.image[0] == 0) return;
	while(isLoading) // one loading at a time please !
		Sleep(10);
	EnterCriticalSection(&critsec);
	isLoading=true;
	char *ext = strrchr(config.image, '.') + 1;
	char buf[MAX_PATH*2];
	strcpy(buf,config.avipath);
	strcat(buf,"\\");
	strcat(buf,config.image);
	if(stricmp(ext, "avi") == 0 && GetFileAttributes(buf)!=INVALID_FILE_ATTRIBUTES) 
	{
		OpenAVI(buf);
		currentMode = modeVideo;
	}
	else if( (stricmp(ext, "jpg")==0 || stricmp(ext, "jpeg")==0) && GetFileAttributes(buf)!=INVALID_FILE_ATTRIBUTES) 
	{
		OpenJPEG(buf);
		currentMode = modeJpeg;
	}
	else if( (stricmp(ext, "gif")==0 && GetFileAttributes(buf)!=INVALID_FILE_ATTRIBUTES))
	{
		OpenGif(buf);
		currentMode = modeGif;
	}
	else if( (stricmp(ext, "png")==0 && GetFileAttributes(buf)!=INVALID_FILE_ATTRIBUTES))
	{
		OpenPng(buf);
		currentMode = modePng;
	}
	setGUI(currentMode);
	lastchangetime=timeGetTime();
	LeaveCriticalSection(&critsec);
	isLoading=false;
}

////////////////
//  open png  //
////////////////
void C_VFXAVIPLAYER::OpenPng(LPCSTR szFile)
{
	static BYTE              *pbImage=NULL;
    int                cxImgSize, cyImgSize;
    int                cImgChannels;
    png_color          bkgColor = {127, 127, 127};

	if(pJpegData)
	{
		free(pJpegData);
		pJpegData=NULL;
	}
	
	if(PngLoadImage ((char*)szFile, &pbImage, &cxImgSize, &cyImgSize, &cImgChannels, &bkgColor))
	{
		pictureWidth = cxImgSize;
		pictureHeight= cyImgSize;
		pictureChannels=cImgChannels;
		pJpegData = (char*)malloc( pictureHeight*pictureWidth*cImgChannels);
		memcpy( pJpegData, pbImage, pictureHeight*pictureWidth*cImgChannels);
		
		png_byte **ppbImage = &pbImage;
		
		if (*ppbImage)
		{
			free (*ppbImage); // grr reste une fuite de mémoire dans PngLoadImage (8ko/image?, pas tt le temps, résolu ???)
			*ppbImage = NULL;
		}
		if( pbImage)
		{
			free(pbImage);
			pbImage=NULL;
		}
	}
	colordepth=24; 
}

#define CGIFFF CGIF::
////////////////
//  open gif  //
////////////////
void C_VFXAVIPLAYER::OpenGif(LPCSTR szFile)
{
	/*if(pGif)
	{
		delete pGif;
		pGif=NULL;
	}*/
	if(pGifData)
	{
		free(pGifData);
		pGifData=NULL;
	}
	// todo: convert szFile to 8.3 shortname for fopen ?
	CGIFFF GifFileType *giff=NULL;
	CGIFFF SavedImage *sp;
	CGIFFF ColorMapObject *cm;
	giff = CGIFFF DGifOpenFileName((char*)szFile);
	if( giff==NULL || GIF_ERROR==CGIFFF DGifSlurp(giff))
		return; // todo: free ressources ?
	if (giff->ImageCount<1)
	{
		CGIFFF DGifCloseFile(giff);
		return;
	}
	sp=giff->SavedImages+0;
	cm = (sp->ImageDesc.ColorMap ? sp->ImageDesc.ColorMap : giff->SColorMap);
	pictureWidth=sp->ImageDesc.Width;
	pictureHeight=sp->ImageDesc.Height;
	pGifData = (char*)malloc( sp->ImageDesc.Width*sp->ImageDesc.Height+1);
	CGIFFF GifColorType *co=cm->Colors, *ce=co+cm->ColorCount;
	// todo: find local palette
	int palNum=0;
	while (co!=ce) 
	{ 
		palR[palNum]=co->Red;
		palG[palNum]=co->Green;
		palB[palNum]=co->Blue;
		palNum++;
		/**p++=(char)co->Red; 
		*p++=(char)co->Green; 
		*p++=(char)co->Blue; 
		co++; */
		co++;
	}
	// if (sp->transp!=-1) img->setTransp(sp->transp); // todo : transparency color index
	memcpy(pGifData, sp->RasterBits, sp->ImageDesc.Width*sp->ImageDesc.Height);
	CGIFFF DGifCloseFile(giff); /* also frees memory structure */
	//free(p);
}


///////////////
// open jpeg //
///////////////
void C_VFXAVIPLAYER::OpenJPEG(LPCSTR szFile)
{
	// free buffer , todo : realloc instead
	if(pJpegData)
	{
		free(pJpegData);
			pJpegData = NULL;
	}
	// todo: convert szFile to 8.3 shortname for fopen ?
	pJpegData = (char*)JpegFile::JpegFileToRGB((char*)szFile, (UINT*)&pictureWidth, (UINT*)&pictureHeight);
	colordepth=24; 
}

//////////////
// open avi //
//////////////
void C_VFXAVIPLAYER::OpenAVI(LPCSTR szFile)
{
	// close if already open 
	if(isVideoLoaded) { 
		isVideoLoaded=false;
		isIndexable=false;
		AVIFileRelease(pAviFile);
		memset(&pAviFile,0,sizeof(pAviFile));
		AVIStreamRelease(pStream);
		AVIStreamGetFrameClose(pgf);
	}
	// non exclusive file open
	CoInitialize(NULL);
	int ret=AVIFileOpen(&pAviFile, szFile, OF_SHARE_DENY_NONE, NULL);
	if(ret){ 
		char temp[MAX_PATH];
		char temp2[512];
		GetFormattedError(temp,MAX_PATH,ret);
		sprintf(temp2,"Error opening file %s\nerror: %s",szFile,temp);
		MessageBox(HWND_DESKTOP, temp2, "VFX avi player", MB_OK);
		isIndexable=false;
		return;
	}
	// get file's info
	AVIFILEINFO info;
	AVIFileInfo(pAviFile,&info,sizeof(info));
	lastframe=info.dwLength; 
	// find video stream
	if(AVIFileGetStream(pAviFile, &pStream,streamtypeVIDEO,0)){
		MessageBox(HWND_DESKTOP,"Error: no video stream found in avi file","VFX avi player",MB_OK);
		isIndexable=false;
		return;
	}
	// get stream info
	AVISTREAMINFO infoVideo; 
	if(AVIStreamInfo(pStream, &infoVideo, sizeof(infoVideo))) {
		MessageBox(HWND_DESKTOP,"Error: can't retrieve infos from video stream","VFX avi player",MB_OK);
		isIndexable=false;
		return;
	} 
	tpf=AVIStreamSampleToTime(pStream,lastframe)/lastframe;
	// determinate stream's format
	LONG lSize; // in bytes
	if(AVIStreamReadFormat(pStream, AVIStreamStart(pStream), NULL, &lSize)) {
		MessageBox(HWND_DESKTOP,"Error: can't retrieve stream format","VFX avi player",MB_OK);
		isIndexable=false;
		return;
	}
	LPBYTE pChunk = new BYTE[lSize];
	if(!pChunk) {
		MessageBox(HWND_DESKTOP,"Error: can't allocate memory","VFX avi player",MB_OK);
		isIndexable=false;
		return;
	}
	if(AVIStreamReadFormat(pStream, AVIStreamStart(pStream), pChunk, &lSize)) {
		MessageBox(HWND_DESKTOP,"Error: can't retrieve stream format","VFX avi player",MB_OK);
		isIndexable=false;
		return;
	}
	LPBITMAPINFO pInfo = (LPBITMAPINFO)pChunk;
	width=pInfo->bmiHeader.biWidth;
	height=pInfo->bmiHeader.biHeight;
	colordepth=pInfo->bmiHeader.biBitCount;
	pgf=NULL;
	
	BITMAPINFOHEADER myFormat;
	memset(&myFormat,0,sizeof(myFormat));
	myFormat.biSize=40;
	if(colordepth>8) { 
		myFormat.biWidth=width;
		myFormat.biHeight=height;
		myFormat.biPlanes=1;
		myFormat.biBitCount=pInfo->bmiHeader.biBitCount?pInfo->bmiHeader.biBitCount:24; // set to 24 bits if header claims 0
		myFormat.biCompression=BI_RGB;
		pgf = AVIStreamGetFrameOpen(pStream,&myFormat );
	}else{
		pgf = AVIStreamGetFrameOpen(pStream,NULL );
	}
	if(!pgf) {
		pgf = AVIStreamGetFrameOpen(pStream,NULL );
		if(!pgf){
			char temp[256];
			char fcc[5];
			memcpy(fcc,&infoVideo.fccHandler,4);
			fcc[4]=0;
			sprintf(temp,"Error : can't find a RGB decompressor for this format ( %s )",fcc);
			MessageBox(HWND_DESKTOP,temp,"VFX avi player",MB_OK);
			isIndexable=false;
			return;
		}
	}

	// everything ok (???)
	memset( &framesindex,0,MAXINDEX);
	pData=NULL;
	isIndexable = (myFormat.biBitCount==24) && ((unsigned long long)(width*height*3*(1+lastframe)) < (unsigned long long)MAXMEM) && (lastframe+1<MAXINDEX);
	if(isIndexable) // allocate frames buffer
	{
		if(framesbuffer==NULL)
			framesbuffer=(char*)malloc(width*height*3*(1+lastframe));
		else
			framesbuffer=(char*)realloc(framesbuffer,width*height*3*(1+lastframe));
	}
	isIndexable&= framesbuffer!=NULL;

	DWORD res = 0;
	SendMessageTimeout( hwndDlg, WM_ENABLE+WM_USER, isIndexable, IDC_REVERSE_ON_BEAT,SMTO_NORMAL,1000,&res); 
	config.frameskip = abs(config.frameskip);
	SendMessageTimeout( GetDlgItem( hwndDlg, IDC_FRAMESKIP), TBM_SETPOS, 1, config.frameskip, SMTO_NORMAL,1000,&res); 
	isVideoLoaded=true;
	frame=-1;
	loopcount=0;
}


////////////////////////////////////////////
// grab a frame from avi stream to buffer //
////////////////////////////////////////////
void C_VFXAVIPLAYER::GrabAVIFrame(int frame)
{
	if(isVideoLoaded && frame<lastframe && frame>=0)
	{
		LPBITMAPINFOHEADER lpbi=NULL;
		if(isIndexable && framesindex[frame]) // get frame pointer from buffer
		{
			pData = &framesbuffer[width*height*3*frame];
		}
		else if(isIndexable) // try to bufferize frame
		{	
			lpbi = (LPBITMAPINFOHEADER)AVIStreamGetFrame(pgf,frame); 
			if(lpbi == NULL)
			{
				pData=NULL;
				return;
			}
			pData=(char *)lpbi+lpbi->biSize+lpbi->biClrUsed * sizeof(RGBQUAD); // pointer to uncompressed frame
			framesindex[frame]= (memcpy_s( &framesbuffer[width*height*3*frame], width*height*3, pData, width*height*3)==0);
		}
		else // not indexable
		{
			lpbi = (LPBITMAPINFOHEADER)AVIStreamGetFrame(pgf,frame); 
			if(lpbi == NULL)
			{
				pData=NULL;
				return;
			}
			pData=(char *)lpbi+lpbi->biSize+lpbi->biClrUsed * sizeof(RGBQUAD); // pointer to uncompressed frame
			if(colordepth<=8){ // build palette for palletized streams
				palData=(char *)lpbi+lpbi->biSize; // pointer to first palette component (RGBQUAD)
				for(UINT16 i=0;i<lpbi->biClrUsed;i++){
					int palbase=i*sizeof(RGBQUAD);
					palB[i]=palData[palbase];
					palG[i]=palData[++palbase];
					palR[i]=palData[++palbase];
				}
			}
		}
	}
	else
		pData=NULL;
}


//////////////////////////////////////
////         Render Routine       ////
//////////////////////////////////////
int C_VFXAVIPLAYER::render(char visdata[2][2][576], int isBeat, int *framebuffer, int *fbout, int w, int h)
{
	if(!config.enabled /*|| (!isVideoLoaded && !pJpegData )*/) return 0; // nothing to do
	
	if(reload)
	{
		reload_image();
		reload=false;
	}
	
	////////////////////////////
	// auto change management //
	////////////////////////////
	if(config.enable_auto) { 
		beatcount+=isBeat;
		// check if change is needed
		if( totfiles>0 && 
			((config.end_of_stream && loopcount>=config.autovalue) ||
			(config.x_beats && beatcount>=config.autovalue) ||
			(config.x_seconds && timeGetTime()>=(DWORD)(lastchangetime+config.autovalue*1000)))) { 
			// auto change file
			beatcount=0;
			if(config.next) {
				if(curfile<totfiles-1) {
					curfile++;}
				else {
					curfile=0;}
			}
			else if(config.previous) {
				if(curfile>0) {
					curfile--;}
				else{
					curfile=totfiles-1;}
			}
			else { // random
				curfile=(timeGetTime())%totfiles; 
			}
			strcpy(config.image,filelist[curfile]);
			DWORD res;
			SendMessageTimeout(GetDlgItem(hwndDlg,IDC_PICTURE),CB_SETCURSEL,curfile,0,SMTO_NORMAL,1000,&res); // fix [x2] problem !!!!!!!
			strcpy(config.image,filelist[curfile]);
			reload_image(); 
			lastframetime=timeGetTime();
		}	
	}// end auto change

	if(config.blend_onbeat) {  // on beat additive elapsed frames counter
		if(membeat<=0 && isBeat){
			membeat=config.beatpersistance;
		} else {
			membeat--;
			if(membeat<0)membeat=0;
		}
	}
	static bool wasbeat=false; // remember there was a beat for next frame advance
	if(isBeat)
		wasbeat=true;
	DWORD curtime=timeGetTime();
	
	if( currentMode==modeVideo)
	{
		if(  frame==-1 ||
			(!config.paused && (curtime >= lastframetime+(tpf-config.speed))) ||
			(config.skiponbeat && isBeat)) 
		{ // time to grab next avi frame ?
			if(wasbeat && config.reverse_onbeat) // reverse on beat (only if resulting frame is indexed)
			{
				if( curtime-lastchangetime>750 && isIndexable) // wait 750ms before reverse on beat works
				{
					config.frameskip=-config.frameskip;
					DWORD res;
					SendMessageTimeout(GetDlgItem(hwndDlg,IDC_FRAMESKIP),TBM_SETPOS,1,config.frameskip,SMTO_NORMAL,1000,&res);
				}
				wasbeat=false;
			}

			if(config.skiponbeat && isBeat)
				frame+=config.frameskip_onbeat;
			else if(frame<lastframe-config.frameskip){ // avi frame counter
				frame+= isIndexable ? config.frameskip : abs(config.frameskip) ;} // advance frame, no negative skipping on non indexable streams
			else{ 
				loopcount+=config.end_of_stream; 
				frame=0;
			} 
			
			if(frame<0)frame+=lastframe-1;
			if(frame>lastframe-1)frame=frame-lastframe/*lastframe-1*/;
			if(frame<0)frame=0;
			GrabAVIFrame(frame); // grab frame to pData buffer
			lastframetime=timeGetTime();
		}
	}

	if(currentMode==modeVideo && pData==NULL)
		return 0;

	if(currentMode==modeJpeg && pJpegData==NULL)
		return 0;

	if(currentMode==modeGif && pGifData==NULL)
		return 0;

	if(currentMode==modePng && pJpegData==NULL)
		return 0;

	int pixbase=0; 
	UINT8 R=0,G=0,B=0;
	UINT8 adjustable=(config.blend_value-2)*5;
	UINT8 adjustable_onbeat=(config.blend_value_onbeat-2)*5;
	int L;
	bool drawThisPixel;

	int frame_output;
	if (config.blend_onbeat && membeat > 0) frame_output=config.output_onbeat; else frame_output=config.output;
	int fbpos=w*h;
	int l=config.chromavalue/2; // chroma distance
	l=l*l;

	int sourcesize;
	switch(currentMode)
	{
		case modeJpeg:
			sourcesize=pictureWidth*pictureHeight*3-3;
			break;
		case modePng:
			sourcesize=pictureWidth*pictureHeight*pictureChannels-pictureChannels;
			break;
		case modeGif:
			sourcesize=pictureWidth*pictureHeight-1;
			break;
	}
	
	for(int y=0;y<h;y++) {
		for(int x=w-1;x>-1;x--) {
			drawThisPixel=true;
			fbpos--;
			switch(currentMode)
			{
				case modePng:
					pixbase= sourcesize-(((w-x)*pictureWidth/w+y*pictureHeight/h*pictureWidth)*pictureChannels);
					if(pixbase<0)
						pixbase=0;
					R=pJpegData[pixbase];
					G=pJpegData[++pixbase];
					B=pJpegData[++pixbase];
					if(pictureChannels==4) // alpha channel
					{
						int pixel = colorblend( framebuffer[fbpos], B|G<<8|R<<16, (UINT8)(255-pJpegData[++pixbase]));
						R = (pixel&0xff0000)>>16;
						G = (pixel&0xff00)>>8;
						B = pixel&0xff;
					}
					break;
			
				case modeJpeg:
					pixbase= sourcesize-(((w-x)*pictureWidth/w+y*pictureHeight/h*pictureWidth)*3);
					R=pJpegData[pixbase];
					G=pJpegData[++pixbase];
					B=pJpegData[++pixbase];
					break;

				case modeGif:
					pixbase=sourcesize-(((w-x)*pictureWidth/w+y*pictureHeight/h*pictureWidth));
					R = palR[pGifData[pixbase]];
					G = palG[pGifData[pixbase]];
					B = palB[pGifData[pixbase]];
					break;

				case modeVideo:
					if(colordepth==24)
					{ // 24 bits
						pixbase=(x*width/w+y*height/h*width)*3; // simple stretching
						B=pData[pixbase];//&0xff;	// B from aviframe buffer
						G=pData[++pixbase];//&0xff; // G		""
						R=pData[++pixbase];//&0xff; // R		""
					}
					else
					{ // 8 bits palettized
						pixbase=(x*width/w + y*height/h  *width);
						R=palR[pData[pixbase]];
						G=palG[pData[pixbase]];
						B=palB[pData[pixbase]];
					}
					break;
			}
			
			if(config.lumablack ) {// luma on black
				L=(R+G+B)/3; // calcul luminance simple moyenne
				drawThisPixel= L>=config.black;
				if(config.invblack) drawThisPixel=!drawThisPixel;
			}
			if(config.lumawhite && drawThisPixel) {// luma on white
				L=(R+G+B)/3; // calcul luminance simple moyenne
				drawThisPixel= L<=0x100-config.white;
				if(config.invwhite) drawThisPixel=!drawThisPixel;
			}
			if(config.chroma && drawThisPixel ){//&& config.chromavalue>0) { // chroma
				int r=R-Rd,g=G-Gd,b=B-Bd;
				drawThisPixel = !(r*r+g*g+b*b < l);
				if(config.invchroma) drawThisPixel=!drawThisPixel;
			}
	
			// draw pixel
			if(drawThisPixel) {
				switch (frame_output) {
					case OUT_REPLACE:
						framebuffer[fbpos]=B|G<<8|R<<16;
						break;
					case OUT_ADDITIVE:
						framebuffer[fbpos] = blendADDITIVE(B|G<<8|R<<16,framebuffer[fbpos]);
						break;
					case OUT_MAXIMUM:
						framebuffer[fbpos] = maxblend(B|G<<8|R<<16,framebuffer[fbpos]);
						break;
					case OUT_MINIMUM:
						framebuffer[fbpos] = minblend(B|G<<8|G<<16,framebuffer[fbpos]);
						break;
					case OUT_5050: 
						framebuffer[fbpos] = blend5050(B|G<<8|R<<16,framebuffer[fbpos]); 
						break;
					case OUT_SUB1:
						framebuffer[fbpos] = blendSUB1(B|G<<8|R<<16,framebuffer[fbpos]); 
						break;
					case OUT_SUB2:
						framebuffer[fbpos] = blendSUB2(B|G<<8|R<<16,framebuffer[fbpos]); 
						break;
					case OUT_MULTIPLY:
						framebuffer[fbpos] = blendMULTIPLY(B|G<<8|R<<16,framebuffer[fbpos]); 
						break;
					case OUT_XOR:
						framebuffer[fbpos] = blendXOR(B|G<<8|R<<16,framebuffer[fbpos]); 
						break;
					case OUT_ADJUSTABLE:
						framebuffer[fbpos]=colorblend(framebuffer[fbpos], B|G<<8|R<<16, adjustable);
						break;
					case OUT_IGNORE:
						break;
				}
			}
		}
	}
	_asm
		emms;
	
	return 0;
}

HWND C_VFXAVIPLAYER::conf(HINSTANCE hInstance, HWND hwndParent) 
{
	g_ConfigThis = this;
	hInst=hInstance; // keep track of instance 
	return CreateDialog(hInstance, MAKEINTRESOURCE(IDD_CONFIG), hwndParent, (DLGPROC)g_DlgProc);
}


char *C_VFXAVIPLAYER::get_desc(void)
{ 
	return MOD_NAME; 
}

//////////////////////////////////////////////////
// fills filelist array with enabled files type //
//////////////////////////////////////////////////
void C_VFXAVIPLAYER::PopulateFileList()
{
	// if avi path is not found use avs directory
	if (!(GetFileAttributes(config.avipath) & FILE_ATTRIBUTE_DIRECTORY)) 
	{
		GetModuleFileName(g_hDllInstance, config.avipath, MAX_PATH);
		strcpy(strrchr(config.avipath, '\\') + 1, "");
	}
	// fill filelist
	totfiles=0;
	WIN32_FIND_DATA wfd;
	HANDLE h;
	char buf[MAX_PATH];
	if(config.enable_videos)
	{
		strcpy(buf,config.avipath);
		strcat(buf,"\\*.avi");
		h = FindFirstFile(buf, &wfd);
		if (h != INVALID_HANDLE_VALUE) 
		{
			bool rep = true;
			while (rep) 
			{
				totfiles++;
				strcpy(filename[totfiles-1],wfd.cFileName);
				filelist[totfiles-1]=filename[totfiles-1];
				if (!FindNextFile(h, &wfd)) 
				{
					rep = false;
				}
			};
		}
		FindClose(h);
	}
	if(config.enable_pictures)
	{
		strcpy(buf,config.avipath);
		strcat(buf,"\\*.jpg");
		h = FindFirstFile(buf, &wfd);
		if (h != INVALID_HANDLE_VALUE) 
		{
			bool rep = true;
			while (rep) 
			{
				totfiles++;
				strcpy(filename[totfiles-1],wfd.cFileName);
				filelist[totfiles-1]=filename[totfiles-1];
				if (!FindNextFile(h, &wfd)) 
				{
					rep = false;
				}
			};
		}
		FindClose(h);
		strcpy(buf,config.avipath);
		strcat(buf,"\\*.jpeg");
		h = FindFirstFile(buf, &wfd);
		if (h != INVALID_HANDLE_VALUE) 
		{
			bool rep = true;
			while (rep) 
			{
				totfiles++;
				strcpy(filename[totfiles-1],wfd.cFileName);
				filelist[totfiles-1]=filename[totfiles-1];
				if (!FindNextFile(h, &wfd)) 
				{
					rep = false;
				}
			};
		}
		FindClose(h);
		strcpy(buf,config.avipath);
		strcat(buf,"\\*.gif");
		h = FindFirstFile(buf, &wfd);
		if (h != INVALID_HANDLE_VALUE) 
		{
			bool rep = true;
			while (rep) 
			{
				totfiles++;
				strcpy(filename[totfiles-1],wfd.cFileName);
				filelist[totfiles-1]=filename[totfiles-1];
				if (!FindNextFile(h, &wfd)) 
				{
					rep = false;
				}
			};
		}
		FindClose(h);
		strcpy(buf,config.avipath);
		strcat(buf,"\\*.png");
		h = FindFirstFile(buf, &wfd);
		if (h != INVALID_HANDLE_VALUE) 
		{
			bool rep = true;
			while (rep) 
			{
				totfiles++;
				strcpy(filename[totfiles-1],wfd.cFileName);
				filelist[totfiles-1]=filename[totfiles-1];
				if (!FindNextFile(h, &wfd)) 
				{
					rep = false;
				}
			};
		}
		FindClose(h);
	}
	if(totfiles>-1) 
	{ // then sort list
		alphasort(filelist,totfiles);
		// try to find file to load pos# in list
		for(int i=0;i<totfiles;i++) 
		{
			if(!stricmp(filelist[i],config.image))
			curfile=i;
		}
	}
}

void C_VFXAVIPLAYER::load_config(unsigned char *data, int len) 
{
	if (len <= sizeof(apeconfig)){
		memcpy(&this->config, data, len);
		unsigned char R,G,B;
		R = config.chromakey>>16;
		G = config.chromakey>>8;
		B = config.chromakey;
		// compute desired rgb for chroma keying
		Rd = (config.chromakey>>16)&255;
		Gd = (config.chromakey>>8)&255;
		Bd = config.chromakey&255;
		// compatibility with v1.0-v1.04
		if (config.blendadjustable) {
			config.output= OUT_ADJUSTABLE;
			config.blendadjustable = false;
		}
		if (config.replace) {
			config.output = OUT_REPLACE;
			config.replace = false;
		}
		if (config.additive) {
			config.output = OUT_ADDITIVE;
			config.additive = false;
		}
		if (config.blend) {
			config.output = OUT_5050;
			config.blend = false;
		}
	}
	else {
		memset(&this->config, 0, sizeof(apeconfig));
	}
	PopulateFileList();
	reload=true;
}


int  C_VFXAVIPLAYER::save_config(unsigned char *data) 
{
	memcpy(data, &this->config, sizeof(apeconfig));
	return sizeof(apeconfig);
}

C_RBASE *R_RetrFunc(char *desc) 
{
	if (desc) { 
		strcpy(desc,MOD_NAME); 
		return NULL; 
	} 
	return (C_RBASE *) new C_VFXAVIPLAYER();
}

extern "C"
{
	__declspec (dllexport) int _AVS_APE_RetrFunc(HINSTANCE hDllInstance, char **info, int *create)
	{
		g_hDllInstance=hDllInstance;
		*info=UNIQUEIDSTRING;
		*create=(int)(void*)R_RetrFunc;
		return 1;
	}
};
