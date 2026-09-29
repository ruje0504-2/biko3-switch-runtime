/* Private implementation included after renderer storage/wait helpers. */
#ifndef BK_VULKAN_SKIN_INTERNAL_H
#define BK_VULKAN_SKIN_INTERNAL_H
static int skin_pipeline_create(BkRenderer *r, char error[256]) {
  VkShaderModule module = VK_NULL_HANDLE;
  VkDescriptorSetLayoutBinding b[4];
  for (unsigned i = 0; i < 4; i++)
    b[i] = (VkDescriptorSetLayoutBinding){
        .binding = i,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT};
  VkDescriptorSetLayoutCreateInfo dl = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 4,
      .pBindings = b};
  VK_TRY(vkCreateDescriptorSetLayout(r->device, &dl, NULL,
                                     &r->skin_descriptor_layout));
  VkPipelineLayoutCreateInfo layout = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &r->skin_descriptor_layout};
  VK_TRY(vkCreatePipelineLayout(r->device, &layout, NULL, &r->skin_layout));
  VkShaderModuleCreateInfo sm = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = sizeof(bk_skin_comp_spv),
      .pCode = bk_skin_comp_spv};
  VK_TRY(vkCreateShaderModule(r->device, &sm, NULL, &module));
  VkComputePipelineCreateInfo cp = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = module,
                .pName = "main"},
      .layout = r->skin_layout};
  VK_TRY(vkCreateComputePipelines(r->device, VK_NULL_HANDLE, 1, &cp, NULL,
                                  &r->skin_pipeline));
  vkDestroyShaderModule(r->device, module, NULL);
  return 1;
