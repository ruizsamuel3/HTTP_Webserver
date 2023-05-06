// HTTP Library (framework only)
//Samuel Ruiz for IoT
//Implementing web server
//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: -
// Target uC:       -
// System Clock:    -

// Hardware configuration:
// -

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "http.h"
#include "gpio.h"
#define GREEN_LED PORTF,3
//Keeps track of Iot Devices and give them a default value
bool ledBool = false;
bool motorOn = MOTOR_OFF;
char *temperature = "0";
char *distance = "0";
char *detected = "false";
char *barcode = "12345689";
uint8_t motorSpeed = MOTOR_SPEED_SLOW;
uint8_t motorDirection = MOTOR_DIRECTION_FORWARD;
bool updateMotorPower = false;
bool updateMotorSpeed = false;
bool updateMotorDirection = false;

//Gets HTTP request and reads url for requested web page
//On failure sends a not found 401
char* getHttpPage(etherHeader *ether)
{
    ipHeader *ip = (ipHeader*) ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader*) ((uint8_t*) ip + ipHeaderLength);
    char *token;
    char *pageName;
    char httpResponse[2000];
    const char *httpPage;

    //Parse through httpRequest
    token = strtok(tcp->data, " ");
    if (strcmp(token, "GET") == 0)
    {
        token = strtok(NULL, "/ ");
        char *fileName = token;
        if (strcmp(fileName, "test.html") == 0)
        {
            token = strtok(NULL, " ");
            if (strcmp(token, "motoron") == 0)
            {
                updateMotorPower = true;
                motorOn = MOTOR_ON;
                motorSpeed = MOTOR_SPEED_SLOW;
                motorDirection = MOTOR_DIRECTION_FORWARD;
            }
            else if (strcmp(token, "motoroff") == 0)
            {
                updateMotorPower = true;
                motorOn = MOTOR_OFF;
            }
            if (strcmp(token, "motorslow") == 0)
            {
                updateMotorSpeed = true;
                motorSpeed = MOTOR_SPEED_SLOW;
            }
            if (strcmp(token, "motormedium") == 0)
            {
                updateMotorSpeed = true;
                motorSpeed = MOTOR_SPEED_MEDIUM;
            }
            if (strcmp(token, "motorfast") == 0)
            {
                updateMotorSpeed = true;
                motorSpeed = MOTOR_SPEED_FAST;
            }
            if (strcmp(token, "motorforward") == 0)
            {
                updateMotorDirection = true;
                motorDirection = MOTOR_DIRECTION_FORWARD;
            }
            if (strcmp(token, "motorreverse") == 0)
            {
                updateMotorDirection = true;
                motorDirection = MOTOR_DIRECTION_REVERSE;
            }
            //sprintf(httpPage, dashboard);
            char *tempPage = buildWebpage();
            uint16_t tempPageSize = strlen(tempPage);
            sprintf(httpResponse,
                    "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s",
                    strlen(tempPage), tempPage);
        }
        else
        {
            httpPage =
                    ("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            sprintf(httpResponse, httpPage);

        }
    }

    return httpResponse;
}

