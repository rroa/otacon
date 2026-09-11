// VulkanRenderer.cpp — Vulkan backend (Otacon).
//
// Consumes the same logical-space triangle lists as the GL backends
// (submitTriangles / submitTextured) and renders them with a minimal but
// complete Vulkan 2D pipeline: instance (+ MoltenVK portability) -> device ->
// swapchain -> render pass -> solid+textured graphics pipelines -> per-frame
// dynamic vertex buffers -> command buffers + sync.
//
// Cross-backend contract notes (see docs/backends):
//   * Logical 480x320, origin top-left, y DOWN. Vulkan's clip space already has
//     y pointing down (unlike OpenGL), so the vertex shader maps logical->clip
//     as `coord/(dim*0.5) - 1` on BOTH axes — no Y flip (the GL shaders flip Y).
//   * Straight-alpha blending (SRC_ALPHA / ONE_MINUS_SRC_ALPHA), same as GL.
//   * Geometry is the identical tessellation produced by IRenderer.
#include "render/IRenderer.hpp"
#include "platform/Window.hpp"
#include "asset/Image.hpp"
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>

#include "render/vulkan/shaders/solid.vert.h"
#include "render/vulkan/shaders/solid.frag.h"
#include "render/vulkan/shaders/tex.vert.h"
#include "render/vulkan/shaders/tex.frag.h"

namespace otacon {

#define VKCHECK(expr, msg) do { if ((expr) != VK_SUCCESS) { std::fprintf(stderr, "[vk] %s\n", msg); return false; } } while (0)

static constexpr int kMaxFramesInFlight = 2;
static constexpr VkDeviceSize kVertexBufferBytes = 4 * 1024 * 1024;   // per frame

class VulkanRenderer final : public IRenderer {
public:
    void shutdown() override {
        if (device_) vkDeviceWaitIdle(device_);
        for (auto& t : textures_) destroyTex(t);
        textures_.clear();
        destroySwapchainObjects();
        if (sampler_) vkDestroySampler(device_, sampler_, nullptr);
        if (samplerRepeat_) vkDestroySampler(device_, samplerRepeat_, nullptr);
        if (descPool_) vkDestroyDescriptorPool(device_, descPool_, nullptr);
        if (descLayout_) vkDestroyDescriptorSetLayout(device_, descLayout_, nullptr);
        for (auto p : {pipeSolidFill_, pipeSolidLine_, pipeTex_})
            if (p) vkDestroyPipeline(device_, p, nullptr);
        if (pipeLayoutSolid_) vkDestroyPipelineLayout(device_, pipeLayoutSolid_, nullptr);
        if (pipeLayoutTex_) vkDestroyPipelineLayout(device_, pipeLayoutTex_, nullptr);
        if (renderPass_) vkDestroyRenderPass(device_, renderPass_, nullptr);
        for (int i = 0; i < kMaxFramesInFlight; ++i) {
            if (frames_[i].vbuf) vkDestroyBuffer(device_, frames_[i].vbuf, nullptr);
            if (frames_[i].vmem) vkFreeMemory(device_, frames_[i].vmem, nullptr);
            if (frames_[i].imgAvail) vkDestroySemaphore(device_, frames_[i].imgAvail, nullptr);
            if (frames_[i].fence) vkDestroyFence(device_, frames_[i].fence, nullptr);
        }
        if (cmdPool_) vkDestroyCommandPool(device_, cmdPool_, nullptr);
        if (device_) vkDestroyDevice(device_, nullptr);
        if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
        if (instance_) vkDestroyInstance(instance_, nullptr);
    }
    const char* name() const override { return "Vulkan"; }
    void setWireframe(bool on) override { wireframe_ = on; }

