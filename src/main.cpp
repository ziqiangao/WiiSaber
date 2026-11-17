#include <gccore.h>
#include <ogc/system.h>
#include <stdio.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    VIDEO_Init();
    PAD_Init();

    GXRModeObj *video_mode = VIDEO_GetPreferredMode(nullptr);
    void *framebuffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(video_mode));

    console_init(framebuffer, 20, 20, video_mode->fbWidth,
                 video_mode->xfbHeight, video_mode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(video_mode);
    VIDEO_SetNextFramebuffer(framebuffer);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (video_mode->viTVMode & VI_NON_INTERLACE) {
        VIDEO_WaitVSync();
    }

    puts("WiiSaber - libogc starter project");
    puts("Press START on a GameCube controller to exit.");

    while (true) {
        PAD_ScanPads();
        if (PAD_ButtonsDown(0) & PAD_BUTTON_START) {
            break;
        }
        VIDEO_WaitVSync();
    }

    return 0;
}