fail:
  if (module)
    vkDestroyShaderModule(r->device, module, NULL);
  return 0;
}
static void skin_queue(BkRenderer *r, BkGpuMesh *m) {
  m->skin_replay = 0;
  if (!m->skin_pending) {
    m->next_skin = r->skin_updates;
    r->skin_updates = m;
    m->skin_pending = 1;
  }
}
static int skin_unit_w(const float *p) {
  double delta = (double)p[15] - 1.0;
  return p[3] == 0 && p[7] == 0 && p[11] == 0 &&
         delta >= -(double)1e-5f && delta <= (double)1e-5f;
}
static int skin_coordinate_valid(const float *matrix, const float position[3]) {
  if (skin_unit_w(matrix))
    return 1;
  float transformed[3];
  return bk_matrix_transform_coord(transformed, position, matrix);
}
BkSkinPalette *bk_skin_palette_create(BkRenderer *r, unsigned count,
                                      char error[256]) {
  if (!r || r->active || !count || count > 65536) {
    snprintf(error, 256, "invalid skin palette dimensions/state");
    return NULL;
  }
  BkSkinPalette *p = calloc(1, sizeof(*p));
  if (!p) {
    snprintf(error, 256, "skin palette allocation failed");
    return NULL;
  }
  p->owner = r;
  p->count = count;
  p->references = 1;
  p->used = calloc(count, 1);
  if (!p->used) {
    snprintf(error, 256, "skin palette allocation failed");
    goto fail;
  }
  if (!buffer_create(r, &p->matrices, (size_t)count * 64,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, error))
    goto fail;
  for (unsigned i = 0; i < count; i++)
    memcpy((float *)p->matrices.mapped + i * 16, bk_identity, 64);
  VK_TRY(buffer_flush(r, &p->matrices, 0));
  return p;
fail:
  bk_skin_palette_destroy(r, p);
  return NULL;
}
void bk_skin_palette_destroy(BkRenderer *r, BkSkinPalette *p) {
  if (!p || p->owner != r || --p->references)
    return;
  if (r->submitted) {
    vkDeviceWaitIdle(r->device);
    r->submitted = 0;
  }
  buffer_destroy(r, &p->matrices);
  free(p->used);
  free(p);
}
int bk_skin_palette_update(BkRenderer *r, BkSkinPalette *p, const float *world,
                           unsigned count, char error[256]) {
  if (!r || !p || p->owner != r || r->active || !world || count != p->count) {
    snprintf(error, 256, "invalid skin palette update");
    return 0;
  }
  int check_coordinates = 0;
  for (unsigned i = 0; i < count; i++) {
    for (unsigned j = 0; j < 16; j++)
      if (!isfinite(world[i * 16 + j])) {
        snprintf(error, 256, "nonfinite skin matrix");
        return 0;
      }
    if (p->used[i] && !skin_unit_w(world + i * 16))
      check_coordinates = 1;
  }
  size_t bytes = (size_t)count * 64;
  if (!memcmp(world, p->matrices.mapped, bytes))
    return 1;
  /* Ordinary affine/near-unit palettes never scan vertices. For a genuine
   * homogeneous correction, reject invalid bound coordinates before any
   * upload or dispatch is queued; the previous palette stays intact. */
  if (check_coordinates)
    for (BkGpuMesh *m = p->meshes; m; m = m->next_palette) {
      const uint32_t *header = m->skin_weights.mapped;
      unsigned weight_count = header[m->vertex_count + 1];
      const BkGpuSkinWeight *weights =
          (const void *)(header + m->vertex_count + 2);
      for (unsigned i = 0; i < weight_count; i++) {
        const BkGpuSkinWeight *w = weights + i;
        if (!skin_coordinate_valid(world + w->bone * 16, w->position)) {
          snprintf(error, 256, "invalid skin homogeneous coordinate at bone%u",
                   w->bone);
          return 0;
        }
      }
    }
  if (!wait_frame(r, error))
    return 0;
  memcpy(p->matrices.mapped, world, bytes);
  VK_TRY(buffer_flush(r, &p->matrices, 0));
  for (BkGpuMesh *m = p->meshes; m; m = m->next_palette)
    skin_queue(r, m);
  r->stats.uploaded_bytes += bytes;
  return 1;
fail:
  return 0;
}
int bk_lit_mesh_skin(BkRenderer *r, BkGpuMesh *m, BkSkinPalette *p,
                     const uint32_t *offsets, const BkGpuSkinWeight *weights,
                     unsigned count, char error[256]) {
  if (!r || !m || m->owner != r || !m->lit || m->palette || r->active || !p ||
      p->owner != r || !offsets || !weights || !count || count > 2000000 ||
      offsets[0] || offsets[m->vertex_count] != count) {
    snprintf(error, 256, "invalid GPU skin binding");
    return 0;
  }
  for (unsigned i = 0; i < m->vertex_count; i++) {
    if (offsets[i] > offsets[i + 1] || offsets[i + 1] > count ||
        (offsets[i] < offsets[i + 1] && !weights[offsets[i]].reset)) {
      snprintf(error, 256, "invalid GPU skin vertex ranges/reset");
      return 0;
    }
  }
  for (unsigned i = 0; i < count; i++) {
    const BkGpuSkinWeight *w = weights + i;
    if (w->bone >= p->count || w->reset > 1 || !isfinite(w->weight)) {
      snprintf(error, 256, "invalid GPU skin weight/bone");
      return 0;
    }
    for (unsigned j = 0; j < 3; j++)
      if (!isfinite(w->position[j]) || !isfinite(w->normal[j])) {
        snprintf(error, 256, "nonfinite GPU skin input");
        return 0;
      }
    if (!skin_coordinate_valid((float *)p->matrices.mapped + w->bone * 16,
                               w->position)) {
      snprintf(error, 256, "invalid GPU skin homogeneous coordinate at bone%u",
               w->bone);
      return 0;
    }
  }
  if (!wait_frame(r, error))
    return 0;
  Buffer source = {0}, data = {0};
  VkDescriptorSet descriptor = VK_NULL_HANDLE;
  size_t vb = (size_t)m->vertex_count * sizeof(BkLitVertex);
  size_t header = ((size_t)m->vertex_count + 2) * 4,
         wb = (size_t)count * sizeof(*weights);
  _Static_assert(sizeof(BkGpuSkinWeight) == 36, "packed skin operations");
  if (!buffer_create(r, &source, vb, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     error) ||
      !buffer_create(r, &data, header + wb, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     error))
    goto fail;
  memcpy(source.mapped, m->vertices.mapped, vb);
  memcpy(data.mapped, &m->vertex_count, 4);
  memcpy((uint8_t *)data.mapped + 4, offsets, header - 4);
  memcpy((uint8_t *)data.mapped + header, weights, wb);
  VK_TRY(buffer_flush(r, &source, 0));
  VK_TRY(buffer_flush(r, &data, 0));
  VkDescriptorSetAllocateInfo alloc = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = r->descriptors,
      .descriptorSetCount = 1,
      .pSetLayouts = &r->skin_descriptor_layout};
  VK_TRY(vkAllocateDescriptorSets(r->device, &alloc, &descriptor));
  VkDescriptorBufferInfo buffers[4] = {
      {m->vertices.handle, 0, vb},
      {source.handle, 0, vb},
      {data.handle, 0, header + wb},
      {p->matrices.handle, 0, (size_t)p->count * 64}};
  VkWriteDescriptorSet writes[4];
  for (unsigned i = 0; i < 4; i++)
    writes[i] = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor,
        .dstBinding = i,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = buffers + i};
  vkUpdateDescriptorSets(r->device, 4, writes, 0, NULL);
  m->skin_source = source;
  m->skin_weights = data;
  m->skin_descriptor = descriptor;
  m->palette = p;
  p->references++;
  m->next_palette = p->meshes;
  p->meshes = m;
  for (unsigned i = 0; i < count; i++)
    p->used[weights[i].bone] = 1;
  skin_queue(r, m);
  return 1;
