/*
  Sample WTS Client for Arduino Uno boards.
*/

/*
 !!! IMPORTANT NOTE !!!
 You must get a valid Node ID from:
 http://wts.3open.org/getNodeID
*/
#define WTS_NODE_ID "PUT_YOUR_NODE_ID_HERE"

/*
  Assume these hardware configuration:
  Digital Output: D8
  Digital Input: D9
  Analog Input: A0
*/

// required libs
#include <SPI.h>
#include <Ethernet.h>
#include <utility/w5100.h>
#include <json_arduino.h>

/* change READLINE_MAX in Readline.h if input is longer; */
#include <Readline.h>
#include <string.h>

// WTS network parameters:
#define WTS_SERVER_NAME "wts.3open.org"
#define WTS_SERVER_PORT 9191
#define WTS_DEFAULT_KEEPALIVE_INTERVAL 120000  // 120 sec in ms
#define WTS_GET_TS_INTERVAL 86400000 // 1 day in ms: 86400000

// WTS Server side constants. Do not change:
#define WTS_ERRNO_OK 0
#define WTS_KEEPALIVE_PING "K"
#define WTS_KEEPALIVE_PONG "A"
#define WTS_MIN_DATA_COLLECTION_INTERVAL 300000  // minimum: 300 sec in ms

// user customizable node side error codes, must start from 201:
#define ERRNO_JSON 210
#define ERRNO_CMD_MISSING 211
#define ERRNO_ID_MISSING 212
#define ERRNO_PAYLOAD_MISSING 213
#define ERRNO_PAYLOAD_SYNTAX 214

// network stuff:
byte mac[] = { 0xDE, 0xAB, 0xBA, 0xEF, 0xFE, 0xCD };
EthernetClient client;
// protocol states:
enum State {
  CONNECT,
  AUTH,
  READY
};
State state;
ReadLine r;
char *line;
unsigned long ts = 0;  // timestamp from server
unsigned long last_ts_millis = 0;  // millis() when last get timestamp from server
static const unsigned long get_ts_interval = WTS_GET_TS_INTERVAL;  // how often to update ts from server
unsigned long next_get_ts_millis = 0;  // when should I get ts, in millis
unsigned long last_keepalive = 0;  //  millis() value of last keepalive rcvd
unsigned long keepalive_timeout = WTS_DEFAULT_KEEPALIVE_INTERVAL * 2;  // timeout if missed 2 keepalive packets
static const char server[] = WTS_SERVER_NAME;

// authentication request, use either A or B:
//
// A. use 'key' field in the request if you need another layer of security.
// The value is the base64 format of the sha256 hash of the key.
// Example: if key is 'NOKEY' the hash in base64 is '+9oOusmyJZbrB2KeODWGcn8uAtUx/ShUEfd+M0ruFlk'
// static const char auth [] = "{\"id\":1,\"c\":\"auth\",\"nid\":\"" WTS_NODE_ID "\",\"mdl\":\"UNO\",\"ver\":\"1.0\",\"key\":\"+9oOusmyJZbrB2KeODWGcn8uAtUx/ShUEfd+M0ruFlk\"}\r\n";
//
// B. skipping the 'key' field will set the key to the default of 'NOKEY'
static const char auth [] = "{\"id\":1,\"c\":\"auth\",\"nid\":\"" WTS_NODE_ID "\",\"mdl\":\"UNO\",\"ver\":\"1.0\"}\r\n";

// timestamp request:
static const char get_ts [] = "{\"id\":1,\"c\":\"ts\"}\r\n";

// JSON stuff:
char *json_command;
char *json_errno;
char *command;
char *payload;
uint8_t command_e = 1;
char *rid; // request ID, e.g. "+dwz7Nnhuqh+7P6uX5ibBg"
char *io_port; // pin to read/write
char *reply;  // reply string in json, eg. {"id":"+dwz7Nnhuqh+7P6uX5ibBg","e":"0"}
token_list_t *token_list = NULL;

// Hardware:
int d9_last_state;
unsigned long last_report_millis;

