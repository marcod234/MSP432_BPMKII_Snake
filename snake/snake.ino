#include <Screen_HX8353E.h>
#include "coord.h"

enum{UP, DOWN, RIGHT, LEFT}; //input directions
enum{MENU, MENU_PRINTED, PAUSE, SETUP, PLAYING, GAME_WON, GAME_OVER, GAME_END}; //game state

const int joyX = 2, joyY = 26, joySel = 5, JS1 = 33, JS2 = 32, buzzer = 40;
const uint8_t ROWS = 64, COLS = 64;
const int MAX_LENGTH = 4096; //64 * 64
const int DEAD_ZONE = 20, NOTE_DURATION = 1000/8, NOTE = 800;
const uint16_t snakeDieColors[2] = {0xF800, 0x0000};

Screen_HX8353E lcd;
uint8_t gameState = MENU;

int snakeLength, score;
coord snake[MAX_LENGTH], fruit, oldFruit; //stores snake's body and fruit location
coord prevTail;
uint8_t snakeDir; //direction snake is moving

void nextState() //interrupt for button JS1
{
  if(gameState == MENU_PRINTED || gameState == GAME_END)
    gameState = SETUP;
}

void pause()
{
  if(gameState == PLAYING)
    gameState = PAUSE;

  else if(gameState == PAUSE)
    gameState = PLAYING;
}

void initSnake()
{
  score = 0;
  snakeLength = 3; //starting length
  //start moving in random direction
  snakeDir = random(4);

  //start snake in a location away from edges
  snake[0].x = random(10, 55);
  snake[0].y = random(10, 55);

  switch(snakeDir) //set the other two coordinates
  {
    case UP: //place them below
    snake[2].x = snake[1].x = snake[0].x;
    snake[2].y = snake[0].y + 2;
    snake[1].y = snake[0].y + 1;
    break;

    case RIGHT: //place them to the left
    snake[2].y = snake[1].y = snake[0].y;
    snake[2].x = snake[0].x - 2;
    snake[1].x = snake[0].x - 1;
    break;

    case DOWN: //place them above
    snake[2].x = snake[1].x = snake[0].x;
    snake[2].y = snake[0].y - 2;
    snake[1].y = snake[0].y - 1;
    break;

    case LEFT: //place them to the right
    snake[2].y = snake[1].y = snake[0].y;
    snake[2].x = snake[0].x + 2;
    snake[1].x = snake[0].x + 1;
    break;
  }
  
  //rest of snake is empty 
  for(int i = 3; i < snakeLength; i++)
  {
    snake[i].x = snake[i].y = -1;
  }  
}

//gets the direction of the joystick
int getDir()
{
  int joyXState = analogRead(joyX) - 2048; 
  int joyYState = analogRead(joyY) - 2048;

  Serial.print("x = ");
  Serial.print(joyXState);
  Serial.print(" y = ");
  Serial.println(joyYState);

  int xAbs = abs(joyXState);
  int yAbs = abs(joyYState);

  uint8_t xPushed = xAbs > DEAD_ZONE;
  uint8_t yPushed = yAbs > DEAD_ZONE;
  uint8_t bothPushed = xPushed && yPushed;

  //neither direction is pushed past dead zone
  if(!bothPushed)
    return snakeDir; //keep same direction

  //diagonal push
  if(bothPushed && (xAbs > yAbs))
    yPushed = 0; //cancel y push as x is stronger
  else
    xPushed = 0; //cancel x push as y is stronger
  
  if(xPushed)
  {
    //turn right if pushed right and not moving left
    if(joyXState > 0 && snakeDir != LEFT)
      return RIGHT;

    //turn left if pushed left and not moving right
    else if( joyXState < 0 && snakeDir != RIGHT)
      return LEFT;

    //otherwise keep current direction
    else
      return snakeDir;
  }
  
  if(yPushed) //vertical push axis is greater
  {
    //go up if pushed up and not moving down
    if(joyYState > 0 && snakeDir != DOWN)
      return UP;

    //go down if pushed down and not moving up
    else if(joyYState < 0 && snakeDir != UP)
      return DOWN;

    //otherwise keep current direction
    else
      return snakeDir;
  }
}

//checks if the given coordinate collides with the snake
char collision(coord *c, int i)
{ 
  //i = 0, check for fruit collide with snake
  //i = 1, check for snake collide with self
  for(i; i < snakeLength; i++)
  {
    if((c->x == snake[i].x) && (c->y == snake[i].y))
      return 1;
  }

  return 0;
}

//check if snake ate the fruit
char ateFruit()
{
  return ((snake[0].x == fruit.x) && (snake[0].y == fruit.y));
}

//has the snake hit a wall?
char outOfBounds()
{
  return (snake[0].x < 0 || snake[0].x >= COLS || snake[0].y < 0 || snake[0].y >= ROWS);
}

