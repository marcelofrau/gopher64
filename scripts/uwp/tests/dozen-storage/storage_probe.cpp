#include <volk.h>
#include <cstdio>
#include <cwchar>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <fstream>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define CHECK(expr) do { VkResult r = (expr); if (r != VK_SUCCESS) { \
    std::fprintf(stderr, "%s failed: %d\n", #expr, int(r)); return 1; } } while (0)

static bool module_matches(const wchar_t* name, const wchar_t* variable) {
    wchar_t expected[32768]{};
    wchar_t actual[32768]{};
    HMODULE module = GetModuleHandleW(name);
    DWORD e = GetEnvironmentVariableW(variable, expected, 32768);
    DWORD a = module ? GetModuleFileNameW(module, actual, 32768) : 0;
    if (!e || e >= 32768 || !a || a >= 32768 || _wcsicmp(expected, actual) != 0) {
        std::fwprintf(stderr, L"module mismatch: %ls expected=%ls actual=%ls\n", name, expected, actual);
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3 || (argc == 3 && std::strcmp(argv[2], "--alias") != 0)) {
        std::fprintf(stderr, "usage: storage_probe shader.spv [--alias]\n"); return 1;
    }
    const bool alias = argc == 3;
    CHECK(volkInitialize());
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "storage probe";
    app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    VkInstance instance;
    CHECK(vkCreateInstance(&ci, nullptr, &instance));
    volkLoadInstance(instance);
    uint32_t count = 0;
    CHECK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
    if (!count) { std::fprintf(stderr, "no Vulkan GPU\n"); return 1; }
    std::vector<VkPhysicalDevice> devices(count);
    CHECK(vkEnumeratePhysicalDevices(instance, &count, devices.data()));
    if (!module_matches(L"vulkan_dzn.dll", L"G64_TEST_DOZEN_PATH")) return 1;
    VkPhysicalDevice gpu = devices[0];
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(gpu, &props);
    std::printf("GPU: %s\n", props.deviceName);
    VkPhysicalDevice16BitStorageFeatures narrow16{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES};
    VkPhysicalDevice8BitStorageFeatures narrow8{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES};
    VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features.pNext = &narrow16;
    narrow16.pNext = &narrow8;
    vkGetPhysicalDeviceFeatures2(gpu, &features);
    std::printf("storage16=%u storage8=%u shaderInt16=%u\n",
                narrow16.storageBuffer16BitAccess, narrow8.storageBuffer8BitAccess, features.features.shaderInt16);
    if (!narrow16.storageBuffer16BitAccess || !narrow8.storageBuffer8BitAccess) return 1;

    uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &family_count, families.data());
    uint32_t family = 0;
    while (family < family_count && !(families[family].queueFlags & VK_QUEUE_COMPUTE_BIT)) ++family;
    if (family == family_count) { std::fprintf(stderr, "no compute queue\n"); return 1; }
    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    VkPhysicalDevice16BitStorageFeatures enable16{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES};
    VkPhysicalDevice8BitStorageFeatures enable8{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES};
    enable16.storageBuffer16BitAccess = VK_TRUE;
    enable8.storageBuffer8BitAccess = VK_TRUE;
    enable16.pNext = &enable8;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.pNext = &enable16;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    VkDevice device;
    CHECK(vkCreateDevice(gpu, &dci, nullptr, &device));
    volkLoadDevice(device);
    VkPhysicalDeviceMemoryProperties memprops{};
    vkGetPhysicalDeviceMemoryProperties(gpu, &memprops);
    struct Buffer { VkBuffer buffer; VkDeviceMemory memory; void* mapped; } buffers[3]{};
    for (auto& b : buffers) {
        VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bci.size = 1024;
        bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        CHECK(vkCreateBuffer(device, &bci, nullptr, &b.buffer));
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, b.buffer, &req);
        uint32_t type = 0;
        const auto wanted = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        while (type < memprops.memoryTypeCount &&
               (!(req.memoryTypeBits & (1u << type)) || (memprops.memoryTypes[type].propertyFlags & wanted) != wanted)) ++type;
        if (type == memprops.memoryTypeCount) { std::fprintf(stderr, "no coherent host memory\n"); return 1; }
        VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        mai.allocationSize = req.size;
        mai.memoryTypeIndex = type;
        CHECK(vkAllocateMemory(device, &mai, nullptr, &b.memory));
        CHECK(vkBindBufferMemory(device, b.buffer, b.memory, 0));
        CHECK(vkMapMemory(device, b.memory, 0, VK_WHOLE_SIZE, 0, &b.mapped));
        std::memset(b.mapped, 0xa5, 1024);
    }
    VkDescriptorSetLayoutBinding bindings[3]{};
    for (uint32_t i = 0; i < 3; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dlci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dlci.bindingCount = 3;
    dlci.pBindings = bindings;
    VkDescriptorSetLayout layout;
    CHECK(vkCreateDescriptorSetLayout(device, &dlci, nullptr, &layout));
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &layout;
    VkPipelineLayout pipeline_layout;
    CHECK(vkCreatePipelineLayout(device, &plci, nullptr, &pipeline_layout));
    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3};
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &ps;
    VkDescriptorPool pool;
    CHECK(vkCreateDescriptorPool(device, &dpci, nullptr, &pool));
    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = pool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &layout;
    VkDescriptorSet set;
    CHECK(vkAllocateDescriptorSets(device, &dsai, &set));
    VkDescriptorBufferInfo infos[3]{};
    VkWriteDescriptorSet writes[3]{};
    for (uint32_t i = 0; i < 3; ++i) {
        infos[i] = {buffers[alias && i == 1 ? 0 : i].buffer, 0, 1024};
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &infos[i];
    }
    vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);
    std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
    if (!input) { std::fprintf(stderr, "shader file unavailable\n"); return 1; }
    const auto size = input.tellg();
    if (size <= 0 || size % 4 != 0) { std::fprintf(stderr, "invalid shader size\n"); return 1; }
    std::vector<uint32_t> code(static_cast<size_t>(size) / 4);
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(code.data()), size)) { std::fprintf(stderr, "shader read failed\n"); return 1; }
    VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = static_cast<size_t>(size);
    smci.pCode = code.data();
    VkShaderModule shader;
    CHECK(vkCreateShaderModule(device, &smci, nullptr, &shader));
    VkComputePipelineCreateInfo cpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpci.stage.module = shader;
    cpci.stage.pName = "main";
    cpci.layout = pipeline_layout;
    VkPipeline pipeline;
    CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipeline));
    if (!module_matches(L"dxil.dll", L"G64_TEST_DXIL_PATH")) return 1;
    std::puts("compute pipeline created");
    VkQueue queue;
    vkGetDeviceQueue(device, family, 0, &queue);
    VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pci.queueFamilyIndex = family;
    VkCommandPool cmdpool;
    CHECK(vkCreateCommandPool(device, &pci, nullptr, &cmdpool));
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = cmdpool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer command;
    CHECK(vkAllocateCommandBuffers(device, &cbai, &command));
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CHECK(vkBeginCommandBuffer(command, &begin));
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &set, 0, nullptr);
    vkCmdDispatch(command, 1, 1, 1);
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);
    CHECK(vkEndCommandBuffer(command));
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command;
    CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
    CHECK(vkQueueWaitIdle(queue));
    auto* bytes = static_cast<uint8_t*>(buffers[0].mapped);
    auto* halves = static_cast<uint16_t*>(buffers[alias ? 0 : 1].mapped);
    auto* results = static_cast<uint32_t*>(buffers[2].mapped);
    for (uint32_t i = 0; i < 64; ++i) {
        uint32_t b = (i * 17u + 3u) & 255u;
        uint32_t h = (i * 1009u + 7u) & 65535u;
        uint32_t hi = i + (alias ? 32u : 0u);
        uint32_t peer = (i + 1u) & 63u;
        uint32_t peer_byte = (peer * 17u + 3u) & 255u;
        uint32_t peer_half = (peer * 1009u + 7u) & 65535u;
        if (bytes[i] != b || halves[hi] != h || results[i] != (peer_byte | (peer_half << 8))) {
            std::fprintf(stderr, "mismatch %u: byte=%u half=%u combined=%u\n", i, bytes[i], halves[hi], results[i]);
            return 1;
        }
    }
    for (uint32_t i = alias ? 192 : 64; i < 1024; ++i)
        if (bytes[i] != 0xa5) { std::fprintf(stderr, "byte guard overwritten at %u\n", i); return 1; }
    for (uint32_t i = alias ? 0 : 128; i < 1024; ++i)
        if (static_cast<uint8_t*>(buffers[1].mapped)[i] != 0xa5) { std::fprintf(stderr, "half guard overwritten at %u\n", i); return 1; }
    for (uint32_t i = 256; i < 1024; ++i)
        if (static_cast<uint8_t*>(buffers[2].mapped)[i] != 0xa5) { std::fprintf(stderr, "result guard overwritten at %u\n", i); return 1; }
    std::puts("PASS: 64 concurrent 8/16-bit writes, reads and guard bytes");
    vkDestroyCommandPool(device, cmdpool, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyShaderModule(device, shader, nullptr);
    vkDestroyDescriptorPool(device, pool, nullptr);
    vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(device, layout, nullptr);
    for (auto& b : buffers) {
        vkUnmapMemory(device, b.memory);
        vkDestroyBuffer(device, b.buffer, nullptr);
        vkFreeMemory(device, b.memory, nullptr);
    }
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    return 0;
}
