#include "Protocol.h"
#include "Agent.h"
#include "Field.h"
#include "BOARD.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static char Encode[PROTOCOL_MAX_PAYLOAD_LEN];
static int wordCount;
static unsigned int intTemp1, intTemp2, intTemp3;
static unsigned char Check;

typedef enum {
    WAITING,
    RECORDING,
    FIRST_CHECKSUM_HALF,
    SECOND_CHECKSUM_HALF,
    NEWLINE
} ProtocolStates;

typedef struct {
    int index;
    uint8_t hash;
    char Sentence[PROTOCOL_MAX_PAYLOAD_LEN];
} protocolStruct;
protocolStruct pData;
ProtocolStates states = WAITING;

ProtocolParserStatus MSGID(char *input);
static uint8_t Checksum(char *inStr, int wordCount);
static uint8_t AsciiToHex(char input);
static int clear;

int CheckHex(char input);

/**
 * Encodes the coordinate data for a guess into the string `message`. This string must be big
 * enough to contain all of the necessary data. The format is specified in PAYLOAD_TEMPLATE_COO,
 * which is then wrapped within the message as defined by MESSAGE_TEMPLATE. The final length of this
 * message is then returned. There is no failure mode for this function as there is no checking
 * for NULL pointers.
 * @param message The character array used for storing the output. Must be long enough to store the
 *                entire string, see PROTOCOL_MAX_MESSAGE_LEN.
 * @param data The data struct that holds the data to be encoded into `message`.
 * @return The length of the string stored into `message`.
 */
int ProtocolEncodeCooMessage(char *message, const GuessData *data) {
    sprintf(Encode, PAYLOAD_TEMPLATE_COO, data->row, data->col);
    //create coo template with data
    wordCount = strlen(Encode);
    Check = Checksum(Encode, wordCount); //create checksum
    sprintf(message, MESSAGE_TEMPLATE, Encode, Check); //adds checksum to string
    wordCount = strlen(message);
    return wordCount; //returns back the string length
}

int ProtocolEncodeHitMessage(char *message, const GuessData *data) {
    sprintf(Encode, PAYLOAD_TEMPLATE_HIT, data->row, data->col, data->hit);
    //create hit template with data
    wordCount = strlen(Encode);
    Check = Checksum(Encode, wordCount); //create checksum
    sprintf(message, MESSAGE_TEMPLATE, Encode, Check); //adds checksum to string
    wordCount = strlen(message);
    return wordCount; //returns back the string length
}

int ProtocolEncodeChaMessage(char *message, const NegotiationData *data) {
    sprintf(Encode, PAYLOAD_TEMPLATE_CHA, data->encryptedGuess, data->hash);
    //create cha template with data
    wordCount = strlen(Encode);
    Check = Checksum(Encode, wordCount); //create checksum
    sprintf(message, MESSAGE_TEMPLATE, Encode, Check); //adds checksum to string
    wordCount = strlen(message);
    return wordCount; //returns back the string length
}

int ProtocolEncodeDetMessage(char *message, const NegotiationData *data) {
    sprintf(Encode, PAYLOAD_TEMPLATE_DET, data->guess, data->encryptionKey);
    //creates det template with data
    wordCount = strlen(Encode);
    Check = Checksum(Encode, wordCount); //create checksum
    sprintf(message, MESSAGE_TEMPLATE, Encode, Check); //adds checksum to string
    wordCount = strlen(message);
    return wordCount; //returns back the string length
}
/**
 * This function decodes a message into either the NegotiationData or GuessData structs depending
 * on what the type of message is. This function receives the message one byte at a time, where the
 * messages are in the format defined by MESSAGE_TEMPLATE, with payloads of the format defined by
 * the PAYLOAD_TEMPLATE_* macros. It returns the type of message that was decoded and also places
 * the decoded data into either the `nData` or `gData` structs depending on what the message held.
 * The onus is on the calling function to make sure the appropriate structs are available (blame the
 * lack of function overloading in C for this ugliness).
 *
 * PROTOCOL_PARSING_FAILURE is returned if there was an error of any kind (though this excludes
 * checking for NULL pointers), while
 * 
 * @param in The next character in the NMEA0183 message to be decoded.
 * @param nData A struct used for storing data if a message is decoded that stores NegotiationData.
 * @param gData A struct used for storing data if a message is decoded that stores GuessData.
 * @return A value from the UnpackageDataEnum enum.
 */

