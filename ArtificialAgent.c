#include "Agent.h"
#include "Field.h"
#include "Protocol.h"
#include "Oled.h"
#include "BOARD.h"
#include "xc.h"
#include "FieldOled.h"
#include "Uart1.h"

typedef struct {
    Field myField;
    Field yourField;
    NegotiationData myData;
    NegotiationData yourData;
    GuessData gData;
    NegotiationData nData;
} AgentStruct;
AgentStruct AgentData;

static GuessData guess;
static int i;
static int turnOrder = 0;
static AgentState state = AGENT_STATE_GENERATE_NEG_DATA;
static ProtocolParserStatus protocolStatus;

int RandomFunct(Field *field, BoatType boat);

/**
 * The Init() function for an Agent sets up everything necessary for an agent before the game
 * starts. This can include things like initialization of the field, placement of the boats,
 * etc. The agent can assume that stdlib's rand() function has been seeded properly in order to
 * use it safely within.
 */
void AgentInit(void)
{
    int temp1 = 0;
    int temp2 = 0;
    int temp3 = 0;
    int temp4 = 0;
    BoatType type;
    FieldInit(&AgentData.myField, FIELD_POSITION_EMPTY);
    FieldInit(&AgentData.yourField, FIELD_POSITION_UNKNOWN);
    //initializes my field and enemy's field 
    while (temp1 == 0) { //continues randomizing until adding each boat works
        type = FIELD_BOAT_SMALL;
        if (RandomFunct(&AgentData.myField, type) == SUCCESS) {
            temp1 = 1;
        }
    }
    while (temp2 == 0) {
        type = FIELD_BOAT_MEDIUM;
        if (RandomFunct(&AgentData.myField, type) == SUCCESS) {
            temp2 = 1;
        }
    }
    while (temp3 == 0) {
        type = FIELD_BOAT_LARGE;
        if (RandomFunct(&AgentData.myField, type) == SUCCESS) {
            temp3 = 1;
        }
    }
    while (temp4 == 0) {
        type = FIELD_BOAT_HUGE;
        if (RandomFunct(&AgentData.myField, type) == SUCCESS) {
            temp4 = 1;
        }
    }
}

/**
 * The Run() function for an Agent takes in a single character. It then waits until enough
 * data is read that it can decode it as a full sentence via the Protocol interface. This data
 * is processed with any output returned via 'outBuffer', which is guaranteed to be 255
 * characters in length to allow for any valid NMEA0183 messages. The return value should be
 * the number of characters stored into 'outBuffer': so a 0 is both a perfectly valid output and
 * means a successful run.
 * @param in The next character in the incoming message stream.
 * @param outBuffer A string that should be transmit to the other agent. NULL if there is no
 *                  data.
 * @return The length of the string pointed to by outBuffer (excludes \0 character).
 */
