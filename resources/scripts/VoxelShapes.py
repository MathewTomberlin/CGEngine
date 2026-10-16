"""Shapes made from boxes, for the voxel sample's runtime meshes: the outlines of the block in reach, and the creatures that
walk on the terrain. Each returns the arrays that cge.create_mesh_body and cge.update_mesh_body take."""
from array import array

WIRE_THICKNESS = 0.02       # how far an outline's bars stick out from the block, in blocks


def box_faces(lo, hi):
    """The six faces of the box lo..hi, each as (corners, outward normal)."""
    x0, y0, z0 = lo
    x1, y1, z1 = hi
    return [
        ([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], (1, 0, 0)),
        ([(x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x0, y0, z0)], (-1, 0, 0)),
        ([(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)], (0, 1, 0)),
        ([(x0, y0, z1), (x0, y0, z0), (x1, y0, z0), (x1, y0, z1)], (0, -1, 0)),
        ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1)),
        ([(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)], (0, 0, -1)),
    ]


def boxes_arrays(boxes):
    """Arrays for solid boxes: each (lo, hi, colour) is a closed box whose faces are wound to face outward."""
    positions, normals, colours, uvs, material_ids, indices = [], [], [], [], [], []
    face_uvs = ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))
    for lo, hi, colour in boxes:
        for corners, normal in box_faces(lo, hi):
            first = len(positions) // 3
            for corner, uv in zip(corners, face_uvs):
                positions.extend(corner)
                normals.extend(normal)
                colours.extend(colour)
                uvs.extend(uv)
                material_ids.append(0.0)
            # Two triangles, wound so their normal faces outward.
            a, b, c = corners[0], corners[1], corners[2]
            u = [b[i] - a[i] for i in range(3)]
            w = [c[i] - a[i] for i in range(3)]
            cross = [u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]]
            if sum(cross[i] * normal[i] for i in range(3)) >= 0:
                indices.extend((first, first + 1, first + 2, first, first + 2, first + 3))
            else:
                indices.extend((first, first + 2, first + 1, first, first + 3, first + 2))
    return (array("f", positions), array("f", normals), array("f", colours), array("f", uvs),
            array("f", material_ids), array("I", indices))


def outline_arrays(lo, hi, colour):
    """Arrays for the twelve edges of the box lo..hi, each edge a thin bar that sticks out a little from the box."""
    t = WIRE_THICKNESS
    boxes = []
    for axis in range(3):
        o1, o2 = [i for i in range(3) if i != axis]
        for v1 in (lo[o1], hi[o1]):
            for v2 in (lo[o2], hi[o2]):
                bmin, bmax = [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]
                bmin[axis], bmax[axis] = lo[axis] - t, hi[axis] + t
                bmin[o1], bmax[o1] = v1 - t, v1 + t
                bmin[o2], bmax[o2] = v2 - t, v2 + t
                boxes.append((bmin, bmax, colour))
    return boxes_arrays(boxes)


def mix(a, b, t):
    """Colour a moved t of the way from a to b (t in 0 to 1)."""
    return tuple(x + (y - x) * t for x, y in zip(a, b))
