#include "render.h"

#include "cgltf.h"
#include "core/allocator.h"
#include "core/map.h"
#include "core/math.h"
#include "core/platform.h"

#include <math.h>

#define INSTANCE_CAP 65536
#define INSTANCE_DATA_BUFFER_SIZE sizeof(Instance_Data) * INSTANCE_CAP

typedef struct Global_Storage_Data {
  Mat4 mat_proj;
  Mat4 mat_view;
  Mat4 mat_proj_view;
  f32 total_time;
  f32 _padding[3];
} Global_Storage_Data;

static const char default_shader[] = {
#embed "../assets/shaders/default.wgsl"
  , '\0'
};

/////////////////////////////////////
// Actual rendering
/////////////////////////////////////
void init_renderer(
    Renderer *renderer, i32 render_w, i32 render_h, Allocator allocator
) {
  renderer->depth_texture =
      make_gpu_depth_texture((u32)render_w, (u32)render_h);

  renderer->geometry_buffer = make_gpu_buffer(&(GPU_Buffer_Create_Info){
    .usage = GPU_Buffer_Usage_Vertex | GPU_Buffer_Usage_Index |
             GPU_Buffer_Usage_Copy_Dst,
    // NOTE(nico): should be enough for now. The models are small
    .size = 64 * MEGABYTE,
  });
  renderer->storage_buffer = make_gpu_buffer(&(GPU_Buffer_Create_Info){
    .usage = GPU_Buffer_Usage_Uniform | GPU_Buffer_Usage_Storage |
             GPU_Buffer_Usage_Copy_Dst,
    .size = sizeof(Global_Storage_Data) + INSTANCE_DATA_BUFFER_SIZE + KILOBYTE,
  });
  renderer->gpu_global_data =
      gpu_buffer_alloc(&renderer->storage_buffer, sizeof(Global_Storage_Data));
  renderer->gpu_instances_data =
      gpu_buffer_alloc(&renderer->storage_buffer, INSTANCE_DATA_BUFFER_SIZE);
  assert(
      gpu_buffer_memory_is_valid(renderer->gpu_global_data) &&
      gpu_buffer_memory_is_valid(renderer->gpu_instances_data)
  );

  GPU_Vertex_Attribute vertex_attrs[] = {
    VERTEX_ATTR_F32x4(Vertex, position, 0),
    VERTEX_ATTR_F32x4(Vertex, normal, 1),
    VERTEX_ATTR_F32x2(Vertex, tex_coord, 2),
  };

  renderer->default_pipeline = make_gpu_pipeline(
      &(GPU_Pipeline_Create_Info){
        .shader_source =
            {
              .kind = GPU_Shader_Source_Raw,
              .data = from_c_str(default_shader),
            },
        .vertex_attributes = vertex_attrs,
        .vertex_attribute_count = 3,
        .vertex_stride = sizeof(Vertex),
        .depth_test = true,
        .bind_groups = ARRAY_LIT(
            GPU_Bind_Group_Create_Info,
            {
              .shader_data_infos = ARRAY_LIT(
                  GPU_Shader_Data_Info,
                  SHADER_UNIFORM(0, sizeof(Global_Storage_Data)),
                  SHADER_STORAGE(1, INSTANCE_DATA_BUFFER_SIZE)
              ),
            },
            {
              .shader_data_infos = ARRAY_LIT(
                  GPU_Shader_Data_Info, SHADER_TEXTURE(0), SHADER_SAMPLER(1)
              ),
            }
        ),
      },
      allocator
  );

  renderer->global_bind_group = gpu_pipeline_derive_bind_group(
      renderer->default_pipeline,
      (GPU_Shader_Data_Source_Array)ARRAY_LIT(
          GPU_Shader_Data_Source,
          BIND_MEMORY(renderer->gpu_global_data),
          BIND_MEMORY(renderer->gpu_instances_data)
      ),
      0,
      allocator
  );

  renderer->material_cache = make_open_map(
      u32, Material, 32, open_map_u32_hash, open_map_u32_eq, allocator
  );
  renderer->texture_cache = make_open_map(
      u32, GPU_Texture, 32, open_map_u32_hash, open_map_u32_eq, allocator
  );
  renderer->instances_data =
      make_array(renderer->instances_data, INSTANCE_CAP, allocator);

  byte white_pixel[4] = {255, 255, 255, 255};
  GPU_Texture white_texture = make_gpu_texture(&(GPU_Texture_Create_Info){
    .space = GPU_Texture_Space_sRGB,
    .kind = GPU_Texture_Create_Info_Raw_Memory,
    .raw = {.data = white_pixel, .width = 1, .height = 1, .channels = 4},
  });
  assert(gpu_texture_is_valid(white_texture));
  open_map_set(renderer->texture_cache, DEFAULT_MATERIAL_HANDLE, white_texture);

  renderer->default_material = (Material){
    .handle = DEFAULT_MATERIAL_HANDLE,
    .map_albedo = white_texture,
    .sampler = make_gpu_sampler(&(GPU_Sampler_Create_Info){
      .filter = GPU_Sampler_Filter_Nearest,
      .wrap = GPU_Sampler_Wrap_Repeat,
    }),
  };
  renderer->default_material.bind_group = gpu_pipeline_derive_bind_group(
      renderer->default_pipeline,
      (GPU_Shader_Data_Source_Array)ARRAY_LIT(
          GPU_Shader_Data_Source,
          BIND_TEXTURE(renderer->default_material.map_albedo),
          BIND_SAMPLER(renderer->default_material.sampler)
      ),
      1,
      allocator
  );
  open_map_set(
      renderer->material_cache,
      DEFAULT_MATERIAL_HANDLE,
      renderer->default_material
  );
}

