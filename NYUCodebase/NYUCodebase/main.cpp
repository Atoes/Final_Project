#ifdef _WINDOWS
	#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>

#include "ShaderProgram.h"
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
#define LEVEL_HEIGHT_1 60
#define LEVEL_WIDTH_1 80
#define LEVEL_HEIGHT_2
#define LEVEL_WIDTH_2
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
			float placeY = (float)atoi(yPosition.c_str()) * -TILE_SIZE + 0.5;

			//placeEntity(type, placeX, placeY);
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

std::vector<float> vertexData;
std::vector<float> texCoordData;
void makeMap(){
	for (int y = 0; y < LEVEL_HEIGHT_1; y++){
		for (int x = 0; x < LEVEL_WIDTH_1; x++){
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
	//GLuint sheet = LoadTexture("spritesheet.png");

	program = new ShaderProgram(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");

	/*ifstream infile("FirstTiledMapOne.txt");
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
	}*/

	//makeMap();

	float lastFrameTicks = 0.0f;
	float ticks = (float)SDL_GetTicks() / 1000.0f;
	lastFrameTicks = ticks;

	projectionMatrix.setOrthoProjection(-3.55f, 3.55f, -4.0f, 4.0f, -1.0f, 1.0f);
	enum GameState { STATE_MENU, STATE_LEVEL_ONE, STATE_LEVEL_TWO, STATE_LEVEL_THREE, STATE_WIN, STATE_LOSE, STATE_QUIT };
	int state = STATE_MENU;

	glUseProgram(program->programID);

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

		switch (state){
		case STATE_MENU :
			modelMatrix.identity();
			modelMatrix.Translate(-3.0f, 2.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "NAME", 0.2f, 0.001f);

			modelMatrix.identity();
			modelMatrix.Translate(-3.0f, 1.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "PRESS E TO BEGIN", 0.2f, 0.00001f);

			modelMatrix.identity();
			modelMatrix.Translate(-3.0f, 0.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "ARROW KEYS TO MOVE", 0.2f, 0.00001f);

			modelMatrix.identity();
			modelMatrix.Translate(-3.0f, -1.0f, 0.0f);
			program->setModelMatrix(modelMatrix);
			DrawText(program, font, "SPACE TO JUMP", 0.2f, 0.00001f);

			if (keys[SDL_SCANCODE_E]){
				state = STATE_LEVEL_ONE;
			}
			break;

		case(STATE_LEVEL_ONE) :
			tileToWorldCoordinates(&x, &y, 10, 10);
			viewMatrix.Translate(-x, -y, 0.0f);
			break;
		case(STATE_LEVEL_TWO) :
			break;
		case(STATE_LEVEL_THREE) :
			break;
		case(STATE_WIN) :
			break;
		case(STATE_LOSE) :
			break;
		case(STATE_QUIT) :
			break;
		}

		SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();
	return 0;
}
