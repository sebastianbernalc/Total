#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "mpu9250.h"
#include "hardware/uart.h" 

#define UART_ID uart0
#define BAUD_RATE 115200

//WIFI
// Uart pins
#define UART_TX_PIN 0
#define UART_RX_PIN 1
//WIFI
char SSID[] = "Sebas";
char password[] = "12345678";
char ServerIP[] = "192.168.243.109";
char Port[] = "8080";
char uart_command[50] = "";
char buf[256] = {0};

bool send_sensor_values(const char *sensorName, const char *humidity, const char *temperature);
bool sendCMD(const char *cmd, const char *act);
void connectToWifi();
//WIFI
//WIFI FUNCTIONS
/// Send sensor values to REST api using TCP connection
bool send_sensor_values(const char *sensorName, const char *humidity, const char *temperature)
{
    // Open connection
    sprintf(uart_command, "AT+CIPSTART=\"TCP\",\"%s\",%s", ServerIP, Port);
    sendCMD(uart_command, "OK");

    // Send data
    sendCMD("AT+CIPMODE=1", "OK");
    sleep_ms(1000);
    sendCMD("AT+CIPSEND", ">");
    sleep_ms(2000);

    char buf[20];
    snprintf(buf, sizeof(buf), "%s%s\r\n", "hola", "mundo");
    printf(buf);
    uart_puts(UART_ID, buf);

    sleep_ms(5000);            // Seems like this is critical
    uart_puts(UART_ID, "+++"); // Look for ESP docs

    // Close connection
    //sleep_ms(1000);
    //sendCMD("AT+CIPCLOSE", "OK");
    //sleep_ms(1000);
    //sendCMD("AT+CIPMODE=0", "OK");

    return true;
}

bool sendCMD(const char *cmd, const char *act)
{
    int i = 0;
    uint64_t t = 0;

    uart_puts(UART_ID, cmd);
    uart_puts(UART_ID, "\r\n");

    t = time_us_64();
    while (time_us_64() - t < 2500 * 1000)
    {
        while (uart_is_readable_within_us(UART_ID, 2000))
        {
            buf[i++] = uart_getc(UART_ID);
        }
        if (i > 0)
        {
            buf[i] = '\0';
            printf("%s\r\n", buf);
            if (strstr(buf, act) != NULL)
            {
                return true;
            }
            else
            {
                i = 0;
            }
        }
    }
    //printf("false\r\n");
    return false;
}

void connectToWifi()
{
    sendCMD("AT", "OK");
    sendCMD("AT+CWMODE=3", "OK");
    sprintf(uart_command, "AT+CWJAP=\"%s\",\"%s\"", SSID, password);
    sendCMD(uart_command, "OK");
}
//WIFI FUNCTIONS

//Gps------------------------------------
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


void decode(char gpsString[256], float * latitud, float * longitud) {
  char *token = strtok(gpsString, ",");
  char *token_old, *latitude = 0, *longitude = 0;

  while (token != NULL) {
    if (strcmp(token, "N") == 0) {
      latitude = token_old;
    }
    else if (strcmp(token, "W") == 0) {
      longitude = token_old;
    }
    token_old = token;
    token = strtok(NULL, ",");
  }

  if (latitude != NULL && longitude != NULL){
    float lat = atof(latitude);
    int aux = (int)lat/100;
    *latitud = (lat-(100*aux))/60 + (float)aux;
    

    float lon = -atof(longitude);
    int aux2 = (int)lon/100;
    *longitud = (lon-(100*aux2))/60 + (float)aux2;
  }
  
}


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

//Gps---------------------------------------

int16_t* calibrar(int16_t gyro[], int16_t numero){
  static int16_t resultado[3];
  for (int i = 0; i < numero; i++) {
    mpu9250_read_raw_gyro(gyro);
    resultado[0] += gyro[0];
    resultado[1] += gyro[1];
    resultado[2] += gyro[2];
  }
  resultado[0] /= numero;
  resultado[1] /= numero;
  resultado[2] /= numero;
  return resultado;
}