//typedef enum {
//    PROTOCOL_PARSING_FAILURE = -1, // Parsing failed for some reason. Could signify and unknown
//                                   // message was received or the checksum was incorrect.
//    PROTOCOL_WAITING,              // Parsing is waiting for the starting '$' of a new message
//    PROTOCOL_PARSING_GOOD,         // A success value that indicates no message received yet.
//    
//    PROTOCOL_PARSED_COO_MESSAGE,   // Coordinate message. This is used for exchanging guesses.
//    PROTOCOL_PARSED_HIT_MESSAGE,   // Hit message. Indicates a response to a Coordinate message.
//    PROTOCOL_PARSED_CHA_MESSAGE,   // Challenge message. Used in the first step of negotiating the
//                                   // turn order.
//    PROTOCOL_PARSED_DET_MESSAGE    // Determine message. Used in the second and final step of
//                                   // negotiating the turn order.
//} ProtocolParserStatus;

ProtocolParserStatus ProtocolDecode(char in, NegotiationData *nData, GuessData *gData) {

    switch (states) {
        case (WAITING):
            if (in != '$') { //dont do anything without start char '$'
                states = WAITING;
                return PROTOCOL_WAITING;
            } else if (in == '$') {
                for (clear = 0; clear < pData.index; clear++) {
                    pData.Sentence[clear] = '\0';
                }
                pData.index = 0; //start index at 0
                states = RECORDING; //move to recording
                return PROTOCOL_PARSING_GOOD;
            }
            break;
        case (RECORDING):
            if (in != '*') { //keep in recording until hit char '*'
                pData.Sentence[pData.index] = in;
                //add each char to decode sentence
                pData.index += 1; //increments index
                states = RECORDING;
                return PROTOCOL_PARSING_GOOD;
            } else if (in == '*') {
                states = FIRST_CHECKSUM_HALF; //move onto checksum
                return PROTOCOL_PARSING_GOOD;
            }
            break;
        case (FIRST_CHECKSUM_HALF):
            if (CheckHex(in) == SUCCESS) { //check for valid hex character
                pData.hash = AsciiToHex(in) << 4; //shift to the top 4 bits
                states = SECOND_CHECKSUM_HALF; //move to second half of checksum
                return PROTOCOL_PARSING_GOOD;
            } else return PROTOCOL_PARSING_FAILURE;
            break;
        case (SECOND_CHECKSUM_HALF):
            if (states == SECOND_CHECKSUM_HALF) {
                pData.hash = pData.hash^AsciiToHex(in);
                //adds second half of checksum
                wordCount = strlen(pData.Sentence);
                if ((CheckHex(in) == SUCCESS) && (pData.hash ==
                        (Checksum(pData.Sentence, wordCount)))) {
                    //checks that checksum is valid hex and is correct
                    //                    pData.Sentence[pData.index+1] = '\0';
                    pData.hash = pData.hash & 0x00; //clears hash for reuse
                    states = NEWLINE;
                    return PROTOCOL_PARSING_GOOD;
                } else return PROTOCOL_PARSING_FAILURE;
            }
            break;
        case(NEWLINE):
            if (in == '\n') { //end of the string with newline
                states=WAITING;
                if (MSGID(pData.Sentence) == PROTOCOL_PARSING_FAILURE) {
                    //if parse fails if string from record isn't valid
                    states = WAITING;
                    return PROTOCOL_PARSING_FAILURE;
                } else if (MSGID(pData.Sentence) == PROTOCOL_PARSED_DET_MESSAGE) {
                    //if valid string (det,cha,coo,hit) create the decoded sentence
                    printf("DET\n");
                    sscanf(pData.Sentence, PAYLOAD_TEMPLATE_DET, &intTemp1, &intTemp2);
                    nData->guess = intTemp1;
                    nData->encryptionKey = intTemp2;
                    return PROTOCOL_PARSED_DET_MESSAGE;
                } else if (MSGID(pData.Sentence) == PROTOCOL_PARSED_CHA_MESSAGE) {
                    printf("CHA\n");
                    sscanf(pData.Sentence, PAYLOAD_TEMPLATE_CHA, &intTemp1, &intTemp2);
                    nData->encryptedGuess = intTemp1;
                    nData->hash = intTemp2;
                    return PROTOCOL_PARSED_CHA_MESSAGE;
                } else if (MSGID(pData.Sentence) == PROTOCOL_PARSED_COO_MESSAGE) {
                    printf("COO\n");
                    sscanf(pData.Sentence, PAYLOAD_TEMPLATE_COO, &intTemp1, &intTemp2);
                    gData->row = intTemp1;
                    gData->col = intTemp2;
                    return PROTOCOL_PARSED_COO_MESSAGE;
                } else if (MSGID(pData.Sentence) == PROTOCOL_PARSED_HIT_MESSAGE) {
                    printf("HIT\n");
                    sscanf(pData.Sentence, PAYLOAD_TEMPLATE_HIT, &intTemp1, &intTemp2, &intTemp3);
                    gData->row = intTemp1;
                    gData->col = intTemp2;
                    gData->hit = intTemp3;
                    return PROTOCOL_PARSED_HIT_MESSAGE;
                }
            } else { //fails if no newline char
                states = WAITING;
                return PROTOCOL_PARSING_FAILURE;
            }
            break;
    }
    return PROTOCOL_PARSING_FAILURE;
}

