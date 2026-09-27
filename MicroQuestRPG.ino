
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// =====================================================
// BUTTONS
// =====================================================

#define BTN_UP     4
#define BTN_DOWN   5
#define BTN_LEFT   6
#define BTN_RIGHT  7

// =====================================================
// WORLD
// =====================================================

#define TILE 8

#define WORLD_W 100
#define WORLD_H 100

#define GRASS     0
#define FOREST    1
#define WATER     2
#define MOUNTAIN  3
#define DESERT    4
#define ROCK      5
#define TREE      6
#define ROAD      7
#define HOUSE     8
#define SHOP      9
#define INN       10

// =====================================================
// PLAYER
// =====================================================

int nilsX = 50 * TILE;
int nilsY = 62 * TILE;

int cameraX;
int cameraY;

byte nilsDirection = 0;

int hp = 100;
int maxHP = 100;

int xp = 0;
int xpNeeded = 100;

int level = 1;
int coins = 20;

byte weaponLevel = 1;

// =====================================================
// VILLAGE
// =====================================================

#define VILLAGE_X 43
#define VILLAGE_Y 42
#define VILLAGE_W 15
#define VILLAGE_H 16

#define SHOP_X 44
#define SHOP_Y 44
#define SHOP_W 5
#define SHOP_H 4

#define INN_X 52
#define INN_Y 44
#define INN_W 5
#define INN_H 4

// =====================================================
// ENEMIES
// =====================================================

#define MAX_ENEMIES 3

struct Enemy {
  bool alive;
  int x;
  int y;
  int hp;
  int maxHP;
  byte type;
};

Enemy enemies[MAX_ENEMIES];

unsigned long lastEnemySpawn = 0;
unsigned long lastEnemyMove = 0;

// =====================================================
// SHOP
// =====================================================

bool shopOpen = false;
bool shopTouchLock = false;

byte shopItem = 1;

byte shopMessage = 0;

unsigned long shopMessageTime = 0;

int weaponCost(byte weapon) {

  if (weapon == 1) return 10;
  if (weapon == 2) return 25;
  return 50;
}

int weaponPower(byte weapon) {

  if (weapon == 1) return 10;
  if (weapon == 2) return 25;
  return 45;
}

// =====================================================
// TURN BASED COMBAT
// =====================================================

bool battleActive = false;

int battleEnemy = -1;

bool playerTurn = true;

byte battleMessage = 0;

int battleDamage = 0;

unsigned long battleMessageTime = 0;

// =====================================================
// BUTTON STATE
// =====================================================

bool lastUp = false;
bool lastDown = false;
bool lastLeft = false;
bool lastRight = false;

bool upNow;
bool downNow;
bool leftNow;
bool rightNow;

bool upPressed;
bool downPressed;
bool leftPressed;
bool rightPressed;

void readButtons() {

  upNow = digitalRead(BTN_UP) == LOW;
  downNow = digitalRead(BTN_DOWN) == LOW;
  leftNow = digitalRead(BTN_LEFT) == LOW;
  rightNow = digitalRead(BTN_RIGHT) == LOW;

  upPressed = upNow && !lastUp;
  downPressed = downNow && !lastDown;
  leftPressed = leftNow && !lastLeft;
  rightPressed = rightNow && !lastRight;

  lastUp = upNow;
  lastDown = downNow;
  lastLeft = leftNow;
  lastRight = rightNow;
}

// =====================================================
// TERRAIN
// =====================================================

