/**
 * @example SDLvncviewer.c
 * Once built, you can run it via `SDLvncviewer <remote-host>`.
 */

#include <malloc.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <3ds.h>
#include <SDL/SDL.h>
#include <rfb/rfbclient.h>

struct { int sdl; int rfb; } buttonMapping[]={
	{1, rfbButton1Mask},
	{2, rfbButton2Mask},
	{3, rfbButton3Mask},
	{4, rfbButton4Mask},
	{5, rfbButton5Mask},
	{0,0}
};

static int viewOnly=0, buttonMask;
/* client's pointer position */
int x,y;
rfbClient* cl;

// default
struct {
	char *name;
	unsigned int key;
	SDLKey sdl_key;
	rfbKeySym rfb_key;
} buttons3ds[] = {
	{"A", KEY_A, SDLK_a,	XK_a},
	{"B", KEY_B, SDLK_b, XK_b},
	{"X", KEY_X, SDLK_x, XK_x},
	{"Y", KEY_Y, SDLK_y, XK_y},
	{"L", KEY_L, SDLK_q, XK_q},
	{"R", KEY_R, SDLK_w, XK_w},
	{"ZL", KEY_ZL, SDLK_1, XK_1},
	{"ZR", KEY_ZR, SDLK_2, XK_2},
	{"START", KEY_START, SDLK_RETURN, XK_Return},
	{"SELECT", KEY_SELECT, SDLK_ESCAPE, XK_Escape},

	{"CPAD_UP", KEY_CPAD_UP, SDLK_UP, XK_Up}, // C-PAD
	{"CPAD_DOWN", KEY_CPAD_DOWN, SDLK_DOWN, XK_Down},
	{"CPAD_LEFT", KEY_CPAD_LEFT, SDLK_LEFT, XK_Left},
	{"CPAD_RIGHT", KEY_CPAD_RIGHT, SDLK_RIGHT, XK_Right},

	{"DPAD_UP", KEY_DUP, SDLK_t, XK_t},	// D-PAD
	{"DPAD_DOWN", KEY_DDOWN, SDLK_g, XK_g},
	{"DPAD_LEFT", KEY_DLEFT, SDLK_f, XK_f},
	{"DPAD_RIGHT", KEY_DRIGHT, SDLK_h, XK_h},

	{"CSTCK_UP", KEY_CSTICK_UP, SDLK_i, XK_i}, // C-STICK
	{"CSTCK_DOWN", KEY_CSTICK_DOWN, SDLK_k, XK_k},
	{"CSTCK_LEFT", KEY_CSTICK_LEFT, SDLK_j, XK_j},
	{"CSTCK_RIGHT", KEY_CSTICK_RIGHT, SDLK_l, XK_l},

	{NULL, 0,0,0}
};
/*
// breath of the wild in cemu
struct {
	unsigned int key;
	SDLKey sdl_key;
	rfbKeySym rfb_key;
} buttons3ds[] = {
	{KEY_A, SDLK_a,	XK_space},
	{KEY_B, SDLK_b, XK_e},
	{KEY_X, SDLK_x, XK_q},
	{KEY_Y, SDLK_y, XK_c},
	{KEY_L, SDLK_q, XK_1},
	{KEY_R, SDLK_w, XK_4},
	{KEY_ZL, SDLK_1, XK_2},
	{KEY_ZR, SDLK_2, XK_3},
	{KEY_START, SDLK_RETURN, XK_plus},
	{KEY_SELECT, SDLK_ESCAPE, XK_minus},

	{KEY_CPAD_UP, SDLK_UP, XK_w}, // C-PAD
	{KEY_CPAD_DOWN, SDLK_DOWN, XK_s},
	{KEY_CPAD_LEFT, SDLK_LEFT, XK_a},
	{KEY_CPAD_RIGHT, SDLK_RIGHT, XK_d},

	{KEY_DUP, SDLK_t, XK_t},	// D-PAD
	{KEY_DDOWN, SDLK_g, XK_g},
	{KEY_DLEFT, SDLK_f, XK_f},
	{KEY_DRIGHT, SDLK_h, XK_h},

	{KEY_CSTICK_UP, SDLK_i, XK_KP_8}, // C-STICK
	{KEY_CSTICK_DOWN, SDLK_k, XK_KP_5},
	{KEY_CSTICK_LEFT, SDLK_j, XK_KP_4},
	{KEY_CSTICK_RIGHT, SDLK_l, XK_KP_6},

	{0,0,0}
};
*/
static void vlog_citra(const char *format, va_list arg ) {
	char buf[2000];
    vsnprintf(buf, 2000, format, arg);
	int i=strlen(buf);
	svcOutputDebugString(buf, i);
	printf("%s",buf);
	if (i && buf[i-1] != '\n')
		printf("\n");
}

