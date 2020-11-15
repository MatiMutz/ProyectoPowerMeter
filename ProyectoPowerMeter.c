#pragma region ZonaIncludes
/****** INCLUDES DE LIBRERIAS A UTILIZAR ******/
/**********************************************/
#include "mbed.h"
#include "ctype.h"
#include "string.h"
#include "esp_at.h"
#include "esp_socket.h"
#include "TextLCD.h"
#include <math.h>
/**********************************************/
#pragma endregion ZonaIncludes

#pragma region ZonaPuertoSerie
/****** DEFINICION DEL PUERTO SERIE ******/
Serial DBG(USBTX, USBRX); // tx, rx
#define PC DBG
/*****************************************/
#pragma endregion ZonaPuertoSerie

#pragma region ZonaDeEntradasYSalidas
/****** ENTRADAS Y SALIDAS DEL SISTEMA ******/
TextLCD LCD(PTB9, PTB10, PTB11, PTE2, PTE3, PTE4, TextLCD::LCD16x2); //Definicion de las patas del LCD.
AnalogIn Current(A5);                                                //Sensor de Corriente
AnalogIn Voltage(A3);                                                //Sensor de Tension
DigitalOut Rele(PTB8);                                               //Rele para accionar o no accionar lampara
DigitalOut Buzzer(PTC16);                                            //Buzzer para alarma
DigitalOut Rojo(PTC12);                                              //Led Rojo
DigitalOut Verde(PTC13);                                             //Led Verde
DigitalIn P1(PTC17);                                                 //Pulsador 1
DigitalIn P2(PTA16);                                                 //Pulsador 2
DigitalIn P3(PTA17);                                                 //Pulsador 3
/********************************************/
#pragma endregion ZonaDefinicionDeEstados

#pragma region ZonaDefinicionDeEstados
/****** DEFINICIONES DE ESTADOS ******/
enum
{
    INICIO,
    POTENCIA_ACTIVA,
    CORRIENTE,
    TENSION,
    COSENO_FI = 1,
    POTENCIA_APARENTE,
    POTENCIA_REACTIVA,
    ACUMULATIVO
};
enum
{
    PROC_POST_OPEN,
    PROC_POST_SEND,
    PROC_POST_RECV,
    PROC_POST_CLOSE,
};
/*************************************/
#pragma endregion ZonaDefinicionDeEstados

#pragma region ZonaDefinicionDeVariables
/****** DEFINICIONES DE VARIABLES ******/
#define MAX_DATA_TO_READ_DEF 512
unsigned char buffer_data_http[MAX_DATA_TO_READ_DEF];
short DISPLAY_ABAJO = 0, DISPLAY_ARRIBA = 0;
char CompruebaP1 = 0, CompruebaP2 = 0, CompruebaP3 = 0, muestraArriba = 1, muestraAbajo = 1, SonarAlarma = 0;
short n = 0, m = 0, a = 0, b = 0, caseVoltage = 0, caseCurrent = 0, voltMin = 0, voltMax = 0, currMin = 0, currMax = 0, suma1 = 0, suma2 = 0, suma3 = 0, suma4 = 0;
unsigned int VoltageValue, CurrentValue, SampleV1, SampleV2, SampleV3, SampleC1, SampleC2, SampleC3, MostrarVoltaje, Alarma;
float Acumulativo;
float picoMinimoVoltaje = 0, picoMaximoVoltaje = 0, picoMinimoCorriente = 0, picoMaximoCorriente = 0, CosenoFi = 0, SenoFi = 0, ftiempo, CorrienteReal, voltajeReal, PotenciaAparente, PotenciaActiva, PotenciaReactiva, ACosenoFi;
/***************************************/
#pragma endregion ZonaDefinicionDeVariables

