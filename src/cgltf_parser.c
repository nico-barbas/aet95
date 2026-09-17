#include "cgltf_parser.h"

#include "cgltf.h"
#include "core/array.h"
#include "core/fmt.h"
#include "core/math.h"
#include "core/runtime.h"
#include "core/strings.h"
#include "core/types.h"
#include "stb_image.h"

#include <string.h>

static u64 gltf_document_hash_index(Gltf_Document *document, usize index) {
  String_Builder builder =
      make_builder_from_buf(document->hash_buf.items, document->hash_buf.len);

  fmt_printb(&builder, "{}#{}", document->filepath, index);

  String handle = builder_get_string(&builder);
  u64 hash = hash_fnv1a(handle.data, handle.len);

  return hash;
}

Gltf_Document_Open_Result
open_gltf_document(String filepath, Allocator allocator) {
  errdefer_scope;

  Gltf_Document document = {
    .allocator = allocator,
  };

  cgltf_data *data = {0};
  cgltf_options options = {0};

  cgltf_result parse_result = cgltf_parse_file(&options, filepath.data, &data);
  if (parse_result != cgltf_result_success) {
    return err(Gltf_Document_Open_Result, Gltf_Error_Failed_To_Open_File);
  }
  errdefer {
    cgltf_free(data);
  };

  cgltf_result load_result = cgltf_load_buffers(&options, data, filepath.data);
  if (load_result != cgltf_result_success) {
    return err(Gltf_Document_Open_Result, Gltf_Error_Failed_To_Load_Data);
  }

  document.data = data;
  document.filepath = or_return(
      string_clone(filepath, allocator),
      err(Gltf_Document_Open_Result, Gltf_Error_Failed_To_Load_Data)
  );
  errdefer {
    delete_string(document.filepath, allocator);
  };

  document.hash_buf =
      make_array(document.hash_buf, document.filepath.len + 16, allocator);
  if (document.hash_buf.items == nullptr) {
    return err(Gltf_Document_Open_Result, Gltf_Error_Failed_To_Load_Data);
  }

  return_ok(Gltf_Document_Open_Result, document);
}

Gltf_Error close_gltf_document(Gltf_Document document) {
  cgltf_free(document.data);
  delete_string(document.filepath, document.allocator);
  delete_array(document.hash_buf);
  return Gltf_Error_None;
}

Gltf_Material_Index_Option
gltf_document_query_material_index(Gltf_Document *document, String name) {
  cgltf_data *data = document->data;

  for (usize i = 0; i < data->materials_count; i += 1) {
    String material_name = from_cstring(data->materials[i].name);
    if (string_equal(name, material_name)) {
      return some(Gltf_Material_Index_Option, i);
    }
  }

  return none(Gltf_Material_Index_Option);
}

