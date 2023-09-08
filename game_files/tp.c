#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>


const float FPS = 60;

const int SCREEN_W = 960;
const int SCREEN_H = 540;
const int display_scale = 2;

const int MAX_PILLARS = 10;
const int MAX_PLAYERS = 2;
const int MAX_SCORE = 999999;
const int SCORE_TYPES = 6;
const int SCORE_DISPLAYED = 5;
const int ground = 500;
const int score_factor = 60;


const unsigned char MENU_UP = ALLEGRO_KEY_UP;
const unsigned char MENU_LEFT = ALLEGRO_KEY_LEFT;
const unsigned char MENU_RIGHT = ALLEGRO_KEY_RIGHT;
const unsigned char MENU_DOWN = ALLEGRO_KEY_DOWN;
const unsigned char MENU_ACCEPT = ALLEGRO_KEY_SPACE;
const unsigned char MENU_BACK = ALLEGRO_KEY_ESCAPE;
const unsigned char GAME_UNPAUSE = ALLEGRO_KEY_P;



typedef enum {IDLE_UP, IDLE_LEFT, IDLE_RIGHT, MOVE_LEFT, MOVE_RIGHT, PILLAR} plstate;
typedef enum {INACTIVE, GREEN, YELLOW, RED} pillarstate;
typedef enum {LEFT, RIGHT} direction;
typedef enum {MAIN_MENU, START, OPTIONS, SCORE, PLAYING, GAME_OVER, DONE} state;
typedef enum {EASY, MEDIUM, HARD} difficulty;
typedef enum {MENU_START, MENU_OPTIONS, MENU_SCORE} main_menu_button;
typedef enum {CONTINUE, QUIT} game_over_button;
typedef enum {OPTIONS_LEFT, OPTIONS_RIGHT, OPTIONS_ACTION, OPTIONS_RESET} options_button;
typedef enum {P1_SOLO, P2_SOLO, MULTI} start_possibility;
typedef enum {_1P = 1, _2P} mode;
typedef struct Node{
    int score;
    struct Node *next;
} Node;

ALLEGRO_EVENT ev;


//-------------[GAME OBJECTS]-------------//

typedef struct{
    int active;
    float x, y, fall_speed;

    ALLEGRO_BITMAP *sprite;

    int sprite_w, sprite_h, w, h;

    int radius, min_catch_height;

} plateobj;

typedef struct{
    int id, started, player_action;

    float x, y;
    float max_timer, timer;

    pillarstate cur;

    float radius;

    ALLEGRO_BITMAP *sprite;
    ALLEGRO_BITMAP *glow_sprite;
    int circle_timer, circle_frames, frames_per_circle_frame;

    int sprite_w, sprite_h, w, h;

    plateobj plate;

} pillarobj;

typedef struct{
    int id;
    float x, y, movespeed;

    plstate cur;
    direction dir;

    pillarobj *pillar;

    ALLEGRO_BITMAP *sprite;
    int run_timer, run_frames, frames_per_run_frame;
    int sprite_w, sprite_h;
    int w, h;


    int MIN_X, MAX_X;

    int pillar_update;

    ALLEGRO_BITMAP *shadow;


    unsigned char key_left, key_right, key_action;

} playerobj;

//----------------------------------------//





//-------------[MISCELLANEOUS]-------------//

void errmsg(const char * err){
    fprintf(stderr, "Failed to initialize %s\n", err);
    exit(1);
}

void file_read_error(){
    fprintf(stderr, "Failed to read file\n");
    exit(1);
}

void file_write_error(){
    fprintf(stderr, "Failed to write to file\n");
    exit(1);
}

void start_key(unsigned char key[]){
    memset(key, 0, ALLEGRO_KEY_MAX);
}

void invert_string(char str[]){
    int sz = strlen(str);

    int i, j = sz - 1;
    for (i = 0; i < sz/2; ++i){
        char temp = str[j];
        str[j] = str[i];
        str[i] = temp;
        --j;
    }
}

void itostring(int num, char str[]){
    if (num > MAX_SCORE) return;
    int c = 0;
    while (num > 0){
        str[c] = (num%10) + '0';
        c++;
        num/=10;
    }
    str[c] = '\0';
    invert_string(str);

}

//-----------------------------------------//





//-------------[SCOREBOARD]-------------//

Node *init_node(){
    Node *newnode = (Node *) malloc(sizeof(Node));
    newnode->score = 0;
    newnode->next = NULL;
    return newnode;
}

void init_scoreboard(Node **board){
    FILE *arq = fopen("score.bin", "rb");
    if (!arq) errmsg("arquivo de score");

    int t;
    for (t = 0; t < SCORE_TYPES; ++t){
        Node *cur = init_node();
        if (fread(&cur->score, sizeof(int), 1, arq) != 1) file_read_error();
        board[t] = cur;

        int i;
        for (i = 0; i < SCORE_DISPLAYED - 1; ++i){
            Node *newnode = init_node();
            if (fread(&newnode->score, sizeof(int), 1, arq) != 1) file_read_error();
            cur->next = newnode;
            cur = newnode;
        }
    }

    fclose(arq);

}

int insert_score(Node **head, int score){
    Node *cur = *head;

    while (cur->next != NULL && cur->next->score < score){
        cur = cur->next;
    }

    if (cur->score > score) return 0;

    Node *newnode = init_node();
    newnode->score = score;

    Node *aux1 = cur->next;
    cur->next = newnode;
    newnode->next = aux1;

    Node *aux2 = *head;
    *head = (*head)->next;
    free(aux2);

    if (newnode->next == NULL) return 1;
    return 0;

}

void save_scoreboard(Node **board){
    FILE *arq = fopen("score.bin", "wb");
    if (!arq) errmsg("arquivo de score");

    int t;
    for (t = 0; t < SCORE_TYPES; ++t){
        Node *cur = board[t];

        int i;
        for (i = 0; i < SCORE_DISPLAYED; ++i){
            Node *next = cur->next;
            if(fwrite(&cur->score, sizeof(int), 1, arq) != 1) file_write_error();
            cur = next;
        }

    }

    fclose(arq);
}

void display_score(ALLEGRO_FONT *font, Node *head, int dif){
    int x = 98, y = 220 + 32*5;

    x += dif * 300;


    Node *cur = head;

    int i;
    for (i = SCORE_DISPLAYED; i > 0; --i){
        al_draw_textf(font, al_map_rgb_f(1,1,1), x, y, 0, "%d. %d", i, cur->score);
        cur = cur->next;
        y-=32;
    }

}

