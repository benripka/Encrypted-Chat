#include <Arduino.h>


int digPin = 13;  //used for defining sever or client
int anaPin = 13;  //used to collect random(ish) numbers from static

uint32_t otherPublicKey;
const uint32_t p = 2147483647;//generator
const uint32_t g = 16807;// prime number-both known by each party

//Start-up Serial and configure pins
void setup(){
	init();
	Serial.begin(9600);
	Serial3.begin(9600);//this must be included to allowed for transfer between Arduinos
	pinMode(anaPin, INPUT);
	pinMode(digPin, INPUT);
}

//computes a 32-bit private key for each Arduino

uint32_t getPrivateKey(){
	uint32_t privKey;
	int anaRead;//reads from analog
	int refNum = 1;//
	for (int i = 0; i < 32; i++) {
		anaRead = analogRead(anaPin);
		privKey = privKey << refNum;  //shifts privkey left by refNum
		privKey = privKey + (anaRead & refNum); // if anaRead is 0, then zero is added
		//to privKey, if anaRead is 1 then 1 is added to privKey
		delay(20);
	}
	return privKey;//privkey stores random number

}

uint32_t next_key(uint32_t current_key) {
	const uint32_t modulus = 0x7FFFFFFF; // 2^31-1
	const uint32_t consta = 48271;  // we use that consta<2^16
	uint32_t lo = consta*(current_key & 0xFFFF);
	uint32_t hi = consta*(current_key >> 16);
	lo += (hi & 0x7FFF)<<16;
	lo += hi>>15;
	if (lo > modulus) lo -= modulus;
	return lo;
}


// computes the value of (a*b) mod m, while avoiding overflow
uint32_t mulMod(uint32_t a, uint32_t b, uint32_t m) {

	uint32_t amm = a % m; //mod both inputs prior to multiplication
	uint32_t bmm = b % m;
	uint32_t tester = 1;  // tester will be used to check value of individual bits
	uint32_t result = 0;  // result will store (a*b)modm
	for(int i = 0; i < 32; i++) {
		if(amm & (tester << i)) { //excecutes if i'th bit of amm is one
		result += bmm;  //sums values on each iteration
		result %= m;    //mods the sum to because (a+b)modp = (amodp+bmodp)modp
	}
	bmm = (bmm << 1);   //bumbs bmm over by a bit
	bmm %= m;
}
return result;
}
// will compute base^power mod mod
uint32_t powMod(uint32_t base, uint32_t power, uint32_t mod) {
	uint32_t expontent = power;
	uint32_t result = 1 % mod;    //just for mathematical consistancy

	uint32_t newBase = base % mod;


	for(int i = 0; i < 32; i++) {   //cycle through 32 times
		if ((expontent >> i) & 1) { //checks value of each bit of exponent, starting from the right
			result = mulMod(result, newBase, mod); //multiply stored result by newBase and mod, if corresponding
		}                                           //bit of exponent is 1.

		newBase = mulMod(newBase, newBase, mod);  //square the previous newBase
	}
	result = result % mod; //mod the result
	return result;
}

// function to return true if there is a specified number of bytes in the serial buffer.
// and timeout if a specified timelimit is reached
bool wait_on_serial3( uint8_t nbytes, long timeout ) {
	unsigned long deadline = millis() + timeout;//wraparound not a problem
	while (Serial3.available()<nbytes && (timeout<0 || millis()<deadline))
	{
		delay(1); // give it some time
	}
	return Serial3.available()>=nbytes;
}

/** Writes an uint32_t to Serial3, starting from the least-significant
* and finishing with the most significant byte.
*/
void uint32_to_serial3(uint32_t num) {
	Serial3.write((char) (num >> 0));
	Serial3.write((char) (num >> 8));
	Serial3.write((char) (num >> 16));
	Serial3.write((char) (num >> 24));
}

/** Reads an uint32_t from Serial3, starting from the least-significant
* and finishing with the most significant byte.
*/
uint32_t uint32_from_serial3() {
	uint32_t num = 0;
	num = num | ((uint32_t) Serial3.read()) << 0;
	num = num | ((uint32_t) Serial3.read()) << 8;
	num = num | ((uint32_t) Serial3.read()) << 16;
	num = num | ((uint32_t) Serial3.read()) << 24;
	return num;
}




uint32_t client(uint32_t ckey){
	enum state{Start, WaitingForAck, SendingA};// enum defines state data type
	Serial.println("I am the Client");

	uint32_t skey;

	state currentState = Start;
	while(true){

		switch(currentState){

			case Start :
			Serial3.write('C');//C

			uint32_to_serial3(ckey);//sending ckey to sever

			currentState = WaitingForAck;

			case WaitingForAck ://waits for A key from server to continue to chat
			if(wait_on_serial3(1,1000) == true){//waiting for 1 byte
				if (Serial3.read() == 'A'){// if Acknowledgement is read from Serial
					currentState = SendingA;
				}
				else{
					Serial.println("Timeout ERR WFA");//wait_on_serial3 returns false
					currentState = Start;
				}
			}
			case SendingA :
			if(wait_on_serial3(4,1000) == true){
				skey = uint32_from_serial3();//reads skey from the server and stores

				Serial.println(skey);
				Serial3.write('A');// sending Acknowledgement back to server

				goto exit_loop;//goto functions
			}
			else if(wait_on_serial3(4,1000) == false){
				Serial.println("Timeout ERR SA");
				currentState = Start;
			}

		}

	}
	exit_loop:  ;
	return skey;
}


