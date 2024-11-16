#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>
#include <asm-generic/socket.h>

#define PLAYER1_PORT 2201
#define PLAYER2_PORT 2202
#define BUFFER_SIZE 1024
#define MAX_SHIPS 5

// Structure to track ship movements for validation
typedef struct {
    char* rotations[7][4];  // [shape][rotation]
} ShipMoves;

// Game state structure
typedef struct {
    int width;
    int height;
    int** board;
    int ships_remaining;
} GameState;

// Initialize ship movement patterns
ShipMoves* init_ship_moves() {
    ShipMoves* moves = malloc(sizeof(ShipMoves));
    
    // Square - all rotations same
    for(int i = 0; i < 4; i++) {
        moves->rotations[0][i] = strdup("rdl");
    }
    
    // Line piece
    moves->rotations[1][0] = strdup("ddd");
    moves->rotations[1][1] = strdup("rrr");
    moves->rotations[1][2] = strdup("ddd");
    moves->rotations[1][3] = strdup("rrr");
    
    // L piece
    moves->rotations[2][0] = strdup("rur");
    moves->rotations[2][1] = strdup("drd");
    moves->rotations[2][2] = strdup("rur");
    moves->rotations[2][3] = strdup("drd");
    
    // Reverse L piece
    moves->rotations[3][0] = strdup("ddr");
    moves->rotations[3][1] = strdup("durr");
    moves->rotations[3][2] = strdup("rdd");
    moves->rotations[3][3] = strdup("rru");
    
    // T piece
    moves->rotations[4][0] = strdup("rdr");
    moves->rotations[4][1] = strdup("duru");
    moves->rotations[4][2] = strdup("rdr");
    moves->rotations[4][3] = strdup("duru");
    
    // S piece
    moves->rotations[5][0] = strdup("ruu");
    moves->rotations[5][1] = strdup("drr");
    moves->rotations[5][2] = strdup("rldd");
    moves->rotations[5][3] = strdup("rrd");
    
    // Z piece
    moves->rotations[6][0] = strdup("rdur");
    moves->rotations[6][1] = strdup("rudd");
    moves->rotations[6][2] = strdup("rudr");
    moves->rotations[6][3] = strdup("drld");
    
    return moves;
}

// Free ship moves
void free_ship_moves(ShipMoves* moves) {
    for(int i = 0; i < 7; i++) {
        for(int j = 0; j < 4; j++) {
            free(moves->rotations[i][j]);
        }
    }
    free(moves);
}

// Create new game state
GameState* create_game_state(int width, int height) {
    GameState* state = malloc(sizeof(GameState));
    state->width = width;
    state->height = height;
    state->ships_remaining = MAX_SHIPS;
    
    state->board = malloc(height * sizeof(int*));
    for(int i = 0; i < height; i++) {
        state->board[i] = malloc(width * sizeof(int));
        memset(state->board[i], 0, width * sizeof(int));
    }
    
    return state;
}

// Free game state
void free_game_state(GameState* state) {
    for(int i = 0; i < state->height; i++) {
        free(state->board[i]);
    }
    free(state->board);
    free(state);
}

int place_ship(GameState* state, ShipMoves* moves, int shape, int rotation, 
               int start_col, int start_row, int ship_num) {
    char* pattern = moves->rotations[shape-1][rotation-1];
    int test_row = start_row;
    int test_col = start_col;
    
    // Check for overlaps
    if(state->board[test_row][test_col] != 0) {
        return 303;
    }

    for(int i = 0; pattern[i] != '\0'; i++) {
        switch(pattern[i]) {
            case 'r': test_col++; break;
            case 'l': test_col--; break;
            case 'u': test_row--; break;
            case 'd': test_row++; break;
        }
        
        if(state->board[test_row][test_col] != 0) {
            return 303;
        }
    }

    // Place the ship
    state->board[start_row][start_col] = ship_num;
    test_row = start_row;
    test_col = start_col;
    
    for(int i = 0; pattern[i] != '\0'; i++) {
        switch(pattern[i]) {
            case 'r': test_col++; break;
            case 'l': test_col--; break;
            case 'u': test_row--; break;
            case 'd': test_row++; break;
        }
        state->board[test_row][test_col] = ship_num;
    }
    
    return 0;
}