    TextureHandle createTexture(int w, int h, const std::uint8_t* rgba, bool repeat) override;
    void destroyTexture(TextureHandle t) override {
        if (t == 0 || t > textures_.size()) return;
        vkDeviceWaitIdle(device_);
        destroyTex(textures_[t - 1]);
    }

protected:
    bool onInit(IWindow* window) override;
    void onBeginFrame(Color clear) override;
    void onEndFrame() override;
    void submitTriangles(const Vertex* v, std::size_t n) override;
    void submitTextured(const TexVertex* v, std::size_t n, TextureHandle tex) override;

private:
    struct FrameData {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkSemaphore imgAvail = VK_NULL_HANDLE;       // per frame-in-flight (acquire)
        VkFence fence = VK_NULL_HANDLE;
        VkBuffer vbuf = VK_NULL_HANDLE;
        VkDeviceMemory vmem = VK_NULL_HANDLE;
        void* mapped = nullptr;
        VkDeviceSize offset = 0;
    };
    struct VkTexture {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory mem = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkDescriptorSet set = VK_NULL_HANDLE;
        bool alive = false;
    };

    // init helpers (return false on failure)
    bool createInstance(IWindow*);
    bool createSurface(IWindow*);
    bool pickPhysicalDevice();
    bool createDevice();
    bool createSwapchain();
    bool createRenderPass();
    bool createFramebuffers();
    bool createSyncAndCommands();
    bool createPipelines();
    bool createDescriptorsAndSampler();
    bool createFrameVertexBuffers();
    void destroySwapchainObjects();
    bool recreateSwapchain();

    VkShaderModule makeShader(const uint32_t* code, std::size_t bytes);
    uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props);
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                      VkBuffer& buf, VkDeviceMemory& mem);
    VkPipeline buildPipeline(bool textured, bool wireframe);
    void destroyTex(VkTexture& t);
    void appendDraw(const void* data, std::size_t bytes, std::size_t vertexCount,
                    bool textured, VkDescriptorSet set);

    IWindow* window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice phys_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t queueFamily_ = 0;
    VkQueue queue_ = VK_NULL_HANDLE;
    bool portability_ = false;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapFormat_ = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D extent_{480, 320};
    std::vector<VkImage> swapImages_;
    std::vector<VkImageView> swapViews_;
    std::vector<VkFramebuffer> framebuffers_;
    std::vector<VkSemaphore> renderFinished_;   // per swapchain image (present wait)
    VkRenderPass renderPass_ = VK_NULL_HANDLE;

    VkCommandPool cmdPool_ = VK_NULL_HANDLE;
    FrameData frames_[kMaxFramesInFlight];
    int frame_ = 0;
    uint32_t imageIndex_ = 0;
    bool skipFrame_ = false;
    Color clear_{};

    VkDescriptorSetLayout descLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descPool_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE, samplerRepeat_ = VK_NULL_HANDLE;
    VkPipelineLayout pipeLayoutSolid_ = VK_NULL_HANDLE, pipeLayoutTex_ = VK_NULL_HANDLE;
    VkPipeline pipeSolidFill_ = VK_NULL_HANDLE, pipeSolidLine_ = VK_NULL_HANDLE, pipeTex_ = VK_NULL_HANDLE;
    bool wireframe_ = false;
    bool fillModeNonSolid_ = false;
    bool canReadback_ = false;

    std::vector<VkTexture> textures_;
};

// ---------------------------------------------------------------------------
uint32_t VulkanRenderer::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(phys_, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props)
            return i;
    return 0;
}

bool VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                                  VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size; bi.usage = usage; bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VKCHECK(vkCreateBuffer(device_, &bi, nullptr, &buf), "vkCreateBuffer");
    VkMemoryRequirements req; vkGetBufferMemoryRequirements(device_, buf, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, props);
    VKCHECK(vkAllocateMemory(device_, &ai, nullptr, &mem), "vkAllocateMemory(buffer)");
    vkBindBufferMemory(device_, buf, mem, 0);
    return true;
}

VkShaderModule VulkanRenderer::makeShader(const uint32_t* code, std::size_t bytes) {
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = bytes; ci.pCode = code;
    VkShaderModule m = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device_, &ci, nullptr, &m) != VK_SUCCESS) return VK_NULL_HANDLE;
    return m;
}