void destroy_scoreboard(Node **board){
    int t;
    for (t = 0; t < SCORE_TYPES; ++t){
        Node *cur = board[t];

        int i;
        for (i = 0; i < SCORE_DISPLAYED; ++i){
            Node *next = cur->next;
            free(cur);
            cur = next;
        }

    }

}

//--------------------------------------//





//-------------[INITIALISING GAME OBJECTS]-------------//

void plate_init(plateobj *plate, pillarobj *pillar, difficulty dif){
    switch (dif){
        case EASY:
            plate->fall_speed = 3;
            break;
        case MEDIUM:
            plate->fall_speed = 3;
            break;
        case HARD:
            plate->fall_speed = 5;
            break;
    }


    plate->active = 0;

    plate->x = pillar->x;
    plate->y = 170;

    plate->sprite = al_load_bitmap("plate.png");
    if (!plate->sprite) errmsg("plate sprite");

    plate->sprite_w = 32;
    plate->sprite_h = 32;
    plate->w = plate->sprite_w * display_scale;
    plate->h = plate->sprite_h * display_scale;

    plate->radius = 30;
    plate->min_catch_height = 60;


}

void pillar_init(pillarobj *pillar, int id, difficulty dif){
    switch (dif){
        case EASY:
            pillar->max_timer = 2000;
            break;
        case MEDIUM:
            pillar->max_timer = 1500;
            break;
        case HARD:
            pillar->max_timer = 1000;
            break;
    }


    pillar->id = id;
    pillar->started = 0;
    pillar->player_action = 0;

    pillar->x = 66 + 92 * id;
    pillar->y = 20;

    pillar->timer = pillar->max_timer;
    pillar->cur = INACTIVE;

    pillar->radius = 10;

    pillar->sprite = al_load_bitmap("pillarimg.png");
    if (!pillar->sprite) errmsg("pillar sprite");

    pillar->glow_sprite = al_load_bitmap("pillar_glow.png");
    if (!pillar->glow_sprite) errmsg("pillar glow sprite");

    pillar->circle_timer = 0;
    pillar->circle_frames = 8;
    pillar->frames_per_circle_frame = 4;

    pillar->sprite_w = 32;
    pillar->sprite_h = 200;
    pillar->w = pillar->sprite_w * display_scale;
    pillar->h = pillar->sprite_h * display_scale;

    plateobj plate;
    plate_init(&plate, pillar, dif);
    pillar->plate = plate;


}

void pillar_list_init(pillarobj *pillar_list, difficulty dif){
    int id;
    for (id = 0; id < MAX_PILLARS; ++id){
        pillarobj pillar;
        pillar_init(&pillar, id, dif);
        pillar_list[id] = pillar;
    }

}

void player_init(playerobj *player, playerobj temp){
    *player = temp;
}

void player_list_init (playerobj* player_list, playerobj template1, playerobj template2){
    playerobj player1, player2;
    player_init(&player1, template1);
    player_init(&player2, template2);
    player_list[0] = player1;
    player_list[1] = player2;
}

//-----------------------------------------------------//





//-------------[DRAWING GAME OBJECTS]-------------//

void draw_pillar_and_plate(pillarobj *pillar){
    if (pillar->player_action){
        al_draw_scaled_bitmap(pillar->glow_sprite, 0, 0, pillar->sprite_w, pillar->sprite_h, pillar->x - pillar->w/2, pillar->y, pillar->w, pillar->h, 0);
    }


    int anim_frame = pillar->circle_timer/pillar->frames_per_circle_frame;

    int apos, bpos;
    switch (pillar->cur){
        case INACTIVE:
            apos = 0; bpos = 3;
            break;
        case GREEN:
            apos = anim_frame; bpos = 0;
            break;
        case YELLOW:
            apos = anim_frame; bpos = 1;
            break;
        case RED:
            apos = anim_frame; bpos = 2;
            break;
    }

    al_draw_scaled_bitmap(pillar->sprite, pillar->sprite_w * apos, pillar->sprite_h * bpos, pillar->sprite_w, pillar->sprite_h, pillar->x - pillar->w/2, pillar->y, pillar->w, pillar->h, 0);

    if (pillar->plate.active){
        plateobj *plate = &pillar->plate;
        al_draw_scaled_bitmap(plate->sprite, 0, 0, 32, 32, plate->x - plate->w/2, plate->y - plate->h/2 - 5, 64 ,64 , 0);
    }
}

void draw_player(playerobj *player){
    int anim_frame = player->run_timer/player->frames_per_run_frame;
    int apos, bpos;

    switch (player->cur){
        case IDLE_RIGHT:
            apos = 0; bpos = 0;
            break;

        case IDLE_LEFT:
            apos = 1; bpos = 0;
            break;

        case MOVE_RIGHT:
            apos = anim_frame; bpos = 1;
            break;

        case MOVE_LEFT:
            apos = anim_frame; bpos = 2;
            break;

        case PILLAR:
            apos = (player->pillar) ? 4 : 3;
            bpos = 0;
            break;
    }

    al_draw_scaled_bitmap(player->shadow, 0, 0, 16, 16, player->x - 16, player->y - 20, 32, 32, 0);
    al_draw_scaled_bitmap(player->sprite, player->sprite_w * apos, player->sprite_h * bpos, player->sprite_w, player->sprite_h, player->x - player->w/2, player->y - player->h, player->w, player->h, 0);
}

void draw_playscore(ALLEGRO_FONT *font, int game_time){
    al_draw_textf(font, al_map_rgb_f(1,1,1), 1, 1, 0, "%06ld", game_time/score_factor);
}

void display_pause_message(ALLEGRO_FONT *font){
    int w = al_get_text_width(font, "PRESS P TO UNPAUSE");
    al_draw_text(font, al_map_rgb_f(1,1,1), SCREEN_W/2 - w/2, 20, 0, "PRESS P TO UNPAUSE");
}

//------------------------------------------------//





//-------------[DESTROYING GAME OBJECTS]-------------//

void destroy_plate(plateobj *plate){
    al_destroy_bitmap(plate->sprite);
}

void destroy_pillar(pillarobj *pillar){
    destroy_plate(&pillar->plate);
    al_destroy_bitmap(pillar->sprite);
    al_destroy_bitmap(pillar->glow_sprite);
}

void destroy_play_objects(pillarobj *pillar_list){
    int id;
    for (id = 0; id < MAX_PILLARS; ++id) destroy_pillar(&pillar_list[id]);
}