// NOTE(nico): fuck you if your vertex type isn't using floats
Gltf_Mesh_Parse_Result gltf_document_parse_mesh(
    Gltf_Document *document, Gltf_Mesh_Parse_Info *info, Allocator allocator
) {
  errdefer_scope;

  Option(usize) node_index_opt = {0};
  for (usize i = 0; i < document->data->nodes_count; i += 1) {
    cgltf_node *node = &document->data->nodes[i];
    String name = from_cstring(node->name);

    if (string_equal(info->node_name, name)) {
      node_index_opt.some = true;
      node_index_opt.value = i;
      break;
    }
  }

  if (!node_index_opt.some) {
    return err(Gltf_Mesh_Parse_Result, Gltf_Error_Mesh_Not_Found);
  }

  usize node_index = node_index_opt.value;
  cgltf_mesh *cmesh = document->data->nodes[node_index].mesh;

  // NOTE(nico): mesh not present on node with given name
  if (cmesh == nullptr) {
    return err(Gltf_Mesh_Parse_Result, Gltf_Error_Mesh_Not_Found);
  }

  Gltf_Mesh mesh = {
    .hash = gltf_document_hash_index(
        document, cgltf_mesh_index(document->data, cmesh)
    ),
    .primitives =
        make_array(mesh.primitives, cmesh->primitives_count, allocator),
  };
  if (mesh.primitives.items == nullptr) {
    return err(Gltf_Mesh_Parse_Result, Gltf_Error_Failed_To_Parse_Mesh);
  }
  errdefer {
    for (usize i = 0; i < mesh.primitives.len; i += 1) {
      delete_array(mesh.primitives.items[i].vertex_data);
      delete_array(mesh.primitives.items[i].index_data);
    }
    delete_array(mesh.primitives);
  };

  bool32 primitives_failed = false;
  for (usize i = 0; i < cmesh->primitives_count; i += 1) {
    cgltf_primitive *cprim = &cmesh->primitives[i];

    cgltf_accessor *accessors[Gltf_Supported_Attribute_MAX] = {0};
    usize accessor_len = 0;

    for (usize j = 0; j < cprim->attributes_count; j += 1) {
      cgltf_attribute *attribute = &cprim->attributes[j];

      if (attribute->type == cgltf_attribute_type_position) {
        accessors[Gltf_Supported_Attribute_Position] = attribute->data;
        accessor_len = attribute->data->count;
      } else if (attribute->type == cgltf_attribute_type_normal) {
        accessors[Gltf_Supported_Attribute_Normal] = attribute->data;
      } else if (
          attribute->type == cgltf_attribute_type_texcoord &&
          attribute->index == 0
      ) {
        // FIXME(nico): add support for multiple uv channels when needed
        accessors[Gltf_Supported_Attribute_Texcoord] = attribute->data;
      }
    }

    for (usize j = 0; j < Gltf_Supported_Attribute_MAX; j += 1) {
      if (info->attribute_formats[j].count == 0) {
        continue;
      }

      if (accessors[j] == nullptr || accessors[j]->count != accessor_len) {
        return err(Gltf_Mesh_Parse_Result, Gltf_Error_Failed_To_Parse_Mesh);
      }
    }

    cgltf_accessor *index_accessor = cprim->indices;
    if (index_accessor == nullptr) {
      return err(Gltf_Mesh_Parse_Result, Gltf_Error_Failed_To_Parse_Mesh);
    }

    mesh.primitives.items[i].vertex_data = make_array(
        mesh.primitives.items[i].vertex_data,
        accessor_len * info->vertex_stride,
        allocator
    );
    mesh.primitives.items[i].index_data = make_array(
        mesh.primitives.items[i].index_data, index_accessor->count, allocator
    );

    if (cprim->material != nullptr) {
      mesh.primitives.items[i].material.some = true;
      mesh.primitives.items[i].material.value =
          cgltf_material_index(document->data, cprim->material);
    }

    if (mesh.primitives.items[i].vertex_data.items == nullptr ||
        mesh.primitives.items[i].index_data.items == nullptr) {
      primitives_failed = true;
      break;
    }

    for (usize j = 0; j < accessor_len; j += 1) {
      usize base_offset = j * info->vertex_stride;
      for (usize k = 0; k < Gltf_Supported_Attribute_MAX; k += 1) {
        if (info->attribute_formats[k].count == 0) {
          continue;
        }

        usize offset = base_offset + info->attribute_formats[k].offset;
        f32 *ptr = mesh.primitives.items[i].vertex_data.items + offset;

        for (usize l = 0; l < info->attribute_formats[k].count; l += 1) {
          ptr[l] = info->attribute_formats[k].splat;
        }

        cgltf_accessor *accessor = accessors[k];
        usize num_components = cgltf_num_components(accessor->type);
        if (num_components > info->attribute_formats[k].count) {
          return err(Gltf_Mesh_Parse_Result, Gltf_Error_Failed_To_Parse_Mesh);
        }

        cgltf_accessor_read_float(accessor, j, ptr, num_components);
      }
    }

    for (usize j = 0; j < index_accessor->count; j += 1) {
      u32 *index = array_get_ptr(mesh.primitives.items[i].index_data, j);
      cgltf_accessor_read_uint(index_accessor, j, index, 1);
    }
  }

  if (primitives_failed) {
    return err(Gltf_Mesh_Parse_Result, Gltf_Error_Failed_To_Parse_Mesh);
  }

  return_ok(Gltf_Mesh_Parse_Result, mesh);
}