// ---------------------------------------------------------------------------
bool VulkanRenderer::onInit(IWindow* window) {
    window_ = window;
    // Help the loader find the MoltenVK ICD on macOS Homebrew installs.
#if defined(__APPLE__)
    if (!std::getenv("VK_ICD_FILENAMES")) {
        const char* icd = "/opt/homebrew/etc/vulkan/icd.d/MoltenVK_icd.json";
        if (FILE* f = std::fopen(icd, "r")) { std::fclose(f); setenv("VK_ICD_FILENAMES", icd, 0); }
    }
#endif
    if (!createInstance(window)) return false;
    if (!createSurface(window)) return false;
    if (!pickPhysicalDevice()) return false;
    if (!createDevice()) return false;
    if (!createSwapchain()) return false;
    if (!createRenderPass()) return false;
    if (!createFramebuffers()) return false;
    if (!createSyncAndCommands()) return false;
    if (!createDescriptorsAndSampler()) return false;
    if (!createPipelines()) return false;
    if (!createFrameVertexBuffers()) return false;
    std::printf("[vk] Vulkan backend ready (%dx%d, %u swapchain images)\n",
                extent_.width, extent_.height, unsigned(swapImages_.size()));
    return true;
}

bool VulkanRenderer::createInstance(IWindow* window) {
    const char** winExts = nullptr; uint32_t winExtCount = 0;
    window->vulkanInstanceExtensions(winExts, winExtCount);
    std::vector<const char*> exts(winExts, winExts + winExtCount);

    // Probe available instance extensions for portability enumeration (MoltenVK).
    uint32_t n = 0; vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> have(n);
    vkEnumerateInstanceExtensionProperties(nullptr, &n, have.data());
    VkInstanceCreateFlags flags = 0;
    for (auto& e : have)
        if (std::strcmp(e.extensionName, "VK_KHR_portability_enumeration") == 0) {
            exts.push_back("VK_KHR_portability_enumeration");
            flags |= 0x00000001 /*VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR*/;
            portability_ = true;
        }

    // Enable validation layer if present (debugging aid; harmless if absent).
    std::vector<const char*> layers;
    uint32_t ln = 0; vkEnumerateInstanceLayerProperties(&ln, nullptr);
    std::vector<VkLayerProperties> lp(ln);
    vkEnumerateInstanceLayerProperties(&ln, lp.data());
    for (auto& l : lp)
        if (std::strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0)
            layers.push_back("VK_LAYER_KHRONOS_validation");

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Otacon"; app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.flags = flags; ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = uint32_t(exts.size()); ci.ppEnabledExtensionNames = exts.data();
    ci.enabledLayerCount = uint32_t(layers.size()); ci.ppEnabledLayerNames = layers.data();
    VKCHECK(vkCreateInstance(&ci, nullptr, &instance_), "vkCreateInstance");
    return true;
}

bool VulkanRenderer::createSurface(IWindow* window) {
    // The forward-declared handle types in Window.hpp are the same as Vulkan's.
    if (!window->createVulkanSurface(instance_, surface_)) {
        std::fprintf(stderr, "[vk] surface creation failed\n");
        return false;
    }
    return true;
}

bool VulkanRenderer::pickPhysicalDevice() {
    uint32_t n = 0; vkEnumeratePhysicalDevices(instance_, &n, nullptr);
    if (!n) { std::fprintf(stderr, "[vk] no physical devices\n"); return false; }
    std::vector<VkPhysicalDevice> devs(n); vkEnumeratePhysicalDevices(instance_, &n, devs.data());
    for (auto d : devs) {
        uint32_t qn = 0; vkGetPhysicalDeviceQueueFamilyProperties(d, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qf(qn);
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qn, qf.data());
        for (uint32_t i = 0; i < qn; ++i) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(d, i, surface_, &present);
            if ((qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
                phys_ = d; queueFamily_ = i;
                VkPhysicalDeviceFeatures feat; vkGetPhysicalDeviceFeatures(d, &feat);
                fillModeNonSolid_ = feat.fillModeNonSolid;
                return true;
            }
        }
    }
    std::fprintf(stderr, "[vk] no graphics+present queue\n");
    return false;
}

