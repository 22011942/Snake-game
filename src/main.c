#include "graphics.h"
#include <driver/gpio.h>
#include <esp_timer.h>
#include <fonts.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "esp_system.h"
#include <nvs_flash.h>

int capacity = 1;
int score = 0;
int scoreLimit = 0;
int speed = 50;
const char directions[] = {'l', 'u', 'r', 'd'};
float snake_x, snake_y, apple_X, apple_y;
char buffer[50];
char buffer1[50];
int dirction_index = 2;
bool initial_start = false;
static int high_score_e=0;
static int high_score_m=0;
static int high_score_h=0;

volatile int button0_pressed = 0;
volatile int button35_pressed = 0;

typedef enum {MODES, SPAWNRATE, MAINMENU} selector;
selector currrentSelect = MAINMENU;

typedef enum {MENU, GAME} Mode;
Mode currentMode = MENU;

typedef struct {
    float x;
    float y;
    bool eaten;
} Apple;

typedef struct {
    float x;
    float y;
    struct snakeSegment* next;
    struct snakeSegment* prev;
} snakeSegment;

snakeSegment* tail;

#define MAX_HISTORY 1000
float positionHistoryX[MAX_HISTORY];
float positionHistoryY[MAX_HISTORY];
int historyIndex = 0;

nvs_handle_t storage_open(nvs_open_mode_t mode) { //Opens the storage so the data can either be written or read.
    esp_err_t err;
    nvs_handle_t my_handle;
    err = nvs_open("storage", mode, &my_handle);
    if(err!=0) {
        nvs_flash_init();
        err = nvs_open("storage", mode, &my_handle);
        printf("err1: %d\n",err);
    }
    return my_handle;
}

int storage_read_int(char *name, int def) { // Reads data from storage
    nvs_handle_t handle=storage_open(NVS_READONLY);
    int32_t val=def;
    nvs_get_i32(handle, name, &val);
    nvs_close(handle);
    return val;
}

void storage_write_int(char *name, int val) { // Writes data to storage
    nvs_handle_t handle=storage_open(NVS_READWRITE);
    nvs_set_i32(handle, name, val);
    nvs_commit(handle);
    nvs_close(handle);
}

void update_position_history(float new_X, float new_y) { // Stores a new position into the history of past positions
    positionHistoryX[historyIndex] = new_X;
    positionHistoryY[historyIndex] = new_y;
    historyIndex = (historyIndex + 1) % MAX_HISTORY; 
}

snakeSegment* intialise_snake(int x, int y) { // This is used for the doubly linked list created for each snake segment with their positions
    snakeSegment* head = (snakeSegment*)malloc(sizeof(snakeSegment));
    head->x = x;
    head->y = y;
    head->next = NULL;
    head->prev = NULL;
    tail = head; 
    return head;
}

void add_segment() { // Adds a new segment to the snake linked list
    snakeSegment* new_segment = (snakeSegment*)malloc(sizeof(snakeSegment));
    
    new_segment->next = NULL;
    new_segment->prev = tail; 
    
    tail->next = new_segment; 
    tail = new_segment;
}

void move_snake(float new_X, float new_y) { // This is responsible for calculating the snakes previous postions
    update_position_history(new_X, new_y);
    snakeSegment* current = tail;
    int segmentIndex = 1;  // This ensures that each segment is equally separated

    // This calculates specific delays for each of the speeds
    while(current->prev != NULL) {
        int delay = segmentIndex * 10;
        if (speed == 75) {
            delay = segmentIndex * 6;
        } else if (speed == 110) {
            delay = segmentIndex * 5;
        }
        
        int historyPos = (historyIndex - delay + MAX_HISTORY) % MAX_HISTORY;  // Retrieves the delayed position
        
        current->x = positionHistoryX[historyPos];
        current->y = positionHistoryY[historyPos];

        current = current->prev;  
        segmentIndex++; 
    }

    current->x = new_X;
    current->y = new_y;
}

void set_start_pos() { // Sets the starting position of the snake
    snake_x = display_width / 2;
    snake_y = display_height / 2;
}


