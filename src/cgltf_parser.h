#ifndef CGLTF_PARSER_H
#define CGLTF_PARSER_H

#include "core/allocator.h"
#include "core/array.h"
#include "core/math.h"
#include "core/strings.h"
#include "core/types.h"

typedef struct cgltf_data cgltf_data;
typedef struct cgltf_material cgltf_material;

// NOTE(nico): Only supports interleaved format

typedef enum Gltf_Error {
  Gltf_Error_None,
  Gltf_Error_Failed_To_Open_File,
  Gltf_Error_Failed_To_Load_Data,
  Gltf_Error_Failed_To_Parse_Mesh,
  Gltf_Error_Failed_To_Parse_Material,
  Gltf_Error_Failed_To_Parse_Texture,
  Gltf_Error_Mesh_Not_Found,
  Gltf_Error_Material_Not_Found,
  Gltf_Error_Invalid_Texture_Format,
} Gltf_Error;

typedef enum Gltf_Supported_Attribute {
  Gltf_Supported_Attribute_Position,
  Gltf_Supported_Attribute_Normal,
  Gltf_Supported_Attribute_Texcoord,
  Gltf_Supported_Attribute_MAX,
} Gltf_Supported_Attribute;

typedef struct Gltf_Document {
  Allocator allocator;
  cgltf_data *data;
  String filepath;

  Array(char) hash_buf;
} Gltf_Document;

typedef struct Gltf_Mesh {
  u64 hash;
  Vec3 translation;
  Quat rotation;
  Vec3 scale;

  Array(struct {
    Array(f32) vertex_data;
    Array(u32) index_data;
    Option(usize) material;
  }) primitives;
} Gltf_Mesh;

typedef struct Gltf_Texture {
  u64 hash;
  u32 width;
  u32 height;
  u32 channels;
  Array(u8) data;
} Gltf_Texture;

typedef struct Gltf_Material {
  u64 hash;
  Option(usize) albedo_index;
} Gltf_Material;

typedef struct Gltf_Mesh_Parse_Info {
  String node_name;
  usize vertex_stride;
  struct {
    usize count;
    usize offset;
    f32 splat;
  } attribute_formats[Gltf_Supported_Attribute_MAX];
} Gltf_Mesh_Parse_Info;

typedef Result(Gltf_Document, Gltf_Error) Gltf_Document_Open_Result;
typedef Result(Gltf_Mesh, Gltf_Error) Gltf_Mesh_Parse_Result;
typedef Result(Gltf_Material, Gltf_Error) Gltf_Material_Parse_Result;
typedef Result(Gltf_Texture, Gltf_Error) Gltf_Texture_Parse_Result;

typedef Option(usize) Gltf_Material_Index_Option;

Gltf_Document_Open_Result
open_gltf_document(String filepath, Allocator allocator);
Gltf_Error close_gltf_document(Gltf_Document document);

Gltf_Material_Index_Option
gltf_document_query_material_index(Gltf_Document *document, String name);

Gltf_Mesh_Parse_Result gltf_document_parse_mesh(
    Gltf_Document *document, Gltf_Mesh_Parse_Info *info, Allocator allocator
);
Gltf_Material_Parse_Result
gltf_document_parse_material(Gltf_Document *document, usize index);
Gltf_Texture_Parse_Result gltf_document_parse_texture(
    Gltf_Document *document, usize index, Allocator allocator
);

#endif