uint32_t server(uint32_t skey){
	enum state{Listen, WaitingForKey, WaitingForAck, ToChat};
	Serial.println("I am the Server");



	bool serialState;
	uint32_t ckey;

	state currentState = Listen;
	while(true){

		switch(currentState){

			case Listen :
			serialState = wait_on_serial3(1,1000);// waits until the buffer has 1 byte
			if(serialState == true){//meaning their is a byte in the serial
				if(Serial3.read() == 'C'){//waits for C key from client
					currentState = WaitingForKey;
				}
			}
			else if(serialState == false){
				Serial.println("TIMEOUT ERR L");
				currentState = Listen;
			}

			case WaitingForKey :
			serialState = wait_on_serial3(4,1000);
			if(serialState == true){
				ckey = uint32_from_serial3();

				Serial.print("This is the received ckey: ");
				Serial.println(ckey);

				Serial3.write('A');//sends Acknowledgement to client
				uint32_to_serial3(skey);
				currentState = WaitingForAck;
			}
			else if(serialState == false){
				Serial.println("TIMEOUT ERR WFK");
				currentState = Listen;
			}

			case WaitingForAck ://wait for Acknowledgement from client
			serialState = wait_on_serial3(1,1000);
			if(serialState == true){
				if(Serial3.read() == 'A'){
					currentState = ToChat;
				}
				else if(Serial3.read() == 'C'){// if C is read will return to
					//WaitingForKey state
					Serial.println("Recieved C in WFA");
					currentState = WaitingForKey;
				}
			}
			else if(serialState == false){
				Serial.println("TIMEOUT ERR WFK");
				currentState = Listen;
			}

			case ToChat :
			goto exit_loop;//exits loop


		}

	}
	exit_loop: ;
	return ckey;
}

void chat(uint32_t sharedSecretKey){
	Serial.println();

	Serial.println("Begin chat.");
	uint32_t receiverKey = sharedSecretKey;//receiverKey & senderkey orignally
	// need to be equal.
	uint32_t senderkey = sharedSecretKey;



	//while true loop is the chat uinterface that is always running
	//program bascially ends here
	while(true){
		// wait until some data has arrived
		// wait until the sender has sent some data
		if(Serial.available() != 0) {
			char byte = Serial.read();
			uint8_t encryptedByte = byte ^ (senderkey);//encrypting


			if( (uint8_t) byte == 13 ){
				Serial.println();
				Serial3.write(encryptedByte);// 13 aand 10 are ASCII values for
				// return and new line respectively. Sending both is the equivalent of
				// the receiver reading the enter key on a keyboard

				senderkey = next_key(senderkey);//changes key to prevent lagging encryption
				//since this if statement sends 2 bytes

				Serial3.write(10 ^ (senderkey) );

			}
			else{
				Serial3.write(encryptedByte);// sends encrypted data to other Arduino
				Serial.print(byte);//prints to senders terminal
			}

			senderkey = next_key(senderkey);//changes key using next_key function
		}

		//Reciever code
		if(Serial3.available() != 0) {
			uint8_t encryptByteRead = Serial3.read();
			char byteRead = encryptByteRead ^ (receiverKey);//decrypting

			Serial.write(byteRead);//simply prints the received data from the other Arduino

			receiverKey = next_key(receiverKey);//changes receiverKey everytime senderkey
			// on other arduino is changed

		}
	}
}

int main(){
	setup();

	if(digitalRead(digPin) == HIGH){//This is the Sever
		uint32_t myPrivateKey = getPrivateKey();//random fuction gentter
		uint32_t skey = powMod(g, myPrivateKey, p);//computes severs key
		uint32_t 	otherPublicKey = server(skey);// this will call the server
		//function that will exchange keys with the client

		uint32_t sharedSecretKey = powMod(otherPublicKey, myPrivateKey, p);
		chat(sharedSecretKey);
	}
	else{//This is the client
		uint32_t myPrivateKey = getPrivateKey();
		uint32_t ckey = powMod(g, myPrivateKey, p);
		uint32_t otherPublicKey = client(ckey);//this will call the client
		//function that will exchange keys with the server

		uint32_t sharedSecretKey = powMod(otherPublicKey, myPrivateKey, p);
		chat(sharedSecretKey);
	}

	Serial.flush();//not technically needed as chat will repeat indefinitly
	Serial3.flush();//not technically needed as chat will repeat indefinitly
}
