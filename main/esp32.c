#include "3dconfig.hpp"
//disabled by default.
//uncomment if you want to execute by esp32
#ifdef ESP32
/* SPI Master example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
spi_device_handle_t spi;

/*
 This code displays some fancy graphics on the 320x240 LCD on an ESP-WROVER_KIT board.
 This example demonstrates the use of both spi_device_transmit as well as
 spi_device_queue_trans/spi_device_get_trans_result and pre-transmit callbacks.
 Some info about the ILI9341/ST7789V: It has an C/D line, which is connected to a GPIO here. It expects this
 line to be low for a command and high for data. We use a pre-transmit callback here to control that
 line: every transaction has as the user-definable argument the needed state of the D/C line and just
 before the transaction is sent, the callback will set this line to the correct state.
*/

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5

#define PIN_NUM_DC   21
#define PIN_NUM_RST  -1
#define PIN_NUM_BCKL 14

//To speed up transfers, every SPI transfer sends a bunch of lines. This define specifies how many. More means more memory use,
//but less overhead for setting up / finishing transfers. Make sure 240 is dividable by this.

#define PARALLEL_LINES DRAW_NLINES

/*
 The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct.
*/

#define CONFIG_LCD_TYPE_ILI9341
//#define CONFIG_LCD_OVERCLOCK


