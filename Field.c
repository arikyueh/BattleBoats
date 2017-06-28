#include "Field.h"
#include "BOARD.h"
#include "Protocol.h"


static int i, j, temp;

/**
 * FieldInit() will fill the passed field array with the data specified in positionData. Also the
 * lives for each boat are filled according to the `BoatLives` enum.
 * @param f The field to initialize.
 * @param p The data to initialize the entire field to, should be a member of enum
 *                     FieldPosition.
 */
void FieldInit(Field *f, FieldPosition p) {
    for (i = 0; i < FIELD_ROWS; i++) {
        //fills field with 6 rows
        for (j = 0; j < FIELD_COLS; j++) {
            //fills field with 10 columns
            f->field [i][j] = p;
        }
    }
    //initializes all the lives
    f->hugeBoatLives = FIELD_BOAT_LIVES_HUGE;
    f->largeBoatLives = FIELD_BOAT_LIVES_LARGE;
    f->mediumBoatLives = FIELD_BOAT_LIVES_MEDIUM;
    f->smallBoatLives = FIELD_BOAT_LIVES_SMALL;
}

/**
 * Retrieves the value at the specified field position.
 * @param f The Field being referenced
 * @param row The row-component of the location to retrieve
 * @param col The column-component of the location to retrieve
 * @return
 */
FieldPosition FieldAt(const Field *f, uint8_t row, uint8_t col) {
    //returns requested field position
    temp = f->field[row][col];
    return temp;
}

/**
 * This function provides an interface for setting individual locations within a Field struct. This
 * is useful when FieldAddBoat() doesn't do exactly what you need. For example, if you'd like to use
 * FIELD_POSITION_CURSOR, this is the function to use.
 * 
 * @param f The Field to modify.
 * @param row The row-component of the location to modify
 * @param col The column-component of the location to modify
 * @param p The new value of the field location
 * @return The old value at that field location
 */
FieldPosition FieldSetLocation(Field *f, uint8_t row, uint8_t col, FieldPosition p) {
    //sets the current field position
    temp = FieldAt(f, row, col);
    f->field[row][col] = p;
    return temp;
}

/**
 * FieldAddBoat() places a single ship on the player's field based on arguments 2-5. Arguments 2, 3
 * represent the x, y coordinates of the pivot point of the ship.  Argument 4 represents the
 * direction of the ship, and argument 5 is the length of the ship being placed. All spaces that
 * the boat would occupy are checked to be clear before the field is modified so that if the boat
 * can fit in the desired position, the field is modified as SUCCESS is returned. Otherwise the
 * field is unmodified and STANDARD_ERROR is returned. There is no hard-coded limit to how many
 * times a boat can be added to a field within this function.
 *
 * So this is valid test code:
 * {
 *   Field myField;
 *   FieldInit(&myField,FIELD_POSITION_EMPTY);
 *   FieldAddBoat(&myField, 0, 0, FIELD_BOAT_DIRECTION_EAST, FIELD_BOAT_SMALL);
 *   FieldAddBoat(&myField, 1, 0, FIELD_BOAT_DIRECTION_EAST, FIELD_BOAT_MEDIUM);
 *   FieldAddBoat(&myField, 1, 0, FIELD_BOAT_DIRECTION_EAST, FIELD_BOAT_HUGE);
 *   FieldAddBoat(&myField, 0, 6, FIELD_BOAT_DIRECTION_SOUTH, FIELD_BOAT_SMALL);
 * }
 *
 * should result in a field like:
 *  _ _ _ _ _ _ _ _
 * [ 3 3 3       3 ]
 * [ 4 4 4 4     3 ]
 * [             3 ]
 *  . . . . . . . .
 *
 * @param f The field to grab data from.
 * @param row The row that the boat will start from, valid range is from 0 and to FIELD_ROWS - 1.
 * @param col The column that the boat will start from, valid range is from 0 and to FIELD_COLS - 1.
 * @param dir The direction that the boat will face once places, from the BoatDirection enum.
 * @param boatType The type of boat to place. Relies on the FIELD_POSITION_*_BOAT values from the
 * FieldPosition enum.
 * @return TRUE for success, FALSE for failure
 */