bool VulkanRenderer::createDevice() {
    float prio = 1.0f;
    VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    q.queueFamilyIndex = queueFamily_; q.queueCount = 1; q.pQueuePriorities = &prio;

    std::vector<const char*> exts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    uint32_t n = 0; vkEnumerateDeviceExtensionProperties(phys_, nullptr, &n, nullptr);
    std::vector<VkExtensionProperties> have(n);
    vkEnumerateDeviceExtensionProperties(phys_, nullptr, &n, have.data());
    for (auto& e : have)
        if (std::strcmp(e.extensionName, "VK_KHR_portability_subset") == 0)
            exts.push_back("VK_KHR_portability_subset");   // required by spec for MoltenVK

    VkPhysicalDeviceFeatures feat{};
    feat.fillModeNonSolid = fillModeNonSolid_ ? VK_TRUE : VK_FALSE;
    VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    ci.queueCreateInfoCount = 1; ci.pQueueCreateInfos = &q;
    ci.enabledExtensionCount = uint32_t(exts.size()); ci.ppEnabledExtensionNames = exts.data();
    ci.pEnabledFeatures = &feat;
    VKCHECK(vkCreateDevice(phys_, &ci, nullptr, &device_), "vkCreateDevice");
    vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);
    return true;
}

bool VulkanRenderer::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_, surface_, &caps);

    uint32_t fn = 0; vkGetPhysicalDeviceSurfaceFormatsKHR(phys_, surface_, &fn, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fn);
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys_, surface_, &fn, formats.data());
    swapFormat_ = formats[0].format;
    VkColorSpaceKHR space = formats[0].colorSpace;
    for (auto& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM) { swapFormat_ = f.format; space = f.colorSpace; break; }

    int fbw = 0, fbh = 0; window_->framebufferSize(fbw, fbh);
    extent_ = caps.currentExtent.width != 0xFFFFFFFF ? caps.currentExtent
              : VkExtent2D{uint32_t(fbw), uint32_t(fbh)};
    if (extent_.width == 0 || extent_.height == 0) extent_ = {480, 320};

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount && imgCount > caps.maxImageCount) imgCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = surface_; ci.minImageCount = imgCount;
    ci.imageFormat = swapFormat_; ci.imageColorSpace = space; ci.imageExtent = extent_;
    ci.imageArrayLayers = 1;
    // TRANSFER_SRC lets us copy the image out for screenshots (if supported).
    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    canReadback_ = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
    if (canReadback_) usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ci.imageUsage = usage;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;            // always supported (vsync)
    ci.clipped = VK_TRUE;
    VKCHECK(vkCreateSwapchainKHR(device_, &ci, nullptr, &swapchain_), "vkCreateSwapchainKHR");

    uint32_t sc = 0; vkGetSwapchainImagesKHR(device_, swapchain_, &sc, nullptr);
    swapImages_.resize(sc); vkGetSwapchainImagesKHR(device_, swapchain_, &sc, swapImages_.data());
    swapViews_.resize(sc);
    for (uint32_t i = 0; i < sc; ++i) {
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = swapImages_[i]; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = swapFormat_;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VKCHECK(vkCreateImageView(device_, &vi, nullptr, &swapViews_[i]), "vkCreateImageView(swap)");
    }
    // One present-wait semaphore per swapchain image (avoids reuse-while-in-use).
    renderFinished_.resize(sc);
    for (uint32_t i = 0; i < sc; ++i) {
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VKCHECK(vkCreateSemaphore(device_, &si, nullptr, &renderFinished_[i]), "sem(renderFinished)");
    }
    return true;
}

bool VulkanRenderer::createRenderPass() {
    VkAttachmentDescription color{};
    color.format = swapFormat_; color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{}; sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1; sub.pColorAttachments = &ref;
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL; dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo ci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    ci.attachmentCount = 1; ci.pAttachments = &color;
    ci.subpassCount = 1; ci.pSubpasses = &sub;
    ci.dependencyCount = 1; ci.pDependencies = &dep;
    VKCHECK(vkCreateRenderPass(device_, &ci, nullptr, &renderPass_), "vkCreateRenderPass");
    return true;
}