// These are the button handlers for the left and right button which have seprate functionality depending on the current section of the game
void IRAM_ATTR gpio0_isr_handler(void* arg) { 
    if (currentMode == MENU) {
        button0_pressed = 1;
    } else if (currentMode == GAME) {
        dirction_index -= 1;
        if (dirction_index < 0) {
            dirction_index = 3;
        }
    }
}

void IRAM_ATTR gpio35_isr_handler(void* arg) {
    if (currentMode == MENU) {
        button35_pressed = 1;
    } else if (currentMode == GAME) {
        dirction_index += 1;
        if (dirction_index > 3) {
            dirction_index = 0;
        }
    }
    
}

void setButtons() {// Sets the buttons and makes them negative edge triggered
    gpio_set_direction(0, GPIO_MODE_INPUT);
    gpio_set_intr_type(0, GPIO_INTR_NEGEDGE);
    gpio_get_level(0);
    gpio_set_direction(35, GPIO_MODE_INPUT);
    gpio_set_intr_type(35, GPIO_INTR_NEGEDGE);
    gpio_get_level(35);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(0, gpio0_isr_handler, (void*) 0);
    gpio_isr_handler_add(35, gpio35_isr_handler, (void*) 0);
}

void spawnBlock(Apple * apples) { // Is responsible for spawning the Apples in random positions
    for (int indx = 0; indx < capacity; indx++) {
        if (apples[indx].eaten == true || initial_start == false) {
            apples[indx].x = rand() % (display_width - 10);
            apples[indx].y = rand() % (display_height - 10);
            apples[indx].eaten = false;
        }
    }
    initial_start = true;
}


void gameOver() { // This is the game over screen 
    snprintf(buffer, 50, "Score: %d", score); // A specific high score is printed depending on the difficulty you were playing
    if (speed == 50) {
        snprintf(buffer1, 50, "HiScore: %d", high_score_e);
    } else if (speed == 75) {
        snprintf(buffer1, 50, "HiScore: %d", high_score_m);
    } else if (speed == 110) {
        snprintf(buffer1, 50, "HiScore: %d", high_score_h);
    }
    cls(rgbToColour(0, 0, 0));
    setFont(FONT_DEJAVU18);
    print_xy(buffer, CENTER, CENTER);
    print_xy(buffer1, CENTER, 130);
    flip_frame();


    clock_t startTime = clock();
    float delaySeconds = 2.0f;
    //set_start_pos();
    while ((float)(clock() - startTime) / CLOCKS_PER_SEC < delaySeconds) { 
        cls(rgbToColour(0, 0, 0));
        print_xy(buffer, CENTER, CENTER);
        print_xy(buffer1, CENTER, 90);
        flip_frame();
    }
    if (speed == 50 && score > high_score_e) { // Stores the different types of high scores from the dificulties 
        high_score_e = score;
        storage_write_int("highscore easy", score);
    } else if (speed == 75 && score > high_score_m) {
        high_score_m = score;
        storage_write_int("highscore med", score);
    } else if (speed == 110 && score > high_score_h) {
        high_score_h = score;
        storage_write_int("highscore hard", score);
    }

    exit(10);
}