Gltf_Material_Parse_Result
gltf_document_parse_material(Gltf_Document *document, usize index) {
  if (index >= document->data->materials_count) {
    return err(Gltf_Material_Parse_Result, Gltf_Error_Material_Not_Found);
  }
  cgltf_material *cmat = &document->data->materials[index];

  Gltf_Material material = {
    .hash = gltf_document_hash_index(document, index),
  };

  if (cmat->has_pbr_metallic_roughness) {
    // FIXME(nico): What the fuck is the fallback when no base color texture is
    // provided? I forgot..
    if (cmat->pbr_metallic_roughness.base_color_texture.texture == nullptr) {
      return err(
          Gltf_Material_Parse_Result, Gltf_Error_Failed_To_Parse_Material
      );
    }

    material.albedo_index.some = true;
    material.albedo_index.value = cgltf_texture_index(
        document->data, cmat->pbr_metallic_roughness.base_color_texture.texture
    );
  }

  return ok(Gltf_Material_Parse_Result, material);
}

Gltf_Texture_Parse_Result gltf_document_parse_texture(
    Gltf_Document *document, usize index, Allocator allocator
) {
  if (index >= document->data->textures_count) {
    return err(Gltf_Texture_Parse_Result, Gltf_Error_Failed_To_Parse_Texture);
  }
  cgltf_texture *ctexture = &document->data->textures[index];
  cgltf_image *cimage = ctexture->image;

  Gltf_Texture texture = {
    .hash = gltf_document_hash_index(document, index),
  };

  if (cimage->buffer_view != nullptr) {
    cgltf_buffer_view *cview = cimage->buffer_view;
    const u8 *cdata = cgltf_buffer_view_data(cview);

    i32 width, height, channels;
    byte *data = stbi_load_from_memory(
        cdata + cview->offset,
        (i32)cimage->buffer_view->size,
        &width,
        &height,
        &channels,
        4
    );
    defer {
      stbi_image_free(data);
    };

    texture.width = (u32)width;
    texture.height = (u32)height;
    texture.channels = (u32)channels;
    texture.data =
        make_array(texture.data, (usize)(width * height * channels), allocator);
    if (texture.data.items == nullptr) {
      return err(Gltf_Texture_Parse_Result, Gltf_Error_Failed_To_Parse_Texture);
    }

    memcpy(texture.data.items, data, sizeof(u8) * texture.data.len);
  } else if (cimage->uri != nullptr) {
    // NOTE(nico): Might be dog slow for base64. W/e for now
    String uri = from_cstring(cimage->uri);

    if (string_equal(string_slice(uri, 0, 5), from_cstring("data:"))) {
      return err(Gltf_Texture_Parse_Result, Gltf_Error_Invalid_Texture_Format);
    }

    // NOTE(nico): This is also a bit shit, but w/e
    if (document->filepath.len + uri.len >= 512) {
      return err(Gltf_Texture_Parse_Result, Gltf_Error_Failed_To_Parse_Texture);
    }

    char str_buf[512];
    String_Builder builder = make_builder_from_buf(str_buf, 512);
    fmt_printb(&builder, "{}/{}", document->filepath, uri);

    char *image_filepath = builder_terminate_string(&builder);

    i32 width, height, channels;
    byte *data = stbi_load(image_filepath, &width, &height, &channels, 4);
    defer {
      stbi_image_free(data);
    };

    texture.width = (u32)width;
    texture.height = (u32)height;
    texture.channels = (u32)channels;
    texture.data =
        make_array(texture.data, (usize)(width * height * channels), allocator);
    if (texture.data.items == nullptr) {
      return err(Gltf_Texture_Parse_Result, Gltf_Error_Failed_To_Parse_Texture);
    }

    memcpy(texture.data.items, data, sizeof(u8) * texture.data.len);
  } else {
    return err(Gltf_Texture_Parse_Result, Gltf_Error_Invalid_Texture_Format);
  }

  return ok(Gltf_Texture_Parse_Result, texture);
}