void log_citra(const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);
	vlog_citra(format, argptr);
    va_end(argptr);
}

static rfbBool resize(rfbClient* client) {
	int width=client->width;
	int height=client->height;
	int depth=client->format.bitsPerPixel;
//log_citra("%s: %d %d %d",__func__,width,height,depth);

	if (width > 1024 || height > 1024) {
		rfbClientErr("resize: screen size >1024px not supported");
		return FALSE;
	}

	client->updateRect.x = client->updateRect.y = 0;
	client->updateRect.w = width;
	client->updateRect.h = height;

	/* (re)create the surface used as the client's framebuffer */
	int flags = SDL_TOPSCR;
	if (width > 400 || height > 240) {
		flags |= ((width * 1000) / 400 > (height * 1000) / 240)? SDL_FITWIDTH : SDL_FITHEIGHT;
	}
	SDL_Surface* sdl = SDL_SetVideoMode(width, height, depth, flags);

	if(!sdl) {
		rfbClientErr("resize: error creating surface: %s", SDL_GetError());
		return FALSE;
	}

	SDL_ShowCursor(SDL_DISABLE);
	rfbClientSetClientData(client, SDL_Init, sdl);
	client->width = sdl->pitch / (depth / 8);
	client->frameBuffer=sdl->pixels;

	client->format.bitsPerPixel=depth;
	client->format.redShift=sdl->format->Rshift;
	client->format.greenShift=sdl->format->Gshift;
	client->format.blueShift=sdl->format->Bshift;

	client->format.redShift=sdl->format->Rshift;
	client->format.greenShift=sdl->format->Gshift;
	client->format.blueShift=sdl->format->Bshift;

	client->format.redMax=sdl->format->Rmask>>client->format.redShift;
	client->format.greenMax=sdl->format->Gmask>>client->format.greenShift;
	client->format.blueMax=sdl->format->Bmask>>client->format.blueShift;
	SetFormatAndEncodings(client);

	return TRUE;
}

static rfbKeySym SDL_key2rfbKeySym(SDL_KeyboardEvent* e) {
	SDLKey sym = e->keysym.sym;

	// init 3DS buttons to their values
	for (int i=0; buttons3ds[i].key!=0; ++i)
		if (sym == buttons3ds[i].sdl_key) return buttons3ds[i].rfb_key;
	return 0;
}

/*
static void update(rfbClient* cl,int x,int y,int w,int h) {
}

static void kbd_leds(rfbClient* cl, int value, int pad) {
	// note: pad is for future expansion 0=unused
	rfbClientErr("Led State= 0x%02X", value);
}

// trivial support for textchat
static void text_chat(rfbClient* cl, int value, char *text) {
	switch(value) {
	case rfbTextChatOpen:
		rfbClientErr("TextChat: We should open a textchat window!");
		TextChatOpen(cl);
		break;
	case rfbTextChatClose:
		rfbClientErr("TextChat: We should close our window!");
		break;
	case rfbTextChatFinished:
		rfbClientErr("TextChat: We should close our window!");
		break;
	default:
		rfbClientErr("TextChat: Received \"%s\"", text);
		break;
	}
	fflush(stderr);
}
*/

