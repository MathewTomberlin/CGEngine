#include <pybind11/pybind11.h>
#include <SFML/System/Vector3.hpp>
#include "../Core/Voxel/VoxelWorld.h"

namespace py = pybind11;
using namespace py::literals;
using namespace CGEngine;

// VoxelWorld: blocks in chunks, generated and meshed on worker threads and uploaded by update() (docs/games/voxels.md).
void bindVoxel(py::module_& m) {
    py::class_<VoxelWorld>(m, "VoxelWorld")
        .def(py::init<int, int, const std::string&>(), py::arg("chunk_size") = 16, py::arg("chunk_height") = 64,
             py::arg("material"), "A block world in chunks of chunk_size by chunk_height by chunk_size blocks. The material is used for every chunk.")
        .def_property_readonly("chunk_size", &VoxelWorld::chunkSize)
        .def_property_readonly("chunk_height", &VoxelWorld::chunkHeight)
        .def("set_colour", [](VoxelWorld& world, int blockId, sf::Vector3f colour) {
            world.setColour(blockId, glm::vec3(colour.x, colour.y, colour.z));
        }, py::arg("block_id"), py::arg("colour"), "The tint of a block id, 0 to 1 per channel. Blocks without one are white.")
        .def("set_terrain", [](VoxelWorld& world, uint32_t seed, int sea_level, int base_height, int height_range, int snow_level) {
            VoxelTerrain terrain;
            terrain.seed = seed;
            terrain.seaLevel = sea_level;
            terrain.baseHeight = base_height;
            terrain.heightRange = height_range;
            terrain.snowLevel = snow_level;
            world.setTerrain(terrain);
        }, py::arg("seed"), py::arg("sea_level") = 14, py::arg("base_height") = 8, py::arg("height_range") = 44,
           py::arg("snow_level") = 40,
           "The generator's settings. Chunks requested after this use them.")
        .def("get_block", &VoxelWorld::getBlock, py::arg("x"), py::arg("y"), py::arg("z"),
             "The block id at world coordinates, 0 (air) outside the world height or in chunks that are not loaded.")
        .def("set_block", &VoxelWorld::setBlock, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("block_id"),
             "Set one block. Its chunk, and the neighbours it touches, are re-meshed by update(). Ignored if its chunk is not loaded.")
        .def("fill_column", &VoxelWorld::fillColumn, py::arg("x"), py::arg("z"), py::arg("y_from"), py::arg("y_to"), py::arg("block_id"),
             "Set the blocks of one column from y_from (inclusive) to y_to (exclusive).")
        .def("request_chunk", &VoxelWorld::requestChunk, py::arg("cx"), py::arg("cz"),
             "Start generating chunk (cx, cz) on a worker thread. Returns False if it is requested or loaded already.")
        .def("is_loaded", &VoxelWorld::isLoaded, py::arg("cx"), py::arg("cz"),
             "True once the chunk's blocks are in the world (update() installs them).")
        .def("unload_chunk", &VoxelWorld::unloadChunk, py::arg("cx"), py::arg("cz"), "Remove a chunk and its body.")
        .def("update", &VoxelWorld::update, py::arg("budget_ms") = 2.0, py::arg("focus_x") = 0, py::arg("focus_z") = 0,
             "Call once a frame. Installs generated chunks, uploads finished meshes within budget_ms (at least one), and starts "
             "meshing the dirty chunks nearest (focus_x, focus_z), a chunk coordinate. Returns the meshes uploaded.")
        .def("stats", [](const VoxelWorld& world) {
            return py::dict("chunks"_a = world.chunkCount(), "loaded"_a = world.loadedCount(), "pending"_a = world.pendingCount(),
                "dirty"_a = world.dirtyCount(), "meshing"_a = world.meshingCount(), "quads"_a = world.quadCount(),
                "upload_ms"_a = world.lastUploadMs());
        }, "Counts of chunks by state, quads in the meshes, and how long the last update's uploads took.")
        .def("clear", &VoxelWorld::clear, "Remove every chunk and its body. Call this before dropping the world.");
}
