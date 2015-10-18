//Don Browne
//10/17/2015
//core code to connect and communicate with a real axiom cycle trainer
//requires a mega adk or the circuits at home usb board.
//
#include <cdcftdi.h>
#include <usbhub.h>
#include "pgmstrings.h"
// Satisfy IDE, which only needs to see the include statment in the ino.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif

class FTDIAsync : public FTDIAsyncOper
{
  public:
    virtual uint8_t OnInit(FTDI *pftdi);
};

uint8_t FTDIAsync::OnInit(FTDI *pftdi)
{
  uint8_t rcode = 0;

  rcode = pftdi->SetBaudRate(9600);

  if (rcode)
  {
    ErrorMessage<uint8_t>(PSTR("SetBaudRate"), rcode);
    return rcode;
  }

  rcode = pftdi->SetData(FTDI_SIO_SET_DATA_PARITY_SPACE);

  if (rcode)
  {
    ErrorMessage<uint8_t>(PSTR("SetBaudRate"), rcode);
    return rcode;
  }

  rcode = pftdi->SetFlowControl(FTDI_SIO_DISABLE_FLOW_CTRL);

  if (rcode)
    ErrorMessage<uint8_t>(PSTR("SetFlowControl"), rcode);

  return rcode;
}

USB              Usb;
FTDIAsync        FtdiAsync;
FTDI             Ftdi(&Usb, &FtdiAsync);

bool debug=false;
bool port_fail = false;
int counter = 0;
uint8_t rcode;
//
//how often to send resistance value to the trainer
//do this too often and the trainer will set resistance to 0 and ignore you
//do this too little and the resistance will decrease to 0
static const int TRAINER_UPDATE_INTERVAL = 50;
//
static const int SLOPE = 6173;
static const int INTERCEPT = 0;

//test value, should come from somewhere
int gradeM = 0;
int gradeCount = 0;
void setup()
{
  Serial.begin( 115200 );
  while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
  Serial.println("\nStart");

  if (Usb.Init() == -1)
    Serial.println("OSC did not start.");

  delay( 200 );

  Serial.println("Starting Loop\n");
}

void loop()
{

  uint8_t ch[6];

  Usb.Task();

  if ( Usb.getUsbTaskState() == USB_STATE_RUNNING )
  {
    if (counter ==  TRAINER_UPDATE_INTERVAL) {
      counter = 0;
    }
    if (counter == 0) {
      if (! port_fail) {
        Serial.println("\nSend resistance to trainer");
        //low numbers = easy pedaling
        long slopeToSend = gradeM / 0.000162; //(SLOPE * gradeM) + INTERCEPT;
        //double slopeToSend = (SLOPE * gradeM) + INTERCEPT;
        if (slopeToSend < 0) {
          slopeToSend = 0;
        }

        //scaling down the value since resistance doesnt seem correct
        slopeToSend = slopeToSend/1.5;
        if (slopeToSend > 44811) {
          slopeToSend = 44811;
        }
        Serial.print(gradeM);
        Serial.print(":");
        Serial.print(slopeToSend);
        Serial.print(":");
        Serial.println(slopeToSend, HEX);
        //
        ch[0] = 0xF0;
        ch[1] = 0x01;
        ch[2] = (int)((slopeToSend >> 8) & 0XFF);
        ch[3] = (int)((slopeToSend & 0XFF));
        ch[4] = 0x00;
        ch[5] = 0xF7;

        rcode = Ftdi.SndData(6, (uint8_t*)ch);

        if (rcode) {
          ErrorMessage<uint8_t>(PSTR("SndData"), rcode);
          port_fail = true;
        }
        if (gradeCount == 5) {
          gradeCount = 0;
          gradeM = gradeM + 1;
        }
        if(gradeM>8){
          gradeM=0;
          }
        gradeCount++;
      }
    }
    counter++;
    delay(50);

    uint16_t rcvd = 12;
    uint8_t  buf[64];
    rcode = Ftdi.RcvData(&rcvd, buf);
    //
    //Packet structure
    //
    //01:64:F0:1:0:FF:FF:FF:FF:FF:FF:F7
    //5     = buton pressed
    //6,7   = rpm
    //8,9   = cadence
    //10,11 = hr

    //Buttons  byte 5
    //+     = 0x01
    //-     = 0x02
    //left  = 0x04
    //right = 0x08

    if (rcvd == 12 && buf[2] == 0xF0) {
      if (debug) {
        //print out the full packet
        for (int i = 0; i < rcvd; i++) {
          Serial.print(buf[i], HEX);
          Serial.print(":");
         }
        Serial.println("");
      }

      word speed = ( (buf[5] << 8)
                     + (buf[6] ) );

      word cadence = ( (buf[7] << 8)
                       + (buf[8] ) );

      word hr = ( (buf[9] << 8)
                  + (buf[10] ) );

      char button;

      switch (buf[4]) {
        case 0x01:
          button = '+';
          break;
        case 0x02:
          button = '-';
          break;
        case 0x04:
          button = 'L';
          break;
        case 0x08:
          button = 'R';
          break;
        default:
          button = 'N';
          break;
      }
      Serial.print("Button:");
      Serial.print(button);
      Serial.print(":");
      Serial.print(" MPH:");
      if(speed==65535){
        speed=0;
      }else{
        speed=((10.197 * (1 / ((double)speed / 1000))) + 2.0204);
      }
      Serial.print(speed);
      Serial.print(":");
      Serial.print(" CAD:");
       if(cadence==65535){
        cadence=0;
      }else{
        cadence=(-0.043 * (double)cadence + 115.44);
      }
      Serial.print(cadence);
      Serial.print(":");
      Serial.print(" HR:");
      if(hr==65535){
        hr=0;
      }else{
        hr=(-0.24 * (double)hr + 241.98);
      }
      Serial.print(hr);
      Serial.println("");

    }
    delay(10);
  }
}

