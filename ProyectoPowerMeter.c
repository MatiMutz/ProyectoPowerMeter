#include "mbed.h"
#include "ctype.h"
#include "string.h"
#include "esp_at.h"
#include "esp_socket.h"
#include "TextLCD.h"

//Defino el puerto serie
Serial DBG(USBTX, USBRX); // tx, rx

//Defino las patas que vamos a utilizar en el LCD
TextLCD LCD(PTB9, PTB10, PTB11, PTE2, PTE3, PTE4, TextLCD::LCD16x2);

//Definimos los dos sensores como entrada analogica
AnalogIn Current(A5);
AnalogIn Voltage(A3);

//Prototipos del programa
void proc_post(void);
void proc_post_tic_cb();
void protocol_ticker_cb();
void system_ticker_cb();
void ShowInLCD(int);
void ReadCurrent();
void ReadVoltage();
void protocol_process_data_in(unsigned char);
int concat(int);

//Variables a utilizar durante el proyecto:
short n = 0, m = 0, a = 0, b = 0, count = 0, first = 0, second = 0, thirst = 0, fourth = 0, caseVoltage = 0, caseCurrent = 0, voltMin = 0, voltMax = 0, currMin = 0, currMax = 0, suma1 = 0, suma2 = 0, suma3 = 0, suma4 = 0;
unsigned int VoltageValue, CurrentValue, SampleV1, SampleV2, SampleV3, SampleC1, SampleC2, SampleC3, ValorAlarma;
unsigned char buffer_data_http[255];
unsigned short proc_post_tic = 0;
float picoMinimoVoltaje = 0, picoMaximoVoltaje = 0, picoMinimoCorriente = 0, picoMaximoCorriente = 0;

/*****Definiciones para el timer del programa*****/
unsigned char tiempo = 0, tiempo2 = 0;
#define PROTOCOL_TIC_DEF (0.001)
#define PROTOCOL_TIMEOUT_RX (0.5)

//Declaro que voy a usar el timer
Ticker protocol_ticker;
/************************************/

/*****Definiciones para el tmo del sistema*****/
#define SYSTEM_TICKER_TIC_DEF (0.01) //10 mseg
#define SYSTEM_TICKER_1_SEG (1 / SYSTEM_TICKER_TIC_DEF)
Ticker system_ticker;

void system_ticker_cb()
{
    //procesamos los tiempos de la maquina de estados para hacer un post
    proc_post_tic_cb();
}
/************************************/

//Callback cuando se detecta una entrada
void onCharReceived()
{
    //Con el dato recibido llamamos a procesar el protcolo
    while (DBG.readable())
        protocol_process_data_in(DBG.getc());
}

int main()
{
    (void)WiFi_init("TeleCentro-df39", "EDNJZ2NYYGN2");

    //system_ticker.attach(&system_ticker_cb, SYSTEM_TICKER_TIC_DEF);

    protocol_ticker.attach(&protocol_ticker_cb, 0.0001);

    DBG.printf("POWER METER - GRUPO 14 - KOVALOW MORTOLA MUTZ VILLAMEDIANA\r\n");

    while (true)
    {

        ReadCurrent();
        ReadVoltage();
        ShowInLCD(1);
        (void)WiFi_Step();

        proc_post();

        /**************** CIRCO ****************/
        if (socket_connected)
            led_rojo = 0;
        else
            led_rojo = 1;
        /**************** CIRCO ****************/
        if (suma1 == 1 && suma2 == 1 && suma3 == 1 && suma4 == 1)
        {
            if ((tiempo <= 100) && (tiempo2 <= 100))
            {
                DBG.printf("\r\nVoltaje:\r\nMinimo:%.0f\r\n", picoMinimoVoltaje);
                DBG.printf("Maximo:%.0f\r\n", picoMaximoVoltaje);
                DBG.printf("Corriente:\r\nMinimo:%.0f\r\n", picoMinimoCorriente);
                DBG.printf("Maximo:%.0f\r\n", picoMaximoCorriente);
                DBG.printf("Valor medio volt:%.0f\r\n", (picoMinimoVoltaje + ((picoMaximoVoltaje - picoMinimoVoltaje) / 2)));
                DBG.printf("Valor medio corr:%.0f\r\n", (picoMinimoCorriente + ((picoMaximoCorriente - picoMinimoCorriente) / 2)));
                DBG.printf("Tiempo:%d\r\n", tiempo);
                DBG.printf("Tiempo2:%d\r\n", tiempo2);
            }
            suma1 = 0;
            suma2 = 0;
            suma3 = 0;
            suma4 = 0;
            picoMinimoVoltaje = 0;
            picoMaximoVoltaje = 0;
            picoMinimoCorriente = 0;
            picoMaximoCorriente = 0;
            tiempo = 0;
            tiempo2 = 0;
            n = 0;
            m = 0;
            a = 0;
            b = 0;
        }
    }
}

