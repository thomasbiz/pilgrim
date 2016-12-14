#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include "SDL.h"
#include "SDL_image.h"
#include <vector>
#include <list>
#include <cstdio>
#include <algorithm>

struct Tile
{
	int oxygen = 0;
	int light = 0;
	bool floor = false;
	bool wall = false;
};

struct Point
{
	int x, y;
};

struct Actor
{
	Point pos;
	Point target;
	enum {
		IDLE,
		MOVING,
		DEAD
	} state;
	std::vector<Point> path;
};

Tile *GetTile(Tile *map, int x, int y)
{
	return &map[(x * 256) + y];
}

void DrawMap(Tile *map, SDL_Renderer *renderer)
{
	SDL_Rect rect;
	rect.w = 20;
	rect.h = 20;

	for (int y = 0; y < 256; y++) {
		for (int x = 0; x < 256; x++) {
			Tile *t = GetTile(map, x, y);
			rect.x = x * rect.w;
			rect.y = y * rect.h;
			if (t->floor) {
				SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
				SDL_RenderFillRect(renderer, &rect);
			} 
			if (t->wall) {
				SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
				SDL_RenderFillRect(renderer, &rect);
			}
			if (t->oxygen == 100) {
				SDL_SetRenderDrawColor(renderer, 50, 50, 250, 255);
				SDL_RenderFillRect(renderer, &rect);
			}
		}
	}
}

void DrawActors(std::vector<Actor*> &actors, SDL_Renderer *renderer)
{
	SDL_Rect rect;
	rect.w = 20;
	rect.h = 20;

	for (auto actor : actors) {
		rect.x = actor->pos.x * rect.w;
		rect.y = actor->pos.y * rect.h;
		SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
		SDL_RenderFillRect(renderer, &rect);
	}
}

SDL_Texture *LoadTexture(SDL_Renderer *renderer, const char * file)
{
	SDL_Surface *surface = IMG_Load(file);
	if (!surface) {
		printf("IMG_Load error: %s\n", IMG_GetError());
		return nullptr;
	}
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_FreeSurface(surface);
	if (!texture) {
		printf("SDL_CreateTextureFromSurface error: %s\n", SDL_GetError());
		return nullptr;
	}
	return texture;
}

void UpdateOxygen(Tile *map, int x, int y)
{
	return;
	Tile *t = GetTile(map, x, y);
	if (t->wall || t->oxygen == 100) return;
	t->oxygen = 100;
	UpdateOxygen(map, x + 1, y);
	UpdateOxygen(map, x - 1, y);
	UpdateOxygen(map, x, y + 1);
	UpdateOxygen(map, x, y - 1);
}

void CreateRoom(Tile *map, int x, int y, int width, int height) {
	for (int _y = y; _y <= y+height; ++_y) {
		for (int _x = x; _x <= x+width; ++_x) {
			Tile *t = GetTile(map, _x, _y);
			if (_y == y || _y == y + height || _x == x || _x == x + width || _x == x + height-4)
				t->wall = true;
			else
				t->floor = true;
		}
	}
}

void DrawString(SDL_Renderer *renderer, SDL_Texture *font, int x, int y, const char *string)
{
	int texWidth, texHeight;
	SDL_QueryTexture(font, 0, 0, &texWidth, &texHeight);
	int charsPerRow = 16, charsPerCol = 14;
	int charWidth = texWidth / charsPerRow;
	int charHeight = texHeight / charsPerCol;

	SDL_Rect srcRect = {
		0, 0,
		charWidth, charHeight
	};
	
	SDL_Rect dstRect = {
		x, y,
		charWidth, charHeight
	};

	for (unsigned int i = 0; i < strlen(string); ++i) {
		int ascii = string[i] - 32;
		int _x = ascii % charsPerRow * charWidth;
		int _y = ascii / charsPerRow * charHeight;
		srcRect.x = _x;
		srcRect.y = _y;
		dstRect.x += charWidth;
		SDL_RenderCopy(renderer, font, &srcRect, &dstRect);
	}
}

struct Node {
	Node *parent;
	int x, y;
	float f, g, h;
};

float ManhattanDistance(int x1, int y1, int x2, int y2)
{
	return (float)((x1 - x2) + (y1 - y2));
}

void GenerateNeighbor(Tile *map, Node *current, int x, int y, Point end, std::list<Node*> & open, std::list<Node*> & closed)
{
	Node *node = new Node;
	node->parent = current;
	node->x = x;
	node->y = y;
	node->g = current->g + 1;

	bool isWall = GetTile(map, node->x, node->y)->wall;
	if (!isWall) {
		/* if already on closed list, skip */
		bool isClosed = false;
		for (auto _node : closed) {
			if (_node->x == node->x && _node->y == node->y)
				isClosed = true;
		}
		if (!isClosed) {
			Node *_node = nullptr;
			for (auto __node : open) {
				if (__node->x == node->x && __node->y == node->y)
					_node = __node;
			}
			/* if not on closed and not on open, add to open */
			if (!_node) {
				open.push_back(node);
			}
			else {
				/* if already on open, check if this path is faster and change parent if so*/
				if (node->g < _node->g)
					_node->parent = node;
			}

		}
	}
}