// Process a shot
int process_shot(GameState* state, int row, int col) {
    if(row < 0 || row >= state->height || col < 0 || col >= state->width) {
        return 400;
    }
    
    if(state->board[row][col] == -1 || state->board[row][col] == -2) {
        return 401;
    }
    
    if(state->board[row][col] > 0) {
        int ship_id = state->board[row][col];
        state->board[row][col] = -1;  // Mark hit
        
        // Check if ship is completely sunk
        int ship_remains = 0;
        for(int i = 0; i < state->height && !ship_remains; i++) {
            for(int j = 0; j < state->width && !ship_remains; j++) {
                if(state->board[i][j] == ship_id) {
                    ship_remains = 1;
                }
            }
        }
        
        if(!ship_remains) {
            state->ships_remaining--;
        }
        
        return -1;  // Hit
    }
    
    state->board[row][col] = -2;  // Mark miss
    return -2;
}

// Generate query response
char* create_query_response(GameState* state) {
    char* response = malloc(BUFFER_SIZE);
    int pos = sprintf(response, "G %d", state->ships_remaining);
    
    for(int i = 0; i < state->height; i++) {
        for(int j = 0; j < state->width; j++) {
            if(state->board[i][j] == -1) {
                pos += sprintf(response + pos, " H %d %d", i, j);
            }
            else if(state->board[i][j] == -2) {
                pos += sprintf(response + pos, " M %d %d", i, j);
            }
        }
    }
    
    return response;
}

int validate_init_packet(const char* buffer) {
    if(strlen(buffer) < 2 || buffer[0] != 'I' || buffer[1] != ' ')
        return 0;
    
    int count = 0;
    char *str = strdup(buffer);
    char *save_ptr = NULL;
    char *token = strtok_r(str + 2, " ", &save_ptr);
    
    while(token != NULL) {
        for(int i = 0; token[i]; i++) {
            if(!isdigit(token[i])) {
                free(str);
                return 0;
            }
        }
        count++;
        token = strtok_r(NULL, " ", &save_ptr);
    }
    
    free(str);
    return count == 20;
}

int validate_init_values(int* values, int width, int height) {
    for(int i = 0; i < 20; i += 4) {
        // Check shape first (1-7)
        if(values[i] < 1 || values[i] > 7) {
            return 300;
        }
        
        // Check rotation second (1-4)
        if(values[i + 1] < 1 || values[i + 1] > 4) {
            return 301;
        }
        
        // Check position bounds
        if(values[i + 2] < 0 || values[i + 2] >= width ||
           values[i + 3] < 0 || values[i + 3] >= height) {
            return 302;
        }
    }
    
    return 0;
}

int handle_initialize(int client_fd, GameState* state, ShipMoves* moves, char* buffer) {
    int values[20];
    
    // Check packet type
    if(buffer[0] != 'I') {
        send(client_fd, "E 101", 5, 0);
        return -1;
    }
    
    // Check format
    if(!validate_init_packet(buffer)) {
        send(client_fd, "E 201", 5, 0);
        return -1;
    }
    
    // Parse values
    int count = 0;
    char* token = strtok(buffer + 2, " ");
    while(token && count < 20) {
        values[count++] = atoi(token);
        token = strtok(NULL, " ");
    }
    
    if(count != 20) {
        send(client_fd, "E 201", 5, 0);
        return -1;
    }

    // Validate all shapes first (300)
    for(int i = 0; i < 20; i += 4) {
        if(values[i] < 1 || values[i] > 7) {
            send(client_fd, "E 300", 5, 0);
            return -1;
        }
    }

    // Validate all rotations (301)
    for(int i = 0; i < 20; i += 4) {
        if(values[i + 1] < 1 || values[i + 1] > 4) {
            send(client_fd, "E 301", 5, 0);
            return -1;
        }
    }

    // Check ALL positions for boundary issues first
    for(int i = 0; i < 20; i += 4) {
        int shape = values[i];
        int rotation = values[i + 1];
        int col = values[i + 2];
        int row = values[i + 3];
        
        // Initial position check
        if(row < 0 || row >= state->height || col < 0 || col >= state->width) {
            send(client_fd, "E 302", 5, 0);
            return -1;
        }
        
        // Get pattern and check each position
        char* pattern = moves->rotations[shape-1][rotation-1];
        int test_row = row;
        int test_col = col;
        
        for(int j = 0; pattern[j] != '\0'; j++) {
            switch(pattern[j]) {
                case 'r': test_col++; break;
                case 'l': test_col--; break;
                case 'u': test_row--; break;
                case 'd': test_row++; break;
            }
            
            if(test_row < 0 || test_row >= state->height || 
               test_col < 0 || test_col >= state->width) {
                send(client_fd, "E 302", 5, 0);
                return -1;
            }
        }
    }

    // Only after ALL boundary checks pass, try placing ships
    for(int i = 0; i < 20; i += 4) {
        int result = place_ship(state, moves, values[i], values[i+1], 
                              values[i+2], values[i+3], (i/4) + 1);
        if(result == 303) {
            send(client_fd, "E 303", 5, 0);
            // Clear board
            for(int j = 0; j < state->height; j++) {
                memset(state->board[j], 0, state->width * sizeof(int));
            }
            return -1;
        }
    }
    
    send(client_fd, "A", 1, 0);
    return 0;
}