void proc_post_tic_cb()
{
    if (proc_post_tic)
        proc_post_tic--;
}

enum
{
    PROC_POST_OPEN,
    PROC_POST_SEND,
    PROC_POST_RECV,
    PROC_POST_CLOSE,
};

void proc_post(void)
{
    int res;
    static unsigned char proc_post_step = PROC_POST_OPEN;

    if (proc_post_tic)
        return;

    proc_post_tic = SYSTEM_TICKER_1_SEG;

    //static unsigned char proc_post_step = 0;
    if (!has_ip)
    { //no tenemos ip aun nos vamos
        proc_post_step = PROC_POST_OPEN;
        return;
    }

    char buff_send[128];
    switch (proc_post_step)
    {
    case PROC_POST_OPEN:
        if (sock_connect(SOCKET_TCP, "whitenergy.online", 80, 10000) == 0)
        {
            proc_post_tic = 1 * SYSTEM_TICKER_1_SEG;
            proc_post_step = PROC_POST_SEND;
        }
        else
        {
            //no pudimos abrir, intentamos en 30 segundos
            proc_post_tic = 30 * SYSTEM_TICKER_1_SEG;
            proc_post_step = PROC_POST_OPEN;
            sock_close();
        }
        break;

    case PROC_POST_SEND:
        //mandar y con el que manda recibir alarmas, todo en 1
        sprintf(buff_send, "GET /php/getAlarms.php HTTP/1.1\r\nHost: whitenergy.online\r\nConnection: Close\r\n\r\n");
        res = sock_send((unsigned char *)buff_send,
                        strlen(buff_send), 25000);
        DBG.printf("proc_post: send: (%d)%s\r\n", res, buff_send);
        if (res > 0)
        {
            proc_post_tic = SYSTEM_TICKER_1_SEG;
            proc_post_step = PROC_POST_RECV;
            DBG.printf("proc_posrt: -> [%d] PROC_POST_RECV\r\n", __LINE__);
        }
        else
        {
            proc_post_tic = 2 * SYSTEM_TICKER_1_SEG;
            proc_post_step = PROC_POST_CLOSE;
            DBG.printf("proc_posrt: -> [%d] PROC_POST_CLOSE\r\n", __LINE__);
        }
        break;

    case PROC_POST_RECV:
        //cambiar el largo del dato, esta en [250] ahora
        res = sock_recv(buffer_data_http, 250, 500);
        DBG.printf("proc_posrt: -> [%d] PROC_POST_RECV:%d\r\n", __LINE__, res);
        if (res > 0)
        {
            if (strstr((char *)buffer_data_http, "200 OK"))
            {
                DBG.printf("proc_posrt: -> [%d] 200 OK\r\n", __LINE__);
            }
        }
        //usar el strstr para obtener la informacion del json recibido.
        proc_post_tic = SYSTEM_TICKER_1_SEG;
        proc_post_step = PROC_POST_CLOSE;
        DBG.printf("proc_posrt: -> [%d] PROC_POST_CLOSE\r\n", __LINE__);
        break;

    default:
    case PROC_POST_CLOSE:
        //no pudimos abrir, intentamos en 10 segundos
        proc_post_tic = 25 * SYSTEM_TICKER_1_SEG;
        proc_post_step = PROC_POST_OPEN;
        //No es necesario cerrar, se lo estamos pidiendo al servidor
        //sock_close();
        break;
    }
}

