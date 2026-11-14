#include <jni.h>
#include <vulkan/vulkan.h>
#include <android/log.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#define LOG_TAG "LibreCutsVulkan"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct Runtime {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    uint32_t queueFamily = UINT32_MAX;
    VkPhysicalDeviceProperties properties{};
};

static void destroyRuntime(Runtime* runtime) {
    if (!runtime) return;
    if (runtime->device != VK_NULL_HANDLE) vkDestroyDevice(runtime->device, nullptr);
    if (runtime->instance != VK_NULL_HANDLE) vkDestroyInstance(runtime->instance, nullptr);
    delete runtime;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_tharunbirla_librecuts_shaders_VulkanShaderRuntime_nativeCreate(JNIEnv*, jobject) {
    auto* runtime = new Runtime();
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "LibreCuts";
    app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app.pEngineName = "LibreCuts Effects";
    app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &app;
    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &runtime->instance);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateInstance failed: %d", result);
        destroyRuntime(runtime);
        return 0;
    }

    uint32_t physicalCount = 0;
    result = vkEnumeratePhysicalDevices(runtime->instance, &physicalCount, nullptr);
    if (result != VK_SUCCESS || physicalCount == 0) {
        LOGE("No Vulkan physical devices found (%d)", result);
        destroyRuntime(runtime);
        return 0;
    }
    std::vector<VkPhysicalDevice> devices(physicalCount);
    result = vkEnumeratePhysicalDevices(runtime->instance, &physicalCount, devices.data());
    if (result != VK_SUCCESS) {
        destroyRuntime(runtime);
        return 0;
    }

    for (VkPhysicalDevice candidate : devices) {
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());
        for (uint32_t i = 0; i < familyCount; ++i) {
            if ((families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 && families[i].queueCount > 0) {
                runtime->physical = candidate;
                runtime->queueFamily = i;
                break;
            }
        }
        if (runtime->physical != VK_NULL_HANDLE) break;
    }
    if (runtime->physical == VK_NULL_HANDLE) {
        LOGE("No Vulkan compute queue available");
        destroyRuntime(runtime);
        return 0;
    }

    vkGetPhysicalDeviceProperties(runtime->physical, &runtime->properties);
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = runtime->queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkPhysicalDeviceFeatures supported{};
    vkGetPhysicalDeviceFeatures(runtime->physical, &supported);
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.pEnabledFeatures = &supported;
    result = vkCreateDevice(runtime->physical, &deviceInfo, nullptr, &runtime->device);
    if (result != VK_SUCCESS) {
        LOGE("vkCreateDevice failed: %d", result);
        destroyRuntime(runtime);
        return 0;
    }
    vkGetDeviceQueue(runtime->device, runtime->queueFamily, 0, &runtime->computeQueue);
    return reinterpret_cast<jlong>(runtime);
}