bool VulkanRenderer::createFramebuffers() {
    framebuffers_.resize(swapViews_.size());
    for (std::size_t i = 0; i < swapViews_.size(); ++i) {
        VkFramebufferCreateInfo ci{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        ci.renderPass = renderPass_; ci.attachmentCount = 1; ci.pAttachments = &swapViews_[i];
        ci.width = extent_.width; ci.height = extent_.height; ci.layers = 1;
        VKCHECK(vkCreateFramebuffer(device_, &ci, nullptr, &framebuffers_[i]), "vkCreateFramebuffer");
    }
    return true;
}

bool VulkanRenderer::createSyncAndCommands() {
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; pi.queueFamilyIndex = queueFamily_;
    VKCHECK(vkCreateCommandPool(device_, &pi, nullptr, &cmdPool_), "vkCreateCommandPool");
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = cmdPool_; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1;
        VKCHECK(vkAllocateCommandBuffers(device_, &ai, &frames_[i].cmd), "vkAllocateCommandBuffers");
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VKCHECK(vkCreateSemaphore(device_, &si, nullptr, &frames_[i].imgAvail), "sem");
        VKCHECK(vkCreateFence(device_, &fi, nullptr, &frames_[i].fence), "fence");
    }
    return true;
}

bool VulkanRenderer::createFrameVertexBuffers() {
    for (int i = 0; i < kMaxFramesInFlight; ++i) {
        if (!createBuffer(kVertexBufferBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                          frames_[i].vbuf, frames_[i].vmem))
            return false;
        vkMapMemory(device_, frames_[i].vmem, 0, kVertexBufferBytes, 0, &frames_[i].mapped);
    }
    return true;
}

bool VulkanRenderer::createDescriptorsAndSampler() {
    VkDescriptorSetLayoutBinding b{};
    b.binding = 0; b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b.descriptorCount = 1; b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    li.bindingCount = 1; li.pBindings = &b;
    VKCHECK(vkCreateDescriptorSetLayout(device_, &li, nullptr, &descLayout_), "descSetLayout");

    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pi.maxSets = 64; pi.poolSizeCount = 1; pi.pPoolSizes = &ps;
    VKCHECK(vkCreateDescriptorPool(device_, &pi, nullptr, &descPool_), "descPool");

    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = si.minFilter = VK_FILTER_NEAREST;        // crisp pixel art
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    VKCHECK(vkCreateSampler(device_, &si, nullptr, &sampler_), "sampler");
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VKCHECK(vkCreateSampler(device_, &si, nullptr, &samplerRepeat_), "samplerRepeat");
    return true;
}

VkPipeline VulkanRenderer::buildPipeline(bool textured, bool wireframe) {
    VkShaderModule vs = makeShader(textured ? kTexVert : kSolidVert,
                                   textured ? sizeof(kTexVert) : sizeof(kSolidVert));
    VkShaderModule fs = makeShader(textured ? kTexFrag : kSolidFrag,
                                   textured ? sizeof(kTexFrag) : sizeof(kSolidFrag));
    if (!vs || !fs) return VK_NULL_HANDLE;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vs; stages[0].pName = "main";
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = fs; stages[1].pName = "main";

    VkVertexInputBindingDescription bind{};
    bind.binding = 0; bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bind.stride = textured ? sizeof(TexVertex) : sizeof(Vertex);
    VkVertexInputAttributeDescription attrs[3]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    int attrCount;
    if (textured) {
        attrs[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, 2 * sizeof(float)};
        attrs[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 4 * sizeof(float)};
        attrCount = 3;
    } else {
        attrs[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 2 * sizeof(float)};
        attrCount = 2;
    }
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1; vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = attrCount; vi.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1; vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = (wireframe && fillModeNonSolid_) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = 0xF; cba.blendEnable = VK_TRUE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.colorBlendOp = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cba.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1; cb.pAttachments = &cba;

    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    ds.dynamicStateCount = 2; ds.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pi.stageCount = 2; pi.pStages = stages;
    pi.pVertexInputState = &vi; pi.pInputAssemblyState = &ia; pi.pViewportState = &vp;
    pi.pRasterizationState = &rs; pi.pMultisampleState = &ms; pi.pColorBlendState = &cb;
    pi.pDynamicState = &ds; pi.layout = textured ? pipeLayoutTex_ : pipeLayoutSolid_;
    pi.renderPass = renderPass_; pi.subpass = 0;
    VkPipeline pipe = VK_NULL_HANDLE;
    vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pi, nullptr, &pipe);
    vkDestroyShaderModule(device_, vs, nullptr);
    vkDestroyShaderModule(device_, fs, nullptr);
    return pipe;
}

