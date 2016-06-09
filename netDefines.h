#define ACTION_MASK 0xFC000000
#define ACTION_NAME_REQUEST 0x04000000
#define ACTION_NAME_TAKEN 0x08000000
#define ACTION_NAME_IS_YOURS 0x0c000000
#define ACTION_REQ_PLAYERS_LIST 0x10000000
#define ACTION_PLAY_PLAYERNAME 0x14000000

// Mask for long transfer sizes: uses all extra space left after command bits for size
#define LONG_TRANSFER_SIZE_MASK ~ACTION_MASK

// Up to 255 chars/bytes
#define TRANSFER_SIZE_MASK 0x000000FF

#define MAX_NAME_LEN 64