void destroy_renderer(Renderer *renderer) {
  delete_array(renderer->instances_data);

  Open_Map_Iterator it = open_map_iterator(renderer->material_cache);
  while (open_map_has_next(&it)) {
    Material *material = open_map_next(&it);
    destroy_gpu_sampler(material->sampler);
    destroy_gpu_bind_group(material->bind_group);
  }
  delete_open_map(renderer->material_cache);

  it = open_map_iterator(renderer->texture_cache);
  while (open_map_has_next(&it)) {
    GPU_Texture *texture = open_map_next(&it);
    destroy_gpu_texture(*texture);
  }
  delete_open_map(renderer->texture_cache);

  destroy_gpu_bind_group(renderer->global_bind_group);
  destroy_gpu_pipeline(renderer->default_pipeline);
  destroy_gpu_buffer(renderer->geometry_buffer);
  destroy_gpu_buffer(renderer->storage_buffer);
  destroy_gpu_texture(renderer->depth_texture);
}

void begin_render(Renderer *renderer, Raw_Camera *camera) {
  Global_Storage_Data globals = {
    .mat_proj = camera->mat_proj,
    .mat_view = camera->mat_view,
    .mat_proj_view = camera->mat_proj_view,
  };
  gpu_buffer_write(
      renderer->gpu_global_data, &globals, sizeof(Global_Storage_Data)
  );

  renderer->_active_pass = gpu_render_pass_begin(&(GPU_Render_Pass_Create_Info){
    .depth = {.some = true, .value = renderer->depth_texture},
    .clear = true,
    .clear_colors = {(Color){.raw = {0.1f, 0.1f, 0.1f, 1.f}}},
  });

  gpu_render_pass_bind_pipeline(
      renderer->_active_pass, renderer->default_pipeline
  );
  gpu_render_pass_bind_group(
      renderer->_active_pass, renderer->global_bind_group
  );
}

void end_render(Renderer *renderer) {
  gpu_buffer_write(
      renderer->gpu_instances_data,
      renderer->instances_data.items,
      renderer->instance_count * sizeof(Instance_Data)
  );
  gpu_render_pass_end(renderer->_active_pass);

  renderer->instance_count = 0;
}

void draw_model(Renderer *renderer, Model_Draw_Info *info) {
  for (usize i = 0; i < info->model.primitive_count; i += 1) {
    Mesh_Primitive *primitive = &info->model.primitives[i];
    Material *material =
        open_map_get(renderer->material_cache, primitive->material_handle);
    assert(material != nullptr);

    gpu_render_pass_bind_group(renderer->_active_pass, material->bind_group);
    gpu_render_pass_draw_indexed(
        renderer->_active_pass,
        primitive->gpu_vertices,
        primitive->gpu_indices,
        primitive->index_count,
        renderer->instance_count
    );
  }

  Instance_Data data = {
    .transform = info->transform,
    .normal = mat4_transpose(mat4_inverse(info->transform)),
    .color = info->color,
  };

  array_set(renderer->instances_data, renderer->instance_count, data);
  renderer->instance_count += 1;
}