void snake(float delta_time) { // This is responsible for the players movements
    float new_x = snake_x, new_y = snake_y;
    snprintf(buffer, 50, "Score: %d", score); // Displays the score and the high score associated with the selected difficulty
    if (speed == 50) {
        snprintf(buffer1, 50, "HiScore: %d", high_score_e);
    } else if (speed == 75) {
        snprintf(buffer1, 50, "HiScore: %d", high_score_m);
    } else if (speed == 110) {
        snprintf(buffer1, 50, "HiScore: %d", high_score_h);
    }
    print_xy(buffer, 10, 10);
    print_xy(buffer1, 10, 20);
    

    if (directions[dirction_index] == 'l') { // Check what direction the player is moving to change the axis
        new_x -= speed * delta_time;
    } else if (directions[dirction_index] == 'u') {
        new_y -= speed * delta_time;
    } else if (directions[dirction_index] == 'r') {
        new_x += speed * delta_time;
    } else if ((directions[dirction_index] == 'd')) {
        new_y += speed * delta_time;
    }

    move_snake(new_x, new_y);

    snakeSegment* current = tail;
    snakeSegment* head = tail; 

    while (head->prev != NULL) {
        head = head->prev;
    }

    while(current != NULL) { // Draws postion of the snake on screen
        draw_rectangle(current->x, current->y, 5, 5, rgbToColour(0, 255, 0));
        if (current != head) {
            if (speed == 110) {
                if (current->x + 3.2 > head->x && current->x < head->x && current->y + 3.2 > head->y && current->y < head->y) { // Checks if the snake touched one of its own segments
                    gameOver();
                }
            } else {
                if (current->x + 3 > head->x && current->x < head->x && current->y + 3 > head->y && current->y < head->y) { 
                    gameOver();
                }
            }
            
        }
        current = current->prev;
    }

    snake_x = new_x;
    snake_y = new_y;
}

void collision(Apple* apples) { // Checks for collions with apples to add points and segments. Also check for collisions with the walls
    for (int indx = 0; indx < capacity; indx++) {
        if (apples[indx].x + 10 > snake_x && apples[indx].x < snake_x + 5 && apples[indx].y + 10 > snake_y && apples[indx].y < snake_y + 5) {
            score += 10;
            add_segment();
            apples[indx].eaten = true;
        }
    }

    if ((snake_x + 5 > display_width || snake_x + 5 < 0) || (snake_y + 5 > display_height || snake_y + 5 < 0)) {
        gameOver();
    }

}

void random_seed() { // Creates a seed for the randomizer based on the time
    srand((unsigned int) time(NULL));
}

int menu(char * title, int nentries, char *entries[], int select) { // The menu which displays the options
    while(1) {
        cls(rgbToColour(50,50,50));
        setFont(FONT_DEJAVU18);
        draw_rectangle(0,3,display_width,24,rgbToColour(220,220,0));
        draw_rectangle(0,select*18+24+3,display_width,18,rgbToColour(0,180,180));
        setFontColour(0, 0, 0);
        print_xy(title, 10, 8);
        setFontColour(255, 255, 255);
        setFont(FONT_UBUNTU16);
        for(int i=0;i<nentries;i++) {
            print_xy(entries[i],10,LASTY+((i==0)?21:18));
        }
        if(get_orientation()) {
            print_xy("\x86",4,display_height-16); // down arrow 
            print_xy("\x90",display_width-16,display_height-16); // OK
        }
        if (currrentSelect == MODES) { // These check which menu the person is in to show the selected options
            if (speed == 50) {
                entries[0] = "[Easy]";
            } else if (speed == 75) {
                entries[1] = "[Medium]";
            } else if (speed == 110) {
                entries[2] = "[Hard]";
            }
        } else if (currrentSelect == SPAWNRATE) {
            if (capacity == 1) {
                entries[0] = "[Classic]";
            } else if (capacity == 5) {
                entries[1] = "[Few]";
            } else if (capacity == 10) {
                entries[2] = "[Many]";
            }
        }
        flip_frame();
        if (button0_pressed == 1) {
            select=(select+1)%nentries;
            button0_pressed = 0;
        }
        if (button35_pressed == 1) {
            button35_pressed = 0;
            return select;
        }
    }
}

int ins_menu(char * title, int nentries, char *entries[], int select) { // This is a menu specifically for the instructions
    while(1) {
        cls(rgbToColour(50,50,50));
        setFont(FONT_DEJAVU18);
        draw_rectangle(0,3,display_width,24,rgbToColour(220,220,0));
        draw_rectangle(0,display_height - 20,display_width,18,rgbToColour(0,180,180));
        
        setFontColour(0, 0, 0);
        print_xy(title, 10, 8);
        setFontColour(255, 255, 255);
        setFont(FONT_UBUNTU16);
        print_xy("Snake game: Use the left and", 10, 30);
        print_xy("right buttons to move. Eat the", 10, 45);
        print_xy("apples to grow the snake and", 10, 60);
        print_xy("get points. You cannot touch", 10, 75);
        print_xy("the walls or snake body.", 10, 90);
        print_xy(entries[0],10,display_height - 20);
        if(get_orientation()) {
            print_xy("\x86",4,display_height-16); // down arrow 
            print_xy("\x90",display_width-16,display_height-16); // OK
        }
        
        flip_frame();
        if (button0_pressed == 1) {
            select=(select+1)%nentries;
            button0_pressed = 0;
        }
        if (button35_pressed == 1) {
            button35_pressed = 0;
            return select;
        }
        
    }
    
}

