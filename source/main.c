
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/usb.h>
#include <stdio.h>
#include <string.h>

#define MAX_USB 32
#define VID 0x057E
#define PID 0x2009
#define EP_IN  0x81
#define EP_OUT 0x01
#define SIZE 64

static u8 inbuf[SIZE] __attribute__((aligned(32)));
static u8 outbuf[SIZE] __attribute__((aligned(32)));

static volatile int pending = 0;
static volatile int finished = 0;
static volatile s32 read_result = 0;

static s32 callback(s32 result, void *unused)
{
    (void)unused;
    read_result = result;
    finished = 1;
    pending = 0;
    return 0;
}

static int home(void)
{
    WPAD_ScanPads();
    return (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) != 0;
}

static void screen(void)
{
    printf("\x1b[2J\x1b[H");
    printf("Wii Pro Controller v1.0 EXP\n");
    printf("===========================\n\n");
}

static s32 send_usb(s32 fd, u8 command)
{
    memset(outbuf, 0, SIZE);
    outbuf[0] = 0x80;
    outbuf[1] = command;

    return USB_WriteIntrMsg(fd, EP_OUT, SIZE, outbuf);
}

static u8 packet_counter = 0;

static s32 send_subcommand(s32 fd, u8 command, u8 value)
{
    memset(outbuf, 0, SIZE);

    outbuf[0] = 0x01;  // Reporte de salida
    outbuf[1] = packet_counter++ & 0x0F;

    // Vibración neutral
    outbuf[2] = 0x00;
    outbuf[3] = 0x01;
    outbuf[4] = 0x40;
    outbuf[5] = 0x40;

    outbuf[6] = 0x00;
    outbuf[7] = 0x01;
    outbuf[8] = 0x40;
    outbuf[9] = 0x40;

    outbuf[10] = command;
    outbuf[11] = value;

    return USB_WriteIntrMsg(fd, EP_OUT, SIZE, outbuf);
}

static s16 read_s16_le(const u8 *p)
{
    return (s16)(
        (u16)p[0] |
        ((u16)p[1] << 8)
    );
}

static s32 set_report_mode(s32 fd)
{
    memset(outbuf, 0, SIZE);

    outbuf[0] = 0x01; /* Output report */
    outbuf[1] = 0x00; /* Packet counter */

    /* Neutral rumble data */
    outbuf[2] = 0x00;
    outbuf[3] = 0x01;
    outbuf[4] = 0x40;
    outbuf[5] = 0x40;

    outbuf[6] = 0x00;
    outbuf[7] = 0x01;
    outbuf[8] = 0x40;
    outbuf[9] = 0x40;

    outbuf[10] = 0x03; /* Set input report mode */
    outbuf[11] = 0x30; /* Full input reports */

    return USB_WriteIntrMsg(fd, EP_OUT, SIZE, outbuf);
}

static void dump_packet(const u8 *p, int length)
{
    if (length > SIZE) length = SIZE;

    for (int i = 0; i < length; i++) {
        printf("%02X ", p[i]);

        if ((i + 1) % 16 == 0)
            printf("\n");
    }

    printf("\n");
}

static int stick_x(const u8 *p)
{
    return p[6] | ((p[7] & 0x0F) << 8);
}

static int stick_y(const u8 *p)
{
    return (p[7] >> 4) | (p[8] << 4);
}

static int right_x(const u8 *p)
{
    return p[9] | ((p[10] & 0x0F) << 8);
}

static int right_y(const u8 *p)
{
    return (p[10] >> 4) | (p[11] << 4);
}

static void show_buttons(const u8 *p)
{
    u8 b0 = p[3];
    u8 b1 = p[4];
    u8 b2 = p[5];

    printf("Buttons:\n");

    if (b0 & 0x08) printf("A ");
    if (b0 & 0x04) printf("B ");
    if (b0 & 0x02) printf("X ");
    if (b0 & 0x01) printf("Y ");

    if (b0 & 0x40) printf("R ");
    if (b0 & 0x80) printf("ZR ");

    if (b2 & 0x40) printf("L ");
    if (b2 & 0x80) printf("ZL ");

    if (b1 & 0x01) printf("MINUS ");
    if (b1 & 0x02) printf("PLUS ");
    if (b1 & 0x10) printf("HOME ");
    if (b1 & 0x20) printf("CAPTURE ");

    if (b2 & 0x02) printf("UP ");
    if (b2 & 0x01) printf("DOWN ");
    if (b2 & 0x08) printf("LEFT ");
    if (b2 & 0x04) printf("RIGHT ");

    if (b1 & 0x08) printf("L-STICK ");
    if (b1 & 0x04) printf("R-STICK ");

    printf("\n\n");

    printf("Left stick : X=%4d Y=%4d\n",
           stick_x(p), stick_y(p));

    printf("Right stick: X=%4d Y=%4d\n",
           right_x(p), right_y(p));

    printf("\nMOVIMIENTO DEL PRO CONTROLLER\n");
printf("Muestra |   AX   AY   AZ |   GX   GY   GZ\n");

for (int i = 0; i < 3; i++) {
    int offset = 13 + (i * 12);

    s16 ax = read_s16_le(&p[offset]);
    s16 ay = read_s16_le(&p[offset + 2]);
    s16 az = read_s16_le(&p[offset + 4]);

    s16 gx = read_s16_le(&p[offset + 6]);
    s16 gy = read_s16_le(&p[offset + 8]);
    s16 gz = read_s16_le(&p[offset + 10]);

    printf("%d       | %5d %5d %5d | %5d %5d %5d\n",
           i + 1, ax, ay, az, gx, gy, gz);
}
}