//---------------------------------------------------//





//-------------[PLAY STATE FUNCTIONS]--------------//

void start_pillars(pillarobj *pillar_list, int *id, int *start_time, int game_time, int *n){
    int sec = game_time/60;

    if (sec >= start_time[*n]){
        pillarobj *pillar = &pillar_list[id[*n]];
        pillar->started = 1;
        *n += 1;
    }

}

pillarobj *is_contact_plate(playerobj *player, pillarobj *pillar_list){
    int i;
    for (i = 0; i < MAX_PILLARS; ++i){
        pillarobj *pillar = &pillar_list[i];
        if (!pillar->started) continue;
        plateobj plate = pillar->plate;
        if (!plate.active) continue;
        if (fabs(player->x - plate.x) <= plate.radius && plate.y > ground - plate.min_catch_height) return pillar;
    }
    return NULL;
}

void reset_falling_plate(playerobj *player, pillarobj *pillar_list, difficulty dif){
    pillarobj *plate_pillar = is_contact_plate(player, pillar_list);
    if (plate_pillar){
        plateobj *plate = &plate_pillar->plate;
        destroy_plate(plate);
        plate_init(plate, plate_pillar, dif);
        plate_pillar->cur = RED;
        plate_pillar->timer = plate_pillar->max_timer/4;
    }
}

pillarobj *is_under_pillar(playerobj *player, pillarobj *pillar_list){
    int i;
    for (i = 0; i < MAX_PILLARS; ++i){
        pillarobj *pillar = &pillar_list[i];
        if (!pillar->started) continue;
        if (pillar->cur == INACTIVE || pillar->player_action) continue;
        if (fabs(player->x - pillar->x) <= pillar->radius) return pillar;
    }
    return NULL;
}

int is_game_over(pillarobj *pillar_list){
    int id;
    for (id = 0; id < MAX_PILLARS; ++id){
        pillarobj *pillar = &pillar_list[id];
        if (!pillar->started) continue;
        if (pillar->plate.active && pillar->plate.y >= ground) return 1;
    }
    return 0;
}

//--------------------------------------------------------//





//-------------[UPDATING GAME OBJECTS]-------------//

void update_plate(plateobj *plate){
    plate->y += plate->fall_speed;
}

void update_pillar_timer(playerobj *player, pillarobj *pillar){
    pillar->timer += player->pillar_update;
    pillar->timer = (pillar->timer < pillar->max_timer) ? pillar->timer : pillar->max_timer;
}

void update_player(playerobj *player, unsigned char *key, pillarobj *pillar_list, int *draw_priority){
    float move_dist = 0;

    if (! (key[player->key_left] && key[player->key_right]) ){
        if (key[player->key_left]) move_dist -= player->movespeed;
        else if (key[player->key_right]) move_dist += player->movespeed;
    }


    if (key[player->key_action] && move_dist == 0){

        if (player->pillar) update_pillar_timer(player, player->pillar);
        else{
            player->cur = PILLAR;
            pillarobj *pillar = is_under_pillar(player, pillar_list);
            if (pillar){
                pillar->player_action = 1;
                player->pillar = pillar;
                update_pillar_timer(player, pillar);
            }

        }

    } else if (player->pillar){
        (player->pillar)->player_action = 0;
        player->pillar = NULL;
    }



    if (!key[player->key_action] && move_dist == 0){

        if (player->dir == RIGHT) player->cur = IDLE_RIGHT;
        else player->cur = IDLE_LEFT;

    } else if (move_dist > 0){

        if (player->cur != MOVE_RIGHT){
            *draw_priority = player->id;
            player->run_timer = 0;
            player->dir = RIGHT;
            player->cur = MOVE_RIGHT;
        } else player->run_timer++;

    } else if (move_dist < 0) {

        if (player->cur != MOVE_LEFT){
            *draw_priority = player->id;
            player->run_timer = 0;
            player->dir = LEFT;
            player->cur = MOVE_LEFT;
        } else player->run_timer++;

    }

    player->x += move_dist;
    player->x = (player->x >= player->MIN_X) ? player->x : player->MIN_X;
    player->x = (player->x <= player->MAX_X) ? player->x : player->MAX_X;


    if (player->run_timer >= player->run_frames * player->frames_per_run_frame) player->run_timer = 0;


}

void update_pillar(pillarobj *pillar){
    if (!pillar->started) return;

    if (pillar->plate.active == 1){
        update_plate(&pillar->plate);
        return;
    }

    if (pillar->timer == 0){
        pillar->cur = INACTIVE;
        pillar->plate.active = 1;
        return;
    }

    if (pillar->timer > pillar->max_timer/2) pillar->cur = GREEN;
    else if (pillar->timer > pillar->max_timer/4) pillar->cur = YELLOW;
    else pillar->cur = RED;

    if (pillar->cur != INACTIVE && !(pillar->player_action)){
        (pillar->timer)--;
        (pillar->circle_timer)++;
    }

    if (pillar->circle_timer >= pillar->circle_frames * pillar->frames_per_circle_frame) pillar->circle_timer = 0;


}

void update_game(playerobj *player_list, int md, unsigned char *key, pillarobj *pillar_list, difficulty dif, int *draw_priority){
    int id;

    for (id = 0; id < md; ++id){
        playerobj *player = &player_list[id];
        update_player(player, key, pillar_list, draw_priority);
        reset_falling_plate(player, pillar_list, dif);
    }

    for (id = 0; id < MAX_PILLARS; ++id){
        update_pillar(&pillar_list[id]);
    }

}

//-------------------------------------------------//





//-------------[PLAYER TEMPLATE FUNCTIONS]-------------//

void player_template1_init(playerobj *player){
    player->id = 1;

    player->x = 200;
    player->y = ground;
    player->movespeed = 4;

    player->cur = IDLE_RIGHT;
    player->dir = RIGHT;

    player->pillar = NULL;

    player->sprite = al_load_bitmap("playersheet1.png");
    if (!player->sprite) errmsg("player1 sprite");

    player->run_timer = 0;
    player->run_frames = 6;
    player->frames_per_run_frame = 6;

    player->sprite_w = 64;
    player->sprite_h = 64;

    player->w = player->sprite_w*display_scale;
    player->h = player->sprite_h*display_scale;

    player->MAX_X = SCREEN_W - player->w/2;
    player->MIN_X = player->w/2;

    player->pillar_update = 4;

    player->shadow = al_load_bitmap("shadow.png");
    if (!player->shadow) errmsg("player shadow sprite");

    player->key_left = ALLEGRO_KEY_LEFT;
    player->key_right = ALLEGRO_KEY_RIGHT;
    player->key_action = ALLEGRO_KEY_DOWN;
}

