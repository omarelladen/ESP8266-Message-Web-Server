#include <ESP8266WiFi.h>

#define GPIO0 0
#define GPIO1 1
#define GPIO2 2
#define GPIO3 3

#define MAX_NUM_MESSAGES 50
#define MAX_BODY_LEN 1024
#define MAX_it_msg 512

const char ssid[] = "";
const char password[] = "";

WiFiServer server(80); // Instance of the server. arg is the port to listen on

int num_messages = 0;
char g_messages[MAX_NUM_MESSAGES][MAX_it_msg]; // pre alocated space for the messages so that there is no memory fragmentation
char g_body[MAX_BODY_LEN];
char g_message[MAX_it_msg];


void readBody(WiFiClient* p_client)
{
    int it_msg = 0;
    int body_len = 0;

    delay(1000); // without delay it sometimes catches only the first 32 bytes
    while(p_client->available() && it_msg < MAX_BODY_LEN - 1)
    {
        char c = p_client->read();
        g_body[it_msg++] = c;
        body_len++;
    }

    Serial.print(F("# Body size: "));
    Serial.println(body_len);
    g_body[it_msg] = '\0';

    Serial.print(F("# Body: "));
    Serial.println(g_body);
    Serial.println();
}

void extractMsgFromBody()
{
    int idx_start_msg = -1;
    const char* key = "message=";
    char* addr_start_msg = strstr(g_body, key);
    if (addr_start_msg != NULL)
        idx_start_msg = addr_start_msg - g_body; // position where "message=" starts
    if(idx_start_msg != -1 && idx_start_msg + 8 < MAX_BODY_LEN)
    {
        int it_body = idx_start_msg + 8;
        int it_msg = 0;
        while(it_body < MAX_BODY_LEN - 1 && // copies only untill the body limit and/or
                g_body[it_body] != '\0' &&    // copies only untill proper message end and/or
                it_msg < MAX_it_msg - 1)    // copies only untill the message limit
            g_message[it_msg++] = g_body[it_body++];
        g_message[it_msg] = '\0'; // ensures str terminator
        Serial.print(F("# Message size: "));
        Serial.println(it_msg);
    }
    else
        g_message[0] = '\0'; // empty message

    Serial.print(F("# Message: "));
    Serial.println(g_message);
}

void decode()
{
    char *leader = g_message;
    char *follower = leader;

    // While we're not at the end of the string (current character not NULL)
    while (*leader)
    {
        // Check to see if the current character is a %
        if (*leader == '%')
        {
            // Grab the next two characters and move leader forwards
            leader++;
            char high = *leader;
            leader++;
            char low = *leader;

            // Convert ASCII 0-9A-F to a value 0-15
            if (high > 0x39)
                high -= 7;
            high &= 0x0f;

            // Same again for the low byte:
            if (low > 0x39) low -= 7;
            low &= 0x0f;

            // Combine the two into a single byte and store in follower:
            *follower = (high << 4) | low;
        }
        else
            // All other characters copy verbatim
            *follower = *leader;

        // Move both pointers to the next character:
        leader++;
        follower++;
    }
    // Terminate the new string with a NULL character to trim it off
    *follower = 0;
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    // pinMode(GPIO0, OUTPUT);
    // pinMode(GPIO1, OUTPUT);
    // pinMode(GPIO2, OUTPUT);
    // pinMode(GPIO3, OUTPUT);

    // Connect to WiFi network
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while(WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(F("."));
    }
    Serial.println();

    // Start the server
    server.begin();

    // Print the IP address
    Serial.print(F("Serving at "));
    Serial.println(WiFi.localIP());
}