//place the fruit in a random location
void nextFruit()
{
  do //make sure it is not placed on top of snake
  {
    fruit.x = random(COLS);
    fruit.y = random(ROWS);
  }while(collision(&fruit, 0));
}

void moveSnake()
{
  //save current tail before moving
  prevTail.x = snake[snakeLength - 1].x;
  prevTail.y = snake[snakeLength - 1].y;
  
  //move body towards head
  for(int i = snakeLength - 1; i > 0; i --)
  {
    snake[i].x = snake[i - 1].x;
    snake[i].y = snake[i - 1].y;
  }

  //move head based on movement direction
  switch(snakeDir)
  {
    case UP:
    snake[0].y--;
    break;

    case RIGHT:
    snake[0].x++;
    break;

    case DOWN:
    snake[0].y++;
    break;

    case LEFT:
    snake[0].x--;
    break;
  }

  if(ateFruit())
  {
    snake[snakeLength].x = prevTail.x;
    snake[snakeLength].y = prevTail.y;
    snakeLength++;
    score++;

    //play a sound when a fruit is eaten
    tone(buzzer, NOTE, NOTE_DURATION);
    
    if(snakeLength == MAX_LENGTH)
      gameState = GAME_WON;
    else
      nextFruit();
  }

  //check if snake ran into itself
  else if(collision(&snake[0], 1))
  {
    gameState = GAME_OVER;
  }

  //check if snake hit a wall
  else if(outOfBounds())
  {
    gameState = GAME_OVER;
  }
}

//draws a single game "pixel"
void drawPoint(coord * c, uint16_t colour = whiteColour)
{
  //each "pixel" is 4 actual pixels
  uint16_t x_2 = 2 * c->x;
  uint16_t y_2 = 2 * c->y;
  
  lcd.rectangle(x_2, y_2, x_2 + 1, y_2 + 1, colour); 
}

//draw initial snake and fruit
void drawInit()
{
  for(int i = 0; i < snakeLength; i++)
  {
    drawPoint(&snake[i]);
  }

  drawPoint(&fruit); //draw fruit
}

void draw()
{
  //clear old tail
  drawPoint(&prevTail, blackColour);

  //draw new head and tail
  drawPoint(&snake[0], whiteColour);
  drawPoint(&snake[snakeLength - 1], whiteColour);

  //draw fruit
  drawPoint(&fruit); //draw fruit
}

void setup() {
  pinMode(joySel, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);
  pinMode(JS1, INPUT_PULLUP);
  pinMode(JS2, INPUT_PULLUP);
  
  attachInterrupt(JS1, nextState, FALLING);
  attachInterrupt(JS2, pause, FALLING);
  
  analogReadResolution(12);
  
  Serial.begin(9600);
  
  randomSeed(analogRead(0));

  lcd.begin();
  lcd.clear(); //clear to black
}

void loop() {

  switch(gameState)
  {
    case MENU:
      lcd.clear();
      lcd.gText(37, 32, "S N A K E");
      lcd.gText(13, 52, "Press S1 to Start");
      gameState = MENU_PRINTED;
      break;

    case SETUP:
      lcd.clear(); //clear to black
      
      //place snake in random location moving in random direction
      initSnake();

      //place the fruit in a random location
      nextFruit();

      //draw startup
      drawInit();

      //flash head green and white to show snake's orientation
      for(uint8_t i = 0; i < 5; i++)
      {
        delay(333);
        drawPoint(&snake[0], 0xF81F);
        delay(333);
        drawPoint(&snake[0], whiteColour);
      }

      gameState = PLAYING;
      break;

    case PLAYING:
      snakeDir = getDir();
      moveSnake();
      draw();
      delay(10);
      break;

    case GAME_WON:
      //play win song
      tone(buzzer, 600, NOTE_DURATION);
      delay(NOTE_DURATION);
      tone(buzzer, 600, NOTE_DURATION);
      delay(NOTE_DURATION);
      tone(buzzer, 800, NOTE_DURATION * 3);

      //print win screen
      lcd.clear();
      lcd.gText(30, 32, "Y O U  W I N");
      lcd.gText(33, 52, "Score: " + i32toa(score));
      lcd.gText(8, 72, "Press S1 to Restart");
      gameState = GAME_END;      
      break;

    case GAME_OVER:
      //play lose song
      tone(buzzer, 400, NOTE_DURATION*2); //eigth note
      delay(NOTE_DURATION *2);
      tone(buzzer, 200, NOTE_DURATION * 3); //eigth note

      //animation of snake dying
      for(int i = 0; i < 2; i++)
        for(int j = 0; j < snakeLength; j++)
        {
          drawPoint(&snake[j], snakeDieColors[i]);
          delay(50);
        }
        
      //print lose screen
      lcd.clear();
      lcd.gText(15, 32, "G A M E  O V E R");
      lcd.gText(33, 52, "Score: " + i32toa(score));
      lcd.gText(8, 72, "Press S1 to Restart");
      gameState = GAME_END;
      break;
  }
}