#pragma region ZonaPrototipos
/****** PROTOTIPOS DEL CODIGO ******/
void proc_post(void);
void proc_post_tic_cb();
void programa();
void ReadCurrent();
void ReadVoltage();
void system_ticker_cb();
void protocol_ticker_cb();
/***********************************/
#pragma endregion ZonaPrototipos

#pragma region ZonaTimer
/****** TIMER DEL SISTEMA ******/
#define SYSTEM_TICKER_TIC_DEF (0.01) //10mseg
#define SYSTEM_TICKER_1_SEG (1 / SYSTEM_TICKER_TIC_DEF)
Ticker system_ticker;
Ticker protocol_ticker;
unsigned short proc_post_tic = 0, t1 = 0, t2 = 0, t3 = 0;
unsigned char tiempo = 0, tiempo2 = 0;

void system_ticker_cb()
{
    //procesamos los tiempos de la maquina de estados para hacer un post
    proc_post_tic_cb();
    t1++;
    t2++;
    t3++;
    if (SonarAlarma == 1)
    {
        Alarma++;
    }
    if (Alarma >= 2000)
    {
        Alarma = 0;
        SonarAlarma = 0;
        Buzzer = 0;
        Rojo = 0;
    }
}

void proc_post_tic_cb()
{
    if (proc_post_tic)
        proc_post_tic--;
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
/*******************************/
#pragma endregion ZonaTimer

#pragma region ZonaMain
int main()
{
    DBG.printf("Puerto de Debug\r\n");

    (void)WiFi_init("TeleCentro-df39", "EDNJZ2NYYGN2");

    system_ticker.attach(&system_ticker_cb, SYSTEM_TICKER_TIC_DEF);

    protocol_ticker.attach(&protocol_ticker_cb, 0.0001);

    while (true)
    {

        (void)WiFi_Step();

        proc_post();
        programa();
        ReadCurrent();
        ReadVoltage();
    }
}
#pragma endregion ZonaMain

#pragma region ZonaProgramaPrincipal
void programa()
{
    if (suma1 == 1 && suma2 == 1 && suma3 == 1 && suma4 == 1)
    {
        muestraArriba = 1;
        muestraAbajo = 1;
        if ((tiempo <= 100) && (tiempo2 <= 100))
        {
            if (((tiempo2 - tiempo) < 10) || ((tiempo - tiempo2) < 10) && tiempo < 30)
            {
                ftiempo = tiempo;
                CosenoFi = cos((((ftiempo * 90) / 100) * 3.14) / 180);
                SenoFi = sin((((ftiempo * 90) / 100) * 3.14) / 180);
                if (CosenoFi > 0.7)
                {
                    ACosenoFi = CosenoFi;
                }
                else
                {
                    CosenoFi = ACosenoFi;
                }
            }
        }
        voltajeReal = ((110 * 220) / (picoMaximoVoltaje - picoMinimoVoltaje));\
        if (voltajeReal < 200 || voltajeReal > 260)
        {
            Rele = 0;
        }
        
        MostrarVoltaje = voltajeReal;
        picoMinimoVoltaje = 0;
        picoMaximoVoltaje = 0;
        CorrienteReal = (((picoMaximoCorriente - (picoMinimoCorriente + ((picoMaximoCorriente - picoMinimoCorriente) / 2))) / 10) / 1.4142);
        picoMinimoCorriente = 0;
        picoMaximoCorriente = 0;
        PotenciaAparente = voltajeReal * CorrienteReal;
        PotenciaActiva = PotenciaAparente * CosenoFi;
        PotenciaReactiva = PotenciaAparente * SenoFi;
        suma1 = 0;
        suma2 = 0;
        suma3 = 0;
        suma4 = 0;
        tiempo = 0;
        tiempo2 = 0;
        n = 0;
        m = 0;
        a = 0;
        b = 0;
    }
    if (P1 == 1 && t1 > 3 && CompruebaP1 != 1)
    {
        //transicion
        DISPLAY_ARRIBA++;
        if (DISPLAY_ARRIBA == (TENSION + 1))
        {
            DISPLAY_ARRIBA = POTENCIA_ACTIVA;
        }
        CompruebaP1 = 1;
        muestraArriba = 1;
    }
    else if (P1 == 0)
    {
        t1 = 0; //Solo hacemos t=0 cuando P1=0
        CompruebaP1 = 0;
    }
    if (P3 == 1 && t3 > 3 && CompruebaP3 != 1)
    {
        //transicion
        DISPLAY_ABAJO++;
        if (DISPLAY_ABAJO == (ACUMULATIVO + 1))
        {
            DISPLAY_ABAJO = COSENO_FI;
        }
        CompruebaP3 = 1;
    }
    else if (P3 == 0)
    {
        t3 = 0; //Solo hacemos t=0 cuando P2=0
        CompruebaP3 = 0;
        muestraAbajo = 1;
    }
    if (P2 == 1 && t2 > 3 && CompruebaP2 != 1)
    {
        //transicion
        SonarAlarma = 0;
        Alarma = 0;
        Buzzer = 0;
        Rojo = 0;
    }
    else if (P2 == 0)
    {
        t2 = 0; //Solo hacemos t=0 cuando P1=0
        CompruebaP2 = 0;
    }
    switch (DISPLAY_ARRIBA)
    {
    case INICIO:
        if (muestraArriba == 1)
        {
            LCD.locate(0, 0);
            LCD.printf("GRUPO 14");
            muestraArriba = 0;
        }
        break;
    case POTENCIA_ACTIVA:
        if (muestraArriba == 1)
        {
            LCD.locate(0, 0);
            LCD.printf("PA:%.0fW    ", PotenciaActiva);
            muestraArriba = 0;
        }
        break;
    case CORRIENTE:
        if (muestraArriba == 1)
        {
            LCD.locate(0, 0);
            LCD.printf("I: %.2fA    ", CorrienteReal);
            muestraArriba = 0;
        }
        break;
    case TENSION:
        if (muestraArriba == 1)
        {
            LCD.locate(0, 0);
            LCD.printf("V: %iAV    ", MostrarVoltaje);
            muestraArriba = 0;
        }
        break;
    }
    switch (DISPLAY_ABAJO)
    {
    case INICIO:
        if (muestraAbajo == 1)
        {
            LCD.locate(0, 1);
            LCD.printf("W ENERGY");
            muestraAbajo = 0;
        }
        break;
    case COSENO_FI:
        if (muestraAbajo == 1)
        {
            LCD.locate(0, 1);
            LCD.printf("C.F:%.2f    ", CosenoFi);
            muestraAbajo = 0;
        }
        break;
    case POTENCIA_APARENTE:
        if (muestraAbajo == 1)
        {
            LCD.locate(0, 1);
            LCD.printf("PP:%.0fW    ", PotenciaAparente);
            muestraAbajo = 0;
        }
        break;
    case POTENCIA_REACTIVA:
        if (muestraAbajo == 1)
        {
            LCD.locate(0, 1);
            LCD.printf("PR:%.0fW    ", PotenciaReactiva);
            muestraAbajo = 0;
        }
        break;
    case ACUMULATIVO:
        if (muestraAbajo == 1)
        {
            LCD.locate(0, 1);
            if (Acumulativo <= 10)
            {
                LCD.printf("C:%.2fKH    ", Acumulativo);
            }
            else if (Acumulativo > 10 && Acumulativo <= 100)
            {
                LCD.printf("C:%.1fKH    ", Acumulativo);
            }
            else if (Acumulativo > 100)
            {
                LCD.printf("C:%.0fKH    ", Acumulativo);
            }
            muestraAbajo = 0;
        }
        break;
    }
}
#pragma endregion ZonaProgramaPrincipal

#pragma region ZonaLeidaASensores
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
#pragma endregion ZonaLeidaASensores

#pragma region ZonaComunicacionESPConKL
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
    if (proc_post_step == PROC_POST_CLOSE || proc_post_step == PROC_POST_OPEN)
    {
        Verde = 0;
    }
    else
    {
        Verde = 1;
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
        sprintf(buff_send, "GET /php/Communication.php?voltage=%.2f&current=%.2f&power=%.2f HTTP/1.1\r\nHost: whitenergy.online\r\nConnection: Close\r\n\r\n", voltajeReal, CorrienteReal, PotenciaActiva);

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
        res = sock_recv(buffer_data_http, MAX_DATA_TO_READ_DEF, 500);
        DBG.printf("proc_posrt: -> [%d] PROC_POST_RECV:%d\r\n", __LINE__, res);
        if (res > 0)
        {
            if (strstr((char *)buffer_data_http, "200 OK"))
            {
                DBG.printf("proc_posrt: -> [%d] 200 OK\r\n", __LINE__);
            }
            for (int i = 0; i <= 500; i++)
            {
                if (buffer_data_http[i] == 'A' && buffer_data_http[i + 1] == 'L')
                {
                    if (buffer_data_http[i + 4] == '1')
                    {
                        SonarAlarma = 1;
                        Buzzer = 1;
                        Rojo = 1;
                    }
                }
                if (buffer_data_http[i] == 'K' && buffer_data_http[i + 1] == 'W')
                {
                    if (buffer_data_http[i + 6] == '}')
                    {
                        Acumulativo = ((buffer_data_http[i + 4] - 48) * 10) + (buffer_data_http[i + 5] - 48);
                        Acumulativo = Acumulativo / 1000;
                    }
                    if (buffer_data_http[i + 7] == '}')
                    {
                        Acumulativo = ((buffer_data_http[i + 4] - 48) * 100) + ((buffer_data_http[i + 5] - 48) * 10) + (buffer_data_http[i + 6] - 48);
                        Acumulativo = Acumulativo / 1000;
                    }
                    if (buffer_data_http[i + 8] == '}')
                    {
                        Acumulativo = ((buffer_data_http[i + 4] - 48) * 1000) + ((buffer_data_http[i + 5] - 48) * 100) + ((buffer_data_http[i + 6] - 48) * 10) + (buffer_data_http[i + 7] - 48);
                        Acumulativo = Acumulativo / 1000;
                    }
                    if (buffer_data_http[i + 9] == '}')
                    {
                        Acumulativo = (((buffer_data_http[i + 4] - 48) * 10) + (buffer_data_http[i + 5] - 48) + ((buffer_data_http[i + 6] - 48) * 0.1) + ((buffer_data_http[i + 7] - 48) * 0.01) + ((buffer_data_http[i + 8] - 48) * 0.001));
                    }
                    if (buffer_data_http[i + 10] == '}')
                    {
                        Acumulativo = (((buffer_data_http[i + 4] - 48) * 100) + ((buffer_data_http[i + 5] - 48) * 10) + (buffer_data_http[i + 6] - 48) + ((buffer_data_http[i + 7] - 48) * 0.1) + ((buffer_data_http[i + 8] - 48) * 0.01) + ((buffer_data_http[i + 9] - 48) * 0.001));
                    }
                }
            }
        }
        proc_post_tic = SYSTEM_TICKER_1_SEG;
        proc_post_step = PROC_POST_CLOSE;
        DBG.printf("proc_posrt: -> [%d] PROC_POST_CLOSE\r\n", __LINE__);
        break;

    default:
    case PROC_POST_CLOSE:
        //no pudimos abrir, intentamos en 10 segundos
        proc_post_tic = 60 * SYSTEM_TICKER_1_SEG;
        proc_post_step = PROC_POST_OPEN;
        //No es necesario cerrar, se lo estamos pidiendo al servidor
        //sock_close();
        break;
    }
}
#pragma endregion ZonaComunicacionESPConKL