void player_template2_init(playerobj *player){
    player->id = 2;

    player->x = 760;
    player->y = ground;
    player->movespeed = 3;

    player->cur = IDLE_LEFT;
    player->dir = LEFT;

    player->pillar = NULL;

    player->sprite = al_load_bitmap("playersheet2.png");
    if (!player->sprite) errmsg("player2 sprite");

    player->run_timer = 0;
    player->run_frames = 6;
    player->frames_per_run_frame = 6;

    player->sprite_w = 64;
    player->sprite_h = 64;

    player->w = player->sprite_w*display_scale;
    player->h = player->sprite_h*display_scale;

    player->MAX_X = SCREEN_W - player->w/2;
    player->MIN_X = player->w/2;

    player->pillar_update = 8;

    player->shadow = al_load_bitmap("shadow.png");
    if (!player->shadow) errmsg("player shadow sprite");

    player->key_left = ALLEGRO_KEY_A;
    player->key_right = ALLEGRO_KEY_D;
    player->key_action = ALLEGRO_KEY_S;
}

void change_temp_control(playerobj *temp, options_button bt, unsigned char key){
    switch (bt){
        case OPTIONS_LEFT: temp->key_left = key; break;
        case OPTIONS_RIGHT: temp->key_right = key; break;
        case OPTIONS_ACTION: temp->key_action = key; break;
    }
}

//-----------------------------------------------------//





//-------------[DRAWING GENERAL OBJECTS]-------------//

void draw_background(ALLEGRO_BITMAP *background){
    al_draw_scaled_bitmap(background, 0, 0, SCREEN_W/2, SCREEN_H/2, 0, 0, SCREEN_W , SCREEN_H , 0);
}

void draw_mainmenu_UI(ALLEGRO_BITMAP *menu_start, ALLEGRO_BITMAP *menu_options, ALLEGRO_BITMAP *menu_score, main_menu_button bt){
    int ystart = 240, selectstart = 0;
    int yoptions = 330, selectoptions = 0;
    int yscore = 420, selectscore = 0;

    int x = 330;
    int ogwidth = 150;
    int ogheight = 40;

    switch (bt){
        case MENU_START: selectstart = 1; break;
        case MENU_OPTIONS: selectoptions = 1; break;
        case MENU_SCORE: selectscore = 1; break;
    }



    al_draw_scaled_bitmap(menu_start, ogwidth * selectstart, 0, ogwidth, ogheight, x, ystart, ogwidth * display_scale, ogheight * display_scale, 0);
    al_draw_scaled_bitmap(menu_options, ogwidth * selectoptions, 0, ogwidth, ogheight, x, yoptions, ogwidth * display_scale, ogheight * display_scale, 0);
    al_draw_scaled_bitmap(menu_score, ogwidth * selectscore, 0, ogwidth, ogheight, x, yscore, ogwidth * display_scale, ogheight * display_scale, 0);
}

void draw_mainmenu_title(ALLEGRO_BITMAP *menu_title){
    al_draw_scaled_bitmap(menu_title, 0, 0, 250, 120, 228, 16, 500, 240, 0);
}

void draw_scoremenu_mode(ALLEGRO_BITMAP *score_mode, mode md){
    int t = !(md == 1);
    al_draw_scaled_bitmap(score_mode, t * 220, 0, 220, 40, 260, 16, 440, 80, 0);
}

void draw_scoremenu_dif(ALLEGRO_BITMAP *score_dif){
    al_draw_scaled_bitmap(score_dif, 0, 0, 480, 40, 0, 116, 960, 80, 0);
}

void draw_optionsmenu_headings(ALLEGRO_BITMAP *options_playerone, ALLEGRO_BITMAP *options_playertwo){
    al_draw_scaled_bitmap(options_playerone, 0, 0, 180, 40, 64, 50, 360, 80, 0);
    al_draw_scaled_bitmap(options_playertwo, 0, 0, 180, 40, 536, 50, 360, 80, 0);
}

void draw_optionsmenu_UI(ALLEGRO_BITMAP *options_left, ALLEGRO_BITMAP *options_right, ALLEGRO_BITMAP *options_action, ALLEGRO_BITMAP *options_reset, mode md, options_button bt){
    int yleft = 160, selectleft = 0;
    int yright = 250, selectright = 0;
    int yaction = 340, selectaction = 0;
    int yreset = 430, selectreset = 0;

    int x = 94;
    int xdist = 472;
    int ogwidth = 150;
    int ogheight = 40;

    switch (bt){
        case OPTIONS_LEFT: selectleft = 1; break;
        case OPTIONS_RIGHT: selectright = 1; break;
        case OPTIONS_ACTION: selectaction = 1; break;
        case OPTIONS_RESET: selectreset= 1; break;
    }


    int i;
    for (i = 1; i <= MAX_PLAYERS; ++i){
        al_draw_scaled_bitmap(options_left, ogwidth * selectleft * ( (int) md == i), 0, ogwidth, ogheight, x + (i-1) * xdist, yleft, ogwidth * display_scale, ogheight * display_scale, 0);
        al_draw_scaled_bitmap(options_right, ogwidth * selectright * ( (int) md == i), 0, ogwidth, ogheight, x + (i-1) * xdist, yright, ogwidth * display_scale, ogheight * display_scale, 0);
        al_draw_scaled_bitmap(options_action, ogwidth * selectaction * ( (int) md == i), 0, ogwidth, ogheight, x + (i-1) * xdist, yaction, ogwidth * display_scale, ogheight * display_scale, 0);
        al_draw_scaled_bitmap(options_reset, ogwidth * selectreset * ( (int) md == i), 0, ogwidth, ogheight, x + (i-1) * xdist, yreset, ogwidth * display_scale, ogheight * display_scale, 0);
    }

}

void draw_optionsmenu_prompt(ALLEGRO_FONT *font){
    int w = al_get_text_width(font, "PRESS ANY KEY");
    al_draw_text(font, al_map_rgb_f(1,1,1), SCREEN_W/2 - w/2, 150, 0, "PRESS ANY KEY");
}