byte getTerrain(int tx, int ty) {

  if (tx < 2 || ty < 2 ||
      tx >= WORLD_W - 2 ||
      ty >= WORLD_H - 2) {

    return MOUNTAIN;
  }

  if (tx >= VILLAGE_X &&
      tx < VILLAGE_X + VILLAGE_W &&
      ty >= VILLAGE_Y &&
      ty < VILLAGE_Y + VILLAGE_H) {

    if (tx >= SHOP_X &&
        tx < SHOP_X + SHOP_W &&
        ty >= SHOP_Y &&
        ty < SHOP_Y + SHOP_H) {

      if (tx == SHOP_X + 2 &&
          ty == SHOP_Y + SHOP_H - 1) {

        return ROAD;
      }

      return SHOP;
    }

    if (tx >= INN_X &&
        tx < INN_X + INN_W &&
        ty >= INN_Y &&
        ty < INN_Y + INN_H) {

      if (tx == INN_X + 2 &&
          ty == INN_Y + INN_H - 1) {

        return ROAD;
      }

      return INN;
    }

    if (tx == 49 ||
        ty == 48 ||
        ty == 50 ||
        ty == 55) {

      return ROAD;
    }

    if ((tx >= 44 && tx <= 47 &&
         ty >= 53 && ty <= 56) ||
        (tx >= 53 && tx <= 56 &&
         ty >= 53 && ty <= 56)) {

      return HOUSE;
    }

    return ROAD;
  }

  if (tx >= 64 && tx <= 75 &&
      ty >= 12 && ty <= 80) {

    return WATER;
  }

  if (tx >= 8 && tx <= 37 &&
      ty >= 67 && ty <= 93) {

    return DESERT;
  }

  if (tx >= 70 && tx <= 95 &&
      ty >= 67 && ty <= 94) {

    return MOUNTAIN;
  }

  if (tx >= 8 && tx <= 40 &&
      ty >= 8 && ty <= 39) {

    if ((tx * 17 + ty * 13) % 6 != 0)
      return FOREST;
  }

  if ((tx * 31 + ty * 17) % 29 == 0)
    return TREE;

  if ((tx * 13 + ty * 19) % 47 == 0)
    return ROCK;

  return GRASS;
}

// =====================================================
// COLLISION
// =====================================================

bool canMove(int x, int y) {

  int radius = 3;

  int points[4][2] = {
    {x - radius, y},
    {x + radius, y},
    {x, y - radius},
    {x, y + radius}
  };

  for (int i = 0; i < 4; i++) {

    int tx = points[i][0] / TILE;
    int ty = points[i][1] / TILE;

    if (tx < 0 || tx >= WORLD_W ||
        ty < 0 || ty >= WORLD_H) {

      return false;
    }

    byte terrain = getTerrain(tx, ty);

    if (terrain == WATER ||
        terrain == MOUNTAIN ||
        terrain == ROCK ||
        terrain == TREE ||
        terrain == HOUSE ||
        terrain == SHOP ||
        terrain == INN) {

      return false;
    }

    for (int e = 0; e < MAX_ENEMIES; e++) {

      if (!enemies[e].alive)
        continue;

      int ex = enemies[e].x / TILE;
      int ey = enemies[e].y / TILE;

      if (tx == ex && ty == ey)
        return false;
    }
  }

  return true;
}

// =====================================================
// DISTANCE
// =====================================================

int tileDistance(
  int x1,
  int y1,
  int x2,
  int y2
) {

  return abs(x1 - x2) +
         abs(y1 - y2);
}

// =====================================================
// XP / LEVEL
// =====================================================

void gainXP(int amount) {

  xp += amount;

  while (xp >= xpNeeded) {

    xp -= xpNeeded;

    level++;

    xpNeeded += 25;

    maxHP += 10;

    hp = maxHP;
  }
}

// =====================================================
// ENEMY IN DIRECTION
// =====================================================

int enemyInDirection(byte direction) {

  int targetX = nilsX;
  int targetY = nilsY;

  if (direction == 0)
    targetY += TILE;

  if (direction == 1)
    targetY -= TILE;

  if (direction == 2)
    targetX -= TILE;

  if (direction == 3)
    targetX += TILE;

  for (int e = 0; e < MAX_ENEMIES; e++) {

    if (!enemies[e].alive)
      continue;

    if (abs(enemies[e].x - targetX) <= 4 &&
        abs(enemies[e].y - targetY) <= 4) {

      return e;
    }
  }

  return -1;
}

// =====================================================
// START BATTLE
// =====================================================

