#include "mbed.h"

#define SLAVE_ADDR      0x58
#define REGISTER_SIZE   256
#define I2C_BUFSIZE     2

Timeout ONESHOT;
DigitalOut IRQ(dp17);
Serial UART(dp16, dp15);
I2CSlave SLAVE(dp5, dp27);

uint8_t REGISTER[REGISTER_SIZE];    // use only 13 items
enum { ST_OFF, ST_START, ST_IDLE, ST_BREW, ST_END, ST_EMPTY_WATER, ST_COVER_OPEN, ST_DRAWER_OPEN } STATE;

void initialize();
void handle_uart();
void poll_i2c();
void check_state();

int main() {
    initialize();
    SLAVE.address(SLAVE_ADDR << 1);
    UART.attach(handle_uart , Serial::RxIrq);
    poll_i2c();
}

void initialize()
{
    if (IRQ != 0) IRQ = 0;
    for (int i=0; i<REGISTER_SIZE; i++) REGISTER[i] = 0xFF;
    REGISTER[0x80] = 0x07;
    STATE = ST_OFF;
}

void handle_uart()
{
    char recv = UART.getc();
    switch(recv) {
    case 's':   // START
        if (STATE != ST_OFF) { UART.putc('N'); return; }
        REGISTER[0x00] = 0x01;
        STATE = ST_START;
        UART.putc('Y');
        IRQ = 1;
        break;
    case 'e':   // END
        if (STATE == ST_OFF) { UART.putc('N'); return; }
        REGISTER[0x00] = 0x01;
        STATE = ST_END;
        UART.putc('Y');
        IRQ = 1;
        break;
    case 'E':   // Espresso
    case 'M':   // Black Coffee (Mag size)
    case 'B':   // Black Coffee
    case 'C':   // Cappuccino
    case 'L':   // Cafe Latte
        if (STATE != ST_IDLE) { UART.putc('N'); return; }
        REGISTER[0x00] = ((recv == 'E') ? 0x02 :
                          (recv == 'M') ? 0x10 :
                          (recv == 'B') ? 0x20 :
                          (recv == 'C') ? 0x08 : 0x04);    
        STATE = ST_BREW;
        UART.putc('Y');
        IRQ = 1;
        break;
    case '?':   // Status
        UART.putc((STATE == ST_OFF)   ? 'X' :
                  (STATE == ST_START) ? 'S' :
                  (STATE == ST_IDLE)  ? 'I' :
                  (STATE == ST_BREW)  ? 'B' :
                  (STATE == ST_END)   ? 'E' :
                  (STATE == ST_EMPTY_WATER) ? 'W' :
                  (STATE == ST_COVER_OPEN)  ? 'C' :
                  (STATE == ST_DRAWER_OPEN) ? 'D' : '?');
        break;
    case 'd':   // DEBUG or DUMP
        UART.printf("DEBUG\r\n");
        UART.printf("STATE = %d\r\n", STATE);
        UART.printf("\tREG[0x%02X] = 0x%02X\r\n", 0x00, REGISTER[0x00]);
        for (int i=0x10; i<=0x19; i++) UART.printf("\tREG[0x%02X] = 0x%02X\r\n", i, REGISTER[i]);
        UART.printf("\tREG[0x%02X] = 0x%02X\r\n", 0x20, REGISTER[0x20]);
        UART.printf("\tREG[0x%02X] = 0x%02X\r\n", 0x80, REGISTER[0x80]);
        break;
    }
}

void poll_i2c()
{
    uint8_t read_addr = 0x00;
    uint8_t button_id = 0x00;
    uint8_t buf[I2C_BUFSIZE];
    
    while (1) {
        switch (SLAVE.receive()) {
        case I2CSlave::WriteAddressed:
            SLAVE.read((char *)buf, I2C_BUFSIZE);
            if (buf[0] == 0x00 || buf[0] == 0x80) {
                read_addr = buf[0];
            } else if (0x10 <= buf[0] <= 0x19 || buf[0] == 0x20) {
                REGISTER[buf[0]] = buf[1];
            }
            break;
        case I2CSlave::ReadAddressed:
            if (IRQ != 0) IRQ = 0;
            SLAVE.write((char *)REGISTER + read_addr, 1);
            if (read_addr != 0x00) break;
            if (REGISTER[read_addr] != 0x00) {
                button_id = REGISTER[read_addr];
                REGISTER[read_addr] = 0x00;
            } else {
                REGISTER[read_addr] = button_id;
                button_id = 0x00;
            }
            break;
        default:
            continue;
        }
        
        ONESHOT.detach();
        ONESHOT.attach_us(check_state, 200000);
    }
}

void check_state()
{
    /* Second IRQ */
    if (REGISTER[0x00] == 0x00) {
        switch(STATE) {
        case ST_START:
            if (REGISTER[0x20] == 0x01) {   // Re-try
                initialize();
                REGISTER[0x00] = 0x01;
                STATE = ST_START;
            }
        case ST_END:
        case ST_BREW:
            IRQ = 1;
            return;
        }
    }
    
    /* State transition */
    if (REGISTER[0x20] == 0x01) {           // Power off
        initialize();
    } else if (REGISTER[0x11] == 0x02) {    // Heater
        STATE = ST_START;
    } else if (REGISTER[0x10] == 0x00 && REGISTER[0x11] == 0x01) {  // Error
        if (REGISTER[0x14] == 0x02) STATE = ST_EMPTY_WATER;
        if (REGISTER[0x15] == 0x02) STATE = ST_COVER_OPEN;
        if (REGISTER[0x18] == 0x02) STATE = ST_DRAWER_OPEN;
    } else if (REGISTER[0x10] == 0x01 && REGISTER[0x11] == 0x00) {  // Idle or Brew
        bool flg = true;
        for (int i=0x15; i<=0x19; i++) flg = flg && (REGISTER[i] == 0x01);
        STATE = flg ? ST_IDLE : ST_BREW;
    }
}
