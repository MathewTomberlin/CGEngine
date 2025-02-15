#include "Tilemap.h"
#include "../../Core/Engine/Engine.h"

namespace CGEngine {
	Tilemap::Tilemap(const filesystem::path& tilesetPath, Vector2u tileDimensions, Vector2u mapSizeByTiles, vector<int> data, string dataPath): tileSize(tileDimensions), dimensions(mapSizeByTiles), tileset(textures->get(tilesetPath)) {
		//Set the tilemap data path and try to load the map data
		mapDataPath = dataPath;
		if (dataPath != "") {
			mapData = loadMapData(dataPath);
		} else {
			//If there is supplied data, use that
			if (data.size() > 0) {
				mapData = data;
			//Otherwise initialize the grid to 0
			} else {
				mapData = vector<int>(dimensions.x * dimensions.y, 0);
			}
		}

		vertices.setPrimitiveType(PrimitiveType::Triangles);
		vertices.resize(mapSizeByTiles.x * mapSizeByTiles.y * 6);
		update();
	}

	void Tilemap::update() {
		const int* tiles = mapData.data();
		for (unsigned int i = 0; i < dimensions.x; ++i) {
			for (unsigned int j = 0; j < dimensions.y; ++j) {
				const int tileNumber = tiles[i + j * dimensions.x];
				const int tu = tileNumber % (tileset->getSize().x / tileSize.x);
				const int tv = tileNumber / (tileset->getSize().x / tileSize.x);

				Vertex* triangles = &vertices[(i + j * dimensions.x) * 6];

				triangles[0].position = Vector2f(i * tileSize.x, j * tileSize.y);
				triangles[1].position = Vector2f((i + 1) * tileSize.x, j * tileSize.y);
				triangles[2].position = Vector2f(i * tileSize.x, (j + 1) * tileSize.y);
				triangles[3].position = Vector2f(i * tileSize.x, (j + 1) * tileSize.y);
				triangles[4].position = Vector2f((i + 1) * tileSize.x, j * tileSize.y);
				triangles[5].position = Vector2f((i + 1) * tileSize.x, (j + 1) * tileSize.y);

				triangles[0].texCoords = Vector2f(tu * tileSize.x, tv * tileSize.y);
				triangles[1].texCoords = Vector2f((tu + 1) * tileSize.x, tv * tileSize.y);
				triangles[2].texCoords = Vector2f(tu * tileSize.x, (tv + 1) * tileSize.y);
				triangles[3].texCoords = Vector2f(tu * tileSize.x, (tv + 1) * tileSize.y);
				triangles[4].texCoords = Vector2f((tu + 1) * tileSize.x, tv * tileSize.y);
				triangles[5].texCoords = Vector2f((tu + 1) * tileSize.x, (tv + 1) * tileSize.y);
			}
		}
	}

	void Tilemap::assign(int tileId, int value, bool shouldUpdate) {
		mapData.at(tileId) = value;
		if (shouldUpdate) {
			update();
		}
	}

	int Tilemap::query(int tileId) {
		return mapData.at(tileId);
	}

	void Tilemap::setMapData(vector<int> data, bool shouldUpdate) {
		mapData = data;
		if (shouldUpdate) {
			update();
		}
	}

	vector<int> Tilemap::getMapData() {
		return mapData;
	}

	void Tilemap::saveMapData() {
		if (mapDataPath == "") return;
		
		cout << "Saving " << mapDataPath << "\n";
		ofstream out(mapDataPath);
		for (int i = 0; i < mapData.size(); i++) {
			out << setprecision(3) << to_string(mapData[i]) << "\n";
		}
		out.close();
	}

	Vector2u Tilemap::getDimensions() {
		return dimensions;
	}
	Vector2u Tilemap::getTilesize() {
		return tileSize;
	}

	Vector2u Tilemap::getTile(V2f localPos) {
		return (localPos / tileSize);
	}

	int Tilemap::localPosToTileID(V2f localPos) {
		Vector2i tile = getTileByLocalPos(localPos);
		return tile.y * dimensions.x + tile.x;
	}

	Vector2i Tilemap::getTileByLocalPos(V2f localPos) {
		return localPos / tileSize;
	}

	FloatRect Tilemap::getGlobalBounds() {
		return getTransform().transformRect(FloatRect(getPosition(), { (float)tileSize.x * dimensions.x, (float)tileSize.y * dimensions.y }));
	}

	FloatRect Tilemap::getLocalBounds() {
		return FloatRect({0,0}, {(float)tileSize.x * dimensions.x * getScale().x, (float)tileSize.y * dimensions.y * getScale().y});
	}

	void Tilemap::draw(RenderTarget& target, RenderStates states) const {
		states.transform *= getTransform();
		states.texture = tileset;
		target.draw(vertices, states);
	}

	vector<int> Tilemap::loadMapData(string path) {
		cout << "Loading " << path << "\n";
		ifstream in(path.c_str());
		string value;
		int tileId = 0;
		vector<int> tiles(dimensions.x*dimensions.y);
		while (getline(in, value)) {
			if (tileId < dimensions.x * dimensions.y) {
				//Assume value is 3 digits or less
				if (value.size() == 1) {
					value = "00" + value;
				} else if (value.size() == 2) {
					value = "0" + value;
				}
				int number = ((value[0] - '0') * 100) + ((value[1] - '0') * 10) + (value[2] - '0');
				tiles.at(tileId) = number;
			}
			tileId++;
		}
		return tiles;
	}
}