bool VulkanRenderer::createPipelines() {
    VkPushConstantRange pc{VK_SHADER_STAGE_VERTEX_BIT, 0, 2 * sizeof(float)};   // viewport vec2
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.pushConstantRangeCount = 1; li.pPushConstantRanges = &pc;
    VKCHECK(vkCreatePipelineLayout(device_, &li, nullptr, &pipeLayoutSolid_), "pipeLayout(solid)");
    li.setLayoutCount = 1; li.pSetLayouts = &descLayout_;
    VKCHECK(vkCreatePipelineLayout(device_, &li, nullptr, &pipeLayoutTex_), "pipeLayout(tex)");

    pipeSolidFill_ = buildPipeline(false, false);
    pipeSolidLine_ = buildPipeline(false, true);
    pipeTex_ = buildPipeline(true, false);
    if (!pipeSolidFill_ || !pipeSolidLine_ || !pipeTex_) {
        std::fprintf(stderr, "[vk] pipeline creation failed\n"); return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
void VulkanRenderer::destroySwapchainObjects() {
    for (auto fb : framebuffers_) if (fb) vkDestroyFramebuffer(device_, fb, nullptr);
    for (auto v : swapViews_) if (v) vkDestroyImageView(device_, v, nullptr);
    for (auto s : renderFinished_) if (s) vkDestroySemaphore(device_, s, nullptr);
    framebuffers_.clear(); swapViews_.clear(); swapImages_.clear(); renderFinished_.clear();
    if (swapchain_) { vkDestroySwapchainKHR(device_, swapchain_, nullptr); swapchain_ = VK_NULL_HANDLE; }
}

bool VulkanRenderer::recreateSwapchain() {
    vkDeviceWaitIdle(device_);
    destroySwapchainObjects();
    return createSwapchain() && createFramebuffers();
}

void VulkanRenderer::onBeginFrame(Color clear) {
    clear_ = clear; skipFrame_ = false;
    FrameData& f = frames_[frame_];
    vkWaitForFences(device_, 1, &f.fence, VK_TRUE, UINT64_MAX);

    VkResult acq = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, f.imgAvail,
                                         VK_NULL_HANDLE, &imageIndex_);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(); skipFrame_ = true; return; }

    vkResetFences(device_, 1, &f.fence);
    vkResetCommandBuffer(f.cmd, 0);
    f.offset = 0;

    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(f.cmd, &bi);

    VkClearValue cv{}; cv.color = {{clear.r, clear.g, clear.b, clear.a}};
    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = renderPass_; rp.framebuffer = framebuffers_[imageIndex_];
    rp.renderArea.extent = extent_; rp.clearValueCount = 1; rp.pClearValues = &cv;
    vkCmdBeginRenderPass(f.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    // The render pass already cleared the whole framebuffer (letterbox bars);
    // restrict the viewport to the aspect-correct, centered rect.
    int vx, vy, vw, vh; letterbox(int(extent_.width), int(extent_.height), vx, vy, vw, vh);
    VkViewport vp{float(vx), float(vy), float(vw), float(vh), 0, 1};
    VkRect2D sc{{0, 0}, extent_};
    vkCmdSetViewport(f.cmd, 0, 1, &vp);
    vkCmdSetScissor(f.cmd, 0, 1, &sc);
}

void VulkanRenderer::appendDraw(const void* data, std::size_t bytes, std::size_t vertexCount,
                                bool textured, VkDescriptorSet set) {
    if (skipFrame_) return;
    FrameData& f = frames_[frame_];
    // 16-byte align each draw's vertex data.
    VkDeviceSize off = (f.offset + 15) & ~VkDeviceSize(15);
    if (off + bytes > kVertexBufferBytes) return;          // out of room this frame
    std::memcpy(static_cast<std::uint8_t*>(f.mapped) + off, data, bytes);
    f.offset = off + bytes;

    // Push constant: logical viewport (the projection divisor).
    float vpDims[2] = {float(logicalW_), float(logicalH_)};
    VkPipelineLayout layout = textured ? pipeLayoutTex_ : pipeLayoutSolid_;
    VkPipeline pipe = textured ? pipeTex_ : (wireframe_ ? pipeSolidLine_ : pipeSolidFill_);
    vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
    if (textured)
        vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayoutTex_, 0, 1, &set, 0, nullptr);
    vkCmdPushConstants(f.cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vpDims), vpDims);
    VkDeviceSize voff = off;
    vkCmdBindVertexBuffers(f.cmd, 0, 1, &f.vbuf, &voff);
    vkCmdDraw(f.cmd, uint32_t(vertexCount), 1, 0, 0);
}

