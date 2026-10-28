
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/usb.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PRO_VID 0x057E
#define PRO_PID 0x2009
#define MAX_USB 32

static usb_device_entry devices[MAX_USB] ATTRIBUTE_ALIGN(32);
static u8 input[64] ATTRIBUTE_ALIGN(32);
static u8 output[64] ATTRIBUTE_ALIGN(32);

static s32 fd = -1;
static u8 endpoint_in = 0x81;
static u8 endpoint_out = 0x01;
static u8 counter = 0;

static s16 read16(const u8 *p)
{
    return (s16)((u16)p[0] | ((u16)p[1] << 8));
}

static void init_screen(void)
{
    VIDEO_Init();

    GXRModeObj *mode = VIDEO_GetPreferredMode(NULL);
    void *fb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));

    console_init(
        fb, 20, 20,
        mode->fbWidth, mode->xfbHeight,
        mode->fbWidth * VI_DISPLAY_PIX_SZ
    );

    VIDEO_Configure(mode);
    VIDEO_SetNextFramebuffer(fb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
}

static int find_pro_controller(void)
{
    u8 count = 0;

    memset(devices, 0, sizeof(devices));

    s32 ret = USB_GetDeviceList(
        devices, MAX_USB, USB_CLASS_HID, &count
    );

    if (ret < 0)
        return ret;

    for (int i = 0; i < count; i++)
    {
        if (devices[i].vid == PRO_VID &&
            devices[i].pid == PRO_PID)
        {
            ret = USB_OpenDevice(
                devices[i].device_id,
                PRO_VID,
                PRO_PID,
                &fd
            );

            if (ret < 0)
                return ret;

            return 1;
        }
    }

    return 0;
}

static s32 usb_command(u8 command)
{
    memset(output, 0, sizeof(output));

    output[0] = 0x80;
    output[1] = command;

    return USB_WriteIntrMsg(
        fd, endpoint_out, 64, output
    );
}

static s32 subcommand(u8 command, u8 value)
{
    memset(output, 0, sizeof(output));

    output[0] = 0x01;
    output[1] = counter++ & 0x0F;

    /* Neutral rumble data */
    output[2] = 0x00;
    output[3] = 0x01;
    output[4] = 0x40;
    output[5] = 0x40;

    output[6] = 0x00;
    output[7] = 0x01;
    output[8] = 0x40;
    output[9] = 0x40;

    output[10] = command;
    output[11] = value;

    return USB_WriteIntrMsg(
        fd, endpoint_out, 64, output
    );
}

static void show_report(s32 length)
{
    printf("\x1b[2J\x1b[H");

    printf("WII PRO CONTROLLER DETECTOR v0.2\n");
    printf("--------------------------------\n\n");

    printf("Nintendo Switch Pro Controller\n");
    printf("USB connection: ACTIVE\n\n");

    printf("Report: 0x%02X\n", input[0]);
    printf("Length: %ld bytes\n\n", (long)length);

    if (input[0] != 0x30 || length < 49)
    {
        printf("Waiting for full report 0x30...\n\n");

        printf("RAW DATA:\n");

        for (int i = 0; i < length && i < 32; i++)
        {
            printf("%02X ", input[i]);

            if ((i + 1) % 8 == 0)
                printf("\n");
        }

        printf("\n");
        return;
    }

    printf("BUTTONS (RAW)\n");
    printf("%02X %02X %02X\n\n",
           input[3], input[4], input[5]);

    u16 lx = input[6] |
             ((input[7] & 0x0F) << 8);

    u16 ly = (input[7] >> 4) |
             (input[8] << 4);

    u16 rx = input[9] |
             ((input[10] & 0x0F) << 8);

    u16 ry = (input[10] >> 4) |
             (input[11] << 4);

    printf("ANALOG STICKS\n");
    printf("LEFT:  X:%4u Y:%4u\n", lx, ly);
    printf("RIGHT: X:%4u Y:%4u\n\n", rx, ry);

    printf("ACCELEROMETER - RAW VALUES\n");

    for (int i = 0; i < 3; i++)
    {
        int offset = 13 + i * 12;

        s16 x = read16(&input[offset]);
        s16 y = read16(&input[offset + 2]);
        s16 z = read16(&input[offset + 4]);

        printf(
            "S%d X:%6d Y:%6d Z:%6d\n",
            i + 1, x, y, z
        );
    }

    printf("\nGYROSCOPE - RAW VALUES\n");

    for (int i = 0; i < 3; i++)
    {
        int offset = 19 + i * 12;

        s16 x = read16(&input[offset]);
        s16 y = read16(&input[offset + 2]);
        s16 z = read16(&input[offset + 4]);

        printf(
            "S%d X:%6d Y:%6d Z:%6d\n",
            i + 1, x, y, z
        );
    }

    printf("\nMove and tilt the controller.\n");
}

int main(int argc, char **argv)
{
    init_screen();
    WPAD_Init();

    printf("Wii Pro Controller Detector v0.2\n\n");

    s32 ret = USB_Initialize();

    if (ret < 0)
    {
        printf("USB initialization failed: %ld\n",
               (long)ret);

        while (1)
            VIDEO_WaitVSync();
    }

    printf("Connect your ORIGINAL Nintendo\n");
    printf("Switch Pro Controller via USB.\n\n");

    while (fd < 0)
    {
        ret = find_pro_controller();

        if (ret == 1)
            break;

        printf("\rSearching... %ld    ", (long)ret);
        fflush(stdout);

        sleep(2);
    }

    printf("\n\nController detected!\n");
    printf("Initializing...\n");

    ret = usb_command(0x01);
    printf("USB handshake 1: %ld\n", (long)ret);
    usleep(100000);

    ret = usb_command(0x02);
    printf("USB handshake 2: %ld\n", (long)ret);
    usleep(100000);

    /* Enable motion sensors */
    ret = subcommand(0x40, 0x01);
    printf("Enable IMU: %ld\n", (long)ret);
    usleep(100000);

    /* Request full input reports */
    ret = subcommand(0x03, 0x30);
    printf("Report mode 0x30: %ld\n", (long)ret);

    sleep(1);

    while (1)
    {
        memset(input, 0, sizeof(input));

        ret = USB_ReadIntrMsg(
            fd,
            endpoint_in,
            sizeof(input),
            input
        );

        if (ret < 0)
        {
            printf("\nUSB read error: %ld\n", (long)ret);
            break;
        }

        if (ret > 0)
            show_report(ret);

        usleep(50000);
    }

    USB_CloseDevice(&fd);
    USB_Deinitialize();

    while (1)
        VIDEO_WaitVSync();

    return 0;
}