uint8_t FieldAddBoat(Field *f, uint8_t row, uint8_t col, BoatDirection dir, BoatType type) {
    int BOATSIZE = (type + 3);
    //adds boat while checking bounds, accounts for every direction
    if (dir == FIELD_BOAT_DIRECTION_NORTH) {
        if (col >= FIELD_COLS || col < 0) { 
            //if starting point goes off bounds
            return FALSE;
        }
        if (row >= FIELD_ROWS || row - BOATSIZE + 1 < 0) { 
            //if max boatsize goes off bounds
            return FALSE;
        }
        for (i = 0; i < BOATSIZE; i++) {
            if (f->field[row - i][col] != FIELD_POSITION_EMPTY) {
                //checks each time that each position is empty
                return FALSE;
            }
        }
        for (i = 0; i < BOATSIZE; i++) {
            //add boat for respective size
            if (BOATSIZE == 3) {
                f->field[row - i][col] = FIELD_POSITION_SMALL_BOAT;
            } else if (BOATSIZE == 4) {
                f->field[row - i][col] = FIELD_POSITION_MEDIUM_BOAT;
            } else if (BOATSIZE == 5) {
                f->field[row - i][col] = FIELD_POSITION_LARGE_BOAT;
            } else if (BOATSIZE == 6) {
                f->field[row - i][col] = FIELD_POSITION_HUGE_BOAT;
            }
        }
    } else if (dir == FIELD_BOAT_DIRECTION_EAST) {
        if (row >= FIELD_ROWS || row < 0) {
            return FALSE;
        }
        if (col < 0 || col + BOATSIZE > FIELD_COLS) {
            return FALSE;
        }
        for (i = 0; i < BOATSIZE; i++) {
            if (f->field[row][col + i] != FIELD_POSITION_EMPTY) {
                return FALSE;
            }
        }
        for (i = 0; i < BOATSIZE; i++) {
            if (BOATSIZE == 3) {
                f->field[row][col + i] = FIELD_POSITION_SMALL_BOAT;
            } else if (BOATSIZE == 4) {
                f->field[row][col + i] = FIELD_POSITION_MEDIUM_BOAT;
            } else if (BOATSIZE == 5) {
                f->field[row][col + i] = FIELD_POSITION_LARGE_BOAT;
            } else if (BOATSIZE == 6) {
                f->field[row][col + i] = FIELD_POSITION_HUGE_BOAT;
            }
        }
    } else if (dir == FIELD_BOAT_DIRECTION_SOUTH) {
        if (col >= FIELD_COLS || col < 0) {
            return FALSE;
        }
        if (row < 0 || row + BOATSIZE > FIELD_ROWS) {
            return FALSE;
        }
        for (i = 0; i < BOATSIZE; i++) {
            if (f->field[row + i][col] != FIELD_POSITION_EMPTY) {
                return FALSE;
            }
        }
        for (i = 0; i < BOATSIZE; i++) {
            if (BOATSIZE == 3) {
                f->field[row + i][col] = FIELD_POSITION_SMALL_BOAT;
            } else if (BOATSIZE == 4) {
                f->field[row + i][col] = FIELD_POSITION_MEDIUM_BOAT;
            } else if (BOATSIZE == 5) {
                f->field[row + i][col] = FIELD_POSITION_LARGE_BOAT;
            } else if (BOATSIZE == 6) {
                f->field[row + i][col] = FIELD_POSITION_HUGE_BOAT;
            }
        }
    } else if (dir == FIELD_BOAT_DIRECTION_WEST) {
        if (row >= FIELD_ROWS || row < 0) {
            return FALSE;
        }
        if (col >= FIELD_COLS || col - BOATSIZE + 1 < 0) {
            return FALSE;
        }
        for (i = 0; i < BOATSIZE; i++) {
            if (f->field[row][col - i] != FIELD_POSITION_EMPTY) {
                return FALSE;
            }
        }
        for (i = 0; i < BOATSIZE; i++) {
            if (BOATSIZE == 3) {
                f->field[row][col - i] = FIELD_POSITION_SMALL_BOAT;
            } else if (BOATSIZE == 4) {
                f->field[row][col - i] = FIELD_POSITION_MEDIUM_BOAT;
            } else if (BOATSIZE == 5) {
                f->field[row][col - i] = FIELD_POSITION_LARGE_BOAT;
            } else if (BOATSIZE == 6) {
                f->field[row][col - i] = FIELD_POSITION_HUGE_BOAT;
            }
        }
    }
    return TRUE;
}

/**
 * This function registers an attack at the gData coordinates on the provided field. This means that
 * 'f' is updated with a FIELD_POSITION_HIT or FIELD_POSITION_MISS depending on what was at the
 * coordinates indicated in 'gData'. 'gData' is also updated with the proper HitStatus value
 * depending on what happened AND the value of that field position BEFORE it was attacked. Finally
 * this function also reduces the lives for any boat that was hit from this attack.
 * @param f The field to check against and update.
 * @param gData The coordinates that were guessed. The HIT result is stored in gData->hit as an
 *               output.
 * @return The data that was stored at the field position indicated by gData before this attack.
 */
