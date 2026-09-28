/* Private implementation, after Buffer helpers and before renderer creation. */
#ifndef BK_VULKAN_TRANSFER_INTERNAL_H
#define BK_VULKAN_TRANSFER_INTERNAL_H
struct BkVertexTransfer {
  BkRenderer *owner;
  BkGpuMesh *source, *target;
  Buffer pairs;
  VkDescriptorSet descriptor;
  struct {
    float world[16];
    uint32_t count, serial;
  } parameters;
};
static int transfer_pipeline_create(BkRenderer *r, char error[256]) {
  if (r->transfer_pipeline)
    return 1;
  VkShaderModule module = VK_NULL_HANDLE;
  VkDescriptorSetLayoutBinding b[3];
  for (unsigned i = 0; i < 3; i++)
    b[i] = (VkDescriptorSetLayoutBinding){
        .binding = i,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT};
  VkDescriptorSetLayoutCreateInfo d = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 3,
      .pBindings = b};
  VK_TRY(vkCreateDescriptorSetLayout(r->device, &d, NULL,
                                     &r->transfer_descriptor_layout));
  VkPushConstantRange push = {VK_SHADER_STAGE_COMPUTE_BIT, 0, 72};
  VkPipelineLayoutCreateInfo layout = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &r->transfer_descriptor_layout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push};
  VK_TRY(vkCreatePipelineLayout(r->device, &layout, NULL, &r->transfer_layout));
  VkShaderModuleCreateInfo sm = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = sizeof(bk_transfer_comp_spv),
      .pCode = bk_transfer_comp_spv};
  VK_TRY(vkCreateShaderModule(r->device, &sm, NULL, &module));
  VkComputePipelineCreateInfo cp = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = module,
                .pName = "main"},
      .layout = r->transfer_layout};
  VK_TRY(vkCreateComputePipelines(r->device, VK_NULL_HANDLE, 1, &cp, NULL,
                                  &r->transfer_pipeline));
  vkDestroyShaderModule(r->device, module, NULL);
  return 1;
fail:
  if (module)
    vkDestroyShaderModule(r->device, module, NULL);
  if (r->transfer_pipeline)
    vkDestroyPipeline(r->device, r->transfer_pipeline, NULL);
  r->transfer_pipeline = VK_NULL_HANDLE;
  if (r->transfer_layout)
    vkDestroyPipelineLayout(r->device, r->transfer_layout, NULL);
  if (r->transfer_descriptor_layout)
    vkDestroyDescriptorSetLayout(r->device, r->transfer_descriptor_layout,
                                 NULL);
  r->transfer_layout = VK_NULL_HANDLE;
  r->transfer_descriptor_layout = VK_NULL_HANDLE;
  return 0;
}
BkVertexTransfer *bk_vertex_transfer_create(BkRenderer *r, BkGpuMesh *source,
                                            BkGpuMesh *target,
                                            const BkVertexPair *pairs,
                                            unsigned count, char error[256]) {
  if (!r || r->active || !source || !target || source->owner != r ||
      target->owner != r || !source->lit || !target->lit || !pairs || !count ||
      count > 2000000) {
    snprintf(error, 256, "invalid vertex transfer binding");
    return NULL;
  }
  for (unsigned i = 0; i < count; i++)
    if (pairs[i].source >= source->vertex_count ||
        pairs[i].target >= target->vertex_count) {
      snprintf(error, 256, "vertex transfer index outside mesh");
      return NULL;
    }
  BkVertexTransfer *t = calloc(1, sizeof(*t));
  uint32_t *last = NULL;
  if (!t) {
    snprintf(error, 256, "vertex transfer allocation failed");
    return NULL;
  }
  t->owner = r;
  unsigned used = count;
  if (source != target) {
    last = malloc((size_t)target->vertex_count * sizeof(*last));
    if (!last) {
      snprintf(error, 256, "vertex transfer dedup allocation failed");
      goto fail;
    }
    for (unsigned i = 0; i < target->vertex_count; i++)
      last[i] = UINT32_MAX;
    for (unsigned i = 0; i < count; i++)
      last[pairs[i].target] = i;
    used = 0;
    for (unsigned i = 0; i < count; i++)
      used += last[pairs[i].target] == i;
  }
  if (!transfer_pipeline_create(r, error) ||
      !buffer_create(r, &t->pairs, (size_t)used * sizeof(*pairs),
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, error))
    goto fail;
  unsigned at = 0;
  for (unsigned i = 0; i < count; i++)
    if (!last || last[pairs[i].target] == i)
      ((BkVertexPair *)t->pairs.mapped)[at++] = pairs[i];
  VK_TRY(buffer_flush(r, &t->pairs, 0));
  VkDescriptorSetAllocateInfo alloc = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = r->descriptors,
      .descriptorSetCount = 1,
      .pSetLayouts = &r->transfer_descriptor_layout};
  VK_TRY(vkAllocateDescriptorSets(r->device, &alloc, &t->descriptor));
  VkDescriptorBufferInfo buffers[3] = {
      {target->vertices.handle, 0,
       (size_t)target->vertex_count * sizeof(BkLitVertex)},
      {source->vertices.handle, 0,
       (size_t)source->vertex_count * sizeof(BkLitVertex)},
      {t->pairs.handle, 0, (size_t)used * sizeof(*pairs)}};
  VkWriteDescriptorSet writes[3];
  for (unsigned i = 0; i < 3; i++)
    writes[i] = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = t->descriptor,
        .dstBinding = i,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = buffers + i};
  vkUpdateDescriptorSets(r->device, 3, writes, 0, NULL);
  t->source = source;
  t->target = target;
  source->references++;
  target->references++;
  memcpy(t->parameters.world, bk_identity, 64);
  t->parameters.count = used;
  t->parameters.serial = source == target;
  free(last);
  return t;
