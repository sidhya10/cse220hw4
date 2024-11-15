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

// Validate and place a ship piece
int place_ship(GameState* state, ShipMoves* moves, int shape, int rotation, int start_col, int start_row, int ship_num) {
    // Validate inputs
    if(shape < 1 || shape > 7) return 300;
    if(rotation < 1 || rotation > 4) return 301;
    if(start_row < 0 || start_row >= state->height || 
       start_col < 0 || start_col >= state->width) return 302;
    
    // Get movement pattern
    char* pattern = moves->rotations[shape-1][rotation-1];
    int curr_row = start_row;
    int curr_col = start_col;
    int** temp_board = malloc(state->height * sizeof(int*));
    for(int i = 0; i < state->height; i++) {
        temp_board[i] = malloc(state->width * sizeof(int));
        memcpy(temp_board[i], state->board[i], state->width * sizeof(int));
    }
    
    // Place first cell
    if(temp_board[curr_row][curr_col] != 0) {
        for(int i = 0; i < state->height; i++) free(temp_board[i]);
        free(temp_board);
        return 303;
    }
    temp_board[curr_row][curr_col] = ship_num;
    
    // Follow pattern
    for(int i = 0; pattern[i] != '\0'; i++) {
        switch(pattern[i]) {
            case 'r': curr_col++; break;
            case 'l': curr_col--; break;
            case 'u': curr_row--; break;
            case 'd': curr_row++; break;
        }
        
        // Check bounds
        if(curr_row < 0 || curr_row >= state->height || 
           curr_col < 0 || curr_col >= state->width) {
            for(int i = 0; i < state->height; i++) free(temp_board[i]);
            free(temp_board);
            return 302;
        }
        
        // Check overlap
        if(temp_board[curr_row][curr_col] != 0) {
            for(int i = 0; i < state->height; i++) free(temp_board[i]);
            free(temp_board);
            return 303;
        }
        
        temp_board[curr_row][curr_col] = ship_num;
    }
    
    // If we got here, placement is valid - commit to real board
    for(int i = 0; i < state->height; i++) {
        memcpy(state->board[i], temp_board[i], state->width * sizeof(int));
        free(temp_board[i]);
    }
    free(temp_board);
    
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
    
    // Handle Begin phase
    while(1) {
        int width, height;
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
        
        if(sscanf(buffer, "B %d %d", &width, &height) != 2) {
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
    
    // Handle Player 2 Begin
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
    
    // Handle Initialize phase - Player 1
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        read(client1_fd, buffer, BUFFER_SIZE);
        
        if(buffer[0] == 'F') {
            send(client1_fd, "H 0", 3, 0);
            send(client2_fd, "H 1", 3, 0);
            goto cleanup;
        }
        
        if(buffer[0] != 'I') {
            send(client1_fd, "E 101", 5, 0);
            continue;
        }
        
        int values[20];
        int count = 0;
        char* token = strtok(buffer + 2, " ");
        while(token && count < 20) {
            values[count++] = atoi(token);
            token = strtok(NULL, " ");
        }
        
        if(count != 20) {
            send(client1_fd, "E 201", 5, 0);
            continue;
        }
        
        int error = 0;
        for(int i = 0; i < 20; i += 4) {
            int result = place_ship(player1_state, ship_moves, 
                                  values[i], values[i+1], 
                                  values[i+2], values[i+3], 
                                  (i/4) + 1);
            if(result != 0) {
                error = result;
                break;
            }
            }
        
        if(error) {
            send(client1_fd, error == 300 ? "E 300" :
                           error == 301 ? "E 301" :
                           error == 302 ? "E 302" : "E 303", 5, 0);
            continue;
        }
        
        send(client1_fd, "A", 1, 0);
        break;
    }
    
    // Handle Initialize phase - Player 2
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        read(client2_fd, buffer, BUFFER_SIZE);
        
        if(buffer[0] == 'F') {
            send(client2_fd, "H 0", 3, 0);
            send(client1_fd, "H 1", 3, 0);
            goto cleanup;
        }
        
        if(buffer[0] != 'I') {
            send(client2_fd, "E 101", 5, 0);
            continue;
        }
        
        int values[20];
        int count = 0;
        char* token = strtok(buffer + 2, " ");
        while(token && count < 20) {
            values[count++] = atoi(token);
            token = strtok(NULL, " ");
        }
        
        if(count != 20) {
            send(client2_fd, "E 201", 5, 0);
            continue;
        }
        
        int error = 0;
        for(int i = 0; i < 20; i += 4) {
            int result = place_ship(player2_state, ship_moves,
                                  values[i], values[i+1],
                                  values[i+2], values[i+3],
                                  (i/4) + 1);
            if(result != 0) {
                error = result;
                break;
            }
        }
        
        if(error) {
            send(client2_fd, error == 300 ? "E 300" :
                           error == 301 ? "E 301" :
                           error == 302 ? "E 302" : "E 303", 5, 0);
            continue;
        }
        
        send(client2_fd, "A", 1, 0);
        break;
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
            sprintf(response, "R %d %c", target_state->ships_remaining,
                    result == -1 ? 'H' : 'M');
            send(client_fd, response, strlen(response), 0);
            
            // Check win condition
            if(target_state->ships_remaining == 0) {
                // Send halt messages
                if(current_player == 1) {
                    send(client2_fd, "H 0", 3, 0);
                    send(client1_fd, "H 1", 3, 0);
                } else {
                    send(client1_fd, "H 0", 3, 0);
                    send(client2_fd, "H 1", 3, 0);
                }
                goto cleanup;
            }
            
            // Switch players
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