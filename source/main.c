
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/usb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MAX_USB 32
#define PRO_VID 0x057E
#define PRO_PID 0x2009

#define EP_IN  0x81
#define EP_OUT 0x01
#define PACKET_SIZE 64

static unsigned char input[PACKET_SIZE]
    __attribute__((aligned(32)));

static unsigned char output[PACKET_SIZE]
    __attribute__((aligned(32)));

static volatile int read_pending = 0;
static volatile int read_finished = 0;
static volatile s32 read_result = 0;

static s32 read_callback(s32 result, void *userdata)
{
    read_result = result;
    read_finished = 1;
    read_pending = 0;
    return 0;
}

static int home_pressed(void)
{
    WPAD_ScanPads();
    return (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) != 0;
}

static void heading(void)
{
    printf("\x1b[2J\x1b[H");
    printf("Wii Pro Controller v0.4\n");
    printf("=======================\n\n");
}

static void show_packet(void)
{
    printf("\nReceived data:\n");

    for (int i = 0; i < PACKET_SIZE; i++) {
        printf("%02X ", input[i]);

        if ((i + 1) % 16 == 0)
            printf("\n");
    }

    printf("\n");
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

    heading();

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

    printf("Searching for Pro Controller...\n");

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

        if (result >= 0) {
            for (unsigned i = 0;
                 i < count && i < MAX_USB;
                 i++) {

                if (entries[i].vid != PRO_VID ||
                    entries[i].pid != PRO_PID)
                    continue;

                printf("Controller found!\n");

                result = USB_OpenDevice(
                    entries[i].device_id,
                    PRO_VID,
                    PRO_PID,
                    &fd
                );

                printf("Open result: %ld\n",
                       (long)result);

                if (result >= 0 && fd >= 0)
                    connected = 1;

                break;
            }
        }

        if (!connected) {
            for (int i = 0; i < 120; i++) {
                if (home_pressed())
                    goto cleanup;

                VIDEO_WaitVSync();
            }
        }
    }

    printf("\nController connected.\n");

    /*
     * Initial USB handshake.
     * This requests the controller's USB handshake.
     */
    memset(output, 0, sizeof(output));

    output[0] = 0x80;
    output[1] = 0x01;

    result = USB_WriteIntrMsg(
        fd,
        EP_OUT,
        PACKET_SIZE,
        output
    );

    printf("Handshake write: %ld\n",
           (long)result);

    printf("\nStarting USB input reader...\n");
    printf("Press buttons on the controller.\n");
    printf("HOME on Wii Remote: exit.\n");

    unsigned packet_count = 0;
    int stop_requested = 0;

    while (1) {
        if (home_pressed())
            stop_requested = 1;

        /*
         * Do not close the USB device while
         * an asynchronous read is still pending.
         */
        if (stop_requested && !read_pending)
            break;

        if (!stop_requested &&
            !read_pending &&
            !read_finished) {

            memset(input, 0, sizeof(input));

            read_pending = 1;

            result = USB_ReadIntrMsgAsync(
                fd,
                EP_IN,
                PACKET_SIZE,
                input,
                read_callback,
                NULL
            );

            if (result < 0) {
                read_pending = 0;

                printf("\nRead submission error: %ld\n",
                       (long)result);

                break;
            }
        }

        if (read_finished) {
            read_finished = 0;

            if (read_result < 0) {
                printf("\nUSB read error: %ld\n",
                       (long)read_result);
                break;
            }

            packet_count++;

            if (packet_count == 1 ||
                packet_count % 10 == 0) {

                heading();

                printf("Controller connected.\n");
                printf("Packets received: %u\n",
                       packet_count);

                printf("Last read result: %ld\n",
                       (long)read_result);

                show_packet();

                printf("HOME: Exit\n");
            }
        }

        VIDEO_WaitVSync();
    }

    printf("\nReader stopped.\n");

    while (read_pending)
        VIDEO_WaitVSync();

    printf("Press HOME to finish.\n");

    while (!home_pressed())
        VIDEO_WaitVSync();

cleanup:

    if (fd >= 0)
        USB_CloseDevice(&fd);

    USB_Deinitialize();

    return 0;
}