/**
 * This function generates all of the data necessary for the negotiation process used to determine
 * the player that goes first. It relies on the pseudo-random functionality built into the standard
 * library. The output is stored in the passed NegotiationData struct. The negotiation data is
 * generated by creating two random 16-bit numbers, one for the actual guess and another for an
 * encryptionKey used for encrypting the data. The 'encryptedGuess' is generated with an
 * XOR(guess, encryptionKey). The hash is simply an 8-bit value that is the XOR() of all of the
 * bytes making up both the guess and the encryptionKey. There is no checking for NULL pointers
 * within this function.
 * @param data The struct used for both input and output of negotiation data.
 */
void ProtocolGenerateNegotiationData(NegotiationData *data) {
    //creates encryption for data
    data->encryptionKey = rand() &0xFFFF;
    data->guess = rand() &0xFFFF;
    data->encryptedGuess = data->encryptionKey^data->guess;
    data-> hash = ((data->encryptionKey & 0xFF) ^ (data->guess & 0xFF)
            ^ (data->encryptionKey >> 8) ^ (data->guess >> 8));
}

/**
 * Validates that the negotiation data within 'data' is correct according to the algorithm given in
 * GenerateNegotitateData(). Used for verifying another agent's supplied negotiation data. There is
 * no checking for NULL pointers within this function. Returns TRUE if the NegotiationData struct
 * is valid or FALSE on failure.
 * @param data A filled NegotiationData struct that will be validated.
 * @return TRUE if the NegotiationData struct is consistent and FALSE otherwise.
 */
