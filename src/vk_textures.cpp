#include "vk_textures.hpp"

#include "assetlib/asset_loader.hpp"
#include "assetlib/texture_asset.hpp"
#include "vk_initializers.hpp"
#include <iostream>

#include "stb_image_implementation.hpp"

auto vkutil::load_image_from_file(VulkanEngine &engine,
                                  const std::filesystem::path &file,
                                  AllocatedImage &outImage) -> bool {
  int texWidth, texHeight, texChannels;
  stbi_uc *pixels = stbi_load(file.string().c_str(), &texWidth, &texHeight,
                              &texChannels, STBI_rgb_alpha);
  if (!static_cast<bool>(pixels)) {
    std::cout << "Failed to load texture file " << file << '\n';
    return false;
  }
  void *pixel_ptr = pixels;
  VkDeviceSize imageSize = texWidth * texHeight * 4;

  // The format R8G8B8A8 matches exactly with the pixels loaded from stb_image
  // lib
  VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;

  // Allocate temporary buffer for holding texture data to upload
  AllocatedBuffer stagingBuffer = engine.create_buffer(
      imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

  // Copy data to buffer
  void *data;
  vmaMapMemory(engine._allocator, stagingBuffer._allocation, &data);
  memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));
  vmaUnmapMemory(engine._allocator, stagingBuffer._allocation);

  // We no longer need the loaded data, so we can free the pixels as they are
  // now in the staging buffer
  stbi_image_free(pixels);

  VkExtent3D imageExtent;
  imageExtent.width = static_cast<std::uint32_t>(texWidth);
  imageExtent.height = static_cast<std::uint32_t>(texHeight);
  imageExtent.depth = 1;

  VkImageCreateInfo dimg_info = vkinit::image_create_info(
      image_format,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      imageExtent);

  AllocatedImage newImage;

  VmaAllocationCreateInfo dimg_allocinfo = {};
  dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  // Allocate and create the image
  vmaCreateImage(engine._allocator, &dimg_info, &dimg_allocinfo,
                 &newImage._image, &newImage._allocation, nullptr);

  engine.immediate_submit([&](VkCommandBuffer cmd) {
    VkImageSubresourceRange range;
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    VkImageMemoryBarrier ib = {};
    ib.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    ib.pNext = nullptr;

    ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ib.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    ib.image = newImage._image;
    ib.subresourceRange = range;
    ib.srcAccessMask = 0;
    ib.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    // Barrier the image into the transfer-receive layout
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &ib);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = imageExtent;

    // Copy the buffer into the image
    vkCmdCopyBufferToImage(cmd, stagingBuffer._buffer, newImage._image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copyRegion);

    VkImageMemoryBarrier ibtr = ib;
    ibtr.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    ibtr.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ibtr.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    ibtr.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    // Barrier the image into the shader readable layout
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &ibtr);
  });

  engine._mainDeletionQueue.push_function([=]() {
    vmaDestroyImage(engine._allocator, newImage._image, newImage._allocation);
  });

  vmaDestroyBuffer(engine._allocator, stagingBuffer._buffer,
                   stagingBuffer._allocation);

  std::cout << "Texture loaded successfully " << file << '\n';

  outImage = newImage;

  return true;
}

auto vkutil::load_image_from_asset(VulkanEngine &engine,
                                   const std::filesystem::path &filename,
                                   AllocatedImage &outImage) -> bool {
  assets::AssetFile file;
  bool loaded = assets::load_binaryfile(filename, file);

  if (!loaded) {
    std::cout << "Error when loading image\n";
    return false;
  }

  assets::TextureInfo textureInfo = assets::read_texture_info(&file);

  VkDeviceSize imageSize = textureInfo.textureSize;
  VkFormat image_format;
  switch (textureInfo.textureFormat) {
  case assets::TextureFormat::RGBA8:
    image_format = VK_FORMAT_R8G8B8A8_SRGB;
    break;
  default:
    return false;
  }

  AllocatedBuffer stagingBuffer = engine.create_buffer(
      imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

  std::vector<char> buffer{};
  buffer.reserve(textureInfo.textureSize);

  void *data;
  vmaMapMemory(engine._allocator, stagingBuffer._allocation, &data);
  assets::unpack_texture(&textureInfo, file.binaryBlob.data(),
                         file.binaryBlob.size(), buffer.data());
  memcpy(data, buffer.data(), textureInfo.textureSize);

  vmaUnmapMemory(engine._allocator, stagingBuffer._allocation);

  outImage = upload_image(textureInfo.pixelsize[0], textureInfo.pixelsize[1],
                          image_format, engine, stagingBuffer);
  vmaDestroyBuffer(engine._allocator, stagingBuffer._buffer,
                   stagingBuffer._allocation);

  return true;
}

auto vkutil::upload_image(int texWidth, int texHeight, VkFormat image_format,
                          VulkanEngine &engine, AllocatedBuffer &stagingBuffer)
    -> AllocatedImage {
  VkExtent3D imageExtent;
  imageExtent.width = static_cast<uint32_t>(texWidth);
  imageExtent.height = static_cast<uint32_t>(texHeight);
  imageExtent.depth = 1;

  VkImageCreateInfo dimg_info = vkinit::image_create_info(
      image_format,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      imageExtent);

  AllocatedImage newImage;

  VmaAllocationCreateInfo dimg_allocinfo = {};
  dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

  // allocate and create the image
  vmaCreateImage(engine._allocator, &dimg_info, &dimg_allocinfo,
                 &newImage._image, &newImage._allocation, nullptr);

  // transition image to transfer-receiver
  engine.immediate_submit([&](VkCommandBuffer cmd) {
    VkImageSubresourceRange range;
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    VkImageMemoryBarrier imageBarrier_toTransfer = {};
    imageBarrier_toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;

    imageBarrier_toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageBarrier_toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier_toTransfer.image = newImage._image;
    imageBarrier_toTransfer.subresourceRange = range;

    imageBarrier_toTransfer.srcAccessMask = 0;
    imageBarrier_toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    // barrier the image into the transfer-receive layout
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &imageBarrier_toTransfer);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = imageExtent;

    // copy the buffer into the image
    vkCmdCopyBufferToImage(cmd, stagingBuffer._buffer, newImage._image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copyRegion);

    VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;

    imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier_toReadable.newLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    // barrier the image into the shader readable layout
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &imageBarrier_toReadable);
  });

  // build a default imageview
  VkImageViewCreateInfo view_info = vkinit::imageview_create_info(
      image_format, newImage._image, VK_IMAGE_ASPECT_COLOR_BIT);

  vkCreateImageView(engine._device, &view_info, nullptr,
                    &newImage._defaultView);

  engine._mainDeletionQueue.push_function([=, &engine]() {
    vkDestroyImageView(engine._device, newImage._defaultView, nullptr);
    vmaDestroyImage(engine._allocator, newImage._image, newImage._allocation);
  });

  newImage.mipLevels = 1; // mips.size();
  return newImage;
}