void draw_startmenu_dif(ALLEGRO_BITMAP *start_easy, ALLEGRO_BITMAP *start_medium, ALLEGRO_BITMAP *start_hard, difficulty dif){
    int xeasy = 30, selecteasy = 0;
    int xmedium = 330, selectmedium = 0;
    int xhard = 630, selecthard = 0;

    int y = 30;
    int ogwidth = 150;
    int ogheight = 40;

    switch (dif){
        case EASY: selecteasy = 1; break;
        case MEDIUM: selectmedium = 1; break;
        case HARD: selecthard = 1; break;
    }


    al_draw_scaled_bitmap(start_easy, ogwidth * selecteasy, 0, ogwidth, ogheight, xeasy, y, ogwidth * display_scale, ogheight * display_scale, 0);
    al_draw_scaled_bitmap(start_medium, ogwidth * selectmedium, 0, ogwidth, ogheight, xmedium, y, ogwidth * display_scale, ogheight * display_scale, 0);
    al_draw_scaled_bitmap(start_hard, ogwidth * selecthard, 0, ogwidth, ogheight, xhard, y, ogwidth * display_scale, ogheight * display_scale, 0);
}

void draw_startmenu_playerselect(ALLEGRO_BITMAP *start_mode, ALLEGRO_BITMAP *start_playerbox, playerobj template1, playerobj template2, mode md, start_possibility pos){
    int t = !(md == 1);
    al_draw_scaled_bitmap(start_mode, t * 220, 0, 220, 40, 260, 192, 440, 80, 0);

    if (pos == MULTI){
        al_draw_scaled_bitmap(start_playerbox, 0, 0, 80, 80, 298, 272, 160, 160, 0);
        al_draw_scaled_bitmap(start_playerbox, 0, 0, 80, 80, 502, 272, 160, 160, 0);
        al_draw_scaled_bitmap(template1.sprite, 128, 0, 64, 64, 314, 272, 128, 128, 0);
        al_draw_scaled_bitmap(template2.sprite, 128, 0, 64, 64, 518, 272, 128, 128, 0);

    }

    else {
        al_draw_scaled_bitmap(start_playerbox, 0, 0, 80, 80, 400, 272, 160, 160, 0);
        switch (pos){
            case P1_SOLO:
                al_draw_scaled_bitmap(template1.sprite, 128, 0, 64, 64, 416, 272, 128, 128, 0);
                break;
            case P2_SOLO:
                al_draw_scaled_bitmap(template2.sprite, 128, 0, 64, 64, 416, 272, 128, 128, 0);
                break;
        }

    }
}

void draw_gameover_score(ALLEGRO_FONT *font, ALLEGRO_BITMAP *gameover_score, int score, int is_high_score){
    al_draw_scaled_bitmap(gameover_score, 0, 0, 150, 40, 330, 20, 300, 80, 0);

    char *str = (char *) malloc(7 * sizeof(char));

    itostring(score, str);
    int w1 = al_get_text_width(font, str);
    int w2 = al_get_text_width(font, "NEW HIGH SCORE");
    int x = SCREEN_W/2;

    al_draw_textf(font, al_map_rgb_f(1,1,1), x - w1/2, 120, 0, "%d", score);
    if (is_high_score) al_draw_text(font, al_map_rgb_f(1,1,1), x - w2/2, 150, 0, "NEW HIGH SCORE");

    free(str);
}

void draw_gameover_UI(ALLEGRO_BITMAP *gameover_continue, ALLEGRO_BITMAP *gameover_quit, game_over_button bt){
    int ycontinue = 260, selectcontinue = 0;
    int yquit = 350, selectquit = 0;

    int x = 330;
    int ogwidth = 150;
    int ogheight = 40;

    switch (bt){
        case CONTINUE: selectcontinue = 1; break;
        case QUIT: selectquit = 1; break;
    }

    al_draw_scaled_bitmap(gameover_continue, ogwidth * selectcontinue, 0, ogwidth, ogheight, x, ycontinue, ogwidth * display_scale, ogheight * display_scale, 0);
    al_draw_scaled_bitmap(gameover_quit, ogwidth * selectquit, 0, ogwidth, ogheight, x, yquit, ogwidth * display_scale, ogheight * display_scale, 0);
}

//---------------------------------------------------//





//-------------[DESTROYING GENERAL OBJECTS]-------------//

void destroy_image(ALLEGRO_BITMAP *image){
    al_destroy_bitmap(image);
}

void destroy_player_temp(playerobj *player_temp){
    al_destroy_bitmap(player_temp->sprite);
    al_destroy_bitmap(player_temp->shadow);
}

void destroy_mainmenu(ALLEGRO_BITMAP *menu_start, ALLEGRO_BITMAP *menu_options, ALLEGRO_BITMAP *menu_score, ALLEGRO_BITMAP *menu_title){
    destroy_image(menu_start);
    destroy_image(menu_options);
    destroy_image(menu_score);
    destroy_image(menu_title);
}

void destroy_scoremenu(ALLEGRO_BITMAP *score_mode, ALLEGRO_BITMAP *score_dif){
    destroy_image(score_mode);
    destroy_image(score_dif);
}

void destroy_optionsmenu(ALLEGRO_BITMAP *options_playerone, ALLEGRO_BITMAP *options_playertwo, ALLEGRO_BITMAP *options_left, ALLEGRO_BITMAP *options_right, ALLEGRO_BITMAP *options_action, ALLEGRO_BITMAP *options_reset){
    destroy_image(options_playerone);
    destroy_image(options_playertwo);
    destroy_image(options_left);
    destroy_image(options_right);
    destroy_image(options_action);
    destroy_image(options_reset);
}

void destroy_startmenu(ALLEGRO_BITMAP *start_easy, ALLEGRO_BITMAP *start_medium, ALLEGRO_BITMAP *start_hard, ALLEGRO_BITMAP *start_mode, ALLEGRO_BITMAP *start_playerbox){
    destroy_image(start_easy);
    destroy_image(start_medium);
    destroy_image(start_hard);
    destroy_image(start_mode);
    destroy_image(start_playerbox);
}

void destroy_gameovermenu(ALLEGRO_BITMAP *gameover_score, ALLEGRO_BITMAP *gameover_continue, ALLEGRO_BITMAP *gameover_quit){
    destroy_image(gameover_score);
    destroy_image(gameover_continue);
    destroy_image(gameover_quit);
}

//------------------------------------------------------//





//---------------[GENGERAL STATE FUNCTIONS]---------------//