static void cleanup()
{
  if(cl)
    rfbClientCleanup(cl);
  cl = NULL;
}

static void safeexit();

static rfbBool handleSDLEvent(rfbClient *cl, SDL_Event *e)
{
	switch(e->type) {
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEMOTION:
	{
		int state, i;
		if (viewOnly)
			break;

		if (e->type == SDL_MOUSEMOTION) {
			x = e->motion.x;
			y = e->motion.y;
			state = e->motion.state;
		}
		else {
			x = e->button.x;
			y = e->button.y;
			state = e->button.button;
			for (i = 0; buttonMapping[i].sdl; i++)
				if (state == buttonMapping[i].sdl) {
					state = buttonMapping[i].rfb;
					if (e->type == SDL_MOUSEBUTTONDOWN)
						buttonMask |= state;
					else
						buttonMask &= ~state;
					break;
				}
		}
		SendPointerEvent(cl, x, y, buttonMask);
		buttonMask &= ~(rfbButton4Mask | rfbButton5Mask);
		break;
	}
	case SDL_KEYUP:
	case SDL_KEYDOWN:
		if (viewOnly)
			break;
		rfbKeySym s =  SDL_key2rfbKeySym(&e->key);
		if (s <= 0) return 0;
		SendKeyEvent(cl, s, e->type == SDL_KEYDOWN ? TRUE : FALSE);
		break;
	case SDL_QUIT:
		safeexit();
	default:
		rfbClientLog("ignore SDL event: 0x%x", e->type);
	}
	return TRUE;
}

//static void get_selection(rfbClient *cl, const char *text, int len){}
char *user1, *pass1;

static char* get_password(rfbClient* cl) {
	char* p=calloc(1,strlen(pass1)+1);
	strcpy(p, pass1);
	return p;
}

static rfbCredential* get_credential(rfbClient* cl, int credentialType) {
	rfbCredential *c = malloc(sizeof(rfbCredential));
	c->userCredential.username = malloc(RFB_BUF_SIZE);
	c->userCredential.password = malloc(RFB_BUF_SIZE);

	if(credentialType != rfbCredentialTypeUser) {
	    rfbClientErr("Something else than username and password required for authentication");
	    return NULL;
	}

	rfbClientLog("Username and password required for authentication!");
	snprintf(c->userCredential.username, RFB_BUF_SIZE, "%s", user1);
	snprintf(c->userCredential.password, RFB_BUF_SIZE, "%s", pass1);
	return c;
}

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000
static u32 *SOC_buffer = NULL;

typedef struct {
	char name[128];
	char host[128];
	int port;
	char user[128];
	char pass[128];
} vnc_config;

#define NUMCONF 25
const char *config_filename = "/3ds/TinyVNC/vnc.cfg";
vnc_config conf[NUMCONF] = {0};
int cpy = -1;

static int mkpath(char* file_path, int complete) {
	char* p;

	for (p=strchr(file_path+1, '/'); p; p=strchr(p+1, '/')) {
		*p='\0';
		if (mkdir(file_path, 0644)==-1) {
			if (errno!=EEXIST) { *p='/'; goto mkpath_err; }
		}
		*p='/';
	}
	if (complete) {
		mkdir(file_path, 0644);
	}
	return 0;
mkpath_err:
	return 1;
}

static void printlist(int num) {
	char buf[41];
	printf(	"\x1b[2J\x1b[H\x1b[7m"
			" Choose VNC Server                      ");
	if (cpy!=-1) {
		snprintf(buf,41,"L:paste '%s'                              ",conf[cpy].name);
		printf("\x1b[29;0H%s", buf);
	}
	printf(	"\x1b[30;0H"
			"A:select B:quit X:delete Y:copy R:edit  "
			"\x1b[0m");
	for (int i=0;i<NUMCONF; ++i) {
		printf("\x1b[%d;0H", i+3);
		if (i==num) printf("\x1b[7m");
		printf("%-40.40s", conf[i].host[0]?conf[i].name:"-");
		if (i==num) printf("\x1b[0m");
	}
}

