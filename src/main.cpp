#include <Arduino.h>
#include "LedControl.h"
#include <stdio.h>

// pins max7219 8x8 matrix LED
const int pin_DIN = 23;
const int pin_CLK = 18;
const int pin_CS = 5;
LedControl lc = LedControl(pin_DIN, pin_CLK, pin_CS, 1);
#define ON 1
#define OFF 0

// pins adc joystick
const int pin_X = 12;
const int pin_Y = 14;
const int pin_SW = 27;

// movement
int direction = 0;
#define right 1
#define left 2
#define up 3
#define down 4

// field & gameobjects
#define snake 5
#define apple 1

struct Position
{
  int row;
  int column;
};

struct Snake
{
  struct Position segment[64];
  int xSegment[64];
  int ySegment[64];
  int segments = 1;
} Snake;

int field[8][8]; // array for representing the field

int gameover = 0; // flag for gameover
int pause_game = 0;    // flag for pause

// tasks
TaskHandle_t logicTaskHandle = NULL;
TaskHandle_t inputTaskHandle = NULL;
TaskHandle_t gameoverTaskHandle = NULL;

// functions
void random_apples();

void Logic_Task(void *arg)
{
  while (1)
  {
    // move every segment one forward
    for (int i = Snake.segments; i > 0; i--)
    {
      Snake.segment[i].row = Snake.segment[i - 1].row;
      Snake.segment[i].column = Snake.segment[i - 1].column;

      // printf("%d\t%d\t%d\n", i, _position[i][0], _position[i][1]);
    }
    // delete last field of snake
    field[Snake.segment[Snake.segments].row][Snake.segment[Snake.segments].column] = 0;
    lc.setLed(0, Snake.segment[Snake.segments].row, Snake.segment[Snake.segments].column, OFF);

    // // get direction/ new heading
    switch (direction)
    {
    case right:
      Snake.segment[0].column += 1;
      break;
    case left:
      Snake.segment[0].column -= 1;
      break;
    // change row
    case up:
      Snake.segment[0].row -= 1;
      break;
    case down:
      Snake.segment[0].row += 1;
      break;
    default:
      break;
    }

    // // allow snake to loop back around at corner
    if (Snake.segment[0].row > 7)
      Snake.segment[0].row = 0;
    if (Snake.segment[0].row < 0)
      Snake.segment[0].row = 7;
    if (Snake.segment[0].column > 7)
      Snake.segment[0].column = 0;
    if (Snake.segment[0].column < 0)
      Snake.segment[0].column = 7;

    // logic
    // printf("Feld: %d\t %d\t %d\n", _position[0][0], _position[0][1], field[_position[0][0]][_position[0][1]]);

    // detect Gameover
    if (field[Snake.segment[0].row][Snake.segment[0].column] == snake)
    {
      Serial.printf("GameOver\n");
      vTaskSuspend(NULL);
    }

    // detect apple
    if (field[Snake.segment[0].row][Snake.segment[0].column] == apple)
    {
      Snake.segments += 1;
      field[Snake.segment[0].row][Snake.segment[0].column] = snake;
      random_apples();
    }

    // draw snake
    for (int i = 0; i < Snake.segments; i++)
    {
      field[Snake.segment[i].row][Snake.segment[i].column] = snake;
    }
    lc.setLed(0, Snake.segment[0].row, Snake.segment[0].column, ON); // turn on new led

    // draw field on serial monitor
    for (int row = 0; row < 8; row++)
    {
      Serial.printf("row:%d\t", row);
      for (int column = 0; column < 8; column++)
      {
        Serial.printf("%d\t", field[row][column]);
      }
      Serial.printf("\n");
    }
    Serial.printf("\n");

    // repeat every 300 ms
    vTaskDelay(300 / portTICK_RATE_MS);
  }
}

void Input_Task(void *arg)
{
  int xValue;
  int yValue;
  int swValue;

  while (1)
  {
    // read analogue stick
    xValue = analogRead(pin_X);
    yValue = analogRead(pin_Y);
    swValue = digitalRead(pin_SW);

    //  determine direction
    if (abs(240 - xValue) > abs(240 - yValue)) // check if swing in x or y is larger
    {
      if (xValue < 200)
        direction = right;
      else if (xValue > 300)
        direction = left;
    }
    else
    {
      if (yValue < 200)
        direction = down;
      else if (yValue > 300)
        direction = up;
    }

    // pause game
    // swValue == LOW --> joystick sw3itch is NC
    // if ((swValue == LOW) && (pause_game == 0))
    // {
    //   pause_game = 1;
    //   vTaskSuspend(logicTaskHandle);
    // }

    // resume game
    if ((swValue == LOW) && (pause_game == 1))
    {
      pause_game = 0;
      vTaskResume(logicTaskHandle);
    }

    // Serial.printf("x:\t%d\n", xValue);
    // Serial.printf("y:\t%d\n", yValue);
    Serial.printf("sw:\t%d\n", swValue);

    vTaskDelay(100 / portTICK_RATE_MS);
  }
}

void Gameover_Task(void *arg)
{
  while (1)
  {
    vTaskDelay(300 / portTICK_RATE_MS);
  }
}

void setup()
{
  // put your setup code here, to run once:

  Serial.begin(9600);

  //  init snake at 4,4
  Snake.segment[0].row = 4;
  Snake.segment[0].column = 4;

  // starting direction
  direction = right;

  

  analogReadResolution(9);

  // pin mode
  pinMode(pin_SW, INPUT_PULLUP);
  pinMode(pin_X, INPUT);
  pinMode(pin_Y, INPUT);

  // led init
  lc.shutdown(0, false);
  lc.setIntensity(0, 8); // Set the brightness to a medium values
  lc.clearDisplay(0);    // clear display

  // create apple at random location
  srand(time(NULL)); // set rand seed
  random_apples();

  xTaskCreate(Logic_Task, "Logic_Task", 4096, NULL, 10, &logicTaskHandle);
  xTaskCreate(Input_Task, "Input_Task", 4096, NULL, 10, &inputTaskHandle);
  xTaskCreate(Gameover_Task, "Gameover_Task", 4096, NULL, 10, &gameoverTaskHandle);

  // pause game at start
  pause_game = 1;
  vTaskSuspend(logicTaskHandle);
  vTaskSuspend(gameoverTaskHandle);
}

void random_apples()
{
  int apple_position[64][2]; // hold all possible apple positions on the field
  int i = 0;
  for (int row = 0; row < 8; row++)
  {
    for (int column = 0; column < 8; column++)
    {
      if (field[row][column] != snake)
      {
        apple_position[i][0] = row;
        apple_position[i][1] = column;
      }
      i++;
    }
  }

  int k = rand() % (63 - Snake.segments + 1); // get random unoccupied position from array
  // printf("apple_position\t x: y:\t%d\t%d\n", apple_position[k][0], apple_position[k][1] );
  field[apple_position[k][0]][apple_position[k][1]] = apple;
  lc.setLed(0, apple_position[k][0], apple_position[k][1], ON); // turn on led

  return;
}

void loop()
{
  // put your main code here, to run repeatedly:
}