#include <AP_HAL/AP_HAL.h>

#if CONFIG_HAL_BOARD == HAL_BOARD_86DUINO

#include <stdio.h>
#include <assert.h>
#include <signal.h>

#include "HAL_86Duino_Class.h"
#include "Scheduler.h"
#include "AnalogIn.h"
#include "UARTDriver.h"
#include "I2CDevice.h"
#include "SPIDevice.h"
#include "Storage.h"
#include "RCInput.h"
#include "RCOutput.h"
#include "GPIO.h"
#include "Util.h"
#include "USBSerial.h"

#include "io.h"
#include "irq.h"
#include "pins_arduino.h"

unsigned _stklen = 4096 * 1024; // set stack to 4096k

using namespace x86Duino;
static Scheduler x86Scheduler;
static RCInput  x86RCInput;
static RCOutput x86RCOutput;
static AnalogIn x86AnalogIn;
static GPIO x86GPIO;
static I2CDeviceManager x86I2C;
static SPIDeviceManager x86SPI;
static Storage x86Storage;
static Util x86Util;


static UARTDriver Serial1(COM1, 115200L, BYTESIZE8|NOPARITY|STOPBIT1, 0L, 500L);
static UARTDriver Serial2(COM2, 115200L, BYTESIZE8|NOPARITY|STOPBIT1, 0L, 500L);
static UARTDriver Serial3(COM3, 115200L, BYTESIZE8|NOPARITY|STOPBIT1, 0L, 500L);
static UARTDriver Serial485(COM4, 115200L, BYTESIZE8|NOPARITY|STOPBIT1, 0L, 500L);
static USBSerial usbUart;

extern int wdt_count, timer_1k_count, spi_count, spi_mpu9250_count ,rc_in_count, uart_count;
volatile bool in_loop = false ;

void _86Duino_error_process(int num) {
	int ledpin = 13;
	// disable all irq except usb irq (5)
	i8259_DisableIRQ(0);
	i8259_DisableIRQ(1);
	i8259_DisableIRQ(3);
	i8259_DisableIRQ(4);
	i8259_DisableIRQ(6);
	i8259_DisableIRQ(7);
	i8259_DisableIRQ(8);
	i8259_DisableIRQ(9);
	i8259_DisableIRQ(10);
	i8259_DisableIRQ(11);
	i8259_DisableIRQ(12);
	i8259_DisableIRQ(13);
	i8259_DisableIRQ(14);

	// print error message
	printf("\nOop, this program is crash :(\n");
	printf("You may write a bug in your sketch, check and upload it again.\n");

//	// led blink pattern
//	pinMode(ledpin, OUTPUT);
	while(1)
	{
//		error_led_blink(ledpin);
	}
}

static __attribute__((constructor(101))) void _f_init()
{
	signal(SIGSEGV, _86Duino_error_process);
	signal(SIGFPE, _86Duino_error_process);
}

HAL_86Duino::HAL_86Duino() :
    AP_HAL::HAL(
        &usbUart,   /* uartA */
        &Serial1,   /* uartB */
        &Serial2,   /* uartC */
        &Serial3,   /* uartD */
        &Serial485,   /* uartE */
        nullptr,   /* uartF */
        &x86I2C,   /* i2c */
        &x86SPI,          /* spi */
        &x86AnalogIn,      /* analogin */
        &x86Storage, /* storage */
        &usbUart,   /* console */
        &x86GPIO,          /* gpio */
        &x86RCInput,       /* rcinput */
        &x86RCOutput,      /* rcoutput */
        &x86Scheduler,     /* scheduler */
        &x86Util,      /* util */
        nullptr, /* onboard optical flow */
        nullptr) /* CAN bus */
{}