fail:
  free(last);
  bk_vertex_transfer_destroy(t);
  return NULL;
}
int bk_vertex_transfer_world(BkVertexTransfer *t, const float world[16],
                             char error[256]) {
  if (!t || t->owner->active || !world) {
    snprintf(error, 256, "invalid vertex transfer update");
    return 0;
  }
  for (unsigned i = 0; i < 16; i++)
    if (!isfinite(world[i])) {
      snprintf(error, 256, "nonfinite vertex transfer world");
      return 0;
    }
  /* Constant zero W is never defined; other projective horizons are a caller
   * precondition, as data may already reside solely on the GPU. */
  if (world[3] == 0 && world[7] == 0 && world[11] == 0 && world[15] == 0) {
    snprintf(error, 256, "zero homogeneous transfer W");
    return 0;
  }
  memcpy(t->parameters.world, world, 64);
  /* A prior callback overwrote the skinned output. Even an unchanged palette
   * must regenerate it before this frame's callbacks (which may be disabled).
   * This schedules compute only; no source/palette upload or CPU skinning. */
  if (t->target->palette)
    skin_queue(t->owner, t->target);
  return 1;
}
void bk_vertex_transfer_destroy(BkVertexTransfer *t) {
  if (!t)
    return;
  BkRenderer *r = t->owner;
  if (r->submitted) {
    vkDeviceWaitIdle(r->device);
    r->submitted = 0;
  }
  if (t->descriptor)
    vkFreeDescriptorSets(r->device, r->descriptors, 1, &t->descriptor);
  buffer_destroy(r, &t->pairs);
  /* A surviving actor must not keep the final callback's skinned output. */
  if (t->target && t->target->palette)
    skin_queue(r, t->target);
  bk_mesh_destroy(r, t->source);
  bk_mesh_destroy(r, t->target);
  free(t);
}
int bk_vertex_transfers_apply(BkRenderer *r, BkVertexTransfer *const *transfers,
                              unsigned count, char error[256]) {
  if (!r || !r->active || count > 64 || (count && !transfers)) {
    snprintf(error, 256, "invalid active vertex transfer batch");
    return 0;
  }
  for (unsigned i = 0; i < count; i++)
    if (!transfers[i] || transfers[i]->owner != r) {
      snprintf(error, 256, "foreign/missing vertex transfer");
      return 0;
    }
  if (!count)
    return 1;
  vkCmdEndRenderPass(r->command);
  VkMemoryBarrier access = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                       VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT};
  vkCmdPipelineBarrier(
      r->command,
      VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
          VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &access, 0, NULL, 0, NULL);
  vkCmdBindPipeline(r->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                    r->transfer_pipeline);
  for (unsigned i = 0; i < count; i++) {
    BkVertexTransfer *t = transfers[i];
    if (i) {
      access.srcAccessMask =
          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(r->command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &access,
                           0, NULL, 0, NULL);
    }
    vkCmdBindDescriptorSets(r->command, VK_PIPELINE_BIND_POINT_COMPUTE,
                            r->transfer_layout, 0, 1, &t->descriptor, 0, NULL);
    vkCmdPushConstants(r->command, r->transfer_layout,
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(t->parameters),
                       &t->parameters);
    vkCmdDispatch(r->command,
                  t->parameters.serial ? 1 : (t->parameters.count + 63) / 64, 1,
                  1);
    /* Draw-only presentations can reuse prepared actors without world edits.
     * Restore modified skinned outputs at the next begin, never between
     * repeated callbacks within this frame. */
    if (t->target->palette) {
      skin_queue(r, t->target);
      /* Current output is complete; only its next presentation is pending. */
      t->target->skin_replay = 1;
    }
  }
  access.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  access.dstAccessMask =
      VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_HOST_READ_BIT;
  vkCmdPipelineBarrier(r->command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
                           VK_PIPELINE_STAGE_HOST_BIT,
                       0, 1, &access, 0, NULL, 0, NULL);
  VkRenderPassBeginInfo pass = {.sType =
                                    VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                .renderPass = r->resume_pass,
                                .framebuffer = r->frames[r->index],
                                .renderArea = {{0, 0}, {r->width, r->height}}};
  vkCmdBeginRenderPass(r->command, &pass, VK_SUBPASS_CONTENTS_INLINE);
  return bk_renderer_viewport(r, &r->viewport, error);
}
#endif
