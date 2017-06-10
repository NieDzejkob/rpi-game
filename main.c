#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "global.h"

// power of two recommended for performance - untested
#define FRAME_VALUES 8

SDL_Window * gWindow = NULL;
SDL_Renderer * gRenderer = NULL;
SDL_Texture * gScreen = NULL;

#define X(var,path,key) SDL_Texture * var = NULL;
IMAGE_LIST
#undef X

render_func_t gRenderFunc = NULL;
u16 gJoypadHeld, gJoypadPressed;
u8 gExit;
u8 gScreenFade = 255;

u32 gFrameTimes[FRAME_VALUES];
u32 gFrameTimeLast, gFrameCount;

int main(void){
	ERROR_ON_SDL(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0, "SDL_Init"); atexit(SDL_Quit);
	ERROR_ON_IMG(!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG), "IMG_Init"); atexit(IMG_Quit);
	ERROR_ON_MIX(Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 1, 1024) < 0, "Mix_OpenAudio"); atexit(Mix_Quit);
	atexit(cleanup);

	gWindow = SDL_CreateWindow("rpi-game", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
	ERROR_ON_SDL(!gWindow, "SDL_CreateWindow");
	SDL_ShowCursor(SDL_DISABLE);

	gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	ERROR_ON_SDL(!gRenderer, "SDL_CreateRenderer");
	SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);

	gScreen = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_ABGR8888,
		SDL_TEXTUREACCESS_TARGET, SCREEN_WIDTH, SCREEN_HEIGHT);

#define X(var,path,key) var = loadTexture(path,key);
IMAGE_LIST
#undef X

	for(u8 i = 0;i < GAME_COUNT;i++){
		gGames[i].menuIcon = loadTexture(gGames[i].menuIconPath, KEY_NONE);

		CALL_UNLESS_NULL(gGames[i].initFunction);
	}

	InitJoypad();
	InitRandom();

	gRenderFunc = Render_SplashScreen;
	gExit = 0;

	memset(gFrameTimes, 0, sizeof(gFrameTimes));
	gFrameCount = 0;
	gFrameTimeLast = SDL_GetTicks();

	while(!gExit){
		u16 oldJoypad = gJoypadHeld;
		gJoypadHeld = ReadJoypad();
		gJoypadPressed = gJoypadHeld & ~oldJoypad;

		SDL_SetRenderTarget(gRenderer, gScreen);

		gRenderFunc();

		SDL_SetRenderTarget(gRenderer, NULL);
		SDL_SetTextureColorMod(gScreen, gScreenFade, gScreenFade, gScreenFade);
		SDL_RenderCopy(gRenderer, gScreen, NULL, NULL);

		Render_DoFPS();

		SDL_RenderPresent(gRenderer);

		if(gJoypadHeld == (JOY_L | JOY_R | JOY_SELECT | JOY_START)) gExit = 1;
	}

	return 0;
}

void Render_DoFPS(void){
	u32 mCurrentTicks, mDisplayFPS;
	float mFPS;
	
	SetFontColor16(0x32, 0xCD, 0x32);

	mCurrentTicks = SDL_GetTicks();
	gFrameTimes[gFrameCount % FRAME_VALUES] = mCurrentTicks - gFrameTimeLast;
	gFrameTimeLast = mCurrentTicks;
	gFrameCount++;

	mFPS = 0;
	for(u8 i = 0;i < FRAME_VALUES;i++){
		mFPS += gFrameTimes[i];
	}

	mFPS /= FRAME_VALUES;
	mFPS = 1000.f / mFPS;
	mDisplayFPS = floorf(mFPS + 0.5f);
	if(mDisplayFPS > 99) mDisplayFPS = 99;

	RenderChar16(0, 0, (mDisplayFPS / 10) + '0');
	RenderChar16(8, 0, (mDisplayFPS % 10) + '0');
}

void Render_SplashScreen(void){
	static int frameCounter;
	SDL_RenderCopy(gRenderer, gTextureSplash, NULL, NULL);

	frameCounter++;

	if(frameCounter >= 60){
		gRenderFunc = Render_FadeTransition;
		gRenderFuncAfterFade = Render_GameSelectMenu;
	}
}

void Render_GameSelectMenu_RenderGame(const game_t * game, int dx){
	SDL_Rect dstrect = { .x = 112 + dx, .y = 64, .w = 96, .h = 96 };
	SDL_RenderCopy(gRenderer, game->menuIcon, NULL, &dstrect);

	SetFontColor16(0, 0, 0);
	RenderText16((SCREEN_WIDTH - strlen(game->menuNameTop) * 8) / 2, 164 + dx, game->menuNameTop);
	RenderText16((SCREEN_WIDTH - strlen(game->menuNameBottom) * 8) / 2, 180 + dx, game->menuNameBottom);
}

void Render_GameSelectMenu(void){
	static game_index_t currentGame = GAME_EKANS;
	SDL_Rect dstrect = { .x = 0, .y = 0, .w = 40, .h = 20 };
	for(int y = 0;y < 12;y++){
		for(int x = 0;x < 8;x++){
			SDL_RenderCopy(gRenderer, gTextureMenuBackground, NULL, &dstrect);
			dstrect.x += 40;
		}

		dstrect.y += 20;
		dstrect.x = 0;
	}
	
	// XXX: Make this support more than one game when needed
	Render_GameSelectMenu_RenderGame(&gGames[0], 0);

	if(!gScreenFade) return; // don't check input if mid-fade render

	if(gJoypadPressed & (JOY_START | JOY_A)){
		CALL_UNLESS_NULL(gGames[currentGame].startFunction);
		gRenderFunc = Render_FadeTransition;
		gRenderFuncAfterFade = gGames[0].renderFunction;
	}
}

void InitRandom(void){
	u32 seed;
	int fd = open("/dev/hwrng", O_RDONLY);
	ERROR_ON_SYS(fd < 0, "open hwrng");
	ERROR_ON_SYS(read(fd, &seed, 4) != 4, "read hwrng");
	close(fd);
	srand(seed);
}

void cleanup(void){
	for(u8 i = 0;i < GAME_COUNT;i++){
		CALL_UNLESS_NULL(gGames[i].cleanupFunction);
		
		if(gGames[i].menuIcon) SDL_DestroyTexture(gGames[i].menuIcon);
	}

#define X(var,path,key) if(var) SDL_DestroyTexture(var);
IMAGE_LIST
#undef X

	if(gRenderer)			SDL_DestroyRenderer(gRenderer);
	if(gWindow)			SDL_DestroyWindow(gWindow);
}
