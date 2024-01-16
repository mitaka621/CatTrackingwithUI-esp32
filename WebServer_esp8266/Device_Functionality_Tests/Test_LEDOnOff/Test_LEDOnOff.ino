#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

const char* ssid = "ditoge03";
const char* password = "mitko111";

IPAddress staticIP(192,168,0,9); // Set the desired static IP address
IPAddress gateway(192,168,0,1);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);

const int ledPin = 2; // D2 pin on ESP8266

void setup() {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  Serial.begin(115200);

  // Connect to Wi-Fi with static IP
  WiFi.begin(ssid, password);
  WiFi.config(staticIP, gateway, subnet);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html",
                "<html><body>"
                "<h1>ESP8266 Web Server</h1>"
                "<h2>Get http request demo:</h2>"
                "<p><a href=\"/on\"><button>ON</button></a></p>"
                "<p><a href=\"/off\"><button>OFF</button></a></p>"
                "<h2>Post http request demo:</h2>"
                "<form method='post' action='/message'>"
                "<input type='text' name='message'>"
                "<input type='submit' value='Submit'>"
                "</form>"
                "</body></html>");
  });

 server.on("/off", HTTP_GET, []() {
    digitalWrite(ledPin, HIGH);
    server.send(200, "text/plain", "LED is OFF");
  });

  server.on("/on", HTTP_GET, []() {
    digitalWrite(ledPin, LOW);
    server.send(200, "text/plain", "LED is ON");
  });

  server.on("/message", HTTP_POST, []() {
    if (server.hasArg("message")) {
      String message = server.arg("message");
      Serial.print("Received message: ");
      Serial.println(message);
      server.send(200, "text/plain", "Message received and displayed on Serial Monitor");
    } else {
      server.send(400, "text/plain", "Bad Request");
    }
  });

   
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}