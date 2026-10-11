#include <pybind11/pybind11.h>
#include <SFML/System/Vector3.hpp>
#include "../Core/Voxel/VoxelWorld.h"

namespace py = pybind11;
using namespace CGEngine;

// VoxelWorld: blocks in chunks, meshed in C++ (docs/games/voxels.md). Python decides what the blocks are.
void bindVoxel(py::module_& m) {
    py::class_<VoxelWorld>(m, "VoxelWorld")
        .def(py::init<int, int, const std::string&>(), py::arg("chunk_size") = 16, py::arg("chunk_height") = 64,
             py::arg("material"), "A block world in chunks of chunk_size by chunk_height by chunk_size blocks. The material is used for every chunk.")
        .def_property_readonly("chunk_size", &VoxelWorld::chunkSize)
        .def_property_readonly("chunk_height", &VoxelWorld::chunkHeight)
        .def("set_colour", [](VoxelWorld& world, int blockId, sf::Vector3f colour) {
            world.setColour(blockId, glm::vec3(colour.x, colour.y, colour.z));
        }, py::arg("block_id"), py::arg("colour"), "The tint of a block id, 0 to 1 per channel. Blocks without one are white.")
        .def("get_block", &VoxelWorld::getBlock, py::arg("x"), py::arg("y"), py::arg("z"),
             "The block id at world coordinates, 0 (air) outside the world height or in chunks that are not loaded.")
        .def("set_block", &VoxelWorld::setBlock, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("block_id"),
             "Set one block. Its chunk, and the neighbours it touches, are re-meshed by rebuild_dirty. Ignored if its chunk is not loaded.")
        .def("fill_column", &VoxelWorld::fillColumn, py::arg("x"), py::arg("z"), py::arg("y_from"), py::arg("y_to"), py::arg("block_id"),
             "Set the blocks of one column from y_from (inclusive) to y_to (exclusive).")
        .def("load_chunk", &VoxelWorld::loadChunk, py::arg("cx"), py::arg("cz"),
             "Create an empty chunk at chunk coordinates (cx, cz). Returns False if it is loaded already.")
        .def("unload_chunk", &VoxelWorld::unloadChunk, py::arg("cx"), py::arg("cz"),
             "Remove a chunk and its body.")
        .def("is_loaded", &VoxelWorld::isLoaded, py::arg("cx"), py::arg("cz"))
        .def("rebuild_dirty", &VoxelWorld::rebuildDirty, py::arg("max_chunks") = 4,
             "Re-mesh up to max_chunks chunks that changed. Returns how many were meshed.")
        .def("dirty_count", &VoxelWorld::dirtyCount)
        .def("chunk_count", &VoxelWorld::chunkCount)
        .def("quad_count", &VoxelWorld::quadCount, "Quads in the meshes of the loaded chunks.")
        .def("clear", &VoxelWorld::clear, "Remove every chunk and its body. Call this before dropping the world.");
}
