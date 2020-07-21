# New SGM File Layout

The SGM file format (standing for Scene Graph Model) combines collision,
materials, meshes, a node hierarchy, and all of the data required to load a
full scenegraph model from a single file.

The SGM file format has historically been everything lumped together in a very
particular order, with little redundancy or support for random-access loading
of larger meshes.

This document attempts to change that, blending some semantics from glTF 2.0
with the existing needs and duties of the SGM file format.

### Document Layout

The document starts with a 4 byte magic number, equivalent to the characters
'SGM' followed by the byte value '\020'.

	0x0000: 'SGM' \020

This is followed by a four-byte length and a tightly-packed JSON object,
encoding the structure of the model. The end of the JSON object is zero-padded
to the closest 8-byte boundary; this padding is not included in the length of
the object.

	0x0004: 00000FF8
	0x0008: {

The top-level object contains several keys:

* `collision: {}` contains the definition for the top-level collision mesh.
* `materials: []` contains definitions for all the materials used by the model.
* `meshes: []` contains definitions for all primitive meshes used by the model.
* `name: string` the name of the model, usually the same as the last
  component of the file name less the extension.
* `nodes: []` contains the tree structure of the model.
* `lods: []` contains the LOD definitions for each LOD-capable mesh.
* `tags: []` contains an array of node tags.
* `animations: []` contains definitions for each animation in the model.

### Binary Data Buffers

The rest of the object is dedicated to a series of binary buffers, each
represented by a four-byte ID, a four-byte length, and a chunk of binary data.
The end of the data is zero-padded to the closest 8-byte boundary; this padding
is not included in the length of the buffer.

	0x1000 'ANIM'
	0x1004 000000F1
	0x1008 < data >

	0x10F9 00000
	0x10FF <next buffer...>

The data for meshes, animations, collision meshes, and material definitions are
all stored in binary buffers pointed to by the JSON structure defined at the
start of the model file.

The ID field of the buffer indicates which type it is:

* `ANIM`: animation keyframe data buffer
* `COLL`: collision mesh data (mesh vertex, acceleration structures, etc.)
* `MESH`: mesh data buffer (vertex positions, normals, etc.)
* `MTRL`: material data
* `TRNS`: packed matrix transform data in matrix4x4d format.

This is a non-exclusive list; the implementation *must* be able to safely
ignore buffers it does not recognize.

## JSON Scene Format

All information necessary to reconstruct the hierarchy of the scene is
contained in the JSON object located at the start of the file. 



### Node Hierarchy

Similar to GLTF, the model hierarchy is represented by nodes are stored in a
flat array. Each entry in the array is an object containing the minimum amount
of data to reconstruct the node.

If a node has children, they are represented by a `children` array containing
node indicies. The model root is node ID 0, all other nodes are loaded by
following the links encoded in the `children` array.

A node is allowed to be the child of more than one node, this achieves a
primitive form of instancing. While cyclical references *are* possible to
encode in the format, it is the responsibility of the loading code to break
these cycles and ensure that the hierarchy forms an acyclic graph.

All nodes contain a "name" field containing the string name of the node, and a
"type" field which is used to determine which loader to dispatch the node to.


```json
"nodes": [
	{
		"name": "root",
		"type": "Group",
		"children": [1,3]
	},
	{
		"name": "mesh_1",
		"type": "StaticGeometry",
		"meshes": [0,1]
	}
	{
		"name": "transform_node",
		"type": "MatrixTransform",
		"transform": {"buffer": 0, "offset": 0x280},
		"children": [2]
	},
	...
]
```

### Mesh Data Information

Each entry in the `meshes` array contains information about the mesh data for a
specific primitive. It includes information on which material to apply to the
mesh primitive and where the mesh is located.

The `buffer` index specifies which mesh buffer this data should be read from.
The `offset` determines where in the buffer the mesh data starts. `indexes`
specifies the number of elements in the mesh primitive, while `entries`
specifies the number of vertexes the mesh stores data for.

The `attributes` section contains the offsets of chunks of vertex attribute
data. If an attribute is not recognized by the implementation it should be
silently ignored. If a required attribute is not present, the implementation
should log an error and discard the mesh primitive.

Vertex attribute data must be stored in contiguous, non-interleaved chunks

```json
"meshes": [
	{
		"attributes": {
			"position": 0x400,
			"normal": 0x1000,
			"uv": 0x1C00,
			"tangent": 0x2400
		},
		"buffer": 0,
		"entries": 0x100,
		"indexes": 0x100,
		"offset": 0x280,
		"material": 0
	}
]
```