int main(int argc, char **argv)
{
    VIDEO_Init();

    GXRModeObj *mode = VIDEO_GetPreferredMode(NULL);

    void *fb = MEM_K0_TO_K1(
        SYS_AllocateFramebuffer(mode)
    );

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
    screen();

    s32 result = USB_Initialize();

    printf("USB init: %ld\n", (long)result);

    if (result < 0) {
        printf("USB initialization failed.\n");

        while (!home())
            VIDEO_WaitVSync();

        return 1;
    }

    s32 fd = -1;
    int connected = 0;

    printf("Searching for controller...\n");

    while (!connected) {
        if (home())
            goto cleanup;

        usb_device_entry entries[MAX_USB];
        memset(entries, 0, sizeof(entries));

        u8 count = 0;

        result = USB_GetDeviceList(
            entries, MAX_USB, USB_CLASS_HID, &count
        );

        if (result >= 0) {
            for (unsigned i = 0; i < count; i++) {
                if (entries[i].vid != VID ||
                    entries[i].pid != PID)
                    continue;

                result = USB_OpenDevice(
                    entries[i].device_id,
                    VID, PID, &fd
                );

                printf("Open: %ld\n", (long)result);

                if (result >= 0 && fd >= 0)
                    connected = 1;

                break;
            }
        }

        if (!connected) {
            for (int i = 0; i < 120; i++) {
                if (home())
                    goto cleanup;

                VIDEO_WaitVSync();
            }
        }
    }

    printf("Controller connected!\n");

    /*
     * Protocol states:
     * 0 = waiting for 81 01
     * 1 = waiting for 81 02
     * 2 = waiting for full input reports
     */
    int state = 0;
    unsigned reports = 0;
    unsigned frames_waiting = 0;
    int stopping = 0;

    result = send_usb(fd, 0x01);

    printf("Identify write: %ld\n", (long)result);

    if (result < 0)
        goto cleanup;

    while (1) {
        if (home())
            stopping = 1;

        /*
         * Never close the device while an
         * asynchronous read is pending.
         */
        if (stopping && !pending)
            break;

        if (!stopping && !pending && !finished) {
            memset(inbuf, 0, SIZE);

            pending = 1;

            result = USB_ReadIntrMsgAsync(
                fd, EP_IN, SIZE,
                inbuf, callback, NULL
            );

            if (result < 0) {
                pending = 0;
                printf("Read submit error: %ld\n",
                       (long)result);
                stopping = 1;
            }
        }

        if (finished) {
            finished = 0;
            frames_waiting = 0;

            if (read_result < 0) {
                printf("Read error: %ld\n",
                       (long)read_result);
                stopping = 1;
            } else {
                u8 id = inbuf[0];

                if (state == 0 &&
                    id == 0x81 &&
                    inbuf[1] == 0x01) {

                    printf("Identify reply OK.\n");

                    result = send_usb(fd, 0x02);

                    printf("Handshake write: %ld\n",
                           (long)result);

                    if (result >= 0)
                        state = 1;
                    else
                        stopping = 1;

                } else if (state == 1 &&
                           id == 0x81 &&
                           inbuf[1] == 0x02) {

                    printf("Handshake reply OK.\n");

                    result = send_usb(fd, 0x04);

                    printf("Force USB: %ld\n",
                           (long)result);

                    if (result >= 0) {
    result = send_subcommand(fd, 0x40, 0x01);
    printf("Enable IMU: %ld\n", (long)result);

    if (result >= 0) {
        result = send_subcommand(fd, 0x03, 0x30);
        printf("Set report mode 0x30: %ld\n",
               (long)result);

        if (result >= 0) {
            state = 2;
        } else {
            stopping = 1;
        }
    } else {
        stopping = 1;
    }
} else {
    stopping = 1;
} else {
                        stopping = 1;
                    }

                } else if (state == 2 && id == 0x30) {
                    reports++;

                    /*
                     * Refresh at most every fourth
                     * input report.
                     */
                    if (reports == 1 ||
                        reports % 4 == 0) {

                        screen();

                        printf("USB: CONNECTED\n");
                        printf("Full reports: %u\n\n",
                               reports);

                        show_buttons(inbuf);

                        printf("\nHOME on Wii Remote: Exit\n");
                    }

                } else {
                    screen();

                    printf("Protocol state: %d\n", state);
                    printf("Received report: %02X\n\n", id);
                    dump_packet(inbuf, 16);
                    printf("\nWaiting for next packet...\n");
                }
            }
        }

        frames_waiting++;

        if (frames_waiting == 300) {
            printf("\nWaiting for USB response...\n");
        }

        VIDEO_WaitVSync();
    }

    printf("\nStopping USB reader...\n");

cleanup:
    if (fd >= 0 && !pending)
        USB_CloseDevice(&fd);

    if (!pending)
        USB_Deinitialize();

    return 0;
}