void draw_mesh_primitive(Renderer *renderer, Mesh_Primitive_Draw_Info *info) {
  Mesh_Primitive primitive = info->primitive;
  Material *material =
      open_map_get(renderer->material_cache, primitive.material_handle);
  assert(material != nullptr);

  gpu_render_pass_bind_group(renderer->_active_pass, material->bind_group);
  gpu_render_pass_draw_indexed(
      renderer->_active_pass,
      primitive.gpu_vertices,
      primitive.gpu_indices,
      primitive.index_count,
      renderer->instance_count
  );

  Instance_Data data = {
    .transform = info->transform,
    .normal = mat4_transpose(mat4_inverse(info->transform)),
    .color = info->color,
  };

  array_set(renderer->instances_data, renderer->instance_count, data);
  renderer->instance_count += 1;
}

static AABB_Collider get_vertex_array_aabb_collider(Vertex_Array vertices) {
  Vec3 min = vec3(INFINITY, INFINITY, INFINITY);
  Vec3 max = vec3(-INFINITY, -INFINITY, -INFINITY);
  for (usize i = 0; i < vertices.len; i += 1) {
    min.x = min_f32(min.x, vertices.items[i].position.x);
    min.y = min_f32(min.y, vertices.items[i].position.y);
    min.z = min_f32(min.z, vertices.items[i].position.z);

    max.x = max_f32(max.x, vertices.items[i].position.x);
    max.y = max_f32(max.y, vertices.items[i].position.y);
    max.z = max_f32(max.z, vertices.items[i].position.z);
  }

  return (AABB_Collider){
    .min = min,
    .max = max,
  };
}

static GPU_Texture
texture_load_gltf(Renderer *renderer, cgltf_texture *ctex, String root_path) {
  // TODO(nico): validate the type
  assert(ctex != nullptr && ctex->image->uri);
  usize uri_len = c_str_len(ctex->image->uri);
  u32 uri_hash = (u32)hash_fnv1a(ctex->image->uri, uri_len);

  GPU_Texture *existing_texture =
      open_map_get(renderer->texture_cache, uri_hash);
  if (existing_texture != nullptr) {
    return *existing_texture;
  }

  char str_buf[512];
  String_Builder b = make_builder_from_buf(&str_buf[0], 512);
  builder_write(&b, "%s/%ss", root_path, ctex->image->uri);
  builder_terminate_string(&b);

  GPU_Texture texture = make_gpu_texture(&(GPU_Texture_Create_Info){
    .space = GPU_Texture_Space_sRGB,
    .kind = GPU_Texture_Create_Info_File,
    .file_path = builder_get_string(&b),
  });
  open_map_set(renderer->texture_cache, uri_hash, texture);

  return texture;
}

static Material_Create_Result material_load_gltf(
    Renderer *renderer,
    cgltf_material *cmat,
    String root_path,
    Allocator allocator
) {
  // NOTE(nico): We need the name to hash everything
  assert(cmat->name != nullptr);
  usize name_len = c_str_len(cmat->name);
  u32 name_hash = (u32)hash_fnv1a(
      cmat->name, name_len
  ); // TODO(nico): the cast might be a problem

  Material *existing_material =
      open_map_get(renderer->material_cache, name_hash);
  if (existing_material != nullptr) {
    return ok(Material_Create_Result, *existing_material);
  }

  Material material = {
    .handle = name_hash,
  };

  if (cmat && cmat->has_pbr_metallic_roughness) {
    material.map_albedo = texture_load_gltf(
        renderer,
        cmat->pbr_metallic_roughness.base_color_texture.texture,
        root_path
    );
  }
  assert(gpu_texture_is_valid(material.map_albedo));

  material.sampler = make_gpu_sampler(&(GPU_Sampler_Create_Info){
    .filter = GPU_Sampler_Filter_Linear,
    .wrap = GPU_Sampler_Wrap_Repeat,
  });

  material.bind_group = gpu_pipeline_derive_bind_group(
      renderer->default_pipeline,
      (GPU_Shader_Data_Source_Array)ARRAY_LIT(
          GPU_Shader_Data_Source,
          BIND_TEXTURE(material.map_albedo),
          BIND_SAMPLER(material.sampler)
      ),
      1,
      allocator
  );

  open_map_set(renderer->material_cache, name_hash, material);
  return ok(Material_Create_Result, material);
}

#define V(px, py, pz, nx, ny, nz, u, v)                                        \
  {                                                                            \
    .position = {.raw = {px, py, pz, 1.f}},                                    \
    .normal = {.raw = {nx, ny, nz, 0.f}},                                      \
    .tex_coord = {.raw = {u, v}},                                              \
  }