void startBattle(int enemyIndex) {

  if (enemyIndex < 0 ||
      enemyIndex >= MAX_ENEMIES)
    return;

  if (!enemies[enemyIndex].alive)
    return;

  battleEnemy = enemyIndex;

  battleActive = true;

  playerTurn = true;

  battleMessage = 0;
  battleDamage = 0;
}

// =====================================================
// PLAYER ATTACK
// =====================================================

void playerAttack() {

  if (!battleActive)
    return;

  if (!playerTurn)
    return;

  if (battleEnemy < 0 ||
      battleEnemy >= MAX_ENEMIES)
    return;

  Enemy &enemy = enemies[battleEnemy];

  int damage = weaponPower(weaponLevel);

  damage += random(-2, 4);

  if (damage < 1)
    damage = 1;

  if (damage >= enemy.hp) {

    enemy.hp = 0;
    enemy.alive = false;

    coins += 5 + enemy.type * 3;

    gainXP(25 + enemy.type * 10);

    battleMessage = 5;
    battleDamage = damage;
    battleMessageTime = millis() + 900;

    delay(700);

    battleActive = false;
    battleEnemy = -1;

    return;
  }

  enemy.hp -= damage;

  battleMessage = 1;
  battleDamage = damage;
  battleMessageTime = millis() + 700;

  playerTurn = false;

  delay(600);
}

// =====================================================
// ENEMY ATTACK
// =====================================================

void enemyAttack() {

  if (!battleActive)
    return;

  if (battleEnemy < 0 ||
      battleEnemy >= MAX_ENEMIES)
    return;

  Enemy &enemy = enemies[battleEnemy];

  if (!enemy.alive)
    return;

  int damage;

  if (enemy.type == 1)
    damage = 5 + random(0, 4);
  else
    damage = 8 + random(0, 5);

  if (damage >= hp) {

    hp = 0;

    battleMessage = 6;
    battleDamage = damage;
    battleMessageTime = millis() + 900;

    delay(700);

    hp = maxHP;

    nilsX = 50 * TILE;
    nilsY = 60 * TILE;

    for (int e = 0; e < MAX_ENEMIES; e++)
      enemies[e].alive = false;

    battleActive = false;
    battleEnemy = -1;

    return;
  }

  hp -= damage;

  battleMessage = 2;
  battleDamage = damage;
  battleMessageTime = millis() + 700;

  delay(600);

  playerTurn = true;
}

// =====================================================
// BATTLE UPDATE
// =====================================================

void updateBattle() {

  if (!battleActive)
    return;

  if (battleEnemy < 0 ||
      battleEnemy >= MAX_ENEMIES)
    return;

  if (!playerTurn) {

    enemyAttack();

    return;
  }

  if (rightPressed) {

    playerAttack();

    // Enemy attacks immediately after player attack
    if (battleActive && !playerTurn)
      enemyAttack();

    return;
  }

  if (upPressed) {

    if (hp < maxHP) {

      int oldHP = hp;

      hp += 15;

      if (hp > maxHP)
        hp = maxHP;

      battleDamage = hp - oldHP;

      battleMessage = 3;
      battleMessageTime = millis() + 700;

      playerTurn = false;

      // Enemy attacks after healing
      enemyAttack();

      return;
    }

    return;
  }

  if (downPressed) {

    battleMessage = 4;
    battleMessageTime = millis() + 600;

    delay(350);

    battleActive = false;
    battleEnemy = -1;

    return;
  }
}

// =====================================================
// MOVE
// =====================================================

void moveOrAttack(byte direction) {

  nilsDirection = direction;

  int enemy =
    enemyInDirection(direction);

  if (enemy >= 0) {

    startBattle(enemy);

    return;
  }

  int dx = 0;
  int dy = 0;

  if (direction == 0)
    dy = 3;

  if (direction == 1)
    dy = -3;

  if (direction == 2)
    dx = -3;

  if (direction == 3)
    dx = 3;

  int newX = nilsX + dx;
  int newY = nilsY + dy;

  if (canMove(newX, newY)) {

    nilsX = newX;
    nilsY = newY;
  }
}

// =====================================================
// CAMERA
// =====================================================

