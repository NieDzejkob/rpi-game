#include <stdlib.h>
#include <unistd.h>

#include "global.h"

#define IMAGE_LIST \
	X(gTextureFont, "assets/font.png", KEY_BLACK)				\
	X(gTextureSplash, "assets/splash.png", KEY_NONE)			\
	X(gTextureMenuBackground, "assets/menu_background.png", KEY_NONE)

SDL_Surface * loadImage(char * name);

SDL_Window * gWindow = NULL;
SDL_Renderer * gRenderer = NULL;

#define X(var,path,key) SDL_Texture * var = NULL;
IMAGE_LIST
#undef X

Mix_Chunk * gSoundStartup = NULL;

render_func_t gRenderFunc = NULL;
render_state_t gRenderState;
u16 gJoypad;
u8 gExit;

int main(void){
	ERROR_ON_SDL(SDL_Init(SDL_INIT_VIDEO) != 0, "SDL_Init"); atexit(SDL_Quit);
	ERROR_ON_IMG(!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG), "IMG_Init"); atexit(IMG_Quit);
	ERROR_ON_MIX(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 1, 2048) < 0, "Mix_OpenAudio"); atexit(Mix_Quit);
	atexit(cleanup);

	gWindow = SDL_CreateWindow("rpi-game", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		320, 240, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
	ERROR_ON_SDL(!gWindow, "SDL_CreateWindow");
	SDL_ShowCursor(SDL_DISABLE);

	gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	ERROR_ON_SDL(!gRenderer, "SDL_CreateRenderer");

	SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);

	init_joypad();
	gRenderFunc = Render_SplashScreen;
	gRenderState.splash.frameCounter = 0;
	gExit = 0;

#define X(var,path,key) var = loadTexture(path,key);
IMAGE_LIST
#undef X

	gSoundStartup = Mix_LoadWAV("assets/startup.wav");
	ERROR_ON_MIX(!gSoundStartup, "startup.wav load");

	for(u8 i = 0;i < GAME_COUNT;i++){
		gGames[i].menuIcon = loadTexture(gGames[i].menuIconPath, KEY_NONE);
		if(gGames[i].initFunction) gGames[i].initFunction();
	}

//	u32 time_now, time_last, fps;
	
//	time_last = 0;
//	time_now = SDL_GetTicks();

	while(!gExit){
		gJoypad = read_joypad();

		SDL_RenderClear(gRenderer);
		gRenderFunc();

		SetFontColor(0x32, 0xCD, 0x32);
		RenderText(32, 32, "Just testing my font engine...");

		SDL_RenderPresent(gRenderer);
	}

	return 0;
}

void Render_SplashScreen(void){
	if(gRenderState.splash.frameCounter == 0){
//		Mix_PlayChannel(-1, gSoundStartup, 0);
	}

	SDL_RenderCopy(gRenderer, gTextureSplash, NULL, NULL);

	if(gJoypad & JOY_START){
		gExit = 1;
	}

	gRenderState.splash.frameCounter++;
}

void cleanup(void){
	for(u8 i = 0;i < GAME_COUNT;i++){
		if(gGames[i].cleanupFunction) gGames[i].cleanupFunction();
		if(gGames[i].menuIcon) SDL_DestroyTexture(gGames[i].menuIcon);
	}

#define X(var,path,key) if(var) SDL_DestroyTexture(var);
IMAGE_LIST
#undef X

	if(gSoundStartup)		Mix_FreeChunk(gSoundStartup);

	if(gRenderer)			SDL_DestroyRenderer(gRenderer);
	if(gWindow)			SDL_DestroyWindow(gWindow);
}

SDL_Texture * loadTexture(char * path, color_key_index_t key){
	printf("Loading %s\n", path);

	SDL_Surface * surface = IMG_Load(path);
	ERROR_ON_IMG(surface == NULL, "IMG_Load");

	switch(key){
		case KEY_NONE:
			break;
		case KEY_BLACK:
			SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 0, 0, 0));
			break;
	}

	SDL_Texture * texture = SDL_CreateTextureFromSurface(gRenderer, surface);
	ERROR_ON_SDL(texture == NULL, "SDL_CreateTextureFromSurface");

	SDL_FreeSurface(surface);

	return texture;
}

void RenderChar(int x, int y, char c){
	SDL_Rect srcrect = { .x = (c % 16) * 8, .y = (c / 16) * 16, .w = 8, .h = 16 };
	SDL_Rect dstrect = { .x = x, .y = y, .w = 8, .h = 16 };
	SDL_RenderCopy(gRenderer, gTextureFont, &srcrect, &dstrect);
}

void RenderText(int x, int y, char * s){
	while(*s){
		RenderChar(x, y, *s++);
		x += 8;
	}
}