Model_Create_Result model_make_cube(Renderer *renderer, Material *material) {
  // 4 vertices per face for hard edges, CCW front faces
  Vertex vertices[] = {
    // +Y (top)
    V(-.5f, .5f, -.5f, 0.f, 1.f, 0.f, 0.f, 0.f),
    V(-.5f, .5f, .5f, 0.f, 1.f, 0.f, 0.f, 1.f),
    V(.5f, .5f, .5f, 0.f, 1.f, 0.f, 1.f, 1.f),
    V(.5f, .5f, -.5f, 0.f, 1.f, 0.f, 1.f, 0.f),
    // -Y (bottom)
    V(-.5f, -.5f, -.5f, 0.f, -1.f, 0.f, 0.f, 0.f),
    V(.5f, -.5f, -.5f, 0.f, -1.f, 0.f, 1.f, 0.f),
    V(.5f, -.5f, .5f, 0.f, -1.f, 0.f, 1.f, 1.f),
    V(-.5f, -.5f, .5f, 0.f, -1.f, 0.f, 0.f, 1.f),
    // +X
    V(.5f, -.5f, -.5f, 1.f, 0.f, 0.f, 0.f, 1.f),
    V(.5f, .5f, -.5f, 1.f, 0.f, 0.f, 0.f, 0.f),
    V(.5f, .5f, .5f, 1.f, 0.f, 0.f, 1.f, 0.f),
    V(.5f, -.5f, .5f, 1.f, 0.f, 0.f, 1.f, 1.f),
    // -X
    V(-.5f, -.5f, -.5f, -1.f, 0.f, 0.f, 1.f, 1.f),
    V(-.5f, -.5f, .5f, -1.f, 0.f, 0.f, 0.f, 1.f),
    V(-.5f, .5f, .5f, -1.f, 0.f, 0.f, 0.f, 0.f),
    V(-.5f, .5f, -.5f, -1.f, 0.f, 0.f, 1.f, 0.f),
    // +Z
    V(-.5f, -.5f, .5f, 0.f, 0.f, 1.f, 0.f, 1.f),
    V(.5f, -.5f, .5f, 0.f, 0.f, 1.f, 1.f, 1.f),
    V(.5f, .5f, .5f, 0.f, 0.f, 1.f, 1.f, 0.f),
    V(-.5f, .5f, .5f, 0.f, 0.f, 1.f, 0.f, 0.f),
    // -Z
    V(-.5f, -.5f, -.5f, 0.f, 0.f, -1.f, 1.f, 1.f),
    V(-.5f, .5f, -.5f, 0.f, 0.f, -1.f, 1.f, 0.f),
    V(.5f, .5f, -.5f, 0.f, 0.f, -1.f, 0.f, 0.f),
    V(.5f, -.5f, -.5f, 0.f, 0.f, -1.f, 0.f, 1.f),
  };

  u32 indices[36];
  for (u32 face = 0; face < 6; face += 1) {
    u32 base = face * 4;
    indices[face * 6 + 0] = base;
    indices[face * 6 + 1] = base + 1;
    indices[face * 6 + 2] = base + 2;
    indices[face * 6 + 3] = base;
    indices[face * 6 + 4] = base + 2;
    indices[face * 6 + 5] = base + 3;
  }

  return model_load_from_geometry(
      renderer,
      (Vertex_Array){.items = vertices, .len = 24},
      (Index_Array){.items = indices, .len = 36},
      material
  );
}

Model_Create_Result model_make_plane(Renderer *renderer, Material *material) {
  Vertex vertices[] = {
    V(-.5f, 0.f, -.5f, 0.f, 1.f, 0.f, 0.f, 0.f),
    V(-.5f, 0.f, .5f, 0.f, 1.f, 0.f, 0.f, 1.f),
    V(.5f, 0.f, .5f, 0.f, 1.f, 0.f, 1.f, 1.f),
    V(.5f, 0.f, -.5f, 0.f, 1.f, 0.f, 1.f, 0.f),
  };
  u32 indices[] = {0, 1, 2, 0, 2, 3};

  return model_load_from_geometry(
      renderer,
      (Vertex_Array){.items = vertices, .len = 4},
      (Index_Array){.items = indices, .len = 6},
      material
  );
}

#undef V