void loop()
{
    // it is OK for multiple small client.print/write,
    // because nagle algorithm will group them into one single packet

    // Check if a client has connected
    WiFiClient client = server.accept();
    if(client)
    {        
        client.setTimeout(1000);

        // Read the first line of the request
        String req = client.readStringUntil('\r');
        Serial.println(F("Request: "));
        Serial.println(req);
        Serial.println();

        if(req.indexOf(F("/form")) != -1)
        {
            client.print(F("HTTP/1.1 200 OK\r\ncountent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\n"));
            client.print(F("<head><title>form</title></head>\r\n"));
            client.print(F("<body style='font-family: Arial; text-align: center;'>\r\n"));
            client.print(F("<h2>Send a message:</h2>\r\n"));

            // Form to send a text
            client.print(F("<br><br><form action='/send/browser/text' method='POST'>"));
            client.print(F("<input type='text' name='message' placeholder='Text'>"));
            client.print(F("<input type='submit' value='Send'></form>"));

            // Form to send a URL
            client.print(F("<br><br><form action='/send/browser/url' method='POST'>"));
            client.print(F("<input type='text' name='message' placeholder='URL'>"));
            client.print(F("<input type='submit' value='Send'></form>"));

            // Back button
            client.print(F("<br><br><button onclick=\"window.location.href='/'\">Back</button>"));
            client.print(F("</body></html>"));
        }
        else if(req.indexOf("/send/browser/text") != -1 ||
                req.indexOf("/send/browser/url") != -1  ||
                req.indexOf("/send/cli") != -1)
        {
            // Read HTML body and write in the g_body array
            readBody(&client);

            // Extract the message from the body and write in the g_message array
            extractMsgFromBody();

            if(req.indexOf("/send/browser/text") != -1)
            {
                // Replace '+' with ' '
                for (int i = 0; g_message[i] != '\0'; i++)
                    if (g_message[i] == '+')
                        g_message[i] = ' ';
            }

            // Decode only messages sent from browser
            if(req.indexOf("/send/browser") != -1)
            {
                decode();
                Serial.print(F("# Decoded Message: "));
                Serial.println(g_message);
                Serial.println();
            }

            // Save text in memory
            if(num_messages < MAX_NUM_MESSAGES)
            {
                strcpy(g_messages[num_messages], g_message);
                num_messages++;
            }

            // Dialog
            client.print(F("HTTP/1.1 200 OK\r\ncountent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\n"));
            client.print(F("<head><title>sent</title></head>\r\n"));
            client.print(F("<body style='font-family: Arial; text-align: center;'>\r\n"));
            client.print(F("<br><br><dialog open id='md'><p>Message sent!</p>"));
            client.print(F("<button onclick=\"window.location.href='/'\">Close</button>"));
            client.print(F("</body></html>"));
        }
        else if(req.indexOf(F("/look")) != -1)
        {
            // Show messages sent
            
            client.print(F("HTTP/1.1 200 OK\r\ncountent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\n"));
            client.print(F("<head><title>look</title></head>\r\n"));
            client.print(F("<body style='font-family: Arial; text-align: center;'>\r\n"));
            client.print(F("<h2>List of messages:</h2>\r\n"));
            
            for(int i=0; i < num_messages; i++)
            {  
                client.print(F("<br>"));
                client.print(F("<pre>"));
                client.print(g_messages[i]);
                client.print(F("</pre>"));
            }

            // Back button
            client.print(F("<br><br><button onclick=\"window.location.href='/'\">Back</button>"));
            client.print(F("</body></html>"));
        }
        else if(req.indexOf(F("/clear")) != -1)
        {
            // Clear messages 

            num_messages = 0;

            client.print(F("HTTP/1.1 200 OK\r\ncountent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\n"));
            client.print(F("<head><title>clear</title></head>\r\n"));
            client.print(F("<body style='font-family: Arial; text-align: center;'>\r\n"));
            client.print(F("<br><br><dialog open id='mcd'><p>Messages cleared!</p>"));
            client.print(F("<button onclick=\"window.location.href='/'\">Close</button>")); // <button onclick='document.getElementById(\"mcd\").close()'>Close</button></dialog>"));
            client.print(F("</body></html>"));
        }
        else // Home
        {
            client.print(F("HTTP/1.1 200 OK\r\ncountent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\n"));
            client.print(F("<head><title>Home</title></head>\r\n"));
            client.print(F("<body style='font-family: Arial; text-align: center;'>\r\n"));
            client.print(F("<h1>Omar's message server</h1>\r\n"));

            // 1. Form to send a message
            client.print(F("<br><br><form action='http://"));
            client.print(WiFi.localIP());
            client.print(F("/form' method='GET'>"));
            client.print(F("<button type='submit'>Send</button></form>\r\n"));

            // 2. Look messages sent
            client.print(F("<br><form action='http://"));
            client.print(WiFi.localIP());
            client.print(F("/look' method='GET'>"));
            client.print(F("<button type='submit'>Look</button></form>\r\n"));

            // 3. Clear messages
            client.print(F("<br><form action='http://"));
            client.print(WiFi.localIP());
            client.print(F("/clear' method='GET'>"));
            client.print(F("<button type='submit'>Clear</button></form>\r\n"));

            // 4. Hardware info
            client.print(F("<br><br>"));
            client.print(F("<dialog id='hd'><p>ESP8266-01</p><button onclick='hd.close()'>Close</button></dialog><button onclick='hd.showModal()'>Hardware info</button>"));
            client.print(F("</body></html>"));
        }

        while(client.available())
            client.read();
    }
}