void updateCamera() {

  cameraX =
    nilsX - SCREEN_WIDTH / 2;

  cameraY =
    nilsY - SCREEN_HEIGHT / 2;

  int maxX =
    WORLD_W * TILE - SCREEN_WIDTH;

  int maxY =
    WORLD_H * TILE - SCREEN_HEIGHT;

  if (cameraX < 0)
    cameraX = 0;

  if (cameraY < 0)
    cameraY = 0;

  if (cameraX > maxX)
    cameraX = maxX;

  if (cameraY > maxY)
    cameraY = maxY;
}

// =====================================================
// TERRAIN DRAWING
// =====================================================

void drawTerrain(
  byte terrain,
  int x,
  int y
) {

  if (terrain == GRASS) {

    if ((x + y) % 3 == 0)
      display.drawPixel(
        x + 2,
        y + 4,
        SSD1306_WHITE
      );
  }

  else if (terrain == FOREST) {

    display.drawPixel(
      x + 1,
      y + 2,
      SSD1306_WHITE
    );

    display.drawPixel(
      x + 6,
      y + 5,
      SSD1306_WHITE
    );

    display.drawLine(
      x + 3,
      y + 1,
      x + 3,
      y + 6,
      SSD1306_WHITE
    );
  }

  else if (terrain == WATER) {

    display.drawLine(
      x + 1,
      y + 2,
      x + 6,
      y + 2,
      SSD1306_WHITE
    );

    display.drawLine(
      x + 2,
      y + 5,
      x + 7,
      y + 5,
      SSD1306_WHITE
    );
  }

  else if (terrain == MOUNTAIN) {

    display.fillTriangle(
      x + 1,
      y + 7,
      x + 4,
      y + 1,
      x + 7,
      y + 7,
      SSD1306_WHITE
    );

    display.drawPixel(
      x + 4,
      y + 2,
      SSD1306_BLACK
    );
  }

  else if (terrain == DESERT) {

    display.drawPixel(
      x + 2,
      y + 3,
      SSD1306_WHITE
    );

    display.drawPixel(
      x + 6,
      y + 6,
      SSD1306_WHITE
    );
  }

  else if (terrain == ROCK) {

    display.fillCircle(
      x + 4,
      y + 4,
      3,
      SSD1306_WHITE
    );

    display.drawPixel(
      x + 3,
      y + 3,
      SSD1306_BLACK
    );
  }

  else if (terrain == TREE) {

    display.fillTriangle(
      x + 4,
      y + 1,
      x + 1,
      y + 6,
      x + 7,
      y + 6,
      SSD1306_WHITE
    );

    display.drawLine(
      x + 4,
      y + 5,
      x + 4,
      y + 7,
      SSD1306_BLACK
    );
  }

  else if (terrain == ROAD) {

    display.drawPixel(
      x + 2,
      y + 3,
      SSD1306_WHITE
    );

    display.drawPixel(
      x + 6,
      y + 6,
      SSD1306_WHITE
    );
  }

  else if (terrain == HOUSE) {

    display.fillRect(
      x + 1,
      y + 3,
      6,
      5,
      SSD1306_WHITE
    );

    display.fillTriangle(
      x,
      y + 3,
      x + 4,
      y,
      x + 7,
      y + 3,
      SSD1306_WHITE
    );

    display.drawPixel(
      x + 4,
      y + 6,
      SSD1306_BLACK
    );
  }

  else if (terrain == SHOP) {

    display.fillRect(
      x,
      y + 2,
      8,
      6,
      SSD1306_WHITE
    );

    display.drawLine(
      x,
      y + 2,
      x + 7,
      y + 2,
      SSD1306_BLACK
    );
  }

  else if (terrain == INN) {

    display.fillRect(
      x,
      y + 2,
      8,
      6,
      SSD1306_WHITE
    );

    display.drawLine(
      x,
      y + 2,
      x + 7,
      y + 2,
      SSD1306_BLACK
    );
  }
}

// =====================================================
// WORLD
// =====================================================