fail:
  if (descriptor)
    vkFreeDescriptorSets(r->device, r->descriptors, 1, &descriptor);
  buffer_destroy(r, &source);
  buffer_destroy(r, &data);
  return 0;
}
static void skin_mesh_release(BkRenderer *r, BkGpuMesh *m) {
  if (m->skin_pending) {
    BkGpuMesh **link = &r->skin_updates;
    while (*link && *link != m)
      link = &(*link)->next_skin;
    if (*link)
      *link = m->next_skin;
  }
  if (m->palette) {
    BkGpuMesh **link = &m->palette->meshes;
    while (*link && *link != m)
      link = &(*link)->next_palette;
    if (*link)
      *link = m->next_palette;
    bk_skin_palette_destroy(r, m->palette);
  }
  if (m->skin_descriptor)
    vkFreeDescriptorSets(r->device, r->descriptors, 1, &m->skin_descriptor);
  buffer_destroy(r, &m->skin_source);
  buffer_destroy(r, &m->skin_weights);
}
static void record_skin_updates(BkRenderer *r) {
  if (!r->skin_updates)
    return;
  VkMemoryBarrier access = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT |
                       VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
                       VK_ACCESS_SHADER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
  vkCmdPipelineBarrier(
      r->command,
      VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &access, 0, NULL, 0, NULL);
  vkCmdBindPipeline(r->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                    r->skin_pipeline);
  while (r->skin_updates) {
    BkGpuMesh *m = r->skin_updates;
    vkCmdBindDescriptorSets(r->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                            r->skin_layout, 0, 1, &m->skin_descriptor, 0, NULL);
    vkCmdDispatch(r->command, (m->vertex_count + 63) / 64, 1, 1);
    r->stats.skin_dispatches++;
    r->stats.skin_vertices += m->vertex_count;
    r->skin_updates = m->next_skin;
    m->next_skin = NULL;
    m->skin_pending = 0;
    m->skin_replay = 0;
  }
  access.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  access.dstAccessMask =
      VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_HOST_READ_BIT;
  vkCmdPipelineBarrier(r->command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
                           VK_PIPELINE_STAGE_HOST_BIT,
                       0, 1, &access, 0, NULL, 0, NULL);
}
int bk_lit_mesh_readback(BkRenderer *r, BkGpuMesh *m, BkLitVertex *out,
                         unsigned count, char error[256]) {
  if (!r || !m || m->owner != r || !m->lit || r->active ||
      (m->skin_pending && !m->skin_replay) || !out ||
      count != m->vertex_count) {
    snprintf(error, 256, "invalid mesh readback state/count");
    return 0;
  }
  if (!wait_frame(r, error))
    return 0;
  VK_TRY(buffer_flush(r, &m->vertices, 1));
  memcpy(out, m->vertices.mapped, (size_t)count * sizeof(*out));
  return 1;
fail:
  return 0;
}
#endif