void instructions() { // The instructions options
    int sel = 0;
    while (1) {
        char * entries[]={"Back"};
        sel=ins_menu("Instructions", sizeof(entries)/sizeof(char *), entries, sel);
        switch(sel) {
            case 0:
                return;
        }
    }
}

void difficulty() { // The difficulty options
    int sel = 0;
    while (1) {
        char * entries[]={"Easy", "Medium", "Hard", "Back"};
        sel=menu("Difficulty", sizeof(entries)/sizeof(char *), entries, sel);
        switch(sel) {
            case 0:
                speed = 50;
                break;
            case 1:
                speed = 75;
                break;
            case 2:
                speed = 110;
                break;
            case 3:
                currrentSelect = MAINMENU;
                return;
        }
    }
}

void apple_spawn_rate() { // The apple spawn rate options
    int sel = 0;
    while(1) {
        char * entries[] = {"Classic", "Few", "Many", "Back"};
        sel=menu("Spawn Rate", sizeof(entries)/sizeof(char *), entries, sel);
        switch(sel) {
            case 0:
                capacity = 1;
                break;
            case 1:
                capacity = 5;
                break;
            case 2:
                capacity = 10;
                break;
            case 3:
                currrentSelect = MAINMENU;
                return;
        }
    }
}

void selectors() { // The menu options
    int sel = 0;
    bool game = false;
    while (!game) {
        char *entries[] = {"Instructions", "Difficulty", "Apple Amount", "Start Game"};
        sel = menu("Game Options", sizeof(entries)/sizeof(char *), entries, sel);
        switch(sel) {
            case 0:
                instructions();
                break;
            case 1:
                currrentSelect = MODES;
                difficulty();
                break;
            case 2:
                currrentSelect = SPAWNRATE;
                apple_spawn_rate();
                break;
            case 3:
                currentMode = GAME;
                game = true;
                break;
        }
    }
}


void app_main() { // This is the main function which runs the program
    graphics_init();
    setButtons();
    random_seed();
    set_start_pos();
    volatile unsigned *GPIO_IN=(unsigned *)0x3FF4403C;
    volatile unsigned *GPIO_IN1=(unsigned *)0x3ff44040;
    
    high_score_e=storage_read_int("highscore easy",0);
    high_score_m=storage_read_int("highscore med",0);
    high_score_h=storage_read_int("highscore hard",0);
    selectors();
    setFont(FONT_SMALL);
    Apple* apples = malloc(capacity * sizeof(Apple));
    float time_delta = 1e-3f;
    tail = intialise_snake(snake_x, snake_y);
    uint64_t current_time, last_time;
    
    while(1) {
        cls(rgbToColour(0, 0, 0));
        last_time=esp_timer_get_time();
        while(1) {
            cls(rgbToColour(0, 0, 0));
            if (score == scoreLimit) { // Checks score and spawns an apple when a apple is eaten
                spawnBlock(apples);
                scoreLimit += 10;
            }
            for (int indx = 0; indx < capacity; indx++) { // visually shows the apples
                if (apples[indx].eaten == false) {
                    draw_rectangle(apples[indx].x, apples[indx].y, 10, 10, rgbToColour(255, 0, 0));
                }
            }
            snake(time_delta);
            collision(apples);
            flip_frame();
            current_time = esp_timer_get_time();
            float dt=(current_time - last_time)*1e-6f;
            time_delta = dt;
            last_time = current_time;
        }
    }
}