#include <ESP8266WiFi.h>

#define GPIO0 0
#define GPIO1 1
#define GPIO2 2
#define GPIO3 3

#define MAX_NUM_MSGS 50
#define MAX_BODY_LEN 1024
#define MAX_MSG_LEN 512
#define MAX_REQ_LEN 50


const char ssid[] = "";
const char password[] = "";

WiFiServer server(80); // Instance of the server. arg is the port to listen on

int num_messages = 0;
char g_messages[MAX_NUM_MSGS][MAX_MSG_LEN]; // pre alocated space for the messages so that there is no memory fragmentation
char g_body[MAX_BODY_LEN];
char g_message[MAX_MSG_LEN];
char g_request[MAX_REQ_LEN];


void readRequestLine(WiFiClient* p_client)
{
    int i = 0;
    char c;

    delay(1000);
    while (p_client->available() && i < MAX_REQ_LEN - 1)
    {
        c = p_client->read();
        
        // Stops in the first '\r'
        if (c == '\r')
            break;

        g_request[i++] = c;
    }
    g_request[i] = '\0';
}

void readBody(WiFiClient* p_client)
{
    int i = 0;
    int body_len = 0;
    char c;

    delay(1000); // without delay it sometimes catches only the first 32 bytes
    while (p_client->available() && i < MAX_BODY_LEN - 1)
    {
        c = p_client->read();
        g_body[i++] = c;
        body_len++;
    }
    g_body[i] = '\0';

    Serial.print(F("# Body size: "));
    Serial.println(body_len);

    Serial.print(F("# Body: "));
    Serial.println(g_body);
    Serial.println();
}

void extractMsgFromBody()
{
    int idx_start_msg = -1;
    const char* key = "message=";
    char* addr_start_msg = strstr(g_body, key);
    int it_body;
    int it_msg = 0;

    if (addr_start_msg != NULL)
        idx_start_msg = addr_start_msg - g_body; // position where "message=" starts

    if (idx_start_msg != -1 && idx_start_msg + 8 < MAX_BODY_LEN)
    {
        it_body = idx_start_msg + 8;
        while (it_body < MAX_BODY_LEN - 1 && // copies only untill the body limit and/or
                g_body[it_body] != '\0' &&    // copies only untill proper message end and/or
                it_msg < MAX_MSG_LEN - 1)    // copies only untill the message limit
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
    char high;
    char low;
    
    // While we're not at the end of the str (current character not NULL)
    while (*leader)
    {
        // Check to see if the current character is a %
        if (*leader == '%')
        {
            // Grab the next two characters and move leader forwards
            leader++;
            high = *leader;
            leader++;
            low = *leader;

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
    // Terminate the new str with a NULL character to trim it off
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
    while (WiFi.status() != WL_CONNECTED)
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

    int i;

    WiFiClient client = server.accept();

    if (client)
    {        
        client.setTimeout(1000);

        // Read the first line of the request
        readRequestLine(&client);

        if (strstr(g_request, "/form") != NULL)
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
        else if (strstr(g_request, "/send/browser/text") != NULL ||
                strstr(g_request, "/send/browser/url") != NULL ||
                strstr(g_request, "/send/cli") != NULL)
        {
            // Read HTML body and write in the g_body array
            readBody(&client);

            // Extract the message from the body and write in the g_message array
            extractMsgFromBody();

            if (strstr(g_request, "/send/browser/text") != NULL)
            {
                // Replace '+' with ' '
                for (i = 0; g_message[i] != '\0'; i++)
                    if (g_message[i] == '+')
                        g_message[i] = ' ';
            }

            // Decode only messages sent from browser
            if (strstr(g_request, "/send/browser") != NULL)
            {
                decode();
                Serial.print(F("# Decoded Message: "));
                Serial.println(g_message);
                Serial.println();
            }

            // Save text in memory
            if (num_messages < MAX_NUM_MSGS)
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
        else if (strstr(g_request, "/look") != NULL)
        {
            // Show messages sent
            
            client.print(F("HTTP/1.1 200 OK\r\ncountent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\n"));
            client.print(F("<head><title>look</title></head>\r\n"));
            client.print(F("<body style='font-family: Arial; text-align: center;'>\r\n"));
            client.print(F("<h2>List of messages:</h2>\r\n"));
            
            for (i=0; i < num_messages; i++)
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
        else if (strstr(g_request, "/clear") != NULL)
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
            client.print(F("<h1>Message server</h1>\r\n"));

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

        while (client.available())
            client.read();
    }
}