Model_Create_Result model_load_from_geometry(
    Renderer *renderer,
    Vertex_Array vertices,
    Index_Array indices,
    Material *material
) {
  assert(material != nullptr);

  Model model = {
    .primitives =
        {
          [0] =
              (Mesh_Primitive){
                .material_handle = material->handle,
                .index_count = indices.len,
                .gpu_vertices = gpu_buffer_append(
                    &renderer->geometry_buffer,
                    vertices.items,
                    sizeof(vertices.items[0]) * vertices.len
                ),
                .gpu_indices = gpu_buffer_append(
                    &renderer->geometry_buffer,
                    indices.items,
                    sizeof(indices.items[0]) * indices.len
                ),
              },
        },
    .primitive_count = 1,
    .collider = get_vertex_array_aabb_collider(vertices),
  };

  return ok(Model_Create_Result, model);
}

Model_Create_Result model_load_gltf_from_file(
    Renderer *renderer,
    Model_Create_Info *info,
    Allocator allocator,
    Allocator temp_allocator
) {
  assert(info->root_path.len > 0);
  cgltf_data *data = (cgltf_data *)info->gltf_data;

  if (data->nodes_count == 0) {
    return err(Model_Create_Result, Model_Create_Error_Emtpy_GLTF_File);
  }

  Model model = {0};
  cgltf_mesh *mesh = nullptr;
  for (usize i = 0; i < data->nodes_count; i += 1) {
    cgltf_node *node = &data->nodes[i];
    String name = from_c_str(node->name);
    if (string_equal(name, info->model_name) && node->mesh != nullptr) {
      mesh = node->mesh;
      break;
    }
  }

  assert(mesh != nullptr && mesh->primitives_count <= MESH_PRIMITIVE_CAP);
  model.primitive_count = mesh->primitives_count;
  for (usize i = 0; i < model.primitive_count; i += 1) {
    cgltf_primitive *prim = &mesh->primitives[i];

    cgltf_accessor *pos_accessor = nullptr;
    cgltf_accessor *norm_accessor = nullptr;
    cgltf_accessor *tex_coord_accessor = nullptr;

    for (usize j = 0; j < prim->attributes_count; j += 1) {
      cgltf_attribute *attribute = &prim->attributes[j];

      if (attribute->type == cgltf_attribute_type_position) {
        pos_accessor = attribute->data;
      } else if (attribute->type == cgltf_attribute_type_normal) {
        norm_accessor = attribute->data;
      } else if (
          attribute->type == cgltf_attribute_type_texcoord &&
          attribute->index == 0
      ) {
        // FIXME(nico): add support for multiple uv channels when needed
        tex_coord_accessor = attribute->data;
      }
    }

    assert(pos_accessor != nullptr);
    assert(norm_accessor != nullptr);
    assert(tex_coord_accessor != nullptr);
    assert(
        pos_accessor->count == norm_accessor->count &&
        pos_accessor->count == tex_coord_accessor->count
    );

    Vertex_Array vertices =
        make_array(vertices, pos_accessor->count, allocator);
    // Vertex *vertices =
    //     temp_allocator.alloc(temp_allocator, sizeof(Vertex) * vertex_count)
    //         .allocation;

    for (usize j = 0; j < vertices.len; j += 1) {
      Vertex *v = array_get_ptr(vertices, j);
      cgltf_accessor_read_float(pos_accessor, j, v->position.raw, 3);
      v->position.w = 1.0f;
      cgltf_accessor_read_float(norm_accessor, j, v->normal.raw, 3);
      v->normal.w = 0.0f;
      cgltf_accessor_read_float(tex_coord_accessor, j, v->tex_coord.raw, 2);
    }

    cgltf_accessor *index_accessor = prim->indices;
    assert(index_accessor != nullptr);

    usize index_count = index_accessor->count;
    u32 *indices =
        temp_allocator.alloc(temp_allocator, sizeof(u32) * index_count)
            .allocation;

    for (usize j = 0; j < index_count; j += 1) {
      cgltf_accessor_read_uint(index_accessor, j, &indices[j], 1);
    }

    Material_Create_Result material_result = material_load_gltf(
        renderer, prim->material, info->root_path, allocator
    );
    assert(material_result.ok);

    model.primitives[i] = (Mesh_Primitive){
      .material_handle = material_result.value.handle,
      .index_count = index_count,
      .gpu_vertices = gpu_buffer_append(
          &renderer->geometry_buffer,
          vertices.items,
          sizeof(Vertex) * vertices.len
      ),
      .gpu_indices = gpu_buffer_append(
          &renderer->geometry_buffer, indices, sizeof(u32) * index_count
      ),
      .collider = get_vertex_array_aabb_collider(vertices),
    };
  }

  model.collider.min = vec3(INFINITY, INFINITY, INFINITY);
  model.collider.max = vec3(-INFINITY, -INFINITY, -INFINITY);
  for (usize i = 0; i < model.primitive_count; i += 1) {
    model.collider.min.x =
        min_f32(model.collider.min.x, model.primitives[i].collider.min.x);
    model.collider.min.y =
        min_f32(model.collider.min.y, model.primitives[i].collider.min.y);
    model.collider.min.z =
        min_f32(model.collider.min.z, model.primitives[i].collider.min.z);

    model.collider.max.x =
        max_f32(model.collider.max.x, model.primitives[i].collider.max.x);
    model.collider.max.y =
        max_f32(model.collider.max.y, model.primitives[i].collider.max.y);
    model.collider.max.z =
        max_f32(model.collider.max.z, model.primitives[i].collider.max.z);
  }

  return ok(Model_Create_Result, model);
}

