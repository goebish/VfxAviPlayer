// AVS APE (Plug-in Effect) header

#pragma warning( disable : 4799 ) // MMX has no emms ...
// base class to derive from
class C_RBASE {
	public:
		C_RBASE() { }
		virtual ~C_RBASE() { };
		virtual int render(char visdata[2][2][576], int isBeat, int *framebuffer, int *fbout, int w, int h)=0; // returns 1 if fbout has dest, 0 if framebuffer has dest
		virtual HWND conf(HINSTANCE hInstance, HWND hwndParent){return 0;};
		virtual char *get_desc()=0;
		virtual void load_config(unsigned char *data, int len) { }
		virtual int  save_config(unsigned char *data) { return 0; }
};
struct workAround {
	C_RBASE* this_thread;
};

#ifndef IDC_HAND
#define IDC_HAND MAKEINTRESOURCE(32649)
#endif

#define IDM_ADDPOINT     61001
#define IDM_EDITPOINT    61002
#define IDM_DELPOINT     61003

// this will be the directory and APE name displayed in the AVS Editor
#define MOD_NAME "Virtual Effect / Avi Player"

// this is how AVS will recognize this APE internally
#define UNIQUEIDSTRING "VFX AVI PLAYER"

struct apeconfig { // vars config (loaded/saved)
	char image[MAX_PATH];  // filename (without path)
	int enabled;	// ape enabled
	int lumablack;	
	int lumawhite;
	int replace;
	int additive;
	int blend;
	int blendadditive;
	int beatpersistance;
	int speed;
	int black;
	int white;
	int chroma;
	int chromakey;
	int chromavalue;
	int paused;
	int frameskip;
	char avipath[MAX_PATH];
	int invblack; // check invert on luma black
	int invwhite;
	int invchroma;
	int enable_auto; // auto stream change
	int end_of_stream;
	int x_beats;
	int x_seconds;
	int autovalue;
	int next;
	int previous;
	int random;
	int blendadjustable;
	int output; // blend mode
	int output_onbeat; // blend mode onbeat
	int blend_value; // adjustable blend value
	int blend_value_onbeat;
	int blend_onbeat; // switch blend mode on beat

	// v1.06-
	int skiponbeat; // different frameskip on beat ?
	int frameskip_onbeat; // frameskip on beat value
	int reverse_onbeat; // reverse playing on beat ?

	// v1.07-
	int enable_videos;
	int enable_pictures;
	int enable_transition;
};

#define OUT_REPLACE       0
#define OUT_ADDITIVE      1
#define OUT_MAXIMUM       2
#define OUT_MINIMUM       3
#define OUT_5050          4
#define OUT_SUB1          5
#define OUT_SUB2          6
#define OUT_MULTIPLY      7
#define OUT_XOR           8
#define OUT_ADJUSTABLE    9
#define OUT_IGNORE       10
#define OUT_DEFAULT		 11

enum e_currentMode{
	modeVideo,
	modeJpeg,
	modeGif,
	modePng
};

static char *outputs[] = { "Replace", "Additive", "Maximum", "Minimum", "50/50", "Subtractive1", "Subtractive2", "Multiply", "Xor", "Adjustable", "Ignore","Default renderer" };

//////////////////////////////
///  MMX optimised blends  ///
//////////////////////////////

// out adjustable blend
static unsigned int __inline colorblend(unsigned int a, unsigned int b, unsigned int t) {
	static unsigned __int64 salpha = 0x0100010001000100;
	static unsigned __int32 alpha[] = { 0x0, 0x0 };
	UINT color;
	__asm {
		// Check if full or no blend, return only source or dest color
		mov		  eax,t;
		test	  eax,eax; 
		je		  full;
		cmp		  eax,255; 
		jne		  start;
		mov		  eax,a; // return background color
		mov		  color,eax;
		jmp		  fin;
		full:	// return foreground color
		mov		  eax,b;
		mov		  color,eax;
		jmp		  fin;
		// color blend routine start here
		// Duplicate t into 64-bit
		start:
		mov       edx, eax;
		shl       eax, 16;
		or        eax, edx;
		mov       [alpha], eax;
		mov       [alpha+4], eax;
		// Load colors and duplicated t
		pxor	  mm4,mm4;
		movd      mm0, a;
		movd      mm1, b;
		movq      mm2, alpha;
		// Calc (256 - t)
		movq      mm3, salpha;
		psubusw   mm3, alpha;
		// Unpack colors
		punpcklbw mm0, mm4;
		punpcklbw mm1, mm4;
		// Multiply by weights
		pmullw    mm0, mm2;
		pmullw    mm1, mm3;
		// Add and shift
		paddusw   mm0, mm1;
		psrlw     mm0, 8;
		// Pack and save
		packuswb  mm0, mm0;
		movd      color, mm0;
		//emms;
		fin:
	}
	return color;
};

