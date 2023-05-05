// IO
#include <stdio.h>
// Default functionality
#include "pico/stdlib.h"
// String functions
#include <string.h>

// Define uart properties for the GPS
const uint32_t GPS_BAUDRATE = 9600;
const uint8_t GPS_TX = 16, GPS_RX = 17;
const uint8_t DATABITS = 8;
const uint8_t STARTBITS = 1;
const uint8_t STOPBITS = 1;

// Calculate the delay between bytes
const uint8_t BITS_PER_BYTE = STARTBITS + DATABITS + STOPBITS;
const uint32_t MICROSECONDS_PER_SECOND = 1000000;
const uint32_t GPS_DELAY = BITS_PER_BYTE * MICROSECONDS_PER_SECOND / GPS_BAUDRATE;

size_t uart_read_line(uart_inst_t *uart, char *buffer, const size_t max_length){
    size_t i;
    // Receive the bytes with as much delay as possible without missing data
    buffer[0] = uart_getc(uart);
    for(i = 1;i < max_length - 1 && buffer[i - 1] != '\n';i++){
        sleep_us(GPS_DELAY);
        buffer[i] = uart_getc(uart);
    }

    // End the string with a terminating 0 and return the length
    buffer[i] = '\0';
    return i;
}

bool is_correct(const char *message, const size_t length){
    char sum = 0;
    char checksum[3];
    size_t i;

    // The message should start with $ and end with \r\n
    if(message[0] != '$' || message[length - 1] != '\n' || message[length - 2] != '\r'){
        return false;
    }

    // Calculate the checksum    
    for(i = 1;i < length && message[i] != '*';i++){
        sum ^= message[i];
    }

    // If the current character isn't *, the message doesn't contain it and is invalid
    if(message[i] != '*'){
        return false;
    }

    // Convert the checksum to a hexadecimal string
    for(size_t i = 0;i < 2;i++){
        if(sum % 16 < 10){
            checksum[1 - i] = '0' + sum % 16;
        }else{
            checksum[1 - i] = 'A' + sum % 16 - 10;
        }
        sum >>= 4;
    }
    checksum[2] = '\0';

    // Return whether the checksum is equal to the found checksum
    return strncmp(checksum, message + i + 1, 2) == 0;
}

void send_with_checksum(uart_inst_t *uart, const char *message, const size_t length){
    char sum = 0;
    char checksum[3];

    // Calcute the checksum
    for(size_t i = 0;i < length && message[i] != '*';i++){
        sum ^= message[i];
    }

    // Convert the checksum to a hexadecimal string
    for(size_t i = 0;i < 2;i++){
        if(sum % 16 < 10){
            checksum[1 - i] = '0' + sum % 16;
        }else{
            checksum[1 - i] = 'A' + sum % 16 - 10;
        }
        sum >>= 4;
    }
    checksum[2] = '\0';

    // Send the message to the GPS in the expected format
    uart_putc_raw(uart, '$');
    uart_puts(uart, message);
    uart_putc(uart, '*');
    uart_puts(uart, checksum);
    uart_puts(uart, "\r\n");
}

int main() {
    char message[256];
    const char CONFIGURATIONS[] = "PMTK103*30<CR><LF>";
    uart_inst_t *gps_uart = uart0;

    // Initialize the standard IO and the uart for the gps
    stdio_init_all();
    uart_init(gps_uart, GPS_BAUDRATE);

    // Don't convert \n to \r\n
    uart_set_translate_crlf(gps_uart, false);

    // Enable the uart functionality for the pins connected to the GPS
    gpio_set_function(GPS_TX, GPIO_FUNC_UART);
    gpio_set_function(GPS_RX, GPIO_FUNC_UART);

    // Disable all types except GPRMC
    send_with_checksum(gps_uart, CONFIGURATIONS, sizeof(CONFIGURATIONS));
    
    while (1) {
        // Read a line from the GPS data
        const size_t length = uart_read_line(gps_uart, message, sizeof(message));

        // Skip to the next iteration, if the data is not correct or not of the correct type
        if(!is_correct(message, length)){
            continue;
        }
        
        // Print the received line of data
        printf("%s", message);
    }
}