state main_menu(ALLEGRO_EVENT_QUEUE *queue, ALLEGRO_FONT *font, ALLEGRO_BITMAP *background){
    ALLEGRO_BITMAP *menu_start = al_load_bitmap("menu_start.png");
    if (!menu_start) errmsg("menu start UI image");

    ALLEGRO_BITMAP *menu_options = al_load_bitmap("menu_options.png");
    if (!menu_options) errmsg("menu options UI image");

    ALLEGRO_BITMAP *menu_score = al_load_bitmap("menu_score.png");
    if (!menu_score) errmsg("menu score UI image");

    ALLEGRO_BITMAP *menu_title = al_load_bitmap("title.png");
    if (!menu_title) errmsg("menu title image");

    int stop = 0, redraw = 0;
    unsigned char cur_key;

    main_menu_button bt = MENU_START;
    state return_state = DONE;

    while (1){
        al_wait_for_event(queue, &ev);
        switch(ev.type){

            case ALLEGRO_EVENT_TIMER:
                redraw = 1;
                break;

            case ALLEGRO_EVENT_KEY_DOWN:

                cur_key = ev.keyboard.keycode;

                if (cur_key == MENU_DOWN){
                    bt = (bt + 1) % 3;
                }

                else if (cur_key == MENU_UP){
                    bt = (3 + (bt - 1)) % 3;
                }

                else if (cur_key == MENU_ACCEPT){
                    stop = 1;
                    switch (bt){
                        case MENU_START: return_state = START; break;
                        case MENU_OPTIONS: return_state = OPTIONS; break;
                        case MENU_SCORE: return_state = SCORE; break;
                    }
                }

                break;

            case ALLEGRO_EVENT_DISPLAY_CLOSE:
                stop = 1;
                return_state = DONE;
                break;
        }

        if (stop){
            destroy_mainmenu(menu_start, menu_options, menu_score, menu_title);
            return return_state;
        }

        if (redraw && al_is_event_queue_empty(queue)){
            al_clear_to_color(al_map_rgb(0, 0, 0));
            draw_background(background);
            draw_mainmenu_title(menu_title);
            draw_mainmenu_UI(menu_start, menu_options, menu_score, bt);
            al_flip_display();
            redraw = 0;
        }
    }

}

state play(ALLEGRO_EVENT_QUEUE *queue, ALLEGRO_FONT *font, ALLEGRO_BITMAP *background, unsigned char key[], int md, playerobj template1, playerobj template2, difficulty dif, int id_order[], int spawn_time[][10], pillarobj *pillar_list, playerobj *player_list, int *last_score){

    start_key(key);

    player_list_init(player_list, template1, template2);

    int started_pillars = 0;
    int id;
    pillar_list_init(pillar_list, dif);

    int stop = 0, redraw = 0, paused = 0, game_over = 0;
    int game_time = 0;

    state return_state = DONE;
    int draw_priority = template1.id;

    ALLEGRO_KEYBOARD_STATE ks;

    while (1){
        al_wait_for_event(queue, &ev);

        switch(ev.type){

            case ALLEGRO_EVENT_TIMER:
                redraw = 1;

                if (!paused){
                    if (is_game_over(pillar_list)){
                        stop = 1;
                        return_state = GAME_OVER;
                        break;
                    }

                    game_time++;

                    if (started_pillars < MAX_PILLARS) start_pillars(pillar_list, id_order, spawn_time[dif], game_time, &started_pillars);
                    update_game(player_list, md, key, pillar_list, dif, &draw_priority);
                }

                int i;
                for (i = 0; i < ALLEGRO_KEY_MAX; ++i) key[i] &= 2;

                break;


            case ALLEGRO_EVENT_KEY_DOWN:

                if (paused && ev.keyboard.keycode == GAME_UNPAUSE){
                    al_get_keyboard_state(&ks);

                    int i;
                    for (i = 0; i < ALLEGRO_KEY_MAX; ++i){
                        if (al_key_down(&ks, i)) key[i] = 2;
                        else key[i] = 0;
                    }
                    paused = 0;
                }

                key[ev.keyboard.keycode] = 3;
                break;

            case ALLEGRO_EVENT_KEY_UP:

                if (!paused || !(ev.keyboard.keycode == GAME_UNPAUSE)) key[ev.keyboard.keycode] = 1;
                break;

            case ALLEGRO_EVENT_DISPLAY_SWITCH_OUT:
                paused = 1;
                break;

            case ALLEGRO_EVENT_DISPLAY_CLOSE:
                stop = 1;
                return_state = DONE;
                break;
        }

        if (stop || game_time > score_factor * MAX_SCORE){
            int score = game_time/score_factor;
            *last_score = (score < MAX_SCORE) ? score : MAX_SCORE;
            destroy_play_objects(pillar_list);

            return return_state;
        }


        if (redraw && al_is_event_queue_empty(queue)){
            al_clear_to_color(al_map_rgb(0, 0, 0));

            draw_background(background);
            if (draw_priority == 1){
                for (id = md - 1; id >= 0; --id){
                    draw_player(&player_list[id]);
                }
            }
            else {
                for (id = 0; id < md; ++id){
                    draw_player(&player_list[id]);
                }
            }

            for (id = 0; id < MAX_PILLARS; ++id){
                draw_pillar_and_plate(&pillar_list[id]);
            }

            draw_playscore(font, game_time);

            if (paused) display_pause_message(font);

            al_flip_display();

            redraw = 0;
        }


    }
}

state score(ALLEGRO_EVENT_QUEUE *queue, ALLEGRO_FONT *font, ALLEGRO_BITMAP *background, Node **scoreboard){
    mode cur_md = _1P;

    ALLEGRO_BITMAP *score_mode = al_load_bitmap("mode.png");
    if (!score_mode) errmsg("score mode UI image");

    ALLEGRO_BITMAP *score_dif = al_load_bitmap("score_dif.png");
    if (!score_dif) errmsg("score difficulty UI image");

    int stop = 0, redraw = 0;
    state return_state = DONE;

    unsigned char cur_key;

    while (1){
        al_wait_for_event(queue, &ev);
        switch(ev.type){

            case ALLEGRO_EVENT_TIMER:
                redraw = 1;
                break;

            case ALLEGRO_EVENT_KEY_DOWN:
                cur_key = ev.keyboard.keycode;

                if (cur_key == MENU_LEFT || cur_key == MENU_RIGHT){
                    cur_md = (!(cur_md - 1)) + 1;
                }

                 else if (cur_key == MENU_BACK){
                    stop = 1;
                    return_state = MAIN_MENU;
                }
                break;

            case ALLEGRO_EVENT_DISPLAY_CLOSE:
                stop = 1;
                return_state = DONE;
                break;
        }

        if (stop){
            destroy_scoremenu(score_mode, score_dif);
            return return_state;
        }

        if (redraw && al_is_event_queue_empty(queue)){
            al_clear_to_color(al_map_rgb(0, 0, 0));
            draw_background(background);
            draw_scoremenu_mode(score_mode, cur_md);
            draw_scoremenu_dif(score_dif);

            int i = (cur_md == _1P) ? 0 : 3;
            int t;
            for (t = 0; t < 3; ++t) display_score(font, scoreboard[i + t], t);

            al_flip_display();
            redraw = 0;
        }
    }

}

