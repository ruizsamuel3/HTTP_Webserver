//HTTP Library
//Samuel Ruiz

#include <stdint.h>
#include <stdbool.h>
#include "ip.h"
#include "tcp.h"

#define MOTOR_SPEED_SLOW 0
#define MOTOR_SPEED_MEDIUM 1
#define MOTOR_SPEED_FAST 2
#define MOTOR_DIRECTION_FORWARD 1
#define MOTOR_DIRECTION_REVERSE 0
#define MOTOR_OFF 0
#define MOTOR_ON 1


char* getHttpPage(etherHeader *ether);
char* getHttpPageName(etherHeader *ether);
char* buildWebpage();
void setTemperature(char *temp);
void setBarcode(char *bar);
void setDistance(char *dist);
void setDetect(char *det);
bool isMotorPower();
bool isMotorSpeed();
bool isMotorDirection();
char* getMotorPower();
char* getMotorSpeed();
char* getMotorDirection();
