#include <ESP8266WiFi.h>
#include "ESP8266WebServer.h"

#define	WIFI_SSID	"XXXX"
#define	WIFI_PASS	"XXXX"
#define LED_PIN 4

ESP8266WebServer server(80);

void handle_root();
void handle_coffee();

void setup() {
	Serial.begin(9600);

	WiFi.begin(WIFI_SSID, WIFI_PASS);
	WiFi.mode(WIFI_STA);
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
	}

	server.on("/", handle_root);
	server.on("/espresso", handle_coffee);
	server.on("/black", handle_coffee);
	server.on("/magblack", handle_coffee);
	server.on("/cappuccino", handle_coffee);
	server.on("/cafelatte", handle_coffee);
	server.begin();

	pinMode(LED_PIN, OUTPUT);
	digitalWrite(LED_PIN, HIGH);
}

void loop() {
	server.handleClient();
}

void handle_root() {
	server.send(200, "text/html", "Is the order a rabbit?");
}

char drive_barista(char ch) {
	Serial.write(ch);
	Serial.flush();
	while (1) {
		if (Serial.available() > 0) break;
		delay(500);
	}
	return Serial.read();
}

void handle_coffee() {
	HTTPMethod method = server.method();
	String path = server.uri();

	if (method == HTTP_BREW || method == HTTP_POST) {
		char status = drive_barista('?');	
		if (status == 'X') {
			server.send(503, "text/html", "Barista is off, So I turned on it. Please request again later.");
			drive_barista('s');
		} else if (status == 'S') {
			server.send(503, "text/html", "Barista is starting up. Please request again later.");
		} else if (status == 'I') {
			server.send(200, "text/html", "OK! Bang! (Things are greatly damaged.)");
			if (path == "/espresso") {
				drive_barista('E');
			} else if (path == "/black") {
				drive_barista('B');
			} else if (path == "/magblack") {
				drive_barista('M');
			} else if (path == "/cappuccino") {
				drive_barista('C');
			} else if (path == "/cafelatte") {
				drive_barista('L');
			}
		} else if (status == 'B') {
			server.send(503, "text/html", "Barista is brewing a coffee. Please request again later.");
		} else if (status == 'E') {
			server.send(503, "text/html", "Barista is shutting down. Please request again later.");
		} else if (status == 'W') {
			server.send(503, "text/html", "I want to drink water.");
		} else if (status == 'C') {
			server.send(503, "text/html", "I want my cover closed.");
		} else if (status == 'D') {
			server.send(503, "text/html", "I want my drawer closed.");
		}
		return;
	}

	if (method == HTTP_PROPFIND) {
		if (path == "/espresso") {
			server.send(200, "text/html", "Rabbit House");
		} else if (path == "/black") {
			server.send(200, "text/html", "Rabbit Horse");
		} else if (path == "/magblack") {
			server.send(200, "text/html", "MAG = Maya Ayane-sakura meG");
		} else if (path == "/cappuccino") {
			server.send(200, "text/html", "Chino-chan!");
		} else if (path == "/cafelatte") {
			server.send(200, "text/html", "La Ta Ta");
		}
		return;
	}

	if (method == HTTP_WHEN) {
		server.send(501, "text/html", "JFW");
		return;
	}

	server.sendHeader("Location","/");
	server.send(303);
}
