#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#define BUFFER_SIZE 128

int main()
{
    int err;
    int i;
    char buffer[BUFFER_SIZE];
    char* str1 = "temperature:";
    char* str2 = "humidity:";
    char buft[40];
    char bufh[40];
    int fd = wiringPiI2CSetup(0x3e);//Définir l'adresse de communication i2c
    delay(100);
    /* Initialiser LCD */
    wiringPiI2CWriteReg8(fd, 0x80, 0x3c);
    wiringPiI2CWriteReg8(fd, 0x80, 0x0c);
    wiringPiI2CWriteReg8(fd, 0x80, 0x01);
    wiringPiI2CWriteReg8(fd, 0x80, 0x06);
    int fd1 = open("/dev/rtdm/rtdm_driver_project_0", O_RDONLY);  //Lire les données du noyau
    if (fd1 < 0) {
        fprintf(stderr, " Read from rtdm_driver_project_0 failed with error: %s\n", strerror(-err));
        exit(EXIT_FAILURE);
    }

    while (1) {
        if((i = read(fd1, buffer, BUFFER_SIZE - 1)) > 0)
        { 
            buffer[i] = '\0';
            for (char* s1 = str1; *s1 != '\0'; ++s1)
            {
                wiringPiI2CWriteReg8(fd, 0x40, *s1);
            }

            sprintf(buft, "%d", buffer[0]);
            for (char* st = buft; *st != '\0'; ++st)
            {
                wiringPiI2CWriteReg8(fd, 0x40, *st);
            }//Afficher la température 

            wiringPiI2CWriteReg8(fd, 0x80, 0xc0);//Affichage de la deuxième ligne de l'écran LCD

            for (char* s2 = str2; *s2 != '\0'; ++s2)
            {
                wiringPiI2CWriteReg8(fd, 0x40, *s2);
            }

            sprintf(bufh, "%d", buffer[1]);
            for (char* st = bufh; *st != '\0'; ++st)
            {
                wiringPiI2CWriteReg8(fd, 0x40, *st);
            }//Afficher l'humidité

            wiringPiI2CWriteReg8(fd, 0x80, 0x80); //Déplacer le curseur lCD à la position initiale
        }
        delay(20);
    }

    close(fd);
    close(fd1);
    return EXIT_SUCCESS;
}