state game_over(ALLEGRO_EVENT_QUEUE *queue, ALLEGRO_FONT *font, int score, int is_high_score){
    ALLEGRO_BITMAP *gameover_continue = al_load_bitmap("gameover_continue.png");
    if (!gameover_continue) errmsg("gameover continue UI image");

    ALLEGRO_BITMAP *gameover_quit = al_load_bitmap("gameover_quit.png");
    if (!gameover_quit) errmsg("gameover quit UI image");

    ALLEGRO_BITMAP *gameover_score = al_load_bitmap("menu_score.png");
    if (!gameover_score) errmsg("gameover score UI image");

    unsigned char cur_key;

    game_over_button bt = CONTINUE;
    int stop = 0, redraw = 0;
    state return_state = DONE;

    while (1){
        al_wait_for_event(queue, &ev);

        switch(ev.type){
            case ALLEGRO_EVENT_TIMER:
                redraw = 1;
                break;

            case ALLEGRO_EVENT_KEY_DOWN:

                cur_key = ev.keyboard.keycode;

                if (cur_key == MENU_DOWN){
                    bt = (bt + 1) % 2;
                }

                else if (cur_key == MENU_UP){
                    bt = (2 + (bt - 1)) % 2;
                }

                else if (cur_key == MENU_ACCEPT){
                    stop = 1;
                    switch (bt){
                        case CONTINUE: return_state = PLAYING; break;
                        case QUIT: return_state = MAIN_MENU; break;
                    }
                }

                break;

            case ALLEGRO_EVENT_DISPLAY_CLOSE:
                stop = 1;
                return_state = DONE;
                break;
        }

        if (stop){
            destroy_gameovermenu(gameover_score, gameover_continue, gameover_quit);
            return return_state;
        }

        if (redraw && al_is_event_queue_empty(queue)){
            al_clear_to_color(al_map_rgb(0, 0, 0));

            draw_gameover_score(font, gameover_score, score, is_high_score);
            draw_gameover_UI(gameover_continue, gameover_quit, bt);

            al_flip_display();
            redraw = 0;
        }



    }
}

state options(ALLEGRO_EVENT_QUEUE *queue, ALLEGRO_FONT *font, ALLEGRO_BITMAP *background, playerobj *template1, playerobj *template2){

    ALLEGRO_BITMAP *options_playerone = al_load_bitmap("options_playerone.png");
    if (!options_playerone) errmsg("options playerone UI image");

    ALLEGRO_BITMAP *options_playertwo = al_load_bitmap("options_playertwo.png");
    if (!options_playertwo) errmsg("options playertwo UI image");

    ALLEGRO_BITMAP *options_left = al_load_bitmap("options_left.png");
    if (!options_left) errmsg("options left UI image");

    ALLEGRO_BITMAP *options_right = al_load_bitmap("options_right.png");
    if (!options_right) errmsg("options right UI image");

    ALLEGRO_BITMAP *options_action = al_load_bitmap("options_action.png");
    if (!options_action) errmsg("options action UI image");

    ALLEGRO_BITMAP *options_reset = al_load_bitmap("options_reset.png");
    if (!options_reset) errmsg("options reset UI image");

    options_button bt = LEFT;
    mode cur_md = _1P;
    unsigned char cur_key;


    int stop = 0, redraw = 0, prompt = 0;
    state return_state = DONE;

    while (1){
        al_wait_for_event(queue, &ev);

        switch(ev.type){
            case ALLEGRO_EVENT_TIMER:
                redraw = 1;
                break;

            case ALLEGRO_EVENT_KEY_DOWN:

                cur_key = ev.keyboard.keycode;

                if (prompt){
                    switch (cur_md){
                        case _1P:
                            change_temp_control(template1, bt, cur_key);
                            break;
                        case _2P:
                            change_temp_control(template2, bt, cur_key);
                            break;
                    }
                    prompt = 0;
                }

                else {
                    if (cur_key == MENU_DOWN){
                        bt = (bt + 1) % 4;
                    }

                    else if (cur_key == MENU_UP){
                        bt = (4 + (bt - 1)) % 4;
                    }

                    else if (cur_key == MENU_LEFT || cur_key == MENU_RIGHT){
                        cur_md = (!(cur_md - 1)) + 1;
                    }

                    else if (cur_key == MENU_ACCEPT){
                        if (bt == OPTIONS_RESET){
                            switch (cur_md){
                                case _1P:
                                    destroy_player_temp(template1);
                                    player_template1_init(template1);
                                    break;
                                case _2P:
                                    destroy_player_temp(template2);
                                    player_template2_init(template2);
                                    break;
                            }

                        }

                        else prompt = 1;
                    }

                    else if (cur_key == MENU_BACK){
                        stop = 1;
                        return_state = MAIN_MENU;
                    }

                }



                break;

            case ALLEGRO_EVENT_DISPLAY_CLOSE:
                stop = 1;
                return_state = DONE;
                break;
        }

        if (stop){
            destroy_optionsmenu(options_playerone, options_playertwo, options_left, options_right, options_action, options_reset);
            return return_state;
        }

        if (redraw && al_is_event_queue_empty(queue)){
            al_clear_to_color(al_map_rgb(0, 0, 0));

            if (prompt){
                draw_optionsmenu_prompt(font);
            }
            else {
                draw_background(background);
                draw_optionsmenu_headings(options_playerone, options_playertwo);
                draw_optionsmenu_UI(options_left, options_right, options_action, options_reset, cur_md, bt);
            }

            al_flip_display();
            redraw = 0;
        }



    }
}