// out additive blend
static unsigned int __inline blendADDITIVE(unsigned int a, unsigned int b) {
	unsigned int tmp;
	__asm {
		movq mm0, a
		movq mm1, b
		paddusb mm0, mm1
		movq tmp,mm0
	}
	return tmp;
};

// out maximum blend
static unsigned int __inline maxblend(unsigned int a, unsigned int b) {
    unsigned int tmp;
    __asm {
        movd    mm0, a
        movd    mm1, b
        PSUBUSB mm0, mm1
        PADDB   mm0, mm1
        movd    tmp, mm0
    }
    return tmp;
};

// out 50/50
static unsigned int __inline blend5050(unsigned int a, unsigned int b) {
	unsigned int tmp;
	__asm {
		pxor mm7, mm7
		movd mm0, a 
		movd mm1, b 
		punpcklbw mm0, mm7
		punpcklbw mm1, mm7
		paddusw mm0, mm1
		psrlw mm0, 1
		packuswb mm0, mm0
		movd tmp, mm0 
	}
	return tmp;
};

// out Substractive 1
static unsigned int __inline blendSUB1(unsigned int a, unsigned int b) {
	unsigned int tmp;
	__asm {
		movq mm0, b
		psubusb mm0, a
		movq tmp, mm0
	}
	return tmp;
};

// out Substractive 2
static unsigned int __inline blendSUB2(unsigned int a, unsigned int b) {
	unsigned int tmp;
	__asm {
		movq mm0, a
		psubusb mm0, b
		movq tmp, mm0
	}
	return tmp;
};

//out multiply
static unsigned int __inline blendMULTIPLY(unsigned int a, unsigned int b) {
	unsigned int tmp;
	__asm {
		pxor mm7, mm7
		movd mm0, a 
		movd mm1, b 
		punpcklbw mm0, mm7;
		punpcklbw mm1, mm7;
		pmullw mm0, mm1;
		psrlw mm0, 8;
		packuswb mm0, mm0;
		movd tmp, mm0 
	}
	return tmp;
};

// out XOR
static unsigned int __inline blendXOR(unsigned int a, unsigned int b) {
	unsigned int tmp;
	__asm {
		movq mm0, a 
		pxor mm0, b 
		movq tmp, mm0 
	}
	return tmp;
};

// out minimum blend
static unsigned int __inline minblend(unsigned int a, unsigned int b) {
    unsigned int tmp;
    __asm {
        movd    mm0, a
        movd    mm1, b
        MOVQ    mm2, mm0
        PSUBUSB mm2, mm1
        PSUBUSB mm0, mm2
        movd    tmp, mm0
    }
    return tmp;
};

//extended APE stuff

// to use this, you should have:
// APEinfo *g_extinfo;
// void __declspec(dllexport) AVS_APE_SetExtInfo(HINSTANCE hDllInstance, APEinfo *ptr)
// {
//    g_extinfo = ptr;
// }

typedef void *VM_CONTEXT;
typedef void *VM_CODEHANDLE;
typedef struct
{
  int ver; // ver=1 to start
  double *global_registers; // 100 of these

  // lineblendmode: 0xbbccdd
  // bb is line width (minimum 1)
  // dd is blend mode:
     // 0=replace
     // 1=add
     // 2=max
     // 3=avg
     // 4=subtractive (1-2)
     // 5=subtractive (2-1)
     // 6=multiplicative
     // 7=adjustable (cc=blend ratio)
     // 8=xor
     // 9=minimum
  int *lineblendmode; 

  //evallib interface
  VM_CONTEXT (*allocVM)(); // return a handle
  void (*freeVM)(VM_CONTEXT); // free when done with a VM and ALL of its code have been freed, as well

  // you should only use these when no code handles are around (i.e. it's okay to use these before
  // compiling code, or right before you are going to recompile your code. 
  void (*resetVM)(VM_CONTEXT);
  double * (*regVMvariable)(VM_CONTEXT, char *name);

  // compile code to a handle
  VM_CODEHANDLE (*compileVMcode)(VM_CONTEXT, char *code);

  // execute code from a handle
  void (*executeCode)(VM_CODEHANDLE, char visdata[2][2][576]);

  // free a code block
  void (*freeCode)(VM_CODEHANDLE);

  // requires ver >= 2
  void (*doscripthelp)(HWND hwndDlg,char *mytext); // mytext can be NULL for no custom page

  /// requires ver >= 3
  void *(*getNbuffer)(int w, int h, int n, int do_alloc); // do_alloc should be 0 if you dont want it to allocate if empty
                                                          // w and h should be the current width and height
                                                          // n should be 0-7

} APEinfo;