extern "C" JNIEXPORT void JNICALL
Java_com_tharunbirla_librecuts_shaders_VulkanShaderRuntime_nativeDestroy(JNIEnv*, jobject, jlong handle) {
    destroyRuntime(reinterpret_cast<Runtime*>(handle));
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_tharunbirla_librecuts_shaders_VulkanShaderRuntime_nativeDeviceName(JNIEnv* env, jobject, jlong handle) {
    auto* runtime = reinterpret_cast<Runtime*>(handle);
    if (!runtime) return env->NewStringUTF("");
    return env->NewStringUTF(runtime->properties.deviceName);
}

extern "C" JNIEXPORT jint JNICALL
Java_com_tharunbirla_librecuts_shaders_VulkanShaderRuntime_nativeApiVersion(JNIEnv*, jobject, jlong handle) {
    auto* runtime = reinterpret_cast<Runtime*>(handle);
    return runtime ? static_cast<jint>(runtime->properties.apiVersion) : 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_tharunbirla_librecuts_shaders_VulkanShaderRuntime_nativeDeviceType(JNIEnv*, jobject, jlong handle) {
    auto* runtime = reinterpret_cast<Runtime*>(handle);
    return runtime ? static_cast<jint>(runtime->properties.deviceType) : -1;
}

static bool createHostBuffer(Runtime* runtime, VkDeviceSize size, VkBufferUsageFlags usage,
                             VkBuffer* buffer, VkDeviceMemory* memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(runtime->device, &bufferInfo, nullptr, buffer) != VK_SUCCESS) return false;

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(runtime->device, *buffer, &requirements);
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(runtime->physical, &memoryProperties);
    uint32_t memoryType = UINT32_MAX;
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        const VkMemoryPropertyFlags flags = memoryProperties.memoryTypes[i].propertyFlags;
        if ((requirements.memoryTypeBits & (1u << i)) != 0 &&
            (flags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            memoryType = i;
            break;
        }
    }
    if (memoryType == UINT32_MAX) return false;

    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    if (vkAllocateMemory(runtime->device, &allocation, nullptr, memory) != VK_SUCCESS) return false;
    return vkBindBufferMemory(runtime->device, *buffer, *memory, 0) == VK_SUCCESS;
}

static void destroyBuffer(Runtime* runtime, VkBuffer buffer, VkDeviceMemory memory) {
    if (buffer != VK_NULL_HANDLE) vkDestroyBuffer(runtime->device, buffer, nullptr);
    if (memory != VK_NULL_HANDLE) vkFreeMemory(runtime->device, memory, nullptr);
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_tharunbirla_librecuts_shaders_VulkanShaderRuntime_nativeRenderRgba(
    JNIEnv* env, jobject, jlong handle, jbyteArray shaderArray, jintArray inputArray, jint width, jint height) {
    auto* runtime = reinterpret_cast<Runtime*>(handle);
    if (!runtime || !shaderArray || !inputArray || width <= 0 || height <= 0) return nullptr;
    const uint64_t pixelCount64 = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
    if (pixelCount64 > 16ull * 1024ull * 1024ull) return nullptr;
    const jsize shaderLength = env->GetArrayLength(shaderArray);
    const jsize inputLength = env->GetArrayLength(inputArray);
    if (shaderLength < 20 || (shaderLength % 4) != 0 || inputLength != static_cast<jsize>(pixelCount64)) return nullptr;

    std::vector<uint32_t> shaderWords(static_cast<size_t>(shaderLength) / 4);
    env->GetByteArrayRegion(shaderArray, 0, shaderLength, reinterpret_cast<jbyte*>(shaderWords.data()));
    if (shaderWords.empty() || shaderWords[0] != 0x07230203u) return nullptr;
    std::vector<jint> input(static_cast<size_t>(inputLength));
    env->GetIntArrayRegion(inputArray, 0, inputLength, input.data());
    if (env->ExceptionCheck()) return nullptr;

    const VkDeviceSize bufferSize = pixelCount64 * sizeof(jint);
    VkBuffer inputBuffer = VK_NULL_HANDLE, outputBuffer = VK_NULL_HANDLE;
    VkDeviceMemory inputMemory = VK_NULL_HANDLE, outputMemory = VK_NULL_HANDLE;
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    bool success = false;
    std::vector<jint> output(static_cast<size_t>(inputLength));

    do {
        if (!createHostBuffer(runtime, bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              &inputBuffer, &inputMemory) ||
            !createHostBuffer(runtime, bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              &outputBuffer, &outputMemory)) break;
        void* mapped = nullptr;
        if (vkMapMemory(runtime->device, inputMemory, 0, bufferSize, 0, &mapped) != VK_SUCCESS) break;
        std::memcpy(mapped, input.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(runtime->device, inputMemory);

        VkShaderModuleCreateInfo shaderInfo{};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = static_cast<size_t>(shaderLength);
        shaderInfo.pCode = shaderWords.data();
        if (vkCreateShaderModule(runtime->device, &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS) break;

        VkDescriptorSetLayoutBinding bindings[2]{};
        for (uint32_t i = 0; i < 2; ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        VkDescriptorSetLayoutCreateInfo descriptorInfo{};
        descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorInfo.bindingCount = 2;
        descriptorInfo.pBindings = bindings;
        if (vkCreateDescriptorSetLayout(runtime->device, &descriptorInfo, nullptr, &descriptorLayout) != VK_SUCCESS) break;

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = 2 * sizeof(uint32_t); // width, height
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &descriptorLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;
        if (vkCreatePipelineLayout(runtime->device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) break;

        VkPipelineShaderStageCreateInfo stage{};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = shaderModule;
        stage.pName = "main";
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = stage;
        pipelineInfo.layout = pipelineLayout;
        if (vkCreateComputePipelines(runtime->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) break;

        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        if (vkCreateDescriptorPool(runtime->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) break;
        VkDescriptorSetAllocateInfo setInfo{};
        setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setInfo.descriptorPool = descriptorPool;
        setInfo.descriptorSetCount = 1;
        setInfo.pSetLayouts = &descriptorLayout;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        if (vkAllocateDescriptorSets(runtime->device, &setInfo, &descriptorSet) != VK_SUCCESS) break;
        VkDescriptorBufferInfo bufferInfos[2]{};
        bufferInfos[0] = {inputBuffer, 0, bufferSize};
        bufferInfos[1] = {outputBuffer, 0, bufferSize};
        VkWriteDescriptorSet writes[2]{};
        for (uint32_t i = 0; i < 2; ++i) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = descriptorSet;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].pBufferInfo = &bufferInfos[i];
        }
        vkUpdateDescriptorSets(runtime->device, 2, writes, 0, nullptr);

        VkCommandPoolCreateInfo commandPoolInfo{};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.queueFamilyIndex = runtime->queueFamily;
        if (vkCreateCommandPool(runtime->device, &commandPoolInfo, nullptr, &commandPool) != VK_SUCCESS) break;
        VkCommandBufferAllocateInfo commandAlloc{};
        commandAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandAlloc.commandPool = commandPool;
        commandAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandAlloc.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(runtime->device, &commandAlloc, &commandBuffer) != VK_SUCCESS) break;
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) break;
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        const uint32_t dimensions[2] = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(dimensions), dimensions);
        vkCmdDispatch(commandBuffer, (static_cast<uint32_t>(width) + 7u) / 8u,
                      (static_cast<uint32_t>(height) + 7u) / 8u, 1);
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) break;
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffer;
        if (vkQueueSubmit(runtime->computeQueue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS) break;
        if (vkQueueWaitIdle(runtime->computeQueue) != VK_SUCCESS) break;
        if (vkMapMemory(runtime->device, outputMemory, 0, bufferSize, 0, &mapped) != VK_SUCCESS) break;
        std::memcpy(output.data(), mapped, static_cast<size_t>(bufferSize));
        vkUnmapMemory(runtime->device, outputMemory);
        success = true;
    } while (false);

    if (runtime->device != VK_NULL_HANDLE) vkDeviceWaitIdle(runtime->device);
    if (commandPool != VK_NULL_HANDLE) vkDestroyCommandPool(runtime->device, commandPool, nullptr);
    if (descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(runtime->device, descriptorPool, nullptr);
    if (pipeline != VK_NULL_HANDLE) vkDestroyPipeline(runtime->device, pipeline, nullptr);
    if (pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(runtime->device, pipelineLayout, nullptr);
    if (descriptorLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(runtime->device, descriptorLayout, nullptr);
    if (shaderModule != VK_NULL_HANDLE) vkDestroyShaderModule(runtime->device, shaderModule, nullptr);
    destroyBuffer(runtime, inputBuffer, inputMemory);
    destroyBuffer(runtime, outputBuffer, outputMemory);
    if (!success) return nullptr;
    jintArray result = env->NewIntArray(inputLength);
    if (result) env->SetIntArrayRegion(result, 0, inputLength, output.data());
    return result;
}