typedef struct {
    uint8_t cmd;
    uint8_t data[128];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

typedef enum {
    LCD_TYPE_ILI = 1,
    LCD_TYPE_ST,
    LCD_TYPE_MAX,
} type_lcd_t;

/* //Place data into DRAM. Constant data gets placed into DROM by default, which is not accessible by DMA. */
/* DRAM_ATTR static const lcd_init_cmd_t st_init_cmds[]={ */
/*     /\* Memory Data Access Control, MX=MV=1, MY=ML=MH=0, RGB=0 *\/ */
/*     {0x36, {(1<<5)|(1<<6)}, 1}, */
/*     /\* Interface Pixel Format, 16bits/pixel for RGB/MCU interface *\/ */
/*     {0x3A, {0x55}, 1}, */
/*     /\* Porch Setting *\/ */
/*     {0xB2, {0x0c, 0x0c, 0x00, 0x33, 0x33}, 5}, */
/*     /\* Gate Control, Vgh=13.65V, Vgl=-10.43V *\/ */
/*     {0xB7, {0x45}, 1}, */
/*     /\* VCOM Setting, VCOM=1.175V *\/ */
/*     {0xBB, {0x2B}, 1}, */
/*     /\* LCM Control, XOR: BGR, MX, MH *\/ */
/*     {0xC0, {0x2C}, 1}, */
/*     /\* VDV and VRH Command Enable, enable=1 *\/ */
/*     {0xC2, {0x01, 0xff}, 2}, */
/*     /\* VRH Set, Vap=4.4+... *\/ */
/*     {0xC3, {0x11}, 1}, */
/*     /\* VDV Set, VDV=0 *\/ */
/*     {0xC4, {0x20}, 1}, */
/*     /\* Frame Rate Control, 60Hz, inversion=0 *\/ */
/*     {0xC6, {0x0f}, 1}, */
/*     /\* Power Control 1, AVDD=6.8V, AVCL=-4.8V, VDDS=2.3V *\/ */
/*     {0xD0, {0xA4, 0xA1}, 1}, */
/*     /\* Positive Voltage Gamma Control *\/ */
/*     {0xE0, {0xD0, 0x00, 0x05, 0x0E, 0x15, 0x0D, 0x37, 0x43, 0x47, 0x09, 0x15, 0x12, 0x16, 0x19}, 14}, */
/*     /\* Negative Voltage Gamma Control *\/ */
/*     {0xE1, {0xD0, 0x00, 0x05, 0x0D, 0x0C, 0x06, 0x2D, 0x44, 0x40, 0x0E, 0x1C, 0x18, 0x16, 0x19}, 14}, */
/*     /\* Sleep Out *\/ */
/*     {0x11, {0}, 0x80}, */
/*     /\* Display On *\/ */
/*     {0x29, {0}, 0x80}, */
/*     {0, {0}, 0xff} */
/* }; */

#if 0
DRAM_ATTR static const lcd_init_cmd_t ili_init_cmds[]={
    /* Power contorl B, power control = 0, DC_ENA = 1 */
    {0xCF, {0x00, 0x83, 0X30}, 3},
    /* Power on sequence control,
     * cp1 keeps 1 frame, 1st frame enable
     * vcl = 0, ddvdh=3, vgh=1, vgl=2
     * DDVDH_ENH=1
     */
    {0xED, {0x64, 0x03, 0X12, 0X81}, 4},
    /* Driver timing control A,
     * non-overlap=default +1
     * EQ=default - 1, CR=default
     * pre-charge=default - 1
     */
    {0xE8, {0x85, 0x01, 0x79}, 3},
    /* Power control A, Vcore=1.6V, DDVDH=5.6V */
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    /* Pump ratio control, DDVDH=2xVCl */
    {0xF7, {0x20}, 1},
    /* Driver timing control, all=0 unit */
    {0xEA, {0x00, 0x00}, 2},
    /* Power control 1, GVDD=4.75V */
    {0xC0, {0x26}, 1},
    /* Power control 2, DDVDH=VCl*2, VGH=VCl*7, VGL=-VCl*3 */
    {0xC1, {0x11}, 1},
    /* VCOM control 1, VCOMH=4.025V, VCOML=-0.950V */
    {0xC5, {0x35, 0x3E}, 2},
    /* VCOM control 2, VCOMH=VMH-2, VCOML=VML-2 */
    {0xC7, {0xBE}, 1},
    /* Memory access contorl, MX=MY=0, MV=1, ML=0, BGR=1, MH=0 */
    {0x36, {0x28}, 1},
    /* Pixel format, 16bits/pixel for RGB/MCU interface */
    {0x3A, {0x55}, 1},
    /* Frame rate control, f=fosc, 70Hz fps */
    {0xB1, {0x00, 0x1B}, 2},
    /* Enable 3G, disabled */
    {0xF2, {0x08}, 1},
    /* Gamma set, curve 1 */
    {0x26, {0x01}, 1},
    /* Positive gamma correction */
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
    /* Negative gamma correction */
    {0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
    /* Column address set, SC=0, EC=0xEF */
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    /* Page address set, SP=0, EP=0x013F */
    {0x2B, {0x00, 0x00, 0x01, 0x3f}, 4},
    /* Memory write */
    {0x2C, {0}, 0},
    /* Entry mode set, Low vol detect disabled, normal display */
    {0xB7, {0x07}, 1},
    /* Display function control */
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
    /* Sleep out */
    {0x11, {0}, 0x80},
    /* Display on */
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};
#else
#define TFT_CMD_SWRESET	0x01
#define TFT_CMD_SLEEP 0x10
#define TFT_CMD_DISPLAY_OFF 0x28

#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_MH 0x04
#define TFT_RGB_BGR 0x08

DRAM_ATTR static const lcd_init_cmd_t ili_init_cmds[] = {
    {TFT_CMD_SWRESET, {0}, 0x80},
    {0xCF, {0x00, 0xc3, 0x30}, 3},
    {0xED, {0x64, 0x03, 0x12, 0x81}, 4},
    {0xE8, {0x85, 0x00, 0x78}, 3},
    {0xCB, {0x39, 0x2c, 0x00, 0x34, 0x02}, 5},
    {0xF7, {0x20}, 1},
    {0xEA, {0x00, 0x00}, 2},
    {0xC0, {0x1B}, 1},    //Power control   //VRH[5:0]
    {0xC1, {0x12}, 1},    //Power control   //SAP[2:0];BT[3:0]
    {0xC5, {0x32, 0x3C}, 2},    //VCM control
    {0xC7, {0x91}, 1},    //VCM control2
    {0x36, {(MADCTL_MV | MADCTL_MY | TFT_RGB_BGR)}, 1},    // Memory Access Control
    {0x3A, {0x55}, 1},
    {0xB1, {0x00, 0x1B}, 2},  // Frame Rate Control (1B=70, 1F=61, 10=119)
    {0xB6, {0x0A, 0xA2}, 2},    // Display Function Control
    {0xF6, {0x01, 0x30}, 2},
    {0xF2, {0x00}, 1},    // 3Gamma Function Disable
    {0x26, {0x01}, 1},     //Gamma curve selected

    //Set Gamma
    {0xE0, {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00}, 15},
    {0XE1, {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F}, 15},

    {0x11, {0}, 0x80},    //Exit Sleep
    {0x29, {0}, 0x80},    //Display on

    {0, {0}, 0xff}
};

#endif

//Send a command to the LCD. Uses spi_device_transmit, which waits until the transfer is complete.
void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    ret=spi_device_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

#ifdef ILI9341
//Send data to the LCD. Uses spi_device_transmit, which waits until the transfer is complete.
void lcd_data(spi_device_handle_t spi,uint8_t *data,int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=len*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               //Data
    t.user=(void*)1;                //D/C needs to be set to 1
    ret=spi_device_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}
#else
void lcd_data(spi_device_handle_t spi,uint8_t data)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=1*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=&data;               //Data
    t.user=(void*)1;                //D/C needs to be set to 1
    ret=spi_device_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}
#endif
//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc=(int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}

uint32_t lcd_get_id(spi_device_handle_t spi)
{
    //get_id cmd
    lcd_cmd( spi, 0x04);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length=8*3;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.user = (void*)1;

    esp_err_t ret = spi_device_transmit(spi, &t);
    assert( ret == ESP_OK );

    return *(uint32_t*)t.rx_data;
}

// Change the width and height if required (defined in portrait mode)
// or use the constructor to over-ride defaults
#define TFT_WIDTH  240
#define TFT_HEIGHT 320


// Color definitions for backwards compatibility with old sketches
// use colour definitions like TFT_BLACK to make sketches more portable
#define ILI9341_BLACK       0x0000      /*   0,   0,   0 */
#define ILI9341_NAVY        0x000F      /*   0,   0, 128 */
#define ILI9341_DARKGREEN   0x03E0      /*   0, 128,   0 */
#define ILI9341_DARKCYAN    0x03EF      /*   0, 128, 128 */
#define ILI9341_MAROON      0x7800      /* 128,   0,   0 */
#define ILI9341_PURPLE      0x780F      /* 128,   0, 128 */
#define ILI9341_OLIVE       0x7BE0      /* 128, 128,   0 */
#define ILI9341_LIGHTGREY   0xC618      /* 192, 192, 192 */
#define ILI9341_DARKGREY    0x7BEF      /* 128, 128, 128 */
#define ILI9341_BLUE        0x001F      /*   0,   0, 255 */
#define ILI9341_GREEN       0x07E0      /*   0, 255,   0 */
#define ILI9341_CYAN        0x07FF      /*   0, 255, 255 */
#define ILI9341_RED         0xF800      /* 255,   0,   0 */
#define ILI9341_MAGENTA     0xF81F      /* 255,   0, 255 */
#define ILI9341_YELLOW      0xFFE0      /* 255, 255,   0 */
#define ILI9341_WHITE       0xFFFF      /* 255, 255, 255 */
#define ILI9341_ORANGE      0xFD20      /* 255, 165,   0 */
#define ILI9341_GREENYELLOW 0xAFE5      /* 173, 255,  47 */
#define ILI9341_PINK        0xF81F

#define BLACK 0x0000       /*   0,   0,   0 */
#define NAVY 0x000F        /*   0,   0, 128 */
#define DARKGREEN 0x03E0   /*   0, 128,   0 */
#define DARKCYAN 0x03EF    /*   0, 128, 128 */
#define MAROON 0x7800      /* 128,   0,   0 */
#define PURPLE 0x780F      /* 128,   0, 128 */
#define OLIVE 0x7BE0       /* 128, 128,   0 */
#define LIGHTGREY 0xC618   /* 192, 192, 192 */
#define DARKGREY 0x7BEF    /* 128, 128, 128 */
#define BLUE 0x001F        /*   0,   0, 255 */
#define GREEN 0x07E0       /*   0, 255,   0 */
#define CYAN 0x07FF        /*   0, 255, 255 */
#define RED 0xF800         /* 255,   0,   0 */
#define MAGENTA 0xF81F     /* 255,   0, 255 */
#define YELLOW 0xFFE0      /* 255, 255,   0 */
#define WHITE 0xFFFF       /* 255, 255, 255 */
#define ORANGE 0xFD20      /* 255, 165,   0 */
#define GREENYELLOW 0xAFE5 /* 173, 255,  47 */
#define PINK 0xF81F


// Delay between some initialisation commands
#define TFT_INIT_DELAY 0x80 // Not used unless commandlist invoked


// Generic commands used by TFT_eSPI.cpp
#define TFT_NOP     0x00
#define TFT_SWRST   0x01

#define TFT_CASET   0x2A
#define TFT_PASET   0x2B
#define TFT_RAMWR   0x2C

#define TFT_RAMRD   0x2E
#define TFT_IDXRD   0xDD // ILI9341 only, indexed control register read

#define TFT_MADCTL  0x36
#define TFT_MAD_MY  0x80
#define TFT_MAD_MX  0x40
#define TFT_MAD_MV  0x20
#define TFT_MAD_ML  0x10
#define TFT_MAD_BGR 0x08
#define TFT_MAD_MH  0x04
#define TFT_MAD_RGB 0x00

#define TFT_INVOFF  0x20
#define TFT_INVON   0x21


// All ILI9341 specific commands some are used by init()
#define ILI9341_NOP     0x00
#define ILI9341_SWRESET 0x01
#define ILI9341_RDDID   0x04
#define ILI9341_RDDST   0x09

#define ILI9341_SLPIN   0x10
#define ILI9341_SLPOUT  0x11
#define ILI9341_PTLON   0x12
#define ILI9341_NORON   0x13

#define ILI9341_RDMODE  0x0A
#define ILI9341_RDMADCTL  0x0B
#define ILI9341_RDPIXFMT  0x0C
#define ILI9341_RDIMGFMT  0x0A
#define ILI9341_RDSELFDIAG  0x0F

#define ILI9341_INVOFF  0x20
#define ILI9341_INVON   0x21
#define ILI9341_GAMMASET 0x26
#define ILI9341_DISPOFF 0x28
#define ILI9341_DISPON  0x29

#define ILI9341_CASET   0x2A
#define ILI9341_PASET   0x2B
#define ILI9341_RAMWR   0x2C
#define ILI9341_RAMRD   0x2E

#define ILI9341_PTLAR   0x30
#define ILI9341_VSCRDEF 0x33
#define ILI9341_MADCTL  0x36
#define ILI9341_VSCRSADD 0x37
#define ILI9341_PIXFMT  0x3A

#define ILI9341_WRDISBV  0x51
#define ILI9341_RDDISBV  0x52
#define ILI9341_WRCTRLD  0x53

#define ILI9341_FRMCTR1 0xB1
#define ILI9341_FRMCTR2 0xB2
#define ILI9341_FRMCTR3 0xB3
#define ILI9341_INVCTR  0xB4
#define ILI9341_DFUNCTR 0xB6

#define ILI9341_PWCTR1  0xC0
#define ILI9341_PWCTR2  0xC1
#define ILI9341_PWCTR3  0xC2
#define ILI9341_PWCTR4  0xC3
#define ILI9341_PWCTR5  0xC4
#define ILI9341_VMCTR1  0xC5
#define ILI9341_VMCTR2  0xC7

#define ILI9341_RDID4   0xD3
#define ILI9341_RDINDEX 0xD9
#define ILI9341_RDID1   0xDA
#define ILI9341_RDID2   0xDB
#define ILI9341_RDID3   0xDC
#define ILI9341_RDIDX   0xDD // TBC

#define ILI9341_GMCTRP1 0xE0
#define ILI9341_GMCTRN1 0xE1

#define ILI9341_MADCTL_MY  0x80
#define ILI9341_MADCTL_MX  0x40
#define ILI9341_MADCTL_MV  0x20
#define ILI9341_MADCTL_ML  0x10
#define ILI9341_MADCTL_RGB 0x00
#define ILI9341_MADCTL_BGR 0x08
#define ILI9341_MADCTL_MH  0x04

static void backlight_init()
{
    // (duty range is 0 ~ ((2**bit_num)-1)
    const int DUTY_MAX = 0x1fff;

    //configure timer0
    ledc_timer_config_t ledc_timer;
    memset(&ledc_timer, 0, sizeof(ledc_timer));

    ledc_timer.bit_num = LEDC_TIMER_13_BIT; //set timer counter bit number
    ledc_timer.freq_hz = 5000;              //set frequency of pwm
    ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;   //timer mode,
    ledc_timer.timer_num = LEDC_TIMER_0;    //timer index


    ledc_timer_config(&ledc_timer);


    //set the configuration
    ledc_channel_config_t ledc_channel;
    memset(&ledc_channel, 0, sizeof(ledc_channel));

    //set LEDC channel 0
    ledc_channel.channel = LEDC_CHANNEL_0;
    //set the duty for initialization.(duty range is 0 ~ ((2**bit_num)-1)
    ledc_channel.duty = 0;
    //GPIO number
    ledc_channel.gpio_num = PIN_NUM_BCKL;
    //GPIO INTR TYPE, as an example, we enable fade_end interrupt here.
    ledc_channel.intr_type = LEDC_INTR_FADE_END;
    //set LEDC mode, from ledc_mode_t
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    //set LEDC timer source, if different channel use one timer,
    //the frequency and bit_num of these channels should be the same
    ledc_channel.timer_sel = LEDC_TIMER_0;


    ledc_channel_config(&ledc_channel);


    //initialize fade service.
    ledc_fade_func_install(0);

    // duty range is 0 ~ ((2**bit_num)-1)
    ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, DUTY_MAX, 500);
    ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
}

