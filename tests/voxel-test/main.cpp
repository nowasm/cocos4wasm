#include "platform/BasePlatform.h"
#include "SDL2/SDL_main.h"

int SDL_main(int argc, char **argv) {
    START_PLATFORM(argc, (const char **)argv);
}