static void saveconfig() {
	FILE *f;
	if((f=fopen(config_filename, "w"))!=NULL) {
		fwrite((void*)conf, sizeof(vnc_config), NUMCONF, f);
		fclose(f);
	}
}

static int editconfig(vnc_config *c) {
	int sel = 1;
	const char *sep = "----------------------------------------";
	const char *pass = "****************************************";
	char input[128];
	int ret = 0;
	static SwkbdState swkbd;
	SDL_Event e;
	SwkbdButton button;

	vnc_config nc = *c;
	if (nc.port <= 0) nc.port = 5900; // VNC default port
	int upd = 1;

	while (ret==0) {
		if (upd) {
			printf(	"\x1b[2J\x1b[H\x1b[7m"
					" Edit VNC Server                        ");
			printf(	"\x1b[30;0H"
					"A:edit B:cancel Y:OK                    "
					"\x1b[0m");
			printf(	"\x1b[3;0HName: ");
			if (sel == 0) printf("\x1b[7m");
			printf(	"%-34.34s", nc.name);
			if (sel == 0) printf("\x1b[0m");

			printf(	"\x1b[5;0H%s", sep);
			printf(	"\x1b[7;0HHost: ");
			if (sel == 1) printf("\x1b[7m");
			printf(	"%-34.34s", nc.host);
			if (sel == 1) printf("\x1b[0m");
			printf(	"\x1b[9;0HPort: ");
			if (sel == 2) printf("\x1b[7m");
			printf(	"%-34d", nc.port);
			if (sel == 2) printf("\x1b[0m");
			printf(	"\x1b[11;0H%s", sep);
			printf(	"\x1b[13;0HUsername: ");
			if (sel == 3) printf("\x1b[7m");
			printf(	"%-30.30s", nc.user);
			if (sel == 3) printf("\x1b[0m");
			printf(	"\x1b[15;0HPassword: ");
			if (sel == 4) printf("\x1b[7m");
			printf(	"%*.*s%*s", strlen(nc.pass), strlen(nc.pass), pass, 30-strlen(nc.pass), "");
			if (sel == 4) printf("\x1b[0m");
			SDL_Flip(SDL_GetVideoSurface());
		}
		while (!SDL_PollEvent(&e)) SDL_Delay(20);
		if (e.type == SDL_QUIT)
			safeexit();
		if (e.type == SDL_KEYDOWN) {
			switch (e.key.keysym.sym) {
			case SDLK_b:
				ret=-1; break;
			case SDLK_y:
				ret=1; break;
			case SDLK_DOWN:
			case SDLK_g:
			case SDLK_k:
				sel = (sel+1) % 5;
				upd = 1;
				break;
			case SDLK_UP:
			case SDLK_t:
			case SDLK_i:
				sel = (sel + 4) % 5;
				upd = 1;
				break;
			case SDLK_a:
				upd = 1;
				switch (sel) {
				case 0: // entry name
					swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, sizeof(input));
					swkbdSetHintText(&swkbd, "Entry Name");
					swkbdSetInitialText(&swkbd, nc.name);
					swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
					button = swkbdInputText(&swkbd, input, sizeof(input));
					if(button != SWKBD_BUTTON_LEFT)
						strcpy(nc.name, input);
					break;
				case 1: // hostname
					swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, sizeof(input));
					swkbdSetHintText(&swkbd, "Host Name / IP address");
					swkbdSetInitialText(&swkbd, nc.host);
					swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
					button = swkbdInputText(&swkbd, input, sizeof(input));
					if(button != SWKBD_BUTTON_LEFT) {
						strcpy(nc.host, input);
						if (!nc.name[0])
							strcpy(nc.name, input);
					}
					break;
				case 2: // port
					swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 2, 6);
					swkbdSetHintText(&swkbd, "Port");
					sprintf(input, "%d", nc.port);
					swkbdSetInitialText(&swkbd, input);
					//swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
					button = swkbdInputText(&swkbd, input, 6);
					if(button != SWKBD_BUTTON_LEFT)
						nc.port = atoi(input);
					break;
				case 3: // username
					swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, sizeof(input));
					swkbdSetHintText(&swkbd, "Username");
					swkbdSetInitialText(&swkbd, nc.user);
					swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
					button = swkbdInputText(&swkbd, input, sizeof(input));
					if(button != SWKBD_BUTTON_LEFT)
						strcpy(nc.user, input);
					break;
				case 4: // password
					swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 2, sizeof(input));
					swkbdSetHintText(&swkbd, "Password");
					swkbdSetInitialText(&swkbd, nc.pass);
					swkbdSetFeatures(&swkbd, SWKBD_DEFAULT_QWERTY);
					swkbdSetPasswordMode(&swkbd, SWKBD_PASSWORD_HIDE_DELAY);
					button = swkbdInputText(&swkbd, input, sizeof(input));
					if(button != SWKBD_BUTTON_LEFT)
						strcpy(nc.pass, input);
					break;
				}
				break;
			default:
				break;
			}
		}
	}
	if (ret == 1) {
		memcpy(c, &nc, sizeof(nc));
		return 0;
	}
	return -1;
}

