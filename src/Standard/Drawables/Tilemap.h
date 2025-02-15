#pragma once

#include "SFML/Graphics.hpp"
#include "SFML/System.hpp"
#include "../../Core/Types/V2.h"
#include <iostream>
#include <fstream>
using namespace sf;
using namespace std;

namespace CGEngine {
	class Tilemap : public Drawable, public Transformable {
	public:
		Tilemap(const filesystem::path& tilesetPath, Vector2u tileDimensions, Vector2u mapSizeByTiles, vector<int> data, string dataPath = "");
		void update();
		void setMapData(vector<int> data, bool shouldUpdate = true);
		void saveMapData();
		vector<int> getMapData();
		Vector2u getTile(V2f localPos);
		Vector2i getTileByLocalPos(V2f localPos);
		int localPosToTileID(V2f localPos);
		FloatRect getGlobalBounds();
		FloatRect getLocalBounds();
		vector<int> loadMapData(string path);
		Vector2u getDimensions();
		Vector2u getTilesize();
		void assign(int tileId, int value, bool shouldUpdate = true);
		int query(int tileId);
	private:
		void draw(RenderTarget& target, RenderStates states) const override;

		VertexArray vertices;
		Texture* tileset;
		Vector2u tileSize;
		Vector2u dimensions;
		vector<int> mapData;
		string mapDataPath = "";
	};
}