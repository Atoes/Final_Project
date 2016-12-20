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

SDL_Window* displayWindow;

GLuint LoadTexture(const char *image_path){
	SDL_Surface *surface = IMG_Load(image_path);

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);

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
void tileToWorldCoordinates(float *worldX, float *worldY, int gridX, int gridY){
	*worldX = (float)(gridX * TILE_SIZE);
	*worldY = (float)(gridY * TILE_SIZE);
}

struct button{
	int locationX;
	int locationY;
	bool On;
	float timer;
};

struct key{
	int spriteValue;
	float positionX;
	float positionY;
};

void generateKeyLocation(vector<key>& keyList){
	if (keyList.size() != 3){
		return;
	}
	int ind1 = int(keyList[0].positionY);
	int ind2 = int(keyList[0].positionX);
	levelData[ind1][ind2] = char(15);

}

bool spikeCollisions(float& varX, float& varY){
	float left = float(int(varX));
	float bot = float(int(varY) + 1);
	float top = bot - 0.3;
	float right = left + 1.0;

	if (left < varX && varX < right){
		if (varY > top && varY < bot){
			return true;
		}
	}
	return false;
}

void buttonSwitch(button& swt){
	levelData[swt.locationY][swt.locationX] = char(int(levelData[swt.locationY][swt.locationX]) + 60);
	swt.On = true;
	swt.timer = 0.0;
}

GLuint sheet;

enum objecttype{ hero, enemy, hazard, rock, jumpPad, keys, openButtons };

//-------------------------------------------------

class gameObjects{
	objecttype type;

	int spriteLocation;
	float positionX;
	float positionY;
	float velocityX;
	float velocityY;

	float gravity;

	Matrix objectMatrix;

	vector<int> keyValues;
	vector<button> buttonLocation;

	float direction;
	bool jumper;
	bool win;

	float accelerationY;
	bool SocialStatus;  //This is only for the heroObject, villians don't have to worry about getting killed
	bool pursuit;

public:
	gameObjects(){}
	gameObjects(string type, float xPos, float yPos){
		gravity = -9.81;
		SocialStatus = true;
		pursuit = false;
		direction = 0.0f;
		jumper = false;
		win = false;
		if (type == "Player"){
			type = hero;
			spriteLocation = 19;
		}
		else if (type == "Enemy"){
			type = enemy;
			spriteLocation = 79;
		}
		positionX = xPos;
		positionY = yPos;
		velocityY = 0.0f;
		velocityX = 0.5f;
	}

	float getXPosition(){ return positionX; }
	float getYPosition(){ return positionY; }

	bool getWinStatus(){ return win; }
	void revertWin(){ win = false; }
	bool getSocialStatus(){ return SocialStatus; }

	bool collide(float& varX, float& varY){
		//varX and varY are the indexes of the matrices, 0.2 is the uniform length of a side of a tile

		if (levelData[int(varY)][int(varX)] == '0'){
			return false;
		}

		float ed1 = float(int(varX));
		float ed2 = float(int(varX) + 1);

		float edy1 = float(int(varY));
		float edy2 = float(int(varY) + 1);

		if (ed1 < varX && varX < ed2){
			if (varY > edy1 && varY < edy2){
				return true;
			}
		}
		return false;
	}

	void fall(){
		jumper = true;
	}

	bool spikeColTwo(float& varX, float& varY){
		float top = float(int(varY)) + 0.6;
		float bot = top + 0.4;

		if (varX > float(int(varX)) && varX < float(int(varX) + 1)){
			if (varY > top && varY < bot){
				return true;
			}
		}
		return false;
	}

	void death(){
		SocialStatus = false;
		return;
	}

	void collisions(){
		//positionX and positionY are both float variables for the table.  WE convert them into values for the vertices.
		float rightEdge = positionX + 1.0;
		float botEdge = positionY + 1.0;

		float botR1Side = positionY + 0.2;
		float botR2Side = positionY + 0.8;

		float topSideRef1 = positionX + 0.2;
		float topSideRef2 = positionX + 0.8;


		//winning
		if (int(levelData[int(botEdge)][int(topSideRef2)]) == 130){
			win = true;
		}

		// jumpPad

		if (int(levelData[int(botEdge)][int(topSideRef2)]) == 233){
			velocityY = 12.0;
			positionY -= 0.0001;
		}

		//button switches
		if (int(levelData[int(botEdge)][int(topSideRef2)]) == 74 || int(levelData[int(botEdge)][int(topSideRef1)]) == 74){
			buttonSwitch(buttonLocation[0]);
		}

		//trapdoor code
		if (int(levelData[int(botEdge)][int(topSideRef2)]) == 38){
			levelData[int(botEdge)][int(topSideRef2)] = char(194);
		}

		//leftCollisions
		if (collide(positionX, botR1Side) || collide(positionX, botR2Side)){
			if (int(levelData[int(botEdge)][int(topSideRef1)]) != 70){
				positionX = float(int(positionX) + 1);
				positionX += 0.00001;
			}
		}

		//rightCollisions
		if (collide(rightEdge, botR1Side) || collide(rightEdge, botR2Side)){
			if (int(levelData[int(botEdge)][int(topSideRef1)]) != 70){
				positionX = float(int(positionX));
				positionX -= 0.00001;
			}
		}

		//topCollisions
		if (collide(topSideRef1, positionY) || collide(topSideRef2, positionY)){
			velocityY = 0.0;
			positionY = float(int(positionY) + 1);
			positionY += 0.002;
		}

		//bottomCollisions
		if (collide(topSideRef1, botEdge) || collide(topSideRef2, botEdge)){
			if (int(levelData[int(botEdge)][int(topSideRef1)]) == 70){
				if (spikeColTwo(topSideRef1, botEdge) && spikeColTwo(topSideRef2, botEdge)){
					death();
				}
			}
			if (int(levelData[int(botEdge)][int(topSideRef1)]) != 70){
				jumper = false;
				positionY = float(int(positionY));
			}
		}

		if (int(levelData[int(botEdge)][int(positionX)]) == 194 && int(levelData[int(botEdge)][int(positionX) + 1]) == 194){
			fall();
		}
	}
	//-------------------------------------------------

