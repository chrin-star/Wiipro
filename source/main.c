
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/usb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_USB 32
#define PRO_VID 0x057E
#define PRO_PID 0x2009

static void wait_for_home(void)
{
    printf("\nPress HOME on Wii Remote to exit.\n");

    while (1) {
        WPAD_ScanPads();

        if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME)
            break;

        VIDEO_WaitVSync();
    }
}

int main(int argc, char **argv)
{
    VIDEO_Init();

    GXRModeObj *mode = VIDEO_GetPreferredMode(NULL);
    void *fb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));

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

    printf("Wii Pro Controller v0.2\n");
    printf("========================\n\n");

    printf("Connect controller before starting.\n\n");

    s32 result = USB_Initialize();

    printf("USB init: %ld\n", (long)result);

    if (result < 0) {
        printf("ERROR: USB initialization failed.\n");
        wait_for_home();
        return 1;
    }

    usb_device_entry entries[MAX_USB];
    u8 count = 0;

    result = USB_GetDeviceList(
        entries,
        MAX_USB,
        USB_CLASS_HID,
        &count
    );

    printf("USB scan: %ld\n", (long)result);
    printf("HID devices: %u\n\n", (unsigned)count);

    if (result < 0) {
        printf("ERROR: USB scan failed.\n");
        wait_for_home();
        USB_Deinitialize();
        return 1;
    }

    int found = -1;

    for (unsigned i = 0; i < count && i < MAX_USB; i++) {
        printf(
            "VID:%04X PID:%04X\n",
            entries[i].vid,
            entries[i].pid
        );

        if (
            entries[i].vid == PRO_VID &&
            entries[i].pid == PRO_PID
        ) {
            found = (int)i;
        }
    }

    if (found < 0) {
        printf("\nPRO CONTROLLER NOT FOUND.\n");
        wait_for_home();
        USB_Deinitialize();
        return 0;
    }

    printf("\nPRO CONTROLLER FOUND!\n");

    s32 fd = -1;

    result = USB_OpenDevice(
        entries[found].device_id,
        PRO_VID,
        PRO_PID,
        &fd
    );

    printf("USB_OpenDevice: %ld\n", (long)result);

    if (result < 0 || fd < 0) {
        printf("ERROR: Cannot open controller.\n");
        wait_for_home();
        USB_Deinitialize();
        return 1;
    }

    printf("Controller opened successfully!\n\n");

    usb_devdesc desc = {0};

    result = USB_GetDescriptors(fd, &desc);

    printf("USB_GetDescriptors: %ld\n", (long)result);

    if (result >= 0) {
        printf("Configurations: %u\n",
               (unsigned)desc.bNumConfigurations);

        if (desc.configurations) {
            for (unsigned c = 0;
                 c < desc.bNumConfigurations;
                 c++) {

                usb_configurationdesc *config =
                    &desc.configurations[c];

                printf("\nCONFIG %u\n", c);
                printf("Interfaces: %u\n",
                       (unsigned)config->bNumInterfaces);

                if (!config->interfaces)
                    continue;

                for (unsigned i = 0;
                     i < config->bNumInterfaces;
                     i++) {

                    usb_interfacedesc *iface =
                        &config->interfaces[i];

                    printf(
                        "Interface %u | Class %02X\n",
                        (unsigned)iface->bInterfaceNumber,
                        (unsigned)iface->bInterfaceClass
                    );

                    printf(
                        "Endpoints: %u\n",
                        (unsigned)iface->bNumEndpoints
                    );

                    if (!iface->endpoints)
                        continue;

                    for (unsigned e = 0;
                         e < iface->bNumEndpoints;
                         e++) {

                        usb_endpointdesc *ep =
                            &iface->endpoints[e];

                        printf(
                            " EP %02X Type %02X Size %u\n",
                            (unsigned)ep->bEndpointAddress,
                            (unsigned)(ep->bmAttributes & 3),
                            (unsigned)ep->wMaxPacketSize
                        );
                    }
                }
            }
        }

        USB_FreeDescriptors(&desc);
    } else {
        printf("Could not read USB descriptors.\n");
    }

    printf("\nDescriptor test completed.\n");

    wait_for_home();

    USB_CloseDevice(&fd);
    USB_Deinitialize();

    return 0;
}
