#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/gpio/driver.h>
#include <linux/delay.h>

#define DEVICE_NAME "esp32_bridge"
#define NUM_GPIO 40

#define SYNC 0xAA
#define CMD_GPIO_SET 0x03
#define CMD_GPIO_GET 0x04

static struct file *tty_filp;

static u8 crc8(const u8 *data, size_t len)
{
    static const u8 table[256] = {
        0x00,0x5e,0xbc,0xe2,0x61,0x3f,0xdd,0x83,
        0xc2,0x9c,0x7e,0x20,0xa3,0xfd,0x1f,0x41,
        0x9d,0xc3,0x21,0x7f,0xfc,0xa2,0x40,0x1e,
        0x5f,0x01,0xe3,0xbd,0x3e,0x60,0x82,0xdc,
        0x23,0x7d,0x9f,0xc1,0x42,0x1c,0xfe,0xa0,
        0xe1,0xbf,0x5d,0x03,0x80,0xde,0x3c,0x62,
        0xbe,0xe0,0x02,0x5c,0xdf,0x81,0x63,0x3d,
        0x7c,0x22,0xc0,0x9e,0x1d,0x43,0xa1,0xff,
        0x46,0x18,0xfa,0xa4,0x27,0x79,0x9b,0xc5,
        0x84,0xda,0x38,0x66,0xe5,0xbb,0x59,0x07,
        0xdb,0x85,0x67,0x39,0xba,0xe4,0x06,0x58,
        0x19,0x47,0xa5,0xfb,0x78,0x26,0xc4,0x9a,
        0x65,0x3b,0xd9,0x87,0x04,0x5a,0xb8,0xe6,
        0xa7,0xf9,0x1b,0x45,0xc6,0x98,0x7a,0x24,
        0xf8,0xa6,0x44,0x1a,0x99,0xc7,0x25,0x7b,
        0x3a,0x64,0x86,0xd8,0x5b,0x05,0xe7,0xb9,
        0x8c,0xd2,0x30,0x6e,0xed,0xb3,0x51,0x0f,
        0x4e,0x10,0xf2,0xac,0x2f,0x71,0x93,0xcd,
        0x11,0x4f,0xad,0xf3,0x70,0x2e,0xcc,0x92,
        0xd3,0x8d,0x6f,0x31,0xb2,0xec,0x0e,0x50,
        0xaf,0xf1,0x13,0x4d,0xce,0x90,0x72,0x2c,
        0x6d,0x33,0xd1,0x8f,0x0c,0x52,0xb0,0xee,
        0x32,0x6c,0x8e,0xd0,0x53,0x0d,0xef,0xb1,
        0xf0,0xae,0x4c,0x12,0x91,0xcf,0x2d,0x73,
        0xca,0x94,0x76,0x28,0xab,0xf5,0x17,0x49,
        0x08,0x56,0xb4,0xea,0x69,0x37,0xd5,0x8b,
        0x57,0x09,0xeb,0xb5,0x36,0x68,0x8a,0xd4,
        0x95,0xcb,0x29,0x77,0xf4,0xaa,0x48,0x16,
        0xe9,0xb7,0x55,0x0b,0x88,0xd6,0x34,0x6a,
        0x2b,0x75,0x97,0xc9,0x4a,0x14,0xf6,0xa8,
        0x74,0x2a,0xc8,0x96,0x15,0x4b,0xa9,0xf7,
        0xb6,0xe8,0x0a,0x54,0xd7,0x89,0x6b,0x35
    };

    u8 crc = 0;
    while (len--)
        crc = table[crc ^ *data++];

    return crc;
}

static int esp32_open_uart(void)
{
    tty_filp = filp_open("/dev/ttyUSB0", O_RDWR | O_NOCTTY, 0);
    if (IS_ERR(tty_filp)) {
        pr_err("ESP32: cannot open ttyUSB0\n");
        return PTR_ERR(tty_filp);
    }

    pr_info("ESP32: UART opened\n");
    return 0;
}