state start(ALLEGRO_EVENT_QUEUE *queue, ALLEGRO_BITMAP *background, playerobj template1, playerobj template2, difficulty *dif, mode *md, start_possibility *pos, int *change){

    ALLEGRO_BITMAP *start_easy = al_load_bitmap("start_easy.png");
    if (!start_easy) errmsg("start easy UI image");

    ALLEGRO_BITMAP *start_medium = al_load_bitmap("start_medium.png");
    if (!start_medium) errmsg("start medium UI image");

    ALLEGRO_BITMAP *start_hard = al_load_bitmap("start_hard.png");
    if (!start_hard) errmsg("start hard UI image");

    ALLEGRO_BITMAP *start_mode = al_load_bitmap("mode.png");
    if (!start_mode) errmsg("start mode UI image");

    ALLEGRO_BITMAP *start_playerbox = al_load_bitmap("playerbox.png");
    if (!start_playerbox) errmsg("start playerbox UI image");

    ALLEGRO_EVENT ev;
    unsigned char cur_key;

    int stop = 0, redraw = 0, player_select = 0;
    state return_state = DONE;

    while (1){
        al_wait_for_event(queue, &ev);
        switch(ev.type){

            case ALLEGRO_EVENT_TIMER:
                redraw = 1;
                break;

            case ALLEGRO_EVENT_KEY_DOWN:

                cur_key = ev.keyboard.keycode;

                if (player_select){
                    if (cur_key == MENU_RIGHT){
                        *pos = (*pos + 1) % 3;
                    }

                    else if (cur_key == MENU_LEFT){
                        *pos = (3 + (*pos - 1)) % 3;
                    }

                    else if (cur_key == MENU_ACCEPT){
                        stop = 1;
                        return_state = PLAYING;
                    }

                    else if (cur_key == MENU_BACK){
                        player_select = 0;
                    }


                    switch (*pos){
                        case P1_SOLO:
                            *change = 0;
                            *md = _1P;
                            break;
                        case P2_SOLO:
                            *change = 1;
                            *md = _1P;
                            break;
                        case MULTI:
                            *change = 0;
                            *md = _2P;
                            break;
                    }

                }

                else {

                    if (cur_key == MENU_RIGHT){
                        *dif = (*dif + 1) % 3;
                    }

                    else if (cur_key == MENU_LEFT){
                        *dif = (3 + (*dif - 1)) % 3;
                    }

                    else if (cur_key == MENU_ACCEPT){
                        player_select = 1;
                    }

                    else if (cur_key == MENU_BACK){
                        stop = 1;
                        return_state = MAIN_MENU;
                    }

                }

                break;

            case ALLEGRO_EVENT_DISPLAY_CLOSE:
                stop = 1;
                return_state = DONE;
                break;
        }

        if (stop){
            destroy_startmenu(start_easy, start_medium, start_hard, start_mode, start_playerbox);
            return return_state;
        }

        if (redraw && al_is_event_queue_empty(queue)){
            al_clear_to_color(al_map_rgb(0, 0, 0));
            draw_background(background);
            draw_startmenu_dif(start_easy, start_medium, start_hard, *dif);
            if (player_select) draw_startmenu_playerselect(start_mode, start_playerbox, template1, template2, *md, *pos);
            al_flip_display();
            redraw = 0;
        }
    }
}

//--------------------------------------------------------//





int main(int argc, char **argv){
    if (!al_init()) errmsg("allegro");
    if (!al_install_keyboard()) errmsg("keyboard");
    if (!al_init_image_addon()) errmsg("image addon");
    al_init_font_addon();
    if (!al_init_ttf_addon()) errmsg("ttf addon");


    ALLEGRO_TIMER* timer = al_create_timer(1.0/FPS);
    if (!timer) errmsg("timer");

    ALLEGRO_DISPLAY* display = al_create_display(SCREEN_W, SCREEN_H);
    if (!display) errmsg("display");

    ALLEGRO_FONT *font = al_load_font("upheavtt.ttf", -28, 0);
    if (!font) errmsg("font");


    ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
    if (!queue) errmsg("queue");

    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_display_event_source(display));
    al_register_event_source(queue, al_get_timer_event_source(timer));

    unsigned char key[ALLEGRO_KEY_MAX];
    start_key(key);

    ALLEGRO_BITMAP *background = al_load_bitmap("background.png");
    if (!background) errmsg("background sprite");

    int pillars_id_order[] = {4, 5, 3, 6, 2, 7, 1, 8, 0, 9};
    int pillars_spawn_time[][10] = {{0, 5, 20, 21, 60, 61, 120, 121, 180, 181}, {0, 1, 5, 6, 30, 31, 60, 61, 90, 91}, {0, 1, 2, 5, 15, 16, 30, 31, 45, 46}};
    pillarobj pillar_list[MAX_PILLARS];

    playerobj template1, template2;
    player_template1_init(&template1);
    player_template2_init(&template2);
    playerobj player_list[MAX_PLAYERS];


    Node *scoreboard[SCORE_TYPES];
    init_scoreboard(scoreboard);


    state st = MAIN_MENU;
    difficulty dif = EASY;
    mode md = _1P;
    start_possibility pos = P1_SOLO;
    int last_score = 0, is_high_score = 0;
    int change_player_char = 0;


    al_start_timer(timer);
    int done = 0;

    while (1){
        switch (st){
            case MAIN_MENU:
                st = main_menu(queue, font, background);
                break;
            case PLAYING:
                if (change_player_char) st = play(queue, font, background, key, (int) md, template2, template1, dif, pillars_id_order, pillars_spawn_time, pillar_list, player_list, &last_score);
                else st = play(queue, font, background, key, (int) md, template1, template2, dif, pillars_id_order, pillars_spawn_time, pillar_list, player_list, &last_score);
                int t = (md == _1P) ? 0 : 3;
                is_high_score = insert_score(&scoreboard[t + dif], last_score);
                save_scoreboard(scoreboard);
                break;
            case GAME_OVER:
                st = game_over(queue, font, last_score, is_high_score);
                break;
            case SCORE:
                st = score(queue, font, background, scoreboard);
                break;
            case OPTIONS:
                st = options(queue, font, background, &template1, &template2);
                break;
            case START:
                st = start(queue, background, template1, template2, &dif, &md, &pos, &change_player_char);
                break;
            case DONE:
                done = 1;
                break;
        }

        if (done) break;

    }

    destroy_scoreboard(scoreboard);
    destroy_image(background);
    destroy_player_temp(&template1);
    destroy_player_temp(&template2);
    al_destroy_font(font);
    al_destroy_display(display);
    al_destroy_timer(timer);
    al_destroy_event_queue(queue);


	return 0;

}