Mesh_Primitive_Create_Result mesh_primitive_load_from_geometry(
    Renderer *renderer, Mesh_Primitive_Create_Info *info
) {
  assert(info->material != nullptr);

  Mesh_Primitive primitive = (Mesh_Primitive){
    .material_handle = info->material->handle,
    .index_count = info->indices.len,
    .gpu_vertices = gpu_buffer_append(
        &renderer->geometry_buffer,
        info->vertices.items,
        sizeof(info->vertices.items[0]) * info->vertices.len
    ),
    .gpu_indices = gpu_buffer_append(
        &renderer->geometry_buffer,
        info->indices.items,
        sizeof(info->indices.items[0]) * info->indices.len
    ),
  };

  return ok(Mesh_Primitive_Create_Result, primitive);
}

bool32 mesh_primitive_update_from_geometry(
    Mesh_Primitive *primitive, Mesh_Primitive_Update_Info *info
) {
  if (info->vertices.len == 0 || info->indices.len == 0) {
    return false;
  }

  // FIXME(nico): for completeness, even if the gpu abstraction asserts on write
  // too large, this procedure should handle it manually

  gpu_buffer_write(
      primitive->gpu_vertices,
      info->vertices.items,
      sizeof(info->vertices.items[0]) * info->vertices.len
  );
  gpu_buffer_write(
      primitive->gpu_indices,
      info->indices.items,
      sizeof(info->indices.items[0]) * info->indices.len
  );

  // NOTE(nico): this is a bit dodgy.. I haven't decided how this case should be
  // handled
  primitive->index_count = info->indices.len;

  return true;
}

#if defined DEBUG
#define DEBUG_LINE_CAP 65536
#define DEBUG_LINE_BUFFER_SIZE DEBUG_LINE_CAP * sizeof(Debug_Vertex)

static const char debug_shader[] = {
#embed "../assets/shaders/debug.wgsl"
  , '\0'
};

void init_debug_renderer(
    Debug_Renderer *renderer, i32 render_w, i32 render_h, Allocator allocator
) {
  (void)render_w;
  (void)render_h;
  renderer->geometry_buffer = make_gpu_buffer(&(GPU_Buffer_Create_Info){
    .usage = GPU_Buffer_Usage_Vertex | GPU_Buffer_Usage_Copy_Dst,
    .size = DEBUG_LINE_BUFFER_SIZE,
  });
  renderer->storage_buffer = make_gpu_buffer(&(GPU_Buffer_Create_Info){
    .usage = GPU_Buffer_Usage_Uniform | GPU_Buffer_Usage_Copy_Dst,
    .size = sizeof(Global_Storage_Data),
  });

  renderer->gpu_vertices =
      gpu_buffer_alloc(&renderer->geometry_buffer, DEBUG_LINE_BUFFER_SIZE);
  renderer->gpu_global_data =
      gpu_buffer_alloc(&renderer->storage_buffer, sizeof(Global_Storage_Data));

  renderer->cpu_vertices =
      make_array(renderer->cpu_vertices, DEBUG_LINE_CAP, allocator);

  GPU_Vertex_Attribute vertex_attrs[] = {
    VERTEX_ATTR_F32x3(Debug_Vertex, position, 0),
    VERTEX_ATTR_U32(Debug_Vertex, color, 1),
  };

  renderer->pipeline = make_gpu_pipeline(
      &(GPU_Pipeline_Create_Info){
        .shader_source =
            {
              .kind = GPU_Shader_Source_Raw,
              .data = from_c_str(debug_shader),
            },
        .vertex_attributes = vertex_attrs,
        .vertex_attribute_count = 2,
        .vertex_stride = sizeof(Debug_Vertex),
        .primitive = {.some = true, .value = GPU_Primitive_Line},
        .depth_test = true,
        .bind_groups = ARRAY_LIT(
            GPU_Bind_Group_Create_Info,
            {
              .shader_data_infos = ARRAY_LIT(
                  GPU_Shader_Data_Info,
                  SHADER_UNIFORM(0, sizeof(Global_Storage_Data)),
              ),
            },
        ),
      },
      allocator
  );

  renderer->global_bind_group = gpu_pipeline_derive_bind_group(
      renderer->pipeline,
      (GPU_Shader_Data_Source_Array)ARRAY_LIT(
          GPU_Shader_Data_Source, BIND_MEMORY(renderer->gpu_global_data)
      ),
      0,
      allocator
  );
}

