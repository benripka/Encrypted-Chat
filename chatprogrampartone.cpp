#include <Arduino.h>

int anaPin = 13;//varaible is used later for random number gernerator

void setup(){
	init();
	Serial.begin(9600);
	Serial3.begin(9600);//this must be included to allowed for transfer
	// between Arduinos
	pinMode(anaPin, INPUT);
}

//computes a 16-bit private key for each Arduino
uint32_t getPrivateKey(){0
	uint32_t privKey;
	int anaRead;//reads from analog
	int refNum = 1;//
	for (int i = 0; i < 31; i++) {
		anaRead = analogRead(anaPin);
		privKey = privKey << refNum;//shifts privkey left by refNum
		privKey = privKey + (anaRead & refNum);// if anaRead is 0, then zero is added
		//to privKey, if anaRead is 1 then 1 is added to privKey
		delay(20);
	}
	return privKey;//privkey stores random number


}

// this is the first part of the Diffie-Hellman procedure.
//computes public key using private key from each Arduino
uint16_t getPublicKey(uint16_t privateKey, int g, int p){

// result = g^a mod p
	privateKey = privateKey % p;
	uint16_t result = 1 % p; // even initialize to be mod m
	for (uint16_t i = 0; i < g; i++) {
		result = (result*privateKey) % p;
	}
	return result;

}

void stringRead(char str[], int len) {
	int index = 0;
	while (index < len - 1) {
		// if something is waiting to be read
		if (Serial.available() > 0) {
			char byte = Serial.read();
			if (byte == '\r') {// if user presses enter loop breaks
				break;
			}
			else {
				str[index] = byte;//adds to the string each time
				index += 1;// index = index + 1
			}
		}
	}
	str[index] = '\0';
}

uint16_t returnString() {

	char str[16];//empty array of char type  will hold 16 bits
	stringRead(str, 16);// calls readString function
	return atol(str);//converts from string data type to integer

}


//diffieHellman creates the public key & sharedSecretKey the parties use in the encrypted chat
uint16_t diffieHellman() {
	const uint16_t p = 19211;//generator
	const uint16_t g = 6;// prime number-both known by each party

	// step 1 of setup procedure
	uint16_t myPrivateKey = getPrivateKey();

	// step 2 of setup procedure
	uint16_t myPublicKey = getPublicKey(g, myPrivateKey, p);
	Serial.print("Your public key is: ");
	Serial.println(myPublicKey);
	Serial.print("Share this with your partner.");
	Serial.println();

	Serial.print("Enter the other public key: ");

	//TODO: make it so the key is finished entering when the enter key is pressed
	uint16_t otherPublicKey = returnString();

	uint16_t sharedSecretKey = getPublicKey(otherPublicKey, myPrivateKey, p);

	return sharedSecretKey;//last thing before chat comences
}




void chat(uint16_t sharedSecretKey){

	Serial.println("Key Recieved.");
	Serial.print("Encrypting with key: ");
	Serial.println(sharedSecretKey);
	Serial.println("Begin chat.");



	//while true loop is the chat uinterface that is always running
	//program bascially ends here
	while(true){
		// wait until some data has arrived
		// wait until the sender has sent some data
		if(Serial.available() != 0) {
			char byte = Serial.read();
			uint8_t encryptedByte = byte ^ (sharedSecretKey);//encrypting

			if( (uint8_t) byte == 13 ){
				Serial.println();
				Serial3.write(13 ^ (sharedSecretKey) );// 13 aand 10 are ASCII values for
				// return and new line respectively. Sending both is the equivalent of
				// the receiver reading the enter key on a keyboard
				Serial3.write(10 ^ (sharedSecretKey) );
			}
			else{
				Serial3.write(encryptedByte);// sends encrypted data to other Arduino
				Serial.print(byte);//prints to senders terminal
			}
		}



		//Reciever code
		if(Serial3.available() != 0) {
			uint8_t encryptByteRead = Serial3.read();
			char byteRead = encryptByteRead ^ (sharedSecretKey);//decrypting
			Serial.write(byteRead);//simply prints the received data from the other Arduino


		}

	}
}

//finally, main function calls setup, gets the private key and then calls the
//chat function
int main() {

	setup();

	uint16_t sharedSecretKey = diffieHellman();//this must come before chat since
	// the chat needs the secretkey


	if(digitalRead(digPin) == HIGH){
		server();
		chat(sharedSecretKey);
	}
	else{
		client();
		chat(sharedSecretKey);
	}

	Serial.flush();
	Serial3.flush();

	return 0;

}
