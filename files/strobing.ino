    /////////////////////////////////////////////////////////////////////
    ///////////////////Strobing software/////////////////////////////////
    /////////////////for teensy 3.1 or 3.2/////////////////////////////// 
    //and a OSC (open sound control) based frontend in pure data (PD)///
    ////////////////////////////////////////////////////////////////////

    /*
    This version is 'semi-flexible' - you can expose a camera-frame with LED-pulses with a minimum duration of 1 microsecond up to (maybe) ~8-9 microseconds.
    --> with >5 microseconds we get overexposure in our microscopy setup, but if you want to expose for longer then simply change the intervall for the interrupt, 
    the way it is programmed (with a simple delay), or use no interrupt at all because you might not need the precision...
      
    The duration of the pulse and its frequency is defined via the PD (pure data) frontend, which allows you to adjust the settings quickly. 
      
    Electronics:
    - circuit is based on: 
    "Performance evaluation of an overdriven LED for high-speed schlieren imaging" 
    S. Wilson, G. Gustafson, D. Lincoln, K. Murari, C. Johansen; J Vis (2015) 18:35–45 [DOI 10.1007/s12650-014-0220-7]
    
    -teensy- pin2 does the strobing

    How it works:
    The delaytime (delayUS) in microseconds ('inValue') and length (in microseconds 'length') of the pulse from an arbitray (not camera-synchronized) point in time,
    is transferred from the PD-frontend to the microcontroller via the OSC (Open Sound Control) protocol. 
    If the microcontroller received a set of values it will send them back to PD, where it is used to 
    confirm that the communication works.

    The delaytime defines when a pulse has to be performed and is transferred from the PD-frontend. You relate this to the framerate of your camera and achieve one or multiple pulses per frame.
    One pulse/frame should give you a crisp image of fast-moving objects, multiple exposures should give you an overlay of the movement of this object in time - within a single image.
    (until you saturate the image and have it overexposed of course). 
    The strenght of the exposure is defined by the length of the pulse as well as the light intensity delivered (controlled by changing the voltage on the circuit and the focusing/distance of your LED to your sample).
    
    If an exposure is due is determined by jumping into an interrupt every 10 microseconds. 
    If so, pin2 is pulled high, the system waits for the exposure time being over (therefore you have to change the interrupt interval if you want to expose >10us),
    then pulled low again.

    
    -This program is licensed under The MIT License (https://opensource.org/licenses/MIT), Copyright (c) 2016, Martin Fischlechner,
    with the exception of the code-blocks for initializing, receiving and sending OSC-messages, which were copied from https://github.com/CNMAT/OSC/tree/master/examples
    and have the following, similarly liberal, license: [the code-blocks are marked //&&; //&&] 
    
    The Center for New Music and Audio Technologies,
    University of California, Berkeley.  Copyright (c) 2008-14, The Regents of
    the University of California (Regents). 
    Permission to use, copy, modify, distribute, and distribute modified versions
    of this software and its documentation without fee and without a signed
    licensing agreement, is hereby granted, provided that the above copyright
    notice, this paragraph and the following two paragraphs appear in all copies,
    modifications, and distributions.

    IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
    SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING
    OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF REGENTS HAS
    BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED
    HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE
    MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS. 
    */
    
    //library for interrupt
    #include <TimerOne.h>
    ///libraries and code for OSC communication
    #include <OSCBundle.h>
    #include <OSCBoards.h>

    //&&
    #ifdef BOARD_HAS_USB_SERIAL
    #include <SLIPEncodedUSBSerial.h>
    SLIPEncodedUSBSerial SLIPSerial( thisBoardsSerialUSB );
    #else
    #include <SLIPEncodedSerial.h>
     SLIPEncodedSerial SLIPSerial(Serial);
    #endif
    //&& 
    
    ////// define variables////////
    
    ////////Volatile variables needed if in interrupt//////
    volatile unsigned long timer = 0; // timer counter for pulse
     
    
    volatile unsigned long counter = 0;//counter and counter1 help in interrupt to adjust for (us) delay times while flashing
    volatile unsigned long counter1 = 0;
    
    volatile unsigned long delayUS = 1000000; // delay timer, should expose 1/s on startup (when the real values have not been received yet)
    
    
    ///////SLIPserial/ OSC ///////
    int32_t inLength = 0;     // incoming OSC length of pulse (first received integer)
    int32_t inValue = 0;      // incoming OSC delaytime, the frequency of pulses (second received integer)
    int32_t onoff = 0;        // incoming enable strobing yes/no (third received integer)
    int32_t onoff1 = 0;        // incoming enable strobing yes/no (fourth received integer); (usb-buffer = 64 bytes, therefore 4 messages (or something that 64 can be divided with) work much better)
    
    //pins
    const int strobepin = 2;  // the pin that is used for controlling the LED
    
    void setup(void)
    {
      ///// setup the pins to input or output
      
      pinMode(strobepin, OUTPUT); // pin to drive to drive the LED
      
      //initialize timer
      Timer1.initialize(10);//interrupt will occur every 10 microseconds
      Timer1.attachInterrupt(strobe); // strobe will run every 10 microseconds

      //&&
      // initialize SLIPserial communications:
      //begin SLIPSerial just like Serial
      SLIPSerial.begin(115200);   // set this as high as you can reliably run on your platform
      #if ARDUINO >= 100
          while(!Serial)
            ;   // Leonardo bug
      #endif 
      //&&
    }
    
    /////////////////////////////////////////
    /////this is the interrupt///////////////
    ////which runs every 10 microseconds/////
    /////////////////////////////////////////
    
    void strobe(void)
    {
      
      //decide if it is time to put LED on  
       if(timer+counter1 >= delayUS) ///counter1 has the length of exposure from last cycle (every exposure 'costs' useconds)
       {
          if(onoff==1)
          {
          digitalWriteFast(strobepin, 1); //strobe-pin (pin2) on
          }
          delayMicroseconds(inLength);
          digitalWriteFast(strobepin, 0); //strobe-pin off
          timer=0; //sets time for the cycle to 0 when strobing occurred
          counter=counter+inLength; ///if strobepin has been triggered it adds duration of 'inLength'
        } 
         timer=timer+10; //this counts timer up in '10usecond steps' every time the interrupt runs and compares delayUS with it
        
         counter1 = 0; //reset counter1
         counter1 = counter; //if this cycle triggered the strobe, then next cycle the time to compare to delayUS will  be right
         counter = 0; //reset counter after every time interrupt runs, will be 0 anyway if last cycle did not trigger
    }
    
    
    /////////////////////////////////
    ///////main loop cycle///////////
    /////////////////////////////////
    void loop(void)
    {
        //////////////////////////////////////////////////////////////////////////
        // transfer OSC values with SLIPserial to and from PD and work with it ///
        //////////////////////////////////////////////////////////////////////////
          //&&
          //OSC slipserial incoming communication
           OSCMessage messageIN(""); //left empty because the code doesn't seem to discriminate on it
             int size;
             while(!SLIPSerial.endofPacket())
             if ((size =SLIPSerial.available()) > 0)
               {
                  while(size--)
                  messageIN.fill(SLIPSerial.read());
               }
               
               if(!messageIN.hasError())
                 { 

                   //get frequency (delay time) and length for strobe pulses///
                   ////////////////////////////////////////////////////////////
                      
                       messageIN.getInt(0); 
                       inLength = messageIN.getInt(0); //inLenght gets first integer of received message (length of exposure in usec)
                       messageIN.getInt(1); 
                       inValue = messageIN.getInt(1); //inValue gets second integer of received message (frequency interval in usec)
                       messageIN.getInt(2);
                       onoff = messageIN.getInt(2); //onoff gets third integer of received message (strobe=on if onoff==1) 
                       messageIN.getInt(3);
                       onoff1 = messageIN.getInt(3); //not used, just here to make the number of items dividable through 64 (usb-buffer), works better      
                  }
               
               messageIN.empty(); ///clears message from contents
               //&&
           
          delayMicroseconds(100);//necessary?
         
          
          //////(strobe)/////////// 
          delayUS = long(inValue);   //this is the delay variable in microseconds for strobing which the interrupt compares time against
          //&&         
          //declare the OSC bundle to send back to PD
             OSCBundle bndlOUT;  //name of bundle that is created
             //strobe  
             bndlOUT.add("/length").add(inLength); 
             bndlOUT.add("/interval").add(inValue); 
             bndlOUT.add("/onoff").add(onoff); 
             bndlOUT.add("/onoff1").add(onoff1); 
             
             SLIPSerial.beginPacket();
               bndlOUT.send(SLIPSerial); // send the bytes to the SLIP stream
             SLIPSerial.endPacket(); // mark the end of the OSC Packet
               bndlOUT.empty(); // empty the bundle to free room for a new one
          //&&
          
          delayMicroseconds(100); // necessary? 
          
    }
    
    ////////////////////////////
    /////end of main loop///////
    ////////////////////////////