void destroy_debug_renderer(Debug_Renderer *renderer) {
  delete_array(renderer->cpu_vertices);

  destroy_gpu_pipeline(renderer->pipeline);
  destroy_gpu_bind_group(renderer->global_bind_group);
  destroy_gpu_buffer(renderer->geometry_buffer);
  destroy_gpu_buffer(renderer->storage_buffer);
}

void begin_debug_render(
    Debug_Renderer *renderer, Raw_Camera *camera, GPU_Texture *depth_texture
) {
  Global_Storage_Data globals = {
    .mat_proj = camera->mat_proj,
    .mat_view = camera->mat_view,
    .mat_proj_view = camera->mat_proj_view,
  };
  gpu_buffer_write(
      renderer->gpu_global_data, &globals, sizeof(Global_Storage_Data)
  );

  GPU_Texture _depth_texture = {0};
  if (depth_texture != nullptr) {
    _depth_texture = *depth_texture;
  }

  renderer->_active_pass = gpu_render_pass_begin(&(GPU_Render_Pass_Create_Info){
    .depth = {.some = depth_texture != nullptr, .value = _depth_texture},
    .clear = false,
  });

  gpu_render_pass_bind_pipeline(renderer->_active_pass, renderer->pipeline);
  gpu_render_pass_bind_group(
      renderer->_active_pass, renderer->global_bind_group
  );
}

void end_debug_render(Debug_Renderer *renderer) {
  gpu_buffer_write(
      renderer->gpu_vertices,
      renderer->cpu_vertices.items,
      renderer->vertex_count * sizeof(Debug_Vertex)
  );

  gpu_render_pass_draw(
      renderer->_active_pass, renderer->gpu_vertices, renderer->vertex_count
  );
  gpu_render_pass_end(renderer->_active_pass);
  renderer->vertex_count = 0;
}

static u32 color_to_u32(Color color) {
  u32 _c = ((u32)(color.r * 255.f) << 24) | ((u32)(color.g * 255.f) << 16) |
           ((u32)(color.b * 255.f) << 8) | ((u32)(color.a * 255.f));

  return _c;
}

void draw_debug_line(
    Debug_Renderer *renderer, Vec3 start, Vec3 end, Color color
) {
  u32 _c = color_to_u32(color);

  array_set(
      renderer->cpu_vertices,
      renderer->vertex_count++,
      ((Debug_Vertex){
        .position = start,
        .color = _c,
      })
  );
  array_set(
      renderer->cpu_vertices,
      renderer->vertex_count++,
      ((Debug_Vertex){
        .position = end,
        .color = _c,
      })
  );
}

void draw_debug_cube(
    Debug_Renderer *renderer, Vec3 min, Vec3 max, Color color
) {
  Vec3 bottom_bl = min;
  Vec3 bottom_br = vec3(max.x, min.y, min.z);
  Vec3 bottom_fl = vec3(min.x, min.y, max.z);
  Vec3 bottom_fr = vec3(max.x, min.y, max.z);

  Vec3 top_bl = vec3(min.x, max.y, min.z);
  Vec3 top_br = vec3(max.x, max.y, min.z);
  Vec3 top_fl = vec3(min.x, max.y, max.z);
  Vec3 top_fr = max;

  draw_debug_line(renderer, bottom_bl, bottom_br, color);
  draw_debug_line(renderer, bottom_br, bottom_fr, color);
  draw_debug_line(renderer, bottom_fr, bottom_fl, color);
  draw_debug_line(renderer, bottom_fl, bottom_bl, color);

  draw_debug_line(renderer, top_bl, top_br, color);
  draw_debug_line(renderer, top_br, top_fr, color);
  draw_debug_line(renderer, top_fr, top_fl, color);
  draw_debug_line(renderer, top_fl, top_bl, color);

  draw_debug_line(renderer, bottom_bl, top_bl, color);
  draw_debug_line(renderer, bottom_br, top_br, color);
  draw_debug_line(renderer, bottom_fl, top_fl, color);
  draw_debug_line(renderer, bottom_fr, top_fr, color);
}