void drawWorld() {

  int firstX =
    cameraX / TILE;

  int firstY =
    cameraY / TILE;

  int lastX =
    (cameraX + SCREEN_WIDTH) /
    TILE + 1;

  int lastY =
    (cameraY + SCREEN_HEIGHT) /
    TILE + 1;

  if (lastX >= WORLD_W)
    lastX = WORLD_W - 1;

  if (lastY >= WORLD_H)
    lastY = WORLD_H - 1;

  for (int ty = firstY;
       ty <= lastY;
       ty++) {

    for (int tx = firstX;
         tx <= lastX;
         tx++) {

      int sx =
        tx * TILE - cameraX;

      int sy =
        ty * TILE - cameraY;

      drawTerrain(
        getTerrain(tx, ty),
        sx,
        sy
      );
    }
  }
}

// =====================================================
// SMALL SIGNS
// =====================================================

void drawSmallSigns() {

  int shopSX =
    SHOP_X * TILE - cameraX;

  int shopSY =
    SHOP_Y * TILE - cameraY;

  int innSX =
    INN_X * TILE - cameraX;

  int innSY =
    INN_Y * TILE - cameraY;

  if (shopSX > -20 &&
      shopSX < SCREEN_WIDTH) {

    display.setTextSize(1);

    display.setCursor(
      shopSX - 5,
      shopSY - 2
    );

    display.print("S");
  }

  if (innSX > -10 &&
      innSX < SCREEN_WIDTH) {

    display.setTextSize(1);

    display.setCursor(
      innSX + 5,
      innSY - 2
    );

    display.print("I");
  }
}

// =====================================================
// SMALL SQUARE PLAYER
// =====================================================

void drawNils() {

  int x =
    nilsX - cameraX;

  int y =
    nilsY - cameraY;

  display.fillRect(
    x - 3,
    y - 3,
    6,
    6,
    SSD1306_WHITE
  );

  display.drawPixel(
    x - 1,
    y - 1,
    SSD1306_BLACK
  );

  display.drawPixel(
    x + 1,
    y - 1,
    SSD1306_BLACK
  );
}

// =====================================================
// ENEMY SPAWN
// =====================================================

void spawnEnemy() {

  for (int e = 0;
       e < MAX_ENEMIES;
       e++) {

    if (enemies[e].alive)
      continue;

    int px =
      nilsX / TILE;

    int py =
      nilsY / TILE;

    int ex =
      px + random(-12, 13);

    int ey =
      py + random(-8, 9);

    if (ex < 2 ||
        ey < 2 ||
        ex >= WORLD_W - 2 ||
        ey >= WORLD_H - 2) {

      continue;
    }

    byte terrain =
      getTerrain(ex, ey);

    if (terrain == WATER ||
        terrain == MOUNTAIN ||
        terrain == ROCK ||
        terrain == TREE ||
        terrain == HOUSE ||
        terrain == SHOP ||
        terrain == INN) {

      continue;
    }

    if (tileDistance(
          ex,
          ey,
          px,
          py) < 5) {

      continue;
    }

    bool occupied = false;

    for (int other = 0;
         other < MAX_ENEMIES;
         other++) {

      if (other == e)
        continue;

      if (enemies[other].alive &&
          enemies[other].x / TILE == ex &&
          enemies[other].y / TILE == ey) {

        occupied = true;
        break;
      }
    }

    if (occupied)
      continue;

    enemies[e].alive = true;

    enemies[e].x =
      ex * TILE;

    enemies[e].y =
      ey * TILE;

    enemies[e].type =
      random(1, 3);

    if (enemies[e].type == 1) {

      enemies[e].maxHP = 30;
      enemies[e].hp = 30;

    } else {

      enemies[e].maxHP = 50;
      enemies[e].hp = 50;
    }

    return;
  }
}

// =====================================================
// ENEMY MOVEMENT
// =====================================================