//Funcion para leer los picos maximos y minimos del sensor de tension
void ReadVoltage()
{
    VoltageValue = ((Voltage.read() * 100) * 3.3f);
    switch (caseVoltage)
    {
    case 0:
        SampleV1 = VoltageValue;
        caseVoltage = 1;
        break;
    case 1:
        SampleV2 = VoltageValue;
        if (SampleV1 == SampleV2)
        {
            caseVoltage = 1;
        }
        else if (SampleV2 < SampleV1)
        {
            caseVoltage = 2;
        }
        else if (SampleV2 > SampleV1)
        {
            caseVoltage = 3;
        }
        break;
    case 2:
        SampleV3 = VoltageValue;
        if ((SampleV2 < SampleV1) && (SampleV2 < SampleV3) && suma1 == 0)
        {
            n = 1;
            //PICO MINIMO VOLTAGE
            picoMinimoVoltaje = picoMinimoVoltaje + SampleV2;
            voltMin++;
            if (voltMin == 10 && suma1 == 0)
            {
                picoMinimoVoltaje = picoMinimoVoltaje / 10;
                suma1 = 1;
                voltMin = 0;
            }
            caseVoltage = 0;
        }
        else if (SampleV2 == SampleV3)
        {
            caseVoltage = 2;
        }
        else
        {
            caseVoltage = 0;
        }
        break;
    case 3:
        SampleV3 = VoltageValue;
        if ((SampleV2 > SampleV1) && (SampleV2 > SampleV3) && suma2 == 0)
        {
            a = 1;
            //PICO MAXIMO VOLTAGE
            picoMaximoVoltaje = picoMaximoVoltaje + SampleV2;
            voltMax++;
            if (voltMax == 10 && suma2 == 0)
            {
                picoMaximoVoltaje = picoMaximoVoltaje / 10;
                suma2 = 1;
                voltMax = 0;
            }
            caseVoltage = 0;
        }
        else if (SampleV2 == SampleV3)
        {
            caseVoltage = 3;
        }
        else
        {
            caseVoltage = 0;
        }
        break;
    }
}

//Funcion para leer los picos maximos y minimos del sensor de corriente
void ReadCurrent()
{
    CurrentValue = ((Current.read() * 100) * 3.3);
    switch (caseCurrent)
    {
    case 0:
        SampleC1 = CurrentValue;
        caseCurrent = 1;
        break;
    case 1:
        SampleC2 = CurrentValue;
        if (SampleC1 == SampleC2)
        {
            caseCurrent = 1;
        }
        else if (SampleC2 < SampleC1)
        {
            caseCurrent = 2;
        }
        else if (SampleC2 > SampleC1)
        {
            caseCurrent = 3;
        }
        break;
    case 2:
        SampleC3 = CurrentValue;
        if ((SampleC2 < SampleC1) && (SampleC2 < SampleC3) && suma3 == 0)
        {
            if (n == 1)
            {
                m = 1;
            }
            //PICO MINIMO CORRIENTE
            picoMinimoCorriente = picoMinimoCorriente + SampleC2;
            currMin++;
            if (currMin == 10 && suma3 == 0)
            {
                picoMinimoCorriente = picoMinimoCorriente / 10;
                suma3 = 1;
                currMin = 0;
            }
            caseCurrent = 0;
        }
        else if (SampleC2 == SampleC3)
        {
            caseCurrent = 2;
        }
        else
        {
            caseCurrent = 0;
        }
        break;
    case 3:
        SampleC3 = CurrentValue;
        if ((SampleC2 > SampleC1) && (SampleC2 > SampleC3) && suma4 == 0)
        {
            if (a == 1)
            {
                b = 1;
            }
            //PICO MAXIMO CORRIENTE
            picoMaximoCorriente = picoMaximoCorriente + SampleC2;
            currMax++;
            if (currMax == 10 && suma4 == 0)
            {
                picoMaximoCorriente = picoMaximoCorriente / 10;
                suma4 = 1;
                currMax = 0;
            }
            caseCurrent = 0;
        }
        else if (SampleC2 == SampleC3)
        {
            caseCurrent = 3;
        }
        else
        {
            caseCurrent = 0;
        }
        break;
    }
}

//Funcion para mostrar en el Display LCD
void ShowInLCD(int option)
{
    switch (option)
    {
    case 1:
        LCD.locate(0, 0);
        LCD.printf("KOVA");
        LCD.locate(0, 1);
        LCD.printf("PUTO");
        break;
    default:
        LCD.locate(0, 0);
        LCD.printf("DALE");
        LCD.locate(0, 1);
        LCD.printf("BOKE");
        break;
    }
}

//funcion con llamado recurrente
void protocol_ticker_cb()
{
    if (n == 1 && m == 0)
    {
        tiempo++;
    }
    if (a == 1 && b == 0)
    {
        tiempo2++;
    }
}
