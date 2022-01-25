#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRtext.h>
#include <IRac.h>
#include <IRutils.h>

//ir receiver connected to D5
const int pin_rec = D5;
const int pin_send = D2;
const int baud_rate = 115200;
const int kCaptureBufferSize = 1024;
const int kTimeout = 15;
const int kMinUnknownSize = 12;
const int kTolerancePercentage = kTolerance;

//initialize irReceiver
IRrecv irrecv(pin_rec, kCaptureBufferSize, kTimeout, true);

//used to store the result from ir receiver
decode_results results;

void setup() {
  Serial.begin(baud_rate);
  irrecv.enableIRIn();
  if(!Serial)
    return;
  Serial.println();
  Serial.println("IR Recv Running...");
  Serial.println(pin_rec);
}

void loop() {
  // put your main code here, to run repeatedly:
  if(irrecv.decode(&results)){
    uint32_t cur_time = millis();
    Serial.printf(D_STR_TIMESTAMP " : %06u.%03u\n",cur_time / 1000, cur_time % 1000);
    
    if(results.overflow){
      Serial.printf(D_WARN_BUFFERFULL "\n", kCaptureBufferSize);  
    }

    if(kTolerancePercentage != kTolerance){
      Serial.printf(D_STR_TOLERANCE " : %d%%\n",kTolerancePercentage);
    }

    Serial.print(resultToHumanReadableBasic(&results));

    String description = IRAcUtils::resultAcToString(&results);
    if(description.length()){
      Serial.println(D_STR_MESGDESC ": " + description);
    }

    yield();

    Serial.println(resultToSourceCode(&results));
    Serial.println();
    yield();
  }
  delay(100);
}