void VulkanRenderer::submitTriangles(const Vertex* v, std::size_t n) {
    if (!n) return;
    appendDraw(v, n * sizeof(Vertex), n, false, VK_NULL_HANDLE);
}

void VulkanRenderer::submitTextured(const TexVertex* v, std::size_t n, TextureHandle tex) {
    if (!n || tex == 0 || tex > textures_.size()) return;
    VkTexture& t = textures_[tex - 1];
    if (!t.alive) return;
    appendDraw(v, n * sizeof(TexVertex), n, true, t.set);
}

void VulkanRenderer::onEndFrame() {
    if (skipFrame_) { frame_ = (frame_ + 1) % kMaxFramesInFlight; return; }
    FrameData& f = frames_[frame_];
    vkCmdEndRenderPass(f.cmd);

    // Screenshot: copy the swapchain image to a host buffer WITHIN this command
    // buffer (the image is still acquired, so this is valid — unlike copying
    // after present). The render pass left it in PRESENT_SRC; bounce it through
    // TRANSFER_SRC and back so present still sees PRESENT_SRC.
    VkBuffer capBuf = VK_NULL_HANDLE; VkDeviceMemory capMem = VK_NULL_HANDLE;
    int capW = 0, capH = 0;
    bool capturing = capturePath_ && canReadback_;
    if (capturing) {
        capW = int(extent_.width); capH = int(extent_.height);
        if (createBuffer(VkDeviceSize(capW) * capH * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         capBuf, capMem)) {
            VkImage img = swapImages_[imageIndex_];
            auto bar = [&](VkImageLayout from, VkImageLayout to, VkAccessFlags sa, VkAccessFlags da) {
                VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                b.oldLayout = from; b.newLayout = to; b.image = img;
                b.srcAccessMask = sa; b.dstAccessMask = da;
                b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                vkCmdPipelineBarrier(f.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     0, 0, nullptr, 0, nullptr, 1, &b);
            };
            bar(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            VkBufferImageCopy region{};
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageExtent = {uint32_t(capW), uint32_t(capH), 1};
            vkCmdCopyImageToBuffer(f.cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, capBuf, 1, &region);
            bar(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_MEMORY_READ_BIT);
        } else capturing = false;
    }

    vkEndCommandBuffer(f.cmd);

    VkSemaphore signalSem = renderFinished_[imageIndex_];   // keyed by image, not frame
    VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = 1; si.pWaitSemaphores = &f.imgAvail; si.pWaitDstStageMask = &wait;
    si.commandBufferCount = 1; si.pCommandBuffers = &f.cmd;
    si.signalSemaphoreCount = 1; si.pSignalSemaphores = &signalSem;
    vkQueueSubmit(queue_, 1, &si, f.fence);

    if (capturing) {                              // wait for the copy, then write the PNG
        vkWaitForFences(device_, 1, &f.fence, VK_TRUE, UINT64_MAX);
        void* p = nullptr; vkMapMemory(device_, capMem, 0, VK_WHOLE_SIZE, 0, &p);
        Image img; img.width = capW; img.height = capH;
        img.rgba.resize(std::size_t(capW) * capH * 4);
        const std::uint8_t* src = static_cast<const std::uint8_t*>(p);
        bool bgra = (swapFormat_ == VK_FORMAT_B8G8R8A8_UNORM || swapFormat_ == VK_FORMAT_B8G8R8A8_SRGB);
        for (std::size_t i = 0; i < std::size_t(capW) * capH; ++i) {
            std::uint8_t c0 = src[i*4+0], c1 = src[i*4+1], c2 = src[i*4+2], c3 = src[i*4+3];
            img.rgba[i*4+0] = bgra ? c2 : c0; img.rgba[i*4+1] = c1;
            img.rgba[i*4+2] = bgra ? c0 : c2; img.rgba[i*4+3] = c3;
        }
        vkUnmapMemory(device_, capMem);
        if (writePng(capturePath_, img)) std::printf("[shot] wrote %s (%dx%d)\n", capturePath_, capW, capH);
        vkDestroyBuffer(device_, capBuf, nullptr); vkFreeMemory(device_, capMem, nullptr);
        capturePath_ = nullptr;
    }

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &signalSem;
    pi.swapchainCount = 1; pi.pSwapchains = &swapchain_; pi.pImageIndices = &imageIndex_;
    VkResult pr = vkQueuePresentKHR(queue_, &pi);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) recreateSwapchain();

    frame_ = (frame_ + 1) % kMaxFramesInFlight;
}