// Restarts program from beginning but does not reset the peripherals and registers
void software_reset() {
    delay(1000);
    asm volatile ("  jmp 0");
}

// check free mem
int freeRam () {
    extern int __heap_start, *__brkval;
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

// return the current unix timestamp in seconds precision
unsigned long now_ts () {
  return (ts + ((millis() - last_ts_millis) / 1000));  // note: auto rounding
}

void return_result(unsigned int e) {
        // reply command ok or not
        char *buf;
        buf = (char *)malloc(42);
        sprintf (buf, "{\"id\":\"%s\",\"e\":%d}\r\n", rid, e);
        client.print(buf);
        free(buf);
}

void setup() {

    Serial.begin(38400);
    Serial.println("Program start");

    // setup HW
    pinMode(8, OUTPUT);
    pinMode(9, INPUT_PULLUP);
    d9_last_state = HIGH;

    // start the Ethernet connection:
    state = CONNECT;

    if (Ethernet.begin(mac) == 0) {
        Serial.println("ERR:DHCP");
        // no point in carrying on, so restart try again:
        software_reset();
    }
    // give the Ethernet shield a second to initialize:
    delay(1000);
    W5100.setRetransmissionTime(4000); // 400ms
    Serial.println("Connecting...");

    // if you get a connection, report back via serial:
    while (state == CONNECT) {
      if (client.connect(server, WTS_SERVER_PORT)) {
          Serial.println("Connected");
          last_keepalive = millis();
          // send auth request
          client.print(auth);
          state = AUTH;

      } else {
          Serial.println("ERR:CONN");
          delay(5000); // retry in 5 seconds
      }
    }
}

void loop() {

    char *tx_buf;

    // check connection 1: by stack
    if (!client.connected()) {
        Serial.println("ERR:CONN");
        software_reset();
    }
    // check connection 2: by keepalive
    if ((millis() - last_keepalive) > keepalive_timeout) {
        Serial.println("ERR:KA");
        software_reset();
    }

    // get server timestamp on start and every get_ts_interval
    if ((state == READY) && (millis() > next_get_ts_millis)) {
      Serial.println("do get_ts");
      next_get_ts_millis = millis() + WTS_GET_TS_INTERVAL;
      client.print(get_ts);
    }

    if ((line = r.feed(&client)) != NULL) {
        Serial.print("RCVD:");
        Serial.println(line);
        // any incoming data is treated as keepalive
        last_keepalive = millis();

        // process keepalive
        if (strcmp(WTS_KEEPALIVE_PING,line) == 0) {
            client.print(WTS_KEEPALIVE_PONG "\r\n");
            Ethernet.maintain();
            Serial.print(now_ts());
            Serial.print(" M:");
            Serial.println(freeRam());

        } else {
            // process JSON frame
            token_list = create_token_list(10); // Create the Token List.
            if ((json_to_token_list(line, token_list) ) == 0) {; // Convert JSON String to a Hashmap of Key/Value
              Serial.println("ERR:JSON");
              release_token_list(token_list);
              return_result(ERRNO_JSON);
              return;
            }

            // check mandatory field: c
            if ((json_command = json_get_value(token_list, "c")) == NULL) {
              Serial.println("ERR:CMD");
              release_token_list(token_list);
              return_result(ERRNO_CMD_MISSING);
              return;
            }

            /* --- process responses --- */
            // if it has an "e" field, it is a reply
            if  ((json_errno = json_get_value(token_list, "e")) != NULL) {
              // process TS response
              if (strcmp("ts", json_command) == 0) {
                Serial.print("do set ts: ");
                ts = strtoul(json_get_value(token_list, "ts"), NULL, 10);
                Serial.println(ts);
                last_ts_millis = millis();
              }
              // process AUTH response
              else if (strcmp("auth", json_command) == 0) {
                if (atoi(json_errno) == 0) {
                  Serial.println("Auth OK");
                  state = READY;
                } else {
                  Serial.println("ERR:AUTH");
                  while (1) {
                    // dead loop
                  }
                }
               // process other responses if any,  with more 'else if'
              } else {
                // unexpected command
              }
              release_token_list(token_list);
              return;  // end of reply frame processing
            }

            /* --- process requests --- */
            // check system config request
            if (strcmp("config", json_command) == 0) {
              // set keepalive interval ( ki in seconds)
              if (json_get_value(token_list, "ki") != NULL) {
                Serial.print("do set ki:");
                keepalive_timeout = (strtoul(json_get_value(token_list, "ki"), NULL, 10)) * 1000 * 2;
                Serial.println(keepalive_timeout);
              }
              // set timestamp
              if (json_get_value(token_list, "ts") != NULL) {
                Serial.print("do set ts:");
                ts = strtoul(json_get_value(token_list, "ts"), NULL, 10);
                last_ts_millis = millis();
                Serial.println(ts);
                next_get_ts_millis = last_ts_millis + WTS_GET_TS_INTERVAL;
              }
              // check other config options if any ...
              // finally, done with this frame
              release_token_list(token_list);
              return;
            }

            /* --- from here on assume json_command is 'payload' --- */

            // check 'id'
            if ((rid = json_get_value(token_list, "id")) == NULL) {
              Serial.println("ERR:ID");
              release_token_list(token_list);
              return_result(ERRNO_ID_MISSING);
              return;
            }

            // check payload 'p'
            if ((payload = json_get_value(token_list, "p")) == NULL) {
              Serial.println("ERR:P");
              release_token_list(token_list);
              return_result(ERRNO_PAYLOAD_MISSING);  // no payload
              return;
            }
            release_token_list(token_list);

            /* parse incoming request in payload */
            Serial.print("P:");
            Serial.println(payload);
            Serial.print("M:");
            Serial.println(freeRam());

            // get command
            if ( (command = strtok(payload, " ")) == NULL) {
              Serial.println("ERR:P");
              //release_token_list(token_list);
              return_result(ERRNO_PAYLOAD_SYNTAX);  // no command in payload
              return;
            }

            if (strcmp("dw", command) == 0) {
              // dw = digital write, e.g. "dw 8 1"

              // get pin number and val
              int pin = atoi(strtok(NULL, " "));
              int pinVal = atoi(strtok(NULL, " "));
              // do it
              digitalWrite(pin, pinVal);
              return_result(WTS_ERRNO_OK);  // OK

            } else if (strcmp("rs", command) == 0) {
              // rs = read sensors values
              tx_buf = (char *)malloc(58);
              sprintf (tx_buf, "{\"id\":\"%s\",\"e\":%d,\"p\":\"D9,%d,A0,%d\"}\r\n", rid, WTS_ERRNO_OK, digitalRead(9), analogRead(0));
              Serial.print("RS:");
              Serial.print(tx_buf);
              client.print(tx_buf);
              free(tx_buf);
            }

            // INVALID command
            else {
              return_result(ERRNO_PAYLOAD_SYNTAX);  // invalid command in payload
            }
        }
    }

    /* --- Do HW check --- */
    // D9 trigger check
    int d9_state = digitalRead(9);
    if ( d9_state != d9_last_state) {
      d9_last_state = d9_state;
      if (d9_state == LOW) {
        Serial.println("D9 Triggered");
        // send notification
        tx_buf = (char *)malloc(40);
        sprintf (tx_buf, "{\"c\":\"noti\",\"p\":\"%ld D9 %d\"}\r\n", now_ts(), d9_state);
        client.print(tx_buf);
        free(tx_buf);
      }
    }

    // A0 data report, every WTS_MIN_DATA_COLLECTION_INTERVAL
    // too frequent report may be regarded as abuse
    if ((millis() - last_report_millis) > WTS_MIN_DATA_COLLECTION_INTERVAL) {
      last_report_millis = millis();
      int sensorVal = analogRead(0);
      Serial.print("Report A0:");
      Serial.println(sensorVal);
      tx_buf = (char *)malloc(50);
      sprintf (tx_buf, "{\"c\":\"data\",\"p\":\"%ld A0 %d\"}\r\n", now_ts(), sensorVal);
      client.print(tx_buf);
      free(tx_buf);
    }

    delay(1);  // for serial console
}

