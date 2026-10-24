#include <gccore.h>
#include <ogc/usb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_USB 32
#define PRO_VID 0x057e
#define PRO_PID 0x2009

int main(int argc, char **argv) {
    VIDEO_Init();
    GXRModeObj *mode = VIDEO_GetPreferredMode(NULL);
    void *fb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
    console_init(fb, 20, 20, mode->fbWidth, mode->xfbHeight, mode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(mode);
    VIDEO_SetNextFramebuffer(fb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (mode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
    WPAD_Init();
    printf("Wii Pro Controller Detector v0.1\n");
    printf("Connect the ORIGINAL Switch Pro Controller via USB.\n");
    printf("HOME: exit | Scans every 2 seconds.\n\n");
    s32 init = USB_Initialize();
    printf("USB_Initialize: %ld\n", (long)init);
    if (init < 0) { printf("USB initialization failed.\n"); while(1) { WPAD_ScanPads(); if(WPAD_ButtonsDown(0)&WPAD_BUTTON_HOME) break; usleep(50000); } return 1; }
    while (1) {
        usb_device_entry entries[MAX_USB];
        u8 count = 0;
        s32 result = USB_GetDeviceList(entries, MAX_USB, USB_CLASS_HID, &count);
        int found = 0;
        printf("\nScan: result=%ld, HID devices=%u\n", (long)result, (unsigned)count);
        if (result >= 0) {
            for (unsigned i=0; i<count && i<MAX_USB; ++i) {
                printf("VID:%04X PID:%04X\n", entries[i].vid, entries[i].pid);
                if (entries[i].vid == PRO_VID && entries[i].pid == PRO_PID) found = 1;
            }
        }
        printf(found ? "PRO CONTROLLER DETECTED!\n" : "Pro Controller not detected in HID list.\n");
        for (int i=0;i<40;i++) { WPAD_ScanPads(); if(WPAD_ButtonsDown(0)&WPAD_BUTTON_HOME) { USB_Deinitialize(); return 0; } usleep(50000); }
    }
}