// ---------------------------------------------------------------------------
TextureHandle VulkanRenderer::createTexture(int w, int h, const std::uint8_t* rgba, bool repeat) {
    VkDeviceSize bytes = VkDeviceSize(w) * h * 4;
    VkBuffer staging; VkDeviceMemory stagingMem;
    if (!createBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      staging, stagingMem)) return 0;
    void* p = nullptr; vkMapMemory(device_, stagingMem, 0, bytes, 0, &p);
    std::memcpy(p, rgba, bytes); vkUnmapMemory(device_, stagingMem);

    VkTexture t; t.alive = true;
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D; ii.format = VK_FORMAT_R8G8B8A8_UNORM;
    ii.extent = {uint32_t(w), uint32_t(h), 1}; ii.mipLevels = 1; ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT; ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkCreateImage(device_, &ii, nullptr, &t.image);
    VkMemoryRequirements req; vkGetImageMemoryRequirements(device_, t.image, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(device_, &ai, nullptr, &t.mem);
    vkBindImageMemory(device_, t.image, t.mem, 0);

    // One-time command: transition, copy, transition to shader-read.
    VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cai.commandPool = cmdPool_; cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
    VkCommandBuffer cmd; vkAllocateCommandBuffers(device_, &cai, &cmd);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    auto barrier = [&](VkImageLayout from, VkImageLayout to, VkAccessFlags sa, VkAccessFlags da,
                       VkPipelineStageFlags ss, VkPipelineStageFlags dstS) {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = from; b.newLayout = to; b.image = t.image;
        b.srcAccessMask = sa; b.dstAccessMask = da;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkCmdPipelineBarrier(cmd, ss, dstS, 0, 0, nullptr, 0, nullptr, 1, &b);
    };
    barrier(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {uint32_t(w), uint32_t(h), 1};
    vkCmdCopyBufferToImage(cmd, staging, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    barrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    vkEndCommandBuffer(cmd);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    vkQueueSubmit(queue_, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue_);
    vkFreeCommandBuffers(device_, cmdPool_, 1, &cmd);
    vkDestroyBuffer(device_, staging, nullptr);
    vkFreeMemory(device_, stagingMem, nullptr);

    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = t.image; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCreateImageView(device_, &vi, nullptr, &t.view);

    VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dai.descriptorPool = descPool_; dai.descriptorSetCount = 1; dai.pSetLayouts = &descLayout_;
    vkAllocateDescriptorSets(device_, &dai, &t.set);
    VkDescriptorImageInfo dii{repeat ? samplerRepeat_ : sampler_, t.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet w2{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w2.dstSet = t.set; w2.dstBinding = 0; w2.descriptorCount = 1;
    w2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w2.pImageInfo = &dii;
    vkUpdateDescriptorSets(device_, 1, &w2, 0, nullptr);

    textures_.push_back(t);
    return TextureHandle(textures_.size());   // handle = index + 1
}

void VulkanRenderer::destroyTex(VkTexture& t) {
    if (!t.alive) return;
    if (t.view) vkDestroyImageView(device_, t.view, nullptr);
    if (t.image) vkDestroyImage(device_, t.image, nullptr);
    if (t.mem) vkFreeMemory(device_, t.mem, nullptr);
    t = VkTexture{};
}

IRenderer* createRendererVulkan() { return new VulkanRenderer(); }

} // namespace otacon