void FindPath(Tile *map, const Point &start, const Point &end, Actor *actor)
{
	std::list<Node*> open;
	std::list<Node*> closed;

	Node *startNode = new Node;
	startNode->parent = nullptr;
	startNode->x = start.x;
	startNode->y = start.y;
	startNode->g = 0;
	startNode->h = ManhattanDistance(startNode->x, startNode->y, end.x, end.y);
	startNode->f = startNode->g + startNode->h;
	open.push_back(startNode);

	while (!open.empty()) {
		/* get node with lowest f on the open list */
		float lowestF = 999999.f;
		Node *current;
		for (auto node : open) {
			if (node->f < lowestF) {
				lowestF = node->f;
				current = node;
			}
		}
		/* remove from open list */
		open.remove(current);
		if (current->x == end.x && current->y == end.y) {
			printf("Found a path!\n");
			while (current->parent) {
				actor->path.push_back({ current->x, current->y });
				current = current->parent;
			}
			break;
		}

		/* generate surrounding 8 nodes */
		GenerateNeighbor(map, current, current->x - 1, current->y - 1, end, open, closed);			/* north-west */
		GenerateNeighbor(map, current, current->x, current->y - 1, end, open, closed);				/* north */
		GenerateNeighbor(map, current, current->x + 1, current->y - 1, end, open, closed);			/* north-east */
		GenerateNeighbor(map, current, current->x + 1, current->y, end, open, closed);				/* east */
		GenerateNeighbor(map, current, current->x + 1, current->y + 1, end, open, closed);			/* south-east */
		GenerateNeighbor(map, current, current->x, current->y + 1, end, open, closed);				/* south */
		GenerateNeighbor(map, current, current->x - 1, current->y + 1, end, open, closed);			/* south-west */
		GenerateNeighbor(map, current, current->x - 1, current->y, end, open, closed);				/* west */

		closed.push_back(current);
	}

	while (!open.empty()) {
		Node *node = open.back();
		delete node;
		open.pop_back();
	}
	
	while (!closed.empty()) {
		Node *node = closed.back();
		delete node;
		closed.pop_back();
	}
}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_CreateWindowAndRenderer(800, 600, 0, &window, &renderer);

	Tile map[256 * 256];
	CreateRoom(map, 8, 4, 20, 15);
	CreateRoom(map, 10, 8, 4, 4);
	CreateRoom(map, 22, 10, 4, 4);
	UpdateOxygen(map, 9, 5);

	std::vector<Actor*> actors;
	Actor *a = new Actor({
		{ 9, 5 },
		{ 9, 5 },
		Actor::IDLE
	});
	actors.push_back(a);
	
	Actor *b = new Actor({
		{ 15, 15 },
		{ 9, 5 },
		Actor::IDLE
	});
	actors.push_back(b);
	FindPath(map, a->pos, b->pos, a);
	for (auto point : a->path) {
		GetTile(map, point.x, point.y)->oxygen = 100;
	}
	SDL_Texture *font = LoadTexture(renderer, "data/font/font.gif");

	SDL_Event e;
	bool running = true;
	while (running) {
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				running = false;
			if (e.type == SDL_MOUSEBUTTONUP) {
				int _x = e.button.x / 20;
				int _y = e.button.y / 20;
				Tile *t = GetTile(map, _x, _y);
				if (e.button.button == SDL_BUTTON_LEFT) {
					Point p = { _x, _y };
					FindPath(map, a->pos, p, a);
					/*t->wall = true;
					t->floor = false;
					t->oxygen = 0;*/
				}
				else if (e.button.button == SDL_BUTTON_RIGHT) {
					/*t->wall = false;
					t->floor = true;
					t->oxygen = 0;*/
				}
				UpdateOxygen(map, _x, _y);
			}
		}

		if (!a->path.empty()) {
			Point *p = &a->path.back();
			a->pos.x = p->x;
			a->pos.y = p->y;
			a->path.pop_back();
		}

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		DrawMap(map, renderer);
		DrawActors(actors, renderer);
		SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
		DrawString(renderer, font, 300, 550, "Hello, World!");
		SDL_RenderPresent(renderer);
		SDL_Delay(200);
	}

	while (!actors.empty()) {
		Actor *a = actors.back();
		delete a;
		actors.pop_back();
	}

	SDL_DestroyTexture(font);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	_CrtDumpMemoryLeaks();

	return 0;
}