FieldPosition FieldRegisterEnemyAttack(Field *f, GuessData *gData) {
    temp = FieldAt(f, gData->row, gData->col); //store previous position in temp
    switch (temp) { //if any field position is a boat its a hit
        case(FIELD_POSITION_SMALL_BOAT):
            f->smallBoatLives--;
            if (f->smallBoatLives == 0) { //when lives run to 0 sink ship
                gData->hit = HIT_SUNK_SMALL_BOAT;
            } else gData->hit = HIT_HIT; //else if still alive update with a hit
            break;
        case(FIELD_POSITION_MEDIUM_BOAT):
            f->mediumBoatLives--;
            if (f->mediumBoatLives == 0) {
                gData->hit = HIT_SUNK_MEDIUM_BOAT;
            } else gData->hit = HIT_HIT;
            break;
        case(FIELD_POSITION_LARGE_BOAT):
            f->largeBoatLives--;
            if (f->largeBoatLives == 0) {
                gData->hit = HIT_SUNK_LARGE_BOAT;
            } else gData->hit = HIT_HIT;
            break;
        case(FIELD_POSITION_HUGE_BOAT):
            f->hugeBoatLives--;
            if (f->hugeBoatLives == 0) {
                gData->hit = HIT_SUNK_HUGE_BOAT;
            } else gData->hit = HIT_HIT;
            break;
        default: //if hit none of the ships update hit with miss
            gData->hit = HIT_MISS;
            break;
    }
    //update coordinates with either hits or misses
    if (gData->hit == HIT_HIT) {
        FieldSetLocation(f, gData->row, gData->col, FIELD_POSITION_HIT);
    } else if (gData->hit == HIT_MISS) {
        FieldSetLocation(f, gData->row, gData->col, FIELD_POSITION_MISS);
    }
    return temp; //keep old position data before attack
}

/**
 * This function updates the FieldState representing the opponent's game board with whether the
 * guess indicated within gData was a hit or not. If it was a hit, then the field is updated with a
 * FIELD_POSITION_HIT at that position. If it was a miss, display a FIELD_POSITION_EMPTY instead, as
 * it is now known that there was no boat there. The FieldState struct also contains data on how
 * many lives each ship has. Each hit only reports if it was a hit on any boat or if a specific boat
 * was sunk, this function also clears a boats lives if it detects that the hit was a
 * HIT_SUNK_*_BOAT.
 * @param f The field to grab data from.
 * @param gData The coordinates that were guessed along with their HitStatus.
 * @return The previous value of that coordinate position in the field before the hit/miss was
 * registered.
 */
FieldPosition FieldUpdateKnowledge(Field *f, const GuessData *gData) {
    //sets hits and misses and updates sunken ships
    temp = FieldAt(f, gData->row, gData->col);
    if (gData->hit != HIT_MISS) {
        FieldSetLocation(f, gData->row, gData->col, FIELD_POSITION_HIT);
        if (gData->hit == HIT_SUNK_HUGE_BOAT) {
            f->hugeBoatLives = 0;
        } else if (gData->hit == HIT_SUNK_LARGE_BOAT) {
            f->largeBoatLives = 0;
        } else if (gData->hit == HIT_SUNK_MEDIUM_BOAT) {
            f->mediumBoatLives = 0;
        } else if (gData->hit == HIT_SUNK_SMALL_BOAT) {
            f->smallBoatLives = 0;
        }
    } else FieldSetLocation(f, gData->row, gData->col, FIELD_POSITION_EMPTY);
    return temp; //returns position data
}

/**
 * This function returns the alive states of all 4 boats as a 4-bit bitfield (stored as a uint8).
 * The boats are ordered from smallest to largest starting at the least-significant bit. So that:
 * 0b00001010 indicates that the small boat and large boat are sunk, while the medium and huge boat
 * are still alive. See the BoatStatus enum for the bit arrangement.
 * @param f The field to grab data from.
 * @return A 4-bit value with each bit corresponding to whether each ship is alive or not.
 */
uint8_t FieldGetBoatStates(const Field *f) {
    uint8_t BinaryStatus = 0b00001111;
    if (f->hugeBoatLives == 0) { //toggles huge bit in binarystatus to 0
        BinaryStatus ^= 0b00001000;
    }
    if (f->largeBoatLives == 0) { //toggles large bit in binarystatus to 0
        BinaryStatus ^= 0b0000100;
    }
    if (f->mediumBoatLives == 0) { //toggles medium bit in binarystatus to 0
        BinaryStatus ^= 0b00000010;
    }
    if (f->smallBoatLives == 0) { //toggles small bit in binarystatus to 0
        BinaryStatus ^= 0b00000001;
    }
    return BinaryStatus;
}