void updateEnemies() {

  if (battleActive)
    return;

  if (millis() - lastEnemySpawn > 9000) {

    lastEnemySpawn = millis();

    if (random(0, 2) == 1)
      spawnEnemy();
  }

  if (millis() - lastEnemyMove < 700)
    return;

  lastEnemyMove = millis();

  int px =
    nilsX / TILE;

  int py =
    nilsY / TILE;

  for (int e = 0;
       e < MAX_ENEMIES;
       e++) {

    if (!enemies[e].alive)
      continue;

    int ex =
      enemies[e].x / TILE;

    int ey =
      enemies[e].y / TILE;

    int distance =
      tileDistance(
        ex,
        ey,
        px,
        py
      );

    if (distance > 10)
      continue;

    int nx = ex;
    int ny = ey;

    if (abs(px - ex) >
        abs(py - ey)) {

      if (px > ex)
        nx++;
      else if (px < ex)
        nx--;

    } else {

      if (py > ey)
        ny++;
      else if (py < ey)
        ny--;
    }

    if (nx == px &&
        ny == py) {

      continue;
    }

    byte terrain =
      getTerrain(nx, ny);

    if (terrain == WATER ||
        terrain == MOUNTAIN ||
        terrain == ROCK ||
        terrain == TREE ||
        terrain == HOUSE ||
        terrain == SHOP ||
        terrain == INN) {

      continue;
    }

    bool occupied = false;

    for (int other = 0;
         other < MAX_ENEMIES;
         other++) {

      if (other == e)
        continue;

      if (enemies[other].alive &&
          enemies[other].x / TILE == nx &&
          enemies[other].y / TILE == ny) {

        occupied = true;
        break;
      }
    }

    if (occupied)
      continue;

    enemies[e].x =
      nx * TILE;

    enemies[e].y =
      ny * TILE;
  }
}

// =====================================================
// DRAW ENEMIES
// =====================================================

void drawEnemies() {

  for (int e = 0;
       e < MAX_ENEMIES;
       e++) {

    if (!enemies[e].alive)
      continue;

    int x =
      enemies[e].x - cameraX;

    int y =
      enemies[e].y - cameraY;

    if (x < -10 ||
        x > SCREEN_WIDTH + 10 ||
        y < -12 ||
        y > SCREEN_HEIGHT + 10) {

      continue;
    }

    display.fillRect(
      x - 3,
      y - 3,
      7,
      7,
      SSD1306_WHITE
    );

    display.drawPixel(
      x - 2,
      y - 1,
      SSD1306_BLACK
    );

    display.drawPixel(
      x + 2,
      y - 1,
      SSD1306_BLACK
    );

    display.drawRect(
      x - 6,
      y - 9,
      13,
      3,
      SSD1306_WHITE
    );

    int bar = 0;

    if (enemies[e].maxHP > 0) {

      bar =
        (enemies[e].hp * 9) /
        enemies[e].maxHP;
    }

    if (bar > 0) {

      display.fillRect(
        x - 4,
        y - 8,
        bar,
        1,
        SSD1306_WHITE
      );
    }
  }
}

// =====================================================
// HUD
// =====================================================

void drawHUD() {

  display.fillRect(
    0,
    0,
    128,
    18,
    SSD1306_BLACK
  );

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(2, 1);

  display.print("LV");
  display.print(level);

  display.setCursor(25, 1);

  display.print("$");
  display.print(coins);

  display.setCursor(50, 1);

  display.print("HP");
  display.print(hp);

  display.drawRect(
    2,
    10,
    60,
    6,
    SSD1306_WHITE
  );

  int hpBar = 0;

  if (maxHP > 0) {

    hpBar =
      (hp * 56) /
      maxHP;
  }

  if (hpBar > 0) {

    display.fillRect(
      4,
      12,
      hpBar,
      2,
      SSD1306_WHITE
    );
  }

  display.drawRect(
    66,
    10,
    60,
    6,
    SSD1306_WHITE
  );

  int xpBar = 0;

  if (xpNeeded > 0) {

    xpBar =
      (xp * 56) /
      xpNeeded;
  }

  if (xpBar > 0) {

    display.fillRect(
      68,
      12,
      xpBar,
      2,
      SSD1306_WHITE
    );
  }
}

// =====================================================
// BATTLE SCREEN
// =====================================================