void draw_debug_obb(
    Debug_Renderer *renderer, Vec3 center, Quat rotation, Vec3 half, Color c
) {
  Vec3 corners[8];
  for (i32 i = 0; i < 8; i += 1) {
    Vec3 local = vec3(
        (i & 1) ? half.x : -half.x,
        (i & 2) ? half.y : -half.y,
        (i & 4) ? half.z : -half.z
    );
    corners[i] = vec3_add(center, vec3_rotate_by_quat(local, rotation));
  }

  static const i32 edges[12][2] = {
    {0, 1},
    {2, 3},
    {4, 5},
    {6, 7},
    {0, 2},
    {1, 3},
    {4, 6},
    {5, 7},
    {0, 4},
    {1, 5},
    {2, 6},
    {3, 7},
  };

  for (i32 i = 0; i < 12; i += 1) {
    draw_debug_line(renderer, corners[edges[i][0]], corners[edges[i][1]], c);
  }
}

// FIXME(nico): Kinda bad. Needs a refactor
void draw_debug_camera_frustum(
    Debug_Renderer *renderer, Raw_Camera *camera, Color color
) {
  if (camera->frame_width <= 0.f || camera->frame_height <= 0.f) {
    return;
  }

  f32 aspect = camera->frame_width / camera->frame_height;
  f32 tan_half_fovy = tanf(to_radians_f32(camera->fovy) * 0.5f);

  f32 near_half_h = camera->z_near * tan_half_fovy;
  f32 near_half_w = near_half_h * aspect;
  f32 far_half_h = camera->z_far * tan_half_fovy;
  f32 far_half_w = far_half_h * aspect;

  Vec3 forward = vec3_normalize(vec3_sub(camera->target, camera->position));
  Vec3 right = vec3_normalize(vec3_cross(forward, camera->up));
  Vec3 up = vec3_normalize(vec3_cross(right, forward));

  Vec3 near_center =
      vec3_add(camera->position, vec3_scale(forward, camera->z_near));
  Vec3 far_center =
      vec3_add(camera->position, vec3_scale(forward, camera->z_far));

  Vec3 near_tl = vec3_add(
      vec3_add(near_center, vec3_scale(up, near_half_h)),
      vec3_scale(right, -near_half_w)
  );
  Vec3 near_tr = vec3_add(
      vec3_add(near_center, vec3_scale(up, near_half_h)),
      vec3_scale(right, near_half_w)
  );
  Vec3 near_bl = vec3_add(
      vec3_add(near_center, vec3_scale(up, -near_half_h)),
      vec3_scale(right, -near_half_w)
  );
  Vec3 near_br = vec3_add(
      vec3_add(near_center, vec3_scale(up, -near_half_h)),
      vec3_scale(right, near_half_w)
  );

  Vec3 far_tl = vec3_add(
      vec3_add(far_center, vec3_scale(up, far_half_h)),
      vec3_scale(right, -far_half_w)
  );
  Vec3 far_tr = vec3_add(
      vec3_add(far_center, vec3_scale(up, far_half_h)),
      vec3_scale(right, far_half_w)
  );
  Vec3 far_bl = vec3_add(
      vec3_add(far_center, vec3_scale(up, -far_half_h)),
      vec3_scale(right, -far_half_w)
  );
  Vec3 far_br = vec3_add(
      vec3_add(far_center, vec3_scale(up, -far_half_h)),
      vec3_scale(right, far_half_w)
  );

  draw_debug_line(renderer, near_tl, near_tr, color);
  draw_debug_line(renderer, near_tr, near_br, color);
  draw_debug_line(renderer, near_br, near_bl, color);
  draw_debug_line(renderer, near_bl, near_tl, color);

  draw_debug_line(renderer, far_tl, far_tr, color);
  draw_debug_line(renderer, far_tr, far_br, color);
  draw_debug_line(renderer, far_br, far_bl, color);
  draw_debug_line(renderer, far_bl, far_tl, color);

  draw_debug_line(renderer, near_tl, far_tl, color);
  draw_debug_line(renderer, near_tr, far_tr, color);
  draw_debug_line(renderer, near_bl, far_bl, color);
  draw_debug_line(renderer, near_br, far_br, color);
}

#endif