#include <ESP8266WiFi.h>

#define GPIO0 0
#define GPIO1 1
#define GPIO2 2
#define GPIO3 3

#define MAX_NUM_MSGS 50
#define MAX_LEN_BODY 1024
#define MAX_LEN_MSG  512
#define MAX_LEN_REQ  50


#define ssid ""
#define password ""

WiFiServer g_server(80);  // arg is the port to listen on

int g_num_messages = 0;
char g_messages[MAX_NUM_MSGS][MAX_LEN_MSG];  // pre alocated  to prevent fragmentation
char g_body[MAX_LEN_BODY];
char g_message[MAX_LEN_MSG];
char g_request[MAX_LEN_REQ];


void sendHeader(WiFiClient* p_client)
{
    p_client->print(F(
        "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
    ));
}

void readRequestLine(WiFiClient* p_client)
{
    int i = 0;
    char c;

    delay(1000);
    while (p_client->available() && i < MAX_LEN_REQ - 1)
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

    delay(1000);  // without delay it sometimes catches only the first 32 bytes
    while (p_client->available() && i < MAX_LEN_BODY - 1)
    {
        c = p_client->read();
        g_body[i++] = c;
        body_len++;
    }
    g_body[i] = '\0';

    Serial.print(F("Body size: "));
    Serial.println(body_len);

    Serial.print(F("Body: "));
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

    if (addr_start_msg)
        idx_start_msg = addr_start_msg - g_body;  // position where "message=" starts

    if (idx_start_msg != -1 && idx_start_msg + 8 < MAX_LEN_BODY)
    {
        it_body = idx_start_msg + 8;
        while (it_body < MAX_LEN_BODY - 1 &&  // body limit
               g_body[it_body] != '\0' &&     // proper message end and/or
               it_msg < MAX_LEN_MSG - 1)      // message limit
            g_message[it_msg++] = g_body[it_body++];
        g_message[it_msg] = '\0';  // ensures str terminator

        Serial.print(F("Message size: "));
        Serial.println(it_msg);
    }
    else
        g_message[0] = '\0';  // empty message

    Serial.print(F("Message: "));
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
    g_server.begin();

    // Print the IP address
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());
}

void loop()
{
    // it is OK for multiple small client.print/write,
    // because nagle algorithm will group them into one single packet

    // Check if a client has connected

    int i;

    WiFiClient client = g_server.accept();

    if (client)
    {
        client.setTimeout(1000);

        // Read the first line of the request
        readRequestLine(&client);

        if (strstr(g_request, "/form"))
        {
            sendHeader(&client);
            client.print(F(
                "<!DOCTYPE HTML><html>"
                "<head><title>form</title></head>"
                "<body style='font-family: Arial; text-align: center;'>"
                "<h2>Send a message:</h2>"

                // Form to send a text
                "<br><br><form action='/send/browser/text' method='POST'>"
                "<input type='text' name='message' placeholder='Text'>"
                "<input type='submit' value='Send'></form>"

                // Form to send a URL
                "<br><br><form action='/send/browser/url' method='POST'>"
                "<input type='text' name='message' placeholder='URL'>"
                "<input type='submit' value='Send'></form>"

                // Back button
                "<br><br><button onclick=\"window.location.href='/'\">Back</button>"
                "</body></html>"
            ));
        }
        else if (strstr(g_request, "/send/browser/text") ||
                 strstr(g_request, "/send/browser/url") ||
                 strstr(g_request, "/send/cli"))
        {
            // Read HTML body and write in the g_body array
            readBody(&client);

            // Extract the message from the body and write in the g_message array
            extractMsgFromBody();

            if (strstr(g_request, "/send/browser/text"))
            {
                // Replace '+' with ' '
                for (i = 0; g_message[i] != '\0'; i++)
                    if (g_message[i] == '+')
                        g_message[i] = ' ';
            }

            // Decode only messages sent from browser
            if (strstr(g_request, "/send/browser"))
            {
                decode();
                Serial.print(F("Decoded Message: "));
                Serial.println(g_message);
                Serial.println();
            }

            // Save text in memory
            if (g_num_messages < MAX_NUM_MSGS)
            {
                strcpy(g_messages[g_num_messages], g_message);
                g_num_messages++;
            }

            sendHeader(&client);

            // Dialog
            if (strstr(g_request, "/send/browser"))
            {
                client.print(F(
                    "<!DOCTYPE HTML><html>"
                    "<head><title>Sent</title></head>"
                    "<script>"
                    "alert('Message sent!');"
                    "window.location.href='/form';"
                    "</script>"
                    "</html>"
                ));
            }
        }
        else if (strstr(g_request, "/look"))
        {
            // Show messages sent

            sendHeader(&client);
            client.print(F(
                "<!DOCTYPE HTML><html>"
                "<head><title>look</title></head>"
                "<body style='font-family: Arial; text-align: center;'>"
                "<h2>List of messages:</h2>"
            ));

            for (i=0; i < g_num_messages; i++)
            {
                client.print(F("<br><pre>"));
                client.print(g_messages[i]);
                client.print(F("</pre>"));
            }

            // Back button
            client.print(F(
                "<br><br><button onclick=\"window.location.href='/'\">Back</button>"
                "</body></html>"
            ));
        }
        else if (strstr(g_request, "/clear"))
        {
            // Clear messages

            g_num_messages = 0;

            sendHeader(&client);
            client.print(F(
                "<!DOCTYPE HTML><html>"
                "<head><title>Cleared</title></head>"
                "<script>"
                "alert('Messages cleared!');"
                "window.location.href='/';"
                "</script>"
                "</html>"
            ));
        }
        else // Home
        {
            sendHeader(&client);
            client.print(F(
                "<!DOCTYPE HTML><html>"
                "<head><title>Home</title></head>"
                "<body style='font-family: Arial; text-align: center;'>"
                "<h1>Message server</h1>"

            // 1. Form to send a message
                "<br><br><form action='/form' method='GET'>"
                "<button type='submit'>Send</button></form>"

            // 2. Look messages sent
                "<br><form action='look' method='GET'>"
                "<button type='submit'>Look</button></form>"

            // 3. Clear messages
                "<br><form action='/clear' method='GET'>"
                "<button type='submit'>Clear</button></form>"

            // 4. Hardware info
                "<br><br>"
                "<dialog id='hd'><p>ESP8266-01</p><button onclick='hd.close()'>Close</button></dialog>"
                "<button onclick='hd.showModal()'>Hardware info</button>"
                "</body></html>"
            ));
        }

        while (client.available())
            client.read();
    }
}