int main()
{
  sleep_ms(5000);

  //WIFI
  printf("\nProgram start\n");

  uart_init(UART_ID, BAUD_RATE); // Set up UART

  // Set the TX and RX pins using the function select
  gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

  uart_puts(UART_ID, "+++");
  sleep_ms(1000);
  while (uart_is_readable(UART_ID))
      uart_getc(UART_ID);
  sleep_ms(2000);

  
  //Remove comments to connect to wifi 
  sleep_ms(2000);
  connectToWifi(); 
  sleep_ms(2000);
  

  // Uncomment to get the IP address of the ESP
  // sendCMD("AT+CIFSR", "OK"); // ASK IP

  // Example values
  send_sensor_values("test", "59.2", "42.2");
  //WIFI

  //GPS-----------------------------------------------------
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

  //GPS------------------------------------------------------
    

    printf("Hello, MPU9250! Reading raw data from registers via SPI...\n");

    start_spi();  //Starts the mpu

    int16_t  magCal[3],mag[3],acceleration[3], gyro[3], gyroCal[3], eulerAngles[2], fullAngles[2]; //Declares the variables required for calculations
    float gyro_f[3],acceleration_f[3],mag_f[3];
    absolute_time_t timeOfLastCheck;

    calibrate_gyro(gyroCal, 100);  //Calibrates the gyro
    calibrate_mag(magCal, 100);  //Calibrates the magnetometer

    mpu9250_read_raw_accel(acceleration);  //Sets the absolute angle using the direction of gravity
    calculate_angles_from_accel(eulerAngles, acceleration);
    timeOfLastCheck = get_absolute_time();

    while (1)
    {
        //WIFI
        send_sensor_values("test", "59.2", "42.2");
        sleep_ms(1000);
        //WIFI

        //GPS----------------------------------------------------------------------
        // Read a line from the GPS data
        const size_t length = uart_read_line(gps_uart, message, sizeof(message));

        // Skip to the next iteration, if the data is not correct or not of the correct type
        if(!is_correct(message, length)){
            continue;
        }
          
          // Print the received line of data
        //Compara trama para solo usar GNRMC y extraer datos
        //printf("%s", message);
        if (strncmp(message, "$GNRMC", strlen("$GNRMC")) == 0){
          float latitud = 0, longitud = 0;
          decode(message, &latitud, &longitud);
          printf("Latitud %.4f, Longitud %.4f\n", latitud, longitud);
        }
        
        //gps--------------------------------------------------------------------------
        mpu9250_read_raw_accel(acceleration);  //Reads the accel and gyro 
        mpu9250_read_raw_gyro(gyro);
        mpu9250_read_raw_mag(mag);
        //Calibracion giroscopo
        gyro[0] -= gyroCal[0];  //Applies the calibration
        gyro[1] -= gyroCal[1];
        gyro[2] -= gyroCal[2];

        gyro_f[0] = ((float)gyro[0]/32768)*250;  //Applies the calibration
        gyro_f[1] = ((float)gyro[1]/32768)*250;
        gyro_f[2] = ((float)gyro[2]/32768)*250;

        //Calibracion magnetometro
        mag[0] -= magCal[0];  //Applies the calibration
        mag[1] -= magCal[1];
        mag[2] -= magCal[2];

        mag_f[0] = ((float)mag[0]/32768)*4800;  //Applies the calibration
        mag_f[1] = ((float)mag[1]/32768)*4800;
        mag_f[2] = ((float)mag[2]/32768)*4800;


        //Calibracion aceleracion
        acceleration_f[0] = ((float)acceleration[0]/32768)*2;
        acceleration_f[1] = ((float)acceleration[1]/32768)*2;
        acceleration_f[2] = ((float)acceleration[2]/32768)*2;

        
        calculate_angles(eulerAngles, acceleration, gyro, absolute_time_diff_us(timeOfLastCheck, get_absolute_time()));  //Calculates the angles
        timeOfLastCheck = get_absolute_time();

        convert_to_full(eulerAngles, acceleration, fullAngles);

        //printf("Gyro. X_FINAL = %f, Y_FINAL = %f, Z_FINAL = %f\n", gyro_f[0], gyro_f[1], gyro_f[2]);
        //printf("Acc. X = %f, Y = %f, Z = %f\n", acceleration_f[0], acceleration_f[1], acceleration_f[2]);  //Prints the angles
        //printf("Euler. Roll = %d, Pitch = %d\n", eulerAngles[0], eulerAngles[1]);
        //printf("Full. Roll = %d, Pitch = %d\n", fullAngles[0], fullAngles[1]);
        printf("Mag. X_FINAL = %f, Y_FINAL = %f, Z_FINAL = %f\n", mag_f[0], mag_f[1], mag_f[2]);
        
        //sleep_ms(1);

    }
}