#include <algorithm>
#include <optional>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <SFML/System/Vector3.hpp>
#include "../Core/Voxel/VoxelWorld.h"
#include "../Core/Voxel/VoxelBody.h"

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
        .def("set_terrain", [](VoxelWorld& world, uint32_t seed, int sea_level, int base_height, int height_range, int snow_level, float tree_chance, float cave_width) {
            VoxelTerrain terrain;
            terrain.seed = seed;
            terrain.seaLevel = sea_level;
            terrain.baseHeight = base_height;
            terrain.heightRange = height_range;
            terrain.snowLevel = snow_level;
            terrain.treeChance = tree_chance;
            terrain.caveWidth = cave_width;
            world.setTerrain(terrain);
        }, py::arg("seed"), py::arg("sea_level") = 14, py::arg("base_height") = 8, py::arg("height_range") = 44,
           py::arg("snow_level") = 40, py::arg("tree_chance") = 0.01f, py::arg("cave_width") = 0.07f,
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
                "upload_ms"_a = world.lastUploadMs(), "edits"_a = world.editCount());
        }, "Counts of chunks by state, quads in the meshes, edited blocks, and how long the last update's uploads took.")
        .def("set_smoothing", &VoxelWorld::setSmoothing, py::arg("resolution"),
             "The surface style: 0 for cubes, or the number of samples per block for a smooth skin (1 for the block scale, 2 for half blocks). Loaded chunks are re-meshed.")
        .def("smoothing", &VoxelWorld::smoothing, "The surface style set by set_smoothing: 0 for cubes, else the samples per block.")
        .def("define_block", [](VoxelWorld& world, int block_id, sf::Vector3f colour, bool solid, bool falls, bool liquid, int emits,
                const std::string& shape, const std::string& group, const std::optional<std::string>& material, int mesh,
                int join_above, const std::optional<std::vector<int>>& join_below, bool transparent) {
            if (shape != "smooth" && shape != "cube") throw py::value_error("shape must be \"smooth\" or \"cube\"");
            if (group != "terrain" && group != "own") throw py::value_error("group must be \"terrain\" or \"own\"");
            if (shape == "cube" && group == "terrain") throw py::value_error("a cube block is drawn in its own mesh: group must be \"own\"");
            if (mesh < 0 || mesh > 255) throw py::value_error("mesh must be a block id, 0 to 255");
            if (mesh != 0 && group != "own") throw py::value_error("mesh is for a block in its own mesh: group must be \"own\"");
            if (join_above < 0 || join_above > 255) throw py::value_error("join_above must be a block id, 0 to 255");
            if (join_above != 0 && (group != "own" || shape != "smooth")) {
                throw py::value_error("join_above is for a smooth block in its own mesh: shape \"smooth\", group \"own\"");
            }
            world.setColour(block_id, glm::vec3(colour.x, colour.y, colour.z));
            BlockType type{ solid, falls, liquid, static_cast<uint8_t>(std::clamp(emits, 0, 255)) };
            type.own = group == "own";
            type.cube = shape == "cube";
            type.mesh = static_cast<uint8_t>(mesh);
            type.joinAbove = static_cast<uint8_t>(join_above);
            type.transparent = transparent;
            if (join_below) {
                for (int below : *join_below) {
                    if (below < 0 || below > 255) throw py::value_error("join_below must be block ids, 0 to 255");
                    type.joinBelow.set(static_cast<size_t>(below));
                }
                if (!type.joinBelow.any()) throw py::value_error("join_below is empty");
                if (group != "own" || shape != "smooth") throw py::value_error("join_below is for a smooth block in its own mesh");
            }
            world.defineBlock(block_id, type);
            world.setMaterial(block_id, material.value_or(std::string()));
        }, py::arg("block_id"), py::arg("colour"), py::arg("solid") = true, py::arg("falls") = false, py::arg("liquid") = false,
           py::arg("emits") = 0, py::arg("shape") = "smooth", py::arg("group") = "terrain", py::arg("material") = py::none(),
           py::arg("mesh") = 0, py::arg("join_above") = 0, py::arg("join_below") = py::none(), py::arg("transparent") = false,
           "Define a block id: its colour, whether it is solid (blocks movement), falls (sand-like, moves down into air or liquid), "
           "liquid (flows, as water does) and emits (the light it gives off at its own cell, 0 to 255; a lamp is 255). How it is "
           "drawn: shape is \"smooth\" (the skin) or \"cube\" (a cube that never merges with its neighbours); group is \"terrain\" "
           "(the chunk's terrain mesh) or \"own\" (a mesh of its own per chunk, drawn with material, a material name, or the "
           "world's material if None). A cube block must be in its own group.")
        .def("skin_at", &VoxelWorld::skinAt, py::arg("x"), py::arg("y"), py::arg("z"),
             "The collision skin of the block at these coordinates: 0 for air and liquids, 1 for the terrain, 256 + id for a smooth "
             "block drawn in its own mesh, 512 + id for a cube. Solid blocks are the non-zero ones; VoxelPhysics uses it as its solid().")
        .def("chunk_group_quads", &VoxelWorld::chunkGroupQuads, py::arg("cx"), py::arg("cz"),
             "The quads of each mesh a loaded chunk has, as a dict: 0 for the terrain, else the block id with its own mesh. Missing "
             "groups have none.")
        .def("is_solid", &VoxelWorld::isSolid, py::arg("x"), py::arg("y"), py::arg("z"),
             "True if the block at these coordinates is solid. Air and liquids are not.")
        .def("biome_at", &VoxelWorld::biomeName, py::arg("x"), py::arg("z"),
             "The biome of the column at x, z: \"plains\", \"forest\" or \"desert\". Set by the terrain settings and the seed.")
        .def("skin_surface", [](const VoxelWorld& world, std::array<double, 3> origin, std::array<double, 3> direction,
                double maxDistance) -> py::object {
            std::array<double, 3> surface{};
            if (!world.skinSurface(origin, direction, maxDistance, surface)) return py::none();
            return py::make_tuple(surface[0], surface[1], surface[2]);
        }, py::arg("origin"), py::arg("direction"), py::arg("max_distance"),
           "The first point where the drawn terrain surface is along the ray (a unit direction), within max_distance, or None.")
        .def("light_at", &VoxelWorld::lightAt, py::arg("x"), py::arg("y"), py::arg("z"),
             "The light at a block, 0 to 255, as the mesh shades it: the sky's (255 is open to the sky, dark caves are low), or a light block's, whichever is brighter.")
        .def("step_body", [](const VoxelWorld& world, std::array<double, 3> pos, std::array<double, 3> vel, double dt,
                bool flying, double halfWidth, double height, bool onGround) {
            VoxelBodyShape shape;
            shape.halfWidth = halfWidth;
            shape.height = height;
            VoxelBodyState body;
            body.pos = pos;
            body.vel = vel;
            body.onGround = onGround;
            const VoxelBodyState next = world.stepBody(body, shape, dt, flying);
            return py::make_tuple(py::make_tuple(next.pos[0], next.pos[1], next.pos[2]),
                py::make_tuple(next.vel[0], next.vel[1], next.vel[2]), next.onGround);
        }, py::arg("pos"), py::arg("vel"), py::arg("dt"), py::arg("flying") = false, py::arg("half_width") = 0.3,
           py::arg("height") = 1.8, py::arg("on_ground") = false,
           "Move a box with its feet at pos (x, y, z) and speed vel, for dt seconds, over the terrain's skin, as VoxelPhysics.step "
           "does. on_ground is whether it stood on the skin at the last step (it lets the box climb a block). Returns (pos, vel, on_ground).")
        .def("simulate", &VoxelWorld::simulate, py::arg("dt"), py::arg("budget_ms") = 1.0, py::arg("tick_seconds") = 0.1,
             "Advance the block physics by dt seconds, within budget_ms of work. Ticks run every tick_seconds. Returns the cells looked at.")
        .def("set_detail", &VoxelWorld::setDetail, py::arg("near_chunks"), py::arg("near_samples") = 2,
             "Finer smooth skin near the camera: the chunks within near_chunks of the focus use near_samples per block. 0 turns it off.")
        .def("chunk_quads", &VoxelWorld::chunkQuads, py::arg("cx"), py::arg("cz"),
             "The quads in a loaded chunk's mesh (0 if it is not loaded or has none).")
        .def("set_move_seconds", &VoxelWorld::setMoveSeconds, py::arg("seconds"),
             "How long a falling block takes to move one cell (0 for instant moves). A block in flight is not in the world until it lands.")
        .def("moving_blocks", [](const VoxelWorld& world) {
            py::list result;
            for (const VoxelSimulation::Move& move : world.movingBlocks()) {
                const double progress = move.duration > 0.0 ? std::min(1.0, move.elapsed / move.duration) : 1.0;
                result.append(py::make_tuple(static_cast<int>(move.block),
                    py::make_tuple(move.from[0], move.from[1], move.from[2]),
                    py::make_tuple(move.to[0], move.to[1], move.to[2]), progress));
            }
            return result;
        }, "The blocks in flight: a list of (block, from, to, progress) with progress from 0 to 1.")
        .def("settle_moves", &VoxelWorld::settleMoves, "Land every block in flight now.")
        .def("activate", [](VoxelWorld& world, int x, int y, int z) { world.physics().activate(x, y, z); },
             py::arg("x"), py::arg("y"), py::arg("z"), "Queue a cell for the simulation, so it is looked at on the next tick.")
        .def("active_count", [](VoxelWorld& world) { return world.physics().activeCount(); },
             "Cells queued for the simulation. A world with nothing moving has none.")
        .def("add_rule", [](VoxelWorld& world, int block, int neighbour, int becomes, float chance) {
            world.physics().addRule(VoxelSimulation::Rule{ static_cast<uint8_t>(block), static_cast<uint8_t>(neighbour),
                static_cast<uint8_t>(becomes), chance });
        }, py::arg("block"), py::arg("neighbour"), py::arg("becomes"), py::arg("chance") = 1.0f,
           "A reaction: a block that touches the neighbour kind becomes the other block, with this chance per tick.")
        .def("clear_rules", [](VoxelWorld& world) { world.physics().clearRules(); }, "Remove every reaction rule.")
        .def("set_tick_hook", [](VoxelWorld& world, py::object fn) {
            if (fn.is_none()) {
                world.physics().setHook(nullptr);
                return;
            }
            // Called once per queued block per tick, from the main thread (the GIL is held). Return a block id to replace
            // the block, or None to leave it.
            world.physics().setHook([fn](int x, int y, int z, int block) {
                py::object result = fn(x, y, z, block);
                return result.is_none() ? -1 : result.cast<int>();
            });
        }, py::arg("fn"), "A Python function (x, y, z, block) -> block id or None, run on every queued block each tick. None removes it.")
        .def("save_edits", &VoxelWorld::saveEdits, py::arg("path"),
             "Write the edited blocks to a binary file. False if it cannot be written.")
        .def("load_edits", &VoxelWorld::loadEdits, py::arg("path"),
             "Add the edits from a file written by save_edits. Loaded chunks change at once. False if the file is unreadable.")
        .def("edit_count", &VoxelWorld::editCount, "How many blocks differ from what the player has edited (all edits, loaded or not).")
        .def("clear_edits", &VoxelWorld::clearEdits, "Forget every edit. Loaded chunks keep their current blocks until they are unloaded or changed.")
        .def("clear", &VoxelWorld::clear, "Remove every chunk and its body. Call this before dropping the world.");
}
