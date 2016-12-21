#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

#include "ShaderProgram.h"
#include "Matrix.h"
#include <vector>
#include <algorithm>
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

using namespace std;

#define FIXED_TIMESTEP 0.0166666f
#define MAX_TIMESTEPS 6
#define SPRITE_COUNT_X 30
#define SPRITE_COUNT_Y 16
#define TILE_SIZE 0.2f

unsigned char **levelData;

ShaderProgram *program;
Matrix projectionMatrix;
Matrix modelMatrix;
Matrix viewMatrix;

int mapHeight;
int mapWidth;

GLuint sheet;

SDL_Window* displayWindow;

GLuint LoadTexture(const char *image_path, int type = GL_RGBA){
	SDL_Surface *surface = IMG_Load(image_path);

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, type, GL_UNSIGNED_BYTE, surface->pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	SDL_FreeSurface(surface);

	return textureID;
}
void worldToTileCoordinates(float worldX, float worldY, int *gridX, int *gridY)
{
	*gridX = (int)(worldX / TILE_SIZE);
	*gridY = (int)(-worldY / TILE_SIZE);
}

int buttonCount = 4;
int keyCount = 4;
std::vector<float> vertexData;
std::vector<float> texCoordData;
void makeMap(){
	vertexData.clear();
	texCoordData.clear();
	for (int y = 0; y < mapHeight; y++){
		for (int x = 0; x < mapWidth; x++){
			if (levelData[y][x] != 0){
				float u = (float)(((int)levelData[y][x]) % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X;
				float v = (float)(((int)levelData[y][x]) / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y;

				float spriteWidth = 1.0f / (float)SPRITE_COUNT_X;
				float spriteHeight = 1.0f / (float)SPRITE_COUNT_Y;

				vertexData.insert(vertexData.end(), {
					TILE_SIZE * x, -TILE_SIZE * y,
					TILE_SIZE * x, (-TILE_SIZE * y) - TILE_SIZE,
					(TILE_SIZE * x) + TILE_SIZE, (-TILE_SIZE * y) - TILE_SIZE,

					TILE_SIZE * x, -TILE_SIZE * y,
					(TILE_SIZE * x) + TILE_SIZE, (-TILE_SIZE * y) - TILE_SIZE,
					(TILE_SIZE * x) + TILE_SIZE, -TILE_SIZE * y
				});

				texCoordData.insert(texCoordData.end(), {
					u, v,
					u, v + (spriteHeight),
					u + spriteWidth, v + (spriteHeight),

					u, v,
					u + spriteWidth, v + (spriteHeight),
					u + spriteWidth, v
				});
			}
		}
	}
}

Mix_Chunk *pick;
Mix_Chunk *jump;

enum objecttype{ hero, enemy, hazard, rock, jumpPad, keys, openButtons };

//-------------------------------------------------

class GameObject{
public:
	objecttype type;

	int spriteLocation;
	float positionX;
	float positionY;
	float velocityX;
	float velocityY;

	float gravity;

	Matrix objectMatrix;

	float direction;
	bool jumper;
	bool superJump;
	bool win;

	float accelerationY;
	bool SocialStatus;  //This is only for the heroObject, villians don't have to worry about getting killed
	bool pursuit;

	bool isSolid(int index, int xPos, int yPos){
		//walls, floor, ceiling
		if (index == 32) return true;
		else if (index == 1) return true;
		else if (index == 2) return true;
		else if (index == 152) return true;
		else if (index == 34) return true;
		else if (index == 36) return true;
		else if (index == 37) return true;
		else if (index == 224) return true;
		else if (index == 225) return true;
		else if (index == 194) return true;
		else if (index == 195) return true;
		else if (index == 108) {
			superJump = true; return true;
		}
		//spikes
		else if (index == 70){
			SocialStatus = false;
			return true;
		}
		else if (index == 130){
			win = true;
		}
		// //button values are 74, 75, 104, 105    off switches
		//on switches are 134, 135, 164, 165
		else if (index == 74) return true;
		else if (index == 75) return true;
		else if (index == 104) return true;
		else if (index == 105) return true;
		else if (index == 134) return true;
		else if (index == 135) return true;
		else if (index == 164) return true;
		else if (index == 165) return true;
		else{
			return false;
		}
	}

	void keysAndLock(int index, int xPos, int yPos){
		if (index == 44){
			Mix_PlayChannel(-1, pick, 0);
			levelData[yPos][xPos] = 0;
			for (int y = 0; y < mapHeight; y++){
				for (int x = 0; x < mapWidth; x++){
					if (levelData[y][x] == 224){
						levelData[y][x] = 0;
						makeMap();
						break;
					}
				}
			}
			keyCount--;
		}
		else if (index == 14){
			Mix_PlayChannel(-1, pick, 0);
			levelData[yPos][xPos] = 0;
			for (int y = 0; y < mapHeight; y++){
				for (int x = 0; x < mapWidth; x++){
					if (levelData[y][x] == 194){
						levelData[y][x] = 0;
						makeMap();
						break;
					}
				}
			}
			keyCount--;
		}
		else if (index == 45){
			Mix_PlayChannel(-1, pick, 0);
			levelData[yPos][xPos] = 0;
			for (int y = 0; y < mapHeight; y++){
				for (int x = 0; x < mapWidth; x++){
					if (levelData[y][x] == 225){
						levelData[y][x] = 0;
						makeMap();
						break;
					}
				}
			}
			keyCount--;
		}
		else if (index == 15){
			Mix_PlayChannel(-1, pick, 0);
			levelData[yPos][xPos] = 0;
			for (int y = 0; y < mapHeight; y++){
				for (int x = 0; x < mapWidth; x++){
					if (levelData[y][x] == 195){
						levelData[y][x] = 0;
						makeMap();
						break;
					}
				}
			}
			keyCount--;
		}
	}
	void trapDoor(int index, int xPos, int yPos){
		if (index == 396){
			levelData[yPos][xPos] = 0;
			makeMap();
		}
	}
	// //button values are 74, 75, 104, 105    off switches
	//on switches are 134, 135, 164, 165
	void buttons(int index, int xPos, int yPos){
		if (index == 74){
			Mix_PlayChannel(-1, pick, 0);
			levelData[yPos][xPos] = 134;
			buttonCount--;
			makeMap();
		}
		else if (index == 75){
			Mix_PlayChannel(-1, pick, 0);
			levelData[yPos][xPos] = 135;
			buttonCount--;
			makeMap();
		}
		else if (index == 104){
			Mix_PlayChannel(-1, pick, 0);
			levelData[yPos][xPos] = 164;
			buttonCount--;
			makeMap();
		}
		else if (index == 105){
			Mix_PlayChannel(-1, pick, 0);
			levelData[yPos][xPos] = 165;
			buttonCount--;
			makeMap();
		}
	}

	GameObject(){}
	GameObject(string ty, float xPos, float yPos){
		gravity = -2.0f;
		SocialStatus = true;
		pursuit = false;
		direction = 0.0f;
		jumper = false;
		superJump = false;
		win = false;
		if (ty == "Player"){
			type = hero;
			spriteLocation = 19;
			velocityX = 0.75f;
		}
		else if (ty == "Enemy"){
			type = enemy;
			spriteLocation = 79;
			velocityX = 0.5f;
		}
		positionX = xPos;
		positionY = yPos;
		velocityY = 0.0f;
	}
	bool collide(float xPos, float yPos){
		if (((positionX + TILE_SIZE / 2 >= xPos - TILE_SIZE / 2) && (positionX + TILE_SIZE / 2 <= xPos + TILE_SIZE / 2)) &&
			(positionY - TILE_SIZE / 2 <= yPos + TILE_SIZE / 2) && (positionY + TILE_SIZE/2 >= yPos - TILE_SIZE/2)){
			return true;
		}
		else if (((positionX - TILE_SIZE / 2 >= xPos - TILE_SIZE / 2) && (positionX - TILE_SIZE / 2 <= xPos + TILE_SIZE / 2)) &&
			(positionY - TILE_SIZE / 2 <= yPos + TILE_SIZE / 2) && (positionY + TILE_SIZE / 2 >= yPos - TILE_SIZE / 2)){
			return true;
		}

		return false;
	}
	void collisionX(){
		int gridX = 0;
		int gridY = 0;
		float penetration_x = 0.0f;
		//Right
		worldToTileCoordinates(positionX + TILE_SIZE / 2, positionY, &gridX, &gridY);
		if (isSolid(levelData[gridY][gridX], gridX, gridY)){
			if (levelData[gridY][gridX] != 108) superJump = false;
			buttons(levelData[gridY][gridX], gridX, gridY);
			penetration_x = (positionX + TILE_SIZE / 2) - (TILE_SIZE * gridX);
			positionX -= (penetration_x + 0.003);
		}
		keysAndLock(levelData[gridY][gridX], gridX, gridY);
		//Left
		worldToTileCoordinates(positionX - TILE_SIZE / 2, positionY, &gridX, &gridY);
		if (isSolid(levelData[gridY][gridX], gridX, gridY)){
			if (levelData[gridY][gridX] != 108) superJump = false;
			buttons(levelData[gridY][gridX], gridX, gridY);
			penetration_x = (TILE_SIZE * gridX) + TILE_SIZE - (positionX - TILE_SIZE / 2);
			positionX += (penetration_x + 0.003);
		}
		keysAndLock(levelData[gridY][gridX], gridX, gridY);
	}
	void collisionY(){
		int gridX = 0;
		int gridY = 0;
		float penetration_y = 0.0f;
		//bottom
		worldToTileCoordinates(positionX, positionY - TILE_SIZE/2, &gridX, &gridY);
		if (isSolid(levelData[gridY][gridX], gridX, gridY)){
			if (levelData[gridY][gridX] == 108) superJump = true;
			else superJump = false;
			buttons(levelData[gridY][gridX], gridX, gridY);
			penetration_y = (-TILE_SIZE * gridY) - (positionY - TILE_SIZE / 2);
			positionY += (penetration_y + 0.002);
			switchOn = false;
			jumper = false;
		}
		keysAndLock(levelData[gridY][gridX], gridX, gridY);
		//top
		worldToTileCoordinates(positionX, positionY + TILE_SIZE / 2, &gridX, &gridY);
		if (isSolid(levelData[gridY][gridX], gridX, gridY)){
			//penetration_y = (positionY + TILE_SIZE / 2) - (-TILE_SIZE*gridY);
			velocityY = 0.0f;
			positionY -= (penetration_y + 0.003);
		}
		keysAndLock(levelData[gridY][gridX], gridX, gridY);
	}

	bool inSight(float& playerVarX, float& playerVarY){
		float leftEdge = positionX - (0.5 + TILE_SIZE/2);
		float rightEdge = positionX + (0.5 + TILE_SIZE/2);
		float topEdge = positionY + (0.5 + TILE_SIZE/2);
		float bottomEdge = positionY - (0.5 + TILE_SIZE/2);

		if (playerVarX < rightEdge && playerVarX > leftEdge){
			if (playerVarY < topEdge && playerVarY > bottomEdge){
				return true;
			}
			if (playerVarY < topEdge && playerVarY > bottomEdge){
				return true;
			}
		}
		else if (playerVarX + 0.2 > leftEdge && playerVarX + 0.2 < rightEdge){
			if (playerVarY < topEdge && playerVarY > bottomEdge){
				return true;
			}
			if (playerVarY < topEdge && playerVarY > bottomEdge){
				return true;
			}
		}
		return false;
	}
	bool switchOn = false;
	void pursue(float playerVarX, float playerVarY, float elapsed){    //
		direction = 0.0f;
		if (inSight(playerVarX, playerVarY)){
			if (playerVarX > positionX){
				direction = 1.0f;
				jumper = false;
				velocityY = 0.0f;
			}
			else if (playerVarX < positionX){
				direction = -1.0f;
				velocityY = 0.0f;
				jumper = false;
			}
		}
		else{
			if (switchOn == false){
				if (jumper == false){
					velocityY = 0.1;
					switchOn = true;
					jumper = true;
				}
			}
			else if (switchOn == true){
				velocityY += elapsed*gravity;
			}
		}
		velocityY += elapsed*gravity;
		positionY += velocityY*elapsed;
		collisionY();

		positionX += velocityX*direction*elapsed;
		collisionX();
	}

	void update(float elapsed){
		const Uint8 *keys = SDL_GetKeyboardState(NULL);
		direction = 0.0f;
		if (keys[SDL_SCANCODE_W]){
			if (jumper != true){
				Mix_PlayChannel(-1, jump, 0);
				velocityY = 2.5;
				if (superJump == true){
					velocityY = 4.0f;
					superJump = false;
				}
				gravity = -2.0f;
				jumper = true;
			}
		}
		if (keys[SDL_SCANCODE_D]){
			direction = 1.0f;
		}
		if (keys[SDL_SCANCODE_A]){
			direction = -1.0f;
		}

		velocityY += gravity*elapsed;
		positionY += velocityY*elapsed;
		collisionY();
		positionX += velocityX*direction*elapsed;
		collisionX();
	}
	void draw(){    // Function that is responsible for drawing the character
		float u = (float)((spriteLocation) % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X;
		float v = (float)((spriteLocation) / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y;

		float spriteWidth = 1.0f / (float)SPRITE_COUNT_X;
		float spriteHeight = 1.0f / (float)SPRITE_COUNT_Y;

		GLfloat texCoords[] = {
			u, v + spriteHeight,
			u + spriteWidth, v,
			u, v,
			u + spriteWidth, v,
			u, v + spriteHeight,
			u + spriteWidth, v + spriteHeight
		};

		float vertices[] = {
			-0.5f*TILE_SIZE, -0.5f*TILE_SIZE,
			0.5f*TILE_SIZE, 0.5f*TILE_SIZE,
			-0.5f*TILE_SIZE, 0.5f*TILE_SIZE,
			0.5f*TILE_SIZE, 0.5f*TILE_SIZE,
			-0.5f*TILE_SIZE, -0.5f*TILE_SIZE,
			0.5f*TILE_SIZE, -0.5f*TILE_SIZE
		};

		program->setModelMatrix(objectMatrix);
		objectMatrix.identity();
		objectMatrix.Translate(positionX, positionY, 0.0f);

		glUseProgram(program->programID);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program->positionAttribute);
		glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
		glEnableVertexAttribArray(program->texCoordAttribute);

		glBindTexture(GL_TEXTURE_2D, sheet);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glDisableVertexAttribArray(program->positionAttribute);
		glDisableVertexAttribArray(program->texCoordAttribute);
	}
};

GameObject player;
vector<GameObject> enemies;

void placeEntity(string type, float x, float y){
	if (type == "Player"){
		player = GameObject(type, x, y);
	}
	else if (type == "Enemy"){
		enemies.push_back(GameObject(type, x, y));
	}
}

bool readHeader(std::ifstream &stream){
	string line = "";
	mapWidth = -1;
	mapHeight = -1;
	while (getline(stream, line)){
		if (line == ""){ break; }

		istringstream sStream(line);
		string key, value;
		getline(sStream, key, '=');
		getline(sStream, value);

		if (key == "width"){
			mapWidth = atoi(value.c_str());
		}
		else if (key == "height"){
			mapHeight = atoi(value.c_str());
		}
	}

	if (mapWidth == -1 || mapHeight == -1){
		return false;
	}
	else{
		levelData = new unsigned char*[mapHeight];
		for (int i = 0; i < mapHeight; ++i){
			levelData[i] = new unsigned char[mapWidth];
		}
		return true;
	}
}
bool readLayerData(std::ifstream &stream){
	string line = "";
	while (getline(stream, line)){
		if (line == ""){ break; }
		istringstream sStream(line);
		string key, value;
		getline(sStream, key, '=');
		getline(sStream, value);
		if (key == "data"){
			for (int y = 0; y < mapHeight; y++){
				getline(stream, line);
				istringstream lineStream(line);
				string tile;

				for (int x = 0; x < mapWidth; x++){
					getline(lineStream, tile, ',');
					unsigned char val = (unsigned char)atoi(tile.c_str());
					if (val > 0){
						levelData[y][x] = val - 1;
					}
					else{
						levelData[y][x] = 0;
					}
				}
			}
		}
	}
	return true;
}
bool readEntityData(std::ifstream &stream){
	string line = "";
	string type = "";

	while (getline(stream, line)){
		if (line == ""){ break; }

		istringstream sStream(line);
		string key, value;
		getline(sStream, key, '=');
		getline(sStream, value);

		if (key == "type"){
			type = value;
		}
		else if (key == "location"){
			istringstream lineStream(value);
			string xPosition, yPosition;
			getline(lineStream, xPosition, ',');
			getline(lineStream, yPosition, ',');

			float placeX = (float)atoi(xPosition.c_str()) * TILE_SIZE;
			float placeY = (float)atoi(yPosition.c_str()) * -TILE_SIZE + 0.3;

			placeEntity(type, placeX, placeY);
		}
	}
	return true;
}
void DrawText(ShaderProgram *program, GLuint fontTexture, std::string text, float size, float spacing){
	float texture_size = 1.0 / 16.0f;
	std::vector<float> vertexData1;
	std::vector<float> texCoordData1;

	for (int i = 0; i < text.size(); i++){
		float texture_x = (float)(((int)text[i]) % 16) / 16.0f;
		float texture_y = (float)(((int)text[i]) / 16) / 16.0f;
		vertexData1.insert(vertexData1.end(), {
			((size + spacing) * i) + (-0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size - 0.2f,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (0.5f * size), -0.5f * size - 0.2f,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size - 0.2f,
		});
		texCoordData1.insert(texCoordData1.end(), {
			texture_x, texture_y,
			texture_x, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x + texture_size, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x, texture_y + texture_size,
		});
	}
	glUseProgram(program->programID);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData1.data());
	glEnableVertexAttribArray(program->positionAttribute);
	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData1.data());
	glEnableVertexAttribArray(program->texCoordAttribute);
	glBindTexture(GL_TEXTURE_2D, fontTexture);
	glDrawArrays(GL_TRIANGLES, 0, text.size() * 6);

	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}
void readFile(string file){
	ifstream infile(file);
	string line;
	while (getline(infile, line)){
		if (line == "[header]"){
			if (!readHeader(infile)){
				break;
			}
		}
		else if (line == "[layer]"){
			readLayerData(infile);
		}
		else if (line == "[Object Layer 1]"){
			readEntityData(infile);
		}
	}
}

void drawMap(GLuint sheet){
	program->setModelMatrix(modelMatrix);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(program->programID);
	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program->positionAttribute);
	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(program->texCoordAttribute);

	modelMatrix.identity();
	program->setModelMatrix(modelMatrix);

	glBindTexture(GL_TEXTURE_2D, sheet);
	glDrawArrays(GL_TRIANGLES, 0, vertexData.size() / 2);
	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}

void centerPlayer(){
	viewMatrix.identity();
	viewMatrix.Scale(2.0f, 2.0f, 1.0f);
	viewMatrix.Translate(-player.positionX, -player.positionY, 0.0f);
	program->setViewMatrix(viewMatrix);
}
void screenShake(float& shake){
		viewMatrix.identity();
		centerPlayer();
		viewMatrix.Translate(0.02f*shake, 0.002f*shake*-1.0f, 0.0f);
		shake *= -1.0f;
}
void render(){
	player.draw();
	for (int i = 0; i < enemies.size(); i++){
		enemies[i].draw();
	}
}
void update(float elapsed){
	player.update(elapsed);
	for (int i = 0; i < enemies.size(); i++){
		enemies[i].pursue(player.positionX, player.positionY, elapsed);
		if (player.collide(enemies[i].positionX, enemies[i].positionY)){
			player.SocialStatus = false;
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	#ifdef _WINDOWS
		glewInit();
	#endif

		glViewport(0, 0, 640, 360);
		GLuint font = LoadTexture("font2.png");
		sheet = LoadTexture("spritesheet.png", GL_RGB);
		GLuint background = LoadTexture("background.png");
		GLuint winScreen = LoadTexture("win.png");
		GLuint loseScreen = LoadTexture("lose.png");

		float count = 4;
		float shake = 1.0f;

		program = new ShaderProgram(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");

		Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);

		Mix_Music *music;
		music = Mix_LoadMUS("music.mp3"); 

		pick = Mix_LoadWAV("pickup.wav");
		jump = Mix_LoadWAV("jump.wav");

		float lastFrameTicks = 0.0f;
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		lastFrameTicks = ticks;

		projectionMatrix.setOrthoProjection(-3.55f, 3.55f, -4.0f, 4.0f, -1.0f, 1.0f);
		enum GameState { STATE_MENU, STATE_LEVEL_ONE, STATE_LEVEL_TWO, STATE_LEVEL_THREE, STATE_WIN, STATE_LOSE };
		int state = STATE_MENU;

		if (state == STATE_MENU){
			readFile("FirstTiledMap.txt");
		}
		else if (state == STATE_LEVEL_TWO){
			readFile("SecondTiledMap.txt");
		}
		else if (state == STATE_LEVEL_THREE){
			readFile("ThirdTileMap.txt");
		}
		float screenShakeValue = 0.0f;
		float screenShakeSpeed = 5.0f;
		float screenShakeIntensity = 0.5f;
		makeMap();
		glUseProgram(program->programID);
		Mix_PlayMusic(music, -1);
	
	SDL_Event event;
	bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;
		glClear(GL_COLOR_BUFFER_BIT);

		program->setProjectionMatrix(projectionMatrix);
		program->setViewMatrix(viewMatrix);
		program->setModelMatrix(modelMatrix);

		glEnable(GL_BLEND);

		const Uint8 *keys = SDL_GetKeyboardState(NULL);
		float x = 0;
		float y = 0;
		float vertices[] = { -3.55f, -4.0f, 3.55f, -4.0f, 3.55f, 4.0f, -3.55f, -4.0f, 3.55f, 4.0f, -3.55f, 4.0f };
		float texCoords[] = { 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0 };
		float fixedElapsed = elapsed;

		switch (state){
		case STATE_MENU:
			program->setModelMatrix(modelMatrix);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			glUseProgram(program->programID);
			glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
			glEnableVertexAttribArray(program->positionAttribute);
			glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
			glEnableVertexAttribArray(program->texCoordAttribute);

			modelMatrix.identity();
			program->setModelMatrix(modelMatrix);

			glBindTexture(GL_TEXTURE_2D, background);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glDisableVertexAttribArray(program->positionAttribute);
			glDisableVertexAttribArray(program->texCoordAttribute);

			modelMatrix.identity();
			modelMatrix.Translate(-1.4f, 2.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "MAZE CAVE CRAZE", 0.2f, 0.001f);

			modelMatrix.identity();
			modelMatrix.Translate(-1.7f, 1.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "PRESS \"E\" TO BEGIN", 0.2f, 0.00001f);

			modelMatrix.identity();
			modelMatrix.Translate(-1.4f, 0.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "\"A\",\"D\" TO MOVE", 0.2f, 0.00001f);

			modelMatrix.identity();
			modelMatrix.Translate(-1.0f, -1.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "\"W\" TO JUMP", 0.2f, 0.00001f);

			modelMatrix.identity();
			modelMatrix.Translate(-1.0f, -2.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "\"Z\" TO QUIT", 0.2f, 0.00001f);
			if (keys[SDL_SCANCODE_Q]){
				state = STATE_LOSE;
			}
			else if (keys[SDL_SCANCODE_E]){
				state = STATE_LEVEL_ONE;
			}
			break;

		case(STATE_LEVEL_ONE) :
			if (player.SocialStatus == false){
				screenShakeValue = 0.0f;
				state = STATE_LOSE;
				break;
			}
			if (player.win == true){
				player.win = false;
				count = 4;
				screenShakeValue = 0.0f;
				readFile("SecondTiledMap.txt");
				screenShakeValue = 0.0f;
				makeMap();
				state = STATE_LEVEL_TWO;
				break;
			}
			if (keys[SDL_SCANCODE_Z]){
				screenShakeValue = 0.0f;
				state = STATE_LOSE;
				break;
			}
			if (fixedElapsed > FIXED_TIMESTEP * MAX_TIMESTEPS){
				fixedElapsed = FIXED_TIMESTEP * MAX_TIMESTEPS;
			}
			while (fixedElapsed >= FIXED_TIMESTEP){
				fixedElapsed -= FIXED_TIMESTEP;
				update(FIXED_TIMESTEP);
			}
			update(fixedElapsed);
			drawMap(sheet);
			centerPlayer();
			render();
				screenShake(shake);
			break;
		case(STATE_LEVEL_TWO) :
			if (player.SocialStatus == false){
				screenShakeValue = 0.0f;
				state = STATE_LOSE;
				break;
			}
			else if (player.win == true){
				player.win = false;
				count = 4;
				keyCount = 4;
				screenShakeValue = 0.0f;
				readFile("ThirdTileMap.txt");
				vertexData.clear();
				texCoordData.clear();
				makeMap();
				state = STATE_LEVEL_THREE;
				break;
			}
			if (keys[SDL_SCANCODE_Z]){
				screenShakeValue = 0.0f;
				state = STATE_LOSE;
				break;
			}
			if (fixedElapsed > FIXED_TIMESTEP * MAX_TIMESTEPS){
				fixedElapsed = FIXED_TIMESTEP * MAX_TIMESTEPS;
			}
			while (fixedElapsed >= FIXED_TIMESTEP){
				fixedElapsed -= FIXED_TIMESTEP;
				update(FIXED_TIMESTEP);
			}
			update(fixedElapsed);
			screenShakeValue += elapsed;
			viewMatrix.Translate(0.0f, sin(screenShakeValue * screenShakeSpeed)*screenShakeIntensity, 0.0f);
			drawMap(sheet);
			centerPlayer();
			render();
				screenShake(shake);
			break;
		case(STATE_LEVEL_THREE) :
			if (buttonCount == 0) player.win = true;
			if (player.SocialStatus == false){
				screenShakeValue = 0.0f;
				state = STATE_LOSE;
				break;
			}
			else if (player.win == true){
				screenShakeValue = 0.0f;
				state = STATE_WIN;
				break;
			}
			if (keys[SDL_SCANCODE_Z]){
				screenShakeValue = 0.0f;
				state = STATE_LOSE;
				break;
			}
			if (fixedElapsed > FIXED_TIMESTEP * MAX_TIMESTEPS){
				fixedElapsed = FIXED_TIMESTEP * MAX_TIMESTEPS;
			}
			while (fixedElapsed >= FIXED_TIMESTEP){
				fixedElapsed -= FIXED_TIMESTEP;
				update(FIXED_TIMESTEP);
			}
			update(fixedElapsed);
			screenShakeValue += elapsed;
			viewMatrix.Translate(0.0f, sin(screenShakeValue * screenShakeSpeed)*screenShakeIntensity, 0.0f);
			drawMap(sheet);
			centerPlayer();
			render();
				screenShake(shake);
			break;
		case(STATE_WIN) :
			viewMatrix.identity();
			viewMatrix.Scale(1.0f, 1.0f, 1.0f);
			program->setViewMatrix(viewMatrix);

			program->setModelMatrix(modelMatrix);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			glUseProgram(program->programID);
			glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
			glEnableVertexAttribArray(program->positionAttribute);
			glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
			glEnableVertexAttribArray(program->texCoordAttribute);

			modelMatrix.identity();
			program->setModelMatrix(modelMatrix);

			glBindTexture(GL_TEXTURE_2D, winScreen);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glDisableVertexAttribArray(program->positionAttribute);
			glDisableVertexAttribArray(program->texCoordAttribute);

			modelMatrix.identity();
			modelMatrix.Translate(-3.0f, 0.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "YOU HAVE ESCAPED", 0.2f, 0.001f);

			modelMatrix.identity();
			modelMatrix.Translate(-3.0f, -1.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "PRESS E TO PLAY AGAIN", 0.2f, 0.001f);

			modelMatrix.identity();
			modelMatrix.Translate(-3.0f, -2.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "PRESS Q TO EXIT", 0.2f, 0.001f);

			if (keys[SDL_SCANCODE_E]){
				keyCount = 4;
				buttonCount = 4;
				count = 4;
				readFile("FirstTiledMap.txt");
				vertexData.clear();
				texCoordData.clear();
				makeMap();
				player.win = false;
				enemies.clear();
				state = STATE_LEVEL_ONE;
			}
			else if (keys[SDL_SCANCODE_Q]){
				vertexData.clear();
				texCoordData.clear();
				enemies.clear();
				Mix_FreeChunk(pick);
				Mix_FreeChunk(jump);
				Mix_FreeMusic(music);
				SDL_Quit();
			}
			break;
		case(STATE_LOSE) :
			viewMatrix.identity();
			viewMatrix.Scale(1.0f, 1.0f, 1.0f);
			program->setViewMatrix(viewMatrix);

			program->setModelMatrix(modelMatrix);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			glUseProgram(program->programID);
			glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
			glEnableVertexAttribArray(program->positionAttribute);
			glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
			glEnableVertexAttribArray(program->texCoordAttribute);

			modelMatrix.identity();
			program->setModelMatrix(modelMatrix);

			glBindTexture(GL_TEXTURE_2D, loseScreen);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glDisableVertexAttribArray(program->positionAttribute);
			glDisableVertexAttribArray(program->texCoordAttribute);

			modelMatrix.identity();
			modelMatrix.Translate(-1.3f, 2.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "YOU HAVE DIED", 0.2f, 0.001f);

			modelMatrix.identity();
			modelMatrix.Translate(-1.7f, 1.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "PRESS E TO RESTART", 0.2f, 0.001f);

			modelMatrix.identity();
			modelMatrix.Translate(-1.3f, 0.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "PRESS Q TO EXIT", 0.2f, 0.001f);

			if (keys[SDL_SCANCODE_E]){
				keyCount = 4;
				buttonCount = 4;
				count = 4;
				enemies.clear();
				player.win = false;
				readFile("FirstTiledMap.txt");
				vertexData.clear();
				texCoordData.clear();
				makeMap();
				state = STATE_LEVEL_ONE;
			}
			else if (keys[SDL_SCANCODE_Q]){
				vertexData.clear();
				texCoordData.clear();
				enemies.clear();
				Mix_FreeChunk(pick);
				Mix_FreeChunk(jump);
				Mix_FreeMusic(music);
				SDL_Quit();
			}
			break;
		}

		SDL_GL_SwapWindow(displayWindow);
	}
	Mix_FreeChunk(pick);
	Mix_FreeChunk(jump);
	Mix_FreeMusic(music);
	SDL_Quit();
	return 0;
}