void drawBattle() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.drawRect(
    1,
    1,
    126,
    62,
    SSD1306_WHITE
  );

  display.setCursor(
    43,
    4
  );

  display.print("BATTLE");

  if (battleEnemy < 0 ||
      battleEnemy >= MAX_ENEMIES) {

    display.display();
    return;
  }

  Enemy &enemy =
    enemies[battleEnemy];

  display.fillRect(
    25,
    22,
    12,
    12,
    SSD1306_WHITE
  );

  display.drawPixel(
    28,
    26,
    SSD1306_BLACK
  );

  display.drawPixel(
    34,
    26,
    SSD1306_BLACK
  );

  display.fillRect(
    91,
    20,
    16,
    16,
    SSD1306_WHITE
  );

  display.drawPixel(
    95,
    25,
    SSD1306_BLACK
  );

  display.drawPixel(
    103,
    25,
    SSD1306_BLACK
  );

  display.setCursor(
    68,
    40
  );

  display.print("FOE ");
  display.print(enemy.hp);
  display.print("/");
  display.print(enemy.maxHP);

  display.setCursor(
    5,
    40
  );

  display.print("YOU ");
  display.print(hp);
  display.print("/");
  display.print(maxHP);

  display.setCursor(
    5,
    52
  );

  if (battleMessage != 0 &&
      millis() < battleMessageTime) {

    if (battleMessage == 1) {

      display.print("ATTACK ");
      display.print(battleDamage);
      display.print(" DMG");
    }

    else if (battleMessage == 2) {

      display.print("HIT ");
      display.print(battleDamage);
      display.print(" DMG");
    }

    else if (battleMessage == 3) {

      display.print("HEAL +");
      display.print(battleDamage);
    }

    else if (battleMessage == 4) {

      display.print("ESCAPED!");
    }

    else if (battleMessage == 5) {

      display.print("VICTORY! +");
      display.print(battleDamage);
    }

    else if (battleMessage == 6) {

      display.print("DEFEATED!");
    }

  } else {

    if (playerTurn)
      display.print("R=ATTACK U=HEAL D=RUN");
    else
      display.print("ENEMY TURN...");
  }

  display.display();
}

// =====================================================
// SHOP SCREEN
// =====================================================