//Initialize the display
void lcd_init(spi_device_handle_t spi)
{
    int cmd=0;
    const lcd_init_cmd_t* lcd_init_cmds;
    lcd_init_cmds = ili_init_cmds;

    //Initialize non-SPI GPIOs
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
    //gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    
    // //Reset the display
    // gpio_set_level(PIN_NUM_RST, 0);
    // vTaskDelay(100 / portTICK_RATE_MS);
    // gpio_set_level(PIN_NUM_RST, 1);
    // vTaskDelay(150 / portTICK_RATE_MS);

    backlight_init();

#ifndef ILI9341
    lcd_cmd(spi,0xEF);
    lcd_data(spi,0x03);
    lcd_data(spi,0x80);
    lcd_data(spi,0x02);

    lcd_cmd(spi,0xCF);
    lcd_data(spi,0x00);
    lcd_data(spi,0XC1);
    lcd_data(spi,0X30);

    lcd_cmd(spi,0xED);
    lcd_data(spi,0x64);
    lcd_data(spi,0x03);
    lcd_data(spi,0X12);
    lcd_data(spi,0X81);

    lcd_cmd(spi,0xE8);
    lcd_data(spi,0x85);
    lcd_data(spi,0x00);
    lcd_data(spi,0x78);

    lcd_cmd(spi,0xCB);
    lcd_data(spi,0x39);
    lcd_data(spi,0x2C);
    lcd_data(spi,0x00);
    lcd_data(spi,0x34);
    lcd_data(spi,0x02);

    lcd_cmd(spi,0xF7);
    lcd_data(spi,0x20);

    lcd_cmd(spi,0xEA);
    lcd_data(spi,0x00);
    lcd_data(spi,0x00);

    lcd_cmd(spi,ILI9341_PWCTR1);    //Power control
    lcd_data(spi,0x23);   //VRH[5:0]

    lcd_cmd(spi,ILI9341_PWCTR2);    //Power control
    lcd_data(spi,0x10);   //SAP[2:0];BT[3:0]

    lcd_cmd(spi,ILI9341_VMCTR1);    //VCM control
    lcd_data(spi,0x3e);
    lcd_data(spi,0x28);

    lcd_cmd(spi,ILI9341_VMCTR2);    //VCM control2
    lcd_data(spi,0x86);  //--

    lcd_cmd(spi,ILI9341_MADCTL);    // Memory Access Control
  /* #ifdef M5STACK */
    lcd_data(spi,0xA8); // Rotation 0 (portrait mode)
  /* #else */
    //    lcd_data(spi,0x48); // Rotation 0 (portrait mode)
  /* #endif */

    lcd_cmd(spi,ILI9341_PIXFMT);
    lcd_data(spi,0x55);

    lcd_cmd(spi,ILI9341_FRMCTR1);
    lcd_data(spi,0x00);
    lcd_data(spi,0x13); // 0x18 79Hz, 0x1B default 70Hz, 0x13 100Hz

    lcd_cmd(spi,ILI9341_DFUNCTR);    // Display Function Control
    lcd_data(spi,0x08);
    lcd_data(spi,0x82);
    lcd_data(spi,0x27);

    lcd_cmd(spi,0xF2);    // 3Gamma Function Disable
    lcd_data(spi,0x00);

    lcd_cmd(spi,ILI9341_GAMMASET);    //Gamma curve selected
    lcd_data(spi,0x01);

    lcd_cmd(spi,ILI9341_GMCTRP1);    //Set Gamma
    lcd_data(spi,0x0F);
    lcd_data(spi,0x31);
    lcd_data(spi,0x2B);
    lcd_data(spi,0x0C);
    lcd_data(spi,0x0E);
    lcd_data(spi,0x08);
    lcd_data(spi,0x4E);
    lcd_data(spi,0xF1);
    lcd_data(spi,0x37);
    lcd_data(spi,0x07);
    lcd_data(spi,0x10);
    lcd_data(spi,0x03);
    lcd_data(spi,0x0E);
    lcd_data(spi,0x09);
    lcd_data(spi,0x00);

    lcd_cmd(spi,ILI9341_GMCTRN1);    //Set Gamma
    lcd_data(spi,0x00);
    lcd_data(spi,0x0E);
    lcd_data(spi,0x14);
    lcd_data(spi,0x03);
    lcd_data(spi,0x11);
    lcd_data(spi,0x07);
    lcd_data(spi,0x31);
    lcd_data(spi,0xC1);
    lcd_data(spi,0x48);
    lcd_data(spi,0x08);
    lcd_data(spi,0x0F);
    lcd_data(spi,0x0C);
    lcd_data(spi,0x31);
    lcd_data(spi,0x36);
    lcd_data(spi,0x0F);

    lcd_cmd(spi,ILI9341_SLPOUT);    //Exit Sleep

    vTaskDelay(120/ portTICK_RATE_MS);
    
    lcd_cmd(spi,ILI9341_DISPON); //Display on
    vTaskDelay(10/ portTICK_RATE_MS);

    
    // This is the command sequence that rotates the ILI9341 driver coordinate frame
    
    lcd_cmd(spi,TFT_MADCTL);
    lcd_data(spi,TFT_MAD_BGR);

#endif
/* //detect LCD type */
/*     uint32_t lcd_id = lcd_get_id(spi); */
/*     int lcd_detected_type = 0; */
/*     int lcd_type; */

/*     printf("LCD ID: %08X\n", lcd_id); */
/*     if ( lcd_id == 0 ) { */
/*         //zero, ili */
/*         lcd_detected_type = LCD_TYPE_ILI; */
/*         printf("ILI9341 detected.\n"); */
/*     } else { */
/*         // none-zero, ST */
/*         lcd_detected_type = LCD_TYPE_ST; */
/*         printf("ST7789V detected.\n"); */
/*     } */

/* #ifdef CONFIG_LCD_TYPE_AUTO */
/*     lcd_type = lcd_detected_type; */
/* #elif defined( CONFIG_LCD_TYPE_ST7789V ) */
/*     printf("kconfig: force CONFIG_LCD_TYPE_ST7789V.\n"); */
/*     lcd_type = LCD_TYPE_ST; */
/* #elif defined( CONFIG_LCD_TYPE_ILI9341 ) */
/*     printf("kconfig: force CONFIG_LCD_TYPE_ILI9341.\n"); */
/*     lcd_type = LCD_TYPE_ILI; */
/* #endif */
/*     if ( lcd_type == LCD_TYPE_ST ) { */
/*         printf("LCD ST7789V initialization.\n"); */
/*         lcd_init_cmds = st_init_cmds; */
/*     } else { */
/*         printf("LCD ILI9341 initialization.\n"); */
/*         lcd_init_cmds = ili_init_cmds; */
/*     } */

/*     //Send all the commands */
#ifdef ILI9341
    while (lcd_init_cmds[cmd].databytes!=0xff) {
        lcd_cmd(spi, lcd_init_cmds[cmd].cmd);
        lcd_data(spi, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes&0x1F);
        if (lcd_init_cmds[cmd].databytes&0x80) {
            vTaskDelay(100 / portTICK_RATE_MS);
        }
        cmd++;
    }
#endif
}


