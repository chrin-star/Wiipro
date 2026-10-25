
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/usb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_USB 32
#define PRO_VID 0x057E
#define PRO_PID 0x2009

static int home_pressed(void)
{
    WPAD_ScanPads();
    return (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) != 0;
}

static void wait_frames(int frames)
{
    for (int i = 0; i < frames; i++)
        VIDEO_WaitVSync();
}

static void show_descriptors(s32 fd)
{
    usb_devdesc desc;
    memset(&desc, 0, sizeof(desc));

    s32 result = USB_GetDescriptors(fd, &desc);

    printf("\nDescriptors result: %ld\n", (long)result);

    if (result < 0) {
        printf("ERROR: Cannot read descriptors.\n");
        return;
    }

    printf("Configurations: %u\n",
           (unsigned)desc.bNumConfigurations);

    if (desc.configurations) {
        for (unsigned c = 0;
             c < desc.bNumConfigurations;
             c++) {

            usb_configurationdesc *cfg =
                &desc.configurations[c];

            printf("\nConfig %u: %u interfaces\n",
                   c,
                   (unsigned)cfg->bNumInterfaces);

            if (!cfg->interfaces)
                continue;

            for (unsigned i = 0;
                 i < cfg->bNumInterfaces;
                 i++) {

                usb_interfacedesc *iface =
                    &cfg->interfaces[i];

                printf("Interface %u Class %02X\n",
                       (unsigned)iface->bInterfaceNumber,
                       (unsigned)iface->bInterfaceClass);

                if (!iface->endpoints)
                    continue;

                for (unsigned e = 0;
                     e < iface->bNumEndpoints;
                     e++) {

                    usb_endpointdesc *ep =
                        &iface->endpoints[e];

                    unsigned address =
                        ep->bEndpointAddress;

                    unsigned type =
                        ep->bmAttributes & 0x03;

                    printf(" EP:%02X %s Type:%u Size:%u\n",
                           address,
                           (address & 0x80) ? "IN " : "OUT",
                           type,
                           (unsigned)ep->wMaxPacketSize);
                }
            }
        }
    }

    USB_FreeDescriptors(&desc);
}

int main(int argc, char **argv)
{
    VIDEO_Init();

    GXRModeObj *mode =
        VIDEO_GetPreferredMode(NULL);

    void *fb =
        MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));

    console_init(
        fb, 20, 20,
        mode->fbWidth,
        mode->xfbHeight,
        mode->fbWidth * VI_DISPLAY_PIX_SZ
    );

    VIDEO_Configure(mode);
    VIDEO_SetNextFramebuffer(fb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    if (mode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();

    WPAD_Init();

    printf("Wii Pro Controller v0.3\n");
    printf("=======================\n\n");
    printf("USB connection diagnostic\n");
    printf("HOME: Exit\n\n");

    s32 result = USB_Initialize();

    printf("USB init: %ld\n", (long)result);

    if (result < 0) {
        printf("USB initialization failed.\n");

        while (!home_pressed())
            VIDEO_WaitVSync();

        return 1;
    }

    s32 fd = -1;
    int connected = 0;
    unsigned scan_number = 0;

    while (!connected) {
        if (home_pressed())
            goto cleanup;

        usb_device_entry entries[MAX_USB];
        memset(entries, 0, sizeof(entries));

        u8 count = 0;

        result = USB_GetDeviceList(
            entries,
            MAX_USB,
            USB_CLASS_HID,
            &count
        );

        scan_number++;

        printf("Scan %u: result=%ld HID=%u\n",
               scan_number,
               (long)result,
               (unsigned)count);

        if (result >= 0) {
            for (unsigned i = 0;
                 i < count && i < MAX_USB;
                 i++) {

                if (entries[i].vid != PRO_VID ||
                    entries[i].pid != PRO_PID)
                    continue;

                printf("\nPRO CONTROLLER FOUND!\n");
                printf("VID:057E PID:2009\n");

                s32 open_result = USB_OpenDevice(
                    entries[i].device_id,
                    PRO_VID,
                    PRO_PID,
                    &fd
                );

                printf("Open result: %ld\n",
                       (long)open_result);

                if (open_result >= 0 && fd >= 0) {
                    connected = 1;
                } else {
                    printf("Open failed. Retrying...\n");
                    fd = -1;
                }

                break;
            }
        }

        if (!connected) {
            printf("Waiting for controller...\n");

            for (int j = 0; j < 120; j++) {
                if (home_pressed())
                    goto cleanup;

                VIDEO_WaitVSync();
            }
        }

        /*
         * Clear the console occasionally so repeated
         * scanning does not fill the screen.
         */
        if (!connected && scan_number % 5 == 0) {
            printf("\x1b[2J\x1b[H");
            printf("Wii Pro Controller v0.3\n");
            printf("=======================\n\n");
            printf("HOME: Exit\n\n");
            printf("Scans completed: %u\n",
                   scan_number);
        }
    }

    printf("\nUSB CONNECTION OPENED!\n");

    show_descriptors(fd);

    printf("\nDiagnostic completed.\n");
    printf("Take a photo of this screen.\n");
    printf("Press HOME to exit.\n");

    while (!home_pressed())
        VIDEO_WaitVSync();

cleanup:

    if (fd >= 0)
        USB_CloseDevice(&fd);

    USB_Deinitialize();

    return 0;
}