uint8_t ProtocolValidateNegotiationData(const NegotiationData *data) {
    //confirms encryption for data is correct
    if (data->guess == (data->encryptionKey ^ data->encryptedGuess)) {
        if (data-> hash == ((data->encryptionKey & 0xFF) ^ (data->guess & 0xFF)
                ^ (data->encryptionKey >> 8) ^ (data->guess >> 8))) {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * This function returns a TurnOrder enum type representing which agent has won precedence for going
 * first. The value returned relates to the agent whose data is in the 'myData' variable. The turn
 * ordering algorithm relies on the XOR() of the 'encryptionKey' used by both agents. The least-
 * significant bit of XOR(myData.encryptionKey, oppData.encryptionKey) is checked so that if it's a
 * 1 the player with the largest 'guess' goes first otherwise if it's a 0, the agent with the
 * smallest 'guess' goes first. The return value of TURN_ORDER_START indicates that 'myData' won,
 * TURN_ORDER_DEFER indicates that 'oppData' won, otherwise a tie is indicated with TURN_ORDER_TIE.
 * There is no checking for NULL pointers within this function.
 * @param myData The negotiation data representing the current agent.
 * @param oppData The negotiation data representing the opposing agent.
 * @return A value from the TurnOrdering enum representing which agent should go first.
 */
TurnOrder ProtocolGetTurnOrder(const NegotiationData *myData, const NegotiationData *oppData) {
    //chooses turn order based off the encryption keys
    uint8_t turn = (myData->encryptionKey ^ oppData->encryptionKey) & 0x0001;
    if (turn == 0) {
        if (myData->encryptionKey < oppData->encryptionKey) {
            return TURN_ORDER_START;
        } else {
            return TURN_ORDER_DEFER;
        }
    } else if (turn) {
        if (myData->encryptionKey > oppData->encryptionKey) {
            return TURN_ORDER_START;
        } else {
            return TURN_ORDER_DEFER;
        }
    } else return TURN_ORDER_TIE;
}

ProtocolParserStatus MSGID(char *input) {
    //this function checks what the recorded string is
    int count1 = 0;
    int count2 = 0;
    int count3 = 0;
    int count4 = 0;
    char check1 = 'E';
    char check2 = 'H';
    char check3 = 'O';
    char check4 = 'I';
    if (check1 == input[1]) {
        //checks the second char of the recorded sentence
        count1++;
    } else if (check2 == input[1]) {
        count2++;
    } else if (check3 == input[1]) {
        count3++;
    } else if (check4 == input[1]) {
        count4++;
    }
    if (count1 == 1) {
        //if matches pass respective parsed message
        return PROTOCOL_PARSED_DET_MESSAGE;
    } else if (count2 == 1) {
        return PROTOCOL_PARSED_CHA_MESSAGE;
    } else if (count3 == 1) {
        return PROTOCOL_PARSED_COO_MESSAGE;
    } else if (count4 == 1) {
        return PROTOCOL_PARSED_HIT_MESSAGE;
    } else return PROTOCOL_PARSING_FAILURE;
}

static uint8_t AsciiToHex(char input) {
    //converts char to hex value
    if (CheckHex(input) == SUCCESS) {
        if (input == '0' || input == '1' || input == '2' || input == '3' ||
                input == '4' || input == '5' || input == '6' || input == '7' ||
                input == '8' || input == '9') {
            return input - 48;
        } else if (input == 'A' || input == 'B' || input == 'C' || input == 'D'
                || input == 'E' || input == 'F') {
            return input - 87;
        } else if (input == 'a' || input == 'b' || input == 'c' || input == 'd'
                || input == 'e' || input == 'f') {
            return input - 87;
        }
    }
    return STANDARD_ERROR;
}

static uint8_t Checksum(char *inStr, int wordCount) {
    //xor's every recorded char to create checksum
    uint8_t Check = 0;
    int Index = 0;
    while (Index < wordCount) {
        Check ^= inStr[Index];
        Index++;
    }
    return Check;
}

int CheckHex(char input) {
    //checks that input is a valid hex character
    if (input == '0' || input == '1' || input == '2' || input == '3' ||
            input == '4' || input == '5' || input == '6' || input == '7' ||
            input == '8' || input == '9' || input == 'A' || input == 'B' ||
            input == 'C' || input == 'D' || input == 'E' || input == 'F' ||
            input == 'a' || input == 'b' || input == 'c' || input == 'd' ||
            input == 'e' || input == 'f') {
        return SUCCESS;
    } else return STANDARD_ERROR;
}