static void send_line_finish(spi_device_handle_t spi)
{
    spi_transaction_t *rtrans;
    esp_err_t ret;
    //Wait for all 6 transactions to be done and get back the results.
    for (int x=0; x<6; x++) {
        ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        assert(ret==ESP_OK);
        //We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though.
    }
}
static
int isnotfirst;

//To send a set of lines we have to send a command, 2 data bytes, another command, 2 more data bytes and another command
//before sending the line data itself; a total of 6 transactions. (We can't put all of this in just one transaction
//because the D/C line needs to be toggled in the middle.)
//This routine queues these commands up so they get sent as quickly as possible.
void send_line(int ypos, uint16_t *linedata)
{
  if(isnotfirst){
    send_line_finish(spi);
  }
  isnotfirst = 1;
    esp_err_t ret;
    int x;
    //Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this
    //function is finished because the SPI driver needs access to it even while we're already calculating the next line.
    static spi_transaction_t trans[6];

    //In theory, it's better to initialize trans and data only once and hang on to the initialized
    //variables. We allocate them on the stack, so we need to re-init them each call.
    for (x=0; x<6; x++) {
        memset(&trans[x], 0, sizeof(spi_transaction_t));
        if ((x&1)==0) {
            //Even transfers are commands
            trans[x].length=8;
            trans[x].user=(void*)0;
        } else {
            //Odd transfers are data
            trans[x].length=8*4;
            trans[x].user=(void*)1;
        }
        trans[x].flags=SPI_TRANS_USE_TXDATA;
    }
    trans[0].tx_data[0]=0x2A;           //Column Address Set
    trans[1].tx_data[0]=(160-window_width/2)>>8;              //Start Col High
    trans[1].tx_data[1]=(160-window_width/2)&0xFF;              //Start Col Low
    trans[1].tx_data[2]=(window_width/2+160-1)>>8;       //End Col High
    trans[1].tx_data[3]=(window_width/2+160-1)&0xff;     //End Col Low
    trans[2].tx_data[0]=0x2B;           //Page address set
    trans[3].tx_data[0]=(ypos+120-window_height/2)>>8;        //Start page high
    trans[3].tx_data[1]=(ypos+120-window_height/2)&0xff;      //start page low
    trans[3].tx_data[2]=(ypos+120-window_height/2+PARALLEL_LINES+2)>>8;    //end page high
    trans[3].tx_data[3]=(ypos+120-window_height/2+PARALLEL_LINES)&0xff;  //end page low
    trans[4].tx_data[0]=0x2C;           //memory write
    trans[5].tx_buffer=linedata;        //finally send the line data
    trans[5].length=window_width*2*8*PARALLEL_LINES;          //Data length, in bits
    trans[5].flags=0; //undo SPI_TRANS_USE_TXDATA flag

    //Queue all transactions.
    for (x=0; x<6; x++) {
        ret=spi_device_queue_trans(spi, &trans[x], portMAX_DELAY);
        assert(ret==ESP_OK);
    }

    //When we are here, the SPI driver is busy (in the background) getting the transactions sent. That happens
    //mostly using DMA, so the CPU doesn't have much to do here. We're not going to wait for the transaction to
    //finish because we may as well spend the time calculating the next line. When that is done, we can call
    //send_line_finish, which will wait for the transfers to be done and check their status.
}