static int getconfig(vnc_config *c) {
	static int isinit = 0;
	if (!isinit) {
		FILE *f;
		if((f=fopen(config_filename, "r"))!=NULL) {
			fread((void*)conf, sizeof(vnc_config), NUMCONF, f);
			fclose(f);
		}
		isinit = 1;
	}

	SDL_Event e;
	int ret=-1;
	int sel=0;

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	int upd = 1;
	while (ret == -1) {
		if (upd) {
			printlist(sel);
			upd=0;
		}
		while (!SDL_PollEvent(&e)) SDL_Delay(20);
		if (e.type == SDL_QUIT)
			safeexit();
		if (e.type == SDL_KEYDOWN) {
			switch (e.key.keysym.sym) {
			case SDLK_b:
				ret=1; break;
			case SDLK_DOWN:
			case SDLK_g:
			case SDLK_k:
				sel = (sel+1) % NUMCONF;
				upd = 1;
				break;
			case SDLK_UP:
			case SDLK_t:
			case SDLK_i:
				sel = (sel+NUMCONF-1) % NUMCONF;
				upd = 1;
				break;
			case SDLK_x:
				memset(&(conf[sel]), 0, sizeof(vnc_config));
				upd = 1;
				break;
			case SDLK_y:
				if (conf[sel].host[0])
					cpy = sel;
				else cpy = -1;
				upd = 1;
				break;
			case SDLK_q:
				if (cpy != -1)
					memcpy(&(conf[sel]), &(conf[cpy]), sizeof(vnc_config));
				cpy = -1;
				upd = 1;
				break;
			case SDLK_a:
				if (conf[sel].host[0]) {
					memcpy(c, &(conf[sel]), sizeof(vnc_config));
					ret=0;
				} else {
					editconfig(&(conf[sel]));
				}
				upd = 1;
				break;
			case SDLK_w:
				editconfig(&(conf[sel]));
				upd = 1;
				break;
			default:
				break;
			}
		}
	}
	SDL_EnableKeyRepeat(0,0);
	cpy = -1;
	return (ret ? -1 : 0);
}

static void safeexit() {
	cleanup();
	saveconfig();
	socExit();
	SDL_Quit();
	exit(0);
}

const char *keymap_filename = "/3ds/TinyVNC/keymap";
#define BUFSIZE 1024

