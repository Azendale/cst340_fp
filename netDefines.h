/************************************
 * Author: Erik Andersen
 * Lab: CST340 Final Lab
 * 
 * All the defines for network messages. In a separate file so that they can be
 * shared and kept in sync between the client and server.
 ***********************************/

// Message actions
#define ACTION_MASK 0xFC000000
#define ACTION_NAME_REQUEST 0x04000000
#define ACTION_NAME_TAKEN 0x08000000
#define ACTION_NAME_IS_YOURS 0x0c000000
#define ACTION_REQ_PLAYERS_LIST 0x10000000
#define ACTION_PLAY_PLAYERNAME 0x14000000
#define ACTION_INVITE_RESPONSE 0x18000000
#define ACTION_INVITE_REQ 0x1c000000
#define ACTION_MOVE 0x20000000
#define ACTION_MOVE_RESULTS 0x24000000

// For transferring coordinates of moves and their results
#define MOVE_X_COORD_SHIFT 16
#define MOVE_Y_COORD_SHIFT 12
#define MOVE_X_COORD_MASK_UNSHIFTED 0x0000000F<<MOVE_X_COORD_SHIFT
#define MOVE_Y_COORD_MASK_UNSHIFTED 0x0000000F<<MOVE_Y_COORD_SHIFT

// For results of moves messages (use with ACTION_MOVE_RESULTS)
#define MOVE_HIT_SHIP_MASK 1<<25
#define MOVE_SINK_SHIP_MASK 1<<24
// Size of the ship that was hit stuff (use with ACTION_MOVE_RESULTS 
// when MOVE_HIT_SHIP_MASK is set)
#define MOVE_SIZE_OF_HIT_SHIP_SHIFT 21
#define MOVE_SIZE_OF_HIT_SHIP_MASK_UNSHIFTED 0x00000007<<MOVE_SIZE_OF_HIT_SHIP_SHIFT

// Mask for long transfer sizes: up to 1M
#define LONG_TRANSFER_SIZE_MASK 0x000003FF

// Mask for saying whether a move was winning or not
#define WIN_BIT_MASK 1<<20
#define WIN_YES WIN_BIT_MASK
#define WIN_NO ~WIN_YES

// Up to 255 chars/bytes
#define TRANSFER_SIZE_MASK 0x000000FF

// Responses to a game invite
#define INVITE_RESPONSE_NO 0
#define INVITE_RESPONSE_YES 1

// Max length of a username
#define MAX_NAME_LEN 64