int main3d();

/* /\* SPI Master example */

/*    This example code is in the Public Domain (or CC0 licensed, at your option.) */

/*    Unless required by applicable law or agreed to in writing, this */
/*    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR */
/*    CONDITIONS OF ANY KIND, either express or implied. */
/* *\/ */
/* #include <stdio.h> */
/* #include <stdlib.h> */
/* #include <string.h> */
/* #include "freertos/FreeRTOS.h" */
/* #include "freertos/task.h" */
/* #include "esp_system.h" */
/* #include "driver/spi_master.h" */
/* #include "soc/gpio_struct.h" */
/* #include "driver/gpio.h" */

/* spi_device_handle_t spi; */

/* /\* */
/*   This code displays some fancy graphics on the ILI9341-based 320x240 LCD on an ESP-WROVER_KIT board. */
/*   It is not very fast, even when the SPI transfer itself happens at 8MHz and with DMA, because */
/*   the rest of the code is not very optimized. Especially calculating the image line-by-line */
/*   is inefficient; it would be quicker to send an entire screenful at once. This example does, however, */
/*   demonstrate the use of both spi_device_transmit as well as spi_device_queue_trans/spi_device_get_trans_result */
/*   as well as pre-transmit callbacks. */

/*   Some info about the ILI9341: It has an C/D line, which is connected to a GPIO here. It expects this */
/*   line to be low for a command and high for data. We use a pre-transmit callback here to control that */
/*   line: every transaction has as the user-definable argument the needed state of the D/C line and just */
/*   before the transaction is sent, the callback will set this line to the correct state. */
/* *\/ */


/* int main3d(); */

/* /\* */
/*   The ILI9341 needs a bunch of command/argument values to be initialized. They are stored in this struct. */
/* *\/ */
/* typedef struct { */
/*   uint8_t cmd; */
/*   uint8_t data[16]; */
/*   uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds. */
/* } ili_init_cmd_t; */

/* //Send a command to the ILI9341. Uses spi_device_transmit, which waits until the transfer is complete. */
/* void ili_cmd(spi_device_handle_t spi, const uint8_t cmd)  */
/* { */
/*   esp_err_t ret; */
/*   spi_transaction_t t; */
/*   memset(&t, 0, sizeof(t));       //Zero out the transaction */
/*   t.length=8;                     //Command is 8 bits */
/*   t.tx_buffer=&cmd;               //The data is the cmd itself */
/*   t.user=(void*)0;                //D/C needs to be set to 0 */
/*   ret=spi_device_transmit(spi, &t);  //Transmit! */
/*   assert(ret==ESP_OK);            //Should have had no issues. */
/* } */