void drawShop() {

  display.clearDisplay();

  display.drawRect(
    2,
    2,
    124,
    60,
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(
    43,
    5
  );

  display.print("SHOP");

  display.setCursor(
    8,
    16
  );

  display.print("MONEY: $");
  display.print(coins);

  display.setCursor(
    8,
    26
  );

  display.print("CURRENT: ");

  if (weaponLevel == 1)
    display.print("WOOD");
  else if (weaponLevel == 2)
    display.print("IRON");
  else
    display.print("STEEL");

  display.setCursor(
    8,
    37
  );

  if (shopItem == 1)
    display.print("> WOOD  $10");
  else
    display.print("  WOOD  $10");

  display.setCursor(
    8,
    46
  );

  if (shopItem == 2)
    display.print("> IRON  $25");
  else
    display.print("  IRON  $25");

  display.setCursor(
    8,
    55
  );

  if (shopItem == 3)
    display.print("> STEEL $50");
  else
    display.print("  STEEL $50");

  display.setCursor(
    91,
    16
  );

  if (shopMessage == 1 &&
      millis() - shopMessageTime < 1000) {

    display.print("BOUGHT!");

  } else if (shopMessage == 2 &&
             millis() - shopMessageTime < 1000) {

    display.print("NEED $");

    int need =
      weaponCost(shopItem) - coins;

    if (need < 0)
      need = 0;

    display.print(need);

  } else if (shopItem <= weaponLevel) {

    display.print("OWNED");
  }

  // IMPORTANT: actually send shop screen to OLED
  display.display();
}

// =====================================================
// SHOP UPDATE
// =====================================================

void updateShop() {

  if (leftPressed) {

    if (shopItem > 1)
      shopItem--;
  }

  if (rightPressed) {

    if (shopItem < 3)
      shopItem++;
  }

  if (upPressed) {

    if (shopItem <= weaponLevel) {

      shopMessage = 2;
      shopMessageTime = millis();

    } else if (shopItem == weaponLevel + 1) {

      int cost =
        weaponCost(shopItem);

      if (coins >= cost) {

        coins -= cost;

        weaponLevel =
          shopItem;

        shopMessage = 1;
        shopMessageTime = millis();

      } else {

        shopMessage = 2;
        shopMessageTime = millis();
      }
    }
  }

  // DOWN = EXIT
  if (downPressed) {

    shopOpen = false;

    shopMessage = 0;
  }
}

// =====================================================
// INN
// =====================================================

bool nearInn() {

  int doorX =
    (INN_X + 2) * TILE;

  int doorY =
    (INN_Y + INN_H - 1) * TILE;

  return
    abs(nilsX - doorX) <= 10 &&
    abs(nilsY - doorY) <= 10;
}

void sleepAtInn() {

  display.clearDisplay();

  display.setTextSize(1);

  display.setCursor(
    37,
    25
  );

  display.print("RESTING...");

  display.display();

  delay(600);

  hp = maxHP;

  display.clearDisplay();

  display.setCursor(
    34,
    25
  );

  display.print("HP RESTORED!");

  display.display();

  delay(700);
}

// =====================================================
// SHOP TOUCH
// =====================================================

bool nearShop() {

  int doorX =
    (SHOP_X + 2) * TILE;

  int doorY =
    (SHOP_Y + SHOP_H - 1) * TILE;

  return
    abs(nilsX - doorX) <= 12 &&
    abs(nilsY - doorY) <= 12;
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  pinMode(
    BTN_UP,
    INPUT_PULLUP
  );

  pinMode(
    BTN_DOWN,
    INPUT_PULLUP
  );

  pinMode(
    BTN_LEFT,
    INPUT_PULLUP
  );

  pinMode(
    BTN_RIGHT,
    INPUT_PULLUP
  );

  randomSeed(
    analogRead(A0)
  );

  for (int e = 0;
       e < MAX_ENEMIES;
       e++) {

    enemies[e].alive = false;
  }

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C)) {

    while (true);
  }

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.setCursor(
    32,
    20
  );

  display.print("NILS RPG");

  display.setCursor(
    25,
    34
  );

  display.print("ADVENTURE");

  display.display();

  delay(1000);

  updateCamera();
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  readButtons();

  // ===================================================
  // BATTLE MODE
  // ===================================================

  if (battleActive) {

    updateBattle();

    if (battleActive)
      drawBattle();

    delay(30);

    return;
  }

  // ===================================================
  // SHOP MODE
  // ===================================================

  if (shopOpen) {

    updateShop();

    if (shopOpen)
      drawShop();

    delay(30);

    return;
  }

  // ===================================================
  // SHOP TOUCH TO OPEN
  // ===================================================

  // Unlock shop again after walking away
  if (!nearShop())
    shopTouchLock = false;

  // Open shop when touching the door
  if (nearShop() && !shopTouchLock) {

    shopOpen = true;
    shopTouchLock = true;

    shopMessage = 0;

    shopItem = weaponLevel + 1;

    if (shopItem > 3)
      shopItem = 3;

    return;
  }

  // ===================================================
  // INN
  // ===================================================

  if (upPressed && nearInn()) {

    sleepAtInn();

    return;
  }

  // ===================================================
  // FAST MOVEMENT
  // ===================================================

  if (upNow) {

    moveOrAttack(1);

  } else if (downNow) {

    moveOrAttack(0);

  } else if (leftNow) {

    moveOrAttack(2);

  } else if (rightNow) {

    moveOrAttack(3);
  }

  // ===================================================
  // ENEMIES
  // ===================================================

  updateEnemies();

  // ===================================================
  // CAMERA
  // ===================================================

  updateCamera();

  // ===================================================
  // DRAW
  // ===================================================

  display.clearDisplay();

  drawWorld();

  drawSmallSigns();

  drawEnemies();

  drawNils();

  drawHUD();

  display.display();

  delay(25);
}