int main() {
    int server1_fd, server2_fd, client1_fd, client2_fd;
    struct sockaddr_in addr1, addr2;
    char buffer[BUFFER_SIZE] = {0};
    GameState *player1_state = NULL, *player2_state = NULL;
    ShipMoves* ship_moves = init_ship_moves();
    
    // Create sockets
    if((server1_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket 1 creation failed");
        exit(EXIT_FAILURE);
    }
    if((server2_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket 2 creation failed");
        exit(EXIT_FAILURE);
    }
    
    // Configure socket options
    int opt = 1;
    setsockopt(server1_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    setsockopt(server2_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    
    // Configure addresses
    addr1.sin_family = AF_INET;
    addr1.sin_addr.s_addr = INADDR_ANY;
    addr1.sin_port = htons(PLAYER1_PORT);
    
    addr2.sin_family = AF_INET;
    addr2.sin_addr.s_addr = INADDR_ANY;
    addr2.sin_port = htons(PLAYER2_PORT);
    
    // Bind sockets
    if(bind(server1_fd, (struct sockaddr *)&addr1, sizeof(addr1)) < 0) {
        perror("Bind 1 failed");
        exit(EXIT_FAILURE);
    }
    if(bind(server2_fd, (struct sockaddr *)&addr2, sizeof(addr2)) < 0) {
        perror("Bind 2 failed");
        exit(EXIT_FAILURE);
    }
    
    // Listen for connections
    if(listen(server1_fd, 1) < 0 || listen(server2_fd, 1) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    // Accept connections
    int addrlen = sizeof(addr1);
    if((client1_fd = accept(server1_fd, (struct sockaddr *)&addr1, (socklen_t*)&addrlen)) < 0) {
        perror("Accept 1 failed");
        exit(EXIT_FAILURE);
    }
    if((client2_fd = accept(server2_fd, (struct sockaddr *)&addr2, (socklen_t*)&addrlen)) < 0) {
        perror("Accept 2 failed");
        exit(EXIT_FAILURE);
    }
    
      // Modified Begin phase for Player 1
    while(1) {
        int width, height;
        char extra;
        memset(buffer, 0, BUFFER_SIZE);
        read(client1_fd, buffer, BUFFER_SIZE);
        
        if(buffer[0] == 'F') {
            send(client1_fd, "H 0", 3, 0);
            send(client2_fd, "H 1", 3, 0);
            goto cleanup;
        }
        
        if(buffer[0] != 'B') {
            send(client1_fd, "E 100", 5, 0);
            continue;
        }
        
        int params = sscanf(buffer, "B %d %d%c", &width, &height, &extra);
        if(params != 2 || buffer[1] != ' ') {
            send(client1_fd, "E 200", 5, 0);
            continue;
        }
        
        if(width < 10 || height < 10) {
            send(client1_fd, "E 200", 5, 0);
            continue;
        }
        
        player1_state = create_game_state(width, height);
        player2_state = create_game_state(width, height);
        send(client1_fd, "A", 1, 0);
        break;
    }

    // Modified Begin phase for Player 2
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        read(client2_fd, buffer, BUFFER_SIZE);
        
        if(buffer[0] == 'F') {
            send(client2_fd, "H 0", 3, 0);
            send(client1_fd, "H 1", 3, 0);
            goto cleanup;
        }
        
        if(buffer[0] != 'B') {
            send(client2_fd, "E 100", 5, 0);
            continue;
        }
        
        if(strlen(buffer) > 1) {
            send(client2_fd, "E 200", 5, 0);
            continue;
        }
        
        send(client2_fd, "A", 1, 0);
        break;
    }

    // Modified Initialize phase for Player 1
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(client1_fd, buffer, BUFFER_SIZE - 1);
        if(bytes_read <= 0) {
            goto cleanup;
        }
        buffer[bytes_read] = '\0';
        
        if(strcmp(buffer, "F") == 0) {
            send(client1_fd, "H 0", 3, 0);
            send(client2_fd, "H 1", 3, 0);
            goto cleanup;
        }
        
        int result = handle_initialize(client1_fd, player1_state, ship_moves, buffer);
        if(result == 0) break;  // Successfully initialized
    }

    // Modified Initialize phase for Player 2
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_read = read(client2_fd, buffer, BUFFER_SIZE - 1);
        if(bytes_read <= 0) {
            goto cleanup;
        }
        buffer[bytes_read] = '\0';
        
        if(strcmp(buffer, "F") == 0) {
            send(client2_fd, "H 0", 3, 0);
            send(client1_fd, "H 1", 3, 0);
            goto cleanup;
        }
        
        int result = handle_initialize(client2_fd, player2_state, ship_moves, buffer);
        if(result == 0) break;  // Successfully initialized
    }
    
    // Main game loop
    int current_player = 1;  // Start with player 1
    char response[BUFFER_SIZE];
    
    while(1) {
        int client_fd = (current_player == 1) ? client1_fd : client2_fd;
        GameState* target_state = (current_player == 1) ? player2_state : player1_state;
        
        memset(buffer, 0, BUFFER_SIZE);
        read(client_fd, buffer, BUFFER_SIZE);
        
        // Handle forfeit
        if(buffer[0] == 'F') {
            send(client_fd, "H 0", 3, 0);
            send((current_player == 1) ? client2_fd : client1_fd, "H 1", 3, 0);
            break;
        }
        
        // Handle query
        if(buffer[0] == 'Q') {
            if(strlen(buffer) != 1) {
                send(client_fd, "E 102", 5, 0);
                continue;
            }
            char* query_response = create_query_response(target_state);
            send(client_fd, query_response, strlen(query_response), 0);
            free(query_response);
            continue;
        }
        
       // Handle shot
if(buffer[0] == 'S') {
    int row, col;
    char extra;
    
    if(sscanf(buffer, "S %d %d%c", &row, &col, &extra) != 2) {
        send(client_fd, "E 202", 5, 0);
        continue;
    }
    
    int result = process_shot(target_state, row, col);
    
    if(result == 400) {
        send(client_fd, "E 400", 5, 0);
        continue;
    }
    
    if(result == 401) {
        send(client_fd, "E 401", 5, 0);
        continue;
    }
    
    // Send shot response
    char msg[8];
    memset(msg, 0, sizeof(msg));
    if(result == -1) { // Hit
        sprintf(msg, "R %d H", target_state->ships_remaining);
    } else { // Miss
        sprintf(msg, "R %d M", target_state->ships_remaining);
    }
    send(client_fd, msg, strlen(msg), 0);

    // If game is over, let next read handle the win condition
    if(target_state->ships_remaining == 0) {
        int next_fd = (current_player == 1) ? client2_fd : client1_fd;
        memset(buffer, 0, BUFFER_SIZE);
        read(next_fd, buffer, BUFFER_SIZE);  // Wait for next read
        
        // Now send halt packets
        if(current_player == 1) {
            send(client2_fd, "H 0", 3, 0);
            send(client1_fd, "H 1", 3, 0);
        } else {
            send(client1_fd, "H 0", 3, 0);
            send(client2_fd, "H 1", 3, 0);
        }
        goto cleanup;
    }
    
    current_player = (current_player == 1) ? 2 : 1;
    continue;
}
        
        // Invalid packet type
        send(client_fd, "E 102", 5, 0);
    }
    
cleanup:
    // Cleanup resources
    if(player1_state) free_game_state(player1_state);
    if(player2_state) free_game_state(player2_state);
    free_ship_moves(ship_moves);
    close(client1_fd);
    close(client2_fd);
    close(server1_fd);
    close(server2_fd);
    
    return 0;
}