static void readkeymaps(char *cname) {
	FILE *f=NULL;
	int i, x;
	char name[32], *p=NULL, *fn=NULL, buf[BUFSIZE];
	if (cname && cname[0]) {
		p=malloc(strlen(keymap_filename)+2+strlen(cname));
		strcpy(p, keymap_filename);
		sprintf(strrchr(p,'/'),"/%s.keymap", cname);
	}
	if( (p && (f=fopen(p, "r"))!=NULL && (fn=p)) ||
		((f=fopen(keymap_filename, "r"))!=NULL && (fn=(char*)keymap_filename))) {
		rfbClientLog("Reading keymap from %s", fn);
		while (fgets(buf, BUFSIZE, f)) {
			if (buf[0]=='#' || sscanf(buf,"%s %x\n",name, &x) != 2) continue;
			for (i=0; buttons3ds[i].key!=0; ++i) {
				if (strcmp(buttons3ds[i].name, name)==0) {
					buttons3ds[i].rfb_key=x;
					break;
				}
			}
		}
		fclose(f);
	} else if ((f=fopen(keymap_filename, "w"))!=NULL) {
		rfbClientLog("Saving standard keymap to %s", keymap_filename);
		
		fprintf(f,
			"# mappings as per https://libvnc.github.io/doc/html/keysym_8h_source.html\n"
			"# 0x0 = disconnect\n");
		for (i=0; buttons3ds[i].key != 0; ++i) {
			fprintf(f,"%s\t0x%04lX\n", buttons3ds[i].name, buttons3ds[i].rfb_key);
		}
		fclose(f);
	}
	if (p) free(p);
}

int main() {
	int i;
	SDL_Event e;

	osSetSpeedupEnable(1);
	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(SDL_DISABLE);
	SDL_SetVideoMode(400,240,32, SDL_TOPSCR);
	consoleInit(GFX_BOTTOM, NULL);

	mkpath((char*)config_filename, false);

	// init 3DS buttons to their values
	for (int i=0; buttons3ds[i].key!=0; ++i)
		SDL_N3DSKeyBind(buttons3ds[i].key, buttons3ds[i].sdl_key);

	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
	socInit(SOC_buffer, SOC_BUFFERSIZE);

	rfbClientLog=rfbClientErr=log_citra;
	char *hoststr = NULL;

	while (1) {
		// get config
		vnc_config config;

		if (getconfig(&config) ||
			config.host[0] == 0) goto quit;
		
		user1=config.user;
		pass1=config.pass;
		printf("\x1b[2J"); // clear screen
		hoststr = malloc(strlen(config.host)+20);
		sprintf(hoststr,"%s:%d",config.host, config.port);

		cl=rfbGetClient(8,3,4);
		cl->MallocFrameBuffer = resize;
		cl->canHandleNewFBSize = TRUE;
		cl->GetCredential = get_credential;
		cl->GetPassword = get_password;

		char *argv[] = {
			"TinyVNC",
			hoststr};
		int argc = sizeof(argv)/sizeof(char*);

		readkeymaps(config.name);
		rfbClientLog("Connecting to %s", hoststr);
		if(!rfbInitClient(cl, &argc, argv))
		{
			cl = NULL; /* rfbInitClient has already freed the client struct */
		}
		free(hoststr);

		while(cl) {
			if(SDL_PollEvent(&e)) {
				/*
				handleSDLEvent() return 0 if user requested window close.
				*/
				if(!handleSDLEvent(cl, &e)) {
					rfbClientLog("Disconnecting");
					break;
				}
			}
			else {
				i=WaitForMessage(cl,500);
				if(i<0)
				{
					break;
				}
				if(i)
					if(!HandleRFBServerMessage(cl))
					{
						break;
					}
			}
			SDL_Flip(rfbClientGetClientData(cl, SDL_Init));
		}
		cleanup();

		printf("\x1b[7mA:retry B:quit                          \x1b[0m");
		while (1) {
			while (!SDL_PollEvent(&e)) SDL_Delay(20);
			if (e.type == SDL_KEYDOWN) {
				if (e.key.keysym.sym == SDLK_a)
					break;
				if (e.key.keysym.sym == SDLK_b)
					goto quit;
			}
		}
	}
quit:
	safeexit();
}
