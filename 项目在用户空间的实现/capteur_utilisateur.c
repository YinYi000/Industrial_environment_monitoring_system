#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <wiringPiI2C.h>

#define ECHO_PIN 5
#define DHT_PIN 12

#define GPIO_BUZZER 16
#define GPIO_BUTTON 19
#define GPIO_LED 18

#define BUFFER_SIZE 128

static pthread_mutex_t mutex_dht;
static int fd;

void dht11_reset()
{
    pinMode(DHT_PIN, OUTPUT); // set mode to output
    delayMicroseconds(30);
    digitalWrite(DHT_PIN, HIGH); // output a low level
    delayMicroseconds(30);
    digitalWrite(DHT_PIN, LOW); // output a high level
    delay(20);
    digitalWrite(DHT_PIN, HIGH); // output a low level
    delayMicroseconds(30);
    pinMode(DHT_PIN, INPUT); // set mode to input
}

char DHT11_Read(char* temperature, char* humidity)
{
    unsigned char t, i, j;
    unsigned char buf[5] = { 0,0,0,0,0 };

    while (digitalRead(DHT_PIN) && t < 100)
    {
        t++;
        delayMicroseconds(1);
    };
    if (t >= 100)
        return 1;
    t = 0;

    while (!digitalRead(DHT_PIN) && t < 100)
    {
        t++;
        delayMicroseconds(1);
    };
    if (t >= 100)
        return 1;

    for (i = 0; i < 5; i++)
    {
        buf[i] = 0;
        for (j = 0; j < 8; j++)
        {
            buf[i] <<= 1;
            t = 0;
            while (digitalRead(DHT_PIN) && t < 100)
            {
                t++;
                delayMicroseconds(1);
            }
            if (t >= 100)
                return 1;
            t = 0;
            while (!digitalRead(DHT_PIN) && t < 100)
            {
                t++;
                delayMicroseconds(1);
            };
            if (t >= 100)
                return 1;
            delayMicroseconds(40);
            if (digitalRead(DHT_PIN))
                buf[i] += 1;
            //            printf("buf[%d] = %d\r\n",i ,buf[i]);
        }
    }

    if (buf[0] + buf[1] + buf[2] + buf[3] != buf[4])return 2;
    *humidity = buf[0];
    *temperature = buf[2];
    return 0;
}
/*

*/
float dis_read()
{
    float dis;
    struct timeval tv1;
    struct timeval tv2;
    long time1, time2;

    if (wiringPiSetupGpio() == -1) {
        printf("setip wiringPi failed");
        return 1;
    }

    pinMode(ECHO_PIN, OUTPUT);
    digitalWrite(ECHO_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(ECHO_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(ECHO_PIN, LOW);

    pinMode(ECHO_PIN, INPUT);
    while (!(digitalRead(ECHO_PIN) == 1));
    gettimeofday(&tv1, NULL); //获取当前时间
    while (!(digitalRead(ECHO_PIN) == 0));
    gettimeofday(&tv2, NULL); //获取当前时间
    time1 = tv1.tv_sec * 1000000 + tv1.tv_usec;
    time2 = tv2.tv_sec * 1000000 + tv2.tv_usec;

    dis = (float)(time2 - time1) / 1000000 * 34000 / 2; //求出距离

    printf("%0.2f cm\n\n", dis);
    delay(900);
    return dis;
}

void alarme()
{

        pinMode(GPIO_BUZZER, OUTPUT);
        pinMode(GPIO_LED, OUTPUT);
        pinMode(GPIO_BUTTON, INPUT);

        while (1) {
            static int value = 0;

            digitalWrite(GPIO_BUZZER, value);
            digitalWrite(GPIO_LED, value);
            value = 1 - value;
            delay(100);
            if (digitalRead(GPIO_BUTTON) == 0)
            {
                digitalWrite(GPIO_LED, 0);
                digitalWrite(GPIO_BUZZER, 0);
                break;
            }
        }

}
void LCD_initialisation()    /* Initialiser LCD */
{
    fd = wiringPiI2CSetup(0x3e);//Définir l'adresse de communication i2c
    wiringPiI2CWriteReg8(fd, 0x80, 0x3c);
    wiringPiI2CWriteReg8(fd, 0x80, 0x0c);
    wiringPiI2CWriteReg8(fd, 0x80, 0x01);
    wiringPiI2CWriteReg8(fd, 0x80, 0x06);
    delay(100);
}

void LCD_affichage(char temperature, char humidity)
{
    int i;
    char buffer[BUFFER_SIZE];
    char* str1 = "temperature:";
    char* str2 = "humidity:";
    char buft[40];
    char bufh[40];
    wiringPiI2CWriteReg8(fd, 0x80, 0x80);
    for (char* s1 = str1; *s1 != '\0'; ++s1)
    {
        wiringPiI2CWriteReg8(fd, 0x40, *s1);
    }

    sprintf(buft, "%d", temperature);
    for (char* st = buft; *st != '\0'; ++st)
    {
        wiringPiI2CWriteReg8(fd, 0x40, *st);
    }//Afficher la température 

    wiringPiI2CWriteReg8(fd, 0x80, 0xc0);//Affichage de la deuxième ligne de l'écran LCD

    for (char* s2 = str2; *s2 != '\0'; ++s2)
    {
        wiringPiI2CWriteReg8(fd, 0x40, *s2);
    }

    sprintf(bufh, "%d", humidity);
    for (char* st = bufh; *st != '\0'; ++st)
    {
        wiringPiI2CWriteReg8(fd, 0x40, *st);
    }//Afficher l'humidité

    //wiringPiI2CWriteReg8(fd, 0x80, 0x80); //Déplacer le curseur lCD à la position initiale
    delay(20);
}

void* DHT11Thread(void* arg)
{
    char temperature;
    char humidity;
    char value;

    printf("start Thread dht\n");
    while(1)
    { 
        dht11_reset();
        value = DHT11_Read(&temperature, &humidity);
        if (value == 0)
        {
            LCD_affichage(temperature, humidity);
            printf("temperature = %d\r\n", temperature);
            printf("humidity = %d\r\n", humidity);
        }
        else if (value == 2)
        {
            printf("Sensor dosent ans!\n");
        }
        else
        {
            printf("time out!\n");
        }
        delay(1000);
    }
    return NULL;
}

void* DistanceThread(void* arg)
{
    while(1)
    { 
        float dis = dis_read();
        if (dis < 50.0)
        {
            alarme();
        }
        delay(1000);
    }
    return NULL;
}

int main(void)
{
    
    wiringPiSetupGpio();
    LCD_initialisation();

    pthread_mutex_init(&mutex_dht, NULL);
//    pthread_mutex_init(&mutex2, NULL);

    pthread_t DHTThreadID, DistanceThreadId;
    if (pthread_create(&DHTThreadID, NULL, DHT11Thread, NULL) != 0) {
        fprintf(stderr, "Failed to create thread 1\n");
        return 1;
    }
    if (pthread_create(&DistanceThreadId, NULL, DistanceThread, NULL) != 0) {
        fprintf(stderr, "Failed to create thread 2\n");
        return 1;
    }
    if (pthread_join(DHTThreadID, NULL) != 0) {
        fprintf(stderr, "Failed to join thread 1\n");
        return 1;
    }
    if (pthread_join(DistanceThreadId, NULL) != 0) {
        fprintf(stderr, "Failed to join thread 2\n");
        return 1;
    }


    pthread_mutex_destroy(&mutex_dht);
    return 0;
}