int AgentRun(char in, char *outBuffer)
{
    outBuffer[0] = '\0';
    if (in != '\0') { //check status when input isnt null
        protocolStatus = ProtocolDecode(in, &AgentData.nData, &AgentData.gData);
    }
    if (protocolStatus == PROTOCOL_PARSING_FAILURE) { 
        //when status fails print error
        OledClear(OLED_COLOR_BLACK);
        OledDrawString(AGENT_ERROR_STRING_PARSING);
        OledUpdate();
        state = AGENT_STATE_INVALID;
    }
    switch (state) {
    case AGENT_STATE_GENERATE_NEG_DATA: //creates negotiation data and sends it
        ProtocolGenerateNegotiationData(&AgentData.myData);
        ProtocolEncodeChaMessage(outBuffer, &AgentData.myData);
        //sends challenge message
        state = AGENT_STATE_SEND_CHALLENGE_DATA;
        break;
    case AGENT_STATE_SEND_CHALLENGE_DATA: //once recieve enemy's challenge
        if (protocolStatus == PROTOCOL_PARSED_CHA_MESSAGE) {
            //send determine message
            AgentData.yourData.encryptedGuess = AgentData.nData.encryptedGuess;
            AgentData.yourData.hash = AgentData.nData.hash;
            ProtocolEncodeDetMessage(outBuffer, &AgentData.myData);
            state = AGENT_STATE_DETERMINE_TURN_ORDER;
        }
        break;
    case AGENT_STATE_DETERMINE_TURN_ORDER: //determines turn order
        if (protocolStatus == PROTOCOL_PARSED_DET_MESSAGE) {
            AgentData.yourData.guess = AgentData.nData.guess;
            AgentData.yourData.encryptionKey = AgentData.nData.encryptionKey;
            if (ProtocolValidateNegotiationData(&AgentData.yourData) == FALSE) {
                OledClear(OLED_COLOR_BLACK);
                OledDrawString(AGENT_ERROR_STRING_NEG_DATA);
                OledUpdate();
                state = AGENT_STATE_INVALID;
            } else {
                if (turnOrder == TURN_ORDER_TIE) {
                    OledClear(OLED_COLOR_BLACK);
                    OledDrawString(AGENT_ERROR_STRING_ORDERING);
                    OledUpdate();
                    state = AGENT_STATE_INVALID;
                } else if (turnOrder == TURN_ORDER_START) {
                    //Won turn order update oled to my turn
                    FieldOledDrawScreen(&AgentData.myField, &AgentData.yourField, FIELD_OLED_TURN_MINE);
                    state = AGENT_STATE_SEND_GUESS;
                } else if (turnOrder == TURN_ORDER_DEFER) {
                    //Lost turn order update oled to your turn
                    FieldOledDrawScreen(&AgentData.myField, &AgentData.yourField, FIELD_OLED_TURN_THEIRS);
                    state = AGENT_STATE_WAIT_FOR_GUESS;
                }
            }
        }
        break;
    case AGENT_STATE_SEND_GUESS: //send random guess encoded with coo
        for (i = 0; i < BOARD_GetPBClock() / 8; i++); //delays coo msg
        guess.row = (rand() % (FIELD_ROWS));
        guess.col = (rand() % (FIELD_COLS));
        while (FieldAt(&AgentData.yourField, guess.row, guess.col) != FIELD_POSITION_UNKNOWN) {
            //guess until valid
            guess.row = (rand() % (FIELD_ROWS));
            guess.col = (rand() % (FIELD_COLS));
        }
        ProtocolEncodeCooMessage(outBuffer, &guess);
        state = AGENT_STATE_WAIT_FOR_HIT;
        break;
    case AGENT_STATE_WAIT_FOR_HIT: //if hit update field and check if you won
        if (protocolStatus == PROTOCOL_PARSED_HIT_MESSAGE) {
            if (AgentGetEnemyStatus() != 0) { 
                //still alive update field with hitmark
                FieldUpdateKnowledge(&AgentData.yourField, &AgentData.gData);
                FieldOledDrawScreen(&AgentData.myField, &AgentData.yourField, FIELD_OLED_TURN_THEIRS);
                state = AGENT_STATE_WAIT_FOR_GUESS;
            } else {
                //else move to win state
                FieldOledDrawScreen(&AgentData.myField, &AgentData.yourField, FIELD_OLED_TURN_NONE);
                state = AGENT_STATE_WON;
            }
        }
        break;
    case AGENT_STATE_WAIT_FOR_GUESS:
        if (protocolStatus == PROTOCOL_PARSED_COO_MESSAGE) {
            if (AgentGetStatus() == 0) {
                //if no ships you lose
                FieldOledDrawScreen(&AgentData.myField, &AgentData.yourField, FIELD_OLED_TURN_NONE);
                state = AGENT_STATE_LOST;
            } else {
                //register enemy attacks and updatethen  send coo msg to enemy
                FieldRegisterEnemyAttack(&AgentData.myField, &AgentData.gData);
                FieldOledDrawScreen(&AgentData.myField, &AgentData.yourField, FIELD_OLED_TURN_MINE);
                state = AGENT_STATE_SEND_GUESS;
            }
            ProtocolEncodeHitMessage(outBuffer, &AgentData.gData);
        }
        break;
    case AGENT_STATE_WON:
        break;
    case AGENT_STATE_LOST:
        break;
    case AGENT_STATE_INVALID:
        break;
    }
    return strlen(outBuffer);
}

/**
 * StateCheck() returns a 4-bit number indicating the status of that agent's ships. The smallest
 * ship, the 3-length one, is indicated by the 0th bit, the medium-length ship (4 tiles) is the
 * 1st bit, etc. until the 3rd bit is the biggest (6-tile) ship. This function is used within
 * main() to update the LEDs displaying each agents' ship status. This function is similar to
 * Field::FieldGetBoatStates().
 * @return A bitfield indicating the sunk/unsunk status of each ship under this agent's control.
 *
 * @see Field.h:FieldGetBoatStates()
 * @see Field.h:BoatStatus
 */
uint8_t AgentGetStatus(void)
{
    return FieldGetBoatStates(&AgentData.myField);
}

/**
 * This function returns the same data as `AgentCheckState()`, but for the enemy agent.
 * @return A bitfield indicating the sunk/unsunk status of each ship under the enemy agent's
 *         control.
 *
 * @see Field.h:FieldGetBoatStates()
 * @see Field.h:BoatStatus
 */
uint8_t AgentGetEnemyStatus(void)
{
    return FieldGetBoatStates(&AgentData.yourField);
}

/**
 * The function randomizes the row, column, and direction of the boat
 * @par f the field the boat is to be added to
 * @par t the type of boat to be added
 * @return SUCCESS if successfully added. STANDARD_ERROR if failed
 */

int RandomFunct(Field *field, BoatType boat) 
//randomizes direction and coordinates for boat
{ 
    int chooseDir = 0;
    int Row, Col;
    BoatDirection Dir;
    chooseDir = (rand() % 4);
    if (chooseDir == 0) { //randomizes direction
        Dir = FIELD_BOAT_DIRECTION_SOUTH;
    } else if (chooseDir == 1) {
        Dir = FIELD_BOAT_DIRECTION_WEST;
    } else if (chooseDir == 2) {
        Dir = FIELD_BOAT_DIRECTION_NORTH;
    } else if (chooseDir == 3) {
        Dir = FIELD_BOAT_DIRECTION_EAST;
    }
    Row = (rand() % (FIELD_ROWS)); //then randomizes row and col
    Col = (rand() % (FIELD_COLS));
    if (FieldAddBoat(field, Row, Col, Dir, boat) == TRUE) { //if adds properly success
        return SUCCESS;
    }
    return STANDARD_ERROR;
}