/* //Send data to the ILI9341. Uses spi_device_transmit, which waits until the transfer is complete. */
/* void ili_data(spi_device_handle_t spi, const uint8_t *data, int len)  */
/* { */
/*   esp_err_t ret; */
/*   spi_transaction_t t; */
/*   if (len==0) return;             //no need to send anything */
/*   memset(&t, 0, sizeof(t));       //Zero out the transaction */
/*   t.length=len*8;                 //Len is in bytes, transaction length is in bits. */
/*   t.tx_buffer=data;               //Data */
/*   t.user=(void*)1;                //D/C needs to be set to 1 */
/*   ret=spi_device_transmit(spi, &t);  //Transmit! */
/*   assert(ret==ESP_OK);            //Should have had no issues. */
/* } */

/* //This function is called (in irq context!) just before a transmission starts. It will */
/* //set the D/C line to the value indicated in the user field. */
/* void ili_spi_pre_transfer_callback(spi_transaction_t *t)  */
/* { */
/*   int dc=(int)t->user; */
/*   gpio_set_level(PIN_NUM_DC, dc); */
/* } */

/* //Initialize the display */
/* void ili_init(spi_device_handle_t spi)  */
/* { */
/*   //Initialize non-SPI GPIOs */
/*   gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT); */
/*   gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT); */
/*   gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT); */

/*   //Reset the display */
/*   gpio_set_level(PIN_NUM_RST, 0); */
/*   vTaskDelay(100 / portTICK_RATE_MS); */
/*   gpio_set_level(PIN_NUM_RST, 1); */
/*   vTaskDelay(100 / portTICK_RATE_MS); */

/*   /\* //Send all the commands *\/ */
/*   /\* ili_cmd(spi, ili_init_cmds[cmd].cmd); *\/ */
/*   /\* ili_data(spi, ili_init_cmds[cmd].data, ili_init_cmds[cmd].databytes&0x1F); *\/ */
/*   uint8_t data; */
/* #define TRANS_1DATA(x) {			\ */
/*     data = (x);					\ */
/*     ili_data(spi,&data,1);			\ */
/*   } */

/*   ili_cmd(spi,0x11); //Exit Sleep */
/*   vTaskDelay(20/portTICK_RATE_MS); */

/*   ili_cmd(spi,0x26); //Set Default Gamma */
/*   TRANS_1DATA(0x04); */

/*   ili_cmd(spi,0xB1);//Set Frame Rate */
/*   TRANS_1DATA(0x0C); */
/*   TRANS_1DATA(0x14); */

/*   ili_cmd(spi,0xC0); //Set VRH1[4:0] & VC[2:0] for VCI1 & GVDD */
/*   TRANS_1DATA(0x0C); */
/*   TRANS_1DATA(0x05); */

/*   ili_cmd(spi,0xC1); //Set BT[2:0] for AVDD & VCL & VGH & VGL */
/*   TRANS_1DATA(0x02);//0x00 */

/*   ili_cmd(spi,0xC5); //Set VMH[6:0] & VML[6:0] for VOMH & VCOML */
/*   TRANS_1DATA(0x29);  //31  21  29 */
/*   TRANS_1DATA(0x43);  //58   48  43 */

/*   ili_cmd(spi,0xC7); */
/*   TRANS_1DATA(0x40); */

/*   ili_cmd(spi,0x3a); //Set Color Format */
/*   TRANS_1DATA(0x05); */
/*   /\* */
/*     ili_cmd(spi,0x2A); //Set Column Address */
/*     TRANS_1DATA(0x00); */
/*     TRANS_1DATA(0x00); */
/*     TRANS_1DATA(0x00); */
/*     TRANS_1DATA(0x7F);	 */

/*     ili_cmd(spi,0x2B); //Set Page Address */
/*     TRANS_1DATA(0x00); */
/*     TRANS_1DATA(0x00); */
/*     TRANS_1DATA(0x00); */
/*     TRANS_1DATA(0x9F);	 */
/*   *\/ */
/*   ili_cmd(spi,0x36); //Set Scanning Direction */
/* #if  0 */
/*   TRANS_1DATA(0xC8); //0xc8 */
/* #else */
/*   TRANS_1DATA(0xA8); //0xA8 */
/* #endif */
/*   ili_cmd(spi,0xB7); //Set Source Output Direction */
/*   TRANS_1DATA(0x00); */

/*   ili_cmd(spi,0xF2); //Enable Gamma bit */
/*   TRANS_1DATA(0x01);	 */
/*   /\* */
/*     ili_cmd(spi,0xE0); */
/*     TRANS_1DATA(0x36);//p1 */
/*     TRANS_1DATA(0x29);//p2 */
/*     TRANS_1DATA(0x12);//p3 */
/*     TRANS_1DATA(0x22);//p4 */
/*     TRANS_1DATA(0x1C);//p5 */
/*     TRANS_1DATA(0x15);//p6 */
/*     TRANS_1DATA(0x42);//p7 */
/*     TRANS_1DATA(0xB7);//p8 */
/*     TRANS_1DATA(0x2F);//p9 */
/*     TRANS_1DATA(0x13);//p10 */
/*     TRANS_1DATA(0x12);//p11 */
/*     TRANS_1DATA(0x0A);//p12 */
/*     TRANS_1DATA(0x11);//p13 */
/*     TRANS_1DATA(0x0B);//p14 */
/*     TRANS_1DATA(0x06);//p15 */