static int esp_write_all(const u8 *buf, size_t len)
{
    int ret;
    int total = 0;

    while (total < len) {
        ret = kernel_write(tty_filp, buf + total, len - total, NULL);
        if (ret <= 0)
            return ret;
        total += ret;
    }

    return total;
}

static int esp_read_exact(u8 *buf, int len)
{
    int got = 0;
    int ret;
    int timeout = 100; // ~5 сек

    while (got < len && timeout--) {
        ret = kernel_read(tty_filp, buf + got, len - got, NULL);
        if (ret > 0) {
            got += ret;
        } else {
            msleep(50);
        }
    }

    return (got == len) ? got : -EIO;
}

static int esp_read_packet(u8 *buf)
{
    int ret;

    ret = esp_read_exact(buf, 1);
    if (ret <= 0) return ret;

    if (buf[0] != SYNC) {
        pr_err("bad sync\n");
        return -EIO;
    }

    ret = esp_read_exact(buf + 1, 2);
    if (ret <= 0) return ret;

    int len = buf[2];

    ret = esp_read_exact(buf + 3, len + 1);
    if (ret <= 0) return ret;

    return 3 + len + 1;
}


static int esp_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
    u8 pkt[5];
    u8 resp[64];
    int ret;

    pkt[0] = SYNC;
    pkt[1] = CMD_GPIO_GET;
    pkt[2] = 1;
    pkt[3] = offset;
    pkt[4] = crc8(pkt, 4);

    pr_info("GPIO GET %d\n", offset);

    ret = esp_write_all(pkt, 5);
    if (ret != 5)
        return 0;

    ret = esp_read_packet(resp);
    if (ret <= 0) {
        pr_err("NO RESPONSE\n");
        return 0;
    }

    print_hex_dump(KERN_INFO, "ESP RX: ", DUMP_PREFIX_NONE, 16, 1, resp, ret, 1);

    return resp[3];
}

static int esp_gpio_set(struct gpio_chip *chip,
                        unsigned int offset, int value)
{
    u8 pkt[6];
    u8 resp[64];
    int ret;

    pkt[0] = SYNC;
    pkt[1] = CMD_GPIO_SET;
    pkt[2] = 2;
    pkt[3] = offset;
    pkt[4] = value;
    pkt[5] = crc8(pkt, 5);

    pr_info("GPIO SET %d = %d\n", offset, value);

    print_hex_dump(KERN_INFO, "ESP TX: ", DUMP_PREFIX_NONE, 16, 1, pkt, 6, 1);

    ret = esp_write_all(pkt, 6);
    if (ret != 6)
        return -EIO;

    ret = esp_read_packet(resp);
    if (ret <= 0) {
        pr_err("NO RESPONSE\n");
        return -EIO;
    }

    print_hex_dump(KERN_INFO, "ESP RX: ", DUMP_PREFIX_NONE, 16, 1, resp, ret, 1);

    return 0;
}
static int esp_gpio_direction_output(struct gpio_chip *chip,
                                     unsigned int offset, int value)
{
    return esp_gpio_set(chip, offset, value);
}

static int esp_gpio_direction_input(struct gpio_chip *chip,
                                    unsigned int offset)
{
    return 0;
}


static struct gpio_chip gpio = {
    .label = DEVICE_NAME,
    .owner = THIS_MODULE,
    .get = esp_gpio_get,
    .set = esp_gpio_set,
    .direction_output = esp_gpio_direction_output,
    .direction_input = esp_gpio_direction_input,
    .base = -1,
    .ngpio = NUM_GPIO,
};


static int __init esp_init(void)
{
    int ret;

    pr_info("ESP32 bridge init\n");

    ret = esp32_open_uart();
    if (ret)
        return ret;

    ret = gpiochip_add_data(&gpio, NULL);
    if (ret)
        return ret;

    pr_info("ESP32 bridge ready\n");
    return 0;
}

static void __exit esp_exit(void)
{
    gpiochip_remove(&gpio);

    if (tty_filp)
        filp_close(tty_filp, NULL);

    pr_info("ESP32 bridge exit\n");
}

module_init(esp_init);
module_exit(esp_exit);

MODULE_LICENSE("GPL");