//Nightmare of a function to build dynamic http page using global variables
char* buildWebpage()
{
    char webpage[2000];
    //------------------------------------------------------------------------
    //HTTP Header
    char *header = "<!doctype html>\n"
            "<html>"
            "<META HTTP-EQUIV=\"refresh\" content=\"1\">\n"
            "<head>"
            "<title>IoT Devices</title>\n";
    //------------------------------------------------------------------------
    //HTML Styling
    char *style =
            "<style>"
                    "html {text-align: center;}\n"
                    "body {text-align: center;}\n"
                    ".button {display: block;width: 20px;border-radius: 8px;padding: 12px 36px;border: none;color: white;font-size: 14px;margin: 0px auto 25px;}\n"
                    ".button-on {background-color:#5cb85c;text-align:center;}\n"
                    ".button-on:active {background-color:#5cb85c;}\n"
                    ".button-off {background-color:#d9534f;}\n"
                    ".button-off:active {background-color:#d9534f;}\n"
                    ".button-selected-on {background-color:#0275d8;}\n"
                    ".button-selected-on:active {background-color:#0275d8;}\n"
                    ".button-selected-off {background-color:#5bc0de;}\n"
                    ".button-selected-off:active {background-color:#5bc0de;}\n"
                    "</style>\n";
    //------------------------------------------------------------------------
    //HTML body
//    char *led = "";
//    if (ledBool)
//    {
//        led =
//                "<p>Green LED</p><a  class=\"button button-on\" href=\"/test.html/ledoff\">ON</a>\n";
//    }
//    else
//    {
//        led =
//                "<p>Green LED</p><a class=\"button button-off\" href=\"/test.html/ledon\">OFF</a>\n";
//    }
    //MOTOR
    char *motorPower = "";
    char *motorSpeedHtml = "";
    char *motorDirectionHtml = "";
    if (motorOn) //If motor is on
    {
        motorPower =
                "<p><h2>Motor</h2></p><a class=\"button button-on\" href=\"/test.html/motoroff\">ON</a>\n";
        switch (motorSpeed)
        {
        case MOTOR_SPEED_SLOW:
            motorSpeedHtml =
                    "<p><h2>Motor Speed</h2></p>"
                            "<a class=\"button button-selected-on\" href=\"/test.html/motormedium\">Slow</a>\n";
            break;
        case MOTOR_SPEED_MEDIUM:
            motorSpeedHtml =
                    "<p><h2>Motor Speed</h2></p>"
                            "<a class=\"button button-selected-on\" href=\"/test.html/motorfast\">Medium</a>\n";
            break;
        case MOTOR_SPEED_FAST:
            motorSpeedHtml =
                    "<p><h2>Motor Speed</h2></p>"
                            "<a class=\"button button-selected-on\" href=\"/test.html/motorslow\">Fast</a>\n";
            break;
        }

        if (motorDirection) //direction is forward else is reverse
        {
            motorDirectionHtml =
                    "<p><h2>Direction</h2></p><a  class=\"button button-selected-on\" href=\"/test.html/motorreverse\">Forward</a>\n";
        }
        else
        {
            motorDirectionHtml =
                    "<p><h2>Direction</h2></p><a class=\"button button-selected-on\" href=\"/test.html/motorforward\">Reverse</a>\n";
        }
    }
    else
    {
        motorPower =
                "<p><h2>Motor</h2></p><a class=\"button button-off\" href=\"/test.html/motoron\">OFF</a>\n";
    }

    char temperatureHtml[40];
    sprintf(temperatureHtml, "<p><h2>Temperature</h2></p><a \">%s</a>\n",
            temperature);
    char distanceHtml[40];
    sprintf(distanceHtml, "<p><h2>Item Count</h2></p><a \">%s</a>\n", distance);
    char distanceDetectHtml[40];
    sprintf(distanceDetectHtml, "<p><h2>Item Detected</h2></p><a\">%s</a>\n",
            detected);
    char barcodeHtml[40];
    sprintf(barcodeHtml, "<p><h2>Barcode</h2></p><a\">%s</a>\n", barcode);
    //------------------------------------------------------------------
    //Build Webpage
//    strcat(webpage, header); //Start header
//    strcat(webpage, style); //Styling Page
//    strcat(webpage, "</head>"); //Close Header
//    //Custom body content
//    strcat(webpage, "<body>"); //Start body
//    strcat(webpage, "<h1>IoT Devices</h1>");
//    strcat(webpage, led);
//    strcat(webpage, motorPower);
//    strcat(webpage, motorSpeedHtml);
//    strcat(webpage, motorDirectionHtml);
//    strcat(webpage, temperatureHtml);
//    strcat(webpage, distanceHtml);
//    strcat(webpage, distanceDetectHtml);
//    strcat(webpage, barcodeHtml);
//    strcat(webpage, "</body>" //Close body
//           "</html>");
    sprintf(webpage, "%s%s</head>"
            "<body>"
            "<h1>Iot Devices</h1>"
            "%s%s"
            "%s%s%s%s%s"
            "</body>"
            "</html>",
            header, style, motorPower, motorSpeedHtml, motorDirectionHtml,
            temperatureHtml, distanceHtml, distanceDetectHtml, barcodeHtml);
    //------------------------------------------------------------------
    return webpage;
}
void setTemperature(char *temp)
{

    temperature = temp;
}
void setBarcode(char *bar)
{
    barcode = bar;
}
void setDistance(char *dist)
{
    distance = dist;
}
void setDetect(char *det)
{
    detected = det;
}
bool isMotorPower()
{
    if(updateMotorPower){
        updateMotorPower = false;
        return true;
    }
    else{
        return false;
    }
}
bool isMotorSpeed()
{
    if(updateMotorSpeed){
        updateMotorSpeed = false;
        return true;
    }
    else{
        return false;
    }
}
bool isMotorDirection()
{
    if(updateMotorDirection){
        updateMotorDirection = false;
        return true;
    }
    else{
        return false;
    }
}
char* getMotorSpeed()
{
    char *str;
    switch (motorSpeed)
    {
    case MOTOR_SPEED_SLOW:
        str = "Slow";
        break;
    case MOTOR_SPEED_MEDIUM:
        str = "Medium";
        break;
    case MOTOR_SPEED_FAST:
        str = "Fast";
        break;
    }
    return str;
}
char* getMotorPower()
{
    char *str;
    switch (motorOn)
    {
    case MOTOR_ON:
        str = "Start";
        break;
    case MOTOR_OFF:
        str = "Stop";
        break;
    }
    return str;
}
char* getMotorDirection()
{
    char *str;
    switch (motorDirection)
    {
    case MOTOR_DIRECTION_FORWARD:
        str = "Forward";
        break;
    case MOTOR_DIRECTION_REVERSE:
        str = "Reverse";
        break;
    }
    return str;
}