/*     ili_cmd(spi,0xE1); */
/*     TRANS_1DATA(0x09);//p1 */
/*     TRANS_1DATA(0x16);//p2 */
/*     TRANS_1DATA(0x2D);//p3 */
/*     TRANS_1DATA(0x0D);//p4 */
/*     TRANS_1DATA(0x13);//p5 */
/*     TRANS_1DATA(0x15);//p6 */
/*     TRANS_1DATA(0x40);//p7 */
/*     TRANS_1DATA(0x48);//p8 */
/*     TRANS_1DATA(0x53);//p9 */
/*     TRANS_1DATA(0x0C);//p10 */
/*     TRANS_1DATA(0x1D);//p11 */
/*     TRANS_1DATA(0x25);//p12 */
/*     TRANS_1DATA(0x2E);//p13 */
/*     TRANS_1DATA(0x34);//p14 */
/*     TRANS_1DATA(0x39);//p15 */
/*   *\/ */
/*   ili_cmd(spi,0x29); // Display On */
/*   ili_cmd(spi,0x2c); */
/*   //CS0 = 1; */
/* } */


/* void send_line_finish(void)  */
/* { */
/*   spi_transaction_t *rtrans; */
/*   esp_err_t ret; */
/*   //Wait for all 6 transactions to be done and get back the results. */
/*   for (int x=0; x<5; x++) { */
/*     ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY); */
/*     assert(ret==ESP_OK); */
/*     //We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though. */
/*   } */
/* } */

/* void send_aline_finish(void)  */
/* { */
/*   spi_transaction_t *rtrans; */
/*   esp_err_t ret; */
/*   //Wait for all 6 transactions to be done and get back the results. */
/*   for (int x=0; x<1; x++) { */
/*     ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY); */
/*     assert(ret==ESP_OK); */
/*     //We could inspect rtrans now if we received any info back. The LCD is treated as write-only, though. */
/*   } */
/* } */

/* //To send a line we have to send a command, 2 data bytes, another command, 2 more data bytes and another command */
/* //before sending the line data itself; a total of 6 transactions. (We can't put all of this in just one transaction */
/* //because the D/C line needs to be toggled in the middle.) */
/* //This routine queues these commands up so they get sent as quickly as possible. */
/* void send_first(int ypos)  */
/* { */
/*   esp_err_t ret; */
/*   int x; */
/*   //Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this */
/*   //function is finished because the SPI driver needs access to it even while we're already calculating the next line. */
/*   static spi_transaction_t trans[6]; */
/*   static int transed; */
  
/*   //In theory, it's better to initialize trans and data only once and hang on to the initialized */
/*   //variables. We allocate them on the stack, so we need to re-init them each call. */
/*   for (x=0; x<6; x++) { */
/*     memset(&trans[x], 0, sizeof(spi_transaction_t)); */
/*     if ((x&1)==0) { */
/*       //Even transfers are commands */
/*       trans[x].length=8; */
/*       trans[x].user=(void*)0; */
/*     } else { */
/*       //Odd transfers are data */
/*       trans[x].length=8*4; */
/*       trans[x].user=(void*)1; */
/*     } */
/*     trans[x].flags=SPI_TRANS_USE_TXDATA; */
/*   } */
/*   trans[0].tx_data[0]=0x2A;           //Column Address Set */
/*   trans[1].tx_data[0]=1;              //Start Col High */
/*   trans[1].tx_data[1]=1;              //Start Col Low */
/*   trans[1].tx_data[3]=160;     //End Col Low */
/*   trans[2].tx_data[0]=0x2B;           //Page address set */
/*   trans[3].tx_data[0]=(ypos+2)>>8;        //Start page high */
/*   trans[3].tx_data[1]=(ypos+2)&0xff;      //start page low */
/*   trans[3].tx_data[2]=(129)>>8;    //end page high */
/*   trans[3].tx_data[3]=(129)&0xff;  //end page low */
/*   trans[4].tx_data[0]=0x2C;           //memory write */

/*   //Queue all transactions. */
/*   for (x=0; x<5; x++) { */
/*     ret=spi_device_queue_trans(spi, &trans[x], portMAX_DELAY); */
/*     assert(ret==ESP_OK); */
/*   } */

/*   //When we are here, the SPI driver is busy (in the background) getting the transactions sent. That happens */
/*   //mostly using DMA, so the CPU doesn't have much to do here. We're not going to wait for the transaction to */
/*   //finish because we may as well spend the time calculating the next line. When that is done, we can call */
/*   //send_line_finish, which will wait for the transfers to be done and check their status. */
/*   send_line_finish(); */
/* } */

/* //To send a line we have to send a command, 2 data bytes, another command, 2 more data bytes and another command */
/* //before sending the line data itself; a total of 6 transactions. (We can't put all of this in just one transaction */
/* //because the D/C line needs to be toggled in the middle.) */
/* //This routine queues these commands up so they get sent as quickly as possible. */
/* void send_line(int ypos, uint16_t *line)  */
/* { */
/*   esp_err_t ret; */
/*   int x; */
/*   //Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this */
/*   //function is finished because the SPI driver needs access to it even while we're already calculating the next line. */
/*   static spi_transaction_t trans[6]; */
/*   static int transed; */
  
/*   if(transed!=0)send_aline_finish(); */
/*   transed = 1; */
/*   //In theory, it's better to initialize trans and data only once and hang on to the initialized */
/*   //variables. We allocate them on the stack, so we need to re-init them each call. */
/*   for (x=0; x<6; x++) { */
/*     memset(&trans[x], 0, sizeof(spi_transaction_t)); */
/*     if ((x&1)==0) { */
/*       //Even transfers are commands */
/*       trans[x].length=8; */
/*       trans[x].user=(void*)1; */
/*     } else { */
/*       //Odd transfers are data */
/*       trans[x].length=8*4; */
/*       trans[x].user=(void*)1; */
/*     } */
/*     trans[x].flags=SPI_TRANS_USE_TXDATA; */
/*   } */
/*   trans[0].tx_buffer=line;            //finally send the line data */
/*   trans[0].length=160*2*8;            //Data length, in bits */
/*   trans[0].flags=0; //undo SPI_TRANS_USE_TXDATA flag */

/*   //Queue all transactions. */
/*   for (x=0; x<1; x++) { */
/*     ret=spi_device_queue_trans(spi, &trans[x], portMAX_DELAY); */
/*     assert(ret==ESP_OK); */
/*   } */

/*   //When we are here, the SPI driver is busy (in the background) getting the transactions sent. That happens */
/*   //mostly using DMA, so the CPU doesn't have much to do here. We're not going to wait for the transaction to */
/*   //finish because we may as well spend the time calculating the next line. When that is done, we can call */
/*   //send_line_finish, which will wait for the transfers to be done and check their status. */
/* } */