void HAL_86Duino::run(int argc, char * const argv[], Callbacks* callbacks) const
{
    assert(callbacks);

    if(io_Init() == false) return;
    timer_NowTime(); // initialize timer

    // Set IRQ4 as level-trigger
    io_outpb(0x4D0, io_inpb(0x4D0) | 0x10);

    //set corssbar Base Address
    int crossbarBase = sb_Read16(SB_CROSSBASE) & 0xfffe;
    if(crossbarBase == 0 || crossbarBase == 0xfffe)
    {
        sb_Write16(SB_CROSSBASE, CROSSBARBASE | 0x01);
    }

    // Force set HIGH speed ISA on SB
    sb_Write(SB_FCREG, sb_Read(SB_FCREG) | 0x8000C000L);

    // GPIO->init
    x86GPIO.init();
    // AD->init
    x86AnalogIn.init();
    // set MCM Base Address, init before using PWM in/out
    set_MMIO();
    mcmInit();
    for(int i=0; i<4; i++)
        mc_SetMode(i, MCMODE_PWM_SIFB);

    // set MCM IRQ
    if(irq_Init() == false) printf("MCM IRQ init fail\n");
    if(irq_Setting(GetMCIRQ(), IRQ_LEVEL_TRIGGER | IRQ_DISABLE_INTR | IRQ_USE_FPU) == false)
        printf("MCM IRQ Setting fail\n");

    Set_MCIRQ(GetMCIRQ());
    // RC in/out
    x86RCOutput.init();
    x86RCInput.init();

    // USB-CDC init()
    usbUart.begin(115200);

    // Scheduler init
    x86Scheduler.init();

    // Storage init
    Serial1.begin(115200);    
    x86Storage.init();

    // test zone
    x86GPIO.pinMode(13, HAL_GPIO_OUTPUT);
//    x86Util.set_system_clock(1506577910909*1000ULL);


    callbacks->setup();
    scheduler->system_initialized();

    for (;;) {

//        x86Scheduler.run_spi_thread();
//        x86Scheduler.run_i2c_thread();    
        x86Scheduler.run_io();

        callbacks->loop();  // use polling - self control frequency
//        static uint64_t next_loop_us = AP_HAL::micros64() + 2500;
//        if( next_loop_us < AP_HAL::micros64() )
//        {
//            next_loop_us = AP_HAL::micros64() + 2500;   // call loop at 400 hz (2500us)
//            in_loop = true ;
//            callbacks->loop();
//            in_loop = false ;
//        }

//        static uint32_t alive_count = AP_HAL::millis();
//        if( AP_HAL::millis() > alive_count + 1000 )
//        {
//            alive_count = AP_HAL::millis();
////            usbUart.printf("alive: %d, 1k: %d, wdt: %d, spi: %d, mpu: %d\n", alive_count, timer_1k_count, wdt_count, spi_count, spi_mpu9250_count);
//            usbUart.printf("alive: %d, 1k: %d, wdt: %d, rc_in: %d, uart: %d, spi: %d, mpu: %d\n"
//                           , alive_count, timer_1k_count, wdt_count, rc_in_count, uart_count, spi_count, spi_mpu9250_count );
//        }
//        usbUart.printf("main..\n");
//        x86Scheduler.delay(100);
//        usbUart.printf("A: %d, B: %d, C: %d\n", Serial1.txspace(), Serial2.txspace(), Serial3.txspace());

//        Serial1.printf("ms: %llu\n", x86Util.get_system_clock_ms() );

//        static uint32_t count = 1000;
//        static int8_t sign = 1;
//        count+= 20*sign;
//        x86RCOutput.write(CH_1, count);
//        if(count > 2000) sign = -1;
//        if(count < 1000) sign = 1;

//        if( x86RCInput.new_input() )
//        {
//            uint16_t RC_in[6];
//            x86RCInput.read( RC_in, 6);
//            Serial1.printf("ms:%d CH %d %d %d %d ,%d %d\n",AP_HAL::millis(),RC_in[0] ,RC_in[1] ,RC_in[2] ,RC_in[3] ,RC_in[4] ,RC_in[5]);
////            Serial1.printf("ms:%d \n",AP_HAL::millis());
//            x86RCOutput.write(CH_2, RC_in[2]);
//        }

//        static int last_print = AP_HAL::millis() ;
//        if( AP_HAL::millis() - last_print > 1000 )
//        {
//            last_print = AP_HAL::millis();
////            Serial1.printf("ms: %d , wdt_count: %d \n", last_print, wdt_count );
//            usbUart.printf("sys:%llu, compile:%lu\n",x86Util.get_system_clock_ms()/1000, x86Util.compile_time(__DATE__ , __TIME__) );
//        }
//        // storage test & hal test
//        static auto _perf_write = x86Util.perf_alloc(AP_HAL::Util::PC_ELAPSED, "DF_write");
//        static uint32_t idx_w = 0 ;
//        if( idx_w < 16 )
//        {
//            idx_w ++ ;
//            uint8_t buf[1024] ;
//            memset( buf, idx_w, sizeof(buf)) ;
//            x86Util.perf_begin(_perf_write);
//            x86Storage.write_block(idx_w*1024, buf, sizeof(buf));
//            x86Util.perf_end(_perf_write);
//        }

//        x86Util._debug_counters();

////        if(AP_HAL::millis()/1000 > 15 ) x86Scheduler.reboot(1);

////        Serial1.printf("ms:%d, usb:%s\n", AP_HAL::millis(), x86GPIO.usb_connected()? "connected" : "not connect");    // @nasamit
////        Serial1.printf("ms:%d, ch:%d, value:%d\n", AP_HAL::millis(), x86AnalogIn.channel(1)->read_latest(),
////                       x86AnalogIn.channel(0)->read_latest());    // @nasamit
////        x86GPIO.toggle(13);
////        x86AnalogIn.update();

//        static auto _perf_write = x86Util.perf_alloc(AP_HAL::Util::PC_ELAPSED, "loop_time");
//        x86Util.perf_begin(_perf_write);
//        callbacks->loop();
//        x86Util.perf_end(_perf_write);
//        x86Util._debug_counters();

    }
}

const AP_HAL::HAL& AP_HAL::get_HAL() {
    static const HAL_86Duino hal;
    return hal;
}

#endif  // CONFIG_HAL_BOARD == HAL_BOARD_86DUINO