	//The villians pursuitFunctions
	bool inSight(float& playerVarX, float& playerVarY){
		float leftEdge = positionX - 3.0;
		float rightEdge = positionX + 4.0;
		float topEdge = positionY + 3.0;
		float bottomEdge = positionY - 2.0;

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

	void patrol(float& elapsed){
		positionX += velocityX*elapsed*direction;
	}

	void pursue(float playerVarX, float playerVarY){    //
		if (inSight(playerVarX, playerVarY)){
			if (playerVarX > positionX){
				direction = 1.0f;
			}
			else if (playerVarX < positionX){
				direction = -1.0f;
			}
		}
		else{
			direction = 0.0f;
		}
	}
	//----------------------

	void jump(){
		if (jumper == false){
			velocityY = 3.0;
			jumper = true;
		}
	}

	void keyPickUp(){
		int rightEdge = int(positionX) + 1;
		int leftEdge = rightEdge - 1;
		int topEdge = int(positionY);
		int botEdge = topEdge + 1;

		int righ = int(positionX) + 2;

		if (int(levelData[int(positionY)][righ]) == 14){
			levelData[int(positionY)][righ] = char(194);
			keyValues.push_back(14);
		}
		if (int(levelData[int(positionY)][righ]) == 15){
			levelData[int(positionY)][righ] = char(194);
			keyValues.push_back(15);
		}
		if (int(levelData[int(positionY)][righ]) == 44){
			levelData[int(positionY)][righ] = char(194);
			keyValues.push_back(44);
		}
		if (int(levelData[int(positionY)][righ]) == 45){
			levelData[int(positionY)][righ] = char(194);
			keyValues.push_back(45);
		}
	}

	void jumpFall(float& elapsed){
		velocityY += elapsed*gravity;
		positionY += velocityY*elapsed - (0.5*gravity*(elapsed*elapsed));
	}

	void objMove(float& elapsed){
		positionX += velocityX*direction*elapsed;
	}



	//-----------------------
	//Movement for GameObject
	void move(float& elapsed){  //function to update positions
		const Uint8 *keys = SDL_GetKeyboardState(NULL);
		direction = 0.0;
		if (this->type == hero){
			if (keys[SDL_SCANCODE_W]){
				jump();
			}

			if (keys[SDL_SCANCODE_D]){
				direction = 1.0;

			}

			if (keys[SDL_SCANCODE_A]){
				direction = -1.0;
			}

			if (keys[SDL_SCANCODE_RETURN]){

			}
			if (keys[SDL_SCANCODE_P]){
				keyPickUp();
			}

			if (jumper){ jumpFall(elapsed); }
			objMove(elapsed);
		}
		//-------------------------------
		else if (type == enemy){
			pursue(player.getXPosition(), player.getYPosition());
			objMove(elapsed);
		}


		//-------------------
		//collision detection
		collisions();
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

gameObjects player;
vector<gameObjects> enemies;

void placeEntity(string type, float x, float y){
	if (type == "Player"){
		player = gameObjects(type, x, y);
	}
	else if (type == "Enemy"){
		enemies.push_back(gameObjects(type, x, y));
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

			float placeX = atoi(xPosition.c_str()) / 30.0f * TILE_SIZE;
			float placeY = atoi(yPosition.c_str()) / 16.0f * -TILE_SIZE;

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
			((size + spacing) * i) + (-0.5f * size), -0.5f * size-0.2f,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (0.5f * size), -0.5f * size-0.2f,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size-0.2f,
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

void render(){
	player.draw();
	for (int i = 0; i < enemies.size(); i++){
		enemies.at(i).draw();
	}
}
void update(float elapsed){
	player.move(elapsed);
	for (int i = 0; i < enemies.size(); i++){
		enemies.at(i).move(elapsed);
	}
}

std::vector<float> vertexData;
std::vector<float> texCoordData;
void makeMap(){
	for (int y = 0; y < mapHeight; y++){
		for (int x = 0; x < mapWidth; x++){
			if (levelData[y][x] != 0){
				float u = (float)(((int)levelData[y][x]) % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X;
				float v = (float)(((int)levelData[y][x]) / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y;

				float spriteWidth = 1.0f / (float)SPRITE_COUNT_X;
				float spriteHeight = 1.0f / (float)SPRITE_COUNT_Y;

				vertexData.clear();
				texCoordData.clear();

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

void centerPlayer(){
	viewMatrix.identity();
	viewMatrix.Scale(2.0f, 2.0f, 1.0f);
	viewMatrix.Translate(-player.getXPosition(), -player.getYPosition(), 0.0f);
	program->setViewMatrix(viewMatrix);
}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1000, 600, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	#ifdef _WINDOWS
		glewInit();
	#endif

	glViewport(0, 0, 1000, 600);
	GLuint font = LoadTexture("font2.png");
	//sheet = LoadTexture("spritesheet.png");
	GLuint background = LoadTexture("background.png");
	GLuint winScreen = LoadTexture("win.png");
	GLuint loseScreen = LoadTexture("lose.png");

	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);

	Mix_Music *music;
	music = Mix_LoadMUS("music.mp3");

	program = new ShaderProgram(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");

	readFile("FirstTiledMapOne.txt");

	makeMap();

	float lastFrameTicks = 0.0f;
	float ticks = (float)SDL_GetTicks() / 1000.0f;
	lastFrameTicks = ticks;

	projectionMatrix.setOrthoProjection(-3.55f, 3.55f, -4.0f, 4.0f, -1.0f, 1.0f);
	enum GameState { STATE_MENU, STATE_LEVEL_ONE, STATE_LEVEL_TWO, STATE_LEVEL_THREE, STATE_WIN, STATE_LOSE };
	int state = STATE_MENU;

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
		case STATE_MENU :
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

			if (keys[SDL_SCANCODE_E]){
				state = STATE_LEVEL_ONE;
			}
			break;

		case(STATE_LEVEL_ONE) :
			if (player.getSocialStatus() == false){
				state = STATE_LOSE;
			}
			else if (player.getWinStatus() == true){
				player.revertWin();
				readFile("SecondTiledMap.txt");
				makeMap();
				state = STATE_LEVEL_TWO;
			}
			drawMap(sheet);
			if (fixedElapsed > FIXED_TIMESTEP * MAX_TIMESTEPS){
				fixedElapsed = FIXED_TIMESTEP * MAX_TIMESTEPS;
			}
			while (fixedElapsed >= FIXED_TIMESTEP){
				fixedElapsed -= FIXED_TIMESTEP;
				update(FIXED_TIMESTEP);
			}
			update(fixedElapsed);

			render();
			break;
		case(STATE_LEVEL_TWO) :
			if (player.getSocialStatus() != false){
				state = STATE_LOSE;
			}
			else if (player.getWinStatus()){
				player.revertWin();
				readFile("ThirdTileMap.txt");
				makeMap();
				state = STATE_LEVEL_THREE;
			}
			drawMap(sheet);
			if (fixedElapsed > FIXED_TIMESTEP * MAX_TIMESTEPS){
				fixedElapsed = FIXED_TIMESTEP * MAX_TIMESTEPS;
			}
			while (fixedElapsed >= FIXED_TIMESTEP){
				fixedElapsed -= FIXED_TIMESTEP;
				update(FIXED_TIMESTEP);
			}
			update(fixedElapsed);

			render();
			break;
		case(STATE_LEVEL_THREE) :
			if (player.getSocialStatus() != false){
				state = STATE_LOSE;
			}
			else if (player.getWinStatus()){
				player.revertWin();
				state = STATE_WIN;
			}
			drawMap(sheet);
			if (fixedElapsed > FIXED_TIMESTEP * MAX_TIMESTEPS){
				fixedElapsed = FIXED_TIMESTEP * MAX_TIMESTEPS;
			}
			while (fixedElapsed >= FIXED_TIMESTEP){
				fixedElapsed -= FIXED_TIMESTEP;
				update(FIXED_TIMESTEP);
			}
			update(fixedElapsed);

			render();
			break;
		case(STATE_WIN) :

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
				readFile("FirstTiledMapOne.txt");
				makeMap();
				player.revertWin();
				enemies.clear();
				state = STATE_LEVEL_ONE;
			}
			else if (keys[SDL_SCANCODE_Q]){
				enemies.clear();
				SDL_Quit();
			}
			break;
		case(STATE_LOSE) :

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
				enemies.clear();
				player.revertWin();
				readFile("FirstTiledMapOne.txt");
				makeMap();
				state = STATE_LEVEL_ONE;
			}
			else if (keys[SDL_SCANCODE_Q]){
				enemies.clear();
				SDL_Quit();
			}
			break;
		}

		SDL_GL_SwapWindow(displayWindow);
	}
	Mix_FreeMusic(music);
	SDL_Quit();
	return 0;
}