/* //Simple routine to generate some patterns and send them to the LCD. Don't expect anything too */
/* //impressive. Because the SPI driver handles transactions in the background, we can calculate the next line */
/* //while the previous one is being sent. */
/* static void display_pretty_colors(spi_device_handle_t spi) */
/* { */
/*   uint16_t line[2][160]; */
/*   int x, y, frame=0; */
/*   //Indexes of the line currently being sent to the LCD and the line we're calculating. */
/*   int sending_line=-1; */
/*   int calc_line=0; */
    
/*   while(1) { */
/*     frame++; */
/*     for (y=0; y<128; y++) { */
/*       //Calculate a line. */
/*       for (x=0; x<160; x++) { */
/* 	line[calc_line][x]=((x<<3)^(y<<3)^(frame+x*y)); */
/*       } */
/*       //Finish up the sending process of the previous line, if any */
/*       //Swap sending_line and calc_line */
/*       sending_line=calc_line; */
/*       calc_line=(calc_line==1)?0:1; */
/*       //Send the line we currently calculated. */
/*       send_line(y, line[sending_line]); */
/*       //The line is queued up for sending now; the actual sending happens in the */
/*       //background. We can go on to calculate the next line as long as we do not */
/*       //touch line[sending_line]; the SPI sending process is still reading from that. */
/*     } */
/*   } */
/* } */

/* void app_main() */
/* { */
/*   esp_err_t ret; */
/*   spi_bus_config_t buscfg={ */
/*     .miso_io_num=PIN_NUM_MISO, */
/*     .mosi_io_num=PIN_NUM_MOSI, */
/*     .sclk_io_num=PIN_NUM_CLK, */
/*     .quadwp_io_num=-1, */
/*     .quadhd_io_num=-1 */
/*   }; */
/*   spi_device_interface_config_t devcfg={ */
/*     .clock_speed_hz=20000000,               //Clock out at 20 MHz */
/*     .mode=0,                                //SPI mode 0 */
/*     .spics_io_num=PIN_NUM_CS,               //CS pin */
/*     .queue_size=7,                          //We want to be able to queue 7 transactions at a time */
/*     .pre_cb=ili_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line */
/*   }; */
/*   //Initialize the SPI bus */
/*   ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1); */
/*   assert(ret==ESP_OK); */
/*   //Attach the LCD to the SPI bus */
/*   ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi); */
/*   assert(ret==ESP_OK); */
/*   //Initialize the LCD */
/*   ili_init(spi); */
/*   //Go do nice stuff. */
/*   /\* display_pretty_colors(spi); *\/ */
/*   uint16_t hoge[160]; */
/*   printf("start!\n"); */
/*   main3d(); */
/* } */
//Simple routine to generate some patterns and send them to the LCD. Don't expect anything too
//impressive. Because the SPI driver handles transactions in the background, we can calculate the next line
//while the previous one is being sent.
/*
   This code generates an effect that should pass the 'fancy graphics' qualification
   as set in the comment in the spi_master code.
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_heap_caps.h"

static void display_pretty_colors(spi_device_handle_t spi)
{
    uint16_t *lines[2];
    //Allocate memory for the pixel buffers
    for (int i=0; i<2; i++) {
      lines[i]=heap_caps_malloc(320*PARALLEL_LINES*sizeof(uint16_t),MALLOC_CAP_DMA);
        assert(lines[i]!=NULL);
    }
    int frame=0;
    //Indexes of the line currently being sent to the LCD and the line we're calculating.
    int sending_line=-1;
    int calc_line=0;
    int i;
    while(1) {
        frame++;
        for (int y=0; y<240; y+=PARALLEL_LINES) {
            //Calculate a line.

	  for(i=0;i<320;i++)lines[calc_line][i] = y*i*1145147+frame*810931;
            //Finish up the sending process of the previous line, if any
            if (sending_line!=-1) send_line_finish(spi);
            //Swap sending_line and calc_line
            sending_line=calc_line;
            calc_line=(calc_line==1)?0:1;
            //Send the line we currently calculated.
            send_line(y, lines[sending_line]);
            //The line set is queued up for sending now; the actual sending happens in the
            //background. We can go on to calculate the next line set as long as we do not
            //touch line[sending_line]; the SPI sending process is still reading from that.
        }
    }
}

void app_main()
{
  /* printf("\nbone size is %d byte\n",sizeof(bone)); */
    esp_err_t ret;
    spi_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,
        .mosi_io_num=PIN_NUM_MOSI,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=PARALLEL_LINES*320*2+8
    };
    spi_device_interface_config_t devcfg={
#ifdef CONFIG_LCD_OVERCLOCK
        .clock_speed_hz=26*1000*1000,           //Clock out at 40 MHz
#else
        .clock_speed_hz=26*1000*1000,           //Clock out at 26 MHz
#endif
        .mode=0,                                //SPI mode 0
        .spics_io_num=PIN_NUM_CS,               //CS pin
        .queue_size=7,                          //We want to be able to queue 7 transactions at a time
        .pre_cb=lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };
    //Initialize the SPI bus
    ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    ESP_ERROR_CHECK(ret);
    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
    //Initialize the LCD
    lcd_init(spi);

    // gpio_set_direction(32, GPIO_MODE_OUTPUT);
 
    // gpio_set_level(32,1);

    // gpio_set_direction(37, GPIO_MODE_INPUT);
    // gpio_set_direction(38, GPIO_MODE_INPUT);
    // gpio_set_direction(39, GPIO_MODE_INPUT);

    
    /* //Initialize the effect displayed */
    /* ret=pretty_effect_init(); */
    /* ESP_ERROR_CHECK(ret); */

    /* //Go do nice stuff. */
    /* display_pretty_colors(spi); */
    //display_pretty_colors(spi);
    /* main3d(); */
    TaskHandle_t thread1,thread2;
    void vTask(void*);
#if PROCESS_NUM!=1
    xTaskCreatePinnedToCore(vTask,"vTask", 4096,  NULL,5,&thread1,1);
#endif
    main3d();
    while(1);
}
#endif
